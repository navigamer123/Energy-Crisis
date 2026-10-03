// =============================================================================
// [b-showcase] Living skyline (HX-03) and blackout set piece (HX-04)
// =============================================================================
#include "../includes/UI_skyline.h"
#include "../includes/UI_showcaseModel.h"
#include "../includes/UI_types.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

// --- Palette (integrator: map these to UI_theme tokens) ---------------------
const sf::Color kP1Outline(0, 200, 255);
const sf::Color kP2Outline(255, 120, 200);
const sf::Color kP1Roof(80, 230, 255);
const sf::Color kP2Roof(255, 160, 220);
const sf::Color kP1BodyDay(32, 44, 62);
const sf::Color kP2BodyDay(48, 34, 52);
const sf::Color kP1BodyNight(18, 26, 38);
const sf::Color kP2BodyNight(30, 20, 32);
const sf::Color kP1WinDay(90, 160, 200, 190);
const sf::Color kP2WinDay(210, 180, 130, 190);
const sf::Color kP1WinNight(0, 235, 255, 245);
const sf::Color kP2WinNight(255, 130, 190, 245);
const sf::Color kWinOff(14, 18, 26, 240);
const sf::Color kCrane(255, 196, 40);
const sf::Color kScaffold(130, 140, 156);
const sf::Color kSpark(255, 245, 190);
const sf::Color kHazard(255, 215, 0, 160);
const sf::Color kAlarmRed(255, 52, 52);
const sf::Color kWorldDim(4, 6, 14);
const sf::Color kCityDark(2, 4, 10);
const sf::Color kBrownoutVeil(0, 0, 0, 52);
const sf::Color kBannerBg(26, 6, 9);
const sf::Color kBannerTitle(255, 238, 214);
const sf::Color kBannerAmber(255, 196, 64);
const sf::Color kBannerDetail(214, 200, 196);
const sf::Color kStripeDark(30, 20, 10);

constexpr float kHeaderBottomY = Showcase::CITY_TOP + 26.0f; // UI_city header banner (91)
constexpr float kWorldDimMaxAlpha = 125.0f;
constexpr float kCityDarkMaxAlpha = 175.0f;

std::uint8_t toAlpha(float a) {
    return static_cast<std::uint8_t>(std::clamp(a, 0.0f, 255.0f));
}

sf::Color withAlpha(sf::Color c, float a) {
    c.a = toAlpha(a);
    return c;
}

sf::Color scaled(sf::Color c, float k) {
    auto ch = [k](std::uint8_t v) { return static_cast<std::uint8_t>(std::clamp(v * k, 0.0f, 255.0f)); };
    return sf::Color(ch(c.r), ch(c.g), ch(c.b), c.a);
}

void fillRect(sf::RenderTarget& target, float x, float y, float w, float h, sf::Color fill,
              sf::Color outline = sf::Color::Transparent, float thickness = 0.0f) {
    if (w <= 0.0f || h <= 0.0f) return;
    sf::RectangleShape r({ w, h });
    r.setPosition({ x, y });
    r.setFillColor(fill);
    if (thickness > 0.0f) {
        r.setOutlineThickness(thickness);
        r.setOutlineColor(outline);
    }
    target.draw(r);
}

// Rectangle split at the city capture line: the part left of captureX is P1's, the rest P2's
void splitRect(sf::RenderTarget& target, float x, float y, float w, float h, float captureX,
               sf::Color fillP1, sf::Color fillP2, sf::Color outP1, sf::Color outP2, float thickness) {
    float right = x + w;
    if (captureX <= x) {
        fillRect(target, x, y, w, h, fillP2, outP2, thickness);
    } else if (captureX >= right) {
        fillRect(target, x, y, w, h, fillP1, outP1, thickness);
    } else {
        fillRect(target, x, y, captureX - x, h, fillP1, outP1, thickness);
        fillRect(target, captureX, y, right - captureX, h, fillP2, outP2, thickness);
    }
}

