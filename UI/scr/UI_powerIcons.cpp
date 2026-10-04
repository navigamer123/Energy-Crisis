// =============================================================================
// Team b-power: procedural icons for the new building types, terrain badges
// and hazard symbols (no image files). Same call style as UI_icons.
// =============================================================================
#include "../includes/UI_power.h"
#include <cmath>
#include <initializer_list>

namespace {

// Colours (the integrator maps these to UI_theme tokens)
const sf::Color IC_OUTLINE(20, 26, 38);
const sf::Color IC_METAL(196, 204, 216);
const sf::Color IC_METAL_DARK(120, 132, 150);
const sf::Color IC_STEAM(240, 244, 250, 235);
const sf::Color IC_WARN(255, 215, 0);
const sf::Color IC_MAGMA(255, 110, 40);
const sf::Color IC_EARTH(92, 66, 48);
const sf::Color IC_PLASMA(255, 120, 255);
const sf::Color IC_PLASMA_CORE(130, 240, 255);
const sf::Color IC_TOKAMAK(118, 96, 190);
const sf::Color IC_GOLD(232, 192, 84);
const sf::Color IC_PANEL(44, 112, 222);
const sf::Color IC_BEAM(255, 232, 130, 120);
const sf::Color IC_WATER(52, 134, 224);
const sf::Color IC_WATER_LIGHT(110, 190, 245);
const sf::Color IC_CONCRETE(176, 180, 190);

struct Pen {
    sf::RenderTarget& t;
    sf::Vector2f c;
    float u; // one unit = size / 24
    sf::Vector2f p(float x, float y) const { return sf::Vector2f(c.x + x * u, c.y + y * u); }

    void rect(float x0, float y0, float x1, float y1, sf::Color fill, sf::Color line = sf::Color::Transparent, float th = 0.0f) const {
        sf::RectangleShape r({ (x1 - x0) * u, (y1 - y0) * u });
        r.setPosition(p(x0, y0));
        r.setFillColor(fill);
        if (th > 0.0f) {
            r.setOutlineThickness(th * u);
            r.setOutlineColor(line);
        }
        t.draw(r);
    }
    void circle(float x, float y, float r, sf::Color fill, sf::Color line = sf::Color::Transparent, float th = 0.0f) const {
        sf::CircleShape s(r * u, 24);
        s.setOrigin({ r * u, r * u });
        s.setPosition(p(x, y));
        s.setFillColor(fill);
        if (th > 0.0f) {
            s.setOutlineThickness(th * u);
            s.setOutlineColor(line);
        }
        t.draw(s);
    }
    void poly(std::initializer_list<sf::Vector2f> pts, sf::Color fill, sf::Color line = sf::Color::Transparent, float th = 0.0f) const {
        sf::ConvexShape s(pts.size());
        size_t i = 0;
        for (const auto& q : pts) s.setPoint(i++, p(q.x, q.y));
        s.setFillColor(fill);
        if (th > 0.0f) {
            s.setOutlineThickness(th * u);
            s.setOutlineColor(line);
        }
        t.draw(s);
    }
    void line(float x0, float y0, float x1, float y1, float th, sf::Color col) const {
        sf::Vector2f a = p(x0, y0), b = p(x1, y1);
        sf::Vector2f d = b - a;
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len < 0.01f) return;
        sf::RectangleShape r({ len, th * u });
        r.setOrigin({ 0.0f, th * u * 0.5f });
        r.setPosition(a);
        r.setRotation(sf::radians(std::atan2(d.y, d.x)));
        r.setFillColor(col);
        t.draw(r);
    }
};

