// =============================================================================
// ENERGY CRISIS - JUDGE DEMO MODE (HX-02) TESTS                      [Team Demo]
// Runs the default beat timeline headlessly (engine side only) to the end and
// checks that it is valid, complete, deterministic and extensible.
// Headless: this file + Game/scr/*.cpp, no SFML ("make test").
// =============================================================================
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "../Game/includes/game_demo.h"

#ifdef _WIN32
// Declared by hand: strict -std=c++17 hides _putenv in some MinGW headers (msvcrt exports it)
extern "C" __declspec(dllimport) int __cdecl _putenv(const char*);
static void setSeedEnv(const char* value) {
    std::string s = std::string("EC_SEED=") + value;
    _putenv(s.c_str());
}
#else
static void setSeedEnv(const char* value) { setenv("EC_SEED", value, 1); }
#endif

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond, details)                                                                   \
    do {                                                                                       \
        ++g_checks;                                                                            \
        if (!(cond)) {                                                                         \
            ++g_failures;                                                                      \
            std::cerr << "    FAIL (line " << __LINE__ << "): " << #cond << " | " << details << "\n"; \
        }                                                                                      \
    } while (0)

using namespace Demo;

// Everything that matters for "the same show every run"
struct Fingerprint {
    float share = 0.0f;
    int winner = 0;
    int day = 0;
    float hour = 0.0f;
    size_t buildings = 0;
    int p1Money = 0, p2Money = 0, p1Gold = 0, p2Gold = 0, p1Wood = 0, p2Wood = 0;
    int demand = 0;
    long long ticks = 0;

    bool operator==(const Fingerprint& o) const {
        return std::memcmp(&share, &o.share, sizeof(float)) == 0 && winner == o.winner && day == o.day &&
               std::memcmp(&hour, &o.hour, sizeof(float)) == 0 && buildings == o.buildings &&
               p1Money == o.p1Money && p2Money == o.p2Money && p1Gold == o.p1Gold && p2Gold == o.p2Gold &&
               p1Wood == o.p1Wood && p2Wood == o.p2Wood && demand == o.demand && ticks == o.ticks;
    }
};

Fingerprint fingerprint(const GameEngine& e, const DemoDirector& d) {
    Fingerprint f;
    f.share = e.getCityState().p1CityShare;
    f.winner = e.getCityState().winner;
    f.day = e.getCurrentDay();
    f.hour = e.getHour24();
    f.buildings = e.getBuildings().size();
    f.p1Money = e.getPlayerEconomy(1).money;
    f.p2Money = e.getPlayerEconomy(2).money;
    f.p1Gold = e.getPlayerEconomy(1).gold;
    f.p2Gold = e.getPlayerEconomy(2).gold;
    f.p1Wood = e.getPlayerEconomy(1).wood;
    f.p2Wood = e.getPlayerEconomy(2).wood;
    f.demand = e.getCityState().cityEnergyDemand;
    f.ticks = d.getTickCount();
    return f;
}

std::string describe(const Fingerprint& f) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "share=%.6f winner=%d day=%d hour=%.4f buildings=%u money=%d/%d gold=%d/%d ticks=%lld",
                  f.share, f.winner, f.day, f.hour, static_cast<unsigned>(f.buildings), f.p1Money, f.p2Money,
                  f.p1Gold, f.p2Gold, f.ticks);
    return buf;
}

struct BeatSnapshot {
    int beat = -1;
    std::string action;
    float time = 0.0f;
    float shareAfter = 0.0f;
    int winnerAfter = 0;
    int dayAfter = 0;
};

