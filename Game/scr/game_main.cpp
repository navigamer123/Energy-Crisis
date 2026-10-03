#include "../includes/game_main.h"
#include <cmath>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <cstdlib>

namespace {

// Largest simulation sub-step (game-seconds). Big frames (time acceleration, test harnesses)
// are split so energy, batteries, payouts and day-ends do not depend on the frame size.
constexpr float MAX_SIM_STEP_SEC = 0.25f;

// Match seed: EC_SEED environment variable when set (reproducible matches / tests), else the clock
unsigned int pickMatchSeed(bool& fromEnv) {
    fromEnv = false;
    if (const char* env = std::getenv("EC_SEED")) {
        char* end = nullptr;
        unsigned long value = std::strtoul(env, &end, 10);
        if (end != env && *end == '\0') {
            fromEnv = true;
            return static_cast<unsigned int>(value);
        }
        std::cerr << "[GameEngine] EC_SEED=\"" << env << "\" is not a number; using a clock seed.\n";
    }
    static unsigned int initCounter = 0; // keeps two engines created in the same clock tick apart
    unsigned long long t = static_cast<unsigned long long>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    return static_cast<unsigned int>(t ^ (t >> 32)) + 0x9E3779B9u * (++initCounter);
}

} // namespace

GameEngine::GameEngine()
    : gameSeconds(Balance::gameSecondsAtHour(Balance::MATCH_START_HOUR)),
      currentDay(1),
      hour24(Balance::MATCH_START_HOUR),
      revenueTimer(0.0f),
      p1Weather(WeatherType::SUNNY),
      p2Weather(WeatherType::WINDY),
      currentSeason(SeasonType::SPRING),
      timeScale(1.0f) {
}

void GameEngine::init(float screenWidth, float screenHeight) {
    (void)screenWidth;
    (void)screenHeight;

    // Reset EVERY piece of match state (clock, season, revenue timer, city, players, buildings)
    *this = GameEngine();

    // Seed both the weather RNG and std::rand (lightning, bot, particles) once per match
    bool seedFromEnv = false;
    unsigned int seed = pickMatchSeed(seedFromEnv);
    seedRandom(seed);
    std::srand(seed);
    std::cout << "[GameEngine] RNG seed: " << seed << (seedFromEnv ? " (from EC_SEED)" : " (from clock)")
              << ". Set EC_SEED=" << seed << " to replay this match.\n";

    landPlots.clear();
    buildings.clear();

    // Reset City State on fresh game or restart (First 2 days are Grace Period = 0 MW)
    city = CityConquestState();
    city.cityEnergyDemand = (currentDay <= Balance::GRACE_PERIOD_DAYS) ? 0 : Balance::STARTING_CITY_DEMAND_MW;
    city.p1CityShare = 0.50f;
    city.winner = 0;
    city.lastCutMessage = "ДОБРЕ ДОШЛИ! ГРАТИСЕН ПЕРИОД: ПЪРВИТЕ 2 ДЕНА ГРАДЪТ ИСКА 0 ЕНЕРГИЯ ЗА РАЗВИТИЕ!";

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
            plot.costGold = Balance::getLandPlotCost(r, c);
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
            // Prices mirror P1's around the river: P2's start column (c == 2) is the cheapest tier
            plot.costGold = Balance::getLandPlotCost(r, 2 - c);
            landPlots.push_back(plot);
        }
    }

    // Clear and reset players
    buildings.clear();
    p1 = PlayerEconomy();
    p2 = PlayerEconomy();

    // Players start with 0 buildings and 0 MW (buildings must be earned and constructed)
    buildings.clear();

    // Initialize day 1 weather (season of day 1 = spring) with weather_report from weatherF
    currentSeason = Balance::getSeasonForDay(currentDay);
    rollDailyWeather();

    // Initial update of building energies
    updateBuildingsEnergy(0.0f);
    p1.cityInfluence = 0.50f;
    p2.cityInfluence = 0.50f;

    std::cout << "[GameEngine] Backend initialized with " << landPlots.size() << " land plots & "
              << buildings.size() << " starter buildings.\n";
}

