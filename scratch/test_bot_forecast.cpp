// =============================================================================
// ENERGY CRISIS - BOT FORECAST TESTS ([AI team], governor and НЕВЪЗМОЖНО foresight)
// Headless: built from this file + Game/scr/*.cpp only, no SFML needed ("make test").
// The projection of the day being settled must match the engine's real day average.
// Exits with 1 when any check failed.
// =============================================================================
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include "../Game/includes/game_forecast.h"

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

sf::Vector2f slotOf(const GameEngine& e, int player, int plotId, int sub) {
    int idx = (plotId - 1) % 12;
    int col = (idx % 3) * 3 + sub % 3;
    int row = (idx / 3) * 3 + sub / 3;
    return e.getGridSlot(player, col, row);
}

void place(GameEngine& e, int player, BuildingType type, sf::Vector2f pos) {
    auto& p = e.getPlayerEconomyMut(player);
    p.wood = p.iron = p.copper = p.coal = p.silicon = p.silver = 1000;
    std::string msg;
    bool ok = e.placeBuilding(player, type, pos, msg);
    REQUIRE(ok, "placeBuilding P" << player << " failed: " << msg);
}

void buyPlot(GameEngine& e, int player, int plotId) {
    e.getPlayerEconomyMut(player).gold += 100000;
    std::string msg;
    REQUIRE(e.buyLandPlot(player, plotId, msg), msg);
}

// Advance until the given clock hour of the current day (06:00 -> 06:00 ordering)
void runToHour(GameEngine& e, float hour) {
    auto key = [](float h) { return h < Balance::CLOCK_HOUR_AT_ZERO ? h + 24.0f : h; };
    int day = e.getCurrentDay();
    while (key(e.getHour24()) < key(hour) && e.getCurrentDay() == day) e.update(kFrame);
}

// Runs to the end of the current day and returns the player's real day average (last frame before settlement)
float runDayAndMeasure(GameEngine& e, int player) {
    int day = e.getCurrentDay();
    float last = 0.0f;
    long guard = 0;
    while (e.getCurrentDay() == day && ++guard < 100000) {
        last = e.getTodayAverageMW(player);
        e.update(kFrame);
    }
    return last;
}

void begin(const char* name) { std::cout << "\n[" << name << "]\n"; }

void testDemandAndClock() {
    begin("demand schedule and hours until the 06:00 settlement");
    CHECK(Forecast::demandForDay(1) == 0 && Forecast::demandForDay(2) == 0, "grace days demand nothing");
    CHECK(Forecast::demandForDay(3) == 30 && Forecast::demandForDay(4) == 45 && Forecast::demandForDay(20) == 285,
          "30 MW on day 3, then +15 MW per day");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    CHECK(std::abs(Forecast::hoursUntilRollover(e) - 22.0f) < 0.01f, "match starts at 08:00 -> 22 h left");
}

void testProjectionMatchesSettlement(int player) {
    begin(player == 1 ? "projection matches the real day average (P1 mix)" : "projection matches the real day average (P2 mix)");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    int start = (player == 1) ? 1 : 15;
    int river = (player == 1) ? 3 : 13;
    buyPlot(e, player, river);
    // Day 1 and 2 are the grace period; settle the projection on day 3 with a demand of 30 MW
    while (e.getCurrentDay() < 3) e.update(kFrame * 4.0f);
    runToHour(e, 9.0f);
    place(e, player, BuildingType::SOLAR_PANEL, slotOf(e, player, start, 0));
    place(e, player, BuildingType::SOLAR_PANEL, slotOf(e, player, start, 1));
    place(e, player, BuildingType::BATTERY, slotOf(e, player, start, 2));
    place(e, player, BuildingType::WIND_TURBINE, slotOf(e, player, start, 3));
    place(e, player, BuildingType::HYDRO_PLANT, slotOf(e, player, river, 0));
    runToHour(e, 11.0f);
    float early = Forecast::projectTodayAverageMW(e, player);
    runToHour(e, 20.0f);
    place(e, player, BuildingType::LAMP, slotOf(e, player, start, 4)); // night lamp draws 10 MW from now on
    float evening = Forecast::projectTodayAverageMW(e, player);
    runToHour(e, 3.0f);
    float night = Forecast::projectTodayAverageMW(e, player);
    float real = runDayAndMeasure(e, player);
    std::cout << "  projected at 20:00 " << evening << " MW, at 03:00 " << night << " MW, real " << real << " MW\n";
    CHECK(real > 30.0f, "test setup should deliver more than the demand: " << real);
    CHECK(std::abs(evening - real) <= 0.02f * real + 0.5f, "20:00 projection " << evening << " vs real " << real);
    CHECK(std::abs(night - real) <= 0.02f * real + 0.5f, "03:00 projection " << night << " vs real " << real);
    CHECK(early > evening, "the lamp placed at 20:00 lowers the projection (" << early << " -> " << evening << ")");
}

void testWorstCase() {
    begin("worst case over every weather is a lower bound");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    buyPlot(e, 2, 13);
    place(e, 2, BuildingType::WIND_TURBINE, slotOf(e, 2, 15, 0));
    place(e, 2, BuildingType::SOLAR_PANEL, slotOf(e, 2, 15, 1));
    place(e, 2, BuildingType::HYDRO_PLANT, slotOf(e, 2, 13, 0));
    for (int s = 0; s < 4; ++s) {
        SeasonType season = static_cast<SeasonType>(s);
        float worst = Forecast::worstCaseDayAverageMW(e, 2, season, 30);
        CHECK(worst > 0.0f, "season " << s << " worst " << worst);
        for (int w = 0; w < 6; ++w) {
            float v = Forecast::fullDayAverageMW(e, 2, static_cast<WeatherType>(w), season, 30);
            CHECK(worst <= v + 1e-3f, "season " << s << " weather " << w << ": " << v << " < worst " << worst);
        }
    }
    // A hydro plant gives 110 MW x weather all day; wind 85 MW x weather (hour curve averages out)
    float hydroSunny = Forecast::buildingDayAverageMW(BuildingType::HYDRO_PLANT, WeatherType::SUNNY, SeasonType::SPRING);
    CHECK(std::abs(hydroSunny - 77.0f) < 0.5f, "hydro on a sunny day " << hydroSunny);
    float windStorm = Forecast::buildingDayAverageMW(BuildingType::WIND_TURBINE, WeatherType::STORMY, SeasonType::SPRING);
    CHECK(std::abs(windStorm - 187.0f) < 2.0f, "wind in a storm " << windStorm);
    float solarNight = Forecast::buildingDayAverageMW(BuildingType::SOLAR_PANEL, WeatherType::SUNNY, SeasonType::WINTER);
    CHECK(solarNight > 5.0f && solarNight < 30.0f, "solar averages well below its 60 MW peak: " << solarNight);
}

} // namespace

int main() {
    std::cout << "========================================================\n";
    std::cout << " ENERGY CRISIS - BOT FORECAST TESTS\n";
    std::cout << "========================================================\n";
    testDemandAndClock();
    testProjectionMatchesSettlement(1);
    testProjectionMatchesSettlement(2);
    testWorstCase();
    std::cout << "\n========================================================\n";
    if (g_failures == 0) {
        std::cout << " ALL " << g_checks << " FORECAST CHECKS PASSED\n";
        return 0;
    }
    std::cout << " " << g_failures << " OF " << g_checks << " FORECAST CHECKS FAILED\n";
    return 1;
}
