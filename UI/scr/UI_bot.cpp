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
            moveSpeed = 260.0f;
            decisionInterval = 1.4f;
            harvestTimeBudget = 2.4f;
            break;
        case BotDifficulty::MEDIUM:
            moveSpeed = 400.0f;
            decisionInterval = 0.85f;
            harvestTimeBudget = 1.6f;
            break;
        case BotDifficulty::HARD:
            moveSpeed = 560.0f;
            decisionInterval = 0.35f;
            harvestTimeBudget = 1.0f;
            break;
        case BotDifficulty::NONE:
        default:
            break;
    }
    std::cout << "[UIBot] Initialized with difficulty: " << static_cast<int>(diff) << "\n";
}

void UIBot::reset() {
    actionState = BotActionState::THINKING;
    targetPos = { 1150.0f, 450.0f };
    stateTimer = 0.6f;
    mineCooldown = 0.0f;
    harvestTimer = 0.0f;
    plannedBuilding = BuildingType::NONE;
    plannedResource = ResourceType::NONE;
    plannedPlotId = -1;
}

void UIBot::planNextAction(GameEngine& engine, const UI_resourceNodes& nodes, sf::Vector2f curPos) {
    (void)curPos;
    const auto& econ = engine.getPlayerEconomy(2);
    int wood = econ.wood;
    int iron = econ.iron;
    int copper = econ.copper;
    int silicon = econ.silicon;
    int coal = econ.coal;
    int gold = econ.gold;
    bool isDay = engine.isDaylight();
    WeatherType weather = engine.getPlayerWeather(2);

    // 1. Gather all free buildable slots on Player 2's purchased land plots
    std::vector<sf::Vector2f> freeSlots;
    std::vector<sf::Vector2f> freeRiverSlots;

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
                    if (std::abs(b.position.x - slot.x) < 25.0f && std::abs(b.position.y - slot.y) < 25.0f) {
                        occupied = true;
                        break;
                    }
                }
                if (!occupied) {
                    freeSlots.push_back(slot);
                    if (slot.x <= 880.0f) {
                        freeRiverSlots.push_back(slot);
                    }
                }
            }
        }
    }

    // 2. Priority A: Land plot expansion if free slots are low or have abundant gold
    if (freeSlots.size() <= 3 || (gold >= 25 && rand() % 3 == 0)) {
        for (const auto& plot : engine.getLandPlots()) {
            if (plot.playerOwner == 2 && !plot.isPurchased && gold >= plot.costGold) {
                actionState = BotActionState::MOVING_TO_BUY_LAND;
                targetPos = sf::Vector2f(plot.bounds.position.x + plot.bounds.size.x / 2.0f,
                                         plot.bounds.position.y + plot.bounds.size.y / 2.0f);
                plannedPlotId = plot.id;
                plannedBuilding = BuildingType::NONE;
                return;
            }
        }
    }

    // 3. Priority B: Upgrade high-yield mines with gold
    if (gold >= 15 && (difficulty == BotDifficulty::HARD || (difficulty == BotDifficulty::MEDIUM && rand() % 2 == 0))) {
        ResourceType upResList[4] = { ResourceType::WOOD, ResourceType::IRON, ResourceType::COPPER, ResourceType::SILICON };
        for (auto resType : upResList) {
            int level = econ.mineLevels[static_cast<int>(resType)];
            if (level < 5 && gold >= (15 * level)) {
                const auto* st = nodes.getStation(2, resType);
                if (st) {
                    actionState = BotActionState::MOVING_TO_UPGRADE;
                    targetPos = sf::Vector2f(st->upgradeBtnBounds.position.x + st->upgradeBtnBounds.size.x / 2.0f,
                                             st->upgradeBtnBounds.position.y + st->upgradeBtnBounds.size.y / 2.0f);
                    plannedBuilding = BuildingType::NONE;
                    return;
                }
            }
        }
    }

    // 4. Priority C: Build Power Plants if free slots are available
    if (!freeSlots.empty()) {
        // Option 1: Hydro Turbine along river edge (highest 24/7 output!)
        if (!freeRiverSlots.empty() && (difficulty == BotDifficulty::HARD || rand() % 2 == 0)) {
            BuildingCost hc = engine.getBuildingCost(BuildingType::HYDRO_PLANT);
            if (wood >= hc.woodCost && iron >= hc.ironCost && copper >= hc.copperCost) {
                actionState = BotActionState::MOVING_TO_BUILD;
                plannedBuilding = BuildingType::HYDRO_PLANT;
                targetPos = freeRiverSlots[0];
                return;
            }
        }

        // Option 2: Lighting Lamp if night and unlit
        if (!isDay && freeSlots.size() > 0) {
            BuildingCost lc = engine.getBuildingCost(BuildingType::LAMP);
            if (wood >= lc.woodCost && copper >= lc.copperCost) {
                // Check if any lamp exists
                int lampCount = 0;
                for (const auto& b : engine.getBuildings()) {
                    if (b.playerOwner == 2 && b.type == BuildingType::LAMP) lampCount++;
                }
                if (lampCount < 2) {
                    actionState = BotActionState::MOVING_TO_BUILD;
                    plannedBuilding = BuildingType::LAMP;
                    targetPos = freeSlots[0];
                    return;
                }
            }
        }

        // Option 3: Battery Storage (if day and has high power, Medium or Hard)
        if (isDay && econ.energyMW >= 120 && difficulty != BotDifficulty::EASY) {
            BuildingCost bc = engine.getBuildingCost(BuildingType::BATTERY);
            if (wood >= bc.woodCost && silicon >= bc.siliconCost && copper >= bc.copperCost) {
                int batCount = 0;
                for (const auto& b : engine.getBuildings()) {
                    if (b.playerOwner == 2 && b.type == BuildingType::BATTERY) batCount++;
                }
                if (batCount < 2) {
                    actionState = BotActionState::MOVING_TO_BUILD;
                    plannedBuilding = BuildingType::BATTERY;
                    targetPos = freeSlots[0];
                    return;
                }
            }
        }

        // Option 4: Wind Turbine (especially in windy/stormy conditions or at night)
        BuildingCost wc = engine.getBuildingCost(BuildingType::WIND_TURBINE);
        bool preferWind = (!isDay || weather == WeatherType::WINDY || weather == WeatherType::STORMY || (rand() % 2 == 0));
        if (preferWind && wood >= wc.woodCost && iron >= wc.ironCost && copper >= wc.copperCost) {
            actionState = BotActionState::MOVING_TO_BUILD;
            plannedBuilding = BuildingType::WIND_TURBINE;
            targetPos = freeSlots[0];
            return;
        }

        // Option 5: Solar Panel (daylight staple)
        BuildingCost sc = engine.getBuildingCost(BuildingType::SOLAR_PANEL);
        if (wood >= sc.woodCost && iron >= sc.ironCost) {
            actionState = BotActionState::MOVING_TO_BUILD;
            plannedBuilding = BuildingType::SOLAR_PANEL;
            targetPos = freeSlots[0];
            return;
        }
    }

    // 5. Priority D: Missing Resource Mining
    BuildingCost goal = engine.getBuildingCost(BuildingType::WIND_TURBINE);
    if (difficulty == BotDifficulty::EASY) goal = engine.getBuildingCost(BuildingType::SOLAR_PANEL);

    ResourceType missing = ResourceType::WOOD;
    if (wood < goal.woodCost) {
        missing = ResourceType::WOOD;
    } else if (iron < goal.ironCost) {
        missing = ResourceType::IRON;
    } else if (goal.copperCost > 0 && copper < goal.copperCost) {
        missing = ResourceType::COPPER;
    } else if (difficulty != BotDifficulty::EASY && silicon < 20) {
        missing = ResourceType::SILICON;
    } else if (difficulty == BotDifficulty::HARD && coal < 20) {
        missing = ResourceType::COAL;
    } else {
        // Random resource rotation
        ResourceType pool[4] = { ResourceType::WOOD, ResourceType::IRON, ResourceType::COPPER, ResourceType::SILICON };
        missing = pool[rand() % 4];
    }

    const auto* st = nodes.getStation(2, missing);
    if (st) {
        actionState = BotActionState::MOVING_TO_MINE;
        plannedResource = missing;
        targetPos = sf::Vector2f(st->bounds.position.x + st->bounds.size.x / 2.0f,
                                 st->bounds.position.y + st->bounds.size.y / 2.0f);
        harvestTimer = 0.0f;
        mineCooldown = 0.0f;
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

    // Smooth cursor movement toward target position
    sf::Vector2f delta = targetPos - botPos;
    float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    float step = moveSpeed * dt;

    if (dist > step && dist > 14.0f) {
        sf::Vector2f norm = delta / dist;
        botPos += norm * step;

        if (actionState == BotActionState::MOVING_TO_BUILD) {
            outSelectedBuilding = plannedBuilding;
        }
        return;
    }

    // Bot has reached the target location!
    botPos = targetPos;

    if (actionState == BotActionState::MOVING_TO_BUILD) {
        outSelectedBuilding = plannedBuilding;
        outTriggerAction = true;
        actionState = BotActionState::THINKING;
        stateTimer = decisionInterval;
    } else if (actionState == BotActionState::MOVING_TO_BUY_LAND) {
        outSelectedBuilding = BuildingType::NONE;
        outTriggerAction = true;
        actionState = BotActionState::THINKING;
        stateTimer = decisionInterval;
    } else if (actionState == BotActionState::MOVING_TO_UPGRADE) {
        outTriggerUpgrade = true;
        actionState = BotActionState::THINKING;
        stateTimer = decisionInterval;
    } else if (actionState == BotActionState::MOVING_TO_MINE) {
        outSelectedBuilding = BuildingType::NONE;
        harvestTimer += dt;
        mineCooldown -= dt;

        if (mineCooldown <= 0.0f) {
            outTriggerAction = true;
            mineCooldown = 1.05f; // Wait for the 1.0s station cooldown
        }

        if (harvestTimer >= harvestTimeBudget) {
            actionState = BotActionState::THINKING;
            stateTimer = decisionInterval;
        }
    }
}
