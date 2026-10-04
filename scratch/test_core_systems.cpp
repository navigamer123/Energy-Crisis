// =============================================================================
// ENERGY CRISIS - CORE SYSTEMS UNIT TESTS
// Unit tests for the stable engine systems: mining and the 1 s pause between hits, mine
// upgrades, land, the plot/slot grid, placement rules, demolition, weather, the generator
// curves, the grace period, seasons and restart.
// Headless: built from this file + Game/scr/*.cpp only, no SFML needed ("make test").
//
// Deterministic: the engine seeds its RNG from EC_SEED. When EC_SEED is not set this program
// sets EC_SEED=20261003, so every run is identical. Run with EC_SEED=<n> to try another seed;
// every check holds for any seed (weather-dependent checks read the weather that was rolled).
//
// Documented values (docs/GAMEPLAY.md) are pinned in the kDoc* tables below. When a value is
// retuned on purpose, update the code, docs/GAMEPLAY.md and the table here together.
// Every check is reported; the program exits with 1 when any check failed.
// =============================================================================
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "../Game/includes/game_main.h"

#ifdef _WIN32
// Declared by hand: strict -std=c++17 hides _putenv in some MinGW headers (msvcrt exports it)
extern "C" __declspec(dllimport) int __cdecl _putenv(const char*);
#endif