void line(sf::RenderTarget& target, sf::Vector2f a, sf::Vector2f b, sf::Color c) {
    sf::Vertex v[2];
    v[0].position = a;
    v[0].color = c;
    v[1].position = b;
    v[1].color = c;
    target.draw(v, 2, sf::PrimitiveType::Lines);
}

// Places a text so its visible glyphs start at y and are centred on cx; shrinks it to maxW
void centerText(sf::Text& text, float cx, float y, float maxW) {
    sf::FloatRect lb = text.getLocalBounds();
    float scale = (lb.size.x > maxW && lb.size.x > 0.0f) ? maxW / lb.size.x : 1.0f;
    text.setScale({ scale, scale });
    text.setOrigin({ lb.position.x + lb.size.x / 2.0f, lb.position.y });
    text.setPosition({ std::round(cx), std::round(y) });
}

int windowHash(int towerIdx, int row, int col) {
    return ((row * 13 + col * 29 + towerIdx * 41 + 7) % 100 + 100) % 100;
}

} // namespace

UI_skyline::UI_skyline() {
    reset();
}

void UI_skyline::reset() {
    riseSeconds.assign(Showcase::skylinePlan().size(), -1.0f);
    seenDay = 0;
    seenSerial = 0;
    p1Brownout = false;
    p2Brownout = false;
    blackoutActive = false;
    blackoutT = 0.0f;
    boP1Failed = boP2Failed = false;
    boDemandMW = boP1MW = boP2MW = 0;
    soundCues.clear();
}

// Every tower planned up to `day` stands; the ones planned for `day` itself rise now when animateToday
void UI_skyline::syncToDay(int day, bool animateToday) {
    const auto& plan = Showcase::skylinePlan();
    const float built = Showcase::TOWER_RISE_SEC + Showcase::CRANE_LINGER_SEC + 1.0f;
    for (size_t i = 0; i < plan.size(); ++i) {
        if (plan[i].day > day || riseSeconds[i] >= 0.0f) continue;
        riseSeconds[i] = (animateToday && plan[i].day == day && day > 1) ? 0.0f : built;
    }
    seenDay = day;
}

void UI_skyline::update(float dt, const GameEngine& engine) {
    const int day = engine.getCurrentDay();
    const DaySettlement& s = engine.getLastDaySettlement();

    // A restarted (or loaded) match: start over from its current state without replaying anything
    if (seenDay != 0 && (day < seenDay || s.serial < seenSerial)) {
        reset();
    }
    if (seenDay == 0) {
        syncToDay(day, false);
        seenSerial = s.serial;
        p1Brownout = (s.serial > 0) && s.p1Failed;
        p2Brownout = (s.serial > 0) && s.p2Failed;
    }

    if (day > seenDay) {
        syncToDay(day, true);
    }

    for (float& r : riseSeconds) {
        if (r >= 0.0f && r < 60.0f) r += dt;
    }

    if (s.serial != seenSerial) {
        const bool nextInLine = (s.serial == seenSerial + 1);
        seenSerial = s.serial;
        p1Brownout = s.p1Failed;
        p2Brownout = s.p2Failed;
        if (nextInLine && (s.p1Failed || s.p2Failed) && engine.getCityState().winner == 0) {
            // (blackout set piece: HX-04)
        }
    }

    if (blackoutActive) {
        blackoutT += dt;
        if (blackoutT >= Showcase::BLACKOUT_DURATION) {
            blackoutActive = false;
            blackoutT = 0.0f;
        }
    }
}

void UI_skyline::triggerBlackout(bool p1Failed, bool p2Failed, int demandMW, int p1AvgMW, int p2AvgMW) {
    if (!p1Failed && !p2Failed) return;
    blackoutActive = true;
    blackoutT = 0.0f;
    boP1Failed = p1Failed;
    boP2Failed = p2Failed;
    boDemandMW = demandMW;
    boP1MW = p1AvgMW;
    boP2MW = p2AvgMW;
    p1Brownout = p1Brownout || p1Failed;
    p2Brownout = p2Brownout || p2Failed;
    soundCues.push_back("siren");
}

bool UI_skyline::takeSoundCue(std::string& outCue) {
    if (soundCues.empty()) return false;
    outCue = soundCues.front();
    soundCues.erase(soundCues.begin());
    return true;
}

