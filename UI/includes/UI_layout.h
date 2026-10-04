#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

// =============================================================================
// Team b-session (UX-12, HX-15): shared layout anchors, UI scale and crisp text.
//
// * Named rects that BOTH the drawing code and the click handling use, so a button
//   can never be drawn in one place and clicked in another.
// * overlayView(): the UI-scale zoom for menus and dialogs. It magnifies a content
//   rect about its centre and keeps it inside the 1600x900 canvas, so 125 % / 150 %
//   (and projector mode) never push a menu off screen. Hit tests map the mouse with
//   the same view.
// * Crisp text: glyphs rasterised at the real pixel size of the current view
//   (characterSize x pixels-per-unit, scaled back), so zoomed or 1440p text is sharp.
// * palette(): colours of the session screens; projector mode switches to a
//   high-contrast set. (The integrator can map these to UI_theme.h tokens.)
// =============================================================================

#include <SFML/Graphics.hpp>
#include <string>
#include "UI_types.h"

namespace ui {

// --- In-match HUD buttons (bottom-right) and the key-hint bar (bottom-centre) ---
namespace hud {
const sf::FloatRect MENU_BTN({ 1600.0f - 130.0f, 900.0f - 34.0f }, { 120.0f, 28.0f });
const sf::FloatRect FULLSCREEN_BTN({ 1600.0f - 275.0f, 900.0f - 34.0f }, { 135.0f, 28.0f });
const sf::FloatRect HELP_BTN({ 1600.0f - 405.0f, 900.0f - 34.0f }, { 120.0f, 28.0f });
const sf::FloatRect HINT_BAR({ 250.0f, 900.0f - 30.0f }, { 930.0f, 26.0f });
} // namespace hud

// --- Main menu: a centred column of equal buttons ---
namespace menu {
constexpr float BTN_W = 320.0f;
constexpr float BTN_H = 54.0f;
constexpr float FIRST_Y = 404.0f; // below the main-menu logo (Wave A layout)
constexpr float SPACING = 76.0f;
inline sf::FloatRect button(int index) {
    return sf::FloatRect({ (VIRTUAL_WIDTH - BTN_W) / 2.0f, FIRST_Y + SPACING * static_cast<float>(index) }, { BTN_W, BTN_H });
}
} // namespace menu

// --- In-match pause card with N options ---
namespace pause {
constexpr float BOX_W = 500.0f;
constexpr float BTN_W = 390.0f;
constexpr float BTN_H = 50.0f;
constexpr float SPACING = 58.0f;
constexpr float FIRST_OFFSET = 100.0f; // from the card top to the first button
inline sf::FloatRect box(int count) {
    float h = FIRST_OFFSET + SPACING * static_cast<float>(count) + 26.0f;
    return sf::FloatRect({ (VIRTUAL_WIDTH - BOX_W) / 2.0f, (VIRTUAL_HEIGHT - h) / 2.0f }, { BOX_W, h });
}
inline sf::FloatRect option(int index, int count) {
    sf::FloatRect b = box(count);
    return sf::FloatRect({ b.position.x + (BOX_W - BTN_W) / 2.0f, b.position.y + FIRST_OFFSET + SPACING * static_cast<float>(index) },
                         { BTN_W, BTN_H });
}
} // namespace pause

// --- UI scale ---
// Scale for menus and dialogs from the settings (projector mode: at least 125 %)
float overlayScale();
// The base 1600x900 view zoomed by `scale` about `content`'s centre; the zoom is reduced
// when needed so `content` stays fully on the canvas. Same viewport as `base`.
sf::View overlayView(const sf::View& base, sf::FloatRect content, float scale);
// Same, with scale = max(1, overlayScale()): for overlays that paint a full-canvas backdrop
sf::View overlayViewNoShrink(const sf::View& base, sf::FloatRect content);

// --- Crisp text ---
// Window pixels per canvas unit for the target's current view
float pixelRatio(const sf::RenderTarget& target);
// sf::Text rasterised at the real on-screen size (setScale compensates)
sf::Text crispText(const sf::Font& font, const sf::String& str, unsigned int size, const sf::RenderTarget& target);
// Draws UTF-8 text; align: 0 = left, 1 = centre, 2 = right of pos.x. pos.y is the top of the
// letters (not the font's internal top padding). Returns the drawn size in canvas units.
sf::Vector2f drawText(sf::RenderTarget& target, const sf::Font& font, const std::string& utf8, unsigned int size,
                      sf::Vector2f pos, sf::Color color, int align = 0, bool bold = false);
// Width of UTF-8 text at `size` in canvas units (for fitting)
float textWidth(const sf::Font& font, const std::string& utf8, unsigned int size);
// Largest size <= `size` (and >= minSize) whose text fits `maxWidth`
unsigned int fitTextSize(const sf::Font& font, const std::string& utf8, unsigned int size, unsigned int minSize, float maxWidth);

// --- Colours for the session screens (settings, key bindings, saves, dialogs) ---
struct Palette {
    sf::Color backdrop;    // full-screen dim
    sf::Color panel;       // card background
    sf::Color panelAlt;    // header strip / alternating rows
    sf::Color border;      // card outline
    sf::Color text;        // primary text
    sf::Color textDim;     // secondary text (still >= 4.5:1 on panel)
    sf::Color accent;      // titles, active tab
    sf::Color focus;       // keyboard/gamepad focus outline
    sf::Color rowFocus;    // focused row fill
    sf::Color button;      // button fill
    sf::Color buttonHover; // hovered / focused button fill
    sf::Color danger;      // conflicts, errors
    sf::Color ok;          // success messages
    float outline;         // standard outline thickness
    float focusOutline;    // focus outline thickness
};
const Palette& palette(); // normal or high-contrast (projector mode)

// --- Widgets shared by the session screens ---
// Card with a header strip and a centred title
void drawPanel(sf::RenderTarget& target, const sf::Font& font, sf::FloatRect rect, const std::string& title);
// Button with a centred label that is shrunk until it fits
void drawButton(sf::RenderTarget& target, const sf::Font& font, sf::FloatRect rect, const std::string& label,
                bool focused, bool hovered, bool enabled = true, unsigned int size = 16);
// Small filled triangle pointing left or right (value selectors)
void drawArrow(sf::RenderTarget& target, sf::Vector2f center, float size, bool left, sf::Color color);
// Full-canvas dim drawn with the target's current view
void drawBackdrop(sf::RenderTarget& target);

} // namespace ui

#endif // UI_LAYOUT_H
