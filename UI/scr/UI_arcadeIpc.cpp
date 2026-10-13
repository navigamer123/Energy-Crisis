#include "../includes/UI_arcadeIpc.h"
#include "../includes/UI_arcadePopup.h"
#include "../includes/UI_arcadeLifecycle.h"
#include "../includes/UI_settings.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <algorithm>

namespace {

// Lightweight helper to extract a string field from compact JSON
std::string extractJsonString(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.length();
    size_t end = json.find("\"", pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

// Lightweight helper to extract an integer field from compact JSON
bool extractJsonInt(const std::string& json, const std::string& key, int& outVal) {
    std::string needle = "\"" + key + "\":";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return false;
    pos += needle.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    size_t end = pos;
    if (end < json.length() && (json[end] == '-' || json[end] == '+')) end++;
    while (end < json.length() && json[end] >= '0' && json[end] <= '9') end++;
    if (end == pos) return false;
    try {
        outVal = std::stoi(json.substr(pos, end - pos));
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

ArcadeIpcClient& ArcadeIpcClient::get() {
    static ArcadeIpcClient instance;
    return instance;
}

ArcadeIpcClient::ArcadeIpcClient() {
    const char* envPath = std::getenv("DHO_AGENT_SOCKET_PATH");
    socketPath = (envPath && envPath[0] != '\0') ? std::string(envPath) : "/run/dho-agent/agent.sock";
    releaseId = readReleaseIdFromMetadata();
}

ArcadeIpcClient::~ArcadeIpcClient() {
    shutdown();
}

std::string ArcadeIpcClient::readReleaseIdFromMetadata() {
    const std::vector<std::string> paths = { "metadata.json", "../metadata.json", "./assets/metadata.json" };
    for (const auto& path : paths) {
        std::ifstream f(path);
        if (f.is_open()) {
            std::stringstream buffer;
            buffer << f.rdbuf();
            std::string content = buffer.str();
            std::string id = extractJsonString(content, "release_id");
            if (!id.empty()) {
                std::cout << "[ArcadeIpc] Read release_id \"" << id << "\" from " << path << "\n";
                return id;
            }
        }
    }
    return "ec-release-0.1.2";
}

void ArcadeIpcClient::setSocketPath(const std::string& path) {
    std::lock_guard<std::mutex> lock(stateMutex);
    socketPath = path;
}

int ArcadeIpcClient::getCredits() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return currentCredits;
}

int ArcadeIpcClient::getTotalCollected() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return totalCollected;
}

std::string ArcadeIpcClient::generateId(const std::string& prefix) {
    std::lock_guard<std::mutex> lock(stateMutex);
    return prefix + "-" + std::to_string(idCounter++);
}

void ArcadeIpcClient::init() {
    if (running.load()) return;
    running.store(true);
    workerThread = std::thread(&ArcadeIpcClient::workerLoop, this);
}

void ArcadeIpcClient::shutdown() {
    running.store(false);
    closeSocket();
    if (workerThread.joinable()) {
        workerThread.join();
    }
}

void ArcadeIpcClient::closeSocket() {
    if (socketFd >= 0) {
        ::close(socketFd);
        socketFd = -1;
    }
    status.store(IpcConnectionStatus::OFFLINE);
}

bool ArcadeIpcClient::connectSocket() {
    closeSocket();
    std::string path;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        path = socketPath;
    }

    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;

    // Set non-blocking
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0) ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (path.length() >= sizeof(addr.sun_path)) {
        ::close(fd);
        return false;
    }
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        if (errno != EINPROGRESS) {
            ::close(fd);
            return false;
        }
    }

    socketFd = fd;
    status.store(IpcConnectionStatus::CONNECTING);

    // Send hello handshake
    std::string relId;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        relId = releaseId;
    }

    std::string helloId = generateId("ec-hello");
    std::string helloJson = "{\"type\":\"hello\",\"protocol_version\":1,\"id\":\"" + helloId +
                            "\",\"supported_protocol_versions\":[1],\"role\":\"unity\","
                            "\"client\":{\"name\":\"energy-crisis-native-bridge\",\"version\":\"1.0.0\"},"
                            "\"release_id\":\"" + relId + "\",\"sdk_version\":\"1.0.0\","
                            "\"local_protocol_version\":1}\n";

    sendFrame(helloJson);
    std::cout << "[ArcadeIpc] Connected to agent socket " << path << ", sent hello handshake.\n";
    return true;
}

void ArcadeIpcClient::sendFrame(const std::string& json) {
    if (socketFd < 0) return;
    std::string frame = json;
    if (frame.empty() || frame.back() != '\n') frame.push_back('\n');
    if (frame.length() > MAX_FRAME_SIZE) {
        std::cerr << "[ArcadeIpc] Error: outgoing frame exceeded MAX_FRAME_SIZE (" << frame.length() << " > 65536)\n";
        return;
    }
    ::send(socketFd, frame.c_str(), frame.length(), MSG_NOSIGNAL);
}

