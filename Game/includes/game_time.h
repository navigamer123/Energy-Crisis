#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include "game_weather.h"

// =============================================================================
// ENERGY CRISIS - TIME, SEASONS & ADAPTIVE SUN SYSTEM
// Manages continuous 24h clock, seasonal daylight, sunrise/sunset, and grace period
// =============================================================================

namespace Balance {

// -----------------------------------------------------------------------------
// 1. Time & Grace Period Settings
// -----------------------------------------------------------------------------
constexpr float SECONDS_PER_DAY = 90.0f;       // Length of 1 full 24h day in seconds (90s = 1 day)
constexpr int GRACE_PERIOD_DAYS = 2;           // First 2 days city requires 0 MW so players expand safely
constexpr float DAY_START_HOUR = 6.0f;         // Fallback daylight start
constexpr float DAY_END_HOUR = 18.0f;          // Fallback daylight end
constexpr float MINE_SPEEDUP_MULT = 6.0f;      // Time advances 6x faster when actively gathering resources
constexpr float MINE_COOLDOWN_SEC = 1.0f;      // Cooldown between resource gathering actions (1.0s)

// -----------------------------------------------------------------------------
// 2. Seasonal Sunrise & Sunset Schedule (Adaptive Day-Night Cycle)
//
// Spring (Пролет): 06:00 - 19:00 (13.0h daylight) - Balanced solar generation
// Summer (Лято):   05:00 - 21:00 (16.0h daylight) - Early sunrise, late sunset, +15% peak solar
// Autumn (Есен):   07:00 - 18:00 (11.0h daylight) - Cooling winds, shorter daylight window
// Winter (Зима):   08:00 - 16:30 (8.5h daylight)  - Late sunrise, early darkness at 16:30, snow
// -----------------------------------------------------------------------------
inline float getSunriseHour(SeasonType season) {
    switch (season) {
        case SeasonType::SPRING: return 6.0f;   // 06:00
        case SeasonType::SUMMER: return 5.0f;   // 05:00
        case SeasonType::AUTUMN: return 7.0f;   // 07:00
        case SeasonType::WINTER: return 8.0f;   // 08:00
    }
    return 6.0f;
}

inline float getSunsetHour(SeasonType season) {
    switch (season) {
        case SeasonType::SPRING: return 19.0f;  // 19:00
        case SeasonType::SUMMER: return 21.0f;  // 21:00
        case SeasonType::AUTUMN: return 18.0f;  // 18:00
        case SeasonType::WINTER: return 16.5f;  // 16:30
    }
    return 18.0f;
}

inline float getDaylightDuration(SeasonType season) {
    return getSunsetHour(season) - getSunriseHour(season);
}

inline bool isDaylightAt(float hour24, SeasonType season) {
    float rise = getSunriseHour(season);
    float set = getSunsetHour(season);
    return hour24 >= rise && hour24 < set;
}

// -----------------------------------------------------------------------------
// 3. Time Formatting Helpers
// -----------------------------------------------------------------------------
inline std::string formatHourMinute(float hour24) {
    int h = static_cast<int>(hour24);
    int m = static_cast<int>(std::round((hour24 - static_cast<float>(h)) * 60.0f));
    if (m >= 60) { h++; m -= 60; }
    h = h % 24;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
    return std::string(buf);
}

inline std::string formatHourMinute12(float hour24) {
    int h = static_cast<int>(hour24);
    int m = static_cast<int>(std::round((hour24 - static_cast<float>(h)) * 60.0f));
    if (m >= 60) { h++; m -= 60; }
    bool isPM = (h >= 12);
    int h12 = h % 12;
    if (h12 == 0) h12 = 12;
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%02d:%02d %s", h12, m, isPM ? "PM" : "AM");
    return std::string(buf);
}

} // namespace Balance
