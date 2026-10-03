// =============================================================================
// ENERGY CRISIS - ENGINE SOAK & FUZZ TEST                       [team: wave-c-soak]
//
// Plays many complete matches (seeds x frame sizes x strategies) up to the end of the
// final day and checks the engine invariants after EVERY update() call and after EVERY
// player action:
//   * no crash / no hang, resources and money never negative, energy finite,
//   * 0 <= city share <= 1 (both players' influence add up to 1),
//   * exactly one settlement per in-game day, share changes only at a settlement and
//     never by more than the daily cap, no change during the grace period,
//   * the city demand follows the schedule (0, 0, 30, +15 MW per day),
//   * a winner (or a draw) exists after the final day, victory rules are respected and the
//     simulation is frozen afterwards,
//   * the game clock does not drift with the frame rate,
//   * every action keeps its contract (refused => nothing changed, accepted => exact cost),
//     illegal input (bad player id, bad enum, NaN position, odd time scale) is refused,
//   * restartGame() resets everything (equal to a fresh engine with the same seed),
//   * identical seeds give identical matches.
// Strategies: "duel" (two scripted builders), "rush" (one builder vs an idle player, or both
// idle), "random" (random legal + illegal actions for both players, lightning, time scale).
//
// Headless: built from this file + Game/scr/*.cpp only ("make test" or build_headless.sh).
// Knobs (environment): EC_SOAK_SEEDS=<n> full matrix of n seeds (default: standard plan ~60 s),
//                      EC_SOAK_FIRST=<k> first seed index (parallel shards),
//                      EC_SOAK_VERBOSE=1 prints one line per match.
// Exit code 1 when any invariant failed.
// =============================================================================
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <streambuf>
#include <string>
#include <vector>
#include "../Game/includes/game_main.h"

#ifdef _WIN32
// Declared by hand: strict -std=c++17 hides _putenv in some MinGW headers (msvcrt exports it)
extern "C" __declspec(dllimport) int __cdecl _putenv(const char*);
#endif

namespace {

// ---------------------------------------------------------------------------
// Invariant registry
// ---------------------------------------------------------------------------
enum Inv {
    INV_NO_HANG,
    INV_NON_NEGATIVE,
    INV_FINITE,
    INV_SHARE_RANGE,
    INV_ONE_SETTLEMENT,
    INV_SHIFT_CAP,
    INV_DEMAND,
    INV_WINNER_EXISTS,
    INV_WINNER_RULES,
    INV_FROZEN,
    INV_CLOCK,
    INV_BUILDINGS,
    INV_LAND,
    INV_ACTION_CONTRACT,
    INV_PREVIEW_MATCHES,
    INV_ILLEGAL_REFUSED,
    INV_DATA_MIRROR,
    INV_TIME_SCALE,
    INV_RESTART,
    INV_DETERMINISM,
    INV_FRAME_INDEPENDENT,
    INV_COUNT
};

const char* const kInvNames[INV_COUNT] = {
    "no hang (match ends by the final day)",
    "resources and money never negative",
    "energy, outputs, charge and clock finite",
    "0 <= share <= 1, influences add up to 1",
    "exactly one settlement per in-game day",
    "per-day share change within the cap",
    "city demand follows the schedule",
    "winner or draw exists after the final day",
    "victory / draw rules respected",
    "simulation frozen after the match ended",
    "game clock does not drift with frame rate",
    "buildings valid (owner, slot, land, hydro)",
    "land plots valid (start plot, tier count)",
    "action contract (refused = unchanged, exact cost)",
    "placement preview matches the action",
    "illegal input refused without side effects",
    "PlayerData mirror equals the economy",
    "time scale stays positive and finite",
    "restartGame() resets everything",
    "identical seeds give identical matches",
    "outcome independent of the frame size",
};

struct InvStat {
    long long checks = 0;
    long long fails = 0;
    std::vector<std::string> samples;
};
InvStat g_inv[INV_COUNT];

// Context of the running match (only formatted when a check fails)
unsigned g_seed = 0;
float g_dt = 0.0f;
const char* g_strategy = "";
const GameEngine* g_engine = nullptr;

std::string ctx() {
    std::ostringstream os;
    os << "[seed " << g_seed << " dt 1/" << static_cast<int>(std::lround(1.0f / g_dt)) << " " << g_strategy;
    if (g_engine) os << " day " << g_engine->getCurrentDay() << " " << std::fixed << std::setprecision(2) << g_engine->getHour24() << "h";
    os << "]";
    return os.str();
}

#define INV(id, cond, details)                                                                    \
    do {                                                                                          \
        InvStat& s_ = g_inv[id];                                                                  \
        ++s_.checks;                                                                              \
        if (!(cond)) {                                                                            \
            ++s_.fails;                                                                           \
            if (s_.samples.size() < 5) {                                                          \
                std::ostringstream os_;                                                           \
                os_ << ctx() << " line " << __LINE__ << ": " << #cond << " | " << details;       \
                s_.samples.push_back(os_.str());                                                  \
            }                                                                                     \
        }                                                                                         \
    } while (0)

// ---------------------------------------------------------------------------
// Small deterministic RNG for the test's own decisions (independent of the engine RNG)
// ---------------------------------------------------------------------------
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed * 0x9E3779B97F4A7C15ull + 0xD1B54A32D192ED03ull) {}
    uint64_t next() {
        uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    int range(int lo, int hi) { return lo + static_cast<int>(next() % static_cast<uint64_t>(hi - lo + 1)); }
    float uniform() { return static_cast<float>(next() >> 40) / static_cast<float>(1ull << 24); }
    bool chance(float p) { return uniform() < p; }
};

// ---------------------------------------------------------------------------
// Hashing of the complete observable engine state
// ---------------------------------------------------------------------------
struct Hasher {
    uint64_t h = 1469598103934665603ull;
    void bytes(const void* p, size_t n) {
        const unsigned char* c = static_cast<const unsigned char*>(p);
        for (size_t i = 0; i < n; ++i) {
            h ^= c[i];
            h *= 1099511628211ull;
        }
    }
    void word(uint64_t v) {
        h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
        h *= 0xBF58476D1CE4E5B9ull;
    }
    void i(long long v) { word(static_cast<uint64_t>(v)); }
    void f(float v) {
        if (v == 0.0f) v = 0.0f; // -0 == +0
        uint32_t u;
        std::memcpy(&u, &v, sizeof(u));
        word(u);
    }
    void s(const std::string& v) {
        i(static_cast<long long>(v.size()));
        bytes(v.data(), v.size());
    }
};

void hashEconomy(Hasher& h, const PlayerEconomy& p) {
    h.i(p.money); h.i(p.gold); h.i(p.silver); h.i(p.iron); h.i(p.coal); h.i(p.copper); h.i(p.silicon); h.i(p.wood);
    h.i(p.ore); h.i(p.energyMW); h.i(p.landTier); h.f(p.cityInfluence); h.i(p.selectedBuilding); h.i(p.lastPlacedBuilding);
    for (int k = 0; k < 8; ++k) h.i(p.mineLevels[k]);
    h.i(p.data.money); h.i(p.data.iron); h.i(p.data.coal); h.i(p.data.gold); h.i(p.data.copper); h.i(p.data.silver);
    h.i(p.data.silicon); h.i(p.data.wood); h.i(p.data.sticks); h.s(p.data.weather); h.s(p.data.wind_speed);
}

uint64_t hashEngine(const GameEngine& e, bool includeClockAndCity = true) {
    Hasher h;
    if (includeClockAndCity) {
        h.i(e.getCurrentDay());
        h.f(e.getHour24());
        h.i(static_cast<int>(e.getSeason()));
        h.f(e.getTimeScale());
        const CityConquestState& c = e.getCityState();
        h.i(c.cityEnergyDemand); h.f(c.p1CityShare); h.f(c.p1DailyDelivered); h.f(c.p2DailyDelivered);
        h.f(c.dailySeconds); h.i(c.dayCutOccurred); h.s(c.lastCutMessage); h.i(c.winner);
    }
    h.i(static_cast<int>(e.getPlayerWeather(1)));
    h.i(static_cast<int>(e.getPlayerWeather(2)));
    hashEconomy(h, e.getPlayerEconomy(1));
    hashEconomy(h, e.getPlayerEconomy(2));
    for (const auto& b : e.getBuildings()) {
        h.i(static_cast<int>(b.type)); h.f(b.position.x); h.f(b.position.y); h.i(b.playerOwner);
        h.f(b.currentOutputMW); h.f(b.animTimer); h.f(b.energyStored); h.f(b.maxCapacity); h.f(b.lightRadius); h.i(b.isBroken);
    }
    for (const auto& p : e.getLandPlots()) {
        h.i(p.id); h.i(p.playerOwner); h.f(p.bounds.position.x); h.f(p.bounds.position.y);
        h.f(p.bounds.size.x); h.f(p.bounds.size.y); h.i(p.isPurchased); h.i(p.costGold);
    }
    return h.h;
}