void ArcadeIpcClient::workerLoop() {
    int backoffSeconds = 1;
    while (running.load()) {
        if (socketFd < 0) {
            if (!connectSocket()) {
                std::this_thread::sleep_for(std::chrono::seconds(backoffSeconds));
                backoffSeconds = std::min(backoffSeconds * 2, 5);
                continue;
            }
            backoffSeconds = 1;
        }

        struct pollfd pfd;
        pfd.fd = socketFd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int ret = ::poll(&pfd, 1, 100);
        if (ret > 0) {
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                std::cout << "[ArcadeIpc] Agent socket disconnected.\n";
                closeSocket();
                continue;
            }
            if (pfd.revents & POLLIN) {
                readFrames();
            }
        } else if (ret < 0 && errno != EINTR) {
            closeSocket();
        }
    }
    closeSocket();
}

void ArcadeIpcClient::readFrames() {
    char buf[4096];
    while (running.load() && socketFd >= 0) {
        ssize_t n = ::recv(socketFd, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            buf[n] = '\0';
            readBuffer.append(buf, n);
            if (readBuffer.length() > MAX_FRAME_SIZE * 2) {
                std::cerr << "[ArcadeIpc] Read buffer overflow without newline, resetting.\n";
                readBuffer.clear();
                return;
            }

            size_t newlinePos;
            while ((newlinePos = readBuffer.find('\n')) != std::string::npos) {
                std::string line = readBuffer.substr(0, newlinePos);
                readBuffer.erase(0, newlinePos + 1);

                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (!line.empty()) {
                    if (line.length() > MAX_FRAME_SIZE) {
                        std::cerr << "[ArcadeIpc] Dropping frame exceeding 65536 bytes.\n";
                        continue;
                    }
                    processIncomingFrame(line);
                }
            }
        } else if (n == 0) {
            std::cout << "[ArcadeIpc] Socket EOF from agent.\n";
            closeSocket();
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            closeSocket();
            break;
        }
    }
}

void ArcadeIpcClient::processIncomingFrame(const std::string& line) {
    std::string type = extractJsonString(line, "type");
    if (type.empty()) return;

    if (type == "welcome") {
        status.store(IpcConnectionStatus::CONNECTED);
        std::cout << "[ArcadeIpc] Received welcome from Agent.\n";

        int timeoutSec = 0;
        if (extractJsonInt(line, "inactivity_timeout_seconds", timeoutSec) && timeoutSec > 0) {
            ArcadeLifecycle::get().setInactivityTimeout(static_cast<float>(timeoutSec));
            std::cout << "[ArcadeIpc] Inactivity timeout negotiated: " << timeoutSec << "s\n";
        }

        // Subscribe to credit_change and agent_status
        std::string subId = generateId("ec-sub");
        std::string subJson = "{\"type\":\"subscribe\",\"protocol_version\":1,\"id\":\"" + subId +
                              "\",\"event_types\":[\"credit_change\",\"agent_status\"]}\n";
        sendFrame(subJson);

        // Query initial credit balance
        std::string balId = generateId("ec-bal");
        std::string balJson = "{\"type\":\"credit_balance_query\",\"protocol_version\":1,\"id\":\"" + balId + "\"}\n";
        sendFrame(balJson);
    } else if (type == "event") {
        std::string eventType = extractJsonString(line, "event_type");
        if (eventType == "credit_change") {
            int newCredits = 0;
            int total = 0;
            int delta = 0;
            bool hasCredits = extractJsonInt(line, "credits", newCredits);
            extractJsonInt(line, "total_collected", total);
            bool hasDelta = extractJsonInt(line, "delta", delta);

            if (hasCredits) {
                std::lock_guard<std::mutex> lock(stateMutex);
                currentCredits = newCredits;
                if (total > 0) totalCollected = total;
            }

            // Only delta > 0 authoritatively proves an addition and displays Credit added
            if (hasDelta && delta > 0) {
                bool isEn = (UI_settings::get().getLanguage() == "en");
                std::string msg = isEn ? ("CREDIT ADDED\n(TOTAL: " + std::to_string(newCredits) + ")")
                                       : ("ДОБАВЕН КРЕДИТ\n(ОБЩО: " + std::to_string(newCredits) + ")");
                ArcadePopup::get().show(msg, 2.5f);
            }
        }
    } else if (type == "credit_balance_result") {
        int credits = 0;
        int total = 0;
        if (extractJsonInt(line, "credits", credits)) {
            std::lock_guard<std::mutex> lock(stateMutex);
            currentCredits = credits;
            if (extractJsonInt(line, "total_collected", total)) totalCollected = total;
        }
    } else if (type == "credit_consume_result") {
        std::string id = extractJsonString(line, "id");
        std::string outcome = extractJsonString(line, "outcome");
        int remaining = 0;
        extractJsonInt(line, "credits", remaining);

        std::lock_guard<std::mutex> lock(consumeMutex);
        if (currentConsume.requestId == id) {
            currentConsume.completed = true;
            currentConsume.remainingCredits = remaining;
            currentConsume.success = (outcome == "consumed");
        }
        if (outcome == "consumed") {
            {
                std::lock_guard<std::mutex> stateLock(stateMutex);
                currentCredits = remaining;
            }
            bool isEn = (UI_settings::get().getLanguage() == "en");
            std::string msg = isEn ? ("1 TOKEN CONSUMED\n(CREDITS: " + std::to_string(remaining) + ")")
                                   : ("1 ЖЕТОН ИЗПОЛЗВАН\n(ОСТАВАТ: " + std::to_string(remaining) + ")");
            ArcadePopup::get().show(msg, 2.5f);
        }
    } else if (type == "error") {
        std::string id = extractJsonString(line, "id");
        std::lock_guard<std::mutex> lock(consumeMutex);
        if (currentConsume.requestId == id) {
            currentConsume.completed = true;
            currentConsume.success = false;
        }
    }
}

