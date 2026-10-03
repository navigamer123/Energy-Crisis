#ifndef UI_SKYLINE_H
#define UI_SKYLINE_H

// =============================================================================
// [b-showcase] Living skyline (HX-03) and blackout set piece (HX-04)
//
// Owned by UI_city. UI_city::drawCity calls the draw*Layer hooks around its own towers and
// windowLit() for every window it draws; UI_map::render calls update(), drawWorldDim() and
// drawBlackoutBanner(). Timing and geometry live in UI_showcaseModel.h (headless-tested).
// =============================================================================

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include "../../Game/includes/game_main.h"

class UI_skyline {
public:
    UI_skyline();

    // New match: forget every tower animation, district state and running set piece
    void reset();

    // Follows the engine: new days start tower construction, settlements dim the districts of a
    // player who missed the demand and start the blackout set piece. Call once per unpaused frame.
    void update(float dt, const GameEngine& engine);

    // --- Hooks for UI_city::drawCity (virtual 1600x900 coordinates) ---
    // Back row of towers, drawn after the city platform and before the front towers
    void drawBackLayer(sf::RenderTarget& target, float captureX, bool isDaylight, float animTime) const;
    // Extra floors, foreground blocks and cranes, drawn after the front towers
    void drawFrontLayer(sf::RenderTarget& target, float captureX, bool isDaylight, float animTime) const;
    // Brownout veil and blackout darkness over the failing districts
    void drawCityOverlay(sf::RenderTarget& target, float captureX, float animTime) const;
    // Final lit/dark state of one city window (brownout of its district, blackout cascade)
    bool windowLit(bool lit, float winX, float winY, float towerTopY, float towerBottomY,
                   bool winInP1, int hash, float captureX) const;

    // --- Hooks for UI_map::render ---
    // Dims the world layer while the set piece runs (draw after the city, before the HUD)
    void drawWorldDim(sf::RenderTarget& target) const;
    // "АВАРИЯ В ЕНЕРГОСИСТЕМАТА" banner over the city (draw above the HUD, below pause/victory)
    void drawBlackoutBanner(sf::RenderTarget& target, const sf::Font& font, bool fontLoaded, float animTime) const;

    // --- State ---
    bool isBlackoutActive() const { return blackoutActive; }
    float getBlackoutTime() const { return blackoutT; }
    bool isDistrictBrownout(int player) const { return player == 1 ? p1Brownout : p2Brownout; }
    // Starts the set piece by hand (screenshots, demo mode); update() calls it on a failed day
    void triggerBlackout(bool p1Failed, bool p2Failed, int demandMW, int p1AvgMW, int p2AvgMW);
    // Screenshot helper: jump inside a running set piece
    void setBlackoutTime(float t) { blackoutT = t; }
    // Audio hook: returns true and the cue name ("siren") once per requested sound. There is no
    // audio system on this branch; the integrator forwards these cues to the audio director.
    bool takeSoundCue(std::string& outCue);

private:
    std::vector<float> riseSeconds; // Per skyline plan entry: seconds since construction began (<0 = not yet)
    int seenDay = 0;                // 0 = not initialised for this match
    int seenSerial = 0;
    bool p1Brownout = false;
    bool p2Brownout = false;

    bool blackoutActive = false;
    float blackoutT = 0.0f;
    bool boP1Failed = false;
    bool boP2Failed = false;
    int boDemandMW = 0;
    int boP1MW = 0;
    int boP2MW = 0;

    std::vector<std::string> soundCues;

    void syncToDay(int day, bool animateToday);
    void drawPlanLayer(sf::RenderTarget& target, int layer, float captureX, bool isDaylight, float animTime) const;
};

#endif // UI_SKYLINE_H
