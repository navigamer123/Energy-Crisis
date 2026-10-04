// =============================================================================
// [b-effects] DS-11 Day/night lighting pass.
// A half-resolution light map holds the ambient light (a sky gradient driven by the
// seasonal sunrise/sunset and tinted per sector by that player's weather) plus additive
// light pools: powered lamps, the city glow, mine floodlights, bridge lamps, car
// headlights, building status lights and lightning. It is multiplied over the terrain
// (full pass) and, lifted towards white, over plots and buildings (mild pass), so the
// world is dark at night while labels stay readable and lamps visibly matter.
// =============================================================================
#include "../includes/UI_fx.h"
#include "../includes/UI_ease.h"
#include "../includes/UI_types.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

// --- Ambient keyframe colours (integrator: map to UI_theme tokens) ---
const sf::Color AMB_NIGHT_TOP(66, 78, 138);
const sf::Color AMB_NIGHT_BOTTOM(88, 100, 156);
const sf::Color AMB_DAWN_TOP(236, 168, 160);
const sf::Color AMB_DAWN_BOTTOM(255, 205, 150);
const sf::Color AMB_DUSK_TOP(196, 128, 162);
const sf::Color AMB_DUSK_BOTTOM(255, 160, 104);
const sf::Color AMB_DAY(255, 255, 255);

// --- Light colours ---
const sf::Color LIGHT_LAMP(255, 208, 140);
const sf::Color LIGHT_CITY(96, 90, 118);
const sf::Color LIGHT_MINE(120, 116, 104);
const sf::Color LIGHT_BRIDGE(255, 200, 135);
const sf::Color LIGHT_HEADLIGHT(255, 240, 205);
const sf::Color LIGHT_P1(0, 200, 255);
const sf::Color LIGHT_P2(255, 110, 190);

const unsigned LIGHTMAP_W = 800, LIGHTMAP_H = 450; // half of the 1600x900 canvas
const float MILD_STRENGTH = 0.45f;                 // mild pass = 1 - (1 - light) * 0.45

sf::Color weatherTint(WeatherType w) {
    switch (w) {
        case WeatherType::SUNNY: return sf::Color(255, 250, 238);
        case WeatherType::WINDY: return sf::Color(246, 250, 255);
        case WeatherType::CLOUDY: return sf::Color(222, 227, 237);
        case WeatherType::RAINY: return sf::Color(196, 205, 224);
        case WeatherType::STORMY: return sf::Color(160, 167, 194);
        case WeatherType::SNOWY: return sf::Color(232, 240, 255);
    }
    return sf::Color::White;
}

sf::Color mix(sf::Color a, sf::Color b, float t) {
    t = Ease::clamp01(t);
    return sf::Color(static_cast<std::uint8_t>(a.r + (b.r - a.r) * t), static_cast<std::uint8_t>(a.g + (b.g - a.g) * t),
                     static_cast<std::uint8_t>(a.b + (b.b - a.b) * t), 255);
}

sf::Color modulate(sf::Color a, sf::Color b) {
    return sf::Color(static_cast<std::uint8_t>(a.r * b.r / 255), static_cast<std::uint8_t>(a.g * b.g / 255),
                     static_cast<std::uint8_t>(a.b * b.b / 255), 255);
}

sf::Color scaled(sf::Color c, float k) {
    k = std::max(0.0f, std::min(1.0f, k));
    return sf::Color(static_cast<std::uint8_t>(c.r * k), static_cast<std::uint8_t>(c.g * k), static_cast<std::uint8_t>(c.b * k), 255);
}

// Station centres (mirrors the UI_resourceNodes layout; two rows per player)
void stationCentres(std::vector<sf::Vector2f>& out) {
    const float row0 = 582.0f + 40.0f, row1 = 668.0f + 40.0f;
    for (float startX : { 244.0f, 998.0f }) {
        for (int i = 0; i < 4; ++i) out.push_back({ startX + 43.0f + static_cast<float>(i) * 92.0f, row0 });
        for (int i = 0; i < 3; ++i) out.push_back({ startX + 58.0f + static_cast<float>(i) * 124.0f, row1 });
    }
}

} // namespace

