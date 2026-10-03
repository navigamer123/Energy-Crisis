// =============================================================================
// [b-showcase] Cinematic intro (HX-01)
// =============================================================================
#include "../includes/UI_intro.h"
#include "../includes/UI_types.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {

// --- Palette (integrator: map these to UI_theme tokens) ---------------------
const sf::Color kSkyTop(4, 7, 18);
const sf::Color kSkyBottom(20, 26, 54);
const sf::Color kSkyline(8, 11, 20);
const sf::Color kSkylineEdge(34, 46, 76);
const sf::Color kWindowDark(16, 20, 32);
const sf::Color kP1Window(0, 229, 255);
const sf::Color kP2Window(255, 120, 200);
const sf::Color kWarmWindow(255, 214, 140);
const sf::Color kBoltCore(255, 255, 255);
const sf::Color kBoltGlow(140, 200, 255);
const sf::Color kLogoGlow(255, 236, 120);
const sf::Color kTagline(236, 242, 255);
const sf::Color kTaglineAccent(255, 226, 64);
const sf::Color kHint(176, 190, 214);
const sf::Color kHintPill(6, 9, 18);
const sf::Color kRain(150, 172, 214);
const sf::Color kAviationRed(255, 60, 60);

// --- Timeline (seconds) ------------------------------------------------------
constexpr float kFadeInEnd = 0.5f;
constexpr float kStrikeA = 0.70f;
constexpr float kLogoIn = 0.72f;
constexpr float kStrikeB = 1.55f;
constexpr float kLightsFrom = 1.9f;
constexpr float kLightsTo = 3.6f;
constexpr float kTaglineAt = 2.5f;
constexpr float kSubTaglineAt = 3.1f;
constexpr float kHintAt = 1.0f;
constexpr float kFadeOutAt = 5.5f;
constexpr float kSkipFade = 0.35f;

// --- Layout ------------------------------------------------------------------
const sf::Vector2f kLogoCenter(800.0f, 318.0f);
constexpr float kLogoWidth = 732.0f;  // On-screen width of the logo
constexpr float kTaglineY = 548.0f;
constexpr float kSubTaglineY = 592.0f;
constexpr float kHintY = 852.0f;

std::uint8_t toAlpha(float a) { return static_cast<std::uint8_t>(std::clamp(a, 0.0f, 255.0f)); }
sf::Color withAlpha(sf::Color c, float a) { c.a = toAlpha(a); return c; }
float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
float smooth01(float v) { v = clamp01(v); return v * v * (3.0f - 2.0f * v); }
float easeOutCubic(float v) { v = clamp01(v); float u = 1.0f - v; return 1.0f - u * u * u; }

// Exponential flash decay after an event at time `at`
float pulseAfter(float t, float at, float rate) {
    return (t < at) ? 0.0f : std::exp(-(t - at) * rate);
}

void drawThickPolyline(sf::RenderTarget& target, const std::vector<sf::Vector2f>& pts, float width, sf::Color c,
                       const sf::RenderStates& states) {
    for (size_t i = 1; i < pts.size(); ++i) {
        sf::Vector2f d = pts[i] - pts[i - 1];
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len < 0.5f) continue;
        sf::RectangleShape seg({ len + width * 0.5f, width });
        seg.setOrigin({ width * 0.25f, width * 0.5f });
        seg.setPosition(pts[i - 1]);
        seg.setRotation(sf::radians(std::atan2(d.y, d.x)));
        seg.setFillColor(c);
        target.draw(seg, states);
    }
}

void centerText(sf::Text& text, float cx, float y) {
    sf::FloatRect lb = text.getLocalBounds();
    text.setOrigin({ lb.position.x + lb.size.x / 2.0f, lb.position.y });
    text.setPosition({ std::round(cx), std::round(y) });
}

} // namespace

UI_intro::UI_intro() : rng(20260403u) {
    if (logoTexture.loadFromFile("assets/logo.png")) {
        logoTexture.setSmooth(true);
        logoLoaded = true;
    } else {
        std::cerr << "[UI_intro] Warning: assets/logo.png not found - the intro shows a text title instead.\n";
    }
    fontLoaded = font.openFromFile("assets/font.ttf");
    buildScene();
    restart();
}