void GameEngine::rollDailyWeather() {
    std::string sName = Balance::getSeasonWeatherKey(currentSeason);

    auto rep1 = weather_report(sName);
    p1Weather = WeatherSystem::reportToWeatherType(rep1);
    p1.data.weather = weather_state;
    p1.data.wind_speed = (rep1.size() > 3) ? rep1[3] : "0";

    auto rep2 = weather_report(sName);
    p2Weather = WeatherSystem::reportToWeatherType(rep2);
    p2.data.weather = weather_state;
    p2.data.wind_speed = (rep2.size() > 3) ? rep2[3] : "0";
}

void GameEngine::update(float dt) {
    // If the match is concluded, freeze all simulation, economy and dividends
    if (city.winner != 0) {
        return;
    }

    float remaining = dt * timeScale;
    while (remaining > 0.0f && city.winner == 0) {
        float step = std::min(remaining, MAX_SIM_STEP_SEC);

        // Never let a step cross the 06:00 day boundary: the day that ends is settled exactly once,
        // with exactly the energy delivered during that day, however large the frame is.
        float dayEndSeconds = static_cast<float>(currentDay) * Balance::SECONDS_PER_DAY;
        bool reachesDayEnd = (gameSeconds + step >= dayEndSeconds);
        if (reachesDayEnd) {
            step = std::max(0.0f, dayEndSeconds - gameSeconds);
        }

        simulateStep(step);
        remaining -= step;

        if (reachesDayEnd) {
            gameSeconds = dayEndSeconds; // avoid float drift at the boundary
            ++currentDay;
            processDayEnd();
        }
    }
}

void GameEngine::simulateStep(float dt) {
    gameSeconds += dt;
    hour24 = std::fmod((gameSeconds / Balance::SECONDS_PER_DAY) * 24.0f + Balance::CLOCK_HOUR_AT_ZERO, 24.0f);
    // Season flips at midnight (dark in every season), never at the 06:00 rollover
    currentSeason = Balance::getSeasonAtGameSeconds(gameSeconds);

    // Update building energy outputs based on real-time continuous weather & sun
    updateBuildingsEnergy(dt);
    city.dailySeconds += dt;

    // Percentage-based city energy revenue, paid once per full game-second (remainder carried over)
    revenueTimer += dt;
    while (revenueTimer >= 1.0f) {
        revenueTimer -= 1.0f;
        payCityRevenue();
    }
}