namespace {

int g_checks = 0;
int g_failures = 0;
int g_groupFailures = 0;

#define CHECK(cond, details)                                                                   \
    do {                                                                                       \
        ++g_checks;                                                                            \
        if (!(cond)) {                                                                         \
            ++g_failures;                                                                      \
            ++g_groupFailures;                                                                 \
            std::cerr << "    FAIL (line " << __LINE__ << "): " << #cond << " | " << details << "\n"; \
        }                                                                                      \
    } while (0)

// A failed precondition (a placement or purchase the test relies on) stops the program
#define REQUIRE(cond, details)                                                                 \
    do {                                                                                       \
        ++g_checks;                                                                            \
        if (!(cond)) {                                                                         \
            std::cerr << "    FATAL (line " << __LINE__ << "): " << #cond << " | " << details << "\n"; \
            std::exit(1);                                                                      \
        }                                                                                      \
    } while (0)

const char* const kDefaultSeed = "20261003";
constexpr float kFrame = 1.0f / 60.0f; // one frame at 60 FPS

void beginGroup(const std::string& name) {
    g_groupFailures = 0;
    std::cout << "\n[" << name << "]\n";
}

void endGroup() {
    std::cout << (g_groupFailures == 0 ? "  -> PASS\n" : "  -> FAIL\n");
}

bool has(const std::string& text, const char* needle) { return text.find(needle) != std::string::npos; }
bool near(float a, float b, float eps) { return std::abs(a - b) <= eps; }
int idx(WeatherType w) { return static_cast<int>(w); }
int idx(SeasonType s) { return static_cast<int>(s); }

const WeatherType kWeathers[6] = { WeatherType::SUNNY, WeatherType::WINDY, WeatherType::RAINY,
                                   WeatherType::STORMY, WeatherType::SNOWY, WeatherType::CLOUDY };
const SeasonType kSeasons[4] = { SeasonType::SPRING, SeasonType::SUMMER, SeasonType::AUTUMN, SeasonType::WINTER };

// ---------------------------------------------------------------------------
// Documented values (docs/GAMEPLAY.md)
// ---------------------------------------------------------------------------
// §4 Yield per hit at level 1, indexed by ResourceType (WOOD..GOLD)
const int kDocBaseYield[8] = { 0, 12, 8, 6, 6, 6, 4, 3 };
// §4 Price of reaching levels 2..6, and the total from level 1 to 6
const int kDocUpgradeCost[5] = { 30, 300, 500, 800, 1500 };
const int kDocGoldUpgradeCost[5] = { 50, 400, 700, 1100, 2000 };
const int kDocUpgradeTotal = 3130;
const int kDocGoldUpgradeTotal = 4250;
// §2 Plot price by row/column counted from the player's own starting corner; total of the 11 for sale
const int kDocLandCost[4][3] = { { 150, 195, 240 }, { 285, 330, 375 }, { 420, 465, 510 }, { 555, 600, 645 } };
const int kDocLandTotal = 4620;
// §3 Sunrise, sunset and the solar season factor, indexed by SeasonType
const float kDocSunrise[4] = { 6.0f, 5.0f, 7.0f, 8.0f };
const float kDocSunset[4] = { 19.0f, 21.0f, 18.0f, 16.5f };
const float kDocSolarSeason[4] = { 1.00f, 1.15f, 0.95f, 0.85f };
// §6 Weather factors, indexed by WeatherType (SUNNY, WINDY, RAINY, STORMY, SNOWY, CLOUDY)
const float kDocSolarWeather[6] = { 1.6f, 1.0f, 0.5f, 0.1f, 0.3f, 0.8f };
const float kDocWindWeather[6] = { 0.8f, 1.8f, 1.2f, 2.2f, 1.2f, 0.8f };
const float kDocHydroWeather[6] = { 0.7f, 1.0f, 2.0f, 2.5f, 1.0f, 0.8f };
// §8 Probability of each weather per season in percent, indexed by WeatherType
const double kDocWeatherPct[4][6] = {
    { 12.2, 19.7, 48.0, 12.0, 0.0, 8.1 },  // spring
    { 12.1, 6.6, 56.0, 24.0, 0.0, 1.3 },   // summer
    { 3.7, 8.9, 50.4, 21.6, 10.0, 5.5 },   // autumn
    { 2.6, 6.9, 10.8, 7.2, 70.0, 2.6 },    // winter
};
// §7 City demand: 0 MW during the 2-day grace period, then 15 x day - 15
int docDemandForDay(int day) { return (day <= 2) ? 0 : 15 * day - 15; }

// ---------------------------------------------------------------------------
// Resources and buildings
// ---------------------------------------------------------------------------
struct ResourceCase {
    ResourceType type;
    const char* name;
    int baseYield;
    int PlayerEconomy::*stock;
    int MineResult::*mined;
};

const ResourceCase kResources[7] = {
    { ResourceType::WOOD, "wood", Balance::WOOD_BASE_YIELD, &PlayerEconomy::wood, &MineResult::wood },
    { ResourceType::IRON, "iron", Balance::IRON_BASE_YIELD, &PlayerEconomy::iron, &MineResult::iron },
    { ResourceType::COPPER, "copper", Balance::COPPER_BASE_YIELD, &PlayerEconomy::copper, &MineResult::copper },
    { ResourceType::COAL, "coal", Balance::COAL_BASE_YIELD, &PlayerEconomy::coal, &MineResult::coal },
    { ResourceType::SILICON, "silicon", Balance::SILICON_BASE_YIELD, &PlayerEconomy::silicon, &MineResult::silicon },
    { ResourceType::SILVER, "silver", Balance::SILVER_BASE_YIELD, &PlayerEconomy::silver, &MineResult::silver },
    { ResourceType::GOLD, "gold", Balance::GOLD_BASE_YIELD, &PlayerEconomy::gold, &MineResult::gold },
};

// Recipe resources in docs order: wood, iron, copper, coal, silicon, silver
int PlayerEconomy::* const kRecipeStock[6] = { &PlayerEconomy::wood, &PlayerEconomy::iron, &PlayerEconomy::copper,
                                               &PlayerEconomy::coal, &PlayerEconomy::silicon, &PlayerEconomy::silver };
int BuildingCost::* const kRecipeCost[6] = { &BuildingCost::woodCost, &BuildingCost::ironCost, &BuildingCost::copperCost,
                                             &BuildingCost::coalCost, &BuildingCost::siliconCost, &BuildingCost::silverCost };

struct BuildCase {
    BuildingType type;
    const char* name;
    int plotId;     // a P1 plot where the building is allowed (hydro needs the river bank)
    int refund[6];  // docs/GAMEPLAY.md §5 "Какво се връща при Премахване (50%)"
};

const BuildCase kBuildCases[5] = {
    { BuildingType::SOLAR_PANEL, "solar panel", 1, { 3, 2, 3, 0, 4, 0 } },
    { BuildingType::WIND_TURBINE, "wind turbine", 1, { 4, 7, 4, 3, 0, 0 } },
    { BuildingType::HYDRO_PLANT, "hydro plant", 3, { 7, 10, 6, 0, 3, 0 } },
    { BuildingType::BATTERY, "battery", 1, { 2, 4, 5, 2, 0, 2 } },
    { BuildingType::LAMP, "lamp", 1, { 2, 2, 1, 0, 0, 0 } },
};

// Every stock of a player, for "nothing else changed" checks
std::vector<int> stocksOf(const PlayerEconomy& p) {
    return { p.money, p.gold, p.silver, p.iron, p.coal, p.copper, p.silicon, p.wood, p.ore };
}

void giveResources(GameEngine& e, int player, int amount) {
    PlayerEconomy& p = e.getPlayerEconomyMut(player);
    for (int r = 0; r < 6; ++r) p.*kRecipeStock[r] = amount;
}

// Grid position of slot `sub` (0..8, row-major) inside a land plot (ids 1-12 = P1, 13-24 = P2)
sf::Vector2f slotOf(const GameEngine& e, int player, int plotId, int sub) {
    int plotIndex = (plotId - 1) % 12;
    int col = (plotIndex % 3) * 3 + sub % 3;
    int row = (plotIndex / 3) * 3 + sub / 3;
    return e.getGridSlot(player, col, row);
}

void place(GameEngine& e, int player, BuildingType type, sf::Vector2f pos) {
    std::string msg;
    bool ok = e.placeBuilding(player, type, pos, msg);
    REQUIRE(ok, "placeBuilding P" << player << " type " << static_cast<int>(type) << " at (" << pos.x << "; "
                                  << pos.y << ") failed: " << msg);
}

void buyPlot(GameEngine& e, int player, int plotId) {
    PlayerEconomy& econ = e.getPlayerEconomyMut(player);
    int goldBefore = econ.gold;
    econ.gold += 100000;
    std::string msg;
    bool ok = e.buyLandPlot(player, plotId, msg);
    REQUIRE(ok, "buyLandPlot P" << player << " plot " << plotId << " failed: " << msg);
    econ.gold = goldBefore;
}

// Six wind turbines on the starting plot: far above the demand of days 3-5; the other player has nothing
void buildWindFarm(GameEngine& e, int player) {
    giveResources(e, player, 1000);
    int startPlot = (player == 1) ? 1 : 15;
    for (int i = 0; i < 6; ++i) place(e, player, BuildingType::WIND_TURBINE, slotOf(e, player, startPlot, i));
}

const PlacedBuilding* findBuilding(const GameEngine& e, int player, BuildingType type) {
    for (const auto& b : e.getBuildings()) {
        if (b.playerOwner == player && b.type == type) return &b;
    }
    return nullptr;
}

// Plays 0.25 s steps until the next day end has been settled
void runToNextDay(GameEngine& e) {
    const int day = e.getCurrentDay();
    int steps = 0;
    while (e.getCurrentDay() == day && e.getCityState().winner == 0) {
        e.update(0.25f);
        REQUIRE(++steps < 1000, "day " << day << " never ended");
    }
}

// Advances the clock to the next time it shows `hour` (1 game-hour = 3.75 s)
void advanceToHour(GameEngine& e, float hour) {
    float delta = std::fmod(hour - e.getHour24() + 24.0f, 24.0f);
    e.update(delta * Balance::SECONDS_PER_DAY / 24.0f);
}

int expectedYield(int baseYield, int level) {
    // round(base x (1 + 0.75 x (level - 1))), computed in integers (half rounds up)
    return (baseYield * (4 + 3 * (level - 1)) + 2) / 4;
}

// EC_SEED handling (nullptr removes the variable)
void setSeedEnv(const char* value) {
#ifdef _WIN32
    std::string s = std::string("EC_SEED=") + (value ? value : "");
    _putenv(s.c_str());
#else
    if (value) {
        setenv("EC_SEED", value, 1);
    } else {
        unsetenv("EC_SEED");
    }
#endif
}

unsigned int testSeed() {
    const char* env = std::getenv("EC_SEED");
    return static_cast<unsigned int>(std::strtoul(env ? env : kDefaultSeed, nullptr, 10));
}

// ---------------------------------------------------------------------------
// Mining yields
// ---------------------------------------------------------------------------
void testMiningYields() {
    beginGroup("Mining yields per resource and player");
    for (const ResourceCase& rc : kResources) {
        CHECK(rc.baseYield == kDocBaseYield[static_cast<int>(rc.type)],
              rc.name << " base yield " << rc.baseYield << ", docs say " << kDocBaseYield[static_cast<int>(rc.type)]);
    }

    for (int player = 1; player <= 2; ++player) {
        for (const ResourceCase& rc : kResources) {
            GameEngine e;
            e.init(1600.0f, 900.0f);
            const PlayerEconomy before = e.getPlayerEconomy(player);
            const std::vector<int> otherBefore = stocksOf(e.getPlayerEconomy(3 - player));
            GameEngine::MineResult res;
            std::string msg;
            CHECK(e.mineResource(player, rc.type, res, msg), "P" << player << " could not mine " << rc.name << ": " << msg);
            const PlayerEconomy& after = e.getPlayerEconomy(player);
            CHECK(after.*rc.stock - before.*rc.stock == rc.baseYield,
                  "P" << player << " " << rc.name << ": +" << (after.*rc.stock - before.*rc.stock) << ", expected +" << rc.baseYield);
            for (const ResourceCase& other : kResources) {
                if (other.type == rc.type) continue;
                CHECK(after.*other.stock == before.*other.stock, "mining " << rc.name << " changed " << other.name);
            }
            CHECK(after.money == before.money && after.ore == before.ore, "mining " << rc.name << " changed money or ore");
            CHECK(res.type == rc.type && res.amount == rc.baseYield && res.*rc.mined == rc.baseYield,
                  rc.name << " MineResult: type " << static_cast<int>(res.type) << " amount " << res.amount);
            const std::string prefix = "+" + std::to_string(rc.baseYield) + " ";
            CHECK(msg.compare(0, prefix.size(), prefix) == 0, rc.name << " message: " << msg);
            CHECK(stocksOf(e.getPlayerEconomy(3 - player)) == otherBefore, "P" << player << " mining changed the other player");
        }
    }

    GameEngine e;
    e.init(1600.0f, 900.0f);
    const std::vector<int> start = stocksOf(e.getPlayerEconomy(1));
    std::string msg;
    CHECK(!e.mineResource(1, ResourceType::MONEY, msg), "money has no station but was mined");
    CHECK(!e.mineResource(1, ResourceType::NONE, msg), "NONE was mined");
    CHECK(!e.mineResource(1, ResourceType::ENERGY, msg), "ENERGY was mined");
    CHECK(stocksOf(e.getPlayerEconomy(1)) == start, "a refused hit changed the stock");

    // Legacy cave expedition (ORE): the base iron, copper, coal and gold yields
    GameEngine::MineResult res;
    CHECK(e.mineResource(1, ResourceType::ORE, res, msg), "ORE refused: " << msg);
    const PlayerEconomy& p = e.getPlayerEconomy(1);
    CHECK(p.iron == Balance::IRON_BASE_YIELD && p.copper == Balance::COPPER_BASE_YIELD && p.coal == Balance::COAL_BASE_YIELD &&
              p.gold == Balance::GOLD_BASE_YIELD && p.ore == 0 && p.wood == 0,
          "ORE gave iron " << p.iron << " copper " << p.copper << " coal " << p.coal << " gold " << p.gold << " ore " << p.ore);

    // The yield does not depend on the time of day: a hit at night gives the same amount
    advanceToHour(e, 23.0f);
    REQUIRE(!e.isDaylight(), "23:00 should be night, hour " << e.getHour24());
    const int ironBefore = e.getPlayerEconomy(1).iron;
    CHECK(e.mineResource(1, ResourceType::IRON, msg), "no mining at night: " << msg);
    CHECK(e.getPlayerEconomy(1).iron - ironBefore == Balance::IRON_BASE_YIELD, "night yield differs");
    endGroup();
}

// ---------------------------------------------------------------------------
// The 1 s pause between hits
// The pause lives in the caller: UI_map keeps ONE timer per player, shared by all of the player's
// stations and decremented by real frame time, and refuses a hit while it is above zero
// (UI/scr/UI_map_controls.cpp, docs/GAMEPLAY.md §4). UI_map needs SFML, so HarvestGate repeats
// that rule on top of the real engine.
// ---------------------------------------------------------------------------
struct HarvestGate {
    float cooldown = 0.0f;
    void tick(float realDt) {
        if (cooldown > 0.0f) cooldown -= realDt;
    }
    bool hit(GameEngine& e, int player, ResourceType type) {
        if (cooldown > 0.0f) return false;
        std::string msg;
        if (!e.mineResource(player, type, msg)) return false;
        cooldown = Balance::MINE_COOLDOWN_SEC;
        return true;
    }
};

void testMiningCooldown() {
    beginGroup("1 s pause between hits, shared by a player's stations");
    CHECK(Balance::MINE_COOLDOWN_SEC == 1.0f, "MINE_COOLDOWN_SEC = " << Balance::MINE_COOLDOWN_SEC << ", docs say 1 s");

    // Pressing every frame for 10 s while hopping across all 7 stations: one hit per second
    {
        GameEngine e;
        e.init(1600.0f, 900.0f);
        HarvestGate gate;
        std::vector<float> hitTimes;
        int expectedGain[7] = { 0 };
        for (int frame = 0; frame < 600; ++frame) {
            gate.tick(kFrame);
            const ResourceCase& rc = kResources[frame % 7];
            if (gate.hit(e, 1, rc.type)) {
                hitTimes.push_back(frame * kFrame);
                expectedGain[frame % 7] += rc.baseYield;
            }
            e.update(kFrame);
        }
        CHECK(hitTimes.size() == 10u, "hits in 10 s: " << hitTimes.size());
        CHECK(!hitTimes.empty() && hitTimes[0] == 0.0f, "the first hit must not wait");
        for (size_t i = 1; i < hitTimes.size(); ++i) {
            float gap = hitTimes[i] - hitTimes[i - 1];
            CHECK(gap >= Balance::MINE_COOLDOWN_SEC - 1e-3f && gap <= Balance::MINE_COOLDOWN_SEC + kFrame + 1e-3f,
                  "gap between hits " << i << " and " << i + 1 << ": " << gap << " s");
        }
        for (int r = 0; r < 7; ++r) {
            CHECK(e.getPlayerEconomy(1).*kResources[r].stock == expectedGain[r],
                  kResources[r].name << ": " << e.getPlayerEconomy(1).*kResources[r].stock << ", expected " << expectedGain[r]);
        }
    }

    // Another station does not skip the pause; the other player has a timer of their own
    {
        GameEngine e;
        e.init(1600.0f, 900.0f);
        HarvestGate p1Gate, p2Gate;
        CHECK(p1Gate.hit(e, 1, ResourceType::WOOD), "P1 first hit refused");
        CHECK(!p1Gate.hit(e, 1, ResourceType::IRON), "P1 skipped the pause by switching station");
        CHECK(p2Gate.hit(e, 2, ResourceType::WOOD), "P2 blocked by P1's pause");
        p1Gate.tick(0.5f);
        p2Gate.tick(0.5f);
        CHECK(!p1Gate.hit(e, 1, ResourceType::GOLD) && !p2Gate.hit(e, 2, ResourceType::GOLD), "hit allowed after 0.5 s");
        p1Gate.tick(0.5f);
        CHECK(p1Gate.hit(e, 1, ResourceType::GOLD), "P1 hit refused after 1 s");
        CHECK(e.getPlayerEconomy(1).wood == Balance::WOOD_BASE_YIELD && e.getPlayerEconomy(1).iron == 0 &&
                  e.getPlayerEconomy(1).gold == Balance::GOLD_BASE_YIELD,
              "P1 stock after the gated hits is wrong");
    }

    // The pause is real time: at the x6 mining speed-up one game day (90 s) lasts 15 s of real time
    // and gives 15 hits, not 90 (docs/GAMEPLAY.md §3)
    {
        GameEngine e;
        e.init(1600.0f, 900.0f);
        e.setTimeScale(Balance::MINE_SPEEDUP_MULT);
        HarvestGate gate;
        int hitsOnDay2 = 0;
        int framesOnDay2 = 0;
        for (int frame = 0; frame < 3000 && e.getCurrentDay() < 3; ++frame) {
            gate.tick(kFrame);
            bool hit = gate.hit(e, 1, ResourceType::IRON);
            if (e.getCurrentDay() == 2) {
                ++framesOnDay2;
                hitsOnDay2 += hit ? 1 : 0;
            }
            e.update(kFrame);
        }
        const float realDay = framesOnDay2 * kFrame;
        const float expectedRealDay = Balance::SECONDS_PER_DAY / Balance::MINE_SPEEDUP_MULT;
        const int expectedHits = static_cast<int>(expectedRealDay / Balance::MINE_COOLDOWN_SEC);
        CHECK(near(realDay, expectedRealDay, 0.1f), "day 2 at x6 lasted " << realDay << " real s, expected " << expectedRealDay);
        CHECK(std::abs(hitsOnDay2 - expectedHits) <= 1, "hits during day 2 at x6: " << hitsOnDay2 << ", expected " << expectedHits);
    }
    endGroup();
}

// ---------------------------------------------------------------------------
// Mine upgrades
// ---------------------------------------------------------------------------
void testMineUpgrades() {
    beginGroup("Mine upgrade cost curve and yield bonus");
    CHECK(Balance::MINE_MAX_LEVEL == 6, "max mine level " << Balance::MINE_MAX_LEVEL << ", docs say 6");
    for (int level = 1; level <= Balance::MINE_MAX_LEVEL; ++level) {
        CHECK(Balance::getMineYieldMultiplier(level) == 1.0f + 0.75f * (level - 1),
              "level " << level << " multiplier " << Balance::getMineYieldMultiplier(level));
    }
    // The cost curve rises at every level and the gold mine is dearer at every level
    for (int level = 1; level < Balance::MINE_MAX_LEVEL; ++level) {
        CHECK(Balance::getMineUpgradeCost(level, true) > Balance::getMineUpgradeCost(level, false), "gold mine not dearer at level " << level);
        if (level > 1) {
            CHECK(Balance::getMineUpgradeCost(level, false) > Balance::getMineUpgradeCost(level - 1, false), "cost falls at level " << level);
            CHECK(Balance::getMineUpgradeCost(level, true) > Balance::getMineUpgradeCost(level - 1, true), "gold cost falls at level " << level);
        }
    }

    for (const ResourceCase& rc : kResources) {
        GameEngine e;
        e.init(1600.0f, 900.0f);
        PlayerEconomy& p1 = e.getPlayerEconomyMut(1);
        const bool goldMine = (rc.type == ResourceType::GOLD);
        const int* docCost = goldMine ? kDocGoldUpgradeCost : kDocUpgradeCost;
        int spent = 0;
        std::string msg;
        for (int level = 1; level <= Balance::MINE_MAX_LEVEL; ++level) {
            CHECK(e.getMineLevel(1, rc.type) == level, rc.name << " level " << e.getMineLevel(1, rc.type) << ", expected " << level);

            // Yield at this level: +75% of the base per level, rounded
            const int stockBefore = p1.*rc.stock;
            REQUIRE(e.mineResource(1, rc.type, msg), "could not mine " << rc.name << ": " << msg);
            const int gain = p1.*rc.stock - stockBefore;
            CHECK(gain == expectedYield(rc.baseYield, level),
                  rc.name << " level " << level << " yield " << gain << ", expected " << expectedYield(rc.baseYield, level));
            CHECK(level == 1 || has(msg, ("[НИВО " + std::to_string(level) + "]").c_str()), rc.name << " message lacks the level: " << msg);

            if (level < Balance::MINE_MAX_LEVEL) {
                const int cost = e.getMineUpgradeCost(1, rc.type);
                CHECK(cost == docCost[level - 1], rc.name << " level " << level << " -> " << level + 1 << " costs " << cost << ", docs say " << docCost[level - 1]);
                CHECK(cost == Balance::getMineUpgradeCost(level, goldMine), rc.name << " engine and Balance disagree on the cost");

                // One gold short: refused, nothing spent
                p1.gold = cost - 1;
                CHECK(!e.upgradeMine(1, rc.type, msg), rc.name << " upgraded with " << cost - 1 << " G for a " << cost << " G upgrade");
                CHECK(p1.gold == cost - 1 && e.getMineLevel(1, rc.type) == level, rc.name << " refused upgrade spent gold or changed the level");

                // The exact price: accepted, all of it spent
                p1.gold = cost;
                CHECK(e.upgradeMine(1, rc.type, msg), rc.name << " upgrade refused with exactly " << cost << " G: " << msg);
                CHECK(p1.gold == 0, rc.name << " upgrade left " << p1.gold << " G of " << cost);
                spent += cost;

                // The message names the bonus that the multiplier really adds
                const size_t plus = msg.find("(+");
                const int shownPct = (plus == std::string::npos) ? -1 : std::atoi(msg.c_str() + plus + 2);
                const int realPct = static_cast<int>(std::lround((Balance::getMineYieldMultiplier(2) - Balance::getMineYieldMultiplier(1)) * 100.0f));
                CHECK(shownPct == realPct, "upgrade message shows +" << shownPct << "%, the multiplier adds +" << realPct << "%: " << msg);
            } else {
                CHECK(e.getMineUpgradeCost(1, rc.type) == -1, rc.name << " max level cost " << e.getMineUpgradeCost(1, rc.type));
                CHECK(Balance::getMineUpgradeCost(level, goldMine) == 0, "Balance cost at max level is not 0");
                p1.gold = 100000;
                CHECK(!e.upgradeMine(1, rc.type, msg), rc.name << " upgraded past the max level");
                CHECK(p1.gold == 100000 && e.getMineLevel(1, rc.type) == Balance::MINE_MAX_LEVEL, rc.name << " max-level refusal spent gold");
            }
        }
        CHECK(spent == (goldMine ? kDocGoldUpgradeTotal : kDocUpgradeTotal), rc.name << " level 1 -> 6 cost " << spent << " G");

        // Upgrades are per mine and per player
        for (const ResourceCase& other : kResources) {
            if (other.type != rc.type) CHECK(e.getMineLevel(1, other.type) == 1, "upgrading " << rc.name << " raised " << other.name);
        }
        CHECK(e.getMineLevel(2, rc.type) == 1, "upgrading P1 " << rc.name << " raised P2's mine");
        const int p2Before = e.getPlayerEconomy(2).*rc.stock;
        e.mineResource(2, rc.type, msg);
        CHECK(e.getPlayerEconomy(2).*rc.stock - p2Before == rc.baseYield, "P2 " << rc.name << " yield changed by P1's upgrades");
    }

    // Things that are not mines cannot be upgraded and cost nothing
    GameEngine e;
    e.init(1600.0f, 900.0f);
    e.getPlayerEconomyMut(1).gold = 100000;
    std::string msg;
    const ResourceType notMines[4] = { ResourceType::NONE, ResourceType::MONEY, ResourceType::ENERGY, ResourceType::ORE };
    for (ResourceType t : notMines) {
        CHECK(!e.upgradeMine(1, t, msg), "resource " << static_cast<int>(t) << " upgraded");
    }
    CHECK(e.getPlayerEconomy(1).gold == 100000, "refused non-mine upgrades spent gold");
    endGroup();
}

// ---------------------------------------------------------------------------
// Land
// ---------------------------------------------------------------------------
void testLandPurchase() {
    beginGroup("Land prices and ownership per side");
    {
        GameEngine e;
        e.init(1600.0f, 900.0f);
        const std::vector<LandPlot>& plots = e.getLandPlots();
        REQUIRE(static_cast<int>(plots.size()) == Balance::TOTAL_PLOTS && Balance::TOTAL_PLOTS == 2 * Balance::PLOTS_PER_PLAYER &&
                    Balance::PLOTS_PER_PLAYER == 12,
                "plots: " << plots.size());
        for (size_t i = 0; i < plots.size(); ++i) {
            CHECK(plots[i].id == static_cast<int>(i) + 1 && plots[i].playerOwner == (i < 12 ? 1 : 2),
                  "plot #" << i << " has id " << plots[i].id << " owner " << plots[i].playerOwner);
            CHECK(plots[i].isPurchased == (plots[i].id == 1 || plots[i].id == 15), "plot " << plots[i].id << " purchased at start: " << plots[i].isPurchased);
        }
        // Prices count from each player's own starting corner, so mirrored plots cost the same
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 3; ++c) {
                const LandPlot& west = plots[r * 3 + c];
                const LandPlot& east = plots[12 + r * 3 + (2 - c)];
                CHECK(west.costGold == kDocLandCost[r][c] && west.costGold == Balance::getLandPlotCost(r, c),
                      "P1 plot " << west.id << " costs " << west.costGold << " G, docs say " << kDocLandCost[r][c]);
                CHECK(east.costGold == kDocLandCost[r][c], "P2 plot " << east.id << " costs " << east.costGold << " G, docs say " << kDocLandCost[r][c]);
            }
        }
        // The starting plots are the top outer corners (P1 top-left, P2 top-right)
        for (const LandPlot& plot : plots) {
            if (plot.playerOwner == 1) {
                CHECK(plot.bounds.position.x >= plots[0].bounds.position.x && plot.bounds.position.y >= plots[0].bounds.position.y,
                      "P1 start plot is not the top-left plot (plot " << plot.id << ")");
            } else {
                CHECK(plot.bounds.position.x <= plots[14].bounds.position.x && plot.bounds.position.y >= plots[14].bounds.position.y,
                      "P2 start plot is not the top-right plot (plot " << plot.id << ")");
            }
        }

