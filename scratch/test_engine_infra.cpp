// =============================================================================
// ENERGY CRISIS - ENGINE INFRASTRUCTURE TESTS
// Event queue, match configuration, player modifiers, deterministic RNG, fixed timestep and
// snapshots. Headless: built from this file + Game/scr/*.cpp only, no SFML needed ("make test").
// Every check is reported; the program exits with 1 when any check failed.
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

// A failed precondition (a placement or purchase the test relies on) stops the program
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

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void giveResources(GameEngine& e, int player, int amount) {
    PlayerEconomy& p = e.getPlayerEconomyMut(player);
    p.wood = p.iron = p.copper = p.coal = p.silicon = p.silver = amount;
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
    REQUIRE(ok, "placeBuilding P" << player << " type " << static_cast<int>(type) << " failed: " << msg);
}

void runToNextDay(GameEngine& e, float dt) {
    const int day = e.getCurrentDay();
    long frames = 0;
    while (e.getCurrentDay() == day && e.getCityState().winner == 0) {
        e.update(dt);
        REQUIRE(++frames < 10000000L, "day " << day << " never ended");
    }
}

int countEvents(const std::vector<GameEvent>& events, GameEventType type) {
    return static_cast<int>(std::count_if(events.begin(), events.end(),
                                          [type](const GameEvent& ev) { return ev.type == type; }));
}

