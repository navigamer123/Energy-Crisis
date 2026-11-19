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

    void drawNodes(sf::RenderTarget& window, const sf::Font& font, bool fontLoaded,
                   const GameEngine* engine = nullptr,
                   const float* p1Cooldowns = nullptr, const float* p2Cooldowns = nullptr,
                   float p1Cooldown = 0.0f, float p2Cooldown = 0.0f);
    void drawLandPlots(sf::RenderTarget& window, const sf::Font& font, bool fontLoaded,
                       const std::vector<LandPlot>& plots, sf::Vector2f mousePos,
                       const std::vector<PlacedBuilding>& buildings = {});
    void drawPlacedBuildings(sf::RenderTarget& window, const sf::Font& font, bool fontLoaded,
                             const std::vector<PlacedBuilding>& buildings);
    void drawBuildingGhost(sf::RenderTarget& window, const sf::Font& font, bool fontLoaded,
                           BuildingType type, sf::Vector2f pos, bool isValidPlacement,
                           const BuildingCost& cost, sf::Color overrideColor = sf::Color::Transparent);
    // Name, cost and keys of the ghost in a tooltip panel; drawn after the city so nothing covers it
    // (missing = "Недостигат: ..." replaces the cost line when the player cannot pay)
    void drawBuildingGhostInfo(sf::RenderTarget& window, const sf::Font& font, bool fontLoaded,
                               BuildingType type, sf::Vector2f pos, bool isValidPlacement,
                               const BuildingCost& cost, const std::string& missing = std::string());

    ResourceType getP1ResourceAt(sf::Vector2f pt) const;
    ResourceType getP2ResourceAt(sf::Vector2f pt) const;
    ResourceType getP1UpgradeAt(sf::Vector2f pt) const;
    ResourceType getP2UpgradeAt(sf::Vector2f pt) const;
    ResourceType getP1StationAt(sf::Vector2f pt) const;
    ResourceType getP2StationAt(sf::Vector2f pt) const;
    const ResourceStation* getStation(int player, ResourceType type) const;
    // Amount one mining action gives for a mine of this type at this level
    static int mineYield(ResourceType type, int level);

    // Backward compatibility helpers
    bool isNearP1Forest(sf::Vector2f pt) const;
    bool isNearP1Mine(sf::Vector2f pt) const;
    bool isNearP2Mine(sf::Vector2f pt) const;
    bool isNearP2Forest(sf::Vector2f pt) const;

    sf::FloatRect getP1ForestBounds() const { return sf::FloatRect({ 232.0f, 538.0f }, { 110.0f, 110.0f }); }
    sf::FloatRect getP1MineBounds() const { return sf::FloatRect({ 232.0f, 538.0f }, { 436.0f, 222.0f }); }
    sf::FloatRect getP2MineBounds() const { return sf::FloatRect({ 932.0f, 538.0f }, { 436.0f, 222.0f }); }
    sf::FloatRect getP2ForestBounds() const { return sf::FloatRect({ 1258.0f, 538.0f }, { 110.0f, 110.0f }); }
};

#endif // UI_RESOURCENODES_H