void UI_fx::computeAmbient() {
    // Night amount and sky colours from the season's sunrise/sunset (dawn/dusk last ~2 game hours)
    const float h = hour;
    const float dawnStart = sunrise - 1.0f, dawnPeak = sunrise + 0.15f, dawnEnd = sunrise + 1.25f;
    const float duskStart = sunset - 1.25f, duskPeak = sunset - 0.2f, duskEnd = sunset + 0.9f;
    sf::Color top = AMB_NIGHT_TOP, bottom = AMB_NIGHT_BOTTOM;
    float n = 1.0f;
    if (h >= dawnStart && h < dawnPeak) {
        float t = (h - dawnStart) / (dawnPeak - dawnStart);
        top = mix(AMB_NIGHT_TOP, AMB_DAWN_TOP, t);
        bottom = mix(AMB_NIGHT_BOTTOM, AMB_DAWN_BOTTOM, t);
        n = 1.0f - 0.6f * t;
    } else if (h >= dawnPeak && h < dawnEnd) {
        float t = (h - dawnPeak) / (dawnEnd - dawnPeak);
        top = mix(AMB_DAWN_TOP, AMB_DAY, t);
        bottom = mix(AMB_DAWN_BOTTOM, AMB_DAY, t);
        n = 0.4f * (1.0f - t);
    } else if (h >= dawnEnd && h < duskStart) {
        top = bottom = AMB_DAY;
        n = 0.0f;
    } else if (h >= duskStart && h < duskPeak) {
        float t = (h - duskStart) / (duskPeak - duskStart);
        top = mix(AMB_DAY, AMB_DUSK_TOP, t);
        bottom = mix(AMB_DAY, AMB_DUSK_BOTTOM, t);
        n = 0.4f * t;
    } else if (h >= duskPeak && h < duskEnd) {
        float t = (h - duskPeak) / (duskEnd - duskPeak);
        top = mix(AMB_DUSK_TOP, AMB_NIGHT_TOP, t);
        bottom = mix(AMB_DUSK_BOTTOM, AMB_NIGHT_BOTTOM, t);
        n = 0.4f + 0.6f * t;
    }
    if (season == SeasonType::WINTER && n > 0.0f) { // longer, colder winter nights
        top = mix(top, scaled(top, 0.9f), n);
        bottom = mix(bottom, scaled(bottom, 0.9f), n);
    }
    night = n;

    // Per-sector weather tint (weaker at night), blended across the river
    sf::Color tint[2];
    for (int s = 0; s < 2; ++s) tint[s] = mix(weatherTint(weather[s]), sf::Color::White, 0.6f * n);
    sf::Color mid = mix(tint[0], tint[1], 0.5f);
    const sf::Color cols[3] = { tint[0], mid, tint[1] };
    for (int c = 0; c < 3; ++c) {
        skyTop[c] = modulate(top, cols[c]);
        skyBottom[c] = modulate(bottom, cols[c]);
    }
}

sf::Color UI_fx::ambientColorAt(float x) const {
    auto avg = [this](int c) { return mix(skyTop[c], skyBottom[c], 0.5f); };
    if (x < 700.0f) return avg(0);
    if (x > 900.0f) return avg(2);
    float t = (x - 700.0f) / 200.0f;
    return t < 0.5f ? mix(avg(0), avg(1), t * 2.0f) : mix(avg(1), avg(2), (t - 0.5f) * 2.0f);
}

