#ifndef GAME_POWER_H
#define GAME_POWER_H

// =============================================================================
// Team b-power: map layouts, terrain plots, nuclear reactor, hazards and
// mega-projects (engine side). Implementation: Game/scr/game_map_layout.cpp,
// game_terrain.cpp, game_nuclear.cpp, game_hazards.cpp, game_mega.cpp.
// Engine-only: uses nothing from SFML except sf::Vector2f / sf::FloatRect.
// =============================================================================

#include <random>
#include <string>
#include <vector>
#include <SFML/Graphics.hpp>
#include "game_balance.h"

// -----------------------------------------------------------------------------
// Terrain of a land plot (F-15). Stored as int in LandPlot::terrain / PlacedBuilding::terrain.
// -----------------------------------------------------------------------------
enum class TerrainType {
    PLAIN = 0, // Равнина: no modifier
    RIVER,     // Речен бряг: hydro plants and the pumped-hydro dam may be built here
    VENT,      // Гейзер: geothermal plants may be built here
    HILL,      // Хълм: wind x1.3
    MEADOW     // Ливада: solar x1.15
};

const char* getTerrainNameBg(TerrainType t);   // e.g. "ХЪЛМ"
const char* getTerrainEffectBg(TerrainType t); // e.g. "Вятър +30%"

// -----------------------------------------------------------------------------
// Map layout presets (F-39). Every layout is mirror-symmetric: East is West mirrored at x = 800.
// -----------------------------------------------------------------------------
enum class MapPreset {
    CLASSIC = 0,  // Класическа: 3x4 plots, river column next to the city (the original map)
    RIVER_DELTA,  // Речна делта: the river fans out along the bottom row, many meadows
    HIGHLANDS,    // Планини: short river bank, many hills and vents, one impassable peak
    LONG_VALLEY,  // Дълга долина: 2x5 long plots along the river
    CROWDED,      // Тясно поле: 3x5 small, cheap plots
    GENERATED,    // Генерирана: built from the match seed (West generated, East mirrored)
    COUNT
};

const char* getMapPresetNameBg(MapPreset p);
const char* getMapPresetDescBg(MapPreset p);

// One map layout: a regular grid of plot cells per side, some cells may be holes (no plot).
// Cell coordinates are "West orientation": col 0 is the far edge, col plotCols-1 borders the city.
// Grid slots (3x3 per plot) and plot rectangles use SCREEN columns (left to right) for both players.
struct MapLayout {
    MapPreset preset = MapPreset::CLASSIC;
    unsigned seed = 0;
    int plotCols = 3;
    int plotRows = 4;
    float plotW = 105.0f;
    float plotH = 95.0f;
    float gapX = 12.0f;
    float gapY = 10.0f;
    float westStartX = 258.0f;
    float startY = 105.0f;
    int startCol = 0;          // West orientation
    int startRow = 0;
    int landCostGrowth = Balance::LAND_TIER_COST_GROWTH;
    std::vector<int> cells;    // row-major, West orientation: -1 = hole, otherwise TerrainType

    int gridCols() const { return plotCols * 3; }
    int gridRows() const { return plotRows * 3; }
    float blockWidth() const { return plotCols * plotW + (plotCols - 1) * gapX; }
    float eastStartX() const { return 1600.0f - westStartX - blockWidth(); }
    // Screen column <-> West-orientation column for a player
    int westCol(int player, int screenCol) const { return (player == 1) ? screenCol : (plotCols - 1 - screenCol); }
    bool inRange(int westColumn, int row) const {
        return westColumn >= 0 && westColumn < plotCols && row >= 0 && row < plotRows;
    }
    int cellValue(int westColumn, int row) const {
        size_t idx = static_cast<size_t>(row * plotCols + westColumn);
        return (inRange(westColumn, row) && idx < cells.size()) ? cells[idx] : -1;
    }
    bool hasPlot(int westColumn, int row) const { return cellValue(westColumn, row) >= 0; }
    TerrainType terrainAt(int westColumn, int row) const {
        int v = cellValue(westColumn, row);
        return (v < 0) ? TerrainType::PLAIN : static_cast<TerrainType>(v);
    }
    sf::FloatRect plotRect(int player, int screenCol, int row) const;
    sf::Vector2f slotCenter(int player, int col, int row) const; // col/row in slots, clamped to the grid
    int plotCount() const;
    int countTerrain(TerrainType t) const;
};

