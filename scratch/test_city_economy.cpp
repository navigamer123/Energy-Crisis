// =============================================================================
// ENERGY CRISIS - CITY ECONOMY TESTS                           [team b-economy]
// BAL-02 proportional verdict, BAL-03 demand curve + peak pricing, BAL-04 anti-snowball,
// F-36 districts, F-37 grid frequency, F-11 energy / CO2 ledger, UX-01 forecast + day cut.
// Headless: built from this file + Game/scr/*.cpp only ("make test").
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

#define REQUIRE(cond, details)                                                                 \
    do {                                                                                       \
        ++g_checks;                                                                            \
        if (!(cond)) {                                                                         \
            std::cerr << "    FATAL (line " << __LINE__ << "): " << #cond << " | " << details << "\n"; \
            std::exit(1);                                                                      \
        }                                                                                      \
    } while (0)

constexpr float kFrame = 1.0f / 60.0f;
constexpr float kEps = 1e-4f;
const float kHourSeconds = Balance::SECONDS_PER_DAY / 24.0f;

void beginGroup(const std::string& name) {
    g_groupFailures = 0;
    std::cout << "\n[" << name << "]\n";
}

void endGroup() { std::cout << (g_groupFailures == 0 ? "  -> PASS\n" : "  -> FAIL\n"); }

void giveResources(GameEngine& e, int player, int amount) {
    PlayerEconomy& p = e.getPlayerEconomyMut(player);
    p.wood = p.iron = p.copper = p.coal = p.silicon = p.silver = amount;
}

int startPlotId(int player) { return (player == 1) ? 1 : 15; }
int riverPlotId(int player) { return (player == 1) ? 3 : 13; }

sf::Vector2f slotOf(const GameEngine& e, int player, int plotId, int sub) {
    int idx = (plotId - 1) % 12;
    int col = (idx % 3) * 3 + sub % 3;
    int row = (idx / 3) * 3 + sub / 3;
    return e.getGridSlot(player, col, row);
}

