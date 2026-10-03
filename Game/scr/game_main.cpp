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

// Absorbs float error so a payout due after exactly 1 game-second (e.g. 60 fixed steps) is not a step late
constexpr float REVENUE_TIMER_TOLERANCE = 1e-4f;

// Frame times arrive as float: a frame of exactly n fixed steps must run n steps, not n - 1
constexpr double FIXED_STEP_TOLERANCE = 1e-6;

// Match seed: MatchConfig::seed when non-zero, else the EC_SEED environment variable when set
// (reproducible matches / tests), else the clock. source names where it came from for the log.
uint32_t pickMatchSeed(uint32_t configSeed, const char*& source) {
    if (configSeed != 0u) {
        source = "from MatchConfig";
        return configSeed;
    }
    if (const char* env = std::getenv("EC_SEED")) {
        char* end = nullptr;
        unsigned long value = std::strtoul(env, &end, 10);
        if (end != env && *end == '\0') {
            source = "from EC_SEED";
            return static_cast<uint32_t>(value);
        }
        std::cerr << "[GameEngine] EC_SEED=\"" << env << "\" is not a number; using a clock seed.\n";
    }
    source = "from clock";
    static unsigned int initCounter = 0; // keeps two engines created in the same clock tick apart
    unsigned long long t = static_cast<unsigned long long>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    return static_cast<uint32_t>(t ^ (t >> 32)) + 0x9E3779B9u * (++initCounter);
}

// Stream ids of the engine-owned generators (any distinct constants; changing them changes every seeded match)
constexpr uint64_t RNG_STREAM_WEATHER = 1;
constexpr uint64_t RNG_STREAM_HAZARD = 2;
constexpr uint64_t RNG_STREAM_GENERAL = 3;

// Wind direction of a weather_report ("left" / "right" / "none") as -1 / +1 / 0
int windDirectionOf(const std::vector<std::string>& report) {
    if (report.size() < 3) return 0;
    if (report[2] == "left") return -1;
    if (report[2] == "right") return 1;
    return 0;
}


// Top-left corner of a land plot (plotCol 0..2 in screen order West->East, plotRow 0..3)
sf::Vector2f plotTopLeft(int player, int plotCol, int plotRow) {
    float startX = (player == 1) ? Balance::WEST_PLOTS_START_X : Balance::EAST_PLOTS_START_X;
    return { startX + plotCol * (Balance::PLOT_WIDTH + Balance::PLOT_GAP_X),
             Balance::PLOTS_START_Y + plotRow * (Balance::PLOT_HEIGHT + Balance::PLOT_GAP_Y) };
}

// Wind speed (4th field of a weather_report, "0" when calm) as a number
float windSpeedOf(const std::vector<std::string>& report) {
    if (report.size() < 4) return 0.0f;
    return static_cast<float>(std::atof(report[3].c_str()));
}

// Base output (MW) of a building type, read from Balance without building a BuildingCost (no strings)
constexpr int basePowerOf(BuildingType type) {
    return GameEngine::getBuildingDef(type) ? GameEngine::getBuildingDef(type)->basePowerMW : 0;
}

// amount x multiplier rounded to an integer (>= 0); exactly amount when the multiplier is neutral
int scaleByMult(int amount, float mult) {
    if (mult == 1.0f) return amount;
    return std::max(0, static_cast<int>(std::lround(static_cast<float>(amount) * mult)));
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
    init(MatchConfig());
}