bool UI_intro::enabledByEnvironment() {
    const char* v = std::getenv("EC_SKIP_INTRO");
    return v == nullptr || std::string(v) == "0";
}

void UI_intro::restart() {
    t = 0.0f;
    frozen = false;
    skipping = false;
    skipStartT = 0.0f;
    finished = false;
    lastCueT = -1.0f;
    soundCues.clear();
    clock.restart();
}

void UI_intro::debugSetTime(float time) {
    t = time;
    frozen = true;
}

void UI_intro::skip() {
    if (skipping || finished) return;
    skipping = true;
    skipStartT = t;
}

bool UI_intro::handleEvent(const sf::Event& event) {
    if (finished) return false;
    if (event.is<sf::Event::KeyPressed>() || event.is<sf::Event::MouseButtonPressed>() ||
        event.is<sf::Event::JoystickButtonPressed>()) {
        skip();
        return true;
    }
    return false;
}

bool UI_intro::takeSoundCue(std::string& outCue) {
    if (soundCues.empty()) return false;
    outCue = soundCues.front();
    soundCues.erase(soundCues.begin());
    return true;
}

void UI_intro::queueCues(float from, float to) {
    if (skipping) return;
    auto crossed = [from, to](float at) { return from < at && to >= at; };
    if (crossed(kStrikeA)) soundCues.push_back("thunder");
    if (crossed(kLogoIn + 0.02f)) soundCues.push_back("sting");
    if (crossed(kStrikeB)) soundCues.push_back("thunder");
}

float UI_intro::fadeAlpha() const {
    float a = 0.0f;
    if (t < kFadeInEnd) a = 1.0f - smooth01(t / kFadeInEnd);
    if (t > kFadeOutAt) a = std::max(a, smooth01((t - kFadeOutAt) / (DURATION - kFadeOutAt)));
    if (skipping) a = std::max(a, smooth01((t - skipStartT) / kSkipFade));
    return a;
}

UI_intro::Bolt UI_intro::makeBolt(sf::Vector2f from, sf::Vector2f to, float at) {
    Bolt bolt;
    bolt.at = at;
    std::uniform_real_distribution<float> jx(-26.0f, 26.0f);
    std::uniform_real_distribution<float> jy(-8.0f, 8.0f);
    std::uniform_int_distribution<int> coin(0, 2);
    const int segments = 16;
    bolt.main.push_back(from);
    for (int i = 1; i < segments; ++i) {
        float k = static_cast<float>(i) / segments;
        sf::Vector2f p = from + (to - from) * k + sf::Vector2f(jx(rng), jy(rng));
        bolt.main.push_back(p);
        if (i > 3 && i < segments - 3 && coin(rng) == 0) {
            std::vector<sf::Vector2f> br{ p };
            sf::Vector2f q = p;
            float side = (jx(rng) > 0.0f) ? 1.0f : -1.0f;
            for (int b = 0; b < 4; ++b) {
                q += sf::Vector2f(side * (10.0f + std::abs(jx(rng))), 14.0f + std::abs(jy(rng)) * 2.0f);
                br.push_back(q);
            }
            bolt.branches.push_back(br);
        }
    }
    bolt.main.push_back(to);
    return bolt;
}

