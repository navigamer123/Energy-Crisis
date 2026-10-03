#include "../includes/UI_bot.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <iostream>

void UIBot::init(BotDifficulty diff) {
    difficulty = diff;
    reset();

    switch (difficulty) {
        case BotDifficulty::EASY:
            moveSpeed = 300.0f;
            decisionInterval = 0.65f;
            mineHitInterval = 1.15f;
            break;
        case BotDifficulty::MEDIUM:
            moveSpeed = 460.0f;
            decisionInterval = 0.22f;
            mineHitInterval = 1.04f;
            break;
        case BotDifficulty::HARD:
            moveSpeed = 660.0f;
            decisionInterval = 0.08f;
            mineHitInterval = 1.01f;
            break;
        case BotDifficulty::NONE:
        default:
            break;
    }
    std::cout << "[UIBot] Initialized with strategic brain difficulty: " << static_cast<int>(diff) << "\n";
}

void UIBot::reset() {
    actionState = BotActionState::THINKING;
    targetPos = { 1150.0f, 450.0f };
    stateTimer = 0.4f;
    mineCooldown = 0.0f;
    stateWatchdog = 0.0f;
    plannedBuilding = BuildingType::NONE;
    plannedBuildSlot = { 0.0f, 0.0f };
    plannedResource = ResourceType::NONE;
    targetResourceQuota = 0;
    plannedPlotId = -1;
    plannedUpgradeRes = ResourceType::NONE;
}

int UIBot::getResourceCount(const PlayerEconomy& econ, ResourceType type) const {
    switch (type) {
        case ResourceType::WOOD:    return econ.wood;
        case ResourceType::IRON:    return econ.iron;
        case ResourceType::COPPER:  return econ.copper;
        case ResourceType::COAL:    return econ.coal;
        case ResourceType::SILICON: return econ.silicon;
        case ResourceType::SILVER:  return econ.silver;
        case ResourceType::GOLD:    return econ.gold;
        case ResourceType::MONEY:   return econ.money;
        default: return 0;
    }
}

