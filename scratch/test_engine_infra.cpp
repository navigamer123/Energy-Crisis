// =============================================================================
// ENERGY CRISIS - ENGINE INFRASTRUCTURE TESTS
// Event queue, match configuration, player modifiers, deterministic RNG, fixed timestep and
// snapshots. Headless: built from this file + Game/scr/*.cpp only, no SFML needed ("make test").
// Every check is reported; the program exits with 1 when any check failed.
// =============================================================================
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
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

// A failed precondition (a placement or purchase the test relies on) stops the program
#define REQUIRE(cond, details)                                                                 \
    do {                                                                                       \
        ++g_checks;                                                                            \
        if (!(cond)) {                                                                         \
            std::cerr << "    FATAL (line " << __LINE__ << "): " << #cond << " | " << details << "\n"; \
            std::exit(1);                                                                      \
        }                                                                                      \
    } while (0)

constexpr float kFrame = 1.0f / 60.0f;

void beginGroup(const std::string& name) {
    g_groupFailures = 0;
    std::cout << "\n[" << name << "]\n";
}

void endGroup() {
    std::cout << (g_groupFailures == 0 ? "  -> PASS\n" : "  -> FAIL\n");
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void giveResources(GameEngine& e, int player, int amount) {
    PlayerEconomy& p = e.getPlayerEconomyMut(player);
    p.wood = p.iron = p.copper = p.coal = p.silicon = p.silver = amount;
}

int startPlotId(int player) { return (player == 1) ? 1 : 15; }

// Grid position of slot `sub` (0..8, row-major) inside a land plot (ids 1-12 = P1, 13-24 = P2)
sf::Vector2f slotOf(const GameEngine& e, int player, int plotId, int sub) {
    int idx = (plotId - 1) % 12;
    int col = (idx % 3) * 3 + sub % 3;
    int row = (idx / 3) * 3 + sub / 3;
    return e.getGridSlot(player, col, row);
}

void place(GameEngine& e, int player, BuildingType type, sf::Vector2f pos) {
    std::string msg;
    bool ok = e.placeBuilding(player, type, pos, msg);
    REQUIRE(ok, "placeBuilding P" << player << " type " << static_cast<int>(type) << " failed: " << msg);
}

void runToNextDay(GameEngine& e, float dt) {
    const int day = e.getCurrentDay();
    long frames = 0;
    while (e.getCurrentDay() == day && e.getCityState().winner == 0) {
        e.update(dt);
        REQUIRE(++frames < 10000000L, "day " << day << " never ended");
    }
}

int countEvents(const std::vector<GameEvent>& events, GameEventType type) {
    return static_cast<int>(std::count_if(events.begin(), events.end(),
                                          [type](const GameEvent& ev) { return ev.type == type; }));
}

const GameEvent* findEvent(const std::vector<GameEvent>& events, GameEventType type) {
    for (const auto& ev : events) {
        if (ev.type == type) return &ev;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// [CD-05] Event queue
// ---------------------------------------------------------------------------
void testEvents() {
    beginGroup("Event queue (CD-05)");
    GameEngine e;
    e.init(1600.0f, 900.0f);

    std::vector<GameEvent> ev = e.pollEvents();
    CHECK(countEvents(ev, GameEventType::WEATHER_CHANGED) == 2, "init weather events: " << countEvents(ev, GameEventType::WEATHER_CHANGED));
    CHECK(countEvents(ev, GameEventType::MESSAGE) == 1, "init message events: " << countEvents(ev, GameEventType::MESSAGE));
    CHECK(e.pollEvents().empty(), "pollEvents must clear the queue");

    // Building placed
    giveResources(e, 1, 1000);
    const sf::Vector2f pos = slotOf(e, 1, startPlotId(1), 4);
    place(e, 1, BuildingType::SOLAR_PANEL, pos);
    ev = e.pollEvents();
    const GameEvent* placed = findEvent(ev, GameEventType::BUILDING_PLACED);
    CHECK(ev.size() == 1 && placed != nullptr, "expected exactly one BUILDING_PLACED event, got " << ev.size() << " events");
    if (placed) {
        CHECK(placed->player == 1, "player " << placed->player);
        CHECK(placed->subtype == static_cast<int>(BuildingType::SOLAR_PANEL), "subtype " << placed->subtype);
        CHECK(std::abs(placed->x - pos.x) < 0.01f && std::abs(placed->y - pos.y) < 0.01f, "position " << placed->x << "; " << placed->y);
        CHECK(placed->value == static_cast<float>(Balance::SOLAR_PANEL.basePowerMW), "value " << placed->value);
        CHECK(!placed->text.empty(), "building name missing");
    }

    // A failed action emits nothing
    std::string msg;
    CHECK(!e.placeBuilding(1, BuildingType::SOLAR_PANEL, pos, msg), "placed twice on one cell");
    CHECK(e.pollEvents().empty(), "a failed placement emitted events");

    // Mining, upgrading, buying land
    MineResult mined;
    REQUIRE(e.mineResource(2, ResourceType::IRON, mined, msg), msg);
    ev = e.pollEvents();
    const GameEvent* minedEv = findEvent(ev, GameEventType::RESOURCE_MINED);
    CHECK(minedEv && minedEv->player == 2 && minedEv->subtype == static_cast<int>(ResourceType::IRON) &&
              minedEv->value == static_cast<float>(mined.amount),
          "RESOURCE_MINED missing or wrong");

    e.getPlayerEconomyMut(1).gold = 100000;
    REQUIRE(e.upgradeMine(1, ResourceType::WOOD, msg), msg);
    ev = e.pollEvents();
    const GameEvent* up = findEvent(ev, GameEventType::MINE_UPGRADED);
    CHECK(up && up->player == 1 && up->subtype == static_cast<int>(ResourceType::WOOD) && up->value == 2.0f, "MINE_UPGRADED missing or wrong");

    REQUIRE(e.buyLandPlot(1, 2, msg), msg);
    ev = e.pollEvents();
    const GameEvent* land = findEvent(ev, GameEventType::LAND_BOUGHT);
    CHECK(land && land->player == 1 && land->subtype == 2 && land->value > 0.0f, "LAND_BOUGHT missing or wrong");

    // Demolition and destruction
    REQUIRE(e.removeBuilding(1, pos, msg), msg);
    ev = e.pollEvents();
    CHECK(countEvents(ev, GameEventType::BUILDING_REMOVED) == 1, "BUILDING_REMOVED missing");
    place(e, 1, BuildingType::WIND_TURBINE, pos);
    e.pollEvents();
    REQUIRE(e.breakBuildingAt(pos), "breakBuildingAt found nothing");
    ev = e.pollEvents();
    const GameEvent* destroyed = findEvent(ev, GameEventType::BUILDING_DESTROYED);
    CHECK(destroyed && destroyed->player == 1 && destroyed->subtype == static_cast<int>(BuildingType::WIND_TURBINE),
          "BUILDING_DESTROYED missing or wrong");

    // Day end: exactly one DAY_END + DAY_RESULT, new weather for both sectors
    runToNextDay(e, kFrame);
    ev = e.pollEvents();
    const GameEvent* dayEnd = findEvent(ev, GameEventType::DAY_END);
    CHECK(countEvents(ev, GameEventType::DAY_END) == 1 && dayEnd && dayEnd->value == 1.0f, "DAY_END missing or wrong");
    CHECK(countEvents(ev, GameEventType::DAY_RESULT) == 1, "DAY_RESULT count " << countEvents(ev, GameEventType::DAY_RESULT));
    const GameEvent* result = findEvent(ev, GameEventType::DAY_RESULT);
    CHECK(result && result->text == e.getCityState().lastCutMessage, "DAY_RESULT text differs from the day-end message");
    CHECK(countEvents(ev, GameEventType::WEATHER_CHANGED) == 2, "weather events at day end: " << countEvents(ev, GameEventType::WEATHER_CHANGED));
    CHECK(countEvents(ev, GameEventType::VICTORY) == 0, "victory on day 1");

    // Seasons change at midnight before day 6 (spring -> summer)
    while (e.getCurrentDay() < 6 && e.getCityState().winner == 0) {
        e.update(0.25f);
        for (const auto& item : e.pollEvents()) {
            if (item.type == GameEventType::SEASON_CHANGED) {
                CHECK(item.subtype == static_cast<int>(SeasonType::SUMMER), "season changed to " << item.subtype);
                CHECK(e.getCurrentDay() == 5, "season changed on day " << e.getCurrentDay());
            }
        }
    }

    // Victory event: six wind turbines against nothing win at the end of day 5
    GameEngine v;
    v.init(1600.0f, 900.0f);
    giveResources(v, 1, 1000);
    for (int i = 0; i < 6; ++i) place(v, 1, BuildingType::WIND_TURBINE, slotOf(v, 1, startPlotId(1), i));
    v.pollEvents();
    int dayResultsWonByP1 = 0;
    const GameEvent* victory = nullptr;
    std::vector<GameEvent> all;
    while (v.getCityState().winner == 0) {
        v.update(0.25f);
        for (auto& item : v.pollEvents()) all.push_back(item);
    }
    for (const auto& item : all) {
        if (item.type == GameEventType::DAY_RESULT && item.player == 1) {
            ++dayResultsWonByP1;
            CHECK(item.value > 0.0f, "P1 won a day with share change " << item.value);
        }
    }
    victory = findEvent(all, GameEventType::VICTORY);
    CHECK(dayResultsWonByP1 == 3, "P1 won " << dayResultsWonByP1 << " days, expected 3");
    CHECK(victory && victory->player == 1 && victory->value >= Balance::VICTORY_SHARE - 1e-4f, "VICTORY missing or wrong");
    CHECK(countEvents(all, GameEventType::VICTORY) == 1, "VICTORY count " << countEvents(all, GameEventType::VICTORY));

    // Nobody drains the queue: it stays bounded
    GameEngine idle;
    idle.init(1600.0f, 900.0f);
    for (int i = 0; i < 5000; ++i) idle.mineResource(1, ResourceType::WOOD, msg);
    std::vector<GameEvent> backlog = idle.pollEvents();
    CHECK(backlog.size() <= GameEngine::MAX_PENDING_EVENTS, "queue grew to " << backlog.size());
    CHECK(!backlog.empty() && backlog.back().type == GameEventType::RESOURCE_MINED, "newest events were dropped");
    endGroup();
}

// ---------------------------------------------------------------------------
// MatchConfig
// ---------------------------------------------------------------------------
void buildWindFarm(GameEngine& e, int player) {
    giveResources(e, player, 1000);
    for (int i = 0; i < 6; ++i) place(e, player, BuildingType::WIND_TURBINE, slotOf(e, player, startPlotId(player), i));
}

void testMatchConfig() {
    beginGroup("MatchConfig");
    {
        // Defaults are today's rules
        MatchConfig def;
        CHECK(def.finalDay == Balance::FINAL_DAY && def.victoryShare == Balance::VICTORY_SHARE &&
                  def.daySeconds == Balance::SECONDS_PER_DAY && def.graceDays == Balance::GRACE_PERIOD_DAYS &&
                  def.seed == 0u && def.mutators.empty() && def.mapPreset == 0 && !def.sandbox,
              "MatchConfig defaults differ from Balance");
        GameEngine e;
        e.init(1600.0f, 900.0f);
        const MatchConfig& c = e.getConfig();
        CHECK(c.finalDay == Balance::FINAL_DAY && c.victoryShare == Balance::VICTORY_SHARE &&
                  c.daySeconds == Balance::SECONDS_PER_DAY && c.graceDays == Balance::GRACE_PERIOD_DAYS && !c.sandbox,
              "init(w, h) does not use the default rules");
        CHECK(e.getCurrentDay() == 1 && std::abs(e.getHour24() - Balance::MATCH_START_HOUR) < 1e-4f, "start time");
        CHECK(e.getCityState().cityEnergyDemand == 0 && e.isGracePeriod(), "day 1 is a grace day by default");
    }
    {
        // Short match without grace: nothing built -> draw when day 3 ends
        MatchConfig cfg;
        cfg.finalDay = 3;
        cfg.graceDays = 0;
        GameEngine e;
        e.init(cfg);
        CHECK(e.getCityState().cityEnergyDemand == Balance::STARTING_CITY_DEMAND_MW && !e.isGracePeriod(), "no grace day 1");
        int guard = 0;
        while (e.getCityState().winner == 0 && ++guard < 100) e.update(10.0f);
        CHECK(e.getCurrentDay() == 4 && e.getCityState().winner == 3, "day " << e.getCurrentDay() << " winner " << e.getCityState().winner);
    }
    {
        // Lower victory share: one won day (+15%) is enough
        MatchConfig cfg;
        cfg.victoryShare = 0.6f;
        cfg.graceDays = 0;
        GameEngine e;
        e.init(cfg);
        buildWindFarm(e, 1);
        runToNextDay(e, 0.25f);
        CHECK(e.getCityState().winner == 1 && e.getCurrentDay() == 2, "winner " << e.getCityState().winner << " day " << e.getCurrentDay());
        CHECK(e.getCityState().lastCutMessage.find("60%") != std::string::npos, "message: " << e.getCityState().lastCutMessage);
    }
    {
        // Half-length days: day 1 (08:00 -> 06:00 = 22 h) lasts 41.25 game-seconds
        MatchConfig cfg;
        cfg.daySeconds = 45.0f;
        GameEngine e;
        e.init(cfg);
        CHECK(std::abs(e.getHour24() - Balance::MATCH_START_HOUR) < 1e-3f, "start hour " << e.getHour24());
        for (int i = 0; i < 165; ++i) e.update(0.25f); // 41.25 s
        CHECK(e.getCurrentDay() == 2, "day " << e.getCurrentDay() << " at hour " << e.getHour24());
        CHECK(std::abs(e.getHour24() - Balance::CLOCK_HOUR_AT_ZERO) < 0.2f, "hour " << e.getHour24());
        e.update(45.0f / 4.0f); // 6 game-hours later
        CHECK(std::abs(e.getHour24() - 12.0f) < 0.2f, "hour " << e.getHour24());
    }
    {
        // Sandbox: the share moves but nobody wins, even after the final day
        MatchConfig cfg;
        cfg.sandbox = true;
        cfg.mutators = { "test_mutator" };
        GameEngine e;
        e.init(cfg);
        buildWindFarm(e, 1);
        for (int day = 0; day < 25; ++day) e.update(Balance::SECONDS_PER_DAY);
        CHECK(e.getCityState().winner == 0, "sandbox winner " << e.getCityState().winner);
        CHECK(e.getCurrentDay() == 26, "sandbox day " << e.getCurrentDay());
        CHECK(e.getCityState().p1CityShare > 0.99f, "sandbox share " << e.getCityState().p1CityShare);
        CHECK(e.getConfig().hasMutator("test_mutator") && !e.getConfig().hasMutator("other"), "mutators not kept");

        // Restart keeps the rules
        e.restartGame();
        CHECK(e.getConfig().sandbox && e.getConfig().hasMutator("test_mutator"), "restart lost the config");
        CHECK(e.getCurrentDay() == 1 && e.getBuildings().empty(), "restart did not reset the match");
    }
    {
        // Out-of-range values are clamped
        MatchConfig cfg;
        cfg.finalDay = 0;
        cfg.victoryShare = 0.2f;
        cfg.daySeconds = -5.0f;
        cfg.graceDays = 7;
        GameEngine e;
        e.init(cfg);
        const MatchConfig& c = e.getConfig();
        CHECK(c.finalDay == MatchConfig::MIN_FINAL_DAY, "finalDay " << c.finalDay);
        CHECK(c.victoryShare == MatchConfig::MIN_VICTORY_SHARE, "victoryShare " << c.victoryShare);
        CHECK(c.daySeconds == MatchConfig::MIN_DAY_SECONDS, "daySeconds " << c.daySeconds);
        CHECK(c.graceDays == 0, "graceDays " << c.graceDays);
        cfg.victoryShare = std::nanf("");
        cfg.daySeconds = std::nanf("");
        e.init(cfg);
        CHECK(e.getConfig().victoryShare == Balance::VICTORY_SHARE && e.getConfig().daySeconds == Balance::SECONDS_PER_DAY,
              "NaN values not replaced by defaults");
    }
    endGroup();
}

// ---------------------------------------------------------------------------
// Deterministic scripted match: both players act every `actionSeconds` of real time (choices from
// the engine's own randInt stream), so the same seed must give the same match.
// ---------------------------------------------------------------------------
std::string fingerprint(const GameEngine& e) {
    std::ostringstream os;
    os.precision(9);
    const auto& c = e.getCityState();
    os << "day " << e.getCurrentDay() << " hour " << e.getHour24() << " season " << static_cast<int>(e.getSeason())
       << " share " << c.p1CityShare << " demand " << c.cityEnergyDemand << " winner " << c.winner
       << " delivered " << c.p1DailyDelivered << "/" << c.p2DailyDelivered << " msg " << c.lastCutMessage << "\n";
    for (int p = 1; p <= 2; ++p) {
        const auto& q = e.getPlayerEconomy(p);
        os << "P" << p << " $" << q.money << " g" << q.gold << " w" << q.wood << " fe" << q.iron << " cu" << q.copper
           << " c" << q.coal << " si" << q.silicon << " ag" << q.silver << " mw" << q.energyMW << " tier" << q.landTier
           << " weather " << static_cast<int>(e.getPlayerWeather(p)) << " wind " << e.getPlayerWindSpeed(p) << "\n";
    }
    for (const auto& b : e.getBuildings()) {
        os << "B" << static_cast<int>(b.type) << " p" << b.playerOwner << " " << b.position.x << "," << b.position.y
           << " out " << b.currentOutputMW << " st " << b.energyStored << " lr " << b.lightRadius << "\n";
    }
    return os.str();
}

void scriptedAction(GameEngine& e, long tick) {
    std::string msg;
    const int player = 1 + static_cast<int>(tick % 2);
    const ResourceType res = static_cast<ResourceType>(e.randInt(1, 7)); // WOOD..GOLD
    e.mineResource(player, res, msg);
    if (tick % 7 == 0) {
        const BuildingType type = static_cast<BuildingType>(e.randInt(1, 5)); // SOLAR..LAMP
        const int plotIdx = e.randInt(0, 2);
        const int sub = e.randInt(0, 8);
        const int plotId = (player == 1) ? (1 + plotIdx) : (15 - plotIdx);
        e.placeBuilding(player, type, slotOf(e, player, plotId, sub), msg);
    }
    if (tick % 11 == 0) e.buyNextLandTier(player, msg);
    if (tick % 13 == 0) e.upgradeMine(player, static_cast<ResourceType>(e.randInt(1, 7)), msg);
}

// Plays until `days` days have been settled (or the match ended); returns the fingerprint
std::string playScriptedMatch(GameEngine& e, int days, float dt, float actionSeconds) {
    const long framesPerAction = std::max(1L, std::lround(actionSeconds / dt));
    long frame = 0;
    long tick = 0;
    while (e.getCurrentDay() <= days && e.getCityState().winner == 0) {
        if (frame % framesPerAction == 0) scriptedAction(e, tick++);
        e.update(dt);
        ++frame;
        REQUIRE(frame < 50000000L, "scripted match does not end");
    }
    return fingerprint(e);
}

// ---------------------------------------------------------------------------
// [CD-03] Engine-owned deterministic RNG
// ---------------------------------------------------------------------------
void testDeterministicRng() {
    beginGroup("Engine-owned deterministic RNG (CD-03)");
    {
        // Portable generator: fixed reference values, ranges inclusive and unbiased enough
        GameRng a(42u, 7u), b(42u, 7u), c(42u, 8u);
        bool same = true, differs = false;
        for (int i = 0; i < 100; ++i) {
            uint32_t x = a.next();
            same = same && (x == b.next());
            differs = differs || (x != c.next());
        }
        CHECK(same, "same seed and stream gave different numbers");
        CHECK(differs, "different streams gave the same numbers");
        GameRng r(1u, 1u);
        int counts[6] = { 0, 0, 0, 0, 0, 0 };
        bool inRange = true;
        for (int i = 0; i < 60000; ++i) {
            int v = r.range(1, 6);
            inRange = inRange && v >= 1 && v <= 6;
            if (v >= 1 && v <= 6) ++counts[v - 1];
        }
        CHECK(inRange, "range(1, 6) left its bounds");
        for (int k = 0; k < 6; ++k) CHECK(counts[k] > 9000 && counts[k] < 11000, "face " << k + 1 << " came " << counts[k] << " times");
        CHECK(r.range(5, 5) == 5 && r.range(9, 3) >= 3, "degenerate ranges");
        float f = r.unit();
        CHECK(f >= 0.0f && f < 1.0f, "unit() = " << f);
    }
    {
        // The seed comes from MatchConfig first
        MatchConfig cfg;
        cfg.seed = 777u;
        GameEngine e;
        e.init(cfg);
        CHECK(e.getSeed() == 777u, "seed " << e.getSeed());
        GameEngine f;
        f.init(cfg);
        bool sameDraws = true;
        for (int i = 0; i < 50; ++i) sameDraws = sameDraws && (e.randInt(0, 1000000) == f.randInt(0, 1000000));
        CHECK(sameDraws, "randInt differs for the same seed");
        float x = e.randFloat();
        CHECK(x >= 0.0f && x < 1.0f, "randFloat " << x);
    }
    {
        // Same seed -> identical 10-day matches (weather, actions, economy, buildings)
        MatchConfig cfg;
        cfg.seed = 20261003u;
        cfg.sandbox = true; // no early victory: always 10 full days
        GameEngine a, b;
        a.init(cfg);
        b.init(cfg);
        const std::string fa = playScriptedMatch(a, 10, kFrame, 0.5f);
        const std::string fb = playScriptedMatch(b, 10, kFrame, 0.5f);
        CHECK(fa == fb, "two matches with seed " << cfg.seed << " differ:\n" << fa << "---\n" << fb);
        CHECK(a.getCurrentDay() == 11, "match stopped on day " << a.getCurrentDay());
        CHECK(!a.getBuildings().empty(), "the script built nothing (test would prove little)");
        std::cout << "  seed " << cfg.seed << ": day " << a.getCurrentDay() << ", " << a.getBuildings().size()
                  << " buildings, P1 share " << a.getCityState().p1CityShare << "\n";

        // UI draws from randInt() do not shift the weather
        GameEngine c, d;
        c.init(cfg);
        d.init(cfg);
        std::vector<int> wc, wd;
        for (int day = 0; day < 10; ++day) {
            for (int i = 0; i < 37; ++i) d.randInt(0, 99);
            wc.push_back(static_cast<int>(c.getPlayerWeather(1)) * 10 + static_cast<int>(c.getPlayerWeather(2)));
            wd.push_back(static_cast<int>(d.getPlayerWeather(1)) * 10 + static_cast<int>(d.getPlayerWeather(2)));
            c.update(Balance::SECONDS_PER_DAY);
            d.update(Balance::SECONDS_PER_DAY);
        }
        CHECK(wc == wd, "randInt() calls changed the weather sequence");

        // A different seed gives a different match
        MatchConfig other = cfg;
        other.seed = 99u;
        GameEngine g;
        g.init(other);
        const std::string fg = playScriptedMatch(g, 10, kFrame, 0.5f);
        CHECK(fg != fa, "seeds " << cfg.seed << " and " << other.seed << " gave the same match");
    }
    endGroup();
}

// ---------------------------------------------------------------------------
// [CD-04] Fixed 60 Hz simulation step
// ---------------------------------------------------------------------------
void testFixedTimestep() {
    beginGroup("Fixed 60 Hz simulation step (CD-04)");
    {
        // Same seed and inputs: the frame rate does not change the match (10 days, actions every 0.5 s)
        MatchConfig cfg;
        cfg.seed = 4242u;
        cfg.sandbox = true;
        GameEngine a, b, c;
        a.init(cfg);
        b.init(cfg);
        c.init(cfg);
        const std::string at30 = playScriptedMatch(a, 10, 1.0f / 30.0f, 0.5f);
        const std::string at144 = playScriptedMatch(b, 10, 1.0f / 144.0f, 0.5f);
        const std::string at20 = playScriptedMatch(c, 10, 0.05f, 0.5f);
        CHECK(at30 == at144, "dt 1/30 and dt 1/144 differ:\n" << at30 << "---\n" << at144);
        CHECK(at30 == at20, "dt 1/30 and dt 1/20 differ:\n" << at30 << "---\n" << at20);
        CHECK(a.getCurrentDay() == 11 && !a.getBuildings().empty(), "day " << a.getCurrentDay());
    }
    {
        // Time below one step is carried over, never lost
        GameEngine e;
        e.init(1600.0f, 900.0f);
        const float h0 = e.getHour24();
        e.update(0.01f);
        CHECK(e.getHour24() == h0, "0.01 s (less than a step) already moved the clock");
        e.update(0.01f);
        const float oneStepHours = Balance::gameSecondsToHours(static_cast<float>(GameEngine::FIXED_STEP_SECONDS));
        CHECK(std::abs(e.getHour24() - h0 - oneStepHours) < 1e-4f, "after 0.02 s the clock moved " << e.getHour24() - h0 << " h");

        // Time scale multiplies the game time of every step
        e.setTimeScale(Balance::MINE_SPEEDUP_MULT);
        const float h1 = e.getHour24();
        e.update(static_cast<float>(GameEngine::FIXED_STEP_SECONDS));
        CHECK(std::abs(e.getHour24() - h1 - oneStepHours * Balance::MINE_SPEEDUP_MULT) < 1e-4f,
              "one step at x" << Balance::MINE_SPEEDUP_MULT << " moved " << e.getHour24() - h1 << " h");
    }
    {
        // Optional cap for real-time hosts: at most n steps per call, the backlog is dropped
        GameEngine e;
        e.setMaxStepsPerUpdate(GameEngine::RECOMMENDED_MAX_STEPS_PER_UPDATE);
        e.init(1600.0f, 900.0f);
        CHECK(e.getMaxStepsPerUpdate() == GameEngine::RECOMMENDED_MAX_STEPS_PER_UPDATE, "init reset the step cap");
        const float h0 = e.getHour24();
        e.update(1.0f); // 60 steps due
        const float eightSteps = Balance::gameSecondsToHours(8.0f * static_cast<float>(GameEngine::FIXED_STEP_SECONDS));
        CHECK(std::abs(e.getHour24() - h0 - eightSteps) < 1e-4f, "capped frame moved " << e.getHour24() - h0 << " h");
        const float h1 = e.getHour24();
        e.update(0.001f);
        CHECK(e.getHour24() == h1, "the dropped backlog was simulated later");
        e.restartGame();
        CHECK(e.getMaxStepsPerUpdate() == GameEngine::RECOMMENDED_MAX_STEPS_PER_UPDATE, "restart reset the step cap");
        e.setMaxStepsPerUpdate(0);
        e.update(Balance::SECONDS_PER_DAY);
        CHECK(e.getCurrentDay() == 2, "uncapped: a whole day in one frame, day " << e.getCurrentDay());
    }
    {
        // Paused or broken frame times do nothing
        GameEngine e;
        e.init(1600.0f, 900.0f);
        const float h0 = e.getHour24();
        e.update(0.0f);
        e.update(-1.0f);
        e.update(std::nanf(""));
        CHECK(e.getHour24() == h0, "zero, negative or NaN dt moved the clock");
    }
    endGroup();
}

// ---------------------------------------------------------------------------
// [CD-11] One source for building costs
// ---------------------------------------------------------------------------
void testBuildingCosts() {
    beginGroup("Building costs from one accessor (CD-11)");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    struct Expect { BuildingType type; const Balance::BuildingDef* def; int legacyOre; };
    const Expect expected[] = {
        { BuildingType::SOLAR_PANEL, &Balance::SOLAR_PANEL, 18 },
        { BuildingType::WIND_TURBINE, &Balance::WIND_TURBINE, 28 },
        { BuildingType::HYDRO_PLANT, &Balance::HYDRO_PLANT, 38 },
        { BuildingType::BATTERY, &Balance::BATTERY, 26 },
        { BuildingType::LAMP, &Balance::STREET_LAMP, 8 },
    };
    for (const auto& x : expected) {
        const Balance::BuildingDef* def = GameEngine::getBuildingDef(x.type);
        REQUIRE(def != nullptr, "no recipe for type " << static_cast<int>(x.type));
        const BuildingCost c = e.getBuildingCost(x.type);
        CHECK(c.type == x.type && c.nameBg == x.def->nameBg && c.nameEn == x.def->nameEn, "names of type " << static_cast<int>(x.type));
        CHECK(c.woodCost == x.def->woodCost && c.ironCost == x.def->ironCost && c.copperCost == x.def->copperCost &&
                  c.coalCost == x.def->coalCost && c.siliconCost == x.def->siliconCost && c.silverCost == x.def->silverCost,
              "recipe of type " << static_cast<int>(x.type));
        CHECK(c.basePowerMW == x.def->basePowerMW && c.oreCost == x.legacyOre,
              "MW / ore of type " << static_cast<int>(x.type) << ": " << c.basePowerMW << " / " << c.oreCost);
        CHECK(def->woodCost == x.def->woodCost && def->basePowerMW == x.def->basePowerMW, "getBuildingDef mismatch");
    }
    CHECK(GameEngine::getBuildingDef(BuildingType::NONE) == nullptr && GameEngine::getBuildingDef(BuildingType::DEMOLISH) == nullptr,
          "NONE / DEMOLISH have no recipe");
    CHECK(e.getBuildingCost(BuildingType::DEMOLISH).type == BuildingType::DEMOLISH &&
              e.getBuildingCost(BuildingType::DEMOLISH).woodCost == 0,
          "demolish tool entry");
    CHECK(e.getBuildingCost(BuildingType::NONE).type == BuildingType::NONE, "NONE entry");
    endGroup();
}

// ---------------------------------------------------------------------------
// PlayerModifiers: income, mining yield, building cost, cooldown, share bonus
// ---------------------------------------------------------------------------
void testPlayerModifiers() {
    beginGroup("PlayerModifiers");
    MatchConfig cfg;
    cfg.seed = 555u;
    {
        // Neutral by default
        GameEngine e;
        e.init(cfg);
        const PlayerModifiers& m = e.getPlayerModifiers(1);
        CHECK(m.incomeMult == 1.0f && m.mineYieldMult == 1.0f && m.costMult == 1.0f && m.cooldownMult == 1.0f && m.shareBonus == 0.0f,
              "modifiers are not neutral after init");
        CHECK(e.getMineCooldown(2) == Balance::MINE_COOLDOWN_SEC, "cooldown " << e.getMineCooldown(2));
    }
    {
        // Income: double city money and gold dividend for P1 only (same seed, same buildings)
        GameEngine base, rich;
        base.init(cfg);
        rich.init(cfg);
        PlayerModifiers m;
        m.incomeMult = 2.0f;
        rich.setPlayerModifiers(1, m);
        for (GameEngine* e : { &base, &rich }) {
            giveResources(*e, 1, 1000);
            giveResources(*e, 2, 1000);
            place(*e, 1, BuildingType::WIND_TURBINE, slotOf(*e, 1, 1, 0));
            place(*e, 2, BuildingType::WIND_TURBINE, slotOf(*e, 2, 15, 0));
            for (int i = 0; i < 600; ++i) e->update(0.25f); // 150 s, into day 3 (city demand, gold dividends)
        }
        const auto& b1 = base.getPlayerEconomy(1);
        const auto& r1 = rich.getPlayerEconomy(1);
        CHECK(b1.money > 0 && r1.money == 2 * b1.money, "P1 money " << r1.money << " vs base " << b1.money);
        CHECK(b1.gold > 0 && r1.gold == 2 * b1.gold, "P1 gold " << r1.gold << " vs base " << b1.gold);
        CHECK(rich.getPlayerEconomy(2).money == base.getPlayerEconomy(2).money, "P2 income changed");
    }
    {
        // Costs: P1 pays half, P2 the base price; the refund follows the price paid
        GameEngine e;
        e.init(cfg);
        PlayerModifiers m;
        m.costMult = 0.5f;
        e.setPlayerModifiers(1, m);
        const BuildingCost cheap = e.getBuildingCost(1, BuildingType::WIND_TURBINE);
        const BuildingCost full = e.getBuildingCost(2, BuildingType::WIND_TURBINE);
        const BuildingCost base = e.getBuildingCost(BuildingType::WIND_TURBINE);
        CHECK(full.woodCost == base.woodCost && full.ironCost == base.ironCost && full.coalCost == base.coalCost, "P2 price changed");
        CHECK(cheap.woodCost == 4 && cheap.ironCost == 7 && cheap.copperCost == 4 && cheap.coalCost == 3,
              "half price " << cheap.woodCost << "/" << cheap.ironCost << "/" << cheap.copperCost << "/" << cheap.coalCost);
        CHECK(cheap.basePowerMW == base.basePowerMW && cheap.nameBg == base.nameBg, "costMult changed output or name");
        giveResources(e, 1, 100);
        const sf::Vector2f pos = slotOf(e, 1, 1, 0);
        place(e, 1, BuildingType::WIND_TURBINE, pos);
        const auto& p = e.getPlayerEconomy(1);
        CHECK(p.wood == 96 && p.iron == 93 && p.copper == 96 && p.coal == 97, "P1 paid " << 100 - p.wood << " wood, " << 100 - p.iron << " iron");
        std::string msg;
        REQUIRE(e.removeBuilding(1, pos, msg), msg);
        CHECK(p.wood == 98 && p.iron == 96, "refund of the half price: wood " << p.wood << ", iron " << p.iron);

        // Dearer buildings: the base recipe is no longer enough
        m.costMult = 2.0f;
        e.setPlayerModifiers(2, m);
        PlayerEconomy& q = e.getPlayerEconomyMut(2);
        q.wood = base.woodCost; q.iron = base.ironCost; q.copper = base.copperCost; q.coal = base.coalCost;
        CHECK(!e.placeBuilding(2, BuildingType::WIND_TURBINE, slotOf(e, 2, 15, 0), msg), "built at double price with the base recipe");
        CHECK(msg.find(std::to_string(2 * base.woodCost)) != std::string::npos, "message does not show the real price: " << msg);
    }
    {
        // Mining yield and cooldown
        GameEngine e;
        e.init(cfg);
        PlayerModifiers m;
        m.mineYieldMult = 2.0f;
        m.cooldownMult = 0.5f;
        e.setPlayerModifiers(2, m);
        std::string msg;
        MineResult r;
        REQUIRE(e.mineResource(2, ResourceType::WOOD, r, msg), msg);
        CHECK(r.amount == 2 * Balance::WOOD_BASE_YIELD && e.getPlayerEconomy(2).wood == 2 * Balance::WOOD_BASE_YIELD, "wood " << r.amount);
        REQUIRE(e.mineResource(1, ResourceType::WOOD, r, msg), msg);
        CHECK(r.amount == Balance::WOOD_BASE_YIELD, "P1 wood " << r.amount);
        CHECK(std::abs(e.getMineCooldown(2) - 0.5f * Balance::MINE_COOLDOWN_SEC) < 1e-6f && e.getMineCooldown(1) == Balance::MINE_COOLDOWN_SEC,
              "cooldowns " << e.getMineCooldown(1) << " / " << e.getMineCooldown(2));
    }
    {
        // Share bonus on won days: grace, grace, then won days of +15% +5%
        GameEngine e;
        e.init(cfg);
        PlayerModifiers m;
        m.shareBonus = 0.05f;
        e.setPlayerModifiers(1, m);
        buildWindFarm(e, 1);
        for (int day = 0; day < 3; ++day) runToNextDay(e, 0.25f);
        CHECK(std::abs(e.getCityState().p1CityShare - (0.5f + Balance::MAX_DAILY_CITY_SHIFT + 0.05f)) < 1e-4f,
              "share after one won day " << e.getCityState().p1CityShare);
        CHECK(e.getCityState().lastCutMessage.find("+20%") != std::string::npos, "message: " << e.getCityState().lastCutMessage);
        runToNextDay(e, 0.25f); // 90% >= 85%: the bonus brings the win one day earlier
        CHECK(e.getCityState().winner == 1 && e.getCurrentDay() == 5, "winner " << e.getCityState().winner << " day " << e.getCurrentDay());
    }
    {
        // Sanitizing and reset on restart
        GameEngine e;
        e.init(cfg);
        PlayerModifiers bad;
        bad.incomeMult = std::nanf("");
        bad.costMult = -3.0f;
        bad.mineYieldMult = 1000.0f;
        bad.shareBonus = 2.0f;
        e.setPlayerModifiers(1, bad);
        const PlayerModifiers& m = e.getPlayerModifiers(1);
        CHECK(m.incomeMult == 1.0f && m.costMult == 0.0f && m.mineYieldMult == PlayerModifiers::MAX_MULT &&
                  m.shareBonus == PlayerModifiers::MAX_SHARE_BONUS,
              "modifiers not clamped");
        e.restartGame();
        CHECK(e.getPlayerModifiers(1).costMult == 1.0f && e.getPlayerModifiers(1).shareBonus == 0.0f, "restart kept the modifiers");
    }
    endGroup();
}

// ---------------------------------------------------------------------------
// [CD-03] Snapshots: saveState / loadState reproduce the match
// ---------------------------------------------------------------------------
std::string snapshotOf(const GameEngine& e) {
    std::ostringstream os;
    bool ok = e.saveState(os);
    REQUIRE(ok, "saveState failed");
    return os.str();
}

bool loadFrom(GameEngine& e, const std::string& text) {
    std::istringstream is(text);
    return e.loadState(is);
}

void testSnapshots() {
    beginGroup("Snapshots: saveState / loadState (CD-03)");
    MatchConfig cfg;
    cfg.seed = 8080u;
    cfg.finalDay = 30;
    cfg.mutators = { "noch mit leerzeichen", "кирилица" };
    GameEngine a;
    a.init(cfg);
    PlayerModifiers m;
    m.costMult = 0.75f;
    m.shareBonus = 0.01f;
    a.setPlayerModifiers(2, m);
    playScriptedMatch(a, 4, kFrame, 0.25f);
    a.update(0.005f); // leave part of a fixed step in the accumulator
    REQUIRE(a.getCityState().winner == 0, "the match ended before the snapshot");

    const std::string s1 = snapshotOf(a);
    CHECK(s1.compare(0, 19, "ENERGY_CRISIS_STATE") == 0, "missing header: " << s1.substr(0, 40));

    // Load into an engine in a completely different state (other rules, other day, a step cap)
    GameEngine b;
    b.setMaxStepsPerUpdate(5);
    b.init(1600.0f, 900.0f);
    b.update(200.0f);
    b.pollEvents();
    std::string msg;
    b.mineResource(1, ResourceType::GOLD, msg); // leaves one pending event
    CHECK(loadFrom(b, s1), "loadState rejected a fresh snapshot");
    CHECK(snapshotOf(b) == s1, "save -> load -> save changed the snapshot");
    CHECK(b.pollEvents().empty(), "events of the old match survived the load");
    CHECK(b.getMaxStepsPerUpdate() == 5, "load changed the host step cap");
    b.setMaxStepsPerUpdate(0);
    CHECK(fingerprint(a) == fingerprint(b), "loaded state differs:\n" << fingerprint(a) << "---\n" << fingerprint(b));
    CHECK(b.getConfig().finalDay == 30 && b.getConfig().seed == 8080u && b.getConfig().hasMutator("кирилица") &&
              b.getConfig().hasMutator("noch mit leerzeichen"),
          "config not restored");
    CHECK(b.getSeed() == a.getSeed() && b.getPlayerModifiers(2).costMult == 0.75f, "seed or modifiers not restored");
    CHECK(b.getBuildings().size() == a.getBuildings().size() && b.getLandPlots().size() == a.getLandPlots().size(),
          "buildings or plots missing");

    // Both continue identically (weather rolls, randInt choices, batteries, payouts)
    const std::string fa = playScriptedMatch(a, 8, kFrame, 0.25f);
    const std::string fb = playScriptedMatch(b, 8, kFrame, 0.25f);
    CHECK(fa == fb, "the loaded match diverged:\n" << fa << "---\n" << fb);
    CHECK(snapshotOf(a) == snapshotOf(b), "snapshots differ after playing on");

    // Bad input is rejected and leaves the match untouched
    const std::string before = snapshotOf(b);
    CHECK(!loadFrom(b, ""), "empty input accepted");
    CHECK(!loadFrom(b, "hello world"), "garbage accepted");
    CHECK(!loadFrom(b, s1.substr(0, s1.size() / 2)), "truncated snapshot accepted");
    std::string wrongVersion = s1;
    wrongVersion.replace(0, std::string("ENERGY_CRISIS_STATE 2").size(), "ENERGY_CRISIS_STATE 99");
    CHECK(!loadFrom(b, wrongVersion), "unknown version accepted");
    std::string badBuilding = s1;
    size_t pos = badBuilding.find("\nbuilding ");
    REQUIRE(pos != std::string::npos, "snapshot without buildings");
    badBuilding.replace(pos, 11, "\nbuilding 99"); // 7..11 are the power buildings since snapshot v2
    CHECK(!loadFrom(b, badBuilding), "invalid building type accepted");
    CHECK(snapshotOf(b) == before, "a rejected load changed the match");
    endGroup();
}

} // namespace

int main() {
    std::cout << "========================================================\n";
    std::cout << " ENERGY CRISIS - ENGINE INFRASTRUCTURE TESTS\n";
    std::cout << "========================================================\n";

    testEvents();
    testMatchConfig();
    testDeterministicRng();
    testFixedTimestep();
    testBuildingCosts();
    testPlayerModifiers();
    testSnapshots();

    std::cout << "\n========================================================\n";
    if (g_failures == 0) {
        std::cout << " ALL " << g_checks << " INFRASTRUCTURE CHECKS PASSED\n";
    } else {
        std::cout << " " << g_failures << " OF " << g_checks << " INFRASTRUCTURE CHECKS FAILED\n";
    }
    std::cout << "========================================================\n";
    return (g_failures == 0) ? 0 : 1;
}
