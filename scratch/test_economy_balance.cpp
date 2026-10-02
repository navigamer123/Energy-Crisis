#include "../Game/includes/game_main.h"
#include <iostream>
#include <cassert>
#include <cmath>

void testAll8ResourcesInitializedToZero() {
    std::cout << "[TEST 1] Testing all 8 resources start at 0...\n";
    GameEngine engine;
    engine.init(1600.0f, 900.0f);

    const auto& p1 = engine.getPlayerEconomy(1);
    const auto& p2 = engine.getPlayerEconomy(2);

    assert(p1.money == 0);
    assert(p1.iron == 0);
    assert(p1.coal == 0);
    assert(p1.gold == 0);
    assert(p1.copper == 0);
    assert(p1.silver == 0);
    assert(p1.silicon == 0);
    assert(p1.wood == 0);

    assert(p2.money == 0);
    assert(p2.iron == 0);
    assert(p2.coal == 0);
    assert(p2.gold == 0);
    assert(p2.copper == 0);
    assert(p2.silver == 0);
    assert(p2.silicon == 0);
    assert(p2.wood == 0);

    std::cout << "  -> PASS: All 8 resources start at exactly 0 for both players.\n";
}

void testEachResourceMinedSeparately() {
    std::cout << "[TEST 2] Testing each resource mined separately...\n";
    GameEngine engine;
    engine.init(1600.0f, 900.0f);

    std::string msg;
    GameEngine::MineResult res;

    // 1. Mine Wood
    bool okWood = engine.mineResource(1, ResourceType::WOOD, res, msg);
    assert(okWood && res.wood == 12);
    assert(engine.getPlayerEconomy(1).wood == 12);
    assert(engine.getPlayerEconomy(1).iron == 0); // Other resources untouched

    // 2. Mine Iron
    bool okIron = engine.mineResource(1, ResourceType::IRON, res, msg);
    assert(okIron && res.iron == 8);
    assert(engine.getPlayerEconomy(1).iron == 8);
    assert(engine.getPlayerEconomy(1).copper == 0);

    // 3. Mine Copper
    bool okCopper = engine.mineResource(1, ResourceType::COPPER, res, msg);
    assert(okCopper && res.copper == 6);
    assert(engine.getPlayerEconomy(1).copper == 6);
    assert(engine.getPlayerEconomy(1).coal == 0);

    // 4. Mine Coal
    bool okCoal = engine.mineResource(1, ResourceType::COAL, res, msg);
    assert(okCoal && res.coal == 6);
    assert(engine.getPlayerEconomy(1).coal == 6);
    assert(engine.getPlayerEconomy(1).silicon == 0);

    // 5. Mine Silicon
    bool okSilicon = engine.mineResource(1, ResourceType::SILICON, res, msg);
    assert(okSilicon && res.silicon == 6);
    assert(engine.getPlayerEconomy(1).silicon == 6);
    assert(engine.getPlayerEconomy(1).silver == 0);

    // 6. Mine Silver
    bool okSilver = engine.mineResource(1, ResourceType::SILVER, res, msg);
    assert(okSilver && res.silver == 4);
    assert(engine.getPlayerEconomy(1).silver == 4);
    assert(engine.getPlayerEconomy(1).gold == 0);

    // 7. Mine Gold
    bool okGold = engine.mineResource(1, ResourceType::GOLD, res, msg);
    assert(okGold && res.gold == 3);
    assert(engine.getPlayerEconomy(1).gold == 3);

    // 8. Mine Money (City contract subsidy)
    bool okMoney = engine.mineResource(1, ResourceType::MONEY, res, msg);
    assert(okMoney && res.money >= 12);
    assert(engine.getPlayerEconomy(1).money >= 12);

    std::cout << "  -> PASS: All 8 resources gathered separately with dedicated yields.\n";
}

void testPrecisionGridMovement() {
    std::cout << "[TEST 3] Testing precision grid coordinates...\n";
    GameEngine engine;
    engine.init(1600.0f, 900.0f);

    // Check 6x6 grid for P1
    for (int r = 0; r < 6; ++r) {
        for (int c = 0; c < 6; ++c) {
            sf::Vector2f slot = engine.getGridSlot(1, c, r);
            // Must be strictly in West sector (X < 610)
            assert(slot.x >= 258.0f && slot.x < 610.0f);
            assert(slot.y >= 120.0f && slot.y < 460.0f);

            // Re-query closest index must return exactly (c, r)
            int qc = -1, qr = -1;
            engine.getClosestGridIndex(1, slot, qc, qr);
            assert(qc == c && qr == r);
        }
    }

    // Check 6x6 grid for P2
    for (int r = 0; r < 6; ++r) {
        for (int c = 0; c < 6; ++c) {
            sf::Vector2f slot = engine.getGridSlot(2, c, r);
            // Must be strictly in East sector (X > 990)
            assert(slot.x > 990.0f && slot.x <= 1360.0f);
            assert(slot.y >= 120.0f && slot.y < 460.0f);

            int qc = -1, qr = -1;
            engine.getClosestGridIndex(2, slot, qc, qr);
            assert(qc == c && qr == r);
        }
    }

    std::cout << "  -> PASS: Precision 6x6 grid coordinates perfectly aligned and invertible.\n";
}

