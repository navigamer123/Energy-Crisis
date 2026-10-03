#ifndef UI_CITY_H
#define UI_CITY_H

#include <SFML/Graphics.hpp>
#include <string>
#include "../../Game/includes/game_weather.h"

class UI_city {
public:
    UI_city();
    void drawCity(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded, float animTime,
                  float p1Share, const std::string& cutMessage, bool isDaylight = true,
                  float currentHour = 12.0f, SeasonType season = SeasonType::SPRING);
    void drawDividingRiver(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded, float animTime,
                           bool isDaylight = true);
    void drawInfluenceBar(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                          int demand, int p1Energy, int p2Energy, float p1Share, int currentDay = 1);

    sf::FloatRect getCityBounds() const { return sf::FloatRect({ 610.0f, 65.0f }, { 380.0f, 330.0f }); }
};

#endif // UI_CITY_H
