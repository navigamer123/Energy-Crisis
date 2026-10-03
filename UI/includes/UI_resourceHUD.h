#ifndef UI_RESOURCEHUD_H
#define UI_RESOURCEHUD_H

#include <SFML/Graphics.hpp>
#include "../../Game/includes/game_main.h"

class UI_resourceHUD {
private:
    sf::FloatRect p1BuyLandBtn;
    sf::FloatRect p2BuyLandBtn;

public:
    UI_resourceHUD();
    void drawQuarterCircle(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                           const PlayerEconomy& econ, bool isWest);

    sf::FloatRect getP1BuyLandButton() const { return p1BuyLandBtn; }
    sf::FloatRect getP2BuyLandButton() const { return p2BuyLandBtn; }
};

#endif // UI_RESOURCEHUD_H
