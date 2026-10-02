#include "../includes/game_main.h"
#include <cmath>
#include <iostream>
#include <algorithm>

GameEngine::GameEngine()
    : gameSeconds(5.0f),
      currentDay(1),
      hour24(8.0f),
      secondsPerDay(60.0f), // 1 real minute = 1 full day
      p1Weather(WeatherType::SUNNY),
      p2Weather(WeatherType::WINDY),
      currentSeason(SeasonType::SPRING),
      timeScale(1.0f) {
}

void GameEngine::init(float screenWidth, float screenHeight) {
    (void)screenWidth;
    (void)screenHeight;
    gameSeconds = 5.0f;
    hour24 = 8.0f;
    currentDay = 1;
    timeScale = 1.0f;
    landPlots.clear();
    buildings.clear();

    // -------------------------------------------------------------------------
    // Generate Purchasable Land Grid on West (P1) and East (P2)
    // 12 land plots per player (3 cols x 4 rows)
    // Land dimensions strictly positioned so they NEVER touch or go under the city!
    // City bounds: X in [610.0, 990.0].
    // -------------------------------------------------------------------------
    float plotW = 105.0f;
    float plotH = 95.0f;
    float gapX = 12.0f;
    float gapY = 10.0f;

    // West Side (P1): 3 columns x 4 rows = 12 land plots
    // Col 0: 258-363, Col 1: 375-480, Col 2: 492-597 < 610. Row 0..3: Y in [105..515] < 582
    float westStartX = 258.0f;
    float startY = 105.0f;
    int idCounter = 1;

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 3; c++) {
            float x = westStartX + c * (plotW + gapX);
            float y = startY + r * (plotH + gapY);

            LandPlot plot;
            plot.id = idCounter++;
            plot.playerOwner = 1;
            plot.bounds = sf::FloatRect({ x, y }, { plotW, plotH });
            // Starting top-left plot is unlocked, others are purchasable
            plot.isPurchased = (r == 0 && c == 0);
            plot.costGold = 150 + (r * 3 + c) * 45;
            landPlots.push_back(plot);
        }
    }

    // East Side (P2): 3 columns x 4 rows = 12 land plots
    // Col 0: 1003-1108 > 990, Col 1: 1120-1225, Col 2: 1237-1342 < 1360. Row 0..3: Y in [105..515]
    float eastStartX = 1003.0f;
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 3; c++) {
            float x = eastStartX + c * (plotW + gapX);
            float y = startY + r * (plotH + gapY);

            LandPlot plot;
            plot.id = idCounter++;
            plot.playerOwner = 2;
            plot.bounds = sf::FloatRect({ x, y }, { plotW, plotH });
            // Starting top-right plot is unlocked, others are purchasable
            plot.isPurchased = (r == 0 && c == 2);
            plot.costGold = 150 + (r * 3 + c) * 45;
            landPlots.push_back(plot);
        }
    }

    // Players start with 0 buildings and 0 resources - they build from scratch
    buildings.clear();
    p1 = PlayerEconomy();
    p2 = PlayerEconomy();

    // Initialize day 1 weather with weather_report from weatherF
    auto rep1 = weather_report("spring");
    p1Weather = WeatherSystem::reportToWeatherType(rep1);
    p1.data.weather = weather_state;
    p1.data.wind_speed = (rep1.size() > 3) ? rep1[3] : "0";

    auto rep2 = weather_report("spring");
    p2Weather = WeatherSystem::reportToWeatherType(rep2);
    p2.data.weather = weather_state;
    p2.data.wind_speed = (rep2.size() > 3) ? rep2[3] : "0";

    std::cout << "[GameEngine] Backend initialized with " << landPlots.size() << " land plots (0 starter resources).\n";
}

