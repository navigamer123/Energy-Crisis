#ifndef UI_MATCHSETUP_H
#define UI_MATCHSETUP_H

#include <SFML/Graphics.hpp>
#include "UI_types.h"
#include "../../Game/includes/game_match.h"

// =============================================================================
// Match Setup screen [team b-options]
// F-03 pacing preset + custom rule values, F-24 mutator toggles (max 3 / random),
// F-35 starting charter per player. Shown by UI_mainMenu between the mode/difficulty
// choice and the match start; the result is handed to UI_map::setMatchRules().
// =============================================================================
class UI_matchSetup {
public:
    UI_matchSetup();

    // Called every time the screen opens; keeps the previous choices
    void open(bool vsBot);

    void handleEvent(const sf::Event& event, const sf::RenderWindow& window);
    void draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded);

    bool isStartRequested() const { return requestStart; }
    bool isBackRequested() const { return requestBack; }
    void resetRequests() { requestStart = false; requestBack = false; }

    const MatchRules& getRules() const { return rules; }

private:
    enum Item {
        ITEM_PRESET = 0,
        ITEM_DAY_SECONDS,
        ITEM_GRACE,
        ITEM_FINAL_DAY,
        ITEM_VICTORY,
        ITEM_START_DEMAND,
        ITEM_GROWTH,
        ITEM_MINING,
        ITEM_MUTATOR_FIRST,                                   // 8 mutator rows
        ITEM_MUTATOR_RANDOM = ITEM_MUTATOR_FIRST + MUTATOR_COUNT,
        ITEM_CHARTER_P1,
        ITEM_CHARTER_P2,
        ITEM_START,
        ITEM_BACK,
        ITEM_COUNT
    };

    MatchRules rules;
    bool vsBot = false;
    int focus = ITEM_START;
    bool requestStart = false;
    bool requestBack = false;
    float refusedFlash = 0.0f;           // red flash when a 4th mutator is refused
    sf::Clock flashClock;
    sf::Vector2f lastMouse = { -999.0f, -999.0f };

    void adjust(int item, int dir);       // left / right on a row
    void activate(int item);              // Enter / Space / click on a row
    void randomizeMutators();

    // Layout (shared by draw and mouse handling)
    sf::FloatRect itemRect(int item) const;
    sf::FloatRect presetChipRect(int preset) const;
    sf::FloatRect arrowRect(int item, int dir) const;   // dir -1 = left arrow, +1 = right arrow
    int itemAt(sf::Vector2f p) const;

    std::string valueText(int item) const;
    std::string labelText(int item) const;
};

#endif // UI_MATCHSETUP_H
