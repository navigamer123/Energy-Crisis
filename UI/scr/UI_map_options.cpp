// =============================================================================
// UI_map glue for match options & progression [team b-options]
// F-03 rules on the HUD, F-35/F-33/F-24 real costs on the building menus.
// =============================================================================
#include "../includes/UI_map.h"
#include <iostream>

void UI_map::setMatchRules(const MatchRules& rules) {
    engine.setMatchRules(rules); // applied by the next restartMatch() -> engine.init()
    std::cout << "[UI_map] Match rules: preset " << MatchInfo::presetName(rules.preset)
              << ", mutators 0x" << std::hex << rules.mutators << std::dec
              << (rules.sandbox ? ", sandbox" : "") << "\n";
}

void UI_map::syncMatchOptionsUI(float dt) {
    (void)dt;
    p1Clock.setRules(engine.getFinalDay(), engine.getGraceDays());
    p2Clock.setRules(engine.getFinalDay(), engine.getGraceDays());
    city.setRuleInfo(engine.getGraceDays(), engine.getVictoryShare());
    p1Buildings.syncCosts(engine);
    p2Buildings.syncCosts(engine);
}

// -----------------------------------------------------------------------------
// UI_buildings: costs after perks (charter, research, Building Boom)
// -----------------------------------------------------------------------------
void UI_buildings::syncCosts(const GameEngine& engine) {
    for (auto& b : buildings) {
        if (b.type == BuildingType::DEMOLISH || b.type == BuildingType::NONE) continue;
        BuildingCost c = engine.getBuildingCostFor(playerIndex, b.type);
        b.woodCost = c.woodCost;
        b.ironCost = c.ironCost;
        b.copperCost = c.copperCost;
        b.coalCost = c.coalCost;
        b.siliconCost = c.siliconCost;
        b.silverCost = c.silverCost;
        b.oreCost = c.oreCost;
    }
}