void UI_intro::buildScene() {
    // Lightning bolts land on the tips of the two lightning arms drawn in the logo
    const float s = kLogoWidth / 1408.0f; // logo.png is 1408 x 768
    auto logoPoint = [s](float px, float py) {
        return sf::Vector2f(kLogoCenter.x + (px - 704.0f) * s, kLogoCenter.y + (py - 384.0f) * s);
    };
    bolts.push_back(makeBolt({ 540.0f, -20.0f }, logoPoint(378.0f, 46.0f), kStrikeA));
    bolts.push_back(makeBolt({ 1090.0f, -20.0f }, logoPoint(1004.0f, 46.0f), kStrikeB));

    // Electric arcs over the logo after each strike
    std::uniform_real_distribution<float> ax(kLogoCenter.x - 300.0f, kLogoCenter.x + 300.0f);
    std::uniform_real_distribution<float> ay(kLogoCenter.y - 150.0f, kLogoCenter.y + 150.0f);
    std::uniform_real_distribution<float> aj(-9.0f, 9.0f);
    for (int a = 0; a < 8; ++a) {
        sf::Vector2f p0(ax(rng), ay(rng));
        sf::Vector2f p1 = p0 + sf::Vector2f(aj(rng) * 9.0f, aj(rng) * 5.0f);
        std::vector<sf::Vector2f> arc;
        for (int k = 0; k <= 6; ++k) {
            float f = k / 6.0f;
            arc.push_back(p0 + (p1 - p0) * f + sf::Vector2f(aj(rng), aj(rng)));
        }
        arcs.push_back(arc);
    }

    // Night skyline along the bottom, taller at the sides so it frames the logo
    std::uniform_real_distribution<float> bw(38.0f, 104.0f);
    std::uniform_real_distribution<float> bh(0.0f, 1.0f);
    std::uniform_int_distribution<int> pct(0, 99);
    float x = -12.0f;
    while (x < VIRTUAL_WIDTH + 12.0f) {
        float w = bw(rng);
        float edge = std::abs((x + w * 0.5f) - 800.0f) / 800.0f;
        float h = 80.0f + bh(rng) * 120.0f + edge * 70.0f;
        sf::FloatRect block({ x, VIRTUAL_HEIGHT - h }, { w, h });
        skylineBlocks.push_back(block);

        // Windows: west half in P1 cyan, east half in P2 magenta, some warm offices
        for (float wy = block.position.y + 10.0f; wy + 7.0f < VIRTUAL_HEIGHT - 8.0f; wy += 15.0f) {
            for (float wx = block.position.x + 6.0f; wx + 5.0f < block.position.x + w - 5.0f; wx += 11.0f) {
                int roll = pct(rng);
                if (roll < 38) continue; // Stays dark
                CityWindow cw;
                cw.rect = sf::FloatRect({ wx, wy }, { 5.0f, 7.0f });
                cw.hash = pct(rng);
                float centerBias = std::abs(wx - 800.0f) / 800.0f; // Lights spread out from the centre
                cw.onAt = kLightsFrom + (kLightsTo - kLightsFrom) * (0.55f * centerBias + 0.45f * (cw.hash / 99.0f));
                cw.color = (roll < 52) ? kWarmWindow : (wx < 800.0f ? kP1Window : kP2Window);
                windows.push_back(cw);
            }
        }
        x += w + 2.0f + bh(rng) * 6.0f;
    }

    // Aviation lights on the three tallest blocks
    std::vector<sf::FloatRect> tall = skylineBlocks;
    std::sort(tall.begin(), tall.end(), [](const sf::FloatRect& a, const sf::FloatRect& b) { return a.size.y > b.size.y; });
    for (size_t i = 0; i < std::min<size_t>(3, tall.size()); ++i) {
        Antenna a;
        a.top = { tall[i].position.x + tall[i].size.x * 0.5f, tall[i].position.y - 22.0f };
        a.phase = static_cast<float>(i) * 1.3f;
        antennas.push_back(a);
    }

    std::uniform_real_distribution<float> sx(0.0f, VIRTUAL_WIDTH);
    std::uniform_real_distribution<float> sy(0.0f, 600.0f);
    std::uniform_real_distribution<float> ph(0.0f, 6.28f);
    for (int i = 0; i < 120; ++i) stars.push_back({ sx(rng), sy(rng), ph(rng) });
    std::uniform_real_distribution<float> ry(0.0f, 1000.0f);
    std::uniform_real_distribution<float> rs(620.0f, 900.0f);
    for (int i = 0; i < 150; ++i) rain.push_back({ sx(rng), ry(rng), rs(rng) });
}

