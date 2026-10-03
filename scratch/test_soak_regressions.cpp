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

} // namespace

int main() {
    testClockDoesNotDrift();
    testStepCostDoesNotScaleWithRecipeLookups();
    testLandPurchaseKeepsMirror();
    std::cout << "\n" << (g_failures == 0 ? "ALL PASSED" : "FAILED") << ": " << (g_checks - g_failures) << "/" << g_checks
              << " checks\n";
    return g_failures == 0 ? 0 : 1;
}
