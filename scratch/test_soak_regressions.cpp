// =============================================================================
// ENERGY CRISIS - REGRESSION CHECKS FOR DEFECTS FOUND BY THE SOAK TEST   [team: wave-c-soak]
// One group per defect that scratch/test_soak.cpp found in the engine. Each group fails on
// the engine before its fix and passes after it.
// Headless: built from this file + Game/scr/*.cpp only ("make test" or build_headless.sh).
// Exit code 1 when any check failed.
// =============================================================================
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <streambuf>
#include <string>
#include <vector>
#include "../Game/includes/game_main.h"

#ifdef _WIN32
extern "C" __declspec(dllimport) int __cdecl _putenv(const char*);
#endif

namespace {

int g_checks = 0;
int g_failures = 0;
int g_groupFailures = 0;

#define CHECK(cond, details)                                                                        \
    do {                                                                                            \
        ++g_checks;                                                                                 \
        if (!(cond)) {                                                                              \
            ++g_failures;                                                                           \
            ++g_groupFailures;                                                                      \
            std::cerr << "    FAIL (line " << __LINE__ << "): " << #cond << " | " << details << "\n"; \
        }                                                                                           \
    } while (0)

#define REQUIRE(cond, details)                                                                       \
    do {                                                                                             \
        ++g_checks;                                                                                  \
        if (!(cond)) {                                                                               \
            std::cerr << "    FATAL (line " << __LINE__ << "): " << #cond << " | " << details << "\n"; \
            std::exit(1);                                                                            \
        }                                                                                            \
    } while (0)

void beginGroup(const std::string& name) {
    g_groupFailures = 0;
    std::cout << "\n[" << name << "]\n";
}

void endGroup() { std::cout << (g_groupFailures == 0 ? "  -> PASS\n" : "  -> FAIL\n"); }

struct NullBuf : std::streambuf {
    int overflow(int c) override { return c; }
};
NullBuf g_nullBuf;

// Fresh engine with a fixed seed and the engine's console log swallowed
void initEngine(GameEngine& e, unsigned seed) {
#ifdef _WIN32
    std::string s = "EC_SEED=" + std::to_string(seed);
    _putenv(s.c_str());
#else
    setenv("EC_SEED", std::to_string(seed).c_str(), 1);
#endif
    std::streambuf* old = std::cout.rdbuf(&g_nullBuf);
    e.init(1600.0f, 900.0f);
    std::cout.rdbuf(old);
}

void giveResources(GameEngine& e, int player, int amount) {
    PlayerEconomy& p = e.getPlayerEconomyMut(player);
    p.wood = p.iron = p.copper = p.coal = p.silicon = p.silver = amount;
    p.data.wood = p.data.iron = p.data.copper = p.data.coal = p.data.silicon = p.data.silver = amount;
}

void place(GameEngine& e, int player, BuildingType type, int col, int row) {
    std::string msg;
    bool ok = e.placeBuilding(player, type, e.getGridSlot(player, col, row), msg);
    REQUIRE(ok, "placeBuilding P" << player << " type " << static_cast<int>(type) << " at " << col << "," << row << ": " << msg);
}

// Engine clock in game-seconds since 06:00 of day 1, rebuilt from the public day + hour
double engineClock(const GameEngine& e) {
    float h = e.getHour24() - Balance::CLOCK_HOUR_AT_ZERO;
    if (h < 0.0f) h += 24.0f;
    return (e.getCurrentDay() - 1) * static_cast<double>(Balance::SECONDS_PER_DAY) + h / 24.0 * Balance::SECONDS_PER_DAY;
}

// Distance between two clock readings, ignoring a whole-day wrap of the hour display at 06:00
double clockError(double a, double b) {
    double d = std::fmod(std::abs(a - b), static_cast<double>(Balance::SECONDS_PER_DAY));
    return std::min(d, Balance::SECONDS_PER_DAY - d);
}

// ---------------------------------------------------------------------------
// [S1] The float game clock drifted with the frame rate: at 60 FPS the match clock was ~2.4 s
// off after 20 days, at 144 FPS ~3 s, and days lasted 89.8-90.01 s depending on the FPS.
// ---------------------------------------------------------------------------
void testClockDoesNotDrift() {
    beginGroup("S1 game clock does not drift with the frame rate");
    const float dts[] = { 1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 144.0f, 1.0f / 240.0f };
    for (float dt : dts) {
        GameEngine e;
        initEngine(e, 11);
        double fed = Balance::gameSecondsAtHour(Balance::MATCH_START_HOUR);
        double worst = 0.0;
        long frames = 0;
        float longestDay = 0.0f, shortestDay = 1e9f, prevDaily = 0.0f;
        int prevDay = 1;
        while (e.getCityState().winner == 0) {
            e.update(dt);
            fed += dt;
            ++frames;
            worst = std::max(worst, clockError(engineClock(e), fed));
            if (e.getCurrentDay() != prevDay) {
                if (prevDay > 1) {
                    longestDay = std::max(longestDay, prevDaily);
                    shortestDay = std::min(shortestDay, prevDaily);
                }
                prevDay = e.getCurrentDay();
            }
            prevDaily = e.getCityState().dailySeconds;
        }
        const long expectedFrames = static_cast<long>(std::lround(
            (Balance::FINAL_DAY * Balance::SECONDS_PER_DAY - Balance::gameSecondsAtHour(Balance::MATCH_START_HOUR)) / dt));
        CHECK(worst < 0.01, "dt 1/" << std::lround(1.0f / dt) << ": engine clock off by " << worst << " s");
        CHECK(std::labs(frames - expectedFrames) <= 1, "dt 1/" << std::lround(1.0f / dt) << ": match took " << frames
                                                                << " frames, expected " << expectedFrames);
        // dailySeconds just before a settlement: a whole day minus at most one frame
        CHECK(longestDay <= Balance::SECONDS_PER_DAY + 1e-3f && shortestDay >= Balance::SECONDS_PER_DAY - dt - 1e-3f,
              "dt 1/" << std::lround(1.0f / dt) << ": days lasted " << shortestDay << " .. " << longestDay << " s");
    }
    endGroup();
}

// ---------------------------------------------------------------------------
// [S2] Every simulation step looked up each building's recipe with getBuildingCost(), which
// builds two std::strings: ~900 heap allocations per step with full sectors (216 buildings),
// ~280 us per step unoptimised. The soak test could not finish in time because of it.
// The check compares one step with 216 buildings against the cost of a single recipe lookup
// measured in the same process (min of several runs), so machine load cancels out.
// ---------------------------------------------------------------------------
void fillSector(GameEngine& e, int player) {
    PlayerEconomy& ec = e.getPlayerEconomyMut(player);
    ec.gold = 1000000;
    std::string msg;
    for (int i = 0; i < Balance::PLOTS_PER_PLAYER; ++i) e.buyNextLandTier(player, msg);
    ec.gold = 0;
    ec.data.gold = 0;
    giveResources(e, player, 1000000);
    const BuildingType mix[] = { BuildingType::WIND_TURBINE, BuildingType::SOLAR_PANEL, BuildingType::BATTERY, BuildingType::LAMP };
    for (int r = 0; r < 12; ++r) {
        for (int c = 0; c < 9; ++c) {
            bool river = (c / 3) == (player == 1 ? Balance::P1_RIVER_BANK_PLOT_COL : Balance::P2_RIVER_BANK_PLOT_COL);
            place(e, player, river ? BuildingType::HYDRO_PLANT : mix[(r * 9 + c) % 4], c, r);
        }
    }
}

double secondsSince(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

void testStepCostDoesNotScaleWithRecipeLookups() {
    beginGroup("S2 a simulation step does not look up every building's recipe");
    GameEngine e;
    initEngine(e, 12);
    e.update(Balance::SECONDS_PER_DAY * 0.1f); // daylight, so building is allowed everywhere
    fillSector(e, 1);
    fillSector(e, 2);
    REQUIRE(e.getBuildings().size() == 216, "buildings " << e.getBuildings().size());

    double bestStep = 1e9, bestLookup = 1e9;
    long sink = 0;
    for (int run = 0; run < 5; ++run) {
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 400; ++i) e.update(1.0f / 144.0f);
        bestStep = std::min(bestStep, secondsSince(t0) / 400.0);
        t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 4000; ++i) sink += e.getBuildingCost(static_cast<BuildingType>(1 + i % 3)).basePowerMW;
        bestLookup = std::min(bestLookup, secondsSince(t0) / 4000.0);
    }
    double ratio = bestStep / bestLookup;
    std::cout << "  step with 216 buildings = " << ratio << " recipe lookups (" << bestStep * 1e6 << " us; sink " << (sink % 7) << ")\n";
    // before the fix a step cost ~2 lookups per building (432); allow loops and scratch vectors
    CHECK(ratio < 150.0, "one step costs as much as " << ratio << " recipe lookups");
    endGroup();
}

