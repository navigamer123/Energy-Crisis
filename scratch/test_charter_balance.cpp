// =============================================================================
// ENERGY CRISIS - F-35 CHARTER BALANCE (headless bot-vs-bot)              [team b-options]
// A scripted agent (identical logic for both seats) plays full STANDARD matches for every
// pair of charters, both seat orders, over several weather seeds. The agent adapts to its
// own perks: it values generators by average output after perks per mining action (recipe
// after perks / its current mine yields), keeps the river bank and buys river plots when hydro
// is its best generator, and spends money in the lab.
// Scoring: competent agents both out-build the demand after the first week, after which the
// city share stops moving and STANDARD matches end in a draw. So each match runs the first
// kDays days (where demand still binds); the winner is the territory winner if there is one,
// otherwise the side that delivered more energy to the city (the stronger economy).
// Mining pace: one action per 6 game-seconds, because mining speeds the clock up 6x.
// Goal from the backlog: no charter above a 55% win rate against the field. Weather luck
// dominates single matches, so a second check plays every charter against no charter under
// Mirror Weather and requires its delivered energy to stay within 10% ("econ index").
// Headless: this file + Game/scr/*.cpp ("make test"). Exits 1 when the goal is missed.
// =============================================================================
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include "../Game/includes/game_main.h"

namespace {

constexpr float kTick = 1.0f;               // game seconds per agent decision
constexpr int kDays = 10;                   // the contested part of a match (see the header)
float kMiningActionFactor = 6.0f; // one mining action per ~6 s: a contested human-like pace (tunable: EC_BAL_MINE)
int kSeeds = 4; // tunable: EC_BAL_SEEDS
constexpr float kGoal = 0.55f;
constexpr double kIndexTolerance = 0.10; // delivered MW within +-10% of NONE under identical weather

// Left-aligns a UTF-8 name in `width` columns (printf counts bytes, Cyrillic letters take two)
std::string padName(const char* s, int width) {
    std::string out(s);
    int chars = 0;
    for (const char* p = s; *p; ++p) if ((static_cast<unsigned char>(*p) & 0xC0) != 0x80) ++chars;
    if (chars < width) out.append(static_cast<size_t>(width - chars), ' ');
    return out;
}

struct Need { int wood, iron, copper, coal, silicon, silver; };

Need costOf(const BuildingCost& c) { return { c.woodCost, c.ironCost, c.copperCost, c.coalCost, c.siliconCost, c.silverCost }; }

struct Agent {
    int player = 1;
    float mineCooldown = 0.0f;
    bool occupied[12][9] = {};   // grid cells taken by any building (rebuilt every tick)
    bool owned[12][9] = {};      // grid cells on purchased land

    // Expected average output factor of a generator over a day (rough season/weather mean)
    static float avgFactor(BuildingType t) {
        switch (t) {
            case BuildingType::SOLAR_PANEL:  return 0.38f;
            case BuildingType::WIND_TURBINE: return 1.05f;
            case BuildingType::HYDRO_PLANT:  return 0.95f;
            default:                         return 0.0f;
        }
    }

    bool onPurchasedLand(const GameEngine& e, sf::Vector2f pos) const {
        for (const auto& plot : e.getLandPlots()) {
            if (plot.playerOwner == player && plot.bounds.contains(pos)) return plot.isPurchased;
        }
        return false;
    }

    void refreshGrid(const GameEngine& e) {
        for (int r = 0; r < 12; ++r)
            for (int c = 0; c < 9; ++c) {
                occupied[r][c] = false;
                owned[r][c] = onPurchasedLand(e, e.getGridSlot(player, c, r));
            }
        for (const auto& b : e.getBuildings()) {
            if (b.playerOwner != player) continue;
            int c = 0, r = 0;
            e.getClosestGridIndex(player, b.position, c, r);
            occupied[r][c] = true;
        }
    }