// ---------------------------------------------------------------------------
// Environment / output helpers
// ---------------------------------------------------------------------------
void setSeedEnv(unsigned seed) {
    std::string v = std::to_string(seed);
#ifdef _WIN32
    std::string s = "EC_SEED=" + v;
    _putenv(s.c_str());
#else
    setenv("EC_SEED", v.c_str(), 1);
#endif
}

int envInt(const char* name, int fallback) {
    const char* v = std::getenv(name);
    if (!v || !*v) return fallback;
    return std::atoi(v);
}

// Swallows the engine's per-match console log while the soak runs
struct NullBuf : std::streambuf {
    int overflow(int c) override { return c; }
};
NullBuf g_nullBuf;

struct QuietCout {
    std::streambuf* old;
    QuietCout() : old(std::cout.rdbuf(&g_nullBuf)) {}
    ~QuietCout() { std::cout.rdbuf(old); }
};

// ---------------------------------------------------------------------------
// Rules mirrored from the design (Balance) for the checks
// ---------------------------------------------------------------------------
int expectedDemandForDay(int day) {
    if (day <= Balance::GRACE_PERIOD_DAYS) return 0;
    return Balance::STARTING_CITY_DEMAND_MW + (day - Balance::GRACE_PERIOD_DAYS - 1) * Balance::DAILY_DEMAND_INCREASE_MW;
}

int baseYield(ResourceType t) {
    switch (t) {
        case ResourceType::WOOD: return Balance::WOOD_BASE_YIELD;
        case ResourceType::IRON: return Balance::IRON_BASE_YIELD;
        case ResourceType::COPPER: return Balance::COPPER_BASE_YIELD;
        case ResourceType::COAL: return Balance::COAL_BASE_YIELD;
        case ResourceType::SILICON: return Balance::SILICON_BASE_YIELD;
        case ResourceType::SILVER: return Balance::SILVER_BASE_YIELD;
        case ResourceType::GOLD: return Balance::GOLD_BASE_YIELD;
        default: return 0;
    }
}

int* resourceField(PlayerEconomy& p, ResourceType t) {
    switch (t) {
        case ResourceType::WOOD: return &p.wood;
        case ResourceType::IRON: return &p.iron;
        case ResourceType::COPPER: return &p.copper;
        case ResourceType::COAL: return &p.coal;
        case ResourceType::SILICON: return &p.silicon;
        case ResourceType::SILVER: return &p.silver;
        case ResourceType::GOLD: return &p.gold;
        default: return nullptr;
    }
}

bool isValidPlayer(int p) { return p == 1 || p == 2; }
bool isBuildable(BuildingType t) {
    return t == BuildingType::SOLAR_PANEL || t == BuildingType::WIND_TURBINE || t == BuildingType::HYDRO_PLANT ||
           t == BuildingType::BATTERY || t == BuildingType::LAMP;
}

const float kShareEps = 1e-4f;

// ---------------------------------------------------------------------------
// Grid helpers (pure arithmetic, mirrors GameEngine::getGridSlot)
// ---------------------------------------------------------------------------
const int kCols = 9, kRows = 12;
int plotIdOfSlot(int player, int col, int row) { return (player == 1 ? 1 : 13) + (row / 3) * 3 + (col / 3); }
bool isRiverCol(int player, int col) {
    return (col / 3) == (player == 1 ? Balance::P1_RIVER_BANK_PLOT_COL : Balance::P2_RIVER_BANK_PLOT_COL);
}
const LandPlot* plotById(const GameEngine& e, int id) {
    const auto& plots = e.getLandPlots();
    if (id >= 1 && id <= static_cast<int>(plots.size()) && plots[id - 1].id == id) return &plots[id - 1];
    for (const auto& p : plots)
        if (p.id == id) return &p;
    return nullptr;
}
bool plotPurchased(const GameEngine& e, int id) {
    const LandPlot* p = plotById(e, id);
    return p && p->isPurchased;
}

// Maps an exact building position back to its slot; false when it is not exactly on a slot
bool slotOfPosition(const GameEngine& e, int player, sf::Vector2f pos, int& col, int& row) {
    if (!isValidPlayer(player)) return false;
    float startX = (player == 1) ? 258.0f : 1003.0f;
    float fx = pos.x - startX, fy = pos.y - 105.0f;
    if (!(fx >= 0.0f && fy >= 0.0f)) return false;
    int plotC = static_cast<int>(fx / 117.0f), plotR = static_cast<int>(fy / 105.0f);
    float inX = fx - plotC * 117.0f, inY = fy - plotR * 105.0f;
    int subC = static_cast<int>(inX / 35.0f), subR = static_cast<int>(inY / (95.0f / 3.0f));
    if (plotC > 2 || plotR > 3 || subC > 2 || subR > 2) return false;
    col = plotC * 3 + subC;
    row = plotR * 3 + subR;
    sf::Vector2f s = e.getGridSlot(player, col, row);
    return std::abs(s.x - pos.x) < 0.01f && std::abs(s.y - pos.y) < 0.01f;
}

struct Occupancy {
    bool used[3][kRows][kCols];
    int count[3];
    void build(const GameEngine& e) {
        std::memset(used, 0, sizeof(used));
        count[0] = count[1] = count[2] = 0;
        for (const auto& b : e.getBuildings()) {
            int c, r;
            if (isValidPlayer(b.playerOwner) && slotOfPosition(e, b.playerOwner, b.position, c, r)) {
                used[b.playerOwner][r][c] = true;
                ++count[b.playerOwner];
            }
        }
    }
};

// ---------------------------------------------------------------------------
// Structural checks (buildings, land, mirror) - run after every action tick
// ---------------------------------------------------------------------------
void checkStructure(const GameEngine& e) {
    bool used[3][kRows][kCols];
    std::memset(used, 0, sizeof(used));
    for (const auto& b : e.getBuildings()) {
        int c = -1, r = -1;
        bool onSlot = slotOfPosition(e, b.playerOwner, b.position, c, r);
        INV(INV_BUILDINGS, isValidPlayer(b.playerOwner), "building owner " << b.playerOwner);
        INV(INV_BUILDINGS, isBuildable(b.type), "building of type " << static_cast<int>(b.type));
        INV(INV_BUILDINGS, onSlot, "building P" << b.playerOwner << " at (" << b.position.x << "; " << b.position.y << ") is off-grid");
        if (!onSlot) continue;
        INV(INV_BUILDINGS, !used[b.playerOwner][r][c], "two buildings in slot " << c << "," << r << " of P" << b.playerOwner);
        used[b.playerOwner][r][c] = true;
        INV(INV_BUILDINGS, plotPurchased(e, plotIdOfSlot(b.playerOwner, c, r)),
            "building on unpurchased plot " << plotIdOfSlot(b.playerOwner, c, r));
        if (b.type == BuildingType::HYDRO_PLANT) {
            INV(INV_BUILDINGS, isRiverCol(b.playerOwner, c), "hydro off the river bank at col " << c);
        }
    }
    for (int p = 1; p <= 2; ++p) {
        const PlayerEconomy& ec = e.getPlayerEconomy(p);
        int bought = 0;
        for (const auto& plot : e.getLandPlots()) {
            if (plot.playerOwner == p && plot.isPurchased) ++bought;
        }
        INV(INV_LAND, plotPurchased(e, p == 1 ? 1 : 15), "starting plot of P" << p << " not owned");
        INV(INV_LAND, ec.landTier == bought, "P" << p << " landTier " << ec.landTier << " but owns " << bought << " plots");
        for (int k = 0; k < 8; ++k) {
            INV(INV_LAND, ec.mineLevels[k] >= 1 && ec.mineLevels[k] <= Balance::MINE_MAX_LEVEL,
                "P" << p << " mine " << k << " level " << ec.mineLevels[k]);
        }
        INV(INV_DATA_MIRROR,
            ec.data.money == ec.money && ec.data.gold == ec.gold && ec.data.silver == ec.silver && ec.data.iron == ec.iron &&
                ec.data.coal == ec.coal && ec.data.copper == ec.copper && ec.data.silicon == ec.silicon && ec.data.wood == ec.wood,
            "P" << p << " data mirror: money " << ec.data.money << "/" << ec.money << " gold " << ec.data.gold << "/" << ec.gold
                << " wood " << ec.data.wood << "/" << ec.wood << " iron " << ec.data.iron << "/" << ec.iron);
    }
    INV(INV_LAND, e.getLandPlots().size() == static_cast<size_t>(Balance::TOTAL_PLOTS), "plots " << e.getLandPlots().size());
}

