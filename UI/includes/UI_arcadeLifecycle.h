#ifndef UI_ARCADELIFECYCLE_H
#define UI_ARCADELIFECYCLE_H

#include <chrono>
#include <string>
#include <functional>

enum class ArcadeLifecycleState {
    START_MENU,
    ACTIVE_GAMEPLAY,
    END_GAME_MENU,
    END_OF_GAME_PRESENTATION
};

class ArcadeLifecycle {
public:
    static ArcadeLifecycle& get();

    void setState(ArcadeLifecycleState newState);
    ArcadeLifecycleState getState() const { return currentState; }

    // Physical button 8 (CHANGE GAME / Exit) handlers
    void onExitButtonDown();
    void onExitButtonUp();

    // Player activity resets the inactivity timer (when in menus)
    void onPlayerActivity();

    // Update called every frame with delta seconds
    void update(float dt);

    // Callbacks / query
    bool shouldExit() const { return exitRequested; }
    void requestExit() { exitRequested = true; }
    void resetExit() { exitRequested = false; }

    // Inactivity timeout configuration (from welcome packet or default 120s)
    void setInactivityTimeout(float seconds) { inactivityTimeout = seconds; }
    float getInactivityTimeout() const { return inactivityTimeout; }

    // Confirmation window (2 presses in 5.0s to exit)
    bool isConfirmationArmed() const { return confirmationArmed; }
    float getConfirmationArmedElapsed() const;

    // Backward-compatible stubs for previous hold query
    bool isForceQuitHolding() const { return false; }
    float getForceQuitHeldDuration() const { return 0.0f; }

    // Reset all state (useful for deterministic tests)
    void reset();

private:
    ArcadeLifecycle();
    ~ArcadeLifecycle() = default;

    ArcadeLifecycleState currentState = ArcadeLifecycleState::START_MENU;

    // 5-second confirmation window (2 presses within 5 seconds to exit)
    bool confirmationArmed = false;
    std::chrono::steady_clock::time_point confirmationArmedTime;

    bool exitButtonPhysicallyDown = false;
    bool forceQuitHolding = false;
    std::chrono::steady_clock::time_point forceQuitStartTime;

    // Inactivity timer (120s default)
    float inactivityTimeout = 120.0f;
    std::chrono::steady_clock::time_point lastActivityTime;

    bool exitRequested = false;
};

#endif // UI_ARCADELIFECYCLE_H