        PlayerEconomy& p1 = e.getPlayerEconomyMut(1);
        PlayerEconomy& p2 = e.getPlayerEconomyMut(2);
        std::string msg;
        p1.gold = 194;
        CHECK(!e.buyLandPlot(1, 2, msg), "plot 2 (195 G) bought with 194 G");
        CHECK(p1.gold == 194 && !plots[1].isPurchased && p1.landTier == 1, "refused purchase changed something");
        CHECK(has(msg, "195"), "refusal does not name the price: " << msg);
        p1.gold = 195;
        CHECK(e.buyLandPlot(1, 2, msg), "plot 2 refused with exactly 195 G: " << msg);
        CHECK(p1.gold == 0 && plots[1].isPurchased && p1.landTier == 2, "purchase: gold " << p1.gold << " tier " << p1.landTier);
        p1.gold = 1000;
        CHECK(!e.buyLandPlot(1, 2, msg) && p1.gold == 1000 && p1.landTier == 2, "plot 2 bought twice");

        // Nobody can buy the other side's plots, and unknown ids are refused
        CHECK(!e.buyLandPlot(1, 14, msg) && p1.gold == 1000 && !plots[13].isPurchased, "P1 bought P2's plot 14");
        p2.gold = 1000;
        CHECK(!e.buyLandPlot(2, 3, msg) && p2.gold == 1000 && !plots[2].isPurchased, "P2 bought P1's plot 3");
        const int badIds[3] = { 0, 25, -1 };
        for (int id : badIds) CHECK(!e.buyLandPlot(1, id, msg), "plot id " << id << " bought");
        CHECK(p1.gold == 1000 && p2.gold == 1000, "refused purchases spent gold");
    }

    // "+ КУПИ ЗЕМЯ" buys the cheapest plot left; both players get the same price sequence
    const int expectedOrder[2][11] = { { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 }, { 14, 13, 18, 17, 16, 21, 20, 19, 24, 23, 22 } };
    std::vector<int> paidBy[2];
    for (int player = 1; player <= 2; ++player) {
        GameEngine e;
        e.init(1600.0f, 900.0f);
        PlayerEconomy& econ = e.getPlayerEconomyMut(player);
        econ.gold = kDocLandTotal;
        std::string msg;
        for (int k = 0; k < 11; ++k) {
            const int goldBefore = econ.gold;
            REQUIRE(e.buyNextLandTier(player, msg), "P" << player << " purchase " << k + 1 << " refused: " << msg);
            const LandPlot& plot = e.getLandPlots()[expectedOrder[player - 1][k] - 1];
            CHECK(plot.isPurchased, "P" << player << " purchase " << k + 1 << " should be plot " << plot.id);
            CHECK(goldBefore - econ.gold == plot.costGold, "P" << player << " paid " << goldBefore - econ.gold << " for plot " << plot.id);
            paidBy[player - 1].push_back(goldBefore - econ.gold);
        }
        CHECK(econ.gold == 0 && econ.landTier == 12, "P" << player << " after buying all land: gold " << econ.gold << " tier " << econ.landTier);
        econ.gold = 100000;
        CHECK(!e.buyNextLandTier(player, msg) && econ.gold == 100000, "P" << player << " bought a 13th plot");
        int otherOwned = 0;
        for (const LandPlot& plot : e.getLandPlots()) {
            if (plot.playerOwner != player && plot.isPurchased) ++otherOwned;
        }
        CHECK(otherOwned == 1, "P" << player << "'s purchases changed the other side (" << otherOwned << " owned)");
        CHECK(std::is_sorted(paidBy[player - 1].begin(), paidBy[player - 1].end()), "P" << player << " did not buy the cheapest plot first");
    }
    CHECK(paidBy[0] == paidBy[1], "the two players pay different price sequences");
    endGroup();
}