void UI_fx::ensureLightingResources() {
    if (lightingReady || lightingFailed) return;
    lightMap = std::make_unique<sf::RenderTexture>();
    mildMap = std::make_unique<sf::RenderTexture>();
    if (!lightMap->resize({ LIGHTMAP_W, LIGHTMAP_H }) || !mildMap->resize({ LIGHTMAP_W, LIGHTMAP_H })) {
        std::cerr << "[UI_fx] Render textures unavailable: using the simple night tint instead of the lighting pass.\n";
        lightingFailed = true;
        lightMap.reset();
        mildMap.reset();
        return;
    }
    lightMap->setSmooth(true);
    mildMap->setSmooth(true);
    sf::View v(sf::FloatRect({ 0.0f, 0.0f }, { VIRTUAL_WIDTH, VIRTUAL_HEIGHT }));
    lightMap->setView(v);
    mildMap->setView(v);

    // Radial falloff: full light in the centre, smooth edge
    const unsigned R = 128;
    sf::Image img({ R, R }, sf::Color::Black);
    for (unsigned y = 0; y < R; ++y) {
        for (unsigned x = 0; x < R; ++x) {
            float dx = (static_cast<float>(x) + 0.5f) / static_cast<float>(R) * 2.0f - 1.0f;
            float dy = (static_cast<float>(y) + 0.5f) / static_cast<float>(R) * 2.0f - 1.0f;
            float d = std::sqrt(dx * dx + dy * dy);
            float k = 1.0f - Ease::clamp01((d - 0.35f) / 0.65f);
            k = k * k * (3.0f - 2.0f * k);
            auto v8 = static_cast<std::uint8_t>(255.0f * k);
            img.setPixel({ x, y }, sf::Color(v8, v8, v8, 255));
        }
    }
    radialTex = std::make_unique<sf::Texture>();
    if (!radialTex->loadFromImage(img)) {
        lightingFailed = true;
        return;
    }
    radialTex->setSmooth(true);
    lightingReady = true;
}

