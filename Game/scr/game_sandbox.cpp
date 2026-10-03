// =============================================================================
// ENERGY CRISIS - F-21 PRACTICE SANDBOX                                     [team b-options]
// Infinite resources, a pausable clock, weather / season / demand control and a projected
// 24 h output curve. The setters work in any mode (handy for tests and debug scenes); the UI
// only offers them when MatchRules::sandbox is set.
// =============================================================================
#include "../includes/game_main.h"
#include <algorithm>
#include <cmath>

namespace {

// Stock kept in every sandbox inventory (fits the corner HUD; refilled every frame)
constexpr int SANDBOX_RESOURCE_STOCK = 9999;
constexpr int SANDBOX_GOLD_STOCK = 99999;
constexpr int SANDBOX_MONEY_STOCK = 999999;
constexpr float SANDBOX_MAX_CLOCK_SPEED = 32.0f;
constexpr int SANDBOX_MAX_DEMAND_MW = 5000;

const char* weatherKey(WeatherType w) {
    switch (w) {
        case WeatherType::RAINY:  return "rain";
        case WeatherType::STORMY: return "thunder_storm";
        case WeatherType::SNOWY:  return "snow";
        case WeatherType::CLOUDY: return "cloudy";
        case WeatherType::WINDY:
        case WeatherType::SUNNY:
        default:                  return "clear";
    }
}

} // namespace

void GameEngine::applyFrameRules() {
    if (!rules.sandbox) return;
    for (PlayerEconomy* e : { &p1, &p2 }) {
        e->wood = e->iron = e->copper = e->coal = e->silicon = e->silver = SANDBOX_RESOURCE_STOCK;
        e->gold = SANDBOX_GOLD_STOCK;
        e->money = SANDBOX_MONEY_STOCK;
        e->data.wood = e->data.iron = e->data.copper = e->data.coal = e->data.silicon = e->data.silver = SANDBOX_RESOURCE_STOCK;
        e->data.gold = SANDBOX_GOLD_STOCK;
        e->data.money = SANDBOX_MONEY_STOCK;
    }
}

void GameEngine::sandboxSetClockSpeed(float speed) {
    if (!(speed == speed)) speed = 1.0f; // NaN guard
    sandbox.clockSpeed = std::max(0.0f, std::min(speed, SANDBOX_MAX_CLOCK_SPEED));
}

void GameEngine::sandboxSetHour(float hour) {
    if (!(hour == hour)) return;
    // A day runs 06:00 -> 06:00; stay inside the current day so no settlement is skipped
    float offsetHours = std::fmod(hour - Balance::CLOCK_HOUR_AT_ZERO + 48.0f, 24.0f);
    float dayStart = static_cast<float>(currentDay - 1) * Balance::SECONDS_PER_DAY;
    float target = dayStart + offsetHours / 24.0f * Balance::SECONDS_PER_DAY;
    target = std::min(target, dayStart + Balance::SECONDS_PER_DAY - 0.05f);
    gameSeconds = target;
    hour24 = std::fmod((gameSeconds / Balance::SECONDS_PER_DAY) * 24.0f + Balance::CLOCK_HOUR_AT_ZERO, 24.0f);
    currentSeason = seasonAtGameSeconds(gameSeconds);
    // The daily average restarts from the jump, so the day-end verdict matches what was seen
    city.p1DailyDelivered = 0.0f;
    city.p2DailyDelivered = 0.0f;
    city.dailySeconds = 0.0f;
    updateBuildingsEnergy(0.0f);
}

void GameEngine::sandboxSetWeather(int player, WeatherType w) {
    int i = (player == 2) ? 1 : 0;
    sandbox.weatherLocked[i] = true;
    sandbox.lockedWeather[i] = w;
    PlayerEconomy& econ = (i == 0) ? p1 : p2;
    (i == 0 ? p1Weather : p2Weather) = w;
    econ.data.weather = weatherKey(w);
    econ.data.wind_speed = (w == WeatherType::WINDY || w == WeatherType::STORMY) ? "60" : "0";
    updateBuildingsEnergy(0.0f);
}

void GameEngine::sandboxUnlockWeather() {
    sandbox.weatherLocked[0] = false;
    sandbox.weatherLocked[1] = false;
}

void GameEngine::sandboxSetSeason(int season) {
    sandbox.seasonOverride = (season >= 0 && season <= 3) ? season : -1;
    currentSeason = seasonAtGameSeconds(gameSeconds);
    updateBuildingsEnergy(0.0f);
}

void GameEngine::sandboxSetDemand(int demandMW) {
    demandMW = std::max(0, std::min(demandMW, SANDBOX_MAX_DEMAND_MW));
    sandbox.demandLocked = true;
    sandbox.lockedDemandMW = demandMW;
    city.cityEnergyDemand = demandMW;
    updateBuildingsEnergy(0.0f);
}

int GameEngine::sandboxClearBuildings(int player) {
    size_t before = buildings.size();
    buildings.erase(std::remove_if(buildings.begin(), buildings.end(),
                                   [player](const PlacedBuilding& b) { return b.playerOwner == player; }),
                    buildings.end());
    updateBuildingsEnergy(0.0f);
    return static_cast<int>(before - buildings.size());
}

float GameEngine::projectGenerationMW(int player, float hour) const {
    // Raw generation (before lamps and batteries) of the player's current buildings at the
    // given hour, with today's weather and season and the player's perks
    const PlayerPerks perks = getPlayerPerks(player);
    const WeatherType w = getPlayerWeather(player);
    float h = std::fmod(hour + 48.0f, 24.0f);
    float total = 0.0f;
    for (const auto& b : buildings) {
        if (b.playerOwner != player) continue;
        float base = static_cast<float>(getBuildingCost(b.type).basePowerMW);
        if (b.type == BuildingType::SOLAR_PANEL) {
            total += base * WeatherSystem::getSolarMultiplier(w, h, currentSeason) * perks.solarOutputMult;
        } else if (b.type == BuildingType::WIND_TURBINE) {
            total += base * WeatherSystem::getWindMultiplier(w, h) * perks.windOutputMult;
        } else if (b.type == BuildingType::HYDRO_PLANT) {
            total += base * WeatherSystem::getHydroMultiplier(w) * perks.hydroOutputMult;
        }
    }
    return total;
}
