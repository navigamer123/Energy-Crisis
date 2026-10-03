#include "../includes/UI_text.h"
#include "../includes/UI_types.h"
#include <algorithm>
#include <cstdio>

namespace {

struct TextRecord {
    sf::String str;
    sf::FloatRect bounds;
    bool hasContainer = false;
    sf::FloatRect container;
    std::uint8_t alpha = 255;
    unsigned int size = 0;
    std::vector<char32_t> missingGlyphs;
    bool occluded = false;
};

bool g_enabled = false;
std::vector<TextRecord> g_records;
std::vector<sf::FloatRect> g_containers;

// Texts fainter than this are invisible (fading notices) and never collide
constexpr std::uint8_t MIN_VISIBLE_ALPHA = 40;
// Sub-pixel slack for anti-aliased glyph boxes
constexpr float TOL = 1.0f;

bool isBlank(char32_t c) {
    return c == U' ' || c == U'\n' || c == U'\t' || c == U'\r' || c == 0x00A0;
}

std::string toStd(const sf::String& s) {
    auto u8 = s.toUtf8();
    return std::string(u8.begin(), u8.end());
}

// Short, single-line quote of a text for the report
std::string quote(const sf::String& s) {
    sf::String flat;
    for (char32_t c : s) flat += (c == U'\n' || c == U'\r') ? U' ' : c;
    const std::size_t maxLen = 48;
    std::string out = "\"" + toStd(flat.getSize() > maxLen ? flat.substring(0, maxLen) : flat);
    if (flat.getSize() > maxLen) out += "...";
    return out + "\"";
}

std::string rectStr(const sf::FloatRect& r) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "[%.0f,%.0f %.0fx%.0f]", r.position.x, r.position.y, r.size.x, r.size.y);
    return buf;
}

float area(const sf::FloatRect& r) { return std::max(0.0f, r.size.x) * std::max(0.0f, r.size.y); }

// Intersection of two rectangles (empty size when they do not touch)
sf::FloatRect intersect(const sf::FloatRect& a, const sf::FloatRect& b) {
    float l = std::max(a.position.x, b.position.x);
    float t = std::max(a.position.y, b.position.y);
    float r = std::min(a.position.x + a.size.x, b.position.x + b.size.x);
    float btm = std::min(a.position.y + a.size.y, b.position.y + b.size.y);
    return sf::FloatRect({ l, t }, { r - l, btm - t });
}

// How far `inner` sticks out of `outer` on its worst side (0 when inside, with tolerance)
float overhang(const sf::FloatRect& inner, const sf::FloatRect& outer) {
    float left = outer.position.x - inner.position.x;
    float top = outer.position.y - inner.position.y;
    float right = (inner.position.x + inner.size.x) - (outer.position.x + outer.size.x);
    float bottom = (inner.position.y + inner.size.y) - (outer.position.y + outer.size.y);
    return std::max({ 0.0f, left, top, right, bottom });
}

void record(const sf::Text& text, const sf::FloatRect* explicitContainer) {
    TextRecord r;
    r.str = text.getString();
    r.bounds = text.getGlobalBounds();
    r.alpha = text.getFillColor().a;
    r.size = text.getCharacterSize();
    if (explicitContainer) {
        r.hasContainer = true;
        r.container = *explicitContainer;
    } else if (!g_containers.empty()) {
        r.hasContainer = true;
        r.container = g_containers.back();
    }
    const sf::Font& font = text.getFont();
    for (char32_t c : r.str) {
        if (isBlank(c)) continue;
        if (!font.hasGlyph(c) &&
            std::find(r.missingGlyphs.begin(), r.missingGlyphs.end(), c) == r.missingGlyphs.end()) {
            r.missingGlyphs.push_back(c);
        }
    }
    g_records.push_back(std::move(r));
}

} // namespace