bool UI_skyline::windowLit(bool lit, float winX, float winY, float towerTopY, float towerBottomY,
                           bool winInP1, int hash, float captureX) const {
    const int h = ((hash % 100) + 100) % 100;
    const bool brown = winInP1 ? p1Brownout : p2Brownout;
    const bool brownLit = lit && (!brown || h < Showcase::DISTRICT_BROWNOUT_PERCENT);

    const bool failing = winInP1 ? boP1Failed : boP2Failed;
    if (!blackoutActive || !failing) return brownLit;

    float d = winInP1 ? (captureX - winX) / std::max(20.0f, captureX - Showcase::CITY_LEFT)
                      : (winX - captureX) / std::max(20.0f, Showcase::CITY_RIGHT - captureX);
    float rowFrac = (winY - towerTopY) / std::max(1.0f, towerBottomY - towerTopY);
    switch (Showcase::blackoutWindowPhase(blackoutT, d, rowFrac, h)) {
        case Showcase::WindowPhase::BEFORE: return lit; // Still yesterday's lighting
        case Showcase::WindowPhase::DARK: return false;
        case Showcase::WindowPhase::RECOVERED: break;
    }
    return brownLit;
}

void UI_skyline::drawBackLayer(sf::RenderTarget& target, float captureX, bool isDaylight, float animTime) const {
    drawPlanLayer(target, static_cast<int>(Showcase::TowerLayer::BACK), captureX, isDaylight, animTime);
}

void UI_skyline::drawFrontLayer(sf::RenderTarget& target, float captureX, bool isDaylight, float animTime) const {
    drawPlanLayer(target, static_cast<int>(Showcase::TowerLayer::FRONT_EXTENSION), captureX, isDaylight, animTime);
    drawPlanLayer(target, static_cast<int>(Showcase::TowerLayer::FOREGROUND), captureX, isDaylight, animTime);
}

