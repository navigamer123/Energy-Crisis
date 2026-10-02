#ifndef UI_CITY_H
#define UI_CITY_H

#include <SFML/Graphics.hpp>
#include <string>

class UI_city {
public:
    UI_city();
    void drawCity(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded, float animTime,
                  float p1Share, const std::string& cutMessage);
    void drawDividingRiver(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded, float animTime);
    void drawInfluenceBar(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                          int demand, int p1Energy, int p2Energy, float p1Share);

    sf::FloatRect getCityBounds() const { return sf::FloatRect({ 610.0f, 65.0f }, { 380.0f, 330.0f }); }
};

#endif // UI_CITY_H
