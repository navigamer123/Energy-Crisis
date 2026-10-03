// =============================================================================
// ENERGY CRISIS - ENGINE REGRESSION TESTS
// One group per engine fix (bug IDs from the project bug list in brackets).
// Headless: built from this file + Game/scr/*.cpp only, no SFML needed ("make test").
// Every check is reported; the program exits with 1 when any check failed.
// Set EC_SEED=<n> to replay a run with the same weather (the engine prints its seed).
// =============================================================================
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cmath>
#include <cstdlib>
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

constexpr float kFrame = 1.0f / 60.0f; // one frame at 60 FPS
constexpr float kShareEps = 1e-4f;     // float error of summed daily shifts

void beginGroup(const std::string& name) {
    g_groupFailures = 0;
    std::cout << "\n[" << name << "]\n";
}

void endGroup() {
    std::cout << (g_groupFailures == 0 ? "  -> PASS\n" : "  -> FAIL\n");
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void setResources(PlayerEconomy& p, int wood, int iron, int copper, int coal, int silicon, int silver) {
    p.wood = wood;
    p.iron = iron;
    p.copper = copper;
    p.coal = coal;
    p.silicon = silicon;
    p.silver = silver;
}

void giveResources(GameEngine& e, int player, int amount) {
    setResources(e.getPlayerEconomyMut(player), amount, amount, amount, amount, amount, amount);
}

int startPlotId(int player) { return (player == 1) ? 1 : 15; }

// Grid position of slot `sub` (0..8, row-major) inside a land plot (ids 1-12 = P1, 13-24 = P2)
sf::Vector2f slotOf(const GameEngine& e, int player, int plotId, int sub) {
    int idx = (plotId - 1) % 12;
    int col = (idx % 3) * 3 + sub % 3;
    int row = (idx / 3) * 3 + sub / 3;
    return e.getGridSlot(player, col, row);
}

void place(GameEngine& e, int player, BuildingType type, sf::Vector2f pos) {
    std::string msg;
    bool ok = e.placeBuilding(player, type, pos, msg);
    REQUIRE(ok, "placeBuilding P" << player << " type " << static_cast<int>(type) << " at (" << pos.x << "; "
                                  << pos.y << ") failed: " << msg);
}

void buyPlot(GameEngine& e, int player, int plotId) {
    auto& econ = e.getPlayerEconomyMut(player);
    int goldBefore = econ.gold;
    econ.gold += 100000;
    std::string msg;
    bool ok = e.buyLandPlot(player, plotId, msg);
    REQUIRE(ok, "buyLandPlot P" << player << " plot " << plotId << " failed: " << msg);
    econ.gold = goldBefore;
}

// Six wind turbines on the player's starting plot: at least 6 x 85 MW x 0.68 (weakest weather
// and hour) = 347 MW, far above the demand of days 3-5. The other player has nothing.
void buildWindFarm(GameEngine& e, int player) {
    giveResources(e, player, 1000);
    for (int i = 0; i < 6; ++i) {
        place(e, player, BuildingType::WIND_TURBINE, slotOf(e, player, startPlotId(player), i));
    }
}

void demolishAll(GameEngine& e, int player) {
    std::vector<sf::Vector2f> positions;
    for (const auto& b : e.getBuildings()) {
        if (b.playerOwner == player) positions.push_back(b.position);
    }
    for (const auto& pos : positions) {
        std::string msg;
        bool ok = e.removeBuilding(player, pos, msg);
        REQUIRE(ok, "removeBuilding P" << player << " failed: " << msg);
    }
}

// [b-economy] Share of a player who serves the whole city while the rival delivers nothing, after
// `days` judged days: every day moves the full VERDICT_MAX_SHIFT, gains above 70 % count half (BAL-02/04)
float dominantShareAfter(int days) {
    float share = 0.5f;
    for (int i = 0; i < days; ++i) share += Econ::applyLeaderDamping(share, Econ::VERDICT_MAX_SHIFT);
    return share;
}

// Number of judged days the dominant player needs to reach VICTORY_SHARE
int dominantDaysToWin() {
    int days = 0;
    while (dominantShareAfter(days) < Balance::VICTORY_SHARE - kShareEps) ++days;
    return days;
}

int expectedDemandForDay(int day) {
    if (day <= Balance::GRACE_PERIOD_DAYS) return 0;
    return Balance::STARTING_CITY_DEMAND_MW + (day - Balance::GRACE_PERIOD_DAYS - 1) * Balance::DAILY_DEMAND_INCREASE_MW;
}

// Plays frames of `dt` until the next day end has been settled (or the match is already over)
void runToNextDay(GameEngine& e, float dt) {
    const int day = e.getCurrentDay();
    const long maxFrames = static_cast<long>(2.0f * Balance::SECONDS_PER_DAY / (dt * e.getTimeScale())) + 10;
    long frames = 0;
    while (e.getCurrentDay() == day && e.getCityState().winner == 0) {
        e.update(dt);
        REQUIRE(++frames < maxFrames, "day " << day << " never ended");
    }
}

const PlacedBuilding* findBuilding(const GameEngine& e, int player, BuildingType type) {
    for (const auto& b : e.getBuildings()) {
        if (b.playerOwner == player && b.type == type) return &b;
    }
    return nullptr;
}

float rawGeneration(const GameEngine& e, int player) {
    float sum = 0.0f;
    for (const auto& b : e.getBuildings()) {
        if (b.playerOwner != player) continue;
        if (b.type == BuildingType::SOLAR_PANEL || b.type == BuildingType::WIND_TURBINE || b.type == BuildingType::HYDRO_PLANT) {
            sum += b.currentOutputMW;
        }
    }
    return sum;
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

// ---------------------------------------------------------------------------
// [B1 T1 B15 B18] Exactly one settlement per in-game day, at 06:00, with the right demand
// ---------------------------------------------------------------------------
void testOneSettlementPerDay(float dt, float timeScale, const std::string& label) {
    beginGroup("One settlement per day, " + label + " (B1, T1, B15, B18)");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    e.setTimeScale(timeScale);
    const float frameGame = dt * timeScale;
    const float day1Length = Balance::SECONDS_PER_DAY - Balance::gameSecondsAtHour(Balance::MATCH_START_HOUR);
    const float clockDrift = 0.5f; // the float game clock drifts by up to ~0.35 s per 90 s day at 60 FPS

    CHECK(e.getCurrentDay() == 1 && std::abs(e.getHour24() - Balance::MATCH_START_HOUR) < 1e-4f,
          "match must start on day 1 at 08:00, got day " << e.getCurrentDay() << " hour " << e.getHour24());

    int settlements = 0;
    int prevDay = e.getCurrentDay();
    float prevDaily = e.getCityState().dailySeconds;
    int prevDemand = e.getCityState().cityEnergyDemand;
    std::string prevMsg = e.getCityState().lastCutMessage;
    bool prevLight = e.isDaylight();
    double elapsed = 0.0;
    double lastToggle = -1.0;
    double shortestLightOrDark = 1e9;
    const long maxFrames = static_cast<long>((Balance::FINAL_DAY + 2) * Balance::SECONDS_PER_DAY / frameGame) + 10;
    long frames = 0;

    while (e.getCityState().winner == 0 && frames < maxFrames) {
        float hourBefore = e.getHour24();
        e.update(dt);
        ++frames;
        elapsed += frameGame;

        const auto& c = e.getCityState();
        const int day = e.getCurrentDay();
        const bool dayChanged = (day != prevDay);
        const bool countersReset = (c.dailySeconds < prevDaily);

        // A settlement (it resets the daily counters) happens exactly when the day number changes
        CHECK(countersReset == dayChanged, "day " << prevDay << " -> " << day << ", hour " << hourBefore << " -> "
                                                  << e.getHour24() << ", dailySeconds " << prevDaily << " -> " << c.dailySeconds);
        if (dayChanged) {
            ++settlements;
            CHECK(day == prevDay + 1, "one frame settled " << (day - prevDay) << " days");
            float expectedLength = (prevDay == 1) ? day1Length : Balance::SECONDS_PER_DAY;
            // prevDaily was read one frame before the day end
            CHECK(prevDaily <= expectedLength + clockDrift && prevDaily >= expectedLength - frameGame - clockDrift,
                  "day " << prevDay << " lasted about " << prevDaily << " s, expected " << expectedLength);
            float hoursPastRollover = e.getHour24() - Balance::CLOCK_HOUR_AT_ZERO;
            CHECK(hoursPastRollover >= -1e-3f && hoursPastRollover <= Balance::gameSecondsToHours(frameGame) + 1e-3f,
                  "day " << prevDay << " was settled at hour " << e.getHour24() << " instead of 06:00");
            CHECK(c.cityEnergyDemand == expectedDemandForDay(day),
                  "day " << day << " demand " << c.cityEnergyDemand << ", expected " << expectedDemandForDay(day));
            if (day > Balance::GRACE_PERIOD_DAYS + 1) {
                CHECK(c.cityEnergyDemand - prevDemand == Balance::DAILY_DEMAND_INCREASE_MW,
                      "demand rose by " << (c.cityEnergyDemand - prevDemand) << " MW on day " << day);
            }
            if (day <= Balance::FINAL_DAY) {
                CHECK(c.winner == 0, "the match ended early after day " << prevDay);
            }
        } else {
            CHECK(c.cityEnergyDemand == prevDemand, "demand changed without a settlement at hour " << e.getHour24());
            CHECK(c.lastCutMessage == prevMsg, "a day-end message appeared without a settlement at hour " << e.getHour24());
        }

        bool light = e.isDaylight();
        if (light != prevLight) {
            if (lastToggle >= 0.0) shortestLightOrDark = std::min(shortestLightOrDark, elapsed - lastToggle);
            lastToggle = elapsed;
            prevLight = light;
        }

        prevDay = day;
        prevDaily = c.dailySeconds;
        prevDemand = c.cityEnergyDemand;
        prevMsg = c.lastCutMessage;
    }

    CHECK(settlements == Balance::FINAL_DAY, "settled " << settlements << " days, expected " << Balance::FINAL_DAY);
    CHECK(e.getCurrentDay() == Balance::FINAL_DAY + 1, "match ended on day " << e.getCurrentDay());
    // Nobody built anything: 50/50 when the final day ends is a draw
    CHECK(e.getCityState().winner == 3, "winner " << e.getCityState().winner << ", expected 3 (draw)");
    CHECK(std::abs(e.getCityState().p1CityShare - 0.5f) < kShareEps, "share " << e.getCityState().p1CityShare);
    // [B18] Daylight never switches back for a moment at a season change (shortest real period: 8 h)
    const double fourHours = 4.0 * Balance::SECONDS_PER_DAY / 24.0;
    CHECK(shortestLightOrDark >= fourHours - frameGame,
          "shortest day or night period was " << shortestLightOrDark * 24.0 / Balance::SECONDS_PER_DAY << " game-hours");
    std::cout << "  settlements: " << settlements << ", frames: " << frames << ", result: draw\n";
    endGroup();
}

// ---------------------------------------------------------------------------
// [B1 T1] Huge frames still settle every crossed day once, with that day's own energy
// ---------------------------------------------------------------------------
void testHugeFrames() {
    beginGroup("Huge frames settle every crossed day (B1, T1)");
    {
        GameEngine e;
        e.init(1600.0f, 900.0f);
        buildWindFarm(e, 1);
        // [b-economy] grace, grace, then +12% a day (gains above 70% count half): 62, 72, 78, 84, 90%
        const int winDays = dominantDaysToWin();
        const int decidedDay = Balance::GRACE_PERIOD_DAYS + winDays; // the day whose end decides the match
        e.update((static_cast<float>(decidedDay) + 0.5f) * Balance::SECONDS_PER_DAY); // one huge frame
        CHECK(e.getCurrentDay() == decidedDay + 1, "day " << e.getCurrentDay() << ", expected " << (decidedDay + 1));
        CHECK(e.getCityState().winner == 1, "winner " << e.getCityState().winner);
        float expected = dominantShareAfter(winDays);
        CHECK(std::abs(e.getCityState().p1CityShare - expected) < kShareEps,
              "share " << e.getCityState().p1CityShare << ", expected " << expected);
        CHECK(e.getCityState().cityEnergyDemand == expectedDemandForDay(decidedDay + 1),
              "demand " << e.getCityState().cityEnergyDemand);
    }
    {
        GameEngine e;
        e.init(1600.0f, 900.0f);
        e.update(25.0f * Balance::SECONDS_PER_DAY); // more than a whole match in one frame
        CHECK(e.getCurrentDay() == Balance::FINAL_DAY + 1, "day " << e.getCurrentDay());
        CHECK(e.getCityState().winner == 3, "winner " << e.getCityState().winner);
    }
    endGroup();
}

// ---------------------------------------------------------------------------
// [B7 T15 B1] Daily shift <= MAX_DAILY_CITY_SHIFT, victory at VICTORY_SHARE, frozen afterwards
// ---------------------------------------------------------------------------
void testVictoryAtShare(int champion) {
    beginGroup("Victory at VICTORY_SHARE for P" + std::to_string(champion) + ", daily shift cap (B7, T15, B1)");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    buildWindFarm(e, champion);
    const float sign = (champion == 1) ? 1.0f : -1.0f;

    float prevShare = e.getCityState().p1CityShare;
    while (e.getCityState().winner == 0 && e.getCurrentDay() <= Balance::FINAL_DAY) {
        const int endedDay = e.getCurrentDay();
        runToNextDay(e, kFrame);
        const float share = e.getCityState().p1CityShare;
        const float won = (share - prevShare) * sign; // territory the champion won that day
        if (endedDay <= Balance::GRACE_PERIOD_DAYS) {
            CHECK(std::abs(won) < 1e-6f, "territory moved by " << won << " during grace day " << endedDay);
        } else {
            // [b-economy] full verdict every day (served 100% vs 0%), damped above 70%
            const float prevChampionShare = (champion == 1) ? prevShare : 1.0f - prevShare;
            const float expectedWon = Econ::applyLeaderDamping(prevChampionShare, Econ::VERDICT_MAX_SHIFT);
            CHECK(won > 0.0f && won <= Econ::VERDICT_MAX_SHIFT + kShareEps,
                  "day " << endedDay << " moved " << won << " (allowed 0 - " << Econ::VERDICT_MAX_SHIFT << ")");
            CHECK(std::abs(won - expectedWon) < kShareEps, "day " << endedDay << " moved " << won << ", expected " << expectedWon);
        }
        const float championShare = (champion == 1) ? share : 1.0f - share;
        if (championShare >= Balance::VICTORY_SHARE - kShareEps) {
            CHECK(e.getCityState().winner == champion, "share " << championShare << " but winner " << e.getCityState().winner);
        } else {
            CHECK(e.getCityState().winner == 0, "winner " << e.getCityState().winner << " at a share of only " << championShare);
        }
        prevShare = share;
    }

    const auto& city = e.getCityState();
    const float championShare = (champion == 1) ? city.p1CityShare : 1.0f - city.p1CityShare;
    CHECK(city.winner == champion, "winner " << city.winner);
    // [b-economy] 62, 72, 78, 84% are not enough; 90% after the fifth judged day (day 7) is
    const int decidedDay = Balance::GRACE_PERIOD_DAYS + dominantDaysToWin();
    CHECK(e.getCurrentDay() == decidedDay + 1, "won when day " << (e.getCurrentDay() - 1) << " ended, expected day " << decidedDay);
    CHECK(championShare >= Balance::VICTORY_SHARE, "winning share " << championShare);
    CHECK(std::abs(championShare - dominantShareAfter(dominantDaysToWin())) < kShareEps,
          "share was changed on victory: " << championShare);
    CHECK(std::abs(e.getPlayerEconomy(champion).cityInfluence - championShare) < 1e-6f, "influence out of sync");

    // The match is frozen after the result
    const float hour = e.getHour24();
    const int day = e.getCurrentDay();
    const int money = e.getPlayerEconomy(champion).money;
    e.update(10.0f);
    CHECK(e.getHour24() == hour && e.getCurrentDay() == day && e.getPlayerEconomy(champion).money == money,
          "the engine kept running after the match ended");
    endGroup();
}

// ---------------------------------------------------------------------------
// [B7 T15] When day FINAL_DAY ends, the larger share wins; an exact tie is a draw
// dayThreeWinner powers the city on day 3 only, dayFourWinner on day 4 only (0 = nobody)
// ---------------------------------------------------------------------------
void testFinalDay(int dayThreeWinner, int dayFourWinner, int expectedWinner, float expectedShare, const std::string& label) {
    beginGroup("Final day result: " + label + " (B7, T15)");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    if (dayThreeWinner != 0) buildWindFarm(e, dayThreeWinner);

    while (e.getCurrentDay() < Balance::FINAL_DAY) {
        runToNextDay(e, kFrame);
        CHECK(e.getCityState().winner == 0, "match ended early, after day " << (e.getCurrentDay() - 1));
        if (e.getCurrentDay() == 4) {
            // First frame of day 4 (06:00, daylight): day 3 is settled, swap who supplies the city
            if (dayThreeWinner != 0) demolishAll(e, dayThreeWinner);
            if (dayFourWinner != 0) buildWindFarm(e, dayFourWinner);
        } else if (e.getCurrentDay() == 5 && dayFourWinner != 0) {
            demolishAll(e, dayFourWinner);
        }
    }
    CHECK(e.getCurrentDay() == Balance::FINAL_DAY && e.getCityState().winner == 0, "no result before the final day");
    runToNextDay(e, kFrame);

    const auto& city = e.getCityState();
    CHECK(e.getCurrentDay() == Balance::FINAL_DAY + 1, "day " << e.getCurrentDay());
    CHECK(city.winner == expectedWinner, "winner " << city.winner << ", expected " << expectedWinner);
    // [b-economy] the frame in which a fleet is demolished still delivers a little: proportional verdicts
    // turn that into a shift of about 0.0001, so allow 0.002 here
    CHECK(std::abs(city.p1CityShare - expectedShare) < 0.002f, "share " << city.p1CityShare << ", expected " << expectedShare);
    if (expectedWinner == 3) {
        CHECK(std::abs(city.p1CityShare - 0.5f) < Balance::DRAW_SHARE_TOLERANCE, "a draw needs a 50/50 split");
    }
    std::cout << "  P1 share after day " << Balance::FINAL_DAY << ": " << city.p1CityShare << ", winner code " << city.winner << "\n";
    endGroup();
}

// ---------------------------------------------------------------------------
// [B2 T2] A solar-only player that meets the demand on AVERAGE gains territory,
// although its output at the 06:00 settlement is 0
// ---------------------------------------------------------------------------
void testSolarOnlyAverage() {
    beginGroup("Solar-only player is judged on the daily average (B2, T2)");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    giveResources(e, 1, 1000);
    buyPlot(e, 1, 2);
    buyPlot(e, 1, 3);
    // 27 panels: at least 27 x 2.07 MW = 56 MW on average even on a stormy spring day (day-3 demand: 30 MW)
    for (int plotId = 1; plotId <= 3; ++plotId) {
        for (int sub = 0; sub < 9; ++sub) place(e, 1, BuildingType::SOLAR_PANEL, slotOf(e, 1, plotId, sub));
    }
    runToNextDay(e, kFrame);
    runToNextDay(e, kFrame);
    REQUIRE(e.getCurrentDay() == 3, "day " << e.getCurrentDay());
    CHECK(std::abs(e.getCityState().p1CityShare - 0.5f) < 1e-6f, "territory moved during the grace period");

    const float shareBefore = e.getCityState().p1CityShare;
    float averageBeforeSettlement = 0.0f;
    while (e.getCurrentDay() == 3) {
        averageBeforeSettlement = e.getTodayAverageMW(1);
        e.update(kFrame);
    }
    const int outputAtSettlement = e.getPlayerEconomy(1).energyMW; // 06:00 (+ at most one frame): no sun
    const float gained = e.getCityState().p1CityShare - shareBefore;

    CHECK(averageBeforeSettlement >= Balance::STARTING_CITY_DEMAND_MW, "day-3 average " << averageBeforeSettlement << " MW");
    CHECK(outputAtSettlement < Balance::STARTING_CITY_DEMAND_MW, "output at 06:00 was " << outputAtSettlement << " MW");
    // [b-economy] BAL-02: solar serves the daytime quota only, so it gains part of the maximum shift
    CHECK(gained > 0.02f && gained <= Econ::VERDICT_MAX_SHIFT + kShareEps,
          "solar-only P1 gained " << gained);
    std::cout << "  day-3 average " << averageBeforeSettlement << " MW, output at 06:00 " << outputAtSettlement
              << " MW, territory gained " << gained << "\n";
    endGroup();
}

// ---------------------------------------------------------------------------
// [B3 B4 T4] Batteries discharge at night without lamps, charging is not delivered,
// and a battery gives back exactly what it stored
// ---------------------------------------------------------------------------
void testBatteryNightWithoutLamps() {
    beginGroup("Batteries serve the city at night without lamps (B3, B4, T4)");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    giveResources(e, 1, 1000);
    buyPlot(e, 1, 2);
    place(e, 1, BuildingType::BATTERY, slotOf(e, 1, 1, 0));
    for (int sub = 1; sub < 9; ++sub) place(e, 1, BuildingType::SOLAR_PANEL, slotOf(e, 1, 1, sub));
    for (int sub = 0; sub < 9; ++sub) place(e, 1, BuildingType::SOLAR_PANEL, slotOf(e, 1, 2, sub));
    REQUIRE(findBuilding(e, 1, BuildingType::LAMP) == nullptr, "test needs a player without lamps");

    runToNextDay(e, kFrame);
    runToNextDay(e, kFrame);
    REQUIRE(e.getCurrentDay() == 3, "day " << e.getCurrentDay());
    const int demand = e.getCityState().cityEnergyDemand;
    REQUIRE(demand == Balance::STARTING_CITY_DEMAND_MW, "demand " << demand);

    // (a) Day 3: energy that goes into the battery is not delivered to the city
    const float stepSeconds = 0.1f; // a single engine sub-step
    const float stepHours = Balance::gameSecondsToHours(stepSeconds);
    int chargingSteps = 0;
    while (e.getCurrentDay() == 3 && e.getHour24() < 19.25f) {
        const float storedBefore = findBuilding(e, 1, BuildingType::BATTERY)->energyStored;
        e.update(stepSeconds);
        const float charged = findBuilding(e, 1, BuildingType::BATTERY)->energyStored - storedBefore;
        if (charged > 1e-3f) {
            ++chargingSteps;
            const float chargePower = charged / stepHours;
            const float expectedDelivered = rawGeneration(e, 1) - chargePower;
            CHECK(std::abs(expectedDelivered - e.getPlayerEconomy(1).energyMW) <= 0.51f,
                  "generation " << rawGeneration(e, 1) << " MW, charging " << chargePower << " MW, delivered "
                                << e.getPlayerEconomy(1).energyMW << " MW");
            CHECK(chargePower <= Balance::BATTERY_MAX_POWER_MW + 0.01f, "charged at " << chargePower << " MW");
        }
    }
    CHECK(chargingSteps > 0, "the battery never charged on day 3");
    REQUIRE(e.getCurrentDay() == 3, "day 3 ended before nightfall");

    // (b) Night: no sun, no lamps -> the battery alone covers the city demand
    const PlacedBuilding* bat = findBuilding(e, 1, BuildingType::BATTERY);
    const float storedAtNight = bat->energyStored;
    CHECK(rawGeneration(e, 1) == 0.0f, "solar output at hour " << e.getHour24() << ": " << rawGeneration(e, 1));
    CHECK(storedAtNight > 1.0f, "battery holds only " << storedAtNight << " MWh at nightfall");
    CHECK(bat->currentOutputMW > 0.0f, "battery does not discharge at night");
    // [b-economy] BAL-03: the night load is the hourly quota, capped by the battery power
    const int expectedNight = static_cast<int>(std::lround(std::min(e.getPlayerLoadTargetMW(1), Balance::BATTERY_MAX_POWER_MW)));
    CHECK(std::abs(e.getPlayerEconomy(1).energyMW - expectedNight) <= 1,
          "night delivery " << e.getPlayerEconomy(1).energyMW << " MW, expected the quota " << expectedNight);

    // (c) It gives back what it stored (no free energy) and can run dry
    const float deliveredStart = e.getCityState().p1DailyDelivered;
    float lastStored = storedAtNight;
    while (e.getCurrentDay() == 3 && findBuilding(e, 1, BuildingType::BATTERY)->energyStored > 1e-3f) {
        e.update(kFrame);
        const float stored = findBuilding(e, 1, BuildingType::BATTERY)->energyStored;
        CHECK(stored <= lastStored + 1e-4f, "battery charged at night (hour " << e.getHour24() << ")");
        lastStored = stored;
    }
    REQUIRE(e.getCurrentDay() == 3, "battery still not empty at 06:00 (200 MWh / 30 MW = 6.7 h < 11 h night)");
    const float deliveredMWh = (e.getCityState().p1DailyDelivered - deliveredStart) * 24.0f / Balance::SECONDS_PER_DAY;
    CHECK(std::abs(deliveredMWh - storedAtNight) <= 0.01f * storedAtNight + 1.0f,
          "battery held " << storedAtNight << " MWh but delivered " << deliveredMWh << " MWh");
    e.update(kFrame);
    CHECK(e.getPlayerEconomy(1).energyMW == 0, "still delivering " << e.getPlayerEconomy(1).energyMW << " MW from an empty battery");
    std::cout << "  stored at nightfall " << storedAtNight << " MWh, delivered " << deliveredMWh << " MWh, run dry at hour "
              << e.getHour24() << "\n";
    endGroup();
}

// ---------------------------------------------------------------------------
// [B5 T3] No building without its specific resources (the legacy ore pool is no wildcard)
// ---------------------------------------------------------------------------
struct BuildCase {
    BuildingType type;
    int plotId; // hydro needs the river-bank plot 3
    int sub;
    const char* name;
};

const BuildCase kBuildCases[] = {
    { BuildingType::SOLAR_PANEL, 1, 0, "solar" },
    { BuildingType::WIND_TURBINE, 1, 1, "wind" },
    { BuildingType::BATTERY, 1, 2, "battery" },
    { BuildingType::LAMP, 1, 3, "lamp" },
    { BuildingType::HYDRO_PLANT, 3, 0, "hydro" },
};

void recipeOf(const BuildingCost& c, int out[6]) {
    out[0] = c.woodCost;
    out[1] = c.ironCost;
    out[2] = c.copperCost;
    out[3] = c.coalCost;
    out[4] = c.siliconCost;
    out[5] = c.silverCost;
}

void resourcesOf(const PlayerEconomy& p, int out[6]) {
    out[0] = p.wood;
    out[1] = p.iron;
    out[2] = p.copper;
    out[3] = p.coal;
    out[4] = p.silicon;
    out[5] = p.silver;
}

void testNoOreWildcard() {
    beginGroup("No building without the specific resources (B5, T3)");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    buyPlot(e, 1, 3);
    auto& p = e.getPlayerEconomyMut(1);

    std::string msg;
    e.mineResource(1, ResourceType::IRON, msg);
    CHECK(p.iron == Balance::IRON_BASE_YIELD && p.ore == 0, "mining iron gave iron " << p.iron << ", ore " << p.ore);

    // Plenty of wood, iron and legacy ore, but no copper or silicon: no solar panel
    setResources(p, 1000, 1000, 0, 0, 0, 0);
    p.ore = 100000;
    std::string reason;
    sf::Vector2f solarSlot = slotOf(e, 1, 1, 0);
    CHECK(!e.canPlaceBuilding(1, BuildingType::SOLAR_PANEL, solarSlot, reason), "ore replaced copper and silicon");
    CHECK(reason.find("НЕДОСТИГ") != std::string::npos, "unexpected reason: " << reason);
    CHECK(!e.placeBuilding(1, BuildingType::SOLAR_PANEL, solarSlot, msg), "solar placed without copper and silicon");
    CHECK(e.getBuildings().empty() && p.wood == 1000 && p.iron == 1000 && p.ore == 100000, "a failed placement changed resources");

    for (const BuildCase& bc : kBuildCases) {
        int need[6];
        recipeOf(e.getBuildingCost(bc.type), need);
        const sf::Vector2f pos = slotOf(e, 1, bc.plotId, bc.sub);
        for (int r = 0; r < 6; ++r) {
            if (need[r] == 0) continue;
            int have[6] = { need[0], need[1], need[2], need[3], need[4], need[5] };
            have[r] = need[r] - 1;
            setResources(p, have[0], have[1], have[2], have[3], have[4], have[5]);
            p.ore = 100000;
            CHECK(!e.canPlaceBuilding(1, bc.type, pos, reason), bc.name << " allowed with resource #" << r << " one short");
        }
        // The exact recipe is enough, and exactly the recipe is paid (nothing clamped, nothing free)
        setResources(p, need[0], need[1], need[2], need[3], need[4], need[5]);
        p.ore = 0;
        place(e, 1, bc.type, pos);
        int left[6];
        resourcesOf(p, left);
        CHECK(left[0] == 0 && left[1] == 0 && left[2] == 0 && left[3] == 0 && left[4] == 0 && left[5] == 0,
              bc.name << " did not cost exactly its recipe");
    }
    endGroup();
}

// ---------------------------------------------------------------------------
// [B6 B40] Demolition refunds a fraction of what was paid, never more; an empty slot never
// demolishes its neighbour
// ---------------------------------------------------------------------------
void testDemolitionRefund() {
    beginGroup("Demolition refund never exceeds what was paid (B6, B40)");
    for (const BuildCase& bc : kBuildCases) {
        GameEngine e;
        e.init(1600.0f, 900.0f);
        if (bc.plotId != 1) buyPlot(e, 1, bc.plotId);
        auto& p = e.getPlayerEconomyMut(1);
        giveResources(e, 1, 1000);
        p.ore = 0;
        p.gold = 0;
        p.money = 0;
        int before[6], afterPlace[6], afterRemove[6], need[6];
        recipeOf(e.getBuildingCost(bc.type), need);
        resourcesOf(p, before);

        const sf::Vector2f pos = slotOf(e, 1, bc.plotId, bc.sub);
        place(e, 1, bc.type, pos);
        resourcesOf(p, afterPlace);
        std::string msg;
        REQUIRE(e.removeBuilding(1, pos, msg), "could not demolish the " << bc.name << ": " << msg);
        resourcesOf(p, afterRemove);

        for (int r = 0; r < 6; ++r) {
            const int paid = before[r] - afterPlace[r];
            const int refund = afterRemove[r] - afterPlace[r];
            CHECK(paid == need[r], bc.name << " resource #" << r << ": paid " << paid << ", recipe " << need[r]);
            CHECK(refund >= 0 && refund <= paid, bc.name << " resource #" << r << ": refund " << refund << " > paid " << paid);
            CHECK(refund == static_cast<int>(paid * Balance::DEMOLISH_REFUND_FRACTION),
                  bc.name << " resource #" << r << ": refund " << refund << " for " << paid << " paid");
        }
        CHECK(p.ore == 0 && p.gold == 0 && p.money == 0, bc.name << ": demolition created ore, gold or money");
        CHECK(e.getBuildings().empty(), bc.name << " still standing");
    }

    GameEngine e;
    e.init(1600.0f, 900.0f);
    giveResources(e, 1, 1000);
    place(e, 1, BuildingType::SOLAR_PANEL, slotOf(e, 1, 1, 0));
    std::string reason, msg;
    const sf::Vector2f neighbour = slotOf(e, 1, 1, 1); // empty slot next to the panel
    CHECK(!e.canPlaceBuilding(1, BuildingType::DEMOLISH, neighbour, reason), "demolish preview accepts an empty slot");
    CHECK(!e.removeBuilding(1, neighbour, msg), "demolishing an empty slot removed the neighbour");
    CHECK(!e.removeBuilding(2, slotOf(e, 1, 1, 0), msg), "P2 demolished P1's building");
    CHECK(e.getBuildings().size() == 1, "buildings left: " << e.getBuildings().size());
    endGroup();
}

// ---------------------------------------------------------------------------
// [B29 B12 B15 T7 T13] Restart resets season, clock, timers, time scale, winner and the economy
// ---------------------------------------------------------------------------
void testRestartResets() {
    beginGroup("Restart resets season and timers (B29, B12, B15, T7, T13)");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    buildWindFarm(e, 1);
    // Frames of 0.25 s: P1 wins at exactly 630 game-seconds (622.5 s after the 08:00 start, end of day 7),
    // so half a second is left in the revenue timer at the end of the match
    long frames = 0;
    while (e.getCityState().winner == 0) {
        e.update(0.25f);
        REQUIRE(++frames < 100000, "no winner");
    }
    REQUIRE(e.getCityState().winner == 1 && e.getCurrentDay() == Balance::GRACE_PERIOD_DAYS + dominantDaysToWin() + 1, "winner " << e.getCityState().winner << " day " << e.getCurrentDay());
    CHECK(e.getSeason() == SeasonType::SUMMER, "day 6 should be summer, season " << static_cast<int>(e.getSeason()));
    e.setTimeScale(Balance::MINE_SPEEDUP_MULT);

    e.restartGame();
    const auto& city = e.getCityState();
    const auto& p1 = e.getPlayerEconomy(1);
    const auto& p2 = e.getPlayerEconomy(2);
    CHECK(e.getCurrentDay() == 1, "day " << e.getCurrentDay());
    CHECK(std::abs(e.getHour24() - Balance::MATCH_START_HOUR) < 1e-4f, "hour " << e.getHour24());
    CHECK(e.getSeason() == SeasonType::SPRING, "season " << static_cast<int>(e.getSeason()));
    CHECK(e.getTimeScale() == 1.0f, "time scale " << e.getTimeScale());
    CHECK(city.winner == 0 && city.cityEnergyDemand == 0 && city.p1CityShare == 0.5f, "city state not reset");
    CHECK(city.dailySeconds == 0.0f && city.p1DailyDelivered == 0.0f && city.p2DailyDelivered == 0.0f, "daily counters not reset");
    CHECK(e.getBuildings().empty(), "buildings: " << e.getBuildings().size());
    int purchased = 0;
    for (const auto& plot : e.getLandPlots()) purchased += plot.isPurchased ? 1 : 0;
    CHECK(static_cast<int>(e.getLandPlots().size()) == Balance::TOTAL_PLOTS && purchased == 2, "plots not reset, purchased " << purchased);
    CHECK(p1.money == 0 && p1.gold == 0 && p1.wood == 0 && p2.money == 0 && p2.gold == 0, "economy not reset");
    CHECK(p1.cityInfluence == 0.5f && p2.cityInfluence == 0.5f, "influence not reset");
    CHECK(e.getMineLevel(1, ResourceType::IRON) == 1, "mine levels not reset");

    // The revenue timer starts from zero again: the first payout comes after exactly 1 game-second
    giveResources(e, 1, 1000);
    place(e, 1, BuildingType::WIND_TURBINE, slotOf(e, 1, 1, 0));
    e.update(0.75f);
    CHECK(e.getPlayerEconomy(1).money == 0, "paid after 0.75 s: the old revenue timer leaked into the new match");
    e.update(0.25f);
    CHECK(e.getPlayerEconomy(1).money > 0, "no payout after 1 s");
    CHECK(e.getHour24() > Balance::MATCH_START_HOUR, "the clock does not run after a restart");
    endGroup();
}

// ---------------------------------------------------------------------------
// [B11 T9] EC_SEED makes two runs identical (weather and std::rand)
// ---------------------------------------------------------------------------
std::vector<long> recordMatch() {
    GameEngine e;
    e.init(1600.0f, 900.0f);
    std::vector<long> trace;
    for (int i = 0; i < 4; ++i) trace.push_back(std::rand()); // std::rand is seeded too (lightning, bot, particles)
    while (e.getCityState().winner == 0) {
        trace.push_back(static_cast<long>(e.getPlayerWeather(1)) * 10 + static_cast<long>(e.getPlayerWeather(2)));
        e.update(Balance::SECONDS_PER_DAY);
    }
    return trace;
}

void testSeedReproducible() {
    beginGroup("EC_SEED makes two runs identical (B11, T9)");
    SavedSeedEnv saved; // restored at the end of the group

    setSeedEnv("12345");
    const std::vector<long> a = recordMatch();
    const std::vector<long> b = recordMatch();
    CHECK(a.size() == 4u + Balance::FINAL_DAY, "trace length " << a.size());
    CHECK(a == b, "two runs with EC_SEED=12345 differ");

    setSeedEnv("67890");
    const std::vector<long> c = recordMatch();
    CHECK(a != c, "EC_SEED=12345 and EC_SEED=67890 gave the same match");

    setSeedEnv(nullptr);
    const std::vector<long> d = recordMatch();
    const std::vector<long> f = recordMatch();
    CHECK(!std::equal(d.begin(), d.begin() + 4, f.begin()), "two clock-seeded matches got the same seed");
    endGroup();
}

// ---------------------------------------------------------------------------
// [B12 T13] City income does not depend on the frame size
// ---------------------------------------------------------------------------
int incomeAfter(float dt, int frames) {
    GameEngine e;
    e.init(1600.0f, 900.0f);
    giveResources(e, 1, 1000);
    giveResources(e, 2, 1000);
    place(e, 1, BuildingType::WIND_TURBINE, slotOf(e, 1, 1, 0));
    place(e, 1, BuildingType::WIND_TURBINE, slotOf(e, 1, 1, 1));
    place(e, 2, BuildingType::SOLAR_PANEL, slotOf(e, 2, 15, 0));
    for (int i = 0; i < frames; ++i) e.update(dt);
    return e.getPlayerEconomy(1).money + e.getPlayerEconomy(2).money;
}

void testIncomeIndependentOfFrameSize() {
    beginGroup("City income does not depend on the frame size (B12, T13)");
    SavedSeedEnv saved;
    setSeedEnv("4242"); // identical weather in every run
    // 63.6 game-seconds each: 63 payouts
    const int smooth = incomeAfter(kFrame, 3816);
    const int medium = incomeAfter(0.3f, 212);
    const int coarse = incomeAfter(6.36f, 10);
    std::cout << "  income: dt=1/60 " << smooth << ", dt=0.3 " << medium << ", dt=6.36 " << coarse << "\n";
    CHECK(smooth > 0, "no income");
    CHECK(std::abs(medium - smooth) <= smooth / 50, "dt=0.3 income " << medium << " vs " << smooth);
    CHECK(std::abs(coarse - smooth) <= smooth / 50, "dt=6.36 income " << coarse << " vs " << smooth);
    endGroup();
}

// ---------------------------------------------------------------------------
// [B8 B14] Mirrored land prices; hydro only on the river bank
// ---------------------------------------------------------------------------
void testLandPricesAndRiverBank() {
    beginGroup("Mirrored land prices and river-bank hydro (B8, B14)");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    const auto& plots = e.getLandPlots();
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 3; ++c) {
            const LandPlot& west = plots[r * 3 + c];
            const LandPlot& east = plots[12 + r * 3 + (2 - c)];
            CHECK(west.playerOwner == 1 && east.playerOwner == 2, "unexpected plot order");
            CHECK(west.costGold == east.costGold, "plot " << west.id << " costs " << west.costGold << " G, mirrored plot "
                                                         << east.id << " costs " << east.costGold << " G");
        }
    }
    e.getPlayerEconomyMut(1).gold = 1000;
    e.getPlayerEconomyMut(2).gold = 1000;
    std::string msg;
    REQUIRE(e.buyNextLandTier(1, msg), msg);
    REQUIRE(e.buyNextLandTier(2, msg), msg);
    CHECK(e.getPlayerEconomy(1).gold == e.getPlayerEconomy(2).gold,
          "next plot: P1 paid " << (1000 - e.getPlayerEconomy(1).gold) << " G, P2 paid " << (1000 - e.getPlayerEconomy(2).gold) << " G");

    buyPlot(e, 1, 3);  // P1 river bank (column next to the city)
    buyPlot(e, 2, 13); // P2 river bank
    giveResources(e, 1, 1000);
    giveResources(e, 2, 1000);
    std::string reason;
    CHECK(!e.canPlaceBuilding(1, BuildingType::HYDRO_PLANT, slotOf(e, 1, 1, 4), reason), "P1 hydro allowed far from the river");
    CHECK(reason.find("ВЕЦ") != std::string::npos, "unexpected reason: " << reason);
    CHECK(e.canPlaceBuilding(1, BuildingType::HYDRO_PLANT, slotOf(e, 1, 3, 4), reason), "P1 hydro refused on the river bank: " << reason);
    CHECK(!e.canPlaceBuilding(2, BuildingType::HYDRO_PLANT, slotOf(e, 2, 15, 4), reason), "P2 hydro allowed far from the river");
    CHECK(e.canPlaceBuilding(2, BuildingType::HYDRO_PLANT, slotOf(e, 2, 13, 4), reason), "P2 hydro refused on the river bank: " << reason);
    CHECK(e.isRiverBankSlot(1, slotOf(e, 1, 3, 0)) && !e.isRiverBankSlot(1, slotOf(e, 1, 1, 0)), "P1 river bank");
    CHECK(e.isRiverBankSlot(2, slotOf(e, 2, 13, 0)) && !e.isRiverBankSlot(2, slotOf(e, 2, 15, 0)), "P2 river bank");
    endGroup();
}