void UI_skyline::drawPlanLayer(sf::RenderTarget& target, int layerId, float captureX, bool isDaylight,
                               float animTime) const {
    using Showcase::TowerLayer;
    const TowerLayer layer = static_cast<TowerLayer>(layerId);
    const auto& plan = Showcase::skylinePlan();

    for (size_t i = 0; i < plan.size(); ++i) {
        const Showcase::TowerDef& t = plan[i];
        if (t.layer != layer || riseSeconds[i] < 0.0f) continue;

        const float rs = riseSeconds[i];
        const bool rising = rs < Showcase::TOWER_RISE_SEC;
        const float top = std::round(Showcase::currentTopY(t, rs));
        const float bottom = (layer == TowerLayer::FRONT_EXTENSION) ? t.baseY + 0.5f : t.baseY;
        const float h = bottom - top;
        if (h < 1.0f) continue;
        const bool westBuilt = (t.x + t.w * 0.5f) < 800.0f; // Side the tower originally belongs to

        // --- Body ---
        sf::Color fillP1 = isDaylight ? kP1BodyDay : kP1BodyNight;
        sf::Color fillP2 = isDaylight ? kP2BodyDay : kP2BodyNight;
        sf::Color outP1 = kP1Outline;
        sf::Color outP2 = kP2Outline;
        float thickness = 1.5f;
        if (layer == TowerLayer::BACK) {
            fillP1 = scaled(fillP1, 0.72f);
            fillP2 = scaled(fillP2, 0.72f);
            outP1 = withAlpha(kP1Outline, 120.0f);
            outP2 = withAlpha(kP2Outline, 120.0f);
            thickness = 1.0f;
        } else if (layer == TowerLayer::FOREGROUND) {
            fillP1 = scaled(fillP1, 1.12f);
            fillP2 = scaled(fillP2, 1.12f);
            thickness = 1.2f;
        }
        splitRect(target, t.x, top, t.w, h, captureX, fillP1, fillP2, outP1, outP2, thickness);

        // --- Roof (finished part only) ---
        if (layer != TowerLayer::BACK || !rising) {
            float rl = t.x + 4.0f;
            float rw = t.w - 8.0f;
            float roofH = (layer == TowerLayer::FOREGROUND) ? 4.0f : 6.0f;
            sf::Color roofP1 = (layer == TowerLayer::BACK) ? withAlpha(kP1Roof, 150.0f) : kP1Roof;
            sf::Color roofP2 = (layer == TowerLayer::BACK) ? withAlpha(kP2Roof, 150.0f) : kP2Roof;
            if (!rising) {
                splitRect(target, rl, top - roofH, rw, roofH, captureX, roofP1, roofP2,
                          sf::Color::Transparent, sf::Color::Transparent, 0.0f);
            }
        }

        // --- Scaffolding on the floors still under construction ---
        const float scaffoldH = rising ? std::min(h, 20.0f) : 0.0f;
        if (scaffoldH > 0.0f) {
            sf::Color sc = withAlpha(kScaffold, 210.0f);
            fillRect(target, t.x, top, t.w, scaffoldH, sf::Color(24, 28, 36, 235), sc, 1.0f);
            for (float sx = t.x; sx < t.x + t.w - 1.0f; sx += 8.0f) {
                float ex = std::min(sx + 8.0f, t.x + t.w);
                line(target, { sx, top }, { ex, top + scaffoldH }, sc);
                line(target, { ex, top }, { sx, top + scaffoldH }, sc);
            }
            // Welding sparks on the working floor
            for (int k = 0; k < 2; ++k) {
                float phase = std::fmod(animTime * 3.1f + static_cast<float>(i) * 0.37f + k * 0.5f, 1.0f);
                if (phase < 0.45f) {
                    float sx = t.x + 4.0f + std::fmod(static_cast<float>(i * 17 + k * 23) + animTime * 11.0f, std::max(1.0f, t.w - 8.0f));
                    sf::CircleShape spark(1.6f);
                    spark.setOrigin({ 1.6f, 1.6f });
                    spark.setPosition({ sx, top + scaffoldH - 2.0f });
                    spark.setFillColor(withAlpha(kSpark, 255.0f * (1.0f - phase / 0.45f)));
                    target.draw(spark);
                }
            }
        }

        // --- Windows ---
        float winW = 5.0f, winH = 7.0f, pitchX = 10.0f, pitchY = 15.0f;
        if (layer == TowerLayer::BACK) { winW = 4.0f; winH = 6.0f; pitchX = 8.0f; pitchY = 13.0f; }
        if (layer == TowerLayer::FOREGROUND) { winW = 5.0f; winH = 6.0f; pitchX = 9.0f; pitchY = 13.0f; }
        // New floors continue the window columns of the front tower below them (UI_city: x + c * 10)
        const bool frontCols = (layer == TowerLayer::FRONT_EXTENSION);
        const int maxCols = frontCols ? std::max(1, static_cast<int>(t.w / 11.0f) - 1) : 64;
        const float firstColX = frontCols ? t.x + 10.0f : t.x + pitchX * 0.6f;
        const float winTopLimit = top + scaffoldH + 5.0f;
        const float winBottomLimit = (layer == TowerLayer::FOREGROUND) ? bottom - 12.0f : bottom - 4.0f;
        int row = 0;
        // Rows are anchored to the base, so a rising tower reveals new floors instead of sliding them
        for (float wy = t.baseY - pitchY + (layer == TowerLayer::FRONT_EXTENSION ? 3.0f : 0.0f);
             wy >= winTopLimit; wy -= pitchY, ++row) {
            if (wy + winH > winBottomLimit) continue;
            int col = 0;
            for (float wx = firstColX; wx + winW <= t.x + t.w - 3.0f && col < maxCols; wx += pitchX, ++col) {
                const int hash = windowHash(static_cast<int>(i), row, col);
                const bool inP1 = (wx + winW * 0.5f) < captureX;
                bool lit = windowLit(hash < Showcase::DISTRICT_LIT_PERCENT, wx, wy, top, bottom, inP1, hash, captureX);
                sf::Color c = kWinOff;
                if (lit) {
                    c = isDaylight ? (inP1 ? kP1WinDay : kP2WinDay) : (inP1 ? kP1WinNight : kP2WinNight);
                }
                if (layer == TowerLayer::BACK) c.a = toAlpha(c.a * 0.78f);
                fillRect(target, wx, wy, winW, winH, c);
            }
        }

        // --- Foreground shop fronts glow in the district owner's colour ---
        if (layer == TowerLayer::FOREGROUND && h > 14.0f) {
            float stripY = bottom - 9.0f;
            bool shopLit = windowLit(true, t.x + t.w * 0.5f, stripY, top, bottom,
                                     (t.x + t.w * 0.5f) < captureX, static_cast<int>(i * 7) % 28, captureX);
            sf::Color sP1 = shopLit ? withAlpha(kP1Outline, isDaylight ? 120.0f : 210.0f) : kWinOff;
            sf::Color sP2 = shopLit ? withAlpha(kP2Outline, isDaylight ? 120.0f : 210.0f) : kWinOff;
            splitRect(target, t.x + 3.0f, stripY, t.w - 6.0f, 4.0f, captureX, sP1, sP2,
                      sf::Color::Transparent, sf::Color::Transparent, 0.0f);
        }

        // --- Hazard stripes on the conquered part of a new floor (same rule as UI_city) ---
        if (layer == TowerLayer::FRONT_EXTENSION && !rising) {
            float minX = t.x + 2.0f, maxX = t.x + t.w - 2.0f;
            float sx = westBuilt ? std::max(minX, captureX) : minX;
            float ex = westBuilt ? maxX : std::min(maxX, captureX);
            if (ex > sx + 2.0f) {
                for (float hy = top + 8.0f; hy < bottom - 4.0f; hy += 24.0f) {
                    fillRect(target, sx, hy, ex - sx, 3.0f, kHazard);
                }
            }
        }

        // --- Construction crane on the roof ---
        if (Showcase::craneVisible(rs)) {
            float fade = rising ? 1.0f : 1.0f - std::clamp((rs - Showcase::TOWER_RISE_SEC) / Showcase::CRANE_LINGER_SEC, 0.0f, 1.0f);
            sf::Color cc = withAlpha(kCrane, 255.0f * fade);
            float dir = westBuilt ? 1.0f : -1.0f; // Jib points towards the river
            float mastX = westBuilt ? t.x + t.w * 0.3f : t.x + t.w * 0.7f;
            float mastH = std::clamp(top - (kHeaderBottomY + 3.0f), 6.0f, 26.0f);
            float mastTop = top - mastH;
            fillRect(target, mastX - 1.5f, mastTop, 3.0f, mastH, cc);
            float jibLen = 30.0f, counterLen = 10.0f;
            float jibStart = (dir > 0.0f) ? mastX - counterLen : mastX - jibLen;
            fillRect(target, jibStart, mastTop, jibLen + counterLen, 2.0f, cc);
            fillRect(target, mastX - dir * counterLen - (dir > 0.0f ? 0.0f : 5.0f), mastTop + 2.0f, 5.0f, 4.0f,
                     withAlpha(sf::Color(90, 96, 110), 255.0f * fade));
            line(target, { mastX, mastTop - 5.0f }, { mastX + dir * jibLen, mastTop }, cc);
            fillRect(target, mastX - 1.0f, mastTop - 5.0f, 2.0f, 5.0f, cc);
            // Cable and swinging load
            float hookX = mastX + dir * jibLen * 0.72f + std::sin(animTime * 1.7f + static_cast<float>(i)) * 2.0f;
            float cableLen = 9.0f + 5.0f * std::sin(animTime * 0.9f + static_cast<float>(i));
            line(target, { mastX + dir * jibLen * 0.72f, mastTop + 2.0f }, { hookX, mastTop + 2.0f + cableLen },
                 withAlpha(sf::Color(200, 200, 210), 220.0f * fade));
            fillRect(target, hookX - 3.0f, mastTop + 2.0f + cableLen, 6.0f, 4.0f, withAlpha(sf::Color(150, 110, 70), 255.0f * fade));
            // Aviation light at the jib tip
            if (std::sin(animTime * 4.0f + static_cast<float>(i)) > 0.0f) {
                sf::CircleShape light(2.0f);
                light.setOrigin({ 2.0f, 2.0f });
                light.setPosition({ mastX + dir * jibLen, mastTop + 1.0f });
                light.setFillColor(withAlpha(kAlarmRed, 255.0f * fade));
                target.draw(light);
            }
        }
    }
}

