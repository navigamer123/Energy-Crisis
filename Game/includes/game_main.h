#ifndef GAME_MAIN_H
#define GAME_MAIN_H

#include <string>
#include <vector>
#include <SFML/Graphics.hpp>
#include "game_weather.h"
#include "game_expedition.h"
#include "game_random.h"
#include "game_balance.h"
#include "game_power.h" // Team b-power: map layouts, terrain, nuclear, hazards, mega-projects

// -----------------------------------------------------------------------------
// PlayerData (integrated from weatherF branch)
// -----------------------------------------------------------------------------
struct PlayerData {
    int money = 0;
    int iron = 0;
    int coal = 0;
    int gold = 0;
    int copper = 0;
    int silver = 0;
    int silicon = 0;
    int wood = 0;
    int sticks = 0;
    std::string weather = "clear";
    std::string wind_speed = "0";
};

// -----------------------------------------------------------------------------
// Resource Types
// -----------------------------------------------------------------------------
enum class ResourceType {
    NONE = 0,
    WOOD,     // Дървесина (Гора)
    IRON,     // Желязо (Желязна мина)
    COPPER,   // Мед (Медна мина)
    COAL,     // Въглища (Въглищен пласт)
    SILICON,  // Силиций (Силициева кариера)
    SILVER,   // Сребро (Сребърна жила)
    GOLD,     // Злато (Златна жила)
    MONEY,    // Пари (Градска валута от ток)
    ENERGY,   // Електроенергия (MW)
    ORE       // Legacy alias for tests
};

// -----------------------------------------------------------------------------
// Building Types
// -----------------------------------------------------------------------------
enum class BuildingType {
    NONE = 0,
    SOLAR_PANEL,
    WIND_TURBINE,
    HYDRO_PLANT,
    BATTERY,
    LAMP,
    DEMOLISH,
    // Team b-power: advanced power (DEMOLISH stays 6 for the 1..6 hotkeys). See game_power.h.
    NUCLEAR,            // 7  АЕЦ: 400 MW after a one-day ramp, silver fuel, SCRAM, whole plot
    GEOTHERMAL,         // 8  Геотермална ЦЕЦ: steady output, only on vent plots
    MEGA_FUSION,        // 9  Mega-project: термоядрен реактор
    MEGA_SPACE_SOLAR,   // 10 Mega-project: космическа слънчева централа
    MEGA_PUMPED_HYDRO   // 11 Mega-project: ПАВЕЦ (pumped-hydro storage dam, river plot)
};

struct BuildingCost {
    BuildingType type;
    std::string nameBg;
    std::string nameEn;
    int woodCost = 0;
    int ironCost = 0;
    int copperCost = 0;
    int coalCost = 0;
    int siliconCost = 0;
    int silverCost = 0;
    int oreCost = 0; // Legacy backwards compatibility
    int basePowerMW = 0;
};

struct PlacedBuilding {
    BuildingType type;
    sf::Vector2f position;
    int playerOwner; // 1 or 2
    float currentOutputMW;
    float animTimer;
    float energyStored = 0.0f;  // Current stored charge in MWh
    float maxCapacity = 200.0f; // Max capacity in MWh
    float lightRadius = 150.0f; // For Lamp light cone
    bool isBroken = false;      // Damaged/broken by lightning strike
    // Team b-power: terrain, nuclear, hazards, mega-projects (game_power.h)
    int terrain = 0;               // TerrainType of the plot it stands on (set when placed)
    int damageKind = 0;            // HazardKind that broke it (valid while isBroken)
    float rampProgress = 0.0f;     // Nuclear: 0..1 output ramp after (re)start
    float scramTimer = 0.0f;       // Nuclear: game-seconds left in a SCRAM shutdown
    bool needsFuel = false;        // Nuclear: shut down until 12 silver are available
    float constructionLeft = 0.0f; // Mega-projects: game-seconds of construction left (0 = done)
    float constructionTotal = 0.0f;// Mega-projects: full construction time (game-seconds)
};

