// =============================================================================
// Team b-power: hazard system — hail, floods, wildfires, quakes (F-34)
// Headless: built from this file + Game/scr/*.cpp only ("make test").
// =============================================================================
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
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

const LandPlot* buyTerrain(GameEngine& e, int player, TerrainType t) {
    for (const auto& p : e.getLandPlots()) {
        if (p.playerOwner == player && p.terrain == static_cast<int>(t)) {
            std::string msg;
            if (!p.isPurchased) e.buyLandPlot(player, p.id, msg);
            return &p;
        }
    }
    return nullptr;
}

const LandPlot* startPlot(const GameEngine& e, int player) {
    for (const auto& p : e.getLandPlots()) {
        if (p.playerOwner == player && p.isPurchased && p.terrain == static_cast<int>(TerrainType::PLAIN)) return &p;
    }
    return nullptr;
}

sf::Vector2f slotIn(const GameEngine& e, const LandPlot& p, int sub) {
    return e.getGridSlot(p.playerOwner, p.screenCol * 3 + sub % 3, p.row * 3 + sub / 3);
}

void fill(GameEngine& e, const LandPlot& p, BuildingType t) {
    for (int s = 0; s < 9; ++s) {
        std::string msg;
        e.placeBuilding(p.playerOwner, t, slotIn(e, p, s), msg);
    }
}

int brokenIn(const GameEngine& e, int player, const LandPlot* p) {
    int n = 0;
    for (const auto& b : e.getBuildings()) {
        if (b.playerOwner == player && b.isBroken && (p == nullptr || p->bounds.contains(b.position))) ++n;
    }
    return n;
}

const PowerFx* lastHit(const std::vector<PowerFx>& fx, HazardKind k, int player) {
    const PowerFx* found = nullptr;
    for (const auto& f : fx) {
        if (f.kind == PowerFxKind::HAZARD_HIT && f.hazard == k && (player == 0 || f.player == player)) found = &f;
    }
    return found;
}

} // namespace