bool ArcadeIpcClient::consumeCredit(int amount, int timeoutMs) {
    if (amount <= 0) return true;

    // First round is already charged by the Launcher upon exec!
    if (firstRoundCharged) {
        firstRoundCharged = false;
        std::cout << "[ArcadeIpc] First round covered by Launcher initial credit. No duplicate charge.\n";
        return true;
    }

    if (!isConnected()) {
        std::cout << "[ArcadeIpc] Not connected to Agent. Checking local credits...\n";
        return false;
    }

    std::string reqId = generateId("ec-charge");
    {
        std::lock_guard<std::mutex> lock(consumeMutex);
        currentConsume.requestId = reqId;
        currentConsume.completed = false;
        currentConsume.success = false;
        currentConsume.remainingCredits = 0;
    }

    std::string reqJson = "{\"type\":\"credit_consume_request\",\"protocol_version\":1,\"id\":\"" +
                          reqId + "\",\"amount\":" + std::to_string(amount) + "}\n";
    sendFrame(reqJson);

    // Wait synchronously up to timeoutMs
    int waited = 0;
    while (waited < timeoutMs) {
        {
            std::lock_guard<std::mutex> lock(consumeMutex);
            if (currentConsume.completed) {
                if (!currentConsume.success) {
                    bool isEn = (UI_settings::get().getLanguage() == "en");
                    std::string msg = isEn ? "INSERT COIN\n(0 CREDITS)" : "НЯМА КРЕДИТИ!\nПУСНЕТЕ МОНЕТА";
                    ArcadePopup::get().show(msg, 2.5f);
                }
                return currentConsume.success;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        waited += 25;
    }

    std::cerr << "[ArcadeIpc] Credit consume timed out after " << timeoutMs << "ms.\n";
    return false;
}

void ArcadeIpcClient::startSession(const std::string& customId) {
    sessionActive = true;
    sessionEndSent = false;
    sessionStartTime = std::chrono::steady_clock::now();
    currentPlaySessionId = customId.empty() ? generateId("ec-play") : customId;

    std::string reqId = generateId("ec-sess-start");
    std::string json = "{\"type\":\"session_start\",\"protocol_version\":1,\"id\":\"" + reqId +
                       "\",\"play_session_id\":\"" + currentPlaySessionId + "\"}\n";
    sendFrame(json);
    std::cout << "[ArcadeIpc] Started session: " << currentPlaySessionId << "\n";
}

void ArcadeIpcClient::endSession(float durationSeconds, int score) {
    if (!sessionActive || sessionEndSent) return;
    sessionEndSent = true;
    sessionActive = false;

    float dur = durationSeconds;
    if (dur < 0.0f) {
        auto now = std::chrono::steady_clock::now();
        dur = std::chrono::duration<float>(now - sessionStartTime).count();
    }

    std::string reqId = generateId("ec-sess-end");
    std::string json = "{\"type\":\"session_end\",\"protocol_version\":1,\"id\":\"" + reqId +
                       "\",\"play_session_id\":\"" + currentPlaySessionId +
                       "\",\"duration_seconds\":" + std::to_string(dur);
    if (score >= 0) {
        json += ",\"score\":" + std::to_string(score);
    }
    json += "}\n";

    sendFrame(json);
    std::cout << "[ArcadeIpc] Ended session: " << currentPlaySessionId << " (duration: " << dur << "s)\n";
}

void ArcadeIpcClient::update(float dt) {
    (void)dt;
}
