#include "../includes/game_weather.h"
#include "../includes/game_random.h" // for randomInt
#include <cmath>
#include <iostream>

std::string weather_state = "clear";
bool wind = false;
std::string wind_direction = "none";

// Returns {cloud, precipitation, wind direction, wind speed}
std::vector<std::string> weather_report(const std::string& season)
{
    // 0 = cloud, 1 = precipitation, 2 = wind direction, 3 = wind speed
    std::vector<std::string> Weather = {"clear", "clear", "none", "0"};
    int rain_roll = randomInt(1, 10);
    int wind_roll = randomInt(1, 10);
    int thunder_storm_roll = randomInt(1, 10);
    int snow_roll = randomInt(1, 10);
    int hail_roll = randomInt(1, 10);
    int cloudy_roll = randomInt(1, 10);
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
        int knots = randomInt(1, 50);
        double wind_speed = knots * 1.9;
        Weather[2] = (randomInt(1, 2) == 1) ? "right" : "left";
        Weather[3] = std::to_string(wind_speed);
        wind = true;
        wind_direction = Weather[2];
    }
    else
    {
        wind = false;
        wind_direction = "none";
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

    weather_state = (Weather[1] != "clear") ? Weather[1] : Weather[0];
    return Weather;
}

WeatherType WeatherSystem::reportToWeatherType(const std::vector<std::string>& report) {
    if (report.size() < 2) return WeatherType::SUNNY;
    const std::string& precip = report[1];
    if (precip == "thunder_storm") return WeatherType::STORMY;
    if (precip == "rain" || precip == "snow" || precip == "hail") return WeatherType::RAINY;
    if (report.size() >= 4 && !report[3].empty() && report[3] != "0") {
        try {
            if (std::stod(report[3]) > 18.0) return WeatherType::WINDY;
        } catch (...) {}
    }
    return WeatherType::SUNNY;
}

const char* getWeatherName(WeatherType w) {
    switch (w) {
        case WeatherType::SUNNY:  return "Слънчево (Sunny)";
        case WeatherType::WINDY:  return "Ветровито (Windy)";
        case WeatherType::RAINY:  return "Дъждовно (Rainy)";
        case WeatherType::STORMY: return "Бурно (Stormy)";
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

float WeatherSystem::getSolarMultiplier(WeatherType w, float hour24) {
    if (hour24 < 6.0f || hour24 > 18.0f) {
        return 0.0f;
    }
    float sunArc = std::sin((hour24 - 6.0f) / 12.0f * 3.14159265f);
    float weatherMod = 1.0f;
    switch (w) {
        case WeatherType::SUNNY:  weatherMod = 1.6f; break;
        case WeatherType::WINDY:  weatherMod = 1.0f; break;
        case WeatherType::RAINY:  weatherMod = 0.5f; break;
        case WeatherType::STORMY: weatherMod = 0.1f; break;
    }
    return sunArc * weatherMod;
}

float WeatherSystem::getWindMultiplier(WeatherType w, float hour24) {
    float timeMod = 1.0f + 0.15f * std::sin(hour24 * 0.5f);
    float weatherMod = 1.0f;
    switch (w) {
        case WeatherType::SUNNY:  weatherMod = 0.8f; break;
        case WeatherType::WINDY:  weatherMod = 1.8f; break;
        case WeatherType::RAINY:  weatherMod = 1.2f; break;
        case WeatherType::STORMY: weatherMod = 2.2f; break;
    }
    return timeMod * weatherMod;
}

float WeatherSystem::getHydroMultiplier(WeatherType w) {
    switch (w) {
        case WeatherType::SUNNY:  return 0.7f;
        case WeatherType::WINDY:  return 1.0f;
        case WeatherType::RAINY:  return 2.0f;
        case WeatherType::STORMY: return 2.5f;
    }
    return 1.0f;
}

WeatherType WeatherSystem::generateDailyWeather(int day, int player) {
    // Generate using weather_report from weatherF
    std::string sName = (day % 4 == 1) ? "spring" : ((day % 4 == 2) ? "summer" : ((day % 4 == 3) ? "fall" : "winter"));
    auto report = weather_report(sName);
    (void)player;
    return reportToWeatherType(report);
}
