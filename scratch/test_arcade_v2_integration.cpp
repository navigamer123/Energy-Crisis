#include <cassert>
#include <iostream>
#include <string>
#include <cstring>
#include <chrono>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>

#include "UI/includes/UI_arcadeLifecycle.h"
#include "UI/includes/UI_arcadePopup.h"
#include "UI/includes/UI_arcadeIpc.h"
#include "UI/includes/UI_settings.h"

// -----------------------------------------------------------------------------
// Test 1: ArcadeLifecycle Change Game 3s confirmation in menus
// -----------------------------------------------------------------------------
void test_lifecycle_menu_change_game() {
    std::cout << "[Test] Running test_lifecycle_menu_change_game (2 presses in 5.0s)...\n";
    ArcadeLifecycle& lifecycle = ArcadeLifecycle::get();
    lifecycle.reset();
    lifecycle.setState(ArcadeLifecycleState::START_MENU);

    assert(!lifecycle.shouldExit());

    // 1st press: arms confirmation window
    lifecycle.onExitButtonDown();
    lifecycle.onExitButtonUp();
    assert(lifecycle.isConfirmationArmed());
    assert(!lifecycle.shouldExit());

    // 2nd press within 5s: confirms exit
    lifecycle.onExitButtonDown();
    lifecycle.onExitButtonUp();
    assert(lifecycle.shouldExit());

    // Test expiration: 1st press, then wait > 5s, then 2nd press should NOT exit
    lifecycle.reset();
    lifecycle.setState(ArcadeLifecycleState::START_MENU);
    lifecycle.onExitButtonDown();
    lifecycle.onExitButtonUp();
    assert(lifecycle.isConfirmationArmed());
    assert(!lifecycle.shouldExit());

    // Simulate 5.1s elapsed
    std::this_thread::sleep_for(std::chrono::milliseconds(5100));
    lifecycle.update(5.1f);
    assert(!lifecycle.isConfirmationArmed());

    // This is now treated as a new first press, not a confirmation
    lifecycle.onExitButtonDown();
    lifecycle.onExitButtonUp();
    assert(lifecycle.isConfirmationArmed());
    assert(!lifecycle.shouldExit());

    std::cout << "  -> test_lifecycle_menu_change_game PASSED\n";
}

// -----------------------------------------------------------------------------
// Test 2: ArcadeLifecycle Exit 2 presses within 5.0s in active gameplay
// -----------------------------------------------------------------------------
void test_lifecycle_force_quit() {
    std::cout << "[Test] Running test_lifecycle_force_quit (2 presses in 5.0s in gameplay)...\n";
    ArcadeLifecycle& lifecycle = ArcadeLifecycle::get();
    lifecycle.reset();
    lifecycle.setState(ArcadeLifecycleState::ACTIVE_GAMEPLAY);

    // 1st press: arms confirmation window, does not exit
    lifecycle.onExitButtonDown();
    lifecycle.onExitButtonUp();
    assert(lifecycle.isConfirmationArmed());
    assert(!lifecycle.shouldExit());

    // 2nd press within 5s: confirms exit
    lifecycle.onExitButtonDown();
    lifecycle.onExitButtonUp();
    assert(lifecycle.shouldExit());

    // Test expiration in gameplay: wait > 5s
    lifecycle.reset();
    lifecycle.setState(ArcadeLifecycleState::ACTIVE_GAMEPLAY);
    lifecycle.onExitButtonDown();
    lifecycle.onExitButtonUp();
    assert(lifecycle.isConfirmationArmed());

    std::this_thread::sleep_for(std::chrono::milliseconds(5100));
    lifecycle.update(5.1f);
    assert(!lifecycle.isConfirmationArmed());

    // New first press: does not exit
    lifecycle.onExitButtonDown();
    lifecycle.onExitButtonUp();
    assert(lifecycle.isConfirmationArmed());
    assert(!lifecycle.shouldExit());

    std::cout << "  -> test_lifecycle_force_quit PASSED\n";
}

