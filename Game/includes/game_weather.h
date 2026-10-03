#ifndef GAME_WEATHER_H
#define GAME_WEATHER_H

#include <vector>
#include <string>

class GameRng;

// Daily weather report (from the weatherF branch): {cloud, precipitation, wind direction, wind speed}.
// cloud: "clear" / "cloudy"; precipitation: "clear" / "rain" / "thunder_storm" / "snow" / "hail";
// wind direction: "none" / "left" / "right"; wind speed: number as text ("0" when calm).
// The GameRng overload is the deterministic one the engine uses; the other draws from randomInt.
std::vector<std::string> weather_report(const std::string& season, GameRng& rng);
std::vector<std::string> weather_report(const std::string& season);

// -----------------------------------------------------------------------------
// Weather types for Player Sectors
// -----------------------------------------------------------------------------
enum class WeatherType {
    SUNNY,    // Слънчево (+60% Solar energy)
    WINDY,    // Ветровито (+80% Wind energy)
    RAINY,    // Дъждовно (+100% Hydro energy, -50% Solar)
    STORMY,   // Бурно (+120% Wind, +150% Hydro, -90% Solar)
    SNOWY,    // Снежно (сняг/градушка: -70% Solar, +20% Wind, нормален Hydro)
    CLOUDY    // Облачно (сухо, но облачно: -20% Solar, без +60% слънчев бонус)
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
    static float getSolarMultiplier(WeatherType w, float hour24, SeasonType season = SeasonType::SPRING);
    static float getWindMultiplier(WeatherType w, float hour24);
    static float getHydroMultiplier(WeatherType w);
    static WeatherType reportToWeatherType(const std::vector<std::string>& report);
};

#endif // GAME_WEATHER_H
