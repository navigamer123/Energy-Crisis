#ifndef GAME_DEMO_H
#define GAME_DEMO_H

// =============================================================================
// ENERGY CRISIS - JUDGE DEMO MODE (HX-02), engine side             [Team Demo]
//
// A seeded, scripted ~3-minute showcase that plays the same way on every run:
//   * a data-driven BEAT SCRIPT: timed beats, each with a big Bulgarian caption,
//     an optional time-scale change and an optional ACTION ("build", "weather", ...)
//   * a DemoDirector that steps the engine with a FIXED tick (1/60 s), so the frame
//     rate of the presenting laptop never changes the outcome
//   * an extension API: registerDemoAction(name, fn). Features merged later
//     (event deck, nuclear plant, mega-projects, blackout, dashboard, ...) register
//     their action and the matching TODO beat of the default script comes alive.
//
// Engine only (no SFML graphics): the whole timeline runs headless in
// scratch/test_demo_mode.cpp. The UI side lives in UI/scr/UI_demo.cpp.
// See docs/demo_mode.md for the beat list and how to add beats.
// =============================================================================

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "game_main.h"

namespace Demo {

constexpr unsigned int DEMO_SEED = 2026u;       // RNG seed of every demo match (weather rolls, std::rand)
constexpr float DEMO_TICK_SEC = 1.0f / 60.0f;   // Fixed director step (real seconds)
constexpr int DEMO_MAX_TICKS_PER_FRAME = 8;     // A slow frame runs at most this many steps (then the show slows down)

// -----------------------------------------------------------------------------
// Action arguments: "key=value key=value", values may be "double quoted".
//   player=1 type=solar cells=0,0;1,0 text="Мълния удари!"
// -----------------------------------------------------------------------------
class DemoArgs {
public:
    DemoArgs() = default;
    // Parses a spec; on a syntax problem *error gets a message and the valid pairs are kept
    static DemoArgs parse(const std::string& spec, std::string* error = nullptr);

    bool has(const std::string& key) const;
    std::string getString(const std::string& key, const std::string& def = "") const;
    int getInt(const std::string& key, int def = 0) const;
    float getFloat(const std::string& key, float def = 0.0f) const;
    bool getBool(const std::string& key, bool def = false) const;
    std::vector<std::string> getList(const std::string& key, char sep = ',') const;
    // "c,r;c,r;..." -> {(c, r), ...} (grid cells, plot coordinates)
    std::vector<std::pair<int, int>> getPairs(const std::string& key) const;

    void set(const std::string& key, const std::string& value) { values[key] = value; }
    const std::map<std::string, std::string>& all() const { return values; }

private:
    std::map<std::string, std::string> values;
};

// -----------------------------------------------------------------------------
// One beat of the script. Build beats fluently:
//   DemoBeat(20.0f).say("ПЪРВИТЕ ЦЕНТРАЛИ", "...").act("build", "player=1 type=solar cells=0,0")
// Caption / subtitle placeholders, expanded right after the action ran:
//   {result} last day-end message   {day} current day   {p1} {p2} city share in %
//   {demand} city demand MW         {mw1} {mw2} live MW  {season} season name
// -----------------------------------------------------------------------------
struct DemoBeat {
    float at = 0.0f;          // Demo seconds from the start (fixed-step demo clock)
    std::string caption;      // Big line (Bulgarian). Empty: the current caption stays
    std::string subtitle;     // Second, smaller line
    float timeScale = 0.0f;   // > 0: engine time scale from this beat on
    std::string action;       // Registered action name, "" = caption only
    std::string args;         // Action arguments (see DemoArgs)
    float hold = 0.0f;        // Seconds the caption stays; 0 = until the next caption
    bool titleCard = false;   // Full-screen title style (dimmed map, very large text)
    bool optional = false;    // TODO beat: silently skipped while its action is not registered
    std::string feature;      // Optional beats: the later feature that fills this beat

    DemoBeat() = default;
    explicit DemoBeat(float atSec) : at(atSec) {}

