#include "../includes/game_weather.h"
#include "../includes/game_random.h"
#include "../includes/game_balance.h"
#include <cmath>
#include <iostream>

namespace {

// Rolls one weather report; roll(lo, hi) returns a uniform integer in [lo, hi].
// The order of the rolls is part of the format: the same generator state gives the same weather.
template <class RollFn>
std::vector<std::string> makeWeatherReport(const std::string& season, RollFn roll)
{
    // 0 = cloud, 1 = precipitation, 2 = wind direction, 3 = wind speed
    std::vector<std::string> Weather = {"clear", "clear", "none", "0"};
    int rain_roll = roll(1, 10);
    int wind_roll = roll(1, 10);
    int thunder_storm_roll = roll(1, 10);
    int snow_roll = roll(1, 10);
    int hail_roll = roll(1, 10);
    int cloudy_roll = roll(1, 10);
    int cloudy_num, wind_num, rain_num, thunder_storm_num, snow_num, hail_num;

    if (season == "spring") { cloudy_num = 6; wind_num = 4; rain_num = 4; thunder_storm_num = 8; snow_num = 10; hail_num = 10; }
    else if (season == "summer") { cloudy_num = 9; wind_num = 6; rain_num = 2; thunder_storm_num = 7; snow_num = 10; hail_num = 10; }
    else if (season == "fall") { cloudy_num = 4; wind_num = 4; rain_num = 2; thunder_storm_num = 7; snow_num = 9; hail_num = 9; }
    else if (season == "winter") { cloudy_num = 5; wind_num = 3; rain_num = 4; thunder_storm_num = 6; snow_num = 3; hail_num = 6; }
    else { return Weather; } // unknown season

    if (cloudy_roll > cloudy_num)
        Weather[0] = "cloudy";

    if (wind_roll > wind_num)
    {
        int knots = roll(1, 50);
        double wind_speed = knots * 1.9;
        Weather[2] = (roll(1, 2) == 1) ? "right" : "left";
        Weather[3] = std::to_string(wind_speed);
    }

    if (snow_roll > snow_num)
    {
        Weather[0] = "cloudy";
        Weather[1] = (hail_roll > hail_num) ? "hail" : "snow";
    }
    else if (rain_roll > rain_num)
    {
        Weather[0] = "cloudy";
        Weather[1] = (thunder_storm_roll > thunder_storm_num) ? "thunder_storm" : "rain";
    }

    return Weather;
}

} // namespace

// Returns {cloud, precipitation, wind direction, wind speed} from the legacy shared generator
std::vector<std::string> weather_report(const std::string& season)
{
    return makeWeatherReport(season, [](int lo, int hi) { return randomInt(lo, hi); });
}

std::vector<std::string> weather_report(const std::string& season, GameRng& rng)
{
    return makeWeatherReport(season, [&rng](int lo, int hi) { return rng.range(lo, hi); });
}

WeatherType WeatherSystem::reportToWeatherType(const std::vector<std::string>& report) {
    if (report.size() < 2) return WeatherType::SUNNY;
    const std::string& precip = report[1];
    if (precip == "thunder_storm") return WeatherType::STORMY;
    if (precip == "rain") return WeatherType::RAINY;
    if (precip == "snow" || precip == "hail") return WeatherType::SNOWY;
    if (report.size() >= 4 && !report[3].empty() && report[3] != "0") {
        try {
            if (std::stod(report[3]) > 18.0) return WeatherType::WINDY;
        } catch (...) {}
    }
    // Dry but overcast days must not get the full sunny solar bonus
    if (report[0] == "cloudy") return WeatherType::CLOUDY;
    return WeatherType::SUNNY;
}

const char* getWeatherName(WeatherType w) {
    switch (w) {
        case WeatherType::SUNNY:  return "Слънчево (Sunny)";
        case WeatherType::WINDY:  return "Ветровито (Windy)";
        case WeatherType::RAINY:  return "Дъждовно (Rainy)";
        case WeatherType::STORMY: return "Бурно (Stormy)";
        case WeatherType::SNOWY:  return "Снежно (Snowy)";
        case WeatherType::CLOUDY: return "Облачно (Cloudy)";
    }
    return "Слънчево";
}

const char* getSeasonName(SeasonType s) {
    switch (s) {
        case SeasonType::SPRING: return "Пролет (Spring)";
        case SeasonType::SUMMER: return "Лято (Summer)";
        case SeasonType::AUTUMN: return "Есен (Autumn)";
        case SeasonType::WINTER: return "Зима (Winter)";
    }
    return "Пролет";
}

float WeatherSystem::getSolarMultiplier(WeatherType w, float hour24, SeasonType season) {
    float sunrise = Balance::getSunriseHour(season);
    float sunset = Balance::getSunsetHour(season);
    // Same daylight window as Balance::isDaylightAt: night from the sunset minute on
    // (sin(float pi) is slightly negative, so the sun arc must not be evaluated at sunset)
    if (hour24 < sunrise || hour24 >= sunset) {
        return 0.0f;
    }
    float dayDuration = sunset - sunrise;
    if (dayDuration <= 0.0f) return 0.0f;

    float sunArc = std::sin((hour24 - sunrise) / dayDuration * 3.14159265f);
    float weatherMod = 1.0f;
    switch (w) {
        case WeatherType::SUNNY:  weatherMod = 1.6f; break;
        case WeatherType::WINDY:  weatherMod = 1.0f; break;
        case WeatherType::RAINY:  weatherMod = 0.5f; break;
        case WeatherType::STORMY: weatherMod = 0.1f; break;
        case WeatherType::SNOWY:  weatherMod = 0.3f; break;
        case WeatherType::CLOUDY: weatherMod = 0.8f; break;
    }

    // Seasonal solar irradiance adjustment (Summer has +15% stronger solar peak, Winter -15%)
    float seasonMod = 1.0f;
    if (season == SeasonType::SUMMER) seasonMod = 1.15f;
    else if (season == SeasonType::AUTUMN) seasonMod = 0.95f;
    else if (season == SeasonType::WINTER) seasonMod = 0.85f;

    return sunArc * weatherMod * seasonMod;
}

float WeatherSystem::getWindMultiplier(WeatherType w, float hour24) {
    // 24 h period (continuous across midnight): strongest wind at 15:00, calmest at 03:00
    float timeMod = 1.0f + 0.15f * std::sin((hour24 - 9.0f) * (2.0f * 3.14159265f / 24.0f));
    float weatherMod = 1.0f;
    switch (w) {
        case WeatherType::SUNNY:  weatherMod = 0.8f; break;
        case WeatherType::WINDY:  weatherMod = 1.8f; break;
        case WeatherType::RAINY:  weatherMod = 1.2f; break;
        case WeatherType::STORMY: weatherMod = 2.2f; break;
        case WeatherType::SNOWY:  weatherMod = 1.2f; break;
        case WeatherType::CLOUDY: weatherMod = 0.8f; break;
    }
    return timeMod * weatherMod;
}

float WeatherSystem::getHydroMultiplier(WeatherType w) {
    switch (w) {
        case WeatherType::SUNNY:  return 0.7f;
        case WeatherType::WINDY:  return 1.0f;
        case WeatherType::RAINY:  return 2.0f;
        case WeatherType::STORMY: return 2.5f;
        case WeatherType::SNOWY:  return 1.0f; // Snow stays on the ground; no instant river boost
        case WeatherType::CLOUDY: return 0.8f;
    }
    return 1.0f;
}
