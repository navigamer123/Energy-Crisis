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

// Clock mapping: gameSeconds = 0 is 06:00 of day 1. A new day (and its single day-end
// settlement) starts at every 06:00. Seasons switch at the preceding midnight, when it is
// dark in every season, so daylight never toggles back at a season change.
constexpr float CLOCK_HOUR_AT_ZERO = 6.0f;     // Hour shown at gameSeconds = 0 (= daily rollover hour)
constexpr float MATCH_START_HOUR = 8.0f;       // A new match starts at 08:00 of day 1
constexpr int DAYS_PER_SEASON = 5;

// Converts game seconds to in-game hours (90 s = 24 h -> 1 game-hour = 3.75 s).
// daySeconds: length of a day in this match (MatchConfig::daySeconds), standard 90 s.
inline float gameSecondsToHours(float seconds, float daySeconds = SECONDS_PER_DAY) {
    return seconds * 24.0f / daySeconds;
}

// gameSeconds value at which the given clock hour of day 1 is reached (hour >= CLOCK_HOUR_AT_ZERO)
inline float gameSecondsAtHour(float hour24, float daySeconds = SECONDS_PER_DAY) {
    return (hour24 - CLOCK_HOUR_AT_ZERO) / 24.0f * daySeconds;
}

inline SeasonType getSeasonForDay(int day, int finalDay = 20) {
    int d = (day < 1) ? 1 : day;
    int daysPerSeason = std::max(1, finalDay / 4);
    int seasonIdx = (d - 1) / daysPerSeason;
    if (seasonIdx > 3) seasonIdx = 3;
    return static_cast<SeasonType>(seasonIdx % 4);
}

// Season in effect at a given game time: it already belongs to the next day from midnight on
inline SeasonType getSeasonAtGameSeconds(float gameSeconds, float daySeconds = SECONDS_PER_DAY, int finalDay = 20) {
    float secondsFromMidnightToRollover = CLOCK_HOUR_AT_ZERO / 24.0f * daySeconds; // 00:00 -> 06:00
    int calendarDay = 1 + static_cast<int>(std::floor((gameSeconds + secondsFromMidnightToRollover) / daySeconds));
    return getSeasonForDay(calendarDay, finalDay);
}

// Season keyword expected by weather_report()
inline const char* getSeasonWeatherKey(SeasonType season) {
    switch (season) {
        case SeasonType::SPRING: return "spring";
        case SeasonType::SUMMER: return "summer";
        case SeasonType::AUTUMN: return "fall";
        case SeasonType::WINTER: return "winter";
    }
    return "spring";
}

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