    DemoBeat& say(const std::string& cap, const std::string& sub = "") { caption = cap; subtitle = sub; return *this; }
    DemoBeat& title(const std::string& cap, const std::string& sub = "") { say(cap, sub); titleCard = true; return *this; }
    DemoBeat& act(const std::string& name, const std::string& argSpec = "") { action = name; args = argSpec; return *this; }
    DemoBeat& speed(float scale) { timeScale = scale; return *this; }
    DemoBeat& holdFor(float seconds) { hold = seconds; return *this; }
    DemoBeat& todo(const std::string& featureName) { optional = true; feature = featureName; return *this; }
};

// -----------------------------------------------------------------------------
// Presentation cue: what an action wants the UI to show (cursor flight, sparks,
// lightning bolt, floating notice). The headless engine just collects them.
// Built-in kinds: build, plot, mine, lightning, notice, weather, jump, settle, winner.
// Unknown kinds with a text are shown by the UI as a floating notice.
// -----------------------------------------------------------------------------
struct DemoCue {
    std::string kind;
    int player = 0;                       // 0 = both / neutral
    sf::Vector2f pos = sf::Vector2f(0.0f, 0.0f);
    bool hasPos = false;
    int value = 0;                        // BuildingType / ResourceType / WeatherType / hit flag / winner
    std::string text;
};

class DemoDirector;

// Passed to every action
struct DemoContext {
    GameEngine& engine;
    DemoDirector& director;
    const DemoBeat& beat;
    float time; // demo seconds when the beat fired

    void cue(const DemoCue& c);
    void log(const std::string& line);
};

// An action returns false when it could not do its job (logged, the show goes on)
using DemoActionFn = std::function<bool(DemoContext&, const DemoArgs&)>;

// ----- Extension API ---------------------------------------------------------
// Register (or replace) an action. Call it from your feature's own .cpp, either at
// start-up through a static DemoActionRegistrar or at runtime (UI code may capture
// UI objects in the lambda). Returns true when the name was new.
bool registerDemoAction(const std::string& name, DemoActionFn fn);
bool unregisterDemoAction(const std::string& name);
bool hasDemoAction(const std::string& name);
std::vector<std::string> listDemoActions();

// Static registration helper:  static Demo::DemoActionRegistrar reg("nuclear.build", [](...){...});
struct DemoActionRegistrar {
    DemoActionRegistrar(const std::string& name, DemoActionFn fn) { registerDemoAction(name, std::move(fn)); }
};

// ----- Scripts ---------------------------------------------------------------
std::vector<DemoBeat> defaultDemoScript();               // Game/scr/game_demo_script.cpp
// Problems of a script (unsorted beats, unknown required actions, bad args, ...). Empty = OK.
std::vector<std::string> validateScript(const std::vector<DemoBeat>& script);
// Optional beats whose action is not registered yet ("name -> feature"), for the TODO list
std::vector<std::string> pendingFeatureBeats(const std::vector<DemoBeat>& script);

// Name tables shared by the actions (lower-case keys)
bool parseBuildingType(const std::string& name, BuildingType& out);
bool parseResourceType(const std::string& name, ResourceType& out);
bool parseWeatherType(const std::string& name, WeatherType& out);
bool parseSeasonType(const std::string& name, SeasonType& out);

// -----------------------------------------------------------------------------
// The director: owns the script and the fixed-step demo clock
// -----------------------------------------------------------------------------
class DemoDirector {
public:
    DemoDirector();

    void setScript(const std::vector<DemoBeat>& beats); // stable-sorted by time
    const std::vector<DemoBeat>& getScript() const { return script; }

    // Fresh seeded match, demo clock at 0. The engine is (re)initialised here.
    void start(GameEngine& engine, unsigned int seed = DEMO_SEED);
    void stop();

    // Real frame time in, whole fixed ticks out (remainder carried to the next frame)
    void advance(GameEngine& engine, float realDt);
    // Exactly one fixed tick: fire due beats, then step the engine by DEMO_TICK_SEC
    void tick(GameEngine& engine);

