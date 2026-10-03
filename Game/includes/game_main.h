#ifndef GAME_MAIN_H
#define GAME_MAIN_H

#include <string>
#include <vector>
#include <SFML/Graphics.hpp>
#include "game_weather.h"
#include "game_expedition.h"
#include "game_random.h"
#include "game_balance.h"
#include "game_events.h"
#include "game_config.h"

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
    DEMOLISH
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
    float maxCapacity = static_cast<float>(Balance::BATTERY.batteryCapacityMWh); // Max capacity in MWh
    float lightRadius = Balance::STREET_LAMP.lightRadius; // For Lamp light cone
    bool isBroken = false;      // Damaged/broken by lightning strike
};

struct LandPlot {
    int id;
    int playerOwner; // 1 = West, 2 = East
    sf::FloatRect bounds;
    bool isPurchased;
    int costGold;
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
};

struct CityConquestState {
    int cityEnergyDemand = 0;   // Starts at 0 MW for Day 1-2 Grace Period, then 30 MW from Day 3
    float p1CityShare = 0.50f;  // 0.0 to 1.0 (P1 vs P2 city control tug-of-war)
    float p1DailyDelivered = 0.0f; // Energy delivered to the city so far today (MW x game-seconds)
    float p2DailyDelivered = 0.0f;
    float dailySeconds = 0.0f;     // Game-seconds elapsed in the current day (06:00 -> 06:00)
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
    double gameSeconds;         // game time since 06:00 of day 1 (double: fixed steps do not drift over a match)
    int currentDay;
    float hour24;
    float revenueTimer;         // Accumulates game-seconds towards the next 1 s city payout

    WeatherType p1Weather;
    WeatherType p2Weather;
    SeasonType currentSeason;
    float timeScale;
    double stepAccumulator = 0.0;  // real seconds not yet simulated (less than one fixed step)
    int maxStepsPerUpdate = 0;     // 0 = unlimited

    PlayerEconomy p1;
    PlayerEconomy p2;
    CityConquestState city;

    std::vector<PlacedBuilding> buildings;
    std::vector<LandPlot> landPlots;
    std::vector<GameEvent> events; // pending events, drained by pollEvents()
    MatchConfig config;            // rules of the current match

    // Engine-owned random numbers: one independent stream per purpose, all derived from matchSeed, so
    // UI calls to randInt() never shift the weather sequence of a seeded match
    uint32_t matchSeed = 0;
    GameRng weatherRng;            // daily weather rolls
    GameRng hazardRng;             // random building losses (breakRandomBuilding)
    GameRng generalRng;            // randInt() / randFloat() for UI, bot and features
    float p1WindSpeed = 0.0f;      // wind speed of the day per sector (weather report, 0 = calm)
    float p2WindSpeed = 0.0f;
    int p1WindDirection = 0;       // -1 = blowing left, 0 = calm, +1 = blowing right
    int p2WindDirection = 0;

    void advanceGameTime(float gameDt); // game-seconds, split at day ends and into sub-steps
    void simulateStep(float dt);
    void updateBuildingsEnergy(float dt);
    void payCityRevenue();
    void processDayEnd();
    void rollDailyWeather();
    int findOwnedBuildingInSlot(int player, sf::Vector2f pos) const;
    void emitEvent(GameEventType type, int player, float value, const std::string& text = std::string(),
                   int subtype = 0, float x = 0.0f, float y = 0.0f);

public:
    GameEngine();
    // Starts a new match with the standard rules (MatchConfig defaults)
    void init(float screenWidth, float screenHeight);
    // Starts a new match with the given rules (out-of-range values are clamped, see MatchConfig)
    void init(const MatchConfig& cfg);
    const MatchConfig& getConfig() const { return config; }
    // Advances the match by dt real seconds (x time scale) in fixed steps of FIXED_STEP_SECONDS.
    // Leftover time below one step is kept for the next call, so any frame rate gives the same result.
    void update(float dt);
    static constexpr double FIXED_STEP_SECONDS = 1.0 / 60.0;
    // Spiral-of-death guard for real-time hosts: at most n fixed steps per update() call, a larger
    // backlog is dropped (8 steps = 133 ms per frame). 0 (default) = unlimited, so headless runs
    // and tests may pass whole days as one frame.
    void setMaxStepsPerUpdate(int n) { maxStepsPerUpdate = std::max(0, n); }
    int getMaxStepsPerUpdate() const { return maxStepsPerUpdate; }
    static constexpr int RECOMMENDED_MAX_STEPS_PER_UPDATE = 8;

    // Events since the last call (oldest first); the queue is cleared. Call once per frame.
    // At most MAX_PENDING_EVENTS are kept when nobody drains the queue (the oldest are dropped).
    std::vector<GameEvent> pollEvents();
    static constexpr size_t MAX_PENDING_EVENTS = 1024;

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
    // New match with the same rules (a fixed config seed replays the same weather)
    void restartGame() { MatchConfig same = config; init(same); }

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
    static constexpr float LAMP_POWER_MW = static_cast<float>(Balance::STREET_LAMP.lampConsumptionMW);

    // Building recipes: the single source of building costs for engine, UI and bot.
    // getBuildingDef: the Balance recipe of a type (nullptr for NONE / DEMOLISH).
    static constexpr const Balance::BuildingDef* getBuildingDef(BuildingType type) {
        return (type == BuildingType::SOLAR_PANEL)    ? &Balance::SOLAR_PANEL
               : (type == BuildingType::WIND_TURBINE) ? &Balance::WIND_TURBINE
               : (type == BuildingType::HYDRO_PLANT)  ? &Balance::HYDRO_PLANT
               : (type == BuildingType::BATTERY)      ? &Balance::BATTERY
               : (type == BuildingType::LAMP)         ? &Balance::STREET_LAMP
                                                      : nullptr;
    }
    // getBuildingCost: name, resource recipe and base MW of a type (base prices, no player modifiers)
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
    bool isGracePeriod() const { return currentDay <= config.graceDays; }

    WeatherType getPlayerWeather(int player) const { return (player == 1) ? p1Weather : p2Weather; }
    float getPlayerWindSpeed(int player) const { return (player == 1) ? p1WindSpeed : p2WindSpeed; }
    int getPlayerWindDirection(int player) const { return (player == 1) ? p1WindDirection : p2WindDirection; }

    // Match seed: MatchConfig::seed when non-zero, else the EC_SEED environment variable, else the clock.
    // std::rand is seeded with it too, for the legacy rand() calls in the UI (particles, bot, lightning).
    uint32_t getSeed() const { return matchSeed; }
    // Deterministic random numbers for UI, bot and features (own stream; the weather does not shift)
    int randInt(int lo, int hi) { return generalRng.range(lo, hi); } // inclusive range
    float randFloat() { return generalRng.unit(); }                   // [0, 1)
    SeasonType getSeason() const { return currentSeason; }
};

#endif // GAME_MAIN_H