void GameEngine::update(float dt) {
    float effectiveDt = dt * timeScale;
    gameSeconds += effectiveDt;
    float prevHour = hour24;
    hour24 = std::fmod((gameSeconds / secondsPerDay) * 24.0f + 6.0f, 24.0f);

    int calculatedDay = 1 + static_cast<int>(gameSeconds / secondsPerDay);
    if (calculatedDay > currentDay || (prevHour > 23.0f && hour24 < 1.0f)) {
        currentDay = calculatedDay;
        processDayEnd();
    }

    // Update building energy outputs based on real-time continuous weather & sun
    updateBuildingsEnergy(effectiveDt);

    // Percentage-based city energy revenue and gradual dynamic market influence
    static float revenueTimer = 0.0f;
    revenueTimer += effectiveDt;
    if (revenueTimer >= 1.0f) {
        revenueTimer = 0.0f;

        float totalGrid = static_cast<float>(p1.energyMW + p2.energyMW);
        if (totalGrid > 0.0f) {
            float p1Share = static_cast<float>(p1.energyMW) / totalGrid;
            float p2Share = static_cast<float>(p2.energyMW) / totalGrid;

            // City energy contract pool (scales with total clean power provided)
            int contractPool = 25 + static_cast<int>(totalGrid * 0.25f);
            int p1Payout = static_cast<int>(std::round(contractPool * p1Share));
            int p2Payout = static_cast<int>(std::round(contractPool * p2Share));

            p1.money += p1Payout;
            p1.data.money = p1.money;

            p2.money += p2Payout;
            p2.data.money = p2.money;

            // Gold dividend for sustained power supply
            if (p1.energyMW >= 30) {
                p1.gold += std::max(1, static_cast<int>(p1.energyMW * 0.03f));
                p1.data.gold = p1.gold;
            }
            if (p2.energyMW >= 30) {
                p2.gold += std::max(1, static_cast<int>(p2.energyMW * 0.03f));
                p2.data.gold = p2.gold;
            }

            // Gradual tug-of-war city influence progression (moves smoothly instead of jumping instantly)
            if (city.winner == 0) {
                float powerDiff = static_cast<float>(p1.energyMW - p2.energyMW);
                float driftStep = (powerDiff / std::max(50.0f, static_cast<float>(city.cityEnergyDemand))) * 0.015f;
                driftStep = std::clamp(driftStep, -0.03f, 0.03f);
                city.p1CityShare = std::clamp(city.p1CityShare + driftStep, 0.0f, 1.0f);
            }
        }

        p1.cityInfluence = city.p1CityShare;
        p2.cityInfluence = 1.0f - city.p1CityShare;

        // Victory condition when someone reaches 100% (1.0)
        if (city.p1CityShare >= 0.999f) {
            city.p1CityShare = 1.0f;
            p1.cityInfluence = 1.0f;
            p2.cityInfluence = 0.0f;
            city.winner = 1;
            city.lastCutMessage = "ПОБЕДА ЗА ИГРАЧ 1! ЦЕЛИЯТ ГРАД Е ПОД НЕГОВ КОНТРОЛ!";
        } else if (city.p1CityShare <= 0.001f) {
            city.p1CityShare = 0.0f;
            p1.cityInfluence = 0.0f;
            p2.cityInfluence = 1.0f;
            city.winner = 2;
            city.lastCutMessage = "ПОБЕДА ЗА ИГРАЧ 2! ЦЕЛИЯТ ГРАД Е ПОД НЕГОВ КОНТРОЛ!";
        }
    }
}

