// =============================================================================
// ENERGY CRISIS - SHOWCASE TESTS [b-showcase]
//   - engine: the day-end verdict record used by the blackout set piece (HX-04)
//   - presentation model: living skyline plan (HX-03) and blackout timeline (HX-04)
// Headless: this file + Game/scr/*.cpp, no SFML ("make test" or build_headless.sh).
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
#include "../UI/includes/UI_showcaseModel.h"

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

void beginGroup(const std::string& name) {
    g_groupFailures = 0;
    std::cout << "\n[" << name << "]\n";
}

void endGroup() {
    std::cout << (g_groupFailures == 0 ? "  -> PASS\n" : "  -> FAIL\n");
}

void runToNextDay(GameEngine& e) {
    int day = e.getCurrentDay();
    for (int guard = 0; guard < 100000 && e.getCurrentDay() == day && e.getCityState().winner == 0; ++guard) {
        e.update(kFrame * 6.0f);
    }
}

// Six wind turbines on the starting plot: far above the demand of the first settlements
void buildWindFarm(GameEngine& e, int player) {
    auto& econ = e.getPlayerEconomyMut(player);
    econ.wood = econ.iron = econ.copper = econ.coal = econ.silicon = econ.silver = 1000;
    for (int i = 0; i < 6; ++i) {
        int plotIdx = (player == 1) ? 0 : 2; // starting plot column on screen
        sf::Vector2f pos = e.getGridSlot(player, plotIdx * 3 + i % 3, i / 3);
        std::string msg;
        bool ok = e.placeBuilding(player, BuildingType::WIND_TURBINE, pos, msg);
        REQUIRE(ok, "placeBuilding failed: " << msg);
    }
}

// -----------------------------------------------------------------------------
// Engine: DaySettlement record
// -----------------------------------------------------------------------------
void testSettlementRecord() {
    beginGroup("Engine records every day-end verdict (HX-04 hook)");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    CHECK(e.getLastDaySettlement().serial == 0, "fresh match serial " << e.getLastDaySettlement().serial);

    buildWindFarm(e, 1);
    runToNextDay(e);
    const DaySettlement& d1 = e.getLastDaySettlement();
    CHECK(d1.serial == 1, "serial after day 1: " << d1.serial);
    CHECK(d1.day == 1, "settled day " << d1.day);
    CHECK(d1.graceDay, "day 1 is a grace day");
    CHECK(!d1.p1Failed && !d1.p2Failed, "nobody fails during the grace period");
    CHECK(d1.demandMW == 0, "grace demand " << d1.demandMW);

    runToNextDay(e);
    CHECK(e.getLastDaySettlement().serial == 2 && e.getLastDaySettlement().graceDay, "day 2 is grace too");

    const float shareBefore = e.getCityState().p1CityShare;
    runToNextDay(e); // day 3: first real verdict, P1 has a wind farm, P2 has nothing
    const DaySettlement& d3 = e.getLastDaySettlement();
    CHECK(d3.serial == 3, "serial " << d3.serial);
    CHECK(d3.day == 3, "settled day " << d3.day);
    CHECK(!d3.graceDay, "day 3 has a verdict");
    CHECK(d3.demandMW == Balance::STARTING_CITY_DEMAND_MW, "demand " << d3.demandMW);
    CHECK(!d3.p1Failed, "P1 delivered " << d3.p1AvgMW << " MW");
    CHECK(d3.p2Failed, "P2 delivered " << d3.p2AvgMW << " MW");
    CHECK(d3.p2AvgMW == 0, "P2 average " << d3.p2AvgMW);
    CHECK(std::abs((e.getCityState().p1CityShare - shareBefore) - d3.shareShift) < 1e-5f,
          "recorded shift " << d3.shareShift << " vs real " << (e.getCityState().p1CityShare - shareBefore));
    CHECK(d3.shareShift > 0.09f, "P1 gained territory: " << d3.shareShift);

    // Both fail when nobody has power
    GameEngine both;
    both.init(1600.0f, 900.0f);
    for (int i = 0; i < 3; ++i) runToNextDay(both);
    CHECK(both.getLastDaySettlement().p1Failed && both.getLastDaySettlement().p2Failed, "both failed day 3");
    CHECK(std::abs(both.getLastDaySettlement().shareShift) < 1e-6f, "no shift when both fail");

    // Restart forgets the record
    e.restartGame();
    CHECK(e.getLastDaySettlement().serial == 0, "serial after restart " << e.getLastDaySettlement().serial);
    endGroup();
}

