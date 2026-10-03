#ifndef GAME_MAIN_H
#define GAME_MAIN_H

#include <string>
#include <vector>
#include <SFML/Graphics.hpp>
#include "game_weather.h"
#include "game_expedition.h"
#include "game_random.h"
#include "game_balance.h"
#include "game_politics.h" // [b-politics] city events, council, contracts, exchange

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

    // ---- [b-politics] City politics state & engine hooks (game_politics.cpp, game_contracts.cpp,
    //      game_market.cpp). Off by default so headless tests keep their numbers; the UI turns it on.
    CityPolitics politics;
    bool cityPoliticsEnabled = false;
    void resetCityPolitics(unsigned int seed);   // init(): fresh deck, board and market
    void tickCityPoliticsRealTime(float realDt); // update(): council countdown in real seconds
    void updateCityPolitics(float dt);           // simulateStep(): contracts, buffs, council timing
    void applyPowerImport(float dt);             // updateBuildingsEnergy(): cross-river import
    void restoreBaseCityDemand();                // processDayEnd(): undo today's event multiplier
    void onCityPoliticsNewDay(int endedDay);     // processDayEnd(): settle the day, roll the next one
    float politicsGenMult(int player, BuildingType type) const;
    float politicsPayoutMult(int player) const;
    float politicsGoldMult(int player) const;
    float politicsMineMult(int player, ResourceType type) const;
    // helpers shared by the politics files
    void politicsNotice(const std::string& text, int player, Politics::Tone tone);
    void addCityShare(int player, int tenths);   // +10 = +1% share for `player`
    void startCouncilCard();
    void resolveCouncil();
    void postDailyContracts(int day);
    void updateContracts(float dt, float prevDayHour, float dayHour);
    void settleContractsAtDayEnd();
    float dayHourNow() const;                    // hours since 06:00 (0..24)

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

    // -------------------------------------------------------------------------
    // [b-politics] City politics public API
    // -------------------------------------------------------------------------
    void setCityPoliticsEnabled(bool on) { cityPoliticsEnabled = on; }
    bool isCityPoliticsEnabled() const { return cityPoliticsEnabled; }
    const CityPolitics& getPolitics() const { return politics; }
    std::vector<Politics::PoliticsNotice> drainPoliticsNotices();
    float getPriceScale() const { return Politics::priceScale(currentDay); }

    // F-09 City Event Deck
    const Politics::EventDef& getActiveCityEvent() const { return Politics::getEventDef(politics.activeEvent); }
    const Politics::EventDef& getForecastCityEvent() const { return Politics::getEventDef(politics.forecastEvent); }
    int getNextFestivalDay() const;              // today or later; 0 when none is left
    int getBaseCityDemand() const { return politics.baseDemandMW; }
    void debugForceCityEvent(Politics::EventId today, Politics::EventId tomorrow); // tests / demo mode

    // F-13 Council Decisions
    bool chooseCouncilOption(int player, int option, std::string& outMsg);
    int suggestCouncilOption(int player, int skill) const; // bot heuristic (skill 1..3)
    void debugStartCouncilCard(int cardId);                // tests / demo mode

    // F-16 City Contract Board
    int getTenderBidStep() const;
    int debugPostContract(Politics::ContractKind kind);    // tests / demo mode: adds one contract, returns its id
    bool placeBid(int player, int contractId, int newTotalBid, std::string& outMsg); // escrowed
    int getPoweredLampCount(int player) const;
    float getStoredBatteryMWh(int player) const;

    // F-31 Exchange & emergency import
    int getMarketLotSize(ResourceType type) const;
    int getMarketBuyPrice(ResourceType type) const;   // money per lot
    int getMarketSellPrice(ResourceType type) const;  // money per lot
    float getMarketMultiplier(ResourceType type) const;
    bool tradeResource(int player, ResourceType type, bool buy, std::string& outMsg); // one lot
    void setImportRequest(int player, bool on);
    void setExportAllowed(int player, bool on);
    int getImportFlowMW(int player) const;          // MW the player imports right now
    float getImportPricePerMWs() const;             // money per MW per game-second

    // Bot: council answers, tender bids, import/export and market use (skill 1..3)
    void politicsBotThink(int player, int skill);
};

#endif // GAME_MAIN_H
