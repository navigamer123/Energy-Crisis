#ifndef UI_MAIN_H
#define UI_MAIN_H

#include "UI_mainMenu.h"
#include "UI_map.h"
#include "UI_shot.h"
#include <SFML/Graphics.hpp>

enum class UIState { MAIN_MENU, PLAYING, QUIT };

class UI_main {
private:
  sf::RenderWindow window;
  sf::View gameView;
  UI_map map;
  UIState currentState;
  bool isFullscreen;
  ShotOptions shot; // Screenshot / layout-lint mode (--shot / --lint)

  void updateViewport();
  void toggleFullscreen();
  void setupShotScene();
  int finishShot(); // Saves the screenshot, prints the lint report; returns the
  // exit code
  bool saveRecordFrame(
      int index); // --record: writes the finished frame as frame_<index>.png

public:
  UI_mainMenu mainMenu;
  explicit UI_main(const ShotOptions &shotOptions = ShotOptions());
  ~UI_main();
  void setEmulationMode(bool enabled) { emulationMode = enabled; }
  bool isEmulationMode() const { return emulationMode; }
  int render(); // Runs until the window closes; returns the process exit code

private:
  bool emulationMode = false;
};

#endif // UI_MAIN_H
