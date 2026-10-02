#ifndef UI_MAIN_H
#define UI_MAIN_H

#include "UI_mainMenu.h"
#include "UI_map.h"
#include <iostream>

class UI_main {
private:
  UI_mainMenu mainMenu;
  UI_map map;

public:
  UI_main();
  ~UI_main();
  void render();
};

#endif