// Run a script to the end with a repeating frame-time pattern; returns the end state
Fingerprint runShow(const std::vector<DemoBeat>& script, const std::vector<float>& framePattern,
                    std::vector<BeatSnapshot>* snaps, bool printTimeline, DemoDirector* outDirector = nullptr) {
    GameEngine engine;
    DemoDirector director;
    director.setEcho(false);
    director.setScript(script);
    director.start(engine);
    int lastSeen = director.getLastBeatIndex();
    size_t frame = 0;
    long long guard = 0;
    auto record = [&]() {
        int idx = director.getLastBeatIndex();
        if (idx == lastSeen || idx < 0) return;
        for (int i = lastSeen + 1; i <= idx; ++i) {
            const DemoBeat& b = director.getScript()[static_cast<size_t>(i)];
            if (snaps) {
                BeatSnapshot s;
                s.beat = i;
                s.action = b.action;
                s.time = b.at;
                s.shareAfter = engine.getCityState().p1CityShare;
                s.winnerAfter = engine.getCityState().winner;
                s.dayAfter = engine.getCurrentDay();
                snaps->push_back(s);
            }
        }
        if (printTimeline) {
            const DemoBeat& b = director.getScript()[static_cast<size_t>(idx)];
            char line[512];
            std::snprintf(line, sizeof(line), "  %6.1fs day %2d %5.2fh x%-4.1f P1 %3d MW P2 %3d MW demand %3d share %5.1f%%  %-14s %s",
                          b.at, engine.getCurrentDay(), engine.getHour24(), engine.getTimeScale(),
                          engine.getPlayerEconomy(1).energyMW, engine.getPlayerEconomy(2).energyMW,
                          engine.getCityState().cityEnergyDemand, engine.getCityState().p1CityShare * 100.0f,
                          b.action.c_str(), director.getCaption().c_str());
            std::cout << line << "\n";
            if (b.action == "settle") std::cout << "          -> " << director.getSubtitle() << "\n";
        }
        lastSeen = idx;
    };
    record();
    while (!director.isFinished() && guard++ < 2000000) {
        director.advance(engine, framePattern[frame++ % framePattern.size()]);
        record();
    }
    Fingerprint f = fingerprint(engine, director);
    if (outDirector) *outDirector = director;
    return f;
}

void testArgs() {
    std::cout << "\n[DemoArgs parsing]\n";
    std::string err;
    DemoArgs a = DemoArgs::parse("player=2 type=solar cells=0,0;1,2 text=\"Мълния удари!\" lock=0 f=2.5", &err);
    CHECK(err.empty(), err);
    CHECK(a.getInt("player") == 2, a.getString("player"));
    CHECK(a.getString("type") == "solar", a.getString("type"));
    CHECK(a.getString("text") == "Мълния удари!", a.getString("text"));
    CHECK(!a.getBool("lock", true), "lock=0 must be false");
    CHECK(std::fabs(a.getFloat("f") - 2.5f) < 1e-6f, a.getFloat("f"));
    auto cells = a.getPairs("cells");
    CHECK(cells.size() == 2 && cells[1].first == 1 && cells[1].second == 2, cells.size());
    CHECK(a.getInt("missing", 7) == 7, "default int");
    std::string err2;
    DemoArgs::parse("text=\"open", &err2);
    CHECK(!err2.empty(), "unterminated quote must be reported");
    std::string err3;
    DemoArgs b = DemoArgs::parse("res=wood,iron,,gold", &err3);
    CHECK(b.getList("res").size() == 3, b.getList("res").size());
}

void testDefaultScriptRuns() {
    std::cout << "\n[Default script: validity, completeness, story]\n";
    std::vector<DemoBeat> script = defaultDemoScript();
    std::vector<std::string> problems = validateScript(script);
    for (const auto& p : problems) std::cerr << "    script problem: " << p << "\n";
    CHECK(problems.empty(), problems.size() << " problems");

    std::vector<std::string> pending = pendingFeatureBeats(script);
    std::cout << "  TODO beats waiting for later features:\n";
    for (const auto& p : pending) std::cout << "    - " << p << "\n";
    CHECK(pending.size() == 5, pending.size());

    DemoDirector d0;
    d0.setScript(script);
    float duration = d0.getDuration();
    CHECK(duration >= 170.0f && duration <= 200.0f, "duration " << duration << " s (target ~3 min)");

    std::cout << "  Timeline (60 FPS):\n";
    std::vector<BeatSnapshot> snaps;
    DemoDirector finished;
    Fingerprint end = runShow(script, { 1.0f / 60.0f }, &snaps, true, &finished);
    std::cout << "  End: " << describe(end) << "\n";

    CHECK(finished.isFinished(), "timeline must run to completion");
    CHECK(finished.getFailedCount() == 0, finished.getFailedCount() << " beats failed");
    CHECK(finished.getSkippedCount() == 5, finished.getSkippedCount() << " skipped");
    CHECK(finished.getFiredCount() == static_cast<int>(script.size()) - 5, finished.getFiredCount());
    CHECK(end.winner == 1, "the West must win the demo, winner=" << end.winner);
    CHECK(end.day >= 20, "the demo must reach the final day, day=" << end.day);
    CHECK(end.buildings >= 12, end.buildings << " buildings on the map");
    CHECK(std::fabs(finished.getTime() - duration) < 0.05f, finished.getTime());

    // Story checkpoints: each settlement does what its caption claims
    std::vector<BeatSnapshot> settles;
    for (const auto& s : snaps) {
        if (s.action == "settle") settles.push_back(s);
    }
    CHECK(settles.size() == 4, settles.size() << " settlements");
    if (settles.size() == 4) {
        CHECK(settles[0].dayAfter == 4 && settles[0].shareAfter > 0.60f,
              "day 3 (storm): the West must win the day, share " << settles[0].shareAfter);
        CHECK(std::fabs(settles[1].shareAfter - settles[0].shareAfter) < 1e-4f,
              "summer day: both power the city, no change (" << settles[1].shareAfter << ")");
        CHECK(settles[2].shareAfter > settles[1].shareAfter + 0.09f && settles[2].winnerAfter == 0,
              "winter day: the West wins the day, share " << settles[2].shareAfter);
        CHECK(settles[3].winnerAfter == 1, "final day: the engine itself declares the West the winner");
    }
}