// ---------------------------------------------------------------------------
// Per-frame checks
// ---------------------------------------------------------------------------
struct FrameTracker {
    int day = 1;
    float share = 0.5f;
    int demand = 0;
    int winner = 0;
    float dailySeconds = 0.0f;
    std::string msg;
    int settlements = 0;
    double simTime = 0.0;    // game-seconds since 06:00 of day 1, accumulated in double by the test
    double maxClockErr = 0.0;

    void reset(const GameEngine& e) {
        const CityConquestState& c = e.getCityState();
        day = e.getCurrentDay();
        share = c.p1CityShare;
        demand = c.cityEnergyDemand;
        winner = c.winner;
        dailySeconds = c.dailySeconds;
        msg = c.lastCutMessage;
        settlements = 0;
        simTime = Balance::gameSecondsAtHour(Balance::MATCH_START_HOUR);
        maxClockErr = 0.0;
    }
};

bool finite(float v) { return std::isfinite(v); }

void checkEconomyNonNegative(const PlayerEconomy& p, int player) {
    INV(INV_NON_NEGATIVE,
        p.money >= 0 && p.gold >= 0 && p.silver >= 0 && p.iron >= 0 && p.coal >= 0 && p.copper >= 0 && p.silicon >= 0 && p.wood >= 0,
        "P" << player << " money " << p.money << " gold " << p.gold << " silver " << p.silver << " iron " << p.iron << " coal "
            << p.coal << " copper " << p.copper << " silicon " << p.silicon << " wood " << p.wood);
}

void checkFrame(const GameEngine& e, FrameTracker& t, float frameGameSeconds) {
    const CityConquestState& c = e.getCityState();
    const PlayerEconomy& p1 = e.getPlayerEconomy(1);
    const PlayerEconomy& p2 = e.getPlayerEconomy(2);
    const int day = e.getCurrentDay();

    checkEconomyNonNegative(p1, 1);
    checkEconomyNonNegative(p2, 2);

    // Energy and shares finite, share in range
    INV(INV_FINITE, p1.energyMW >= 0 && p2.energyMW >= 0, "energyMW " << p1.energyMW << " / " << p2.energyMW);
    INV(INV_FINITE, finite(c.p1DailyDelivered) && finite(c.p2DailyDelivered) && c.p1DailyDelivered >= 0.0f && c.p2DailyDelivered >= 0.0f,
        "delivered " << c.p1DailyDelivered << " / " << c.p2DailyDelivered);
    INV(INV_FINITE, finite(c.dailySeconds) && c.dailySeconds >= 0.0f && c.dailySeconds <= Balance::SECONDS_PER_DAY + 0.01f,
        "dailySeconds " << c.dailySeconds);
    float avg1 = e.getTodayAverageMW(1), avg2 = e.getTodayAverageMW(2);
    INV(INV_FINITE, finite(avg1) && finite(avg2) && avg1 >= 0.0f && avg2 >= 0.0f, "today avg " << avg1 << " / " << avg2);
    INV(INV_FINITE, finite(e.getHour24()) && e.getHour24() >= 0.0f && e.getHour24() < 24.0f, "hour " << e.getHour24());
    for (const auto& b : e.getBuildings()) {
        if (!(finite(b.currentOutputMW) && finite(b.energyStored) && b.energyStored >= -1e-3f &&
              b.energyStored <= b.maxCapacity + 1e-3f && finite(b.lightRadius) && finite(b.animTimer))) {
            INV(INV_FINITE, false, "building type " << static_cast<int>(b.type) << " out " << b.currentOutputMW << " stored "
                                                    << b.energyStored << "/" << b.maxCapacity << " light " << b.lightRadius);
        }
    }
    INV(INV_SHARE_RANGE, finite(c.p1CityShare) && c.p1CityShare >= 0.0f && c.p1CityShare <= 1.0f, "share " << c.p1CityShare);
    INV(INV_SHARE_RANGE,
        finite(p1.cityInfluence) && finite(p2.cityInfluence) && std::abs(p1.cityInfluence + p2.cityInfluence - 1.0f) < 1e-5f &&
            p1.cityInfluence >= 0.0f && p2.cityInfluence >= 0.0f,
        "influence " << p1.cityInfluence << " + " << p2.cityInfluence);
    INV(INV_TIME_SCALE, finite(e.getTimeScale()) && e.getTimeScale() > 0.0f, "time scale " << e.getTimeScale());

    // Demand schedule at all times
    INV(INV_DEMAND, c.cityEnergyDemand == expectedDemandForDay(day),
        "day " << day << " demand " << c.cityEnergyDemand << " expected " << expectedDemandForDay(day));
    INV(INV_DEMAND, e.isGracePeriod() == (day <= Balance::GRACE_PERIOD_DAYS), "grace flag on day " << day);

    const bool dayChanged = (day != t.day);
    const bool msgChanged = (c.lastCutMessage != t.msg);
    INV(INV_ONE_SETTLEMENT, day >= t.day && day <= t.day + 1, "day jumped " << t.day << " -> " << day);
    INV(INV_ONE_SETTLEMENT, msgChanged == dayChanged || (dayChanged && t.winner == 0 && c.winner != 0),
        "settlement message changed=" << msgChanged << " day changed=" << dayChanged);
    // During a day the counters only grow; they restart at a settlement
    INV(INV_ONE_SETTLEMENT, dayChanged == (c.dailySeconds < t.dailySeconds),
        "dailySeconds " << t.dailySeconds << " -> " << c.dailySeconds << " day " << t.day << " -> " << day);

    if (!dayChanged) {
        INV(INV_ONE_SETTLEMENT, c.p1CityShare == t.share, "share changed inside a day " << t.share << " -> " << c.p1CityShare);
        INV(INV_ONE_SETTLEMENT, c.winner == t.winner, "winner changed inside a day " << t.winner << " -> " << c.winner);
        // Clock: the engine time must equal the time the test fed in (no frame-rate drift)
        float h = e.getHour24() - Balance::CLOCK_HOUR_AT_ZERO;
        if (h < 0.0f) h += 24.0f;
        double engineTime = (day - 1) * static_cast<double>(Balance::SECONDS_PER_DAY) + h / 24.0 * Balance::SECONDS_PER_DAY;
        // (modulo one day: the hour display may already read 06:00 a hair before the settlement)
        double err = std::fmod(std::abs(engineTime - t.simTime), static_cast<double>(Balance::SECONDS_PER_DAY));
        err = std::min(err, Balance::SECONDS_PER_DAY - err);
        t.maxClockErr = std::max(t.maxClockErr, err);
        INV(INV_CLOCK, err < 0.05, "engine clock " << engineTime << " s vs fed time " << t.simTime << " s (error " << err << ")");
    } else {
        ++t.settlements;
        const int endedDay = t.day;
        // The settlement happens on the frame that crosses the end of the day
        double boundary = endedDay * static_cast<double>(Balance::SECONDS_PER_DAY);
        INV(INV_CLOCK, t.simTime >= boundary - 0.05 && t.simTime - frameGameSeconds <= boundary + 0.05,
            "day " << endedDay << " settled at fed time " << t.simTime << " s, boundary " << boundary << " s");
        float delta = c.p1CityShare - t.share;
        INV(INV_SHIFT_CAP, std::abs(delta) <= Balance::MAX_DAILY_CITY_SHIFT + kShareEps, "day " << endedDay << " shift " << delta);
        if (endedDay <= Balance::GRACE_PERIOD_DAYS) {
            INV(INV_SHIFT_CAP, delta == 0.0f, "grace day " << endedDay << " shifted the share by " << delta);
        } else if (delta != 0.0f) {
            bool clamped = (c.p1CityShare <= 0.0f || c.p1CityShare >= 1.0f);
            INV(INV_SHIFT_CAP, clamped || std::abs(delta) >= Balance::MIN_DAILY_CITY_SHIFT - kShareEps,
                "day " << endedDay << " shift " << delta << " below the minimum");
        }
        INV(INV_ONE_SETTLEMENT, c.dayCutOccurred, "dayCutOccurred not set after day " << endedDay);
        INV(INV_ONE_SETTLEMENT, c.dailySeconds <= frameGameSeconds + 1e-3f, "dailySeconds " << c.dailySeconds << " right after a settlement");

        // Winner rules
        if (c.winner != t.winner) {
            INV(INV_WINNER_RULES, t.winner == 0, "winner changed from " << t.winner << " to " << c.winner);
            INV(INV_WINNER_RULES, endedDay > Balance::GRACE_PERIOD_DAYS, "winner during the grace period, day " << endedDay);
            float s1 = c.p1CityShare, s2 = 1.0f - s1;
            bool finalDay = endedDay >= Balance::FINAL_DAY;
            if (c.winner == 1) {
                INV(INV_WINNER_RULES, s1 >= Balance::VICTORY_SHARE - kShareEps || (finalDay && s1 > 0.5f), "P1 won with share " << s1);
            } else if (c.winner == 2) {
                INV(INV_WINNER_RULES, s2 >= Balance::VICTORY_SHARE - kShareEps || (finalDay && s2 > 0.5f), "P2 won with share " << s2);
            } else {
                INV(INV_WINNER_RULES, c.winner == 3 && finalDay && std::abs(s1 - 0.5f) < Balance::DRAW_SHARE_TOLERANCE,
                    "winner code " << c.winner << " share " << s1 << " ended day " << endedDay);
            }
        }
        // A share at or above the victory line must end the match at once
        float s1 = c.p1CityShare;
        if (endedDay > Balance::GRACE_PERIOD_DAYS &&
            (s1 >= Balance::VICTORY_SHARE + kShareEps || 1.0f - s1 >= Balance::VICTORY_SHARE + kShareEps)) {
            INV(INV_WINNER_RULES, c.winner == 1 || c.winner == 2, "share " << s1 << " but no winner after day " << endedDay);
        }
        if (endedDay >= Balance::FINAL_DAY) {
            INV(INV_WINNER_EXISTS, c.winner != 0, "no winner after final day " << endedDay << ", share " << s1);
        }
    }

    t.day = day;
    t.share = c.p1CityShare;
    t.demand = c.cityEnergyDemand;
    t.winner = c.winner;
    t.dailySeconds = c.dailySeconds;
    if (msgChanged) t.msg = c.lastCutMessage;
}

