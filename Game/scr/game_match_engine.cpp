// =============================================================================
// ENERGY CRISIS - GameEngine glue for match rules, mutators and charters   [team b-options]
// F-03 (MatchRules), F-24 (mutators), F-35 (charters). The hooks that call these functions
// are marked "[b-options]" in game_main.cpp.
// =============================================================================
#include "../includes/game_main.h"
#include <algorithm>
#include <cmath>

namespace {

// F-24 Head Start mutator stockpile
constexpr int HEAD_START_RESOURCES = 40;
constexpr int HEAD_START_GOLD = 300;

// Price of a land plot before perks. Mirrors the layout of GameEngine::init(): plot ids
// 1..12 are P1 (row-major from the top-left), 13..24 are P2 whose start column is c == 2.
int baseLandPlotCost(const LandPlot& plot) {
    int idx = (plot.playerOwner == 1) ? (plot.id - 1) : (plot.id - 1 - Balance::PLOTS_PER_PLAYER);
    idx = std::max(0, std::min(idx, Balance::PLOTS_PER_PLAYER - 1));
    int r = idx / 3;
    int c = idx % 3;
    return (plot.playerOwner == 1) ? Balance::getLandPlotCost(r, c) : Balance::getLandPlotCost(r, 2 - c);
}

int scaleCost(int base, float mult) {
    if (base <= 0) return 0;
    if (mult == 1.0f) return base;
    return std::max(1, static_cast<int>(std::lround(static_cast<float>(base) * mult)));
}

} // namespace

// -----------------------------------------------------------------------------
// Rules
// -----------------------------------------------------------------------------
void GameEngine::setMatchRules(const MatchRules& newRules) {
    rules = newRules;
    rules.validate();
}

float GameEngine::getPaceMultiplier() const {
    // The simulation always counts 90 game-seconds per day; a shorter real day just runs it faster
    float pace = Balance::SECONDS_PER_DAY / std::max(1.0f, rules.daySeconds);
    return pace * sandbox.clockSpeed;
}

int GameEngine::getDaysLeft() const {
    if (rules.finalDay <= 0) return -1;
    return std::max(0, rules.finalDay - currentDay + 1);
}

SeasonType GameEngine::seasonForDay(int day) const {
    if (sandbox.seasonOverride >= 0 && sandbox.seasonOverride <= 3) return static_cast<SeasonType>(sandbox.seasonOverride);
    if (rules.hasMutator(MUT_ETERNAL_WINTER)) return SeasonType::WINTER;
    return Balance::getSeasonForDay(day);
}

SeasonType GameEngine::seasonAtGameSeconds(float seconds) const {
    if (sandbox.seasonOverride >= 0 && sandbox.seasonOverride <= 3) return static_cast<SeasonType>(sandbox.seasonOverride);
    if (rules.hasMutator(MUT_ETERNAL_WINTER)) return SeasonType::WINTER;
    return Balance::getSeasonAtGameSeconds(seconds);
}

std::string GameEngine::welcomeMessage() const {
    if (rules.sandbox) {
        return "ПЯСЪЧНИК: БЕЗКРАЙНИ РЕСУРСИ, БЕЗ ПОБЕДА. ПАНЕЛ: [F2]";
    }
    int grace = getGraceDays();
    if (grace <= 0) {
        return "ДОБРЕ ДОШЛИ! БЕЗ ГРАТИСЕН ПЕРИОД: ГРАДЪТ ИСКА " + std::to_string(rules.startDemandMW) + " MW ОЩЕ ОТ ДЕН 1!";
    }
    if (grace == 1) {
        return "ДОБРЕ ДОШЛИ! ГРАТИСЕН ПЕРИОД: ПЪРВИЯТ ДЕН ГРАДЪТ ИСКА 0 ЕНЕРГИЯ ЗА РАЗВИТИЕ!";
    }
    return "ДОБРЕ ДОШЛИ! ГРАТИСЕН ПЕРИОД: ПЪРВИТЕ " + std::to_string(grace) + " ДЕНА ГРАДЪТ ИСКА 0 ЕНЕРГИЯ ЗА РАЗВИТИЕ!";
}

std::string GameEngine::graceDayEndMessage(int endedDay) const {
    int grace = getGraceDays();
    std::string day = std::to_string(endedDay);
    if (endedDay < grace) {
        int left = grace - endedDay;
        return "ДЕН " + day + " ПРИКЛЮЧИ [ГРАТИСЕН ПЕРИОД]: ГРАДЪТ ИСКАШЕ 0 MW. ОЩЕ " + std::to_string(left) +
               (left == 1 ? " ДЕН" : " ДНИ") + " ЗА РАЗВИТИЕ!";
    }
    return "ДЕН " + day + " ПРИКЛЮЧИ: КРАЙ НА ГРАТИСНИЯ ПЕРИОД! ОТ ДЕН " + std::to_string(endedDay + 1) +
           " ГРАДЪТ ИЗИСКВА ЕНЕРГИЯ!";
}

