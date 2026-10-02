#include "../includes/game_main.h"
#include <cmath>
#include <iostream>
#include <algorithm>

GameEngine::GameEngine()
    : gameSeconds(0.0f),
      currentDay(1),
      hour24(8.0f),
      secondsPerDay(60.0f), // 1 real minute = 1 full day
      p1Weather(WeatherType::SUNNY),
      p2Weather(WeatherType::WINDY),
      currentSeason(SeasonType::SPRING) {
}

void GameEngine::init(float screenWidth, float screenHeight) {
    (void)screenWidth;
    (void)screenHeight;
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
    gameSeconds += dt;
    float prevHour = hour24;
    hour24 = std::fmod((gameSeconds / secondsPerDay) * 24.0f + 6.0f, 24.0f);

    int calculatedDay = 1 + static_cast<int>(gameSeconds / secondsPerDay);
    if (calculatedDay > currentDay || (prevHour > 23.0f && hour24 < 1.0f)) {
        currentDay = calculatedDay;
        processDayEnd();
    }

    // Update building energy outputs based on real-time continuous weather & sun
    updateBuildingsEnergy(dt);

    // Passive revenue from delivered power
    static float revenueTimer = 0.0f;
    revenueTimer += dt;
    if (revenueTimer >= 1.0f) {
        revenueTimer = 0.0f;
        p1.gold += std::max(1, static_cast<int>(p1.energyMW * 0.05f));
        p2.gold += std::max(1, static_cast<int>(p2.energyMW * 0.05f));
    }
}

void GameEngine::updateBuildingsEnergy(float dt) {
    (void)dt;
    float p1Total = 0.0f;
    float p2Total = 0.0f;

    for (auto& b : buildings) {
        b.animTimer += dt;
        BuildingCost cost = getBuildingCost(b.type);
        WeatherType w = (b.playerOwner == 1) ? p1Weather : p2Weather;

        float output = 0.0f;
        switch (b.type) {
            case BuildingType::SOLAR_PANEL:
                output = cost.basePowerMW * WeatherSystem::getSolarMultiplier(w, hour24);
                break;
            case BuildingType::WIND_TURBINE:
                output = cost.basePowerMW * WeatherSystem::getWindMultiplier(w, hour24);
                break;
            case BuildingType::HYDRO_PLANT:
                output = cost.basePowerMW * WeatherSystem::getHydroMultiplier(w);
                break;
            case BuildingType::BATTERY: {
                // Daytime: charge battery from surplus power
                if (hour24 >= 6.0f && hour24 <= 18.0f) {
                    float chargeRate = 35.0f * dt;
                    b.energyStored = std::min(b.maxCapacity, b.energyStored + chargeRate);
                    output = 0.0f; // Storing clean energy
                } else {
                    // Nighttime: discharge stored energy to provide steady electricity!
                    if (b.energyStored > 0.0f) {
                        float maxDischarge = static_cast<float>(cost.basePowerMW);
                        float discharge = std::min(maxDischarge, b.energyStored * 0.8f);
                        output = discharge;
                        b.energyStored = std::max(0.0f, b.energyStored - (output * 0.12f * dt));
                    } else {
                        output = 0.0f; // Depleted
                    }
                }
                break;
            }
            case BuildingType::LAMP:
                output = 0.0f; // Lamps consume negligible power and illuminate
                b.lightRadius = 150.0f;
                break;
            case BuildingType::DEMOLISH:
                output = 0.0f;
                break;
            default:
                break;
        }

        b.currentOutputMW = output;
        if (b.playerOwner == 1) p1Total += output;
        else p2Total += output;
    }

    p1.energyMW = static_cast<int>(p1Total);
    p2.energyMW = static_cast<int>(p2Total);

    // Track daily delivery
    city.p1DailyDelivered += p1Total * dt * 0.1f;
    city.p2DailyDelivered += p2Total * dt * 0.1f;
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
    auto& econ = (player == 1) ? p1 : p2;
    if (type == ResourceType::ORE) {
        // Mine expedition (cave) from weatherF branch
        auto res = Expedition(1, "cave");
        // res = {silicon, copper, silver, iron, gold, coal};
        int iron = (res.size() > 3) ? res[3] : 15;
        int gold = (res.size() > 4) ? res[4] : 5;
        int coal = (res.size() > 5) ? res[5] : 20;

        econ.ore += iron;
        econ.gold += gold;
        econ.data.iron += iron;
        econ.data.gold += gold;
        econ.data.coal += coal;

        outMsg = (player == 1 ? "ИГРАЧ 1: +" + std::to_string(iron) + " РУДА, +" + std::to_string(gold) + "G (ПЕЩЕРА)"
                              : "ИГРАЧ 2: +" + std::to_string(iron) + " РУДА, +" + std::to_string(gold) + "G (ПЕЩЕРА)");
        return true;
    } else if (type == ResourceType::WOOD) {
        // Forest expedition from weatherF branch
        auto res = Expedition(1, "forest");
        // res = {wood, sticks};
        int wood = (res.size() > 0) ? res[0] : 20;
        int sticks = (res.size() > 1) ? res[1] : 30;

        econ.wood += wood;
        econ.data.wood += wood;
        econ.data.sticks += sticks;

        outMsg = (player == 1 ? "ИГРАЧ 1: +" + std::to_string(wood) + " ДЪРВО (ГОРА)"
                              : "ИГРАЧ 2: +" + std::to_string(wood) + " ДЪРВО (ГОРА)");
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

bool GameEngine::isAreaIlluminated(int player, sf::Vector2f pos) const {
    if (isDaylight()) {
        return true;
    }
    // Check if within illuminated radius of any Lamp owned by player
    for (const auto& b : buildings) {
        if (b.playerOwner == player && b.type == BuildingType::LAMP) {
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

    const auto& econ = (player == 1) ? p1 : p2;
    BuildingCost cost = getBuildingCost(type);
    if (econ.wood < cost.woodCost || econ.ore < cost.oreCost) {
        reason = "НЕДОСТИГ НА РЕСУРСИ! (Нужно: " + std::to_string(cost.woodCost) + " Дърво, " + std::to_string(cost.oreCost) + " Руда)";
        return false;
    }

    // Night Construction Restriction:
    // Players CANNOT build at night without a Lamp illuminating the area!
    if (!isDaylight() && type != BuildingType::LAMP) {
        if (!isAreaIlluminated(player, pos)) {
            reason = "НОЩЕН МРАК! В тъмнината строителите не виждат.\nПоставете Лампа за осветление!";
            return false;
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
        if (std::sqrt(dx * dx + dy * dy) < 28.0f) {
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
    b.energyStored = (type == BuildingType::BATTERY) ? 60.0f : 0.0f; // Initial partial charge for newly placed battery
    b.maxCapacity = 200.0f;
    b.lightRadius = (type == BuildingType::LAMP) ? 150.0f : 0.0f;
    buildings.push_back(b);

    if (type == BuildingType::LAMP) {
        outMsg = "ПОСТАВЕНА Осветителна лампа! (Осветява нощем в радиус 150 px)";
    } else {
        outMsg = "ПОСТРОЕН " + cost.nameBg + "! (+" + std::to_string(cost.basePowerMW) + " MW)";
    }
    return true;
}
