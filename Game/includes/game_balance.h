#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include "game_weather.h"
#include "game_time.h"

// =============================================================================
// ENERGY CRISIS - CENTRAL GAME BALANCE & CONFIGURATION
// All game numbers, formulas, costs, yields, multipliers and timers
// =============================================================================

namespace Balance {

// -----------------------------------------------------------------------------
// 2. Building Statistics & Costs
// -----------------------------------------------------------------------------
struct BuildingDef {
    int woodCost;
    int ironCost;
    int copperCost;
    int coalCost;
    int siliconCost;
    int silverCost;
    int basePowerMW;       // Positive for generator, 0 for battery/lamp
    int batteryCapacityMWh;// Only for Battery
    float lightRadius;     // Only for Lamp
    int lampConsumptionMW; // Power consumption for lamp (10 MW)
    const char* nameBg;
    const char* nameEn;
};

// Solar Panel: Clean solar energy, high peak during midday
constexpr BuildingDef SOLAR_PANEL = {
    6, 4, 6, 0, 8, 0,      // Wood, Iron, Copper, Coal, Silicon, Silver
    60, 0, 0.0f, 0,        // 60 MW base
    "Слънчев панел", "Solar Panel"
};

// Wind Turbine: Continuous clean wind energy, boosted in wind & storms
constexpr BuildingDef WIND_TURBINE = {
    8, 14, 8, 6, 0, 0,     // Wood, Iron, Copper, Coal, Silicon, Silver
    85, 0, 0.0f, 0,        // 85 MW base
    "Вятърна мелница", "Wind Turbine"
};

// Hydro Plant: High-output river hydro generator, boosted during rain
constexpr BuildingDef HYDRO_PLANT = {
    15, 20, 12, 0, 6, 0,   // Wood, Iron, Copper, Coal, Silicon, Silver
    110, 0, 0.0f, 0,       // 110 MW base (matches user balance adjustment)
    "ВЕЦ / Хидро", "Hydro Plant"
};

// Battery Storage: Stores excess clean power during surplus, discharges at night
constexpr BuildingDef BATTERY = {
    4, 8, 10, 4, 0, 4,     // Wood, Iron, Copper, Coal, Silicon, Silver
    0, 200, 0.0f, 0,       // 200 MWh capacity
    "Батерия", "Battery Storage"
};

// Street / Work Lamp: Illuminates dark sector to enable night construction
constexpr BuildingDef STREET_LAMP = {
    4, 5, 3, 0, 0, 0,      // Wood, Iron, Copper, Coal, Silicon, Silver
    0, 0, 150.0f, 10,      // 150px light radius, consumes 10 MW
    "Осветителна лампа", "Work Lamp"
};

constexpr float DEMOLISH_REFUND_FRACTION = 0.50f; // 50% resource refund when demolishing

// Battery storage: one unit for charge and discharge (MW x game-hours = MWh)
// 1 game-hour = SECONDS_PER_DAY / 24 = 3.75 s. A full 200 MWh battery gives 40 MW for 5 game-hours.
constexpr float BATTERY_MAX_POWER_MW = 40.0f;     // Max charge / discharge power per battery

// -----------------------------------------------------------------------------
// 3. Resource Mining & Upgrades with Gold
// -----------------------------------------------------------------------------
constexpr int WOOD_BASE_YIELD    = 12;
constexpr int IRON_BASE_YIELD    = 8;
constexpr int COPPER_BASE_YIELD  = 6;
constexpr int COAL_BASE_YIELD    = 6;
constexpr int SILICON_BASE_YIELD = 6;
constexpr int SILVER_BASE_YIELD  = 4;
constexpr int GOLD_BASE_YIELD    = 3;

constexpr int MINE_MAX_LEVEL = 6;
constexpr int MINE_UPGRADE_COST_BASE = 30;

// Upgrade multiplier formula: +75% yield per upgrade level
inline float getMineYieldMultiplier(int level) {
    return 1.0f + std::max(0, level - 1) * 0.75f;
}

// Upgrade cost formula: Exponential progression (30 -> 300 -> 500 -> 800 -> 1500)
// Prevents rushing max upgrades in early game and makes late-game economy deeply rewarding
inline int getMineUpgradeCost(int level, bool isGoldMine) {
    if (level >= MINE_MAX_LEVEL) return 0;
    switch (level) {
        case 1: return isGoldMine ? 50   : 30;
        case 2: return isGoldMine ? 400  : 300;
        case 3: return isGoldMine ? 700  : 500;
        case 4: return isGoldMine ? 1100 : 800;
        case 5: return isGoldMine ? 2000 : 1500;
        default: return isGoldMine ? 2500 : 2000;
    }
}

// -----------------------------------------------------------------------------
// 4. Land Grid & Purchases
// -----------------------------------------------------------------------------
constexpr int PLOTS_PER_PLAYER = 12; // 3 columns x 4 rows
constexpr int SLOTS_PER_PLOT   = 9;  // 3x3 grid per land plot = 108 slots per player
constexpr int TOTAL_PLOTS      = 24; // 12 West (P1) + 12 East (P2)

constexpr int LAND_BASE_COST_GOLD = 150;
constexpr int LAND_TIER_COST_GROWTH = 45;

// row/col are counted from the player's own starting corner (col 0 = far side, col 2 = river side),
// so both players pay the same price for mirrored plots.
inline int getLandPlotCost(int row, int col) {
    return LAND_BASE_COST_GOLD + (row * 3 + col) * LAND_TIER_COST_GROWTH;
}

// Land plot column (0..2, West->East screen order) that borders the city river for each player.
// Hydro plants may only be built on these river-bank plots.
constexpr int P1_RIVER_BANK_PLOT_COL = 2;
constexpr int P2_RIVER_BANK_PLOT_COL = 0;

// -----------------------------------------------------------------------------
// 5. City Demand, Revenue & Dynamic Tug-of-War Influence
// -----------------------------------------------------------------------------
constexpr int STARTING_CITY_DEMAND_MW = 30; // Starts at 30 MW (15 MW quota each)
constexpr int DAILY_DEMAND_INCREASE_MW = 15;

constexpr int CITY_CONTRACT_POOL_BASE = 25;
constexpr float CITY_CONTRACT_POOL_PER_MW = 0.25f;

// Formula: Contract money pool based on total grid generation
inline int calculateContractPool(float totalGridMW) {
    return CITY_CONTRACT_POOL_BASE + static_cast<int>(totalGridMW * CITY_CONTRACT_POOL_PER_MW);
}

// Formula: Player share payout in money ($)
inline int calculatePlayerPayout(int contractPool, float playerShare) {
    return static_cast<int>(std::round(contractPool * playerShare));
}

// Formula: Gold dividend earned by supplying sustained clean power.
// CAPPED BY CITY DEMAND: If city asks for 50 MW and player supplies 1200 MW,
// eligible generation is capped at 50 MW so players cannot exploit infinite gold!
constexpr int GOLD_DIVIDEND_MIN_MW = 15;
inline int calculateGoldDividend(int playerMW, int cityDemand) {
    int cap = (cityDemand > 0) ? cityDemand : STARTING_CITY_DEMAND_MW;
    int eligibleMW = std::min(playerMW, cap);
    if (eligibleMW < GOLD_DIVIDEND_MIN_MW) return 0;
    return std::max(1, static_cast<int>(eligibleMW * 0.03f));
}

// Maximum percentage of city territory that can be won or lost in a single day (10% - 15%)
// Prevents sudden total takeover in a single day (e.g. 0 MW vs 1200 MW gives at most 15% shift)
constexpr float MAX_DAILY_CITY_SHIFT = 0.15f; // 15% maximum per day
constexpr float MIN_DAILY_CITY_SHIFT = 0.10f; // 10% minimum shift when a player powers the city

inline float calculateDailyCityShift(int p1PowerMW, int p2PowerMW, int cityDemand) {
    if (cityDemand <= 0) return 0.0f;

    bool p1Succeeded = (p1PowerMW >= cityDemand);
    bool p2Succeeded = (p2PowerMW >= cityDemand);

    // Rule: Territory shifts ONLY if a player manages to generate the energy for the city!
    // Otherwise ("иначе нищо да не се случва"), no territory changes hands.
    if (p1Succeeded && !p2Succeeded) {
        // Player 1 powered the city, Player 2 failed -> P2 gives 10% - 15% territory to P1
        float failureRatio = 1.0f - std::clamp(static_cast<float>(p2PowerMW) / static_cast<float>(cityDemand), 0.0f, 1.0f);
        return MIN_DAILY_CITY_SHIFT + (MAX_DAILY_CITY_SHIFT - MIN_DAILY_CITY_SHIFT) * failureRatio;
    } else if (p2Succeeded && !p1Succeeded) {
        // Player 2 powered the city, Player 1 failed -> P1 gives 10% - 15% territory to P2
        float failureRatio = 1.0f - std::clamp(static_cast<float>(p1PowerMW) / static_cast<float>(cityDemand), 0.0f, 1.0f);
        return -(MIN_DAILY_CITY_SHIFT + (MAX_DAILY_CITY_SHIFT - MIN_DAILY_CITY_SHIFT) * failureRatio);
    } else if (p1Succeeded && p2Succeeded) {
        // Both players successfully produced enough energy for the city!
        // "ако играч 1 и играч 2 могат да дадат нужната енергия на града нито един да не губи територия"
        return 0.0f;
    }

    // Neither player produced enough energy for the city!
    // "иначе нищо да не се случва" -> No territory changes hands!
    return 0.0f;
}

// Facility Repair Costs (when broken by lightning strikes)
constexpr int REPAIR_WOOD_COST = 5;
constexpr int REPAIR_IRON_COST = 5;

// Victory Conditions (checked once per day end):
// - a player wins as soon as their city share is >= VICTORY_SHARE (85%)
// - when day FINAL_DAY (20) ends, the larger share wins; |share - 50%| < DRAW_SHARE_TOLERANCE is a draw
// Winner codes in CityConquestState::winner: 0 = none, 1 = P1, 2 = P2, 3 = draw
constexpr float VICTORY_SHARE = 0.85f;
constexpr int FINAL_DAY = 20;
constexpr float DRAW_SHARE_TOLERANCE = 0.005f;

// Legacy names kept for compatibility (P1 share thresholds)
constexpr float VICTORY_INFLUENCE_P1 = VICTORY_SHARE;
constexpr float VICTORY_INFLUENCE_P2 = 1.0f - VICTORY_SHARE;

} // namespace Balance