// Every resource of the PlayerData mirror equals the economy value
bool mirrorInSync(const PlayerEconomy& p) {
    return p.data.money == p.money && p.data.gold == p.gold && p.data.silver == p.silver && p.data.iron == p.iron &&
           p.data.coal == p.coal && p.data.copper == p.copper && p.data.silicon == p.silicon && p.data.wood == p.wood;
}

// ---------------------------------------------------------------------------
// [S3] buyLandPlot() took the gold from PlayerEconomy::gold but left PlayerEconomy::data.gold
// (the weatherF mirror) unchanged, so the two disagreed after every land purchase.
// ---------------------------------------------------------------------------
void testLandPurchaseKeepsMirror() {
    beginGroup("S3 buying land keeps the PlayerData mirror in sync");
    GameEngine e;
    initEngine(e, 13);
    for (int p = 1; p <= 2; ++p) {
        PlayerEconomy& ec = e.getPlayerEconomyMut(p);
        ec.gold = ec.data.gold = 1000;
        std::string msg;
        bool ok = e.buyNextLandTier(p, msg);
        REQUIRE(ok, "P" << p << " could not buy land: " << msg);
        const PlayerEconomy& now = e.getPlayerEconomy(p);
        CHECK(now.gold < 1000 && mirrorInSync(now), "P" << p << " gold " << now.gold << " mirror " << now.data.gold);
        ok = e.buyLandPlot(p, p == 1 ? 3 : 13, msg);
        REQUIRE(ok, "P" << p << " could not buy the river plot: " << msg);
        CHECK(mirrorInSync(e.getPlayerEconomy(p)), "P" << p << " gold " << e.getPlayerEconomy(p).gold << " mirror "
                                                       << e.getPlayerEconomy(p).data.gold);
    }
    endGroup();
}

