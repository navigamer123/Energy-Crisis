// =============================================================================
// Team b-power: terrain plots, shared river flow and geothermal vents (F-15)
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

const LandPlot* buyTerrain(GameEngine& e, int player, TerrainType t, int skip = 0) {
    for (const auto& p : e.getLandPlots()) {
        if (p.playerOwner == player && p.terrain == static_cast<int>(t)) {
            if (skip-- > 0) continue;
            if (!p.isPurchased) {
                std::string msg;
                e.buyLandPlot(player, p.id, msg);
            }
            return &p;
        }
    }
    return nullptr;
}

sf::Vector2f slotIn(const GameEngine& e, const LandPlot& p, int sub) {
    int col = p.screenCol * 3 + sub % 3;
    int row = p.row * 3 + sub / 3;
    return e.getGridSlot(p.playerOwner, col, row);
}

const PlacedBuilding* at(const GameEngine& e, sf::Vector2f pos) {
    for (const auto& b : e.getBuildings()) {
        if (std::abs(b.position.x - pos.x) < 1.0f && std::abs(b.position.y - pos.y) < 1.0f) return &b;
    }
    return nullptr;
}

bool place(GameEngine& e, int player, BuildingType t, sf::Vector2f pos, std::string& msg) {
    return e.placeBuilding(player, t, pos, msg);
}

// Runs to the given hour of the current or next day in small steps
void runToHour(GameEngine& e, float hour) {
    for (int i = 0; i < 4000; ++i) {
        if (std::abs(e.getHour24() - hour) < 0.05f) return;
        e.update(0.05f);
    }
}

} // namespace

