#ifndef UI_DEMO_H
#define UI_DEMO_H

// =============================================================================
// ENERGY CRISIS - JUDGE DEMO MODE (HX-02), UI side                 [Team Demo]
// F9 in the main menu (or the --demo flag) plays the seeded beat script of
// Game/includes/game_demo.h on the real map: the DemoDirector steps the engine
// with a fixed tick, this class turns the director's cues into map effects
// (cursor flights, sparks, lightning, notices) and draws the big captions.
// Any key, mouse button or gamepad button (or Esc) returns to the main menu.
//
// Rehearsal helpers (environment variables):
//   EC_DEMO_SPEED=4        play the show 4x faster (up to 30 ticks per frame)
//   EC_DEMO_SHOTS=<dir>    save a PNG of every caption (slides for the presentation)
// =============================================================================

#include <SFML/Graphics.hpp>
#include <string>
#include "../../Game/includes/game_demo.h"

class UI_map;

class UI_demo {
public:
    UI_demo();

    void start(UI_map& map);   // fresh seeded demo match on the map
    void stop(UI_map& map);    // hand the map back to normal play
    bool isActive() const { return active; }

    void handleEvent(const sf::Event& event);
    bool isExitRequested() const { return exitRequested; }

    void update(UI_map& map);                                   // call before map.render()
    void drawOverlay(sf::RenderWindow& window, const UI_map& map); // call after map.render()
    void drawMenuHint(sf::RenderWindow& window, const UI_map& map) const; // "F9" hint in the main menu

    Demo::DemoDirector& getDirector() { return director; }

private:
    void applyCue(UI_map& map, const Demo::DemoCue& cue);
    void animateMap(UI_map& map, float dt);
    void drawCaptionPanel(sf::RenderWindow& window, const sf::Font& font, float appear);
    void drawTitleCard(sf::RenderWindow& window, const sf::Font& font, float appear);
    void drawProgress(sf::RenderWindow& window, const sf::Font& font, float y);
    void captureShotIfDue(sf::RenderWindow& window);

    Demo::DemoDirector director;
    bool active = false;
    bool exitRequested = false;
    sf::Clock frameClock;
    float realTime = 0.0f;        // real seconds since the demo started (input guard, animations)
    float speed = 1.0f;           // EC_DEMO_SPEED
    int shownSerial = -1;         // caption serial currently animated in
    float captionShownAt = 0.0f;  // realTime when that caption appeared

    sf::Vector2f cursorTarget[3];
    int statFrames = 0;           // console FPS log
    float statTime = 0.0f;
    bool finishLogged = false;

    std::string shotDir;          // EC_DEMO_SHOTS
    int lastShotSerial = -1;
    bool finalShotTaken = false;
};

#endif // UI_DEMO_H