void UI_skyline::drawCityOverlay(sf::RenderTarget& target, float captureX, float animTime) const {
    const float y0 = kHeaderBottomY;
    const float h = Showcase::CITY_TOP + Showcase::CITY_HEIGHT - y0;
    const float leftW = std::max(0.0f, captureX - Showcase::CITY_LEFT);
    const float rightW = std::max(0.0f, Showcase::CITY_RIGHT - captureX);

    // Brownout veil over the districts of a player who missed yesterday's demand
    // (fades in while the set piece recovers)
    float veil = 1.0f;
    if (blackoutActive) {
        veil = Showcase::smooth01((blackoutT - Showcase::BLACKOUT_RECOVER_START) / Showcase::BLACKOUT_RECOVER_SPAN);
    }
    if (veil > 0.0f) {
        sf::Color v = withAlpha(kBrownoutVeil, kBrownoutVeil.a * veil);
        if (p1Brownout) fillRect(target, Showcase::CITY_LEFT, y0, leftW, h, v);
        if (p2Brownout) fillRect(target, captureX, y0, rightW, h, v);
    }

    if (!blackoutActive) return;

    // Blackout darkness, street band by street band from the capture line outwards, and a pulsing
    // red alarm frame around each failing district
    const float dark = Showcase::blackoutCityDark(blackoutT);
    const float pulse = 0.5f + 0.5f * std::sin(animTime * 12.0f);
    sf::Color frame = withAlpha(kAlarmRed, (110.0f + 120.0f * pulse) * dark);
    auto drawStreets = [&](bool west, float width) {
        const float bandW = width / static_cast<float>(Showcase::BLACKOUT_STREETS);
        for (int s = 0; s < Showcase::BLACKOUT_STREETS; ++s) {
            float k = Showcase::blackoutStreetDark(blackoutT, s);
            if (k <= 0.0f) continue;
            // Street 0 touches the capture line
            float bx = west ? captureX - (s + 1) * bandW : captureX + s * bandW;
            fillRect(target, std::round(bx), y0, std::ceil(bandW), h, withAlpha(kCityDark, kCityDarkMaxAlpha * k));
        }
    };
    if (boP1Failed && leftW > 2.0f) {
        drawStreets(true, leftW);
        fillRect(target, Showcase::CITY_LEFT + 2.0f, y0 + 2.0f, leftW - 4.0f, h - 4.0f, sf::Color::Transparent, frame, 2.0f);
    }
    if (boP2Failed && rightW > 2.0f) {
        drawStreets(false, rightW);
        fillRect(target, captureX + 2.0f, y0 + 2.0f, rightW - 4.0f, h - 4.0f, sf::Color::Transparent, frame, 2.0f);
    }
}

