// =============================================================================
// Team b-power (F-34): hazard system — repairable damage from hail, floods,
// wildfires and quakes. Lightning keeps deleting buildings (team decision).
//  - Weather streaks are tracked at every day end (rain / dry days per player).
//  - At the start of each day hazards are scheduled for a random time of that
//    day; players get a warning (except for quakes) so they can stock repairs.
//  - A broken building produces 0 MW until repaired (5 wood + 5 iron, the
//    existing GameEngine::repairBuilding). Reactors SCRAM on quakes instead.
//  - Every strike is reported to the UI through the PowerFx queue (HazardFx).
// Uses its own RNG (PowerWorldState::rng) so the weather sequence is unchanged.
// =============================================================================
#include "../includes/game_main.h"
#include <algorithm>
#include <cmath>
#include <functional>

namespace {

bool isWet(WeatherType w) { return w == WeatherType::RAINY || w == WeatherType::STORMY; }
bool isDry(WeatherType w) { return w == WeatherType::SUNNY || w == WeatherType::CLOUDY || w == WeatherType::WINDY; }

sf::Vector2f centreOf(const sf::FloatRect& r) {
    return sf::Vector2f(r.position.x + r.size.x * 0.5f, r.position.y + r.size.y * 0.5f);
}

} // namespace

const char* getHazardNameBg(HazardKind k) {
    switch (k) {
        case HazardKind::HAIL:     return "ГРАДУШКА";
        case HazardKind::FLOOD:    return "НАВОДНЕНИЕ";
        case HazardKind::WILDFIRE: return "ГОРСКИ ПОЖАР";
        case HazardKind::QUAKE:    return "ЗЕМЕТРЕСЕНИЕ";
        case HazardKind::NONE:     break;
    }
    return "";
}

bool GameEngine::damageBuilding(PlacedBuilding& b, HazardKind kind) {
    if (b.isBroken || isPlotWideBuilding(b.type)) return false; // reactors / mega-projects: own rules
    b.isBroken = true;
    b.damageKind = static_cast<int>(kind);
    b.currentOutputMW = 0.0f;
    if (b.type == BuildingType::LAMP) b.lightRadius = 0.0f;
    return true;
}

void GameEngine::updateWeatherStreaks() {
    for (int player = 1; player <= 2; ++player) {
        WeatherType w = (player == 1) ? p1Weather : p2Weather; // weather of the day that just ended
        world.rainStreak[player] = isWet(w) ? world.rainStreak[player] + 1 : 0;
        world.dryStreak[player] = isDry(w) ? world.dryStreak[player] + 1 : 0;
    }
}

void GameEngine::scheduleHazard(HazardKind kind, int player, int westCol, int row) {
    HazardPlan plan;
    plan.kind = kind;
    plan.player = player;
    plan.westCol = westCol;
    plan.row = row;
    plan.atDaySeconds = config.daySeconds * (0.15f + 0.70f * rollUnit());
    world.plans.push_back(plan);

    if (kind == HazardKind::QUAKE) return; // quakes come without warning
    PowerFx fx;
    fx.kind = PowerFxKind::HAZARD_WARNING;
    fx.hazard = kind;
    fx.player = player;
    fx.title = std::string("ПРЕДУПРЕЖДЕНИЕ: ") + getHazardNameBg(kind) + "!";
    switch (kind) {
        case HazardKind::HAIL:
            fx.detail = "Градушка днес: слънчевите панели и\nмелниците може да бъдат повредени.";
            break;
        case HazardKind::FLOOD:
            fx.detail = "Пороен дъжд " + std::to_string(world.rainStreak[player] + 1) + " дни: реката може\nда залее парцелите на брега.";
            break;
        case HazardKind::WILDFIRE:
            fx.detail = "Суша " + std::to_string(world.dryStreak[player]) + " дни: опасност от горски пожар.\nДръжте дърво и желязо за ремонт.";
            break;
        default:
            break;
    }
    pushPowerFx(fx);
}

void GameEngine::scheduleDailyHazards() {
    world.plans.clear();
    if (!hazardsEnabled) return;
    if (currentDay <= config.graceDays) return; // no hazards while players set up

    for (int player = 1; player <= 2; ++player) {
        WeatherType w = (player == 1) ? p1Weather : p2Weather; // weather of the new day
        if (world.hailToday[player]) {
            scheduleHazard(HazardKind::HAIL, player, -1, -1);
            if (rollUnit() < 0.5f) scheduleHazard(HazardKind::HAIL, player, -1, -1); // sometimes a second burst
        }
        if (isWet(w) && world.rainStreak[player] >= PowerBalance::FLOOD_RAIN_STREAK && rollUnit() < PowerBalance::FLOOD_CHANCE) {
            scheduleHazard(HazardKind::FLOOD, player, -1, -1);
        }
        if (isDry(w) && currentSeason != SeasonType::WINTER && world.dryStreak[player] >= PowerBalance::WILDFIRE_DRY_STREAK) {
            float chance = (PowerBalance::WILDFIRE_CHANCE + PowerBalance::WILDFIRE_CHANCE_PER_DRY_DAY * (world.dryStreak[player] - 1)) *
                           ((currentSeason == SeasonType::SUMMER) ? 1.5f : 1.0f);
            if (rollUnit() < chance) scheduleHazard(HazardKind::WILDFIRE, player, -1, -1);
        }
    }
    if (rollUnit() < PowerBalance::QUAKE_DAILY_CHANCE) {
        scheduleHazard(HazardKind::QUAKE, 0, -1, -1); // epicentre chosen when it strikes
    }
}

