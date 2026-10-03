#ifndef UI_TEXT_H
#define UI_TEXT_H

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// Text helper: every sf::Text of the UI is drawn through ui::drawText.
//
// In normal play drawText is just target.draw(text). With the layout lint on
// (energy_crisis.exe --lint) it also records each drawn text's global bounds
// and the container it has to stay inside, so the last frame can be checked
// for: text overlapping other text, text leaving its container, text outside
// the 1600x900 canvas, empty strings or characters missing from the font, text
// smaller than the minimum size (fontsize::Caption) and text touching an icon.
// -----------------------------------------------------------------------------
namespace ui {

// Builds a text from a UTF-8 string (Cyrillic safe) with colour and position.
sf::Text makeText(const sf::Font& font, const std::string& utf8, unsigned int size,
                  sf::Color color = sf::Color::White, sf::Vector2f pos = { 0.0f, 0.0f });

// Width in px of a single line of UTF-8 text (same layout rules as sf::Text).
float measureText(const sf::Font& font, const std::string& utf8, unsigned int size, bool bold = false);

// Inserts line breaks so that no line is wider than maxWidth (existing line breaks are kept;
// a single word wider than maxWidth stays on its own line).
std::string wrapText(const sf::Font& font, const std::string& utf8, unsigned int size, float maxWidth,
                     bool bold = false);

// Largest character size from `size` down to `minSize` at which the text fits maxWidth.
unsigned int fitTextSize(const sf::Font& font, const std::string& utf8, unsigned int size, unsigned int minSize,
                         float maxWidth, bool bold = false);

// Draws the text. The container (if any) is the rectangle the text must stay inside;
// without one the innermost ui::lint::ContainerScope is used.
void drawText(sf::RenderTarget& target, const sf::Text& text);
void drawText(sf::RenderTarget& target, const sf::Text& text, const sf::FloatRect& container);

namespace lint {

void setEnabled(bool on);
bool isEnabled();

// Forget the previous frame's records (called once at the start of every frame).
void beginFrame();

// An opaque overlay (dialog, backdrop, tag) was just drawn over this area: texts drawn earlier
// that it touches are (partly) hidden under it and no longer take part in the overlap check.
void occlude(const sf::FloatRect& area);

// An opaque element of the same layout (a button, a badge) was drawn: unlike occlude(), a text
// drawn earlier that it covers only partly is reported, because part of that text is hidden.
void solid(const sf::FloatRect& area);

// An icon or another small mark (sun dial, badge glyph) was drawn here. Text must not touch it,
// whether the text is drawn before or after it; an occlude() over it hides the icon as well.
void icon(const sf::FloatRect& area);

// Every text drawn while a scope is alive must stay inside its rectangle
// (unless drawText gets an explicit container). Scopes nest.
class ContainerScope {
public:
    explicit ContainerScope(const sf::FloatRect& area);
    ~ContainerScope();
    ContainerScope(const ContainerScope&) = delete;
    ContainerScope& operator=(const ContainerScope&) = delete;
};

// Problems found in the frame recorded since the last beginFrame(), one line each.
std::vector<std::string> report();

} // namespace lint
} // namespace ui

#endif // UI_TEXT_H