int main() {
    std::cout << "[Hazards (F-34)]\n";

    // --- Hail: only solar panels / turbines break, at most HAZARD_MAX_HITS, broken = 0 MW, repairable ---
    {
        GameEngine e;
        e.init(1600.0f, 900.0f);
        rich(e, 1);
        const LandPlot* home = startPlot(e, 1);
        const LandPlot* meadow = buyTerrain(e, 1, TerrainType::MEADOW);
        fill(e, *home, BuildingType::SOLAR_PANEL);
        fill(e, *meadow, BuildingType::BATTERY);
        e.drainPowerFx();
        e.triggerHazardNow(HazardKind::HAIL, 1);
        std::vector<PowerFx> fx = e.drainPowerFx();
        const PowerFx* hit = lastHit(fx, HazardKind::HAIL, 1);
        int broken = brokenIn(e, 1, nullptr);
        CHECK(hit != nullptr, "no HAZARD_HIT event for hail");
        CHECK(broken >= 1 && broken <= PowerBalance::HAZARD_MAX_HITS, "hail broke " << broken);
        CHECK(hit && static_cast<int>(hit->hits.size()) == broken, "event lists " << (hit ? hit->hits.size() : 0) << " hits");
        CHECK(brokenIn(e, 1, meadow) == 0, "hail broke a battery");
        CHECK(e.hasBrokenBuilding(1) && !e.hasBrokenBuilding(2) && e.countBrokenBuildings(1) == broken, "broken counters");
        e.update(0.25f);
        for (const auto& b : e.getBuildings()) {
            if (b.isBroken) CHECK(b.currentOutputMW == 0.0f && b.damageKind == static_cast<int>(HazardKind::HAIL), "broken panel output / kind");
        }
        // Repair: 5 wood + 5 iron, back to work
        sf::Vector2f pos;
        for (const auto& b : e.getBuildings()) if (b.isBroken) { pos = b.position; break; }
        int wood = e.getPlayerEconomy(1).wood, iron = e.getPlayerEconomy(1).iron;
        std::string msg;
        CHECK(e.repairBuilding(1, pos, msg), "repair: " << msg);
        CHECK(wood - e.getPlayerEconomy(1).wood == Balance::REPAIR_WOOD_COST && iron - e.getPlayerEconomy(1).iron == Balance::REPAIR_IRON_COST, "repair cost");
        CHECK(e.countBrokenBuildings(1) == broken - 1, "repair did not fix one building");
    }

    // --- Flood: only buildings on river plots ---
    {
        GameEngine e;
        e.init(1600.0f, 900.0f);
        rich(e, 1);
        const LandPlot* river = buyTerrain(e, 1, TerrainType::RIVER);
        const LandPlot* home = startPlot(e, 1);
        fill(e, *river, BuildingType::WIND_TURBINE);
        fill(e, *home, BuildingType::WIND_TURBINE);
        e.triggerHazardNow(HazardKind::FLOOD, 1);
        CHECK(brokenIn(e, 1, river) >= 1, "flood broke nothing on the river bank");
        CHECK(brokenIn(e, 1, home) == 0, "flood reached the start plot");
    }

    // --- Wildfire: one plot (and at most one neighbour) ---
    {
        int spreadSeen = 0, contained = 0;
        for (int round = 0; round < 12; ++round) {
            GameEngine e;
            e.init(1600.0f, 900.0f);
            rich(e, 1);
            const LandPlot* home = startPlot(e, 1);
            fill(e, *home, BuildingType::SOLAR_PANEL);
            const LandPlot* far = nullptr;
            for (const auto& p : e.getLandPlots()) {
                if (p.playerOwner == 1 && !p.isPurchased && std::abs(p.screenCol - home->screenCol) + std::abs(p.row - home->row) >= 3) {
                    std::string msg;
                    e.buyLandPlot(1, p.id, msg);
                    far = &p;
                    break;
                }
            }
            fill(e, *far, BuildingType::SOLAR_PANEL);
            e.triggerHazardNow(HazardKind::WILDFIRE, 1);
            int a = brokenIn(e, 1, home), b = brokenIn(e, 1, far);
            CHECK(a + b >= 1, "wildfire broke nothing");
            CHECK(a == 0 || b == 0, "wildfire jumped between distant plots (" << a << ", " << b << ")");
            contained += (a == 0 || b == 0) ? 1 : 0;
            spreadSeen += 0;
        }
        CHECK(contained == 12, "wildfire not contained");
        (void)spreadSeen;
    }

    // --- Quake: mirrored epicentre hits both players, every reactor SCRAMs ---
    {
        GameEngine e;
        e.init(1600.0f, 900.0f);
        rich(e, 1);
        rich(e, 2);
        const LandPlot* h1 = startPlot(e, 1);
        const LandPlot* h2 = startPlot(e, 2);
        fill(e, *h1, BuildingType::WIND_TURBINE);
        fill(e, *h2, BuildingType::WIND_TURBINE);
        // A reactor for P2 on a far plot (6 plots owned)
        int bought = 1;
        const LandPlot* site = nullptr;
        for (const auto& p : e.getLandPlots()) {
            if (p.playerOwner == 2 && !p.isPurchased && bought < 6) {
                std::string msg;
                e.buyLandPlot(2, p.id, msg);
                site = &p;
                ++bought;
            }
        }
        std::string msg;
        CHECK(e.placeBuilding(2, BuildingType::NUCLEAR, slotIn(e, *site, 4), msg), "reactor: " << msg);
        e.drainPowerFx();
        const MapLayout& L = e.getMapLayout();
        e.triggerHazardNow(HazardKind::QUAKE, 0, L.startCol, L.startRow);
        std::vector<PowerFx> fx = e.drainPowerFx();
        CHECK(lastHit(fx, HazardKind::QUAKE, 1) && lastHit(fx, HazardKind::QUAKE, 2), "quake events for both players");
        bool scram = false;
        for (const auto& f : fx) scram = scram || f.kind == PowerFxKind::REACTOR_SCRAM;
        CHECK(scram && e.getReactor(2)->scramTimer > 0.0f, "reactor did not SCRAM on the quake");
        CHECK(brokenIn(e, 1, h1) + brokenIn(e, 2, h2) >= 1, "quake at the start plots broke nothing");
        CHECK(e.getReactor(2) != nullptr && !e.getReactor(2)->isBroken, "reactor broken/destroyed by the quake");
    }

    // --- Broken lamp gives no light, broken battery does not discharge ---
    {
        GameEngine e;
        e.init(1600.0f, 900.0f);
        rich(e, 1);
        const LandPlot* home = startPlot(e, 1);
        std::string msg;
        sf::Vector2f lamp = slotIn(e, *home, 0);
        CHECK(e.placeBuilding(1, BuildingType::LAMP, lamp, msg), msg);
        for (int i = 0; i < 9 && !e.hasBrokenBuilding(1); ++i) e.triggerHazardNow(HazardKind::WILDFIRE, 1);
        e.update(0.25f);
        for (const auto& b : e.getBuildings()) {
            if (b.type == BuildingType::LAMP && b.isBroken) CHECK(b.lightRadius == 0.0f && b.currentOutputMW == 0.0f, "broken lamp still lit");
        }
    }

    // --- Scheduling over whole matches: nothing in the grace period, warnings before hits ---
    {
        int hazardHits = 0, warnings = 0, graceEvents = 0, matches = 0;
        std::map<int, int> perKind;
        for (int m = 0; m < 8; ++m) {
            GameEngine e;
            e.setHazardsEnabled(true);
            e.init(1600.0f, 900.0f);
            rich(e, 1);
            rich(e, 2);
            ++matches;
            for (int p = 1; p <= 2; ++p) {
                const LandPlot* home = startPlot(e, p);
                fill(e, *home, BuildingType::WIND_TURBINE); // keeps both above demand: no winner
                const LandPlot* river = buyTerrain(e, p, TerrainType::RIVER);
                fill(e, *river, BuildingType::SOLAR_PANEL);
            }
            std::map<int, bool> warnedToday;
            int lastDay = e.getCurrentDay();
            while (e.getCurrentDay() < 20 && e.getCityState().winner == 0) {
                e.update(0.5f);
                if (e.getCurrentDay() != lastDay) { warnedToday.clear(); lastDay = e.getCurrentDay(); }
                for (const auto& f : e.drainPowerFx()) {
                    if (f.kind != PowerFxKind::HAZARD_HIT && f.kind != PowerFxKind::HAZARD_WARNING) continue;
                    if (e.getCurrentDay() <= Balance::GRACE_PERIOD_DAYS) ++graceEvents;
                    if (f.kind == PowerFxKind::HAZARD_WARNING) { ++warnings; warnedToday[f.player * 10 + static_cast<int>(f.hazard)] = true; }
                    if (f.kind == PowerFxKind::HAZARD_HIT) {
                        ++hazardHits;
                        ++perKind[static_cast<int>(f.hazard)];
                        if (f.hazard != HazardKind::QUAKE) {
                            CHECK(warnedToday[f.player * 10 + static_cast<int>(f.hazard)], "hazard " << getHazardNameBg(f.hazard) << " without warning on day " << e.getCurrentDay());
                        }
                    }
                }
                // Repair everything at the day end like an attentive player
                for (int p = 1; p <= 2; ++p) {
                    while (e.hasBrokenBuilding(p)) {
                        sf::Vector2f pos;
                        for (const auto& b : e.getBuildings()) if (b.isBroken && b.playerOwner == p) { pos = b.position; break; }
                        std::string msg;
                        if (!e.repairBuilding(p, pos, msg)) break;
                    }
                }
            }
        }
        std::cout << "  " << matches << " matches: " << hazardHits << " hazard strikes (hail " << perKind[1] << ", flood " << perKind[2]
                  << ", wildfire " << perKind[3] << ", quake " << perKind[4] << "), " << warnings << " warnings\n";
        CHECK(graceEvents == 0, graceEvents << " hazard events during the grace period");
        CHECK(hazardHits >= 4, "only " << hazardHits << " hazards in " << matches << " matches");
        CHECK(hazardHits <= matches * 30, hazardHits << " hazards is too many");
    }

    // --- Off by default (deterministic engine tests); the switch survives restarts ---
    {
        GameEngine e;
        e.init(1600.0f, 900.0f);
        CHECK(!e.areHazardsEnabled(), "hazards on by default");
        rich(e, 1);
        rich(e, 2);
        for (int p = 1; p <= 2; ++p) fill(e, *startPlot(e, p), BuildingType::WIND_TURBINE);
        int events = 0;
        while (e.getCurrentDay() < 20 && e.getCityState().winner == 0) {
            e.update(0.5f);
            for (const auto& f : e.drainPowerFx()) {
                if (f.kind == PowerFxKind::HAZARD_HIT || f.kind == PowerFxKind::HAZARD_WARNING) ++events;
            }
        }
        CHECK(events == 0 && !e.hasBrokenBuilding(1) && !e.hasBrokenBuilding(2), events << " hazard events with hazards off");
        e.setHazardsEnabled(true);
        e.restartGame();
        CHECK(e.areHazardsEnabled(), "restart lost the hazard switch");
    }

    std::cout << (g_failures == 0 ? "ALL " : "") << g_checks - g_failures << "/" << g_checks << " HAZARD CHECKS PASSED\n";
    return g_failures == 0 ? 0 : 1;
}