// ---------------------------------------------------------------------------
// Action wrappers that verify every action's contract
// ---------------------------------------------------------------------------
struct EconSnap {
    PlayerEconomy p[3];
    size_t buildings;
    uint64_t hash;
    void take(const GameEngine& e) {
        p[1] = e.getPlayerEconomy(1);
        p[2] = e.getPlayerEconomy(2);
        buildings = e.getBuildings().size();
        hash = hashEngine(e);
    }
};

bool resourcesEqual(const PlayerEconomy& a, const PlayerEconomy& b) {
    return a.money == b.money && a.gold == b.gold && a.silver == b.silver && a.iron == b.iron && a.coal == b.coal &&
           a.copper == b.copper && a.silicon == b.silicon && a.wood == b.wood;
}

bool actMine(GameEngine& e, int player, ResourceType type) {
    EconSnap before;
    before.take(e);
    MineResult r;
    std::string msg;
    bool ok = e.mineResource(player, type, r, msg);
    if (!isValidPlayer(player)) {
        INV(INV_ILLEGAL_REFUSED, !ok && hashEngine(e) == before.hash, "mineResource(player " << player << ") ok=" << ok);
        return ok;
    }
    PlayerEconomy now = e.getPlayerEconomy(player);
    int other = 3 - player;
    INV(INV_ACTION_CONTRACT, resourcesEqual(e.getPlayerEconomy(other), before.p[other]), "mining changed the other player");
    if (baseYield(type) > 0) {
        int lvl = e.getMineLevel(player, type);
        int expected = static_cast<int>(std::round(baseYield(type) * Balance::getMineYieldMultiplier(lvl)));
        PlayerEconomy exp = before.p[player];
        *resourceField(exp, type) += expected;
        INV(INV_ACTION_CONTRACT, ok && resourcesEqual(now, exp) && r.amount == expected,
            "mine type " << static_cast<int>(type) << " level " << lvl << " ok=" << ok << " amount " << r.amount << " expected " << expected);
    } else if (type == ResourceType::ORE) {
        INV(INV_ACTION_CONTRACT, ok, "legacy ORE mining refused");
    } else {
        INV(INV_ILLEGAL_REFUSED, !ok && hashEngine(e) == before.hash, "mineResource(type " << static_cast<int>(type) << ") ok=" << ok);
    }
    return ok;
}

bool actUpgrade(GameEngine& e, int player, ResourceType type) {
    EconSnap before;
    before.take(e);
    int cost = isValidPlayer(player) ? e.getMineUpgradeCost(player, type) : 0;
    int lvl = isValidPlayer(player) ? e.getMineLevel(player, type) : 0;
    std::string msg;
    bool ok = e.upgradeMine(player, type, msg);
    if (!isValidPlayer(player)) {
        INV(INV_ILLEGAL_REFUSED, !ok && hashEngine(e) == before.hash, "upgradeMine(player " << player << ") ok=" << ok);
        return ok;
    }
    if (ok) {
        PlayerEconomy exp = before.p[player];
        exp.gold -= cost;
        INV(INV_ACTION_CONTRACT, cost > 0 && resourcesEqual(e.getPlayerEconomy(player), exp) && e.getMineLevel(player, type) == lvl + 1,
            "upgrade type " << static_cast<int>(type) << " cost " << cost << " level " << lvl << " -> " << e.getMineLevel(player, type));
        INV(INV_ILLEGAL_REFUSED, baseYield(type) > 0, "upgradeMine accepted resource type " << static_cast<int>(type));
    } else {
        INV(INV_ACTION_CONTRACT, hashEngine(e) == before.hash, "refused upgrade changed the state: " << msg);
    }
    return ok;
}

bool actBuyPlot(GameEngine& e, int player, int plotId, bool next) {
    EconSnap before;
    before.take(e);
    int expectedId = plotId;
    if (next && isValidPlayer(player)) {
        expectedId = -1;
        int best = std::numeric_limits<int>::max();
        for (const auto& p : e.getLandPlots()) {
            if (p.playerOwner == player && !p.isPurchased && p.costGold < best) {
                best = p.costGold;
                expectedId = p.id;
            }
        }
    }
    const LandPlot* plot = plotById(e, expectedId);
    int cost = plot ? plot->costGold : 0;
    bool wasOwnedAndFree = plot && plot->playerOwner == player && !plot->isPurchased;
    std::string msg;
    bool ok = next ? e.buyNextLandTier(player, msg) : e.buyLandPlot(player, plotId, msg);
    if (!isValidPlayer(player)) {
        INV(INV_ILLEGAL_REFUSED, !ok && hashEngine(e) == before.hash, "buy land (player " << player << ") ok=" << ok);
        return ok;
    }
    if (ok) {
        PlayerEconomy exp = before.p[player];
        exp.gold -= cost;
        INV(INV_ACTION_CONTRACT, wasOwnedAndFree && resourcesEqual(e.getPlayerEconomy(player), exp) && plotPurchased(e, expectedId),
            "bought plot " << expectedId << " cost " << cost << " gold " << before.p[player].gold << " -> " << e.getPlayerEconomy(player).gold);
    } else {
        INV(INV_ACTION_CONTRACT, hashEngine(e) == before.hash, "refused land purchase changed the state: " << msg);
        if (wasOwnedAndFree && before.p[player].gold >= cost) {
            INV(INV_ACTION_CONTRACT, false, "affordable plot " << expectedId << " refused: " << msg);
        }
    }
    return ok;
}

