#include "../includes/UI_map.h"
#include <iostream>

UI_map::UI_map() {
    std::cout << "[UI_map] Map view initialized.\n";
}

UI_map::~UI_map() {
    std::cout << "[UI_map] Map view destroyed.\n";
}

void UI_map::render() {
    std::cout << "[UI_map] Rendering map component...\n";
}