void UIBot::planNextAction(GameEngine& engine, const UI_resourceNodes& nodes, sf::Vector2f curPos) {
    (void)curPos;
    const auto& econ = engine.getPlayerEconomy(2);
    bool isDay = engine.isDaylight();
    WeatherType weather = engine.getPlayerWeather(2);
    int gold = econ.gold;
    int curEnergy = econ.energyMW;

    // -------------------------------------------------------------------------
    // 1. Gather all free buildable slots on Player 2's purchased land plots
    // -------------------------------------------------------------------------
    std::vector<sf::Vector2f> freeSlots;
    std::vector<sf::Vector2f> illuminatedFreeSlots;

    for (int r = 0; r < 12; ++r) {
        for (int c = 0; c < 9; ++c) {
            sf::Vector2f slot = engine.getGridSlot(2, c, r);
            bool onPurchased = false;
            for (const auto& plot : engine.getLandPlots()) {
                if (plot.playerOwner == 2 && plot.isPurchased && plot.bounds.contains(slot)) {
                    onPurchased = true;
                    break;
                }
            }
            if (onPurchased) {
                bool occupied = false;
                for (const auto& b : engine.getBuildings()) {
                    if (b.playerOwner == 2) {
                        float dx = b.position.x - slot.x;
                        float dy = b.position.y - slot.y;
                        if (std::sqrt(dx * dx + dy * dy) < 18.0f) {
                            occupied = true;
                            break;
                        }
                    }
                }
                if (!occupied) {
                    freeSlots.push_back(slot);
                    if (engine.isAreaIlluminated(2, slot)) {
                        illuminatedFreeSlots.push_back(slot);
                    }
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // 2. Land Expansion Evaluation
    // -------------------------------------------------------------------------
    const LandPlot* cheapestUnboughtPlot = nullptr;
    int lowestPlotCost = 999999;
    for (const auto& plot : engine.getLandPlots()) {
        if (plot.playerOwner == 2 && !plot.isPurchased) {
            if (plot.costGold < lowestPlotCost) {
                lowestPlotCost = plot.costGold;
                cheapestUnboughtPlot = &plot;
            }
        }
    }

    bool needLandUrgent = freeSlots.empty() && cheapestUnboughtPlot;
    bool canExpandComfortably = (freeSlots.size() <= 2 && cheapestUnboughtPlot && gold >= cheapestUnboughtPlot->costGold);

    if (needLandUrgent) {
        if (gold >= cheapestUnboughtPlot->costGold) {
            actionState = BotActionState::MOVING_TO_BUY_LAND;
            targetPos = sf::Vector2f(cheapestUnboughtPlot->bounds.position.x + cheapestUnboughtPlot->bounds.size.x / 2.0f,
                                     cheapestUnboughtPlot->bounds.position.y + cheapestUnboughtPlot->bounds.size.y / 2.0f);
            plannedPlotId = cheapestUnboughtPlot->id;
            plannedBuilding = BuildingType::NONE;
            return;
        } else {
            // Need gold to expand land! Mine Gold continuously until threshold
            const auto* st = nodes.getStation(2, ResourceType::GOLD);
            if (st) {
                actionState = BotActionState::MOVING_TO_MINE;
                plannedResource = ResourceType::GOLD;
                targetResourceQuota = cheapestUnboughtPlot->costGold;
                targetPos = sf::Vector2f(st->bounds.position.x + st->bounds.size.x / 2.0f,
                                         st->bounds.position.y + 35.0f);
                mineCooldown = 0.0f;
                stateWatchdog = 0.0f;
                plannedBuilding = BuildingType::NONE;
                return;
            }
        }
    } else if (canExpandComfortably && (difficulty != BotDifficulty::EASY || rand() % 2 == 0)) {
        actionState = BotActionState::MOVING_TO_BUY_LAND;
        targetPos = sf::Vector2f(cheapestUnboughtPlot->bounds.position.x + cheapestUnboughtPlot->bounds.size.x / 2.0f,
                                 cheapestUnboughtPlot->bounds.position.y + cheapestUnboughtPlot->bounds.size.y / 2.0f);
        plannedPlotId = cheapestUnboughtPlot->id;
        plannedBuilding = BuildingType::NONE;
        return;
    }

    // -------------------------------------------------------------------------
    // 3. Mine Upgrade Evaluation (Permanent +75% Yield Snowball)
    // -------------------------------------------------------------------------
    if (gold >= 15) {
        int maxAllowedLevel = (difficulty == BotDifficulty::HARD ? 5 : (difficulty == BotDifficulty::MEDIUM ? 3 : 2));
        ResourceType upgradePriority[4] = {
            ResourceType::WOOD,
            ResourceType::IRON,
            ResourceType::COPPER,
            ResourceType::SILICON
        };

        for (auto res : upgradePriority) {
            int currentLvl = econ.mineLevels[static_cast<int>(res)];
            int cost = currentLvl * 15;
            if (currentLvl < maxAllowedLevel && gold >= cost) {
                int upgradeRoll = rand() % 100;
                int upgradeThreshold = (difficulty == BotDifficulty::HARD ? 85 : (difficulty == BotDifficulty::MEDIUM ? 50 : 25));
                if (upgradeRoll < upgradeThreshold) {
                    const auto* st = nodes.getStation(2, res);
                    if (st) {
                        actionState = BotActionState::MOVING_TO_UPGRADE;
                        plannedUpgradeRes = res;
                        targetPos = sf::Vector2f(st->upgradeBtnBounds.position.x + st->upgradeBtnBounds.size.x / 2.0f,
                                                 st->upgradeBtnBounds.position.y + st->upgradeBtnBounds.size.y / 2.0f);
                        plannedBuilding = BuildingType::NONE;
                        return;
                    }
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // 4. Strategic Power Plant Selection
    // -------------------------------------------------------------------------
    int lampCount = 0;
    int batteryCount = 0;
    for (const auto& b : engine.getBuildings()) {
        if (b.playerOwner == 2) {
            if (b.type == BuildingType::LAMP) lampCount++;
            if (b.type == BuildingType::BATTERY) batteryCount++;
        }
    }

    struct CandidateChoice {
        BuildingType type;
        float score;
    };
    std::vector<CandidateChoice> candidateList;

    // Hydro Plant: 110 MW base, 24/7 continuous output, rain boost
    float hydroScore = (difficulty == BotDifficulty::HARD ? 120.0f : (difficulty == BotDifficulty::MEDIUM ? 95.0f : 65.0f));
    if (weather == WeatherType::RAINY) hydroScore += 40.0f;
    candidateList.push_back({ BuildingType::HYDRO_PLANT, hydroScore });

    // Wind Turbine: 85 MW base, 24/7 output, massive windy/stormy boost
    float windScore = (difficulty == BotDifficulty::HARD ? 100.0f : (difficulty == BotDifficulty::MEDIUM ? 90.0f : 75.0f));
    if (weather == WeatherType::WINDY) windScore += 60.0f;
    if (weather == WeatherType::STORMY) windScore += 90.0f;
    if (!isDay) windScore += 25.0f;
    candidateList.push_back({ BuildingType::WIND_TURBINE, windScore });

    // Solar Panel: 60 MW base during day, 0 at night
    float solarScore = 0.0f;
    if (isDay) {
        solarScore = (difficulty == BotDifficulty::EASY ? 105.0f : (difficulty == BotDifficulty::MEDIUM ? 70.0f : 50.0f));
    } else {
        solarScore = -999.0f; // Never build solar at night
    }
    candidateList.push_back({ BuildingType::SOLAR_PANEL, solarScore });

    // Battery: Stores excess daytime generation for night stability
    float batteryScore = 0.0f;
    if (batteryCount < 2 && curEnergy >= 95 && difficulty != BotDifficulty::EASY) {
        batteryScore = (difficulty == BotDifficulty::HARD ? 105.0f : 65.0f);
    }
    candidateList.push_back({ BuildingType::BATTERY, batteryScore });

    // Street Lamp: Illuminates night darkness (allows night construction)
    float lampScore = 0.0f;
    if (!isDay && illuminatedFreeSlots.empty() && lampCount < 2 && !freeSlots.empty()) {
        lampScore = 300.0f; // Urgent: plots are dark at night!
    } else if (lampCount == 0 && !freeSlots.empty()) {
        lampScore = 40.0f;
    }
    candidateList.push_back({ BuildingType::LAMP, lampScore });

    std::sort(candidateList.begin(), candidateList.end(), [](const CandidateChoice& a, const CandidateChoice& b) {
        return a.score > b.score;
    });

    // -------------------------------------------------------------------------
    // 5. Select Winning Candidate & Valid Free Slot
    // -------------------------------------------------------------------------
    BuildingType chosenType = BuildingType::NONE;
    sf::Vector2f chosenSlot = { 0.0f, 0.0f };
    bool foundCandidate = false;

    for (const auto& cand : candidateList) {
        if (cand.score <= -500.0f) continue;

        if (cand.type == BuildingType::LAMP) {
            if (!freeSlots.empty()) {
                chosenType = cand.type;
                chosenSlot = freeSlots[freeSlots.size() / 2]; // Central slot
                foundCandidate = true;
                break;
            }
        } else {
            if (!isDay) {
                // Must be an illuminated slot at night!
                if (!illuminatedFreeSlots.empty()) {
                    chosenType = cand.type;
                    chosenSlot = illuminatedFreeSlots[0];
                    foundCandidate = true;
                    break;
                }
            } else {
                if (!freeSlots.empty()) {
                    chosenType = cand.type;
                    chosenSlot = freeSlots[0];
                    foundCandidate = true;
                    break;
                }
            }
        }
    }

    // If night and no illuminated slots exist and cannot build lamp:
    // Strategic Night Stockpiling!
    if (!foundCandidate) {
        ResourceType nightStockpile[5] = {
            ResourceType::WOOD,
            ResourceType::IRON,
            ResourceType::COPPER,
            ResourceType::SILICON,
            ResourceType::COAL
        };
        int stockpileQuota[5] = { 25, 25, 18, 12, 10 };

        for (int i = 0; i < 5; ++i) {
            ResourceType res = nightStockpile[i];
            int quota = stockpileQuota[i];
            if (getResourceCount(econ, res) < quota) {
                const auto* st = nodes.getStation(2, res);
                if (st) {
                    actionState = BotActionState::MOVING_TO_MINE;
                    plannedResource = res;
                    targetResourceQuota = quota;
                    targetPos = sf::Vector2f(st->bounds.position.x + st->bounds.size.x / 2.0f,
                                             st->bounds.position.y + 35.0f);
                    mineCooldown = 0.0f;
                    stateWatchdog = 0.0f;
                    plannedBuilding = BuildingType::NONE;
                    return;
                }
            }
        }
        // If all stockpiles are full, rest briefly
        actionState = BotActionState::THINKING;
        stateTimer = 0.5f;
        return;
    }

    plannedBuilding = chosenType;
    plannedBuildSlot = chosenSlot;

    // -------------------------------------------------------------------------
    // 6. Compute Exact Deficits for Planned Building
    // -------------------------------------------------------------------------
    BuildingCost cost = engine.getBuildingCost(plannedBuilding);

    int needWood = std::max(0, cost.woodCost - econ.wood);
    int needIron = std::max(0, cost.ironCost - econ.iron);
    int needCopper = std::max(0, cost.copperCost - econ.copper);
    int needCoal = std::max(0, cost.coalCost - econ.coal);
    int needSilicon = std::max(0, cost.siliconCost - econ.silicon);
    int needSilver = std::max(0, cost.silverCost - econ.silver);

    bool allDeficitsZero = (needWood == 0 && needIron == 0 && needCopper == 0 &&
                            needCoal == 0 && needSilicon == 0 && needSilver == 0);

    if (allDeficitsZero) {
        // ALL RESOURCES READY! MARCH DIRECTLY TO THE GRID AND BUILD!
        actionState = BotActionState::MOVING_TO_BUILD;
        targetPos = plannedBuildSlot;
        stateWatchdog = 0.0f;
        return;
    }

    // -------------------------------------------------------------------------
    // 7. Deficit Exists -> Commit to Targeted Resource Mining (No wandering!)
    // -------------------------------------------------------------------------
    ResourceType missingRes = ResourceType::NONE;
    int quota = 0;

    if (needWood > 0) {
        missingRes = ResourceType::WOOD;
        quota = cost.woodCost;
    } else if (needIron > 0) {
        missingRes = ResourceType::IRON;
        quota = cost.ironCost;
    } else if (needCopper > 0) {
        missingRes = ResourceType::COPPER;
        quota = cost.copperCost;
    } else if (needCoal > 0) {
        missingRes = ResourceType::COAL;
        quota = cost.coalCost;
    } else if (needSilicon > 0) {
        missingRes = ResourceType::SILICON;
        quota = cost.siliconCost;
    } else if (needSilver > 0) {
        missingRes = ResourceType::SILVER;
        quota = cost.silverCost;
    }

    const auto* st = nodes.getStation(2, missingRes);
    if (st) {
        actionState = BotActionState::MOVING_TO_MINE;
        plannedResource = missingRes;
        targetResourceQuota = quota;
        targetPos = sf::Vector2f(st->bounds.position.x + st->bounds.size.x / 2.0f,
                                 st->bounds.position.y + 35.0f);
        mineCooldown = 0.0f;
        stateWatchdog = 0.0f;
        plannedBuilding = BuildingType::NONE;
    }
}

void UIBot::update(float dt, GameEngine& engine, const UI_resourceNodes& nodes, sf::Vector2f& botPos,
                   bool& outTriggerAction, bool& outTriggerUpgrade, BuildingType& outSelectedBuilding) {
    outTriggerAction = false;
    outTriggerUpgrade = false;
    outSelectedBuilding = BuildingType::NONE;

    if (difficulty == BotDifficulty::NONE) return;

    if (actionState == BotActionState::THINKING) {
        stateTimer -= dt;
        if (stateTimer <= 0.0f) {
            planNextAction(engine, nodes, botPos);
        }
        return;
    }

    // -------------------------------------------------------------------------
    // Movement phase: Smooth traversal toward targetPos
    // -------------------------------------------------------------------------
    if (actionState == BotActionState::MOVING_TO_BUILD ||
        actionState == BotActionState::MOVING_TO_MINE ||
        actionState == BotActionState::MOVING_TO_BUY_LAND ||
        actionState == BotActionState::MOVING_TO_UPGRADE) {

        sf::Vector2f delta = targetPos - botPos;
        float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        float step = moveSpeed * dt;

        if (dist > step && dist > 10.0f) {
            sf::Vector2f norm = delta / dist;
            botPos += norm * step;

            if (actionState == BotActionState::MOVING_TO_BUILD) {
                outSelectedBuilding = plannedBuilding;
            }
            return;
        }

        // Arrived at destination!
        botPos = targetPos;

        if (actionState == BotActionState::MOVING_TO_BUILD) {
            outSelectedBuilding = plannedBuilding;
            outTriggerAction = true;
            actionState = BotActionState::THINKING;
            stateTimer = decisionInterval;
            return;
        }

        if (actionState == BotActionState::MOVING_TO_BUY_LAND) {
            outSelectedBuilding = BuildingType::NONE;
            outTriggerAction = true;
            actionState = BotActionState::THINKING;
            stateTimer = decisionInterval;
            return;
        }

        if (actionState == BotActionState::MOVING_TO_UPGRADE) {
            outTriggerUpgrade = true;
            actionState = BotActionState::THINKING;
            stateTimer = decisionInterval;
            return;
        }

        if (actionState == BotActionState::MOVING_TO_MINE) {
            // Arrived at the mine station! Switch to continuous harvesting state!
            actionState = BotActionState::MINING_RESOURCE;
            mineCooldown = 0.0f; // Immediate first hit!
            stateWatchdog = 0.0f;
            // Proceed to MINING_RESOURCE immediately
        }
    }

    // -------------------------------------------------------------------------
    // Continuous Harvesting Loop: Stay on mine until quota is reached!
    // -------------------------------------------------------------------------
    if (actionState == BotActionState::MINING_RESOURCE) {
        botPos = targetPos; // Maintain position on station
        outSelectedBuilding = BuildingType::NONE;

        mineCooldown -= dt;
        stateWatchdog += dt;

        if (mineCooldown <= 0.0f) {
            outTriggerAction = true;
            mineCooldown = mineHitInterval;
        }

        // Check inventory against required quota
        int curAmount = getResourceCount(engine.getPlayerEconomy(2), plannedResource);
        if (curAmount >= targetResourceQuota || stateWatchdog >= 12.0f) {
            // Quota met! Transition to thinking to select next missing resource or build!
            actionState = BotActionState::THINKING;
            stateTimer = (difficulty == BotDifficulty::HARD ? 0.04f : (difficulty == BotDifficulty::MEDIUM ? 0.12f : 0.25f));
        }
    }
}
