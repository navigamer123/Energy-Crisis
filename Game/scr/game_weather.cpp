#include "../includes/game_weather.h"
#include <cmath>

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
    // Night hours produce no solar energy
    if (hour24 < 6.0f || hour24 > 18.0f) {
        return 0.0f;
    }
    // Sun arc during the day (peaks at noon 12:00)
    float sunArc = std::sin((hour24 - 6.0f) / 12.0f * 3.14159265f);

    float weatherMod = 1.0f;
    switch (w) {
        case WeatherType::SUNNY:  weatherMod = 1.6f; break; // Sunnier days!
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
    int hash = (day * 37 + player * 13) % 100;
    if (hash < 35) return WeatherType::SUNNY;   // Many sunny days
    if (hash < 65) return WeatherType::WINDY;
    if (hash < 85) return WeatherType::RAINY;
    return WeatherType::STORMY;
}
