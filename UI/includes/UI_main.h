#ifndef UI_MAIN_H
#define UI_MAIN_H

#include <SFML/Graphics.hpp>
#include "UI_mainMenu.h"
#include "UI_map.h"
#include "UI_intro.h" // [b-showcase] HX-01 cinematic intro

enum class UIState {
    INTRO,     // [b-showcase] HX-01: cinematic intro before the main menu
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
    UI_intro intro; // [b-showcase]
    UIState currentState;
    bool isFullscreen;

    void updateViewport();
    void toggleFullscreen();

public:
    UI_main();
    ~UI_main();
    void render();
};

#endif // UI_MAIN_H