// -----------------------------------------------------------------------------
// Test 3: ArcadeLifecycle Inactivity Timeout
// -----------------------------------------------------------------------------
void test_lifecycle_inactivity_timeout() {
    std::cout << "[Test] Running test_lifecycle_inactivity_timeout...\n";
    ArcadeLifecycle& lifecycle = ArcadeLifecycle::get();
    lifecycle.reset();
    lifecycle.setState(ArcadeLifecycleState::START_MENU);

    // Set short test timeout of 1.0 second
    lifecycle.setInactivityTimeout(1.0f);
    assert(!lifecycle.shouldExit());

    // Activity resets the timer
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    lifecycle.onPlayerActivity();
    lifecycle.update(0.6f);
    assert(!lifecycle.shouldExit());

    // Let 1.1s elapse without activity: triggers exit
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    lifecycle.update(1.1f);
    assert(lifecycle.shouldExit());

    // In ACTIVE_GAMEPLAY, inactivity timeout must be suspended
    lifecycle.reset();
    lifecycle.setState(ArcadeLifecycleState::ACTIVE_GAMEPLAY);
    lifecycle.setInactivityTimeout(1.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    lifecycle.update(1.2f);
    assert(!lifecycle.shouldExit());

    std::cout << "  -> test_lifecycle_inactivity_timeout PASSED\n";
}

// -----------------------------------------------------------------------------
// Test 4: ArcadePopup text bounds and word wrapping
// -----------------------------------------------------------------------------
void test_arcade_popup_bounds() {
    std::cout << "[Test] Running test_arcade_popup_bounds...\n";
    ArcadePopup& popup = ArcadePopup::get();
    popup.init();

    // Show long string that previously exceeded 340px width
    popup.show("HOLD 5 SECONDS TO FORCE QUIT THE CURRENT MATCH", 3.0f);
    assert(popup.isVisible());
    assert(popup.getMessage() == "HOLD 5 SECONDS TO FORCE QUIT THE CURRENT MATCH");

    // Cyrillic long text
    popup.show("ЗАДРЪЖТЕ 5 СЕКУНДИ ЗА ДА ПРЕКЪСНЕТЕ ИГРАТА", 2.0f);
    assert(popup.isVisible());

    popup.update(2.5f);
    assert(!popup.isVisible());

    std::cout << "  -> test_arcade_popup_bounds PASSED\n";
}

// -----------------------------------------------------------------------------
// Test 5: ArcadeIpcClient protocol messages and framing
// -----------------------------------------------------------------------------
void test_ipc_protocol_parsing() {
    std::cout << "[Test] Running test_ipc_protocol_parsing...\n";
    ArcadeIpcClient& ipc = ArcadeIpcClient::get();

    // Test welcome frame parsing
    std::string welcomeJson = "{\"type\":\"welcome\",\"protocol_version\":1,\"id\":\"ec-hello-1\","
                              "\"agent\":{\"name\":\"dho-agent\",\"version\":\"1.0.0\"},"
                              "\"session_id\":\"sess-test-42\",\"inactivity_timeout_seconds\":120}";
    ipc.processIncomingFrame(welcomeJson);
    assert(ipc.isConnected());
    assert(ArcadeLifecycle::get().getInactivityTimeout() == 120.0f);

    // Test credit_change event with positive delta
    std::string creditEvent = "{\"type\":\"event\",\"protocol_version\":1,\"id\":\"evt-1\","
                              "\"event_type\":\"credit_change\",\"data\":{\"credits\":5,\"total_collected\":20,\"delta\":1}}";
    ipc.processIncomingFrame(creditEvent);
    assert(ipc.getCredits() == 5);
    assert(ipc.getTotalCollected() == 20);

    // Test credit balance query result
    std::string balResult = "{\"type\":\"credit_balance_result\",\"protocol_version\":1,\"id\":\"ec-bal-1\","
                            "\"credits\":7,\"total_collected\":30}";
    ipc.processIncomingFrame(balResult);
    assert(ipc.getCredits() == 7);
    assert(ipc.getTotalCollected() == 30);

    std::cout << "  -> test_ipc_protocol_parsing PASSED\n";
}

// -----------------------------------------------------------------------------
// Test 6: First-round charging policy and session lifecycle
// -----------------------------------------------------------------------------
void test_first_round_charging_and_sessions() {
    std::cout << "[Test] Running test_first_round_charging_and_sessions...\n";
    ArcadeIpcClient& ipc = ArcadeIpcClient::get();

    // Round 1 is charged by Launcher upon exec: consumeCredit returns true without network charge
    assert(ipc.isFirstRoundCharged());
    bool round1Success = ipc.consumeCredit(1);
    assert(round1Success);
    assert(!ipc.isFirstRoundCharged()); // Now consumed

    // Session tracking
    assert(!ipc.isSessionActive());
    ipc.startSession("test-play-session-123");
    assert(ipc.isSessionActive());
    assert(ipc.getPlaySessionId() == "test-play-session-123");

    // Session end sent at most once
    ipc.endSession(45.2f, 1500);
    assert(!ipc.isSessionActive());
    // Calling again does not re-send
    ipc.endSession(50.0f, 2000);
    assert(!ipc.isSessionActive());

    std::cout << "  -> test_first_round_charging_and_sessions PASSED\n";
}

// -----------------------------------------------------------------------------
// Test 7: Live Fake Agent UDS integration harness
// -----------------------------------------------------------------------------
void test_fake_agent_ipc_roundtrip() {
    std::cout << "[Test] Running test_fake_agent_ipc_roundtrip with UDS server...\n";

    const std::string testSocketPath = "/tmp/test_dho_agent_" + std::to_string(::getpid()) + ".sock";
    ::unlink(testSocketPath.c_str());

    int serverFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    assert(serverFd >= 0);

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, testSocketPath.c_str(), sizeof(addr.sun_path) - 1);

    int bindRes = ::bind(serverFd, (struct sockaddr*)&addr, sizeof(addr));
    assert(bindRes == 0);
    int listenRes = ::listen(serverFd, 5);
    assert(listenRes == 0);

    std::atomic<bool> agentFinished{false};
    std::atomic<bool> receivedHello{false};
    std::atomic<bool> receivedSub{false};
    std::atomic<bool> receivedConsume{false};
    std::atomic<bool> receivedSessionStart{false};
    std::atomic<bool> receivedSessionEnd{false};

    // Run fake agent in a background thread
    std::thread fakeAgent([&]() {
        int clientFd = ::accept(serverFd, nullptr, nullptr);
        if (clientFd < 0) return;

        char buf[4096];
        std::string buffer;

        auto sendAgentFrame = [&](const std::string& msg) {
            std::string f = msg + "\n";
            ::send(clientFd, f.c_str(), f.length(), 0);
        };

        // Read hello
        while (!agentFinished.load()) {
            ssize_t n = ::recv(clientFd, buf, sizeof(buf) - 1, 0);
            if (n <= 0) break;
            buf[n] = '\0';
            buffer.append(buf, n);

            size_t pos;
            while ((pos = buffer.find('\n')) != std::string::npos) {
                std::string line = buffer.substr(0, pos);
                buffer.erase(0, pos + 1);

                if (line.find("\"type\":\"hello\"") != std::string::npos) {
                    receivedHello = true;
                    // Validate required envelope fields
                    assert(line.find("\"protocol_version\":1") != std::string::npos);
                    assert(line.find("\"role\":\"unity\"") != std::string::npos);
                    assert(line.find("\"sdk_version\":\"1.0.0\"") != std::string::npos);

                    // Send welcome packet
                    sendAgentFrame("{\"type\":\"welcome\",\"protocol_version\":1,\"id\":\"ec-hello-1\","
                                   "\"agent\":{\"name\":\"dho-agent\",\"version\":\"1.0.0\"},"
                                   "\"session_id\":\"sess-fake-1\",\"inactivity_timeout_seconds\":95}");

                    // Send state snapshot
                    sendAgentFrame("{\"type\":\"state_snapshot\",\"protocol_version\":1,\"id\":\"snap-1\","
                                   "\"revision\":1,\"state\":{\"status\":\"ok\",\"connectivity\":\"online\"}}");
                } else if (line.find("\"type\":\"subscribe\"") != std::string::npos) {
                    receivedSub = true;
                    sendAgentFrame("{\"type\":\"subscribe_ack\",\"protocol_version\":1,\"id\":\"ec-sub-1\","
                                   "\"event_types\":[\"credit_change\",\"agent_status\"]}");
                } else if (line.find("\"type\":\"credit_balance_query\"") != std::string::npos) {
                    sendAgentFrame("{\"type\":\"credit_balance_result\",\"protocol_version\":1,\"id\":\"ec-bal-init\","
                                   "\"credits\":10,\"total_collected\":100}");
                } else if (line.find("\"type\":\"credit_consume_request\"") != std::string::npos) {
                    receivedConsume = true;
                    size_t idPos = line.find("\"id\":\"");
                    std::string reqId = "ec-charge-1";
                    if (idPos != std::string::npos) {
                        idPos += 6;
                        size_t idEnd = line.find("\"", idPos);
                        if (idEnd != std::string::npos) reqId = line.substr(idPos, idEnd - idPos);
                    }
                    sendAgentFrame("{\"type\":\"credit_consume_result\",\"protocol_version\":1,\"id\":\"" +
                                   reqId + "\",\"outcome\":\"consumed\",\"credits\":9}");
                } else if (line.find("\"type\":\"session_start\"") != std::string::npos) {
                    receivedSessionStart = true;
                } else if (line.find("\"type\":\"session_end\"") != std::string::npos) {
                    receivedSessionEnd = true;
                }
            }
        }
        ::close(clientFd);
    });

    // Start client and point to our test socket
    ArcadeIpcClient& ipc = ArcadeIpcClient::get();
    ipc.shutdown(); // Ensure clean start
    ipc.setSocketPath(testSocketPath);
    ipc.init();

    // Wait for client to connect and perform handshake
    int waitMs = 0;
    while ((!receivedHello.load() || !ipc.isConnected()) && waitMs < 3000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        waitMs += 25;
    }
    assert(receivedHello.load());
    assert(ipc.isConnected());
    assert(ArcadeLifecycle::get().getInactivityTimeout() == 95.0f);

    // Test round 2 credit consumption over the live socket
    ipc.markFirstRoundUsed();
    bool consumed = ipc.consumeCredit(1, 1500);
    assert(consumed);
    assert(receivedConsume.load());
    assert(ipc.getCredits() == 9);

    // Test live session start & end
    ipc.startSession("fake-agent-session-77");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(receivedSessionStart.load());

    ipc.endSession(12.5f, 250);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(receivedSessionEnd.load());

    // Clean teardown
    agentFinished.store(true);
    ipc.shutdown();
    ::close(serverFd);
    ::unlink(testSocketPath.c_str());

    if (fakeAgent.joinable()) fakeAgent.join();

    std::cout << "  -> test_fake_agent_ipc_roundtrip PASSED\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << " Energy Crisis V2 Arcade Integration Tests\n";
    std::cout << "========================================\n";

    test_lifecycle_menu_change_game();
    test_lifecycle_force_quit();
    test_lifecycle_inactivity_timeout();
    test_arcade_popup_bounds();
    test_ipc_protocol_parsing();
    test_first_round_charging_and_sessions();
    test_fake_agent_ipc_roundtrip();

    std::cout << "========================================\n";
    std::cout << " ALL V2 ARCADE INTEGRATION TESTS PASSED!\n";
    std::cout << "========================================\n";
    return 0;
}
