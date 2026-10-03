#ifndef UI_MAIN_H
#define UI_MAIN_H

#include <SFML/Graphics.hpp>
#include "UI_mainMenu.h"
#include "UI_map.h"
#include "UI_shot.h"

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

public:
    explicit UI_main(const ShotOptions& shotOptions = ShotOptions());
    ~UI_main();
    int render(); // Runs until the window closes; returns the process exit code
};

#endif // UI_MAIN_H
