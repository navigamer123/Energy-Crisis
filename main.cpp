#include <iostream>
#include "UI/includes/UI_main.h"

int main() {
    std::cout << "========================================\n";
    std::cout << "    Energy Crisis - Game UI Test        \n";
    std::cout << "========================================\n";

    std::cout << "[Main] Initializing UI_main...\n";
    UI_main ui;

    std::cout << "[Main] Calling main UI function render()...\n";
    ui.render();

    std::cout << "[Main] UI test completed successfully.\n";
    return 0;
}