// ---------------------------------------------------------------------------
// Plot / slot grid
// ---------------------------------------------------------------------------
void testGridMapping() {
    beginGroup("Plot and slot grid for both players");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    const std::vector<LandPlot>& plots = e.getLandPlots();

    // Plots: each side's plots stay on its own half of the 1600 px map and never overlap
    for (size_t i = 0; i < plots.size(); ++i) {
        const sf::FloatRect& b = plots[i].bounds;
        if (plots[i].playerOwner == 1) {
            CHECK(b.position.x + b.size.x < 800.0f, "P1 plot " << plots[i].id << " crosses into the east half");
        } else {
            CHECK(b.position.x > 800.0f, "P2 plot " << plots[i].id << " crosses into the west half");
        }
        for (size_t j = i + 1; j < plots.size(); ++j) {
            const sf::FloatRect& o = plots[j].bounds;
            bool overlap = b.position.x < o.position.x + o.size.x && o.position.x < b.position.x + b.size.x &&
                           b.position.y < o.position.y + o.size.y && o.position.y < b.position.y + b.size.y;
            CHECK(!overlap, "plots " << plots[i].id << " and " << plots[j].id << " overlap");
        }
    }

    for (int player = 1; player <= 2; ++player) {
        const sf::Vector2f s00 = e.getGridSlot(player, 0, 0);
        const float cellW = e.getGridSlot(player, 1, 0).x - s00.x;
        const float cellH = e.getGridSlot(player, 0, 1).y - s00.y;
        const float hx = cellW * 0.5f - 1.0f;
        const float hy = cellH * 0.5f - 1.0f;
        const sf::Vector2f offsets[5] = { { 0.0f, 0.0f }, { -hx, -hy }, { hx, -hy }, { -hx, hy }, { hx, hy } };
        std::vector<sf::Vector2f> slots;
        int slotsPerPlot[12] = { 0 };
        int badPlot = 0, badHolders = 0, badRoundTrip = 0, badSnap = 0, badMirror = 0, badRiver = 0;

        for (int row = 0; row < 12; ++row) {
            for (int col = 0; col < 9; ++col) {
                const sf::Vector2f s = e.getGridSlot(player, col, row);
                const int plotIndex = (row / 3) * 3 + col / 3;
                const LandPlot& plot = plots[(player - 1) * 12 + plotIndex];
                if (plot.bounds.contains(s)) {
                    ++slotsPerPlot[plotIndex];
                } else {
                    ++badPlot;
                }
                int holders = 0;
                for (const LandPlot& any : plots) holders += any.bounds.contains(s) ? 1 : 0;
                if (holders != 1) ++badHolders;

                int c = -1, r = -1;
                e.getClosestGridIndex(player, s, c, r);
                if (c != col || r != row) ++badRoundTrip;
                for (const sf::Vector2f& off : offsets) {
                    if (e.snapToBuildingGrid(player, s + off) != s) ++badSnap;
                }

                // P2's grid mirrors P1's around the map centre (x = 800)
                if (player == 1) {
                    const sf::Vector2f m = e.getGridSlot(2, 8 - col, row);
                    if (!near(m.x, 1600.0f - s.x, 1e-3f) || m.y != s.y) ++badMirror;
                }
                // Hydro: the river bank is the plot column next to the city (P1 east column, P2 west column)
                const bool riverColumn = (player == 1) ? (col / 3 == 2) : (col / 3 == 0);
                if (e.isRiverBankSlot(player, s) != riverColumn) ++badRiver;
                slots.push_back(s);
            }
        }
        CHECK(badPlot == 0, "P" << player << ": " << badPlot << " slots outside their plot");
        CHECK(badHolders == 0, "P" << player << ": " << badHolders << " slots inside no plot or two plots");
        CHECK(badRoundTrip == 0, "P" << player << ": " << badRoundTrip << " slots do not map back to their column/row");
        CHECK(badSnap == 0, "P" << player << ": " << badSnap << " points inside a cell snap to another slot");
        CHECK(badMirror == 0, "P2 grid is not the mirror image of P1's (" << badMirror << " slots)");
        CHECK(badRiver == 0, "P" << player << ": " << badRiver << " slots with the wrong river-bank flag");
        for (int i = 0; i < 12; ++i) {
            CHECK(slotsPerPlot[i] == Balance::SLOTS_PER_PLOT, "P" << player << " plot #" << i << " holds " << slotsPerPlot[i] << " slots");
        }

        // 108 distinct slots, all further apart than the 16 px building collision distance
        float minDist = 1e9f;
        for (size_t i = 0; i < slots.size(); ++i) {
            for (size_t j = i + 1; j < slots.size(); ++j) {
                minDist = std::min(minDist, std::hypot(slots[i].x - slots[j].x, slots[i].y - slots[j].y));
            }
        }
        CHECK(slots.size() == 108u && minDist > 16.0f, "P" << player << ": " << slots.size() << " slots, closest pair " << minDist << " px");

        // Out-of-range columns/rows clamp to the grid edge
        CHECK(e.getGridSlot(player, -3, -7) == e.getGridSlot(player, 0, 0), "P" << player << " negative index not clamped");
        CHECK(e.getGridSlot(player, 50, 50) == e.getGridSlot(player, 8, 11), "P" << player << " large index not clamped");
    }
    endGroup();
}

// ---------------------------------------------------------------------------
// Placement rules
// ---------------------------------------------------------------------------
// Every building stands inside a purchased plot of its own player
int buildingsOffOwnLand(const GameEngine& e) {
    int bad = 0;
    for (const PlacedBuilding& b : e.getBuildings()) {
        bool ok = false;
        for (const LandPlot& plot : e.getLandPlots()) {
            if (plot.playerOwner == b.playerOwner && plot.isPurchased && plot.bounds.contains(b.position)) ok = true;
        }
        bad += ok ? 0 : 1;
    }
    return bad;
}

