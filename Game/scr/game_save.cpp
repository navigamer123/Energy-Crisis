// =============================================================================
// Team b-session (F-18): GameEngine snapshot for disk save/load.
//
// Format (UTF-8 text, one value per line):
//   engine_version=1
//   key=value                ... every scalar of the engine, both players and the city
//   plot=id,owner,x,y,w,h,purchased,cost           (one line per land plot)
//   building=type,x,y,owner,out,anim,stored,cap,radius,broken
//   end=engine
// Floats are written with 9 significant digits, which round-trips every float exactly.
// The loader reads the whole block into a temporary engine, validates it and only then
// replaces *this, so a damaged file can never leave a half-loaded match.
// =============================================================================
#include "../includes/game_main.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <istream>
#include <map>
#include <ostream>
#include <sstream>

namespace {

std::string escapeText(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out += c;
    }
    return out;
}

std::string unescapeText(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char n = s[++i];
            if (n == 'n') out += '\n';
            else if (n == 'r') out += '\r';
            else out += n;
        } else {
            out += s[i];
        }
    }
    return out;
}

std::vector<std::string> splitCsv(const std::string& s) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c : s) {
        if (c == ',') { parts.push_back(cur); cur.clear(); }
        else cur += c;
    }
    parts.push_back(cur);
    return parts;
}

bool toInt(const std::string& s, int& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    long v = std::strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || *end != '\0') return false;
    out = static_cast<int>(v);
    return true;
}

bool toFloat(const std::string& s, float& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    double v = std::strtod(s.c_str(), &end);
    if (end == s.c_str() || *end != '\0' || !std::isfinite(v)) return false;
    out = static_cast<float>(v);
    return true;
}

// Reads typed values out of the key=value map and remembers the first problem
class Reader {
public:
    explicit Reader(const std::map<std::string, std::string>& kv) : kv_(kv) {}
    bool ok() const { return error_.empty(); }
    const std::string& error() const { return error_; }
    void fail(const std::string& msg) { if (error_.empty()) error_ = msg; }

    const std::string* raw(const std::string& key) {
        auto it = kv_.find(key);
        if (it == kv_.end()) { fail("missing key '" + key + "'"); return nullptr; }
        return &it->second;
    }
    void i(const std::string& key, int& out, int lo, int hi) {
        const std::string* v = raw(key);
        int x = 0;
        if (v == nullptr) return;
        if (!toInt(*v, x) || x < lo || x > hi) { fail("bad value for '" + key + "'"); return; }
        out = x;
    }
    void f(const std::string& key, float& out, float lo, float hi) {
        const std::string* v = raw(key);
        float x = 0.0f;
        if (v == nullptr) return;
        if (!toFloat(*v, x) || x < lo || x > hi) { fail("bad value for '" + key + "'"); return; }
        out = x;
    }
    void b(const std::string& key, bool& out) {
        int x = 0;
        i(key, x, 0, 1);
        out = (x != 0);
    }
    void s(const std::string& key, std::string& out) {
        const std::string* v = raw(key);
        if (v != nullptr) out = unescapeText(*v);
    }

private:
    const std::map<std::string, std::string>& kv_;
    std::string error_;
};

const float BIG = 1.0e9f;

