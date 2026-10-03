// =============================================================================
// ENERGY CRISIS - PLAYER MODIFIER TESTS ([AI team], НЕВЪЗМОЖНО bot advantages)
// Headless: built from this file + Game/scr/*.cpp only, no SFML needed ("make test").
// Checks the minimal PlayerModifiers hook: neutral defaults, building cost multiplier,
// mining yield multiplier, income multiplier, the daily city-share bonus and the reset on init.
// Exits with 1 when any check failed.
// =============================================================================
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include "../Game/includes/game_main.h"

#ifdef _WIN32
// Declared by hand: strict -std=c++17 hides _putenv in some MinGW headers (msvcrt exports it)
extern "C" __declspec(dllimport) int __cdecl _putenv(const char*);
#endif

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond, details)                                                                         \
    do {                                                                                             \
        ++g_checks;                                                                                  \
        if (!(cond)) {                                                                               \
            ++g_failures;                                                                            \
            std::cerr << "    FAIL (line " << __LINE__ << "): " << #cond << " | " << details << "\n"; \
        }                                                                                            \
    } while (0)

#define REQUIRE(cond, details)                                                                        \
    do {                                                                                              \
        ++g_checks;                                                                                   \
        if (!(cond)) {                                                                                \
            std::cerr << "    FATAL (line " << __LINE__ << "): " << #cond << " | " << details << "\n"; \
            std::exit(1);                                                                             \
        }                                                                                             \
    } while (0)

constexpr float kFrame = 1.0f / 60.0f;

// EC_SEED handling (nullptr removes the variable), restored at the end of a test
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

struct SavedSeedEnv {
    bool had;
    std::string value;
    SavedSeedEnv() {
        const char* v = std::getenv("EC_SEED");
        had = (v != nullptr);
        value = had ? v : "";
    }
    ~SavedSeedEnv() { setSeedEnv(had ? value.c_str() : nullptr); }
};

int startPlotId(int player) { return (player == 1) ? 1 : 15; }

sf::Vector2f slotOf(const GameEngine& e, int player, int plotId, int sub) {
    int idx = (plotId - 1) % 12;
    int col = (idx % 3) * 3 + sub % 3;
    int row = (idx / 3) * 3 + sub / 3;
    return e.getGridSlot(player, col, row);
}

void giveResources(GameEngine& e, int player, int amount) {
    auto& p = e.getPlayerEconomyMut(player);
    p.wood = p.iron = p.copper = p.coal = p.silicon = p.silver = amount;
}

void place(GameEngine& e, int player, BuildingType type, sf::Vector2f pos) {
    std::string msg;
    bool ok = e.placeBuilding(player, type, pos, msg);
    REQUIRE(ok, "placeBuilding P" << player << " failed: " << msg);
}

void runToNextDay(GameEngine& e) {
    const int day = e.getCurrentDay();
    long frames = 0;
    while (e.getCurrentDay() == day && e.getCityState().winner == 0) {
        e.update(kFrame);
        if (++frames >= 100000) {
            std::cerr << "    FATAL: day " << day << " never ended\n";
            std::exit(1);
        }
    }
}

void begin(const char* name) { std::cout << "\n[" << name << "]\n"; }

PlayerModifiers impossibleMods() {
    PlayerModifiers m;
    m.incomeMult = 2.0f;
    m.mineYieldMult = 3.0f;
    m.costMult = 0.5f;
    m.cooldownMult = 0.0f;
    m.shareBonus = 0.06f;
    return m;
}

void testDefaultsAreNeutral() {
    begin("defaults are neutral");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    for (int p = 1; p <= 2; ++p) {
        const PlayerModifiers& m = e.getPlayerModifiers(p);
        CHECK(m.incomeMult == 1.0f && m.mineYieldMult == 1.0f && m.costMult == 1.0f && m.cooldownMult == 1.0f &&
                  m.shareBonus == 0.0f,
              "P" << p << " modifiers are not neutral");
        for (int t = 1; t <= 5; ++t) {
            BuildingCost a = e.getBuildingCost(static_cast<BuildingType>(t));
            BuildingCost b = e.getBuildingCost(p, static_cast<BuildingType>(t));
            CHECK(a.woodCost == b.woodCost && a.ironCost == b.ironCost && a.copperCost == b.copperCost &&
                      a.coalCost == b.coalCost && a.siliconCost == b.siliconCost && a.silverCost == b.silverCost,
                  "type " << t << " cost differs for P" << p);
        }
    }
}

