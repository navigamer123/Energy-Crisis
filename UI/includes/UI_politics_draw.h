#ifndef UI_POLITICS_DRAW_H
#define UI_POLITICS_DRAW_H

// =============================================================================
// [b-politics] Drawing helpers & colours shared by UI_politics*.cpp
// Colours are kept in these few named constants so the integrator can map them to the
// UI_theme.h tokens from wave A. Text helpers never let text leave its box: they shrink the
// size down to a minimum, then truncate with an ellipsis; wrapText breaks on spaces.
// =============================================================================

#include <SFML/Graphics.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "UI_types.h"
#include "../../Game/includes/game_politics.h"

namespace PolUi {

const sf::Color PANEL_BG(16, 22, 34, 246);
const sf::Color PANEL_EDGE(70, 95, 130);
const sf::Color HEADER_BG(26, 36, 56);
const sf::Color ROW_BG(22, 30, 46);
const sf::Color ROW_SEL_BG(38, 56, 86);
const sf::Color TEXT(226, 236, 250);
const sf::Color TEXT_DIM(150, 170, 196);
const sf::Color P1(0, 229, 255);
const sf::Color P2(255, 120, 200);
const sf::Color GOLD(255, 215, 0);
const sf::Color GOOD(110, 230, 140);
const sf::Color BAD(255, 112, 112);
const sf::Color FESTIVAL(255, 172, 64);
const sf::Color NEUTRAL(150, 190, 255);
const sf::Color BAR_BG(40, 50, 66);

inline sf::Color playerColor(int player) { return player == 1 ? P1 : P2; }

inline sf::Color toneColor(Politics::Tone t) {
    switch (t) {
        case Politics::Tone::GOOD:     return GOOD;
        case Politics::Tone::BAD:      return BAD;
        case Politics::Tone::FESTIVAL: return FESTIVAL;
        default:                       return NEUTRAL;
    }
}

inline sf::Color withAlpha(sf::Color c, std::uint8_t a) {
    c.a = a;
    return c;
}

enum class Align { LEFT, CENTER, RIGHT };

inline void drawRect(sf::RenderTarget& t, sf::FloatRect r, sf::Color fill, sf::Color edge = sf::Color::Transparent,
                     float edgeW = 0.0f) {
    sf::RectangleShape s(r.size);
    s.setPosition(r.position);
    s.setFillColor(fill);
    if (edgeW > 0.0f) {
        s.setOutlineThickness(edgeW);
        s.setOutlineColor(edge);
    }
    t.draw(s);
}

// Width of a string at a size (bold optional)
// -----------------------------------------------------------------------------
// Text cache. Building an sf::Text in SFML 3.1 shapes the string (about 1 ms each), so every
// fitted label, measurement and word wrap is computed once and reused; per frame only the
// position and colour change. The cache is dropped when it grows large and on a new match.
// -----------------------------------------------------------------------------
struct CachedText {
    std::optional<sf::Text> text;
    float width = 0.0f;
};

inline std::unordered_map<std::string, CachedText>& textCache() {
    static std::unordered_map<std::string, CachedText> cache;
    return cache;
}

inline std::unordered_map<std::string, std::vector<std::string>>& wrapCache() {
    static std::unordered_map<std::string, std::vector<std::string>> cache;
    return cache;
}

inline void clearTextCache() {
    textCache().clear();
    wrapCache().clear();
}

inline std::string cacheKey(const sf::Font& f, const std::string& utf8, unsigned size, bool bold, float maxW) {
    return utf8 + '\x1f' + std::to_string(size) + (bold ? "b" : "r") + std::to_string(static_cast<int>(maxW)) + '@' +
           std::to_string(reinterpret_cast<std::uintptr_t>(&f));
}

// Uncached measurement (used only while building a cache entry)
inline float measureRaw(const sf::Font& f, const sf::String& s, unsigned size, bool bold) {
    sf::Text tx(f, s, size);
    if (bold) tx.setStyle(sf::Text::Bold);
    return tx.getLocalBounds().size.x;
}

// Width of a string at a size (cached)
inline float textWidth(const sf::Font& f, const sf::String& s, unsigned size, bool bold = false) {
    const auto u8 = s.toUtf8();
    std::string key = cacheKey(f, std::string(u8.begin(), u8.end()), size, bold, -1.0f);
    auto& cache = textCache();
    auto it = cache.find(key);
    if (it != cache.end()) return it->second.width;
    if (cache.size() > 4000) cache.clear();
    CachedText& e = cache[key];
    e.width = measureRaw(f, s, size, bold);
    return e.width;
}

// Draws `s` with its line box top at pos.y. pos.x is the left edge, centre or right edge.
// Shrinks to minSize, then cuts with "…" so the text always fits maxW. Returns the drawn width.
inline float drawFit(sf::RenderTarget& t, const sf::Font& f, const std::string& utf8, unsigned size, sf::Vector2f pos,
                     sf::Color c, float maxW, Align align = Align::LEFT, bool bold = false, unsigned minSize = 10) {
    auto& cache = textCache();
    const std::string key = cacheKey(f, utf8, size, bold, maxW * 100.0f + static_cast<float>(minSize));
    auto it = cache.find(key);
    if (it == cache.end() || !it->second.text) {
        if (cache.size() > 4000) cache.clear();
        sf::String s = toUtf8(utf8);
        unsigned sz = size;
        while (sz > minSize && measureRaw(f, s, sz, bold) > maxW) --sz;
        if (measureRaw(f, s, sz, bold) > maxW) {
            // Binary search for the longest prefix that fits with an ellipsis
            const sf::String dots = sf::String(static_cast<char32_t>(0x2026));
            std::size_t lo = 0, hi = s.getSize();
            while (lo < hi) {
                std::size_t mid = (lo + hi + 1) / 2;
                if (measureRaw(f, s.substring(0, mid) + dots, sz, bold) <= maxW) lo = mid;
                else hi = mid - 1;
            }
            s = s.substring(0, lo) + dots;
        }
        CachedText& e = cache[key];
        e.text.emplace(f, s, sz);
        if (bold) e.text->setStyle(sf::Text::Bold);
        e.width = e.text->getLocalBounds().size.x;
        it = cache.find(key);
    }
    sf::Text& tx = *it->second.text;
    const float w = it->second.width;
    float x = pos.x;
    if (align == Align::CENTER) x -= w / 2.0f;
    else if (align == Align::RIGHT) x -= w;
    tx.setFillColor(c);
    tx.setPosition({ std::round(x), std::round(pos.y) });
    t.draw(tx);
    return w;
}

// Word wrap into lines no wider than maxW (cached; a single over-long word is cut by drawFit later)
inline std::vector<std::string> wrapText(const sf::Font& f, const std::string& utf8, unsigned size, float maxW) {
    const std::string key = cacheKey(f, utf8, size, false, maxW);
    auto& cache = wrapCache();
    auto found = cache.find(key);
    if (found != cache.end()) return found->second;
    if (cache.size() > 500) cache.clear();
    std::vector<std::string> lines;
    std::string cur;
    size_t i = 0;
    while (i <= utf8.size()) {
        size_t sp = utf8.find(' ', i);
        if (sp == std::string::npos) sp = utf8.size();
        std::string word = utf8.substr(i, sp - i);
        std::string trial = cur.empty() ? word : cur + " " + word;
        if (!cur.empty() && measureRaw(f, toUtf8(trial), size, false) > maxW) {
            lines.push_back(cur);
            cur = word;
        } else {
            cur = trial;
        }
        i = sp + 1;
    }
    if (!cur.empty()) lines.push_back(cur);
    cache[key] = lines;
    return lines;
}

// Horizontal progress bar; frac is clamped to [0, 1]
inline void drawBar(sf::RenderTarget& t, sf::FloatRect r, float frac, sf::Color fill) {
    drawRect(t, r, BAR_BG);
    float f = frac < 0.0f ? 0.0f : (frac > 1.0f ? 1.0f : frac);
    if (f > 0.0f) drawRect(t, sf::FloatRect(r.position, { r.size.x * f, r.size.y }), fill);
}

// Money with thin grouping: 12345 -> "12 345"
inline std::string money(int v) {
    bool neg = v < 0;
    std::string d = std::to_string(neg ? -v : v);
    std::string out;
    int n = 0;
    for (size_t k = d.size(); k > 0; --k) {
        out.insert(out.begin(), d[k - 1]);
        if (++n % 3 == 0 && k > 1) out.insert(out.begin(), ' ');
    }
    return (neg ? "-" : "") + out;
}

} // namespace PolUi

#endif // UI_POLITICS_DRAW_H