void GameEngine::updateBuildingsEnergy(float dt) {
    // -------------------------------------------------------------------------
    // 1. Advance building animation timers
    // -------------------------------------------------------------------------
    for (auto& b : buildings) {
        b.animTimer += dt;
    }

    // -------------------------------------------------------------------------
    // 2. Process Energy Grid for Player 1 (West) and Player 2 (East)
    // -------------------------------------------------------------------------
    auto processPlayerGrid = [&](int player, WeatherType w, PlayerEconomy& econ, float& dailyDelivered) {
        float rawGen = 0.0f;
        std::vector<PlacedBuilding*> playerLamps;
        std::vector<PlacedBuilding*> playerBatteries;

        // Step A: Calculate pure generation from Solar, Wind, and Hydro
        for (auto& b : buildings) {
            if (b.playerOwner != player) continue;
            BuildingCost cost = getBuildingCost(b.type);

            if (b.type == BuildingType::SOLAR_PANEL) {
                float out = cost.basePowerMW * WeatherSystem::getSolarMultiplier(w, hour24);
                b.currentOutputMW = out;
                rawGen += out;
            } else if (b.type == BuildingType::WIND_TURBINE) {
                float out = cost.basePowerMW * WeatherSystem::getWindMultiplier(w, hour24);
                b.currentOutputMW = out;
                rawGen += out;
            } else if (b.type == BuildingType::HYDRO_PLANT) {
                float out = cost.basePowerMW * WeatherSystem::getHydroMultiplier(w);
                b.currentOutputMW = out;
                rawGen += out;
            } else if (b.type == BuildingType::LAMP) {
                playerLamps.push_back(&b);
            } else if (b.type == BuildingType::BATTERY) {
                playerBatteries.push_back(&b);
            }
        }

        // Step B: Calculate Lamp demand (each active lamp consumes 10 MW)
        float lampDemand = playerLamps.size() * LAMP_POWER_MW;
        float netPlayerOutput = 0.0f;

        if (rawGen >= lampDemand) {
            // Surplus generation from active power plants!
            // All lamps are fully powered
            for (auto* lamp : playerLamps) {
                lamp->lightRadius = 150.0f;
                lamp->currentOutputMW = -LAMP_POWER_MW;
            }

            float surplus = rawGen - lampDemand;

            // Batteries charge ONLY from actual generated surplus power!
            // Batteries NEVER charge from thin air if player produces 0 energy.
            std::vector<PlacedBuilding*> notFullBatteries;
            for (auto* bat : playerBatteries) {
                if (bat->energyStored < bat->maxCapacity) {
                    notFullBatteries.push_back(bat);
                }
            }

            if (surplus > 0.0f && !notFullBatteries.empty()) {
                float maxChargeRequested = notFullBatteries.size() * 40.0f;
                float actualChargePower = std::min(surplus * 0.6f, maxChargeRequested);
                float chargePerBat = actualChargePower / notFullBatteries.size();

                for (auto* bat : notFullBatteries) {
                    float addEnergy = chargePerBat * dt;
                    bat->energyStored = std::min(bat->maxCapacity, bat->energyStored + addEnergy);
                    bat->currentOutputMW = 0.0f; // Storing clean electricity
                }
            }

            // Batteries that are already full or idling with surplus do not discharge
            for (auto* bat : playerBatteries) {
                if (bat->energyStored >= bat->maxCapacity) {
                    bat->currentOutputMW = 0.0f;
                }
            }

            netPlayerOutput = surplus;
        } else {
            // Deficit (e.g. night with no wind, or low generation)
            // Batteries discharge stored energy to power lamps and supply the grid!
            float totalDischarge = 0.0f;
            for (auto* bat : playerBatteries) {
                if (bat->energyStored > 0.0f) {
                    float maxDischarge = 40.0f;
                    float discharge = std::min(maxDischarge, bat->energyStored * 0.8f);
                    bat->currentOutputMW = discharge;
                    // Slowly consume stored energy based on discharge rate
                    bat->energyStored = std::max(0.0f, bat->energyStored - (discharge * 0.15f * dt));
                    totalDischarge += discharge;
                } else {
                    bat->currentOutputMW = 0.0f; // Depleted (0 MWh)
                }
            }

            float totalAvailable = rawGen + totalDischarge;

            // Power as many lamps as available electricity allows
            int poweredCount = static_cast<int>(totalAvailable / LAMP_POWER_MW);
            for (size_t i = 0; i < playerLamps.size(); i++) {
                if (static_cast<int>(i) < poweredCount) {
                    playerLamps[i]->lightRadius = 150.0f;
                    playerLamps[i]->currentOutputMW = -LAMP_POWER_MW;
                } else {
                    // UNPOWERED LAMP! Shuts down, dark lantern head, no light circle
                    playerLamps[i]->lightRadius = 0.0f;
                    playerLamps[i]->currentOutputMW = 0.0f;
                }
            }

            float remainingPower = std::max(0.0f, totalAvailable - (std::min((int)playerLamps.size(), poweredCount) * LAMP_POWER_MW));
            netPlayerOutput = remainingPower;
        }

        econ.energyMW = static_cast<int>(netPlayerOutput);
        dailyDelivered += netPlayerOutput * dt * 0.1f;
    };

    processPlayerGrid(1, p1Weather, p1, city.p1DailyDelivered);
    processPlayerGrid(2, p2Weather, p2, city.p2DailyDelivered);
}

