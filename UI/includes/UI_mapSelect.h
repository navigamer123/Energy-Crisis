#ifndef UI_MAPSELECT_H
#define UI_MAPSELECT_H

// =============================================================================
// Team b-power (F-39): map layout selector with a live mini-map preview.
// Shown on the mode-select screen of the main menu: [<]/[>] (A/D) change the
// layout, [R] rolls a new seed for the generated map. The integrator can move it
// into the Match Setup screen (MatchConfig) unchanged.
// =============================================================================

#include <SFML/Graphics.hpp>
#include "../../Game/includes/game_main.h"

class UI_mapSelect {
public:
    UI_mapSelect();

    // Draws the selector centred horizontally with its top edge at `top` (about 230 px tall)
    void draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded, float top, sf::Vector2f mousePos);
    bool handleKey(sf::Keyboard::Key code);   // true when the key was used
    bool handleClick(sf::Vector2f pos);       // true when the click was used (arrows / reroll)

    MapPreset getPreset() const { return static_cast<MapPreset>(presetIdx); }
    // Seed of the previewed map (0 when EC_SEED is set: the map then follows the replayable match seed)
    unsigned getSeed() const;
    void reroll();                            // new random seed (also called when returning to the menu)

private:
    int presetIdx = 0;
    unsigned seed = 1;
    MapLayout preview;
    sf::FloatRect prevBtn;
    sf::FloatRect nextBtn;
    sf::FloatRect rerollBtn;

    void step(int delta);
    void rebuildPreview();
};

#endif // UI_MAPSELECT_H
