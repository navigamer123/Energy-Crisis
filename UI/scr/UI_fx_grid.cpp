// =============================================================================
// [b-effects] HX-07 Visible power grid.
// Every generator feeds its plot's transformer through a sagging cable; transformers
// share a bus along the gap below their plot row, carried by pylons to a substation on
// the city wall. Pulses travel along every line: their count, speed and size follow the
// MW carried, and a line glows white-hot then red and crackles when it is overloaded.
// Hydro plants also get a water pipe from the canal that feeds them (DS-10 link).
// Geometry is derived from the engine's plots and buildings every frame.
// =============================================================================
#include "../includes/UI_fx.h"
#include "../includes/UI_ease.h"
#include "../includes/UI_types.h"

#include <algorithm>
#include <cmath>

namespace {

// --- Colours (integrator: map to UI_theme tokens) ---
const sf::Color GRID_P1(0, 229, 255);
const sf::Color GRID_P2(255, 120, 200);
const sf::Color GRID_CABLE(30, 34, 44, 235);
const sf::Color GRID_STEEL(92, 102, 118);
const sf::Color GRID_STEEL_DARK(52, 58, 70);
const sf::Color GRID_HOT(255, 250, 225);
const sf::Color GRID_OVERLOAD(255, 90, 40);
const sf::Color GRID_PIPE(36, 70, 104);
const sf::Color GRID_PIPE_WATER(110, 200, 245);
const sf::Color GRID_LAMP_FLOW(255, 220, 120);

// --- Layout (city wall and canals: keep in sync with UI_city::getCityBounds and UI_fx_world.cpp) ---
const float CITY_LEFT = 610.0f, CITY_RIGHT = 990.0f, CITY_TOP = 65.0f, CITY_BOTTOM = 395.0f;
const float WEST_CANAL_X = 603.5f, EAST_CANAL_X = 996.5f;
const float SUBSTATION_DY = 14.0f;   // substation below the city corner for buses under the city
const float BUS_TURN_OFFSET = 30.0f; // vertical riser this far inside the city's x range
const float PYLON_SPACING = 75.0f;
const float LOCAL_CAPACITY_MW = 160.0f;
const float BUS_CAPACITY_MW = 600.0f;

sf::Color ownerColor(int owner) { return owner == 1 ? GRID_P1 : GRID_P2; }

sf::Color lerpColor(sf::Color a, sf::Color b, float t) {
    t = Ease::clamp01(t);
    return sf::Color(static_cast<std::uint8_t>(a.r + (b.r - a.r) * t), static_cast<std::uint8_t>(a.g + (b.g - a.g) * t),
                     static_cast<std::uint8_t>(a.b + (b.b - a.b) * t), static_cast<std::uint8_t>(a.a + (b.a - a.a) * t));
}

sf::Color alpha(sf::Color c, float a) {
    c.a = static_cast<std::uint8_t>(std::max(0.0f, std::min(255.0f, a)));
    return c;
}

std::uint64_t segKey(sf::Vector2f a, sf::Vector2f b) {
    auto q = [](float v) { return static_cast<std::uint64_t>(static_cast<std::int64_t>(std::lround(v * 2.0f)) & 0xFFFF); };
    return (q(a.x) << 48) | (q(a.y) << 32) | (q(b.x) << 16) | q(b.y);
}

sf::Vector2f catenary(sf::Vector2f a, sf::Vector2f b, float sag, float t) {
    return a + (b - a) * t + sf::Vector2f(0.0f, sag * 4.0f * t * (1.0f - t));
}

// Thick line as a quad (two triangles)
void appendLine(sf::VertexArray& va, sf::Vector2f p, sf::Vector2f q, float w, sf::Color c) {
    sf::Vector2f d = q - p;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 0.001f) return;
    sf::Vector2f n(-d.y / len * w * 0.5f, d.x / len * w * 0.5f);
    va.append(sf::Vertex{ p + n, c, {} });
    va.append(sf::Vertex{ p - n, c, {} });
    va.append(sf::Vertex{ q + n, c, {} });
    va.append(sf::Vertex{ q + n, c, {} });
    va.append(sf::Vertex{ p - n, c, {} });
    va.append(sf::Vertex{ q - n, c, {} });
}

