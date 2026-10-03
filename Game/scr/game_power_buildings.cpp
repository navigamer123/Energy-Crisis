// =============================================================================
// Team b-power: shared core of the advanced buildings — costs, unlocks,
// placement rules (terrain, exclusion zones), outputs, storage, refunds,
// lightning absorption and the HazardFx/PowerFx queue for the UI.
// Per-system logic: game_nuclear.cpp (F-32), game_hazards.cpp (F-34),
// game_mega.cpp (HX-10). Terrain multipliers + river flow (F-15) are here.
// =============================================================================
#include "../includes/game_main.h"
#include <algorithm>
#include <cmath>

namespace {

const Balance::BuildingDef* advancedDef(BuildingType type) {
    switch (type) {
        case BuildingType::NUCLEAR:           return &PowerBalance::NUCLEAR;
        case BuildingType::GEOTHERMAL:        return &PowerBalance::GEOTHERMAL;
        case BuildingType::MEGA_FUSION:       return &PowerBalance::MEGA_FUSION;
        case BuildingType::MEGA_SPACE_SOLAR:  return &PowerBalance::MEGA_SPACE_SOLAR;
        case BuildingType::MEGA_PUMPED_HYDRO: return &PowerBalance::MEGA_PUMPED_HYDRO;
        default:                              return nullptr;
    }
}

bool isTerrain(const LandPlot* plot, TerrainType t) {
    return plot != nullptr && plot->terrain == static_cast<int>(t);
}

} // namespace

// -----------------------------------------------------------------------------
// Classification
// -----------------------------------------------------------------------------
bool GameEngine::isMegaProject(BuildingType type) {
    return type == BuildingType::MEGA_FUSION || type == BuildingType::MEGA_SPACE_SOLAR ||
           type == BuildingType::MEGA_PUMPED_HYDRO;
}

bool GameEngine::isPlotWideBuilding(BuildingType type) {
    return type == BuildingType::NUCLEAR || isMegaProject(type);
}

BuildingCost GameEngine::getAdvancedBuildingCost(BuildingType type) const {
    const Balance::BuildingDef* d = advancedDef(type);
    if (d == nullptr) {
        return { BuildingType::NONE, "", "", 0, 0, 0, 0, 0, 0, 0, 0 };
    }
    int ore = d->ironCost + d->copperCost + d->siliconCost;
    return { type, d->nameBg, d->nameEn, d->woodCost, d->ironCost, d->copperCost, d->coalCost,
             d->siliconCost, d->silverCost, ore, d->basePowerMW };
}

// -----------------------------------------------------------------------------
// Terrain (F-15): site modifiers and the shared river
// -----------------------------------------------------------------------------
int GameEngine::countHydroPlants() const {
    int h = 0;
    for (const auto& b : buildings) {
        if (b.isBroken) continue;
        if (b.type == BuildingType::HYDRO_PLANT) h += 1;
        // A finished pumped-hydro dam draws as much river water as two hydro plants
        if (b.type == BuildingType::MEGA_PUMPED_HYDRO && b.constructionLeft <= 0.0f) h += 2;
    }
    return h;
}

float GameEngine::getRiverFlowFactor() const {
    float h = static_cast<float>(countHydroPlants());
    return std::clamp(PowerBalance::RIVER_FLOW_BASE - PowerBalance::RIVER_FLOW_PER_PLANT * h,
                      PowerBalance::RIVER_FLOW_MIN, PowerBalance::RIVER_FLOW_MAX);
}

float GameEngine::terrainOutputMultiplier(const PlacedBuilding& b) const {
    TerrainType t = static_cast<TerrainType>(b.terrain);
    if (b.type == BuildingType::SOLAR_PANEL && t == TerrainType::MEADOW) return PowerBalance::MEADOW_SOLAR_MULT;
    if (b.type == BuildingType::WIND_TURBINE && t == TerrainType::HILL) return PowerBalance::HILL_WIND_MULT;
    return 1.0f;
}

// -----------------------------------------------------------------------------
// Output of the new generators and storage power
// -----------------------------------------------------------------------------
float GameEngine::advancedOutputMW(const PlacedBuilding& b, WeatherType w) const {
    const Balance::BuildingDef* d = advancedDef(b.type);
    if (d == nullptr || b.isBroken) return 0.0f;
    if (isMegaProject(b.type) && b.constructionLeft > 0.0f) return 0.0f; // still under construction
    float base = static_cast<float>(d->basePowerMW);
    switch (b.type) {
        case BuildingType::GEOTHERMAL:
            return base; // weather-proof baseload
        case BuildingType::NUCLEAR:
            if (b.scramTimer > 0.0f || b.needsFuel) return 0.0f;
            return base * std::clamp(b.rampProgress, 0.0f, 1.0f);
        case BuildingType::MEGA_FUSION:
            return base;
        case BuildingType::MEGA_SPACE_SOLAR:
            return base * ((w == WeatherType::STORMY) ? PowerBalance::SPACE_SOLAR_STORM_MULT : 1.0f);
        case BuildingType::MEGA_PUMPED_HYDRO:
            return base * getRiverFlowFactor(); // natural river flow through the dam turbines
        default:
            return 0.0f;
    }
}

