// =============================================================================
// Team b-session (UX-12, HX-15): UI scale views, crisp text and session palette
// =============================================================================
#include "../includes/UI_layout.h"
#include "../includes/UI_settings.h"
#include <algorithm>
#include <cmath>

namespace ui {

float overlayScale() {
    return gameSettings().overlayScale();
}

sf::View overlayView(const sf::View& base, sf::FloatRect content, float scale) {
    // Never zoom so far that the content no longer fits the canvas
    float fit = std::min(VIRTUAL_WIDTH / std::max(1.0f, content.size.x), VIRTUAL_HEIGHT / std::max(1.0f, content.size.y));
    float s = std::max(0.5f, std::min(scale, fit));
    sf::View v = base;
    sf::Vector2f size(VIRTUAL_WIDTH / s, VIRTUAL_HEIGHT / s);
    v.setSize(size);
    sf::Vector2f c(content.position.x + content.size.x / 2.0f, content.position.y + content.size.y / 2.0f);
    if (s >= 1.0f) {
        // Zoomed in: keep the visible area on the canvas (full-canvas backdrops still cover it)
        c.x = std::max(size.x / 2.0f, std::min(c.x, VIRTUAL_WIDTH - size.x / 2.0f));
        c.y = std::max(size.y / 2.0f, std::min(c.y, VIRTUAL_HEIGHT - size.y / 2.0f));
    } else {
        c = sf::Vector2f(VIRTUAL_WIDTH / 2.0f, VIRTUAL_HEIGHT / 2.0f);
    }
    v.setCenter(c);
    return v;
}

sf::View overlayViewNoShrink(const sf::View& base, sf::FloatRect content) {
    return overlayView(base, content, std::max(1.0f, overlayScale()));
}

float pixelRatio(const sf::RenderTarget& target) {
    const sf::View& v = target.getView();
    sf::IntRect vp = target.getViewport(v);
    if (v.getSize().y <= 0.0f || vp.size.y <= 0) return 1.0f;
    float r = static_cast<float>(vp.size.y) / v.getSize().y;
    // Rasterising below the design size makes tiny text worse, not sharper; cap huge sizes
    return std::max(1.0f, std::min(r, 4.0f));
}

sf::Text crispText(const sf::Font& font, const sf::String& str, unsigned int size, const sf::RenderTarget& target) {
    float r = pixelRatio(target);
    unsigned int px = static_cast<unsigned int>(std::lround(static_cast<float>(size) * r));
    sf::Text t(font, str, std::max(1u, px));
    float inv = static_cast<float>(size) / static_cast<float>(std::max(1u, px));
    t.setScale({ inv, inv });
    return t;
}

namespace {
// Distance from the text origin to the top of a capital letter, in the text's own units
float capTopLocal(const sf::Font& font, unsigned int charSize, bool bold) {
    const sf::Glyph& g = font.getGlyph(0x041D /* Н */, charSize, bold);
    return static_cast<float>(charSize) + g.bounds.position.y;
}
} // namespace

sf::Vector2f drawText(sf::RenderTarget& target, const sf::Font& font, const std::string& utf8, unsigned int size,
                      sf::Vector2f pos, sf::Color color, int align, bool bold) {
    sf::Text t = crispText(font, toUtf8(utf8), size, target);
    if (bold) t.setStyle(sf::Text::Bold);
    t.setFillColor(color);
    float scale = t.getScale().x;
    sf::FloatRect lb = t.getLocalBounds();
    float w = lb.size.x * scale;
    float x = pos.x - lb.position.x * scale;
    if (align == 1) x -= w / 2.0f;
    else if (align == 2) x -= w;
    float capTop = capTopLocal(font, t.getCharacterSize(), bold) * scale;
    // Whole pixels keep the glyph texels aligned with screen pixels
    float r = pixelRatio(target);
    sf::Vector2f p(std::round(x * r) / r, std::round((pos.y - capTop) * r) / r);
    t.setPosition(p);
    target.draw(t);
    float capH = (static_cast<float>(t.getCharacterSize()) * scale) - capTop;
    return sf::Vector2f(w, capH);
}

float textWidth(const sf::Font& font, const std::string& utf8, unsigned int size) {
    sf::Text t(font, toUtf8(utf8), size);
    return t.getLocalBounds().size.x;
}

unsigned int fitTextSize(const sf::Font& font, const std::string& utf8, unsigned int size, unsigned int minSize, float maxWidth) {
    unsigned int s = size;
    while (s > minSize && textWidth(font, utf8, s) > maxWidth) --s;
    return s;
}

const Palette& palette() {
    static const Palette normal = {
        sf::Color(6, 10, 18, 215),    // backdrop
        sf::Color(16, 22, 34, 250),   // panel
        sf::Color(24, 34, 52, 255),   // panelAlt
        sf::Color(0, 200, 255, 200),  // border
        sf::Color(235, 242, 255),     // text
        sf::Color(165, 185, 210),     // textDim
        sf::Color(0, 229, 255),       // accent
        sf::Color(255, 215, 0),       // focus
        sf::Color(40, 58, 88, 230),   // rowFocus
        sf::Color(34, 46, 66),        // button
        sf::Color(52, 82, 122),       // buttonHover
        sf::Color(255, 95, 95),       // danger
        sf::Color(90, 235, 160),      // ok
        1.5f,                         // outline
        2.5f                          // focusOutline
    };
    static const Palette contrast = {
        sf::Color(0, 0, 0, 235),
        sf::Color(6, 8, 12, 255),
        sf::Color(20, 24, 32, 255),
        sf::Color(255, 255, 255),
        sf::Color(255, 255, 255),
        sf::Color(225, 232, 240),
        sf::Color(90, 230, 255),
        sf::Color(255, 235, 0),
        sf::Color(40, 40, 10, 255),
        sf::Color(28, 32, 42),
        sf::Color(70, 78, 20),
        sf::Color(255, 110, 110),
        sf::Color(120, 255, 170),
        2.5f,
        4.0f
    };
    return gameSettings().projectorMode ? contrast : normal;
}

void drawBackdrop(sf::RenderTarget& target) {
    const sf::View& v = target.getView();
    sf::RectangleShape dim(v.getSize());
    dim.setPosition(v.getCenter() - v.getSize() / 2.0f);
    dim.setFillColor(palette().backdrop);
    target.draw(dim);
}

void drawPanel(sf::RenderTarget& target, const sf::Font& font, sf::FloatRect rect, const std::string& title) {
    const Palette& p = palette();
    sf::RectangleShape box(rect.size);
    box.setPosition(rect.position);
    box.setFillColor(p.panel);
    box.setOutlineThickness(p.outline + 0.5f);
    box.setOutlineColor(p.border);
    target.draw(box);

    sf::RectangleShape header({ rect.size.x, 54.0f });
    header.setPosition(rect.position);
    header.setFillColor(p.panelAlt);
    target.draw(header);

    sf::RectangleShape glow({ rect.size.x, 3.0f });
    glow.setPosition({ rect.position.x, rect.position.y + 54.0f });
    glow.setFillColor(p.accent);
    target.draw(glow);

    unsigned int size = fitTextSize(font, title, 24, 16, rect.size.x - 40.0f);
    drawText(target, font, title, size, { rect.position.x + rect.size.x / 2.0f, rect.position.y + 27.0f - size * 0.36f },
             p.accent, 1, true);
}

void drawButton(sf::RenderTarget& target, const sf::Font& font, sf::FloatRect rect, const std::string& label,
                bool focused, bool hovered, bool enabled, unsigned int size) {
    const Palette& p = palette();
    sf::RectangleShape b(rect.size);
    b.setPosition(rect.position);
    sf::Color fill = (focused || hovered) ? p.buttonHover : p.button;
    if (!enabled) fill = sf::Color(fill.r / 2 + 10, fill.g / 2 + 10, fill.b / 2 + 10, fill.a);
    b.setFillColor(fill);
    b.setOutlineThickness(focused ? p.focusOutline : p.outline);
    sf::Color idle(p.border.r, p.border.g, p.border.b, enabled ? 150 : 60);
    b.setOutlineColor(focused ? p.focus : (hovered && enabled ? p.accent : idle));
    target.draw(b);
    unsigned int s = fitTextSize(font, label, size, 10, rect.size.x - 16.0f);
    sf::Color tc = enabled ? (focused ? p.focus : p.text) : p.textDim;
    drawText(target, font, label, s, { rect.position.x + rect.size.x / 2.0f, rect.position.y + rect.size.y / 2.0f - s * 0.36f },
             tc, 1, focused);
}

void drawArrow(sf::RenderTarget& target, sf::Vector2f center, float size, bool left, sf::Color color) {
    sf::ConvexShape tri(3);
    float h = size * 0.5f;
    if (left) {
        tri.setPoint(0, { center.x + h * 0.7f, center.y - h });
        tri.setPoint(1, { center.x + h * 0.7f, center.y + h });
        tri.setPoint(2, { center.x - h * 0.7f, center.y });
    } else {
        tri.setPoint(0, { center.x - h * 0.7f, center.y - h });
        tri.setPoint(1, { center.x - h * 0.7f, center.y + h });
        tri.setPoint(2, { center.x + h * 0.7f, center.y });
    }
    tri.setFillColor(color);
    target.draw(tri);
}

} // namespace ui
