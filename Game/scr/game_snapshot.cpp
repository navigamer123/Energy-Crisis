// =============================================================================
// ENERGY CRISIS - MATCH SNAPSHOTS (save / load)
// A versioned, line-based text format covering all match state: rules, RNG streams, clock,
// weather, city, both economies, modifiers, land plots, buildings and the power world (map layout,
// terrain, reactors, mega-projects, hazard plans and the hazard stream). Floats are written with
// enough digits to read back bit-exactly, so a loaded match continues exactly like the original.
// Not saved: the pending event and power-FX queues, host settings (setMaxStepsPerUpdate,
// setHazardsEnabled) and std::rand,
// which only drives cosmetic UI randomness.
// =============================================================================
#include "../includes/game_main.h"
#include <cstdint>
#include <cstdlib>
#include <istream>
#include <limits>
#include <locale>
#include <ostream>
#include <sstream>

namespace {

const char* const SNAPSHOT_MAGIC = "ENERGY_CRISIS_STATE";

constexpr int MAX_SNAPSHOT_PLOTS = 1000;
constexpr int MAX_SNAPSHOT_BUILDINGS = 100000;
constexpr int MAX_SNAPSHOT_MUTATORS = 1000;
constexpr size_t MAX_SNAPSHOT_STRING = 1u << 20;

// ---------------------------------------------------------------------------
// Writing
// ---------------------------------------------------------------------------
struct Writer {
    std::ostringstream os;
    Writer() { os.imbue(std::locale::classic()); }

    Writer& f(float v) {
        os << ' ';
        os.precision(std::numeric_limits<float>::max_digits10);
        os << v;
        return *this;
    }
    Writer& d(double v) {
        os << ' ';
        os.precision(std::numeric_limits<double>::max_digits10);
        os << v;
        return *this;
    }
    Writer& i(long long v) {
        os << ' ' << v;
        return *this;
    }
    Writer& u(unsigned long long v) {
        os << ' ' << v;
        return *this;
    }
    // Strings may hold spaces and UTF-8: written as <byte count>:<bytes>
    Writer& s(const std::string& v) {
        os << ' ' << v.size() << ':' << v;
        return *this;
    }
    Writer& tag(const char* t) {
        os << t;
        return *this;
    }
    void endl() { os << '\n'; }
};

// ---------------------------------------------------------------------------
// Reading: every helper returns false on malformed input
// ---------------------------------------------------------------------------
struct Reader {
    std::istream& is;
    explicit Reader(std::istream& in) : is(in) {}