void place(GameEngine& e, int player, BuildingType type, sf::Vector2f pos) {
    std::string msg;
    bool ok = e.placeBuilding(player, type, pos, msg);
    REQUIRE(ok, "placeBuilding P" << player << " type " << static_cast<int>(type) << " failed: " << msg);
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

void demolishType(GameEngine& e, int player, BuildingType type, int keep = 0) {
    std::vector<sf::Vector2f> positions;
    for (const auto& b : e.getBuildings()) {
        if (b.playerOwner == player && b.type == type) positions.push_back(b.position);
    }
    for (size_t i = static_cast<size_t>(keep); i < positions.size(); ++i) {
        std::string msg;
        bool ok = e.removeBuilding(player, positions[i], msg);
        REQUIRE(ok, "removeBuilding failed: " << msg);
    }
}

void runToNextDay(GameEngine& e, float dt) {
    const int day = e.getCurrentDay();
    long frames = 0;
    while (e.getCurrentDay() == day && e.getCityState().winner == 0) {
        e.update(dt);
        REQUIRE(++frames < 2000000, "day " << day << " never ended");
    }
}

// Advances until the clock shows `hour` (same day if it is still ahead, else the next day)
void runToHour(GameEngine& e, float hour, float dt = 0.05f) {
    long frames = 0;
    if (e.getHour24() > hour) {
        while (e.getHour24() > hour) {
            e.update(dt);
            REQUIRE(++frames < 2000000, "never wrapped");
        }
    }
    while (e.getHour24() < hour) {
        e.update(dt);
        REQUIRE(++frames < 2000000, "never reached hour " << hour);
    }
}

// ---------------------------------------------------------------------------
// [BAL-03 F-36] Demand curve and district profiles
// ---------------------------------------------------------------------------
void testDemandProfiles() {
    beginGroup("Demand curve and district profiles (BAL-03, F-36)");
    float weightSum = 0.0f;
    for (int d = 0; d < Econ::DISTRICT_COUNT; ++d) {
        float mean = 0.0f;
        for (int h = 0; h < 24; ++h) mean += Econ::getDistrictProfile(d, h);
        mean /= 24.0f;
        CHECK(std::abs(mean - 1.0f) < 1e-4f, "district " << d << " mean " << mean);
        weightSum += Econ::getDistrictDef(d).weight;
    }
    CHECK(std::abs(weightSum - 1.0f) < 1e-5f, "weights sum to " << weightSum);

    float mean = 0.0f, night = 0.0f, evening = 0.0f, day = 0.0f;
    for (int h = 0; h < 24; ++h) mean += Econ::getCityDemandProfile(h);
    for (int h = 0; h < 6; ++h) night += Econ::getCityDemandProfile(h) / 6.0f;
    for (int h = 17; h < 22; ++h) evening += Econ::getCityDemandProfile(h) / 5.0f;
    for (int h = 9; h < 16; ++h) day += Econ::getCityDemandProfile(h) / 7.0f;
    mean /= 24.0f;
    std::cout << "  city profile: night x" << night << ", day x" << day << ", evening x" << evening << "\n";
    CHECK(std::abs(mean - 1.0f) < 1e-4f, "city mean " << mean);
    CHECK(night > 0.6f && night < 0.75f, "night factor " << night);
    CHECK(evening > 1.3f && evening < 1.5f, "evening factor " << evening);
    CHECK(day > 0.95f && day < 1.15f, "day factor " << day);
    CHECK(Econ::isPeakHour(19.5f) && !Econ::isPeakHour(3.0f) && !Econ::isPeakHour(12.0f), "peak hours");

    // Each district tells its own story
    CHECK(Econ::getDistrictProfile(Econ::DISTRICT_BUSINESS, 11) > 1.3f, "business is busy at 11:00");
    CHECK(Econ::getDistrictProfile(Econ::DISTRICT_BUSINESS, 2) < 0.6f, "business sleeps at 02:00");
    CHECK(Econ::getDistrictProfile(Econ::DISTRICT_HOMES, 19) > 2.0f, "homes peak at 19:00");
    CHECK(std::abs(Econ::getDistrictProfile(Econ::DISTRICT_HOSPITAL, 3) - 1.0f) < 1e-5f, "hospital is flat");
    CHECK(Econ::getCityDemandProfileAt(19.99f) == Econ::getCityDemandProfile(19), "profile is constant within an hour");
    CHECK(Econ::getCityDemandProfileAt(24.5f) == Econ::getCityDemandProfile(0), "hour wraps");
    endGroup();
}

// ---------------------------------------------------------------------------
// [BAL-02 BAL-04] Verdict formula and damping
// ---------------------------------------------------------------------------
void testVerdictMath() {
    beginGroup("Verdict formula and leader damping (BAL-02, BAL-04)");
    CHECK(std::abs(Econ::computeVerdictShift(1.0f, 1.0f, 0.5f)) < 1e-6f, "equal day moves nothing");
    CHECK(std::abs(Econ::computeVerdictShift(1.0f, 0.5f, 0.5f) - 0.04f) < 1e-6f, "served term");
    CHECK(std::abs(Econ::computeVerdictShift(1.0f, 1.0f, 0.75f) - 0.025f) < 1e-6f, "supply term");
    CHECK(std::abs(Econ::computeVerdictShift(1.0f, 0.0f, 1.0f) - Econ::VERDICT_MAX_SHIFT) < 1e-6f, "clamped at +12%");
    CHECK(std::abs(Econ::computeVerdictShift(0.0f, 1.0f, 0.0f) + Econ::VERDICT_MAX_SHIFT) < 1e-6f, "clamped at -12%");
    CHECK(std::abs(Econ::computeVerdictShift(0.6f, 0.9f, 0.3f) + Econ::computeVerdictShift(0.9f, 0.6f, 0.7f)) < 1e-6f,
          "symmetric for both players");

    CHECK(std::abs(Econ::applyLeaderDamping(0.5f, 0.12f) - 0.12f) < 1e-6f, "no damping below 70%");
    CHECK(std::abs(Econ::applyLeaderDamping(0.62f, 0.12f) - 0.10f) < 1e-6f, "half of the gain above 70%");
    CHECK(std::abs(Econ::applyLeaderDamping(0.8f, 0.12f) - 0.06f) < 1e-6f, "leader above 70% gains half");
    CHECK(std::abs(Econ::applyLeaderDamping(0.8f, -0.12f) + 0.12f) < 1e-6f, "the trailing player is never damped");
    CHECK(std::abs(Econ::applyLeaderDamping(0.2f, -0.12f) + 0.06f) < 1e-6f, "P2 leader above 70% gains half");
    CHECK(std::abs(Econ::quotaFactor(0.5f) - 1.0f) < 1e-6f && std::abs(Econ::quotaFactor(0.8f) - 1.3f) < 1e-6f,
          "quota factor = 0.5 + share");
    endGroup();
}

// ---------------------------------------------------------------------------
// [BAL-04 F-36] Quotas follow territory and the hourly profile
// ---------------------------------------------------------------------------
void testQuotas() {
    beginGroup("Quota = demand x profile x (0.5 + share) (BAL-03, BAL-04)");
    Econ::CityEconomy ce;
    const int demand = 100;
    for (int h = 0; h < 24; ++h) {
        const float hour = h + 0.5f;
        const float expected = demand * Econ::getCityDemandProfile(h);
        CHECK(std::abs(Econ::playerQuotaMW(ce, 1, demand, hour) - expected) < 1e-3f, "hour " << h);
        CHECK(std::abs(Econ::playerQuotaMW(ce, 2, demand, hour) - expected) < 1e-3f, "hour " << h);
    }
    Econ::syncDistrictsToCityShare(ce, 0.8f);
    CHECK(std::abs(Econ::cityShareFromDistricts(ce) - 0.8f) < 1e-4f, "sync to 80%");
    const float q1 = Econ::playerQuotaMW(ce, 1, demand, 12.0f);
    const float q2 = Econ::playerQuotaMW(ce, 2, demand, 12.0f);
    const float base = demand * Econ::getCityDemandProfile(12);
    CHECK(std::abs(q1 - 1.3f * base) < 0.01f && std::abs(q2 - 0.7f * base) < 0.01f,
          "80/20 split: quotas " << q1 << " / " << q2 << " of " << base);
    CHECK(Econ::playerQuotaMW(ce, 1, 0, 12.0f) == 0.0f, "no quota in the grace period");
    endGroup();
}

// ---------------------------------------------------------------------------
// [BAL-02] Out-building the rival pays every day, proportionally
// ---------------------------------------------------------------------------
float dayThreeShift(int p1Turbines, int p2Turbines) {
    GameEngine e;
    e.init(1600.0f, 900.0f);
    giveResources(e, 1, 5000);
    giveResources(e, 2, 5000);
    for (int i = 0; i < p1Turbines; ++i) place(e, 1, BuildingType::WIND_TURBINE, slotOf(e, 1, startPlotId(1), i));
    for (int i = 0; i < p2Turbines; ++i) place(e, 2, BuildingType::WIND_TURBINE, slotOf(e, 2, startPlotId(2), i));
    runToNextDay(e, 0.25f);
    runToNextDay(e, 0.25f);
    const float before = e.getCityState().p1CityShare;
    runToNextDay(e, 0.25f);
    const auto& r = e.getCityEconomy().lastReport;
    std::cout << "  " << p1Turbines << " vs " << p2Turbines << " turbines: served " << r.served[0] << " / " << r.served[1]
              << ", supply " << r.supplyShare1 << ", shift " << (e.getCityState().p1CityShare - before) << "\n";
    return e.getCityState().p1CityShare - before;
}

void testProportionalVerdict() {
    beginGroup("Proportional verdict: both meet demand, the bigger supplier still gains (BAL-02)");
    const float even = dayThreeShift(4, 4);
    const float ahead = dayThreeShift(6, 2);
    const float alone = dayThreeShift(6, 0);
    // Weather differs per sector, so equal fleets are only roughly even
    CHECK(std::abs(even) < 0.05f, "equal fleets moved " << even);
    CHECK(ahead > 0.0f && ahead < Econ::VERDICT_MAX_SHIFT - kEps, "3x the fleet (both serve the city) moved " << ahead);
    CHECK(std::abs(alone - Econ::VERDICT_MAX_SHIFT) < kEps, "rival without power: full shift, got " << alone);
    endGroup();
}

// ---------------------------------------------------------------------------
// [F-36] Allocation order: the hospital is served first, homes last
// ---------------------------------------------------------------------------
void testDistrictAllocation() {
    beginGroup("District allocation order and per-district shifts (F-36)");
    Econ::CityEconomy ce;
    Econ::GridSample s[2];
    s[0].deliveredMW = 50.0f;  // P1: half of a 100 MW city
    s[0].loadTargetMW = 100.0f;
    s[1].deliveredMW = 200.0f; // P2: everything
    s[1].loadTargetMW = 100.0f;
    const float hour = 19.5f;  // evening peak: homes are the biggest district
    Econ::onSimStep(ce, s, 100, hour, 1000.0f, 1.0f);
    const auto& hosp = ce.districts[Econ::DISTRICT_HOSPITAL];
    const auto& homes = ce.districts[Econ::DISTRICT_HOMES];
    CHECK(std::abs(hosp.allocNowMW[0] - hosp.quotaNowMW[0]) < 1e-3f, "hospital fully served by the weak player");
    CHECK(homes.allocNowMW[0] < homes.quotaNowMW[0] * 0.5f, "homes are served last: " << homes.allocNowMW[0] << " of "
                                                                                      << homes.quotaNowMW[0]);
    float allocSum = 0.0f;
    for (int d = 0; d < Econ::DISTRICT_COUNT; ++d) allocSum += ce.districts[d].allocNowMW[0];
    CHECK(std::abs(allocSum - 50.0f) < 1e-3f, "all delivered power is allocated: " << allocSum);
    for (int d = 0; d < Econ::DISTRICT_COUNT; ++d) {
        CHECK(std::abs(ce.districts[d].allocNowMW[1] - ce.districts[d].quotaNowMW[1]) < 1e-3f, "P2 serves district " << d);
    }

    // A whole day like that: P2 wins the homes much more than the hospital
    for (int i = 1; i < 90; ++i) Econ::onSimStep(ce, s, 100, std::fmod(i * 24.0f / 90.0f, 24.0f), 1000.0f + i, 1.0f);
    float share = 0.5f;
    std::string msg;
    Econ::settleDay(ce, 3, 100, share, msg);
    const auto& r = ce.lastReport;
    CHECK(r.districtShift[Econ::DISTRICT_HOMES] < r.districtShift[Econ::DISTRICT_HOSPITAL] - 0.03f,
          "homes " << r.districtShift[Econ::DISTRICT_HOMES] << " vs hospital " << r.districtShift[Econ::DISTRICT_HOSPITAL]);
    CHECK(share < 0.5f && std::abs(share - Econ::cityShareFromDistricts(ce)) < 1e-5f, "city share " << share);
    CHECK(std::abs(r.appliedShift - (share - 0.5f)) < 1e-5f, "report shift " << r.appliedShift);
    CHECK(msg.find("ИГРАЧ 2") != std::string::npos, "message: " << msg);
    // Daily counters are reset after the settlement
    CHECK(ce.districts[0].quotaEnergy[0] == 0.0f && ce.deliveredEnergyToday[1] == 0.0f, "daily counters reset");
    endGroup();
}

// ---------------------------------------------------------------------------
// [BAL-04] A trailing player is not out in 2-3 days
// ---------------------------------------------------------------------------
void testAntiSnowball() {
    beginGroup("Anti-snowball: leader damping and territory-as-load (BAL-04)");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    giveResources(e, 1, 5000);
    for (int i = 0; i < 6; ++i) place(e, 1, BuildingType::WIND_TURBINE, slotOf(e, 1, startPlotId(1), i));
    std::vector<float> shares;
    while (e.getCityState().winner == 0 && e.getCurrentDay() <= 12) {
        runToNextDay(e, 0.25f);
        shares.push_back(e.getCityState().p1CityShare);
    }
    std::cout << "  P1 share after each day:";
    for (float s : shares) std::cout << " " << static_cast<int>(std::lround(s * 100.0f)) << "%";
    std::cout << "\n";
    REQUIRE(shares.size() >= 5, "only " << shares.size() << " days");
    CHECK(std::abs(shares[2] - 0.62f) < kEps && std::abs(shares[3] - 0.72f) < kEps && std::abs(shares[4] - 0.78f) < kEps,
          "62/72/78%: " << shares[2] << " " << shares[3] << " " << shares[4]);
    CHECK(e.getCityState().winner == 1, "the dominant player still wins, winner " << e.getCityState().winner);
    CHECK(e.getCurrentDay() >= 8, "no victory in 2-3 days: decided on day " << (e.getCurrentDay() - 1));

    // Territory is load: the leader's quota is bigger than the trailing player's
    GameEngine f;
    f.init(1600.0f, 900.0f);
    giveResources(f, 1, 5000);
    for (int i = 0; i < 6; ++i) place(f, 1, BuildingType::WIND_TURBINE, slotOf(f, 1, startPlotId(1), i));
    for (int d = 0; d < 4; ++d) runToNextDay(f, 0.25f); // 62%, 72%
    const float q1 = f.getPlayerLoadTargetMW(1);
    const float q2 = f.getPlayerLoadTargetMW(2);
    const float base = f.getCurrentDemandMW();
    CHECK(q1 > base * 1.15f && q2 < base * 0.85f, "quotas " << q1 << " / " << q2 << " of " << base);
    endGroup();
}

// ---------------------------------------------------------------------------
// [BAL-04] Optional dominance levy and underdog subsidy
// ---------------------------------------------------------------------------
void testLevyAndSubsidy() {
    beginGroup("Optional dominance levy and underdog subsidy (BAL-04)");
    Econ::CityEconomy ce;
    CHECK(!Econ::hasDominanceLevy(ce, 1, 0.9f) && Econ::payoutFactor(ce, 1, 0.9f) == 1.0f, "off by default");
    ce.rules.levyAndSubsidy = true;
    CHECK(Econ::hasDominanceLevy(ce, 1, 0.66f) && !Econ::hasDominanceLevy(ce, 2, 0.66f), "levy above 65%");
    CHECK(std::abs(Econ::payoutFactor(ce, 2, 0.30f) - Econ::LEVY_PAYOUT_FACTOR) < 1e-6f, "P2 at 70% pays the levy");
    CHECK(Econ::hasUnderdogSubsidy(ce, 2, 0.62f) && !Econ::hasUnderdogSubsidy(ce, 1, 0.62f), "subsidy below 40%");

    GameEngine e;
    e.init(1600.0f, 900.0f);
    Econ::Rules rules;
    rules.levyAndSubsidy = true;
    e.setEconomyRules(rules);
    e.restartGame();
    CHECK(e.getCityEconomy().rules.levyAndSubsidy, "rules survive a restart");
    giveResources(e, 1, 5000);
    for (int i = 0; i < 6; ++i) place(e, 1, BuildingType::WIND_TURBINE, slotOf(e, 1, startPlotId(1), i));
    for (int d = 0; d < 4; ++d) runToNextDay(e, 0.25f); // P2 at 28%
    REQUIRE(1.0f - e.getCityState().p1CityShare < Econ::SUBSIDY_BELOW_SHARE, "P2 share " << 1.0f - e.getCityState().p1CityShare);
    runToHour(e, 10.0f);
    giveResources(e, 2, 100);
    place(e, 2, BuildingType::WIND_TURBINE, slotOf(e, 2, startPlotId(2), 0));
    const auto& p2 = e.getPlayerEconomy(2);
    const auto& w = Balance::WIND_TURBINE;
    CHECK(p2.iron == 100 - w.ironCost + static_cast<int>(std::lround(w.ironCost * 0.15f)),
          "iron after the subsidised turbine: " << p2.iron);
    endGroup();
}

// ---------------------------------------------------------------------------
// [BAL-03] Peak pricing: the same MW earn more in the evening than at night
// ---------------------------------------------------------------------------
void testPeakPricing() {
    beginGroup("Peak pricing follows the demand curve (BAL-03)");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    giveResources(e, 1, 5000);
    buyPlot(e, 1, riverPlotId(1));
    for (int i = 0; i < 3; ++i) place(e, 1, BuildingType::HYDRO_PLANT, slotOf(e, 1, riverPlotId(1), i)); // weather-flat output
    runToHour(e, 19.2f);
    int before = e.getPlayerEconomy(1).money;
    e.update(2.0f);
    const int evening = e.getPlayerEconomy(1).money - before;
    runToHour(e, 2.2f);
    before = e.getPlayerEconomy(1).money;
    e.update(2.0f);
    const int night = e.getPlayerEconomy(1).money - before;
    std::cout << "  income in 2 s: evening " << evening << ", night " << night << "\n";
    CHECK(night > 0 && evening > night * 1.8f, "evening " << evening << " vs night " << night);
    endGroup();
}

// ---------------------------------------------------------------------------
// [F-37] Grid frequency: blackouts for fragile fleets, batteries and hydro protect
// ---------------------------------------------------------------------------
struct FleetResult {
    int state;
    float hz;
    int delivered;
    bool lampLit;
};

// P1 runs `turbines` wind turbines (+ optional battery / hydro / lamp) into day 3, then loses
// all wind just before an hourly check
FleetResult loseWindAtNoon(int turbines, bool battery, int hydros, bool lamp) {
    GameEngine e;
    e.init(1600.0f, 900.0f);
    giveResources(e, 1, 9000);
    buyPlot(e, 1, 2);
    buyPlot(e, 1, riverPlotId(1));
    for (int i = 0; i < turbines; ++i) place(e, 1, BuildingType::WIND_TURBINE, slotOf(e, 1, startPlotId(1), i));
    if (battery) {
        for (int i = 0; i < 3; ++i) place(e, 1, BuildingType::BATTERY, slotOf(e, 1, 2, i));
    }
    for (int i = 0; i < hydros; ++i) place(e, 1, BuildingType::HYDRO_PLANT, slotOf(e, 1, riverPlotId(1), i));
    if (lamp) place(e, 1, BuildingType::LAMP, slotOf(e, 1, 2, 8));
    runToNextDay(e, 0.25f);
    runToNextDay(e, 0.25f);
    runToHour(e, 11.9f);
    demolishType(e, 1, BuildingType::WIND_TURBINE);
    runToHour(e, 12.05f); // the 12:00 check sees the lost generation
    if (lamp) {
        // Rebuild generation at once: during a blackout it does not help, the lamp stays dark
        giveResources(e, 1, 9000);
        for (int i = 0; i < 4; ++i) place(e, 1, BuildingType::WIND_TURBINE, slotOf(e, 1, startPlotId(1), i));
    }
    runToHour(e, 12.2f);
    FleetResult r;
    r.state = e.getGridState(1);
    r.hz = e.getGridFrequencyHz(1);
    r.delivered = e.getPlayerEconomy(1).energyMW;
    r.lampLit = false;
    for (const auto& b : e.getBuildings()) {
        if (b.playerOwner == 1 && b.type == BuildingType::LAMP) r.lampLit = b.lightRadius > 0.0f;
    }
    return r;
}

void testGridFrequency() {
    beginGroup("Grid frequency: brownouts and blackouts (F-37)");
    const FleetResult fragile = loseWindAtNoon(4, false, 0, true);
    std::cout << "  wind-only fleet loses its wind: state " << fragile.state << ", " << fragile.hz << " Hz, delivered "
              << fragile.delivered << " MW, lamp lit " << fragile.lampLit << "\n";
    CHECK(fragile.state == Econ::GRID_BLACKOUT, "state " << fragile.state);
    CHECK(fragile.hz < Econ::BLACKOUT_HZ + 0.6f, "frequency " << fragile.hz);
    CHECK(fragile.delivered == 0, "blackout delivers " << fragile.delivered << " MW");
    CHECK(!fragile.lampLit, "lamps stay dark during a blackout");

    const FleetResult stored = loseWindAtNoon(4, true, 0, false);
    std::cout << "  same with 3 batteries: state " << stored.state << ", " << stored.hz << " Hz\n";
    CHECK(stored.state == Econ::GRID_NORMAL, "batteries are a fast reserve, state " << stored.state);

    // Pure model: hydro inertia. Losing 50 MW of wind out of a 100 MW load is a brownout for a
    // wind-only grid, but a grid whose remaining 50 MW are hydro rides through it.
    {
        Econ::CityEconomy windOnly, mixed;
        Econ::GridSample a[2], b[2];
        a[0].windMW = 100.0f; a[0].deliveredMW = 100.0f; a[0].loadTargetMW = 100.0f; a[1] = a[0];
        b[0].windMW = 50.0f; b[0].hydroMW = 50.0f; b[0].deliveredMW = 100.0f; b[0].loadTargetMW = 100.0f; b[1] = b[0];
        Econ::onSimStep(windOnly, a, 100, 12.0f, 10.0f * kHourSeconds + 0.1f, 0.1f);
        Econ::onSimStep(mixed, b, 100, 12.0f, 10.0f * kHourSeconds + 0.1f, 0.1f);
        a[0].windMW = 50.0f; a[0].deliveredMW = 50.0f;
        b[0].windMW = 0.0f; b[0].deliveredMW = 50.0f;
        Econ::onSimStep(windOnly, a, 100, 13.0f, 11.0f * kHourSeconds + 0.1f, 0.1f);
        Econ::onSimStep(mixed, b, 100, 13.0f, 11.0f * kHourSeconds + 0.1f, 0.1f);
        std::cout << "  lose 50 MW: wind-only " << windOnly.grid[0].frequencyHz << " Hz (state " << windOnly.grid[0].state
                  << "), with hydro " << mixed.grid[0].frequencyHz << " Hz (state " << mixed.grid[0].state << ")\n";
        CHECK(windOnly.grid[0].state == Econ::GRID_BROWNOUT, "wind-only state " << windOnly.grid[0].state);
        CHECK(mixed.grid[0].state == Econ::GRID_NORMAL, "hydro inertia, state " << mixed.grid[0].state);
        CHECK(mixed.grid[0].frequencyHz > windOnly.grid[0].frequencyHz + 0.5f, "hydro keeps the frequency up");
    }

    // Pure model: a 40% uncovered drop without inertia is a brownout, the event lasts one hour
    Econ::CityEconomy ce;
    Econ::GridSample s[2];
    s[0].windMW = 100.0f;
    s[0].deliveredMW = 100.0f;
    s[0].loadTargetMW = 100.0f;
    s[1] = s[0];
    Econ::onSimStep(ce, s, 100, 12.0f, 10.0f * kHourSeconds + 0.1f, 0.1f);
    s[0].windMW = 60.0f;
    s[0].deliveredMW = 60.0f;
    Econ::onSimStep(ce, s, 100, 13.0f, 11.0f * kHourSeconds + 0.1f, 0.1f);
    CHECK(ce.grid[0].state == Econ::GRID_BROWNOUT, "40% loss: state " << ce.grid[0].state << ", " << ce.grid[0].frequencyHz << " Hz");
    CHECK(std::abs(Econ::gridOutputFactor(ce, 1) - Econ::BROWNOUT_FACTOR) < 1e-6f, "brownout factor");
    CHECK(ce.grid[1].state == Econ::GRID_NORMAL, "P2 unaffected");
    for (int i = 0; i < 13; ++i) Econ::onSimStep(ce, s, 100, 13.0f, 11.0f * kHourSeconds + 0.2f + i * 0.3f, 0.3f);
    CHECK(ce.grid[0].state == Econ::GRID_NORMAL, "brownout ends after one hour, state " << ce.grid[0].state);
    CHECK(ce.grid[0].brownoutsToday == 1 && ce.grid[0].brownoutsTotal == 1, "brownout counted");
    // No events in the grace period
    Econ::CityEconomy grace;
    s[0].windMW = 100.0f;
    Econ::onSimStep(grace, s, 0, 12.0f, 10.0f * kHourSeconds + 0.1f, 0.1f);
    s[0].windMW = 0.0f;
    s[0].deliveredMW = 0.0f;
    Econ::onSimStep(grace, s, 0, 13.0f, 11.0f * kHourSeconds + 0.1f, 0.1f);
    CHECK(grace.grid[0].state == Econ::GRID_NORMAL, "grace period event");
    endGroup();
}

// ---------------------------------------------------------------------------
// [F-11] Energy mix and CO2 ledger
// ---------------------------------------------------------------------------
void testLedger() {
    beginGroup("Energy-mix and CO2 ledger (F-11)");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    giveResources(e, 1, 5000);
    buyPlot(e, 1, riverPlotId(1));
    place(e, 1, BuildingType::SOLAR_PANEL, slotOf(e, 1, startPlotId(1), 0));
    place(e, 1, BuildingType::WIND_TURBINE, slotOf(e, 1, startPlotId(1), 1));
    place(e, 1, BuildingType::HYDRO_PLANT, slotOf(e, 1, riverPlotId(1), 0));
    place(e, 1, BuildingType::BATTERY, slotOf(e, 1, startPlotId(1), 2));
    runToNextDay(e, 0.25f);
    runToNextDay(e, 0.25f);
    runToNextDay(e, 0.25f);
    const auto& led = e.getCityEconomy().ledger[0];
    for (int s = 0; s < Econ::SRC_COUNT; ++s) {
        std::cout << "  " << Econ::getSourceNameBg(s) << ": " << led.generatedMWh[s] << " MWh\n";
    }
    CHECK(led.generatedMWh[Econ::SRC_SOLAR] > 0.0 && led.generatedMWh[Econ::SRC_WIND] > 0.0 &&
              led.generatedMWh[Econ::SRC_HYDRO] > 0.0, "every source produced");
    const double generated = led.generatedMWh[0] + led.generatedMWh[1] + led.generatedMWh[2];
    CHECK(led.deliveredMWh > 0.0 && led.deliveredMWh <= generated + 1.0, "delivered " << led.deliveredMWh << " of " << generated);
    CHECK(std::abs(Econ::co2AvoidedT(led) - led.deliveredMWh * Econ::GRID_CO2_T_PER_MWH) < 1e-6, "CO2 = MWh x 0.40");
    CHECK(led.servedMWh > 0.0 && led.servedMWh <= led.deliveredMWh + 1e-6, "served " << led.servedMWh);
    // Delivered MWh agrees with the engine's own daily average bookkeeping (MW x game-seconds)
    const auto& rep = e.getCityEconomy().lastReport;
    CHECK(rep.valid && rep.day == 3, "report day " << rep.day);
    CHECK(rep.deliveredMWh[0] > 0.0f && std::abs(rep.co2AvoidedT[0] - rep.deliveredMWh[0] * Econ::GRID_CO2_T_PER_MWH) < 0.01f,
          "report CO2");
    // A battery returns energy, it never creates it: discharge <= what the generators produced
    CHECK(led.generatedMWh[Econ::SRC_BATTERY] <= generated, "battery discharge");
    endGroup();
}

// ---------------------------------------------------------------------------
// [UX-01] Forecast, day cut and Day Report
// ---------------------------------------------------------------------------
void testForecastAndDayCut() {
    beginGroup("Forecast chip data, consumeDayCut and Day Report (UX-01)");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    CHECK(!e.projectPlayerOutput().active, "no forecast in the grace period");
    CHECK(!e.consumeDayCut(), "no day cut at the start");
    giveResources(e, 1, 5000);
    for (int i = 0; i < 3; ++i) place(e, 1, BuildingType::WIND_TURBINE, slotOf(e, 1, startPlotId(1), i));
    runToNextDay(e, 0.25f);
    CHECK(e.consumeDayCut(), "day cut after day 1");
    CHECK(!e.consumeDayCut(), "consumed only once");
    runToNextDay(e, 0.25f);
    e.consumeDayCut();
    runToHour(e, 15.0f);
    const Econ::Forecast f = e.projectPlayerOutput();
    std::cout << "  15:00 day 3: served " << f.served[0] << " / " << f.served[1] << ", now " << f.nowMW[0] << "/" << f.quotaNowMW[0]
              << " MW, projected shift " << f.projectedShift << ", settles in " << f.secondsToSettlement << " s\n";
    CHECK(f.active, "forecast active on day 3");
    CHECK(f.served[0] > 0.99f && f.served[1] == 0.0f, "served so far " << f.served[0] << " / " << f.served[1]);
    CHECK(std::abs(f.projectedShift - Econ::VERDICT_MAX_SHIFT) < kEps, "projected " << f.projectedShift);
    CHECK(std::abs(f.secondsToSettlement - 15.0f * kHourSeconds) < 0.2f, "seconds to 06:00: " << f.secondsToSettlement);
    CHECK(std::abs(f.quotaNowMW[0] - e.getCurrentDemandMW()) < 0.01f, "quota at 50% = current demand");
    const float before = e.getCityState().p1CityShare;
    runToNextDay(e, 0.25f);
    const float after = e.getCityState().p1CityShare;
    CHECK(std::abs((after - before) - f.projectedShift) < 0.005f, "the forecast came true: " << (after - before));
    CHECK(e.consumeDayCut(), "day cut after day 3");
    const auto& r = e.getCityEconomy().lastReport;
    CHECK(r.valid && r.day == 3 && std::abs(r.shareBefore - before) < 1e-6f && std::abs(r.shareAfter - after) < 1e-6f,
          "report " << r.day << " " << r.shareBefore << " -> " << r.shareAfter);
    CHECK(e.getCityState().lastCutMessage.find("ОБСЛУЖЕНИ P1 100%") != std::string::npos, e.getCityState().lastCutMessage);
    endGroup();
}

// ---------------------------------------------------------------------------
// [BAL-02] Day-20 tie-break and district sync
// ---------------------------------------------------------------------------
void testTieBreakAndSync() {
    beginGroup("Tie-break by served energy, district resync (BAL-02, F-36)");
    Econ::CityEconomy ce;
    CHECK(Econ::tieBreakWinner(ce) == 0, "no energy: draw");
    ce.ledger[1].servedMWh = 10.0;
    CHECK(Econ::tieBreakWinner(ce) == 2, "P2 served more");
    ce.ledger[0].servedMWh = 10.2;
    CHECK(Econ::tieBreakWinner(ce) == 0, "within half a MWh: draw");

    Econ::CityEconomy sync;
    sync.districts[0].p1Share = 1.0f;
    sync.districts[3].p1Share = 0.2f;
    Econ::syncDistrictsToCityShare(sync, 0.55f);
    CHECK(std::abs(Econ::cityShareFromDistricts(sync) - 0.55f) < 1e-4f, "resync to " << Econ::cityShareFromDistricts(sync));
    CHECK(sync.districts[0].p1Share <= 1.0f, "districts stay within 0..1");
    endGroup();
}

} // namespace

int main() {
    std::cout << "========================================================\n";
    std::cout << " ENERGY CRISIS - CITY ECONOMY TESTS\n";
    const char* seed = std::getenv("EC_SEED");
    std::cout << " EC_SEED: " << (seed ? seed : "(not set, clock seeds)") << "\n";
    std::cout << "========================================================\n";

    testDemandProfiles();
    testVerdictMath();
    testQuotas();
    testProportionalVerdict();
    testDistrictAllocation();
    testAntiSnowball();
    testLevyAndSubsidy();
    testPeakPricing();
    testGridFrequency();
    testLedger();
    testForecastAndDayCut();
    testTieBreakAndSync();

    std::cout << "\n========================================================\n";
    if (g_failures == 0) {
        std::cout << " ALL " << g_checks << " CITY ECONOMY CHECKS PASSED\n";
    } else {
        std::cout << " " << g_failures << " OF " << g_checks << " CITY ECONOMY CHECKS FAILED\n";
    }
    std::cout << "========================================================\n";
    return (g_failures == 0) ? 0 : 1;
}