float GameEngine::storageMaxPowerMW(const PlacedBuilding& b) const {
    return (b.type == BuildingType::MEGA_PUMPED_HYDRO) ? PowerBalance::PUMPED_HYDRO_MAX_POWER_MW
                                                       : Balance::BATTERY_MAX_POWER_MW;
}

// -----------------------------------------------------------------------------
// Unlocks and placement rules
// -----------------------------------------------------------------------------
const PlacedBuilding* GameEngine::getReactor(int player) const {
    for (const auto& b : buildings) {
        if (b.playerOwner == player && b.type == BuildingType::NUCLEAR) return &b;
    }
    return nullptr;
}

const PlacedBuilding* GameEngine::getMegaProject(int player) const {
    for (const auto& b : buildings) {
        if (b.playerOwner == player && isMegaProject(b.type)) return &b;
    }
    return nullptr;
}

float GameEngine::getConstructionProgress(const PlacedBuilding& b) const {
    if (b.constructionTotal <= 0.0f || b.constructionLeft <= 0.0f) return 1.0f;
    return std::clamp(1.0f - b.constructionLeft / b.constructionTotal, 0.0f, 1.0f);
}

bool GameEngine::checkAdvancedUnlock(int player, BuildingType type, std::string& reason) const {
    if (type == BuildingType::NUCLEAR) {
        int owned = countOwnedPlots(player);
        if (owned < PowerBalance::NUCLEAR_UNLOCK_PLOTS) {
            reason = "АЕЦ СЕ ОТКЛЮЧВА С " + std::to_string(PowerBalance::NUCLEAR_UNLOCK_PLOTS) +
                     " ПАРЦЕЛА! (Имате " + std::to_string(owned) + ")";
            return false;
        }
        if (getReactor(player) != nullptr) {
            reason = "ВЕЧЕ ИМАТЕ АЕЦ! Разрешен е само един реактор на играч.";
            return false;
        }
    } else if (isMegaProject(type)) {
        if (currentDay < PowerBalance::MEGA_UNLOCK_DAY) {
            reason = "МЕГАПРОЕКТИТЕ СЕ ОТКЛЮЧВАТ ОТ ДЕН " + std::to_string(PowerBalance::MEGA_UNLOCK_DAY) + "!";
            return false;
        }
        if (getMegaProject(player) != nullptr) {
            reason = "ВЕЧЕ ИМАТЕ МЕГАПРОЕКТ! Разрешен е само един на играч.";
            return false;
        }
    }
    return true;
}

bool GameEngine::isAdvancedUnlocked(int player, BuildingType type, std::string& reason) const {
    reason.clear();
    return checkAdvancedUnlock(player, type, reason);
}

