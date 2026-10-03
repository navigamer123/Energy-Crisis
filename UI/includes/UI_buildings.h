#ifndef UI_BUILDINGS_H
#define UI_BUILDINGS_H

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include "../../Game/includes/game_main.h"

struct BuildingTypeInfo {
    BuildingType type;
    std::string name;
    std::string bgName; // Bulgarian label
    int woodCost = 0;
    int ironCost = 0;
    int copperCost = 0;
    int coalCost = 0;
    int siliconCost = 0;
    int silverCost = 0;
    int oreCost = 0;
    int powerOutputMW = 0;
    int builtCount = 0;
    sf::FloatRect btnBounds;
};

class UI_buildings {
private:
    int playerIndex;
    sf::Vector2f panelPos;
    sf::Vector2f panelSize;
    sf::Color accentColor;

    std::vector<BuildingTypeInfo> buildings;

public:
    UI_buildings();
    UI_buildings(int playerIdx, sf::Vector2f pos, sf::Vector2f size, sf::Color accent);

    void setPlayer(int playerIdx, sf::Vector2f pos, sf::Vector2f size, sf::Color accent);
    BuildingType handleClick(sf::Vector2f clickPos);

    void draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
              sf::Vector2f mousePos, const PlayerEconomy& econ, BuildingType activeSelection);
};

#endif // UI_BUILDINGS_H