bool actPlace(GameEngine& e, int player, BuildingType type, sf::Vector2f pos) {
    EconSnap before;
    before.take(e);
    std::string reason, msg;
    bool preview = e.canPlaceBuilding(player, type, pos, reason);
    INV(INV_ACTION_CONTRACT, hashEngine(e) == before.hash, "canPlaceBuilding changed the state");
    bool ok = e.placeBuilding(player, type, pos, msg);
    INV(INV_PREVIEW_MATCHES, preview == ok, "preview " << preview << " (" << reason << ") vs placeBuilding " << ok << " (" << msg << ")");

    bool legalInput = isValidPlayer(player) && (isBuildable(type) || type == BuildingType::DEMOLISH) &&
                      std::isfinite(pos.x) && std::isfinite(pos.y);
    if (!legalInput) {
        INV(INV_ILLEGAL_REFUSED, !ok && hashEngine(e) == before.hash,
            "placeBuilding(player " << player << ", type " << static_cast<int>(type) << ", pos " << pos.x << "; " << pos.y << ") ok=" << ok);
        return ok;
    }
    if (!ok) {
        INV(INV_ACTION_CONTRACT, hashEngine(e) == before.hash, "refused placement changed the state: " << msg);
        return ok;
    }
    const PlayerEconomy& now = e.getPlayerEconomy(player);
    int other = 3 - player;
    INV(INV_ACTION_CONTRACT, resourcesEqual(e.getPlayerEconomy(other), before.p[other]), "placement changed the other player");
    if (type == BuildingType::DEMOLISH) {
        INV(INV_ACTION_CONTRACT, e.getBuildings().size() + 1 == before.buildings, "demolish removed " << (before.buildings - e.getBuildings().size()));
        // refund: never more than half of any recipe, never negative
        INV(INV_ACTION_CONTRACT, now.wood >= before.p[player].wood && now.iron >= before.p[player].iron && now.money == before.p[player].money &&
                                     now.gold == before.p[player].gold,
            "demolish refund wood " << before.p[player].wood << " -> " << now.wood);
    } else {
        BuildingCost cost = e.getBuildingCost(type);
        PlayerEconomy exp = before.p[player];
        exp.wood -= cost.woodCost; exp.iron -= cost.ironCost; exp.copper -= cost.copperCost;
        exp.coal -= cost.coalCost; exp.silicon -= cost.siliconCost; exp.silver -= cost.silverCost;
        INV(INV_ACTION_CONTRACT, resourcesEqual(now, exp) && e.getBuildings().size() == before.buildings + 1,
            "placement of type " << static_cast<int>(type) << " charged wrong amounts or did not add a building");
        const PlacedBuilding& b = e.getBuildings().back();
        INV(INV_ACTION_CONTRACT, b.type == type && b.playerOwner == player && b.energyStored == 0.0f, "new building fields");
    }
    return ok;
}

bool actRemove(GameEngine& e, int player, sf::Vector2f pos) {
    EconSnap before;
    before.take(e);
    std::string msg;
    bool ok = e.removeBuilding(player, pos, msg);
    if (!isValidPlayer(player) || !std::isfinite(pos.x) || !std::isfinite(pos.y)) {
        INV(INV_ILLEGAL_REFUSED, !ok && hashEngine(e) == before.hash, "removeBuilding(player " << player << ") ok=" << ok);
        return ok;
    }
    if (ok) {
        INV(INV_ACTION_CONTRACT, e.getBuildings().size() + 1 == before.buildings, "remove did not remove exactly one building");
    } else {
        INV(INV_ACTION_CONTRACT, hashEngine(e) == before.hash, "refused removal changed the state");
    }
    return ok;
}

bool actRepair(GameEngine& e, int player, sf::Vector2f pos) {
    EconSnap before;
    before.take(e);
    std::string msg;
    bool ok = e.repairBuilding(player, pos, msg);
    if (!ok) INV(INV_ACTION_CONTRACT, hashEngine(e) == before.hash, "refused repair changed the state");
    if (!isValidPlayer(player)) INV(INV_ILLEGAL_REFUSED, !ok, "repair with player " << player);
    return ok;
}

// Lightning (as UI_map_particles does): strike the exact position of one building
void actLightning(GameEngine& e, Rng& rng) {
    const auto& bs = e.getBuildings();
    if (bs.empty()) return;
    size_t idx = static_cast<size_t>(rng.next() % bs.size());
    PlacedBuilding target = bs[idx];
    size_t before = bs.size();
    bool ok = e.breakBuildingAt(target.position);
    INV(INV_ACTION_CONTRACT, ok && e.getBuildings().size() + 1 == before, "lightning did not destroy one building");
    for (const auto& b : e.getBuildings()) {
        if (b.position == target.position && b.playerOwner == target.playerOwner) {
            INV(INV_ACTION_CONTRACT, false, "lightning destroyed a different building than the one struck");
        }
    }
}

void actTimeScale(GameEngine& e, float scale) {
    e.setTimeScale(scale);
    float ts = e.getTimeScale();
    INV(INV_TIME_SCALE, std::isfinite(ts) && ts > 0.0f, "setTimeScale(" << scale << ") -> " << ts);
    // Odd values must not freeze or explode the clock
    INV(INV_TIME_SCALE, ts <= 100.0f, "setTimeScale(" << scale << ") accepted " << ts);
}

// ---------------------------------------------------------------------------
// Strategies
// ---------------------------------------------------------------------------
enum Strategy { STRAT_DUEL = 0, STRAT_RUSH = 1, STRAT_RANDOM = 2, STRAT_COUNT = 3 };
const char* const kStrategyNames[STRAT_COUNT] = { "duel", "rush", "random" };

struct Builder {
    int player = 1;
    bool active = true;
    float period = 1.0f;   // game-seconds between actions (like the mining cooldown)
    float timer = 0.0f;
    int next = 0;
    int style = 0;         // 0 = wind-heavy, 1 = solar + battery
    int cap = 1000;        // stops building at this many own buildings (then only mines and upgrades)
};

const BuildingType kOrderWind[] = { BuildingType::WIND_TURBINE, BuildingType::WIND_TURBINE, BuildingType::HYDRO_PLANT,
                                    BuildingType::SOLAR_PANEL, BuildingType::BATTERY, BuildingType::WIND_TURBINE,
                                    BuildingType::HYDRO_PLANT, BuildingType::WIND_TURBINE };
const BuildingType kOrderSolar[] = { BuildingType::SOLAR_PANEL, BuildingType::SOLAR_PANEL, BuildingType::BATTERY,
                                     BuildingType::WIND_TURBINE, BuildingType::SOLAR_PANEL, BuildingType::HYDRO_PLANT,
                                     BuildingType::BATTERY, BuildingType::WIND_TURBINE };

bool canAfford(const GameEngine& e, int player, BuildingType t) {
    const PlayerEconomy& ec = e.getPlayerEconomy(player);
    BuildingCost c = e.getBuildingCost(t);
    return ec.wood >= c.woodCost && ec.iron >= c.ironCost && ec.copper >= c.copperCost && ec.coal >= c.coalCost &&
           ec.silicon >= c.siliconCost && ec.silver >= c.silverCost;
}

ResourceType mostLacking(const GameEngine& e, int player, BuildingType t) {
    const PlayerEconomy& ec = e.getPlayerEconomy(player);
    BuildingCost c = e.getBuildingCost(t);
    struct Need { ResourceType t; int deficit; };
    Need needs[] = { { ResourceType::WOOD, c.woodCost - ec.wood }, { ResourceType::IRON, c.ironCost - ec.iron },
                     { ResourceType::COPPER, c.copperCost - ec.copper }, { ResourceType::COAL, c.coalCost - ec.coal },
                     { ResourceType::SILICON, c.siliconCost - ec.silicon }, { ResourceType::SILVER, c.silverCost - ec.silver } };
    ResourceType best = ResourceType::GOLD;
    float bestScore = 0.0f;
    for (const auto& n : needs) {
        if (n.deficit <= 0) continue;
        float score = static_cast<float>(n.deficit) / static_cast<float>(baseYield(n.t));
        if (score > bestScore) {
            bestScore = score;
            best = n.t;
        }
    }
    return best;
}

// First free purchased slot that suits the building; false when none
bool findFreeSlot(const GameEngine& e, const Occupancy& occ, int player, BuildingType t, sf::Vector2f& out) {
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            if (occ.used[player][r][c]) continue;
            if (!plotPurchased(e, plotIdOfSlot(player, c, r))) continue;
            bool river = isRiverCol(player, c);
            if (t == BuildingType::HYDRO_PLANT && !river) continue;
            if (t != BuildingType::HYDRO_PLANT && river && t != BuildingType::LAMP) {
                // keep river slots for hydro while other land is free
                continue;
            }
            out = e.getGridSlot(player, c, r);
            return true;
        }
    }
    if (t != BuildingType::HYDRO_PLANT) { // fall back to river slots
        for (int r = 0; r < kRows; ++r)
            for (int c = 0; c < kCols; ++c)
                if (!occ.used[player][r][c] && plotPurchased(e, plotIdOfSlot(player, c, r)) && isRiverCol(player, c)) {
                    out = e.getGridSlot(player, c, r);
                    return true;
                }
    }
    return false;
}