void GameEngine::processDayEnd() {
    float quotaPerPlayer = city.cityEnergyDemand / 2.0f;
    city.dayCutOccurred = true;

    bool p1Success = (p1.energyMW >= quotaPerPlayer);
    bool p2Success = (p2.energyMW >= quotaPerPlayer);

    if (p1Success && !p2Success) {
        // Player 1 supplied enough, Player 2 failed -> Player 1 cuts off and captures Player 2's city half!
        city.p1CityShare = std::min(1.0f, city.p1CityShare + 0.15f);
        city.lastCutMessage = "ДЕН " + std::to_string(currentDay) + ": ИГРАЧ 2 НЕ ДОСТАВИ ЕНЕРГИЯ! ЧАСТ ОТ ГРАДА МУ Е ОТРЯЗАНА!";
    } else if (p2Success && !p1Success) {
        // Player 2 supplied enough, Player 1 failed -> Player 2 cuts off and captures Player 1's city half!
        city.p1CityShare = std::max(0.0f, city.p1CityShare - 0.15f);
        city.lastCutMessage = "ДЕН " + std::to_string(currentDay) + ": ИГРАЧ 1 НЕ ДОСТАВИ ЕНЕРГИЯ! ЧАСТ ОТ ГРАДА МУ Е ОТРЯЗАНА!";
    } else {
        // Both succeeded or both failed -> advantage to higher producer
        if (p1.energyMW > p2.energyMW + 50) {
            city.p1CityShare = std::min(1.0f, city.p1CityShare + 0.05f);
            city.lastCutMessage = "ДЕН " + std::to_string(currentDay) + ": ИГРАЧ 1 ДОСТАВИ ПОВЕЧЕ И ВЗЕМА ПРЕДИМСТВО В ГРАДА!";
        } else if (p2.energyMW > p1.energyMW + 50) {
            city.p1CityShare = std::max(0.0f, city.p1CityShare - 0.05f);
            city.lastCutMessage = "ДЕН " + std::to_string(currentDay) + ": ИГРАЧ 2 ДОСТАВИ ПОВЕЧЕ И ВЗЕМА ПРЕДИМСТВО В ГРАДА!";
        } else {
            city.lastCutMessage = "ДЕН " + std::to_string(currentDay) + ": РАВНОВЕСИЕ В ГРАДСКАТА МРЕЖА!";
        }
    }

    p1.cityInfluence = city.p1CityShare;
    p2.cityInfluence = 1.0f - city.p1CityShare;

    // Check Victory Condition
    if (city.p1CityShare >= 0.99f) {
        city.winner = 1;
        city.lastCutMessage = "ПОБЕДА ЗА ИГРАЧ 1! ЦЕЛИЯТ ГРАД Е ПОД НЕГОВ КОНТРОЛ!";
    } else if (city.p1CityShare <= 0.01f) {
        city.winner = 2;
        city.lastCutMessage = "ПОБЕДА ЗА ИГРАЧ 2! ЦЕЛИЯТ ГРАД Е ПОД НЕГОВ КОНТРОЛ!";
    }

    // City expands and demands more power next day (gradual progression)
    city.cityEnergyDemand += 25;
    city.p1DailyDelivered = 0.0f;
    city.p2DailyDelivered = 0.0f;

    // Daily dynamic weather generation using weather_report from weatherF
    int sIdx = ((currentDay - 1) / 5) % 4;
    currentSeason = static_cast<SeasonType>(sIdx);
    std::string sName = (currentSeason == SeasonType::SPRING) ? "spring" :
                        ((currentSeason == SeasonType::SUMMER) ? "summer" :
                        ((currentSeason == SeasonType::AUTUMN) ? "fall" : "winter"));

    auto rep1 = weather_report(sName);
    p1Weather = WeatherSystem::reportToWeatherType(rep1);
    p1.data.weather = weather_state;
    p1.data.wind_speed = (rep1.size() > 3) ? rep1[3] : "0";

    auto rep2 = weather_report(sName);
    p2Weather = WeatherSystem::reportToWeatherType(rep2);
    p2.data.weather = weather_state;
    p2.data.wind_speed = (rep2.size() > 3) ? rep2[3] : "0";
}

bool GameEngine::mineResource(int player, ResourceType type, std::string& outMsg) {
    MineResult ignored;
    return mineResource(player, type, ignored, outMsg);
}

bool GameEngine::mineResource(int player, ResourceType type, MineResult& result, std::string& outMsg) {
    auto& econ = (player == 1) ? p1 : p2;
    result = MineResult();
    result.type = type;

    int lvl = getMineLevel(player, type);
    float mult = 1.0f + (lvl - 1) * 0.75f;
    std::string lvlTag = (lvl > 1 ? " [НИВО " + std::to_string(lvl) + "]" : "");

    switch (type) {
        case ResourceType::WOOD: {
            int amount = static_cast<int>(std::round(12 * mult));
            econ.wood += amount;
            econ.data.wood = econ.wood;
            result.wood = amount;
            result.amount = amount;
            outMsg = "+" + std::to_string(amount) + " Дървесина (Гора)" + lvlTag;
            return true;
        }
        case ResourceType::IRON: {
            int amount = static_cast<int>(std::round(8 * mult));
            econ.iron += amount;
            econ.ore += amount;
            econ.data.iron = econ.iron;
            result.iron = amount;
            result.amount = amount;
            outMsg = "+" + std::to_string(amount) + " Желязо (Желязна мина)" + lvlTag;
            return true;
        }
        case ResourceType::COPPER: {
            int amount = static_cast<int>(std::round(6 * mult));
            econ.copper += amount;
            econ.ore += amount;
            econ.data.copper = econ.copper;
            result.copper = amount;
            result.amount = amount;
            outMsg = "+" + std::to_string(amount) + " Мед (Медна жила)" + lvlTag;
            return true;
        }
        case ResourceType::COAL: {
            int amount = static_cast<int>(std::round(6 * mult));
            econ.coal += amount;
            econ.ore += amount;
            econ.data.coal = econ.coal;
            result.coal = amount;
            result.amount = amount;
            outMsg = "+" + std::to_string(amount) + " Въглища (Въглищен пласт)" + lvlTag;
            return true;
        }
        case ResourceType::SILICON: {
            int amount = static_cast<int>(std::round(6 * mult));
            econ.silicon += amount;
            econ.ore += amount;
            econ.data.silicon = econ.silicon;
            result.silicon = amount;
            result.amount = amount;
            outMsg = "+" + std::to_string(amount) + " Силиций (Силициева кариера)" + lvlTag;
            return true;
        }
        case ResourceType::SILVER: {
            int amount = static_cast<int>(std::round(4 * mult));
            econ.silver += amount;
            econ.ore += amount;
            econ.data.silver = econ.silver;
            result.silver = amount;
            result.amount = amount;
            outMsg = "+" + std::to_string(amount) + " Сребро (Сребърна жила)" + lvlTag;
            return true;
        }
        case ResourceType::GOLD: {
            int amount = static_cast<int>(std::round(3 * mult));
            econ.gold += amount;
            econ.data.gold = econ.gold;
            result.gold = amount;
            result.amount = amount;
            outMsg = "+" + std::to_string(amount) + " Злато (Златна жила)" + lvlTag;
            return true;
        }
        case ResourceType::MONEY: {
            outMsg = "Мината за пари е премахната! Печелете пари от доставка на ток към града.";
            return false;
        }
        case ResourceType::ORE: {
            // Legacy cave expedition support
            int fe = 8, cu = 6, c = 6, au = 3, ore = 20;
            econ.iron += fe; econ.data.iron = econ.iron;
            econ.copper += cu; econ.data.copper = econ.copper;
            econ.coal += c; econ.data.coal = econ.coal;
            econ.gold += au; econ.data.gold = econ.gold;
            econ.ore += ore;
            result.iron = fe; result.copper = cu; result.coal = c; result.gold = au; result.amount = ore;
            outMsg = "+20 Руда, +3 Злато";
            return true;
        }
        default:
            return false;
    }
}

