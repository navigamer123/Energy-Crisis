#include "../includes/UI_arcadeLifecycle.h"
#include "../includes/UI_arcadePopup.h"
#include "../includes/UI_settings.h"
#include <iostream>

ArcadeLifecycle& ArcadeLifecycle::get() {
    static ArcadeLifecycle instance;
    return instance;
}

ArcadeLifecycle::ArcadeLifecycle() {
    reset();
}

void ArcadeLifecycle::reset() {
    currentState = ArcadeLifecycleState::START_MENU;
    confirmationArmed = false;
    exitButtonPhysicallyDown = false;
    forceQuitHolding = false;
    exitRequested = false;
    inactivityTimeout = 120.0f;
    lastActivityTime = std::chrono::steady_clock::now();
}

void ArcadeLifecycle::setState(ArcadeLifecycleState newState) {
    if (currentState != newState) {
        currentState = newState;
        // State transition disarms confirmation
        confirmationArmed = false;
        forceQuitHolding = false;
        lastActivityTime = std::chrono::steady_clock::now();
    }
}

void ArcadeLifecycle::onExitButtonDown() {
    exitButtonPhysicallyDown = true;
    auto now = std::chrono::steady_clock::now();

    if (currentState == ArcadeLifecycleState::END_OF_GAME_PRESENTATION) {
        // Protected presentation: normal controls suppressed
        return;
    }

    if (confirmationArmed) {
        float elapsed = std::chrono::duration<float>(now - confirmationArmedTime).count();
        if (elapsed <= 5.0f) {
            std::cout << "[ArcadeLifecycle] Exit confirmed (2 presses within 5.0s). Exiting...\n";
            exitRequested = true;
            confirmationArmed = false;
            return;
        }
    }

    // First press or expired: arm confirmation window for 5.0 seconds
    confirmationArmed = true;
    confirmationArmedTime = now;
    std::cout << "[ArcadeLifecycle] Exit button pressed (1/2). Press again within 5.0s to exit.\n";
    bool isEn = (UI_settings::get().getLanguage() == "en");
    std::string msg = isEn ? "Press EXIT again to exit." : "Натисни отново за изход.";
    ArcadePopup::get().show(msg, 5.0f);
}

void ArcadeLifecycle::onExitButtonUp() {
    exitButtonPhysicallyDown = false;
}

void ArcadeLifecycle::onPlayerActivity() {
    lastActivityTime = std::chrono::steady_clock::now();
}

float ArcadeLifecycle::getConfirmationArmedElapsed() const {
    if (!confirmationArmed) return 0.0f;
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(now - confirmationArmedTime).count();
}

void ArcadeLifecycle::update(float dt) {
    (void)dt;
    auto now = std::chrono::steady_clock::now();

    // 1. Expiration of 5-second confirmation window
    if (confirmationArmed) {
        float elapsed = std::chrono::duration<float>(now - confirmationArmedTime).count();
        if (elapsed > 5.0f) {
            confirmationArmed = false;
        }
    }

    // 2. Inactivity Timeout (only in StartMenu and EndGameMenu)
    if (currentState == ArcadeLifecycleState::START_MENU || currentState == ArcadeLifecycleState::END_GAME_MENU) {
        float idleSeconds = std::chrono::duration<float>(now - lastActivityTime).count();
        if (idleSeconds >= inactivityTimeout && inactivityTimeout > 0.0f) {
            std::cout << "[ArcadeLifecycle] Inactivity timeout (" << inactivityTimeout
                      << "s) reached without player activity. Exiting to launcher...\n";
            exitRequested = true;
        }
    }
}
