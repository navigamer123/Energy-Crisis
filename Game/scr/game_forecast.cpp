// =============================================================================
// [AI team] Power forecast helpers (see Game/includes/game_forecast.h)
// Mirrors the grid rules of GameEngine::updateBuildingsEnergy on a copy of the player's
// buildings, so the projection matches the real day-end settlement.
// =============================================================================
#include "../includes/game_forecast.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr float kStepSec = 0.25f; // same granularity as the engine's largest sub-step
constexpr WeatherType kAllWeather[] = { WeatherType::SUNNY, WeatherType::WINDY, WeatherType::RAINY,
                                        WeatherType::STORMY, WeatherType::SNOWY, WeatherType::CLOUDY };

struct SimGrid {
    float solarBase = 0.0f;
    float windBase = 0.0f;
    float hydroBase = 0.0f;
    int lamps = 0;
    std::vector<float> stored;   // battery charge (MWh)
    std::vector<float> capacity; // battery capacity (MWh)
};

SimGrid gridOf(const GameEngine& e, int player, bool emptyBatteries) {
    SimGrid g;
    for (const auto& b : e.getBuildings()) {
        if (b.playerOwner != player) continue;
        switch (b.type) {
            case BuildingType::SOLAR_PANEL:  g.solarBase += Balance::SOLAR_PANEL.basePowerMW; break;
            case BuildingType::WIND_TURBINE: g.windBase += Balance::WIND_TURBINE.basePowerMW; break;
            case BuildingType::HYDRO_PLANT:  g.hydroBase += Balance::HYDRO_PLANT.basePowerMW; break;
            case BuildingType::LAMP:         g.lamps++; break;
            case BuildingType::BATTERY:
                g.stored.push_back(emptyBatteries ? 0.0f : b.energyStored);
                g.capacity.push_back(b.maxCapacity);
                break;
            default: break;
        }
    }
    return g;
}

// Season in effect at clock hour h of the settlement day `day` (00:00-06:00 belongs to day + 1)
SeasonType seasonAtHour(int day, float h) {
    return Balance::getSeasonForDay(h < Balance::CLOCK_HOUR_AT_ZERO ? day + 1 : day);
}

// Delivered energy (MW x game-seconds) from `startHour` for `seconds` game-seconds.
// fixedSeason >= 0 forces one season for the whole span (full-day estimates for tomorrow).
float simulate(SimGrid g, WeatherType w, float startHour, float seconds, int day, int fixedSeason, int demandMW) {
    const float stepHours = Balance::gameSecondsToHours(kStepSec);
    const float load = g.lamps * GameEngine::LAMP_POWER_MW + static_cast<float>(std::max(0, demandMW));
    const float hydroOut = g.hydroBase * WeatherSystem::getHydroMultiplier(w);
    float delivered = 0.0f;
    float t = 0.0f;
    while (t < seconds - 1e-4f) {
        float dt = std::min(kStepSec, seconds - t);
        float hour = std::fmod(startHour + Balance::gameSecondsToHours(t + dt), 24.0f);
        SeasonType season = (fixedSeason >= 0) ? static_cast<SeasonType>(fixedSeason) : seasonAtHour(day, hour);
        float raw = g.solarBase * WeatherSystem::getSolarMultiplier(w, hour, season) +
                    g.windBase * WeatherSystem::getWindMultiplier(w, hour) + hydroOut;

        float discharge = 0.0f, charge = 0.0f;
        float sh = stepHours * (dt / kStepSec);
        if (!g.stored.empty() && sh > 0.0f) {
            if (raw < load) {
                float canGive = 0.0f;
                for (float s : g.stored) canGive += std::min(Balance::BATTERY_MAX_POWER_MW, s / sh);
                discharge = std::min(load - raw, canGive);
                if (discharge > 0.0f) {
                    float f = discharge / canGive;
                    for (float& s : g.stored) {
                        float p = std::min(Balance::BATTERY_MAX_POWER_MW, s / sh) * f;
                        s = std::max(0.0f, s - p * sh);
                    }
                }
            } else if (raw > load) {
                float canTake = 0.0f;
                for (size_t i = 0; i < g.stored.size(); ++i)
                    canTake += std::min(Balance::BATTERY_MAX_POWER_MW, std::max(0.0f, g.capacity[i] - g.stored[i]) / sh);
                charge = std::min(raw - load, canTake);
                if (charge > 0.0f) {
                    float f = charge / canTake;
                    for (size_t i = 0; i < g.stored.size(); ++i) {
                        float room = std::max(0.0f, g.capacity[i] - g.stored[i]);
                        float p = std::min(Balance::BATTERY_MAX_POWER_MW, room / sh) * f;
                        g.stored[i] = std::min(g.capacity[i], g.stored[i] + p * sh);
                    }
                }
            }
        }
        float available = std::max(0.0f, raw + discharge - charge);
        int powered = std::min(g.lamps, static_cast<int>((available + 0.001f) / GameEngine::LAMP_POWER_MW));
        delivered += std::max(0.0f, available - powered * GameEngine::LAMP_POWER_MW) * dt;
        t += dt;
    }
    return delivered;
}

} // namespace

