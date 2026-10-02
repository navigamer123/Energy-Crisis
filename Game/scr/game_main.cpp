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
    // Land dimensions strictly positioned so they NEVER touch or go under the city!
    // City bounds: X in [610.0, 990.0].
    // -------------------------------------------------------------------------
    float plotW = 105.0f;
    float plotH = 95.0f;
    float gap = 12.0f;

    // West Side (P1): 3 columns x 3 rows (Col 0: 258-363, Col 1: 375-480, Col 2: 492-597 < 610)
    float westStartX = 258.0f;
    float startY = 120.0f;
    int idCounter = 1;

    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            float x = westStartX + c * (plotW + gap);
            float y = startY + r * (plotH + gap);

            LandPlot plot;
            plot.id = idCounter++;
            plot.playerOwner = 1;
            plot.bounds = sf::FloatRect({ x, y }, { plotW, plotH });
            // Starting central plot is unlocked, others are purchasable
            plot.isPurchased = (r == 0 && c == 0);
            plot.costGold = 180 + (r * 3 + c) * 60;
            landPlots.push_back(plot);
        }
    }

    // East Side (P2): 3 columns x 3 rows (Col 0: 1003-1108 > 990, Col 1: 1120-1225, Col 2: 1237-1342 < 1360)
    float eastStartX = 1003.0f;
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            float x = eastStartX + c * (plotW + gap);
            float y = startY + r * (plotH + gap);

            LandPlot plot;
            plot.id = idCounter++;
            plot.playerOwner = 2;
            plot.bounds = sf::FloatRect({ x, y }, { plotW, plotH });
            // Starting plot is unlocked, others are purchasable
            plot.isPurchased = (r == 0 && c == 2);
            plot.costGold = 180 + (r * 3 + c) * 60;
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

    // Passive revenue from delivered power
    static float revenueTimer = 0.0f;
    revenueTimer += effectiveDt;
    if (revenueTimer >= 1.0f) {
        revenueTimer = 0.0f;
        p1.gold += std::max(1, static_cast<int>(p1.energyMW * 0.05f));
        p2.gold += std::max(1, static_cast<int>(p2.energyMW * 0.05f));
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

    if (type == ResourceType::ORE) {
        // Mine expedition (cave) from weatherF branch
        auto res = Expedition(1, "cave");
        // res = {silicon, copper, silver, iron, gold, coal};
        int silicon = (res.size() > 0) ? res[0] : 6;
        int copper  = (res.size() > 1) ? res[1] : 10;
        int silver  = (res.size() > 2) ? res[2] : 4;
        int iron    = (res.size() > 3) ? res[3] : 12;
        int gold    = (res.size() > 4) ? res[4] : 4;
        int coal    = (res.size() > 5) ? res[5] : 15;

        // Combine all extractable metal ores into Ore + Gold currency
        int totalOre = iron + copper + silicon + silver;
        if (totalOre < 16) totalOre = 16 + (rand() % 12);
        int totalGold = gold;
        if (totalGold < 2) totalGold = 3 + (rand() % 4);

        econ.ore += totalOre;
        econ.gold += totalGold;

        econ.data.iron += iron;
        econ.data.copper += copper;
        econ.data.silicon += silicon;
        econ.data.silver += silver;
        econ.data.gold += totalGold;
        econ.data.coal += coal;

        result.ore = totalOre;
        result.gold = totalGold;
        result.coal = coal;

        outMsg = "+" + std::to_string(totalOre) + " Руда, +" + std::to_string(totalGold) + " Злато";
        return true;
    } else if (type == ResourceType::WOOD) {
        // Forest expedition from weatherF branch
        auto res = Expedition(1, "forest");
        // res = {wood, sticks};
        int rawWood = (res.size() > 0) ? res[0] : 25;
        int sticks  = (res.size() > 1) ? res[1] : 30;

        // Balanced wood yield per action: 20-35
        int totalWood = 20 + (rawWood % 18);
        econ.wood += totalWood;
        econ.data.wood += totalWood;
        econ.data.sticks += sticks;

        result.wood = totalWood;

        outMsg = "+" + std::to_string(totalWood) + " Дървесина";
        return true;
    }
    return false;
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
    if (econ.selectedBuilding < 1 || econ.selectedBuilding >= 6) {
        econ.selectedBuilding = 1;
    } else {
        econ.selectedBuilding++;
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
            return { BuildingType::SOLAR_PANEL, "Слънчев панел", "Solar Panel", 35, 30, 60 };
        case BuildingType::WIND_TURBINE:
            return { BuildingType::WIND_TURBINE, "Вятърна мелница", "Wind Turbine", 50, 45, 90 };
        case BuildingType::HYDRO_PLANT:
            return { BuildingType::HYDRO_PLANT, "ВЕЦ / Хидро", "Hydro Plant", 85, 90, 180 };
        case BuildingType::BATTERY:
            return { BuildingType::BATTERY, "Батерия / Акумулатор", "Battery Storage", 30, 60, 40 };
        case BuildingType::LAMP:
            return { BuildingType::LAMP, "Осветителна лампа", "Light Tower / Lamp", 15, 10, 0 };
        case BuildingType::DEMOLISH:
            return { BuildingType::DEMOLISH, "Премахване", "Demolish Tool", 0, 0, 0 };
        default:
            return { BuildingType::NONE, "", "", 0, 0, 0 };
    }
}

