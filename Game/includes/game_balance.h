#pragma once
#include <algorithm>
#include <cmath>
#include <string>

// =============================================================================
// ENERGY CRISIS - CENTRAL GAME BALANCE & CONFIGURATION
// All game numbers, formulas, costs, yields, multipliers and timers
// =============================================================================

namespace Balance {

// -----------------------------------------------------------------------------
// 1. Time, Day & Night Cycle
// -----------------------------------------------------------------------------
constexpr float SECONDS_PER_DAY = 90.0f;       // Length of 1 full 24h day in seconds
constexpr float DAY_START_HOUR = 6.0f;         // 06:00 - Daylight begins (Sun shines)
constexpr float DAY_END_HOUR = 18.0f;          // 18:00 - Night begins (Lamps required to build)
constexpr float MINE_SPEEDUP_MULT = 6.0f;      // Time advances 6x faster when actively gathering resources
constexpr float MINE_COOLDOWN_SEC = 1.0f;      // Cooldown between resource gathering actions (1.0s)

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

constexpr int MINE_MAX_LEVEL = 5;
constexpr int MINE_UPGRADE_COST_BASE = 15;      // 15 G per level for normal mines
constexpr int MINE_UPGRADE_COST_GOLD_BASE = 20; // 20 G per level for Gold mine

// Upgrade multiplier formula: +75% yield per upgrade level
inline float getMineYieldMultiplier(int level) {
    return 1.0f + std::max(0, level - 1) * 0.75f;
}

// Upgrade cost formula
inline int getMineUpgradeCost(int level, bool isGoldMine) {
    if (level >= MINE_MAX_LEVEL) return 0;
    return level * (isGoldMine ? MINE_UPGRADE_COST_GOLD_BASE : MINE_UPGRADE_COST_BASE);
}

// -----------------------------------------------------------------------------
// 4. Land Grid & Purchases
// -----------------------------------------------------------------------------
constexpr int PLOTS_PER_PLAYER = 12; // 3 columns x 4 rows
constexpr int SLOTS_PER_PLOT   = 9;  // 3x3 grid per land plot = 108 slots per player
constexpr int TOTAL_PLOTS      = 24; // 12 West (P1) + 12 East (P2)

constexpr int LAND_BASE_COST_GOLD = 150;
constexpr int LAND_TIER_COST_GROWTH = 45;

inline int getLandPlotCost(int row, int col) {
    return LAND_BASE_COST_GOLD + (row * 3 + col) * LAND_TIER_COST_GROWTH;
}

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

// Formula: Gold dividend earned by supplying sustained clean power
constexpr int GOLD_DIVIDEND_MIN_MW = 30;
inline int calculateGoldDividend(int playerMW) {
    if (playerMW < GOLD_DIVIDEND_MIN_MW) return 0;
    return std::max(1, static_cast<int>(playerMW * 0.03f));
}

// Formula: Gradual dynamic tug-of-war city influence drift step
constexpr float TUG_OF_WAR_DRIFT_FACTOR = 0.015f;
constexpr float TUG_OF_WAR_MAX_STEP = 0.03f;
inline float calculateInfluenceDrift(int p1PowerMW, int p2PowerMW, int cityDemand) {
    float powerDiff = static_cast<float>(p1PowerMW - p2PowerMW);
    float denom = std::max(50.0f, static_cast<float>(cityDemand));
    float step = (powerDiff / denom) * TUG_OF_WAR_DRIFT_FACTOR;
    return std::clamp(step, -TUG_OF_WAR_MAX_STEP, TUG_OF_WAR_MAX_STEP);
}

// Victory Condition: 100% (1.0) influence threshold
constexpr float VICTORY_INFLUENCE_P1 = 0.999f;
constexpr float VICTORY_INFLUENCE_P2 = 0.001f;

} // namespace Balance
