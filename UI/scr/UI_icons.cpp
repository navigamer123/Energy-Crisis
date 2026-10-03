#include "../includes/UI_icons.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <vector>

// All icon geometry is written in a normalised box: x and y run from -0.5 to
// +0.5 (y points down) and are scaled by `size` around `center`.

namespace {

using Pts = std::vector<sf::Vector2f>;

struct Pen {
    sf::RenderTarget& t;
    sf::Vector2f c;
    float s;     // icon size in px
    float edge;  // outline thickness in px
};

sf::Vector2f at(const Pen& p, float x, float y) {
    return { p.c.x + x * p.s, p.c.y + y * p.s };
}

Pts box(float x0, float y0, float x1, float y1) {
    return { { x0, y0 }, { x1, y0 }, { x1, y1 }, { x0, y1 } };
}

sf::ConvexShape makePoly(const Pen& p, const Pts& pts) {
    sf::ConvexShape sh(pts.size());
    for (std::size_t i = 0; i < pts.size(); ++i) {
        sh.setPoint(i, at(p, pts[i].x, pts[i].y));
    }
    return sh;
}

void fillPoly(const Pen& p, const Pts& pts, sf::Color fill) {
    sf::ConvexShape sh = makePoly(p, pts);
    sh.setFillColor(fill);
    p.t.draw(sh);
}

// One silhouette made of convex pieces: every outline is drawn first, then
// every fill, so overlapping pieces read as one shape without inner seams.
void solid(const Pen& p, std::initializer_list<Pts> pieces, sf::Color fill, sf::Color edge) {
    for (const Pts& pts : pieces) {
        sf::ConvexShape sh = makePoly(p, pts);
        sh.setFillColor(edge);
        sh.setOutlineColor(edge);
        sh.setOutlineThickness(p.edge);
        p.t.draw(sh);
    }
    for (const Pts& pts : pieces) {
        fillPoly(p, pts, fill);
    }
}

// Straight stroke from (x0,y0) to (x1,y1); w is in normalised units, at least 1 px.
void line(const Pen& p, float x0, float y0, float x1, float y1, float w, sf::Color col) {
    sf::Vector2f a = at(p, x0, y0);
    sf::Vector2f b = at(p, x1, y1);
    sf::Vector2f d = b - a;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len <= 0.0f) return;
    float hw = std::max(0.5f, w * p.s * 0.5f);
    sf::Vector2f n(-d.y / len * hw, d.x / len * hw);
    sf::ConvexShape q(4);
    q.setPoint(0, a + n);
    q.setPoint(1, b + n);
    q.setPoint(2, b - n);
    q.setPoint(3, a - n);
    q.setFillColor(col);
    p.t.draw(q);
}

// Stroke with an outline of the given edge colour.
void stroke(const Pen& p, float x0, float y0, float x1, float y1, float w, sf::Color fill, sf::Color edge) {
    line(p, x0, y0, x1, y1, w + 2.0f * p.edge / p.s, edge);
    line(p, x0, y0, x1, y1, w, fill);
}

// Circle; a negative edgePx draws the outline inwards (useful for rings).
void disc(const Pen& p, float x, float y, float r, sf::Color fill,
          sf::Color edge = sf::Color::Transparent, float edgePx = 0.0f) {
    float rp = r * p.s;
    std::size_t points = static_cast<std::size_t>(std::clamp(rp * 1.5f, 10.0f, 48.0f));
    sf::CircleShape c(rp, points);
    c.setOrigin({ rp, rp });
    c.setPosition(at(p, x, y));
    c.setFillColor(fill);
    if (edgePx != 0.0f) {
        c.setOutlineThickness(edgePx);
        c.setOutlineColor(edge);
    }
    p.t.draw(c);
}

// Point (u along the blade, v across it) rotated by `deg` around (cx, cy).
sf::Vector2f rotated(float cx, float cy, float u, float v, float deg) {
    float a = deg * 3.14159265f / 180.0f;
    float cs = std::cos(a);
    float sn = std::sin(a);
    return { cx + u * cs - v * sn, cy + u * sn + v * cs };
}

// -----------------------------------------------------------------------------
// Resources
// -----------------------------------------------------------------------------

void iconWood(const Pen& p) {
    // Pine tree: two stacked tiers over a trunk.
    solid(p, { box(-0.08f, 0.16f, 0.08f, 0.46f) }, sf::Color(125, 82, 45), sf::Color(70, 45, 25));
    solid(p, { { { 0.0f, -0.46f }, { -0.27f, -0.04f }, { 0.27f, -0.04f } },
               { { 0.0f, -0.25f }, { -0.39f, 0.24f }, { 0.39f, 0.24f } } },
          sf::Color(70, 195, 100), sf::Color(25, 95, 50));
}