int GameEngine::getMineLevel(int player, ResourceType type) const {
    const auto& econ = (player == 1) ? p1 : p2;
    int idx = static_cast<int>(type);
    if (idx >= 0 && idx < 8) {
        return std::max(1, econ.mineLevels[idx]);
    }
    return 1;
}

int GameEngine::getMineUpgradeCost(int player, ResourceType type) const {
    int lvl = getMineLevel(player, type);
    if (lvl >= 5) return -1; // Max Level reached
    if (type == ResourceType::GOLD) {
        return 20 * lvl; // Gold mine: 20, 40, 60, 80 G
    }
    return 15 * lvl; // Other mines: 15, 30, 45, 60 G
}

bool GameEngine::upgradeMine(int player, ResourceType type, std::string& outMsg) {
    if (type == ResourceType::NONE || type == ResourceType::MONEY) {
        outMsg = "ТОВА НЕ Е МИНА ЗА НАДГРАЖДАНЕ!";
        return false;
    }
    auto& econ = (player == 1) ? p1 : p2;
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= 8) {
        outMsg = "НЕВАЛИДЕН РЕСУРС!";
        return false;
    }
    int lvl = econ.mineLevels[idx];
    if (lvl >= 5) {
        outMsg = "МАКСИМАЛНО НИВО НА МИНАТА (НИВО 5)!";
        return false;
    }
    int cost = getMineUpgradeCost(player, type);
    if (econ.gold < cost) {
        outMsg = "НЕДОСТИГ НА ЗЛАТО! НУЖНО: " + std::to_string(cost) + " G (ИМАТЕ " + std::to_string(econ.gold) + " G)";
        return false;
    }

    econ.gold -= cost;
    econ.data.gold = econ.gold;
    econ.mineLevels[idx]++;
    int newLvl = econ.mineLevels[idx];

    std::string resName;
    switch (type) {
        case ResourceType::WOOD: resName = "ДЪРВОДОБИВ"; break;
        case ResourceType::IRON: resName = "ЖЕЛЯЗНА МИНА"; break;
        case ResourceType::COPPER: resName = "МЕДНА МИНА"; break;
        case ResourceType::COAL: resName = "ВЪГЛИЩНА МИНА"; break;
        case ResourceType::SILICON: resName = "СИЛИЦИЕВА КАРИЕРА"; break;
        case ResourceType::SILVER: resName = "СРЕБЪРНА МИНА"; break;
        case ResourceType::GOLD: resName = "ЗЛАТНА ЖИЛА"; break;
        default: resName = "МИНА"; break;
    }

    outMsg = "НАДГРАДЕНО: " + resName + " (НИВО " + std::to_string(newLvl) + ")! (+75% ДОБИВ)";
    return true;
}

bool GameEngine::buyLandPlot(int player, int plotId, std::string& outMsg) {
    auto& econ = (player == 1) ? p1 : p2;
    for (auto& plot : landPlots) {
        if (plot.id == plotId && plot.playerOwner == player) {
            if (plot.isPurchased) {
                outMsg = "ТОЗИ ПАРЦЕЛ ВЕЧЕ Е ЗАКУПЕН!";
                return false;
            }
            if (econ.gold >= plot.costGold) {
                econ.gold -= plot.costGold;
                plot.isPurchased = true;
                econ.landTier++;
                outMsg = (player == 1 ? "ИГРАЧ 1 ЗАКУПИ НОВА ЗЕМЯ!" : "ИГРАЧ 2 ЗАКУПИ НОВА ЗЕМЯ!");
                return true;
            } else {
                outMsg = "НЕДОСТИГ НА ЗЛАТО! НУЖНО: " + std::to_string(plot.costGold) + " G";
                return false;
            }
        }
    }
    outMsg = "НЕВАЛИДЕН ПАРЦЕЛ!";
    return false;
}