void iconNuclear(const Pen& p) {
    // Cooling tower (two convex halves give the waist), steam, reactor dome with a warning light
    const float ox = -3.0f;
    p.poly({ { ox - 7.5f, 10.0f }, { ox - 4.4f, 1.0f }, { ox + 4.4f, 1.0f }, { ox + 7.5f, 10.0f } }, IC_METAL, IC_OUTLINE, 0.7f);
    p.poly({ { ox - 4.4f, 1.0f }, { ox - 5.4f, -6.0f }, { ox + 5.4f, -6.0f }, { ox + 4.4f, 1.0f } }, IC_METAL, IC_OUTLINE, 0.7f);
    p.rect(ox - 5.4f, -6.6f, ox + 5.4f, -5.4f, IC_METAL_DARK);
    p.circle(ox, -8.6f, 3.0f, IC_STEAM);
    p.circle(ox - 3.2f, -10.2f, 2.4f, IC_STEAM);
    p.circle(ox + 2.6f, -11.2f, 2.6f, IC_STEAM);
    p.rect(3.5f, 5.0f, 11.5f, 10.0f, IC_METAL_DARK, IC_OUTLINE, 0.6f);
    p.circle(7.5f, 5.0f, 4.0f, IC_METAL, IC_OUTLINE, 0.6f);
    p.rect(3.4f, 5.0f, 11.6f, 6.0f, IC_METAL_DARK);
    p.circle(7.5f, 3.6f, 1.4f, IC_WARN, IC_OUTLINE, 0.4f);
}

void iconGeothermal(const Pen& p) {
    p.rect(-11.0f, 7.0f, 11.0f, 10.5f, IC_EARTH);
    p.line(-9.0f, 8.6f, -4.0f, 9.6f, 1.0f, IC_MAGMA);
    p.line(-4.0f, 9.6f, 1.0f, 8.2f, 1.0f, IC_MAGMA);
    p.line(1.0f, 8.2f, 6.0f, 9.4f, 1.0f, IC_MAGMA);
    p.rect(-8.0f, -1.0f, 2.0f, 7.0f, sf::Color(78, 104, 140), IC_OUTLINE, 0.6f);
    p.poly({ { -9.0f, -1.0f }, { -3.0f, -5.0f }, { 3.0f, -1.0f } }, sf::Color(150, 70, 60), IC_OUTLINE, 0.6f);
    p.rect(-6.0f, 2.0f, -3.5f, 4.5f, IC_WARN);
    p.rect(4.2f, -6.0f, 6.8f, 7.0f, IC_METAL, IC_OUTLINE, 0.5f);
    p.circle(5.5f, 7.0f, 1.6f, IC_MAGMA);
    p.circle(5.5f, -8.2f, 2.6f, IC_STEAM);
    p.circle(3.6f, -10.8f, 2.0f, IC_STEAM);
    p.circle(7.2f, -11.6f, 1.8f, IC_STEAM);
}

void iconFusion(const Pen& p) {
    p.circle(0.0f, 0.0f, 9.0f, sf::Color(40, 30, 70), IC_TOKAMAK, 2.6f);
    for (int i = 0; i < 8; ++i) {
        float a = static_cast<float>(i) * 0.785398f;
        p.circle(std::cos(a) * 9.6f, std::sin(a) * 9.6f, 1.3f, IC_METAL_DARK, IC_OUTLINE, 0.3f);
    }
    p.circle(0.0f, 0.0f, 6.2f, sf::Color::Transparent, IC_PLASMA, 1.8f);
    p.circle(0.0f, 0.0f, 6.2f, sf::Color::Transparent, IC_PLASMA_CORE, 0.6f);
    p.rect(-1.3f, -4.0f, 1.3f, 4.0f, IC_METAL, IC_OUTLINE, 0.4f);
}

void iconSpaceSolar(const Pen& p) {
    p.poly({ { -1.2f, -3.0f }, { 1.2f, -3.0f }, { 6.0f, 8.0f }, { -6.0f, 8.0f } }, IC_BEAM);
    for (int side = -1; side <= 1; side += 2) {
        float x0 = (side < 0) ? -11.5f : 3.6f;
        float x1 = (side < 0) ? -3.6f : 11.5f;
        p.rect(x0, -8.5f, x1, -4.5f, IC_PANEL, IC_OUTLINE, 0.5f);
        for (float gx = x0 + 2.6f; gx < x1 - 0.5f; gx += 2.6f) p.line(gx, -8.5f, gx, -4.5f, 0.35f, sf::Color(150, 200, 255));
    }
    p.rect(-2.8f, -9.5f, 2.8f, -3.0f, IC_GOLD, IC_OUTLINE, 0.6f);
    p.rect(-7.5f, 8.0f, 7.5f, 10.5f, IC_METAL_DARK, IC_OUTLINE, 0.5f);
    for (float gx = -6.0f; gx <= 6.0f; gx += 3.0f) p.line(gx, 8.0f, gx, 10.5f, 0.35f, IC_METAL);
}