    bool tag(const char* expected) {
        std::string t;
        return static_cast<bool>(is >> t) && t == expected;
    }
    // Numbers go through strtof / strtod: tiny (subnormal) values read back instead of failing the stream
    bool f(float& v) {
        std::string t;
        if (!(is >> t)) return false;
        char* end = nullptr;
        v = std::strtof(t.c_str(), &end);
        return end != t.c_str() && *end == 0;
    }
    bool d(double& v) {
        std::string t;
        if (!(is >> t)) return false;
        char* end = nullptr;
        v = std::strtod(t.c_str(), &end);
        return end != t.c_str() && *end == 0;
    }
    bool i(int& v) { return static_cast<bool>(is >> v); }
    bool i(int& v, int lo, int hi) { return i(v) && v >= lo && v <= hi; }
    bool u64(uint64_t& v) {
        unsigned long long x = 0;
        if (!(is >> x)) return false;
        v = static_cast<uint64_t>(x);
        return true;
    }
    bool b(bool& v) {
        int x = 0;
        if (!i(x, 0, 1)) return false;
        v = (x != 0);
        return true;
    }
    bool s(std::string& v) {
        unsigned long long n = 0;
        char colon = 0;
        if (!(is >> n) || n > MAX_SNAPSHOT_STRING || !is.get(colon) || colon != ':') return false;
        v.assign(static_cast<size_t>(n), '\0');
        if (n > 0 && !is.read(&v[0], static_cast<std::streamsize>(n))) return false;
        return true;
    }
};

// Restores the locale of a caller's stream when loading ends
struct LocaleGuard {
    std::istream& is;
    std::locale saved;
    explicit LocaleGuard(std::istream& in) : is(in), saved(in.imbue(std::locale::classic())) {}
    ~LocaleGuard() { is.imbue(saved); }
};

void writeRng(Writer& w, const GameRng& r) {
    w.u(r.getState()).u(r.getIncrement());
}

bool readRng(Reader& r, GameRng& out) {
    uint64_t state = 0, inc = 0;
    if (!r.u64(state) || !r.u64(inc)) return false;
    out.setState(state, inc);
    return true;
}

void writeEconomy(Writer& w, int player, const PlayerEconomy& p) {
    w.tag("player").i(player).i(p.money).i(p.gold).i(p.silver).i(p.iron).i(p.coal).i(p.copper).i(p.silicon).i(p.wood)
        .i(p.ore).i(p.energyMW).i(p.landTier).f(p.cityInfluence).i(p.selectedBuilding).i(p.lastPlacedBuilding);
    for (int level : p.mineLevels) w.i(level);
    w.endl();
}

bool readEconomy(Reader& r, int player, PlayerEconomy& p) {
    int id = 0;
    if (!r.tag("player") || !r.i(id) || id != player) return false;
    if (!r.i(p.money) || !r.i(p.gold) || !r.i(p.silver) || !r.i(p.iron) || !r.i(p.coal) || !r.i(p.copper) ||
        !r.i(p.silicon) || !r.i(p.wood) || !r.i(p.ore) || !r.i(p.energyMW) || !r.i(p.landTier) || !r.f(p.cityInfluence) ||
        !r.i(p.selectedBuilding, 0, static_cast<int>(BuildingType::DEMOLISH)) ||
        !r.i(p.lastPlacedBuilding, 0, static_cast<int>(BuildingType::DEMOLISH))) {
        return false;
    }
    for (int& level : p.mineLevels) {
        if (!r.i(level, 0, Balance::MINE_MAX_LEVEL)) return false;
    }
    return true;
}

void writeMods(Writer& w, int player, const PlayerModifiers& m) {
    w.tag("mods").i(player).f(m.incomeMult).f(m.mineYieldMult).f(m.costMult).f(m.cooldownMult).f(m.shareBonus);
    w.endl();
}

bool readMods(Reader& r, int player, PlayerModifiers& m) {
    int id = 0;
    return r.tag("mods") && r.i(id) && id == player && r.f(m.incomeMult) && r.f(m.mineYieldMult) && r.f(m.costMult) &&
           r.f(m.cooldownMult) && r.f(m.shareBonus);
}

} // namespace