// ---------------------------------------------------------------------------
// [B19 B20 B21] Weather mapping and wind curve
// ---------------------------------------------------------------------------
void testWeatherMapping() {
    beginGroup("Weather mapping and wind curve (B19, B20, B21)");
    typedef std::vector<std::string> Report;
    CHECK(WeatherSystem::reportToWeatherType(Report{ "cloudy", "snow", "none", "0" }) == WeatherType::SNOWY, "snow");
    CHECK(WeatherSystem::reportToWeatherType(Report{ "cloudy", "hail", "left", "30.4" }) == WeatherType::SNOWY, "hail");
    CHECK(WeatherSystem::reportToWeatherType(Report{ "cloudy", "rain", "none", "0" }) == WeatherType::RAINY, "rain");
    CHECK(WeatherSystem::reportToWeatherType(Report{ "cloudy", "thunder_storm", "none", "0" }) == WeatherType::STORMY, "storm");
    CHECK(WeatherSystem::reportToWeatherType(Report{ "cloudy", "clear", "left", "30.4" }) == WeatherType::WINDY, "wind");
    CHECK(WeatherSystem::reportToWeatherType(Report{ "cloudy", "clear", "none", "0" }) == WeatherType::CLOUDY, "overcast");
    CHECK(WeatherSystem::reportToWeatherType(Report{ "clear", "clear", "none", "0" }) == WeatherType::SUNNY, "clear sky");
    CHECK(WeatherSystem::getHydroMultiplier(WeatherType::SNOWY) == 1.0f, "snow must not boost hydro");
    CHECK(WeatherSystem::getSolarMultiplier(WeatherType::CLOUDY, 12.5f, SeasonType::SPRING) <
              WeatherSystem::getSolarMultiplier(WeatherType::SUNNY, 12.5f, SeasonType::SPRING),
          "overcast days get the sunny bonus");
    float beforeMidnight = WeatherSystem::getWindMultiplier(WeatherType::WINDY, 23.999f);
    float afterMidnight = WeatherSystem::getWindMultiplier(WeatherType::WINDY, 0.0f);
    CHECK(std::abs(beforeMidnight - afterMidnight) < 1e-3f, "wind jumps at midnight: " << beforeMidnight << " -> " << afterMidnight);
    endGroup();
}

} // namespace

