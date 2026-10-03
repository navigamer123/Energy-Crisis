// =============================================================================
// ENERGY CRISIS - JUDGE DEMO MODE (HX-02): director, action registry,
// privileged engine hooks and the built-in actions.                  [Team Demo]
// =============================================================================
#include "../includes/game_demo.h"
#include "../includes/game_random.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <sstream>

// =============================================================================
// Privileged engine hooks
// =============================================================================
namespace {

const char* weatherKeyFor(WeatherType w) {
    // Same keywords weather_report() stores in PlayerData::weather
    switch (w) {
        case WeatherType::SUNNY:  return "clear";
        case WeatherType::WINDY:  return "clear";
        case WeatherType::RAINY:  return "rain";
        case WeatherType::STORMY: return "thunder_storm";
        case WeatherType::SNOWY:  return "snow";
        case WeatherType::CLOUDY: return "cloudy";
    }
    return "clear";
}

} // namespace

void DemoEngineAccess::setWeather(GameEngine& e, int player, WeatherType w) {
    if (player == 1 || player == 0) {
        e.p1Weather = w;
        e.p1.data.weather = weatherKeyFor(w);
        e.p1.data.wind_speed = (w == WeatherType::WINDY || w == WeatherType::STORMY) ? "45" : "0";
    }
    if (player == 2 || player == 0) {
        e.p2Weather = w;
        e.p2.data.weather = weatherKeyFor(w);
        e.p2.data.wind_speed = (w == WeatherType::WINDY || w == WeatherType::STORMY) ? "45" : "0";
    }
    e.updateBuildingsEnergy(0.0f); // refresh live MW at once (dt = 0: no energy is counted)
}

void DemoEngineAccess::rerollWeather(GameEngine& e) {
    e.rollDailyWeather();
    e.updateBuildingsEnergy(0.0f);
}

int DemoEngineAccess::demandForDay(int day) {
    // Same schedule as GameEngine::processDayEnd: 0 MW in the grace period, then 30 MW, +15 MW per day
    if (day <= Balance::GRACE_PERIOD_DAYS) return 0;
    return Balance::STARTING_CITY_DEMAND_MW + (day - Balance::GRACE_PERIOD_DAYS - 1) * Balance::DAILY_DEMAND_INCREASE_MW;
}

void DemoEngineAccess::jumpToDay(GameEngine& e, int day, float hour) {
    day = std::max(1, std::min(day, 999));
    if (hour < Balance::CLOCK_HOUR_AT_ZERO) hour += 24.0f; // 02:00 = the night after that day
    hour = std::max(Balance::CLOCK_HOUR_AT_ZERO, std::min(hour, Balance::CLOCK_HOUR_AT_ZERO + 23.99f));

    e.gameSeconds = static_cast<float>(day - 1) * Balance::SECONDS_PER_DAY +
                    (hour - Balance::CLOCK_HOUR_AT_ZERO) / 24.0f * Balance::SECONDS_PER_DAY;
    e.currentDay = day;
    e.hour24 = std::fmod(hour, 24.0f);
    e.currentSeason = Balance::getSeasonAtGameSeconds(e.gameSeconds);
    e.revenueTimer = 0.0f;

    e.city.cityEnergyDemand = demandForDay(day);
    e.city.p1DailyDelivered = 0.0f;
    e.city.p2DailyDelivered = 0.0f;
    e.city.dailySeconds = 0.0f;   // today's average restarts at the jump
    e.city.dayCutOccurred = false;
    // The city banner would still show the result of the last settled day
    e.city.lastCutMessage = (day <= Balance::GRACE_PERIOD_DAYS)
        ? "ДЕН " + std::to_string(day) + ": ГРАТИСЕН ПЕРИОД, ГРАДЪТ ИСКА 0 MW."
        : "ДЕН " + std::to_string(day) + " ЗАПОЧНА: ГРАДЪТ ИЗИСКВА " + std::to_string(e.city.cityEnergyDemand) + " MW.";

    // A new day gets new (seeded) weather; the director re-applies weather locks afterwards
    e.currentSeason = Balance::getSeasonForDay(day);
    e.rollDailyWeather();
    e.currentSeason = Balance::getSeasonAtGameSeconds(e.gameSeconds);
    e.updateBuildingsEnergy(0.0f);
}

void DemoEngineAccess::advanceGameSeconds(GameEngine& e, float gameSeconds) {
    if (gameSeconds <= 0.0f) return;
    const float savedScale = e.timeScale;
    e.timeScale = 1.0f; // advance exactly gameSeconds of game time
    float remaining = gameSeconds;
    int guard = 0;
    while (remaining > 1e-6f && e.city.winner == 0 && guard++ < 1000000) {
        float chunk = std::min(remaining, 5.0f);
        e.update(chunk);
        remaining -= chunk;
    }
    e.timeScale = savedScale;
}

void DemoEngineAccess::advanceToHour(GameEngine& e, float hour) {
    float now = e.hour24;
    float delta = hour - now;
    if (delta <= 0.0f) delta += 24.0f;
    advanceGameSeconds(e, delta / 24.0f * Balance::SECONDS_PER_DAY);
}