void testPlacementRules() {
    beginGroup("Placement rules: occupied slot, unowned land, night, lamp");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    giveResources(e, 1, 1000);
    giveResources(e, 2, 1000);
    std::string reason, msg;
    REQUIRE(e.isDaylight(), "the match should start in daylight");

    // Occupied slot: blocked for every type, also when the point is elsewhere inside the same cell
    const BuildingType types[4] = { BuildingType::SOLAR_PANEL, BuildingType::WIND_TURBINE, BuildingType::BATTERY, BuildingType::LAMP };
    for (int player = 1; player <= 2; ++player) {
        const int plot = (player == 1) ? 1 : 15;
        const sf::Vector2f taken = slotOf(e, player, plot, 4);
        place(e, player, BuildingType::SOLAR_PANEL, taken);
        for (BuildingType t : types) {
            CHECK(!e.canPlaceBuilding(player, t, taken, reason), "P" << player << " type " << static_cast<int>(t) << " allowed on an occupied slot");
            CHECK(has(reason, "ВЕЧЕ ИМА СГРАДА"), "unexpected reason: " << reason);
        }
        CHECK(!e.canPlaceBuilding(player, BuildingType::WIND_TURBINE, taken + sf::Vector2f(6.0f, -5.0f), reason),
              "P" << player << ": a point inside the occupied cell was accepted");
        const std::vector<int> before = stocksOf(e.getPlayerEconomy(player));
        const size_t count = e.getBuildings().size();
        CHECK(!e.placeBuilding(player, BuildingType::WIND_TURBINE, taken, msg), "P" << player << " built on an occupied slot");
        CHECK(stocksOf(e.getPlayerEconomy(player)) == before && e.getBuildings().size() == count, "refused placement still charged or built");
        CHECK(e.canPlaceBuilding(player, BuildingType::WIND_TURBINE, slotOf(e, player, plot, 5), reason),
              "P" << player << ": the free neighbour cell was refused: " << reason);
    }

    // Unowned land: refused with the plot price, allowed once the plot is bought
    for (int player = 1; player <= 2; ++player) {
        const int plotId = (player == 1) ? 2 : 14; // the cheapest plot for sale (195 G)
        const sf::Vector2f pos = slotOf(e, player, plotId, 4);
        CHECK(!e.canPlaceBuilding(player, BuildingType::SOLAR_PANEL, pos, reason), "P" << player << " allowed on unowned plot " << plotId);
        CHECK(has(reason, "НЕПРИТЕЖАВАНА ЗЕМЯ") && has(reason, "195"), "unexpected reason: " << reason);
        const std::vector<int> before = stocksOf(e.getPlayerEconomy(player));
        CHECK(!e.placeBuilding(player, BuildingType::SOLAR_PANEL, pos, msg), "P" << player << " built on unowned land");
        CHECK(stocksOf(e.getPlayerEconomy(player)) == before, "refused placement on unowned land charged resources");
        buyPlot(e, player, plotId);
        CHECK(e.canPlaceBuilding(player, BuildingType::SOLAR_PANEL, pos, reason), "P" << player << " refused on a bought plot: " << reason);
    }

    // A request on the other side never puts a building there (positions snap to the requester's grid)
    e.placeBuilding(1, BuildingType::SOLAR_PANEL, slotOf(e, 2, 15, 0), msg);
    e.placeBuilding(2, BuildingType::SOLAR_PANEL, slotOf(e, 1, 1, 0), msg);
    CHECK(buildingsOffOwnLand(e) == 0, buildingsOffOwnLand(e) << " buildings stand outside their owner's purchased land");

    // Missing one resource of the recipe
    {
        GameEngine poor;
        poor.init(1600.0f, 900.0f);
        giveResources(poor, 1, 1000);
        poor.getPlayerEconomyMut(1).silicon = Balance::SOLAR_PANEL.siliconCost - 1;
        CHECK(!poor.canPlaceBuilding(1, BuildingType::SOLAR_PANEL, slotOf(poor, 1, 1, 0), reason), "solar panel built without enough silicon");
        CHECK(has(reason, "НЕДОСТИГ НА РЕСУРСИ"), "unexpected reason: " << reason);
    }

    // Prepare the night: P1 gets a wind turbine (power for a lamp) and a far plot, P2 only a solar panel
    place(e, 1, BuildingType::WIND_TURBINE, slotOf(e, 1, 1, 0));
    buyPlot(e, 1, 12);
    advanceToHour(e, 21.0f);
    REQUIRE(!e.isDaylight() && near(e.getHour24(), 21.0f, 0.01f), "21:00 in spring should be night, hour " << e.getHour24());
    giveResources(e, 1, 1000);
    giveResources(e, 2, 1000);

    // Night without light: nothing but a lamp may be built
    const sf::Vector2f nearLamp = slotOf(e, 1, 1, 2);
    const sf::Vector2f lampSlot = slotOf(e, 1, 1, 8);
    const BuildingType nonLamps[3] = { BuildingType::SOLAR_PANEL, BuildingType::WIND_TURBINE, BuildingType::BATTERY };
    for (BuildingType t : nonLamps) {
        CHECK(!e.canPlaceBuilding(1, t, nearLamp, reason), "type " << static_cast<int>(t) << " allowed at night without light");
        CHECK(has(reason, "НОЩЕН МРАК"), "unexpected reason: " << reason);
    }
    {
        const std::vector<int> before = stocksOf(e.getPlayerEconomy(1));
        CHECK(!e.placeBuilding(1, BuildingType::SOLAR_PANEL, nearLamp, msg), "built in the dark");
        CHECK(stocksOf(e.getPlayerEconomy(1)) == before, "refused night placement charged resources");
    }

    // Lamp exception: a lamp may be placed in the dark; once powered it lights a 150 px circle
    CHECK(e.canPlaceBuilding(1, BuildingType::LAMP, lampSlot, reason), "lamp refused at night: " << reason);
    place(e, 1, BuildingType::LAMP, lampSlot);
    e.update(0.25f);
    const PlacedBuilding* lamp = findBuilding(e, 1, BuildingType::LAMP);
    REQUIRE(lamp != nullptr, "P1 lamp missing");
    CHECK(lamp->lightRadius == 150.0f && lamp->currentOutputMW == -GameEngine::LAMP_POWER_MW,
          "P1 lamp powered by the turbine should shine: radius " << lamp->lightRadius << " output " << lamp->currentOutputMW);
    CHECK(e.isAreaIlluminated(1, nearLamp), "the slot next to a powered lamp is dark");
    CHECK(e.canPlaceBuilding(1, BuildingType::SOLAR_PANEL, nearLamp, reason), "refused inside the lamp light: " << reason);
    place(e, 1, BuildingType::SOLAR_PANEL, nearLamp);

    // Outside the light circle it is still night
    const sf::Vector2f farSlot = slotOf(e, 1, 12, 4);
    REQUIRE(std::hypot(farSlot.x - lampSlot.x, farSlot.y - lampSlot.y) > 150.0f, "test slot is inside the lamp radius");
    CHECK(!e.canPlaceBuilding(1, BuildingType::SOLAR_PANEL, farSlot, reason) && has(reason, "НОЩЕН МРАК"),
          "allowed outside the lamp light: " << reason);

    // An unpowered lamp gives no light (P2's solar panel produces nothing at night)
    place(e, 2, BuildingType::LAMP, slotOf(e, 2, 15, 8));
    e.update(0.25f);
    const PlacedBuilding* darkLamp = findBuilding(e, 2, BuildingType::LAMP);
    REQUIRE(darkLamp != nullptr, "P2 lamp missing");
    CHECK(darkLamp->lightRadius == 0.0f, "unpowered lamp still shines, radius " << darkLamp->lightRadius);
    CHECK(!e.isAreaIlluminated(2, slotOf(e, 2, 15, 2)), "an unpowered lamp lights the area");
    CHECK(!e.canPlaceBuilding(2, BuildingType::SOLAR_PANEL, slotOf(e, 2, 15, 2), reason) && has(reason, "НОЩЕН МРАК"),
          "built next to an unpowered lamp: " << reason);

    // Demolition works at night without light (docs/GAMEPLAY.md §5, rule 5)
    CHECK(e.removeBuilding(2, slotOf(e, 2, 15, 4), msg), "demolition refused at night: " << msg);
    CHECK(buildingsOffOwnLand(e) == 0, "a building stands outside its owner's purchased land");
    endGroup();
}

// ---------------------------------------------------------------------------
// Demolition refund
// ---------------------------------------------------------------------------
void testDemolitionRefund() {
    beginGroup("Demolition refund");
    CHECK(Balance::DEMOLISH_REFUND_FRACTION == 0.5f, "refund fraction " << Balance::DEMOLISH_REFUND_FRACTION << ", docs say 50%");
    for (int i = 0; i < 5; ++i) {
        const BuildCase& bc = kBuildCases[i];
        GameEngine e;
        e.init(1600.0f, 900.0f);
        if (bc.plotId != 1) buyPlot(e, 1, bc.plotId);
        giveResources(e, 1, 1000);
        const BuildingCost cost = e.getBuildingCost(bc.type);
        const sf::Vector2f pos = slotOf(e, 1, bc.plotId, 4);
        place(e, 1, bc.type, pos);
        const PlayerEconomy placed = e.getPlayerEconomy(1);

        // Both ways to demolish: the API call and the Demolish tool of the build menu
        std::string msg;
        const bool viaTool = (i % 2 == 1);
        const bool ok = viaTool ? e.placeBuilding(1, BuildingType::DEMOLISH, pos, msg) : e.removeBuilding(1, pos, msg);
        REQUIRE(ok, "could not demolish the " << bc.name << ": " << msg);
        const PlayerEconomy& after = e.getPlayerEconomy(1);
        for (int r = 0; r < 6; ++r) {
            const int refund = after.*kRecipeStock[r] - placed.*kRecipeStock[r];
            CHECK(refund == bc.refund[r], bc.name << " resource #" << r << ": refund " << refund << ", docs say " << bc.refund[r]);
            CHECK(refund == cost.*kRecipeCost[r] / 2, bc.name << " resource #" << r << ": refund " << refund << " for a cost of " << cost.*kRecipeCost[r]);
        }
        CHECK(after.gold == placed.gold && after.money == placed.money && after.ore == placed.ore, bc.name << ": demolition changed gold, money or ore");
        CHECK(has(msg, "50%"), bc.name << " message does not show 50%: " << msg);
        CHECK(e.getBuildings().empty(), bc.name << " still standing");
        std::string reason;
        CHECK(e.canPlaceBuilding(1, bc.type, pos, reason), bc.name << ": the freed slot is still blocked: " << reason);
    }

    // Build and demolish over and over: every cycle loses (cost - refund), it never makes resources
    GameEngine e;
    e.init(1600.0f, 900.0f);
    giveResources(e, 1, 1000);
    const BuildingCost cost = e.getBuildingCost(BuildingType::WIND_TURBINE);
    const sf::Vector2f pos = slotOf(e, 1, 1, 0);
    for (int cycle = 0; cycle < 10; ++cycle) {
        const PlayerEconomy before = e.getPlayerEconomy(1);
        place(e, 1, BuildingType::WIND_TURBINE, pos);
        std::string msg;
        REQUIRE(e.removeBuilding(1, pos, msg), "cycle " << cycle << ": " << msg);
        for (int r = 0; r < 6; ++r) {
            const int loss = before.*kRecipeStock[r] - e.getPlayerEconomy(1).*kRecipeStock[r];
            const int expectedLoss = cost.*kRecipeCost[r] - cost.*kRecipeCost[r] / 2;
            CHECK(loss == expectedLoss, "cycle " << cycle << " resource #" << r << ": lost " << loss << ", expected " << expectedLoss);
        }
    }

    // Only the owner can demolish, and only the building in the cell under the cursor
    place(e, 1, BuildingType::WIND_TURBINE, pos);
    std::string msg;
    CHECK(!e.removeBuilding(2, pos, msg), "P2 demolished P1's building");
    CHECK(!e.removeBuilding(1, slotOf(e, 1, 1, 1), msg), "demolishing the empty neighbour cell removed the turbine");
    CHECK(e.getBuildings().size() == 1u, "buildings left: " << e.getBuildings().size());
    endGroup();
}

// ---------------------------------------------------------------------------
// Weather report -> weather type
// ---------------------------------------------------------------------------
typedef std::vector<std::string> Report;

