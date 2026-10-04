// [b-effects] Headless test for FxEventTracker (DS-08 juice + F-01 audio triggers) and the
// engine mining hook (GameEngine::getLastMineAction).
// The tracker implementation is included directly so `make test` (engine objects only) links it.
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "game_main.h"
#include "../UI/scr/UI_fx_events.cpp"

static int failures = 0;
#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            ++failures;                                                   \
        }                                                                 \
    } while (0)

static int countType(const std::vector<FxEvent>& ev, FxEvent::Type t) {
    int n = 0;
    for (const auto& e : ev) n += (e.type == t) ? 1 : 0;
    return n;
}

static const FxEvent* findType(const std::vector<FxEvent>& ev, FxEvent::Type t) {
    for (const auto& e : ev)
        if (e.type == t) return &e;
    return nullptr;
}

static void giveAll(GameEngine& g, int p) {
    PlayerEconomy& e = g.getPlayerEconomyMut(p);
    e.wood = e.iron = e.copper = e.coal = e.silicon = e.silver = 500;
    e.gold = 5000;
}

int main() {
    std::printf("[test_fx_events]\n");
    GameEngine g;
    g.init(1600.0f, 900.0f);
    FxEventTracker tr;
    tr.reset(g);
    std::vector<FxEvent> ev;

    tr.poll(g, ev);
    CHECK(ev.empty(), "no events without changes");

    // Mining: human P1 and (bot-driven) P2 both go through mineResource
    std::string msg;
    MineResult res;
    int before = g.getLastMineAction(1).count;
    CHECK(g.mineResource(1, ResourceType::WOOD, res, msg), "P1 mines wood");
    CHECK(g.getLastMineAction(1).count == before + 1, "mining hook counts the action");
    CHECK(g.getLastMineAction(1).type == ResourceType::WOOD && g.getLastMineAction(1).amount == res.amount, "hook records type and amount");
    CHECK(g.mineResource(2, ResourceType::IRON, msg), "P2 mines iron (3-arg overload)");
    ev.clear();
    tr.poll(g, ev);
    CHECK(countType(ev, FxEvent::Type::Mined) == 2, "two mining events");
    bool p1Wood = false, p2Iron = false;
    for (const auto& e : ev) {
        if (e.type == FxEvent::Type::Mined && e.player == 1 && e.resource == ResourceType::WOOD && e.amount > 0) p1Wood = true;
        if (e.type == FxEvent::Type::Mined && e.player == 2 && e.resource == ResourceType::IRON && e.amount > 0) p2Iron = true;
    }
    CHECK(p1Wood && p2Iron, "mining events carry player, resource and amount");
    CHECK(!g.mineResource(1, ResourceType::MONEY, msg), "money mine is gone");
    ev.clear();
    tr.poll(g, ev);
    CHECK(countType(ev, FxEvent::Type::Mined) == 0, "failed mining makes no event");

    // Building placed and removed (daytime, start plot)
    giveAll(g, 1);
    giveAll(g, 2);
    sf::Vector2f slot = g.getGridSlot(1, 0, 0);
    CHECK(g.placeBuilding(1, BuildingType::SOLAR_PANEL, slot, msg), "place solar");
    ev.clear();
    tr.poll(g, ev);
    const FxEvent* built = findType(ev, FxEvent::Type::Built);
    CHECK(built && built->player == 1 && built->building == BuildingType::SOLAR_PANEL, "built event");
    CHECK(built && built->pos.x == slot.x && built->pos.y == slot.y, "built event position");
    CHECK(g.removeBuilding(1, slot, msg), "remove solar");
    ev.clear();
    tr.poll(g, ev);
    const FxEvent* removed = findType(ev, FxEvent::Type::Removed);
    CHECK(removed && removed->player == 1 && removed->building == BuildingType::SOLAR_PANEL, "removed event");

    // Land purchase
    CHECK(g.buyNextLandTier(2, msg), "P2 buys land");
    ev.clear();
    tr.poll(g, ev);
    const FxEvent* land = findType(ev, FxEvent::Type::LandBought);
    CHECK(land && land->player == 2 && land->area.size.x > 10.0f, "land event with plot area");

    // Mine upgrade
    CHECK(g.upgradeMine(1, ResourceType::COPPER, msg), "upgrade copper");
    ev.clear();
    tr.poll(g, ev);
    const FxEvent* up = findType(ev, FxEvent::Type::MineUpgraded);
    CHECK(up && up->player == 1 && up->resource == ResourceType::COPPER && up->amount == 2, "upgrade event with new level");

    // P1 builds generators, then run three days: nightfall/daybreak and settlements are reported
    for (int c = 0; c < 3; ++c) g.placeBuilding(1, BuildingType::HYDRO_PLANT, g.getGridSlot(1, c, 1), msg);
    g.placeBuilding(1, BuildingType::WIND_TURBINE, g.getGridSlot(1, 0, 2), msg);
    ev.clear();
    tr.poll(g, ev);
    int nightfalls = 0, daybreaks = 0, settlements = 0, p1Gains = 0, p2Gains = 0;
    for (int step = 0; step < 3 * 1800 + 10; ++step) {
        g.update(0.05f);
        ev.clear();
        tr.poll(g, ev);
        for (const auto& e : ev) {
            if (e.type == FxEvent::Type::Nightfall) ++nightfalls;
            if (e.type == FxEvent::Type::DayBreak) ++daybreaks;
            if (e.type == FxEvent::Type::Settlement) {
                ++settlements;
                if (e.player == 1) ++p1Gains;
                if (e.player == 2) ++p2Gains;
                CHECK(e.player == 0 || e.value > 0.0f, "settlement delta is positive for the gaining side");
            }
        }
    }
    std::printf("  3 days: %d nightfalls, %d daybreaks, %d settlements (P1 gained %d, P2 gained %d), share %.2f\n",
                nightfalls, daybreaks, settlements, p1Gains, p2Gains, g.getCityState().p1CityShare);
    CHECK(nightfalls == 3 && daybreaks == 3, "one nightfall and one daybreak per day");
    CHECK(settlements == 3, "one settlement event per day end");
    CHECK(p2Gains == 0, "P2 without power never gains");
    CHECK(p1Gains >= 1, "P1 with power gains after the grace period");

    // Restart: the tracker resyncs silently (no bogus 'removed' storm)
    g.restartGame();
    ev.clear();
    tr.poll(g, ev);
    CHECK(ev.empty(), "restart produces no events");
    CHECK(g.getLastMineAction(1).count == 0, "mining record resets with the match");

    if (failures) {
        std::printf("[test_fx_events] FAILED (%d)\n", failures);
        return 1;
    }
    std::printf("[test_fx_events] PASS\n");
    return 0;
}