void testLastPlacedBuildingMemory() {
    std::cout << "[TEST 4] Testing last placed building memory...\n";
    GameEngine engine;
    engine.init(1600.0f, 900.0f);

    // Give P1 enough resources to place buildings
    auto& p1 = engine.getPlayerEconomyMut(1);
    p1.wood = 100;
    p1.iron = 100;
    p1.copper = 100;
    p1.coal = 100;
    p1.silicon = 100;
    p1.silver = 100;

    // Initially, lastPlacedBuilding defaults to 1 (Solar Panel)
    assert(p1.lastPlacedBuilding == 1);

    // Cycle to BuildingType::WIND_TURBINE (2)
    p1.selectedBuilding = 2;
    sf::Vector2f slot = engine.getGridSlot(1, 0, 0);
    std::string outMsg;
    bool placed = engine.placeBuilding(1, BuildingType::WIND_TURBINE, slot, outMsg);
    assert(placed);

    // Last placed building must now remember WIND_TURBINE (2)
    assert(p1.lastPlacedBuilding == static_cast<int>(BuildingType::WIND_TURBINE));

    // Player stops placing
    engine.clearBuildingSelection(1);
    assert(p1.selectedBuilding == 0);

    // When player starts placing again via cycleBuildingSelection, it MUST select the last placed building (2)!
    engine.cycleBuildingSelection(1);
    assert(p1.selectedBuilding == static_cast<int>(BuildingType::WIND_TURBINE));

    std::cout << "  -> PASS: Lastly placed item is remembered and selected on resume.\n";
}

void testPercentageBasedEnergyRewards() {
    std::cout << "[TEST 5] Testing percentage-based energy rewards & city influence...\n";
    GameEngine engine;
    engine.init(1600.0f, 900.0f);

    auto& p1 = engine.getPlayerEconomyMut(1);
    auto& p2 = engine.getPlayerEconomyMut(2);

    p1.wood = 200; p1.iron = 200; p1.copper = 200; p1.silicon = 200; p1.coal = 200; p1.silver = 200;
    p2.wood = 200; p2.iron = 200; p2.copper = 200; p2.silicon = 200; p2.coal = 200; p2.silver = 200;

    // P1 places 2 Wind Turbines (85 + 85 = 170 MW base)
    std::string msg;
    engine.placeBuilding(1, BuildingType::WIND_TURBINE, engine.getGridSlot(1, 0, 0), msg);
    engine.placeBuilding(1, BuildingType::WIND_TURBINE, engine.getGridSlot(1, 0, 1), msg);

    // P2 places 1 Solar Panel (60 MW base)
    engine.placeBuilding(2, BuildingType::SOLAR_PANEL, engine.getGridSlot(2, 5, 0), msg);

    // Run 2 seconds of simulation
    engine.update(1.0f);
    engine.update(1.0f);

    const auto& p1After = engine.getPlayerEconomy(1);
    const auto& p2After = engine.getPlayerEconomy(2);

    std::cout << "  P1 MW: " << p1After.energyMW << " | P2 MW: " << p2After.energyMW << "\n";
    std::cout << "  P1 Share: " << (p1After.cityInfluence * 100.0f) << "% | P2 Share: " << (p2After.cityInfluence * 100.0f) << "%\n";
    std::cout << "  P1 Money: " << p1After.money << " | P2 Money: " << p2After.money << "\n";

    // P1 generates more energy -> P1 must have greater share and higher money payout!
    assert(p1After.energyMW > p2After.energyMW);
    assert(p1After.cityInfluence > p2After.cityInfluence);
    assert(p1After.money > p2After.money);

    float totalEnergy = static_cast<float>(p1After.energyMW + p2After.energyMW);
    float expectedP1Share = static_cast<float>(p1After.energyMW) / totalEnergy;
    assert(std::abs(p1After.cityInfluence - expectedP1Share) < 0.05f);

    std::cout << "  -> PASS: Energy payout and city influence are strictly percentage-based.\n";
}

void testDemolitionRefundBalance() {
    std::cout << "[TEST 6] Testing recipe costs and demolition refund balance...\n";
    GameEngine engine;
    engine.init(1600.0f, 900.0f);

    auto& p1 = engine.getPlayerEconomyMut(1);
    p1.wood = 50;
    p1.iron = 50;
    p1.copper = 50;
    p1.silicon = 50;

    BuildingCost cost = engine.getBuildingCost(BuildingType::SOLAR_PANEL);
    assert(cost.woodCost == 6 && cost.ironCost == 4 && cost.copperCost == 6 && cost.siliconCost == 8);

    sf::Vector2f slot = engine.getGridSlot(1, 0, 0);
    std::string msg;
    bool placed = engine.placeBuilding(1, BuildingType::SOLAR_PANEL, slot, msg);
    assert(placed);

    assert(p1.wood == 50 - 6);
    assert(p1.iron == 50 - 4);
    assert(p1.copper == 50 - 6);
    assert(p1.silicon == 50 - 8);

    // Demolish and check 50% refund
    bool removed = engine.removeBuilding(1, slot, msg);
    assert(removed);

    assert(p1.wood == (50 - 6) + 3);
    assert(p1.iron == (50 - 4) + 2);
    assert(p1.copper == (50 - 6) + 3);
    assert(p1.silicon == (50 - 8) + 4);

    std::cout << "  -> PASS: Deductions and 50% demolition refunds are balanced and accurate.\n";
}

int main() {
    std::cout << "========================================================\n";
    std::cout << " RUNNING ENERGY CRISIS ECONOMY & BALANCE TEST SUITE\n";
    std::cout << "========================================================\n";

    testAll8ResourcesInitializedToZero();
    testEachResourceMinedSeparately();
    testPrecisionGridMovement();
    testLastPlacedBuildingMemory();
    testPercentageBasedEnergyRewards();
    testDemolitionRefundBalance();

    std::cout << "========================================================\n";
    std::cout << " ALL 6 ECONOMY & BALANCE TESTS PASSED SUCCESSFULLY! 100%\n";
    std::cout << "========================================================\n";
    return 0;
}