void testHalfCost() {
    begin("costMult 0.5 halves recipes (rounded up) for that player only");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    e.setPlayerModifiers(2, impossibleMods());
    BuildingCost hydro = e.getBuildingCost(2, BuildingType::HYDRO_PLANT); // 15 20 12 0 6 0
    CHECK(hydro.woodCost == 8 && hydro.ironCost == 10 && hydro.copperCost == 6 && hydro.coalCost == 0 &&
              hydro.siliconCost == 3 && hydro.silverCost == 0,
          "hydro " << hydro.woodCost << "/" << hydro.ironCost << "/" << hydro.copperCost << "/" << hydro.siliconCost);
    BuildingCost p1Hydro = e.getBuildingCost(1, BuildingType::HYDRO_PLANT);
    CHECK(p1Hydro.woodCost == 15 && p1Hydro.ironCost == 20, "P1 hydro must stay at full price");

    // Placement deducts the halved recipe: wind 8/14/8/6 -> 4/7/4/3
    giveResources(e, 2, 10);
    place(e, 2, BuildingType::WIND_TURBINE, slotOf(e, 2, startPlotId(2), 0));
    const auto& p2 = e.getPlayerEconomy(2);
    CHECK(p2.wood == 6 && p2.iron == 3 && p2.copper == 6 && p2.coal == 7,
          "after wind P2 has " << p2.wood << "/" << p2.iron << "/" << p2.copper << "/" << p2.coal);

    // P1 with the same 10 of everything cannot afford a full-price wind turbine (14 iron)
    giveResources(e, 1, 10);
    std::string reason;
    CHECK(!e.canPlaceBuilding(1, BuildingType::WIND_TURBINE, slotOf(e, 1, startPlotId(1), 0), reason),
          "P1 must not get the discount");

    // Demolition refunds half of what was actually paid
    std::string msg;
    giveResources(e, 2, 0);
    REQUIRE(e.removeBuilding(2, slotOf(e, 2, startPlotId(2), 0), msg), msg);
    CHECK(p2.wood == 2 && p2.iron == 3 && p2.copper == 2 && p2.coal == 1,
          "refund " << p2.wood << "/" << p2.iron << "/" << p2.copper << "/" << p2.coal);
}

void testMiningYield() {
    begin("mineYieldMult 3 triples every mining hit");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    e.setPlayerModifiers(2, impossibleMods());
    std::string msg;
    GameEngine::MineResult r1, r2;
    REQUIRE(e.mineResource(1, ResourceType::WOOD, r1, msg), msg);
    REQUIRE(e.mineResource(2, ResourceType::WOOD, r2, msg), msg);
    CHECK(r1.amount == Balance::WOOD_BASE_YIELD, "P1 wood " << r1.amount);
    CHECK(r2.amount == 3 * Balance::WOOD_BASE_YIELD, "P2 wood " << r2.amount);
    REQUIRE(e.mineResource(2, ResourceType::GOLD, r2, msg), msg);
    CHECK(r2.amount == 3 * Balance::GOLD_BASE_YIELD, "P2 gold " << r2.amount);
}

void testIncome() {
    begin("incomeMult 2 doubles city money and gold dividends");
    // Two engines with the same seed (same weather) and the same wind farms; only one has the multiplier
    SavedSeedEnv keep;
    setSeedEnv("777");
    GameEngine plain, boosted;
    plain.init(1600.0f, 900.0f);
    boosted.init(1600.0f, 900.0f);
    PlayerModifiers m;
    m.incomeMult = 2.0f;
    boosted.setPlayerModifiers(2, m);
    for (GameEngine* e : { &plain, &boosted }) {
        for (int p = 1; p <= 2; ++p) {
            giveResources(*e, p, 1000);
            for (int i = 0; i < 3; ++i) place(*e, p, BuildingType::WIND_TURBINE, slotOf(*e, p, startPlotId(p), i));
        }
        for (int i = 0; i < 60 * 30; ++i) e->update(kFrame); // 30 game-seconds
    }
    const auto& a = plain.getPlayerEconomy(2);
    const auto& b = boosted.getPlayerEconomy(2);
    CHECK(a.money > 0 && b.money == 2 * a.money, "money plain " << a.money << " boosted " << b.money);
    CHECK(a.gold > 0 && b.gold == 2 * a.gold, "gold plain " << a.gold << " boosted " << b.gold);
    CHECK(plain.getPlayerEconomy(1).money == boosted.getPlayerEconomy(1).money, "P1 income must not change");
}