void iconPumpedHydro(const Pen& p) {
    p.rect(-11.5f, -7.0f, -2.5f, 8.0f, IC_WATER, IC_OUTLINE, 0.5f);
    p.line(-10.5f, -4.0f, -4.0f, -4.0f, 0.5f, IC_WATER_LIGHT);
    p.line(-10.5f, -1.0f, -4.0f, -1.0f, 0.5f, IC_WATER_LIGHT);
    p.rect(1.5f, 4.0f, 11.5f, 8.0f, IC_WATER_LIGHT, IC_OUTLINE, 0.5f);
    p.poly({ { -4.0f, -9.0f }, { -0.8f, -9.0f }, { 3.6f, 9.0f }, { -4.0f, 9.0f } }, IC_CONCRETE, IC_OUTLINE, 0.6f);
    p.line(-3.0f, -6.0f, 0.2f, -6.0f, 0.4f, IC_METAL_DARK);
    p.line(-3.0f, -1.0f, 1.4f, -1.0f, 0.4f, IC_METAL_DARK);
    p.circle(5.2f, 6.0f, 1.8f, sf::Color::White, IC_OUTLINE, 0.4f);
    p.poly({ { 6.5f, -7.0f }, { 10.0f, -4.0f }, { 6.5f, -1.0f } }, IC_WARN); // pump / generate arrow
    p.rect(3.0f, -4.8f, 6.6f, -3.2f, IC_WARN);
}

} // namespace

void drawPowerBuildingIcon(sf::RenderTarget& t, BuildingType b, sf::Vector2f center, float size) {
    Pen p{ t, center, size / 24.0f };
    switch (b) {
        case BuildingType::NUCLEAR:           iconNuclear(p); break;
        case BuildingType::GEOTHERMAL:        iconGeothermal(p); break;
        case BuildingType::MEGA_FUSION:       iconFusion(p); break;
        case BuildingType::MEGA_SPACE_SOLAR:  iconSpaceSolar(p); break;
        case BuildingType::MEGA_PUMPED_HYDRO: iconPumpedHydro(p); break;
        default: break;
    }
}

sf::Color terrainAccentColor(TerrainType terrain) {
    switch (terrain) {
        case TerrainType::RIVER:  return sf::Color(70, 160, 240);
        case TerrainType::VENT:   return sf::Color(255, 150, 60);
        case TerrainType::HILL:   return sf::Color(205, 165, 110);
        case TerrainType::MEADOW: return sf::Color(150, 230, 110);
        case TerrainType::PLAIN:
        default:                  return sf::Color(160, 180, 160);
    }
}

void drawTerrainIcon(sf::RenderTarget& t, TerrainType terrain, sf::Vector2f center, float size) {
    Pen p{ t, center, size / 24.0f };
    switch (terrain) {
        case TerrainType::RIVER:
            for (int row = -1; row <= 1; ++row) {
                float y = row * 6.0f;
                for (int k = 0; k < 4; ++k) {
                    float x0 = -10.0f + k * 5.0f;
                    p.line(x0, y + ((k % 2) ? 1.5f : -1.5f), x0 + 5.0f, y + ((k % 2) ? -1.5f : 1.5f), 2.0f, IC_WATER_LIGHT);
                }
            }
            break;
        case TerrainType::VENT:
            p.poly({ { -9.0f, 10.0f }, { 0.0f, 3.0f }, { 9.0f, 10.0f } }, IC_EARTH);
            p.line(-3.0f, 9.0f, 0.0f, 4.5f, 1.4f, IC_MAGMA);
            p.line(0.0f, 4.5f, 3.0f, 9.0f, 1.4f, IC_MAGMA);
            p.circle(0.0f, -1.0f, 3.6f, IC_STEAM);
            p.circle(-3.0f, -5.5f, 3.0f, IC_STEAM);
            p.circle(2.6f, -8.0f, 2.8f, IC_STEAM);
            break;
        case TerrainType::HILL:
            p.poly({ { -11.0f, 9.0f }, { -3.0f, -6.0f }, { 5.0f, 9.0f } }, sf::Color(120, 100, 72), IC_OUTLINE, 0.6f);
            p.poly({ { -1.0f, 9.0f }, { 5.5f, -1.0f }, { 11.0f, 9.0f } }, sf::Color(86, 128, 70), IC_OUTLINE, 0.6f);
            p.poly({ { -5.2f, -2.0f }, { -3.0f, -6.0f }, { -0.8f, -2.0f } }, sf::Color(240, 244, 250));
            break;
        case TerrainType::MEADOW:
            p.line(0.0f, 2.0f, 0.0f, 10.0f, 1.4f, sf::Color(60, 150, 60));
            p.poly({ { 0.0f, 7.0f }, { 5.0f, 4.0f }, { 1.0f, 8.5f } }, sf::Color(80, 180, 70));
            for (int i = 0; i < 5; ++i) {
                float a = static_cast<float>(i) * 1.2566f - 1.5708f;
                p.circle(std::cos(a) * 3.6f, -3.0f + std::sin(a) * 3.6f, 2.6f, sf::Color(255, 140, 190));
            }
            p.circle(0.0f, -3.0f, 2.2f, IC_WARN);
            break;
        case TerrainType::PLAIN:
        default:
            break;
    }
}