int GameEngine::findPlotWideBuildingAt(int player, sf::Vector2f pos) const {
    const LandPlot* plot = findPlotAt(player, pos);
    if (plot == nullptr) return -1;
    for (size_t i = 0; i < buildings.size(); ++i) {
        const auto& b = buildings[i];
        if (b.playerOwner == player && isPlotWideBuilding(b.type) && plot->bounds.contains(b.position)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool GameEngine::isSlotReserved(int player, sf::Vector2f slot) const {
    return findPlotWideBuildingAt(player, slot) >= 0;
}

sf::Vector2f GameEngine::snapAdvanced(int player, BuildingType type, sf::Vector2f pos) const {
    if (!isPlotWideBuilding(type)) return pos;
    const LandPlot* plot = findPlotAt(player, pos);
    if (plot == nullptr) return pos;
    return sf::Vector2f(plot->bounds.position.x + plot->bounds.size.x * 0.5f,
                        plot->bounds.position.y + plot->bounds.size.y * 0.5f);
}

bool GameEngine::checkAdvancedPlacement(int player, BuildingType type, sf::Vector2f pos, std::string& reason) const {
    const LandPlot* plot = findPlotAt(player, pos);
    if (plot == nullptr) return true; // the land checks of canPlaceBuilding report this case

    // Exclusion zone: a reactor or mega-project owns its whole plot
    int wide = findPlotWideBuildingAt(player, pos);
    if (wide >= 0) {
        reason = "ЗАБРАНЕНА ЗОНА! Целият парцел е зает от " + getBuildingCost(buildings[static_cast<size_t>(wide)].type).nameBg + ".";
        return false;
    }
    if (isPlotWideBuilding(type)) {
        for (const auto& b : buildings) {
            if (b.playerOwner == player && plot->bounds.contains(b.position)) {
                reason = "ПАРЦЕЛЪТ ТРЯБВА ДА Е ПРАЗЕН!\n" + getBuildingCost(type).nameBg + " заема целия парцел (3x3).";
                return false;
            }
        }
    }
    if (type == BuildingType::GEOTHERMAL) {
        if (!isTerrain(plot, TerrainType::VENT)) {
            reason = "ГЕОТЕРМАЛНА ЦЕЦ СЕ СТРОИ САМО НА ГЕЙЗЕР!\nТърсете парцелите със знак ГЕЙЗЕР.";
            return false;
        }
        int wells = 0;
        for (const auto& b : buildings) {
            if (b.playerOwner == player && b.type == BuildingType::GEOTHERMAL && plot->bounds.contains(b.position)) ++wells;
        }
        if (wells >= PowerBalance::GEOTHERMAL_PER_VENT_PLOT) {
            reason = "ГЕЙЗЕРЪТ ИМА САМО " + std::to_string(PowerBalance::GEOTHERMAL_PER_VENT_PLOT) + " СОНДАЖА!\nПостройте на друг гейзер.";
            return false;
        }
    }
    if (type == BuildingType::MEGA_PUMPED_HYDRO && !isTerrain(plot, TerrainType::RIVER)) {
        reason = "ПАВЕЦ СЕ СТРОИ САМО НА РЕЧЕН БРЯГ!\nИзползвайте парцел със знак РЕКА.";
        return false;
    }
    return true;
}

// Called for every building just before it is added (sets terrain, starts ramps and construction)
void GameEngine::onAdvancedPlaced(PlacedBuilding& b) {
    const LandPlot* plot = findPlotAt(b.playerOwner, b.position);
    b.terrain = (plot != nullptr) ? plot->terrain : static_cast<int>(TerrainType::PLAIN);
    if (b.type == BuildingType::NUCLEAR) {
        b.rampProgress = 0.0f;
        b.currentOutputMW = 0.0f;
    } else if (isMegaProject(b.type)) {
        float days = (b.type == BuildingType::MEGA_FUSION) ? PowerBalance::MEGA_FUSION_BUILD_DAYS
                   : (b.type == BuildingType::MEGA_SPACE_SOLAR) ? PowerBalance::MEGA_SPACE_SOLAR_BUILD_DAYS
                                                                : PowerBalance::MEGA_PUMPED_HYDRO_BUILD_DAYS;
        b.constructionTotal = days * Balance::SECONDS_PER_DAY;
        b.constructionLeft = b.constructionTotal;
        b.currentOutputMW = 0.0f;
        if (b.type == BuildingType::MEGA_PUMPED_HYDRO) {
            b.maxCapacity = static_cast<float>(PowerBalance::MEGA_PUMPED_HYDRO.batteryCapacityMWh);
            b.energyStored = 0.0f;
        }
        PowerFx fx;
        fx.kind = PowerFxKind::MEGA_STARTED;
        fx.player = 0; // both players see it
        fx.buildingType = static_cast<int>(b.type);
        fx.owner = b.playerOwner;
        fx.pos = b.position;
        fx.title = "МЕГАПРОЕКТ: " + getBuildingCost(b.type).nameBg;
        fx.detail = "Играч " + std::to_string(b.playerOwner) + " започна строеж (" +
                    std::to_string(static_cast<int>(days)) + " дни).";
        pushPowerFx(fx);
    }
}

float GameEngine::advancedRefundFraction(BuildingType type) const {
    // A reactor or a mega-project is a commitment: demolishing it returns nothing
    return isPlotWideBuilding(type) ? 0.0f : Balance::DEMOLISH_REFUND_FRACTION;
}

// -----------------------------------------------------------------------------
// Hazards helpers shared by the systems
// -----------------------------------------------------------------------------
int GameEngine::countBrokenBuildings(int player) const {
    int n = 0;
    for (const auto& b : buildings) {
        if (b.isBroken && (player == 0 || b.playerOwner == player)) ++n;
    }
    return n;
}

void GameEngine::pushPowerFx(const PowerFx& fx) {
    world.fx.push_back(fx);
    if (world.fx.size() > PowerBalance::POWER_FX_QUEUE_MAX) {
        world.fx.erase(world.fx.begin()); // nobody drains it (headless tests): keep the newest
    }
}

std::vector<PowerFx> GameEngine::drainPowerFx() {
    std::vector<PowerFx> out;
    out.swap(world.fx);
    return out;
}

float GameEngine::rollUnit() {
    return static_cast<float>(world.rng() % 100000u) / 100000.0f;
}

// -----------------------------------------------------------------------------
// Per-step and per-day dispatch (called from game_main.cpp)
// -----------------------------------------------------------------------------
void GameEngine::updateAdvancedSystems(float dt) {
    if (dt <= 0.0f) return;
    updateReactors(dt);
    updateMegaProjects(dt);
    updateHazards(dt);
}

void GameEngine::advancedDayEnd() {
    payReactorFuel();
    updateWeatherStreaks();
}

void GameEngine::advancedNewDay() {
    scheduleDailyHazards();
    if (!world.megaUnlockAnnounced && currentDay >= PowerBalance::MEGA_UNLOCK_DAY) {
        world.megaUnlockAnnounced = true;
        PowerFx fx;
        fx.kind = PowerFxKind::MEGA_UNLOCKED;
        fx.player = 0;
        fx.title = "МЕГАПРОЕКТИТЕ СА ОТКЛЮЧЕНИ!";
        fx.detail = "Термоядрен реактор, космическа СЕЦ или ПАВЕЦ - по един на играч.";
        pushPowerFx(fx);
    }
}