bool GameEngine::buyNextLandTier(int player, std::string& outMsg) {
    for (auto& plot : landPlots) {
        if (plot.playerOwner == player && !plot.isPurchased) {
            return buyLandPlot(player, plot.id, outMsg);
        }
    }
    outMsg = "ВСИЧКИ ПАРЦЕЛИ СА ЗАКУПЕНИ!";
    return false;
}

void GameEngine::cycleBuildingSelection(int player) {
    auto& econ = (player == 1) ? p1 : p2;
    if (econ.selectedBuilding == 0) {
        int last = econ.lastPlacedBuilding;
        econ.selectedBuilding = (last >= 1 && last <= 6) ? last : 1;
    } else if (econ.selectedBuilding >= 6) {
        econ.selectedBuilding = 1;
    } else {
        econ.selectedBuilding++;
    }
}

void GameEngine::cycleBuildingSelectionPrev(int player) {
    auto& econ = (player == 1) ? p1 : p2;
    if (econ.selectedBuilding == 0) {
        int last = econ.lastPlacedBuilding;
        econ.selectedBuilding = (last >= 1 && last <= 6) ? last : 6;
    } else if (econ.selectedBuilding <= 1) {
        econ.selectedBuilding = 6;
    } else {
        econ.selectedBuilding--;
    }
}

void GameEngine::clearBuildingSelection(int player) {
    auto& econ = (player == 1) ? p1 : p2;
    econ.selectedBuilding = 0;
}

BuildingType GameEngine::getSelectedBuilding(int player) const {
    const auto& econ = (player == 1) ? p1 : p2;
    return static_cast<BuildingType>(econ.selectedBuilding);
}

BuildingCost GameEngine::getBuildingCost(BuildingType type) const {
    switch (type) {
        case BuildingType::SOLAR_PANEL:
            // 6 Wood, 4 Iron, 6 Copper, 8 Silicon (ore total: 18)
            return { BuildingType::SOLAR_PANEL, "Слънчев панел", "Solar Panel", 6, 4, 6, 0, 8, 0, 18, 60 };
        case BuildingType::WIND_TURBINE:
            // 8 Wood, 14 Iron, 8 Copper, 6 Coal (ore total: 28)
            return { BuildingType::WIND_TURBINE, "Вятърна мелница", "Wind Turbine", 8, 14, 8, 6, 0, 0, 28, 85 };
        case BuildingType::HYDRO_PLANT:
            // 15 Wood, 20 Iron, 12 Copper, 6 Silicon (ore total: 38)
            return { BuildingType::HYDRO_PLANT, "ВЕЦ / Хидро", "Hydro Plant", 15, 20, 12, 0, 6, 0, 38, 160 };
        case BuildingType::BATTERY:
            // 4 Wood, 8 Iron, 10 Copper, 4 Coal, 4 Silver (ore total: 26)
            return { BuildingType::BATTERY, "Батерия / Акумулатор", "Battery Storage", 4, 8, 10, 4, 0, 4, 26, 0 };
        case BuildingType::LAMP:
            // 4 Wood, 5 Iron, 3 Copper (ore total: 8)
            return { BuildingType::LAMP, "Осветителна лампа", "Light Tower / Lamp", 4, 5, 3, 0, 0, 0, 8, 0 };
        case BuildingType::DEMOLISH:
            return { BuildingType::DEMOLISH, "Премахване", "Demolish Tool", 0, 0, 0, 0, 0, 0, 0, 0 };
        default:
            return { BuildingType::NONE, "", "", 0, 0, 0, 0, 0, 0, 0, 0 };
    }
}

sf::Vector2f GameEngine::getGridSlot(int player, int col, int row) const {
    // 9 columns (0..8) and 12 rows (0..11) for 12 plots x 9 slots each
    col = std::max(0, std::min(col, 8));
    row = std::max(0, std::min(row, 11));

    int plotC = col / 3;
    int subC = col % 3;
    int plotR = row / 3;
    int subR = row % 3;

    float plotW = 105.0f;
    float plotH = 95.0f;
    float gapX = 12.0f;
    float gapY = 10.0f;
    float startX = (player == 1) ? 258.0f : 1003.0f;
    float startY = 105.0f;

    float plotLeft = startX + plotC * (plotW + gapX);
    float plotTop = startY + plotR * (plotH + gapY);

    float subW = plotW / 3.0f;
    float subH = plotH / 3.0f;

    float cx = plotLeft + (subC + 0.5f) * subW;
    float cy = plotTop + (subR + 0.5f) * subH;
    return sf::Vector2f(cx, cy);
}