void GameEngine::init(const MatchConfig& cfg) {
    // Reset EVERY piece of match state (clock, season, revenue timer, city, players, buildings)
    MatchConfig rules = cfg; // cfg may alias this->config, which the reset below overwrites
    const int hostMaxSteps = maxStepsPerUpdate; // a host setting, not match state: survives restarts
    *this = GameEngine();
    maxStepsPerUpdate = hostMaxSteps;
    // (static_cast copies: std::clamp takes references, and pre-C++17 compilers such as MinGW g++ 6.3 have no
    // inline variables, so binding the static constexpr members directly would need out-of-line definitions)
    rules.finalDay = std::clamp(rules.finalDay, static_cast<int>(MatchConfig::MIN_FINAL_DAY), static_cast<int>(MatchConfig::MAX_FINAL_DAY));
    rules.victoryShare = std::isfinite(rules.victoryShare)
                             ? std::clamp(rules.victoryShare, static_cast<float>(MatchConfig::MIN_VICTORY_SHARE),
                                          static_cast<float>(MatchConfig::MAX_VICTORY_SHARE))
                             : Balance::VICTORY_SHARE;
    rules.daySeconds = std::isfinite(rules.daySeconds)
                           ? std::clamp(rules.daySeconds, static_cast<float>(MatchConfig::MIN_DAY_SECONDS),
                                        static_cast<float>(MatchConfig::MAX_DAY_SECONDS))
                           : Balance::SECONDS_PER_DAY;
    rules.graceDays = std::clamp(rules.graceDays, 0, rules.finalDay - 1); // the final day is never a grace day
    config = rules;
    gameSeconds = Balance::gameSecondsAtHour(Balance::MATCH_START_HOUR, config.daySeconds);

    // Seed the engine generators, the legacy randomInt and std::rand (lightning, bot, particles) once per match
    const char* seedSource = "";
    matchSeed = pickMatchSeed(config.seed, seedSource);
    weatherRng.seed(matchSeed, RNG_STREAM_WEATHER);
    hazardRng.seed(matchSeed, RNG_STREAM_HAZARD);
    generalRng.seed(matchSeed, RNG_STREAM_GENERAL);
    seedRandom(matchSeed);
    std::srand(matchSeed);
    std::cout << "[GameEngine] RNG seed: " << matchSeed << " (" << seedSource << ")"
              << ". Set EC_SEED=" << matchSeed << " to replay this match.\n";

    landPlots.clear();
    buildings.clear();

    // Reset City State on fresh game or restart (First 2 days are Grace Period = 0 MW)
    city = CityConquestState();
    city.cityEnergyDemand = (currentDay <= config.graceDays) ? 0 : Balance::STARTING_CITY_DEMAND_MW;
    city.p1CityShare = 0.50f;
    city.winner = 0;
    if (config.graceDays <= 0) {
        city.lastCutMessage = "ДОБРЕ ДОШЛИ! БЕЗ ГРАТИСЕН ПЕРИОД: ГРАДЪТ ИЗИСКВА ЕНЕРГИЯ ОТ ДЕН 1!";
    } else if (config.graceDays == 1) {
        city.lastCutMessage = "ДОБРЕ ДОШЛИ! ГРАТИСЕН ПЕРИОД: ПЪРВИЯ ДЕН ГРАДЪТ ИСКА 0 ЕНЕРГИЯ ЗА РАЗВИТИЕ!";
    } else {
        city.lastCutMessage = "ДОБРЕ ДОШЛИ! ГРАТИСЕН ПЕРИОД: ПЪРВИТЕ " + std::to_string(config.graceDays) +
                              " ДЕНА ГРАДЪТ ИСКА 0 ЕНЕРГИЯ ЗА РАЗВИТИЕ!";
    }

    // -------------------------------------------------------------------------
    // Generate Purchasable Land Grid on West (P1) and East (P2)
    // 12 land plots per player (3 cols x 4 rows), laid out by the Balance plot constants so they
    // never touch or go under the city (X in [610.0, 990.0]).
    // -------------------------------------------------------------------------
    int idCounter = 1;
    for (int player = 1; player <= 2; ++player) {
        for (int r = 0; r < Balance::PLOT_ROWS; r++) {
            for (int c = 0; c < Balance::PLOT_COLS; c++) {
                LandPlot plot;
                plot.id = idCounter++;
                plot.playerOwner = player;
                plot.bounds = sf::FloatRect(plotTopLeft(player, c, r), { Balance::PLOT_WIDTH, Balance::PLOT_HEIGHT });
                // Each player starts with the top plot on the far side from the city (P1 top-left, P2 top-right).
                // Prices mirror around the river, so both players pay the same for mirrored plots.
                int ownCol = (player == 1) ? c : (Balance::PLOT_COLS - 1 - c);
                plot.isPurchased = (r == 0 && ownCol == 0);
                plot.costGold = Balance::getLandPlotCost(r, ownCol);
                landPlots.push_back(plot);
            }
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

    emitEvent(GameEventType::MESSAGE, 0, 0.0f, city.lastCutMessage);

    std::cout << "[GameEngine] Backend initialized with " << landPlots.size() << " land plots & "
              << buildings.size() << " starter buildings.\n";
}

void GameEngine::emitEvent(GameEventType type, int player, float value, const std::string& text, int subtype, float x, float y) {
    if (events.size() >= MAX_PENDING_EVENTS) {
        // Nobody drains the queue: keep the newest half instead of growing without bound
        events.erase(events.begin(), events.begin() + static_cast<std::ptrdiff_t>(MAX_PENDING_EVENTS / 2));
    }
    GameEvent ev{ type, player, value, text };
    ev.subtype = subtype;
    ev.x = x;
    ev.y = y;
    events.push_back(std::move(ev));
}

std::vector<GameEvent> GameEngine::pollEvents() {
    std::vector<GameEvent> out;
    out.swap(events);
    return out;
}

void GameEngine::rollDailyWeather() {
    std::string sName = Balance::getSeasonWeatherKey(currentSeason);

    auto rep1 = weather_report(sName, weatherRng);
    p1WindSpeed = windSpeedOf(rep1);
    p1WindDirection = windDirectionOf(rep1);
    p1Weather = WeatherSystem::reportToWeatherType(rep1);
    emitEvent(GameEventType::WEATHER_CHANGED, 1, p1WindSpeed, std::string(), static_cast<int>(p1Weather));

    auto rep2 = weather_report(sName, weatherRng);
    p2WindSpeed = windSpeedOf(rep2);
    p2WindDirection = windDirectionOf(rep2);
    p2Weather = WeatherSystem::reportToWeatherType(rep2);
    emitEvent(GameEventType::WEATHER_CHANGED, 2, p2WindSpeed, std::string(), static_cast<int>(p2Weather));
}

void GameEngine::update(float dt) {
    // If the match is concluded, freeze all simulation, economy and dividends
    if (city.winner != 0) {
        return;
    }
    if (!(dt > 0.0f) || !std::isfinite(dt)) {
        return; // paused / bad frame time: nothing to simulate
    }

    // Fixed timestep: real time is accumulated and the simulation always advances in whole steps of
    // FIXED_STEP_SECONDS (x time scale), so the result does not depend on the frame rate.
    stepAccumulator += static_cast<double>(dt);
    int steps = 0;
    while (stepAccumulator + FIXED_STEP_TOLERANCE >= FIXED_STEP_SECONDS && city.winner == 0) {
        stepAccumulator -= FIXED_STEP_SECONDS;
        advanceGameTime(static_cast<float>(FIXED_STEP_SECONDS) * timeScale);
        if (maxStepsPerUpdate > 0 && ++steps >= maxStepsPerUpdate) {
            // Real-time host fell behind: drop the backlog instead of spiralling
            if (stepAccumulator >= FIXED_STEP_SECONDS) {
                stepAccumulator = 0.0;
            }
            break;
        }
    }
    if (city.winner != 0) {
        stepAccumulator = 0.0;
    }
}

void GameEngine::advanceGameTime(float gameDt) {
    float remaining = gameDt;
    while (remaining > 0.0f && city.winner == 0) {
        float step = std::min(remaining, MAX_SIM_STEP_SEC);

        // Never let a step cross the 06:00 day boundary: the day that ends is settled exactly once,
        // with exactly the energy delivered during that day, however large the frame is.
        double dayEndSeconds = static_cast<double>(currentDay) * config.daySeconds;
        bool reachesDayEnd = (gameSeconds + step >= dayEndSeconds);
        if (reachesDayEnd) {
            step = static_cast<float>(std::max(0.0, dayEndSeconds - gameSeconds));
        }

        simulateStep(step);
        remaining -= step;

        if (reachesDayEnd) {
            gameSeconds = dayEndSeconds; // exact boundary
            ++currentDay;
            processDayEnd();
        }
    }
}

void GameEngine::simulateStep(float dt) {
    gameSeconds += dt;
    hour24 = static_cast<float>(std::fmod((gameSeconds / config.daySeconds) * 24.0 + Balance::CLOCK_HOUR_AT_ZERO, 24.0));
    // Season flips at midnight (dark in every season), never at the 06:00 rollover
    SeasonType seasonNow = Balance::getSeasonAtGameSeconds(static_cast<float>(gameSeconds), config.daySeconds);
    if (seasonNow != currentSeason) {
        currentSeason = seasonNow;
        emitEvent(GameEventType::SEASON_CHANGED, 0, 0.0f, std::string(), static_cast<int>(currentSeason));
    }

    // Update building energy outputs based on real-time continuous weather & sun
    updateBuildingsEnergy(dt);
    city.dailySeconds += dt;

    // Percentage-based city energy revenue, paid once per full game-second (remainder carried over)
    revenueTimer += dt;
    while (revenueTimer >= 1.0f - REVENUE_TIMER_TOLERANCE) {
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
        int p1Payout = scaleByMult(Balance::calculatePlayerPayout(contractPool, p1Share), p1Mods.incomeMult);
        int p2Payout = scaleByMult(Balance::calculatePlayerPayout(contractPool, p2Share), p2Mods.incomeMult);

        p1.money += p1Payout;

        p2.money += p2Payout;

        // Gold dividend for sustained power supply from Balance formula (capped by city demand)
        p1.gold += scaleByMult(Balance::calculateGoldDividend(p1.energyMW, city.cityEnergyDemand), p1Mods.incomeMult);

        p2.gold += scaleByMult(Balance::calculateGoldDividend(p2.energyMW, city.cityEnergyDemand), p2Mods.incomeMult);
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
    // Two passes over the buildings, no allocations: pass 1 sums generation and battery limits,
    // pass 2 applies battery power and lamp states in building order.
    // -------------------------------------------------------------------------
    const float stepHours = Balance::gameSecondsToHours(dt, config.daySeconds);
    auto processPlayerGrid = [&](int player, WeatherType w, PlayerEconomy& econ, float& dailyDelivered) {
        // Weather multipliers are the same for every building of the sector this step
        const float solarMult = WeatherSystem::getSolarMultiplier(w, hour24, currentSeason);
        const float windMult = WeatherSystem::getWindMultiplier(w, hour24);
        const float hydroMult = WeatherSystem::getHydroMultiplier(w);

        // Step A: pure generation from Solar, Wind and Hydro; count lamps; battery charge/discharge limits
        float rawGen = 0.0f;
        int lampCount = 0;
        int batteryCount = 0;
        float canGive = 0.0f; // MW all batteries could discharge this step
        float canTake = 0.0f; // MW all batteries could absorb this step
        for (auto& b : buildings) {
            if (b.playerOwner != player) continue;
            switch (b.type) {
                case BuildingType::SOLAR_PANEL:
                case BuildingType::WIND_TURBINE:
                case BuildingType::HYDRO_PLANT: {
                    const float mult = (b.type == BuildingType::SOLAR_PANEL) ? solarMult
                                       : (b.type == BuildingType::WIND_TURBINE) ? windMult : hydroMult;
                    float out = basePowerOf(b.type) * mult;
                    b.currentOutputMW = out;
                    rawGen += out;
                    break;
                }
                case BuildingType::LAMP:
                    ++lampCount;
                    break;
                case BuildingType::BATTERY:
                    ++batteryCount;
                    b.currentOutputMW = 0.0f;
                    if (stepHours > 0.0f) {
                        canGive += std::min(Balance::BATTERY_MAX_POWER_MW, b.energyStored / stepHours);
                        canTake += std::min(Balance::BATTERY_MAX_POWER_MW,
                                            std::max(0.0f, b.maxCapacity - b.energyStored) / stepHours);
                    }
                    break;
                default:
                    break;
            }
        }

        // Step B: Load the player is trying to serve: own lamps first (10 MW each), then the city demand
        float lampDemand = lampCount * LAMP_POWER_MW;
        float cityTarget = static_cast<float>(std::max(0, city.cityEnergyDemand));
        float loadTarget = lampDemand + cityTarget;

        float batteryDischarge = 0.0f; // MW taken out of batteries this step
        float batteryCharge = 0.0f;    // MW put into batteries this step (NOT delivered to the city)
        if (stepHours > 0.0f && batteryCount > 0) {
            if (rawGen < loadTarget) {
                // Step C: Shortfall -> batteries discharge to cover it (never more than needed,
                // never more than BATTERY_MAX_POWER_MW each, never more than they hold)
                batteryDischarge = std::min(loadTarget - rawGen, canGive);
            } else if (rawGen > loadTarget) {
                // Step D: Real surplus only -> batteries charge (never from thin air)
                batteryCharge = std::min(rawGen - loadTarget, canTake);
            }
        }

        // Step E: Power lamps first from generation + discharge (minus what went into storage)
        float totalAvailable = std::max(0.0f, rawGen + batteryDischarge - batteryCharge);
        int poweredCount = static_cast<int>((totalAvailable + 0.001f) / LAMP_POWER_MW);
        poweredCount = std::min(lampCount, poweredCount);

        if (lampCount > 0 || batteryDischarge > 0.0f || batteryCharge > 0.0f) {
            const float dischargeFraction = (batteryDischarge > 0.0f) ? batteryDischarge / canGive : 0.0f;
            const float chargeFraction = (batteryCharge > 0.0f) ? batteryCharge / canTake : 0.0f;
            int lampIndex = 0;
            for (auto& b : buildings) {
                if (b.playerOwner != player) continue;
                if (b.type == BuildingType::BATTERY) {
                    if (batteryDischarge > 0.0f) {
                        float power = std::min(Balance::BATTERY_MAX_POWER_MW, b.energyStored / stepHours) * dischargeFraction;
                        b.currentOutputMW = power;
                        b.energyStored = std::max(0.0f, b.energyStored - power * stepHours);
                    } else if (batteryCharge > 0.0f) {
                        float room = std::max(0.0f, b.maxCapacity - b.energyStored);
                        float power = std::min(Balance::BATTERY_MAX_POWER_MW, room / stepHours) * chargeFraction;
                        b.energyStored = std::min(b.maxCapacity, b.energyStored + power * stepHours);
                    }
                } else if (b.type == BuildingType::LAMP) {
                    if (lampIndex < poweredCount) {
                        b.lightRadius = Balance::STREET_LAMP.lightRadius;
                        b.currentOutputMW = -LAMP_POWER_MW;
                    } else {
                        // UNPOWERED LAMP! Shuts down, dark lantern head, no light circle
                        b.lightRadius = 0.0f;
                        b.currentOutputMW = 0.0f;
                    }
                    ++lampIndex;
                }
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
    emitEvent(GameEventType::DAY_END, 0, static_cast<float>(endedDay));
    float dayShift = 0.0f; // change of the P1 share settled for this day

    // The day is judged on the AVERAGE power delivered over the whole day (06:00 -> 06:00),
    // not on an instantaneous snapshot, so daytime-only sources (solar) count fully.
    // (+0.01 MW only absorbs float error, e.g. a battery covering exactly the demand all night)
    float daySeconds = (city.dailySeconds > 0.0f) ? city.dailySeconds : config.daySeconds;
    int p1AvgMW = static_cast<int>(std::floor(city.p1DailyDelivered / daySeconds + 0.01f));
    int p2AvgMW = static_cast<int>(std::floor(city.p2DailyDelivered / daySeconds + 0.01f));

    if (endedDay <= config.graceDays) {
        // Grace period (first 2 days by default): 0 energy demanded, no penalties or cuts
        const std::string dayStr = std::to_string(endedDay);
        const int daysLeft = config.graceDays - endedDay;
        if (daysLeft > 0) {
            city.lastCutMessage = "ДЕН " + dayStr + " ПРИКЛЮЧИ [ГРАТИСЕН ПЕРИОД]: ГРАДЪТ ИСКАШЕ 0 MW. ОЩЕ " +
                                  std::to_string(daysLeft) + (daysLeft == 1 ? " ДЕН" : " ДНИ") + " ЗА РАЗВИТИЕ!";
        } else {
            city.lastCutMessage = "ДЕН " + dayStr + " ПРИКЛЮЧИ: КРАЙ НА ГРАТИСНИЯ ПЕРИОД! ОТ ДЕН " + std::to_string(endedDay + 1) +
                                  " ГРАДЪТ ИЗИСКВА ЕНЕРГИЯ!";
        }
    } else {
        bool p1Succeeded = (p1AvgMW >= city.cityEnergyDemand);
        bool p2Succeeded = (p2AvgMW >= city.cityEnergyDemand);
        float shift = Balance::calculateDailyCityShift(p1AvgMW, p2AvgMW, city.cityEnergyDemand);
        // PlayerModifiers::shareBonus: extra share for the winner of the day (never flips the result)
        if (shift > 0.0f) {
            shift = std::max(0.0f, shift + p1Mods.shareBonus);
        } else if (shift < 0.0f) {
            shift = std::min(0.0f, shift - p2Mods.shareBonus);
        }
        dayShift = shift;
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

        // Check Victory Conditions only at day end (never in a sandbox match)
        const float eps = 1e-4f; // tolerate float error from summed daily shifts
        float p1Share = city.p1CityShare;
        float p2Share = 1.0f - p1Share;
        int p1Pct = static_cast<int>(std::lround(p1Share * 100.0f));
        int p2Pct = 100 - p1Pct;
        const float victoryShare = config.victoryShare;
        const std::string victoryPctStr = std::to_string(static_cast<int>(std::lround(victoryShare * 100.0f)));
        if (config.sandbox) {
            // Sandbox: free play, the city share still moves but nobody wins
        } else if (p1Share >= victoryShare - eps) {
            city.winner = 1;
            city.lastCutMessage = "ПОБЕДА ЗА ИГРАЧ 1! КОНТРОЛИРА " + std::to_string(p1Pct) + "% ОТ ГРАДА (НУЖНИ СА " +
                                  victoryPctStr + "%)!";
        } else if (p2Share >= victoryShare - eps) {
            city.winner = 2;
            city.lastCutMessage = "ПОБЕДА ЗА ИГРАЧ 2! КОНТРОЛИРА " + std::to_string(p2Pct) + "% ОТ ГРАДА (НУЖНИ СА " +
                                  victoryPctStr + "%)!";
        } else if (endedDay >= config.finalDay) {
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

    int dayWinner = (dayShift > 0.0f) ? 1 : ((dayShift < 0.0f) ? 2 : 0);
    // subtype = the ended day's city demand (0 on a grace day), x / y = average MW delivered by P1 / P2
    emitEvent(GameEventType::DAY_RESULT, dayWinner, dayShift, city.lastCutMessage, city.cityEnergyDemand,
              static_cast<float>(p1AvgMW), static_cast<float>(p2AvgMW));
    if (city.winner != 0) {
        emitEvent(GameEventType::VICTORY, city.winner, city.p1CityShare, city.lastCutMessage);
    }

    p1.cityInfluence = city.p1CityShare;
    p2.cityInfluence = 1.0f - city.p1CityShare;

    // City expands and demands power next day (0 MW for first 2 days grace, 30 MW Day 3, +15 MW daily)
    if (currentDay <= config.graceDays) {
        city.cityEnergyDemand = 0;
    } else if (currentDay == config.graceDays + 1) {
        city.cityEnergyDemand = Balance::STARTING_CITY_DEMAND_MW;
    } else {
        city.cityEnergyDemand += Balance::DAILY_DEMAND_INCREASE_MW;
    }
    city.p1DailyDelivered = 0.0f;
    city.p2DailyDelivered = 0.0f;
    city.dailySeconds = 0.0f;

    // Daily dynamic weather generation using weather_report from weatherF.
    // The new day's season already took effect at the preceding midnight.
    SeasonType newSeason = Balance::getSeasonForDay(currentDay);
    if (newSeason != currentSeason) {
        currentSeason = newSeason;
        emitEvent(GameEventType::SEASON_CHANGED, 0, 0.0f, std::string(), static_cast<int>(currentSeason));
    }
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
    const float yieldMult = getPlayerModifiers(player).mineYieldMult;
    float mult = Balance::getMineYieldMultiplier(lvl) * yieldMult;
    std::string lvlTag = (lvl > 1 ? " [НИВО " + std::to_string(lvl) + "]" : "");

    switch (type) {
        case ResourceType::WOOD: {
            int amount = static_cast<int>(std::round(Balance::WOOD_BASE_YIELD * mult));
            econ.wood += amount;
            result.wood = amount;
            result.amount = amount;
            outMsg = "+" + std::to_string(amount) + " Дървесина (Гора)" + lvlTag;
            break;
        }
        case ResourceType::IRON: {
            int amount = static_cast<int>(std::round(Balance::IRON_BASE_YIELD * mult));
            econ.iron += amount;
            result.iron = amount;
            result.amount = amount;
            outMsg = "+" + std::to_string(amount) + " Желязо (Желязна мина)" + lvlTag;
            break;
        }
        case ResourceType::COPPER: {
            int amount = static_cast<int>(std::round(Balance::COPPER_BASE_YIELD * mult));
            econ.copper += amount;
            result.copper = amount;
            result.amount = amount;
            outMsg = "+" + std::to_string(amount) + " Мед (Медна жила)" + lvlTag;
            break;
        }
        case ResourceType::COAL: {
            int amount = static_cast<int>(std::round(Balance::COAL_BASE_YIELD * mult));
            econ.coal += amount;
            result.coal = amount;
            result.amount = amount;
            outMsg = "+" + std::to_string(amount) + " Въглища (Въглищен пласт)" + lvlTag;
            break;
        }
        case ResourceType::SILICON: {
            int amount = static_cast<int>(std::round(Balance::SILICON_BASE_YIELD * mult));
            econ.silicon += amount;
            result.silicon = amount;
            result.amount = amount;
            outMsg = "+" + std::to_string(amount) + " Силиций (Силициева кариера)" + lvlTag;
            break;
        }
        case ResourceType::SILVER: {
            int amount = static_cast<int>(std::round(Balance::SILVER_BASE_YIELD * mult));
            econ.silver += amount;
            result.silver = amount;
            result.amount = amount;
            outMsg = "+" + std::to_string(amount) + " Сребро (Сребърна жила)" + lvlTag;
            break;
        }
        case ResourceType::GOLD: {
            int amount = static_cast<int>(std::round(Balance::GOLD_BASE_YIELD * mult));
            econ.gold += amount;
            result.gold = amount;
            result.amount = amount;
            outMsg = "+" + std::to_string(amount) + " Злато (Златна жила)" + lvlTag;
            break;
        }
        case ResourceType::MONEY: {
            outMsg = "Мината за пари е премахната! Печелете пари от доставка на ток към града.";
            return false;
        }
        case ResourceType::ORE: {
            // Legacy cave expedition support (grants the real minerals; the hidden ore pool is no longer used)
            int fe = scaleByMult(Balance::IRON_BASE_YIELD, yieldMult), cu = scaleByMult(Balance::COPPER_BASE_YIELD, yieldMult);
            int c = scaleByMult(Balance::COAL_BASE_YIELD, yieldMult), au = scaleByMult(Balance::GOLD_BASE_YIELD, yieldMult);
            econ.iron += fe;
            econ.copper += cu;
            econ.coal += c;
            econ.gold += au;
            result.iron = fe; result.copper = cu; result.coal = c; result.gold = au; result.amount = fe + cu + c;
            outMsg = "+" + std::to_string(fe) + " Желязо, +" + std::to_string(cu) + " Мед, +" + std::to_string(c) +
                     " Въглища, +" + std::to_string(au) + " Злато";
            break;
        }
        default:
            return false;
    }

    emitEvent(GameEventType::RESOURCE_MINED, player, static_cast<float>(result.amount), std::string(), static_cast<int>(type));
    return true;
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
    emitEvent(GameEventType::MINE_UPGRADED, player, static_cast<float>(newLvl), std::string(), static_cast<int>(type));
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
                emitEvent(GameEventType::LAND_BOUGHT, player, static_cast<float>(plot.costGold), std::string(), plot.id,
                          plot.bounds.position.x + plot.bounds.size.x * 0.5f, plot.bounds.position.y + plot.bounds.size.y * 0.5f);
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
    if (type == BuildingType::DEMOLISH) {
        return { BuildingType::DEMOLISH, "Премахване", "Demolish Tool", 0, 0, 0, 0, 0, 0, 0, 0 };
    }
    const Balance::BuildingDef* def = getBuildingDef(type);
    if (!def) {
        return { BuildingType::NONE, "", "", 0, 0, 0, 0, 0, 0, 0, 0 };
    }
    // Legacy 'ore' total = every mineral of the recipe (no longer spent, kept for old callers)
    int ore = def->ironCost + def->copperCost + def->coalCost + def->siliconCost + def->silverCost;
    return { type, def->nameBg, def->nameEn, def->woodCost, def->ironCost, def->copperCost, def->coalCost,
             def->siliconCost, def->silverCost, ore, def->basePowerMW };
}

BuildingCost GameEngine::getBuildingCost(int player, BuildingType type) const {
    BuildingCost c = getBuildingCost(type);
    const float m = getPlayerModifiers(player).costMult;
    if (m != 1.0f) {
        c.woodCost = scaleByMult(c.woodCost, m);
        c.ironCost = scaleByMult(c.ironCost, m);
        c.copperCost = scaleByMult(c.copperCost, m);
        c.coalCost = scaleByMult(c.coalCost, m);
        c.siliconCost = scaleByMult(c.siliconCost, m);
        c.silverCost = scaleByMult(c.silverCost, m);
        c.oreCost = c.ironCost + c.copperCost + c.coalCost + c.siliconCost + c.silverCost;
    }
    return c;
}

void GameEngine::setPlayerModifiers(int player, const PlayerModifiers& mods) {
    auto cleanMult = [](float v) {
        return std::isfinite(v) ? std::clamp(v, 0.0f, static_cast<float>(PlayerModifiers::MAX_MULT)) : 1.0f;
    };
    PlayerModifiers m;
    m.incomeMult = cleanMult(mods.incomeMult);
    m.mineYieldMult = cleanMult(mods.mineYieldMult);
    m.costMult = cleanMult(mods.costMult);
    m.cooldownMult = cleanMult(mods.cooldownMult);
    m.shareBonus = std::isfinite(mods.shareBonus)
                       ? std::clamp(mods.shareBonus, -PlayerModifiers::MAX_SHARE_BONUS,
                                    static_cast<float>(PlayerModifiers::MAX_SHARE_BONUS))
                       : 0.0f;
    ((player == 1) ? p1Mods : p2Mods) = m;
}

sf::Vector2f GameEngine::getGridSlot(int player, int col, int row) const {
    // 9 columns (0..8) and 12 rows (0..11) for 12 plots x 9 slots each
    col = std::max(0, std::min(col, Balance::GRID_COLS - 1));
    row = std::max(0, std::min(row, Balance::GRID_ROWS - 1));

    int plotC = col / Balance::SLOT_COLS_PER_PLOT;
    int subC = col % Balance::SLOT_COLS_PER_PLOT;
    int plotR = row / Balance::SLOT_ROWS_PER_PLOT;
    int subR = row % Balance::SLOT_ROWS_PER_PLOT;

    sf::Vector2f plotPos = plotTopLeft(player, plotC, plotR);
    float subW = Balance::PLOT_WIDTH / static_cast<float>(Balance::SLOT_COLS_PER_PLOT);
    float subH = Balance::PLOT_HEIGHT / static_cast<float>(Balance::SLOT_ROWS_PER_PLOT);

    float cx = plotPos.x + (subC + 0.5f) * subW;
    float cy = plotPos.y + (subR + 0.5f) * subH;
    return sf::Vector2f(cx, cy);
}

void GameEngine::getClosestGridIndex(int player, sf::Vector2f pos, int& outCol, int& outRow) const {
    float bestD2 = 1e12f;
    outCol = 0;
    outRow = 0;
    for (int r = 0; r < Balance::GRID_ROWS; ++r) {
        for (int c = 0; c < Balance::GRID_COLS; ++c) {
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
    BuildingCost cost = getBuildingCost(player, b.type);
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


    buildings.erase(buildings.begin() + closestIdx);
    outMsg = "ПРЕМАХНАТ " + cost.nameBg + "! (Върнати: " + std::to_string(static_cast<int>(std::lround(refund * 100.0f))) +
             "% ресурси)";
    emitEvent(GameEventType::BUILDING_REMOVED, player, refund, cost.nameBg, static_cast<int>(b.type), b.position.x, b.position.y);
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
    BuildingCost cost = getBuildingCost(player, type);

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
        if (dist < Balance::BUILDING_MIN_SPACING) {
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
    BuildingCost cost = getBuildingCost(player, type);
    econ.wood -= cost.woodCost;
    econ.iron -= cost.ironCost;
    econ.copper -= cost.copperCost;
    econ.coal -= cost.coalCost;
    econ.silicon -= cost.siliconCost;
    econ.silver -= cost.silverCost;


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
    b.lightRadius = (type == BuildingType::LAMP) ? Balance::STREET_LAMP.lightRadius : 0.0f;
    buildings.push_back(b);

    if (type == BuildingType::LAMP) {
        outMsg = "ПОСТАВЕНА Осветителна лампа! (Консумира " + std::to_string(Balance::STREET_LAMP.lampConsumptionMW) +
                 " MW, осветява " + std::to_string(static_cast<int>(Balance::STREET_LAMP.lightRadius)) + " px)";
    } else if (type == BuildingType::BATTERY) {
        outMsg = "ПОСТРОЕН Акумулатор! (0% заряд. Зарежда се от произведената чиста енергия)";
    } else {
        outMsg = "ПОСТРОЕН " + cost.nameBg + "! (+" + std::to_string(cost.basePowerMW) + " MW)";
    }
    emitEvent(GameEventType::BUILDING_PLACED, player, static_cast<float>(cost.basePowerMW), cost.nameBg, static_cast<int>(type), pos.x, pos.y);
    return true;
}

bool GameEngine::repairBuilding(int player, sf::Vector2f pos, std::string& outMsg) {
    auto& econ = (player == 1) ? p1 : p2;

    for (auto& b : buildings) {
        if (b.playerOwner == player && b.isBroken) {
            float dist = std::hypot(b.position.x - pos.x, b.position.y - pos.y);
            if (dist <= Balance::REPAIR_REACH_RADIUS) {
                if (econ.wood < Balance::REPAIR_WOOD_COST || econ.iron < Balance::REPAIR_IRON_COST) {
                    outMsg = "Нужни са " + std::to_string(Balance::REPAIR_WOOD_COST) + " Дърво и " +
                             std::to_string(Balance::REPAIR_IRON_COST) + " Желязо за ремонт!";
                    return false;
                }
                econ.wood -= Balance::REPAIR_WOOD_COST;
                econ.iron -= Balance::REPAIR_IRON_COST;

                b.isBroken = false;
                BuildingCost cost = getBuildingCost(b.type);
                b.currentOutputMW = static_cast<float>(cost.basePowerMW);
                if (b.type == BuildingType::LAMP) {
                    b.lightRadius = Balance::STREET_LAMP.lightRadius;
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
        if (dist <= Balance::STRIKE_HIT_RADIUS) {
            PlacedBuilding hit = *it;
            buildings.erase(it);
            emitEvent(GameEventType::BUILDING_DESTROYED, hit.playerOwner, 0.0f, getBuildingCost(hit.type).nameBg,
                      static_cast<int>(hit.type), hit.position.x, hit.position.y);
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
    size_t chosenIdx = candidates[static_cast<size_t>(hazardRng.range(0, static_cast<int>(candidates.size()) - 1))];
    PlacedBuilding hit = buildings[chosenIdx];
    outPos = buildings[chosenIdx].position;
    buildings.erase(buildings.begin() + chosenIdx);
    emitEvent(GameEventType::BUILDING_DESTROYED, hit.playerOwner, 0.0f, getBuildingCost(hit.type).nameBg,
              static_cast<int>(hit.type), hit.position.x, hit.position.y);
    return true;
}

bool GameEngine::hasBrokenBuilding(int player) const {
    (void)player;
    return false;
}