void UI_fx::renderLightMap() {
    sf::RenderTexture& rt = *lightMap;
    rt.clear(sf::Color::White);

    // Ambient sky gradient in three columns (West, river blend, East)
    sf::VertexArray amb(sf::PrimitiveType::Triangles);
    const float xs[4] = { 0.0f, 700.0f, 900.0f, VIRTUAL_WIDTH };
    for (int band = 0; band < 3; ++band) {
        // Band 1 (river) blends left -> right through the middle colour
        sf::Color tl = skyTop[band == 2 ? 2 : 0], bl = skyBottom[band == 2 ? 2 : 0];
        sf::Color tr = skyTop[band == 0 ? 0 : 2], br = skyBottom[band == 0 ? 0 : 2];
        sf::Vector2f p0(xs[band], 0.0f), p1(xs[band + 1], 0.0f), p2(xs[band], VIRTUAL_HEIGHT), p3(xs[band + 1], VIRTUAL_HEIGHT);
        amb.append(sf::Vertex{ p0, tl, {} });
        amb.append(sf::Vertex{ p1, tr, {} });
        amb.append(sf::Vertex{ p2, bl, {} });
        amb.append(sf::Vertex{ p2, bl, {} });
        amb.append(sf::Vertex{ p1, tr, {} });
        amb.append(sf::Vertex{ p3, br, {} });
    }
    rt.draw(amb);

    // Light pools are strongest at night; dark storm skies switch lamps on a little during the day
    float storm = 0.0f;
    for (int s = 0; s < 2; ++s) {
        if (weather[s] == WeatherType::STORMY) storm = std::max(storm, 0.35f);
        else if (weather[s] == WeatherType::RAINY) storm = std::max(storm, 0.15f);
    }
    float strength = std::min(1.0f, night + storm * (1.0f - night));
    if (strength > 0.01f) {
        sf::Sprite spr(*radialTex);
        spr.setOrigin({ 64.0f, 64.0f });
        auto light = [&](sf::Vector2f pos, float rx, float ry, sf::Color c, float k) {
            spr.setPosition(pos);
            spr.setScale({ rx / 64.0f, ry / 64.0f });
            spr.setColor(scaled(c, k));
            rt.draw(spr, sf::RenderStates(sf::BlendAdd));
        };

        // City glow spilling onto the ground around the skyline
        light({ 800.0f, 232.0f }, 300.0f, 270.0f, LIGHT_CITY, strength);

        // Mine floodlights keep the stations readable
        std::vector<sf::Vector2f> st;
        stationCentres(st);
        for (const auto& p : st) light(p, 82.0f, 66.0f, LIGHT_MINE, strength);

        // Lamps (the gameplay radius) and generator status lights
        for (const auto& b : buildings) {
            if (b.isBroken) continue;
            if (b.type == BuildingType::LAMP) {
                if (b.lightRadius > 0.0f) light(b.position, b.lightRadius * 1.15f, b.lightRadius * 1.15f, LIGHT_LAMP, strength);
            } else {
                light(b.position, 26.0f, 22.0f, b.playerOwner == 1 ? LIGHT_P1 : LIGHT_P2, strength * 0.4f);
            }
        }

        // Bridge lamps and headlights (DS-10)
        for (float y : { 467.0f, 665.0f }) {
            light({ 781.0f, y - 11.0f }, 46.0f, 40.0f, LIGHT_BRIDGE, strength * 0.9f);
            light({ 819.0f, y + 11.0f }, 46.0f, 40.0f, LIGHT_BRIDGE, strength * 0.9f);
        }
        for (const auto& c : cars) {
            float y = (c.road == 0 ? 467.0f : 665.0f) + (c.dir > 0 ? -3.5f : 3.5f);
            light({ c.x + static_cast<float>(c.dir) * 20.0f, y }, 26.0f, 12.0f, LIGHT_HEADLIGHT, strength * 0.85f);
        }
    }

    // Lightning lights up everything for a moment
    if (lightningLight > 0.01f) {
        sf::RectangleShape flash({ VIRTUAL_WIDTH, VIRTUAL_HEIGHT });
        flash.setFillColor(scaled(sf::Color::White, lightningLight * 0.9f));
        rt.draw(flash, sf::RenderStates(sf::BlendAdd));
    }
    rt.display();

    // Mild version for plots and buildings: lerp(white, light, MILD_STRENGTH)
    mildMap->clear(sf::Color::White);
    sf::Sprite full(lightMap->getTexture());
    full.setScale({ VIRTUAL_WIDTH / static_cast<float>(LIGHTMAP_W), VIRTUAL_HEIGHT / static_cast<float>(LIGHTMAP_H) });
    full.setColor(sf::Color(255, 255, 255, static_cast<std::uint8_t>(255.0f * MILD_STRENGTH)));
    mildMap->draw(full);
    mildMap->display();
}

void UI_fx::applyLighting(sf::RenderTarget& target, bool mildPass) {
    ensureLightingResources();
    bool fullDay = night <= 0.001f && lightningLight <= 0.01f;
    if (fullDay) {
        bool allWhite = true;
        for (int c = 0; c < 3; ++c) {
            if (skyTop[c].r < 252 || skyTop[c].g < 252 || skyTop[c].b < 252) allWhite = false;
        }
        if (allWhite) return; // bright clear day: nothing to multiply
    }
    if (lightingFailed) {
        // Fallback: a flat tint like the original day/night overlay
        if (mildPass) return;
        sf::RectangleShape tint({ VIRTUAL_WIDTH, VIRTUAL_HEIGHT });
        sf::Color c = ambientColorAt(800.0f);
        tint.setFillColor(sf::Color(static_cast<std::uint8_t>(c.r / 8), static_cast<std::uint8_t>(c.g / 8), static_cast<std::uint8_t>(c.b / 6),
                                    static_cast<std::uint8_t>(130.0f * night)));
        target.draw(tint);
        return;
    }
    if (!mildPass) renderLightMap();
    sf::Sprite spr(mildPass ? mildMap->getTexture() : lightMap->getTexture());
    spr.setScale({ VIRTUAL_WIDTH / static_cast<float>(LIGHTMAP_W), VIRTUAL_HEIGHT / static_cast<float>(LIGHTMAP_H) });
    target.draw(spr, sf::RenderStates(sf::BlendMultiply));
}
