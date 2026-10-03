#ifdef NDEBUG
#undef NDEBUG // the checks below are assert()s: never compile them away
#endif
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include "../Game/includes/game_main.h"

namespace {

bool isSlotFree(const GameEngine& engine, sf::Vector2f pos) {
    for (const auto& b : engine.getBuildings()) {
        float dx = b.position.x - pos.x;
        float dy = b.position.y - pos.y;
        if (dx * dx + dy * dy < 16.0f * 16.0f) return false;
    }
    return true;
}

bool isGenerator(BuildingType t) {
    return t == BuildingType::SOLAR_PANEL || t == BuildingType::WIND_TURBINE || t == BuildingType::HYDRO_PLANT;
}

} // namespace

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

    std::cout << "\n[PHASE 2] Simulating a whole 100x speed match (until the day-" << Balance::FINAL_DAY << " result)...\n";
    float p1HarvestCooldown = 0.0f;
    float p2HarvestCooldown = 0.0f;

    int p1BuildingsPlaced = 0;
    int p2BuildingsPlaced = 0;
    int p1PlotsBought = 0;
    int p2PlotsBought = 0;
    int p1Upgrades = 0;
    int p2Upgrades = 0;

    float totalSimTime = 0.0f;
    const float dt = 0.02f;                                       // one real frame
    const float frameGameSeconds = dt * engine.getTimeScale();   // = 2 game-seconds at 100x
    const int maxSteps = static_cast<int>((Balance::FINAL_DAY + 1) * Balance::SECONDS_PER_DAY / frameGameSeconds) + 100;
    int stepCount = 0;
    int dayEndsSeen = 0;
    float largestDailyShift = 0.0f;

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
            cd = Balance::MINE_COOLDOWN_SEC; // 1-second cooldown strictly enforced
        }
    };

    // Build order with real recipes (no ore wildcard): fill the oldest (cheapest) owned plot first,
    // every 5th building is a battery, river-bank slots get hydro plants, other slots alternate
    // solar and wind. If the next building is not affordable (or it is dark) the player saves up.
    // No lamps: they would only consume power.
    auto tryPlaceNext = [&](int player, int& placedCount) {
        std::vector<const LandPlot*> owned;
        for (const auto& plot : engine.getLandPlots()) {
            if (plot.playerOwner == player && plot.isPurchased) owned.push_back(&plot);
        }
        std::sort(owned.begin(), owned.end(), [](const LandPlot* a, const LandPlot* b) {
            return (a->costGold != b->costGold) ? (a->costGold < b->costGold) : (a->id < b->id);
        });

        for (const LandPlot* plot : owned) {
            int plotIdx = (plot->id - 1) % 12;
            int plotC = plotIdx % 3;
            int plotR = plotIdx / 3;

            for (int subR = 0; subR < 3; ++subR) {
                for (int subC = 0; subC < 3; ++subC) {
                    sf::Vector2f slotPos = engine.getGridSlot(player, plotC * 3 + subC, plotR * 3 + subR);
                    if (!isSlotFree(engine, slotPos)) continue;

                    BuildingType type;
                    if (placedCount % 5 == 4) {
                        type = BuildingType::BATTERY;
                    } else if (engine.isRiverBankSlot(player, slotPos)) {
                        type = BuildingType::HYDRO_PLANT;
                    } else {
                        type = (placedCount % 2 == 0) ? BuildingType::SOLAR_PANEL : BuildingType::WIND_TURBINE;
                    }

                    std::string reason;
                    if (!engine.canPlaceBuilding(player, type, slotPos, reason)) return; // save up / wait for daylight
                    std::string outMsg;
                    bool placed = engine.placeBuilding(player, type, slotPos, outMsg);
                    if (!placed) std::cerr << "  P" << player << " placement failed: " << outMsg << "\n";
                    assert(placed);
                    placedCount++;
                    return;
                }
            }
        }
    };

    while (engine.getCityState().winner == 0 && stepCount < maxSteps) {
        const int dayBefore = engine.getCurrentDay();
        const float shareBefore = engine.getCityState().p1CityShare;

        engine.update(dt);
        totalSimTime += frameGameSeconds;
        stepCount++;

        // Exactly one settlement per day, and territory moves at most MAX_DAILY_CITY_SHIFT per day
        if (engine.getCurrentDay() != dayBefore) {
            assert(engine.getCurrentDay() == dayBefore + 1);
            dayEndsSeen++;
            float shift = std::abs(engine.getCityState().p1CityShare - shareBefore);
            largestDailyShift = std::max(largestDailyShift, shift);
            if (dayBefore <= Balance::GRACE_PERIOD_DAYS) {
                assert(shift == 0.0f);
            }
            assert(shift <= Balance::MAX_DAILY_CITY_SHIFT + 1e-4f);

            if (dayBefore % 5 == 0) {
                std::cout << "  [Day " << dayBefore << " ended @ 100x Speed] "
                          << "P1: MW=" << engine.getPlayerEconomy(1).energyMW
                          << ", Money=" << engine.getPlayerEconomy(1).money
                          << ", Gold=" << engine.getPlayerEconomy(1).gold
                          << ", Plots=" << (1 + p1PlotsBought)
                          << ", Buildings=" << p1BuildingsPlaced
                          << " | P2: MW=" << engine.getPlayerEconomy(2).energyMW
                          << ", Money=" << engine.getPlayerEconomy(2).money
                          << ", Gold=" << engine.getPlayerEconomy(2).gold
                          << ", Plots=" << (1 + p2PlotsBought)
                          << ", Buildings=" << p2BuildingsPlaced
                          << " | P1 share " << std::fixed << std::setprecision(2)
                          << (engine.getCityState().p1CityShare * 100.0f) << "%\n";
                std::cout.unsetf(std::ios::fixed);
            }
        }
        if (engine.getCityState().winner != 0) break;

        // The 1 s mining cooldown runs on GAME time here: at 100x a whole 20-day match lasts only
        // 18 real seconds, so a real-time cooldown would allow just ~18 harvests per player.
        if (p1HarvestCooldown > 0.0f) p1HarvestCooldown -= frameGameSeconds;
        if (p2HarvestCooldown > 0.0f) p2HarvestCooldown -= frameGameSeconds;

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

        tryPlaceNext(1, p1BuildingsPlaced);
        tryPlaceNext(2, p2BuildingsPlaced);

        // No resource may ever go negative (placement pays exactly the recipe)
        for (int p = 1; p <= 2; ++p) {
            const auto& e = engine.getPlayerEconomy(p);
            assert(e.wood >= 0 && e.iron >= 0 && e.copper >= 0 && e.coal >= 0);
            assert(e.silicon >= 0 && e.silver >= 0 && e.gold >= 0 && e.money >= 0);
        }
    }

    // The match always ends: by a share of 85% or when the final day ends
    const auto& city = engine.getCityState();
    const float finalShare = city.p1CityShare;
    std::cout << "  -> Match result after " << dayEndsSeen << " day ends (" << totalSimTime << " game-seconds): winner code "
              << city.winner << ", P1 share " << (finalShare * 100.0f) << "%, largest daily shift "
              << (largestDailyShift * 100.0f) << "%\n";
    std::cout << "     " << city.lastCutMessage << "\n";
    assert(city.winner != 0);
    assert(stepCount < maxSteps);
    assert(dayEndsSeen == engine.getCurrentDay() - 1);
    assert(engine.getCurrentDay() <= Balance::FINAL_DAY + 1);
    const bool finalDayEnded = (engine.getCurrentDay() == Balance::FINAL_DAY + 1);
    if (city.winner == 3) {
        assert(finalDayEnded && std::abs(finalShare - 0.5f) < Balance::DRAW_SHARE_TOLERANCE);
    } else if (city.winner == 1) {
        assert(finalShare >= Balance::VICTORY_SHARE - 1e-4f || (finalDayEnded && finalShare > 0.5f));
    } else {
        assert(city.winner == 2);
        assert(1.0f - finalShare >= Balance::VICTORY_SHARE - 1e-4f || (finalDayEnded && finalShare < 0.5f));
    }

    // After the result the simulation is frozen
    const float frozenHour = engine.getHour24();
    engine.update(dt);
    assert(engine.getHour24() == frozenHour);

    std::cout << "\n[PHASE 3] Validating 9-Item-Per-Plot & Multi-Land System...\n";
    // Count buildings on Player 1 starting plot
    int p1Plot1Buildings = 0;
    int p1Plot1Generators = 0;
    for (const auto& b : engine.getBuildings()) {
        if (b.playerOwner == 1 && plots[0].bounds.contains(b.position)) {
            p1Plot1Buildings++;
            if (isGenerator(b.type)) p1Plot1Generators++;
        }
    }
    std::cout << "  -> Buildings placed on Player 1 starting plot: " << p1Plot1Buildings << " / 9 slots ("
              << p1Plot1Generators << " generators)!\n";
    assert(p1Plot1Buildings == 9); // Exactly 9 buildings fit on the plot (3x3 grid)!

    // Count buildings on Player 2 starting plot
    int p2Plot1Buildings = 0;
    int p2Plot1Generators = 0;
    for (const auto& b : engine.getBuildings()) {
        if (b.playerOwner == 2 && plots[14].bounds.contains(b.position)) {
            p2Plot1Buildings++;
            if (isGenerator(b.type)) p2Plot1Generators++;
        }
    }
    std::cout << "  -> Buildings placed on Player 2 starting plot: " << p2Plot1Buildings << " / 9 slots ("
              << p2Plot1Generators << " generators)!\n";
    assert(p2Plot1Buildings == 9); // Exactly 9 buildings fit on the plot (3x3 grid)!

    // The starting plots hold real generators (8 of the first 9 buildings), not just lamps or batteries
    assert(p1Plot1Generators >= 8);
    assert(p2Plot1Generators >= 8);
    int p1BaseMW = 0, p2BaseMW = 0, lamps = 0;
    for (const auto& b : engine.getBuildings()) {
        if (b.type == BuildingType::LAMP) lamps++;
        int mw = engine.getBuildingCost(b.type).basePowerMW;
        if (b.playerOwner == 1) p1BaseMW += mw; else p2BaseMW += mw;
    }
    std::cout << "  -> Installed base power: P1 " << p1BaseMW << " MW | P2 " << p2BaseMW << " MW\n";
    assert(lamps == 0);
    assert(p1BaseMW >= 8 * Balance::SOLAR_PANEL.basePowerMW);
    assert(p2BaseMW >= 8 * Balance::SOLAR_PANEL.basePowerMW);

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

    std::cout << "\n[PHASE 6] Validating Gradual Victory at " << static_cast<int>(Balance::VICTORY_SHARE * 100.0f + 0.5f)
              << "% City Share...\n";
    // P1 powers the city every day, P2 never does: no change during the 2-day grace period, then
    // +15% at each day end (50 -> 65 -> 80 -> 95%), so P1 wins when day 5 ends
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
    // One second later the turbines deliver power, but territory only moves at a day end
    victoryEngine.update(1.0f);
    assert(victoryEngine.getPlayerEconomy(1).energyMW > 0);
    assert(victoryEngine.getPlayerEconomy(2).energyMW == 0);

    float infDay1 = victoryEngine.getPlayerEconomy(1).cityInfluence;
    assert(infDay1 == 0.50f); // Grace period: no territory changes hands yet
    assert(victoryEngine.getCityState().winner == 0);

    // Play until the result, at most GRACE + 6 days
    const int maxSeconds = static_cast<int>((Balance::GRACE_PERIOD_DAYS + 6) * Balance::SECONDS_PER_DAY);
    float shareBefore = victoryEngine.getCityState().p1CityShare;
    for (int t = 0; t < maxSeconds; t++) {
        const int day = victoryEngine.getCurrentDay();
        victoryEngine.update(1.0f);
        if (victoryEngine.getCurrentDay() != day) {
            const float share = victoryEngine.getCityState().p1CityShare;
            const float gain = share - shareBefore;
            std::cout << "  Day " << day << " ended: P1 share " << (share * 100.0f) << "%\n";
            if (day <= Balance::GRACE_PERIOD_DAYS) {
                assert(gain == 0.0f);
            } else {
                assert(gain >= Balance::MIN_DAILY_CITY_SHIFT - 1e-4f && gain <= Balance::MAX_DAILY_CITY_SHIFT + 1e-4f);
            }
            if (share < Balance::VICTORY_SHARE - 1e-4f) {
                assert(victoryEngine.getCityState().winner == 0);
            }
            shareBefore = share;
        }
        if (victoryEngine.getCityState().winner != 0) break;
    }
    assert(victoryEngine.getCityState().winner == 1);
    assert(victoryEngine.getCurrentDay() == 6); // decided when day 5 ended
    assert(victoryEngine.getPlayerEconomy(1).cityInfluence >= Balance::VICTORY_SHARE);
    assert(victoryEngine.getPlayerEconomy(2).cityInfluence <= 1.0f - Balance::VICTORY_SHARE + 1e-4f);
    std::cout << "  -> PASS: " << victoryEngine.getCityState().lastCutMessage << "\n";

    std::cout << "========================================================\n";
    std::cout << " 100X SPEED SIMULATION COMPLETED SUCCESSFULLY! ALL PASS!\n";
    std::cout << "========================================================\n";
    return 0;
}