void appendCircle(sf::VertexArray& va, sf::Vector2f c, float r, sf::Color col, int segs = 10) {
    for (int i = 0; i < segs; ++i) {
        float a0 = 6.2831853f * static_cast<float>(i) / static_cast<float>(segs);
        float a1 = 6.2831853f * static_cast<float>(i + 1) / static_cast<float>(segs);
        va.append(sf::Vertex{ c, col, {} });
        va.append(sf::Vertex{ c + sf::Vector2f(std::cos(a0) * r, std::sin(a0) * r), col, {} });
        va.append(sf::Vertex{ c + sf::Vector2f(std::cos(a1) * r, std::sin(a1) * r), col, {} });
    }
}

void appendRect(sf::VertexArray& va, sf::Vector2f pos, sf::Vector2f size, sf::Color c) {
    sf::Vector2f a = pos, b = pos + sf::Vector2f(size.x, 0.0f), d = pos + sf::Vector2f(0.0f, size.y), e = pos + size;
    va.append(sf::Vertex{ a, c, {} });
    va.append(sf::Vertex{ b, c, {} });
    va.append(sf::Vertex{ d, c, {} });
    va.append(sf::Vertex{ d, c, {} });
    va.append(sf::Vertex{ b, c, {} });
    va.append(sf::Vertex{ e, c, {} });
}

// Where the cable attaches on each building type (above its base point)
sf::Vector2f attachPoint(const PlacedBuilding& b) {
    switch (b.type) {
        case BuildingType::WIND_TURBINE: return b.position + sf::Vector2f(0.0f, -10.0f);
        case BuildingType::LAMP: return b.position + sf::Vector2f(0.0f, -12.0f);
        default: return b.position + sf::Vector2f(0.0f, -2.0f);
    }
}

float pulseSpacing(float mw) { return std::max(24.0f, std::min(110.0f, 110.0f - mw * 0.14f)); }
float pulseSpeed(float mw) { return 38.0f + std::min(mw, 700.0f) * 0.32f; }

} // namespace

