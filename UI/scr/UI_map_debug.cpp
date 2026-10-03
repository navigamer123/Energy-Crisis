#include "../includes/UI_map.h"
#include "../includes/UI_shot.h"
#include "../includes/UI_theme.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

// =============================================================================
// UI_map screenshot scenes (energy_crisis.exe --shot out.png --scene NAME)
// Every scene is reached through the public engine API only: resources are granted,
// land is bought and buildings are placed like a player would, and time is advanced
// with GameEngine::update(). Nothing here runs during normal play.
// =============================================================================

namespace {

constexpr float SIM_STEP = 0.25f; // game-seconds per engine step while fast-forwarding

// Plot id of a player's plot at (row, screen column): ids 1..12 West, 13..24 East
int plotId(int player, int row, int col) {
    return (player == 1 ? 1 : 13) + row * 3 + col;
}

void setResources(PlayerEconomy& e, int wood, int iron, int copper, int coal, int silicon, int silver,
                  int gold, int money) {
    e.wood = wood;
    e.iron = iron;
    e.copper = copper;
    e.coal = coal;
    e.silicon = silicon;
    e.silver = silver;
    e.gold = gold;
    e.money = money;
}

} // namespace

void UI_map::setupDebugScene(const std::string& scene, int frames) {
    // withStats: feed the match telemetry like render() does (before / update / after), so the
    // dashboard, the report, the event log and the toasts see the fast-forwarded days
    auto advanceGameSeconds = [this](float seconds, bool withStats = false) {
        engine.setTimeScale(1.0f);
        // Fast-forward steps are longer than the real-time step cap allows: lift it meanwhile
        const int hostMaxSteps = engine.getMaxStepsPerUpdate();
        engine.setMaxStepsPerUpdate(0);
        while (seconds > 0.0f && engine.getCityState().winner == 0) {
            float step = std::min(seconds, SIM_STEP);
            if (withStats) stats.beforeEngineUpdate(engine);
            engine.update(step);
            if (withStats) updateInfoUI(0.0f);
            seconds -= step;
        }
        engine.setMaxStepsPerUpdate(hostMaxSteps);
    };
    // Fast-forward to the given day (1-based) and clock hour
    auto advanceTo = [&](int day, float hour, bool withStats = false) {
        float hoursFromNow = static_cast<float>(day - engine.getCurrentDay()) * 24.0f + (hour - engine.getHour24());
        if (engine.getHour24() < Balance::CLOCK_HOUR_AT_ZERO) hoursFromNow -= 24.0f; // after midnight: same game day
        if (hoursFromNow > 0.0f) advanceGameSeconds(hoursFromNow * Balance::SECONDS_PER_DAY / 24.0f, withStats);
    };
    auto place = [this](int player, BuildingType type, int col, int row) {
        std::string msg;
        if (!engine.placeBuilding(player, type, engine.getGridSlot(player, col, row), msg)) {
            std::cerr << "[Shot] Could not place a building for P" << player << ": " << msg << "\n";
        }
    };
    auto buy = [this](int player, int row, int col) {
        std::string msg;
        engine.buyLandPlot(player, plotId(player, row, col), msg);
    };

    // A mid-game board for both players: bought land, every building type, a few lamps.
    // Afterwards each player keeps a mixed stock, so some build cards are affordable and some not.
    // fullEast = false: P2 starts with a lamp, one solar panel and one wind turbine only.
    auto populate = [&](bool fullEast = true) {
        setResources(engine.getPlayerEconomyMut(1), 400, 400, 400, 400, 400, 400, 3000, 0);
        setResources(engine.getPlayerEconomyMut(2), 400, 400, 400, 400, 400, 400, 3000, 0);

        // West (P1): start plot (row 0, col 0), river-bank plot (row 0, col 2) and two more
        buy(1, 0, 1);
        buy(1, 0, 2);
        buy(1, 1, 0);
        place(1, BuildingType::LAMP, 1, 1);
        place(1, BuildingType::SOLAR_PANEL, 0, 0);
        place(1, BuildingType::SOLAR_PANEL, 1, 0);
        place(1, BuildingType::WIND_TURBINE, 2, 1);
        place(1, BuildingType::BATTERY, 0, 2);
        place(1, BuildingType::BATTERY, 2, 2);
        place(1, BuildingType::SOLAR_PANEL, 3, 0);
        place(1, BuildingType::WIND_TURBINE, 4, 1);
        place(1, BuildingType::HYDRO_PLANT, 7, 1);
        place(1, BuildingType::LAMP, 1, 4);

        // East (P2, the bot): start plot (row 0, col 2), river-bank plot (row 0, col 0)
        buy(2, 0, 1);
        buy(2, 0, 0);
        place(2, BuildingType::LAMP, 7, 1);
        place(2, BuildingType::SOLAR_PANEL, 8, 0);
        if (fullEast) place(2, BuildingType::SOLAR_PANEL, 7, 0);
        place(2, BuildingType::WIND_TURBINE, 6, 1);
        if (fullEast) {
            place(2, BuildingType::BATTERY, 8, 2);
            place(2, BuildingType::WIND_TURBINE, 4, 1);
            place(2, BuildingType::HYDRO_PLANT, 1, 1);
        }

        setResources(engine.getPlayerEconomyMut(1), 30, 12, 7, 2, 3, 0, 420, 260);
        setResources(engine.getPlayerEconomyMut(2), 22, 30, 16, 9, 12, 5, 380, 240);
    };

    // Cursor parking spots: P1 on a free slot of its land, P2 (bot) beside its plots
    auto parkCursors = [&]() {
        p1GridCol = 5;
        p1GridRow = 2;
        p1Pos = engine.getGridSlot(1, p1GridCol, p1GridRow);
        p2Pos = { 1180.0f, 470.0f };
        engine.getClosestGridIndex(2, p2Pos, p2GridCol, p2GridRow);
    };

    // The "new match" notice would never fade (screenshot mode reads no input and runs no timers)
    notices.clear();
    if (scene != "tutorial") tutorial.skip();

    if (scene == "storm") {
        // Find the first seed (from the requested one) whose day 1 has a storm in a sector
        unsigned int seed = 1;
        if (const char* env = std::getenv("EC_SEED")) seed = static_cast<unsigned int>(std::strtoul(env, nullptr, 10));
        for (unsigned int tries = 0; tries < 500; ++tries, ++seed) {
            ui::shot::setSeedEnv(seed);
            engine.restartGame();
            if (engine.getPlayerWeather(1) == WeatherType::STORMY || engine.getPlayerWeather(2) == WeatherType::STORMY) break;
        }
        std::cout << "[Shot] Storm scene uses seed " << seed << "\n";
        restartMatch(); // full UI reset with that seed (same weather)
        notices.clear();
        tutorial.skip();
        populate();
        advanceTo(1, 11.0f);
        // No random strikes during the capture; one bolt is fired a few frames before it instead
        lightningStrikeCooldown = 999.0f;
        lightningStrikeCooldownP2 = 999.0f;
        debugBoltCountdown = std::max(0, frames - 6);
        parkCursors();
        return;
    }

    if (scene == "victory") {
        // P1 builds river hydro plants and wind turbines; P2 builds nothing. From day 3 the city
        // shifts 10-15% per day to P1 until the 85% victory share is reached.
        setResources(engine.getPlayerEconomyMut(1), 600, 600, 600, 600, 600, 600, 4000, 0);
        buy(1, 0, 2);
        buy(1, 1, 2);
        buy(1, 0, 1);
        place(1, BuildingType::HYDRO_PLANT, 7, 1);
        place(1, BuildingType::HYDRO_PLANT, 6, 2);
        place(1, BuildingType::HYDRO_PLANT, 8, 4);
        place(1, BuildingType::WIND_TURBINE, 4, 1);
        place(1, BuildingType::WIND_TURBINE, 0, 1);
        place(1, BuildingType::SOLAR_PANEL, 3, 0);
        place(1, BuildingType::BATTERY, 1, 2);
        advanceTo(Balance::FINAL_DAY + 1, 7.0f);
        parkCursors();
        return;
    }

    // --- Information screens (dashboard, post-match report, event log, developer overlay) ---------
    // P1 starts from the populated board, P2 from a small one: as the city demand grows P2 falls short
    // and loses territory, until it builds four more plants on day EXPAND_DAY and keeps up again.
    // The telemetry takes its first snapshot after the starting board (so the granted stock is not
    // counted as mined) and is then fed through every fast-forward step, so the charts, the report
    // and the event log have the whole history.
    const bool reportScene = (scene == "report" || scene == "report-charts" || scene == "report-mix");
    if (reportScene || scene == "dashboard" || scene == "eventlog" || scene == "dev") {
        constexpr int EXPAND_DAY = 10;
        auto expandP2 = [&]() {
            PlayerEconomy& e = engine.getPlayerEconomyMut(2);
            const PlayerEconomy before = e;
            setResources(e, 400, 400, 400, 400, 400, 400, 3000, before.money);
            place(2, BuildingType::SOLAR_PANEL, 7, 0);
            place(2, BuildingType::BATTERY, 8, 2);
            place(2, BuildingType::WIND_TURBINE, 4, 1);
            place(2, BuildingType::HYDRO_PLANT, 1, 1);
            // the plants are a gift: the stock is put back, so the report shows no spending for them
            setResources(e, before.wood, before.iron, before.copper, before.coal, before.silicon, before.silver,
                         before.gold, before.money);
        };
        // Fast-forward to a day and hour (with P2's expansion at noon of EXPAND_DAY on the way)
        auto playTo = [&](int day, float hour) {
            if (day > EXPAND_DAY || (day == EXPAND_DAY && hour > 12.0f)) {
                advanceTo(EXPAND_DAY, 12.0f, true);
                expandP2();
            }
            advanceTo(day, hour, true);
        };
        populate(false);
        updateInfoUI(0.0f);

        if (reportScene) {
            // Play the match to its end (the fast-forward stops as soon as there is a winner)
            playTo(Balance::FINAL_DAY + 1, 7.0f);
            postMatch.handleKey(scene == "report-charts" ? sf::Keyboard::Key::Num2
                                : scene == "report-mix"  ? sf::Keyboard::Key::Num3
                                                         : sf::Keyboard::Key::Num1);
        } else if (scene == "dashboard") {
            playTo(EXPAND_DAY, 18.0f);
            forceDashboard = true; // [Tab] cannot be held in screenshot mode
        } else if (scene == "eventlog") {
            playTo(EXPAND_DAY, 18.0f);
            isPaused = true;
            pauseSelectedIdx = 3; // the pause menu entry that opens the log
            openEventLog();
        } else { // dev: [F3] panel with the time multiplier raised twice ([F7] [F7] = x4)
            playTo(2, 13.0f);
            devOverlay.toggle();
            devOverlay.handleKey(sf::Keyboard::Key::F7);
            devOverlay.handleKey(sf::Keyboard::Key::F7);
        }
        parkCursors();
        return;
    }

    if (scene == "toasts") {
        // Two human players, so both corners get toasts (alerts for the bot's corner are not shown)
        setBotDifficulty(BotDifficulty::NONE);
        tutorial.skip();
        populate();
        advanceTo(1, 11.0f);
        parkCursors();

        // P1: a failed purchase, then a building lost to lightning
        triggerPlayerPopup(1, "ГРЕШКА", "Няма злато!", "НЕДОСТИГ НА ЗЛАТО! НУЖНО: 195 G",
                           "Продавайте ток на града за злато!", theme::Bad);
        InfoEvent lost;
        lost.type = InfoEventType::LOST_LIGHTNING;
        lost.player = 1;
        lost.building = BuildingType::WIND_TURBINE;
        onInfoEvent(lost);

        // P2: choosing a building, then a storm in its sector
        BuildingCost solar = engine.getBuildingCost(BuildingType::SOLAR_PANEL);
        triggerPlayerPopup(2, "СТРОЕЖ", solar.nameBg,
                           "Произвежда " + std::to_string(solar.basePowerMW) + " MW при слънце.",
                           "[ENTER]: Постави | [PgDn]: Следваща | [Del]: Отказ", theme::P2);
        InfoEvent storm;
        storm.type = InfoEventType::WEATHER_CHANGED;
        storm.player = 2;
        storm.value = static_cast<int>(WeatherType::STORMY);
        onInfoEvent(storm);

        // Both: the 06:00 settlement of day 4, which P1 won
        InfoEvent day;
        day.type = InfoEventType::DAY_SETTLED;
        day.value = 4;
        day.value2 = static_cast<int>(UI_matchStats::DayOutcome::P1_TOOK);
        day.share = 0.65f;
        day.demand = 45;
        day.avgMW[0] = 62;
        day.avgMW[1] = 31;
        onInfoEvent(day);
        return;
    }

    if (scene == "tutorial") {
        parkCursors();
        return;
    }

    if (scene == "winter") {
        // Winter starts on day 16 (seasons change every 5 days); build after fast-forwarding
        advanceTo(3 * Balance::DAYS_PER_SEASON + 1, 10.5f);
        populate();
        advanceGameSeconds(1.5f * Balance::SECONDS_PER_DAY / 24.0f);
        parkCursors();
        return;
    }

    // game, night, pause, help, modal: the populated board on day 1
    populate();
    if (scene == "night") {
        advanceTo(1, 22.5f);
    } else {
        advanceTo(1, 11.0f);
    }
    parkCursors();

    if (scene == "game") {
        // P1 is placing a wind turbine (selected with hotkey 2): ghost, selected card and popup
        engine.getPlayerEconomyMut(1).selectedBuilding = static_cast<int>(BuildingType::WIND_TURBINE);
        BuildingCost c = engine.getBuildingCost(BuildingType::WIND_TURBINE);
        triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg, "Добив: +" + std::to_string(c.basePowerMW) + " MW ток.",
                           "[SPACE]: Постави в грида | [X]: Отказ", theme::P1);
    } else if (scene == "mining") {
        // P1 mines wood: prompt over the station, cooldown on the card, 6x time badges, a notice
        if (const auto* st = nodes.getStation(1, ResourceType::WOOD)) {
            p1Pos = { st->bounds.position.x + st->bounds.size.x / 2.0f, st->bounds.position.y + 30.0f };
        }
        p1ResourceCooldown = 0.6f;
        engine.setTimeScale(Balance::MINE_SPEEDUP_MULT);
        spawnNotice("+12 Дърво", p1Pos + sf::Vector2f(0.0f, -25.0f), theme::Wood);
    } else if (scene == "pause") {
        isPaused = true;
        pauseSelectedIdx = 0;
    } else if (scene == "help") {
        isPaused = true;
        helpOpenedFromPause = false;
        showHelpOverlay = true;
    } else if (scene == "modal") {
        // P1 tries to build a hydro plant on the river bank without the resources for it
        engine.getPlayerEconomyMut(1).selectedBuilding = static_cast<int>(BuildingType::HYDRO_PLANT);
        p1GridCol = 8;
        p1GridRow = 0;
        p1Pos = engine.getGridSlot(1, p1GridCol, p1GridRow);
        executeP1Action();
    }
}
