#ifndef UI_MAIN_H
#define UI_MAIN_H

#include <SFML/Graphics.hpp>
#include "UI_mainMenu.h"
#include "UI_map.h"
#include "UI_demo.h" // [Team Demo / HX-02]

enum class UIState {
    MAIN_MENU,
    PLAYING,
    DEMO, // [Team Demo / HX-02] judge demo: scripted showcase on the map, any key returns to the menu
    QUIT
};

class UI_main {
private:
    sf::RenderWindow window;
    sf::View gameView;
    UI_mainMenu mainMenu;
    UI_map map;
    UI_demo demo; // [Team Demo / HX-02]
    UIState currentState;
    bool isFullscreen;

    void updateViewport();
    void toggleFullscreen();

public:
    UI_main();
    ~UI_main();
    void render();
    void startDemo(); // [Team Demo / HX-02] judge demo (F9 in the main menu, --demo flag)
};

#endif // UI_MAIN_H