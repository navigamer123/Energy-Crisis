#ifndef UI_BUILDINGS_H
#define UI_BUILDINGS_H

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include "../../Game/includes/game_main.h"

struct BuildingTypeInfo {
    BuildingType type;
    std::string shortName; // Short Bulgarian label next to the building icon
    sf::FloatRect btnBounds;
};

// Which keys pick a card directly (shown as a badge on each card)
enum class BuildHotkeys {
    NONE,    // Bot-controlled player: no badge
    DIGITS,  // Keys 1..6 (Player 1)
    NUMPAD   // Numpad 1..6 (Player 2 in co-op)
};

// One resource line of a recipe: what is needed and what the player has
struct ResourceNeed {
    ResourceType type;
    int need;
    int have;
};

// Every resource of the recipe with the player's stock, in a fixed order (wood, iron, copper,
// coal, silicon, silver); resources the building does not use are left out.
std::vector<ResourceNeed> buildingNeeds(const PlayerEconomy& econ, const BuildingCost& cost);

// Lower-case Bulgarian resource name ("желязо") for messages
const char* resourceNameBg(ResourceType type);

// Whole recipe in one style: "6 дърво, 4 желязо, 6 мед, 8 силиций"
std::string recipeText(const BuildingCost& cost);

// "Недостигат: 3 желязо, 2 мед" listing exactly what is missing; empty when the player can pay
std::string missingResourcesText(const PlayerEconomy& econ, const BuildingCost& cost);

class UI_buildings {
private:
    int playerIndex;
    sf::Vector2f panelPos;
    sf::Vector2f panelSize;
    sf::Color accentColor;
    BuildHotkeys hotkeys = BuildHotkeys::NONE;

    std::vector<BuildingTypeInfo> buildings;

    // Team b-power: second page with advanced power (UI_buildings_power.cpp)
    std::vector<BuildingTypeInfo> advancedCards;
    bool advancedPage = false;
    BuildingType lastSelection = BuildingType::NONE;
    sf::FloatRect pageTabBounds;
    const GameEngine* engineView = nullptr;
    void setupAdvancedCards();
    void syncPageWithSelection(BuildingType activeSelection);
    void drawPageTab(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded, sf::Vector2f mousePos);
    std::string advancedLockReason(const GameEngine& engine, BuildingType type) const; // empty = unlocked
    std::string advancedHotkey(BuildingType type) const;
    BuildingType handleAdvancedClick(sf::Vector2f clickPos);

public:
    UI_buildings();
    UI_buildings(int playerIdx, sf::Vector2f pos, sf::Vector2f size, sf::Color accent);

    void setPlayer(int playerIdx, sf::Vector2f pos, sf::Vector2f size, sf::Color accent);
    void setHotkeys(BuildHotkeys keys) { hotkeys = keys; }
    BuildingType handleClick(sf::Vector2f clickPos);
    // Team b-power: engine used for lock reasons (plots, day, one per player) on the advanced page
    void setEngineView(const GameEngine* engine) { engineView = engine; }
    bool isAdvancedPage() const { return advancedPage; }

    // Cards show the building icon, each cost as amount + resource icon (green when the player has
    // enough of it, red when not), the hotkey, how many the player owns and their output right now.
    void draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
              sf::Vector2f mousePos, const GameEngine& engine, BuildingType activeSelection);
};

#endif // UI_BUILDINGS_H
