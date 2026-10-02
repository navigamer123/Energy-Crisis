#ifndef UI_MAIN_H
#define UI_MAIN_H

#include <SFML/Graphics.hpp>
#include "UI_mainMenu.h"
#include "UI_map.h"

enum class UIState {
    MAIN_MENU,
    PLAYING,
    QUIT
};

class UI_main {
private:
    sf::RenderWindow window;
    UI_mainMenu mainMenu;
    UI_map map;
    UIState currentState;

public:
    UI_main();
    ~UI_main();
    void render();
};

#endif // UI_MAIN_H