bool GameEngine::saveState(std::ostream& out) const {
    Writer w;
    w.tag(SNAPSHOT_MAGIC).i(SNAPSHOT_VERSION);
    w.endl();

    w.tag("config").i(config.finalDay).f(config.victoryShare).f(config.daySeconds).i(config.graceDays).u(config.seed)
        .i(config.mapPreset).i(config.sandbox ? 1 : 0).i(static_cast<long long>(config.mutators.size()));
    w.endl();
    for (const auto& m : config.mutators) {
        w.tag("mutator").s(m);
        w.endl();
    }

    w.tag("rng").u(matchSeed);
    writeRng(w, weatherRng);
    writeRng(w, hazardRng);
    writeRng(w, generalRng);
    w.endl();

    w.tag("clock").d(gameSeconds).i(currentDay).f(hour24).f(revenueTimer).f(timeScale).d(stepAccumulator)
        .i(static_cast<int>(currentSeason));
    w.endl();

    w.tag("weather").i(static_cast<int>(p1Weather)).i(static_cast<int>(p2Weather)).f(p1WindSpeed).f(p2WindSpeed)
        .i(p1WindDirection).i(p2WindDirection);
    w.endl();

    w.tag("city").i(city.cityEnergyDemand).f(city.p1CityShare).f(city.p1DailyDelivered).f(city.p2DailyDelivered)
        .f(city.dailySeconds).i(city.winner).s(city.lastCutMessage);
    w.endl();

    writeEconomy(w, 1, p1);
    writeEconomy(w, 2, p2);
    writeMods(w, 1, p1Mods);
    writeMods(w, 2, p2Mods);

    w.tag("plots").i(static_cast<long long>(landPlots.size()));
    w.endl();
    for (const auto& plot : landPlots) {
        w.tag("plot").i(plot.id).i(plot.playerOwner).f(plot.bounds.position.x).f(plot.bounds.position.y)
            .f(plot.bounds.size.x).f(plot.bounds.size.y).i(plot.isPurchased ? 1 : 0).i(plot.costGold).i(plot.terrain)
            .i(plot.screenCol).i(plot.row);
        w.endl();
    }

    w.tag("buildings").i(static_cast<long long>(buildings.size()));
    w.endl();
    for (const auto& b : buildings) {
        w.tag("building").i(static_cast<int>(b.type)).f(b.position.x).f(b.position.y).i(b.playerOwner)
            .f(b.currentOutputMW).f(b.animTimer).f(b.energyStored).f(b.maxCapacity).f(b.lightRadius).i(b.isBroken ? 1 : 0)
            .i(b.terrain).i(b.damageKind).f(b.rampProgress).f(b.scramTimer).i(b.needsFuel ? 1 : 0).f(b.constructionLeft)
            .f(b.constructionTotal);
        w.endl();
    }

    // Team b-power: map layout, terrain, reactors, mega-projects, hazard plans and the hazard stream
    w.tag("power").i(static_cast<int>(mapPreset)).u(mapSeedOverride).i(static_cast<int>(layout.preset)).u(layout.seed)
        .i(layout.plotCols).i(layout.plotRows).f(layout.plotW).f(layout.plotH).f(layout.gapX).f(layout.gapY)
        .f(layout.westStartX).f(layout.startY).i(layout.startCol).i(layout.startRow).i(layout.landCostGrowth)
        .i(static_cast<long long>(layout.cells.size()));
    for (int c : layout.cells) w.i(c);
    w.endl();
    w.tag("world");
    for (int k = 0; k < 3; ++k) w.i(world.rainStreak[k]).i(world.dryStreak[k]).i(world.hailToday[k] ? 1 : 0);
    w.i(world.megaUnlockAnnounced ? 1 : 0).i(static_cast<long long>(world.plans.size()));
    w.endl();
    for (const auto& h : world.plans) {
        w.tag("hazard").i(static_cast<int>(h.kind)).i(h.player).f(h.atDaySeconds).i(h.westCol).i(h.row).i(h.fired ? 1 : 0);
        w.endl();
    }
    w.tag("worldrng");
    w.os << ' ' << world.rng;
    w.endl();

    w.tag("end");
    w.endl();

    out << w.os.str();
    return static_cast<bool>(out);
}