void writeEconomy(std::ostream& o, const char* p, const PlayerEconomy& e) {
    o << p << "money=" << e.money << "\n";
    o << p << "gold=" << e.gold << "\n";
    o << p << "silver=" << e.silver << "\n";
    o << p << "iron=" << e.iron << "\n";
    o << p << "coal=" << e.coal << "\n";
    o << p << "copper=" << e.copper << "\n";
    o << p << "silicon=" << e.silicon << "\n";
    o << p << "wood=" << e.wood << "\n";
    o << p << "ore=" << e.ore << "\n";
    o << p << "energyMW=" << e.energyMW << "\n";
    o << p << "landTier=" << e.landTier << "\n";
    o << p << "cityInfluence=" << e.cityInfluence << "\n";
    o << p << "selectedBuilding=" << e.selectedBuilding << "\n";
    o << p << "lastPlacedBuilding=" << e.lastPlacedBuilding << "\n";
    o << p << "mineLevels=";
    for (int k = 0; k < 8; ++k) o << (k ? "," : "") << e.mineLevels[k];
    o << "\n";
    o << p << "data.money=" << e.data.money << "\n";
    o << p << "data.iron=" << e.data.iron << "\n";
    o << p << "data.coal=" << e.data.coal << "\n";
    o << p << "data.gold=" << e.data.gold << "\n";
    o << p << "data.copper=" << e.data.copper << "\n";
    o << p << "data.silver=" << e.data.silver << "\n";
    o << p << "data.silicon=" << e.data.silicon << "\n";
    o << p << "data.wood=" << e.data.wood << "\n";
    o << p << "data.sticks=" << e.data.sticks << "\n";
    o << p << "data.weather=" << escapeText(e.data.weather) << "\n";
    o << p << "data.wind_speed=" << escapeText(e.data.wind_speed) << "\n";
}

void readEconomy(Reader& r, const std::string& p, PlayerEconomy& e) {
    const int MAXI = 2000000000;
    r.i(p + "money", e.money, -MAXI, MAXI);
    r.i(p + "gold", e.gold, -MAXI, MAXI);
    r.i(p + "silver", e.silver, -MAXI, MAXI);
    r.i(p + "iron", e.iron, -MAXI, MAXI);
    r.i(p + "coal", e.coal, -MAXI, MAXI);
    r.i(p + "copper", e.copper, -MAXI, MAXI);
    r.i(p + "silicon", e.silicon, -MAXI, MAXI);
    r.i(p + "wood", e.wood, -MAXI, MAXI);
    r.i(p + "ore", e.ore, -MAXI, MAXI);
    r.i(p + "energyMW", e.energyMW, -MAXI, MAXI);
    r.i(p + "landTier", e.landTier, 0, 1000);
    r.f(p + "cityInfluence", e.cityInfluence, 0.0f, 1.0f);
    r.i(p + "selectedBuilding", e.selectedBuilding, 0, static_cast<int>(BuildingType::DEMOLISH));
    r.i(p + "lastPlacedBuilding", e.lastPlacedBuilding, 0, static_cast<int>(BuildingType::DEMOLISH));
    const std::string* levels = r.raw(p + "mineLevels");
    if (levels != nullptr) {
        std::vector<std::string> parts = splitCsv(*levels);
        if (parts.size() != 8) {
            r.fail("bad value for '" + p + "mineLevels'");
        } else {
            for (int k = 0; k < 8; ++k) {
                int lv = 0;
                if (!toInt(parts[k], lv) || lv < 0 || lv > 100) { r.fail("bad value for '" + p + "mineLevels'"); break; }
                e.mineLevels[k] = lv;
            }
        }
    }
    r.i(p + "data.money", e.data.money, -MAXI, MAXI);
    r.i(p + "data.iron", e.data.iron, -MAXI, MAXI);
    r.i(p + "data.coal", e.data.coal, -MAXI, MAXI);
    r.i(p + "data.gold", e.data.gold, -MAXI, MAXI);
    r.i(p + "data.copper", e.data.copper, -MAXI, MAXI);
    r.i(p + "data.silver", e.data.silver, -MAXI, MAXI);
    r.i(p + "data.silicon", e.data.silicon, -MAXI, MAXI);
    r.i(p + "data.wood", e.data.wood, -MAXI, MAXI);
    r.i(p + "data.sticks", e.data.sticks, -MAXI, MAXI);
    r.s(p + "data.weather", e.data.weather);
    r.s(p + "data.wind_speed", e.data.wind_speed);
}

