#include "../includes/UI_matchStats.h"
#include <algorithm>
#include <cmath>
#include <iterator>

// =============================================================================
// team info: match telemetry (samples, day records, per-player totals)
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
    lastSettledShare = 0.5f;
}

UI_matchStats::Snapshot UI_matchStats::takeSnapshot(const GameEngine& engine) const {
    Snapshot s;
    s.p1Share = engine.getCityState().p1CityShare;
    for (int p = 0; p < 2; ++p) {
        const PlayerEconomy& e = engine.getPlayerEconomy(p + 1);
        s.money[p] = e.money;
        s.gold[p] = e.gold;
    }
    return s;
}

void UI_matchStats::onInfoEvent(const InfoEvent& ev) {
    PlayerTotals* t = (ev.player == 1 || ev.player == 2) ? &totals[ev.player - 1] : nullptr;
    const int b = static_cast<int>(ev.building);
    switch (ev.type) {
        case InfoEventType::BUILT:
            if (t && b >= 1 && b <= 6) {
                t->built[b] += ev.value;
                t->builtTotal += ev.value;
            }
            break;
        case InfoEventType::DEMOLISHED:
            if (t) t->demolished += ev.value;
            break;
        case InfoEventType::LOST_LIGHTNING:
            if (t) t->lostToLightning++;
            break;
        case InfoEventType::PLOT_BOUGHT:
            if (t) t->plotsBought++;
            break;
        case InfoEventType::MINE_UPGRADED:
            if (t) t->mineUpgrades++;
            break;
        case InfoEventType::MINED: // raw materials only; gold and money are counted as income
            if (t && ev.resource != ResourceType::GOLD && ev.resource != ResourceType::MONEY) t->mined += ev.value;
            break;
        case InfoEventType::DAY_SETTLED: {
            DayRecord rec;
            rec.day = ev.value;
            rec.demand = ev.demand;
            rec.avgMW[0] = ev.avgMW[0];
            rec.avgMW[1] = ev.avgMW[1];
            rec.shareBefore = lastSettledShare;
            rec.shareAfter = ev.share;
            rec.outcome = static_cast<DayOutcome>(ev.value2);
            if (rec.outcome != DayOutcome::GRACE) {
                if (rec.outcome == DayOutcome::P1_TOOK) totals[0].daysWon++;
                if (rec.outcome == DayOutcome::P2_TOOK) totals[1].daysWon++;
                for (int q = 0; q < 2; ++q) {
                    if (rec.avgMW[q] >= rec.demand) totals[q].daysMet++;
                }
            }
            days.push_back(rec);
            lastSettledShare = ev.share;
            break;
        }
        default: // GRACE_ENDED, SEASON_CHANGED, WEATHER_CHANGED, MATCH_ENDED: notifications only
            break;
    }
}

void UI_matchStats::sample(const GameEngine& engine) {
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

    // 2. Income (no engine event reports payouts) and the share range
    for (int p = 0; p < 2; ++p) {
        if (now.money[p] > prev.money[p]) totals[p].moneyEarned += now.money[p] - prev.money[p];
        if (now.gold[p] > prev.gold[p]) totals[p].goldEarned += now.gold[p] - prev.gold[p];
        float share = (p == 0) ? now.p1Share : 1.0f - now.p1Share;
        totals[p].minShare = std::min(totals[p].minShare, share);
        totals[p].maxShare = std::max(totals[p].maxShare, share);
    }

    // 3. Time-series samples (several at once after a large time jump)
    int guard = 0;
    while (nextSampleHours <= hours && guard++ < 400) {
        Sample s;
        s.hours = nextSampleHours;
        s.day = engine.getCurrentDay();
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