void builderAct(GameEngine& e, Builder& b, Rng& rng) {
    const int p = b.player;
    const PlayerEconomy& ec = e.getPlayerEconomy(p);
    Occupancy occ;
    occ.build(e);

    // At the cap: keep the economy busy (mining, upgrades) without building more
    if (occ.count[p] >= b.cap) {
        ResourceType t = static_cast<ResourceType>(rng.range(static_cast<int>(ResourceType::WOOD), static_cast<int>(ResourceType::GOLD)));
        int cost = e.getMineUpgradeCost(p, t);
        if (cost > 0 && cost <= ec.gold && rng.chance(0.2f)) actUpgrade(e, p, t);
        else actMine(e, p, t);
        return;
    }

    const BuildingType* order = (b.style == 0) ? kOrderWind : kOrderSolar;
    BuildingType target = order[b.next % 8];

    // Land: buy the cheapest plot when the owned land is full or no river bank is owned for hydro
    sf::Vector2f slot;
    bool hasSlot = findFreeSlot(e, occ, p, target, slot);
    if (!hasSlot) {
        int cheapest = std::numeric_limits<int>::max();
        int riverId = -1, riverCost = std::numeric_limits<int>::max();
        for (const auto& plot : e.getLandPlots()) {
            if (plot.playerOwner != p || plot.isPurchased) continue;
            cheapest = std::min(cheapest, plot.costGold);
            int idx = (plot.id - 1) % 12;
            if ((idx % 3) == (p == 1 ? Balance::P1_RIVER_BANK_PLOT_COL : Balance::P2_RIVER_BANK_PLOT_COL) && plot.costGold < riverCost) {
                riverCost = plot.costGold;
                riverId = plot.id;
            }
        }
        if (target == BuildingType::HYDRO_PLANT && riverId > 0) {
            if (ec.gold >= riverCost) { actBuyPlot(e, p, riverId, false); return; }
            if (occ.count[p] > 0 && rng.chance(0.5f)) { b.next++; return; } // skip hydro for now
        } else if (cheapest != std::numeric_limits<int>::max() && ec.gold >= cheapest) {
            actBuyPlot(e, p, 0, true);
            return;
        }
        actMine(e, p, ResourceType::GOLD);
        return;
    }

    // Upgrade a basic mine now and then when gold is plentiful
    if (ec.gold >= 120 && rng.chance(0.3f)) {
        ResourceType t = static_cast<ResourceType>(rng.range(static_cast<int>(ResourceType::WOOD), static_cast<int>(ResourceType::GOLD)));
        int cost = e.getMineUpgradeCost(p, t);
        if (cost > 0 && cost <= ec.gold / 2) { actUpgrade(e, p, t); return; }
    }

    if (!canAfford(e, p, target)) {
        actMine(e, p, mostLacking(e, p, target));
        return;
    }
    // Night work needs light: put a lamp first
    if (!e.isAreaIlluminated(p, slot) && target != BuildingType::LAMP) {
        if (canAfford(e, p, BuildingType::LAMP)) {
            sf::Vector2f lampSlot;
            if (findFreeSlot(e, occ, p, BuildingType::LAMP, lampSlot)) actPlace(e, p, BuildingType::LAMP, lampSlot);
        } else {
            actMine(e, p, mostLacking(e, p, BuildingType::LAMP));
        }
        return;
    }
    if (actPlace(e, p, target, slot)) b.next++;
}

// Random legal (and some illegal) actions
sf::Vector2f randomPosition(const GameEngine& e, Rng& rng, int player) {
    int kind = rng.range(0, 9);
    if (kind <= 5) return e.getGridSlot(isValidPlayer(player) ? player : 1, rng.range(0, kCols - 1), rng.range(0, kRows - 1));
    if (kind <= 7) return sf::Vector2f(rng.uniform() * 1800.0f - 100.0f, rng.uniform() * 1100.0f - 100.0f);
    if (kind == 8) {
        // jitter around a slot (still inside the cell)
        sf::Vector2f s = e.getGridSlot(isValidPlayer(player) ? player : 2, rng.range(0, kCols - 1), rng.range(0, kRows - 1));
        return sf::Vector2f(s.x + (rng.uniform() - 0.5f) * 30.0f, s.y + (rng.uniform() - 0.5f) * 26.0f);
    }
    const float odd[] = { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                          -std::numeric_limits<float>::infinity(), 1e30f, -1e30f, 0.0f };
    return sf::Vector2f(odd[rng.range(0, 5)], odd[rng.range(0, 5)]);
}

void randomAct(GameEngine& e, int player, Rng& rng, bool allowIllegal, int buildCap) {
    int p = player;
    if (allowIllegal && rng.chance(0.03f)) {
        const int bad[] = { 0, 3, -1, 99 };
        p = bad[rng.range(0, 3)];
    }
    int roll = rng.range(0, 99);
    if (roll < 45) {
        int t = rng.range(static_cast<int>(ResourceType::WOOD), static_cast<int>(ResourceType::GOLD));
        if (allowIllegal && rng.chance(0.05f)) {
            const int odd[] = { 0, 8, 9, 10, 42, -3 };
            t = odd[rng.range(0, 5)];
        }
        actMine(e, p, static_cast<ResourceType>(t));
    } else if (roll < 70) {
        int t = rng.range(1, 6);
        if (allowIllegal && rng.chance(0.05f)) {
            const int odd[] = { 0, 7, 9, -2, 100 };
            t = odd[rng.range(0, 4)];
        }
        BuildingType bt = static_cast<BuildingType>(t);
        sf::Vector2f pos = randomPosition(e, rng, p);
        if (isValidPlayer(p) && bt != BuildingType::DEMOLISH) {
            // at the cap the player demolishes one of its buildings instead (keeps the grid churning)
            std::vector<sf::Vector2f> own;
            for (const auto& b : e.getBuildings())
                if (b.playerOwner == p) own.push_back(b.position);
            if (static_cast<int>(own.size()) >= buildCap) {
                bt = BuildingType::DEMOLISH;
                pos = own[static_cast<size_t>(rng.next() % own.size())];
            }
        }
        if (isValidPlayer(p) && bt != BuildingType::DEMOLISH && rng.chance(0.6f)) {
            Occupancy occ;
            occ.build(e);
            sf::Vector2f s;
            if (findFreeSlot(e, occ, p, bt, s)) pos = s;
        }
        actPlace(e, p, bt, pos);
    } else if (roll < 74) {
        actRemove(e, p, randomPosition(e, rng, p));
    } else if (roll < 79) {
        actBuyPlot(e, p, rng.range(-2, 26), false);
    } else if (roll < 83) {
        actBuyPlot(e, p, 0, true);
    } else if (roll < 88) {
        int t = rng.range(0, 10);
        actUpgrade(e, p, static_cast<ResourceType>(t));
    } else if (roll < 92) {
        uint64_t before = hashEngine(e);
        int k = rng.range(0, 2);
        if (k == 0) e.cycleBuildingSelection(p);
        else if (k == 1) e.cycleBuildingSelectionPrev(p);
        else e.clearBuildingSelection(p);
        if (isValidPlayer(p)) {
            int sel = e.getPlayerEconomy(p).selectedBuilding;
            INV(INV_ACTION_CONTRACT, sel >= 0 && sel <= 6, "selectedBuilding " << sel);
        } else {
            INV(INV_ILLEGAL_REFUSED, hashEngine(e) == before, "building selection with player " << p << " changed the state");
        }
    } else if (roll < 94) {
        actRepair(e, p, randomPosition(e, rng, p));
    } else if (roll < 96) {
        // read-only queries with odd input must not change anything
        uint64_t before = hashEngine(e);
        sf::Vector2f pos = randomPosition(e, rng, p);
        int c = 0, r = 0;
        e.getClosestGridIndex(p, pos, c, r);
        INV(INV_FINITE, c >= 0 && c < kCols && r >= 0 && r < kRows, "closest grid index " << c << "," << r);
        sf::Vector2f s = e.snapToBuildingGrid(p, pos);
        INV(INV_FINITE, std::isfinite(s.x) && std::isfinite(s.y), "snap gave " << s.x << "; " << s.y);
        (void)e.isAreaIlluminated(p, pos);
        (void)e.isRiverBankSlot(p, pos);
        (void)e.getMineUpgradeCost(p, static_cast<ResourceType>(rng.range(-1, 12)));
        (void)e.getBuildingCost(static_cast<BuildingType>(rng.range(-1, 9)));
        INV(INV_ACTION_CONTRACT, hashEngine(e) == before, "a query changed the state");
    } else if (roll < 97) {
        if (!e.isGracePeriod()) actLightning(e, rng);
    } else {
        // the UI toggles 1x / 6x (mining speed-up); odd values are fuzz
        float scale = rng.chance(0.5f) ? Balance::MINE_SPEEDUP_MULT : 1.0f;
        if (allowIllegal && rng.chance(0.2f)) {
            const float odd[] = { 0.0f, -2.0f, 0.05f, 2.5f, std::numeric_limits<float>::quiet_NaN(),
                                  std::numeric_limits<float>::infinity(), 1e9f, 40.0f };
            scale = odd[rng.range(0, 7)];
        }
        actTimeScale(e, scale);
    }
}