void GameEngine::getClosestGridIndex(int player, sf::Vector2f pos, int& outCol, int& outRow) const {
    float bestD2 = 1e12f;
    outCol = 0;
    outRow = 0;
    for (int r = 0; r < 12; ++r) {
        for (int c = 0; c < 9; ++c) {
            sf::Vector2f s = getGridSlot(player, c, r);
            float d2 = (pos.x - s.x) * (pos.x - s.x) + (pos.y - s.y) * (pos.y - s.y);
            if (d2 < bestD2) {
                bestD2 = d2;
                outCol = c;
                outRow = r;
            }
        }
    }
}

sf::Vector2f GameEngine::snapToBuildingGrid(int player, sf::Vector2f pos) const {
    int c = 0, r = 0;
    getClosestGridIndex(player, pos, c, r);
    return getGridSlot(player, c, r);
}

bool GameEngine::isAreaIlluminated(int player, sf::Vector2f pos) const {
    if (isDaylight()) {
        return true;
    }
    // Check if within illuminated radius of any active, powered Lamp owned by player
    for (const auto& b : buildings) {
        if (b.playerOwner == player && b.type == BuildingType::LAMP && b.lightRadius > 0.0f) {
            float dx = b.position.x - pos.x;
            float dy = b.position.y - pos.y;
            if (std::sqrt(dx * dx + dy * dy) <= b.lightRadius) {
                return true;
            }
        }
    }
    return false;
}

bool GameEngine::removeBuilding(int player, sf::Vector2f pos, std::string& outMsg) {
    auto& econ = (player == 1) ? p1 : p2;
    float closestDist = 999999.0f;
    int closestIdx = -1;

    for (size_t i = 0; i < buildings.size(); i++) {
        if (buildings[i].playerOwner == player) {
            float dx = buildings[i].position.x - pos.x;
            float dy = buildings[i].position.y - pos.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < 38.0f && dist < closestDist) {
                closestDist = dist;
                closestIdx = static_cast<int>(i);
            }
        }
    }

    if (closestIdx == -1) {
        outMsg = "НЯМА ВАША СГРАДА ТУК ЗА ПРЕМАХВАНЕ!";
        return false;
    }

    PlacedBuilding b = buildings[closestIdx];
    BuildingCost cost = getBuildingCost(b.type);
    int refundWood = cost.woodCost / 2;
    int refundIron = cost.ironCost / 2;
    int refundCopper = cost.copperCost / 2;
    int refundCoal = cost.coalCost / 2;
    int refundSilicon = cost.siliconCost / 2;
    int refundSilver = cost.silverCost / 2;
    int refundOre = cost.oreCost / 2;

    econ.wood += refundWood;
    econ.iron += refundIron;
    econ.copper += refundCopper;
    econ.coal += refundCoal;
    econ.silicon += refundSilicon;
    econ.silver += refundSilver;
    econ.ore += refundOre;

    econ.data.wood = econ.wood;
    econ.data.iron = econ.iron;
    econ.data.copper = econ.copper;
    econ.data.coal = econ.coal;
    econ.data.silicon = econ.silicon;
    econ.data.silver = econ.silver;

    buildings.erase(buildings.begin() + closestIdx);
    outMsg = "ПРЕМАХНАТ " + cost.nameBg + "! (Върнати: 50% ресурси)";
    return true;
}