void drawHazardIcon(sf::RenderTarget& t, HazardKind k, sf::Vector2f center, float size) {
    Pen p{ t, center, size / 24.0f };
    switch (k) {
        case HazardKind::HAIL:
            p.circle(-4.0f, -4.0f, 4.5f, sf::Color(150, 160, 178));
            p.circle(2.0f, -6.0f, 5.5f, sf::Color(170, 180, 196));
            p.circle(6.5f, -3.0f, 4.0f, sf::Color(150, 160, 178));
            p.rect(-8.0f, -3.5f, 10.0f, 0.5f, sf::Color(160, 170, 186));
            p.circle(-5.0f, 5.0f, 1.8f, sf::Color::White);
            p.circle(1.0f, 8.0f, 1.8f, sf::Color::White);
            p.circle(6.0f, 4.5f, 1.8f, sf::Color::White);
            break;
        case HazardKind::FLOOD:
            p.rect(-11.0f, 2.0f, 11.0f, 10.0f, IC_WATER);
            p.circle(-6.0f, 2.0f, 4.0f, IC_WATER);
            p.circle(2.0f, 2.0f, 4.0f, IC_WATER);
            p.circle(9.0f, 2.0f, 3.0f, IC_WATER);
            p.circle(-6.0f, 0.5f, 2.0f, IC_WATER_LIGHT);
            p.circle(2.0f, 0.5f, 2.0f, IC_WATER_LIGHT);
            p.poly({ { -2.0f, -9.0f }, { 2.0f, -4.0f }, { -2.0f, -2.0f }, { -6.0f, -4.0f } }, IC_WATER_LIGHT);
            break;
        case HazardKind::WILDFIRE:
            p.poly({ { -8.0f, 10.0f }, { -6.0f, -2.0f }, { 0.0f, -11.0f }, { 6.0f, -2.0f }, { 8.0f, 10.0f } }, sf::Color(235, 80, 30));
            p.poly({ { -4.5f, 10.0f }, { -3.0f, 2.0f }, { 0.0f, -5.0f }, { 3.0f, 2.0f }, { 4.5f, 10.0f } }, sf::Color(255, 170, 40));
            p.poly({ { -2.0f, 10.0f }, { 0.0f, 3.0f }, { 2.0f, 10.0f } }, sf::Color(255, 240, 150));
            break;
        case HazardKind::QUAKE:
            p.rect(-11.0f, -2.0f, 11.0f, 10.0f, IC_EARTH);
            p.line(-9.0f, -2.0f, -4.0f, 3.0f, 1.8f, sf::Color(30, 20, 14));
            p.line(-4.0f, 3.0f, 0.0f, 0.0f, 1.8f, sf::Color(30, 20, 14));
            p.line(0.0f, 0.0f, 4.0f, 7.0f, 1.8f, sf::Color(30, 20, 14));
            p.line(4.0f, 7.0f, 9.0f, 4.0f, 1.8f, sf::Color(30, 20, 14));
            p.line(-8.0f, -8.0f, -5.0f, -5.0f, 1.0f, IC_WARN);
            p.line(8.0f, -8.0f, 5.0f, -5.0f, 1.0f, IC_WARN);
            p.line(0.0f, -10.0f, 0.0f, -6.0f, 1.0f, IC_WARN);
            break;
        case HazardKind::NONE:
            break;
    }
}