// Comparable snapshot of everything a refused action must leave untouched
struct Snapshot {
    PlayerEconomy p1, p2;
    size_t buildings;
    int purchased;
    explicit Snapshot(const GameEngine& e)
        : p1(e.getPlayerEconomy(1)), p2(e.getPlayerEconomy(2)), buildings(e.getBuildings().size()), purchased(0) {
        for (const auto& plot : e.getLandPlots()) purchased += plot.isPurchased ? 1 : 0;
    }
    static bool same(const PlayerEconomy& a, const PlayerEconomy& b) {
        if (!(a.money == b.money && a.gold == b.gold && a.silver == b.silver && a.iron == b.iron && a.coal == b.coal &&
              a.copper == b.copper && a.silicon == b.silicon && a.wood == b.wood && a.landTier == b.landTier &&
              a.selectedBuilding == b.selectedBuilding && a.lastPlacedBuilding == b.lastPlacedBuilding))
            return false;
        for (int k = 0; k < 8; ++k)
            if (a.mineLevels[k] != b.mineLevels[k]) return false;
        return true;
    }
    bool operator==(const Snapshot& o) const {
        return same(p1, o.p1) && same(p2, o.p2) && buildings == o.buildings && purchased == o.purchased;
    }
};

// ---------------------------------------------------------------------------
// [S4] placeBuilding() accepted building types outside the enum (e.g. 7, 9, 100): the recipe
// lookup fell back to an all-zero cost, so a free building of no known kind was placed and
// blocked the slot for the rest of the match.
// ---------------------------------------------------------------------------
void testUnknownBuildingTypesRefused() {
    beginGroup("S4 unknown building types are refused");
    GameEngine e;
    initEngine(e, 14);
    e.update(Balance::SECONDS_PER_DAY * 0.1f); // daylight
    for (int p = 1; p <= 2; ++p) {
        giveResources(e, p, 500);
        const int bad[] = { 0, 7, 9, 100, -2 };
        for (int t : bad) {
            sf::Vector2f pos = e.getGridSlot(p, p == 1 ? 0 : 8, 0);
            Snapshot before(e);
            std::string reason, msg;
            bool preview = e.canPlaceBuilding(p, static_cast<BuildingType>(t), pos, reason);
            bool ok = e.placeBuilding(p, static_cast<BuildingType>(t), pos, msg);
            CHECK(!preview && !ok && Snapshot(e) == before, "P" << p << " type " << t << ": preview " << preview << " placed " << ok);
            CHECK(!msg.empty(), "P" << p << " type " << t << ": no message for the refusal");
        }
        // the real types still work
        place(e, p, BuildingType::SOLAR_PANEL, p == 1 ? 0 : 8, 0);
    }
    endGroup();
}