void testShareBonus() {
    begin("shareBonus: extra share each settled day the player met the demand");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    e.setPlayerModifiers(2, impossibleMods());
    // Both players run far above the demand (6 wind turbines = at least 347 MW)
    for (int p = 1; p <= 2; ++p) {
        giveResources(e, p, 1000);
        for (int i = 0; i < 6; ++i) place(e, p, BuildingType::WIND_TURBINE, slotOf(e, p, startPlotId(p), i));
    }
    runToNextDay(e); // day 1 (grace)
    runToNextDay(e); // day 2 (grace)
    CHECK(std::abs(e.getCityState().p1CityShare - 0.5f) < 1e-4f, "no bonus in the grace period");
    runToNextDay(e); // day 3: both met 30 MW -> no normal shift, P2 bonus 6%
    float share = e.getCityState().p1CityShare;
    CHECK(std::abs(share - 0.44f) < 1e-3f, "P1 share after day 3 = " << share);
    CHECK(e.getCityState().lastCutMessage.find("БОНУС") != std::string::npos,
          "day message names the bonus: " << e.getCityState().lastCutMessage);

    // Days keep adding up until the bonus wins the match on its own (85% for P2 = 15% for P1)
    int guard = 0;
    while (e.getCityState().winner == 0 && ++guard < 20) runToNextDay(e);
    CHECK(e.getCityState().winner == 2, "P2 must win through the daily bonus, winner = " << e.getCityState().winner);
    CHECK(e.getCurrentDay() <= 10, "bonus victory took until day " << e.getCurrentDay());
}

void testNoBonusWhenDemandMissed() {
    begin("no bonus on a day the bonus player missed the demand");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    e.setPlayerModifiers(2, impossibleMods());
    giveResources(e, 1, 1000);
    for (int i = 0; i < 6; ++i) place(e, 1, BuildingType::WIND_TURBINE, slotOf(e, 1, startPlotId(1), i));
    runToNextDay(e);
    runToNextDay(e);
    runToNextDay(e); // day 3: only P1 met the demand -> normal 15% shift to P1, no P2 bonus
    float share = e.getCityState().p1CityShare;
    CHECK(std::abs(share - (0.5f + Balance::MAX_DAILY_CITY_SHIFT)) < 1e-3f, "P1 share after day 3 = " << share);
}

void testResetOnInit() {
    begin("init() starts every match with neutral modifiers");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    e.setPlayerModifiers(2, impossibleMods());
    e.restartGame();
    const PlayerModifiers& m = e.getPlayerModifiers(2);
    CHECK(m.mineYieldMult == 1.0f && m.costMult == 1.0f && m.shareBonus == 0.0f, "modifiers survived a restart");
}

void testSanitized() {
    begin("invalid values are clamped");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    PlayerModifiers bad;
    bad.costMult = -2.0f;
    bad.shareBonus = 5.0f;
    e.setPlayerModifiers(1, bad);
    CHECK(e.getPlayerModifiers(1).costMult == 0.0f, "negative cost multiplier");
    CHECK(e.getPlayerModifiers(1).shareBonus == 1.0f, "share bonus above 100%");
    BuildingCost c = e.getBuildingCost(1, BuildingType::SOLAR_PANEL);
    CHECK(c.woodCost >= 1 && c.siliconCost >= 1, "a non-zero recipe entry never becomes free");
}

} // namespace

int main() {
    std::cout << "========================================================\n";
    std::cout << " ENERGY CRISIS - PLAYER MODIFIER TESTS\n";
    std::cout << "========================================================\n";
    testDefaultsAreNeutral();
    testHalfCost();
    testMiningYield();
    testIncome();
    testShareBonus();
    testNoBonusWhenDemandMissed();
    testResetOnInit();
    testSanitized();
    std::cout << "\n========================================================\n";
    if (g_failures == 0) {
        std::cout << " ALL " << g_checks << " PLAYER MODIFIER CHECKS PASSED\n";
        return 0;
    }
    std::cout << " " << g_failures << " OF " << g_checks << " PLAYER MODIFIER CHECKS FAILED\n";
    return 1;
}