void testWeatherMapping() {
    beginGroup("Weather report to weather type, per season");
    struct Case {
        Report report;
        WeatherType expected;
        const char* what;
    };
    const Case cases[] = {
        { { "cloudy", "thunder_storm", "left", "80.0" }, WeatherType::STORMY, "storm wins over wind" },
        { { "cloudy", "rain", "right", "95.0" }, WeatherType::RAINY, "rain wins over wind" },
        { { "cloudy", "snow", "left", "60.8" }, WeatherType::SNOWY, "snow wins over wind" },
        { { "cloudy", "hail", "none", "0" }, WeatherType::SNOWY, "hail counts as snow" },
        { { "clear", "clear", "left", "19.000000" }, WeatherType::WINDY, "10 knots (19.0) is windy" },
        { { "clear", "clear", "left", "18.000000" }, WeatherType::SUNNY, "exactly 18.0 is not above the threshold" },
        { { "cloudy", "clear", "right", "17.100000" }, WeatherType::CLOUDY, "9 knots (17.1) is not windy" },
        { { "cloudy", "clear", "right", "18.050000" }, WeatherType::WINDY, "just above 18.0 is windy" },
        { { "cloudy", "clear", "none", "0" }, WeatherType::CLOUDY, "dry overcast" },
        { { "clear", "clear", "none", "0" }, WeatherType::SUNNY, "clear sky" },
        { { "cloudy", "clear", "left", "fast" }, WeatherType::CLOUDY, "unreadable wind speed is ignored" },
        { { "clear" }, WeatherType::SUNNY, "short report" },
        { {}, WeatherType::SUNNY, "empty report" },
    };
    for (const Case& c : cases) {
        const WeatherType got = WeatherSystem::reportToWeatherType(c.report);
        CHECK(got == c.expected, c.what << ": got " << idx(got) << ", expected " << idx(c.expected));
    }

    // The season keywords the report understands ("fall", not "autumn")
    const char* keys[4] = { "spring", "summer", "fall", "winter" };
    for (int s = 0; s < 4; ++s) {
        CHECK(std::strcmp(Balance::getSeasonWeatherKey(kSeasons[s]), keys[s]) == 0, "season " << s << " key " << Balance::getSeasonWeatherKey(kSeasons[s]));
    }
    seedRandom(testSeed());
    CHECK(weather_report("autumn") == (Report{ "clear", "clear", "none", "0" }), "unknown keyword should give the default clear report");

    // Sampled reports are well formed and follow the documented probabilities of each season
    const int samples = 6000;
    for (int s = 0; s < 4; ++s) {
        seedRandom(testSeed() + 101u * static_cast<unsigned int>(s));
        int counts[6] = { 0 };
        int malformed = 0;
        for (int i = 0; i < samples; ++i) {
            const Report r = weather_report(keys[s]);
            if (r.size() != 4u) {
                ++malformed;
                continue;
            }
            const bool cloudOk = (r[0] == "clear" || r[0] == "cloudy");
            const bool precipOk = (r[1] == "clear" || r[1] == "rain" || r[1] == "thunder_storm" || r[1] == "snow" || r[1] == "hail");
            const bool wetIsCloudy = (r[1] == "clear" || r[0] == "cloudy");
            const bool dirOk = (r[2] == "none" || r[2] == "left" || r[2] == "right");
            bool speedOk = ((r[2] == "none") == (r[3] == "0"));
            if (speedOk && r[3] != "0") {
                const double knots = std::atof(r[3].c_str()) / 1.9;
                speedOk = knots > 0.99 && knots < 50.01 && std::abs(knots - std::round(knots)) < 1e-6;
            }
            if (!(cloudOk && precipOk && wetIsCloudy && dirOk && speedOk)) ++malformed;
            ++counts[idx(WeatherSystem::reportToWeatherType(r))];
        }
        CHECK(malformed == 0, keys[s] << ": " << malformed << " malformed reports");
        for (int w = 0; w < 6; ++w) {
            const double pct = 100.0 * counts[w] / samples;
            const double doc = kDocWeatherPct[s][w];
            if (doc == 0.0) {
                CHECK(counts[w] == 0, keys[s] << ": weather " << w << " appeared " << counts[w] << " times, docs say never");
            } else {
                CHECK(std::abs(pct - doc) <= 2.5, keys[s] << ": weather " << w << " on " << pct << "% of days, docs say " << doc << "%");
            }
        }
    }

    // Same seed, same reports
    seedRandom(4242u);
    std::vector<Report> first;
    for (int i = 0; i < 50; ++i) first.push_back(weather_report("winter"));
    seedRandom(4242u);
    std::vector<Report> second;
    for (int i = 0; i < 50; ++i) second.push_back(weather_report("winter"));
    CHECK(first == second, "seedRandom does not make weather_report repeatable");
    endGroup();
}

// The engine rolls each sector's weather every day from the season of that day
void testEngineDailyWeather() {
    beginGroup("Daily weather in the engine follows the season");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    int days = 0;
    int distinct = 0;
    bool seen[6] = { false };
    while (e.getCityState().winner == 0) {
        ++days;
        const int day = e.getCurrentDay();
        const SeasonType season = e.getSeason();
        CHECK(season == Balance::getSeasonForDay(day), "day " << day << " season " << idx(season));
        CHECK(e.getSunriseHour() == kDocSunrise[idx(season)] && e.getSunsetHour() == kDocSunset[idx(season)],
              "day " << day << " sun times " << e.getSunriseHour() << "-" << e.getSunsetHour());
        for (int player = 1; player <= 2; ++player) {
            const WeatherType w = e.getPlayerWeather(player);
            CHECK(w != WeatherType::SNOWY || season == SeasonType::AUTUMN || season == SeasonType::WINTER,
                  "day " << day << " P" << player << ": snow in season " << idx(season));
            if (!seen[idx(w)]) {
                seen[idx(w)] = true;
                ++distinct;
            }
        }
        e.update(Balance::SECONDS_PER_DAY);
        REQUIRE(days <= Balance::FINAL_DAY, "the match did not end after day " << Balance::FINAL_DAY);
    }
    CHECK(days == Balance::FINAL_DAY, "days played " << days);
    CHECK(distinct >= 3, "only " << distinct << " kinds of weather in 20 days x 2 sectors: is the weather re-rolled?");
    endGroup();
}

// ---------------------------------------------------------------------------
// Generator curves
// ---------------------------------------------------------------------------
void testSolarCurve() {
    beginGroup("Solar curve by season and hour");
    for (SeasonType s : kSeasons) {
        CHECK(Balance::getSunriseHour(s) == kDocSunrise[idx(s)] && Balance::getSunsetHour(s) == kDocSunset[idx(s)],
              "season " << idx(s) << " sun " << Balance::getSunriseHour(s) << "-" << Balance::getSunsetHour(s));
        CHECK(Balance::getDaylightDuration(s) == kDocSunset[idx(s)] - kDocSunrise[idx(s)], "season " << idx(s) << " daylight length");
    }
    for (SeasonType s : kSeasons) {
        const float rise = Balance::getSunriseHour(s);
        const float set = Balance::getSunsetHour(s);
        const float noon = 0.5f * (rise + set);
        for (WeatherType w : kWeathers) {
            const float peak = WeatherSystem::getSolarMultiplier(w, noon, s);
            const float expectedPeak = kDocSolarWeather[idx(w)] * kDocSolarSeason[idx(s)];
            CHECK(near(peak, expectedPeak, 1e-4f), "season " << idx(s) << " weather " << idx(w) << " noon " << peak << ", expected " << expectedPeak);

            int negative = 0, lightAtNight = 0, darkAtDay = 0, wrongSlope = 0;
            float prev = 0.0f;
            for (int i = 0; i < 24 * 40; ++i) {
                const float h = (i + 0.5f) / 40.0f; // every 1.5 game-minutes, between the exact edges checked below
                const float v = WeatherSystem::getSolarMultiplier(w, h, s);
                const bool day = Balance::isDaylightAt(h, s);
                if (v < 0.0f) ++negative;
                if (!day && v != 0.0f) ++lightAtNight;
                if (day && h > rise && v <= 0.0f) ++darkAtDay;
                if (day && h > rise && ((h <= noon && v < prev) || (h > noon + 0.05f && v > prev))) ++wrongSlope;
                prev = v;
            }
            CHECK(negative == 0 && lightAtNight == 0 && darkAtDay == 0 && wrongSlope == 0,
                  "season " << idx(s) << " weather " << idx(w) << ": negative " << negative << ", light at night " << lightAtNight
                            << ", dark by day " << darkAtDay << ", wrong slope " << wrongSlope);
            // Symmetric around noon; the sun arc is 0 at sunrise (docs/GAMEPLAY.md §6)
            for (float t = 0.5f; t < 4.0f; t += 0.5f) {
                CHECK(near(WeatherSystem::getSolarMultiplier(w, rise + t, s), WeatherSystem::getSolarMultiplier(w, set - t, s), 1e-4f),
                      "season " << idx(s) << " weather " << idx(w) << " not symmetric at " << t << " h from the edges");
            }
            CHECK(WeatherSystem::getSolarMultiplier(w, rise, s) == 0.0f, "season " << idx(s) << " weather " << idx(w) << " at sunrise: "
                                                                                   << WeatherSystem::getSolarMultiplier(w, rise, s));
            // ... and at sunset, which is already night (isDaylightAt): exactly 0, never a negative output
            CHECK(WeatherSystem::getSolarMultiplier(w, set, s) == 0.0f, "season " << idx(s) << " weather " << idx(w) << " at sunset: "
                                                                                  << WeatherSystem::getSolarMultiplier(w, set, s));
        }
    }

    // Weather order at noon: sunny > windy > cloudy > rainy > snowy > stormy
    const WeatherType order[6] = { WeatherType::SUNNY, WeatherType::WINDY, WeatherType::CLOUDY,
                                   WeatherType::RAINY, WeatherType::SNOWY, WeatherType::STORMY };
    for (int i = 1; i < 6; ++i) {
        CHECK(WeatherSystem::getSolarMultiplier(order[i - 1], 12.5f, SeasonType::SPRING) > WeatherSystem::getSolarMultiplier(order[i], 12.5f, SeasonType::SPRING),
              "solar weather order broken at " << i);
    }
    // Daily solar energy: summer > spring > autumn > winter (longer days and a stronger peak)
    float energy[4] = { 0.0f };
    for (SeasonType s : kSeasons) {
        for (int i = 0; i < 24 * 40; ++i) energy[idx(s)] += WeatherSystem::getSolarMultiplier(WeatherType::SUNNY, (i + 0.5f) / 40.0f, s) / 40.0f;
    }
    CHECK(energy[1] > energy[0] && energy[0] > energy[2] && energy[2] > energy[3],
          "daily solar energy spring " << energy[0] << " summer " << energy[1] << " autumn " << energy[2] << " winter " << energy[3]);
    // Docs example: one panel on a sunny summer noon gives about 110 MW
    const float summerNoonMW = Balance::SOLAR_PANEL.basePowerMW * WeatherSystem::getSolarMultiplier(WeatherType::SUNNY, 13.0f, SeasonType::SUMMER);
    CHECK(near(summerNoonMW, 110.4f, 0.05f), "sunny summer noon panel " << summerNoonMW << " MW");
    endGroup();
}

