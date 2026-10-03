#ifndef UI_OPTIONSTEXT_H
#define UI_OPTIONSTEXT_H

#include <SFML/Graphics.hpp>
#include <optional>
#include <string>
#include <unordered_map>

// =============================================================================
// Cached text drawing for the match-options UI [team b-options]
// Building an sf::Text (geometry + glyph lookups) is the most expensive part of a frame, so
// every label keeps its sf::Text in a numbered slot and is rebuilt only when its string, size
// or width limit changes. Colour and position updates are cheap. (Integrator: can be folded
// into Wave A's UI_text helper / layout lint.)
// =============================================================================
class OptionsTextCache {
public:
    enum Align { LEFT = 0, CENTER = 1, RIGHT = 2 };

    // Text for a slot; shrinks the size (down to minSize) until it fits maxWidth
    sf::Text& get(int slot, const sf::Font& font, const std::string& utf8, unsigned int size,
                  float maxWidth = 100000.0f, unsigned int minSize = 9);

    // Draws a slot's text: x by alignment, vertically centred on the caps of its size
    void draw(sf::RenderWindow& window, int slot, const sf::Font& font, const std::string& utf8, unsigned int size,
              sf::Color color, float x, float centerY, float maxWidth = 100000.0f, Align align = LEFT,
              unsigned int minSize = 9);

    // Bounds of plain capitals ("НЕ") at a size, for a common baseline across labels
    static sf::FloatRect capsBounds(const sf::Font& font, unsigned int size);

private:
    struct Entry {
        std::string str;
        unsigned int requestedSize = 0;
        float maxWidth = 0.0f;
        std::optional<sf::Text> text;
    };
    std::unordered_map<int, Entry> entries;
};

#endif // UI_OPTIONSTEXT_H
