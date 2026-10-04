#ifndef UI_MAIN_H
#define UI_MAIN_H

#include <SFML/Graphics.hpp>
#include "UI_mainMenu.h"
#include "UI_map.h"
#include "UI_shot.h"
#include "UI_input.h"  // Team b-session (F-05): gamepad menu bridge
#include "UI_postfx.h" // Team b-session (HX-15): projector picture

enum class UIState {
    MAIN_MENU,
    PLAYING,
    QUIT
};

class UI_main {
private:
    sf::RenderWindow window;
    sf::View gameView;
    UI_mainMenu mainMenu;
    UI_map map;
    UIState currentState;
    bool isFullscreen;
    ShotOptions shot; // Screenshot / layout-lint mode (--shot / --lint)

    void updateViewport();
    void toggleFullscreen();
    void setupShotScene();
    int finishShot(); // Saves the screenshot, prints the lint report; returns the exit code

    // --- Team b-session (F-02, F-05, F-06, UX-12, HX-15) ---
    GamepadMenuBridge padBridge;   // gamepad presses -> key events for menus
    UI_postfx postfx;              // projector-mode picture
    int appliedRevision = -1;      // gameSettings().revision the window was last configured for
    void applyWindowSettings(bool recreate); // size, fullscreen, frame cap, vsync from gameSettings()
    void refreshContinueInfo();    // ПРОДЪЛЖИ: match in memory or newest save on disk
    bool continueMatch();          // back into the match (or load the newest save) on the pause menu
    sf::View currentView() const;  // gameView, or the UI-scale zoom for the main menu
    GamepadMenuBridge::Mode bridgeMode() const;

public:
    explicit UI_main(const ShotOptions& shotOptions = ShotOptions());
    ~UI_main();
    int render(); // Runs until the window closes; returns the process exit code
};

#endif // UI_MAIN_H
