// =============================================================================
// Team b-power: map layout presets and the seeded map generator (F-39)
// Headless: built from this file + Game/scr/*.cpp only ("make test").
// Every layout must be mirror-symmetric, stay clear of the city and the resource
// stations, have a working slot grid and a river bank, and be reproducible from its seed.
// =============================================================================
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
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

bool rectsOverlap(const sf::FloatRect& a, const sf::FloatRect& b) {
    return a.position.x < b.position.x + b.size.x && b.position.x < a.position.x + a.size.x &&
           a.position.y < b.position.y + b.size.y && b.position.y < a.position.y + a.size.y;
}

std::string layoutKey(const GameEngine& e) {
    std::string key;
    for (const auto& p : e.getLandPlots()) {
        key += std::to_string(static_cast<int>(p.bounds.position.x)) + "," + std::to_string(static_cast<int>(p.bounds.position.y)) +
               "," + std::to_string(p.terrain) + "," + std::to_string(p.costGold) + ";";
    }
    return key;
}

void checkLayout(MapPreset preset, unsigned seed) {
    const std::string name = std::string(getMapPresetNameBg(preset)) + " seed " + std::to_string(seed);
    GameEngine e;
    e.setMapPreset(preset, seed);
    e.init(1600.0f, 900.0f);
    const auto& plots = e.getLandPlots();
    const MapLayout& L = e.getMapLayout();

    int p1 = 0, p2 = 0, p1Bought = 0, p2Bought = 0, p1River = 0, p1Vent = 0;
    std::set<int> ids;
    for (const auto& p : plots) {
        ids.insert(p.id);
        if (p.playerOwner == 1) { ++p1; p1Bought += p.isPurchased ? 1 : 0; } else { ++p2; p2Bought += p.isPurchased ? 1 : 0; }
        if (p.playerOwner == 1 && p.terrain == static_cast<int>(TerrainType::RIVER)) ++p1River;
        if (p.playerOwner == 1 && p.terrain == static_cast<int>(TerrainType::VENT)) ++p1Vent;
        // Clear of the city (610..990), the side panels (x < 250 / x > 1350) and the resource stations (y >= 575)
        float x0 = p.bounds.position.x, x1 = x0 + p.bounds.size.x, y0 = p.bounds.position.y, y1 = y0 + p.bounds.size.y;
        if (p.playerOwner == 1) CHECK(x0 >= 250.0f && x1 <= 605.0f, name << ": West plot " << p.id << " x " << x0 << ".." << x1);
        else CHECK(x0 >= 995.0f && x1 <= 1350.0f, name << ": East plot " << p.id << " x " << x0 << ".." << x1);
        CHECK(y0 >= 100.0f && y1 <= 575.0f, name << ": plot " << p.id << " y " << y0 << ".." << y1);
        CHECK(p.bounds.size.x >= 75.0f && p.bounds.size.y >= 75.0f, name << ": plot " << p.id << " too small");
        if (p.isPurchased) CHECK(p.terrain == static_cast<int>(TerrainType::PLAIN), name << ": start plot must be plain");
    }
    CHECK(static_cast<int>(ids.size()) == static_cast<int>(plots.size()), name << ": duplicate plot ids");
    CHECK(p1 == p2 && p1 == L.plotCount(), name << ": plot counts P1 " << p1 << " P2 " << p2);
    CHECK(p1 >= 8, name << ": only " << p1 << " plots");
    CHECK(p1Bought == 1 && p2Bought == 1, name << ": start plots " << p1Bought << "/" << p2Bought);
    CHECK(p1River >= 2, name << ": only " << p1River << " river plots");
    CHECK(p1Vent >= PowerBalance::VENTS_PER_SIDE, name << ": only " << p1Vent << " vents");

    // No overlaps, mirror symmetry (bounds, terrain and price)
    for (size_t i = 0; i < plots.size(); ++i) {
        for (size_t j = i + 1; j < plots.size(); ++j) {
            CHECK(!rectsOverlap(plots[i].bounds, plots[j].bounds), name << ": plots " << plots[i].id << " and " << plots[j].id << " overlap");
        }
        if (plots[i].playerOwner != 1) continue;
        bool mirrored = false;
        for (const auto& q : plots) {
            if (q.playerOwner != 2) continue;
            float mx = 1600.0f - plots[i].bounds.position.x - plots[i].bounds.size.x;
            if (std::abs(q.bounds.position.x - mx) < 0.01f && std::abs(q.bounds.position.y - plots[i].bounds.position.y) < 0.01f) {
                mirrored = (q.terrain == plots[i].terrain && q.costGold == plots[i].costGold && q.isPurchased == plots[i].isPurchased);
            }
        }
        CHECK(mirrored, name << ": plot " << plots[i].id << " has no mirrored twin");
    }

    // Slot grid: every slot maps back to itself and lands in the plot of its cell
    for (int player = 1; player <= 2; ++player) {
        for (int r = 0; r < e.getGridRows(); ++r) {
            for (int c = 0; c < e.getGridCols(); ++c) {
                sf::Vector2f s = e.getGridSlot(player, c, r);
                int bc = -1, br = -1;
                e.getClosestGridIndex(player, s, bc, br);
                CHECK(bc == c && br == r, name << ": slot (" << c << "," << r << ") maps to (" << bc << "," << br << ")");
                bool hasPlot = L.hasPlot(L.westCol(player, c / 3), r / 3);
                const LandPlot* plot = e.findPlotAt(player, s);
                CHECK((plot != nullptr) == hasPlot, name << ": slot (" << c << "," << r << ") plot lookup");
                if (plot) CHECK(plot->screenCol == c / 3 && plot->row == r / 3, name << ": slot in wrong plot");
            }
        }
        const LandPlot* start = e.findPlotAt(player, e.getStartPlotSlot(player, 1, 1));
        CHECK(start != nullptr && start->isPurchased, name << ": start slot of P" << player << " is not on the start plot");
    }

    // Hydro works on a river plot of each player, and nowhere else
    for (int player = 1; player <= 2; ++player) {
        auto& econ = e.getPlayerEconomyMut(player);
        econ.wood = econ.iron = econ.copper = econ.coal = econ.silicon = econ.silver = 1000;
        econ.gold = 100000;
        for (const auto& p : plots) {
            if (p.playerOwner == player && p.terrain == static_cast<int>(TerrainType::RIVER)) {
                std::string msg;
                CHECK(e.buyLandPlot(player, p.id, msg), name << ": buy river plot: " << msg);
                sf::Vector2f c(p.bounds.position.x + p.bounds.size.x * 0.5f, p.bounds.position.y + p.bounds.size.y * 0.5f);
                CHECK(e.isRiverBankSlot(player, c), name << ": river plot not river bank");
                CHECK(e.placeBuilding(player, BuildingType::HYDRO_PLANT, c, msg), name << ": hydro on river plot: " << msg);
                break;
            }
        }
        sf::Vector2f startSlot = e.getStartPlotSlot(player, 0, 0);
        std::string reason;
        CHECK(!e.canPlaceBuilding(player, BuildingType::HYDRO_PLANT, startSlot, reason), name << ": hydro allowed on the start plot");
    }

    // A few simulated days must run without trouble
    for (int i = 0; i < 4 * 90 * 4; ++i) e.update(0.25f);
    CHECK(e.getCurrentDay() >= 4, name << ": day " << e.getCurrentDay());
}

} // namespace

