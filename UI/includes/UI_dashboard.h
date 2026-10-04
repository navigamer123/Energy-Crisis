#ifndef UI_DASHBOARD_H
#define UI_DASHBOARD_H

#include <SFML/Graphics.hpp>
#include "UI_matchStats.h"

// -----------------------------------------------------------------------------
// team info: real-time energy dashboard (HX-06), shown while [Tab] is held.
// Live charts of each player's MW against the city demand, the city share
// timeline, CO2 avoided and the current energy mix, plus headline numbers.
// -----------------------------------------------------------------------------
class UI_dashboard {
public:
    static constexpr float WINDOW_HOURS = 48.0f; // Time span of the live MW chart

    // openAge: seconds since Tab was pressed (drives the reveal animation)
    void draw(sf::RenderTarget& target, const sf::Font& font, const GameEngine& engine,
              const UI_matchStats& stats, float openAge, float animTime) const;
};

#endif // UI_DASHBOARD_H