    // Expected MW per mining action of a generator for this player: average output after perks
    // divided by the mining actions its (perk-adjusted) recipe takes at the current mine levels
    float valuePerAction(const GameEngine& e, BuildingType t) const {
        PlayerPerks pk = e.getPlayerPerks(player);
        BuildingCost c = e.getBuildingCostFor(player, t);
        const std::pair<int, ResourceType> recipe[] = {
            { c.woodCost, ResourceType::WOOD }, { c.ironCost, ResourceType::IRON }, { c.copperCost, ResourceType::COPPER },
            { c.coalCost, ResourceType::COAL }, { c.siliconCost, ResourceType::SILICON }, { c.silverCost, ResourceType::SILVER } };
        float actions = 0.0f;
        for (const auto& r : recipe) {
            if (r.first > 0) actions += static_cast<float>(r.first) / static_cast<float>(std::max(1, e.getMineYield(player, r.second)));
        }
        float mult = (t == BuildingType::SOLAR_PANEL) ? pk.solarOutputMult
                   : (t == BuildingType::WIND_TURBINE ? pk.windOutputMult : pk.hydroOutputMult);
        return c.basePowerMW * avgFactor(t) * mult / std::max(0.1f, actions);
    }

    // Hydro beats every generator that fits elsewhere: keep the river bank for hydro
    bool prefersHydro(const GameEngine& e) const {
        float hydro = valuePerAction(e, BuildingType::HYDRO_PLANT);
        return hydro > valuePerAction(e, BuildingType::WIND_TURBINE) && hydro > valuePerAction(e, BuildingType::SOLAR_PANEL);
    }

    // Id of the cheapest river-bank plot still for sale (0 = none left)
    int cheapestRiverPlot(const GameEngine& e) const {
        const int riverCol = (player == 1) ? Balance::P1_RIVER_BANK_PLOT_COL : Balance::P2_RIVER_BANK_PLOT_COL;
        int best = 0, bestCost = 1 << 30;
        for (const auto& plot : e.getLandPlots()) {
            if (plot.playerOwner != player || plot.isPurchased) continue;
            sf::Vector2f centre(plot.bounds.position.x + plot.bounds.size.x * 0.5f, plot.bounds.position.y + plot.bounds.size.y * 0.5f);
            int c = 0, r = 0;
            e.getClosestGridIndex(player, centre, c, r);
            if (c / 3 == riverCol && plot.costGold < bestCost) { bestCost = plot.costGold; best = plot.id; }
        }
        return best;
    }

    bool freeSlot(const GameEngine& e, BuildingType t, sf::Vector2f& out) const {
        // River bank = the plot column next to the city (same rule as GameEngine::isRiverBankSlot).
        // Hydro may only use river cells; everything else fills the other cells first and keeps
        // the river bank free for hydro as long as possible.
        const int riverCol = (player == 1) ? Balance::P1_RIVER_BANK_PLOT_COL : Balance::P2_RIVER_BANK_PLOT_COL;
        for (int pass = 0; pass < 2; ++pass) {
            bool wantRiver = (t == BuildingType::HYDRO_PLANT) || pass == 1;
            if (t == BuildingType::HYDRO_PLANT && pass == 1) break;
            // When hydro is the best generator, the whole river bank stays free for it (buys more land instead)
            if (pass == 1 && prefersHydro(e)) break;
            for (int r = 0; r < 12; ++r) {
                for (int c = 0; c < 9; ++c) {
                    if (!owned[r][c] || occupied[r][c]) continue;
                    if (((c / 3) == riverCol) != wantRiver) continue;
                    out = e.getGridSlot(player, c, r);
                    return true;
                }
            }
        }
        return false;
    }

    BuildingType chooseTarget(const GameEngine& e) const {
        int gens = 0, batteries = 0;
        for (const auto& b : e.getBuildings()) {
            if (b.playerOwner != player) continue;
            if (b.type == BuildingType::BATTERY) ++batteries;
            else if (b.type != BuildingType::LAMP) ++gens;
        }
        if (gens >= 4 && batteries * 4 < gens) return BuildingType::BATTERY;
        BuildingType best = BuildingType::SOLAR_PANEL;
        float bestValue = -1.0f;
        sf::Vector2f dummy;
        for (BuildingType t : { BuildingType::SOLAR_PANEL, BuildingType::WIND_TURBINE, BuildingType::HYDRO_PLANT }) {
            if (t == BuildingType::HYDRO_PLANT && !freeSlot(e, t, dummy)) continue;
            float value = valuePerAction(e, t);
            if (value > bestValue) { bestValue = value; best = t; }
        }
        return best;
    }

