// =============================================================================
// Team b-power: nuclear reactor — unlock, exclusion zone, ramp, fuel, SCRAM (F-32)
// Headless: built from this file + Game/scr/*.cpp only ("make test").
// =============================================================================
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cmath>
#include <cstdlib>
#include <iostream>
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

void rich(GameEngine& e, int player) {
    auto& p = e.getPlayerEconomyMut(player);
    p.wood = p.iron = p.copper = p.coal = p.silicon = p.silver = 5000;
    p.gold = 1000000;
}

std::vector<const LandPlot*> buyPlots(GameEngine& e, int player, int count) {
    std::vector<const LandPlot*> out;
    for (const auto& p : e.getLandPlots()) {
        if (static_cast<int>(out.size()) >= count) break;
        if (p.playerOwner == player && !p.isPurchased) {
            std::string msg;
            if (e.buyLandPlot(player, p.id, msg)) out.push_back(&p);
        }
    }
    return out;
}

const PlacedBuilding* reactorOf(const GameEngine& e, int player) { return e.getReactor(player); }

sf::Vector2f centre(const LandPlot& p) {
    return sf::Vector2f(p.bounds.position.x + p.bounds.size.x * 0.5f, p.bounds.position.y + p.bounds.size.y * 0.5f);
}

void run(GameEngine& e, float seconds) {
    int steps = static_cast<int>(seconds / 0.25f + 0.5f);
    for (int i = 0; i < steps; ++i) e.update(0.25f);
}

} // namespace