void iconIron(const Pen& p) {
    // Ingot: wide trapezoid with a lighter top face.
    solid(p, { { { -0.26f, -0.34f }, { 0.26f, -0.34f }, { 0.46f, 0.32f }, { -0.46f, 0.32f } } },
          sf::Color(140, 160, 186), sf::Color(62, 76, 96));
    fillPoly(p, { { -0.26f, -0.34f }, { 0.26f, -0.34f }, { 0.33f, -0.1f }, { -0.33f, -0.1f } },
             sf::Color(215, 228, 242));
}

void iconCopper(const Pen& p) {
    // Wire spool: orange windings between two flanges, with a loose wire end.
    solid(p, { box(-0.2f, -0.3f, 0.2f, 0.3f) }, sf::Color(230, 140, 70), sf::Color(120, 60, 25));
    sf::Color winding(165, 80, 32);
    for (float y : { -0.15f, 0.0f, 0.15f }) {
        line(p, -0.2f, y, 0.2f, y, 0.035f, winding);
    }
    solid(p, { box(-0.36f, -0.44f, 0.36f, -0.28f), box(-0.36f, 0.28f, 0.36f, 0.44f) },
          sf::Color(150, 92, 55), sf::Color(80, 45, 25));
    stroke(p, 0.2f, 0.1f, 0.46f, 0.2f, 0.07f, sf::Color(245, 165, 95), sf::Color(120, 60, 25));
}

void iconCoal(const Pen& p) {
    // Faceted dark rock with a light rim so it stays visible on dark panels.
    solid(p, { { { -0.1f, -0.42f }, { 0.28f, -0.32f }, { 0.44f, 0.02f }, { 0.3f, 0.38f },
                 { -0.18f, 0.42f }, { -0.44f, 0.12f }, { -0.36f, -0.22f } } },
          sf::Color(62, 66, 76), sf::Color(165, 172, 185));
    fillPoly(p, { { -0.1f, -0.42f }, { 0.28f, -0.32f }, { 0.06f, -0.06f }, { -0.36f, -0.22f } },
             sf::Color(108, 115, 128));
    fillPoly(p, { { 0.06f, -0.06f }, { 0.44f, 0.02f }, { 0.3f, 0.38f } }, sf::Color(44, 47, 55));
}

void iconSilicon(const Pen& p) {
    // Microchip: square body, cyan die and pins on every side.
    sf::Color pin(150, 215, 235);
    const int pins = (p.s < 22.0f) ? 2 : 3;
    for (int i = 0; i < pins; ++i) {
        float k = (pins == 2) ? (i == 0 ? -0.12f : 0.12f) : (-0.16f + 0.16f * static_cast<float>(i));
        line(p, -0.44f, k, -0.26f, k, 0.07f, pin);
        line(p, 0.26f, k, 0.44f, k, 0.07f, pin);
        line(p, k, -0.44f, k, -0.26f, 0.07f, pin);
        line(p, k, 0.26f, k, 0.44f, 0.07f, pin);
    }
    solid(p, { box(-0.28f, -0.28f, 0.28f, 0.28f) }, sf::Color(18, 44, 66), sf::Color(0, 220, 255));
    fillPoly(p, box(-0.13f, -0.13f, 0.13f, 0.13f), sf::Color(0, 220, 255));
}

void iconSilver(const Pen& p) {
    // Cut gem: light crown, darker pavilion and facet lines.
    sf::Color facet(125, 140, 165);
    solid(p, { { { -0.22f, -0.32f }, { 0.22f, -0.32f }, { 0.44f, -0.06f }, { 0.0f, 0.42f }, { -0.44f, -0.06f } } },
          sf::Color(196, 208, 226), sf::Color(105, 120, 145));
    fillPoly(p, { { -0.22f, -0.32f }, { 0.22f, -0.32f }, { 0.44f, -0.06f }, { -0.44f, -0.06f } },
             sf::Color(236, 242, 250));
    line(p, -0.44f, -0.06f, 0.44f, -0.06f, 0.03f, facet);
    line(p, -0.14f, -0.06f, 0.0f, 0.42f, 0.03f, facet);
    line(p, 0.14f, -0.06f, 0.0f, 0.42f, 0.03f, facet);
    if (p.s >= 24.0f) {
        sf::Color spark(255, 255, 255);
        line(p, 0.36f, -0.49f, 0.36f, -0.29f, 0.04f, spark);
        line(p, 0.26f, -0.39f, 0.46f, -0.39f, 0.04f, spark);
    }
}

