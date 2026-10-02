#include <iostream>
#include <iomanip>
#include <cassert>
#include <cmath>
#include <vector>
#include <string>
#include "../Game/includes/game_main.h"

int main() {
    std::cout << "========================================================\n";
    std::cout << " RUNNING 100X SPEED 2-PLAYER GAMEPLAY SIMULATION TEST\n";
    std::cout << "========================================================\n";

    GameEngine engine;
    engine.init(1600.0f, 900.0f);
    engine.setTimeScale(100.0f);

    std::cout << "[PHASE 1] Initial State & Grid Verification...\n";
    assert(engine.getLandPlots().size() == 24); // 12 plots for P1, 12 plots for P2
    std::cout << "  -> Land plots count: " << engine.getLandPlots().size() << " (12 per player).\n";

    const auto& plots = engine.getLandPlots();
    assert(plots[0].isPurchased);
    assert(plots[14].isPurchased); // r=0, c=2 (top-right of east sector)

    // Verify all 8 resources start at 0
    const auto& p1 = engine.getPlayerEconomy(1);
    const auto& p2 = engine.getPlayerEconomy(2);
    assert(p1.money == 0 && p1.iron == 0 && p1.coal == 0 && p1.gold == 0);
    assert(p1.copper == 0 && p1.silver == 0 && p1.silicon == 0 && p1.wood == 0);
    assert(p2.money == 0 && p2.iron == 0 && p2.coal == 0 && p2.gold == 0);
    assert(p2.copper == 0 && p2.silver == 0 && p2.silicon == 0 && p2.wood == 0);
    std::cout << "  -> All 8 distinct resources initialized at exactly 0.\n";

    std::cout << "\n[PHASE 2] Simulating 100x Speed Co-op Gameplay...\n";
    float p1HarvestCooldown = 0.0f;
    float p2HarvestCooldown = 0.0f;

    int p1BuildingsPlaced = 0;
    int p2BuildingsPlaced = 0;
    int p1PlotsBought = 0;
    int p2PlotsBought = 0;
    int p1Upgrades = 0;
    int p2Upgrades = 0;

    float totalSimTime = 0.0f;
    const float dt = 0.02f;
    int stepCount = 0;
    const int maxSteps = 8000;

    auto playerMineStrategy = [&](int player, float& cd, int step) {
        if (cd > 0.0f) return;

        // Balanced strategy: alternate between Gold (for buying lands & upgrading mines) and Construction resources
        ResourceType targetRes;
        if (step % 2 == 0) {
            targetRes = ResourceType::GOLD;
        } else {
            static const ResourceType buildRes[] = {
                ResourceType::WOOD, ResourceType::IRON, ResourceType::COPPER,
                ResourceType::SILICON, ResourceType::COAL, ResourceType::SILVER
            };
            static int idx1 = 0, idx2 = 0;
            int& idx = (player == 1) ? idx1 : idx2;
            targetRes = buildRes[idx++ % 6];
        }

        GameEngine::MineResult res;
        std::string msg;
        if (engine.mineResource(player, targetRes, res, msg)) {
            cd = 1.0f; // 1-second cooldown strictly enforced
        }
    };

    while (stepCount < maxSteps) {
        engine.update(dt);
        float effectiveDt = dt * engine.getTimeScale();
        totalSimTime += effectiveDt;
        stepCount++;

        if (p1HarvestCooldown > 0.0f) p1HarvestCooldown -= dt;
        if (p2HarvestCooldown > 0.0f) p2HarvestCooldown -= dt;

        playerMineStrategy(1, p1HarvestCooldown, stepCount);
        playerMineStrategy(2, p2HarvestCooldown, stepCount);

        // Buy next available land plots when players have accumulated gold
        std::string buyMsg1;
        if (engine.buyNextLandTier(1, buyMsg1)) {
            p1PlotsBought++;
        }
        std::string buyMsg2;
        if (engine.buyNextLandTier(2, buyMsg2)) {
            p2PlotsBought++;
        }

        // Upgrade mines with gold
        static const ResourceType upgradableMines[] = {
            ResourceType::WOOD, ResourceType::IRON, ResourceType::COPPER,
            ResourceType::SILICON, ResourceType::COAL, ResourceType::SILVER, ResourceType::GOLD
        };
        for (ResourceType t : upgradableMines) {
            int cost = engine.getMineUpgradeCost(1, t);
            if (cost > 0 && engine.getPlayerEconomy(1).gold >= cost && p1PlotsBought >= 1) {
                std::string upMsg;
                if (engine.upgradeMine(1, t, upMsg)) {
                    p1Upgrades++;
                    break;
                }
            }
        }
        for (ResourceType t : upgradableMines) {
            int cost = engine.getMineUpgradeCost(2, t);
            if (cost > 0 && engine.getPlayerEconomy(2).gold >= cost && p2PlotsBought >= 1) {
                std::string upMsg;
                if (engine.upgradeMine(2, t, upMsg)) {
                    p2Upgrades++;
                    break;
                }
            }
        }

        // Place buildings across the 9x12 grid (108 slots per player)
        auto tryPlaceNext = [&](int player, int& placedCount) {
            for (const auto& plot : engine.getLandPlots()) {
                if (plot.playerOwner != player || !plot.isPurchased) continue;
                int plotIdx = (plot.id - 1) % 12;
                int plotC = plotIdx % 3;
                int plotR = plotIdx / 3;

                for (int subR = 0; subR < 3; ++subR) {
                    for (int subC = 0; subC < 3; ++subC) {
                        int c = plotC * 3 + subC;
                        int r = plotR * 3 + subR;
                        sf::Vector2f slotPos = engine.getGridSlot(player, c, r);

                        BuildingType choices[] = {
                            BuildingType::SOLAR_PANEL,
                            BuildingType::WIND_TURBINE,
                            BuildingType::BATTERY,
                            BuildingType::LAMP,
                            BuildingType::HYDRO_PLANT
                        };
                        for (BuildingType type : choices) {
                            std::string reason;
                            if (engine.canPlaceBuilding(player, type, slotPos, reason)) {
                                std::string outMsg;
                                if (engine.placeBuilding(player, type, slotPos, outMsg)) {
                                    placedCount++;
                                    return;
                                }
                            }
                        }
                    }
                }
            }
        };

        tryPlaceNext(1, p1BuildingsPlaced);
        tryPlaceNext(2, p2BuildingsPlaced);

        // Every ~15 in-game days, print simulation progress
        static int lastReportedDay = 0;
        int inGameDay = static_cast<int>(totalSimTime / 120.0f) + 1;
        if (inGameDay > lastReportedDay && inGameDay % 15 == 0) {
            lastReportedDay = inGameDay;
            std::cout << "  [Day " << inGameDay << " @ 100x Speed] "
                      << "P1: MW=" << engine.getPlayerEconomy(1).energyMW
                      << ", Money=" << engine.getPlayerEconomy(1).money
                      << ", Gold=" << engine.getPlayerEconomy(1).gold
                      << ", Plots=" << (1 + p1PlotsBought)
                      << ", Buildings=" << p1BuildingsPlaced
                      << " | P2: MW=" << engine.getPlayerEconomy(2).energyMW
                      << ", Money=" << engine.getPlayerEconomy(2).money
                      << ", Gold=" << engine.getPlayerEconomy(2).gold
                      << ", Plots=" << (1 + p2PlotsBought)
                      << ", Buildings=" << p2BuildingsPlaced << "\n";
        }
    }

    std::cout << "\n[PHASE 3] Validating 9-Item-Per-Plot & Multi-Land System...\n";
    // Count buildings on Player 1 starting plot
    int p1Plot1Buildings = 0;
    for (const auto& b : engine.getBuildings()) {
        if (b.playerOwner == 1 && plots[0].bounds.contains(b.position)) {
            p1Plot1Buildings++;
        }
    }
    std::cout << "  -> Buildings placed on Player 1 starting plot: " << p1Plot1Buildings << " / 9 slots!\n";
    assert(p1Plot1Buildings == 9); // Exactly 9 buildings fit on the plot (3x3 grid)!

    // Count buildings on Player 2 starting plot
    int p2Plot1Buildings = 0;
    for (const auto& b : engine.getBuildings()) {
        if (b.playerOwner == 2 && plots[14].bounds.contains(b.position)) {
            p2Plot1Buildings++;
        }
    }
    std::cout << "  -> Buildings placed on Player 2 starting plot: " << p2Plot1Buildings << " / 9 slots!\n";
    assert(p2Plot1Buildings == 9); // Exactly 9 buildings fit on the plot (3x3 grid)!

    // Verify players bought additional lands (up to 12 available per player)
    int p1TotalOwnedPlots = 0;
    int p2TotalOwnedPlots = 0;
    for (const auto& plot : engine.getLandPlots()) {
        if (plot.playerOwner == 1 && plot.isPurchased) p1TotalOwnedPlots++;
        if (plot.playerOwner == 2 && plot.isPurchased) p2TotalOwnedPlots++;
    }
    std::cout << "  -> Player 1 owned land plots: " << p1TotalOwnedPlots << " / 12\n";
    std::cout << "  -> Player 2 owned land plots: " << p2TotalOwnedPlots << " / 12\n";
    assert(p1TotalOwnedPlots >= 2);
    assert(p2TotalOwnedPlots >= 2);

    // Verify buildings placed on newly purchased plots (> 9 buildings across multiple plots)
    std::cout << "  -> Player 1 total buildings placed across lands: " << p1BuildingsPlaced << "\n";
    std::cout << "  -> Player 2 total buildings placed across lands: " << p2BuildingsPlaced << "\n";
    assert(p1BuildingsPlaced > 9);
    assert(p2BuildingsPlaced > 9);

    std::cout << "\n[PHASE 4] Validating Selection Cycling (Forward & Backward)...\n";
    // Test that PgDn cycles forward and PgUp cycles backward
    engine.clearBuildingSelection(2);
    assert(engine.getSelectedBuilding(2) == BuildingType::NONE);

    engine.cycleBuildingSelection(2);
    BuildingType b1 = engine.getSelectedBuilding(2);
    assert(b1 != BuildingType::NONE);

    engine.cycleBuildingSelection(2);
    BuildingType b2 = engine.getSelectedBuilding(2);
    assert(b2 != b1);

    // Backward cycling test
    engine.cycleBuildingSelectionPrev(2);
    BuildingType bBack = engine.getSelectedBuilding(2);
    assert(bBack == b1); // Step back matches previous building!
    std::cout << "  -> PASS: PgDn and PgUp cycle forward and backward deterministically without random skips.\n";

    std::cout << "\n[PHASE 5] Validating Percentage-Based Rewards & Total Stability...\n";
    const auto& finalP1 = engine.getPlayerEconomy(1);
    const auto& finalP2 = engine.getPlayerEconomy(2);
    std::cout << "  Final P1 Money: " << finalP1.money << " | P2 Money: " << finalP2.money << "\n";
    std::cout << "  Final P1 Gold:  " << finalP1.gold  << " | P2 Gold:  " << finalP2.gold << "\n";
    std::cout << "  Final P1 Power: " << finalP1.energyMW << " MW | P2 Power: " << finalP2.energyMW << " MW\n";

    std::cout << "  -> Player 1 mine upgrades purchased with gold: " << p1Upgrades << "\n";
    std::cout << "  -> Player 2 mine upgrades purchased with gold: " << p2Upgrades << "\n";
    assert(p1Upgrades > 0);
    assert(p2Upgrades > 0);

    assert(finalP1.money > 0);
    assert(finalP2.money > 0);
    assert(finalP1.gold >= 0);
    assert(finalP2.gold >= 0);
    assert(stepCount >= 8000);

    std::cout << "\n[PHASE 6] Validating Gradual Victory Condition & Screen Trigger at 100%...\n";
    // Starting from baseline, verify that dominance moves influence gradually and triggers winner at 100%
    GameEngine victoryEngine;
    victoryEngine.init(1600.0f, 900.0f);
    assert(victoryEngine.getCityState().winner == 0);
    assert(victoryEngine.getPlayerEconomy(1).cityInfluence == 0.50f);

    // Give P1 dominant energy production
    victoryEngine.getPlayerEconomyMut(1).wood = 500;
    victoryEngine.getPlayerEconomyMut(1).iron = 500;
    victoryEngine.getPlayerEconomyMut(1).copper = 500;
    victoryEngine.getPlayerEconomyMut(1).coal = 500;
    victoryEngine.getPlayerEconomyMut(1).silicon = 500;
    for (int i = 0; i < 6; i++) {
        std::string m;
        sf::Vector2f pos = victoryEngine.getGridSlot(1, i % 3, i / 3);
        bool ok = victoryEngine.placeBuilding(1, BuildingType::WIND_TURBINE, pos, m);
        if (!ok) {
            std::cerr << "Placement " << i << " failed: " << m << "\n";
        }
        assert(ok);
    }
    // Over 1-second update, energy outputs are computed and influence increases gradually, NOT jumping instantly to 1.0!
    victoryEngine.update(1.0f);
    assert(victoryEngine.getPlayerEconomy(1).energyMW > 0);
    assert(victoryEngine.getPlayerEconomy(2).energyMW == 0);

    float infDay1 = victoryEngine.getPlayerEconomy(1).cityInfluence;
    assert(infDay1 > 0.50f && infDay1 < 0.90f); // Gradual progression confirmed!
    assert(victoryEngine.getCityState().winner == 0);

    // Run until 100% is reached
    for (int t = 0; t < 150; t++) {
        victoryEngine.update(1.0f);
        if (victoryEngine.getCityState().winner != 0) break;
    }
    assert(victoryEngine.getCityState().winner == 1);
    assert(victoryEngine.getPlayerEconomy(1).cityInfluence >= 0.999f);
    assert(victoryEngine.getPlayerEconomy(2).cityInfluence <= 0.001f);
    std::cout << "  -> PASS: 100% City influence triggers Player 1 victory screen and locks winner state.\n";

    std::cout << "========================================================\n";
    std::cout << " 100X SPEED SIMULATION COMPLETED SUCCESSFULLY! ALL PASS!\n";
    std::cout << "========================================================\n";
    return 0;
}
