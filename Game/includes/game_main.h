#ifndef GAME_MAIN_H
#define GAME_MAIN_H

#include <string>
#include <vector>
#include <SFML/Graphics.hpp>
#include "game_weather.h"
#include "game_expedition.h"
#include "game_random.h"

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
    WOOD,     // Gathered from forests
    ORE,      // Mined from stone/ore mines
    ENERGY,   // Megawatts (MW) generated to power the city
    GOLD      // Currency earned from supplying electricity
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
    int woodCost;
    int oreCost;
    int basePowerMW;
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
};

struct LandPlot {
    int id;
    int playerOwner; // 1 = West, 2 = East
    sf::FloatRect bounds;
    bool isPurchased;
    int costGold;
};

struct PlayerEconomy {
    int gold = 0;               // Currency (starts at 0)
    int wood = 0;               // Harvested from forests (starts at 0)
    int ore = 0;                // Mined from base mines (starts at 0)
    int energyMW = 0;           // Clean electricity generated (starts at 0)
    int landTier = 1;           // Land tier
    float cityInfluence = 0.50f;// Percentage of city supplied / captured (0.0 to 1.0)
    int selectedBuilding = 0;   // 0 = None, 1 = Solar, 2 = Wind, 3 = Hydro, 4 = Battery
    PlayerData data;            // Teammate's detailed inventory from weatherF
};

struct CityConquestState {
    int cityEnergyDemand = 30;  // Starts very low (15 MW each player quota) and grows gradually
    float p1CityShare = 0.50f;  // 0.0 to 1.0 (P1 vs P2 city control tug-of-war)
    float p1DailyDelivered = 0.0f;
    float p2DailyDelivered = 0.0f;
    bool dayCutOccurred = false;
    std::string lastCutMessage;
    int winner = 0;             // 0 = None, 1 = P1, 2 = P2
};

struct MineResult {
    int wood = 0;
    int ore = 0;
    int gold = 0;
    int coal = 0;
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
    float secondsPerDay;

    WeatherType p1Weather;
    WeatherType p2Weather;
    SeasonType currentSeason;
    float timeScale;

    PlayerEconomy p1;
    PlayerEconomy p2;
    CityConquestState city;

    std::vector<PlacedBuilding> buildings;
    std::vector<LandPlot> landPlots;

    void updateBuildingsEnergy(float dt);
    void processDayEnd();

public:
    GameEngine();
    void init(float screenWidth, float screenHeight);
    void update(float dt);

    void setTimeScale(float scale) { timeScale = (scale > 0.1f ? scale : 1.0f); }
    float getTimeScale() const { return timeScale; }

    // Player Actions
    bool mineResource(int player, ResourceType type, std::string& outMsg);
    bool mineResource(int player, ResourceType type, MineResult& result, std::string& outMsg);
    bool buyLandPlot(int player, int plotId, std::string& outMsg);
    bool buyNextLandTier(int player, std::string& outMsg);

    void cycleBuildingSelection(int player);
    void clearBuildingSelection(int player);
    BuildingType getSelectedBuilding(int player) const;

    bool canPlaceBuilding(int player, BuildingType type, sf::Vector2f pos, std::string& reason) const;
    bool placeBuilding(int player, BuildingType type, sf::Vector2f pos, std::string& outMsg);
    bool removeBuilding(int player, sf::Vector2f pos, std::string& outMsg);
    bool isAreaIlluminated(int player, sf::Vector2f pos) const;

    // Lamp consumption constant (MW)
    static constexpr float LAMP_POWER_MW = 10.0f;

    // Building Data helper
    BuildingCost getBuildingCost(BuildingType type) const;
    sf::Vector2f snapToBuildingGrid(int player, sf::Vector2f pos) const;

    // Getters for UI
    const PlayerEconomy& getPlayerEconomy(int player) const { return (player == 1) ? p1 : p2; }
    PlayerEconomy& getPlayerEconomyMut(int player) { return (player == 1) ? p1 : p2; }
    const CityConquestState& getCityState() const { return city; }
    const std::vector<PlacedBuilding>& getBuildings() const { return buildings; }
    const std::vector<LandPlot>& getLandPlots() const { return landPlots; }

    int getCurrentDay() const { return currentDay; }
    float getHour24() const { return hour24; }
    float getDayProgress() const { return hour24 / 24.0f; }
    bool isDaylight() const { return hour24 >= 6.0f && hour24 <= 18.0f; }

    WeatherType getPlayerWeather(int player) const { return (player == 1) ? p1Weather : p2Weather; }
    SeasonType getSeason() const { return currentSeason; }
};

#endif // GAME_MAIN_H
