#include "../includes/UI_bot.h"
#include "../../Game/includes/game_forecast.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <iostream>

// =============================================================================
// Bot brain. Every number that shapes HOW it plays comes from its BotProfile
// (UI/scr/UI_botProfiles.cpp): the difficulty sets speed, reactions and the demand-margin
// governor, the rival sets what it likes to build. [AI team: BAL-01, F-17, НЕВЪЗМОЖНО]
// =============================================================================

namespace {

const char* resourceNameBg(ResourceType r) {
    switch (r) {
        case ResourceType::WOOD:    return "Дърво";
        case ResourceType::IRON:    return "Желязо";
        case ResourceType::COPPER:  return "Мед";
        case ResourceType::COAL:    return "Въглища";
        case ResourceType::SILICON: return "Силиций";
        case ResourceType::SILVER:  return "Сребро";
        case ResourceType::GOLD:    return "Злато";
        default:                    return "Ресурс";
    }
}

const char* buildingNameBg(BuildingType t) {
    switch (t) {
        case BuildingType::SOLAR_PANEL:  return "Соларен панел";
        case BuildingType::WIND_TURBINE: return "Вятърна мелница";
        case BuildingType::HYDRO_PLANT:  return "ВЕЦ";
        case BuildingType::BATTERY:      return "Батерия";
        case BuildingType::LAMP:         return "Лампа";
        default:                         return "Сграда";
    }
}

std::string mwPair(float a, float b) {
    return std::to_string(static_cast<int>(std::lround(a))) + "/" + std::to_string(static_cast<int>(std::lround(b))) + " MW";
}

// Screen column (0..2) of a land plot inside its owner's 3x3 block
int plotColumn(const LandPlot& plot) {
    float startX = (plot.playerOwner == 1) ? 258.0f : 1003.0f;
    int col = static_cast<int>(std::lround((plot.bounds.position.x - startX) / 117.0f));
    return std::max(0, std::min(2, col));
}

int riverPlotColumn(int player) {
    return (player == 1) ? Balance::P1_RIVER_BANK_PLOT_COL : Balance::P2_RIVER_BANK_PLOT_COL;
}

bool rollPct(int pct) { return pct >= 100 || (pct > 0 && std::rand() % 100 < pct); }

} // namespace

void UIBot::init(BotDifficulty diff) {
    difficulty = diff;
    profile = (diff == BotDifficulty::NONE) ? BotProfile() : makeBotProfile(diff, personalityId);
    reset();
    if (diff != BotDifficulty::NONE) {
        std::cout << "[UIBot] " << botDifficultyNameBg(diff) << " bot ready: " << profile.nameBg << " (player "
                  << playerId << ")\n";
    }
}

void UIBot::initWithProfile(BotDifficulty diff, const BotProfile& custom) {
    difficulty = diff;
    profile = custom;
    reset();
}