void iconGold(const Pen& p) {
    // Coin: filled disc with an embossed inner ring and a shine.
    disc(p, 0.0f, 0.0f, 0.42f, sf::Color(255, 205, 30), sf::Color(150, 105, 0), p.edge);
    disc(p, 0.0f, 0.0f, 0.27f, sf::Color::Transparent, sf::Color(200, 145, 5), -std::max(1.0f, p.edge));
    disc(p, -0.15f, -0.17f, 0.07f, sf::Color(255, 242, 170));
}

void iconMoney(const Pen& p) {
    // Banknote: wide green rectangle with an inner frame and a centre seal.
    solid(p, { box(-0.46f, -0.26f, 0.46f, 0.26f) }, sf::Color(70, 200, 120), sf::Color(25, 100, 55));
    fillPoly(p, box(-0.36f, -0.17f, 0.36f, 0.17f), sf::Color(52, 168, 98));
    disc(p, 0.0f, 0.0f, 0.11f, sf::Color(185, 255, 205));
}

void iconEnergy(const Pen& p) {
    // Lightning bolt built from two convex halves.
    solid(p, { { { 0.1f, -0.46f }, { -0.26f, 0.06f }, { 0.0f, 0.06f }, { 0.06f, -0.1f } },
               { { 0.0f, -0.1f }, { 0.28f, -0.1f }, { -0.1f, 0.46f }, { -0.04f, 0.06f } } },
          sf::Color(255, 225, 40), sf::Color(160, 110, 0));
}

// -----------------------------------------------------------------------------
// Buildings
// -----------------------------------------------------------------------------

void iconSolar(const Pen& p) {
    // Tilted panel with a 3x2 cell grid on a stand, sun behind its corner.
    disc(p, 0.3f, -0.34f, 0.13f, sf::Color(255, 205, 60), sf::Color(190, 130, 20), p.edge);
    sf::Color stand(150, 165, 185);
    line(p, 0.0f, 0.16f, 0.0f, 0.4f, 0.08f, stand);
    line(p, -0.18f, 0.42f, 0.18f, 0.42f, 0.06f, stand);
    solid(p, { { { -0.3f, -0.26f }, { 0.3f, -0.26f }, { 0.44f, 0.16f }, { -0.44f, 0.16f } } },
          sf::Color(25, 95, 215), sf::Color(150, 195, 240));
    sf::Color grid(120, 175, 240);
    for (int k = 1; k <= 2; ++k) {
        float f = static_cast<float>(k) / 3.0f;
        line(p, -0.3f + 0.6f * f, -0.26f, -0.44f + 0.88f * f, 0.16f, 0.035f, grid);
    }
    line(p, -0.37f, -0.05f, 0.37f, -0.05f, 0.035f, grid);
}

void iconWind(const Pen& p) {
    // Three-blade turbine on a tapered mast.
    sf::Color edge(95, 115, 140);
    solid(p, { { { -0.03f, -0.14f }, { 0.03f, -0.14f }, { 0.07f, 0.46f }, { -0.07f, 0.46f } } },
          sf::Color(210, 225, 240), edge);
    const float hx = 0.0f;
    const float hy = -0.16f;
    auto blade = [&](float deg) {
        return Pts{ rotated(hx, hy, 0.0f, -0.065f, deg), rotated(hx, hy, 0.32f, 0.0f, deg),
                    rotated(hx, hy, 0.0f, 0.065f, deg) };
    };
    solid(p, { blade(-90.0f), blade(30.0f), blade(150.0f) }, sf::Color(228, 238, 250), edge);
    disc(p, hx, hy, 0.06f, sf::Color(245, 248, 252), edge, p.edge);
}

void iconHydro(const Pen& p) {
    // Spoked water wheel standing in a river with wave crests.
    sf::Color rim(100, 220, 255);
    disc(p, 0.0f, -0.08f, 0.32f, sf::Color(30, 48, 70), rim, p.edge);
    for (float deg : { 0.0f, 45.0f, 90.0f, 135.0f }) {
        sf::Vector2f a = rotated(0.0f, -0.08f, -0.3f, 0.0f, deg);
        sf::Vector2f b = rotated(0.0f, -0.08f, 0.3f, 0.0f, deg);
        line(p, a.x, a.y, b.x, b.y, 0.04f, rim);
    }
    disc(p, 0.0f, -0.08f, 0.07f, rim);
    solid(p, { box(-0.46f, 0.2f, 0.46f, 0.46f) }, sf::Color(35, 115, 200), sf::Color(20, 70, 130));
    for (float x : { -0.3f, 0.0f, 0.3f }) {
        disc(p, x, 0.22f, 0.09f, sf::Color(120, 205, 255));
    }
}

