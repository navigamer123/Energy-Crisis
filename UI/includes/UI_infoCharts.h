#ifndef UI_INFO_CHARTS_H
#define UI_INFO_CHARTS_H

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>
#include "UI_infoText.h"
#include "UI_types.h"

// -----------------------------------------------------------------------------
// team info: tiny chart toolkit shared by the energy dashboard (Tab) and the
// post-match report. Lines and areas are batched into one vertex array each.
// -----------------------------------------------------------------------------
namespace infoCharts {

// Colours used by both chart screens (kept here so a theme pass can map them)
const sf::Color GRID(44, 60, 86);
const sf::Color AXIS_TEXT(150, 175, 210);
const sf::Color TITLE_TEXT(234, 242, 255);
const sf::Color PANEL(20, 29, 45);
const sf::Color PANEL_EDGE(52, 72, 102);

// Data space -> screen space mapping for one chart
struct Frame {
    sf::FloatRect r;     // plot area on screen
    float x0 = 0.0f, x1 = 1.0f;
    float y0 = 0.0f, y1 = 1.0f;
    sf::Vector2f map(float x, float y) const {
        float tx = (x1 > x0) ? (x - x0) / (x1 - x0) : 0.0f;
        float ty = (y1 > y0) ? (y - y0) / (y1 - y0) : 0.0f;
        tx = std::clamp(tx, 0.0f, 1.0f);
        ty = std::clamp(ty, 0.0f, 1.0f);
        return { r.position.x + tx * r.size.x, r.position.y + r.size.y - ty * r.size.y };
    }
};

// Smallest "nice" number (1, 2, 2.5, 5 x 10^k) that is >= v
inline float niceCeil(float v) {
    if (v <= 0.0f) return 1.0f;
    float p = std::pow(10.0f, std::floor(std::log10(v)));
    for (float m : { 1.0f, 2.0f, 2.5f, 5.0f, 10.0f }) {
        if (m * p >= v - 1e-4f) return m * p;
    }
    return 10.0f * p;
}

// Number of grid steps (4, 5 or 2) whose step is itself a "nice" value (1, 2, 2.5, 5 x 10^k)
inline int niceTicks(float yMax) {
    for (int n : { 4, 5, 2 }) {
        float step = yMax / static_cast<float>(n);
        if (std::fabs(niceCeil(step) - step) < 1e-3f * std::max(1.0f, step)) return n;
    }
    return 4;
}

// Thick polyline as one triangle batch
inline void polyline(sf::RenderTarget& t, const std::vector<sf::Vector2f>& pts, sf::Color col, float thickness) {
    if (pts.size() < 2) return;
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    const float h = thickness * 0.5f;
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        sf::Vector2f a = pts[i], b = pts[i + 1];
        sf::Vector2f d = b - a;
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len < 0.01f) continue;
        sf::Vector2f n(-d.y / len * h, d.x / len * h);
        sf::Vertex v0{ a + n, col }, v1{ b + n, col }, v2{ b - n, col }, v3{ a - n, col };
        va.append(v0); va.append(v1); va.append(v2);
        va.append(v0); va.append(v2); va.append(v3);
    }
    t.draw(va);
}

// Filled area between a polyline and a horizontal line (baseY can be the top or the bottom of the plot)
inline void fillArea(sf::RenderTarget& t, const std::vector<sf::Vector2f>& pts, float baseY, sf::Color col) {
    if (pts.size() < 2) return;
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        sf::Vector2f a = pts[i], b = pts[i + 1];
        sf::Vertex p0{ a, col }, p1{ b, col }, p2{ { b.x, baseY }, col }, p3{ { a.x, baseY }, col };
        va.append(p0); va.append(p1); va.append(p2);
        va.append(p0); va.append(p2); va.append(p3);
    }
    t.draw(va);
}

inline void dashedH(sf::RenderTarget& t, float x0, float x1, float y, sf::Color col, float dash = 6.0f, float gap = 4.0f) {
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    for (float x = x0; x < x1; x += dash + gap) {
        float xe = std::min(x1, x + dash);
        sf::Vertex a{ { x, y - 0.5f }, col }, b{ { xe, y - 0.5f }, col }, c{ { xe, y + 0.5f }, col }, d{ { x, y + 0.5f }, col };
        va.append(a); va.append(b); va.append(c);
        va.append(a); va.append(c); va.append(d);
    }
    t.draw(va);
}

inline void rect(sf::RenderTarget& t, sf::FloatRect r, sf::Color fill, sf::Color edge = sf::Color::Transparent, float edgeW = 0.0f) {
    sf::RectangleShape s(r.size);
    s.setPosition(r.position);
    s.setFillColor(fill);
    if (edgeW > 0.0f) {
        s.setOutlineThickness(edgeW);
        s.setOutlineColor(edge);
    }
    t.draw(s);
}

inline void text(sf::RenderTarget& t, const sf::Font& f, const std::string& s, unsigned size, sf::Vector2f pos, sf::Color col,
                 bool bold = false, float maxW = 0.0f) {
    sf::Text tx(f, toUtf8(s), size);
    if (bold) tx.setStyle(sf::Text::Bold);
    if (maxW > 0.0f) tx.setString(infoText::ellipsize(f, toUtf8(s), size, maxW, bold));
    tx.setFillColor(col);
    tx.setPosition(pos);
    t.draw(tx);
}

