#ifndef UI_DEV_OVERLAY_H
#define UI_DEV_OVERLAY_H

#include <SFML/Graphics.hpp>
#include <array>
#include <cstddef>
#include "../../Game/includes/game_main.h"

// -----------------------------------------------------------------------------
// team info: developer overlay (CD-15). [F3] toggles it. While it is visible,
// [F6] / [F7] halve / double a developer time multiplier (x1 .. x16) and [F8]
// resets it; hiding the overlay always returns to x1.
// -----------------------------------------------------------------------------
class UI_devOverlay {
public:
    struct Counts {
        int buildingsP1 = 0;
        int buildingsP2 = 0;
        int weatherParticles = 0;
        int miningParticles = 0;
        int notices = 0;
        int lightnings = 0;
        int toasts = 0;
        std::size_t samples = 0;
        std::size_t logEntries = 0;
    };

    void toggle();
    bool isVisible() const { return visible; }
    bool handleKey(sf::Keyboard::Key key);  // F6 / F7 / F8 while visible
    float timeMultiplier() const { return visible ? MULTIPLIERS[multIdx] : 1.0f; }
    void recordFrame(float rawDt);
    void draw(sf::RenderTarget& target, const sf::Font& font, const GameEngine& engine, const Counts& counts,
              bool paused) const;

private:
    static constexpr std::size_t HISTORY = 120;
    static constexpr float MULTIPLIERS[5] = { 1.0f, 2.0f, 4.0f, 8.0f, 16.0f };

    bool visible = false;
    int multIdx = 0;
    std::array<float, HISTORY> frameTimes{};
    std::size_t frameIdx = 0;
    std::size_t frameCount = 0;
};

#endif // UI_DEV_OVERLAY_H
