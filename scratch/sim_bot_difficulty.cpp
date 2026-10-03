// =============================================================================
// ENERGY CRISIS - BOT DIFFICULTY SIMULATION ([AI team]: BAL-01, F-17, НЕВЪЗМОЖНО)
//
// Plays full matches of the REAL bot (UI/scr/UI_bot.cpp with UI/scr/UI_botProfiles.cpp) on the
// REAL resource-station geometry (UI/scr/UI_resourceNodes.cpp) against the real GameEngine,
// following the rules of UI_map::updateControls / executeP*Action: 1 s mining cooldown for the
// human, the 6x clock while the human's cursor is in a resource zone, action debounce, bot
// engine modifiers and lightning. No window is opened (SFML is linked only for the types).
//
// P1 plays a scripted human strategy (driven by the same brain with human limits:
// 360 px/s cursor, 1 s mining cooldown, reaction delay, no engine advantages):
//   passive  - never acts
//   balanced - a sensible mix (wind, hydro, some solar, a battery) built until the "+MW" printed
//              on its building cards is 1.2x the city demand (weather and night not accounted)
//   optimal  - the strongest plan the brain knows (24/7 sources, river first, land rush,
//              worst-case weather planning), executed at human speed
//
// Build: make botsim   (or compile this file + UI_bot.cpp + UI_botProfiles.cpp +
//        UI_resourceNodes.cpp + Game/scr/*.cpp and link sfml-graphics/window/system)
// Usage: sim_bot_difficulty [seeds] [--rivals] [--lightning 0|1] [--match <diff 1-4> <p1 0-2> <seed>]
// Exit code 1 when НЕВЪЗМОЖНО lost or drew a single match, or the difficulties are out of order.
// =============================================================================
#include "game_main.h"
#include "UI_bot.h"
#include "UI_botProfiles.h"
#include "UI_resourceNodes.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
extern "C" __declspec(dllimport) int __cdecl _putenv(const char*);
#endif

