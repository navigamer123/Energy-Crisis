#include "../includes/UI_matchStats.h"
#include <algorithm>
#include <cmath>
#include <iterator>

// =============================================================================
// team info: match telemetry (samples, day records, per-player totals, info events)
// =============================================================================

namespace {
int sourceOf(BuildingType t) {
    switch (t) {
        case BuildingType::SOLAR_PANEL:  return UI_matchStats::SOLAR;
        case BuildingType::WIND_TURBINE: return UI_matchStats::WIND;
        case BuildingType::HYDRO_PLANT:  return UI_matchStats::HYDRO;
        case BuildingType::BATTERY:      return UI_matchStats::BATTERY;
        default:                         return -1;
    }
}
} // namespace

float UI_matchStats::matchHours(const GameEngine& engine) {
    // Days roll over at 06:00, so hours since 06:00 of day 1
    float sinceRollover = std::fmod(engine.getHour24() - Balance::CLOCK_HOUR_AT_ZERO + 24.0f, 24.0f);
    return static_cast<float>(engine.getCurrentDay() - 1) * 24.0f + sinceRollover;
}

void UI_matchStats::reset() {
    samples.clear();
    days.clear();
    totals[0] = PlayerTotals();
    totals[1] = PlayerTotals();
    for (auto& row : liveMW) std::fill(std::begin(row), std::end(row), 0.0f);
    prev = Snapshot();
    hasPrev = false;
    lastHours = 0.0f;
    nextSampleHours = 0.0f;
    pendingLightning[0] = pendingLightning[1] = 0;
    preTodayAvg[0] = preTodayAvg[1] = 0.0f;
    preDemand = 0;
}

UI_matchStats::Snapshot UI_matchStats::takeSnapshot(const GameEngine& engine) const {
    Snapshot s;
    s.day = engine.getCurrentDay();
    s.winner = engine.getCityState().winner;
    s.p1Share = engine.getCityState().p1CityShare;
    s.season = engine.getSeason();
    for (int p = 0; p < 2; ++p) {
        const int player = p + 1;
        const PlayerEconomy& e = engine.getPlayerEconomy(player);
        s.weather[p] = engine.getPlayerWeather(player);
        for (int k = 0; k < 8; ++k) s.mineLevels[p][k] = e.mineLevels[k];
        s.materials[p] = static_cast<long>(e.wood) + e.iron + e.copper + e.coal + e.silicon + e.silver;
        s.money[p] = e.money;
        s.gold[p] = e.gold;
    }
    for (const auto& b : engine.getBuildings()) {
        int idx = static_cast<int>(b.type);
        if (b.playerOwner >= 1 && b.playerOwner <= 2 && idx >= 0 && idx < 7) s.buildings[b.playerOwner - 1][idx]++;
    }
    for (const auto& plot : engine.getLandPlots()) {
        if (plot.isPurchased && plot.playerOwner >= 1 && plot.playerOwner <= 2) s.plots[plot.playerOwner - 1]++;
    }
    return s;
}

void UI_matchStats::beforeEngineUpdate(const GameEngine& engine) {
    preTodayAvg[0] = engine.getTodayAverageMW(1);
    preTodayAvg[1] = engine.getTodayAverageMW(2);
    preDemand = engine.getCityState().cityEnergyDemand;
}

void UI_matchStats::onInfoEvent(const InfoEvent& ev) {
    if (ev.type == InfoEventType::LOST_LIGHTNING && (ev.player == 1 || ev.player == 2)) {
        totals[ev.player - 1].lostToLightning++;
        pendingLightning[ev.player - 1]++;
    }
}

