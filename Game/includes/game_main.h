#ifndef GAME_MAIN_H
#define GAME_MAIN_H

#include <string>
#include <vector>
#include <SFML/Graphics.hpp>
#include "game_weather.h"
#include "game_expedition.h"
#include "game_random.h"
#include "game_balance.h"
#include "game_match.h" // [b-options] match rules, mutators, charters, research, sandbox

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
    float maxCapacity = 200.0f; // Max capacity in MWh
    float lightRadius = 150.0f; // For Lamp light cone
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
    PlayerData data;            // Teammate's detailed inventory from weatherF
    TechState tech;             // [b-options] F-33 research choices (reset every match)
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

    // [b-options] Match options & progression state + hooks (game_match_engine.cpp,
    // game_research.cpp, game_sandbox.cpp). `rules` survives init()/restartGame().
    MatchRules rules;
    SandboxState sandbox;
    SeasonType seasonForDay(int day) const;               // season lock (Eternal Winter / sandbox)
    SeasonType seasonAtGameSeconds(float seconds) const;
    void applyMatchStartRules();                          // plot prices, Head Start, sandbox refill
    void applyWeatherRules();                             // Mirror Weather, sandbox weather locks
    void applyFrameRules();                               // sandbox: keep resources topped up
    void refreshPerkDependentState(int player);           // battery capacity, unbought plot prices
    int scaledIncome(int player, int payout) const;
    float getMiningYieldScale(int player) const;
    std::string graceDayEndMessage(int endedDay) const;
    std::string welcomeMessage() const;

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
    bool isGracePeriod() const { return currentDay <= getGraceDays(); } // [b-options] from MatchRules

    WeatherType getPlayerWeather(int player) const { return (player == 1) ? p1Weather : p2Weather; }
    SeasonType getSeason() const { return currentSeason; }

    // -------------------------------------------------------------------------
    // [b-options] F-03 Match rules (call setMatchRules() before init()/restartGame())
    // -------------------------------------------------------------------------
    void setMatchRules(const MatchRules& newRules);
    const MatchRules& getMatchRules() const { return rules; }
    int getGraceDays() const { return rules.effectiveGraceDays(); }
    int getFinalDay() const { return rules.finalDay; }          // 0 = endless
    float getVictoryShare() const { return rules.victoryShare; }
    float getDaySeconds() const { return rules.daySeconds; }    // real seconds per day at 1x
    float getPaceMultiplier() const;                            // game-seconds per real second
    bool isSandbox() const { return rules.sandbox; }
    int getDaysLeft() const;                                    // -1 when endless

    // [b-options] F-35 / F-33 / F-24 per-player modifiers
    PlayerPerks getPlayerPerks(int player) const;
    BuildingCost getBuildingCostFor(int player, BuildingType type) const; // cost after perks
    int getMineYield(int player, ResourceType type) const;     // what mineResource() gives now
    float getMiningCooldown(int player) const;                 // real seconds between mining actions
    float getBatteryCapacityFor(int player) const;             // MWh of one battery
    float getLampDrawFor(int player) const;                    // MW one powered lamp consumes
    float getLampRadiusFor(int player) const;                  // px a powered lamp lights

    // [b-options] F-33 Research lab (game_research.cpp)
    int getTechChoice(int player, int branch, int tier) const; // -1 = not researched
    int getResearchedTierCount(int player, int branch) const;
    TechStatus getTechStatus(int player, int branch, int tier, int option) const;
    bool researchTech(int player, int branch, int tier, int option, std::string& outMsg);
    bool autoResearch(int player, std::string& outMsg);        // bot helper: cheapest affordable tier

    // [b-options] F-21 Practice sandbox controls (game_sandbox.cpp; usable in any mode for tests)
    void sandboxSetClockSpeed(float speed);                    // 0 = paused, max 32
    float getSandboxClockSpeed() const { return sandbox.clockSpeed; }
    void sandboxSetHour(float hour24);                         // jump within the current day
    void sandboxSetWeather(int player, WeatherType w);         // locks that sector's weather
    void sandboxUnlockWeather();
    bool isSandboxWeatherLocked(int player) const { return sandbox.weatherLocked[(player == 2) ? 1 : 0]; }
    void sandboxSetSeason(int season);                         // -1 = natural seasons
    int getSandboxSeasonOverride() const { return sandbox.seasonOverride; }
    void sandboxSetDemand(int demandMW);                       // locks the city demand
    bool isSandboxDemandLocked() const { return sandbox.demandLocked; }
    int sandboxClearBuildings(int player);                     // returns how many were removed
    float projectGenerationMW(int player, float hour24) const; // raw output at that hour today
};

#endif // GAME_MAIN_H