void UI_fx::rebuildGrid() {
    gridSegs.clear();
    pylons.clear();
    transformers.clear();
    hydroPipes.clear();

    for (int owner = 1; owner <= 2; ++owner) {
        struct BusNode { float x; float y; float mw; };
        std::vector<BusNode> nodes;

        for (const auto& plot : plots) {
            if (plot.playerOwner != owner || !plot.isPurchased) continue;
            float mw = 0.0f;
            bool any = false;
            sf::Vector2f tPos(owner == 1 ? plot.bounds.position.x - 6.0f : plot.bounds.position.x + plot.bounds.size.x + 6.0f,
                              plot.bounds.position.y + plot.bounds.size.y + 5.0f);
            sf::Vector2f tTop = tPos + sf::Vector2f(0.0f, -10.0f);
            for (const auto& b : buildings) {
                if (b.playerOwner != owner || !plot.bounds.contains(b.position)) continue;
                any = true;
                sf::Vector2f at = attachPoint(b);
                float span = std::sqrt((at.x - tTop.x) * (at.x - tTop.x) + (at.y - tTop.y) * (at.y - tTop.y));
                GridSeg s;
                s.owner = owner;
                s.sag = 2.0f + span * 0.05f;
                s.capacity = LOCAL_CAPACITY_MW;
                if (b.type == BuildingType::LAMP) {
                    // Consumer: power flows from the transformer to the lamp while it is lit
                    s.a = tTop;
                    s.b = at;
                    s.mw = (b.lightRadius > 0.0f && !b.isBroken) ? 10.0f : 0.0f;
                    s.reverse = false;
                } else {
                    s.a = at;
                    s.b = tTop;
                    s.mw = std::max(0.0f, b.currentOutputMW);
                    s.reverse = false;
                    mw += s.mw;
                }
                s.key = segKey(s.a, s.b);
                gridSegs.push_back(s);

                if (b.type == BuildingType::HYDRO_PLANT) {
                    float canalX = (owner == 1) ? WEST_CANAL_X : EAST_CANAL_X;
                    if (std::fabs(canalX - b.position.x) < 160.0f) {
                        hydroPipes.push_back({ sf::Vector2f(canalX, b.position.y + 4.0f), b.position + sf::Vector2f(0.0f, 4.0f), owner,
                                               std::max(0.0f, b.currentOutputMW) });
                    }
                }
            }
            if (!any) continue;
            transformers.push_back({ tPos, owner, mw }); // pole-mounted: also carries the bus
            nodes.push_back({ tPos.x, tPos.y, mw });
        }

        // One bus per row gap, running towards the city and collecting each transformer's power
        std::sort(nodes.begin(), nodes.end(), [owner](const BusNode& a, const BusNode& b) {
            if (std::fabs(a.y - b.y) > 1.0f) return a.y < b.y;
            return owner == 1 ? a.x < b.x : a.x > b.x;
        });
        std::size_t i = 0;
        while (i < nodes.size()) {
            std::size_t j = i;
            while (j < nodes.size() && std::fabs(nodes[j].y - nodes[i].y) <= 1.0f) ++j;
            float y = nodes[i].y;
            // Route: along the gap to the city wall, or (gap below the city) to a riser and up to the substation
            std::vector<sf::Vector2f> route;
            std::vector<float> addAt; // power added at each route point
            const std::size_t transformerCount = j - i;
            for (std::size_t k = i; k < j; ++k) {
                route.push_back({ nodes[k].x, y });
                addAt.push_back(nodes[k].mw);
            }
            bool underCity = !(y > CITY_TOP + 25.0f && y < CITY_BOTTOM - 12.0f);
            float wallX = (owner == 1) ? CITY_LEFT - 1.0f : CITY_RIGHT + 1.0f;
            if (!underCity) {
                route.push_back({ wallX, y });
                addAt.push_back(0.0f);
            } else {
                float riserX = (owner == 1) ? CITY_LEFT + BUS_TURN_OFFSET : CITY_RIGHT - BUS_TURN_OFFSET;
                route.push_back({ riserX, y });
                addAt.push_back(0.0f);
                route.push_back({ riserX, CITY_BOTTOM + SUBSTATION_DY });
                addAt.push_back(0.0f);
            }
            // Insert intermediate pylons on long spans, then emit segments with cumulative power
            float carried = 0.0f;
            for (std::size_t k = 0; k + 1 < route.size(); ++k) {
                carried += addAt[k];
                sf::Vector2f a = route[k], b = route[k + 1];
                float len = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
                int pieces = std::max(1, static_cast<int>(std::ceil(len / PYLON_SPACING)));
                bool horizontal = std::fabs(b.y - a.y) < 1.0f;
                for (int p = 0; p < pieces; ++p) {
                    sf::Vector2f pa = a + (b - a) * (static_cast<float>(p) / static_cast<float>(pieces));
                    sf::Vector2f pb = a + (b - a) * (static_cast<float>(p + 1) / static_cast<float>(pieces));
                    bool endsAtWall = (k + 2 == route.size()) && (p == pieces - 1);
                    // Pylons stand on intermediate points (never on the canals or inside the city)
                    if (p > 0) {
                        bool onCanal = std::fabs(pa.x - WEST_CANAL_X) < 7.0f || std::fabs(pa.x - EAST_CANAL_X) < 7.0f;
                        if (!onCanal) pylons.push_back({ pa, 10.0f, owner });
                    }
                    if (p == 0 && k >= transformerCount) pylons.push_back({ pa, 10.0f, owner }); // riser corner
                    GridSeg s;
                    s.a = pa + sf::Vector2f(0.0f, -10.0f);
                    s.b = endsAtWall && !underCity ? pb + sf::Vector2f(0.0f, -4.0f) : pb + sf::Vector2f(0.0f, -10.0f);
                    s.sag = horizontal ? 2.5f : 1.0f; // keeps bus cables inside the 10 px row gap
                    s.mw = carried;
                    s.capacity = BUS_CAPACITY_MW;
                    s.owner = owner;
                    s.reverse = false;
                    s.key = segKey(s.a, s.b);
                    gridSegs.push_back(s);
                }
            }
            i = j;
        }
    }
}

void UI_fx::advancePulses(float dt) {
    std::map<std::uint64_t, float> next;
    for (const auto& s : gridSegs) {
        float ph = 0.0f;
        auto it = pulsePhase.find(s.key);
        if (it != pulsePhase.end()) ph = it->second;
        if (s.mw > 0.5f) ph += pulseSpeed(s.mw) * dt / pulseSpacing(s.mw);
        ph = std::fmod(ph, 1000.0f);
        next[s.key] = ph;
    }
    for (const auto& p : hydroPipes) {
        std::uint64_t k = segKey(p.from, p.to) ^ 0x5A5A5A5A5A5A5A5Aull;
        float ph = 0.0f;
        auto it = pulsePhase.find(k);
        if (it != pulsePhase.end()) ph = it->second;
        ph = std::fmod(ph + dt * (0.6f + p.mw / 120.0f), 1000.0f);
        next[k] = ph;
    }
    pulsePhase.swap(next);
}

