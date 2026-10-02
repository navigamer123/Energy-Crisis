#ifndef GAME_WEATHER_H
#define GAME_WEATHER_H

#include <vector>
#include <string>

// Teammate's globals from weatherF
extern std::string weather_state;
extern bool wind;
extern std::string wind_direction;

// Returns {cloud, precipitation, wind direction, wind speed}
std::vector<std::string> weather_report(const std::string& season);

// -----------------------------------------------------------------------------
// Weather types for Player Sectors
// -----------------------------------------------------------------------------
enum class WeatherType {
    SUNNY,    // Слънчево (+60% Solar energy)
    WINDY,    // Ветровито (+80% Wind energy)
    RAINY,    // Дъждовно (+100% Hydro energy, -50% Solar)
    STORMY    // Бурно (+120% Wind, +150% Hydro, -90% Solar)
};

// -----------------------------------------------------------------------------
// Seasons
// -----------------------------------------------------------------------------
enum class SeasonType {
    SPRING,   // Пролет
    SUMMER,   // Лято
    AUTUMN,   // Есен
    WINTER    // Зима
};

const char* getWeatherName(WeatherType w);
const char* getSeasonName(SeasonType s);

class WeatherSystem {
public:
    static float getSolarMultiplier(WeatherType w, float hour24);
    static float getWindMultiplier(WeatherType w, float hour24);
    static float getHydroMultiplier(WeatherType w);
    static WeatherType generateDailyWeather(int day, int player);
    static WeatherType reportToWeatherType(const std::vector<std::string>& report);
};

#endif // GAME_WEATHER_H