// Builds a preset; `seed` rolls the terrain (mirrored) and drives GENERATED geometry.
MapLayout buildMapLayout(MapPreset preset, unsigned seed);

// -----------------------------------------------------------------------------
// Hazards (F-34) and power events for the UI (HazardFx queue, drained every frame)
// -----------------------------------------------------------------------------
enum class HazardKind {
    NONE = 0,
    HAIL,      // Градушка: breaks solar panels (and some turbines) on hail days
    FLOOD,     // Наводнение: after rainy streaks, damages buildings on river plots
    WILDFIRE,  // Горски пожар: after dry streaks, burns one plot (may spread)
    QUAKE      // Земетресение: rare, mirrored epicentre on both sides, SCRAMs reactors
};

const char* getHazardNameBg(HazardKind k);

enum class PowerFxKind {
    HAZARD_WARNING,    // Start of a day with a scheduled hazard (gives time to stock repairs)
    HAZARD_HIT,        // A hazard struck: `hits` lists every damaged building
    REACTOR_SCRAM,     // Lightning / quake shut a reactor down
    REACTOR_NO_FUEL,   // Day end without 12 silver: SCRAM until refuelled
    REACTOR_RESTART,   // Reactor restarts its ramp (after cooldown or refuelling)
    REACTOR_FULL,      // Reactor reached 100 % output
    MEGA_STARTED,      // Construction of a mega-project began
    MEGA_SETBACK,      // Lightning / quake set a construction site back
    MEGA_COMPLETE,     // Mega-project finished
    MEGA_UNLOCKED      // Mega-projects became available (both players)
};

struct PowerFx {
    PowerFxKind kind = PowerFxKind::HAZARD_HIT;
    HazardKind hazard = HazardKind::NONE;
    int player = 0;                 // 0 = both players
    int buildingType = 0;           // BuildingType as int (0 = none)
    sf::Vector2f pos;               // Centre of the effect
    float radius = 0.0f;            // Area of the effect (px)
    std::vector<sf::Vector2f> hits; // Damaged building positions
    std::string title;              // Short Bulgarian headline
    std::string detail;             // One or two Bulgarian lines
};

struct HazardPlan {
    HazardKind kind = HazardKind::NONE;
    int player = 0;            // 0 = both (quake)
    float atDaySeconds = 0.0f; // Fires when CityConquestState::dailySeconds reaches this
    int westCol = -1;          // Epicentre cell for quakes (mirrored for P2)
    int row = -1;
    bool fired = false;
};

// All state of the team b-power systems that lives inside GameEngine (copied by init's reset)
struct PowerWorldState {
    std::mt19937 rng;                 // Own stream: hazards never shift the weather sequence
    int rainStreak[3] = { 0, 0, 0 };  // Consecutive wet days per player (index 1, 2)
    int dryStreak[3] = { 0, 0, 0 };   // Consecutive dry days per player
    std::vector<HazardPlan> plans;    // Hazards scheduled for the current day
    std::vector<PowerFx> fx;          // Events for the UI (bounded)
    bool megaUnlockAnnounced = false;
};

