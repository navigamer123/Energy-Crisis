// =============================================================================
// Team b-power (HX-10): mega-projects — fusion reactor, space solar array and
// pumped-hydro dam (ПАВЕЦ). One per player from day 8, a whole plot, very high
// cost, multi-day construction visible to both players (PowerFx events and
// GameEngine::getMegaProject / getConstructionProgress for the shared HUD).
// Output and storage rules: game_power_buildings.cpp; placement: same file.
// =============================================================================
#include "../includes/game_main.h"
#include <algorithm>
#include <cmath>

void GameEngine::updateMegaProjects(float dt) {
    for (auto& b : buildings) {
        if (!isMegaProject(b.type) || b.constructionLeft <= 0.0f) continue;
        float before = getConstructionProgress(b);
        b.constructionLeft = std::max(0.0f, b.constructionLeft - dt);
        float after = getConstructionProgress(b);

        PowerFx fx;
        fx.player = 0; // both players follow every mega-project
        fx.buildingType = static_cast<int>(b.type);
        fx.owner = b.playerOwner;
        fx.pos = b.position;
        if (b.constructionLeft <= 0.0f) {
            fx.kind = PowerFxKind::MEGA_COMPLETE;
            fx.radius = 80.0f;
            fx.title = "МЕГАПРОЕКТЪТ Е ГОТОВ!";
            std::string effect;
            if (b.type == BuildingType::MEGA_FUSION) {
                effect = "+" + std::to_string(PowerBalance::MEGA_FUSION.basePowerMW) + " MW чиста базова мощност";
            } else if (b.type == BuildingType::MEGA_SPACE_SOLAR) {
                effect = "+" + std::to_string(PowerBalance::MEGA_SPACE_SOLAR.basePowerMW) + " MW денем и нощем от орбита";
            } else {
                effect = std::to_string(PowerBalance::MEGA_PUMPED_HYDRO.batteryCapacityMWh) + " MWh съхранение, до " +
                         std::to_string(static_cast<int>(PowerBalance::PUMPED_HYDRO_MAX_POWER_MW)) + " MW";
            }
            fx.detail = "Играч " + std::to_string(b.playerOwner) + ": " + getBuildingCost(b.type).nameBg + "\n" + effect + ".";
            pushPowerFx(fx);
        } else if (before < 0.5f && after >= 0.5f) {
            fx.kind = PowerFxKind::MEGA_STARTED;
            fx.title = "МЕГАПРОЕКТ: 50%";
            fx.detail = "Играч " + std::to_string(b.playerOwner) + ": " + getBuildingCost(b.type).nameBg + " е наполовина готов.";
            pushPowerFx(fx);
        }
    }
}