namespace ui {

sf::Text makeText(const sf::Font& font, const std::string& utf8, unsigned int size, sf::Color color, sf::Vector2f pos) {
    sf::Text t(font, toUtf8(utf8), size);
    t.setFillColor(color);
    t.setPosition(pos);
    return t;
}

void drawText(sf::RenderTarget& target, const sf::Text& text) {
    target.draw(text);
    if (g_enabled) record(text, nullptr);
}

void drawText(sf::RenderTarget& target, const sf::Text& text, const sf::FloatRect& container) {
    target.draw(text);
    if (g_enabled) record(text, &container);
}

namespace lint {

void setEnabled(bool on) { g_enabled = on; }
bool isEnabled() { return g_enabled; }

void beginFrame() {
    g_records.clear();
    g_containers.clear();
}

void occlude(const sf::FloatRect& areaRect) {
    if (!g_enabled) return;
    for (auto& r : g_records) {
        if (r.occluded) continue;
        sf::FloatRect inter = intersect(r.bounds, areaRect);
        if (inter.size.x > 0.0f && inter.size.y > 0.0f && area(inter) >= 0.5f * area(r.bounds)) {
            r.occluded = true;
        }
    }
}

ContainerScope::ContainerScope(const sf::FloatRect& areaRect) {
    if (g_enabled) g_containers.push_back(areaRect);
}

ContainerScope::~ContainerScope() {
    if (g_enabled && !g_containers.empty()) g_containers.pop_back();
}

std::vector<std::string> report() {
    std::vector<std::string> out;
    const sf::FloatRect canvas({ 0.0f, 0.0f }, { VIRTUAL_WIDTH, VIRTUAL_HEIGHT });

    for (const auto& r : g_records) {
        bool blank = std::all_of(r.str.begin(), r.str.end(), isBlank);
        if (blank) {
            out.push_back("LINT empty: empty text at " + rectStr(r.bounds));
            continue;
        }
        if (!r.missingGlyphs.empty()) {
            std::string cps;
            for (char32_t c : r.missingGlyphs) {
                char buf[16];
                std::snprintf(buf, sizeof(buf), " U+%04X", static_cast<unsigned>(c));
                cps += buf;
            }
            out.push_back("LINT glyph: " + quote(r.str) + " " + rectStr(r.bounds) + " has characters missing from the font:" + cps);
        }
        if (r.alpha < MIN_VISIBLE_ALPHA || r.occluded) continue;
        if (overhang(r.bounds, canvas) > TOL) {
            out.push_back("LINT canvas: " + quote(r.str) + " " + rectStr(r.bounds) + " is outside the 1600x900 canvas");
        }
        if (r.hasContainer) {
            float over = overhang(r.bounds, r.container);
            if (over > TOL) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.1f", over);
                out.push_back("LINT container: " + quote(r.str) + " " + rectStr(r.bounds) + " exceeds its container " +
                              rectStr(r.container) + " by " + buf + " px");
            }
        }
    }

    // Overlaps between visible texts (a drop shadow, i.e. the same string a few px away, is fine)
    for (std::size_t i = 0; i < g_records.size(); ++i) {
        const auto& a = g_records[i];
        if (a.alpha < MIN_VISIBLE_ALPHA || a.occluded || std::all_of(a.str.begin(), a.str.end(), isBlank)) continue;
        for (std::size_t j = i + 1; j < g_records.size(); ++j) {
            const auto& b = g_records[j];
            if (b.alpha < MIN_VISIBLE_ALPHA || b.occluded || std::all_of(b.str.begin(), b.str.end(), isBlank)) continue;
            sf::FloatRect inter = intersect(a.bounds, b.bounds);
            if (inter.size.x <= TOL || inter.size.y <= TOL) continue;
            bool shadow = (a.str == b.str) && std::abs(a.bounds.position.x - b.bounds.position.x) <= 4.0f &&
                          std::abs(a.bounds.position.y - b.bounds.position.y) <= 4.0f;
            if (shadow) continue;
            out.push_back("LINT overlap: " + quote(a.str) + " " + rectStr(a.bounds) + " overlaps " + quote(b.str) + " " +
                          rectStr(b.bounds));
        }
    }
    return out;
}

} // namespace lint
} // namespace ui
