// =============================================================================
// [b-effects] World art: DS-10 river, canals, bridges and traffic.
// River: runs the full height of the map along the central border (x = 800), with two
// canals along the city walls that feed the hydro bank plots, two road bridges below the
// city and animated traffic (headlights at night). It freezes over in winter.
// =============================================================================
#include "../includes/UI_fx.h"
#include "../includes/UI_ease.h"
#include "../includes/UI_types.h"

#include <algorithm>
#include <cmath>

namespace {

// --- Colours (integrator: map to UI_theme tokens) ---
const sf::Color WATER_DEEP(26, 66, 108);
const sf::Color WATER_MID(38, 98, 150);
const sf::Color WATER_GLINT(150, 215, 245);
const sf::Color BANK_EARTH(74, 84, 58);
const sf::Color BANK_STONE(96, 102, 112);
const sf::Color ICE(196, 220, 238);
const sf::Color ICE_CRACK(140, 172, 200);
const sf::Color SNOW_WHITE(242, 247, 255);
const sf::Color ROAD(56, 60, 68);
const sf::Color ROAD_EDGE(96, 100, 110);
const sf::Color ROAD_LINE(232, 220, 150);
const sf::Color BRIDGE_DECK(104, 98, 92);
const sf::Color BRIDGE_RAIL(196, 190, 178);
const sf::Color LAMP_HEAD(255, 236, 170);
const sf::Color TAG_BG(15, 20, 32, 245);
const sf::Color TAG_BORDER(0, 220, 100, 200);
const sf::Color TAG_TEXT(140, 255, 180);
const sf::Color CAR_COLORS[5] = { sf::Color(0, 200, 240), sf::Color(250, 120, 190), sf::Color(235, 235, 240), sf::Color(250, 200, 60), sf::Color(220, 70, 60) };

// --- Layout ---
const float RIVER_X = 800.0f;
const float RIVER_HALF = 13.0f;
const float WEST_CANAL_X = 603.5f, EAST_CANAL_X = 996.5f, CANAL_HALF = 3.5f;
const float CANAL_TOP = 100.0f, CANAL_JOIN_Y = 440.0f;
const float ROAD_Y[2] = { 467.0f, 665.0f };
const float ROAD_X0[2] = { 600.0f, 612.0f };
const float ROAD_X1[2] = { 1000.0f, 994.0f };
const float ROAD_HALF = 7.0f;

sf::Color alpha(sf::Color c, float a) {
    c.a = static_cast<std::uint8_t>(std::max(0.0f, std::min(255.0f, a)));
    return c;
}

void quad(sf::VertexArray& va, sf::Vector2f pos, sf::Vector2f size, sf::Color c) {
    sf::Vector2f a = pos, b = pos + sf::Vector2f(size.x, 0.0f), d = pos + sf::Vector2f(0.0f, size.y), e = pos + size;
    va.append(sf::Vertex{ a, c, {} });
    va.append(sf::Vertex{ b, c, {} });
    va.append(sf::Vertex{ d, c, {} });
    va.append(sf::Vertex{ d, c, {} });
    va.append(sf::Vertex{ b, c, {} });
    va.append(sf::Vertex{ e, c, {} });
}

void line(sf::VertexArray& va, sf::Vector2f p, sf::Vector2f q, float w, sf::Color c) {
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

// Deterministic hash noise (ice cracks, snow drifts)
float hash01(std::uint32_t n) {
    n = (n << 13) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return static_cast<float>(n & 0x7fffffffu) / 2147483647.0f;
}

} // namespace

// ---------------------------------------------------------------------------
// DS-10 River, canals, bridges and traffic
// ---------------------------------------------------------------------------
void UI_fx::initTraffic() {
    cars.clear();
    for (int road = 0; road < 2; ++road) {
        for (int i = 0; i < 4; ++i) {
            Car c;
            c.road = road;
            c.dir = (i % 2 == 0) ? 1 : -1;
            float span = ROAD_X1[road] - ROAD_X0[road];
            c.x = ROAD_X0[road] + span * (0.12f + 0.25f * static_cast<float>(i)) ;
            c.speed = 42.0f + 9.0f * static_cast<float>((i * 3 + road) % 4);
            c.body = CAR_COLORS[(i + road * 2) % 5];
            cars.push_back(c);
        }
    }
}

void UI_fx::advanceTraffic(float dt) {
    for (auto& c : cars) {
        float slow = (seasonW[3] > 0.5f) ? 0.7f : 1.0f; // careful on winter roads
        c.x += static_cast<float>(c.dir) * c.speed * slow * dt;
        if (c.dir > 0 && c.x > ROAD_X1[c.road] + 6.0f) c.x = ROAD_X0[c.road] - 6.0f;
        if (c.dir < 0 && c.x < ROAD_X0[c.road] - 6.0f) c.x = ROAD_X1[c.road] + 6.0f;
    }
}

void UI_fx::drawRiver(sf::RenderTarget& target) const {
    const float wWinter = seasonW[3];
    const float ice = Ease::clamp01((wWinter - 0.2f) / 0.8f);
    sf::VertexArray va(sf::PrimitiveType::Triangles);

    // Banks and water: main river (full height), canals along the city walls and the joining channels
    auto channelV = [&](float x, float y0, float y1, float half, sf::Color bank) {
        quad(va, { x - half - 2.0f, y0 }, { half * 2.0f + 4.0f, y1 - y0 }, bank);
        quad(va, { x - half, y0 }, { half * 2.0f, y1 - y0 }, WATER_DEEP);
        quad(va, { x - half * 0.55f, y0 }, { half * 1.1f, y1 - y0 }, WATER_MID);
    };
    auto channelH = [&](float y, float x0, float x1, float half, sf::Color bank) {
        quad(va, { x0, y - half - 2.0f }, { x1 - x0, half * 2.0f + 4.0f }, bank);
        quad(va, { x0, y - half }, { x1 - x0, half * 2.0f }, WATER_DEEP);
        quad(va, { x0, y - half * 0.55f }, { x1 - x0, half * 1.1f }, WATER_MID);
    };
    channelV(RIVER_X, 0.0f, VIRTUAL_HEIGHT, RIVER_HALF, BANK_EARTH);
    channelV(WEST_CANAL_X, CANAL_TOP, CANAL_JOIN_Y + CANAL_HALF, CANAL_HALF, BANK_STONE);
    channelV(EAST_CANAL_X, CANAL_TOP, CANAL_JOIN_Y + CANAL_HALF, CANAL_HALF, BANK_STONE);
    channelH(CANAL_JOIN_Y, WEST_CANAL_X - CANAL_HALF, RIVER_X - RIVER_HALF, CANAL_HALF, BANK_STONE);
    channelH(CANAL_JOIN_Y, RIVER_X + RIVER_HALF, EAST_CANAL_X + CANAL_HALF, CANAL_HALF, BANK_STONE);
    // Re-open the river where the joining channels meet it
    quad(va, { RIVER_X - RIVER_HALF - 2.0f, CANAL_JOIN_Y - CANAL_HALF }, { 2.0f, CANAL_HALF * 2.0f }, WATER_DEEP);
    quad(va, { RIVER_X + RIVER_HALF, CANAL_JOIN_Y - CANAL_HALF }, { 2.0f, CANAL_HALF * 2.0f }, WATER_DEEP);

    // Flow: light streaks drift downstream (river down, canals towards the river)
    float flowA = 150.0f * (1.0f - ice);
    if (flowA > 1.0f) {
        for (int lane = 0; lane < 3; ++lane) {
            float lx = RIVER_X - 7.0f + static_cast<float>(lane) * 7.0f;
            for (int i = 0; i < 16; ++i) {
                float y = std::fmod(worldTime * (34.0f + 6.0f * static_cast<float>(lane)) + static_cast<float>(i) * 61.0f + static_cast<float>(lane) * 23.0f, VIRTUAL_HEIGHT + 20.0f) - 10.0f;
                line(va, { lx, y }, { lx, y + 7.0f }, 1.5f, alpha(WATER_GLINT, flowA * 0.7f));
            }
        }
        for (float cx : { WEST_CANAL_X, EAST_CANAL_X }) {
            for (int i = 0; i < 7; ++i) {
                float y = CANAL_TOP + std::fmod(worldTime * 22.0f + static_cast<float>(i) * 50.0f, CANAL_JOIN_Y - CANAL_TOP - 6.0f);
                line(va, { cx, y }, { cx, y + 5.0f }, 1.2f, alpha(WATER_GLINT, flowA * 0.6f));
            }
            float dir = (cx < RIVER_X) ? 1.0f : -1.0f;
            float x0 = (cx < RIVER_X) ? cx : RIVER_X + RIVER_HALF;
            float len = (cx < RIVER_X) ? (RIVER_X - RIVER_HALF - cx) : (cx - RIVER_X - RIVER_HALF);
            for (int i = 0; i < 6; ++i) {
                float t = std::fmod(worldTime * 24.0f + static_cast<float>(i) * 33.0f, len);
                float x = (dir > 0.0f) ? x0 + t : x0 + len - t;
                line(va, { x, CANAL_JOIN_Y }, { x + dir * 5.0f, CANAL_JOIN_Y }, 1.2f, alpha(WATER_GLINT, flowA * 0.6f));
            }
        }
        // Sun glints by day
        if (night < 0.6f) {
            for (int i = 0; i < 10; ++i) {
                float y = std::fmod(static_cast<float>(i) * 97.0f + worldTime * 30.0f, VIRTUAL_HEIGHT);
                float tw = 0.5f + 0.5f * std::sin(worldTime * 5.0f + static_cast<float>(i) * 1.7f);
                quad(va, { RIVER_X - 4.0f + static_cast<float>((i * 5) % 9), y }, { 2.0f, 2.0f }, alpha(sf::Color::White, 180.0f * tw * (1.0f - night) * (1.0f - ice)));
            }
        }
    }

    // Winter: ice sheet with cracks over all water
    if (ice > 0.01f) {
        quad(va, { RIVER_X - RIVER_HALF, 0.0f }, { RIVER_HALF * 2.0f, VIRTUAL_HEIGHT }, alpha(ICE, 245.0f * ice));
        for (float cx : { WEST_CANAL_X, EAST_CANAL_X }) quad(va, { cx - CANAL_HALF, CANAL_TOP }, { CANAL_HALF * 2.0f, CANAL_JOIN_Y + CANAL_HALF - CANAL_TOP }, alpha(ICE, 245.0f * ice));
        quad(va, { WEST_CANAL_X - CANAL_HALF, CANAL_JOIN_Y - CANAL_HALF }, { RIVER_X - RIVER_HALF - WEST_CANAL_X + CANAL_HALF, CANAL_HALF * 2.0f }, alpha(ICE, 245.0f * ice));
        quad(va, { RIVER_X + RIVER_HALF, CANAL_JOIN_Y - CANAL_HALF }, { EAST_CANAL_X + CANAL_HALF - RIVER_X - RIVER_HALF, CANAL_HALF * 2.0f }, alpha(ICE, 245.0f * ice));
        for (int i = 0; i < 26; ++i) {
            float y = static_cast<float>(i) * 35.0f + 8.0f * hash01(static_cast<std::uint32_t>(i) * 31u);
            float x0 = RIVER_X - RIVER_HALF + 2.0f + 10.0f * hash01(static_cast<std::uint32_t>(i) * 7u);
            float x1 = x0 + 6.0f + 8.0f * hash01(static_cast<std::uint32_t>(i) * 3u);
            line(va, { x0, y }, { std::min(x1, RIVER_X + RIVER_HALF - 2.0f), y + 9.0f * hash01(static_cast<std::uint32_t>(i) * 5u) - 4.0f }, 1.0f, alpha(ICE_CRACK, 220.0f * ice));
        }
        // Snow drifts on the banks
        for (int i = 0; i < 30; ++i) {
            float y = static_cast<float>(i) * 30.0f + 10.0f * hash01(static_cast<std::uint32_t>(i) * 17u);
            quad(va, { RIVER_X - RIVER_HALF - 2.0f, y }, { 5.0f, 12.0f }, alpha(SNOW_WHITE, 220.0f * ice));
            quad(va, { RIVER_X + RIVER_HALF - 3.0f, y + 15.0f }, { 5.0f, 12.0f }, alpha(SNOW_WHITE, 220.0f * ice));
        }
    }

    // Roads and bridges (below the city)
    for (int r = 0; r < 2; ++r) {
        float y = ROAD_Y[r];
        quad(va, { ROAD_X0[r], y - ROAD_HALF - 1.5f }, { ROAD_X1[r] - ROAD_X0[r], ROAD_HALF * 2.0f + 3.0f }, ROAD_EDGE);
        quad(va, { ROAD_X0[r], y - ROAD_HALF }, { ROAD_X1[r] - ROAD_X0[r], ROAD_HALF * 2.0f }, ROAD);
        for (float x = ROAD_X0[r] + 6.0f; x < ROAD_X1[r] - 10.0f; x += 18.0f) {
            if (std::fabs(x + 4.0f - RIVER_X) < RIVER_HALF + 8.0f) continue;
            quad(va, { x, y - 0.75f }, { 8.0f, 1.5f }, ROAD_LINE);
        }
        if (wWinter > 0.01f) { // snow on the verges
            quad(va, { ROAD_X0[r], y - ROAD_HALF - 1.5f }, { ROAD_X1[r] - ROAD_X0[r], 2.0f }, alpha(SNOW_WHITE, 200.0f * wWinter));
            quad(va, { ROAD_X0[r], y + ROAD_HALF - 0.5f }, { ROAD_X1[r] - ROAD_X0[r], 2.0f }, alpha(SNOW_WHITE, 200.0f * wWinter));
        }
        // Bridge over the river: shadow, deck, rails and posts
        float bx0 = RIVER_X - RIVER_HALF - 8.0f, bx1 = RIVER_X + RIVER_HALF + 8.0f;
        quad(va, { bx0 + 2.0f, y + ROAD_HALF + 2.0f }, { bx1 - bx0 - 4.0f, 4.0f }, sf::Color(10, 20, 35, 110));
        quad(va, { bx0, y - ROAD_HALF - 3.0f }, { bx1 - bx0, ROAD_HALF * 2.0f + 6.0f }, BRIDGE_DECK);
        quad(va, { bx0, y - ROAD_HALF - 3.0f }, { bx1 - bx0, 2.0f }, BRIDGE_RAIL);
        quad(va, { bx0, y + ROAD_HALF + 1.0f }, { bx1 - bx0, 2.0f }, BRIDGE_RAIL);
        for (float px = bx0; px <= bx1; px += (bx1 - bx0) / 4.0f) {
            quad(va, { px - 1.0f, y - ROAD_HALF - 4.0f }, { 2.0f, 4.0f }, BRIDGE_RAIL);
            quad(va, { px - 1.0f, y + ROAD_HALF + 1.0f }, { 2.0f, 4.0f }, BRIDGE_RAIL);
        }
        quad(va, { bx0 + 3.0f, y - 0.75f }, { bx1 - bx0 - 6.0f, 1.5f }, alpha(ROAD_LINE, 160.0f));
        // Lamp posts at opposite corners
        line(va, { RIVER_X - 19.0f, y - ROAD_HALF - 3.0f }, { RIVER_X - 19.0f, y - ROAD_HALF - 12.0f }, 1.5f, BRIDGE_RAIL);
        line(va, { RIVER_X + 19.0f, y + ROAD_HALF + 3.0f }, { RIVER_X + 19.0f, y + ROAD_HALF - 6.0f }, 1.5f, BRIDGE_RAIL);
        sf::Color head = (night > 0.3f) ? LAMP_HEAD : sf::Color(150, 150, 140);
        quad(va, { RIVER_X - 21.0f, y - ROAD_HALF - 14.0f }, { 4.0f, 3.0f }, head);
        quad(va, { RIVER_X + 17.0f, y + ROAD_HALF - 8.0f }, { 4.0f, 3.0f }, head);
    }

    // Traffic
    for (const auto& c : cars) {
        float y = ROAD_Y[c.road] + (c.dir > 0 ? -3.5f : 3.5f);
        float fade = Ease::clamp01(std::min(c.x - ROAD_X0[c.road] + 4.0f, ROAD_X1[c.road] + 4.0f - c.x) / 14.0f);
        if (fade <= 0.01f) continue;
        quad(va, { c.x - 5.5f, y - 2.5f }, { 11.0f, 5.0f }, alpha(c.body, 255.0f * fade));
        float wx = (c.dir > 0) ? c.x + 1.0f : c.x - 4.0f;
        quad(va, { wx, y - 2.0f }, { 3.0f, 4.0f }, alpha(sf::Color(30, 40, 55), 230.0f * fade));
        if (night > 0.25f) {
            float hx = (c.dir > 0) ? c.x + 5.5f : c.x - 7.0f;
            quad(va, { hx, y - 2.5f }, { 1.5f, 1.5f }, alpha(sf::Color(255, 250, 220), 255.0f * fade));
            quad(va, { hx, y + 1.0f }, { 1.5f, 1.5f }, alpha(sf::Color(255, 250, 220), 255.0f * fade));
            float tx = (c.dir > 0) ? c.x - 6.5f : c.x + 5.0f;
            quad(va, { tx, y - 2.0f }, { 1.5f, 4.0f }, alpha(sf::Color(255, 60, 50), 230.0f * fade));
        }
    }
    target.draw(va);
}

// Border tag below the city (kept from the original river art); drawn after the lighting pass so it stays readable
void UI_fx::drawBorderTag(sf::RenderTarget& target, const sf::Font& font, bool fontLoaded) const {
    sf::RectangleShape tag({ 200.0f, 26.0f });
    tag.setPosition({ RIVER_X - 100.0f, 400.0f });
    tag.setFillColor(TAG_BG);
    tag.setOutlineThickness(1.0f);
    tag.setOutlineColor(TAG_BORDER);
    target.draw(tag);
    if (fontLoaded) {
        sf::Text bText(font, toUtf8("ЦЕНТРАЛНА ГРАНИЦА"), 12);
        bText.setFillColor(TAG_TEXT);
        sf::FloatRect tb = bText.getLocalBounds();
        bText.setPosition({ RIVER_X - tb.size.x / 2.0f - tb.position.x, 404.0f });
        target.draw(bText);
    }
}
