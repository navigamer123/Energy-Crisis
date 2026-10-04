// =============================================================================
// [b-effects] HX-11 Seasonal world visuals.
// Spring: fresh tint and blossoms. Summer: warm haze, dry patches and heat shimmer.
// Autumn: orange tint and leaf litter. Winter: snow cover (the river freezes in
// UI_fx_world.cpp). Season changes fade over a few seconds; decals stay on open ground.
// =============================================================================
#include "../includes/UI_fx.h"
#include "../includes/UI_ease.h"
#include "../includes/UI_types.h"

#include <algorithm>
#include <cmath>

namespace {

// --- Colours (integrator: map to UI_theme tokens) ---
const sf::Color SNOW_WHITE(242, 247, 255);
const sf::Color TINT_SPRING(150, 215, 110, 26);
const sf::Color TINT_SUMMER(235, 205, 110, 44);
const sf::Color TINT_AUTUMN(205, 120, 40, 70);
const sf::Color TINT_WINTER(220, 232, 250, 80);
const sf::Color DRY_PATCH(225, 200, 120);
const sf::Color CLEARED_SOIL(52, 48, 44);
const sf::Color HEAT_SHIMMER(255, 250, 225);
const sf::Color LEAF_COLORS[3] = { sf::Color(196, 86, 30), sf::Color(222, 152, 40), sf::Color(150, 62, 26) };
const sf::Color PETAL_PINK(255, 182, 210);
const sf::Color PETAL_WHITE(255, 240, 246);
const sf::Color BLOSSOM_CENTRE(255, 214, 90);

const float RIVER_X = 800.0f;
const float RIVER_HALF = 13.0f;

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

// Deterministic hash noise for decals and textures
float hash01(std::uint32_t n) {
    n = (n << 13) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return static_cast<float>(n & 0x7fffffffu) / 2147483647.0f;
}

// Tileable value noise in [0,1]
float valueNoise(float x, float y, int period, std::uint32_t seed) {
    int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y));
    float fx = x - static_cast<float>(xi), fy = y - static_cast<float>(yi);
    auto h = [&](int ix, int iy) {
        ix = ((ix % period) + period) % period;
        iy = ((iy % period) + period) % period;
        return hash01(static_cast<std::uint32_t>(ix * 73856093) ^ static_cast<std::uint32_t>(iy * 19349663) ^ seed);
    };
    float sx = fx * fx * (3.0f - 2.0f * fx), sy = fy * fy * (3.0f - 2.0f * fy);
    float a = h(xi, yi), b = h(xi + 1, yi), c = h(xi, yi + 1), d = h(xi + 1, yi + 1);
    return (a + (b - a) * sx) + ((c + (d - c) * sx) - (a + (b - a) * sx)) * sy;
}

bool insideCityOrPanels(sf::Vector2f p) {
    if (p.x > 600.0f && p.x < 1000.0f && p.y > 55.0f && p.y < 405.0f) return true; // city
    if (p.y < 115.0f && (p.x < 260.0f || p.x > 1340.0f)) return true;              // clocks
    return false;
}

} // namespace

// ---------------------------------------------------------------------------
// HX-11 Seasons
// ---------------------------------------------------------------------------
void UI_fx::ensureSeasonResources() {
    if (snowTex) return;
    const unsigned N = 256;
    sf::Image snow({ N, N }, sf::Color::Transparent);
    for (unsigned y = 0; y < N; ++y) {
        for (unsigned x = 0; x < N; ++x) {
            float fx = static_cast<float>(x) / static_cast<float>(N), fy = static_cast<float>(y) / static_cast<float>(N);
            float n = 0.6f * valueNoise(fx * 6.0f, fy * 6.0f, 6, 17u) + 0.3f * valueNoise(fx * 14.0f, fy * 14.0f, 14, 29u) +
                      0.1f * valueNoise(fx * 40.0f, fy * 40.0f, 40, 41u);
            float a = Ease::clamp01((n - 0.34f) / 0.22f);
            a = a * a * (3.0f - 2.0f * a);
            float sparkle = (hash01(x * 9176u + y * 31u) > 0.995f) ? 0.25f : 0.0f;
            snow.setPixel({ x, y }, sf::Color(255, 255, 255, static_cast<std::uint8_t>(std::min(1.0f, a * 0.92f + sparkle) * 255.0f)));
        }
    }
    snowTex = std::make_unique<sf::Texture>();
    if (snowTex->loadFromImage(snow)) {
        snowTex->setRepeated(true);
        snowTex->setSmooth(true);
    }

    // Leaf litter (kind 0) and blossoms (kind 1) at fixed pseudo-random spots outside the city
    decals.clear();
    std::uint32_t k = 1;
    for (int kind = 0; kind < 2; ++kind) {
        int placed = 0;
        while (placed < 240 && k < 20000) {
            sf::Vector2f p(hash01(k * 2u + 11u) * VIRTUAL_WIDTH, hash01(k * 2u + 97u) * VIRTUAL_HEIGHT);
            ++k;
            if (insideCityOrPanels(p) || std::fabs(p.x - RIVER_X) < RIVER_HALF + 6.0f) continue;
            decals.push_back({ p, hash01(k * 7u) * 360.0f, 0.7f + 0.7f * hash01(k * 13u), kind });
            ++placed;
        }
    }
}

