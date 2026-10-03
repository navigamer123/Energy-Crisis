#ifdef NDEBUG
#undef NDEBUG // the checks below are assert()s: never compile them away
#endif
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

    // 8. Money mine was removed ("махни мината за пари")
    bool okMoney = engine.mineResource(1, ResourceType::MONEY, res, msg);
    assert(!okMoney); // Money cannot be mined from a station; earned from clean power delivery!

    std::cout << "  -> PASS: All 7 mining resources gathered separately with dedicated yields, money mine removed.\n";
}

void testPrecisionGridMovement() {
    std::cout << "[TEST 3] Testing precision 9x12 grid coordinates (12 plots x 9 slots = 108 slots per player)...\n";
    GameEngine engine;
    engine.init(1600.0f, 900.0f);

    assert(engine.getLandPlots().size() == 24); // 12 plots for P1, 12 plots for P2

    // Check 9x12 grid for P1
    for (int r = 0; r < 12; ++r) {
        for (int c = 0; c < 9; ++c) {
            sf::Vector2f slot = engine.getGridSlot(1, c, r);
            // Must be strictly in West sector (X < 610)
            assert(slot.x >= 258.0f && slot.x < 610.0f);
            assert(slot.y >= 105.0f && slot.y < 530.0f);

            // Re-query closest index must return exactly (c, r)
            int qc = -1, qr = -1;
            engine.getClosestGridIndex(1, slot, qc, qr);
            assert(qc == c && qr == r);
        }
    }

    // Check 9x12 grid for P2
    for (int r = 0; r < 12; ++r) {
        for (int c = 0; c < 9; ++c) {
            sf::Vector2f slot = engine.getGridSlot(2, c, r);
            // Must be strictly in East sector (X > 990)
            assert(slot.x > 990.0f && slot.x <= 1360.0f);
            assert(slot.y >= 105.0f && slot.y < 530.0f);

            int qc = -1, qr = -1;
            engine.getClosestGridIndex(2, slot, qc, qr);
            assert(qc == c && qr == r);
        }
    }

    std::cout << "  -> PASS: Precision 9x12 grid coordinates perfectly aligned and invertible (108 slots/player).\n";
}

void testLastPlacedBuildingMemory() {
    std::cout << "[TEST 4] Testing last placed building memory & bidirectional cycling...\n";
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

    // Test backward cycling with cycleBuildingSelectionPrev
    engine.cycleBuildingSelectionPrev(1);
    assert(p1.selectedBuilding == 1); // 2 -> 1
    engine.cycleBuildingSelectionPrev(1);
    assert(p1.selectedBuilding == 6); // 1 -> 6 (wrap to Demolish)
    engine.cycleBuildingSelection(1);
    assert(p1.selectedBuilding == 1); // 6 -> 1 (forward wrap to Solar)

    std::cout << "  -> PASS: Lastly placed item remembered, bidirectional cycling works flawlessly.\n";
}