int main() {
    std::cout << "[Nuclear reactor (F-32)]\n";
    GameEngine e;
    e.init(1600.0f, 900.0f);
    rich(e, 1);
    rich(e, 2);
    std::string msg, reason;
    // P2 keeps up with the demand (six turbines) so nobody wins and freezes the simulation
    for (int i = 0; i < 6; ++i) {
        std::string m;
        e.placeBuilding(2, BuildingType::WIND_TURBINE, e.getStartPlotSlot(2, i % 3, i / 3), m);
    }

    // --- Locked until 6 plots ---
    const LandPlot* first = buyPlots(e, 1, 1)[0];
    CHECK(!e.canPlaceBuilding(1, BuildingType::NUCLEAR, centre(*first), reason), "reactor allowed with 2 plots");
    CHECK(reason.find("6 ПАРЦЕЛА") != std::string::npos, "reason: " << reason);
    std::vector<const LandPlot*> more = buyPlots(e, 1, 4);
    CHECK(e.countOwnedPlots(1) == 6, "owned " << e.countOwnedPlots(1));
    CHECK(e.isAdvancedUnlocked(1, BuildingType::NUCLEAR, reason), "reactor still locked: " << reason);

    // --- Needs an empty plot, sits in its centre, owns the whole plot ---
    const LandPlot* site = more.back();
    sf::Vector2f corner = e.getGridSlot(1, site->screenCol * 3, site->row * 3);
    CHECK(e.placeBuilding(1, BuildingType::SOLAR_PANEL, corner, msg), msg);
    CHECK(!e.canPlaceBuilding(1, BuildingType::NUCLEAR, centre(*site), reason), "reactor on a used plot");
    CHECK(reason.find("ПРАЗЕН") != std::string::npos, "reason: " << reason);
    CHECK(e.removeBuilding(1, corner, msg), msg);
    int silverBefore = e.getPlayerEconomy(1).silver;
    CHECK(e.placeBuilding(1, BuildingType::NUCLEAR, corner, msg), "place reactor: " << msg); // any slot of the plot
    const PlacedBuilding* r = reactorOf(e, 1);
    CHECK(r != nullptr, "no reactor");
    CHECK(std::abs(r->position.x - centre(*site).x) < 0.01f && std::abs(r->position.y - centre(*site).y) < 0.01f, "reactor not centred");
    CHECK(silverBefore - e.getPlayerEconomy(1).silver == PowerBalance::NUCLEAR.silverCost, "reactor cost");
    CHECK(!e.canPlaceBuilding(1, BuildingType::NUCLEAR, centre(*more[0]), reason) && reason.find("ВЕЧЕ") != std::string::npos,
          "second reactor: " << reason);
    CHECK(!e.canPlaceBuilding(1, BuildingType::WIND_TURBINE, corner, reason) && reason.find("ЗАБРАНЕНА ЗОНА") != std::string::npos,
          "building in the exclusion zone: " << reason);
    CHECK(e.isSlotReserved(1, corner) && !e.isSlotReserved(1, e.getStartPlotSlot(1, 0, 0)), "reserved slots");

    // --- Slow ramp: 0 MW at first, about half after half a day, 400 MW after a day ---
    e.update(0.25f);
    r = reactorOf(e, 1);
    CHECK(r->currentOutputMW < 5.0f, "reactor output right after building: " << r->currentOutputMW);
    run(e, Balance::SECONDS_PER_DAY * 0.5f);
    r = reactorOf(e, 1);
    CHECK(std::abs(r->currentOutputMW - 200.0f) < 6.0f, "half-day ramp " << r->currentOutputMW);
    run(e, Balance::SECONDS_PER_DAY * 0.55f);
    r = reactorOf(e, 1);
    CHECK(std::abs(r->currentOutputMW - 400.0f) < 0.01f, "full ramp " << r->currentOutputMW);

    // --- Weather- and night-proof ---
    float minOut = 1e9f;
    for (int i = 0; i < 360; ++i) { e.update(0.25f); minOut = std::min(minOut, reactorOf(e, 1)->currentOutputMW); }
    CHECK(std::abs(minOut - 400.0f) < 0.01f, "reactor output dipped to " << minOut);

    // --- Fuel: 12 silver per day end; without it SCRAM until refuelled ---
    int silver = e.getPlayerEconomy(1).silver;
    int day = e.getCurrentDay();
    while (e.getCurrentDay() == day) e.update(0.25f);
    CHECK(silver - e.getPlayerEconomy(1).silver == PowerBalance::NUCLEAR_FUEL_SILVER_PER_DAY, "fuel paid " << silver - e.getPlayerEconomy(1).silver);
    e.getPlayerEconomyMut(1).silver = 5;
    day = e.getCurrentDay();
    while (e.getCurrentDay() == day) e.update(0.25f);
    e.update(0.25f);
    r = reactorOf(e, 1);
    CHECK(r->needsFuel && r->currentOutputMW == 0.0f, "no SCRAM without fuel (needsFuel " << r->needsFuel << ", " << r->currentOutputMW << " MW)");
    CHECK(e.getPlayerEconomy(1).silver == 5, "silver taken although short");
    bool sawNoFuel = false;
    for (const auto& fx : e.drainPowerFx()) sawNoFuel = sawNoFuel || fx.kind == PowerFxKind::REACTOR_NO_FUEL;
    CHECK(sawNoFuel, "no REACTOR_NO_FUEL event");
    e.getPlayerEconomyMut(1).silver = 30;
    e.update(0.25f);
    r = reactorOf(e, 1);
    CHECK(!r->needsFuel && e.getPlayerEconomy(1).silver == 30 - PowerBalance::NUCLEAR_FUEL_SILVER_PER_DAY, "refuel");
    run(e, Balance::SECONDS_PER_DAY * 1.05f);
    CHECK(std::abs(reactorOf(e, 1)->currentOutputMW - 400.0f) < 0.01f, "ramp after refuel " << reactorOf(e, 1)->currentOutputMW);

    // --- Lightning: SCRAM instead of destruction; the plain lightning rule still deletes others ---
    e.getPlayerEconomyMut(1).silver = 5000;
    sf::Vector2f rpos = reactorOf(e, 1)->position;
    size_t countBefore = e.getBuildings().size();
    CHECK(!e.breakBuildingAt(rpos), "breakBuildingAt removed the reactor");
    CHECK(e.getBuildings().size() == countBefore, "reactor vanished");
    r = reactorOf(e, 1);
    CHECK(r->scramTimer > 0.0f, "no SCRAM after lightning");
    e.update(0.25f);
    CHECK(reactorOf(e, 1)->currentOutputMW == 0.0f, "output during SCRAM");
    run(e, PowerBalance::NUCLEAR_SCRAM_COOLDOWN_SEC + 1.0f);
    r = reactorOf(e, 1);
    CHECK(r->scramTimer == 0.0f && r->rampProgress > 0.0f && r->rampProgress < 0.1f, "restart after SCRAM: ramp " << r->rampProgress);
    sf::Vector2f solar = e.getStartPlotSlot(1, 2, 2);
    CHECK(e.placeBuilding(1, BuildingType::SOLAR_PANEL, solar, msg), msg);
    countBefore = e.getBuildings().size();
    CHECK(e.breakBuildingAt(solar) && e.getBuildings().size() == countBefore - 1, "lightning no longer deletes ordinary buildings");

    // --- Demolish from any slot of the plot, no refund ---
    int woodBefore = e.getPlayerEconomy(1).wood;
    CHECK(e.removeBuilding(1, corner, msg), "demolish reactor: " << msg);
    CHECK(reactorOf(e, 1) == nullptr, "reactor still there");
    CHECK(e.getPlayerEconomy(1).wood == woodBefore, "reactor refunded wood");
    CHECK(e.canPlaceBuilding(1, BuildingType::NUCLEAR, centre(*site), reason), "cannot rebuild after demolish: " << reason);

    CHECK(e.getCityState().winner == 0, "match ended during the test");
    std::cout << (g_failures == 0 ? "ALL " : "") << g_checks - g_failures << "/" << g_checks << " NUCLEAR CHECKS PASSED\n";
    return g_failures == 0 ? 0 : 1;
}