struct LandPlot {
    int id;
    int playerOwner; // 1 = West, 2 = East
    sf::FloatRect bounds;
    bool isPurchased;
    int costGold;
    // Team b-power: map layout cell and terrain (game_power.h)
    int terrain = 0;    // TerrainType
    int screenCol = 0;  // Plot column on screen (left to right)
    int row = 0;        // Plot row (top to bottom)
};

struct PlayerEconomy {
    int money = 0;              // City currency earned from power generation
    int gold = 0;               // Mined Gold
    int silver = 0;             // Mined Silver
    int iron = 0;               // Mined Iron
    int coal = 0;               // Mined Coal
    int copper = 0;             // Mined Copper
    int silicon = 0;            // Mined Silicon
    int wood = 0;               // Harvested Wood
    int ore = 0;                // Legacy mineral total
    int energyMW = 0;           // Clean electricity generated (starts at 0)
    int landTier = 1;           // Land tier
    float cityInfluence = 0.50f;// Percentage of city supplied / captured (0.0 to 1.0)
    int selectedBuilding = 0;   // 0 = None, 1 = Solar, 2 = Wind, 3 = Hydro, 4 = Battery, 5 = Lamp, 6 = Demolish
    int lastPlacedBuilding = 1; // Remembers lastly placed building for instant reuse
    int mineLevels[8] = { 1, 1, 1, 1, 1, 1, 1, 1 }; // Upgrade level for each resource mine (1..5)
    PlayerData data;            // Teammate's detailed inventory from weatherF
};

struct CityConquestState {
    int cityEnergyDemand = 0;   // Starts at 0 MW for Day 1-2 Grace Period, then 30 MW from Day 3
    float p1CityShare = 0.50f;  // 0.0 to 1.0 (P1 vs P2 city control tug-of-war)
    float p1DailyDelivered = 0.0f; // Energy delivered to the city so far today (MW x game-seconds)
    float p2DailyDelivered = 0.0f;
    float dailySeconds = 0.0f;     // Game-seconds elapsed in the current day (06:00 -> 06:00)
    bool dayCutOccurred = false;
    std::string lastCutMessage;
    int winner = 0;             // 0 = None, 1 = P1, 2 = P2, 3 = Draw (equal shares after the final day)
};

struct MineResult {
    ResourceType type = ResourceType::NONE;
    int amount = 0;
    int wood = 0;
    int iron = 0;
    int copper = 0;
    int coal = 0;
    int silicon = 0;
    int silver = 0;
    int gold = 0;
    int money = 0;
};

// -----------------------------------------------------------------------------
// Backend Game Engine
// -----------------------------------------------------------------------------
class GameEngine {
public:
    using MineResult = ::MineResult;

private:
    float gameSeconds;
    int currentDay;
    float hour24;
    float revenueTimer;         // Accumulates game-seconds towards the next 1 s city payout

    WeatherType p1Weather;
    WeatherType p2Weather;
    SeasonType currentSeason;
    float timeScale;

    PlayerEconomy p1;
    PlayerEconomy p2;
    CityConquestState city;

    std::vector<PlacedBuilding> buildings;
    std::vector<LandPlot> landPlots;

    void simulateStep(float dt);
    void updateBuildingsEnergy(float dt);
    void payCityRevenue();
    void processDayEnd();
    void rollDailyWeather();
    int findOwnedBuildingInSlot(int player, sf::Vector2f pos) const;

