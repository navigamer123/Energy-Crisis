// =============================================================================
// Team b-power: mega-projects — fusion, space solar, pumped-hydro dam (HX-10)
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

sf::Vector2f centre(const LandPlot& p) {
    return sf::Vector2f(p.bounds.position.x + p.bounds.size.x * 0.5f, p.bounds.position.y + p.bounds.size.y * 0.5f);
}

const LandPlot* buyPlot(GameEngine& e, int player, TerrainType t, bool matchTerrain) {
    for (const auto& p : e.getLandPlots()) {
        if (p.playerOwner != player || p.isPurchased) continue;
        bool isT = (p.terrain == static_cast<int>(t));
        if (isT != matchTerrain) continue;
        std::string msg;
        if (e.buyLandPlot(player, p.id, msg)) return &p;
    }
    return nullptr;
}

// Both players keep up with the demand so nobody wins and freezes the engine
void windFarms(GameEngine& e) {
    for (int p = 1; p <= 2; ++p) {
        for (int i = 0; i < 9; ++i) {
            std::string m;
            e.placeBuilding(p, BuildingType::WIND_TURBINE, e.getStartPlotSlot(p, i % 3, i / 3), m);
        }
    }
}

void runToDay(GameEngine& e, int day, std::vector<PowerFx>* fxOut = nullptr) {
    while (e.getCurrentDay() < day && e.getCityState().winner == 0) {
        e.update(0.5f);
        std::vector<PowerFx> fx = e.drainPowerFx();
        if (fxOut) fxOut->insert(fxOut->end(), fx.begin(), fx.end());
    }
}

void run(GameEngine& e, float seconds, std::vector<PowerFx>* fxOut = nullptr) {
    int steps = static_cast<int>(seconds / 0.25f + 0.5f);
    for (int i = 0; i < steps; ++i) {
        e.update(0.25f);
        std::vector<PowerFx> fx = e.drainPowerFx();
        if (fxOut) fxOut->insert(fxOut->end(), fx.begin(), fx.end());
    }
}

bool sawFx(const std::vector<PowerFx>& fx, PowerFxKind k) {
    for (const auto& f : fx) if (f.kind == k) return true;
    return false;
}

} // namespace