void testWindAndHydro() {
    beginGroup("Wind and hydro multipliers");
    for (WeatherType w : kWeathers) {
        const float mod = kDocWindWeather[idx(w)];
        CHECK(near(WeatherSystem::getWindMultiplier(w, 15.0f), 1.15f * mod, 1e-4f), "weather " << idx(w) << " wind at 15:00");
        CHECK(near(WeatherSystem::getWindMultiplier(w, 3.0f), 0.85f * mod, 1e-4f), "weather " << idx(w) << " wind at 03:00");
        CHECK(near(WeatherSystem::getWindMultiplier(w, 9.0f), mod, 1e-4f) && near(WeatherSystem::getWindMultiplier(w, 21.0f), mod, 1e-4f),
              "weather " << idx(w) << " wind at 09:00 / 21:00");
        int outOfRange = 0;
        for (int i = 0; i < 24 * 40; ++i) {
            const float v = WeatherSystem::getWindMultiplier(w, i / 40.0f);
            if (v < 0.85f * mod - 1e-4f || v > 1.15f * mod + 1e-4f) ++outOfRange;
        }
        CHECK(outOfRange == 0, "weather " << idx(w) << ": wind leaves the 03:00-15:00 range " << outOfRange << " times");
        CHECK(near(WeatherSystem::getWindMultiplier(w, 23.999f), WeatherSystem::getWindMultiplier(w, 0.0f), 1e-3f), "weather " << idx(w) << ": wind jumps at midnight");

        // Hydro depends on the weather only
        const float hydro = WeatherSystem::getHydroMultiplier(w);
        CHECK(hydro == kDocHydroWeather[idx(w)], "weather " << idx(w) << " hydro " << hydro << ", docs say " << kDocHydroWeather[idx(w)]);
    }
    // Wind: stormy > windy > rainy = snowy > sunny = cloudy; hydro: stormy > rainy > windy = snowy > cloudy > sunny
    const float windAtNoon[6] = { WeatherSystem::getWindMultiplier(WeatherType::SUNNY, 12.0f), WeatherSystem::getWindMultiplier(WeatherType::WINDY, 12.0f),
                            WeatherSystem::getWindMultiplier(WeatherType::RAINY, 12.0f), WeatherSystem::getWindMultiplier(WeatherType::STORMY, 12.0f),
                            WeatherSystem::getWindMultiplier(WeatherType::SNOWY, 12.0f), WeatherSystem::getWindMultiplier(WeatherType::CLOUDY, 12.0f) };
    CHECK(windAtNoon[3] > windAtNoon[1] && windAtNoon[1] > windAtNoon[2] && windAtNoon[2] == windAtNoon[4] &&
              windAtNoon[4] > windAtNoon[0] && windAtNoon[0] == windAtNoon[5],
          "wind weather order broken");
    CHECK(WeatherSystem::getHydroMultiplier(WeatherType::STORMY) > WeatherSystem::getHydroMultiplier(WeatherType::RAINY) &&
              WeatherSystem::getHydroMultiplier(WeatherType::RAINY) > WeatherSystem::getHydroMultiplier(WeatherType::WINDY) &&
              WeatherSystem::getHydroMultiplier(WeatherType::WINDY) == WeatherSystem::getHydroMultiplier(WeatherType::SNOWY) &&
              WeatherSystem::getHydroMultiplier(WeatherType::SNOWY) > WeatherSystem::getHydroMultiplier(WeatherType::CLOUDY) &&
              WeatherSystem::getHydroMultiplier(WeatherType::CLOUDY) > WeatherSystem::getHydroMultiplier(WeatherType::SUNNY),
          "hydro weather order broken");
    endGroup();
}

// Placed generators produce exactly base x curve for the sector's weather, the hour and the season
void testEngineGeneratorOutputs() {
    beginGroup("Generators in the engine follow the curves");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    buyPlot(e, 1, 3);  // P1 river bank
    buyPlot(e, 2, 13); // P2 river bank
    for (int player = 1; player <= 2; ++player) {
        giveResources(e, player, 1000);
        const int start = (player == 1) ? 1 : 15;
        const int river = (player == 1) ? 3 : 13;
        place(e, player, BuildingType::WIND_TURBINE, slotOf(e, player, start, 0));
        place(e, player, BuildingType::SOLAR_PANEL, slotOf(e, player, start, 1));
        place(e, player, BuildingType::HYDRO_PLANT, slotOf(e, player, river, 4));
    }
    // Samples at hh:30 for 24 h (never on the 06:00 day change, when the new weather is rolled)
    e.update(0.5f * Balance::SECONDS_PER_DAY / 24.0f);
    int mismatches = 0, nightSolar = 0, nightWindHydro = 0, wrongTotal = 0, samples = 0;
    for (int hour = 0; hour < 24; ++hour) {
        const float h = e.getHour24();
        for (int player = 1; player <= 2; ++player) {
            const WeatherType w = e.getPlayerWeather(player);
            float sum = 0.0f;
            for (const PlacedBuilding& b : e.getBuildings()) {
                if (b.playerOwner != player) continue;
                float expected = 0.0f;
                if (b.type == BuildingType::SOLAR_PANEL) {
                    expected = Balance::SOLAR_PANEL.basePowerMW * WeatherSystem::getSolarMultiplier(w, h, e.getSeason());
                    if (!e.isDaylight() && b.currentOutputMW != 0.0f) ++nightSolar;
                } else if (b.type == BuildingType::WIND_TURBINE) {
                    expected = Balance::WIND_TURBINE.basePowerMW * WeatherSystem::getWindMultiplier(w, h);
                    if (!e.isDaylight() && b.currentOutputMW <= 0.0f) ++nightWindHydro;
                } else if (b.type == BuildingType::HYDRO_PLANT) {
                    expected = Balance::HYDRO_PLANT.basePowerMW * WeatherSystem::getHydroMultiplier(w);
                    if (!e.isDaylight() && b.currentOutputMW <= 0.0f) ++nightWindHydro;
                }
                if (!near(b.currentOutputMW, expected, 1e-3f)) ++mismatches;
                sum += b.currentOutputMW;
                ++samples;
            }
            // Grace period, no lamps or batteries: everything goes to the city
            if (e.getPlayerEconomy(player).energyMW != static_cast<int>(std::lround(sum))) ++wrongTotal;
        }
        e.update(Balance::SECONDS_PER_DAY / 24.0f);
    }
    CHECK(samples == 24 * 6, "samples " << samples);
    CHECK(mismatches == 0, mismatches << " generator outputs differ from base x multiplier");
    CHECK(nightSolar == 0, "solar panels produced at night " << nightSolar << " times");
    CHECK(nightWindHydro == 0, "wind or hydro stopped at night " << nightWindHydro << " times");
    CHECK(wrongTotal == 0, "player power differs from the sum of the generators " << wrongTotal << " times");
    endGroup();
}

// ---------------------------------------------------------------------------
// Grace period and seasons
// ---------------------------------------------------------------------------
void testGracePeriod() {
    beginGroup("Grace period demand");
    CHECK(Balance::GRACE_PERIOD_DAYS == 2 && Balance::STARTING_CITY_DEMAND_MW == 30 && Balance::DAILY_DEMAND_INCREASE_MW == 15,
          "grace " << Balance::GRACE_PERIOD_DAYS << " days, start " << Balance::STARTING_CITY_DEMAND_MW << " MW, +" << Balance::DAILY_DEMAND_INCREASE_MW);
    {
        GameEngine e;
        e.init(1600.0f, 900.0f);
        CHECK(e.isGracePeriod() && e.getCityState().cityEnergyDemand == 0, "day 1 must be a grace day with 0 MW demand");
        buildWindFarm(e, 1);

        // The gold dividend in the grace period is capped at the day-3 demand (30 MW)
        const int gold1 = e.getPlayerEconomy(1).gold;
        const int gold2 = e.getPlayerEconomy(2).gold;
        e.update(10.0f);
        const int perSecond = Balance::calculateGoldDividend(Balance::STARTING_CITY_DEMAND_MW, Balance::STARTING_CITY_DEMAND_MW);
        CHECK(e.getPlayerEconomy(1).energyMW > Balance::STARTING_CITY_DEMAND_MW, "wind farm only gives " << e.getPlayerEconomy(1).energyMW << " MW");
        CHECK(e.getPlayerEconomy(1).gold - gold1 == 10 * perSecond, "P1 grace dividend " << e.getPlayerEconomy(1).gold - gold1 << " G in 10 s");
        CHECK(e.getPlayerEconomy(2).gold == gold2, "P2 got a dividend for 0 MW");

        // Days 1 and 2 end without any territory change, although P1 supplies everything
        for (int ended = 1; ended <= 2; ++ended) {
            runToNextDay(e);
            const CityConquestState& city = e.getCityState();
            CHECK(e.getCurrentDay() == ended + 1, "day " << e.getCurrentDay());
            CHECK(city.p1CityShare == 0.5f, "share after grace day " << ended << ": " << city.p1CityShare);
            CHECK(has(city.lastCutMessage, "ГРАТИС"), "day " << ended << " message: " << city.lastCutMessage);
        }
        CHECK(!e.isGracePeriod() && e.getCityState().cityEnergyDemand == Balance::STARTING_CITY_DEMAND_MW,
              "day 3 demand " << e.getCityState().cityEnergyDemand);

        // Day 3 is the first judged day: P1 powered the city, P2 delivered nothing
        runToNextDay(e);
        CHECK(near(e.getCityState().p1CityShare, 0.5f + Balance::MAX_DAILY_CITY_SHIFT, 1e-5f), "share after day 3: " << e.getCityState().p1CityShare);
        CHECK(e.getCityState().cityEnergyDemand == docDemandForDay(4), "day 4 demand " << e.getCityState().cityEnergyDemand);
    }

    // Demand of every day of a match (nobody builds, so it runs to the end of day 20)
    GameEngine e;
    e.init(1600.0f, 900.0f);
    int days = 0;
    while (e.getCityState().winner == 0) {
        ++days;
        const int day = e.getCurrentDay();
        CHECK(e.getCityState().cityEnergyDemand == docDemandForDay(day), "day " << day << " demand " << e.getCityState().cityEnergyDemand
                                                                                << ", docs say " << docDemandForDay(day));
        CHECK(e.isGracePeriod() == (day <= Balance::GRACE_PERIOD_DAYS), "day " << day << " grace flag " << e.isGracePeriod());
        e.update(Balance::SECONDS_PER_DAY);
        REQUIRE(days <= Balance::FINAL_DAY, "the match did not end");
    }
    CHECK(days == Balance::FINAL_DAY, "days " << days);
    endGroup();
}

void testSeasonRollover() {
    beginGroup("Season rollover every 5 days");
    CHECK(Balance::DAYS_PER_SEASON == 5, "days per season " << Balance::DAYS_PER_SEASON);
    for (int day = 1; day <= 25; ++day) {
        // docs: spring 1-5, summer 6-10, autumn 11-15, winter 16-20, then it starts over
        const SeasonType expected = (day <= 5 || day >= 21) ? SeasonType::SPRING
                                    : (day <= 10)           ? SeasonType::SUMMER
                                    : (day <= 15)           ? SeasonType::AUTUMN
                                                            : SeasonType::WINTER;
        CHECK(Balance::getSeasonForDay(day) == expected, "day " << day << " season " << idx(Balance::getSeasonForDay(day)));
    }
    // The season changes at the midnight before the first day of the new season
    for (int firstDay = 6; firstDay <= 21; firstDay += 5) {
        const float midnight = (firstDay - 2) * Balance::SECONDS_PER_DAY + Balance::gameSecondsAtHour(24.0f);
        CHECK(Balance::getSeasonAtGameSeconds(midnight - 0.01f) == Balance::getSeasonForDay(firstDay - 1) &&
                  Balance::getSeasonAtGameSeconds(midnight + 0.01f) == Balance::getSeasonForDay(firstDay),
              "season switch for day " << firstDay << " is not at midnight (" << midnight << " s)");
    }

    // The whole match in 0.25 s steps: when the season changes, and when the sun rises and sets
    GameEngine e;
    e.init(1600.0f, 900.0f);
    SeasonType prevSeason = e.getSeason();
    bool prevLight = e.isDaylight();
    std::vector<int> changeDays;
    int sunrises = 0, sunsets = 0, badSeason = 0, badSunrise = 0, badSunset = 0, steps = 0;
    while (e.getCityState().winner == 0) {
        e.update(0.25f);
        REQUIRE(++steps < 10000, "the match did not end");
        if (e.getCityState().winner != 0) break;
        const int day = e.getCurrentDay();
        const float h = e.getHour24();
        const SeasonType s = e.getSeason();
        // Before 06:00 the clock already belongs to the next calendar day
        if (std::abs(h - 6.0f) > 0.01f) {
            const SeasonType expected = Balance::getSeasonForDay(h < 6.0f ? day + 1 : day);
            if (s != expected) ++badSeason;
        }
        if (s != prevSeason) {
            changeDays.push_back(day);
            CHECK(h < 0.1f || h > 23.9f, "season changed on day " << day << " at " << h << " h, not at midnight");
        }
        const bool light = e.isDaylight();
        if (light && !prevLight) {
            ++sunrises;
            if (!(h >= Balance::getSunriseHour(s) && h < Balance::getSunriseHour(s) + 0.07f)) ++badSunrise;
        }
        if (!light && prevLight) {
            ++sunsets;
            if (!(h >= Balance::getSunsetHour(s) && h < Balance::getSunsetHour(s) + 0.07f)) ++badSunset;
        }
        prevSeason = s;
        prevLight = light;
    }
    CHECK(badSeason == 0, badSeason << " steps with the wrong season");
    // The last change is spring for day 21: the match ends at 06:00 that day, while it is still dark
    CHECK(changeDays == (std::vector<int>{ 5, 10, 15, 20 }), changeDays.size() << " season changes, expected 4 (nights after days 5, 10, 15, 20)");
    // The match starts at 08:00 (after sunrise) and ends at 06:00 of day 21 (before the winter sunrise)
    CHECK(sunrises == Balance::FINAL_DAY - 1 && sunsets == Balance::FINAL_DAY, "sunrises " << sunrises << ", sunsets " << sunsets);
    CHECK(badSunrise == 0 && badSunset == 0, "daylight toggled away from the season's sunrise/sunset: " << badSunrise << " / " << badSunset);
    endGroup();
}

