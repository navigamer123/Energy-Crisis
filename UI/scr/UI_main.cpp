#include "../includes/UI_main.h"
#include <iostream>

UI_main::UI_main() { std::cout << "[UI_main] Main UI initialized.\n"; }

UI_main::~UI_main() { std::cout << "[UI_main] Main UI destroyed.\n"; }

void UI_main::render() {
  std::cout << "[UI_main] Rendering main UI interface...\n";
  mainMenu.render();
}
