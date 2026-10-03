// =============================================================================
// ENERGY CRISIS - MATCH OPTIONS & PROGRESSION TESTS                        [team b-options]
// F-03 match rules presets, F-24 mutators, F-35 charters, F-33 research lab, F-21 sandbox.
// Headless: this file + Game/scr/*.cpp ("make test"). Exits 1 when any check failed.
// =============================================================================
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include "../Game/includes/game_main.h"

namespace {

int g_checks = 0;
int g_failures = 0;
int g_groupFailures = 0;

#define CHECK(cond, details)                                                                   \
    do {                                                                                       \
        ++g_checks;                                                                            \
        if (!(cond)) {                                                                         \
            ++g_failures;                                                                      \
            ++g_groupFailures;                                                                 \
            std::cerr << "    FAIL (line " << __LINE__ << "): " << #cond << " | " << details << "\n"; \
        }                                                                                      \
    } while (0)

#define REQUIRE(cond, details)                                                                 \
    do {                                                                                       \
        ++g_checks;                                                                            \
        if (!(cond)) {                                                                         \
            std::cerr << "    FATAL (line " << __LINE__ << "): " << #cond << " | " << details << "\n"; \
            std::exit(1);                                                                      \
        }                                                                                      \
    } while (0)

void beginGroup(const std::string& name) {
    g_groupFailures = 0;
    std::cout << "\n[" << name << "]\n";
}
void endGroup() { std::cout << (g_groupFailures == 0 ? "  -> PASS\n" : "  -> FAIL\n"); }

bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

// Fresh engine with the given rules (seed fixed so weather rolls are reproducible)
void startMatch(GameEngine& e, MatchRules r) {
    if (r.seed == 0u) r.seed = 4242u;
    e.setMatchRules(r);
    e.init(1600.0f, 900.0f);
}

// Advance whole real seconds in 1/60 s frames
void runSeconds(GameEngine& e, float seconds) {
    int frames = static_cast<int>(std::lround(seconds * 60.0f));
    for (int i = 0; i < frames; ++i) e.update(1.0f / 60.0f);
}

// Run until the current day number changes (real time depends on the day length)
void runToNextDay(GameEngine& e) {
    int day = e.getCurrentDay();
    for (int guard = 0; guard < 60 * 600 && e.getCurrentDay() == day && e.getCityState().winner == 0; ++guard) {
        e.update(1.0f / 60.0f);
    }
}

void giveEverything(GameEngine& e, int player, int amount, int gold) {
    PlayerEconomy& p = e.getPlayerEconomyMut(player);
    p.wood = p.iron = p.copper = p.coal = p.silicon = p.silver = amount;
    p.gold = gold;
}

// P1 buys the river-bank plot (id 3) and builds one hydro plant on it
void buildP1Hydro(GameEngine& e) {
    giveEverything(e, 1, 500, 1000);
    std::string msg;
    REQUIRE(e.buyLandPlot(1, 3, msg), msg);
    REQUIRE(e.placeBuilding(1, BuildingType::HYDRO_PLANT, e.getGridSlot(1, 7, 1), msg), msg);
}

// ---------------------------------------------------------------------------
// F-03 match rules
// ---------------------------------------------------------------------------
void testPresets() {
    beginGroup("F-03 presets and validation");
    MatchRules std = MatchRules::fromPreset(MatchPreset::STANDARD);
    CHECK(near(std.daySeconds, Balance::SECONDS_PER_DAY) && std.graceDays == Balance::GRACE_PERIOD_DAYS &&
          std.finalDay == Balance::FINAL_DAY && near(std.victoryShare, Balance::VICTORY_SHARE) &&
          std.startDemandMW == Balance::STARTING_CITY_DEMAND_MW && std.demandGrowthMW == Balance::DAILY_DEMAND_INCREASE_MW,
          "STANDARD must equal the README / Balance constants");
    MatchRules def;
    CHECK(def.preset == MatchPreset::STANDARD && near(def.daySeconds, 90.0f) && def.finalDay == 20,
          "default MatchRules = STANDARD");
    CHECK(MatchRules::fromPreset(MatchPreset::ENDLESS).finalDay == 0, "ENDLESS has no final day");
    MatchRules blitz = MatchRules::fromPreset(MatchPreset::BLITZ);
    CHECK(blitz.daySeconds < 60.0f && blitz.finalDay <= 12 && blitz.victoryShare < 0.85f, "BLITZ is short");

    MatchRules bad;
    bad.daySeconds = 1.0f; bad.graceDays = 9; bad.finalDay = 3; bad.victoryShare = 2.0f; bad.miningMult = -1.0f;
    bad.mutators = 0xFFu;
    bad.validate();
    CHECK(bad.daySeconds >= MatchLimits::DAY_SECONDS_MIN, "day length clamped: " << bad.daySeconds);
    CHECK(bad.graceDays == MatchLimits::GRACE_MAX, "grace clamped: " << bad.graceDays);
    CHECK(bad.finalDay > bad.graceDays, "final day must come after the grace period: " << bad.finalDay);
    CHECK(bad.victoryShare <= 1.0f && bad.miningMult >= MatchLimits::MINING_MIN, "share / mining clamped");
    CHECK(bad.activeMutatorCount() == MAX_ACTIVE_MUTATORS, "at most 3 mutators survive validate()");

    MatchRules custom = MatchRules::fromPreset(MatchPreset::BLITZ);
    custom.applyPreset(MatchPreset::CUSTOM);
    CHECK(custom.preset == MatchPreset::CUSTOM && near(custom.daySeconds, blitz.daySeconds),
          "CUSTOM keeps the current values");
    endGroup();
}

void testStandardUnchanged() {
    beginGroup("F-03 STANDARD rules keep today's behaviour");
    GameEngine e;
    startMatch(e, MatchRules());
    CHECK(e.getCityState().lastCutMessage == "ДОБРЕ ДОШЛИ! ГРАТИСЕН ПЕРИОД: ПЪРВИТЕ 2 ДЕНА ГРАДЪТ ИСКА 0 ЕНЕРГИЯ ЗА РАЗВИТИЕ!",
          "welcome text unchanged: " << e.getCityState().lastCutMessage);
    runToNextDay(e);
    CHECK(e.getCityState().lastCutMessage == "ДЕН 1 ПРИКЛЮЧИ [ГРАТИСЕН ПЕРИОД]: ГРАДЪТ ИСКАШЕ 0 MW. ОЩЕ 1 ДЕН ЗА РАЗВИТИЕ!",
          "day 1 text unchanged: " << e.getCityState().lastCutMessage);
    runToNextDay(e);
    CHECK(e.getCityState().lastCutMessage == "ДЕН 2 ПРИКЛЮЧИ: КРАЙ НА ГРАТИСНИЯ ПЕРИОД! ОТ ДЕН 3 ГРАДЪТ ИЗИСКВА ЕНЕРГИЯ!",
          "day 2 text unchanged: " << e.getCityState().lastCutMessage);
    CHECK(e.getCityState().cityEnergyDemand == 30, "day 3 demand 30 MW: " << e.getCityState().cityEnergyDemand);
    runToNextDay(e);
    CHECK(e.getCityState().cityEnergyDemand == 45, "day 4 demand 45 MW");
    endGroup();
}

void testDayLengthAndBlitzVictory() {
    beginGroup("F-03 BLITZ: 45 s days, 1 grace day, win at 75%");
    GameEngine e;
    startMatch(e, MatchRules::fromPreset(MatchPreset::BLITZ));
    e.sandboxSetWeather(1, WeatherType::WINDY); // hydro x1.0, deterministic
    // A Blitz day lasts 45 real seconds: from 08:00 the day ends after 45 * 22/24 s
    float realToRollover = 45.0f * (22.0f / 24.0f);
    runSeconds(e, realToRollover - 1.0f);
    CHECK(e.getCurrentDay() == 1, "still day 1 one second before 06:00");
    runSeconds(e, 2.0f);
    CHECK(e.getCurrentDay() == 2, "day 2 after one 45 s day, got day " << e.getCurrentDay());
    CHECK(e.getCityState().cityEnergyDemand == 30, "1 grace day: demand 30 MW on day 2, got " << e.getCityState().cityEnergyDemand);

    buildP1Hydro(e);
    runToNextDay(e); // day 2 judged: P1 +15% -> 65%
    CHECK(e.getCityState().winner == 0, "no winner at 65%");
    runToNextDay(e); // day 3 judged: 80% >= 75% -> P1 wins
    CHECK(e.getCityState().winner == 1, "P1 wins at 80% with a 75% threshold, winner=" << e.getCityState().winner
          << " share=" << e.getCityState().p1CityShare);
    endGroup();
}

void testEndlessAndMarathon() {
    beginGroup("F-03 ENDLESS has no final day, MARATHON has 3 grace days");
    GameEngine e;
    startMatch(e, MatchRules::fromPreset(MatchPreset::ENDLESS));
    for (int d = 0; d < 24 && e.getCityState().winner == 0; ++d) runToNextDay(e);
    CHECK(e.getCityState().winner == 0 && e.getCurrentDay() >= 24,
          "no draw after day 20 in ENDLESS (day " << e.getCurrentDay() << ", winner " << e.getCityState().winner << ")");
    CHECK(e.getDaysLeft() == -1, "days left -1 when endless");

    GameEngine s;
    startMatch(s, MatchRules());
    for (int d = 0; d < 20 && s.getCityState().winner == 0; ++d) runToNextDay(s);
    CHECK(s.getCityState().winner == 3, "STANDARD still ends in a draw after day 20 at 50/50");

    GameEngine m;
    startMatch(m, MatchRules::fromPreset(MatchPreset::MARATHON));
    for (int d = 0; d < 2; ++d) runToNextDay(m);
    CHECK(m.getCurrentDay() == 3 && m.getCityState().cityEnergyDemand == 0, "day 3 still grace in MARATHON");
    runToNextDay(m);
    CHECK(m.getCityState().cityEnergyDemand == 30, "day 4 demand 30 MW in MARATHON");
    CHECK(m.getDaysLeft() == 35 - 4 + 1, "days left " << m.getDaysLeft());
    endGroup();
}

void testRulesSurviveRestart() {
    beginGroup("F-03 rules survive restartGame(), research does not");
    GameEngine e;
    MatchRules r = MatchRules::fromPreset(MatchPreset::MARATHON);
    r.charter[0] = CharterType::CITY_INSIDER;
    startMatch(e, r);
    e.getPlayerEconomyMut(1).money = 5000;
    std::string msg;
    REQUIRE(e.researchTech(1, 0, 0, 0, msg), msg);
    e.restartGame();
    CHECK(e.getMatchRules().preset == MatchPreset::MARATHON && e.getMatchRules().charterOf(1) == CharterType::CITY_INSIDER,
          "rules kept");
    CHECK(e.getTechChoice(1, 0, 0) == -1, "research reset by restart");
    endGroup();
}

// ---------------------------------------------------------------------------
// F-24 mutators
// ---------------------------------------------------------------------------
void testMutators() {
    beginGroup("F-24 mutators");
    MatchRules r;
    CHECK(r.toggleMutator(MUT_ETERNAL_WINTER) && r.toggleMutator(MUT_RICH_VEINS) && r.toggleMutator(MUT_NO_GRACE),
          "three mutators can be enabled");
    CHECK(!r.toggleMutator(MUT_HEAD_START) && r.activeMutatorCount() == 3, "a 4th mutator is refused");
    CHECK(r.toggleMutator(MUT_NO_GRACE) && !r.hasMutator(MUT_NO_GRACE), "toggling again disables it");

    { // Eternal Winter
        MatchRules m; m.mutators = MUT_ETERNAL_WINTER;
        GameEngine e; startMatch(e, m);
        bool allWinter = (e.getSeason() == SeasonType::WINTER);
        for (int d = 0; d < 12; ++d) { runToNextDay(e); allWinter = allWinter && e.getSeason() == SeasonType::WINTER; }
        CHECK(allWinter, "season stays winter through day 13");
    }
    { // Mirror Weather
        MatchRules m; m.mutators = MUT_MIRROR_WEATHER;
        GameEngine e; startMatch(e, m);
        bool same = (e.getPlayerWeather(1) == e.getPlayerWeather(2));
        for (int d = 0; d < 15; ++d) { runToNextDay(e); same = same && e.getPlayerWeather(1) == e.getPlayerWeather(2); }
        CHECK(same, "both sectors share the weather every day");
    }
    { // Rich Veins
        MatchRules m; m.mutators = MUT_RICH_VEINS;
        GameEngine e; startMatch(e, m);
        std::string msg;
        int before = e.getPlayerEconomy(1).wood;
        e.mineResource(1, ResourceType::WOOD, msg);
        CHECK(e.getPlayerEconomy(1).wood - before == 2 * Balance::WOOD_BASE_YIELD, "wood yield doubled");
        CHECK(e.getMineYield(1, ResourceType::WOOD) == 2 * Balance::WOOD_BASE_YIELD, "getMineYield matches");
    }
    { // No Grace
        MatchRules m; m.mutators = MUT_NO_GRACE;
        GameEngine e; startMatch(e, m);
        CHECK(e.getCityState().cityEnergyDemand == 30 && !e.isGracePeriod(), "demand on day 1");
        runToNextDay(e);
        CHECK(e.getCityState().cityEnergyDemand == 45, "day 2 demand grows: " << e.getCityState().cityEnergyDemand);
    }
    { // Building Boom
        MatchRules m; m.mutators = MUT_BUILDING_BOOM;
        GameEngine e; startMatch(e, m);
        BuildingCost c = e.getBuildingCostFor(1, BuildingType::WIND_TURBINE);
        CHECK(c.ironCost == 8 && c.woodCost == 5, "wind 14 iron -> 8, 8 wood -> 5 (got " << c.ironCost << ", " << c.woodCost << ")");
        giveEverything(e, 1, 8, 0); // enough for the discounted wind turbine only
        e.getPlayerEconomyMut(1).iron = 8;
        std::string msg;
        CHECK(e.placeBuilding(1, BuildingType::WIND_TURBINE, e.getGridSlot(1, 0, 0), msg), msg);
        CHECK(e.getPlayerEconomy(1).iron == 0, "paid the discounted price");
    }
    { // Volatile City
        MatchRules m; m.mutators = MUT_VOLATILE_CITY;
        GameEngine e; startMatch(e, m);
        e.sandboxSetWeather(1, WeatherType::WINDY);
        buildP1Hydro(e);
        for (int d = 0; d < 3; ++d) runToNextDay(e); // day 3 judged
        CHECK(near(e.getCityState().p1CityShare, 0.80f, 1e-3f), "15% x2 = 30% in one day, share " << e.getCityState().p1CityShare);
    }
    { // Hungry City
        MatchRules m; m.mutators = MUT_HUNGRY_CITY;
        GameEngine e; startMatch(e, m);
        for (int d = 0; d < 3; ++d) runToNextDay(e);
        CHECK(e.getCityState().cityEnergyDemand == 60, "day 4 demand 30+30: " << e.getCityState().cityEnergyDemand);
    }
    { // Head Start
        MatchRules m; m.mutators = MUT_HEAD_START;
        GameEngine e; startMatch(e, m);
        CHECK(e.getPlayerEconomy(1).iron == 40 && e.getPlayerEconomy(2).silver == 40 && e.getPlayerEconomy(1).gold == 300,
              "both start with 40 of each resource and 300 gold");
    }
    endGroup();
}

// ---------------------------------------------------------------------------
// F-35 charters
// ---------------------------------------------------------------------------
void testCharters() {
    beginGroup("F-35 charters");
    // Every expectation comes from the charter table, so retuning a number keeps these checks valid
    auto scaled = [](int base, float mult) { return std::max(1, static_cast<int>(std::lround(base * mult))); };
    const CharterDef& solar = MatchInfo::charterDef(CharterType::SOLAR_COOP);
    const CharterDef& hydro = MatchInfo::charterDef(CharterType::HYDRO_HOLDING);
    const CharterDef& mining = MatchInfo::charterDef(CharterType::MINING_SYNDICATE);
    const CharterDef& insider = MatchInfo::charterDef(CharterType::CITY_INSIDER);
    const CharterDef& night = MatchInfo::charterDef(CharterType::NIGHT_SHIFT);
    { // Solar Co-op vs none: cheaper panels, more output at the same weather and hour
        MatchRules m; m.charter[0] = CharterType::SOLAR_COOP;
        GameEngine e; startMatch(e, m);
        BuildingCost c1 = e.getBuildingCostFor(1, BuildingType::SOLAR_PANEL);
        BuildingCost c2 = e.getBuildingCostFor(2, BuildingType::SOLAR_PANEL);
        CHECK(solar.solarCost < 1.0f && c1.siliconCost == scaled(8, solar.solarCost) && c2.siliconCost == 8,
              "solar silicon 8 -> " << c1.siliconCost << " for P1 only");
        giveEverything(e, 1, 100, 0);
        giveEverything(e, 2, 100, 0);
        std::string msg;
        REQUIRE(e.placeBuilding(1, BuildingType::SOLAR_PANEL, e.getGridSlot(1, 0, 0), msg), msg);
        REQUIRE(e.placeBuilding(2, BuildingType::SOLAR_PANEL, e.getGridSlot(2, 8, 0), msg), msg);
        e.sandboxSetWeather(1, WeatherType::SUNNY);
        e.sandboxSetWeather(2, WeatherType::SUNNY);
        e.sandboxSetHour(12.5f);
        float o1 = e.projectGenerationMW(1, 12.5f), o2 = e.projectGenerationMW(2, 12.5f);
        CHECK(solar.solarOutput > 1.0f && o2 > 0.0f && near(o1 / o2, solar.solarOutput, 1e-3f),
              "P1 solar x" << solar.solarOutput << ": " << o1 << " vs " << o2);
        CHECK(near(static_cast<float>(e.getPlayerEconomy(1).energyMW) / e.getPlayerEconomy(2).energyMW, solar.solarOutput, 0.02f),
              "live grid follows the perk: " << e.getPlayerEconomy(1).energyMW << " vs " << e.getPlayerEconomy(2).energyMW);
    }
    { // Hydro Holding: cheaper and stronger hydro on the river bank
        MatchRules m; m.charter[0] = CharterType::HYDRO_HOLDING;
        GameEngine e; startMatch(e, m);
        BuildingCost c1 = e.getBuildingCostFor(1, BuildingType::HYDRO_PLANT);
        CHECK(hydro.hydroCost < 1.0f && c1.ironCost == scaled(20, hydro.hydroCost) && e.getBuildingCostFor(2, BuildingType::HYDRO_PLANT).ironCost == 20,
              "hydro iron 20 -> " << c1.ironCost << " for P1 only");
        CHECK(hydro.hydroOutput > 1.0f && near(e.getPlayerPerks(1).hydroOutputMult, hydro.hydroOutput) &&
              near(e.getPlayerPerks(2).hydroOutputMult, 1.0f), "hydro output x" << hydro.hydroOutput);
    }
    { // Mining Syndicate
        MatchRules m; m.charter[1] = CharterType::MINING_SYNDICATE;
        GameEngine e; startMatch(e, m);
        int wood = static_cast<int>(std::round(12 * mining.miningYield));
        CHECK(mining.miningYield > 1.0f && e.getMineYield(2, ResourceType::WOOD) == wood && e.getMineYield(1, ResourceType::WOOD) == 12,
              "wood 12 -> " << wood << " for P2");
        CHECK(e.getMineUpgradeCost(2, ResourceType::IRON) == scaled(30, mining.mineUpgradeCost) && e.getMineUpgradeCost(1, ResourceType::IRON) == 30,
              "upgrade 30 -> " << scaled(30, mining.mineUpgradeCost) << " gold");
    }
    { // City Insider: cheaper land, more money
        MatchRules m; m.charter[0] = CharterType::CITY_INSIDER;
        GameEngine e; startMatch(e, m);
        int p1Plot2 = 0, p2Plot14 = 0;
        for (const auto& p : e.getLandPlots()) {
            if (p.id == 2) p1Plot2 = p.costGold;
            if (p.id == 14) p2Plot14 = p.costGold;
        }
        CHECK(insider.landCost < 1.0f && p1Plot2 == scaled(195, insider.landCost) && p2Plot14 == 195,
              "plot 195 G -> " << scaled(195, insider.landCost) << " G for P1 (" << p1Plot2 << ", " << p2Plot14 << ")");
        giveEverything(e, 1, 100, 0);
        giveEverything(e, 2, 100, 0);
        std::string msg;
        REQUIRE(e.placeBuilding(1, BuildingType::WIND_TURBINE, e.getGridSlot(1, 0, 0), msg), msg);
        REQUIRE(e.placeBuilding(2, BuildingType::WIND_TURBINE, e.getGridSlot(2, 8, 0), msg), msg);
        e.sandboxSetWeather(1, WeatherType::WINDY);
        e.sandboxSetWeather(2, WeatherType::WINDY);
        runSeconds(e, 10.0f);
        float ratio = static_cast<float>(e.getPlayerEconomy(1).money) / std::max(1, e.getPlayerEconomy(2).money);
        CHECK(insider.income > 1.0f && near(ratio, insider.income, 0.03f),
              "P1 earns x" << insider.income << ": " << e.getPlayerEconomy(1).money << " vs " << e.getPlayerEconomy(2).money);
    }
    { // Night Shift: bigger batteries, cheaper lamps to run
        MatchRules m; m.charter[0] = CharterType::NIGHT_SHIFT;
        GameEngine e; startMatch(e, m);
        giveEverything(e, 1, 100, 0);
        std::string msg;
        REQUIRE(e.placeBuilding(1, BuildingType::BATTERY, e.getGridSlot(1, 0, 0), msg), msg);
        float cap = 0.0f;
        for (const auto& b : e.getBuildings()) if (b.type == BuildingType::BATTERY) cap = b.maxCapacity;
        CHECK(night.batteryCapacity > 1.0f && near(cap, 200.0f * night.batteryCapacity), "battery " << 200.0f * night.batteryCapacity << " MWh, got " << cap);
        CHECK(night.lampDraw < 1.0f && near(e.getLampDrawFor(1), 10.0f * night.lampDraw) && near(e.getLampDrawFor(2), 10.0f),
              "lamp draw " << 10.0f * night.lampDraw << " MW for P1");
    }
    // Generated texts: every charter has a name and a bonus line; every real charter also has a
    // drawback line, and the lines quote the table numbers
    for (int c = 0; c < static_cast<int>(CharterType::COUNT); ++c) {
        CharterType ct = static_cast<CharterType>(c);
        std::string desc = MatchInfo::charterDescription(ct), minus = MatchInfo::charterDrawback(ct);
        CHECK(std::string(MatchInfo::charterName(ct)) != "?" && !desc.empty(), "charter " << c << " has a name and a description");
        if (ct == CharterType::NONE) {
            CHECK(minus.empty(), "no drawback without a charter");
        } else {
            CHECK(desc.find('%') != std::string::npos && minus.rfind("Минус: ", 0) == 0,
                  "charter " << c << " bonus '" << desc << "' and drawback '" << minus << "'");
        }
    }
    auto pct = [](float mult) {
        int p = static_cast<int>(std::lround((mult - 1.0f) * 100.0f));
        return (p > 0 ? "+" : "") + std::to_string(p) + "%";
    };
    CHECK(std::string(MatchInfo::charterDescription(CharterType::SOLAR_COOP)).find(pct(solar.solarOutput) + " слънчева мощност") != std::string::npos,
          "solar text quotes " << pct(solar.solarOutput) << ": " << MatchInfo::charterDescription(CharterType::SOLAR_COOP));
    CHECK(std::string(MatchInfo::charterDescription(CharterType::NIGHT_SHIFT)).find("лампи " + pct(night.lampDraw) + " ток") != std::string::npos,
          "night text quotes the lamp draw: " << MatchInfo::charterDescription(CharterType::NIGHT_SHIFT));
    CHECK(std::string(MatchInfo::charterDrawback(CharterType::MINING_SYNDICATE)).find("всички централи") != std::string::npos,
          "the same output change on every generator reads as one effect: " << MatchInfo::charterDrawback(CharterType::MINING_SYNDICATE));
    endGroup();
}

// ---------------------------------------------------------------------------
// F-33 research lab
// ---------------------------------------------------------------------------
void testResearch() {
    beginGroup("F-33 research lab");
    GameEngine e;
    startMatch(e, MatchRules());
    CHECK(e.getTechStatus(1, 0, 0, 0) == TechStatus::UNAFFORDABLE, "tier 1 needs money");
    CHECK(e.getTechStatus(1, 0, 1, 0) == TechStatus::LOCKED, "tier 2 locked before tier 1");
    std::string msg;
    CHECK(!e.researchTech(1, 0, 0, 0, msg), "cannot research without money");

    PlayerEconomy& p = e.getPlayerEconomyMut(1);
    p.money = 4500;
    CHECK(e.getTechStatus(1, 0, 0, 1) == TechStatus::AVAILABLE, "affordable now");
    REQUIRE(e.researchTech(1, 0, 0, 0, msg), msg);
    CHECK(e.getPlayerEconomy(1).money == 500, "4 000 $ paid, money " << e.getPlayerEconomy(1).money);
    CHECK(e.getTechStatus(1, 0, 0, 0) == TechStatus::RESEARCHED && e.getTechStatus(1, 0, 0, 1) == TechStatus::EXCLUDED,
          "pick 1 of 2");
    CHECK(!e.researchTech(1, 0, 0, 1, msg), "the other option of the tier is refused");
    CHECK(e.getTechStatus(1, 0, 1, 0) == TechStatus::UNAFFORDABLE, "tier 2 unlocked, 15 000 $ needed");
    CHECK(near(e.getPlayerPerks(1).solarOutputMult, 1.15f) && near(e.getPlayerPerks(2).solarOutputMult, 1.0f),
          "solar +15% for P1 only");
    CHECK(e.getResearchedTierCount(1, 0) == 1 && e.getResearchedTierCount(1, 1) == 0, "tier counters");

    // Storage: existing batteries grow with capacity research
    giveEverything(e, 1, 100, 0);
    REQUIRE(e.placeBuilding(1, BuildingType::BATTERY, e.getGridSlot(1, 1, 1), msg), msg);
    p.money = 4000;
    REQUIRE(e.researchTech(1, 1, 0, 0, msg), msg);
    float cap = 0.0f;
    for (const auto& b : e.getBuildings()) if (b.type == BuildingType::BATTERY) cap = b.maxCapacity;
    CHECK(near(cap, 300.0f), "existing battery 200 -> 300 MWh, got " << cap);

    // Extraction tier 3 (land) re-prices unbought plots
    p.money = 4000 + 15000 + 40000;
    REQUIRE(e.researchTech(1, 2, 0, 1, msg), msg);
    CHECK(near(e.getMiningCooldown(1), Balance::MINE_COOLDOWN_SEC * 0.75f), "automation: -25% mining time");
    REQUIRE(e.researchTech(1, 2, 1, 0, msg), msg);
    CHECK(e.getBuildingCostFor(1, BuildingType::HYDRO_PLANT).woodCost == 13, "modular: hydro wood 15 -> 13");
    REQUIRE(e.researchTech(1, 2, 2, 1, msg), msg);
    int plot2 = 0;
    for (const auto& lp : e.getLandPlots()) if (lp.id == 2) plot2 = lp.costGold;
    CHECK(plot2 == 137, "land 195 G -> 137 G after Земна борса, got " << plot2);
    CHECK(e.getPlayerEconomy(1).money == 0, "all money spent");

    // Bot helper researches the cheapest affordable tier
    GameEngine b;
    startMatch(b, MatchRules());
    b.getPlayerEconomyMut(2).money = 3999;
    CHECK(!b.autoResearch(2, msg), "nothing affordable at 3 999 $");
    b.getPlayerEconomyMut(2).money = 8000;
    CHECK(b.autoResearch(2, msg) && b.autoResearch(2, msg), "two tier-1 techs for 8 000 $");
    CHECK(b.getResearchedTierCount(2, 0) == 1 && b.getResearchedTierCount(2, 2) == 1, "generation and extraction first");
    CHECK(!b.autoResearch(2, msg), "out of money");
    for (int br = 0; br < TECH_BRANCHES; ++br)
        for (int t = 0; t < TECH_TIERS; ++t)
            for (int o = 0; o < TECH_OPTIONS; ++o)
                CHECK(std::string(MatchInfo::techInfo(br, t, o).name) != "?", "tech name " << br << t << o);
    endGroup();
}

// ---------------------------------------------------------------------------
// F-21 sandbox
// ---------------------------------------------------------------------------
void testSandbox() {
    beginGroup("F-21 practice sandbox");
    MatchRules r; r.sandbox = true;
    GameEngine e;
    startMatch(e, r);
    CHECK(e.getPlayerEconomy(1).iron >= 9999 && e.getPlayerEconomy(1).gold >= 9999, "infinite resources from the start");
    std::string msg;
    REQUIRE(e.buyLandPlot(1, 3, msg), msg);
    REQUIRE(e.placeBuilding(1, BuildingType::HYDRO_PLANT, e.getGridSlot(1, 7, 1), msg), msg);
    e.update(1.0f / 60.0f);
    CHECK(e.getPlayerEconomy(1).iron >= 9999, "refilled after spending");

    // Clock control
    e.sandboxSetClockSpeed(0.0f);
    float h0 = e.getHour24();
    runSeconds(e, 5.0f);
    CHECK(near(e.getHour24(), h0), "paused clock does not move");
    e.sandboxSetClockSpeed(4.0f);
    runSeconds(e, 3.75f); // 1 game-hour at 1x -> 4 game-hours at 4x
    CHECK(near(e.getHour24(), h0 + 4.0f, 0.05f), "4x speed: " << h0 << " -> " << e.getHour24());
    e.sandboxSetClockSpeed(1.0f);
    int day = e.getCurrentDay();
    e.sandboxSetHour(22.0f);
    CHECK(near(e.getHour24(), 22.0f, 0.01f) && e.getCurrentDay() == day, "jump to 22:00 same day");
    e.sandboxSetHour(3.0f);
    CHECK(near(e.getHour24(), 3.0f, 0.01f) && e.getCurrentDay() == day, "jump to 03:00 (end of the same day)");

    // Weather / season / demand locks survive day ends
    e.sandboxSetWeather(1, WeatherType::STORMY);
    e.sandboxSetSeason(static_cast<int>(SeasonType::WINTER));
    e.sandboxSetDemand(500);
    for (int d = 0; d < 6; ++d) runToNextDay(e);
    CHECK(e.getPlayerWeather(1) == WeatherType::STORMY, "weather lock kept");
    CHECK(e.getSeason() == SeasonType::WINTER, "season lock kept");
    CHECK(e.getCityState().cityEnergyDemand == 500, "demand lock kept: " << e.getCityState().cityEnergyDemand);
    e.sandboxSetSeason(-1);
    e.sandboxUnlockWeather();
    CHECK(!e.isSandboxWeatherLocked(1) && e.getSandboxSeasonOverride() == -1, "locks released");

    // Never ends even when P1 dominates
    e.sandboxSetDemand(50);
    for (int d = 0; d < 25; ++d) runToNextDay(e);
    CHECK(e.getCityState().winner == 0, "no winner in the sandbox (share " << e.getCityState().p1CityShare << ")");

    // 24 h projection
    e.sandboxSetWeather(1, WeatherType::WINDY);
    float hNoon = e.projectGenerationMW(1, 12.0f), hNight = e.projectGenerationMW(1, 0.0f);
    CHECK(near(hNoon, 110.0f) && near(hNight, 110.0f), "hydro is flat over 24 h: " << hNoon << " / " << hNight);
    REQUIRE(e.placeBuilding(1, BuildingType::SOLAR_PANEL, e.getGridSlot(1, 0, 0), msg) ||
            (e.sandboxSetHour(12.0f), e.placeBuilding(1, BuildingType::SOLAR_PANEL, e.getGridSlot(1, 0, 0), msg)), msg);
    CHECK(e.projectGenerationMW(1, 0.0f) < e.projectGenerationMW(1, 12.0f) - 30.0f, "solar adds output at noon only");
    CHECK(e.sandboxClearBuildings(1) == 2 && e.getBuildings().empty(), "clear removes P1's buildings");
    endGroup();
}

} // namespace

int main() {
    std::cout << "=== Energy Crisis: match options & progression tests [b-options] ===\n";
    testPresets();
    testStandardUnchanged();
    testDayLengthAndBlitzVictory();
    testEndlessAndMarathon();
    testRulesSurviveRestart();
    testMutators();
    testCharters();
    testResearch();
    testSandbox();
    std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    if (g_failures > 0) {
        std::cout << "RESULT: FAIL\n";
        return 1;
    }
    std::cout << "RESULT: PASS\n";
    return 0;
}