void GameEngine::applyMatchStartRules() {
    // F-35 / F-33: land prices after perks (research is empty at the start of a match)
    for (auto& plot : landPlots) {
        plot.costGold = scaleCost(baseLandPlotCost(plot), getPlayerPerks(plot.playerOwner).landCostMult);
    }

    // F-24 Head Start: both players begin with a stockpile
    if (rules.hasMutator(MUT_HEAD_START)) {
        for (PlayerEconomy* e : { &p1, &p2 }) {
            e->wood += HEAD_START_RESOURCES;
            e->iron += HEAD_START_RESOURCES;
            e->copper += HEAD_START_RESOURCES;
            e->coal += HEAD_START_RESOURCES;
            e->silicon += HEAD_START_RESOURCES;
            e->silver += HEAD_START_RESOURCES;
            e->gold += HEAD_START_GOLD;
        }
    }

    // F-21 sandbox: infinite resources from the first frame
    applyFrameRules();
}

void GameEngine::applyWeatherRules() {
    // F-24 Mirror Weather: the East sector gets exactly the West sector's weather
    if (rules.hasMutator(MUT_MIRROR_WEATHER)) {
        p2Weather = p1Weather;
    }
    // F-21 sandbox: a weather chosen in the control panel stays until it is changed
    if (sandbox.weatherLocked[0]) p1Weather = sandbox.lockedWeather[0];
    if (sandbox.weatherLocked[1]) p2Weather = sandbox.lockedWeather[1];
}

void GameEngine::refreshPerkDependentState(int player) {
    // Unbought plots follow the current land price multiplier
    float landMult = getPlayerPerks(player).landCostMult;
    for (auto& plot : landPlots) {
        if (plot.playerOwner == player && !plot.isPurchased) {
            plot.costGold = scaleCost(baseLandPlotCost(plot), landMult);
        }
    }
    // Existing batteries grow with capacity research (stored energy is kept)
    float cap = getBatteryCapacityFor(player);
    for (auto& b : buildings) {
        if (b.playerOwner == player && b.type == BuildingType::BATTERY) {
            b.maxCapacity = cap;
            b.energyStored = std::min(b.energyStored, cap);
        }
    }
}

// -----------------------------------------------------------------------------
// Perks
// -----------------------------------------------------------------------------
PlayerPerks GameEngine::getPlayerPerks(int player) const {
    const PlayerEconomy& econ = (player == 1) ? p1 : p2;
    return MatchInfo::computePlayerPerks(rules, player, econ.tech);
}

int GameEngine::scaledIncome(int player, int payout) const {
    float mult = getPlayerPerks(player).incomeMult;
    if (mult == 1.0f) return payout;
    return static_cast<int>(std::lround(static_cast<float>(payout) * mult));
}

float GameEngine::getMiningYieldScale(int player) const {
    return getPlayerPerks(player).miningYieldMult;
}

BuildingCost GameEngine::getBuildingCostFor(int player, BuildingType type) const {
    BuildingCost c = getBuildingCost(type);
    int idx = static_cast<int>(type);
    if (idx <= 0 || idx >= 8) return c;
    float mult = getPlayerPerks(player).buildCostMult[idx];
    if (mult == 1.0f) return c;
    c.woodCost = scaleCost(c.woodCost, mult);
    c.ironCost = scaleCost(c.ironCost, mult);
    c.copperCost = scaleCost(c.copperCost, mult);
    c.coalCost = scaleCost(c.coalCost, mult);
    c.siliconCost = scaleCost(c.siliconCost, mult);
    c.silverCost = scaleCost(c.silverCost, mult);
    c.oreCost = c.ironCost + c.copperCost + c.coalCost + c.siliconCost + c.silverCost;
    return c;
}

int GameEngine::getMineYield(int player, ResourceType type) const {
    int base = 0;
    switch (type) {
        case ResourceType::WOOD:    base = Balance::WOOD_BASE_YIELD; break;
        case ResourceType::IRON:    base = Balance::IRON_BASE_YIELD; break;
        case ResourceType::COPPER:  base = Balance::COPPER_BASE_YIELD; break;
        case ResourceType::COAL:    base = Balance::COAL_BASE_YIELD; break;
        case ResourceType::SILICON: base = Balance::SILICON_BASE_YIELD; break;
        case ResourceType::SILVER:  base = Balance::SILVER_BASE_YIELD; break;
        case ResourceType::GOLD:    base = Balance::GOLD_BASE_YIELD; break;
        default: return 0;
    }
    // Same formula as mineResource()
    float mult = Balance::getMineYieldMultiplier(getMineLevel(player, type)) * getMiningYieldScale(player);
    return static_cast<int>(std::round(base * mult));
}

float GameEngine::getMiningCooldown(int player) const {
    return Balance::MINE_COOLDOWN_SEC * getPlayerPerks(player).miningCooldownMult;
}

float GameEngine::getBatteryCapacityFor(int player) const {
    return static_cast<float>(Balance::BATTERY.batteryCapacityMWh) * getPlayerPerks(player).batteryCapacityMult;
}

float GameEngine::getLampDrawFor(int player) const {
    return LAMP_POWER_MW * getPlayerPerks(player).lampDrawMult;
}

float GameEngine::getLampRadiusFor(int player) const {
    return Balance::STREET_LAMP.lightRadius * getPlayerPerks(player).lampRadiusMult;
}