// -----------------------------------------------------------------------------
// Numbers for the new buildings and systems (one place, like game_balance.h)
// Recipe order of Balance::BuildingDef: Wood, Iron, Copper, Coal, Silicon, Silver.
// -----------------------------------------------------------------------------
namespace PowerBalance {

// Terrain modifiers and the shared river (F-15)
constexpr float HILL_WIND_MULT = 1.30f;
constexpr float MEADOW_SOLAR_MULT = 1.15f;
// Hydro output x clamp(RIVER_FLOW_BASE - RIVER_FLOW_PER_PLANT * H, MIN, MAX), H = hydro plants of BOTH players
constexpr float RIVER_FLOW_BASE = 1.15f;
constexpr float RIVER_FLOW_PER_PLANT = 0.035f;
constexpr float RIVER_FLOW_MIN = 0.55f;
constexpr float RIVER_FLOW_MAX = 1.15f;
constexpr int VENTS_PER_SIDE = 2;
constexpr int GEOTHERMAL_PER_VENT_PLOT = 2;

// Nuclear reactor (F-32)
constexpr Balance::BuildingDef NUCLEAR = {
    40, 90, 60, 0, 40, 50,
    400, 0, 0.0f, 0,
    "АЕЦ (ядрен реактор)", "Nuclear Plant"
};
constexpr int NUCLEAR_UNLOCK_PLOTS = 6;                                   // Owned plots needed
constexpr int NUCLEAR_FUEL_SILVER_PER_DAY = 12;                           // Paid at every day end
constexpr float NUCLEAR_RAMP_SECONDS = Balance::SECONDS_PER_DAY;          // 0 -> 100 % in one day
constexpr float NUCLEAR_SCRAM_COOLDOWN_SEC = Balance::SECONDS_PER_DAY * 0.5f; // Shutdown after SCRAM

// Geothermal plant (F-15): steady, weather-proof, only on vent plots
constexpr Balance::BuildingDef GEOTHERMAL = {
    12, 22, 14, 0, 8, 6,
    70, 0, 0.0f, 0,
    "Геотермална ЦЕЦ", "Geothermal Plant"
};

// Mega-projects (HX-10): one per player, from MEGA_UNLOCK_DAY, a whole plot, multi-day construction
constexpr int MEGA_UNLOCK_DAY = 8;
constexpr Balance::BuildingDef MEGA_FUSION = {
    0, 160, 120, 0, 140, 90,
    500, 0, 0.0f, 0,
    "Термоядрен реактор", "Fusion Reactor"
};
constexpr float MEGA_FUSION_BUILD_DAYS = 3.0f;
constexpr Balance::BuildingDef MEGA_SPACE_SOLAR = {
    0, 110, 90, 0, 200, 70,
    350, 0, 0.0f, 0,
    "Космическа СЕЦ", "Space Solar Array"
};
constexpr float MEGA_SPACE_SOLAR_BUILD_DAYS = 2.0f;
constexpr float SPACE_SOLAR_STORM_MULT = 0.6f; // Storm clouds weaken the microwave beam
constexpr Balance::BuildingDef MEGA_PUMPED_HYDRO = {
    120, 150, 90, 40, 30, 30,
    80, 3000, 0.0f, 0,
    "ПАВЕЦ (язовир)", "Pumped-Hydro Dam"
};
constexpr float MEGA_PUMPED_HYDRO_BUILD_DAYS = 2.0f;
constexpr float PUMPED_HYDRO_MAX_POWER_MW = 400.0f; // Charge / discharge power of the dam
constexpr float MEGA_LIGHTNING_SETBACK = 0.15f;    // Fraction of the build time lost to a strike

// Hazards (F-34)
constexpr int HAZARD_MAX_HITS = 4;              // Buildings one hazard may damage at most
constexpr float HAIL_BREAK_CHANCE = 0.30f;      // Per solar panel and burst (turbines: half)
constexpr int FLOOD_RAIN_STREAK = 2;            // Wet days before a 3rd wet day may flood
constexpr float FLOOD_CHANCE = 0.65f;
constexpr float FLOOD_BREAK_CHANCE = 0.45f;     // Per building on a river plot (hydro: half)
constexpr int WILDFIRE_DRY_STREAK = 3;          // Dry days before fire risk
constexpr float WILDFIRE_CHANCE = 0.35f;        // Summer: x1.6
constexpr float WILDFIRE_BREAK_CHANCE = 0.60f;
constexpr float WILDFIRE_SPREAD_CHANCE = 0.35f;
constexpr float QUAKE_DAILY_CHANCE = 0.06f;
constexpr float QUAKE_BREAK_CHANCE = 0.40f;     // Epicentre plot (neighbours: half, geothermal: +0.2)
constexpr size_t POWER_FX_QUEUE_MAX = 48;

} // namespace PowerBalance

#endif // GAME_POWER_H