void iconBattery(const Pen& p) {
    // Upright cell with a terminal cap and green charge bars.
    sf::Color metal(210, 215, 225);
    solid(p, { box(-0.1f, -0.46f, 0.1f, -0.34f) }, metal, sf::Color(110, 120, 135));
    solid(p, { box(-0.24f, -0.36f, 0.24f, 0.46f) }, sf::Color(18, 25, 36), metal);
    sf::Color charge(0, 230, 140);
    if (p.s < 20.0f) {
        fillPoly(p, box(-0.14f, 0.14f, 0.14f, 0.38f), charge);
        fillPoly(p, box(-0.14f, -0.14f, 0.14f, 0.08f), charge);
    } else {
        fillPoly(p, box(-0.15f, 0.24f, 0.15f, 0.38f), charge);
        fillPoly(p, box(-0.15f, 0.06f, 0.15f, 0.2f), charge);
        fillPoly(p, box(-0.15f, -0.12f, 0.15f, 0.02f), charge);
    }
}

void iconLamp(const Pen& p) {
    // Street lamp: pole, arm, lit head and three light rays.
    sf::Color ray(255, 225, 110);
    line(p, 0.22f, -0.16f, 0.1f, 0.12f, 0.05f, ray);
    line(p, 0.26f, -0.16f, 0.26f, 0.2f, 0.05f, ray);
    line(p, 0.3f, -0.16f, 0.42f, 0.12f, 0.05f, ray);
    sf::Color pole(170, 185, 205);
    sf::Color poleEdge(85, 95, 115);
    stroke(p, -0.26f, -0.36f, 0.24f, -0.36f, 0.06f, pole, poleEdge);
    solid(p, { box(-0.3f, -0.38f, -0.22f, 0.42f), box(-0.38f, 0.4f, -0.14f, 0.46f) }, pole, poleEdge);
    solid(p, { { { 0.12f, -0.42f }, { 0.4f, -0.42f }, { 0.34f, -0.24f }, { 0.18f, -0.24f } } },
          sf::Color(255, 235, 120), sf::Color(150, 120, 30));
}

void iconDemolish(const Pen& p) {
    // Hammer: wooden handle and a red head across it.
    stroke(p, -0.34f, 0.38f, 0.12f, -0.08f, 0.11f, sf::Color(160, 112, 66), sf::Color(80, 52, 28));
    stroke(p, 0.02f, -0.28f, 0.3f, 0.0f, 0.17f, sf::Color(230, 95, 95), sf::Color(120, 35, 35));
}

} // namespace

void drawResourceIcon(sf::RenderTarget& t, ResourceType r, sf::Vector2f center, float size) {
    Pen p{ t, center, size, std::max(1.0f, std::floor(size / 20.0f)) };
    switch (r) {
        case ResourceType::WOOD:    iconWood(p); break;
        case ResourceType::IRON:    iconIron(p); break;
        case ResourceType::COPPER:  iconCopper(p); break;
        case ResourceType::COAL:    iconCoal(p); break;
        case ResourceType::SILICON: iconSilicon(p); break;
        case ResourceType::SILVER:  iconSilver(p); break;
        case ResourceType::GOLD:    iconGold(p); break;
        case ResourceType::MONEY:   iconMoney(p); break;
        case ResourceType::ENERGY:  iconEnergy(p); break;
        case ResourceType::NONE:
        case ResourceType::ORE:
            break;
    }
}

void drawBuildingIcon(sf::RenderTarget& t, BuildingType b, sf::Vector2f center, float size) {
    Pen p{ t, center, size, std::max(1.0f, std::floor(size / 20.0f)) };
    switch (b) {
        case BuildingType::SOLAR_PANEL:  iconSolar(p); break;
        case BuildingType::WIND_TURBINE: iconWind(p); break;
        case BuildingType::HYDRO_PLANT:  iconHydro(p); break;
        case BuildingType::BATTERY:      iconBattery(p); break;
        case BuildingType::LAMP:         iconLamp(p); break;
        case BuildingType::DEMOLISH:     iconDemolish(p); break;
        case BuildingType::NONE:
            break;
    }
}

sf::Color resourceIconColor(ResourceType r) {
    switch (r) {
        case ResourceType::WOOD:    return sf::Color(70, 195, 100);
        case ResourceType::IRON:    return sf::Color(170, 190, 215);
        case ResourceType::COPPER:  return sf::Color(230, 140, 70);
        case ResourceType::COAL:    return sf::Color(115, 125, 140);
        case ResourceType::SILICON: return sf::Color(0, 220, 255);
        case ResourceType::SILVER:  return sf::Color(215, 225, 240);
        case ResourceType::GOLD:    return sf::Color(255, 215, 0);
        case ResourceType::MONEY:   return sf::Color(70, 220, 130);
        case ResourceType::ENERGY:  return sf::Color(255, 225, 40);
        case ResourceType::NONE:
        case ResourceType::ORE:
            break;
    }
    return sf::Color::White;
}