void UI_fx::drawGround(sf::RenderTarget& target) const {
    const_cast<UI_fx*>(this)->ensureSeasonResources();
    const float wSpring = seasonW[0], wSummer = seasonW[1], wAutumn = seasonW[2], wWinter = seasonW[3];

    // Season tints over the grass
    sf::RectangleShape tint({ VIRTUAL_WIDTH, VIRTUAL_HEIGHT });
    struct T { float w; sf::Color c; } tints[4] = {
        { wSpring, TINT_SPRING },
        { wSummer, TINT_SUMMER },
        { wAutumn, TINT_AUTUMN },
        { wWinter, TINT_WINTER },
    };
    for (const auto& t : tints) {
        if (t.w < 0.01f) continue;
        tint.setFillColor(alpha(t.c, static_cast<float>(t.c.a) * t.w));
        target.draw(tint);
    }

    // Winter snow cover (tileable noise), summer dry patches reuse the same texture
    if (snowTex && (wWinter > 0.01f || wSummer > 0.01f)) {
        sf::Sprite cover(*snowTex);
        cover.setTextureRect(sf::IntRect({ 0, 0 }, { static_cast<int>(VIRTUAL_WIDTH), static_cast<int>(VIRTUAL_HEIGHT) }));
        if (wWinter > 0.01f) {
            cover.setColor(alpha(SNOW_WHITE, 235.0f * wWinter));
            target.draw(cover);
            // Plots are cleared of snow (dark soil), so their labels keep a dark background
            sf::RectangleShape soil;
            soil.setFillColor(alpha(CLEARED_SOIL, 175.0f * wWinter));
            for (const auto& plot : plots) {
                soil.setPosition(plot.bounds.position);
                soil.setSize(plot.bounds.size);
                target.draw(soil);
            }
        }
        if (wSummer > 0.01f) {
            cover.setTextureRect(sf::IntRect({ 97, 53 }, { static_cast<int>(VIRTUAL_WIDTH), static_cast<int>(VIRTUAL_HEIGHT) }));
            cover.setColor(alpha(DRY_PATCH, 60.0f * wSummer));
            target.draw(cover);
        }
    }

    // Leaf litter and blossoms
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    for (const auto& d : decals) {
        float w = (d.kind == 0) ? wAutumn : wSpring;
        if (w < 0.01f) continue;
        bool covered = (d.pos.y > 575.0f && d.pos.y < 755.0f && ((d.pos.x > 238.0f && d.pos.x < 614.0f) || (d.pos.x > 992.0f && d.pos.x < 1366.0f))); // mines
        for (const auto& plot : plots) {
            if (covered) break;
            sf::FloatRect r(plot.bounds.position - sf::Vector2f(4.0f, 4.0f), plot.bounds.size + sf::Vector2f(8.0f, 8.0f));
            covered = r.contains(d.pos);
        }
        if (covered) continue; // only on open ground, never as specks inside plots or mines
        float a = d.rot * 0.0174533f;
        sf::Vector2f ax(std::cos(a), std::sin(a)), ay(-std::sin(a), std::cos(a));
        if (d.kind == 0) {
            sf::Color c = alpha(LEAF_COLORS[static_cast<int>(d.rot) % 3], 215.0f * w);
            float L = 3.6f * d.size, W = 1.8f * d.size;
            sf::Vector2f p0 = d.pos - ax * L, p1 = d.pos + ay * W, p2 = d.pos + ax * L, p3 = d.pos - ay * W;
            va.append(sf::Vertex{ p0, c, {} }); va.append(sf::Vertex{ p1, c, {} }); va.append(sf::Vertex{ p2, c, {} });
            va.append(sf::Vertex{ p0, c, {} }); va.append(sf::Vertex{ p2, c, {} }); va.append(sf::Vertex{ p3, c, {} });
        } else {
            sf::Color petal = alpha((static_cast<int>(d.rot) % 2) ? PETAL_PINK : PETAL_WHITE, 225.0f * w);
            sf::Color centre = alpha(BLOSSOM_CENTRE, 235.0f * w);
            float r = 2.0f * d.size;
            for (int i = 0; i < 5; ++i) {
                float pa = a + static_cast<float>(i) * 1.2566f;
                sf::Vector2f c = d.pos + sf::Vector2f(std::cos(pa), std::sin(pa)) * r;
                quad(va, c - sf::Vector2f(r * 0.6f, r * 0.6f), { r * 1.2f, r * 1.2f }, petal);
            }
            quad(va, d.pos - sf::Vector2f(r * 0.45f, r * 0.45f), { r * 0.9f, r * 0.9f }, centre);
        }
    }
    target.draw(va);

    // Summer heat shimmer: faint wavy bands drifting upwards on sunny afternoons
    float heat = wSummer * (1.0f - night);
    if (heat > 0.02f) {
        sf::VertexArray bands(sf::PrimitiveType::TriangleStrip);
        for (int b = 0; b < 6; ++b) {
            float y0 = std::fmod(VIRTUAL_HEIGHT + 40.0f - worldTime * 14.0f + static_cast<float>(b) * 165.0f, VIRTUAL_HEIGHT + 80.0f) - 40.0f;
            bands.clear();
            for (int i = 0; i <= 40; ++i) {
                float x = static_cast<float>(i) * VIRTUAL_WIDTH / 40.0f;
                float y = y0 + std::sin(x * 0.02f + worldTime * 1.7f + static_cast<float>(b)) * 4.0f;
                float a = 13.0f * heat * (0.6f + 0.4f * std::sin(x * 0.011f + static_cast<float>(b) * 2.0f));
                bands.append(sf::Vertex{ { x, y }, alpha(HEAT_SHIMMER, a), {} });
                bands.append(sf::Vertex{ { x, y + 7.0f }, alpha(HEAT_SHIMMER, 0.0f), {} });
            }
            target.draw(bands);
        }
    }
}