void UIBot::reset() {
    actionState = BotActionState::THINKING;
    targetPos = (playerId == 1) ? sf::Vector2f(450.0f, 450.0f) : sf::Vector2f(1150.0f, 450.0f);
    stateTimer = 0.4f;
    mineCooldown = 0.0f;
    stateWatchdog = 0.0f;
    plannedBuilding = BuildingType::NONE;
    plannedBuildSlot = { 0.0f, 0.0f };
    plannedResource = ResourceType::NONE;
    targetResourceQuota = 0;
    plannedPlotId = -1;
    plannedUpgradeRes = ResourceType::NONE;
    buildGoal = BuildingType::NONE;
    lastSupplyMW = 0.0f;
    lastTargetMW = 0.0f;
    lastSatisfied = false;
    intent = "Оглежда терена...";
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

bool UIBot::startMining(const UI_resourceNodes& nodes, ResourceType res, int quota) {
    const auto* st = nodes.getStation(playerId, res);
    if (!st) return false;
    actionState = BotActionState::MOVING_TO_MINE;
    plannedResource = res;
    targetResourceQuota = quota;
    targetPos = sf::Vector2f(st->bounds.position.x + st->bounds.size.x / 2.0f, st->bounds.position.y + 35.0f);
    mineCooldown = 0.0f;
    stateWatchdog = 0.0f;
    plannedBuilding = BuildingType::NONE;
    return true;
}

void UIBot::setRest(float seconds, const std::string& why) {
    actionState = BotActionState::THINKING;
    stateTimer = seconds;
    intent = why;
}

// -----------------------------------------------------------------------------
// BAL-01 demand-margin governor: true when the bot already supplies enough for its taste
// (margin x demand, or floorMW in the grace period). Above it the bot stops starting new
// generators and stockpiles / upgrades / saves instead, so lower tiers really can fall short.
// -----------------------------------------------------------------------------
bool UIBot::evaluateGovernor(const GameEngine& engine) {
    const int day = engine.getCurrentDay();
    const int demandToday = engine.getCityState().cityEnergyDemand;
    const int demandTomorrow = Forecast::demandForDay(day + 1);
    const bool surging = (profile.surgeDay > 0 && day >= profile.surgeDay);
    const float margin = profile.demandMargin * (surging ? profile.surgeMarginScale : profile.marginScale);

    // What the bot believes it supplies: the lower tiers misjudge it the way new players do
    float nameplate = 0.0f;
    for (const auto& b : engine.getBuildings()) {
        if (b.playerOwner == playerId) nameplate += static_cast<float>(engine.getBuildingCost(b.type).basePowerMW);
    }
    float supplyToday = 0.0f;
    switch (profile.supplyView) {
        case BotSupplyView::NAMEPLATE: supplyToday = nameplate; break;
        case BotSupplyView::INSTANT: supplyToday = static_cast<float>(engine.getPlayerEconomy(playerId).energyMW); break;
        default: supplyToday = Forecast::projectTodayAverageMW(engine, playerId); break;
    }
    float targetToday = std::max(margin * static_cast<float>(demandToday), profile.floorMW);
    bool ok = supplyToday >= targetToday;
    lastSupplyMW = supplyToday;
    lastTargetMW = targetToday;

    if (ok && Forecast::hoursUntilRollover(engine) <= profile.lookaheadHours) {
        SeasonType seasonTomorrow = Balance::getSeasonForDay(day + 1);
        float supplyTomorrow = 0.0f;
        switch (profile.supplyView) {
            case BotSupplyView::NAMEPLATE:
                supplyTomorrow = nameplate;
                break;
            case BotSupplyView::INSTANT:
                supplyTomorrow = static_cast<float>(engine.getPlayerEconomy(playerId).energyMW);
                break;
            case BotSupplyView::PROJECTED:
                supplyTomorrow = Forecast::fullDayAverageMW(engine, playerId, engine.getPlayerWeather(playerId),
                                                            seasonTomorrow, demandTomorrow);
                break;
            case BotSupplyView::WORST_CASE:
                supplyTomorrow = Forecast::worstCaseDayAverageMW(engine, playerId, seasonTomorrow, demandTomorrow);
                break;
        }
        float targetTomorrow = std::max(margin * static_cast<float>(demandTomorrow), profile.floorMW);
        if (supplyTomorrow < targetTomorrow) {
            ok = false;
            lastSupplyMW = supplyTomorrow;
            lastTargetMW = targetTomorrow;
        }
    }
    lastSatisfied = ok;
    return ok;
}

void UIBot::planNextAction(GameEngine& engine, const UI_resourceNodes& nodes, sf::Vector2f curPos) {
    (void)curPos;
    const int me = playerId;
    const auto& econ = engine.getPlayerEconomy(me);
    const bool isDay = engine.isDaylight();
    const WeatherType weather = engine.getPlayerWeather(me);
    const int gold = econ.gold;
    const int curEnergy = econ.energyMW;
    buildGoal = BuildingType::NONE;

    // -------------------------------------------------------------------------
    // 1. Free buildable slots on purchased land (all / lit at night / river bank)
    // -------------------------------------------------------------------------
    // River bank = the plot column next to the city (same rule as GameEngine::isRiverBankSlot, O(1) here)
    const float sectorLeft = (me == 1) ? 258.0f : 1003.0f;
    auto riverSlot = [&](sf::Vector2f s) {
        return static_cast<int>((s.x - sectorLeft) / 117.0f) == riverPlotColumn(me);
    };
    std::vector<sf::Vector2f> freeSlots;
    std::vector<sf::Vector2f> litFreeSlots;
    int freeRiverSlots = 0;
    for (int r = 0; r < 12; ++r) {
        for (int c = 0; c < 9; ++c) {
            sf::Vector2f slot = engine.getGridSlot(me, c, r);
            bool onPurchased = false;
            for (const auto& plot : engine.getLandPlots()) {
                if (plot.playerOwner == me && plot.isPurchased && plot.bounds.contains(slot)) {
                    onPurchased = true;
                    break;
                }
            }
            if (!onPurchased) continue;
            bool occupied = false;
            for (const auto& b : engine.getBuildings()) {
                if (b.playerOwner != me) continue;
                float dx = b.position.x - slot.x;
                float dy = b.position.y - slot.y;
                if (dx * dx + dy * dy < 18.0f * 18.0f) {
                    occupied = true;
                    break;
                }
            }
            if (occupied) continue;
            freeSlots.push_back(slot);
            if (engine.isAreaIlluminated(me, slot)) litFreeSlots.push_back(slot);
            if (riverSlot(slot)) freeRiverSlots++;
        }
    }

    // -------------------------------------------------------------------------
    // 2. Slips: lower tiers sometimes waste a decision (a human-like blunder)
    // -------------------------------------------------------------------------
    if (profile.slipChance > 0.0f && std::rand() % 1000 < static_cast<int>(profile.slipChance * 1000.0f)) {
        const ResourceType wander[4] = { ResourceType::WOOD, ResourceType::IRON, ResourceType::COPPER, ResourceType::COAL };
        ResourceType res = wander[std::rand() % 4];
        if (std::rand() % 2 == 0 && startMining(nodes, res, getResourceCount(econ, res) + 1)) {
            intent = "Колебае се...";
            return;
        }
        setRest(0.8f, "Колебае се...");
        return;
    }

    // -------------------------------------------------------------------------
    // 3. Governor (BAL-01)
    // -------------------------------------------------------------------------
    const bool satisfied = evaluateGovernor(engine);
    const std::string supplyStr = mwPair(lastSupplyMW, lastTargetMW);

    // -------------------------------------------------------------------------
    // 4. Land: the cheapest plot (river-bank column first for hydro lovers without river slots)
    // -------------------------------------------------------------------------
    const LandPlot* plotToBuy = nullptr;
    for (const auto& plot : engine.getLandPlots()) {
        if (plot.playerOwner != me || plot.isPurchased) continue;
        if (!plotToBuy || plot.costGold < plotToBuy->costGold) plotToBuy = &plot;
    }
    if (profile.preferRiverPlots && freeRiverSlots == 0) {
        const LandPlot* river = nullptr;
        for (const auto& plot : engine.getLandPlots()) {
            if (plot.playerOwner != me || plot.isPurchased || plotColumn(plot) != riverPlotColumn(me)) continue;
            if (!river || plot.costGold < river->costGold) river = &plot;
        }
        if (river) plotToBuy = river;
    }

    auto goBuyPlot = [&](const LandPlot* plot) {
        actionState = BotActionState::MOVING_TO_BUY_LAND;
        targetPos = sf::Vector2f(plot->bounds.position.x + plot->bounds.size.x / 2.0f,
                                 plot->bounds.position.y + plot->bounds.size.y / 2.0f);
        plannedPlotId = plot->id;
        plannedBuilding = BuildingType::NONE;
        intent = "КУПУВА ЗЕМЯ (" + std::to_string(plot->costGold) + " G)";
    };

    const bool needLandUrgent = !satisfied && freeSlots.empty() && plotToBuy;
    const bool canExpand = plotToBuy && static_cast<int>(freeSlots.size()) <= profile.expandFreeSlots &&
                           gold >= plotToBuy->costGold;
    if (needLandUrgent) {
        if (gold >= plotToBuy->costGold) {
            goBuyPlot(plotToBuy);
            return;
        }
        if (startMining(nodes, ResourceType::GOLD, plotToBuy->costGold)) {
            intent = "ДОБИВ: Злато за земя";
            return;
        }
    } else if (canExpand && rollPct(profile.expandChancePct)) {
        goBuyPlot(plotToBuy);
        return;
    }

    // -------------------------------------------------------------------------
    // 5. Mine upgrades (+75% yield each) in the rival's priority order
    // -------------------------------------------------------------------------
    if (gold >= Balance::MINE_UPGRADE_COST_BASE) {
        for (ResourceType res : profile.upgradeOrder) {
            int lvl = econ.mineLevels[static_cast<int>(res)];
            int cost = engine.getMineUpgradeCost(me, res);
            if (cost > 0 && lvl < profile.maxMineLevel && gold >= cost && rollPct(profile.upgradeChancePct)) {
                const auto* st = nodes.getStation(me, res);
                if (!st) continue;
                actionState = BotActionState::MOVING_TO_UPGRADE;
                plannedUpgradeRes = res;
                targetPos = sf::Vector2f(st->upgradeBtnBounds.position.x + st->upgradeBtnBounds.size.x / 2.0f,
                                         st->upgradeBtnBounds.position.y + st->upgradeBtnBounds.size.y / 2.0f);
                plannedBuilding = BuildingType::NONE;
                intent = std::string("НАДГРАЖДА: ") + resourceNameBg(res) + " -> ниво " + std::to_string(lvl + 1);
                return;
            }
        }
    }

    // Stockpile helper shared by the "enough power" and the "dark night" branches
    const ResourceType stockRes[5] = { ResourceType::WOOD, ResourceType::IRON, ResourceType::COPPER,
                                       ResourceType::SILICON, ResourceType::COAL };
    auto tryStockpile = [&](const std::string& label) {
        for (int i = 0; i < 5; ++i) {
            int quota = profile.stockpileQuota[i];
            if (getResourceCount(econ, stockRes[i]) < quota && startMining(nodes, stockRes[i], quota)) {
                intent = label + ": " + resourceNameBg(stockRes[i]);
                return true;
            }
        }
        return false;
    };

    // -------------------------------------------------------------------------
    // 6. Governor satisfied: save, upgrade or stockpile instead of building
    // -------------------------------------------------------------------------
    if (satisfied) {
        // Минен Магнат: spends the grace period mining gold for its first mine upgrades
        if (engine.isGracePeriod() && profile.graceMineTarget > 0) {
            ResourceType res = profile.upgradeOrder[0];
            int lvl = econ.mineLevels[static_cast<int>(res)];
            int cost = engine.getMineUpgradeCost(me, res);
            if (lvl < profile.graceMineTarget && cost > gold && startMining(nodes, ResourceType::GOLD, cost)) {
                intent = std::string("ДОБИВ: Злато за ") + resourceNameBg(res) + " ниво " + std::to_string(lvl + 1);
                return;
            }
        }
        if (tryStockpile("ЗАПАСИ")) {
            intent += " (ток " + supplyStr + ")";
            return;
        }
        // НЕВЪЗМОЖНО never idles: gold buys the next plot and the next mine level
        if (difficulty == BotDifficulty::IMPOSSIBLE && plotToBuy && startMining(nodes, ResourceType::GOLD, gold + 60)) {
            intent = "ДОБИВ: Злато (ток " + supplyStr + ")";
            return;
        }
        setRest(0.5f, "Градът е захранен: " + supplyStr);
        return;
    }

    // -------------------------------------------------------------------------
    // 7. Below the margin: pick the generator this rival likes best right now
    // -------------------------------------------------------------------------
    int lampCount = 0;
    int batteryCount = 0;
    for (const auto& b : engine.getBuildings()) {
        if (b.playerOwner != me) continue;
        if (b.type == BuildingType::LAMP) lampCount++;
        if (b.type == BuildingType::BATTERY) batteryCount++;
    }

    struct CandidateChoice {
        BuildingType type;
        float score;
    };
    std::vector<CandidateChoice> candidateList;

    float hydroScore = profile.hydroScore;
    if (weather == WeatherType::RAINY || weather == WeatherType::STORMY) hydroScore += profile.rainHydroBonus;
    candidateList.push_back({ BuildingType::HYDRO_PLANT, hydroScore });

    float windScore = profile.windScore;
    if (weather == WeatherType::WINDY) windScore += profile.windyWindBonus;
    if (weather == WeatherType::STORMY) windScore += profile.stormWindBonus;
    if (!isDay) windScore += profile.nightWindBonus;
    candidateList.push_back({ BuildingType::WIND_TURBINE, windScore });

    // Solar never at night (and never for rivals that refuse panels); clouds, rain and storms make
    // panels less attractive today, so even the solar lover falls back to wind / hydro then
    float solarWeather = 1.0f;
    switch (weather) {
        case WeatherType::SUNNY:  solarWeather = 1.2f; break;
        case WeatherType::CLOUDY: solarWeather = 0.8f; break;
        case WeatherType::RAINY:  solarWeather = 0.5f; break;
        case WeatherType::SNOWY:  solarWeather = 0.3f; break;
        case WeatherType::STORMY: solarWeather = 0.1f; break;
        default: break;
    }
    float solarScore = (isDay && !profile.neverSolar) ? profile.solarScore * solarWeather : -999.0f;
    candidateList.push_back({ BuildingType::SOLAR_PANEL, solarScore });

    float batteryScore = (batteryCount < profile.batteryCap && curEnergy >= profile.batteryMinMW) ? profile.batteryScore
                                                                                                  : -999.0f;
    candidateList.push_back({ BuildingType::BATTERY, batteryScore });

    // A lamp only when it can be powered (10 MW each) and the night left no lit free slot
    float lampScore = -999.0f;
    if (lampCount < profile.lampCap && curEnergy >= 35 + 10 * lampCount && !freeSlots.empty() && !isDay &&
        litFreeSlots.empty()) {
        lampScore = 45.0f;
    }
    candidateList.push_back({ BuildingType::LAMP, lampScore });

    std::sort(candidateList.begin(), candidateList.end(),
              [](const CandidateChoice& a, const CandidateChoice& b) { return a.score > b.score; });

    // -------------------------------------------------------------------------
    // 8. Winning candidate with a valid free slot (river bank kept for hydro)
    // -------------------------------------------------------------------------
    BuildingType chosenType = BuildingType::NONE;
    sf::Vector2f chosenSlot = { 0.0f, 0.0f };
    bool foundCandidate = false;

    for (const auto& cand : candidateList) {
        if (cand.score <= -500.0f) continue;

        if (cand.type == BuildingType::LAMP) {
            // The free slot whose light reaches the most free slots
            int bestCover = -1;
            for (const auto& s : freeSlots) {
                int cover = 0;
                for (const auto& o : freeSlots) {
                    float dx = o.x - s.x, dy = o.y - s.y;
                    if (dx * dx + dy * dy <= 140.0f * 140.0f) cover++;
                }
                if (cover > bestCover) {
                    bestCover = cover;
                    chosenSlot = s;
                }
            }
            chosenType = cand.type;
            foundCandidate = bestCover >= 0;
            if (foundCandidate) break;
            continue;
        }

        // Must be a lit slot at night; hydro needs a river-bank slot, the others leave those free
        const std::vector<sf::Vector2f>& slotPool = isDay ? freeSlots : litFreeSlots;
        for (int pass = 0; pass < 2 && !foundCandidate; ++pass) {
            for (const auto& slot : slotPool) {
                bool river = riverSlot(slot);
                bool fits = (cand.type == BuildingType::HYDRO_PLANT) ? river : (pass == 1 || !river);
                if (fits) {
                    chosenType = cand.type;
                    chosenSlot = slot;
                    foundCandidate = true;
                    break;
                }
            }
            if (cand.type == BuildingType::HYDRO_PLANT) break; // no second pass for hydro
        }
        if (foundCandidate) break;
    }

    // Dark night without a lit slot or a lamp to place: stockpile for the morning
    if (!foundCandidate) {
        if (tryStockpile(isDay ? "ЗАПАСИ" : "НОЩ: ЗАПАСИ")) return;
        setRest(0.5f, isDay ? "Няма свободно място" : "Нощ: чака изгрева");
        return;
    }

    plannedBuilding = chosenType;
    plannedBuildSlot = chosenSlot;

    // -------------------------------------------------------------------------
    // 9. Exact deficits for the planned building (recipe after the bot's own modifiers)
    // -------------------------------------------------------------------------
    BuildingCost cost = engine.getBuildingCost(me, plannedBuilding);
    struct Need {
        ResourceType res;
        int have;
        int want;
    };
    const Need needs[6] = {
        { ResourceType::WOOD, econ.wood, cost.woodCost },       { ResourceType::IRON, econ.iron, cost.ironCost },
        { ResourceType::COPPER, econ.copper, cost.copperCost }, { ResourceType::COAL, econ.coal, cost.coalCost },
        { ResourceType::SILICON, econ.silicon, cost.siliconCost }, { ResourceType::SILVER, econ.silver, cost.silverCost },
    };

    for (const auto& n : needs) {
        if (n.have < n.want) {
            // Deficit: commit to mining exactly this resource up to the recipe amount
            BuildingType goal = plannedBuilding;
            if (startMining(nodes, n.res, n.want)) {
                buildGoal = goal;
                intent = std::string("ДОБИВ: ") + resourceNameBg(n.res) + " -> " + buildingNameBg(goal);
            }
            return;
        }
    }

    // All resources ready: march to the grid and build
    actionState = BotActionState::MOVING_TO_BUILD;
    targetPos = plannedBuildSlot;
    stateWatchdog = 0.0f;
    intent = std::string("СТРОИ: ") + buildingNameBg(plannedBuilding) + " (ток " + supplyStr + ")";
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
        float step = profile.moveSpeed * dt;

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
            stateTimer = profile.decisionInterval;
            return;
        }

        if (actionState == BotActionState::MOVING_TO_BUY_LAND) {
            outSelectedBuilding = BuildingType::NONE;
            outTriggerAction = true;
            actionState = BotActionState::THINKING;
            stateTimer = profile.decisionInterval;
            return;
        }

        if (actionState == BotActionState::MOVING_TO_UPGRADE) {
            outTriggerUpgrade = true;
            actionState = BotActionState::THINKING;
            stateTimer = profile.decisionInterval;
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
            mineCooldown = profile.mineHitInterval;
        }

        // Check inventory against required quota
        int curAmount = getResourceCount(engine.getPlayerEconomy(playerId), plannedResource);
        if (curAmount >= targetResourceQuota || stateWatchdog >= 12.0f) {
            // Quota met! Transition to thinking to select next missing resource or build!
            actionState = BotActionState::THINKING;
            stateTimer = profile.afterMineThink;
        }
    }
}