void testPercentageBasedEnergyRewardsAndGradualProgression() {
    std::cout << "[TEST 5] Testing percentage-based energy rewards & gradual city influence...\n";
    GameEngine engine;
    engine.init(1600.0f, 900.0f);

    auto& p1 = engine.getPlayerEconomyMut(1);
    auto& p2 = engine.getPlayerEconomyMut(2);

    p1.wood = 200; p1.iron = 200; p1.copper = 200; p1.silicon = 200; p1.coal = 200; p1.silver = 200;
    p2.wood = 200; p2.iron = 200; p2.copper = 200; p2.silicon = 200; p2.coal = 200; p2.silver = 200;

    // P1 places 2 Wind Turbines (85 + 85 = 170 MW base) on its starting plot (columns 0-2)
    std::string msg;
    bool placed = engine.placeBuilding(1, BuildingType::WIND_TURBINE, engine.getGridSlot(1, 0, 0), msg);
    assert(placed);
    placed = engine.placeBuilding(1, BuildingType::WIND_TURBINE, engine.getGridSlot(1, 0, 1), msg);
    assert(placed);

    // P2 places 1 Solar Panel (60 MW base) on ITS starting plot (columns 6-8 of the east grid)
    placed = engine.placeBuilding(2, BuildingType::SOLAR_PANEL, engine.getGridSlot(2, 8, 0), msg);
    if (!placed) std::cerr << "  P2 placement failed: " << msg << "\n";
    assert(placed);

    // Run 2 seconds of simulation (day 1, morning)
    engine.update(1.0f);
    engine.update(1.0f);

    const auto& p1After = engine.getPlayerEconomy(1);
    const auto& p2After = engine.getPlayerEconomy(2);

    std::cout << "  P1 MW: " << p1After.energyMW << " | P2 MW: " << p2After.energyMW << "\n";
    std::cout << "  P1 Share: " << (p1After.cityInfluence * 100.0f) << "% | P2 Share: " << (p2After.cityInfluence * 100.0f) << "%\n";
    std::cout << "  P1 Money: " << p1After.money << " | P2 Money: " << p2After.money << "\n";

    // P1 generates more energy -> P1 gets the larger, percentage-based money payout
    assert(p2After.energyMW > 0); // P2's panel really works (it stands on purchased land)
    assert(p1After.energyMW > p2After.energyMW);
    assert(p1After.money > p2After.money);

    // City territory moves only at a day end, and never during the grace period
    assert(std::abs(p1After.cityInfluence - 0.50f) < 1e-6f);
    assert(std::abs(p2After.cityInfluence - 0.50f) < 1e-6f);

    // Play until day 4 has been settled. [b-economy] BAL-02: every judged day moves the share in
    // proportion to how much better each player served the city. P1's turbines always serve their
    // whole quota and deliver far more energy than P2's single solar panel (which is dark at night),
    // so P1 gains on day 3 and on day 4, never more than VERDICT_MAX_SHIFT per day.
    float shareAtDayStart = engine.getCityState().p1CityShare;
    while (engine.getCurrentDay() <= 4) {
        const int day = engine.getCurrentDay();
        engine.update(0.25f);
        if (engine.getCurrentDay() == day) continue;

        const float share = engine.getCityState().p1CityShare;
        const float delta = share - shareAtDayStart;
        const float eps = 1e-4f;
        std::cout << "  Day " << day << " settled: P1 share " << (share * 100.0f) << "% (" << (delta >= 0.0f ? "+" : "")
                  << (delta * 100.0f) << "%)\n";
        if (day <= Balance::GRACE_PERIOD_DAYS) {
            assert(std::abs(delta) < 1e-6f);
        } else {
            assert(delta > 0.0f && delta <= Econ::VERDICT_MAX_SHIFT + eps);
        }
        shareAtDayStart = share;
    }

    const float finalShare = engine.getPlayerEconomy(1).cityInfluence;
    assert(finalShare > 0.50f);
    assert(finalShare <= 0.50f + 2.0f * Econ::VERDICT_MAX_SHIFT + 1e-4f); // Gradual: at most 12% per day
    assert(engine.getPlayerEconomy(1).cityInfluence > engine.getPlayerEconomy(2).cityInfluence);
    assert(engine.getCityState().winner == 0);

    std::cout << "  -> PASS: Energy payout is percentage-based and city influence moves gradually, once per day.\n";
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

void testMineUpgradesWithGold() {
    std::cout << "[TEST 7] Testing resource mine upgrades with Gold...\n";
    GameEngine engine;
    engine.init(1600.0f, 900.0f);

    // Upgrade prices come from the central balance table (game_balance.h)
    const int costTo2 = Balance::getMineUpgradeCost(1, false);
    const int costTo3 = Balance::getMineUpgradeCost(2, false);
    const int costTo4 = Balance::getMineUpgradeCost(3, false);
    assert(costTo2 > 0 && costTo3 > costTo2 && costTo4 > costTo3);

    // Initial state: Level 1
    assert(engine.getMineLevel(1, ResourceType::IRON) == 1);
    assert(engine.getMineUpgradeCost(1, ResourceType::IRON) == costTo2);
    assert(engine.getMineUpgradeCost(1, ResourceType::GOLD) == Balance::getMineUpgradeCost(1, true));

    // Give Player 1 enough gold for two upgrades, but 1 Gold short of the third
    const int startGold = costTo2 + costTo3 + costTo4 - 1;
    engine.getPlayerEconomyMut(1).gold = startGold;

    std::string msg;
    bool upgraded = engine.upgradeMine(1, ResourceType::IRON, msg);
    assert(upgraded);
    assert(engine.getMineLevel(1, ResourceType::IRON) == 2);
    assert(engine.getPlayerEconomy(1).gold == startGold - costTo2);

    // Upgrade to Level 3
    assert(engine.getMineUpgradeCost(1, ResourceType::IRON) == costTo3);
    upgraded = engine.upgradeMine(1, ResourceType::IRON, msg);
    assert(upgraded);
    assert(engine.getMineLevel(1, ResourceType::IRON) == 3);
    assert(engine.getPlayerEconomy(1).gold == costTo4 - 1);

    // Cannot upgrade to Level 4 without enough gold (1 Gold short)
    assert(engine.getMineUpgradeCost(1, ResourceType::IRON) == costTo4);
    upgraded = engine.upgradeMine(1, ResourceType::IRON, msg);
    assert(!upgraded);
    assert(engine.getMineLevel(1, ResourceType::IRON) == 3);
    assert(engine.getPlayerEconomy(1).gold == costTo4 - 1);

    // Verify upgraded yield (Level 3 Iron gives base 8 * 2.5 = 20 Iron!)
    GameEngine::MineResult res;
    engine.mineResource(1, ResourceType::IRON, res, msg);
    assert(res.iron == 20);
    assert(engine.getPlayerEconomy(1).iron == 20);

    std::cout << "  -> PASS: Mine upgrade with gold works, level increases, and yields scale by +75%/lvl.\n";
}

int main() {
    std::cout << "========================================================\n";
    std::cout << " RUNNING ENERGY CRISIS ECONOMY & BALANCE TEST SUITE\n";
    std::cout << "========================================================\n";

    testAll8ResourcesInitializedToZero();
    testEachResourceMinedSeparately();
    testPrecisionGridMovement();
    testLastPlacedBuildingMemory();
    testPercentageBasedEnergyRewardsAndGradualProgression();
    testDemolitionRefundBalance();
    testMineUpgradesWithGold();

    std::cout << "========================================================\n";
    std::cout << " ALL 7 ECONOMY & BALANCE TESTS PASSED SUCCESSFULLY! 100%\n";
    std::cout << "========================================================\n";
    return 0;
}