bool GameEngine::canPlaceBuilding(int player, BuildingType type, sf::Vector2f pos, std::string& reason) const {
    if (type == BuildingType::NONE) {
        reason = "НЯМА ИЗБРАНА СГРАДА!";
        return false;
    }

    if (type == BuildingType::DEMOLISH) {
        // Demolish tool checks if there is an owned building on this slot
        for (const auto& b : buildings) {
            if (b.playerOwner == player) {
                float dx = b.position.x - pos.x;
                float dy = b.position.y - pos.y;
                if (std::sqrt(dx * dx + dy * dy) < 20.0f) {
                    return true;
                }
            }
        }
        reason = "НЯМА ВАША СГРАДА В ТАЗИ ТОЧКА ЗА ПРЕМАХВАНЕ!";
        return false;
    }

    // Snap to 2x2 grid slot inside land plot
    pos = snapToBuildingGrid(player, pos);

    const auto& econ = (player == 1) ? p1 : p2;
    BuildingCost cost = getBuildingCost(type);

    bool hasRes = (econ.wood >= cost.woodCost);
    if (econ.iron < cost.ironCost && econ.ore < cost.oreCost) hasRes = false;
    if (cost.copperCost > 0 && econ.copper < cost.copperCost && econ.ore < cost.oreCost) hasRes = false;
    if (cost.coalCost > 0 && econ.coal < cost.coalCost && econ.ore < cost.oreCost) hasRes = false;
    if (cost.siliconCost > 0 && econ.silicon < cost.siliconCost && econ.ore < cost.oreCost) hasRes = false;
    if (cost.silverCost > 0 && econ.silver < cost.silverCost && econ.ore < cost.oreCost) hasRes = false;

    if (!hasRes) {
        reason = "НЕДОСТИГ НА РЕСУРСИ! Нужно: " + std::to_string(cost.woodCost) + " Дърво";
        if (cost.ironCost > 0) reason += ", " + std::to_string(cost.ironCost) + " Жел";
        if (cost.copperCost > 0) reason += ", " + std::to_string(cost.copperCost) + " Мед";
        if (cost.siliconCost > 0) reason += ", " + std::to_string(cost.siliconCost) + " Сил";
        if (cost.coalCost > 0) reason += ", " + std::to_string(cost.coalCost) + " Въгл";
        if (cost.silverCost > 0) reason += ", " + std::to_string(cost.silverCost) + " Среб";
        return false;
    }

    // Night Construction Restriction:
    // Players CANNOT build at night unless an active powered Lamp illuminates the area!
    if (!isDaylight()) {
        if (type == BuildingType::LAMP) {
            // Placing a lamp at night is allowed (needed to illuminate the darkness)
        } else {
            if (!isAreaIlluminated(player, pos)) {
                reason = "НОЩЕН МРАК! Строежът нощем е забранен без осветление!\nПоставете и захранете Осветителна лампа, за да работите.";
                return false;
            }
        }
    }

    // Must be inside a PURCHASED plot owned by this player
    bool onPurchasedLand = false;
    for (const auto& plot : landPlots) {
        if (plot.playerOwner == player && plot.bounds.contains(pos)) {
            if (plot.isPurchased) {
                onPurchasedLand = true;
            } else {
                reason = "НЕПРИТЕЖАВАНА ЗЕМЯ! Трябва първо да закупите този парцел с " + std::to_string(plot.costGold) + " G!";
                return false;
            }
            break;
        }
    }

    if (!onPurchasedLand) {
        reason = "МОЖЕТЕ ДА СТРОИТЕ САМО ВЪРХУ ВАША ЗАКУПЕНА ЗЕМЯ!";
        return false;
    }

    // Check collision with other buildings (same slot is blocked, adjacent 3x3 slots are allowed)
    for (const auto& b : buildings) {
        float dx = b.position.x - pos.x;
        float dy = b.position.y - pos.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < 16.0f) {
            reason = "В ТАЗИ КЛЕТКА ВЕЧЕ ИМА СГРАДА! Изберете свободна клетка от грида.";
            return false;
        }
    }

    return true;
}

bool GameEngine::placeBuilding(int player, BuildingType type, sf::Vector2f pos, std::string& outMsg) {
    if (type == BuildingType::DEMOLISH) {
        return removeBuilding(player, pos, outMsg);
    }

    // Snap to 2x2 grid slot inside land plot
    pos = snapToBuildingGrid(player, pos);

    std::string reason;
    if (!canPlaceBuilding(player, type, pos, reason)) {
        outMsg = reason;
        return false;
    }

    auto& econ = (player == 1) ? p1 : p2;
    BuildingCost cost = getBuildingCost(type);
    econ.wood = std::max(0, econ.wood - cost.woodCost);
    econ.iron = std::max(0, econ.iron - cost.ironCost);
    econ.copper = std::max(0, econ.copper - cost.copperCost);
    econ.coal = std::max(0, econ.coal - cost.coalCost);
    econ.silicon = std::max(0, econ.silicon - cost.siliconCost);
    econ.silver = std::max(0, econ.silver - cost.silverCost);
    if (econ.ore >= cost.oreCost) econ.ore -= cost.oreCost;

    econ.data.wood = econ.wood;
    econ.data.iron = econ.iron;
    econ.data.copper = econ.copper;
    econ.data.coal = econ.coal;
    econ.data.silicon = econ.silicon;
    econ.data.silver = econ.silver;

    // Remembers lastly placed building for instant reuse!
    econ.lastPlacedBuilding = static_cast<int>(type);

    PlacedBuilding b;
    b.type = type;
    b.position = pos;
    b.playerOwner = player;
    b.currentOutputMW = static_cast<float>(cost.basePowerMW);
    b.animTimer = 0.0f;
    // Spawns with 0% battery charge as requested!
    b.energyStored = 0.0f;
    b.maxCapacity = 200.0f;
    b.lightRadius = (type == BuildingType::LAMP) ? 150.0f : 0.0f;
    buildings.push_back(b);

    if (type == BuildingType::LAMP) {
        outMsg = "ПОСТАВЕНА Осветителна лампа! (Консумира 10 MW, осветява 150 px)";
    } else if (type == BuildingType::BATTERY) {
        outMsg = "ПОСТРОЕН Акумулатор! (0% заряд. Зарежда се от произведената чиста енергия)";
    } else {
        outMsg = "ПОСТРОЕН " + cost.nameBg + "! (+" + std::to_string(cost.basePowerMW) + " MW)";
    }
    return true;
}