namespace {

enum Strategy { PASSIVE = 0, BALANCED = 1, OPTIMAL = 2, STRATEGY_COUNT = 3 };
const char* kStrategyName[STRATEGY_COUNT] = { "passive", "balanced", "optimal" };
const BotDifficulty kDifficulties[4] = { BotDifficulty::EASY, BotDifficulty::MEDIUM, BotDifficulty::HARD,
                                         BotDifficulty::IMPOSSIBLE };
const char* kDifficultyName[4] = { "EASY", "MEDIUM", "HARD", "IMPOSSIBLE" };

constexpr float kFrame = 1.0f / 60.0f;
constexpr float kHumanSpeed = 360.0f;      // UI_map::updateControls cursor speed
constexpr float kHumanActionCd = 0.20f;    // P1 action debounce in updateControls
constexpr float kHumanReaction = 0.25f;    // seconds between two human decisions

void setSeed(unsigned seed) {
    std::string s = "EC_SEED=" + std::to_string(seed);
#ifdef _WIN32
    _putenv(s.c_str());
#else
    setenv("EC_SEED", std::to_string(seed).c_str(), 1);
#endif
}

BotProfile humanProfile(Strategy s) {
    BotProfile p;
    if (s == OPTIMAL) {
        p = makeBotProfile(BotDifficulty::IMPOSSIBLE, 0); // the best plan the brain knows...
        p.engineEdge = PlayerModifiers();                  // ...without any engine advantage
        p.nameBg = "Оптимален човек";
        p.floorMW = 120.0f;
        p.demandMargin = 2.0f;
    } else {
        p.nameBg = "Балансиран човек";
        p.demandMargin = 1.2f;                  // 20% above what the city asks...
        p.solarScore = 100.0f;                // likes cheap panels, like most new players
        p.batteryCap = 1;
        p.floorMW = 30.0f;
        p.supplyView = BotSupplyView::NAMEPLATE; // ...judged on the "+MW" printed on the building cards
        p.lookaheadHours = 3.0f;
        p.maxMineLevel = 3;
        p.upgradeChancePct = 50;
        if (const char* v = std::getenv("SIM_BAL")) { // tuning aid: "margin,view"
            int view = 1;
            std::sscanf(v, "%f,%d", &p.demandMargin, &view);
            p.supplyView = static_cast<BotSupplyView>(view);
        }
    }
    p.moveSpeed = kHumanSpeed;
    p.decisionInterval = kHumanReaction;
    p.afterMineThink = kHumanReaction;
    p.mineHitInterval = Balance::MINE_COOLDOWN_SEC;
    p.actionCooldown = kHumanActionCd;
    p.slipChance = 0.0f;
    return p;
}

// Tuning aid: SIM_SKILL_1..3="margin,floor,view(0 nameplate,1 instant,2 projected),lookahead,slip" overrides EASY..HARD
void applyTuningOverride(UIBot& bot, BotDifficulty diff) {
    std::string key = "SIM_SKILL_" + std::to_string(static_cast<int>(diff));
    const char* v = std::getenv(key.c_str());
    if (!v) return;
    BotProfile p = bot.getProfile();
    float m, f, look, slip;
    int view;
    if (std::sscanf(v, "%f,%f,%d,%f,%f", &m, &f, &view, &look, &slip) != 5) return;
    p.demandMargin = m;
    p.floorMW = f;
    p.supplyView = static_cast<BotSupplyView>(view); // 0 nameplate, 1 instant, 2 projected, 3 worst case
    p.lookaheadHours = look;
    p.slipChance = slip;
    int pid = bot.getPersonality();
    bot.initWithProfile(diff, p);
    bot.setPersonality(pid);
}

struct Side {
    UIBot brain;
    bool active = false;
    sf::Vector2f pos;
    float actionCd = 0.0f;
    float resCd = 0.0f;
    int builds = 0, mines = 0, upgrades = 0, plots = 0;
};

// Pads a UTF-8 label to `width` visible characters (printf pads bytes, Cyrillic takes two)
std::string padUtf8(const std::string& s, size_t width) {
    size_t chars = 0;
    for (unsigned char ch : s) if ((ch & 0xC0) != 0x80) chars++;
    return s + std::string(chars < width ? width - chars : 0, ' ');
}

ResourceType resourceAt(const UI_resourceNodes& n, int p, sf::Vector2f pos) {
    return p == 1 ? n.getP1ResourceAt(pos) : n.getP2ResourceAt(pos);
}

// Mirror of UI_map::executeP1Action / executeP2Action without the visual feedback
void executeAction(GameEngine& e, const UI_resourceNodes& n, int p, Side& s) {
    std::string msg;
    BuildingType sel = e.getSelectedBuilding(p);
    if (sel != BuildingType::NONE) {
        if (sel != BuildingType::DEMOLISH) {
            bool onPlot = false;
            for (const auto& plot : e.getLandPlots()) {
                if (plot.playerOwner == p && plot.bounds.contains(s.pos)) {
                    onPlot = true;
                    if (!plot.isPurchased) {
                        if (e.buyLandPlot(p, plot.id, msg)) s.plots++;
                        return;
                    }
                    break;
                }
            }
            if (!onPlot) return;
        }
        sf::Vector2f target = (sel == BuildingType::DEMOLISH) ? s.pos : e.snapToBuildingGrid(p, s.pos);
        if (e.placeBuilding(p, sel, target, msg)) {
            s.builds++;
            if (sel != BuildingType::DEMOLISH) e.clearBuildingSelection(p);
        }
        return;
    }
    ResourceType res = resourceAt(n, p, s.pos);
    if (res != ResourceType::NONE) {
        if (s.resCd > 0.0f) return;
        GameEngine::MineResult mr;
        if (e.mineResource(p, res, mr, msg)) {
            s.resCd = Balance::MINE_COOLDOWN_SEC * e.getPlayerModifiers(p).cooldownMult;
            s.mines++;
        }
        return;
    }
    for (const auto& plot : e.getLandPlots()) {
        if (plot.playerOwner == p && plot.bounds.contains(s.pos)) {
            if (!plot.isPurchased && e.buyLandPlot(p, plot.id, msg)) s.plots++;
            break;
        }
    }
}

void executeUpgrade(GameEngine& e, const UI_resourceNodes& n, int p, Side& s) {
    ResourceType res = (p == 1) ? n.getP1StationAt(s.pos) : n.getP2StationAt(s.pos);
    if (res == ResourceType::NONE || res == ResourceType::MONEY) return;
    std::string msg;
    if (e.upgradeMine(p, res, msg)) s.upgrades++;
}

struct MatchResult {
    int winner = 0;      // 0 none (time limit), 1 P1, 2 bot, 3 draw
    int endDay = 0;
    float botShare = 0.5f;
    int p1Met = 0, botMet = 0, settled = 0;
    int botBuildings = 0, p1Buildings = 0;
    int lightningKills = 0;
};

MatchResult playMatch(const UI_resourceNodes& nodes, BotDifficulty diff, int rival, Strategy strat, unsigned seed,
                      bool lightning, bool verbose) {
    setSeed(seed);
    GameEngine e;
    e.init(1600.0f, 900.0f);

    Side side[3];
    side[1].pos = { 450.0f, 450.0f };
    side[2].pos = { 1150.0f, 450.0f };
    // P2: the bot exactly as UI_map::setBotDifficulty sets it up
    side[2].active = true;
    side[2].brain.setPersonality(rival);
    side[2].brain.init(diff);
    applyTuningOverride(side[2].brain, diff);
    e.setPlayerModifiers(2, side[2].brain.getEngineModifiers());
    // P1: scripted human
    if (strat != PASSIVE) {
        side[1].active = true;
        side[1].brain.setPlayerId(1);
        side[1].brain.initWithProfile(BotDifficulty::MEDIUM, humanProfile(strat));
    }

    MatchResult r;
    float lightCd[3] = { 0.0f, 5.0f, 5.0f };
    int lastDay = e.getCurrentDay();
    const long maxFrames = static_cast<long>((Balance::FINAL_DAY + 1) * Balance::SECONDS_PER_DAY / kFrame) + 1000;
    for (long frame = 0; frame < maxFrames && e.getCityState().winner == 0; ++frame) {
        // UI_map::render: the engine advances with the time scale of the previous frame
        e.update(kFrame);
        if (e.getCityState().winner != 0) break;

        // updateControls 2: 6x clock only while the human (P1) mines
        bool p1InRes = side[1].active && resourceAt(nodes, 1, side[1].pos) != ResourceType::NONE;
        e.setTimeScale(p1InRes ? Balance::MINE_SPEEDUP_MULT : 1.0f);

        // updateControls 3: the human moves (decisions come from the scripted brain)
        bool p1Act = false, p1Upg = false;
        if (side[1].active) {
            BuildingType sel = BuildingType::NONE;
            side[1].brain.update(kFrame, e, nodes, side[1].pos, p1Act, p1Upg, sel);
            e.getPlayerEconomyMut(1).selectedBuilding = static_cast<int>(sel);
        }

        // updateControls 4: the bot
        {
            bool act = false, upg = false;
            BuildingType sel = BuildingType::NONE;
            side[2].brain.update(kFrame, e, nodes, side[2].pos, act, upg, sel);
            e.getPlayerEconomyMut(2).selectedBuilding = static_cast<int>(sel);
            if (act && side[2].actionCd <= 0.0f) {
                executeAction(e, nodes, 2, side[2]);
                side[2].actionCd = side[2].brain.getActionCooldown();
            }
            if (upg) executeUpgrade(e, nodes, 2, side[2]);
        }

        // updateControls 5: cooldowns
        for (int p = 1; p <= 2; ++p) {
            if (side[p].actionCd > 0.0f) side[p].actionCd -= kFrame;
            if (side[p].resCd > 0.0f) side[p].resCd -= kFrame;
        }

        // updateControls 6: the human's key presses
        if (p1Act && side[1].actionCd <= 0.0f) {
            executeAction(e, nodes, 1, side[1]);
            side[1].actionCd = kHumanActionCd;
        }
        if (p1Upg) executeUpgrade(e, nodes, 1, side[1]);

        // UI_map::updateWeatherParticles: stormy sectors, game-time schedule, 1/12 hit chance
        if (lightning) {
            float gameDt = kFrame * e.getTimeScale();
            for (int sector = 1; sector <= 2; ++sector) {
                if (e.getPlayerWeather(sector) != WeatherType::STORMY) continue;
                lightCd[sector] -= gameDt;
                if (lightCd[sector] > 0.0f) continue;
                lightCd[sector] = 6.0f + static_cast<float>(std::rand() % 6);
                std::vector<sf::Vector2f> targets;
                for (const auto& b : e.getBuildings())
                    if (b.playerOwner == sector) targets.push_back(b.position);
                if (!targets.empty() && !e.isGracePeriod() && std::rand() % 12 == 0) {
                    e.breakBuildingAt(targets[static_cast<size_t>(std::rand()) % targets.size()]);
                    r.lightningKills++;
                }
            }
        }

        if (e.getCurrentDay() != lastDay) {
            const auto& msg = e.getCityState().lastCutMessage;
            if (lastDay > Balance::GRACE_PERIOD_DAYS) {
                r.settled++;
                bool both = msg.find("И ДВАМАТА") != std::string::npos;
                if (both || msg.find("ИГРАЧ 1 ЗАХРАНИ") != std::string::npos) r.p1Met++;
                if (both || msg.find("ИГРАЧ 2 ЗАХРАНИ") != std::string::npos) r.botMet++;
            }
            if (verbose) {
                int nb[3] = { 0, 0, 0 };
                for (const auto& b : e.getBuildings()) nb[b.playerOwner]++;
                std::printf("  day %2d | demand next %3d | P1 %4d MW %2d bld %5d g | BOT %4d MW %3d bld %6d g | bot share %3.0f%% | %s\n",
                            lastDay, e.getCityState().cityEnergyDemand, e.getPlayerEconomy(1).energyMW, nb[1],
                            e.getPlayerEconomy(1).gold, e.getPlayerEconomy(2).energyMW, nb[2], e.getPlayerEconomy(2).gold,
                            100.0f * (1.0f - e.getCityState().p1CityShare), side[2].brain.getIntentText().c_str());
            }
            lastDay = e.getCurrentDay();
        }
    }

    const auto& c = e.getCityState();
    r.winner = c.winner;
    r.endDay = e.getCurrentDay() - 1;
    r.botShare = 1.0f - c.p1CityShare;
    for (const auto& b : e.getBuildings()) (b.playerOwner == 1 ? r.p1Buildings : r.botBuildings)++;
    if (verbose) std::printf("  -> %s\n", c.lastCutMessage.c_str());
    return r;
}

struct Cell {
    int p1 = 0, bot = 0, draw = 0, none = 0, n = 0;
    double days = 0, share = 0;
    int p1Met = 0, botMet = 0, settled = 0;
    void add(const MatchResult& r) {
        n++;
        if (r.winner == 1) p1++;
        else if (r.winner == 2) bot++;
        else if (r.winner == 3) draw++;
        else none++;
        days += r.endDay;
        share += r.botShare;
        p1Met += r.p1Met;
        botMet += r.botMet;
        settled += r.settled;
    }
    double pct(int v) const { return n ? 100.0 * v / n : 0.0; }
};

} // namespace