unsigned int reseedAfterLoad(int day) {
    // Same convention as GameEngine::init: EC_SEED makes runs reproducible
    unsigned int seed = 0;
    if (const char* env = std::getenv("EC_SEED")) {
        seed = static_cast<unsigned int>(std::strtoul(env, nullptr, 10)) + 7919u * static_cast<unsigned int>(day);
    } else {
        unsigned long long t = static_cast<unsigned long long>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
        seed = static_cast<unsigned int>(t ^ (t >> 32));
    }
    seedRandom(seed);
    std::srand(seed);
    return seed;
}

} // namespace

bool GameEngine::saveSnapshot(std::ostream& out) const {
    std::ostringstream o;
    o << std::setprecision(9);
    o << "engine_version=" << SNAPSHOT_VERSION << "\n";
    o << "gameSeconds=" << gameSeconds << "\n";
    o << "currentDay=" << currentDay << "\n";
    o << "hour24=" << hour24 << "\n";
    o << "revenueTimer=" << revenueTimer << "\n";
    o << "p1Weather=" << static_cast<int>(p1Weather) << "\n";
    o << "p2Weather=" << static_cast<int>(p2Weather) << "\n";
    o << "season=" << static_cast<int>(currentSeason) << "\n";
    o << "timeScale=" << timeScale << "\n";
    writeEconomy(o, "p1.", p1);
    writeEconomy(o, "p2.", p2);
    o << "city.demand=" << city.cityEnergyDemand << "\n";
    o << "city.p1Share=" << city.p1CityShare << "\n";
    o << "city.p1Delivered=" << city.p1DailyDelivered << "\n";
    o << "city.p2Delivered=" << city.p2DailyDelivered << "\n";
    o << "city.dailySeconds=" << city.dailySeconds << "\n";
    o << "city.dayCutOccurred=" << (city.dayCutOccurred ? 1 : 0) << "\n";
    o << "city.lastCutMessage=" << escapeText(city.lastCutMessage) << "\n";
    o << "city.winner=" << city.winner << "\n";
    o << "plots=" << landPlots.size() << "\n";
    for (const LandPlot& p : landPlots) {
        o << "plot=" << p.id << "," << p.playerOwner << "," << p.bounds.position.x << "," << p.bounds.position.y << ","
          << p.bounds.size.x << "," << p.bounds.size.y << "," << (p.isPurchased ? 1 : 0) << "," << p.costGold << "\n";
    }
    o << "buildings=" << buildings.size() << "\n";
    for (const PlacedBuilding& b : buildings) {
        o << "building=" << static_cast<int>(b.type) << "," << b.position.x << "," << b.position.y << "," << b.playerOwner << ","
          << b.currentOutputMW << "," << b.animTimer << "," << b.energyStored << "," << b.maxCapacity << ","
          << b.lightRadius << "," << (b.isBroken ? 1 : 0) << "\n";
    }
    o << "end=engine\n";
    out << o.str();
    return static_cast<bool>(out);
}

