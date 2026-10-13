#ifndef UI_ARCADEIPC_H
#define UI_ARCADEIPC_H

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <functional>

enum class IpcConnectionStatus {
    OFFLINE,
    CONNECTING,
    CONNECTED,
    RECONNECTING
};

class ArcadeIpcClient {
public:
    static ArcadeIpcClient& get();

    void init();
    void shutdown();
    void update(float dt);

    bool isConnected() const { return status == IpcConnectionStatus::CONNECTED; }
    IpcConnectionStatus getStatus() const { return status; }

    int getCredits() const;
    int getTotalCollected() const;

    // Request credit consumption for an additional round (2nd round onwards)
    // Synchronously waits up to timeoutMs for the agent's outcome.
    bool consumeCredit(int amount = 1, int timeoutMs = 1500);

    // Session tracking
    void startSession(const std::string& customId = "");
    void endSession(float durationSeconds = -1.0f, int score = -1);
    bool isSessionActive() const { return sessionActive; }
    const std::string& getPlaySessionId() const { return currentPlaySessionId; }

    // First round charging policy
    bool isFirstRoundCharged() const { return firstRoundCharged; }
    void markFirstRoundUsed() { firstRoundCharged = false; }

    // Socket configuration (read from env or set explicitly for tests)
    void setSocketPath(const std::string& path);
    const std::string& getSocketPath() const { return socketPath; }

    void setReleaseId(const std::string& id) { releaseId = id; }
    const std::string& getReleaseId() const { return releaseId; }

    // Manual / Test injection
    void processIncomingFrame(const std::string& jsonLine);

private:
    ArcadeIpcClient();
    ~ArcadeIpcClient();

    void workerLoop();
    bool connectSocket();
    void closeSocket();
    void sendFrame(const std::string& json);
    void readFrames();

    std::string generateId(const std::string& prefix);
    std::string readReleaseIdFromMetadata();

    std::string socketPath;
    std::string releaseId = "ec-release-0.1.2";
    int socketFd = -1;

    std::atomic<IpcConnectionStatus> status{IpcConnectionStatus::OFFLINE};
    std::atomic<bool> running{false};
    std::thread workerThread;

    mutable std::mutex stateMutex;
    int currentCredits = 0;
    int totalCollected = 0;
    int idCounter = 1;

    // Session state
    bool sessionActive = false;
    std::string currentPlaySessionId;
    std::chrono::steady_clock::time_point sessionStartTime;
    bool sessionEndSent = false;

    // First round free from in-game perspective (already charged by Launcher at exec)
    bool firstRoundCharged = true;

    // Correlated request/response handling
    struct PendingConsume {
        std::string requestId;
        bool completed = false;
        bool success = false;
        int remainingCredits = 0;
    };
    PendingConsume currentConsume;
    mutable std::mutex consumeMutex;

    // Frame buffer for fragmented reads
    std::string readBuffer;
    static constexpr size_t MAX_FRAME_SIZE = 65536;
};

#endif // UI_ARCADEIPC_H