// ---------------------------------------------------------------------------
// One complete match
// ---------------------------------------------------------------------------
struct MatchResult {
    int winner = 0;
    int endDay = 0;
    float share = 0.5f;
    uint64_t trace = 0;
    long frames = 0;
    int settlements = 0;
    double maxClockErr = 0.0;
    int buildingsPeak = 0;
};

MatchResult playMatch(unsigned seed, float dt, Strategy strat, bool restartChecks, int buildCap) {
    g_seed = seed;
    g_dt = dt;
    g_strategy = kStrategyNames[strat];

    setSeedEnv(seed);
    GameEngine e;
    e.init(1600.0f, 900.0f);
    g_engine = &e;

    Rng rng(seed * 1000003ull + static_cast<uint64_t>(strat) * 7919ull);
    Builder b1, b2;
    b1.player = 1; b2.player = 2;
    b1.style = 0; b2.style = 1;
    b2.timer = 0.5f; // P2 acts half a second after P1
    b1.cap = b2.cap = buildCap;
    bool idleBoth = false;
    if (strat == STRAT_RUSH) {
        int mode = seed % 3; // 0: P1 builds, 1: P2 builds, 2: nobody builds (draw)
        b1.active = (mode == 0);
        b2.active = (mode == 1);
        idleBoth = (mode == 2);
        b1.period = b2.period = 0.5f;
    }
    float randomTimer = 0.0f;
    const float randomPeriod = 0.4f;

    FrameTracker t;
    t.reset(e);
    checkStructure(e);
    Hasher trace;
    MatchResult res;

    const long maxFrames = static_cast<long>((Balance::FINAL_DAY + 1) * Balance::SECONDS_PER_DAY / dt) + 100;
    long frames = 0;
    while (e.getCityState().winner == 0) {
        // ---- player actions (paced in game time, so they do not depend on the frame size)
        float ts = e.getTimeScale();
        float gameStep = dt * ts;
        bool acted = false;
        if (strat == STRAT_RANDOM) {
            randomTimer += gameStep;
            while (randomTimer >= randomPeriod) {
                randomTimer -= randomPeriod;
                randomAct(e, 1, rng, true, buildCap);
                randomAct(e, 2, rng, true, buildCap);
                acted = true;
            }
        } else if (!idleBoth) {
            Builder* bs[2] = { &b1, &b2 };
            for (Builder* b : bs) {
                if (!b->active) continue;
                b->timer += gameStep;
                while (b->timer >= b->period) {
                    b->timer -= b->period;
                    builderAct(e, *b, rng);
                    acted = true;
                }
            }
        }
        if (acted) {
            checkStructure(e);
            checkEconomyNonNegative(e.getPlayerEconomy(1), 1);
            checkEconomyNonNegative(e.getPlayerEconomy(2), 2);
            res.buildingsPeak = std::max(res.buildingsPeak, static_cast<int>(e.getBuildings().size()));
        }

        // ---- one frame
        ts = e.getTimeScale();
        e.update(dt);
        t.simTime += static_cast<double>(dt) * ts;
        ++frames;
        checkFrame(e, t, dt * ts);
        if (e.getCurrentDay() != t.day || frames % 4096 == 0) {
            // cheap trace of the match for the determinism check
        }
        if (t.settlements != res.settlements) {
            res.settlements = t.settlements;
            trace.i(hashEngine(e));
        }
        if (frames > maxFrames) {
            INV(INV_NO_HANG, false, "match still running after " << frames << " frames (day " << e.getCurrentDay() << ")");
            break;
        }
    }
    INV(INV_NO_HANG, e.getCityState().winner != 0, "no result");
    INV(INV_WINNER_EXISTS, e.getCityState().winner != 0 && e.getCurrentDay() <= Balance::FINAL_DAY + 1,
        "winner " << e.getCityState().winner << " day " << e.getCurrentDay());
    if (idleBoth) {
        INV(INV_WINNER_RULES, e.getCityState().winner == 3 && e.getCurrentDay() == Balance::FINAL_DAY + 1,
            "two idle players: winner " << e.getCityState().winner << " day " << e.getCurrentDay());
    }

    // ---- frozen after the match ended (update() must do nothing)
    {
        uint64_t before = hashEngine(e);
        for (int k = 0; k < 30; ++k) e.update(dt);
        e.update(5.0f);
        INV(INV_FROZEN, hashEngine(e) == before, "update() after the end changed the state");
    }

    res.winner = e.getCityState().winner;
    res.endDay = e.getCurrentDay();
    res.share = e.getCityState().p1CityShare;
    trace.i(hashEngine(e));
    res.trace = trace.h;
    res.frames = frames;
    res.maxClockErr = t.maxClockErr;

    // ---- restart resets everything: equal to a fresh engine with the same seed
    if (restartChecks) {
        setSeedEnv(seed);
        e.setTimeScale(Balance::MINE_SPEEDUP_MULT);
        e.restartGame();
        uint64_t restarted = hashEngine(e);
        INV(INV_RESTART, e.getCurrentDay() == 1 && e.getCityState().winner == 0 && e.getBuildings().empty() &&
                             std::abs(e.getCityState().p1CityShare - 0.5f) < 1e-6f && e.getCityState().cityEnergyDemand == 0 &&
                             e.getPlayerEconomy(1).money == 0 && e.getPlayerEconomy(2).gold == 0 && e.getTimeScale() == 1.0f,
            "restart left match state behind");
        // The weather RNG is shared by all engines in the process (game_random.cpp), so the two
        // engines are compared one after the other, each right after its own (re)seeding.
        for (int k = 0; k < 2000; ++k) e.update(0.25f);
        uint64_t restartedLater = hashEngine(e);
        setSeedEnv(seed);
        GameEngine fresh;
        fresh.init(1600.0f, 900.0f);
        INV(INV_RESTART, restarted == hashEngine(fresh), "restartGame() state differs from a fresh engine");
        for (int k = 0; k < 2000; ++k) fresh.update(0.25f);
        INV(INV_RESTART, restartedLater == hashEngine(fresh), "restarted match diverged from a fresh one");
    }
    g_engine = nullptr;
    return res;
}

// Restart in the middle of a running match: the engine must forget everything
void midMatchRestartCheck(unsigned seed) {
    g_seed = seed;
    g_dt = 1.0f / 60.0f;
    g_strategy = "mid-restart";
    setSeedEnv(seed);
    GameEngine e;
    e.init(1600.0f, 900.0f);
    g_engine = &e;
    Rng rng(seed);
    // play 4.5 days of random actions
    float timer = 0.0f;
    for (long f = 0; f < static_cast<long>(4.5f * Balance::SECONDS_PER_DAY * 60.0f) && e.getCityState().winner == 0; ++f) {
        timer += 1.0f / 60.0f * e.getTimeScale();
        while (timer >= 0.25f) {
            timer -= 0.25f;
            randomAct(e, 1, rng, false, 1000);
            randomAct(e, 2, rng, false, 1000);
        }
        e.update(1.0f / 60.0f);
    }
    setSeedEnv(seed + 1);
    e.restartGame();
    setSeedEnv(seed + 1);
    GameEngine fresh;
    fresh.init(1600.0f, 900.0f);
    INV(INV_RESTART, hashEngine(e) == hashEngine(fresh), "mid-match restartGame() state differs from a fresh engine");
    g_engine = nullptr;
}

} // namespace