    // ---- Team b-power: map layout, terrain, nuclear, hazards, mega-projects (game_power*.cpp) ----
    MapPreset mapPreset = MapPreset::CLASSIC; // Survives init(): chosen in the menu before a match
    unsigned mapSeedOverride = 0;             // 0 = use the match seed for the map
    MapLayout layout;
    PowerWorldState world;
    void setupPowerWorld(unsigned matchSeed);         // game_map_layout.cpp: layout + land plots + RNG
    void updateAdvancedSystems(float dt);             // game_power_update.cpp: every simulation step
    void advancedDayEnd();                            // before the new day's weather is rolled
    void advancedNewDay();                            // after the new day's weather is rolled
    float advancedOutputMW(const PlacedBuilding& b, WeatherType w) const; // nuclear, geothermal, mega
    float terrainOutputMultiplier(const PlacedBuilding& b) const;         // hill / meadow / river flow
    float storageMaxPowerMW(const PlacedBuilding& b) const;               // battery 40, dam 400
    bool checkAdvancedUnlock(int player, BuildingType type, std::string& reason) const;
    bool checkAdvancedPlacement(int player, BuildingType type, sf::Vector2f pos, std::string& reason) const;
    sf::Vector2f snapAdvanced(int player, BuildingType type, sf::Vector2f pos) const;
    void onAdvancedPlaced(PlacedBuilding& b);
    float advancedRefundFraction(BuildingType type) const;
    BuildingCost getAdvancedBuildingCost(BuildingType type) const;
    int findPlotWideBuildingAt(int player, sf::Vector2f pos) const;
    void pushPowerFx(const PowerFx& fx);
    void scramReactor(PlacedBuilding& b, const std::string& cause);
    bool damageBuilding(PlacedBuilding& b, HazardKind kind);
    void scheduleHazard(HazardKind kind, int player, int westCol, int row);
    void fireHazard(HazardPlan& plan);
    float rollUnit();                                 // 0..1 from the hazard RNG

public:
    GameEngine();
    void init(float screenWidth, float screenHeight);
    void update(float dt);

    void setTimeScale(float scale) { timeScale = (scale > 0.1f ? scale : 1.0f); }
    float getTimeScale() const { return timeScale; }

    // Player Actions
    bool mineResource(int player, ResourceType type, std::string& outMsg);
    bool mineResource(int player, ResourceType type, MineResult& result, std::string& outMsg);
    int getMineLevel(int player, ResourceType type) const;
    int getMineUpgradeCost(int player, ResourceType type) const;
    bool upgradeMine(int player, ResourceType type, std::string& outMsg);

    bool buyLandPlot(int player, int plotId, std::string& outMsg);
    bool buyNextLandTier(int player, std::string& outMsg);
    void restartGame() { init(1600.0f, 900.0f); }

    void cycleBuildingSelection(int player);
    void cycleBuildingSelectionPrev(int player);
    void clearBuildingSelection(int player);
    BuildingType getSelectedBuilding(int player) const;

    bool canPlaceBuilding(int player, BuildingType type, sf::Vector2f pos, std::string& reason) const;
    bool placeBuilding(int player, BuildingType type, sf::Vector2f pos, std::string& outMsg);
    bool removeBuilding(int player, sf::Vector2f pos, std::string& outMsg);
    bool repairBuilding(int player, sf::Vector2f pos, std::string& outMsg);
    bool breakBuildingAt(sf::Vector2f pos);
    bool breakRandomBuilding(int playerOwner, sf::Vector2f& outPos);
    bool hasBrokenBuilding(int player) const;
    bool isAreaIlluminated(int player, sf::Vector2f pos) const;
    // True when pos lies on one of the player's river-bank plots (the only place hydro may be built)
    bool isRiverBankSlot(int player, sf::Vector2f pos) const;

    // Lamp consumption constant (MW)
    static constexpr float LAMP_POWER_MW = 10.0f;

    // Building Data helper
    BuildingCost getBuildingCost(BuildingType type) const;
    sf::Vector2f snapToBuildingGrid(int player, sf::Vector2f pos) const;
    sf::Vector2f getGridSlot(int player, int col, int row) const;
    void getClosestGridIndex(int player, sf::Vector2f pos, int& outCol, int& outRow) const;

