#ifndef UI_SHOT_H
#define UI_SHOT_H

#include <SFML/Graphics.hpp>
#include <string>

// -----------------------------------------------------------------------------
// Screenshot / layout-lint mode for automated visual checks:
//   energy_crisis.exe --shot <out.png> [--scene NAME] [--frames N] [--seed S] [--lint]
// renders N frames of one scene, saves the window to a PNG and exits. --lint also
// prints every text layout problem of the last frame; the exit code is their count.
// -----------------------------------------------------------------------------
struct ShotOptions {
    bool enabled = false;       // --shot or --lint given
    std::string outPath;        // PNG file to write (empty: no screenshot, lint only)
    std::string scene = "game"; // see ui::shot::sceneNames()
    int frames = 90;            // frames rendered before the capture
    unsigned int seed = 1;      // match seed (EC_SEED), fixed so captures are reproducible
    bool lint = false;          // print layout problems, exit code = number of problems
};

namespace ui {

namespace shot {
// True while the game runs in screenshot mode: real mouse/keyboard input is ignored.
void setActive(bool on);
bool isActive();
// Comma separated list of the scene names --scene accepts.
const char* sceneNames();
bool isMenuScene(const std::string& scene);
bool isKnownScene(const std::string& scene);
// Sets EC_SEED so the next GameEngine::init() replays the same match.
void setSeedEnv(unsigned int seed);
} // namespace shot

// Pointer position on the 1600x900 canvas. In screenshot mode the real mouse is ignored and a
// point far off the canvas is returned, so nothing is hovered and captures do not depend on it.
sf::Vector2f pointerPos(const sf::RenderWindow& window);

} // namespace ui

#endif // UI_SHOT_H