namespace {

// One planned match
struct Job {
    int strat;
    int dtIdx;
    int seedIdx;
};

const float kDts[3] = { 1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 144.0f };
const int kDtCount = 3;

unsigned seedOf(int seedIdx) { return 1000u + static_cast<unsigned>(seedIdx) * 7u; }

// Scripted and random players fill every slot (216 buildings) on every 4th seed and stop at a cap
// player otherwise (40 per player): full sectors cost ~3x more per step and real matches rarely get there.
int buildCapOf(int seedIdx) { return (seedIdx % 4 == 0) ? 1000 : 40; }

} // namespace

int main() {
    const auto t0 = std::chrono::steady_clock::now();
    // EC_SOAK_SEEDS=n plays the full matrix n seeds x 3 frame sizes x 3 strategies (EC_SOAK_FIRST=k
    // shifts the seeds, for parallel shards). Without it a standard plan sized for ~60 s unoptimised.
    const int fullSeeds = envInt("EC_SOAK_SEEDS", 0);
    const int firstSeed = std::max(0, envInt("EC_SOAK_FIRST", 0));
    const bool verbose = envInt("EC_SOAK_VERBOSE", 0) != 0;

    std::vector<Job> jobs;
    std::vector<Job> replays;
    if (fullSeeds > 0) {
        for (int s = 0; s < STRAT_COUNT; ++s)
            for (int d = 0; d < kDtCount; ++d)
                for (int k = firstSeed; k < firstSeed + fullSeeds; ++k) jobs.push_back(Job{ s, d, k });
        for (int s = 0; s < STRAT_COUNT; ++s)
            for (int d = 0; d < kDtCount; ++d)
                for (int k = firstSeed; k < firstSeed + std::min(fullSeeds, 2); ++k) replays.push_back(Job{ s, d, k });
    } else {
        // standard plan (~60 s unoptimised): several seeds at 30 FPS, a frame-size sweep at 60 / 144 FPS
        // on fewer seeds (a 144 FPS match is 258 000 frames); seed 0 and 4 fill every slot
        const int seeds30[STRAT_COUNT] = { 4, 8, 6 };
        for (int s = 0; s < STRAT_COUNT; ++s) {
            for (int k = 0; k < seeds30[s]; ++k) jobs.push_back(Job{ s, 0, k });
            jobs.push_back(Job{ s, 1, 1 });
            if (s != STRAT_DUEL) jobs.push_back(Job{ s, 2, 1 });
            if (s == STRAT_RUSH) {
                jobs.push_back(Job{ s, 1, 2 });
                jobs.push_back(Job{ s, 2, 2 });
            }
            replays.push_back(Job{ s, 0, 1 });
        }
        replays.push_back(Job{ STRAT_RUSH, 2, 2 });
    }

    std::cout << "Energy Crisis engine soak: " << jobs.size() << " matches (strategies duel / rush / random, frames 1/30, 1/60, 1/144 s)"
              << " to the end of day " << Balance::FINAL_DAY << ", " << replays.size() << " replays\n";

    // results by (strategy, dt, seed)
    std::vector<MatchResult> results(jobs.size());
    auto findResult = [&](int s, int d, int k) -> const MatchResult* {
        for (size_t i = 0; i < jobs.size(); ++i)
            if (jobs[i].strat == s && jobs[i].dtIdx == d && jobs[i].seedIdx == k) return &results[i];
        return nullptr;
    };

    long long totalFrames = 0;
    int wins[4] = { 0, 0, 0, 0 };
    double worstClock[kDtCount] = { 0.0, 0.0, 0.0 };
    double stratSeconds[STRAT_COUNT] = { 0.0, 0.0, 0.0 };
    int randomAgree = 0, randomCompared = 0;
    {
        QuietCout quiet;
        std::vector<int> restartDone(STRAT_COUNT, 0);
        for (size_t i = 0; i < jobs.size(); ++i) {
            const Job& j = jobs[i];
            bool restartChecks = (restartDone[j.strat] < 2);
            restartDone[j.strat] += restartChecks ? 1 : 0;
            auto m0 = std::chrono::steady_clock::now();
            MatchResult r = playMatch(seedOf(j.seedIdx), kDts[j.dtIdx], static_cast<Strategy>(j.strat), restartChecks, buildCapOf(j.seedIdx));
            double ms = std::chrono::duration<double>(std::chrono::steady_clock::now() - m0).count();
            stratSeconds[j.strat] += ms;
            results[i] = r;
            totalFrames += r.frames;
            wins[std::min(3, std::max(0, r.winner))]++;
            worstClock[j.dtIdx] = std::max(worstClock[j.dtIdx], r.maxClockErr);
            if (verbose) {
                std::cerr << kStrategyNames[j.strat] << " dt 1/" << std::lround(1.0f / kDts[j.dtIdx]) << " seed " << seedOf(j.seedIdx)
                          << ": winner " << r.winner << " day " << r.endDay << " share " << r.share << " frames " << r.frames
                          << " peak buildings " << r.buildingsPeak << " clock err " << r.maxClockErr << " (" << ms << " s)\n";
            }
        }

        // Determinism: replay matches with identical seeds, compare the per-day state trace
        for (const Job& j : replays) {
            const MatchResult* first = findResult(j.strat, j.dtIdx, j.seedIdx);
            if (!first) continue;
            MatchResult again = playMatch(seedOf(j.seedIdx), kDts[j.dtIdx], static_cast<Strategy>(j.strat), false, buildCapOf(j.seedIdx));
            g_seed = seedOf(j.seedIdx); g_dt = kDts[j.dtIdx]; g_strategy = kStrategyNames[j.strat];
            INV(INV_DETERMINISM, again.trace == first->trace && again.winner == first->winner && again.frames == first->frames,
                "replay differs: winner " << first->winner << " vs " << again.winner << ", frames " << first->frames << " vs " << again.frames);
        }

        // Frame-size independence: the scripted rush must end the same way at every frame rate
        // (the random strategy is only reported: its actions land on different frames)
        for (size_t i = 0; i < jobs.size(); ++i) {
            const Job& j = jobs[i];
            if (j.dtIdx == 0 || (j.strat != STRAT_RUSH && j.strat != STRAT_RANDOM)) continue;
            const MatchResult* ref = findResult(j.strat, 0, j.seedIdx);
            if (!ref) continue;
            const MatchResult& r = results[i];
            g_seed = seedOf(j.seedIdx); g_dt = kDts[j.dtIdx]; g_strategy = kStrategyNames[j.strat];
            if (j.strat == STRAT_RUSH) {
                INV(INV_FRAME_INDEPENDENT, r.winner == ref->winner && r.endDay == ref->endDay,
                    "1/30: winner " << ref->winner << " day " << ref->endDay << " vs winner " << r.winner << " day " << r.endDay);
            } else {
                ++randomCompared;
                randomAgree += (r.winner == ref->winner && r.endDay == ref->endDay) ? 1 : 0;
            }
        }

        for (unsigned k = 0; k < 3; ++k) midMatchRestartCheck(5000u + static_cast<unsigned>(firstSeed) * 3u + k);
    }

    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    // ---- report
    int failedInvariants = 0;
    std::cout << "\nInvariant                                           checks        fails\n";
    for (int i = 0; i < INV_COUNT; ++i) {
        const InvStat& s = g_inv[i];
        std::cout << "  " << std::left << std::setw(50) << kInvNames[i] << std::right << std::setw(12) << s.checks << std::setw(10)
                  << s.fails << (s.fails ? "  FAIL" : (s.checks ? "  ok" : "  (not exercised)")) << "\n";
        if (s.fails) {
            ++failedInvariants;
            for (const auto& m : s.samples) std::cout << "      " << m << "\n";
        }
    }
    std::cout << "\nMatches: " << jobs.size() << " + " << replays.size() << " replays, frames: " << totalFrames << ", results: P1 "
              << wins[1] << ", P2 " << wins[2] << ", draw " << wins[3] << ", none " << wins[0] << "\n";
    std::cout << "Random strategy, same outcome at 60/144 FPS as at 30 FPS: " << randomAgree << "/" << randomCompared << "\n";
    std::cout << "Worst clock error (s): 1/30 " << worstClock[0] << ", 1/60 " << worstClock[1] << ", 1/144 " << worstClock[2] << "\n";
    std::cout << std::fixed << std::setprecision(1) << "Time per strategy (s): duel " << stratSeconds[0] << ", rush " << stratSeconds[1]
              << ", random " << stratSeconds[2] << "\n";
    std::cout << "Runtime: " << secs << " s\n";
    if (failedInvariants) {
        std::cout << "SOAK FAILED: " << failedInvariants << " invariant(s) violated.\n";
        return 1;
    }
    std::cout << "SOAK PASSED: all invariants held.\n";
    return 0;
}