sf::Vector2f GameEngine::snapToBuildingGrid(int player, sf::Vector2f pos) const {
    const LandPlot* targetPlot = nullptr;
    float bestDistSq = 9999999.0f;

    // 1. Check if pos is strictly inside any plot owned by this player
    for (const auto& plot : landPlots) {
        if (plot.playerOwner == player && plot.bounds.contains(pos)) {
            targetPlot = &plot;
            break;
        }
    }

    // 2. If not directly inside, find closest plot owned by player within 120px
    if (!targetPlot) {
        for (const auto& plot : landPlots) {
            if (plot.playerOwner == player) {
                float cx = plot.bounds.position.x + plot.bounds.size.x * 0.5f;
                float cy = plot.bounds.position.y + plot.bounds.size.y * 0.5f;
                float d2 = (pos.x - cx) * (pos.x - cx) + (pos.y - cy) * (pos.y - cy);
                if (d2 < bestDistSq && d2 < (130.0f * 130.0f)) {
                    bestDistSq = d2;
                    targetPlot = &plot;
                }
            }
        }
    }

    // 3. If a target plot is found, snap to its 2x2 grid slots
    if (targetPlot) {
        float left = targetPlot->bounds.position.x;
        float top = targetPlot->bounds.position.y;
        float colW = targetPlot->bounds.size.x * 0.5f; // 52.5f
        float rowH = targetPlot->bounds.size.y * 0.5f; // 47.5f

        int col = (pos.x >= left + colW) ? 1 : 0;
        int row = (pos.y >= top + rowH) ? 1 : 0;

        float snapX = left + (col + 0.5f) * colW;
        float snapY = top + (row + 0.5f) * rowH;
        return sf::Vector2f(snapX, snapY);
    }

    return pos;
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
    int refundOre = cost.oreCost / 2;

    econ.wood += refundWood;
    econ.ore += refundOre;

    buildings.erase(buildings.begin() + closestIdx);
    outMsg = "ПРЕМАХНАТ " + cost.nameBg + "! (Върнати: +" + std::to_string(refundWood) + " Дърво, +" + std::to_string(refundOre) + " Руда)";
    return true;
}

bool GameEngine::canPlaceBuilding(int player, BuildingType type, sf::Vector2f pos, std::string& reason) const {
    if (type == BuildingType::NONE) {
        reason = "НЯМА ИЗБРАНА СГРАДА!";
        return false;
    }

    if (type == BuildingType::DEMOLISH) {
        // Demolish tool checks if there is an owned building within reach
        for (const auto& b : buildings) {
            if (b.playerOwner == player) {
                float dx = b.position.x - pos.x;
                float dy = b.position.y - pos.y;
                if (std::sqrt(dx * dx + dy * dy) < 38.0f) {
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
    if (econ.wood < cost.woodCost || econ.ore < cost.oreCost) {
        reason = "НЕДОСТИГ НА РЕСУРСИ! (Нужно: " + std::to_string(cost.woodCost) + " Дърво, " + std::to_string(cost.oreCost) + " Руда)";
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

    // Check collision with other buildings
    for (const auto& b : buildings) {
        float dx = b.position.x - pos.x;
        float dy = b.position.y - pos.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < 10.0f) {
            reason = "В ТАЗИ КЛЕТКА ВЕЧЕ ИМА СГРАДА! Изберете свободна клетка от грида.";
            return false;
        } else if (dist < 28.0f) {
            reason = "ТВЪРДЕ БЛИЗО ДО ДРУГА СГРАДА!";
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
    econ.wood -= cost.woodCost;
    econ.ore -= cost.oreCost;

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