void UI_skyline::drawWorldDim(sf::RenderTarget& target) const {
    if (!blackoutActive) return;
    float dim = Showcase::blackoutDim(blackoutT);
    if (dim <= 0.0f) return;
    fillRect(target, 0.0f, 0.0f, VIRTUAL_WIDTH, VIRTUAL_HEIGHT, withAlpha(kWorldDim, kWorldDimMaxAlpha * dim));
}

void UI_skyline::drawBlackoutBanner(sf::RenderTarget& target, const sf::Font& font, bool fontLoaded,
                                    float animTime) const {
    if (!blackoutActive || !fontLoaded) return;
    const float a = Showcase::blackoutBannerAlpha(blackoutT);
    if (a <= 0.0f) return;

    // Short shake when the banner slams in
    float shake = 0.0f;
    if (blackoutT < 0.85f) {
        float k = 1.0f - Showcase::smooth01((blackoutT - 0.45f) / 0.4f);
        shake = std::sin(blackoutT * 95.0f) * 3.0f * k;
    }

    const float w = Showcase::CITY_WIDTH - 12.0f;
    const float h = 96.0f;
    const float x = Showcase::CITY_LEFT + 6.0f + shake;
    const float y = 192.0f;
    const float cx = x + w / 2.0f;
    const float pulse = 0.5f + 0.5f * std::sin(animTime * 10.0f);

    fillRect(target, x, y, w, h, withAlpha(kBannerBg, 245.0f * a), withAlpha(kAlarmRed, (150.0f + 105.0f * pulse) * a), 2.0f);

    // Hazard stripes along the top and bottom edges (parallelograms clipped to the banner)
    const float stripeH = 9.0f;
    for (float sy : { y, y + h - stripeH }) {
        fillRect(target, x, sy, w, stripeH, withAlpha(kStripeDark, 255.0f * a));
        for (float sx = x - 20.0f; sx < x + w; sx += 18.0f) {
            sf::ConvexShape s(4);
            auto clampX = [x, w](float v) { return std::clamp(v, x, x + w); };
            s.setPoint(0, { clampX(sx + 8.0f), sy });
            s.setPoint(1, { clampX(sx + 17.0f), sy });
            s.setPoint(2, { clampX(sx + 9.0f), sy + stripeH });
            s.setPoint(3, { clampX(sx), sy + stripeH });
            s.setFillColor(withAlpha(kBannerAmber, 235.0f * a));
            target.draw(s);
        }
    }

    // Rotating-beacon style alarm lights on both sides of the title
    for (int side = 0; side < 2; ++side) {
        bool on = (std::sin(animTime * 9.0f + side * 3.14159f) > 0.0f);
        float bx = side == 0 ? x + 20.0f : x + w - 20.0f;
        float by = y + 30.0f;
        sf::CircleShape glow(11.0f);
        glow.setOrigin({ 11.0f, 11.0f });
        glow.setPosition({ bx, by });
        glow.setFillColor(withAlpha(kAlarmRed, (on ? 90.0f : 25.0f) * a));
        target.draw(glow);
        sf::CircleShape bulb(5.0f);
        bulb.setOrigin({ 5.0f, 5.0f });
        bulb.setPosition({ bx, by });
        bulb.setFillColor(on ? withAlpha(sf::Color(255, 90, 80), 255.0f * a) : withAlpha(sf::Color(110, 20, 20), 255.0f * a));
        target.draw(bulb);
    }

    sf::Text title(font, toUtf8("АВАРИЯ В ЕНЕРГОСИСТЕМАТА"), 21);
    title.setStyle(sf::Text::Bold);
    title.setFillColor(withAlpha(kBannerTitle, 255.0f * a));
    centerText(title, cx, y + 20.0f, w - 76.0f);
    target.draw(title);

    std::string who;
    std::string what;
    if (boP1Failed && boP2Failed) {
        who = "НИТО ЕДИН ИГРАЧ НЕ ЗАХРАНИ ГРАДА (" + std::to_string(boDemandMW) + " MW)";
        what = "ЦЕЛИЯТ ГРАД ОСТАВА НА ПОЛОВИН ТОК ДО УТРЕ";
    } else {
        int player = boP1Failed ? 1 : 2;
        int mw = boP1Failed ? boP1MW : boP2MW;
        who = "ИГРАЧ " + std::to_string(player) + " НЕ ЗАХРАНИ ГРАДА: " + std::to_string(mw) + " / " +
              std::to_string(boDemandMW) + " MW";
        what = "КВАРТАЛИТЕ МУ ОСТАВАТ НА ПОЛОВИН ТОК ДО УТРЕ";
    }
    sf::Text whoText(font, toUtf8(who), 13);
    whoText.setFillColor(withAlpha(kBannerAmber, 255.0f * a));
    centerText(whoText, cx, y + 50.0f, w - 24.0f);
    target.draw(whoText);

    sf::Text whatText(font, toUtf8(what), 11);
    whatText.setFillColor(withAlpha(kBannerDetail, 255.0f * a));
    centerText(whatText, cx, y + 69.0f, w - 24.0f);
    target.draw(whatText);
}