void DemoEngineAccess::settleDay(GameEngine& e) {
    float dayEnd = static_cast<float>(e.currentDay) * Balance::SECONDS_PER_DAY;
    // A hair past 06:00 so the boundary (and its single settlement) is surely crossed
    advanceGameSeconds(e, (dayEnd - e.gameSeconds) + 0.001f);
}

void DemoEngineAccess::setCityShare(GameEngine& e, float p1Share) {
    e.city.p1CityShare = std::max(0.0f, std::min(1.0f, p1Share));
    e.p1.cityInfluence = e.city.p1CityShare;
    e.p2.cityInfluence = 1.0f - e.city.p1CityShare;
}

void DemoEngineAccess::setCityDemand(GameEngine& e, int mw) {
    e.city.cityEnergyDemand = std::max(0, mw);
}

void DemoEngineAccess::setCityMessage(GameEngine& e, const std::string& msg) {
    e.city.lastCutMessage = msg;
}

void DemoEngineAccess::forceWinner(GameEngine& e, int winner, const std::string& msg) {
    e.city.winner = std::max(0, std::min(winner, 3));
    if (!msg.empty()) e.city.lastCutMessage = msg;
}

bool DemoEngineAccess::purchasePlotFree(GameEngine& e, int player, int plotRow, int plotCol) {
    // Plots are generated row by row, 3 per row: West ids 1..12, East ids 13..24
    int index = (player == 2 ? Balance::PLOTS_PER_PLAYER : 0) + plotRow * 3 + plotCol;
    if (plotRow < 0 || plotRow > 3 || plotCol < 0 || plotCol > 2) return false;
    if (index < 0 || index >= static_cast<int>(e.landPlots.size())) return false;
    LandPlot& plot = e.landPlots[static_cast<size_t>(index)];
    if (plot.playerOwner != player) return false;
    if (plot.isPurchased) return true;
    plot.isPurchased = true;
    PlayerEconomy& econ = (player == 1) ? e.p1 : e.p2;
    econ.landTier++;
    return true;
}