void GameEngine::payCityRevenue() {
    float totalGrid = static_cast<float>(p1.energyMW + p2.energyMW);
    if (totalGrid > 0.0f) {
        float p1Share = static_cast<float>(p1.energyMW) / totalGrid;
        float p2Share = static_cast<float>(p2.energyMW) / totalGrid;

        // City energy contract pool from central Balance formula
        int contractPool = Balance::calculateContractPool(totalGrid);
        int p1Payout = Balance::calculatePlayerPayout(contractPool, p1Share);
        int p2Payout = Balance::calculatePlayerPayout(contractPool, p2Share);

        p1.money += p1Payout;
        p1.data.money = p1.money;

        p2.money += p2Payout;
        p2.data.money = p2.money;

        // Gold dividend for sustained power supply from Balance formula (capped by city demand)
        p1.gold += Balance::calculateGoldDividend(p1.energyMW, city.cityEnergyDemand);
        p1.data.gold = p1.gold;

        p2.gold += Balance::calculateGoldDividend(p2.energyMW, city.cityEnergyDemand);
        p2.data.gold = p2.gold;
    }

    // City influence reflects established territorial division from daily outcomes
    p1.cityInfluence = city.p1CityShare;
    p2.cityInfluence = 1.0f - city.p1CityShare;
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
    // Battery energy uses ONE unit for charge and discharge: MW x game-hours (MWh)
    // -------------------------------------------------------------------------
    const float stepHours = Balance::gameSecondsToHours(dt);
    auto processPlayerGrid = [&](int player, WeatherType w, PlayerEconomy& econ, float& dailyDelivered) {
        float rawGen = 0.0f;
        std::vector<PlacedBuilding*> playerLamps;
        std::vector<PlacedBuilding*> playerBatteries;

        // Step A: Calculate pure generation from Solar, Wind, and Hydro
        for (auto& b : buildings) {
            if (b.playerOwner != player) continue;
            BuildingCost cost = getBuildingCost(b.type);

            if (b.type == BuildingType::SOLAR_PANEL) {
                float out = cost.basePowerMW * WeatherSystem::getSolarMultiplier(w, hour24, currentSeason);
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

        // Step B: Load the player is trying to serve: own lamps first (10 MW each), then the city demand
        float lampDemand = playerLamps.size() * LAMP_POWER_MW;
        float cityTarget = static_cast<float>(std::max(0, city.cityEnergyDemand));
        float loadTarget = lampDemand + cityTarget;

        for (auto* bat : playerBatteries) {
            bat->currentOutputMW = 0.0f;
        }

        float batteryDischarge = 0.0f; // MW taken out of batteries this step
        float batteryCharge = 0.0f;    // MW put into batteries this step (NOT delivered to the city)

        if (stepHours > 0.0f && !playerBatteries.empty()) {
            if (rawGen < loadTarget) {
                // Step C: Shortfall -> batteries discharge to cover it (never more than needed,
                // never more than BATTERY_MAX_POWER_MW each, never more than they hold)
                float deficit = loadTarget - rawGen;
                float canGive = 0.0f;
                for (auto* bat : playerBatteries) {
                    canGive += std::min(Balance::BATTERY_MAX_POWER_MW, bat->energyStored / stepHours);
                }
                batteryDischarge = std::min(deficit, canGive);
                if (batteryDischarge > 0.0f) {
                    float fraction = batteryDischarge / canGive;
                    for (auto* bat : playerBatteries) {
                        float power = std::min(Balance::BATTERY_MAX_POWER_MW, bat->energyStored / stepHours) * fraction;
                        bat->currentOutputMW = power;
                        bat->energyStored = std::max(0.0f, bat->energyStored - power * stepHours);
                    }
                }
            } else if (rawGen > loadTarget) {
                // Step D: Real surplus only -> batteries charge (never from thin air)
                float surplus = rawGen - loadTarget;
                float canTake = 0.0f;
                for (auto* bat : playerBatteries) {
                    canTake += std::min(Balance::BATTERY_MAX_POWER_MW,
                                        std::max(0.0f, bat->maxCapacity - bat->energyStored) / stepHours);
                }
                batteryCharge = std::min(surplus, canTake);
                if (batteryCharge > 0.0f) {
                    float fraction = batteryCharge / canTake;
                    for (auto* bat : playerBatteries) {
                        float room = std::max(0.0f, bat->maxCapacity - bat->energyStored);
                        float power = std::min(Balance::BATTERY_MAX_POWER_MW, room / stepHours) * fraction;
                        bat->energyStored = std::min(bat->maxCapacity, bat->energyStored + power * stepHours);
                    }
                }
            }
        }

        // Step E: Power lamps first from generation + discharge (minus what went into storage)
        float totalAvailable = std::max(0.0f, rawGen + batteryDischarge - batteryCharge);
        int poweredCount = static_cast<int>((totalAvailable + 0.001f) / LAMP_POWER_MW);
        poweredCount = std::min(static_cast<int>(playerLamps.size()), poweredCount);
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

        // Step F: Whatever is left goes to the city
        float netPlayerOutput = std::max(0.0f, totalAvailable - poweredCount * LAMP_POWER_MW);
        econ.energyMW = static_cast<int>(std::lround(netPlayerOutput));
        dailyDelivered += netPlayerOutput * dt; // MW x game-seconds, averaged at the day end
    };

    processPlayerGrid(1, p1Weather, p1, city.p1DailyDelivered);
    processPlayerGrid(2, p2Weather, p2, city.p2DailyDelivered);
}

void GameEngine::processDayEnd() {
    int endedDay = currentDay - 1;
    city.dayCutOccurred = true;

    // The day is judged on the AVERAGE power delivered over the whole day (06:00 -> 06:00),
    // not on an instantaneous snapshot, so daytime-only sources (solar) count fully.
    // (+0.01 MW only absorbs float error, e.g. a battery covering exactly the demand all night)
    float daySeconds = (city.dailySeconds > 0.0f) ? city.dailySeconds : Balance::SECONDS_PER_DAY;
    int p1AvgMW = static_cast<int>(std::floor(city.p1DailyDelivered / daySeconds + 0.01f));
    int p2AvgMW = static_cast<int>(std::floor(city.p2DailyDelivered / daySeconds + 0.01f));

    // [b-showcase] Record the verdict for the UI (blackout set piece, living skyline)
    lastSettlement.serial++;
    lastSettlement.day = endedDay;
    lastSettlement.graceDay = (endedDay <= Balance::GRACE_PERIOD_DAYS);
    lastSettlement.demandMW = lastSettlement.graceDay ? 0 : city.cityEnergyDemand;
    lastSettlement.p1AvgMW = p1AvgMW;
    lastSettlement.p2AvgMW = p2AvgMW;
    lastSettlement.p1Failed = !lastSettlement.graceDay && p1AvgMW < city.cityEnergyDemand;
    lastSettlement.p2Failed = !lastSettlement.graceDay && p2AvgMW < city.cityEnergyDemand;
    const float shareBefore = city.p1CityShare;

    if (endedDay <= Balance::GRACE_PERIOD_DAYS) {
        // Grace period for the first 2 days: 0 energy demanded, no penalties or cuts
        if (endedDay == 1) {
            city.lastCutMessage = "ДЕН 1 ПРИКЛЮЧИ [ГРАТИСЕН ПЕРИОД]: ГРАДЪТ ИСКАШЕ 0 MW. ОЩЕ 1 ДЕН ЗА РАЗВИТИЕ!";
        } else {
            city.lastCutMessage = "ДЕН 2 ПРИКЛЮЧИ: КРАЙ НА ГРАТИСНИЯ ПЕРИОД! ОТ ДЕН 3 ГРАДЪТ ИЗИСКВА ЕНЕРГИЯ!";
        }
    } else {
        bool p1Succeeded = (p1AvgMW >= city.cityEnergyDemand);
        bool p2Succeeded = (p2AvgMW >= city.cityEnergyDemand);
        float shift = Balance::calculateDailyCityShift(p1AvgMW, p2AvgMW, city.cityEnergyDemand);
        city.p1CityShare = std::clamp(city.p1CityShare + shift, 0.0f, 1.0f);
        int shiftPct = static_cast<int>(std::round(std::abs(shift) * 100.0f));

        if (p1Succeeded && !p2Succeeded) {
            city.lastCutMessage = "ДЕН " + std::to_string(endedDay) + ": ИГРАЧ 1 ЗАХРАНИ ГРАДА (СРЕДНО " +
                                  std::to_string(p1AvgMW) + "/" + std::to_string(city.cityEnergyDemand) + " MW) И ВЗЕ +" +
                                  std::to_string(shiftPct) + "% ТЕРИТОРИЯ!";
        } else if (p2Succeeded && !p1Succeeded) {
            city.lastCutMessage = "ДЕН " + std::to_string(endedDay) + ": ИГРАЧ 2 ЗАХРАНИ ГРАДА (СРЕДНО " +
                                  std::to_string(p2AvgMW) + "/" + std::to_string(city.cityEnergyDemand) + " MW) И ВЗЕ +" +
                                  std::to_string(shiftPct) + "% ТЕРИТОРИЯ!";
        } else if (p1Succeeded && p2Succeeded) {
            city.lastCutMessage = "ДЕН " + std::to_string(endedDay) + ": И ДВАМАТА ЗАХРАНИХА ГРАДА! НИТО ЕДИН НЕ ГУБИ ТЕРИТОРИЯ (0% ПРОМЯНА)!";
        } else {
            city.lastCutMessage = "ДЕН " + std::to_string(endedDay) + ": НИТО ЕДИН НЕ ЗАХРАНИ ГРАДА (" +
                                  std::to_string(city.cityEnergyDemand) + " MW)! НЯМА ПРОМЯНА В ТЕРИТОРИЯТА!";
        }

        // Check Victory Conditions only at day end
        const float eps = 1e-4f; // tolerate float error from summed daily shifts
        float p1Share = city.p1CityShare;
        float p2Share = 1.0f - p1Share;
        int p1Pct = static_cast<int>(std::lround(p1Share * 100.0f));
        int p2Pct = 100 - p1Pct;
        if (p1Share >= Balance::VICTORY_SHARE - eps) {
            city.winner = 1;
            city.lastCutMessage = "ПОБЕДА ЗА ИГРАЧ 1! КОНТРОЛИРА " + std::to_string(p1Pct) + "% ОТ ГРАДА (НУЖНИ СА " +
                                  std::to_string(static_cast<int>(std::lround(Balance::VICTORY_SHARE * 100.0f))) + "%)!";
        } else if (p2Share >= Balance::VICTORY_SHARE - eps) {
            city.winner = 2;
            city.lastCutMessage = "ПОБЕДА ЗА ИГРАЧ 2! КОНТРОЛИРА " + std::to_string(p2Pct) + "% ОТ ГРАДА (НУЖНИ СА " +
                                  std::to_string(static_cast<int>(std::lround(Balance::VICTORY_SHARE * 100.0f))) + "%)!";
        } else if (endedDay >= Balance::FINAL_DAY) {
            std::string head = "КРАЙ НА ДЕН " + std::to_string(endedDay) + "! ";
            if (std::abs(p1Share - 0.5f) < Balance::DRAW_SHARE_TOLERANCE) {
                city.winner = 3;
                city.lastCutMessage = head + "РАВЕНСТВО - ГРАДЪТ Е РАЗДЕЛЕН ПОРАВНО (" + std::to_string(p1Pct) + "% / " +
                                      std::to_string(p2Pct) + "%)!";
            } else if (p1Share > 0.5f) {
                city.winner = 1;
                city.lastCutMessage = head + "ПОБЕДА ЗА ИГРАЧ 1 С " + std::to_string(p1Pct) + "% ОТ ГРАДА!";
            } else {
                city.winner = 2;
                city.lastCutMessage = head + "ПОБЕДА ЗА ИГРАЧ 2 С " + std::to_string(p2Pct) + "% ОТ ГРАДА!";
            }
        }
    }

    p1.cityInfluence = city.p1CityShare;
    p2.cityInfluence = 1.0f - city.p1CityShare;
    lastSettlement.shareShift = city.p1CityShare - shareBefore; // [b-showcase]

    // City expands and demands power next day (0 MW for first 2 days grace, 30 MW Day 3, +15 MW daily)
    if (currentDay <= Balance::GRACE_PERIOD_DAYS) {
        city.cityEnergyDemand = 0;
    } else if (currentDay == Balance::GRACE_PERIOD_DAYS + 1) {
        city.cityEnergyDemand = Balance::STARTING_CITY_DEMAND_MW;
    } else {
        city.cityEnergyDemand += Balance::DAILY_DEMAND_INCREASE_MW;
    }
    city.p1DailyDelivered = 0.0f;
    city.p2DailyDelivered = 0.0f;
    city.dailySeconds = 0.0f;

    // Daily dynamic weather generation using weather_report from weatherF.
    // The new day's season already took effect at the preceding midnight.
    currentSeason = Balance::getSeasonForDay(currentDay);
    rollDailyWeather();
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
    float mult = Balance::getMineYieldMultiplier(lvl);
    std::string lvlTag = (lvl > 1 ? " [НИВО " + std::to_string(lvl) + "]" : "");

    switch (type) {
        case ResourceType::WOOD: {
            int amount = static_cast<int>(std::round(Balance::WOOD_BASE_YIELD * mult));
            econ.wood += amount;
            econ.data.wood = econ.wood;
            result.wood = amount;
            result.amount = amount;
            outMsg = "+" + std::to_string(amount) + " Дървесина (Гора)" + lvlTag;
            return true;
        }
        case ResourceType::IRON: {
            int amount = static_cast<int>(std::round(Balance::IRON_BASE_YIELD * mult));
            econ.iron += amount;
            econ.data.iron = econ.iron;
            result.iron = amount;
            result.amount = amount;
            outMsg = "+" + std::to_string(amount) + " Желязо (Желязна мина)" + lvlTag;
            return true;
        }
        case ResourceType::COPPER: {
            int amount = static_cast<int>(std::round(Balance::COPPER_BASE_YIELD * mult));
            econ.copper += amount;
            econ.data.copper = econ.copper;
            result.copper = amount;
            result.amount = amount;
            outMsg = "+" + std::to_string(amount) + " Мед (Медна жила)" + lvlTag;
            return true;
        }
        case ResourceType::COAL: {
            int amount = static_cast<int>(std::round(Balance::COAL_BASE_YIELD * mult));
            econ.coal += amount;
            econ.data.coal = econ.coal;
            result.coal = amount;
            result.amount = amount;
            outMsg = "+" + std::to_string(amount) + " Въглища (Въглищен пласт)" + lvlTag;
            return true;
        }
        case ResourceType::SILICON: {
            int amount = static_cast<int>(std::round(Balance::SILICON_BASE_YIELD * mult));
            econ.silicon += amount;
            econ.data.silicon = econ.silicon;
            result.silicon = amount;
            result.amount = amount;
            outMsg = "+" + std::to_string(amount) + " Силиций (Силициева кариера)" + lvlTag;
            return true;
        }
        case ResourceType::SILVER: {
            int amount = static_cast<int>(std::round(Balance::SILVER_BASE_YIELD * mult));
            econ.silver += amount;
            econ.data.silver = econ.silver;
            result.silver = amount;
            result.amount = amount;
            outMsg = "+" + std::to_string(amount) + " Сребро (Сребърна жила)" + lvlTag;
            return true;
        }
        case ResourceType::GOLD: {
            int amount = static_cast<int>(std::round(Balance::GOLD_BASE_YIELD * mult));
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
            // Legacy cave expedition support (grants the real minerals; the hidden ore pool is no longer used)
            int fe = Balance::IRON_BASE_YIELD, cu = Balance::COPPER_BASE_YIELD, c = Balance::COAL_BASE_YIELD, au = Balance::GOLD_BASE_YIELD;
            econ.iron += fe; econ.data.iron = econ.iron;
            econ.copper += cu; econ.data.copper = econ.copper;
            econ.coal += c; econ.data.coal = econ.coal;
            econ.gold += au; econ.data.gold = econ.gold;
            result.iron = fe; result.copper = cu; result.coal = c; result.gold = au; result.amount = fe + cu + c;
            outMsg = "+" + std::to_string(fe) + " Желязо, +" + std::to_string(cu) + " Мед, +" + std::to_string(c) +
                     " Въглища, +" + std::to_string(au) + " Злато";
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
    if (lvl >= Balance::MINE_MAX_LEVEL) return -1; // Max Level reached
    return Balance::getMineUpgradeCost(lvl, type == ResourceType::GOLD);
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
    if (lvl >= Balance::MINE_MAX_LEVEL) {
        outMsg = "МАКСИМАЛНО НИВО НА МИНАТА (НИВО " + std::to_string(Balance::MINE_MAX_LEVEL) + ")!";
        return false;
    }
    int cost = getMineUpgradeCost(player, type);
    if (cost < 0 || econ.gold < cost) {
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
    // Next tier = the cheapest plot still for sale (prices are mirrored, so both players get the same order)
    const LandPlot* next = nullptr;
    for (const auto& plot : landPlots) {
        if (plot.playerOwner == player && !plot.isPurchased) {
            if (!next || plot.costGold < next->costGold) {
                next = &plot;
            }
        }
    }
    if (next) {
        return buyLandPlot(player, next->id, outMsg);
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
        case BuildingType::SOLAR_PANEL: {
            const auto& b = Balance::SOLAR_PANEL;
            int ore = b.ironCost + b.copperCost + b.siliconCost;
            return { BuildingType::SOLAR_PANEL, b.nameBg, b.nameEn, b.woodCost, b.ironCost, b.copperCost, b.coalCost, b.siliconCost, b.silverCost, ore, b.basePowerMW };
        }
        case BuildingType::WIND_TURBINE: {
            const auto& b = Balance::WIND_TURBINE;
            int ore = b.ironCost + b.copperCost + b.coalCost;
            return { BuildingType::WIND_TURBINE, b.nameBg, b.nameEn, b.woodCost, b.ironCost, b.copperCost, b.coalCost, b.siliconCost, b.silverCost, ore, b.basePowerMW };
        }
        case BuildingType::HYDRO_PLANT: {
            const auto& b = Balance::HYDRO_PLANT;
            int ore = b.ironCost + b.copperCost + b.siliconCost;
            return { BuildingType::HYDRO_PLANT, b.nameBg, b.nameEn, b.woodCost, b.ironCost, b.copperCost, b.coalCost, b.siliconCost, b.silverCost, ore, b.basePowerMW };
        }
        case BuildingType::BATTERY: {
            const auto& b = Balance::BATTERY;
            int ore = b.ironCost + b.copperCost + b.coalCost + b.silverCost;
            return { BuildingType::BATTERY, b.nameBg, b.nameEn, b.woodCost, b.ironCost, b.copperCost, b.coalCost, b.siliconCost, b.silverCost, ore, b.basePowerMW };
        }
        case BuildingType::LAMP: {
            const auto& b = Balance::STREET_LAMP;
            int ore = b.ironCost + b.copperCost;
            return { BuildingType::LAMP, b.nameBg, b.nameEn, b.woodCost, b.ironCost, b.copperCost, b.coalCost, b.siliconCost, b.silverCost, ore, b.basePowerMW };
        }
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

bool GameEngine::isRiverBankSlot(int player, sf::Vector2f pos) const {
    // The river runs through the city between the two sectors; the plot column touching the city
    // (P1: right-most column, P2: left-most column) is the river bank.
    int col = 0, row = 0;
    getClosestGridIndex(player, pos, col, row);
    int plotCol = col / 3;
    return plotCol == ((player == 1) ? Balance::P1_RIVER_BANK_PLOT_COL : Balance::P2_RIVER_BANK_PLOT_COL);
}

int GameEngine::findOwnedBuildingInSlot(int player, sf::Vector2f pos) const {
    // A building occupies exactly one grid cell. Both the demolish preview (canPlaceBuilding) and the
    // removal itself only match a building whose cell contains pos, so a neighbour is never removed.
    sf::Vector2f s00 = getGridSlot(player, 0, 0);
    float halfCellW = (getGridSlot(player, 1, 0).x - s00.x) * 0.5f;
    float halfCellH = (getGridSlot(player, 0, 1).y - s00.y) * 0.5f;

    int bestIdx = -1;
    float bestD2 = 1e12f;
    for (size_t i = 0; i < buildings.size(); i++) {
        if (buildings[i].playerOwner != player) continue;
        float dx = buildings[i].position.x - pos.x;
        float dy = buildings[i].position.y - pos.y;
        if (std::abs(dx) <= halfCellW && std::abs(dy) <= halfCellH) {
            float d2 = dx * dx + dy * dy;
            if (d2 < bestD2) {
                bestD2 = d2;
                bestIdx = static_cast<int>(i);
            }
        }
    }
    return bestIdx;
}

bool GameEngine::removeBuilding(int player, sf::Vector2f pos, std::string& outMsg) {
    auto& econ = (player == 1) ? p1 : p2;
    int closestIdx = findOwnedBuildingInSlot(player, pos);

    if (closestIdx == -1) {
        outMsg = "НЯМА ВАША СГРАДА ТУК ЗА ПРЕМАХВАНЕ!";
        return false;
    }

    // Refund a fraction of what was paid (placement always deducts the full recipe)
    PlacedBuilding b = buildings[closestIdx];
    BuildingCost cost = getBuildingCost(b.type);
    const float refund = Balance::DEMOLISH_REFUND_FRACTION;
    int refundWood = static_cast<int>(cost.woodCost * refund);
    int refundIron = static_cast<int>(cost.ironCost * refund);
    int refundCopper = static_cast<int>(cost.copperCost * refund);
    int refundCoal = static_cast<int>(cost.coalCost * refund);
    int refundSilicon = static_cast<int>(cost.siliconCost * refund);
    int refundSilver = static_cast<int>(cost.silverCost * refund);

    econ.wood += refundWood;
    econ.iron += refundIron;
    econ.copper += refundCopper;
    econ.coal += refundCoal;
    econ.silicon += refundSilicon;
    econ.silver += refundSilver;

    econ.data.wood = econ.wood;
    econ.data.iron = econ.iron;
    econ.data.copper = econ.copper;
    econ.data.coal = econ.coal;
    econ.data.silicon = econ.silicon;
    econ.data.silver = econ.silver;

    buildings.erase(buildings.begin() + closestIdx);
    outMsg = "ПРЕМАХНАТ " + cost.nameBg + "! (Върнати: " + std::to_string(static_cast<int>(std::lround(refund * 100.0f))) +
             "% ресурси)";
    return true;
}

bool GameEngine::canPlaceBuilding(int player, BuildingType type, sf::Vector2f pos, std::string& reason) const {
    if (type == BuildingType::NONE) {
        reason = "НЯМА ИЗБРАНА СГРАДА!";
        return false;
    }

    if (type == BuildingType::DEMOLISH) {
        // Demolish tool checks if there is an owned building on this slot (same rule as removeBuilding)
        if (findOwnedBuildingInSlot(player, pos) >= 0) {
            return true;
        }
        reason = "НЯМА ВАША СГРАДА В ТАЗИ ТОЧКА ЗА ПРЕМАХВАНЕ!";
        return false;
    }

    // Snap to 2x2 grid slot inside land plot
    pos = snapToBuildingGrid(player, pos);

    const auto& econ = (player == 1) ? p1 : p2;
    BuildingCost cost = getBuildingCost(type);

    // Every resource of the recipe is required on its own (no hidden 'ore' wildcard)
    bool hasRes = econ.wood >= cost.woodCost &&
                  econ.iron >= cost.ironCost &&
                  econ.copper >= cost.copperCost &&
                  econ.coal >= cost.coalCost &&
                  econ.silicon >= cost.siliconCost &&
                  econ.silver >= cost.silverCost;

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

    // Hydro plants need the river: only the plot column next to the city river counts as river bank
    if (type == BuildingType::HYDRO_PLANT && !isRiverBankSlot(player, pos)) {
        reason = "ВЕЦ СЕ СТРОИ САМО НА БРЕГА НА РЕКАТА!\nИзползвайте парцелите в колоната до града (до реката).";
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

    // canPlaceBuilding guaranteed that every resource is available: pay the full recipe
    auto& econ = (player == 1) ? p1 : p2;
    BuildingCost cost = getBuildingCost(type);
    econ.wood -= cost.woodCost;
    econ.iron -= cost.ironCost;
    econ.copper -= cost.copperCost;
    econ.coal -= cost.coalCost;
    econ.silicon -= cost.siliconCost;
    econ.silver -= cost.silverCost;

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
    b.maxCapacity = static_cast<float>(Balance::BATTERY.batteryCapacityMWh);
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

bool GameEngine::repairBuilding(int player, sf::Vector2f pos, std::string& outMsg) {
    auto& econ = (player == 1) ? p1 : p2;

    for (auto& b : buildings) {
        if (b.playerOwner == player && b.isBroken) {
            float dist = std::hypot(b.position.x - pos.x, b.position.y - pos.y);
            if (dist <= 48.0f) {
                if (econ.wood < Balance::REPAIR_WOOD_COST || econ.iron < Balance::REPAIR_IRON_COST) {
                    outMsg = "Нужни са " + std::to_string(Balance::REPAIR_WOOD_COST) + " Дърво и " +
                             std::to_string(Balance::REPAIR_IRON_COST) + " Желязо за ремонт!";
                    return false;
                }
                econ.wood -= Balance::REPAIR_WOOD_COST;
                econ.iron -= Balance::REPAIR_IRON_COST;
                econ.data.wood = econ.wood;
                econ.data.iron = econ.iron;

                b.isBroken = false;
                BuildingCost cost = getBuildingCost(b.type);
                b.currentOutputMW = static_cast<float>(cost.basePowerMW);
                if (b.type == BuildingType::LAMP) {
                    b.lightRadius = 150.0f;
                }
                outMsg = "ПОПРАВЕНО СЪОРЪЖЕНИЕ: " + cost.nameBg + "! Отново работи на 100%!";
                return true;
            }
        }
    }
    outMsg = "Няма счупено съоръжение наблизо за ремонт.";
    return false;
}

bool GameEngine::breakBuildingAt(sf::Vector2f pos) {
    for (auto it = buildings.begin(); it != buildings.end(); ++it) {
        float dist = std::hypot(it->position.x - pos.x, it->position.y - pos.y);
        if (dist <= 30.0f) {
            buildings.erase(it);
            return true;
        }
    }
    return false;
}

bool GameEngine::breakRandomBuilding(int playerOwner, sf::Vector2f& outPos) {
    std::vector<size_t> candidates;
    for (size_t i = 0; i < buildings.size(); ++i) {
        if (playerOwner == 0 || buildings[i].playerOwner == playerOwner) {
            candidates.push_back(i);
        }
    }
    if (candidates.empty()) return false;
    size_t chosenIdx = candidates[rand() % candidates.size()];
    outPos = buildings[chosenIdx].position;
    buildings.erase(buildings.begin() + chosenIdx);
    return true;
}

bool GameEngine::hasBrokenBuilding(int player) const {
    (void)player;
    return false;
}
