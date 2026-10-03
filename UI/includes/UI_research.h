#ifndef UI_RESEARCH_H
#define UI_RESEARCH_H

#include <SFML/Graphics.hpp>
#include <string>
#include "UI_types.h"
#include "UI_optionsText.h"
#include "../../Game/includes/game_main.h"

// =============================================================================
// F-33 Research lab UI [team b-options]
// One laboratory building per player below the city (its windows show the researched tiers)
// and a research panel over that player's own half of the screen: 3 branch columns x 3 tiers,
// two option cards per tier ("pick 1 of 2"). Both players can have their panel open at once.
// =============================================================================
class UI_research {
public:
    UI_research();

    static sf::FloatRect labRect(int player);    // building on the map (click / cursor target)
    static sf::FloatRect panelRect(int player);  // overlay on the player's half

    bool isOpen(int player) const { return openState[idx(player)]; }
    bool anyOpen() const { return openState[0] || openState[1]; }
    void open(int player);
    void close(int player) { openState[idx(player)] = false; }
    void closeAll() { openState[0] = openState[1] = false; }
    void toggle(int player) { if (isOpen(player)) close(player); else open(player); }

    // Keyboard focus: branch column 0..2, slot 0..5 (= tier * 2 + option)
    void move(int player, int dBranch, int dSlot);
    int focusBranch(int player) const { return focusB[idx(player)]; }
    int focusTier(int player) const { return focusS[idx(player)] / TECH_OPTIONS; }
    int focusOption(int player) const { return focusS[idx(player)] % TECH_OPTIONS; }
    void setFocus(int player, int branch, int tier, int option);

    // Card under a point of that player's open panel
    bool cardAt(int player, sf::Vector2f p, int& branch, int& tier, int& option) const;

    // Feedback line at the bottom of the panel (after a research attempt)
    void flash(int player, const std::string& msg, bool ok);

    void drawLab(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded, const GameEngine& engine,
                 int player, float animTime, bool cursorOver, const std::string& keyHint, bool interactive) const;
    void drawPanel(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded, const GameEngine& engine,
                   int player, float animTime, const std::string& researchKey, const std::string& navHint);

private:
    bool openState[2] = { false, false };
    int focusB[2] = { 0, 0 };
    int focusS[2] = { 0, 0 };
    std::string message[2];
    bool messageOk[2] = { true, true };
    sf::Clock messageClock[2];
    mutable OptionsTextCache texts; // labels rebuilt only when their text changes

    static int idx(int player) { return (player == 2) ? 1 : 0; }
    static sf::FloatRect cardRect(int player, int branch, int tier, int option);
};

#endif // UI_RESEARCH_H