// ---------------------------------------------------------------------------
// [S5] A non-finite position (NaN / infinity) was snapped to the first grid slot and the
// building was placed there, wherever the cursor really was.
// ---------------------------------------------------------------------------
void testNonFinitePositionsRefused() {
    beginGroup("S5 non-finite positions are refused");
    GameEngine e;
    initEngine(e, 15);
    e.update(Balance::SECONDS_PER_DAY * 0.1f); // daylight
    const float nan = std::numeric_limits<float>::quiet_NaN(), inf = std::numeric_limits<float>::infinity();
    const sf::Vector2f bad[] = { sf::Vector2f(nan, 120.0f), sf::Vector2f(300.0f, nan), sf::Vector2f(inf, inf),
                                 sf::Vector2f(-inf, 150.0f), sf::Vector2f(nan, nan) };
    for (int p = 1; p <= 2; ++p) {
        giveResources(e, p, 500);
        for (const auto& pos : bad) {
            Snapshot before(e);
            std::string reason, msg;
            bool preview = e.canPlaceBuilding(p, BuildingType::WIND_TURBINE, pos, reason);
            bool ok = e.placeBuilding(p, BuildingType::WIND_TURBINE, pos, msg);
            CHECK(!preview && !ok && Snapshot(e) == before,
                  "P" << p << " at (" << pos.x << "; " << pos.y << "): preview " << preview << " placed " << ok);
        }
        // finite positions still snap and build
        sf::Vector2f s = e.getGridSlot(p, p == 1 ? 1 : 7, 1);
        std::string msg;
        CHECK(e.placeBuilding(p, BuildingType::WIND_TURBINE, sf::Vector2f(s.x + 5.0f, s.y - 4.0f), msg), "P" << p << ": " << msg);
    }
    endGroup();
}

// ---------------------------------------------------------------------------
// [S6] Any player id other than 1 was treated as player 2: mineResource(0 / 3 / -1, ...) gave
// player 2 resources, upgradeMine() spent player 2's gold and the selection calls changed
// player 2's selected building.
// ---------------------------------------------------------------------------
void testInvalidPlayerIdsRefused() {
    beginGroup("S6 invalid player ids are refused without side effects");
    GameEngine e;
    initEngine(e, 16);
    e.update(Balance::SECONDS_PER_DAY * 0.1f); // daylight
    for (int p = 1; p <= 2; ++p) {
        giveResources(e, p, 500);
        PlayerEconomy& ec = e.getPlayerEconomyMut(p);
        ec.gold = ec.data.gold = 5000;
    }
    place(e, 2, BuildingType::WIND_TURBINE, 8, 0);
    e.cycleBuildingSelection(2);
    const int bad[] = { 0, 3, -1, 99 };
    for (int p : bad) {
        Snapshot before(e);
        std::string msg;
        MineResult r;
        bool mined = e.mineResource(p, ResourceType::WOOD, r, msg);
        bool upgraded = e.upgradeMine(p, ResourceType::IRON, msg);
        bool bought = e.buyLandPlot(p, 14, msg);
        bool boughtNext = e.buyNextLandTier(p, msg);
        bool placed = e.placeBuilding(p, BuildingType::SOLAR_PANEL, e.getGridSlot(2, 7, 0), msg);
        bool removed = e.removeBuilding(p, e.getGridSlot(2, 8, 0), msg);
        bool repaired = e.repairBuilding(p, e.getGridSlot(2, 8, 0), msg);
        e.cycleBuildingSelection(p);
        e.cycleBuildingSelectionPrev(p);
        e.cycleBuildingSelection(p);
        e.clearBuildingSelection(p);
        CHECK(!mined && !upgraded && !bought && !boughtNext && !placed && !removed && !repaired,
              "player " << p << ": mined " << mined << " upgraded " << upgraded << " bought " << bought << "/" << boughtNext
                        << " placed " << placed << " removed " << removed << " repaired " << repaired);
        CHECK(Snapshot(e) == before, "player " << p << " changed the state (P2 wood " << before.p2.wood << " -> "
                                               << e.getPlayerEconomy(2).wood << ", P2 iron mine " << before.p2.mineLevels[2]
                                               << " -> " << e.getPlayerEconomy(2).mineLevels[2] << ", P2 selection "
                                               << before.p2.selectedBuilding << " -> " << e.getPlayerEconomy(2).selectedBuilding << ")");
    }
    // players 1 and 2 still act normally
    std::string msg;
    CHECK(e.mineResource(1, ResourceType::WOOD, msg) && e.mineResource(2, ResourceType::WOOD, msg), msg);
    CHECK(e.upgradeMine(2, ResourceType::IRON, msg), msg);
    endGroup();
}

} // namespace

int main() {
    testClockDoesNotDrift();
    testStepCostDoesNotScaleWithRecipeLookups();
    testLandPurchaseKeepsMirror();
    testUnknownBuildingTypesRefused();
    testNonFinitePositionsRefused();
    testInvalidPlayerIdsRefused();
    std::cout << "\n" << (g_failures == 0 ? "ALL PASSED" : "FAILED") << ": " << (g_checks - g_failures) << "/" << g_checks
              << " checks\n";
    return g_failures == 0 ? 0 : 1;
}