// Right-aligned text ending at pos.x
inline void textRight(sf::RenderTarget& t, const sf::Font& f, const std::string& s, unsigned size, sf::Vector2f pos, sf::Color col,
                      bool bold = false) {
    sf::Text tx(f, toUtf8(s), size);
    if (bold) tx.setStyle(sf::Text::Bold);
    tx.setFillColor(col);
    tx.setPosition({ pos.x - infoText::width(tx), pos.y });
    t.draw(tx);
}

inline void textCentered(sf::RenderTarget& t, const sf::Font& f, const std::string& s, unsigned size, sf::Vector2f center, sf::Color col,
                         bool bold = false) {
    sf::Text tx(f, toUtf8(s), size);
    if (bold) tx.setStyle(sf::Text::Bold);
    tx.setFillColor(col);
    tx.setPosition({ center.x - infoText::width(tx) / 2.0f, center.y });
    t.draw(tx);
}

struct LegendItem {
    sf::Color color;
    std::string label;
};

inline float legendWidth(const sf::Font& f, const std::vector<LegendItem>& items) {
    float w = 0.0f;
    for (const auto& it : items) w += 19.0f + infoText::advance(f, toUtf8(it.label), 11) + 16.0f;
    return items.empty() ? 0.0f : w - 16.0f;
}

// Legend entries laid out so that the last one ends at rightX
inline void legendRight(sf::RenderTarget& t, const sf::Font& f, float rightX, float y, const std::vector<LegendItem>& items) {
    float x = rightX - legendWidth(f, items);
    for (const auto& it : items) {
        rect(t, sf::FloatRect({ x, y + 4.0f }, { 14.0f, 4.0f }), it.color);
        sf::Text tx(f, toUtf8(it.label), 11);
        tx.setFillColor(AXIS_TEXT);
        tx.setPosition({ x + 19.0f, y - 2.0f });
        t.draw(tx);
        x += 19.0f + infoText::width(tx) + 16.0f;
    }
}

// Panel with a title (shortened so it never runs into the legend), an optional legend at the
// top right, horizontal grid lines and y-axis labels. Returns the plot area frame.
inline Frame chartPanel(sf::RenderTarget& t, const sf::Font& f, sf::FloatRect box, const std::string& title,
                        float yMax, int yTicks, const std::function<std::string(float)>& yLabel,
                        const std::vector<LegendItem>& legendItems = {}) {
    rect(t, box, PANEL, PANEL_EDGE, 1.0f);
    const float legendW = legendWidth(f, legendItems);
    if (!legendItems.empty()) legendRight(t, f, box.position.x + box.size.x - 12.0f, box.position.y + 10.0f, legendItems);
    const float titleMaxW = box.size.x - 24.0f - (legendItems.empty() ? 0.0f : legendW + 18.0f);
    text(t, f, title, 13, { box.position.x + 12.0f, box.position.y + 8.0f }, TITLE_TEXT, true, titleMaxW);

    Frame fr;
    fr.r = sf::FloatRect({ box.position.x + 58.0f, box.position.y + 36.0f }, { box.size.x - 74.0f, box.size.y - 62.0f });
    fr.y0 = 0.0f;
    fr.y1 = yMax;
    for (int i = 0; i <= yTicks; ++i) {
        float v = yMax * static_cast<float>(i) / static_cast<float>(yTicks);
        sf::Vector2f p = fr.map(0.0f, v);
        rect(t, sf::FloatRect({ fr.r.position.x, p.y }, { fr.r.size.x, 1.0f }), GRID);
        textRight(t, f, yLabel(v), 10, { fr.r.position.x - 6.0f, p.y - 7.0f }, AXIS_TEXT);
    }
    return fr;
}

// Small coloured legend entry; returns the x after it
inline float legend(sf::RenderTarget& t, const sf::Font& f, float x, float y, sf::Color col, const std::string& label) {
    rect(t, sf::FloatRect({ x, y + 4.0f }, { 14.0f, 4.0f }), col);
    sf::Text tx(f, toUtf8(label), 11);
    tx.setFillColor(AXIS_TEXT);
    tx.setPosition({ x + 19.0f, y - 2.0f });
    t.draw(tx);
    return x + 19.0f + infoText::width(tx) + 16.0f;
}

inline std::string fmtInt(double v) {
    long n = std::lround(v);
    std::string s = std::to_string(std::labs(n));
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<std::size_t>(i), " ");
    return (n < 0 ? "-" : "") + s;
}

// "1 234 т" or "12,5 хил. т" (thousandsUnit forces the second form, e.g. to match another value)
inline std::string fmtTonnes(double t, bool thousandsUnit = false) {
    if (t >= 10000.0 || thousandsUnit) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f", t / 1000.0);
        std::string s(buf);
        std::replace(s.begin(), s.end(), '.', ',');
        return s + " хил. т";
    }
    return fmtInt(t) + " т";
}

} // namespace infoCharts

#endif // UI_INFO_CHARTS_H