    ResourceType mostNeeded(const GameEngine& e, const Need& need) const {
        const PlayerEconomy& p = e.getPlayerEconomy(player);
        struct Opt { ResourceType t; int have; int want; };
        Opt opts[] = { { ResourceType::WOOD, p.wood, need.wood }, { ResourceType::IRON, p.iron, need.iron },
                       { ResourceType::COPPER, p.copper, need.copper }, { ResourceType::COAL, p.coal, need.coal },
                       { ResourceType::SILICON, p.silicon, need.silicon }, { ResourceType::SILVER, p.silver, need.silver } };
        ResourceType best = ResourceType::NONE;
        float worst = 0.0f;
        for (const auto& o : opts) {
            if (o.have >= o.want) continue;
            float actions = static_cast<float>(o.want - o.have) / std::max(1, e.getMineYield(player, o.t));
            if (actions > worst) { worst = actions; best = o.t; }
        }
        return best;
    }

    void tick(GameEngine& e) {
        std::string msg;
        e.autoResearch(player, msg);
        refreshGrid(e);

        const PlayerEconomy& p = e.getPlayerEconomy(player);
        BuildingType target = chooseTarget(e);
        sf::Vector2f slot;
        bool hasSlot = freeSlot(e, target, slot);

        // Land: when hydro is the best generator but no river cell is free, the next plot is the
        // cheapest river-bank plot (bought as soon as the gold is there); otherwise buy the next
        // plot when the owned land is full
        sf::Vector2f riverSlot;
        if (prefersHydro(e) && !freeSlot(e, BuildingType::HYDRO_PLANT, riverSlot)) {
            int riverPlot = cheapestRiverPlot(e);
            if (riverPlot > 0 && e.buyLandPlot(player, riverPlot, msg)) {
                refreshGrid(e);
                target = chooseTarget(e);
                hasSlot = freeSlot(e, target, slot);
            }
        }
        if (!hasSlot) {
            if (e.buyNextLandTier(player, msg)) { refreshGrid(e); hasSlot = freeSlot(e, target, slot); }
        }
        Need need = costOf(e.getBuildingCostFor(player, target));
        bool affordable = p.wood >= need.wood && p.iron >= need.iron && p.copper >= need.copper && p.coal >= need.coal &&
                          p.silicon >= need.silicon && p.silver >= need.silver;
        if (hasSlot && affordable && e.isDaylight()) {
            if (!e.placeBuilding(player, target, slot, msg) && std::getenv("EC_BAL_DEBUG"))
                std::fprintf(stderr, "P%d place %d failed: %s\n", player, static_cast<int>(target), msg.c_str());
        }

        // Spare gold: upgrade the mine of the scarcest resource once land is not waiting for it
        int nextLand = 1 << 30;
        for (const auto& plot : e.getLandPlots())
            if (plot.playerOwner == player && !plot.isPurchased) nextLand = std::min(nextLand, plot.costGold);
        ResourceType scarce = mostNeeded(e, need);
        if (scarce != ResourceType::NONE) {
            int up = e.getMineUpgradeCost(player, scarce);
            if (up > 0 && p.gold >= up + (hasSlot ? 0 : nextLand)) e.upgradeMine(player, scarce, msg);
        }

        // Mining: the scarcest resource of the target, else gold
        mineCooldown -= kTick;
        if (mineCooldown <= 0.0f) {
            ResourceType res = (scarce != ResourceType::NONE && hasSlot) ? scarce : ResourceType::GOLD;
            e.mineResource(player, res, msg);
            mineCooldown = e.getMiningCooldown(player) * kMiningActionFactor;
        }
    }
};

// 1 = P1 wins, 2 = P2 wins, 3 = draw (equal territory and equal delivered energy)
int playMatch(CharterType c1, CharterType c2, unsigned int seed, bool mirrorWeather, float& avgMW1, float& avgMW2) {
    MatchRules r = MatchRules::fromPreset(MatchPreset::STANDARD);
    r.charter[0] = c1;
    r.charter[1] = c2;
    r.seed = seed;
    if (mirrorWeather) r.mutators = MUT_MIRROR_WEATHER;
    GameEngine e;
    e.setMatchRules(r);
    e.init(1600.0f, 900.0f);
    Agent a1; a1.player = 1;
    Agent a2; a2.player = 2; a2.mineCooldown = 0.05f;
    double mw1 = 0.0, mw2 = 0.0;
    long samples = 0;
    const float maxSeconds = kDays * Balance::SECONDS_PER_DAY - Balance::gameSecondsAtHour(Balance::MATCH_START_HOUR) + 1.0f;
    for (float t = 0.0f; t < maxSeconds && e.getCityState().winner == 0; t += kTick) {
        a1.tick(e);
        a2.tick(e);
        e.update(kTick);
        mw1 += e.getPlayerEconomy(1).energyMW;
        mw2 += e.getPlayerEconomy(2).energyMW;
        ++samples;
    }
    avgMW1 = static_cast<float>(mw1 / std::max(1L, samples));
    avgMW2 = static_cast<float>(mw2 / std::max(1L, samples));
    int w = e.getCityState().winner;
    if (w == 1 || w == 2) return w;
    float share = e.getCityState().p1CityShare;
    if (share > 0.5f + 1e-4f) return 1;
    if (share < 0.5f - 1e-4f) return 2;
    if (mw1 > mw2 * 1.0001) return 1;
    if (mw2 > mw1 * 1.0001) return 2;
    return 3;
}

} // namespace