bool GameEngine::loadSnapshot(std::istream& in, std::string* error) {
    std::map<std::string, std::string> kv;
    std::vector<std::string> plotLines;
    std::vector<std::string> buildingLines;
    bool sawEnd = false;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        if (key == "end" && value == "engine") { sawEnd = true; break; }
        if (key == "plot") plotLines.push_back(value);
        else if (key == "building") buildingLines.push_back(value);
        else kv[key] = value;
    }

    auto failWith = [error](const std::string& msg) {
        if (error != nullptr) *error = msg;
        return false;
    };
    if (!sawEnd) return failWith("snapshot is truncated (no end=engine line)");

    Reader r(kv);
    int version = 0;
    r.i("engine_version", version, 1, 1000000);
    if (r.ok() && version != SNAPSHOT_VERSION) {
        return failWith("unsupported snapshot version " + std::to_string(version));
    }

    GameEngine t; // fresh engine, filled field by field
    int w1 = 0, w2 = 0, season = 0;
    r.f("gameSeconds", t.gameSeconds, 0.0f, BIG);
    r.i("currentDay", t.currentDay, 1, 100000);
    r.f("hour24", t.hour24, 0.0f, 24.0f);
    r.f("revenueTimer", t.revenueTimer, 0.0f, 1000.0f);
    r.i("p1Weather", w1, 0, static_cast<int>(WeatherType::CLOUDY));
    r.i("p2Weather", w2, 0, static_cast<int>(WeatherType::CLOUDY));
    r.i("season", season, 0, static_cast<int>(SeasonType::WINTER));
    r.f("timeScale", t.timeScale, 0.0f, 1000.0f);
    t.p1Weather = static_cast<WeatherType>(w1);
    t.p2Weather = static_cast<WeatherType>(w2);
    t.currentSeason = static_cast<SeasonType>(season);
    if (t.timeScale <= 0.1f) t.timeScale = 1.0f;

    readEconomy(r, "p1.", t.p1);
    readEconomy(r, "p2.", t.p2);

    r.i("city.demand", t.city.cityEnergyDemand, -1000000, 1000000);
    r.f("city.p1Share", t.city.p1CityShare, 0.0f, 1.0f);
    r.f("city.p1Delivered", t.city.p1DailyDelivered, -BIG, BIG);
    r.f("city.p2Delivered", t.city.p2DailyDelivered, -BIG, BIG);
    r.f("city.dailySeconds", t.city.dailySeconds, 0.0f, BIG);
    r.b("city.dayCutOccurred", t.city.dayCutOccurred);
    r.s("city.lastCutMessage", t.city.lastCutMessage);
    r.i("city.winner", t.city.winner, 0, 3);

    int plotCount = -1, buildingCount = -1;
    r.i("plots", plotCount, 0, 1000);
    r.i("buildings", buildingCount, 0, 100000);
    if (!r.ok()) return failWith(r.error());
    if (plotCount != static_cast<int>(plotLines.size())) return failWith("plot count does not match the plot lines");
    if (buildingCount != static_cast<int>(buildingLines.size())) return failWith("building count does not match the building lines");
    if (plotCount == 0) return failWith("snapshot has no land plots");

    t.landPlots.clear();
    for (const std::string& pl : plotLines) {
        std::vector<std::string> f = splitCsv(pl);
        LandPlot p;
        int purchased = 0;
        float x = 0, y = 0, w = 0, h = 0;
        if (f.size() != 8 || !toInt(f[0], p.id) || !toInt(f[1], p.playerOwner) || !toFloat(f[2], x) || !toFloat(f[3], y) ||
            !toFloat(f[4], w) || !toFloat(f[5], h) || !toInt(f[6], purchased) || !toInt(f[7], p.costGold) ||
            (p.playerOwner != 1 && p.playerOwner != 2) || w <= 0.0f || h <= 0.0f) {
            return failWith("bad plot line: " + pl);
        }
        p.bounds = sf::FloatRect({ x, y }, { w, h });
        p.isPurchased = (purchased != 0);
        t.landPlots.push_back(p);
    }

    t.buildings.clear();
    for (const std::string& bl : buildingLines) {
        std::vector<std::string> f = splitCsv(bl);
        PlacedBuilding b;
        int type = 0, broken = 0;
        float x = 0, y = 0;
        if (f.size() != 10 || !toInt(f[0], type) || !toFloat(f[1], x) || !toFloat(f[2], y) || !toInt(f[3], b.playerOwner) ||
            !toFloat(f[4], b.currentOutputMW) || !toFloat(f[5], b.animTimer) || !toFloat(f[6], b.energyStored) ||
            !toFloat(f[7], b.maxCapacity) || !toFloat(f[8], b.lightRadius) || !toInt(f[9], broken) ||
            type < static_cast<int>(BuildingType::SOLAR_PANEL) || type > static_cast<int>(BuildingType::LAMP) ||
            (b.playerOwner != 1 && b.playerOwner != 2)) {
            return failWith("bad building line: " + bl);
        }
        b.type = static_cast<BuildingType>(type);
        b.position = { x, y };
        b.isBroken = (broken != 0);
        t.buildings.push_back(b);
    }

    *this = t;
    unsigned int seed = reseedAfterLoad(currentDay);
    (void)seed;
    if (error != nullptr) error->clear();
    return true;
}