    bool isRunning() const { return running; }
    bool isFinished() const;          // every beat fired and the last caption expired
    float getTime() const { return static_cast<float>(ticks) * DEMO_TICK_SEC; }
    float getDuration() const;        // time of the last beat + its hold
    long long getTickCount() const { return ticks; }

    // Caption on screen right now
    bool hasCaption() const;
    const std::string& getCaption() const { return caption; }
    const std::string& getSubtitle() const { return subtitle; }
    bool isTitleCard() const { return captionTitleCard; }
    float getCaptionAge() const { return getTime() - captionStart; }
    int getCaptionSerial() const { return captionSerial; } // changes whenever a new caption appears

    int getLastBeatIndex() const { return lastBeat; }
    int getFiredCount() const { return firedCount; }
    int getSkippedCount() const { return skippedCount; }
    int getFailedCount() const { return failedCount; }
    const std::vector<std::string>& getLog() const { return logLines; }

    std::vector<DemoCue> takeCues();
    void pushCue(const DemoCue& c) { cues.push_back(c); }
    void addLog(const std::string& line);
    void setEcho(bool on) { echo = on; } // mirror the log to std::cout (default on)

    // Forced weather survives the daily weather roll until unlocked (player 1 or 2)
    void lockWeather(int player, WeatherType w);
    void unlockWeather(int player);
    bool isWeatherLocked(int player) const;
    void applyWeatherLocks(GameEngine& engine) const;

    std::string expandText(const std::string& text, const GameEngine& engine) const;

private:
    void fireDueBeats(GameEngine& engine);
    void fireBeat(GameEngine& engine, size_t index);

    std::vector<DemoBeat> script;
    size_t nextBeat = 0;
    long long ticks = 0;
    float accumulator = 0.0f;
    bool running = false;
    bool echo = true;

    std::string caption;
    std::string subtitle;
    bool captionTitleCard = false;
    float captionStart = 0.0f;
    float captionHold = 0.0f;
    int captionSerial = 0;

    int lastBeat = -1;
    int firedCount = 0;
    int skippedCount = 0;
    int failedCount = 0;
    std::vector<std::string> logLines;
    std::vector<DemoCue> cues;

    bool weatherLocked[3] = { false, false, false };
    WeatherType lockedWeather[3] = { WeatherType::SUNNY, WeatherType::SUNNY, WeatherType::SUNNY };
};

} // namespace Demo

// -----------------------------------------------------------------------------
// Privileged engine hooks for the demo (GameEngine declares this class a friend).
// Other features may use them in their own demo actions.
// -----------------------------------------------------------------------------
class DemoEngineAccess {
public:
    static void setWeather(GameEngine& e, int player, WeatherType w);
    static void rerollWeather(GameEngine& e);
    // Teleport to day `day` at clock hour `hour` (6..30; hours < 6 mean the night after that day).
    // Demand follows the normal schedule, today's energy average restarts, shares are kept.
    static void jumpToDay(GameEngine& e, int day, float hour);
    // Simulate forward (full fidelity, time scale ignored); a crossed 06:00 settles that day
    static void advanceGameSeconds(GameEngine& e, float gameSeconds);
    static void advanceToHour(GameEngine& e, float hour);   // later today (wraps past midnight)
    static void settleDay(GameEngine& e);                   // simulate to the next 06:00 settlement
    static void setCityShare(GameEngine& e, float p1Share);
    static void setCityDemand(GameEngine& e, int mw);
    static void setCityMessage(GameEngine& e, const std::string& msg);
    static void forceWinner(GameEngine& e, int winner, const std::string& msg);
    // Buy a plot without paying (plotRow 0..3, plotCol 0..2 in the engine's plot grid)
    static bool purchasePlotFree(GameEngine& e, int player, int plotRow, int plotCol);
    static int plotIndexAt(const GameEngine& e, int player, sf::Vector2f pos); // -1 when none
    // Place ignoring cost and the night rule (slot must be free and on the player's land)
    static bool placeBuildingForced(GameEngine& e, int player, BuildingType type, sf::Vector2f pos);
    static float getGameSeconds(const GameEngine& e);
    static int demandForDay(int day);
};

#endif // GAME_DEMO_H