int main() {
    if (const char* v = std::getenv("EC_BAL_MINE")) kMiningActionFactor = static_cast<float>(std::atof(v));
    if (const char* v = std::getenv("EC_BAL_SEEDS")) kSeeds = std::max(1, std::atoi(v));
    std::printf("=== F-35 charter balance: scripted bot vs bot, STANDARD rules, first %d days [b-options] ===\n", kDays);
    std::cout.setstate(std::ios::failbit); // silence the engine's per-match log lines
    const int n = static_cast<int>(CharterType::COUNT);

    // 1) Win rate in realistic matches: every ordered pair (so both seats), independent weather;
    //    (a, b) and (b, a) share the seeds, so each side gets both weather streams
    std::vector<double> score(n, 0.0), games(n, 0.0), mwSum(n, 0.0);
    int decided = 0, total = 0;
    for (int a = 0; a < n; ++a) {
        for (int b = 0; b < n; ++b) {
            if (a == b) continue;
            for (int s = 1; s <= kSeeds; ++s) {
                float m1 = 0.0f, m2 = 0.0f;
                int w = playMatch(static_cast<CharterType>(a), static_cast<CharterType>(b), 1000u + static_cast<unsigned>(s * 7919), false, m1, m2);
                ++total;
                if (w != 3) ++decided;
                score[a] += (w == 1) ? 1.0 : (w == 3 ? 0.5 : 0.0);
                score[b] += (w == 2) ? 1.0 : (w == 3 ? 0.5 : 0.0);
                games[a] += 1.0;
                games[b] += 1.0;
                mwSum[a] += m1;
                mwSum[b] += m2;
            }
        }
    }

    // 2) Economy index without weather luck: each charter against NONE with Mirror Weather
    //    (identical weather in both sectors), both seats; delivered MW relative to NONE
    std::vector<double> index(n, 1.0);
    const int indexSeeds = 2;
    for (int c = 1; c < n; ++c) {
        double mine = 0.0, base = 0.0;
        for (int s = 1; s <= indexSeeds; ++s) {
            float m1, m2;
            unsigned seed = 5000u + static_cast<unsigned>(s * 104729);
            playMatch(static_cast<CharterType>(c), CharterType::NONE, seed, true, m1, m2);
            mine += m1; base += m2;
            playMatch(CharterType::NONE, static_cast<CharterType>(c), seed, true, m1, m2);
            mine += m2; base += m1;
        }
        index[c] = mine / std::max(1.0, base);
    }
    std::cout.clear();

    bool ok = true;
    std::printf("\n%s %6s %7s %9s %12s\n", padName("charter", 14).c_str(), "games", "win %", "avg MW", "econ index");
    for (int c = 0; c < n; ++c) {
        double rate = score[c] / std::max(1.0, games[c]);
        bool rateBad = rate > kGoal;
        bool indexBad = index[c] < 1.0 - kIndexTolerance || index[c] > 1.0 + kIndexTolerance;
        std::printf("%s %6.0f %6.1f%% %9.1f %11.3f%s%s\n", padName(MatchInfo::charterShortName(static_cast<CharterType>(c)), 14).c_str(), games[c],
                    rate * 100.0, mwSum[c] / std::max(1.0, games[c]), index[c],
                    rateBad ? "  <-- win rate above 55%" : "", indexBad ? "  <-- economy off by more than 10%" : "");
        if (rateBad || indexBad) ok = false;
    }
    std::printf("\nmatches: %d (+%d index matches), decided: %d (%.0f%%)\n", total, (n - 1) * indexSeeds * 2, decided,
                100.0 * decided / std::max(1, total));
    if (decided * 4 < total) {
        std::printf("FAIL: fewer than 25%% of the matches were decided; the agent is too weak to measure balance\n");
        ok = false;
    }
    std::printf("RESULT: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
