#ifndef UI_SANDBOX_H
#define UI_SANDBOX_H

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include "UI_types.h"
#include "UI_optionsText.h"
#include "../../Game/includes/game_main.h"

// =============================================================================
// F-21 Practice sandbox control panel [team b-options]
// Drawn over the idle East sector: clock (jump / pause / 1x-16x), weather, season, city demand,
// clear buildings / buy all land, and a projected 24 h output curve against the demand.
// Mouse buttons plus F2 (show/hide) and F3..F8 shortcuts.
// =============================================================================
class UI_sandboxPanel {
public:
    UI_sandboxPanel();

    static sf::FloatRect panelRect();

    bool isVisible() const { return visible; }
    void setVisible(bool v) { visible = v; }
    void toggle() { visible = !visible; }

    // Both return true when the input was used (the panel then also shows a feedback line)
    bool handleClick(sf::Vector2f p, GameEngine& engine);
    bool handleKey(sf::Keyboard::Key key, GameEngine& engine);

    void draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded, const GameEngine& engine, float animTime);

private:
    enum Action { HOUR_DELTA, HOUR_SET, SPEED, WEATHER, SEASON, DEMAND_DELTA, CLEAR, BUY_LAND };
    struct Button {
        sf::FloatRect rect;
        Action action;
        int value;
        std::string label;
    };
    std::vector<Button> buttons;
    bool visible = true;
    std::string feedback;
    sf::Clock feedbackClock;
    OptionsTextCache texts;

    void apply(Action action, int value, GameEngine& engine);
    bool isActive(const Button& b, const GameEngine& engine) const;
};

#endif // UI_SANDBOX_H