int main() {
    std::cout << "[Mega-projects (HX-10)]\n";
    std::string msg, reason;

    // --- Fusion (P1) and space solar (P2) ---
    {
        GameEngine e;
        e.init(1600.0f, 900.0f);
        rich(e, 1);
        rich(e, 2);
        windFarms(e);
        const LandPlot* f = buyPlot(e, 1, TerrainType::RIVER, false);
        const LandPlot* s = buyPlot(e, 2, TerrainType::RIVER, false);
        CHECK(!e.canPlaceBuilding(1, BuildingType::MEGA_FUSION, centre(*f), reason) && reason.find("ДЕН 8") != std::string::npos,
              "mega-project before day 8: " << reason);
        std::vector<PowerFx> fx;
        runToDay(e, PowerBalance::MEGA_UNLOCK_DAY, &fx);
        CHECK(e.getCurrentDay() == PowerBalance::MEGA_UNLOCK_DAY && e.getCityState().winner == 0, "could not reach day 8");
        CHECK(sawFx(fx, PowerFxKind::MEGA_UNLOCKED), "no MEGA_UNLOCKED event");
        rich(e, 1);
        rich(e, 2);
        e.getPlayerEconomyMut(1).silicon = 100; // short of silicon: refused
        CHECK(!e.canPlaceBuilding(1, BuildingType::MEGA_FUSION, centre(*f), reason), "fusion without enough silicon");
        rich(e, 1);
        fx.clear();
        CHECK(e.placeBuilding(1, BuildingType::MEGA_FUSION, centre(*f), msg), "fusion: " << msg);
        CHECK(e.placeBuilding(2, BuildingType::MEGA_SPACE_SOLAR, centre(*s), msg), "space solar: " << msg);
        std::vector<PowerFx> started = e.drainPowerFx();
        int startedEvents = 0;
        for (const auto& x : started) if (x.kind == PowerFxKind::MEGA_STARTED && x.player == 0) ++startedEvents;
        CHECK(startedEvents == 2, "MEGA_STARTED events for both players: " << startedEvents);
        const PlacedBuilding* fusion = e.getMegaProject(1);
        const PlacedBuilding* space = e.getMegaProject(2);
        CHECK(fusion && space, "mega-projects missing");
        CHECK(std::abs(fusion->constructionTotal - 3.0f * Balance::SECONDS_PER_DAY) < 0.01f, "fusion build time");
        CHECK(std::abs(space->constructionTotal - 2.0f * Balance::SECONDS_PER_DAY) < 0.01f, "space solar build time");
        CHECK(!e.canPlaceBuilding(1, BuildingType::MEGA_PUMPED_HYDRO, centre(*f), reason) && reason.find("ВЕЧЕ") != std::string::npos,
              "second mega-project: " << reason);
        CHECK(!e.canPlaceBuilding(1, BuildingType::SOLAR_PANEL, e.getGridSlot(1, f->screenCol * 3, f->row * 3), reason) &&
              reason.find("ЗАБРАНЕНА ЗОНА") != std::string::npos, "building next to the construction site: " << reason);

        // Nothing while building, progress grows
        run(e, Balance::SECONDS_PER_DAY);
        fusion = e.getMegaProject(1);
        CHECK(fusion->currentOutputMW == 0.0f, "fusion produces while under construction");
        float p1 = e.getConstructionProgress(*fusion);
        CHECK(std::abs(p1 - 1.0f / 3.0f) < 0.01f, "fusion progress after a day " << p1);

        // Lightning on the site: setback, not destruction
        float leftBefore = fusion->constructionLeft;
        CHECK(e.absorbLightningAt(fusion->position), "lightning on the site not absorbed");
        fusion = e.getMegaProject(1);
        CHECK(fusion && std::abs(fusion->constructionLeft - leftBefore - 0.15f * fusion->constructionTotal) < 0.01f, "setback");
        CHECK(!e.breakBuildingAt(fusion->position) && e.getMegaProject(1) != nullptr, "breakBuildingAt deleted the site");

        fx.clear();
        run(e, Balance::SECONDS_PER_DAY * 1.2f, &fx);
        space = e.getMegaProject(2);
        CHECK(space->constructionLeft == 0.0f, "space solar not finished after 2 days");
        CHECK(sawFx(fx, PowerFxKind::MEGA_COMPLETE), "no MEGA_COMPLETE event");
        e.update(0.25f);
        WeatherType w2 = e.getPlayerWeather(2);
        float expect = 350.0f * ((w2 == WeatherType::STORMY) ? PowerBalance::SPACE_SOLAR_STORM_MULT : 1.0f);
        CHECK(std::abs(e.getMegaProject(2)->currentOutputMW - expect) < 0.01f, "space solar output " << e.getMegaProject(2)->currentOutputMW);

        run(e, Balance::SECONDS_PER_DAY * 2.5f);
        fusion = e.getMegaProject(1);
        CHECK(fusion->constructionLeft == 0.0f && std::abs(fusion->currentOutputMW - 500.0f) < 0.01f, "fusion output " << fusion->currentOutputMW);
        // Finished: lightning rods
        CHECK(e.absorbLightningAt(fusion->position) && e.getMegaProject(1)->constructionLeft == 0.0f, "finished fusion hurt by lightning");
        CHECK(std::abs(e.getMegaProject(1)->currentOutputMW - 500.0f) < 0.01f, "fusion output after lightning");

        // Demolish: no refund
        int iron = e.getPlayerEconomy(1).iron;
        CHECK(e.removeBuilding(1, centre(*f), msg), msg);
        CHECK(e.getPlayerEconomy(1).iron == iron && e.getMegaProject(1) == nullptr, "fusion refund / still there");
        CHECK(e.getCityState().winner == 0, "match ended during the test");
    }

    // --- Pumped-hydro dam: river only, stores surplus and covers deficits ---
    {
        GameEngine e;
        e.init(1600.0f, 900.0f);
        rich(e, 1);
        rich(e, 2);
        windFarms(e);
        const LandPlot* dry = buyPlot(e, 1, TerrainType::RIVER, false);
        const LandPlot* river = buyPlot(e, 1, TerrainType::RIVER, true);
        runToDay(e, PowerBalance::MEGA_UNLOCK_DAY);
        rich(e, 1);
        CHECK(!e.canPlaceBuilding(1, BuildingType::MEGA_PUMPED_HYDRO, centre(*dry), reason) && reason.find("РЕЧЕН БРЯГ") != std::string::npos,
              "dam away from the river: " << reason);
        CHECK(e.placeBuilding(1, BuildingType::MEGA_PUMPED_HYDRO, centre(*river), msg), "dam: " << msg);
        int hydroBefore = e.countHydroPlants();
        run(e, 2.0f * Balance::SECONDS_PER_DAY + 2.0f);
        const PlacedBuilding* dam = e.getMegaProject(1);
        CHECK(dam->constructionLeft == 0.0f, "dam not finished");
        CHECK(e.countHydroPlants() == hydroBefore + 2, "finished dam does not count as river user");
        CHECK(std::abs(dam->maxCapacity - 3000.0f) < 0.01f, "dam capacity " << dam->maxCapacity);
        // Nine turbines produce far more than the demand: the dam fills up (faster than any battery)
        float stored0 = dam->energyStored;
        run(e, 10.0f);
        float stored1 = e.getMegaProject(1)->energyStored;
        CHECK(stored1 > stored0, "dam does not charge from surplus (" << stored0 << " -> " << stored1 << ")");
        // Remove the turbines: the dam alone must carry the city demand
        for (int i = 0; i < 9; ++i) e.removeBuilding(1, e.getStartPlotSlot(1, i % 3, i / 3), msg);
        e.getPlayerEconomyMut(1).silver = 0;
        run(e, 1.0f);
        dam = e.getMegaProject(1);
        int demand = e.getCityState().cityEnergyDemand;
        CHECK(e.getPlayerEconomy(1).energyMW >= demand - 1 || dam->energyStored < 1.0f, "dam did not cover the demand: " << e.getPlayerEconomy(1).energyMW << " of " << demand);
        CHECK(dam->currentOutputMW <= PowerBalance::PUMPED_HYDRO_MAX_POWER_MW + 0.01f, "dam power above its limit");
    }

    std::cout << (g_failures == 0 ? "ALL " : "") << g_checks - g_failures << "/" << g_checks << " MEGA-PROJECT CHECKS PASSED\n";
    return g_failures == 0 ? 0 : 1;
}