bool GameEngine::loadState(std::istream& in) {
    LocaleGuard guard(in);
    Reader r(in);
    GameEngine s; // filled completely, then committed only when everything parsed

    int version = 0;
    if (!r.tag(SNAPSHOT_MAGIC) || !r.i(version) || version != SNAPSHOT_VERSION) return false;

    int mutatorCount = 0;
    int sandbox = 0;
    uint64_t seed = 0;
    MatchConfig& c = s.config;
    if (!r.tag("config") || !r.i(c.finalDay) || !r.f(c.victoryShare) || !r.f(c.daySeconds) || !r.i(c.graceDays) ||
        !r.u64(seed) || seed > 0xFFFFFFFFull || !r.i(c.mapPreset) || !r.i(sandbox, 0, 1) ||
        !r.i(mutatorCount, 0, MAX_SNAPSHOT_MUTATORS)) {
        return false;
    }
    c.seed = static_cast<uint32_t>(seed);
    c.sandbox = (sandbox != 0);
    if (c.finalDay < MatchConfig::MIN_FINAL_DAY || c.finalDay > MatchConfig::MAX_FINAL_DAY ||
        !(c.victoryShare >= MatchConfig::MIN_VICTORY_SHARE && c.victoryShare <= MatchConfig::MAX_VICTORY_SHARE) ||
        !(c.daySeconds >= MatchConfig::MIN_DAY_SECONDS && c.daySeconds <= MatchConfig::MAX_DAY_SECONDS) ||
        c.graceDays < 0 || c.graceDays >= c.finalDay) {
        return false;
    }
    c.mutators.resize(static_cast<size_t>(mutatorCount));
    for (auto& m : c.mutators) {
        if (!r.tag("mutator") || !r.s(m)) return false;
    }

    uint64_t matchSeed64 = 0;
    if (!r.tag("rng") || !r.u64(matchSeed64) || matchSeed64 > 0xFFFFFFFFull || !readRng(r, s.weatherRng) ||
        !readRng(r, s.hazardRng) || !readRng(r, s.generalRng)) {
        return false;
    }
    s.matchSeed = static_cast<uint32_t>(matchSeed64);

    int season = 0;
    if (!r.tag("clock") || !r.d(s.gameSeconds) || !r.i(s.currentDay, 1, 1000000) || !r.f(s.hour24) || !r.f(s.revenueTimer) ||
        !r.f(s.timeScale) || !r.d(s.stepAccumulator) || !r.i(season, 0, 3)) {
        return false;
    }
    s.currentSeason = static_cast<SeasonType>(season);
    const int maxWeather = static_cast<int>(WeatherType::CLOUDY);
    int w1 = 0, w2 = 0;
    if (!r.tag("weather") || !r.i(w1, 0, maxWeather) || !r.i(w2, 0, maxWeather) || !r.f(s.p1WindSpeed) || !r.f(s.p2WindSpeed) ||
        !r.i(s.p1WindDirection, -1, 1) || !r.i(s.p2WindDirection, -1, 1)) {
        return false;
    }
    s.p1Weather = static_cast<WeatherType>(w1);
    s.p2Weather = static_cast<WeatherType>(w2);

    CityConquestState& cs = s.city;
    if (!r.tag("city") || !r.i(cs.cityEnergyDemand) || !r.f(cs.p1CityShare) || !r.f(cs.p1DailyDelivered) ||
        !r.f(cs.p2DailyDelivered) || !r.f(cs.dailySeconds) || !r.i(cs.winner, 0, 3) || !r.s(cs.lastCutMessage)) {
        return false;
    }

    if (!readEconomy(r, 1, s.p1) || !readEconomy(r, 2, s.p2) || !readMods(r, 1, s.p1Mods) || !readMods(r, 2, s.p2Mods)) {
        return false;
    }

    int plotCount = 0;
    if (!r.tag("plots") || !r.i(plotCount, 0, MAX_SNAPSHOT_PLOTS)) return false;
    s.landPlots.resize(static_cast<size_t>(plotCount));
    for (auto& plot : s.landPlots) {
        float x = 0.0f, y = 0.0f, wdt = 0.0f, hgt = 0.0f;
        if (!r.tag("plot") || !r.i(plot.id) || !r.i(plot.playerOwner, 1, 2) || !r.f(x) || !r.f(y) || !r.f(wdt) || !r.f(hgt) ||
            !r.b(plot.isPurchased) || !r.i(plot.costGold) || !r.i(plot.terrain, 0, 4) || !r.i(plot.screenCol) ||
            !r.i(plot.row)) {
            return false;
        }
        plot.bounds = sf::FloatRect({ x, y }, { wdt, hgt });
    }

    int buildingCount = 0;
    if (!r.tag("buildings") || !r.i(buildingCount, 0, MAX_SNAPSHOT_BUILDINGS)) return false;
    s.buildings.resize(static_cast<size_t>(buildingCount));
    for (auto& b : s.buildings) {
        int type = 0;
        float x = 0.0f, y = 0.0f;
        if (!r.tag("building") || !r.i(type, static_cast<int>(BuildingType::SOLAR_PANEL), static_cast<int>(BuildingType::MEGA_PUMPED_HYDRO)) ||
            type == static_cast<int>(BuildingType::DEMOLISH) ||
            !r.f(x) || !r.f(y) || !r.i(b.playerOwner, 1, 2) || !r.f(b.currentOutputMW) || !r.f(b.animTimer) ||
            !r.f(b.energyStored) || !r.f(b.maxCapacity) || !r.f(b.lightRadius) || !r.b(b.isBroken) || !r.i(b.terrain, 0, 4) ||
            !r.i(b.damageKind, 0, static_cast<int>(HazardKind::QUAKE)) || !r.f(b.rampProgress) || !r.f(b.scramTimer) ||
            !r.b(b.needsFuel) || !r.f(b.constructionLeft) || !r.f(b.constructionTotal)) {
            return false;
        }
        b.type = static_cast<BuildingType>(type);
        b.position = { x, y };
    }

    int preset = 0, lpreset = 0, cellCount = 0;
    uint64_t mapSeed = 0, layoutSeed = 0;
    const int maxPreset = static_cast<int>(MapPreset::COUNT) - 1;
    MapLayout& L = s.layout;
    if (!r.tag("power") || !r.i(preset, 0, maxPreset) || !r.u64(mapSeed) || mapSeed > 0xFFFFFFFFull ||
        !r.i(lpreset, 0, maxPreset) || !r.u64(layoutSeed) || layoutSeed > 0xFFFFFFFFull || !r.i(L.plotCols, 1, 64) ||
        !r.i(L.plotRows, 1, 64) || !r.f(L.plotW) || !r.f(L.plotH) || !r.f(L.gapX) || !r.f(L.gapY) || !r.f(L.westStartX) ||
        !r.f(L.startY) || !r.i(L.startCol) || !r.i(L.startRow) || !r.i(L.landCostGrowth) || !r.i(cellCount, 0, 64 * 64)) {
        return false;
    }
    s.mapPreset = static_cast<MapPreset>(preset);
    s.mapSeedOverride = static_cast<unsigned>(mapSeed);
    L.preset = static_cast<MapPreset>(lpreset);
    L.seed = static_cast<unsigned>(layoutSeed);
    L.cells.resize(static_cast<size_t>(cellCount));
    for (int& c : L.cells) {
        if (!r.i(c, -1, 4)) return false;
    }
    PowerWorldState& W = s.world;
    int planCount = 0;
    if (!r.tag("world")) return false;
    for (int k = 0; k < 3; ++k) {
        if (!r.i(W.rainStreak[k], 0, 1000000) || !r.i(W.dryStreak[k], 0, 1000000) || !r.b(W.hailToday[k])) return false;
    }
    if (!r.b(W.megaUnlockAnnounced) || !r.i(planCount, 0, MAX_SNAPSHOT_MUTATORS)) return false;
    W.plans.resize(static_cast<size_t>(planCount));
    for (auto& h : W.plans) {
        int kind = 0;
        if (!r.tag("hazard") || !r.i(kind, 0, static_cast<int>(HazardKind::QUAKE)) || !r.i(h.player, 0, 2) ||
            !r.f(h.atDaySeconds) || !r.i(h.westCol) || !r.i(h.row) || !r.b(h.fired)) {
            return false;
        }
        h.kind = static_cast<HazardKind>(kind);
    }
    if (!r.tag("worldrng") || !(in >> W.rng)) return false;
    W.fx.clear();

    if (!r.tag("end")) return false;

    // Commit: host settings stay (step cap, random hazards on/off), pending events and power FX of the old match
    // are dropped
    s.maxStepsPerUpdate = maxStepsPerUpdate;
    s.hazardsEnabled = hazardsEnabled;
    *this = std::move(s);
    return true;
}