int main() {
    std::cout << "[Map layouts (F-39)]\n";

    // Classic keeps the original geometry, ids, start plots and river column
    {
        GameEngine e;
        e.init(1600.0f, 900.0f);
        const auto& plots = e.getLandPlots();
        CHECK(plots.size() == 24, "classic has " << plots.size() << " plots");
        CHECK(plots[0].bounds.position.x == 258.0f && plots[0].bounds.position.y == 105.0f, "classic first plot moved");
        CHECK(plots[12].bounds.position.x == 1003.0f, "classic first East plot at " << plots[12].bounds.position.x);
        CHECK(plots[0].isPurchased && plots[14].isPurchased, "classic start plots changed");
        CHECK(e.getGridCols() == 9 && e.getGridRows() == 12, "classic grid " << e.getGridCols() << "x" << e.getGridRows());
        for (int r = 0; r < 4; ++r) {
            CHECK(plots[r * 3 + 2].terrain == static_cast<int>(TerrainType::RIVER), "classic P1 river column row " << r);
            CHECK(plots[12 + r * 3].terrain == static_cast<int>(TerrainType::RIVER), "classic P2 river column row " << r);
            for (int c = 0; c < 3; ++c) {
                CHECK(plots[r * 3 + c].costGold == Balance::getLandPlotCost(r, c), "classic price changed at " << r << "," << c);
            }
        }
    }

    for (int p = 0; p < static_cast<int>(MapPreset::COUNT); ++p) {
        for (unsigned seed : { 1u, 7u, 42u, 1234567u, 98765u }) {
            checkLayout(static_cast<MapPreset>(p), seed);
        }
    }

    // The generator is reproducible and actually varies with the seed; the preset survives restarts
    {
        GameEngine a, b;
        a.setMapPreset(MapPreset::GENERATED, 555u);
        b.setMapPreset(MapPreset::GENERATED, 555u);
        a.init(1600.0f, 900.0f);
        b.init(1600.0f, 900.0f);
        CHECK(layoutKey(a) == layoutKey(b), "same seed gave different maps");
        std::set<std::string> distinct;
        for (unsigned s = 1; s <= 12; ++s) {
            GameEngine g;
            g.setMapPreset(MapPreset::GENERATED, s * 101u);
            g.init(1600.0f, 900.0f);
            distinct.insert(layoutKey(g));
        }
        CHECK(distinct.size() >= 8, "only " << distinct.size() << " distinct generated maps out of 12");
        a.restartGame();
        CHECK(a.getMapPreset() == MapPreset::GENERATED && layoutKey(a) == layoutKey(b), "restart lost the map preset");
    }

    std::cout << (g_failures == 0 ? "ALL " : "") << g_checks - g_failures << "/" << g_checks << " MAP LAYOUT CHECKS PASSED\n";
    return g_failures == 0 ? 0 : 1;
}