void testDeterminism() {
    std::cout << "\n[Determinism: frame rate and EC_SEED do not change the show]\n";
    std::vector<DemoBeat> script = defaultDemoScript();
    Fingerprint a = runShow(script, { 1.0f / 60.0f }, nullptr, false);
    Fingerprint b = runShow(script, { 1.0f / 60.0f }, nullptr, false);
    CHECK(a == b, "\n      run 1: " << describe(a) << "\n      run 2: " << describe(b));

    // Jittery frames (144 Hz, 30 Hz, a hitch) still give the same fixed ticks
    Fingerprint c = runShow(script, { 1.0f / 144.0f, 1.0f / 30.0f, 0.05f, 1.0f / 60.0f, 0.011f, 0.02f }, nullptr, false);
    CHECK(a == c, "\n      60 FPS : " << describe(a) << "\n      jitter : " << describe(c));

    setSeedEnv("12345");
    Fingerprint d = runShow(script, { 1.0f / 60.0f }, nullptr, false);
    setSeedEnv("");
    CHECK(a == d, "\n      no seed : " << describe(a) << "\n      EC_SEED : " << describe(d));
}

void testExtensionApi() {
    std::cout << "\n[Extension API: registerDemoAction switches TODO beats on]\n";
    std::vector<DemoBeat> script = defaultDemoScript();
    int eventCalls = 0;
    float eventTime = -1.0f;
    std::string eventArg;
    bool isNew = registerDemoAction("event.card", [&](DemoContext& ctx, const DemoArgs& a) {
        ++eventCalls;
        eventTime = ctx.time;
        eventArg = a.getString("id");
        DemoCue c;
        c.kind = "event.card";
        c.text = "ТЕСТОВО СЪБИТИЕ";
        ctx.cue(c);
        return true;
    });
    CHECK(isNew, "event.card must be a new action");
    CHECK(hasDemoAction("event.card"), "registered");
    CHECK(pendingFeatureBeats(script).size() == 4, pendingFeatureBeats(script).size());

    // Nuclear-style feature that uses the privileged hooks
    registerDemoAction("nuclear.build", [](DemoContext& ctx, const DemoArgs& a) {
        DemoEngineAccess::setCityMessage(ctx.engine, "ЯДРЕНА ЦЕНТРАЛА (ТЕСТ) ЗА ИГРАЧ " + a.getString("player"));
        return true;
    });

    DemoDirector d;
    runShow(script, { 1.0f / 60.0f }, nullptr, false, &d);
    CHECK(eventCalls == 1, eventCalls << " calls");
    CHECK(std::fabs(eventTime - 76.0f) < 0.05f, "fired at " << eventTime);
    CHECK(eventArg == "random", eventArg);
    CHECK(d.getSkippedCount() == 3, d.getSkippedCount());
    CHECK(d.getFailedCount() == 0, d.getFailedCount());

    unregisterDemoAction("event.card");
    unregisterDemoAction("nuclear.build");
    CHECK(pendingFeatureBeats(script).size() == 5, "unregistered again");

    // Cues reach the UI in order
    GameEngine engine;
    DemoDirector cueDirector;
    std::vector<DemoBeat> mini;
    mini.push_back(DemoBeat(0.0f).act("build", "player=1 type=solar cells=0,0"));
    mini.push_back(DemoBeat(0.5f).act("lightning", "player=1 cell=0,0 hit=1"));
    mini.push_back(DemoBeat(1.0f).act("notice", "text=\"ЗДРАВЕЙ\""));
    cueDirector.setScript(mini);
    cueDirector.start(engine);
    std::vector<DemoCue> cues = cueDirector.takeCues();
    CHECK(cues.size() == 1 && cues[0].kind == "build" && cues[0].hasPos, cues.size());
    while (!cueDirector.isFinished() && cueDirector.getTickCount() < 1000) cueDirector.tick(engine);
    cues = cueDirector.takeCues();
    CHECK(cues.size() == 2 && cues[0].kind == "lightning" && cues[0].value == 1 && cues[1].text == "ЗДРАВЕЙ", cues.size());
    CHECK(engine.getBuildings().empty(), "lightning hit=1 removes the building");
}