int DemoEngineAccess::plotIndexAt(const GameEngine& e, int player, sf::Vector2f pos) {
    for (size_t i = 0; i < e.landPlots.size(); ++i) {
        if (e.landPlots[i].playerOwner == player && e.landPlots[i].bounds.contains(pos)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool DemoEngineAccess::placeBuildingForced(GameEngine& e, int player, BuildingType type, sf::Vector2f pos) {
    if (type == BuildingType::NONE || type == BuildingType::DEMOLISH) return false;
    pos = e.snapToBuildingGrid(player, pos);
    int plot = plotIndexAt(e, player, pos);
    if (plot < 0 || !e.landPlots[static_cast<size_t>(plot)].isPurchased) return false;
    for (const auto& b : e.buildings) {
        float dx = b.position.x - pos.x;
        float dy = b.position.y - pos.y;
        if (std::sqrt(dx * dx + dy * dy) < 16.0f) return false; // slot taken
    }
    BuildingCost cost = e.getBuildingCost(type);
    PlacedBuilding b;
    b.type = type;
    b.position = pos;
    b.playerOwner = player;
    b.currentOutputMW = static_cast<float>(cost.basePowerMW);
    b.animTimer = 0.0f;
    b.energyStored = 0.0f;
    b.maxCapacity = static_cast<float>(Balance::BATTERY.batteryCapacityMWh);
    b.lightRadius = (type == BuildingType::LAMP) ? 150.0f : 0.0f;
    e.buildings.push_back(b);
    (player == 1 ? e.p1 : e.p2).lastPlacedBuilding = static_cast<int>(type);
    e.updateBuildingsEnergy(0.0f);
    return true;
}

float DemoEngineAccess::getGameSeconds(const GameEngine& e) {
    return e.gameSeconds;
}

namespace Demo {

// =============================================================================
// Arguments
// =============================================================================
DemoArgs DemoArgs::parse(const std::string& spec, std::string* error) {
    DemoArgs out;
    size_t i = 0;
    const size_t n = spec.size();
    auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
    while (i < n) {
        while (i < n && isSpace(spec[i])) ++i;
        if (i >= n) break;
        size_t keyStart = i;
        while (i < n && spec[i] != '=' && !isSpace(spec[i])) ++i;
        std::string key = spec.substr(keyStart, i - keyStart);
        if (i >= n || spec[i] != '=') {
            if (error) *error = "missing '=' after '" + key + "'";
            out.values[key] = "1"; // a bare word is a flag
            continue;
        }
        ++i; // '='
        std::string value;
        if (i < n && spec[i] == '"') {
            ++i;
            size_t valStart = i;
            while (i < n && spec[i] != '"') ++i;
            value = spec.substr(valStart, i - valStart);
            if (i < n) {
                ++i; // closing quote
            } else if (error) {
                *error = "unterminated quote in '" + key + "'";
            }
        } else {
            size_t valStart = i;
            while (i < n && !isSpace(spec[i])) ++i;
            value = spec.substr(valStart, i - valStart);
        }
        if (key.empty()) {
            if (error) *error = "empty key";
            continue;
        }
        out.values[key] = value;
    }
    return out;
}

bool DemoArgs::has(const std::string& key) const {
    return values.find(key) != values.end();
}

std::string DemoArgs::getString(const std::string& key, const std::string& def) const {
    auto it = values.find(key);
    return (it == values.end()) ? def : it->second;
}

int DemoArgs::getInt(const std::string& key, int def) const {
    auto it = values.find(key);
    if (it == values.end() || it->second.empty()) return def;
    char* end = nullptr;
    long v = std::strtol(it->second.c_str(), &end, 10);
    return (end == it->second.c_str()) ? def : static_cast<int>(v);
}

float DemoArgs::getFloat(const std::string& key, float def) const {
    auto it = values.find(key);
    if (it == values.end() || it->second.empty()) return def;
    char* end = nullptr;
    double v = std::strtod(it->second.c_str(), &end);
    return (end == it->second.c_str()) ? def : static_cast<float>(v);
}

bool DemoArgs::getBool(const std::string& key, bool def) const {
    auto it = values.find(key);
    if (it == values.end()) return def;
    const std::string& v = it->second;
    return !(v == "0" || v == "false" || v == "no" || v == "off");
}

std::vector<std::string> DemoArgs::getList(const std::string& key, char sep) const {
    std::vector<std::string> out;
    std::string v = getString(key);
    std::string cur;
    for (char c : v) {
        if (c == sep) {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

std::vector<std::pair<int, int>> DemoArgs::getPairs(const std::string& key) const {
    std::vector<std::pair<int, int>> out;
    std::string v = getString(key);
    std::string cur;
    auto flush = [&out](const std::string& item) {
        size_t comma = item.find(',');
        if (comma == std::string::npos) return;
        char* e1 = nullptr;
        char* e2 = nullptr;
        std::string a = item.substr(0, comma);
        std::string b = item.substr(comma + 1);
        long x = std::strtol(a.c_str(), &e1, 10);
        long y = std::strtol(b.c_str(), &e2, 10);
        if (e1 != a.c_str() && e2 != b.c_str()) out.push_back(std::make_pair(static_cast<int>(x), static_cast<int>(y)));
    };
    for (char c : v) {
        if (c == ';') {
            flush(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    flush(cur);
    return out;
}

// =============================================================================
// Name tables
// =============================================================================
bool parseBuildingType(const std::string& name, BuildingType& out) {
    if (name == "solar") { out = BuildingType::SOLAR_PANEL; return true; }
    if (name == "wind") { out = BuildingType::WIND_TURBINE; return true; }
    if (name == "hydro") { out = BuildingType::HYDRO_PLANT; return true; }
    if (name == "battery") { out = BuildingType::BATTERY; return true; }
    if (name == "lamp") { out = BuildingType::LAMP; return true; }
    return false;
}

bool parseResourceType(const std::string& name, ResourceType& out) {
    if (name == "wood") { out = ResourceType::WOOD; return true; }
    if (name == "iron") { out = ResourceType::IRON; return true; }
    if (name == "copper") { out = ResourceType::COPPER; return true; }
    if (name == "coal") { out = ResourceType::COAL; return true; }
    if (name == "silicon") { out = ResourceType::SILICON; return true; }
    if (name == "silver") { out = ResourceType::SILVER; return true; }
    if (name == "gold") { out = ResourceType::GOLD; return true; }
    return false;
}

bool parseWeatherType(const std::string& name, WeatherType& out) {
    if (name == "sunny") { out = WeatherType::SUNNY; return true; }
    if (name == "windy") { out = WeatherType::WINDY; return true; }
    if (name == "rainy") { out = WeatherType::RAINY; return true; }
    if (name == "stormy") { out = WeatherType::STORMY; return true; }
    if (name == "snowy") { out = WeatherType::SNOWY; return true; }
    if (name == "cloudy") { out = WeatherType::CLOUDY; return true; }
    return false;
}

bool parseSeasonType(const std::string& name, SeasonType& out) {
    if (name == "spring") { out = SeasonType::SPRING; return true; }
    if (name == "summer") { out = SeasonType::SUMMER; return true; }
    if (name == "autumn" || name == "fall") { out = SeasonType::AUTUMN; return true; }
    if (name == "winter") { out = SeasonType::WINTER; return true; }
    return false;
}

// =============================================================================
// Registry
// =============================================================================
namespace {

std::map<std::string, DemoActionFn>& registry() {
    static std::map<std::string, DemoActionFn> actions; // function-local: safe during static init
    return actions;
}

void registerBuiltinDemoActions(); // below

void ensureBuiltins() {
    static bool done = false;
    if (!done) {
        done = true;
        registerBuiltinDemoActions();
    }
}

// Built-ins never replace an action a feature registered earlier under the same name
void registerBuiltin(const std::string& name, DemoActionFn fn) {
    if (registry().find(name) == registry().end()) registry()[name] = std::move(fn);
}

bool lookupAction(const std::string& name, DemoActionFn& out) {
    ensureBuiltins();
    auto it = registry().find(name);
    if (it == registry().end() || !it->second) return false;
    out = it->second; // copy: an action may (un)register actions while it runs
    return true;
}

std::string fmtTime(float t) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%6.2f", t);
    return std::string(buf);
}

} // namespace

bool registerDemoAction(const std::string& name, DemoActionFn fn) {
    bool isNew = registry().find(name) == registry().end();
    registry()[name] = std::move(fn);
    return isNew;
}

bool unregisterDemoAction(const std::string& name) {
    return registry().erase(name) > 0;
}

bool hasDemoAction(const std::string& name) {
    DemoActionFn fn;
    return lookupAction(name, fn);
}

std::vector<std::string> listDemoActions() {
    ensureBuiltins();
    std::vector<std::string> names;
    for (const auto& kv : registry()) names.push_back(kv.first);
    return names;
}

void DemoContext::cue(const DemoCue& c) {
    director.pushCue(c);
}

void DemoContext::log(const std::string& line) {
    director.addLog(line);
}

// =============================================================================
// Script checks
// =============================================================================
std::vector<std::string> validateScript(const std::vector<DemoBeat>& script) {
    std::vector<std::string> problems;
    float prev = -1.0f;
    for (size_t i = 0; i < script.size(); ++i) {
        const DemoBeat& b = script[i];
        std::string where = "beat " + std::to_string(i) + " (t=" + fmtTime(b.at) + ")";
        if (b.at < prev) problems.push_back(where + ": earlier than the beat before it");
        if (b.at < 0.0f) problems.push_back(where + ": negative time");
        prev = std::max(prev, b.at);
        if (b.titleCard && b.caption.empty()) problems.push_back(where + ": title card without caption");
        if (b.optional && b.action.empty()) problems.push_back(where + ": optional beat without action");
        if (b.optional && b.feature.empty()) problems.push_back(where + ": optional beat without feature name");
        if (!b.action.empty() && !b.optional && !hasDemoAction(b.action)) {
            problems.push_back(where + ": unknown action '" + b.action + "'");
        }
        std::string err;
        DemoArgs::parse(b.args, &err);
        if (!err.empty()) problems.push_back(where + ": bad args (" + err + ")");
        if (b.timeScale < 0.0f || (b.timeScale > 0.0f && b.timeScale <= 0.1f)) {
            problems.push_back(where + ": time scale must be > 0.1");
        }
    }
    return problems;
}

std::vector<std::string> pendingFeatureBeats(const std::vector<DemoBeat>& script) {
    std::vector<std::string> out;
    for (const auto& b : script) {
        if (b.optional && !hasDemoAction(b.action)) out.push_back(b.action + " -> " + b.feature);
    }
    return out;
}

// =============================================================================
// Director
// =============================================================================
DemoDirector::DemoDirector() {
    ensureBuiltins();
}

void DemoDirector::setScript(const std::vector<DemoBeat>& beats) {
    script = beats;
    std::stable_sort(script.begin(), script.end(),
                     [](const DemoBeat& a, const DemoBeat& b) { return a.at < b.at; });
    nextBeat = 0;
}

void DemoDirector::start(GameEngine& engine, unsigned int seed) {
    engine.init(1600.0f, 900.0f);
    // Replace the clock/EC_SEED seed with the fixed demo seed, then roll day 1 again with it
    seedRandom(seed);
    std::srand(seed);
    DemoEngineAccess::rerollWeather(engine);
    engine.setTimeScale(1.0f);

    nextBeat = 0;
    ticks = 0;
    accumulator = 0.0f;
    running = true;
    caption.clear();
    subtitle.clear();
    captionTitleCard = false;
    captionStart = 0.0f;
    captionHold = 0.0f;
    captionSerial = 0;
    lastBeat = -1;
    firedCount = 0;
    skippedCount = 0;
    failedCount = 0;
    logLines.clear();
    cues.clear();
    for (int p = 0; p < 3; ++p) weatherLocked[p] = false;

    addLog("start: seed " + std::to_string(seed) + ", " + std::to_string(script.size()) + " beats, " +
           std::to_string(static_cast<int>(getDuration())) + " s");
    // Beats at t = 0 fire before anything is drawn
    fireDueBeats(engine);
}

void DemoDirector::stop() {
    running = false;
    cues.clear();
}

void DemoDirector::advance(GameEngine& engine, float realDt) {
    if (!running) return;
    if (realDt < 0.0f) realDt = 0.0f;
    accumulator += std::min(realDt, 0.75f);
    int steps = 0;
    while (accumulator >= DEMO_TICK_SEC && steps < DEMO_MAX_TICKS_PER_FRAME) {
        tick(engine);
        accumulator -= DEMO_TICK_SEC;
        ++steps;
    }
    // A slow machine drops the backlog instead of spiralling: the show slows down, the result stays the same
    if (steps == DEMO_MAX_TICKS_PER_FRAME && accumulator > DEMO_TICK_SEC) accumulator = 0.0f;
}

void DemoDirector::tick(GameEngine& engine) {
    // After the last beat the show holds its final frame: every run ends on the same tick
    if (!running || isFinished()) return;
    ++ticks;
    fireDueBeats(engine);
    applyWeatherLocks(engine);
    engine.update(DEMO_TICK_SEC);   // the engine applies its own time scale
    applyWeatherLocks(engine);      // a day rollover inside the step rolled new weather
}

void DemoDirector::fireDueBeats(GameEngine& engine) {
    const float now = getTime() + 1e-4f;
    while (nextBeat < script.size() && script[nextBeat].at <= now) {
        size_t index = nextBeat++;
        fireBeat(engine, index);
    }
}

void DemoDirector::fireBeat(GameEngine& engine, size_t index) {
    const DemoBeat& b = script[index];
    DemoActionFn fn;
    bool known = b.action.empty() || lookupAction(b.action, fn);

    if (!known && b.optional) {
        ++skippedCount;
        addLog("t=" + fmtTime(getTime()) + " beat " + std::to_string(index) + " TODO '" + b.action +
               "' (" + b.feature + ") not registered yet -> skipped");
        return;
    }

    ++firedCount;
    lastBeat = static_cast<int>(index);
    if (b.timeScale > 0.0f) engine.setTimeScale(b.timeScale);

    bool ok = true;
    if (!known) {
        ok = false;
        addLog("t=" + fmtTime(getTime()) + " beat " + std::to_string(index) + " ERROR unknown action '" + b.action + "'");
    } else if (fn) {
        std::string err;
        DemoArgs args = DemoArgs::parse(b.args, &err);
        if (!err.empty()) addLog("beat " + std::to_string(index) + " args warning: " + err);
        DemoContext ctx{ engine, *this, b, getTime() };
        try {
            ok = fn(ctx, args);
        } catch (const std::exception& ex) {
            ok = false;
            addLog(std::string("action threw: ") + ex.what());
        } catch (...) {
            ok = false;
            addLog("action threw an unknown exception");
        }
        addLog("t=" + fmtTime(getTime()) + " beat " + std::to_string(index) + " " + b.action + " " + b.args +
               (ok ? "  [ok]" : "  [FAILED]"));
    }
    if (!ok) ++failedCount;
    applyWeatherLocks(engine);

    if (!b.caption.empty() || !b.subtitle.empty()) {
        caption = expandText(b.caption, engine);
        subtitle = expandText(b.subtitle, engine);
        captionTitleCard = b.titleCard;
        captionStart = getTime();
        captionHold = b.hold;
        ++captionSerial;
    }
}

bool DemoDirector::isFinished() const {
    return running && nextBeat >= script.size() && getTime() >= getDuration();
}

float DemoDirector::getDuration() const {
    float end = 0.0f;
    for (const auto& b : script) {
        end = std::max(end, b.at + (b.hold > 0.0f ? b.hold : 0.0f));
    }
    return end;
}

bool DemoDirector::hasCaption() const {
    if (caption.empty() && subtitle.empty()) return false;
    return captionHold <= 0.0f || getCaptionAge() < captionHold;
}

std::vector<DemoCue> DemoDirector::takeCues() {
    std::vector<DemoCue> out;
    out.swap(cues);
    return out;
}

void DemoDirector::addLog(const std::string& line) {
    logLines.push_back(line);
    if (logLines.size() > 400) logLines.erase(logLines.begin());
    if (echo) std::cout << "[Demo] " << line << "\n";
}

void DemoDirector::lockWeather(int player, WeatherType w) {
    for (int p = 1; p <= 2; ++p) {
        if (player == 0 || player == p) {
            weatherLocked[p] = true;
            lockedWeather[p] = w;
        }
    }
}

void DemoDirector::unlockWeather(int player) {
    for (int p = 1; p <= 2; ++p) {
        if (player == 0 || player == p) weatherLocked[p] = false;
    }
}

bool DemoDirector::isWeatherLocked(int player) const {
    return (player == 1 || player == 2) && weatherLocked[player];
}

void DemoDirector::applyWeatherLocks(GameEngine& engine) const {
    for (int p = 1; p <= 2; ++p) {
        if (weatherLocked[p] && engine.getPlayerWeather(p) != lockedWeather[p]) {
            DemoEngineAccess::setWeather(engine, p, lockedWeather[p]);
        }
    }
}

std::string DemoDirector::expandText(const std::string& text, const GameEngine& engine) const {
    if (text.find('{') == std::string::npos) return text;
    const auto& city = engine.getCityState();
    int p1Pct = static_cast<int>(std::lround(city.p1CityShare * 100.0f));
    auto seasonBg = [](SeasonType s) -> std::string {
        switch (s) {
            case SeasonType::SPRING: return "ПРОЛЕТ";
            case SeasonType::SUMMER: return "ЛЯТО";
            case SeasonType::AUTUMN: return "ЕСЕН";
            case SeasonType::WINTER: return "ЗИМА";
        }
        return "";
    };
    std::map<std::string, std::string> vars;
    vars["result"] = city.lastCutMessage;
    vars["day"] = std::to_string(engine.getCurrentDay());
    vars["p1"] = std::to_string(p1Pct);
    vars["p2"] = std::to_string(100 - p1Pct);
    vars["demand"] = std::to_string(city.cityEnergyDemand);
    vars["mw1"] = std::to_string(engine.getPlayerEconomy(1).energyMW);
    vars["mw2"] = std::to_string(engine.getPlayerEconomy(2).energyMW);
    vars["season"] = seasonBg(engine.getSeason());

    std::string out;
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '{') {
            size_t close = text.find('}', i);
            if (close != std::string::npos) {
                auto it = vars.find(text.substr(i + 1, close - i - 1));
                if (it != vars.end()) {
                    out += it->second;
                    i = close + 1;
                    continue;
                }
            }
        }
        out += text[i++];
    }
    return out;
}

// =============================================================================
// Built-in actions
// =============================================================================
namespace {

const char* resourceNameBg(ResourceType t) {
    switch (t) {
        case ResourceType::WOOD: return "Дървесина";
        case ResourceType::IRON: return "Желязо";
        case ResourceType::COPPER: return "Мед";
        case ResourceType::COAL: return "Въглища";
        case ResourceType::SILICON: return "Силиций";
        case ResourceType::SILVER: return "Сребро";
        case ResourceType::GOLD: return "Злато";
        default: return "Ресурс";
    }
}

// player=0 (or missing when allowBoth) means both players
std::vector<int> playersOf(const DemoArgs& a, bool allowBoth) {
    int p = a.getInt("player", allowBoth ? 0 : 1);
    if (p == 1 || p == 2) return { p };
    if (allowBoth) return { 1, 2 };
    return {};
}

void addResource(PlayerEconomy& econ, ResourceType t, int amount) {
    switch (t) {
        case ResourceType::WOOD: econ.wood += amount; econ.data.wood = econ.wood; break;
        case ResourceType::IRON: econ.iron += amount; econ.data.iron = econ.iron; break;
        case ResourceType::COPPER: econ.copper += amount; econ.data.copper = econ.copper; break;
        case ResourceType::COAL: econ.coal += amount; econ.data.coal = econ.coal; break;
        case ResourceType::SILICON: econ.silicon += amount; econ.data.silicon = econ.silicon; break;
        case ResourceType::SILVER: econ.silver += amount; econ.data.silver = econ.silver; break;
        case ResourceType::GOLD: econ.gold += amount; econ.data.gold = econ.gold; break;
        case ResourceType::MONEY: econ.money += amount; econ.data.money = econ.money; break;
        default: break;
    }
}

// Grants exactly what is missing for one building, so the real placement path can be used
void grantMissingFor(GameEngine& engine, int player, BuildingType type) {
    PlayerEconomy& econ = engine.getPlayerEconomyMut(player);
    BuildingCost c = engine.getBuildingCost(type);
    addResource(econ, ResourceType::WOOD, std::max(0, c.woodCost - econ.wood));
    addResource(econ, ResourceType::IRON, std::max(0, c.ironCost - econ.iron));
    addResource(econ, ResourceType::COPPER, std::max(0, c.copperCost - econ.copper));
    addResource(econ, ResourceType::COAL, std::max(0, c.coalCost - econ.coal));
    addResource(econ, ResourceType::SILICON, std::max(0, c.siliconCost - econ.silicon));
    addResource(econ, ResourceType::SILVER, std::max(0, c.silverCost - econ.silver));
}

bool actWeather(DemoContext& ctx, const DemoArgs& a) {
    WeatherType w;
    if (!parseWeatherType(a.getString("type"), w)) return false;
    bool lock = a.getBool("lock", true);
    for (int p : playersOf(a, true)) {
        DemoEngineAccess::setWeather(ctx.engine, p, w);
        if (lock) ctx.director.lockWeather(p, w);
        else ctx.director.unlockWeather(p);
        DemoCue c;
        c.kind = "weather";
        c.player = p;
        c.value = static_cast<int>(w);
        c.text = getWeatherName(w);
        ctx.cue(c);
    }
    return true;
}

bool actUnlockWeather(DemoContext& ctx, const DemoArgs& a) {
    for (int p : playersOf(a, true)) ctx.director.unlockWeather(p);
    return true;
}

bool actJumpDay(DemoContext& ctx, const DemoArgs& a) {
    int day = a.getInt("day", ctx.engine.getCurrentDay() + 1);
    if (day < 1) return false;
    DemoEngineAccess::jumpToDay(ctx.engine, day, a.getFloat("hour", 8.0f));
    ctx.director.applyWeatherLocks(ctx.engine);
    DemoCue c;
    c.kind = "jump";
    c.value = day;
    c.text = "ДЕН " + std::to_string(day);
    ctx.cue(c);
    return true;
}

// season type=summer [hour=8]: first day of the next such season (today if it already is)
bool actSeason(DemoContext& ctx, const DemoArgs& a) {
    SeasonType target;
    if (!parseSeasonType(a.getString("type"), target)) return false;
    int day = ctx.engine.getCurrentDay();
    if (Balance::getSeasonForDay(day) == target) {
        return true;
    }
    int guard = 0;
    while (Balance::getSeasonForDay(day) != target && guard++ < 40) ++day;
    DemoEngineAccess::jumpToDay(ctx.engine, day, a.getFloat("hour", 8.0f));
    ctx.director.applyWeatherLocks(ctx.engine);
    DemoCue c;
    c.kind = "jump";
    c.value = day;
    c.text = "ДЕН " + std::to_string(day);
    ctx.cue(c);
    return true;
}

// advance hours=3 | to_hour=20  (simulates; crossing 06:00 settles the day)
bool actAdvance(DemoContext& ctx, const DemoArgs& a) {
    if (a.has("to_hour")) {
        DemoEngineAccess::advanceToHour(ctx.engine, a.getFloat("to_hour", 12.0f));
    } else {
        float hours = a.getFloat("hours", 1.0f);
        if (hours <= 0.0f) return false;
        DemoEngineAccess::advanceGameSeconds(ctx.engine, hours / 24.0f * Balance::SECONDS_PER_DAY);
    }
    ctx.director.applyWeatherLocks(ctx.engine);
    return true;
}

// settle: run the rest of today at full fidelity -> the real day-end settlement
bool actSettle(DemoContext& ctx, const DemoArgs& a) {
    (void)a;
    int before = ctx.engine.getCurrentDay();
    DemoEngineAccess::settleDay(ctx.engine);
    ctx.director.applyWeatherLocks(ctx.engine);
    const auto& city = ctx.engine.getCityState();
    DemoCue c;
    c.kind = "settle";
    c.value = city.winner;
    c.text = city.lastCutMessage;
    ctx.cue(c);
    return ctx.engine.getCurrentDay() == before + 1 || city.winner != 0;
}

// build player=1 type=solar cells=c,r;c,r [pay=0]
// Grid cells are the engine's 9 x 12 slot grid of that player (3 x 3 slots per plot).
// The plot is bought for free, missing resources are granted (unless pay=1), and the
// real placement rules run first; only a night-time refusal falls back to a forced place.
bool actBuild(DemoContext& ctx, const DemoArgs& a) {
    BuildingType type;
    if (!parseBuildingType(a.getString("type"), type)) return false;
    std::vector<int> players = playersOf(a, false);
    if (players.empty()) return false;
    int player = players[0];
    auto cells = a.getPairs("cells");
    if (cells.empty()) cells = a.getPairs("cell");
    if (cells.empty()) return false;
    bool pay = a.getBool("pay", false);

    bool allOk = true;
    for (const auto& cell : cells) {
        sf::Vector2f pos = ctx.engine.getGridSlot(player, cell.first, cell.second);
        int plot = DemoEngineAccess::plotIndexAt(ctx.engine, player, pos);
        if (plot >= 0) {
            const LandPlot& lp = ctx.engine.getLandPlots()[static_cast<size_t>(plot)];
            if (!lp.isPurchased) {
                int local = plot - (player == 2 ? Balance::PLOTS_PER_PLAYER : 0);
                DemoEngineAccess::purchasePlotFree(ctx.engine, player, local / 3, local % 3);
                DemoCue pc;
                pc.kind = "plot";
                pc.player = player;
                pc.pos = lp.bounds.getCenter();
                pc.hasPos = true;
                ctx.cue(pc);
            }
        }
        if (!pay) grantMissingFor(ctx.engine, player, type);

        std::string msg;
        bool placed = ctx.engine.placeBuilding(player, type, pos, msg);
        if (!placed && !pay && !ctx.engine.isDaylight()) {
            placed = DemoEngineAccess::placeBuildingForced(ctx.engine, player, type, pos);
            if (placed) msg = "forced (night)";
        }
        if (!placed) {
            ctx.log("build failed at " + std::to_string(cell.first) + "," + std::to_string(cell.second) + ": " + msg);
            allOk = false;
            continue;
        }
        DemoCue c;
        c.kind = "build";
        c.player = player;
        c.pos = ctx.engine.snapToBuildingGrid(player, pos);
        c.hasPos = true;
        c.value = static_cast<int>(type);
        c.text = "ПОСТРОЕНО: " + ctx.engine.getBuildingCost(type).nameBg;
        ctx.cue(c);
    }
    return allOk;
}

// buy_plot player=1 plot=row,col[;row,col]  (free)
bool actBuyPlot(DemoContext& ctx, const DemoArgs& a) {
    std::vector<int> players = playersOf(a, false);
    if (players.empty()) return false;
    int player = players[0];
    auto plots = a.getPairs("plot");
    if (plots.empty()) return false;
    bool ok = true;
    for (const auto& rc : plots) {
        if (!DemoEngineAccess::purchasePlotFree(ctx.engine, player, rc.first, rc.second)) {
            ok = false;
            continue;
        }
        int index = (player == 2 ? Balance::PLOTS_PER_PLAYER : 0) + rc.first * 3 + rc.second;
        DemoCue c;
        c.kind = "plot";
        c.player = player;
        c.pos = ctx.engine.getLandPlots()[static_cast<size_t>(index)].bounds.getCenter();
        c.hasPos = true;
        c.text = "ЗАКУПЕН ПАРЦЕЛ!";
        ctx.cue(c);
    }
    return ok;
}

// mine player=0 res=wood,iron times=2  (the real mining call, levels apply)
bool actMine(DemoContext& ctx, const DemoArgs& a) {
    std::vector<std::string> names = a.getList("res");
    if (names.empty()) return false;
    int times = std::max(1, std::min(a.getInt("times", 1), 50));
    bool ok = true;
    for (int p : playersOf(a, true)) {
        for (const auto& name : names) {
            ResourceType t;
            if (!parseResourceType(name, t)) {
                ok = false;
                continue;
            }
            int total = 0;
            for (int k = 0; k < times; ++k) {
                GameEngine::MineResult r;
                std::string msg;
                if (ctx.engine.mineResource(p, t, r, msg)) total += r.amount;
            }
            DemoCue c;
            c.kind = "mine";
            c.player = p;
            c.value = static_cast<int>(t);
            c.text = "+" + std::to_string(total) + " " + resourceNameBg(t);
            ctx.cue(c);
        }
    }
    return ok;
}

// grant player=1 wood=50 iron=20 ... gold=300 money=100
bool actGrant(DemoContext& ctx, const DemoArgs& a) {
    static const char* keys[] = { "wood", "iron", "copper", "coal", "silicon", "silver", "gold" };
    for (int p : playersOf(a, true)) {
        PlayerEconomy& econ = ctx.engine.getPlayerEconomyMut(p);
        for (const char* k : keys) {
            ResourceType t;
            parseResourceType(k, t);
            addResource(econ, t, a.getInt(k, 0));
        }
        addResource(econ, ResourceType::MONEY, a.getInt("money", 0));
    }
    return true;
}

bool actShare(DemoContext& ctx, const DemoArgs& a) {
    if (!a.has("p1")) return false;
    DemoEngineAccess::setCityShare(ctx.engine, a.getFloat("p1", 0.5f));
    return true;
}

bool actDemand(DemoContext& ctx, const DemoArgs& a) {
    if (!a.has("mw")) return false;
    DemoEngineAccess::setCityDemand(ctx.engine, a.getInt("mw", 0));
    return true;
}

// winner player=1 [ifnone=1] [text="..."] [share=0.86]
bool actWinner(DemoContext& ctx, const DemoArgs& a) {
    int w = a.getInt("player", 1);
    if (w < 1 || w > 3) return false;
    const auto& city = ctx.engine.getCityState();
    if (a.getBool("ifnone", false) && city.winner != 0) {
        ctx.log("winner already decided by the engine: " + std::to_string(city.winner));
        return true;
    }
    if (a.has("share")) DemoEngineAccess::setCityShare(ctx.engine, a.getFloat("share", 0.5f));
    std::string text = a.getString("text");
    if (text.empty()) {
        text = (w == 3) ? "РАВЕНСТВО!" : (w == 1 ? "ПОБЕДА ЗА ИГРАЧ 1 (ЗАПАД)!" : "ПОБЕДА ЗА ИГРАЧ 2 (ИЗТОК)!");
    }
    DemoEngineAccess::forceWinner(ctx.engine, w, text);
    DemoCue c;
    c.kind = "winner";
    c.value = w;
    c.text = text;
    ctx.cue(c);
    return true;
}

// lightning player=2 cell=7,0 [hit=1]: a scripted strike (hit=1 destroys the building in that cell)
bool actLightning(DemoContext& ctx, const DemoArgs& a) {
    std::vector<int> players = playersOf(a, false);
    if (players.empty()) return false;
    int player = players[0];
    auto cells = a.getPairs("cell");
    if (cells.empty()) return false;
    sf::Vector2f pos = ctx.engine.getGridSlot(player, cells[0].first, cells[0].second);
    bool hit = a.getBool("hit", true);
    std::string destroyedName;
    if (hit) {
        for (const auto& b : ctx.engine.getBuildings()) {
            if (b.playerOwner == player && std::hypot(b.position.x - pos.x, b.position.y - pos.y) < 16.0f) {
                destroyedName = ctx.engine.getBuildingCost(b.type).nameBg;
                break;
            }
        }
        hit = !destroyedName.empty() && ctx.engine.breakBuildingAt(pos);
    }
    DemoCue c;
    c.kind = "lightning";
    c.player = player;
    c.pos = pos;
    c.hasPos = true;
    c.value = hit ? 1 : 0;
    c.text = destroyedName;
    ctx.cue(c);
    return !a.getBool("hit", true) || hit;
}

// notice text="..." [player=1] [x=800 y=450]: floating text in the UI
bool actNotice(DemoContext& ctx, const DemoArgs& a) {
    DemoCue c;
    c.kind = "notice";
    c.player = a.getInt("player", 0);
    c.text = a.getString("text");
    if (a.has("x") && a.has("y")) {
        c.pos = sf::Vector2f(a.getFloat("x", 800.0f), a.getFloat("y", 450.0f));
        c.hasPos = true;
    }
    if (c.text.empty()) return false;
    ctx.cue(c);
    return true;
}

bool actMessage(DemoContext& ctx, const DemoArgs& a) {
    DemoEngineAccess::setCityMessage(ctx.engine, a.getString("text"));
    return true;
}

bool actTimeScale(DemoContext& ctx, const DemoArgs& a) {
    float v = a.getFloat("value", 1.0f);
    if (v <= 0.1f) return false;
    ctx.engine.setTimeScale(v);
    return true;
}

bool actNoop(DemoContext&, const DemoArgs&) {
    return true;
}

void registerBuiltinDemoActions() {
    registerBuiltin("weather", actWeather);
    registerBuiltin("unlock_weather", actUnlockWeather);
    registerBuiltin("season", actSeason);
    registerBuiltin("jump_day", actJumpDay);
    registerBuiltin("advance", actAdvance);
    registerBuiltin("settle", actSettle);
    registerBuiltin("build", actBuild);
    registerBuiltin("buy_plot", actBuyPlot);
    registerBuiltin("mine", actMine);
    registerBuiltin("grant", actGrant);
    registerBuiltin("share", actShare);
    registerBuiltin("demand", actDemand);
    registerBuiltin("winner", actWinner);
    registerBuiltin("lightning", actLightning);
    registerBuiltin("notice", actNotice);
    registerBuiltin("message", actMessage);
    registerBuiltin("timescale", actTimeScale);
    registerBuiltin("noop", actNoop);
}

} // namespace

} // namespace Demo
