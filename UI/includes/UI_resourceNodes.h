#ifndef UI_RESOURCENODES_H
#define UI_RESOURCENODES_H

#include <SFML/Graphics.hpp>
#include <vector>
#include "../../Game/includes/game_main.h"

class UI_resourceNodes {
public:
    UI_resourceNodes();
    void drawNodes(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded);
    void drawLandPlots(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                       const std::vector<LandPlot>& plots, sf::Vector2f mousePos);
    void drawPlacedBuildings(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                             const std::vector<PlacedBuilding>& buildings);
    void drawBuildingGhost(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                           BuildingType type, sf::Vector2f pos, bool isValidPlacement,
                           const BuildingCost& cost);

    bool isNearP1Forest(sf::Vector2f pt) const;
    bool isNearP1Mine(sf::Vector2f pt) const;
    bool isNearP2Mine(sf::Vector2f pt) const;
    bool isNearP2Forest(sf::Vector2f pt) const;

    sf::FloatRect getP1ForestBounds() const { return sf::FloatRect({ 310.0f, 610.0f }, { 180.0f, 120.0f }); }
    sf::FloatRect getP1MineBounds() const { return sf::FloatRect({ 530.0f, 610.0f }, { 180.0f, 120.0f }); }
    sf::FloatRect getP2MineBounds() const { return sf::FloatRect({ 890.0f, 610.0f }, { 180.0f, 120.0f }); }
    sf::FloatRect getP2ForestBounds() const { return sf::FloatRect({ 1110.0f, 610.0f }, { 180.0f, 120.0f }); }
};

#endif // UI_RESOURCENODES_H