namespace Forecast {

int demandForDay(int day) {
    if (day <= Balance::GRACE_PERIOD_DAYS) return 0;
    return Balance::STARTING_CITY_DEMAND_MW + (day - Balance::GRACE_PERIOD_DAYS - 1) * Balance::DAILY_DEMAND_INCREASE_MW;
}

float hoursUntilRollover(const GameEngine& engine) {
    float left = Balance::CLOCK_HOUR_AT_ZERO - engine.getHour24();
    if (left <= 0.0f) left += 24.0f;
    return left;
}

float projectTodayAverageMW(const GameEngine& engine, int player) {
    const CityConquestState& city = engine.getCityState();
    float deliveredSoFar = (player == 1) ? city.p1DailyDelivered : city.p2DailyDelivered;
    float remainingSec = hoursUntilRollover(engine) / 24.0f * Balance::SECONDS_PER_DAY;
    float future = simulate(gridOf(engine, player, false), engine.getPlayerWeather(player), engine.getHour24(),
                            remainingSec, engine.getCurrentDay(), -1, city.cityEnergyDemand);
    float total = city.dailySeconds + remainingSec;
    return (total > 0.0f) ? (deliveredSoFar + future) / total : 0.0f;
}

float fullDayAverageMW(const GameEngine& engine, int player, WeatherType weather, SeasonType season, int demandMW) {
    float delivered = simulate(gridOf(engine, player, true), weather, Balance::CLOCK_HOUR_AT_ZERO, Balance::SECONDS_PER_DAY,
                               engine.getCurrentDay(), static_cast<int>(season), demandMW);
    return delivered / Balance::SECONDS_PER_DAY;
}

float worstCaseDayAverageMW(const GameEngine& engine, int player, SeasonType season, int demandMW) {
    float worst = 1e9f;
    for (WeatherType w : kAllWeather) worst = std::min(worst, fullDayAverageMW(engine, player, w, season, demandMW));
    return worst;
}

float buildingDayAverageMW(BuildingType type, WeatherType weather, SeasonType season) {
    SimGrid g;
    switch (type) {
        case BuildingType::SOLAR_PANEL:  g.solarBase = Balance::SOLAR_PANEL.basePowerMW; break;
        case BuildingType::WIND_TURBINE: g.windBase = Balance::WIND_TURBINE.basePowerMW; break;
        case BuildingType::HYDRO_PLANT:  g.hydroBase = Balance::HYDRO_PLANT.basePowerMW; break;
        default: return 0.0f;
    }
    return simulate(g, weather, Balance::CLOCK_HOUR_AT_ZERO, Balance::SECONDS_PER_DAY, 1, static_cast<int>(season), 0) /
           Balance::SECONDS_PER_DAY;
}

} // namespace Forecast