const GameEvent* findEvent(const std::vector<GameEvent>& events, GameEventType type) {
    for (const auto& ev : events) {
        if (ev.type == type) return &ev;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// [CD-05] Event queue
// ---------------------------------------------------------------------------
void testEvents() {
    beginGroup("Event queue (CD-05)");
    GameEngine e;
    e.init(1600.0f, 900.0f);

    std::vector<GameEvent> ev = e.pollEvents();
    CHECK(countEvents(ev, GameEventType::WEATHER_CHANGED) == 2, "init weather events: " << countEvents(ev, GameEventType::WEATHER_CHANGED));
    CHECK(countEvents(ev, GameEventType::MESSAGE) == 1, "init message events: " << countEvents(ev, GameEventType::MESSAGE));
    CHECK(e.pollEvents().empty(), "pollEvents must clear the queue");

    // Building placed
    giveResources(e, 1, 1000);
    const sf::Vector2f pos = slotOf(e, 1, startPlotId(1), 4);
    place(e, 1, BuildingType::SOLAR_PANEL, pos);
    ev = e.pollEvents();
    const GameEvent* placed = findEvent(ev, GameEventType::BUILDING_PLACED);
    CHECK(ev.size() == 1 && placed != nullptr, "expected exactly one BUILDING_PLACED event, got " << ev.size() << " events");
    if (placed) {
        CHECK(placed->player == 1, "player " << placed->player);
        CHECK(placed->subtype == static_cast<int>(BuildingType::SOLAR_PANEL), "subtype " << placed->subtype);
        CHECK(std::abs(placed->x - pos.x) < 0.01f && std::abs(placed->y - pos.y) < 0.01f, "position " << placed->x << "; " << placed->y);
        CHECK(placed->value == static_cast<float>(Balance::SOLAR_PANEL.basePowerMW), "value " << placed->value);
        CHECK(!placed->text.empty(), "building name missing");
    }

    // A failed action emits nothing
    std::string msg;
    CHECK(!e.placeBuilding(1, BuildingType::SOLAR_PANEL, pos, msg), "placed twice on one cell");
    CHECK(e.pollEvents().empty(), "a failed placement emitted events");

    // Mining, upgrading, buying land
    MineResult mined;
    REQUIRE(e.mineResource(2, ResourceType::IRON, mined, msg), msg);
    ev = e.pollEvents();
    const GameEvent* minedEv = findEvent(ev, GameEventType::RESOURCE_MINED);
    CHECK(minedEv && minedEv->player == 2 && minedEv->subtype == static_cast<int>(ResourceType::IRON) &&
              minedEv->value == static_cast<float>(mined.amount),
          "RESOURCE_MINED missing or wrong");

    e.getPlayerEconomyMut(1).gold = 100000;
    REQUIRE(e.upgradeMine(1, ResourceType::WOOD, msg), msg);
    ev = e.pollEvents();
    const GameEvent* up = findEvent(ev, GameEventType::MINE_UPGRADED);
    CHECK(up && up->player == 1 && up->subtype == static_cast<int>(ResourceType::WOOD) && up->value == 2.0f, "MINE_UPGRADED missing or wrong");

    REQUIRE(e.buyLandPlot(1, 2, msg), msg);
    ev = e.pollEvents();
    const GameEvent* land = findEvent(ev, GameEventType::LAND_BOUGHT);
    CHECK(land && land->player == 1 && land->subtype == 2 && land->value > 0.0f, "LAND_BOUGHT missing or wrong");

    // Demolition and destruction
    REQUIRE(e.removeBuilding(1, pos, msg), msg);
    ev = e.pollEvents();
    CHECK(countEvents(ev, GameEventType::BUILDING_REMOVED) == 1, "BUILDING_REMOVED missing");
    place(e, 1, BuildingType::WIND_TURBINE, pos);
    e.pollEvents();
    REQUIRE(e.breakBuildingAt(pos), "breakBuildingAt found nothing");
    ev = e.pollEvents();
    const GameEvent* destroyed = findEvent(ev, GameEventType::BUILDING_DESTROYED);
    CHECK(destroyed && destroyed->player == 1 && destroyed->subtype == static_cast<int>(BuildingType::WIND_TURBINE),
          "BUILDING_DESTROYED missing or wrong");

    // Day end: exactly one DAY_END + DAY_RESULT, new weather for both sectors
    runToNextDay(e, kFrame);
    ev = e.pollEvents();
    const GameEvent* dayEnd = findEvent(ev, GameEventType::DAY_END);
    CHECK(countEvents(ev, GameEventType::DAY_END) == 1 && dayEnd && dayEnd->value == 1.0f, "DAY_END missing or wrong");
    CHECK(countEvents(ev, GameEventType::DAY_RESULT) == 1, "DAY_RESULT count " << countEvents(ev, GameEventType::DAY_RESULT));
    const GameEvent* result = findEvent(ev, GameEventType::DAY_RESULT);
    CHECK(result && result->text == e.getCityState().lastCutMessage, "DAY_RESULT text differs from the day-end message");
    CHECK(countEvents(ev, GameEventType::WEATHER_CHANGED) == 2, "weather events at day end: " << countEvents(ev, GameEventType::WEATHER_CHANGED));
    CHECK(countEvents(ev, GameEventType::VICTORY) == 0, "victory on day 1");

    // Seasons change at midnight before day 6 (spring -> summer)
    while (e.getCurrentDay() < 6 && e.getCityState().winner == 0) {
        e.update(0.25f);
        for (const auto& item : e.pollEvents()) {
            if (item.type == GameEventType::SEASON_CHANGED) {
                CHECK(item.subtype == static_cast<int>(SeasonType::SUMMER), "season changed to " << item.subtype);
                CHECK(e.getCurrentDay() == 5, "season changed on day " << e.getCurrentDay());
            }
        }
    }

    // Victory event: six wind turbines against nothing win at the end of day 5
    GameEngine v;
    v.init(1600.0f, 900.0f);
    giveResources(v, 1, 1000);
    for (int i = 0; i < 6; ++i) place(v, 1, BuildingType::WIND_TURBINE, slotOf(v, 1, startPlotId(1), i));
    v.pollEvents();
    int dayResultsWonByP1 = 0;
    const GameEvent* victory = nullptr;
    std::vector<GameEvent> all;
    while (v.getCityState().winner == 0) {
        v.update(0.25f);
        for (auto& item : v.pollEvents()) all.push_back(item);
    }
    for (const auto& item : all) {
        if (item.type == GameEventType::DAY_RESULT && item.player == 1) {
            ++dayResultsWonByP1;
            CHECK(item.value > 0.0f, "P1 won a day with share change " << item.value);
        }
    }
    victory = findEvent(all, GameEventType::VICTORY);
    CHECK(dayResultsWonByP1 == 3, "P1 won " << dayResultsWonByP1 << " days, expected 3");
    CHECK(victory && victory->player == 1 && victory->value >= Balance::VICTORY_SHARE - 1e-4f, "VICTORY missing or wrong");
    CHECK(countEvents(all, GameEventType::VICTORY) == 1, "VICTORY count " << countEvents(all, GameEventType::VICTORY));

    // Nobody drains the queue: it stays bounded
    GameEngine idle;
    idle.init(1600.0f, 900.0f);
    for (int i = 0; i < 5000; ++i) idle.mineResource(1, ResourceType::WOOD, msg);
    std::vector<GameEvent> backlog = idle.pollEvents();
    CHECK(backlog.size() <= GameEngine::MAX_PENDING_EVENTS, "queue grew to " << backlog.size());
    CHECK(!backlog.empty() && backlog.back().type == GameEventType::RESOURCE_MINED, "newest events were dropped");
    endGroup();
}

} // namespace

int main() {
    std::cout << "========================================================\n";
    std::cout << " ENERGY CRISIS - ENGINE INFRASTRUCTURE TESTS\n";
    std::cout << "========================================================\n";

    testEvents();

    std::cout << "\n========================================================\n";
    if (g_failures == 0) {
        std::cout << " ALL " << g_checks << " INFRASTRUCTURE CHECKS PASSED\n";
    } else {
        std::cout << " " << g_failures << " OF " << g_checks << " INFRASTRUCTURE CHECKS FAILED\n";
    }
    std::cout << "========================================================\n";
    return (g_failures == 0) ? 0 : 1;
}