int main() {
    std::cout << "========================================================\n";
    std::cout << " ENERGY CRISIS - ENGINE REGRESSION TESTS\n";
    const char* seed = std::getenv("EC_SEED");
    std::cout << " EC_SEED: " << (seed ? seed : "(not set, clock seeds)") << "\n";
    std::cout << "========================================================\n";

    testOneSettlementPerDay(kFrame, 1.0f, "dt = 1/60");
    testOneSettlementPerDay(kFrame, Balance::MINE_SPEEDUP_MULT, "dt = 1/60 at the mining speed-up");
    testOneSettlementPerDay(0.02f, 100.0f, "dt = 0.02 at 100x");
    testHugeFrames();
    testVictoryAtShare(1);
    testVictoryAtShare(2);
    testFinalDay(1, 0, 1, 0.5f + Econ::VERDICT_MAX_SHIFT, "P1 leads, no 85%");
    testFinalDay(2, 0, 2, 0.5f - Econ::VERDICT_MAX_SHIFT, "P2 leads, no 85%");
    // [b-economy] 50/50 after equal and opposite shifts: P2 served more MWh (bigger day-4 quota) and wins
    testFinalDay(1, 2, 2, 0.5f, "equal shares, tie-break by served energy");
    testSolarOnlyAverage();
    testBatteryNightWithoutLamps();
    testNoOreWildcard();
    testDemolitionRefund();
    testRestartResets();
    testSeedReproducible();
    testIncomeIndependentOfFrameSize();
    testLandPricesAndRiverBank();
    testWeatherMapping();

    std::cout << "\n========================================================\n";
    if (g_failures == 0) {
        std::cout << " ALL " << g_checks << " REGRESSION CHECKS PASSED\n";
    } else {
        std::cout << " " << g_failures << " OF " << g_checks << " REGRESSION CHECKS FAILED\n";
    }
    std::cout << "========================================================\n";
    return (g_failures == 0) ? 0 : 1;
}