// -----------------------------------------------------------------------------
// Presentation model: living skyline plan
// -----------------------------------------------------------------------------
void testSkylinePlan() {
    beginGroup("Living skyline plan is mirrored, inside the city and grows monotonically (HX-03)");
    using namespace Showcase;
    const auto& plan = skylinePlan();
    REQUIRE(!plan.empty() && plan.size() % 2 == 0, "plan size " << plan.size());

    // The eight front towers drawn by UI_city::drawCity (x, w, top)
    const float front[8][3] = { { 618, 38, 150 }, { 660, 44, 105 }, { 708, 36, 175 }, { 748, 38, 120 },
                                { 814, 38, 120 }, { 856, 36, 175 }, { 896, 44, 105 }, { 944, 38, 150 } };

    for (size_t i = 0; i < plan.size(); i += 2) {
        const TowerDef& w = plan[i];
        const TowerDef& e = plan[i + 1];
        CHECK(std::abs(e.x - mirrorX(w.x, w.w)) < 0.01f && e.w == w.w && e.topY == w.topY && e.day == w.day,
              "entry " << i << " is not mirrored");
        for (const TowerDef* t : { &w, &e }) {
            CHECK(t->x >= CITY_LEFT && t->x + t->w <= CITY_RIGHT, "tower x " << t->x << " w " << t->w << " leaves the city");
            CHECK(t->topY >= CITY_SKY_LIMIT_Y, "tower top " << t->topY << " rises into the header banner");
            CHECK(t->topY < t->baseY, "tower " << t->x << " has no height");
            bool west = t->x + t->w <= 789.0f, east = t->x >= 811.0f;
            CHECK(west || east, "tower at x " << t->x << " stands in the river");
            CHECK(t->day >= 1 && t->day <= Balance::FINAL_DAY, "day " << t->day);
        }
        if (w.layer == TowerLayer::FRONT_EXTENSION) {
            bool onFront = false;
            for (const auto& f : front) {
                if (std::abs(f[0] - w.x) < 0.01f && std::abs(f[1] - w.w) < 0.01f) onFront = true;
            }
            CHECK(onFront, "extension at x " << w.x << " does not sit on a front tower");
        }
    }

    // Stacked extensions continue each other (the second floor starts at the first one's roof)
    for (size_t i = 0; i < plan.size(); ++i) {
        if (plan[i].layer != TowerLayer::FRONT_EXTENSION) continue;
        bool baseMatches = false;
        for (const auto& f : front) {
            if (std::abs(f[0] - plan[i].x) < 0.01f && std::abs(f[2] - plan[i].baseY) < 0.01f) baseMatches = true;
        }
        for (size_t j = 0; j < plan.size(); ++j) {
            if (j != i && plan[j].layer == TowerLayer::FRONT_EXTENSION && plan[j].x == plan[i].x &&
                plan[j].day < plan[i].day && std::abs(plan[j].topY - plan[i].baseY) < 0.01f) {
                baseMatches = true;
            }
        }
        CHECK(baseMatches, "extension " << i << " floats (base " << plan[i].baseY << ")");
    }

    int prev = -1;
    for (int day = 1; day <= Balance::FINAL_DAY; ++day) {
        int n = towersStartedByDay(day);
        CHECK(n >= prev, "day " << day << " has fewer towers than the day before");
        prev = n;
    }
    CHECK(towersStartedByDay(1) >= 2, "day 1 already has a skyline");
    CHECK(towersStartedByDay(Balance::FINAL_DAY) == static_cast<int>(plan.size()), "everything is built by the final day");
    CHECK(towersStartedByDay(10) > towersStartedByDay(1), "the city grows during the match");

    CHECK(riseEase(0.0f) == 0.0f && std::abs(riseEase(1.0f) - 1.0f) < 1e-6f && riseEase(2.0f) == 1.0f, "ease ends");
    for (const TowerDef& t : plan) {
        float last = 1e9f;
        for (float s = 0.0f; s <= TOWER_RISE_SEC + 1.0f; s += 0.25f) {
            float y = currentTopY(t, s);
            CHECK(y <= last + 1e-4f, "tower top moves down while rising");
            last = y;
        }
        CHECK(std::abs(currentTopY(t, TOWER_RISE_SEC) - t.topY) < 1e-3f, "rise ends at the roof line");
        CHECK(currentTopY(t, 0.0f) <= t.baseY, "rise starts at or below the base");
    }
    CHECK(craneVisible(0.0f) && craneVisible(TOWER_RISE_SEC) && !craneVisible(TOWER_RISE_SEC + CRANE_LINGER_SEC + 0.1f),
          "crane only during construction");
    CHECK(DISTRICT_BROWNOUT_PERCENT < DISTRICT_LIT_PERCENT, "brownout must be darker");
    endGroup();
}