// ---------------------------------------------------------------------------
// Restart
// ---------------------------------------------------------------------------
std::string economyDiff(const PlayerEconomy& a, const PlayerEconomy& b) {
    std::string d;
#define CMP(field) if (!(a.field == b.field)) d += std::string(" ") + #field
    CMP(money); CMP(gold); CMP(silver); CMP(iron); CMP(coal); CMP(copper); CMP(silicon); CMP(wood); CMP(ore);
    CMP(energyMW); CMP(landTier); CMP(cityInfluence); CMP(selectedBuilding); CMP(lastPlacedBuilding);
    for (int i = 0; i < 8; ++i) CMP(mineLevels[i]);
    return d;
}

std::string cityDiff(const CityConquestState& a, const CityConquestState& b) {
    std::string d;
    CMP(cityEnergyDemand); CMP(p1CityShare); CMP(p1DailyDelivered); CMP(p2DailyDelivered); CMP(dailySeconds);
    CMP(lastCutMessage); CMP(winner);
    return d;
}
#undef CMP

void recordState(const GameEngine& e, std::vector<double>& out) {
    out.push_back(e.getCurrentDay());
    out.push_back(e.getHour24());
    out.push_back(idx(e.getSeason()));
    for (int player = 1; player <= 2; ++player) {
        const PlayerEconomy& p = e.getPlayerEconomy(player);
        out.push_back(idx(e.getPlayerWeather(player)));
        for (int v : stocksOf(p)) out.push_back(v);
        out.push_back(p.energyMW);
        out.push_back(p.cityInfluence);
    }
    const CityConquestState& c = e.getCityState();
    out.push_back(c.cityEnergyDemand);
    out.push_back(c.p1CityShare);
    out.push_back(c.p1DailyDelivered);
    out.push_back(c.p2DailyDelivered);
    out.push_back(c.winner);
    for (const PlacedBuilding& b : e.getBuildings()) {
        out.push_back(b.currentOutputMW);
        out.push_back(b.energyStored);
        out.push_back(b.lightRadius);
    }
}

// The same actions on a restarted and on a brand-new engine must give the same match
std::vector<double> scriptedPlay(GameEngine& e) {
    std::vector<double> trace;
    for (int i = 0; i < 3; ++i) trace.push_back(std::rand());
    giveResources(e, 1, 500);
    giveResources(e, 2, 500);
    place(e, 1, BuildingType::WIND_TURBINE, slotOf(e, 1, 1, 0));
    place(e, 1, BuildingType::BATTERY, slotOf(e, 1, 1, 1));
    place(e, 1, BuildingType::LAMP, slotOf(e, 1, 1, 2));
    place(e, 2, BuildingType::SOLAR_PANEL, slotOf(e, 2, 15, 0));
    std::string msg;
    e.mineResource(1, ResourceType::IRON, msg);
    e.mineResource(2, ResourceType::GOLD, msg);
    for (int i = 0; i < 3 * 360; ++i) {
        e.update(0.25f);
        if (i % 4 == 3) recordState(e, trace);
    }
    return trace;
}

void testRestartResetsEverything() {
    beginGroup("Restart resets everything");
    // Dirty every part of the state: land, mines, buildings, stored energy, selection, time scale,
    // clock, season, city share and the winner
    GameEngine dirty;
    dirty.init(1600.0f, 900.0f);
    buildWindFarm(dirty, 1);
    buyPlot(dirty, 1, 2);
    buyPlot(dirty, 2, 14);
    place(dirty, 1, BuildingType::BATTERY, slotOf(dirty, 1, 2, 0));
    giveResources(dirty, 2, 100);
    place(dirty, 2, BuildingType::LAMP, slotOf(dirty, 2, 15, 0));
    std::string msg;
    dirty.getPlayerEconomyMut(1).gold = 5000;
    dirty.getPlayerEconomyMut(2).gold = 5000;
    REQUIRE(dirty.upgradeMine(1, ResourceType::IRON, msg) && dirty.upgradeMine(1, ResourceType::IRON, msg) &&
                dirty.upgradeMine(2, ResourceType::GOLD, msg),
            "upgrades failed: " << msg);
    dirty.mineResource(1, ResourceType::WOOD, msg);
    dirty.cycleBuildingSelection(1);
    for (int i = 0; i < 3; ++i) dirty.cycleBuildingSelection(2);
    int steps = 0;
    while (dirty.getCityState().winner == 0) {
        dirty.update(0.25f);
        REQUIRE(++steps < 100000, "no winner");
    }
    dirty.setTimeScale(Balance::MINE_SPEEDUP_MULT);
    const PlacedBuilding* battery = findBuilding(dirty, 1, BuildingType::BATTERY);
    REQUIRE(dirty.getCityState().winner == 1 && dirty.getSeason() == SeasonType::SUMMER && battery && battery->energyStored > 0.0f,
            "dirty state not reached: winner " << dirty.getCityState().winner << " season " << idx(dirty.getSeason()));

    dirty.restartGame();
    const GameEngine restarted = dirty; // snapshot before the scripted play
    const std::vector<double> restartedTrace = scriptedPlay(dirty);

    GameEngine fresh;
    fresh.init(1600.0f, 900.0f); // same EC_SEED as the restart
    const GameEngine freshSnapshot = fresh;
    const std::vector<double> freshTrace = scriptedPlay(fresh);

    CHECK(restarted.getCurrentDay() == 1 && near(restarted.getHour24(), Balance::MATCH_START_HOUR, 1e-4f) &&
              restarted.getSeason() == SeasonType::SPRING,
          "clock after restart: day " << restarted.getCurrentDay() << " hour " << restarted.getHour24() << " season " << idx(restarted.getSeason()));
    CHECK(restarted.getTimeScale() == 1.0f, "time scale " << restarted.getTimeScale());
    CHECK(restarted.getBuildings().empty(), "buildings: " << restarted.getBuildings().size());
    CHECK(restarted.getCityState().winner == 0 && restarted.getCityState().cityEnergyDemand == 0 && restarted.getCityState().p1CityShare == 0.5f,
          "city not reset");
    for (int player = 1; player <= 2; ++player) {
        const std::string d = economyDiff(restarted.getPlayerEconomy(player), freshSnapshot.getPlayerEconomy(player));
        CHECK(d.empty(), "P" << player << " economy differs from a new match:" << d);
        CHECK(restarted.getPlayerWeather(player) == freshSnapshot.getPlayerWeather(player), "P" << player << " weather differs from a new match");
    }
    const std::string cd = cityDiff(restarted.getCityState(), freshSnapshot.getCityState());
    CHECK(cd.empty(), "city differs from a new match:" << cd);
    const std::vector<LandPlot>& a = restarted.getLandPlots();
    const std::vector<LandPlot>& b = freshSnapshot.getLandPlots();
    bool samePlots = (a.size() == b.size());
    for (size_t i = 0; samePlots && i < a.size(); ++i) {
        samePlots = a[i].id == b[i].id && a[i].playerOwner == b[i].playerOwner && a[i].isPurchased == b[i].isPurchased &&
                    a[i].costGold == b[i].costGold && a[i].bounds.position == b[i].bounds.position && a[i].bounds.size == b[i].bounds.size;
    }
    CHECK(samePlots, "land plots differ from a new match");
    for (int t = 1; t <= 7; ++t) {
        CHECK(restarted.getMineLevel(1, static_cast<ResourceType>(t)) == 1 && restarted.getMineLevel(2, static_cast<ResourceType>(t)) == 1,
              "mine " << t << " level not reset");
    }
    // Hidden state (revenue timer, game clock, daily counters, RNG): the same script plays out identically
    CHECK(restartedTrace.size() == freshTrace.size() && restartedTrace == freshTrace,
          "a restarted match plays differently from a new one (trace " << restartedTrace.size() << " vs " << freshTrace.size() << ")");
    endGroup();
}

} // namespace

int main() {
    if (std::getenv("EC_SEED") == nullptr) setSeedEnv(kDefaultSeed);
    std::cout << "========================================================\n";
    std::cout << " ENERGY CRISIS - CORE SYSTEMS UNIT TESTS\n";
    std::cout << " EC_SEED: " << std::getenv("EC_SEED") << "\n";
    std::cout << "========================================================\n";

    testMiningYields();
    testMiningCooldown();
    testMineUpgrades();
    testLandPurchase();
    testGridMapping();
    testPlacementRules();
    testDemolitionRefund();
    testWeatherMapping();
    testEngineDailyWeather();
    testSolarCurve();
    testWindAndHydro();
    testEngineGeneratorOutputs();
    testGracePeriod();
    testSeasonRollover();
    testRestartResetsEverything();

    std::cout << "\n========================================================\n";
    if (g_failures == 0) {
        std::cout << " ALL " << g_checks << " CORE SYSTEM CHECKS PASSED\n";
    } else {
        std::cout << " " << g_failures << " OF " << g_checks << " CORE SYSTEM CHECKS FAILED\n";
    }
    std::cout << "========================================================\n";
    return (g_failures == 0) ? 0 : 1;
}