void UI_matchStats::afterEngineUpdate(const GameEngine& engine, std::vector<InfoEvent>& out) {
    const float hours = matchHours(engine);
    Snapshot now = takeSnapshot(engine);

    if (!hasPrev) {
        prev = now;
        hasPrev = true;
        lastHours = hours;
        nextSampleHours = hours;
    }
    const float dH = std::max(0.0f, hours - lastHours);

    // 1. Energy by source (MWh), CO2 avoided and live output
    for (auto& row : liveMW) std::fill(std::begin(row), std::end(row), 0.0f);
    for (const auto& b : engine.getBuildings()) {
        int src = sourceOf(b.type);
        if (src < 0 || b.playerOwner < 1 || b.playerOwner > 2) continue;
        float mw = std::max(0.0f, b.currentOutputMW);
        liveMW[b.playerOwner - 1][src] += mw;
    }
    for (int p = 0; p < 2; ++p) {
        double clean = 0.0;
        for (int s = 0; s < SOURCE_COUNT; ++s) {
            double e = static_cast<double>(liveMW[p][s]) * dH;
            totals[p].mwh[s] += e;
            if (s != BATTERY) clean += e; // Battery output is clean energy already counted when generated
        }
        totals[p].co2t += clean * CO2_T_PER_MWH;
        totals[p].peakMW = std::max(totals[p].peakMW, engine.getPlayerEconomy(p + 1).energyMW);
    }

    // 2. Diff the snapshots into totals and info events
    for (int p = 0; p < 2; ++p) {
        const int player = p + 1;
        PlayerTotals& t = totals[p];

        int removed = 0;
        for (int k = 1; k <= 6; ++k) {
            int d = now.buildings[p][k] - prev.buildings[p][k];
            if (d > 0) {
                t.built[k] += d;
                t.builtTotal += d;
                InfoEvent ev;
                ev.type = InfoEventType::BUILT;
                ev.player = player;
                ev.building = static_cast<BuildingType>(k);
                ev.value = d;
                out.push_back(ev);
            } else if (d < 0) {
                removed += -d;
            }
        }
        if (removed > 0) {
            int byLightning = std::min(removed, pendingLightning[p]);
            pendingLightning[p] -= byLightning;
            int demolished = removed - byLightning;
            if (demolished > 0) {
                t.demolished += demolished;
                InfoEvent ev;
                ev.type = InfoEventType::DEMOLISHED;
                ev.player = player;
                ev.value = demolished;
                out.push_back(ev);
            }
        }
        pendingLightning[p] = 0; // A strike is always applied before this diff runs

        if (now.plots[p] > prev.plots[p]) {
            t.plotsBought += now.plots[p] - prev.plots[p];
            InfoEvent ev;
            ev.type = InfoEventType::PLOT_BOUGHT;
            ev.player = player;
            ev.value = now.plots[p];
            out.push_back(ev);
        }
        for (int k = 1; k < 8; ++k) {
            if (now.mineLevels[p][k] > prev.mineLevels[p][k]) {
                t.mineUpgrades += now.mineLevels[p][k] - prev.mineLevels[p][k];
                InfoEvent ev;
                ev.type = InfoEventType::MINE_UPGRADED;
                ev.player = player;
                ev.resource = static_cast<ResourceType>(k);
                ev.value = now.mineLevels[p][k];
                out.push_back(ev);
            }
        }
        if (now.materials[p] > prev.materials[p]) t.mined += now.materials[p] - prev.materials[p];
        if (now.money[p] > prev.money[p]) t.moneyEarned += now.money[p] - prev.money[p];
        if (now.gold[p] > prev.gold[p]) t.goldEarned += now.gold[p] - prev.gold[p];

        if (now.weather[p] != prev.weather[p]) {
            InfoEvent ev;
            ev.type = InfoEventType::WEATHER_CHANGED;
            ev.player = player;
            ev.value = static_cast<int>(now.weather[p]);
            out.push_back(ev);
        }
    }

    // 3. Settled days (the engine settles at the 06:00 rollover)
    for (int endedDay = prev.day; endedDay < now.day; ++endedDay) {
        DayRecord rec;
        rec.day = endedDay;
        const bool firstDay = (endedDay == prev.day);
        rec.demand = firstDay ? preDemand : -1;
        rec.avgMW[0] = firstDay ? static_cast<int>(std::floor(preTodayAvg[0] + 0.01f)) : -1;
        rec.avgMW[1] = firstDay ? static_cast<int>(std::floor(preTodayAvg[1] + 0.01f)) : -1;
        rec.shareBefore = prev.p1Share;
        rec.shareAfter = now.p1Share;
        const float shift = now.p1Share - prev.p1Share;
        if (endedDay <= Balance::GRACE_PERIOD_DAYS) {
            rec.outcome = DayOutcome::GRACE;
        } else if (shift > 0.001f) {
            rec.outcome = DayOutcome::P1_TOOK;
        } else if (shift < -0.001f) {
            rec.outcome = DayOutcome::P2_TOOK;
        } else {
            bool p1Met = rec.avgMW[0] >= rec.demand;
            bool p2Met = rec.avgMW[1] >= rec.demand;
            rec.outcome = (p1Met && p2Met) ? DayOutcome::BOTH_MET : DayOutcome::NONE_MET;
        }
        if (rec.outcome != DayOutcome::GRACE) {
            if (rec.outcome == DayOutcome::P1_TOOK) totals[0].daysWon++;
            if (rec.outcome == DayOutcome::P2_TOOK) totals[1].daysWon++;
            for (int p = 0; p < 2; ++p) {
                if (rec.avgMW[p] >= 0 && rec.demand >= 0 && rec.avgMW[p] >= rec.demand) totals[p].daysMet++;
            }
        }
        days.push_back(rec);

        InfoEvent ev;
        ev.type = InfoEventType::DAY_SETTLED;
        ev.value = endedDay;
        ev.value2 = static_cast<int>(rec.outcome);
        ev.share = rec.shareAfter;
        out.push_back(ev);

        if (endedDay == Balance::GRACE_PERIOD_DAYS) {
            InfoEvent g;
            g.type = InfoEventType::GRACE_ENDED;
            g.value = endedDay + 1;
            out.push_back(g);
        }
    }
    if (now.season != prev.season) {
        InfoEvent ev;
        ev.type = InfoEventType::SEASON_CHANGED;
        ev.value = static_cast<int>(now.season);
        out.push_back(ev);
    }
    if (now.winner != 0 && prev.winner == 0) {
        InfoEvent ev;
        ev.type = InfoEventType::MATCH_ENDED;
        ev.value = now.winner;
        ev.share = now.p1Share;
        out.push_back(ev);
    }

    for (int p = 0; p < 2; ++p) {
        float share = (p == 0) ? now.p1Share : 1.0f - now.p1Share;
        totals[p].minShare = std::min(totals[p].minShare, share);
        totals[p].maxShare = std::max(totals[p].maxShare, share);
    }

    // 4. Time-series samples (several at once after a large time jump)
    int guard = 0;
    while (nextSampleHours <= hours && guard++ < 400) {
        Sample s;
        s.hours = nextSampleHours;
        s.day = now.day;
        s.hour24 = engine.getHour24();
        s.mw[0] = engine.getPlayerEconomy(1).energyMW;
        s.mw[1] = engine.getPlayerEconomy(2).energyMW;
        s.demand = engine.getCityState().cityEnergyDemand;
        s.p1Share = now.p1Share;
        s.co2t[0] = static_cast<float>(totals[0].co2t);
        s.co2t[1] = static_cast<float>(totals[1].co2t);
        s.todayAvg[0] = engine.getTodayAverageMW(1);
        s.todayAvg[1] = engine.getTodayAverageMW(2);
        samples.push_back(s);
        nextSampleHours += SAMPLE_EVERY_HOURS;
    }
    if (guard >= 400) nextSampleHours = hours + SAMPLE_EVERY_HOURS;

    prev = now;
    lastHours = hours;
}