// -----------------------------------------------------------------------------
// Presentation model: blackout timeline
// -----------------------------------------------------------------------------
void testBlackoutTimeline() {
    beginGroup("Blackout set piece: dim, street cascade, banner and full recovery (HX-04)");
    using namespace Showcase;
    CHECK(blackoutDim(0.0f) == 0.0f && blackoutDim(BLACKOUT_DURATION) == 0.0f, "no dim outside the set piece");
    CHECK(std::abs(blackoutDim(2.0f) - 1.0f) < 1e-6f, "full dim in the middle");
    CHECK(blackoutBannerAlpha(0.1f) == 0.0f && std::abs(blackoutBannerAlpha(2.0f) - 1.0f) < 1e-6f &&
          blackoutBannerAlpha(BLACKOUT_DURATION) == 0.0f, "banner timing");
    CHECK(blackoutCityDark(BLACKOUT_DURATION + 0.1f) == 0.0f, "city dark fades out");

    const float cascadeEnd = BLACKOUT_CASCADE_START + BLACKOUT_STREETS * BLACKOUT_STREET_GAP + BLACKOUT_ROW_RIPPLE + 0.06f;
    CHECK(cascadeEnd < BLACKOUT_RECOVER_START, "the whole district is dark before the lights return");

    int darkAtHold = 0, total = 0, recovered = 0, beforeAtStart = 0;
    for (int s = 0; s < 40; ++s) {
        float d = s / 39.0f;
        for (int r = 0; r <= 10; ++r) {
            float rowFrac = r / 10.0f;
            for (int hash = 0; hash < 100; hash += 7) {
                ++total;
                if (blackoutWindowPhase(0.01f, d, rowFrac, hash) == WindowPhase::BEFORE) ++beforeAtStart;
                if (blackoutWindowPhase(cascadeEnd + 0.01f, d, rowFrac, hash) == WindowPhase::DARK) ++darkAtHold;
                if (blackoutWindowPhase(BLACKOUT_DURATION, d, rowFrac, hash) == WindowPhase::RECOVERED) ++recovered;
            }
        }
    }
    CHECK(beforeAtStart == total, "windows still lit at the verdict: " << beforeAtStart << "/" << total);
    CHECK(darkAtHold == total, "dark windows at the hold: " << darkAtHold << "/" << total);
    CHECK(recovered == total, "recovered windows: " << recovered << "/" << total);

    // Street by street from the capture line outwards: a nearer street never dies later
    for (int s = 1; s < BLACKOUT_STREETS; ++s) {
        float nearD = (s - 1 + 0.5f) / BLACKOUT_STREETS, farD = (s + 0.5f) / BLACKOUT_STREETS;
        CHECK(blackoutOffTime(nearD, 0.5f, 3) < blackoutOffTime(farD, 0.5f, 3), "street " << s << " order");
        CHECK(blackoutStreetDark(BLACKOUT_CASCADE_START + s * BLACKOUT_STREET_GAP - 0.01f, s) == 0.0f,
              "street band " << s << " darkens too early");
    }
    CHECK(blackoutStreet(0.0f) == 0 && blackoutStreet(1.0f) == BLACKOUT_STREETS - 1 && blackoutStreet(-1.0f) == 0,
          "street index clamps");
    // Top floors die first inside a street
    CHECK(blackoutOffTime(0.1f, 0.0f, 5) < blackoutOffTime(0.1f, 1.0f, 5), "top-to-bottom ripple");
    endGroup();
}

} // namespace

int main() {
    std::cout << "========================================================\n";
    std::cout << " ENERGY CRISIS - SHOWCASE TESTS\n";
    std::cout << "========================================================\n";

    testSettlementRecord();
    testSkylinePlan();
    testBlackoutTimeline();

    std::cout << "\n========================================================\n";
    if (g_failures == 0) {
        std::cout << " ALL " << g_checks << " SHOWCASE CHECKS PASSED\n";
    } else {
        std::cout << " " << g_failures << " OF " << g_checks << " SHOWCASE CHECKS FAILED\n";
    }
    std::cout << "========================================================\n";
    return g_failures == 0 ? 0 : 1;
}