void UI_fx::drawPowerGrid(sf::RenderTarget& target) const {
    // 1. Hydro water pipes (canal -> plant), water dashes flowing towards the plant
    sf::VertexArray pipes(sf::PrimitiveType::Triangles);
    for (const auto& p : hydroPipes) {
        appendLine(pipes, p.from, p.to, 5.0f, GRID_PIPE);
        appendLine(pipes, p.from, p.to, 2.0f, alpha(GRID_PIPE_WATER, 120.0f));
        std::uint64_t k = segKey(p.from, p.to) ^ 0x5A5A5A5A5A5A5A5Aull;
        auto it = pulsePhase.find(k);
        float ph = (it != pulsePhase.end()) ? it->second : 0.0f;
        sf::Vector2f d = p.to - p.from;
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len > 4.0f && p.mw > 0.5f) {
            int n = std::max(1, static_cast<int>(len / 14.0f));
            for (int i = 0; i < n; ++i) {
                float u = std::fmod((static_cast<float>(i) + ph * 3.0f) / static_cast<float>(n), 1.0f);
                sf::Vector2f c = p.from + d * u;
                appendLine(pipes, c - d / len * 2.5f, c + d / len * 2.5f, 2.0f, alpha(GRID_PIPE_WATER, 230.0f));
            }
        }
    }
    target.draw(pipes);

    // 2. Cables (dark catenary with a thin owner-coloured highlight)
    sf::VertexArray cables(sf::PrimitiveType::Triangles);
    const int STEPS = 10;
    for (const auto& s : gridSegs) {
        sf::Vector2f prev = s.a;
        for (int i = 1; i <= STEPS; ++i) {
            sf::Vector2f cur = catenary(s.a, s.b, s.sag, static_cast<float>(i) / static_cast<float>(STEPS));
            appendLine(cables, prev, cur, 1.8f, GRID_CABLE);
            appendLine(cables, prev + sf::Vector2f(0.0f, -0.5f), cur + sf::Vector2f(0.0f, -0.5f), 0.8f, alpha(ownerColor(s.owner), 95.0f));
            prev = cur;
        }
    }

    // 3. Pylons (lattice towers) and transformers / substations
    for (const auto& p : pylons) {
        sf::Vector2f top = p.base + sf::Vector2f(0.0f, -p.height);
        appendLine(cables, p.base + sf::Vector2f(-3.0f, 0.0f), top, 1.4f, GRID_STEEL_DARK);
        appendLine(cables, p.base + sf::Vector2f(3.0f, 0.0f), top, 1.4f, GRID_STEEL_DARK);
        appendLine(cables, p.base + sf::Vector2f(-2.0f, -4.0f), p.base + sf::Vector2f(2.0f, -4.0f), 1.0f, GRID_STEEL);
        appendLine(cables, top + sf::Vector2f(-5.0f, 1.5f), top + sf::Vector2f(5.0f, 1.5f), 1.6f, GRID_STEEL);
        appendCircle(cables, top + sf::Vector2f(-4.5f, 1.5f), 1.1f, alpha(ownerColor(p.owner), 220.0f), 6);
        appendCircle(cables, top + sf::Vector2f(4.5f, 1.5f), 1.1f, alpha(ownerColor(p.owner), 220.0f), 6);
    }
    for (const auto& t : transformers) {
        // Pole transformer: pole, crossarm and a small tank in the owner colour
        appendLine(cables, t.pos, t.pos + sf::Vector2f(0.0f, -10.0f), 1.6f, GRID_STEEL_DARK);
        appendLine(cables, t.pos + sf::Vector2f(-4.0f, -9.0f), t.pos + sf::Vector2f(4.0f, -9.0f), 1.4f, GRID_STEEL);
        appendRect(cables, t.pos + sf::Vector2f(-3.0f, -7.0f), { 6.0f, 6.0f }, sf::Color(40, 46, 58));
        appendRect(cables, t.pos + sf::Vector2f(-3.0f, -7.0f), { 6.0f, 1.5f }, ownerColor(t.owner));
    }
    // Substations where the buses meet the city
    for (const auto& s : gridSegs) {
        bool atWall = std::fabs(s.b.x - (CITY_LEFT - 1.0f)) < 0.5f || std::fabs(s.b.x - (CITY_RIGHT + 1.0f)) < 0.5f;
        bool atRiser = std::fabs(s.b.y - (CITY_BOTTOM + SUBSTATION_DY - 10.0f)) < 0.5f;
        if (!atWall && !atRiser) continue;
        sf::Vector2f c = atWall ? s.b + sf::Vector2f(s.owner == 1 ? -3.0f : 3.0f, 0.0f) : s.b + sf::Vector2f(0.0f, 6.0f);
        appendRect(cables, c + sf::Vector2f(-5.0f, -5.0f), { 10.0f, 10.0f }, sf::Color(34, 40, 52));
        appendRect(cables, c + sf::Vector2f(-5.0f, -5.0f), { 10.0f, 2.0f }, ownerColor(s.owner));
        appendCircle(cables, c + sf::Vector2f(0.0f, 1.5f), 2.2f, GRID_STEEL, 8);
    }
    target.draw(cables);
}