int main() {
    std::cout << "[Terrain, river flow and geothermal (F-15)]\n";
    const float eps = 0.01f;

    // --- Shared river: every hydro plant (either player) lowers the flow for everyone ---
    {
        GameEngine e;
        e.init(1600.0f, 900.0f);
        rich(e, 1);
        rich(e, 2);
        CHECK(std::abs(e.getRiverFlowFactor() - 1.15f) < 1e-4f, "empty river factor " << e.getRiverFlowFactor());
        const LandPlot* r1 = buyTerrain(e, 1, TerrainType::RIVER);
        const LandPlot* r2 = buyTerrain(e, 2, TerrainType::RIVER);
        CHECK(r1 && r2, "no river plots");
        std::string msg;
        sf::Vector2f h1 = slotIn(e, *r1, 0);
        CHECK(place(e, 1, BuildingType::HYDRO_PLANT, h1, msg), msg);
        e.update(0.1f);
        float f1 = e.getRiverFlowFactor();
        CHECK(std::abs(f1 - (1.15f - 0.035f)) < 1e-4f, "factor with one plant " << f1);
        float hydroMult = WeatherSystem::getHydroMultiplier(e.getPlayerWeather(1));
        float out1 = at(e, h1)->currentOutputMW;
        CHECK(std::abs(out1 - 110.0f * hydroMult * f1) < eps, "P1 hydro " << out1 << " expected " << 110.0f * hydroMult * f1);
        // P2 builds three hydro plants: P1's plant produces less
        for (int s = 0; s < 3; ++s) CHECK(place(e, 2, BuildingType::HYDRO_PLANT, slotIn(e, *r2, s), msg), msg);
        e.update(0.1f);
        float f4 = e.getRiverFlowFactor();
        CHECK(std::abs(f4 - (1.15f - 4 * 0.035f)) < 1e-4f, "factor with four plants " << f4);
        float out1b = at(e, h1)->currentOutputMW;
        hydroMult = WeatherSystem::getHydroMultiplier(e.getPlayerWeather(1));
        CHECK(std::abs(out1b - 110.0f * hydroMult * f4) < eps, "P1 hydro after P2 built: " << out1b);
        // Clamped at 0.55 however many plants
        for (int s = 1; s < 9; ++s) place(e, 1, BuildingType::HYDRO_PLANT, slotIn(e, *r1, s), msg);
        const LandPlot* r1b = buyTerrain(e, 1, TerrainType::RIVER, 1);
        const LandPlot* r2b = buyTerrain(e, 2, TerrainType::RIVER, 1);
        for (int s = 0; s < 9; ++s) { place(e, 1, BuildingType::HYDRO_PLANT, slotIn(e, *r1b, s), msg); place(e, 2, BuildingType::HYDRO_PLANT, slotIn(e, *r2b, s), msg); }
        CHECK(e.countHydroPlants() >= 20, "only " << e.countHydroPlants() << " hydro plants");
        CHECK(std::abs(e.getRiverFlowFactor() - 0.55f) < 1e-4f, "clamped factor " << e.getRiverFlowFactor());
    }

    // --- Hill: wind x1.3, meadow: solar x1.15 (same player, same weather, same hour) ---
    {
        GameEngine e;
        e.init(1600.0f, 900.0f);
        rich(e, 1);
        const LandPlot* hill = buyTerrain(e, 1, TerrainType::HILL);
        const LandPlot* meadow = buyTerrain(e, 1, TerrainType::MEADOW);
        CHECK(hill && meadow, "classic map lacks hill/meadow");
        std::string msg;
        sf::Vector2f plainWind = e.getStartPlotSlot(1, 0, 0);
        sf::Vector2f plainSolar = e.getStartPlotSlot(1, 1, 0);
        sf::Vector2f hillWind = slotIn(e, *hill, 4);
        sf::Vector2f meadowSolar = slotIn(e, *meadow, 4);
        CHECK(place(e, 1, BuildingType::WIND_TURBINE, plainWind, msg), msg);
        CHECK(place(e, 1, BuildingType::SOLAR_PANEL, plainSolar, msg), msg);
        CHECK(place(e, 1, BuildingType::WIND_TURBINE, hillWind, msg), msg);
        CHECK(place(e, 1, BuildingType::SOLAR_PANEL, meadowSolar, msg), msg);
        CHECK(at(e, hillWind)->terrain == static_cast<int>(TerrainType::HILL), "hill terrain not stored");
        runToHour(e, 12.5f);
        float w0 = at(e, plainWind)->currentOutputMW, w1 = at(e, hillWind)->currentOutputMW;
        float s0 = at(e, plainSolar)->currentOutputMW, s1 = at(e, meadowSolar)->currentOutputMW;
        CHECK(w0 > 1.0f && std::abs(w1 / w0 - 1.30f) < 0.001f, "hill wind ratio " << w1 << "/" << w0);
        if (s0 > 1.0f) CHECK(std::abs(s1 / s0 - 1.15f) < 0.001f, "meadow solar ratio " << s1 << "/" << s0);
        else std::cout << "  (solar is 0 at noon in this weather, meadow ratio skipped)\n";
    }

    // --- Geothermal: vents only, two wells per vent, steady 70 MW day and night ---
    {
        GameEngine e;
        e.init(1600.0f, 900.0f);
        rich(e, 1);
        std::string msg, reason;
        CHECK(!e.canPlaceBuilding(1, BuildingType::GEOTHERMAL, e.getStartPlotSlot(1, 1, 1), reason), "geothermal on plain start plot");
        CHECK(reason.find("ГЕЙЗЕР") != std::string::npos, "reason: " << reason);
        const LandPlot* vent = buyTerrain(e, 1, TerrainType::VENT);
        CHECK(vent != nullptr, "classic map has no vent");
        sf::Vector2f g1 = slotIn(e, *vent, 0), g2 = slotIn(e, *vent, 2), g3 = slotIn(e, *vent, 4);
        CHECK(place(e, 1, BuildingType::GEOTHERMAL, g1, msg), msg);
        CHECK(place(e, 1, BuildingType::GEOTHERMAL, g2, msg), msg);
        CHECK(!e.canPlaceBuilding(1, BuildingType::GEOTHERMAL, g3, reason), "third well allowed");
        CHECK(reason.find("СОНДАЖА") != std::string::npos, "reason: " << reason);
        // Ordinary buildings may share the vent plot
        CHECK(place(e, 1, BuildingType::SOLAR_PANEL, g3, msg), "solar on vent plot: " << msg);
        float minOut = 1e9f, maxOut = 0.0f;
        for (int i = 0; i < 90 * 20; ++i) {
            e.update(0.05f);
            float o = at(e, g1)->currentOutputMW;
            minOut = std::min(minOut, o);
            maxOut = std::max(maxOut, o);
        }
        CHECK(std::abs(minOut - 70.0f) < eps && std::abs(maxOut - 70.0f) < eps, "geothermal not steady: " << minOut << ".." << maxOut);
        // Demolish refunds half like any ordinary building
        int woodBefore = e.getPlayerEconomy(1).wood;
        CHECK(e.removeBuilding(1, g2, msg), msg);
        CHECK(e.getPlayerEconomy(1).wood - woodBefore == PowerBalance::GEOTHERMAL.woodCost / 2, "geothermal refund");
    }

    // --- Selection cycle reaches the advanced page and Demolish, and wraps ---
    {
        GameEngine e;
        e.init(1600.0f, 900.0f);
        std::vector<int> seen;
        for (int i = 0; i < 12; ++i) {
            e.cycleBuildingSelection(1);
            seen.push_back(static_cast<int>(e.getSelectedBuilding(1)));
        }
        const int expected[12] = { 1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 6, 1 };
        for (int i = 0; i < 12; ++i) CHECK(seen[static_cast<size_t>(i)] == expected[i], "cycle step " << i << " = " << seen[static_cast<size_t>(i)]);
        e.cycleBuildingSelectionPrev(1); // 1 -> 6
        CHECK(static_cast<int>(e.getSelectedBuilding(1)) == 6, "prev from solar");
        e.cycleBuildingSelectionPrev(1); // 6 -> 11
        CHECK(static_cast<int>(e.getSelectedBuilding(1)) == 11, "prev from demolish");
        CHECK(e.getBuildingCost(BuildingType::GEOTHERMAL).basePowerMW == 70, "geothermal cost lookup");
        CHECK(e.getBuildingCost(BuildingType::NUCLEAR).silverCost == PowerBalance::NUCLEAR.silverCost, "nuclear cost lookup");
    }

    std::cout << (g_failures == 0 ? "ALL " : "") << g_checks - g_failures << "/" << g_checks << " TERRAIN CHECKS PASSED\n";
    return g_failures == 0 ? 0 : 1;
}
