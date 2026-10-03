#ifndef UI_SHOWCASE_MODEL_H
#define UI_SHOWCASE_MODEL_H

// =============================================================================
// [b-showcase] Pure presentation model for the showcase features (no SFML):
//   - HX-03 living skyline: which city towers exist on which day and how they rise
//   - HX-04 blackout set piece: the 4-second timeline and the street-by-street window cascade
// Header-only so the headless tests (scratch/test_showcase.cpp) can check it without a window.
// =============================================================================

#include <algorithm>
#include <cmath>
#include <vector>

namespace Showcase {

// -----------------------------------------------------------------------------
// City geometry (matches UI_city::getCityBounds and the towers drawn by UI_city::drawCity)
// -----------------------------------------------------------------------------
constexpr float CITY_LEFT = 610.0f;
constexpr float CITY_TOP = 65.0f;
constexpr float CITY_WIDTH = 380.0f;
constexpr float CITY_HEIGHT = 330.0f;
constexpr float CITY_RIGHT = CITY_LEFT + CITY_WIDTH;
constexpr float CITY_GROUND_Y = 390.0f;   // Every tower stands on this line
constexpr float CITY_SKY_LIMIT_Y = 96.0f; // Nothing may rise above this (city header banner ends at 91)

// -----------------------------------------------------------------------------
// HX-03 Living skyline
// -----------------------------------------------------------------------------
enum class TowerLayer { BACK, FRONT_EXTENSION, FOREGROUND };

struct TowerDef {
    TowerLayer layer;
    float x;       // Left edge
    float w;       // Width
    float topY;    // Roof line once fully built (FRONT_EXTENSION: new roof line of the front tower)
    float baseY;   // Bottom edge (FRONT_EXTENSION: old roof line of the front tower it extends)
    int day;       // Construction starts at the beginning of this day (day 1 = already standing)
};

// Mirror of a west-side x span around the river (city centre x = 800)
inline float mirrorX(float x, float w) { return 1600.0f - x - w; }

// Growth plan: every entry is mirrored, so both sectors always grow equally (no side looks favoured)
inline const std::vector<TowerDef>& skylinePlan() {
    static const std::vector<TowerDef> plan = [] {
        const TowerDef west[] = {
            // Back row: peeks out above the lower front towers (front tops: 150, 105, 175, 120)
            { TowerLayer::BACK, 612.0f, 30.0f, 116.0f, CITY_GROUND_Y, 1 },
            { TowerLayer::BACK, 714.0f, 26.0f, 102.0f, CITY_GROUND_Y, 2 },
            { TowerLayer::BACK, 634.0f, 26.0f, 104.0f, CITY_GROUND_Y, 4 },
            { TowerLayer::BACK, 742.0f, 24.0f, 98.0f, CITY_GROUND_Y, 6 },
            // Extra floors on the two lower front towers (x/w of the front tower)
            { TowerLayer::FRONT_EXTENSION, 708.0f, 36.0f, 145.0f, 175.0f, 3 },
            { TowerLayer::FRONT_EXTENSION, 618.0f, 38.0f, 126.0f, 150.0f, 8 },
            { TowerLayer::FRONT_EXTENSION, 708.0f, 36.0f, 118.0f, 145.0f, 12 },
            // Foreground blocks along the boulevard (low-rise, in front of the towers)
            { TowerLayer::FOREGROUND, 626.0f, 46.0f, 318.0f, CITY_GROUND_Y, 5 },
            { TowerLayer::FOREGROUND, 690.0f, 40.0f, 332.0f, CITY_GROUND_Y, 7 },
            { TowerLayer::FOREGROUND, 738.0f, 44.0f, 310.0f, CITY_GROUND_Y, 10 },
        };
        std::vector<TowerDef> out;
        for (const TowerDef& t : west) {
            out.push_back(t);
            TowerDef east = t;
            east.x = mirrorX(t.x, t.w);
            out.push_back(east);
        }
        return out;
    }();
    return plan;
}

// Number of plan entries that have started construction by the given day
inline int towersStartedByDay(int day) {
    int n = 0;
    for (const TowerDef& t : skylinePlan()) {
        if (t.day <= day) ++n;
    }
    return n;
}

constexpr float TOWER_RISE_SEC = 7.0f;  // Real seconds a new tower or floor needs to rise
constexpr float CRANE_LINGER_SEC = 2.5f; // The crane stays this long after the roof is finished

// Ease-out cubic: fast start, gentle landing (0..1 -> 0..1)
inline float riseEase(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}

// Back-row towers are hidden behind the front towers below this line, so their visible rise
// starts here instead of at the ground (otherwise most of the animation would be hidden)
constexpr float BACK_RISE_FROM_Y = 190.0f;

// Current roof line of a tower, given how many real seconds it has been rising
inline float currentTopY(const TowerDef& t, float secondsRising) {
    float k = riseEase(secondsRising / TOWER_RISE_SEC);
    float from = (t.layer == TowerLayer::BACK) ? std::min(t.baseY, BACK_RISE_FROM_Y) : t.baseY;
    return from + (t.topY - from) * k;
}

inline bool craneVisible(float secondsRising) {
    return secondsRising < TOWER_RISE_SEC + CRANE_LINGER_SEC;
}

// Lit-window threshold (window hash 0..99 below it is lit) for a district
constexpr int DISTRICT_LIT_PERCENT = 65;        // Normal evening occupancy
constexpr int DISTRICT_BROWNOUT_PERCENT = 28;   // District whose owner failed the last settlement

// -----------------------------------------------------------------------------
// HX-04 Blackout set piece (times in real seconds since the settlement)
// -----------------------------------------------------------------------------
constexpr float BLACKOUT_DURATION = 4.0f;
constexpr float BLACKOUT_CASCADE_START = 0.25f;
constexpr int BLACKOUT_STREETS = 5;             // The failing territory goes dark in 5 bands ("streets")
constexpr float BLACKOUT_STREET_GAP = 0.30f;    // Delay between two streets
constexpr float BLACKOUT_ROW_RIPPLE = 0.16f;    // Top-to-bottom delay inside one street
constexpr float BLACKOUT_RECOVER_START = 3.0f;  // Lights start coming back
constexpr float BLACKOUT_RECOVER_SPAN = 0.8f;
constexpr float BLACKOUT_FLICKER_SEC = 0.14f;   // A window flickers this long before it dies

inline float smooth01(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// 0..1 dimming of the world layer (ground, plots, buildings, city)
inline float blackoutDim(float t) {
    if (t <= 0.0f || t >= BLACKOUT_DURATION) return 0.0f;
    if (t < 0.4f) return smooth01(t / 0.4f);
    if (t < BLACKOUT_RECOVER_START) return 1.0f;
    return 1.0f - smooth01((t - BLACKOUT_RECOVER_START) / (BLACKOUT_DURATION - BLACKOUT_RECOVER_START));
}

// 0..1 darkness laid over the failing part of the city (follows the cascade)
inline float blackoutCityDark(float t) {
    if (t <= 0.0f || t >= BLACKOUT_DURATION) return 0.0f;
    float cascadeEnd = BLACKOUT_CASCADE_START + BLACKOUT_STREETS * BLACKOUT_STREET_GAP;
    if (t < cascadeEnd) return smooth01((t - BLACKOUT_CASCADE_START * 0.5f) / cascadeEnd);
    if (t < BLACKOUT_RECOVER_START) return 1.0f;
    return 1.0f - smooth01((t - BLACKOUT_RECOVER_START) / BLACKOUT_RECOVER_SPAN);
}

// 0..1 darkness of one street band of a failing district (bands darken one after the other)
inline float blackoutStreetDark(float t, int street) {
    if (t <= 0.0f || t >= BLACKOUT_DURATION) return 0.0f;
    float start = BLACKOUT_CASCADE_START + static_cast<float>(street) * BLACKOUT_STREET_GAP;
    float k = smooth01((t - start) / (BLACKOUT_STREET_GAP + BLACKOUT_ROW_RIPPLE));
    if (t >= BLACKOUT_RECOVER_START) {
        k *= 1.0f - smooth01((t - BLACKOUT_RECOVER_START) / BLACKOUT_RECOVER_SPAN);
    }
    return k;
}

// 0..1 opacity of the emergency banner
inline float blackoutBannerAlpha(float t) {
    if (t < 0.45f || t >= 3.8f) return 0.0f;
    if (t < 0.75f) return smooth01((t - 0.45f) / 0.3f);
    if (t < 3.3f) return 1.0f;
    return 1.0f - smooth01((t - 3.3f) / 0.5f);
}

// Street (0 = at the frontier, BLACKOUT_STREETS-1 = far edge) of a window at distance fraction d
inline int blackoutStreet(float d) {
    int s = static_cast<int>(std::floor(std::clamp(d, 0.0f, 0.999f) * BLACKOUT_STREETS));
    return std::clamp(s, 0, BLACKOUT_STREETS - 1);
}

// Moment a window dies: street by street from the frontier outwards, top to bottom inside a street
inline float blackoutOffTime(float d, float rowFrac, int hash) {
    return BLACKOUT_CASCADE_START + blackoutStreet(d) * BLACKOUT_STREET_GAP +
           std::clamp(rowFrac, 0.0f, 1.0f) * BLACKOUT_ROW_RIPPLE + static_cast<float>(hash % 10) * 0.006f;
}

// Moment a window comes back (random order)
inline float blackoutOnTime(int hash) {
    return BLACKOUT_RECOVER_START + static_cast<float>(((hash * 37) % 100 + 100) % 100) / 100.0f * BLACKOUT_RECOVER_SPAN;
}

// Phase of one window of a failing district during the set piece
enum class WindowPhase {
    BEFORE,    // The cascade has not reached it yet: it keeps the lighting it had before the verdict
    DARK,      // Dead (or dark in its dying flicker)
    RECOVERED  // Power is back: lighting of the (browned-out) district
};

// d = 0 at the capture line .. 1 at the failing side's outer city edge,
// rowFrac = 0 at the roof .. 1 at the ground, hash = 0..99 per window.
inline WindowPhase blackoutWindowPhase(float t, float d, float rowFrac, int hash) {
    if (t <= 0.0f) return WindowPhase::BEFORE;
    if (t >= BLACKOUT_DURATION) return WindowPhase::RECOVERED;
    float off = blackoutOffTime(d, rowFrac, hash);
    float on = blackoutOnTime(hash);
    if (t >= on) return WindowPhase::RECOVERED;
    if (t >= off) return WindowPhase::DARK;
    if (t >= off - BLACKOUT_FLICKER_SEC) {
        // Dying flicker: deterministic on/off pattern
        return ((static_cast<int>(t * 38.0f) + hash) % 2 == 0) ? WindowPhase::DARK : WindowPhase::BEFORE;
    }
    return WindowPhase::BEFORE;
}

// True when the window must be dark at time t (lit windows only; see blackoutWindowPhase)
inline bool blackoutWindowOff(float t, float d, float rowFrac, int hash) {
    return blackoutWindowPhase(t, d, rowFrac, hash) == WindowPhase::DARK;
}

} // namespace Showcase

#endif // UI_SHOWCASE_MODEL_H