void UI_fx::drawEmissive(sf::RenderTarget& target) const {
    sf::VertexArray glow(sf::PrimitiveType::Triangles);
    sf::VertexArray core(sf::PrimitiveType::Triangles);
    float nightBoost = 0.65f + 0.35f * night;

    for (const auto& s : gridSegs) {
        if (s.mw <= 0.5f) continue;
        auto it = pulsePhase.find(s.key);
        float ph = (it != pulsePhase.end()) ? it->second : 0.0f;
        sf::Vector2f d = s.b - s.a;
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len < 4.0f) continue;
        float spacing = pulseSpacing(s.mw);
        int n = std::max(1, static_cast<int>(len / spacing));
        float load = s.mw / s.capacity;
        sf::Color base = ownerColor(s.owner);
        sf::Color col = (load < 0.7f) ? lerpColor(base, GRID_HOT, load / 0.7f * 0.6f)
                                      : lerpColor(lerpColor(base, GRID_HOT, 0.6f), GRID_OVERLOAD, (load - 0.7f) / 0.3f);
        bool overloaded = load > 1.0f;
        float flicker = overloaded ? (0.7f + 0.3f * std::sin(worldTime * 40.0f + static_cast<float>(s.key & 31u))) : 1.0f;
        float r = 1.6f + std::min(s.mw, 500.0f) / 170.0f;
        for (int i = 0; i < n; ++i) {
            float u = std::fmod((static_cast<float>(i) + ph) / static_cast<float>(n), 1.0f);
            if (s.reverse) u = 1.0f - u;
            sf::Vector2f p = catenary(s.a, s.b, s.sag, u);
            appendCircle(glow, p, r * 2.8f, alpha(col, 70.0f * nightBoost * flicker), 10);
            appendCircle(core, p, r, alpha(lerpColor(col, sf::Color::White, 0.35f), 235.0f * flicker), 8);
        }
        if (overloaded) {
            // Crackle: short zig-zag sparks hopping along the line
            int sparks = 1 + static_cast<int>(std::min(3.0f, (load - 1.0f) * 6.0f));
            for (int k = 0; k < sparks; ++k) {
                float h = std::fmod(std::floor(worldTime * 18.0f) * 0.6180339f + static_cast<float>(k) * 0.37f + static_cast<float>(s.key % 97u) * 0.01f, 1.0f);
                sf::Vector2f p = catenary(s.a, s.b, s.sag, h);
                appendLine(core, p + sf::Vector2f(-3.0f, -2.0f), p + sf::Vector2f(1.0f, 1.0f), 1.2f, alpha(GRID_HOT, 240.0f));
                appendLine(core, p + sf::Vector2f(1.0f, 1.0f), p + sf::Vector2f(-1.0f, 4.0f), 1.2f, alpha(GRID_OVERLOAD, 220.0f));
            }
        }
    }
    // Transformer hum
    for (const auto& t : transformers) {
        if (t.mw <= 0.5f) continue;
        float a = std::min(1.0f, t.mw / 300.0f) * (0.6f + 0.4f * std::sin(worldTime * 6.0f + t.pos.x));
        appendCircle(glow, t.pos + sf::Vector2f(0.0f, -3.0f), 9.0f, alpha(ownerColor(t.owner), 60.0f * a * nightBoost), 12);
    }
    // Lamp halos at night
    if (night > 0.05f) {
        for (const auto& b : buildings) {
            if (b.type != BuildingType::LAMP || b.lightRadius <= 0.0f || b.isBroken) continue;
            sf::Vector2f head = b.position + sf::Vector2f(0.0f, -24.0f);
            appendCircle(glow, head, 16.0f, alpha(GRID_LAMP_FLOW, 70.0f * night), 14);
            appendCircle(glow, head, 8.0f, alpha(GRID_LAMP_FLOW, 90.0f * night), 12);
        }
    }
    sf::RenderStates add;
    add.blendMode = sf::BlendAdd;
    target.draw(glow, add);
    target.draw(core, add);
}
