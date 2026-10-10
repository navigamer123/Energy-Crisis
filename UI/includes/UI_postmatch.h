#ifndef UI_POSTMATCH_H
#define UI_POSTMATCH_H

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include "UI_matchStats.h"

// -----------------------------------------------------------------------------
// team info: post-match report (F-04) shown instead of the old victory box.
// Tabs: ОБОБЩЕНИЕ (awards + P1/P2 table), ГРАФИКИ (share, daily delivery vs demand,
// power over the match), ЕНЕРГИЕН МИКС (energy by source, CO2 avoided).
// -----------------------------------------------------------------------------
class UI_postmatch {
public:
    enum Tab { SUMMARY = 0, CHARTS = 1, MIX = 2, TAB_COUNT = 3 };

    void reset() { tab = SUMMARY; }

    // Draws the whole report and publishes the two button rectangles for click handling
    void draw(sf::RenderTarget& target, const sf::Font& font, bool fontLoaded, const GameEngine& engine,
              const UI_matchStats& stats, sf::Vector2f mousePos, sf::FloatRect& restartBtn, sf::FloatRect& menuBtn,
              float finishedTimer = 999.0f);

    bool handleKey(sf::Keyboard::Key key);     // 1/2/3, Left/Right, Tab, Q/E switch the tab
    bool handleJoystick(int button);           // LB/RB/X/Y switch the tab
    bool handleJoystickAxis(sf::Joystick::Axis axis, float position); // Stick / D-pad X switches tab
    bool handleClick(sf::Vector2f pos);        // click on a tab header

    struct Award {
        std::string title;
        int player = 0;
        std::string detail;
        float score = 0.0f;
    };
    static std::vector<Award> pickAwards(const UI_matchStats& stats, const GameEngine& engine, bool isEn = false);

private:
    void drawSummary(sf::RenderTarget& t, const sf::Font& f, const GameEngine& engine, const UI_matchStats& stats,
                     sf::FloatRect area, bool isEn = false) const;
    void drawCharts(sf::RenderTarget& t, const sf::Font& f, const GameEngine& engine, const UI_matchStats& stats,
                    sf::FloatRect area, bool isEn = false) const;
    void drawMix(sf::RenderTarget& t, const sf::Font& f, const UI_matchStats& stats, sf::FloatRect area,
                 bool isEn = false) const;

    int tab = SUMMARY;
    sf::FloatRect tabRects[TAB_COUNT];
    float lastJoyAxisX = 0.0f;
};

#endif // UI_POSTMATCH_H
