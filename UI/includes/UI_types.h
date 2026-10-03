#ifndef UI_TYPES_H
#define UI_TYPES_H

#include <SFML/Graphics.hpp>
#include <string>

// Virtual Canvas Resolution (16:9)
constexpr float VIRTUAL_WIDTH = 1600.0f;
constexpr float VIRTUAL_HEIGHT = 900.0f;

// -----------------------------------------------------------------------------
// UTF-8 Helper to ensure Cyrillic / Bulgarian text renders properly in SFML
// -----------------------------------------------------------------------------
inline sf::String toUtf8(const std::string& str) {
    return sf::String::fromUtf8(str.begin(), str.end());
}

inline sf::String toUtf8(const char* str) {
    std::string s(str);
    return sf::String::fromUtf8(s.begin(), s.end());
}

// -----------------------------------------------------------------------------
// Control Schemes for 2-Player Co-op
// -----------------------------------------------------------------------------
enum class ControlScheme {
    BOTH_KEYBOARD,          // 1. P1: WASD + Q/E, P2: Arrows + PgUp/PgDn
    P1_KEYBOARD_P2_MOUSE,   // 2. P1: WASD + Q/E, P2: Mouse
    P1_MOUSE_P2_KEYBOARD,   // 3. P1: Mouse, P2: Arrows + PgUp/PgDn
    BOTH_MOUSE              // 4. P1: Mouse, P2: Mouse (2 Mice)
};

// -----------------------------------------------------------------------------
// Bot Difficulty for Single Player Mode
// -----------------------------------------------------------------------------
enum class BotDifficulty {
    NONE,    // Co-op 2-player local (Human vs Human)
    EASY,    // Slower AI, basic power plants
    MEDIUM,  // Balanced AI, builds batteries and expands
    HARD     // Fast aggressive AI, optimizes power & upgrades
};

// -----------------------------------------------------------------------------
// Floating FloatingNotice / Notification Particle
// -----------------------------------------------------------------------------
struct FloatingNotice {
    std::string text;
    sf::Vector2f pos;
    float timer;
    float maxTimer;
    sf::Color color;
};

#endif // UI_TYPES_H
