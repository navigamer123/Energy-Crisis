// =============================================================================
// ENERGY CRISIS - SAVE / LOAD SNAPSHOT TESTS (team b-session, F-18)
// Headless: built from this file + Game/scr/*.cpp only ("make test" / build_headless.sh).
// Checks that GameEngine::saveSnapshot / loadSnapshot round-trip every piece of match
// state exactly, that a loaded match keeps playing identically, and that damaged
// snapshots are rejected without touching the running match.
// =============================================================================
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "../Game/includes/game_main.h"

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond, details)                                                                   \
    do {                                                                                       \
        ++g_checks;                                                                            \
        if (!(cond)) {                                                                         \
            ++g_failures;                                                                      \
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

sf::Vector2f slotOf(const GameEngine& e, int player, int plotId, int sub) {
    int idx = (plotId - 1) % 12;
    int col = (idx % 3) * 3 + sub % 3;
    int row = (idx / 3) * 3 + sub / 3;
    return e.getGridSlot(player, col, row);
}

void give(GameEngine& e, int player, int amount) {
    PlayerEconomy& p = e.getPlayerEconomyMut(player);
    p.wood = p.iron = p.copper = p.coal = p.silicon = p.silver = amount;
    p.gold += amount;
}

std::string snapshotOf(const GameEngine& e) {
    std::ostringstream o;
    bool ok = e.saveSnapshot(o);
    REQUIRE(ok, "saveSnapshot failed");
    return o.str();
}

// A match in an interesting state: day 4, bought land, buildings of four types,
// charged batteries, upgraded mines, a selected building, a lightning loss and mid-day totals.
GameEngine makeBusyMatch() {
    GameEngine e;
    e.init(1600.0f, 900.0f);
    give(e, 1, 5000);
    give(e, 2, 5000);
    std::string msg;
    REQUIRE(e.buyLandPlot(1, 2, msg), "buy P1 plot 2: " << msg);
    REQUIRE(e.buyLandPlot(2, 14, msg), "buy P2 plot 14: " << msg);
    REQUIRE(e.placeBuilding(1, BuildingType::SOLAR_PANEL, slotOf(e, 1, 1, 0), msg), "P1 solar: " << msg);
    REQUIRE(e.placeBuilding(1, BuildingType::WIND_TURBINE, slotOf(e, 1, 1, 1), msg), "P1 wind: " << msg);
    REQUIRE(e.placeBuilding(1, BuildingType::BATTERY, slotOf(e, 1, 1, 2), msg), "P1 battery: " << msg);
    REQUIRE(e.placeBuilding(1, BuildingType::LAMP, slotOf(e, 1, 2, 0), msg), "P1 lamp: " << msg);
    REQUIRE(e.placeBuilding(2, BuildingType::WIND_TURBINE, slotOf(e, 2, 15, 0), msg), "P2 wind: " << msg);
    REQUIRE(e.placeBuilding(2, BuildingType::WIND_TURBINE, slotOf(e, 2, 15, 1), msg), "P2 wind 2: " << msg);
    REQUIRE(e.upgradeMine(1, ResourceType::WOOD, msg), "P1 upgrade wood: " << msg);
    e.cycleBuildingSelection(2);

    // Play into day 4 at 1/60 s frames (grace period over, real settlements happened)
    while (e.getCurrentDay() < 4) e.update(1.0f / 60.0f);
    for (int i = 0; i < 600; ++i) e.update(1.0f / 60.0f); // 10 s into day 4: non-zero daily totals
    sf::Vector2f brokenPos;
    e.breakRandomBuilding(2, brokenPos);
    return e;
}

void testRoundTrip() {
    std::cout << "\n[Round trip: save -> load -> save gives the identical snapshot]\n";
    GameEngine a = makeBusyMatch();
    std::string s1 = snapshotOf(a);
    CHECK(s1.find("engine_version=1") == 0, "header missing: " << s1.substr(0, 40));
    CHECK(s1.find("end=engine") != std::string::npos, "end marker missing");

    GameEngine b;
    b.init(1600.0f, 900.0f);
    std::istringstream in(s1);
    std::string err;
    bool ok = b.loadSnapshot(in, &err);
    REQUIRE(ok, "loadSnapshot failed: " << err);
    std::string s2 = snapshotOf(b);
    CHECK(s1 == s2, "snapshots differ after a round trip");

    // Spot checks through the public API
    CHECK(b.getCurrentDay() == a.getCurrentDay(), b.getCurrentDay() << " vs " << a.getCurrentDay());
    CHECK(b.getBuildings().size() == a.getBuildings().size(), "building count");
    CHECK(b.getLandPlots().size() == a.getLandPlots().size(), "plot count");
    CHECK(b.getPlayerEconomy(1).gold == a.getPlayerEconomy(1).gold, "P1 gold");
    CHECK(b.getPlayerEconomy(1).mineLevels[static_cast<int>(ResourceType::WOOD)] ==
              a.getPlayerEconomy(1).mineLevels[static_cast<int>(ResourceType::WOOD)], "mine level");
    CHECK(b.getSelectedBuilding(2) == a.getSelectedBuilding(2), "P2 selection");
    CHECK(b.getCityState().p1CityShare == a.getCityState().p1CityShare, "city share");
    CHECK(b.getCityState().lastCutMessage == a.getCityState().lastCutMessage, "Bulgarian day message");
    // Tolerance: 32-bit x87 builds compare an 80-bit register with a rounded float
    CHECK(std::fabs(b.getTodayAverageMW(1) - a.getTodayAverageMW(1)) < 1e-3f, "today's delivered average");
    CHECK(b.getPlayerWeather(1) == a.getPlayerWeather(1) && b.getSeason() == a.getSeason(), "weather/season");
    // Lightning removes a facility (it does not leave a broken one): P2 keeps exactly one turbine
    int p2Buildings = 0;
    for (const auto& bl : b.getBuildings()) p2Buildings += (bl.playerOwner == 2) ? 1 : 0;
    CHECK(p2Buildings == 1, "P2 buildings after the lightning strike: " << p2Buildings);
}

void testContinuesIdentically() {
    std::cout << "\n[A loaded match plays on exactly like the original (same day, no RNG)]\n";
    GameEngine a = makeBusyMatch();
    GameEngine b;
    b.init(1600.0f, 900.0f);
    std::istringstream in(snapshotOf(a));
    REQUIRE(b.loadSnapshot(in, nullptr), "load");
    // 20 game-seconds stay inside day 4 (the day-end weather roll is the only RNG use)
    for (int i = 0; i < 1200; ++i) {
        a.update(1.0f / 60.0f);
        b.update(1.0f / 60.0f);
    }
    REQUIRE(a.getCurrentDay() == 4, "test left day 4");
    CHECK(snapshotOf(a) == snapshotOf(b), "simulation diverged after loading");
    CHECK(a.getPlayerEconomy(1).money == b.getPlayerEconomy(1).money, a.getPlayerEconomy(1).money << " vs " << b.getPlayerEconomy(1).money);
}

// Every damaged variant must be refused and leave the running engine exactly as it was
void expectRejected(const std::string& label, const std::string& text) {
    GameEngine running = makeBusyMatch();
    std::string before = snapshotOf(running);
    std::istringstream in(text);
    std::string err;
    bool ok = running.loadSnapshot(in, &err);
    CHECK(!ok, label << ": damaged snapshot was accepted");
    CHECK(!err.empty(), label << ": no error message");
    CHECK(snapshotOf(running) == before, label << ": running match changed by a failed load");
    std::cout << "  " << label << " -> rejected (" << err << ")\n";
}

std::string replaceLine(const std::string& s, const std::string& keyPrefix, const std::string& newLine) {
    size_t pos = s.find("\n" + keyPrefix);
    if (pos == std::string::npos) return s;
    size_t end = s.find('\n', pos + 1);
    return s.substr(0, pos + 1) + newLine + s.substr(end);
}

void testRejectsDamage() {
    std::cout << "\n[Damaged snapshots are refused, the match stays untouched]\n";
    std::string good = snapshotOf(makeBusyMatch());
    expectRejected("empty input", "");
    expectRejected("truncated (no end marker)", good.substr(0, good.size() / 2));
    expectRejected("future version", replaceLine("\n" + good, "engine_version=", "engine_version=99"));
    expectRejected("day 0", replaceLine(good, "currentDay=", "currentDay=0"));
    expectRejected("winner 7", replaceLine(good, "city.winner=", "city.winner=7"));
    expectRejected("NaN city share", replaceLine(good, "city.p1Share=", "city.p1Share=nan"));
    expectRejected("missing P2 gold", replaceLine(good, "p2.gold=", "p2.goldX=1"));
    expectRejected("bad building type", replaceLine(good, "building=", "building=9,1,1,1,0,0,0,200,150,0"));
    expectRejected("building count mismatch", replaceLine(good, "buildings=", "buildings=999"));
    expectRejected("bad plot owner", replaceLine(good, "plot=", "plot=1,3,258,105,105,95,1,0"));
    expectRejected("seven mine levels", replaceLine(good, "p1.mineLevels=", "p1.mineLevels=1,1,1,1,1,1,1"));
}

void testFinishedAndFreshMatches() {
    std::cout << "\n[A fresh match and a CRLF-converted file both load]\n";
    GameEngine fresh;
    fresh.init(1600.0f, 900.0f);
    std::string s = snapshotOf(fresh);
    // Windows tools may convert the file to CRLF: the loader must not care
    std::string crlf;
    for (char c : s) {
        if (c == '\n') crlf += "\r\n";
        else crlf += c;
    }
    GameEngine b;
    b.init(1600.0f, 900.0f);
    std::istringstream in(crlf);
    std::string err;
    CHECK(b.loadSnapshot(in, &err), "CRLF snapshot refused: " << err);
    CHECK(snapshotOf(b) == s, "CRLF snapshot loaded differently");
    CHECK(b.getBuildings().empty() && b.getCurrentDay() == 1, "fresh match state");

    // Stream position: the loader stops at end=engine, so a container format can follow it
    std::istringstream two(s + "after=1\n");
    GameEngine c;
    CHECK(c.loadSnapshot(two, &err), "load with trailing data: " << err);
    std::string rest;
    std::getline(two, rest);
    CHECK(rest == "after=1", "loader consumed data after end=engine: '" << rest << "'");
}

} // namespace

int main() {
    std::cout << "========================================================\n";
    std::cout << " ENERGY CRISIS - SAVE / LOAD SNAPSHOT TESTS\n";
    std::cout << "========================================================\n";
    testRoundTrip();
    testContinuesIdentically();
    testRejectsDamage();
    testFinishedAndFreshMatches();
    std::cout << "\n========================================================\n";
    if (g_failures == 0) {
        std::cout << " ALL " << g_checks << " SAVE/LOAD CHECKS PASSED\n";
        return 0;
    }
    std::cout << " " << g_failures << " OF " << g_checks << " SAVE/LOAD CHECKS FAILED\n";
    return 1;
}
