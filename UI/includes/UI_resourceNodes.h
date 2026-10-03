#ifndef UI_RESOURCENODES_H
#define UI_RESOURCENODES_H

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include "../../Game/includes/game_main.h"

struct ResourceStation {
    ResourceType type;
    int playerOwner; // 1 or 2
    sf::FloatRect bounds;
    std::string nameBg;
    std::string yieldStr;
    sf::Color themeColor;
    sf::FloatRect upgradeBtnBounds;
};

class UI_resourceNodes {
private:
    std::vector<ResourceStation> stations;

public:
    UI_resourceNodes();

    void drawNodes(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                   const GameEngine* engine = nullptr,
                   float p1Cooldown = 0.0f, float p2Cooldown = 0.0f);
    void drawLandPlots(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                       const std::vector<LandPlot>& plots, sf::Vector2f mousePos,
                       const std::vector<PlacedBuilding>& buildings = {});
    void drawPlacedBuildings(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                             const std::vector<PlacedBuilding>& buildings);
    void drawBuildingGhost(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                           BuildingType type, sf::Vector2f pos, bool isValidPlacement,
                           const BuildingCost& cost);
    // Name, cost and keys of the ghost in a tooltip panel; drawn after the city so nothing covers it
    void drawBuildingGhostInfo(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                               BuildingType type, sf::Vector2f pos, bool isValidPlacement,
                               const BuildingCost& cost);

    ResourceType getP1ResourceAt(sf::Vector2f pt) const;
    ResourceType getP2ResourceAt(sf::Vector2f pt) const;
    ResourceType getP1UpgradeAt(sf::Vector2f pt) const;
    ResourceType getP2UpgradeAt(sf::Vector2f pt) const;
    ResourceType getP1StationAt(sf::Vector2f pt) const;
    ResourceType getP2StationAt(sf::Vector2f pt) const;
    const ResourceStation* getStation(int player, ResourceType type) const;

    // Backward compatibility helpers
    bool isNearP1Forest(sf::Vector2f pt) const;
    bool isNearP1Mine(sf::Vector2f pt) const;
    bool isNearP2Mine(sf::Vector2f pt) const;
    bool isNearP2Forest(sf::Vector2f pt) const;

    sf::FloatRect getP1ForestBounds() const { return sf::FloatRect({ 240.0f, 575.0f }, { 100.0f, 90.0f }); }
    sf::FloatRect getP1MineBounds() const { return sf::FloatRect({ 330.0f, 575.0f }, { 280.0f, 180.0f }); }
    sf::FloatRect getP2MineBounds() const { return sf::FloatRect({ 995.0f, 575.0f }, { 280.0f, 180.0f }); }
    sf::FloatRect getP2ForestBounds() const { return sf::FloatRect({ 1270.0f, 575.0f }, { 100.0f, 90.0f }); }
};

#endif // UI_RESOURCENODES_H