void UI_intro::render(sf::RenderTarget& target) {
    // --- Advance time ---
    if (!frozen) {
        float dt = std::min(clock.restart().asSeconds(), 0.05f);
        float before = t;
        t += dt;
        queueCues(before, t);
    }
    if ((skipping && t - skipStartT >= kSkipFade) || t >= DURATION) {
        finished = true;
    }

    // Camera shake right after each strike
    sf::Vector2f shake(0.0f, 0.0f);
    for (const Bolt& b : bolts) {
        float k = (t >= b.at && t < b.at + 0.45f) ? pulseAfter(t, b.at, 9.0f) : 0.0f;
        shake += sf::Vector2f(std::sin(t * 83.0f) * 7.0f, std::cos(t * 97.0f) * 4.0f) * k;
    }
    sf::RenderStates states;
    states.transform.translate(shake);

    // --- Sky gradient ---
    sf::VertexArray sky(sf::PrimitiveType::TriangleStrip, 4);
    sky[0].position = { -20.0f, -20.0f };
    sky[1].position = { VIRTUAL_WIDTH + 20.0f, -20.0f };
    sky[2].position = { -20.0f, VIRTUAL_HEIGHT + 20.0f };
    sky[3].position = { VIRTUAL_WIDTH + 20.0f, VIRTUAL_HEIGHT + 20.0f };
    sky[0].color = sky[1].color = kSkyTop;
    sky[2].color = sky[3].color = kSkyBottom;
    target.draw(sky, states);

    // Stars (dimmed while the storm flashes)
    for (const auto& st : stars) {
        float tw = 0.55f + 0.45f * std::sin(t * 2.2f + st.z);
        sf::CircleShape star(1.1f);
        star.setPosition({ st.x, st.y });
        star.setFillColor(sf::Color(200, 215, 255, toAlpha(150.0f * tw)));
        target.draw(star, states);
    }

    // Horizon glow in the players' colours once the city lights come on
    float lights = smooth01((t - kLightsFrom) / (kLightsTo - kLightsFrom));
    if (lights > 0.0f) {
        sf::VertexArray glow(sf::PrimitiveType::Triangles, 12);
        auto quad = [&glow](int base, float x0, float x1, sf::Color top, sf::Color bottom) {
            glow[base + 0].position = { x0, 560.0f }; glow[base + 0].color = top;
            glow[base + 1].position = { x1, 560.0f }; glow[base + 1].color = top;
            glow[base + 2].position = { x0, VIRTUAL_HEIGHT }; glow[base + 2].color = bottom;
            glow[base + 3].position = { x1, 560.0f }; glow[base + 3].color = top;
            glow[base + 4].position = { x1, VIRTUAL_HEIGHT }; glow[base + 4].color = bottom;
            glow[base + 5].position = { x0, VIRTUAL_HEIGHT }; glow[base + 5].color = bottom;
        };
        quad(0, -20.0f, 800.0f, withAlpha(kP1Window, 0.0f), withAlpha(kP1Window, 46.0f * lights));
        quad(6, 800.0f, VIRTUAL_WIDTH + 20.0f, withAlpha(kP2Window, 0.0f), withAlpha(kP2Window, 46.0f * lights));
        target.draw(glow, states);
    }

    // --- Lightning bolts ---
    for (const Bolt& b : bolts) {
        float life = t - b.at;
        if (life < 0.0f || life > 0.34f) continue;
        float k = 1.0f - life / 0.34f;
        // Strobe: real lightning flickers once or twice
        if (life > 0.09f && life < 0.14f) k *= 0.25f;
        drawThickPolyline(target, b.main, 14.0f, withAlpha(kBoltGlow, 70.0f * k), states);
        drawThickPolyline(target, b.main, 6.0f, withAlpha(kBoltGlow, 170.0f * k), states);
        drawThickPolyline(target, b.main, 2.5f, withAlpha(kBoltCore, 255.0f * k), states);
        for (const auto& br : b.branches) {
            drawThickPolyline(target, br, 4.0f, withAlpha(kBoltGlow, 120.0f * k), states);
            drawThickPolyline(target, br, 1.5f, withAlpha(kBoltCore, 220.0f * k), states);
        }
        // Impact glow
        sf::CircleShape impact(28.0f);
        impact.setOrigin({ 28.0f, 28.0f });
        impact.setPosition(b.main.back());
        impact.setFillColor(withAlpha(kBoltGlow, 110.0f * k));
        target.draw(impact, states);
    }

    // --- Logo ---
    float logoK = smooth01((t - kLogoIn) / 0.35f);
    if (logoK > 0.0f) {
        float scale = 1.12f - 0.12f * easeOutCubic((t - kLogoIn) / 0.6f);
        float strikeFlash = std::max(pulseAfter(t, kStrikeA, 7.0f), 0.8f * pulseAfter(t, kStrikeB, 7.0f));
        float breathe = 0.5f + 0.5f * std::sin(t * 2.4f);
        if (logoLoaded) {
            sf::Sprite logo(logoTexture);
            sf::Vector2u ts = logoTexture.getSize();
            float base = kLogoWidth / static_cast<float>(ts.x);
            logo.setOrigin({ ts.x * 0.5f, ts.y * 0.5f });
            logo.setPosition(kLogoCenter);

            // Soft additive halo behind the logo
            sf::RenderStates add = states;
            add.blendMode = sf::BlendAdd;
            for (float grow : { 1.05f, 1.025f }) {
                logo.setScale({ base * scale * grow, base * scale * grow });
                logo.setColor(withAlpha(kLogoGlow, (28.0f + 22.0f * breathe + 120.0f * strikeFlash) * logoK));
                target.draw(logo, add);
            }
            logo.setScale({ base * scale, base * scale });
            logo.setColor(withAlpha(sf::Color::White, 255.0f * logoK));
            target.draw(logo, states);
            // White-hot flash on the logo right after a strike
            if (strikeFlash > 0.02f) {
                logo.setColor(withAlpha(sf::Color::White, 200.0f * strikeFlash * logoK));
                target.draw(logo, add);
            }
        } else if (fontLoaded) {
            sf::Text title(font, toUtf8("ENERGY CRISIS"), 96);
            title.setStyle(sf::Text::Bold);
            title.setFillColor(withAlpha(kTaglineAccent, 255.0f * logoK));
            title.setScale({ scale, scale });
            centerText(title, kLogoCenter.x, kLogoCenter.y - 60.0f);
            target.draw(title, states);
        }

        // Electric arcs crawling over the logo for a moment after each strike
        for (const Bolt& b : bolts) {
            float life = t - b.at;
            if (life < 0.02f || life > 0.38f) continue;
            float k = 1.0f - life / 0.38f;
            size_t first = (&b == &bolts.front()) ? 0 : 4;
            for (size_t a = first; a < first + 4 && a < arcs.size(); ++a) {
                if ((static_cast<int>(life * 30.0f) + static_cast<int>(a)) % 3 == 0) continue;
                drawThickPolyline(target, arcs[a], 3.0f, withAlpha(kBoltGlow, 150.0f * k), states);
                drawThickPolyline(target, arcs[a], 1.2f, withAlpha(kBoltCore, 230.0f * k), states);
            }
        }
    }

    // --- Rain ---
    sf::VertexArray drops(sf::PrimitiveType::Lines);
    for (const auto& r : rain) {
        float x = std::fmod(r.x - 140.0f * t + 3400.0f, 1700.0f) - 50.0f;
        float y = std::fmod(r.y + r.z * t, 1000.0f) - 60.0f;
        sf::Color c = withAlpha(kRain, 70.0f);
        drops.append(sf::Vertex{ { x, y }, c });
        drops.append(sf::Vertex{ { x - 3.0f, y + 15.0f }, c });
    }
    target.draw(drops, states);

    // --- Skyline and its windows ---
    for (const auto& b : skylineBlocks) {
        sf::RectangleShape block(b.size);
        block.setPosition(b.position);
        block.setFillColor(kSkyline);
        block.setOutlineThickness(1.0f);
        block.setOutlineColor(withAlpha(kSkylineEdge, 120.0f + 100.0f * lights));
        target.draw(block, states);
    }
    for (const auto& a : antennas) {
        sf::RectangleShape mast({ 2.0f, 22.0f });
        mast.setPosition({ a.top.x - 1.0f, a.top.y });
        mast.setFillColor(kSkylineEdge);
        target.draw(mast, states);
        if (std::sin(t * 3.2f + a.phase) > 0.2f) {
            sf::CircleShape red(2.6f);
            red.setOrigin({ 2.6f, 2.6f });
            red.setPosition(a.top);
            red.setFillColor(kAviationRed);
            target.draw(red, states);
        }
    }
    for (const auto& w : windows) {
        bool on = t >= w.onAt;
        if (!on && t >= w.onAt - 0.3f) on = ((static_cast<int>(t * 24.0f) + w.hash) % 3) == 0; // Flicker
        sf::RectangleShape win(w.rect.size);
        win.setPosition(w.rect.position);
        win.setFillColor(on ? withAlpha(w.color, 230.0f) : kWindowDark);
        target.draw(win, states);
    }

    // --- Tagline ---
    if (fontLoaded) {
        float tk = smooth01((t - kTaglineAt) / 0.6f);
        if (tk > 0.0f) {
            sf::Text tag(font, toUtf8("БИТКАТА ЗА ТОКА НА ГРАДА ЗАПОЧВА"), 26);
            tag.setStyle(sf::Text::Bold);
            tag.setLetterSpacing(1.5f);
            tag.setFillColor(withAlpha(kTagline, 255.0f * tk));
            centerText(tag, 800.0f, kTaglineY + (1.0f - tk) * 12.0f);
            target.draw(tag, states);

            // Accent rules on both sides of the tagline
            sf::FloatRect gb = tag.getGlobalBounds();
            for (int side = 0; side < 2; ++side) {
                float len = 70.0f * tk;
                float rx = side == 0 ? gb.position.x - 18.0f - len : gb.position.x + gb.size.x + 18.0f;
                sf::RectangleShape rule({ len, 2.0f });
                rule.setPosition({ rx, gb.position.y + gb.size.y * 0.5f });
                rule.setFillColor(withAlpha(side == 0 ? kP1Window : kP2Window, 220.0f * tk));
                target.draw(rule, states);
            }
        }
        float sk = smooth01((t - kSubTaglineAt) / 0.6f);
        if (sk > 0.0f) {
            sf::Text sub(font, toUtf8("СЛЪНЦЕ  ·  ВЯТЪР  ·  ВОДА  ·  БАТЕРИИ"), 15);
            sub.setLetterSpacing(1.8f);
            sub.setFillColor(withAlpha(kTaglineAccent, 235.0f * sk));
            centerText(sub, 800.0f, kSubTaglineY);
            target.draw(sub, states);
        }

        // Skip hint (no shake: it is UI, not scene)
        float hk = smooth01((t - kHintAt) / 0.5f) * (0.72f + 0.28f * std::sin(t * 3.0f));
        if (hk > 0.0f && !skipping) {
            sf::Text hint(font, toUtf8("Натиснете произволен клавиш, за да продължите"), 14);
            hint.setFillColor(withAlpha(kHint, 255.0f * hk));
            centerText(hint, 800.0f, kHintY);
            sf::FloatRect hb = hint.getGlobalBounds();
            sf::RectangleShape pill({ hb.size.x + 28.0f, hb.size.y + 14.0f });
            pill.setPosition({ hb.position.x - 14.0f, hb.position.y - 7.0f });
            pill.setFillColor(withAlpha(kHintPill, 238.0f * smooth01((t - kHintAt) / 0.5f)));
            pill.setOutlineThickness(1.0f);
            pill.setOutlineColor(withAlpha(kSkylineEdge, 200.0f * smooth01((t - kHintAt) / 0.5f)));
            target.draw(pill);
            target.draw(hint);
        }
    }

    // --- Full-screen lightning flash ---
    float flash = std::max(pulseAfter(t, kStrikeA, 11.0f) * 0.85f, pulseAfter(t, kStrikeB, 11.0f) * 0.6f);
    if (flash > 0.01f) {
        sf::RectangleShape f({ VIRTUAL_WIDTH, VIRTUAL_HEIGHT });
        f.setFillColor(sf::Color(225, 238, 255, toAlpha(220.0f * flash)));
        target.draw(f);
    }

    // --- Fades (start, end, skip) ---
    float fade = fadeAlpha();
    if (fade > 0.0f) {
        sf::RectangleShape black({ VIRTUAL_WIDTH, VIRTUAL_HEIGHT });
        black.setFillColor(sf::Color(0, 0, 0, toAlpha(255.0f * fade)));
        target.draw(black);
    }
}
