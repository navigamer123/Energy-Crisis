#include "../includes/UI_mainMenu.h"
#include <iostream>

UI_mainMenu::UI_mainMenu() {
    std::cout << "[UI_mainMenu] Main menu initialized.\n";
}

UI_mainMenu::~UI_mainMenu() {
    std::cout << "[UI_mainMenu] Main menu destroyed.\n";
}

void UI_mainMenu::render() {
    std::cout << "[UI_mainMenu] Rendering main menu...\n";
}