int main(int argc, char** argv) {
    std::cout.setstate(std::ios::failbit); // silence engine and bot logs; results use printf
    int seeds = 6;
    bool rivals = false;
    bool lightning = true;
    bool listMatches = false;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--rivals")) rivals = true;
        else if (!std::strcmp(argv[i], "--list")) listMatches = true;
        else if (!std::strcmp(argv[i], "--lightning") && i + 1 < argc) lightning = std::atoi(argv[++i]) != 0;
        else if (!std::strcmp(argv[i], "--match") && i + 3 < argc) {
            int d = std::atoi(argv[i + 1]), s = std::atoi(argv[i + 2]);
            unsigned seed = static_cast<unsigned>(std::atoi(argv[i + 3]));
            int rival = (i + 4 < argc) ? std::atoi(argv[i + 4]) : 0;
            UI_resourceNodes nodes;
            MatchResult r = playMatch(nodes, kDifficulties[(d - 1) & 3], rival, static_cast<Strategy>(s % 3), seed, lightning, true);
            std::printf("winner %d day %d bot share %.0f%%\n", r.winner, r.endDay, 100.0f * r.botShare);
            return 0;
        } else seeds = std::max(1, std::atoi(argv[i]));
    }

    UI_resourceNodes nodes;
    std::printf("ENERGY CRISIS bot simulation: %d seeds per cell, lightning %s\n", seeds, lightning ? "on" : "off");
    std::printf("P1 = scripted human strategy, BOT = P2. Rivals rotate over the seeds for EASY/MEDIUM/HARD.\n");
    std::printf("Cell: P1 win / draw / BOT win, then the share of settled days P1 / BOT met the demand.\n\n");
    std::printf("%-11s| %-34s| %-34s| %-34s\n", "difficulty", "vs passive", "vs balanced", "vs optimal");
    std::printf("-----------+-----------------------------------+-----------------------------------+-----------------------------------\n");

    Cell cells[4][STRATEGY_COUNT];
    for (int d = 0; d < 4; ++d) {
        std::printf("%-11s|", kDifficultyName[d]);
        for (int s = 0; s < STRATEGY_COUNT; ++s) {
            for (int k = 0; k < seeds; ++k) {
                unsigned seed = 1000u + 97u * static_cast<unsigned>(k) + 13u * static_cast<unsigned>(s);
                MatchResult r = playMatch(nodes, kDifficulties[d], k % BOT_PERSONALITY_COUNT, static_cast<Strategy>(s), seed,
                                          lightning, false);
                cells[d][s].add(r);
                if (listMatches)
                    std::fprintf(stderr, "  %-10s vs %-8s seed %u rival %d: winner %d day %2d met P1 %d BOT %d of %d, bot %d bld\n",
                                 kDifficultyName[d], kStrategyName[s], seed, k % BOT_PERSONALITY_COUNT, r.winner, r.endDay, r.p1Met,
                                 r.botMet, r.settled, r.botBuildings);
            }
            const Cell& c = cells[d][s];
            char buf[64];
            std::snprintf(buf, sizeof(buf), "P1 %3.0f%% =%3.0f%% BOT %3.0f%% met %2.0f/%2.0f%%", c.pct(c.p1), c.pct(c.draw + c.none),
                          c.pct(c.bot), c.settled ? 100.0 * c.p1Met / c.settled : 0.0, c.settled ? 100.0 * c.botMet / c.settled : 0.0);
            std::printf(" %-34s|", buf);
            std::fflush(stdout);
        }
        std::printf("\n");
    }

    if (rivals) {
        std::printf("\nRivals vs the balanced human (%d seeds each): P1 win %% / draw %% / bot win %%\n", seeds);
        std::printf("%-18s| %-24s| %-24s| %-24s\n", "rival", "EASY", "MEDIUM", "HARD");
        for (int rv = 0; rv < BOT_PERSONALITY_COUNT; ++rv) {
            std::printf("%s|", padUtf8(botPersonality(rv).nameBg, 18).c_str());
            for (int d = 0; d < 3; ++d) {
                Cell c;
                for (int k = 0; k < seeds; ++k)
                    c.add(playMatch(nodes, kDifficulties[d], rv, BALANCED, 5000u + 31u * static_cast<unsigned>(k), lightning, false));
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%3.0f / %3.0f / %3.0f", c.pct(c.p1), c.pct(c.draw + c.none), c.pct(c.bot));
                std::printf(" %-24s|", buf);
                std::fflush(stdout);
            }
            std::printf("\n");
        }
    }

    // ---- Verdict --------------------------------------------------------------
    bool ok = true;
    for (int s = 0; s < STRATEGY_COUNT; ++s) {
        if (cells[3][s].bot != cells[3][s].n) {
            std::printf("FAIL: IMPOSSIBLE won only %d of %d matches vs %s\n", cells[3][s].bot, cells[3][s].n, kStrategyName[s]);
            ok = false;
        }
    }
    // Human wins must not grow with the difficulty, and Easy must be beatable
    for (int s = BALANCED; s < STRATEGY_COUNT; ++s) {
        for (int d = 0; d < 2; ++d) {
            if (cells[d][s].p1 < cells[d + 1][s].p1) {
                std::printf("FAIL: P1 (%s) wins more often vs %s than vs %s\n", kStrategyName[s], kDifficultyName[d + 1],
                            kDifficultyName[d]);
                ok = false;
            }
        }
    }
    if (cells[0][BALANCED].p1 == 0 && cells[0][OPTIMAL].p1 == 0) {
        std::printf("FAIL: EASY was never beaten\n");
        ok = false;
    }
    std::printf("\n%s\n", ok ? "VERDICT: PASS (IMPOSSIBLE won every match, difficulties ordered)" : "VERDICT: FAIL");
    return ok ? 0 : 1;
}
