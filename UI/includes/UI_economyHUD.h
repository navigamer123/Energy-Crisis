#ifndef UI_ECONOMYHUD_H
#define UI_ECONOMYHUD_H
// =============================================================================
// City economy HUD                                             [team b-economy]
//  - Grid control dashboard in the free column under the city:
//      UX-01 settlement forecast chip, BAL-03 demand curve "НУЖДА СЕГА",
//      F-37 grid-frequency gauges, F-11 energy mix + CO2 ledger, BAL-04 badges
//  - F-36 district strip in the city footer
//  - F-37 brownout / blackout banners on the player sectors
//  - UX-01 non-modal Day Report card after every settlement
// Owned by UI_map; all layout rectangles are constants at the top of UI_economyHUD.cpp.
// =============================================================================
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include "../../Game/includes/game_main.h"

class UI_economyHUD {
public:
    UI_economyHUD();

    // New match: hides the Day Report and clears the hourly history
    void reset();

    // Every frame. dt = 0 while the match is paused (the report card then waits).
    // Consumes the engine's day-cut flag to open the Day Report card.
    void update(GameEngine& engine, float dt);

    void drawDashboard(sf::RenderTarget& target, const sf::Font& font, bool fontLoaded, const GameEngine& engine,
                       float animTime, float maxBottom = 866.0f); // sections that would pass maxBottom are skipped
    void drawDistricts(sf::RenderTarget& target, const sf::Font& font, bool fontLoaded, const GameEngine& engine,
                       float animTime);
    void drawGridAlerts(sf::RenderTarget& target, const sf::Font& font, bool fontLoaded, const GameEngine& engine,
                        float animTime);
    void drawDayReport(sf::RenderTarget& target, const sf::Font& font, bool fontLoaded);
    // F-37: small "50.0 Hz" badge in the bottom-right corner of each player clock (after the grace period)
    void drawClockFrequencyBadges(sf::RenderTarget& target, const sf::Font& font, bool fontLoaded, const GameEngine& engine,
                                  float animTime);

    bool isDayReportVisible() const { return reportTimer > 0.0f; }
    // Debug / screenshot hook: shows the engine's last report as if the day had just ended
    void showLastReport(const GameEngine& engine);

private:
    // Day Report card
    float reportTimer;
    float reportAge;
    Econ::DayReport shownReport;
    int lastShownDay;

    // Per hour of today and player: covered time / observed time (demand curve coverage rows)
    float hourCovered[2][24]; // time in which the player delivered its full quota
    float hourCount[2][24];
    int historyDay;

    // Word-wrapped last settlement message (re-wrapped only when the message changes)
    std::string wrappedFor;
    std::vector<std::string> wrappedLines;

    void drawForecastChip(sf::RenderTarget& target, const sf::Font& font, const GameEngine& engine, sf::FloatRect box,
                          float animTime);
    void drawDemandCurve(sf::RenderTarget& target, const sf::Font& font, const GameEngine& engine, sf::FloatRect box);
    void drawFrequency(sf::RenderTarget& target, const sf::Font& font, const GameEngine& engine, sf::FloatRect box,
                       float animTime);
    void drawEnergyMix(sf::RenderTarget& target, const sf::Font& font, const GameEngine& engine, sf::FloatRect box);
    float drawLastMessage(sf::RenderTarget& target, const sf::Font& font, const GameEngine& engine, sf::FloatRect box);
};

#endif // UI_ECONOMYHUD_H
