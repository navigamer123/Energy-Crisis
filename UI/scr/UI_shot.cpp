#include "../includes/UI_shot.h"
#include <cstdlib>
#include <string>

namespace {
bool g_shotActive = false;

const char* const MENU_SCENES[] = { "menu", "modes", "bots", "controls", "settings" };
const char* const GAME_SCENES[] = { "game", "mining", "night", "winter", "storm", "victory", "pause", "help", "modal", "tutorial" };
} // namespace

namespace ui {
namespace shot {

void setActive(bool on) { g_shotActive = on; }
bool isActive() { return g_shotActive; }

const char* sceneNames() {
    return "menu, modes, bots, controls, settings, game, mining, night, winter, storm, victory, pause, help, modal, tutorial";
}

bool isMenuScene(const std::string& scene) {
    for (const char* s : MENU_SCENES) {
        if (scene == s) return true;
    }
    return false;
}

bool isKnownScene(const std::string& scene) {
    if (isMenuScene(scene)) return true;
    for (const char* s : GAME_SCENES) {
        if (scene == s) return true;
    }
    return false;
}

void setSeedEnv(unsigned int seed) {
    std::string value = std::to_string(seed);
#ifdef _WIN32
    _putenv_s("EC_SEED", value.c_str());
#else
    setenv("EC_SEED", value.c_str(), 1);
#endif
}

} // namespace shot

sf::Vector2f pointerPos(const sf::RenderWindow& window) {
    if (g_shotActive) return { -10000.0f, -10000.0f };
    return window.mapPixelToCoords(sf::Mouse::getPosition(window));
}

} // namespace ui