    // Getters for UI
    const PlayerEconomy& getPlayerEconomy(int player) const { return (player == 1) ? p1 : p2; }
    PlayerEconomy& getPlayerEconomyMut(int player) { return (player == 1) ? p1 : p2; }
    const CityConquestState& getCityState() const { return city; }
    const std::vector<PlacedBuilding>& getBuildings() const { return buildings; }
    const std::vector<LandPlot>& getLandPlots() const { return landPlots; }
    // Average power (MW) delivered to the city so far today; the day-end result is judged on this value
    float getTodayAverageMW(int player) const {
        if (city.dailySeconds <= 0.0f) return 0.0f;
        return ((player == 1) ? city.p1DailyDelivered : city.p2DailyDelivered) / city.dailySeconds;
    }

    int getCurrentDay() const { return currentDay; }
    float getHour24() const { return hour24; }
    float getDayProgress() const { return hour24 / 24.0f; }
    bool isDaylight() const { return Balance::isDaylightAt(hour24, currentSeason); }
    float getSunriseHour() const { return Balance::getSunriseHour(currentSeason); }
    float getSunsetHour() const { return Balance::getSunsetHour(currentSeason); }
    bool isGracePeriod() const { return currentDay <= Balance::GRACE_PERIOD_DAYS; }

    WeatherType getPlayerWeather(int player) const { return (player == 1) ? p1Weather : p2Weather; }
    SeasonType getSeason() const { return currentSeason; }

    // ---- Team b-power public API (game_power.h) ----
    // Map layout (F-39): pick before init()/restartGame(); seed 0 = derive from the match seed
    void setMapPreset(MapPreset preset, unsigned seed = 0) { mapPreset = preset; mapSeedOverride = seed; }
    MapPreset getMapPreset() const { return mapPreset; }
    const MapLayout& getMapLayout() const { return layout; }
    int getGridCols() const { return layout.gridCols(); }  // slots per row (3 per plot column)
    int getGridRows() const { return layout.gridRows(); }  // slots per column (3 per plot row)
    sf::Vector2f getStartPlotSlot(int player, int subCol, int subRow) const; // slot inside the start plot
    const LandPlot* findPlotAt(int player, sf::Vector2f pos) const;          // nullptr outside the plots
    int countOwnedPlots(int player) const;
    // Terrain (F-15)
    float getRiverFlowFactor() const;   // shared hydro multiplier from all hydro plants of both players
    int countHydroPlants() const;
    // Advanced buildings (F-32, HX-10)
    static bool isPlotWideBuilding(BuildingType type); // nuclear + mega-projects fill a whole plot
    static bool isMegaProject(BuildingType type);
    bool isAdvancedUnlocked(int player, BuildingType type, std::string& reason) const;
    bool isSlotReserved(int player, sf::Vector2f slot) const; // inside a plot used by a plot-wide building
    const PlacedBuilding* getMegaProject(int player) const;   // nullptr when the player has none
    const PlacedBuilding* getReactor(int player) const;
    float getConstructionProgress(const PlacedBuilding& b) const; // 0..1 (1 = finished)
    // Lightning on a reactor or mega-project: SCRAM / construction setback instead of destruction.
    // Returns true when the strike was absorbed (the caller must not delete the building).
    bool absorbLightningAt(sf::Vector2f pos);
    // Hazards (F-34)
    int countBrokenBuildings(int player) const;
    const std::vector<HazardPlan>& getHazardPlans() const { return world.plans; }
    int getRainStreak(int player) const { return world.rainStreak[player == 2 ? 2 : 1]; }
    int getDryStreak(int player) const { return world.dryStreak[player == 2 ? 2 : 1]; }
    // Test / demo hook: trigger a hazard now (quake: westCol/row = epicentre, -1 = random)
    void triggerHazardNow(HazardKind kind, int player, int westCol = -1, int row = -1);
    // UI event queue (HazardFx): returns and clears the pending events
    std::vector<PowerFx> drainPowerFx();
};

#endif // GAME_MAIN_H
