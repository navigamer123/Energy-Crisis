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
// the 1600x900 canvas, and empty strings or characters missing from the font.
// -----------------------------------------------------------------------------
namespace ui {

// Builds a text from a UTF-8 string (Cyrillic safe) with colour and position.
sf::Text makeText(const sf::Font& font, const std::string& utf8, unsigned int size,
                  sf::Color color = sf::Color::White, sf::Vector2f pos = { 0.0f, 0.0f });

// Draws the text. The container (if any) is the rectangle the text must stay inside;
// without one the innermost ui::lint::ContainerScope is used.
void drawText(sf::RenderTarget& target, const sf::Text& text);
void drawText(sf::RenderTarget& target, const sf::Text& text, const sf::FloatRect& container);

namespace lint {

void setEnabled(bool on);
bool isEnabled();

// Forget the previous frame's records (called once at the start of every frame).
void beginFrame();

// An opaque surface was just drawn over this area: texts drawn earlier underneath it
// are hidden and no longer take part in the overlap check.
void occlude(const sf::FloatRect& area);

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