void GameEngine::updateHazards(float dt) {
    (void)dt;
    for (auto& plan : world.plans) {
        if (!plan.fired && city.dailySeconds >= plan.atDaySeconds) {
            plan.fired = true;
            fireHazard(plan);
        }
    }
}

void GameEngine::triggerHazardNow(HazardKind kind, int player, int westCol, int row) {
    if (kind == HazardKind::NONE) return;
    HazardPlan plan;
    plan.kind = kind;
    plan.player = (kind == HazardKind::QUAKE) ? 0 : player;
    plan.westCol = westCol;
    plan.row = row;
    plan.fired = true;
    fireHazard(plan);
}

void GameEngine::fireHazard(HazardPlan& plan) {
    const int maxHits = PowerBalance::HAZARD_MAX_HITS;

    // Damages up to maxHits buildings of `player` that pass `chanceOf` (random order)
    auto strike = [&](int player, PowerFx& fx, const std::function<float(const PlacedBuilding&)>& chanceOf) {
        std::vector<size_t> order;
        for (size_t i = 0; i < buildings.size(); ++i) {
            if (buildings[i].playerOwner == player && !buildings[i].isBroken) order.push_back(i);
        }
        for (size_t i = order.size(); i > 1; --i) {
            size_t j = static_cast<size_t>(world.rng() % static_cast<unsigned>(i));
            std::swap(order[i - 1], order[j]);
        }
        int hits = 0;
        for (size_t idx : order) {
            if (hits >= maxHits) break;
            PlacedBuilding& b = buildings[idx];
            float c = chanceOf(b);
            if (c > 0.0f && rollUnit() < c && damageBuilding(b, plan.kind)) {
                fx.hits.push_back(b.position);
                ++hits;
            }
        }
        return hits;
    };

    auto plotOf = [&](const PlacedBuilding& b) { return findPlotAt(b.playerOwner, b.position); };

    auto finish = [&](PowerFx& fx, int hits, const std::string& what) {
        fx.kind = PowerFxKind::HAZARD_HIT;
        fx.hazard = plan.kind;
        fx.owner = fx.player;
        fx.title = std::string(getHazardNameBg(plan.kind)) + "!";
        fx.detail = (hits > 0) ? "Повредени: " + std::to_string(hits) + " " + what + ".\nРемонт: [ДЕЙСТВИЕ] върху сградата (5 дърво + 5 желязо)."
                               : "Без щети този път.";
        pushPowerFx(fx);
    };

    switch (plan.kind) {
        case HazardKind::HAIL: {
            PowerFx fx;
            fx.player = plan.player;
            // The whole sector: centre of the player's plot block
            float x0 = (plan.player == 1) ? layout.westStartX : layout.eastStartX();
            fx.pos = sf::Vector2f(x0 + layout.blockWidth() * 0.5f, layout.startY + (layout.plotRows * (layout.plotH + layout.gapY)) * 0.5f);
            fx.radius = layout.blockWidth() * 0.6f;
            int hits = strike(plan.player, fx, [](const PlacedBuilding& b) {
                if (b.type == BuildingType::SOLAR_PANEL) return PowerBalance::HAIL_BREAK_CHANCE;
                if (b.type == BuildingType::WIND_TURBINE) return PowerBalance::HAIL_BREAK_CHANCE * 0.5f;
                return 0.0f;
            });
            finish(fx, hits, "съоръжения (панели / мелници)");
            break;
        }
        case HazardKind::FLOOD: {
            PowerFx fx;
            fx.player = plan.player;
            sf::Vector2f sum(0.0f, 0.0f);
            int n = 0;
            for (const auto& p : landPlots) {
                if (p.playerOwner == plan.player && p.terrain == static_cast<int>(TerrainType::RIVER)) {
                    sum += centreOf(p.bounds);
                    ++n;
                }
            }
            fx.pos = (n > 0) ? sf::Vector2f(sum.x / n, sum.y / n) : sf::Vector2f(800.0f, 300.0f);
            fx.radius = layout.plotH * 1.5f;
            int hits = strike(plan.player, fx, [&](const PlacedBuilding& b) {
                const LandPlot* p = plotOf(b);
                if (p == nullptr || p->terrain != static_cast<int>(TerrainType::RIVER)) return 0.0f;
                return (b.type == BuildingType::HYDRO_PLANT) ? PowerBalance::FLOOD_BREAK_CHANCE * 0.5f : PowerBalance::FLOOD_BREAK_CHANCE;
            });
            finish(fx, hits, "сгради на речния бряг");
            break;
        }
        case HazardKind::WILDFIRE: {
            // Burns one purchased plot with buildings (or open land), may spread to a neighbour
            std::vector<const LandPlot*> sites, owned;
            for (const auto& p : landPlots) {
                if (p.playerOwner != plan.player || !p.isPurchased) continue;
                owned.push_back(&p);
                for (const auto& b : buildings) {
                    if (b.playerOwner == plan.player && !b.isBroken && !isPlotWideBuilding(b.type) && p.bounds.contains(b.position)) {
                        sites.push_back(&p);
                        break;
                    }
                }
            }
            const std::vector<const LandPlot*>& pool = sites.empty() ? owned : sites;
            if (pool.empty()) break;
            const LandPlot* origin = pool[static_cast<size_t>(world.rng() % static_cast<unsigned>(pool.size()))];
            const LandPlot* spread = nullptr;
            if (rollUnit() < PowerBalance::WILDFIRE_SPREAD_CHANCE) {
                std::vector<const LandPlot*> near;
                for (const LandPlot* p : owned) {
                    int d = std::abs(p->screenCol - origin->screenCol) + std::abs(p->row - origin->row);
                    if (d == 1) near.push_back(p);
                }
                if (!near.empty()) spread = near[static_cast<size_t>(world.rng() % static_cast<unsigned>(near.size()))];
            }
            PowerFx fx;
            fx.player = plan.player;
            fx.pos = centreOf(origin->bounds);
            fx.radius = std::max(layout.plotW, layout.plotH) * 0.6f;
            if (spread != nullptr) {
                fx.radius *= 1.8f;
                fx.pos = (fx.pos + centreOf(spread->bounds)) * 0.5f;
            }
            int hits = strike(plan.player, fx, [&](const PlacedBuilding& b) {
                if (origin->bounds.contains(b.position)) return PowerBalance::WILDFIRE_BREAK_CHANCE;
                if (spread != nullptr && spread->bounds.contains(b.position)) return PowerBalance::WILDFIRE_BREAK_CHANCE * 0.5f;
                return 0.0f;
            });
            finish(fx, hits, spread ? "сгради (огънят се разпространи)" : "сгради в парцела");
            break;
        }
        case HazardKind::QUAKE: {
            int wc = plan.westCol, row = plan.row;
            if (!layout.hasPlot(wc, row)) {
                std::vector<int> cells;
                for (int r = 0; r < layout.plotRows; ++r)
                    for (int c = 0; c < layout.plotCols; ++c)
                        if (layout.hasPlot(c, r)) cells.push_back(r * layout.plotCols + c);
                if (cells.empty()) break;
                int pick = cells[static_cast<size_t>(world.rng() % static_cast<unsigned>(cells.size()))];
                wc = pick % layout.plotCols;
                row = pick / layout.plotCols;
            }
            // Seismic safety: every reactor trips, construction sites shake loose some progress
            for (auto& b : buildings) {
                if (b.type == BuildingType::NUCLEAR && b.scramTimer <= 0.0f && !b.needsFuel) scramReactor(b, "ЗЕМЕТРЕСЕНИЕ");
                if (isMegaProject(b.type) && b.constructionLeft > 0.0f) {
                    b.constructionLeft = std::min(b.constructionTotal, b.constructionLeft + b.constructionTotal * 0.10f);
                }
            }
            // Mirrored epicentre: both players are hit at the same cell (fair), neighbours at half chance
            for (int player = 1; player <= 2; ++player) {
                int sc = (player == 1) ? wc : (layout.plotCols - 1 - wc);
                PowerFx fx;
                fx.player = player;
                sf::FloatRect epi = layout.plotRect(player, sc, row);
                fx.pos = centreOf(epi);
                fx.radius = std::max(layout.plotW, layout.plotH) * 1.4f;
                int hits = strike(player, fx, [&](const PlacedBuilding& b) {
                    const LandPlot* p = plotOf(b);
                    if (p == nullptr) return 0.0f;
                    int d = std::abs(p->screenCol - sc) + std::abs(p->row - row);
                    float base = (d == 0) ? PowerBalance::QUAKE_BREAK_CHANCE : (d == 1 ? PowerBalance::QUAKE_BREAK_CHANCE * 0.5f : 0.0f);
                    if (base > 0.0f && b.type == BuildingType::GEOTHERMAL) base += 0.2f;
                    return base;
                });
                finish(fx, hits, "сгради около епицентъра");
            }
            break;
        }
        case HazardKind::NONE:
            break;
    }
}