void testRobustness() {
    std::cout << "\n[Robustness: the show never stops]\n";
    GameEngine engine;
    DemoDirector d;
    std::vector<DemoBeat> s;
    s.push_back(DemoBeat(0.0f).say("A").act("no_such_action"));
    s.push_back(DemoBeat(0.2f).say("B").act("throws"));
    s.push_back(DemoBeat(0.4f).say("C").act("build", "player=1 type=castle cells=0,0"));
    s.push_back(DemoBeat(0.6f).say("D {day}").act("noop").holdFor(1.0f));
    registerDemoAction("throws", [](DemoContext&, const DemoArgs&) -> bool { throw std::runtime_error("boom"); });
    CHECK(validateScript(s).size() == 1, "only the unknown action is a script problem (got " << validateScript(s).size() << ")");
    d.setScript(s);
    d.start(engine);
    CHECK(d.getCaption() == "A", "an unknown action still shows its caption");
    while (!d.isFinished() && d.getTickCount() < 1000) d.tick(engine);
    CHECK(d.isFinished(), "finished");
    CHECK(d.getFailedCount() == 3, d.getFailedCount());
    CHECK(d.getCaption() == "D 1", d.getCaption());
    CHECK(!d.hasCaption(), "the last caption expired after its hold");
    unregisterDemoAction("throws");

    // One huge frame runs at most DEMO_MAX_TICKS_PER_FRAME ticks
    DemoDirector slow;
    slow.setScript(defaultDemoScript());
    slow.start(engine);
    slow.advance(engine, 5.0f);
    CHECK(slow.getTickCount() == DEMO_MAX_TICKS_PER_FRAME, slow.getTickCount());
    slow.advance(engine, 1.0f / 60.0f);
    CHECK(slow.getTickCount() == DEMO_MAX_TICKS_PER_FRAME + 1, "backlog dropped, got " << slow.getTickCount());

    // jump_day keeps the demand schedule; settle runs the real day end
    DemoEngineAccess::jumpToDay(engine, 6, 12.0f);
    CHECK(engine.getCurrentDay() == 6 && std::fabs(engine.getHour24() - 12.0f) < 1e-3f, engine.getHour24());
    CHECK(engine.getCityState().cityEnergyDemand == 75, engine.getCityState().cityEnergyDemand);
    CHECK(engine.getSeason() == SeasonType::SUMMER, "day 6 is summer");
    DemoEngineAccess::settleDay(engine);
    CHECK(engine.getCurrentDay() == 7 && engine.getCityState().cityEnergyDemand == 90, engine.getCityState().cityEnergyDemand);
}

} // namespace

int main() {
    // A feature registered before the built-ins keeps its name (built-ins never overwrite)
    int customNoop = 0;
    registerDemoAction("noop", [&customNoop](DemoContext&, const DemoArgs&) { ++customNoop; return true; });

    std::cout << "=== Judge demo mode (HX-02) ===\n";
    testArgs();
    testDefaultScriptRuns();
    testDeterminism();
    testExtensionApi();
    testRobustness();
    CHECK(customNoop == 1, "the feature's own 'noop' ran instead of the built-in (" << customNoop << ")");

    std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    if (g_failures == 0) {
        std::cout << "PASS\n";
        return 0;
    }
    std::cout << "FAIL\n";
    return 1;
}
