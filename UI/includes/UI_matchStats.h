#ifndef UI_MATCH_STATS_H
#define UI_MATCH_STATS_H

#include <vector>
#include "UI_infoEvents.h"

// -----------------------------------------------------------------------------
// team info: match telemetry for the dashboard (Tab), the post-match report and the
// notification system. Samples the engine every quarter of a game hour, keeps one
// record per settled day (from DAY_SETTLED) and per-player totals (from the InfoEvents that
// UI_map::dispatchEngineEvents feeds in, plus the energy and income it measures itself).
// -----------------------------------------------------------------------------
class UI_matchStats {
public:
    static constexpr float SAMPLE_EVERY_HOURS = 0.25f; // 96 samples per in-game day
    static constexpr float CO2_T_PER_MWH = 0.40f;      // Approx. CO2 avoided per MWh of clean power (grid average, to verify)

    enum Source { SOLAR = 0, WIND = 1, HYDRO = 2, BATTERY = 3, SOURCE_COUNT = 4 };

    enum class DayOutcome { GRACE = 0, P1_TOOK = 1, P2_TOOK = 2, BOTH_MET = 3, NONE_MET = 4 };

    struct Sample {
        float hours = 0.0f;   // Game hours since the match started (day 1, 06:00 = 0)
        int day = 1;
        float hour24 = 0.0f;
        int mw[2] = { 0, 0 }; // Power delivered to the city right now
        int demand = 0;
        float p1Share = 0.5f;
        float co2t[2] = { 0.0f, 0.0f };
        float todayAvg[2] = { 0.0f, 0.0f };
    };

    struct DayRecord {
        int day = 0;
        int demand = 0;
        int avgMW[2] = { 0, 0 };
        float shareBefore = 0.5f; // P1 share before the settlement
        float shareAfter = 0.5f;
        DayOutcome outcome = DayOutcome::GRACE;
    };

    struct PlayerTotals {
        double mwh[SOURCE_COUNT] = { 0.0, 0.0, 0.0, 0.0 }; // Energy produced by source (MWh)
        double co2t = 0.0;        // CO2 avoided (t)
        int peakMW = 0;
        int built[7] = { 0, 0, 0, 0, 0, 0, 0 }; // Indexed by BuildingType
        int builtTotal = 0;
        int demolished = 0;
        int lostToLightning = 0;
        int plotsBought = 0;
        int mineUpgrades = 0;
        long mined = 0;           // Raw materials gathered (wood, iron, copper, coal, silicon, silver)
        long moneyEarned = 0;
        long goldEarned = 0;
        int daysWon = 0;          // Settlements that moved the city towards this player
        int daysMet = 0;          // Settlements where this player met the demand
        float minShare = 0.5f;
        float maxShare = 0.5f;
    };

    void reset();
    // Once per frame, after the engine update: energy by source, CO2, income, share range, time series
    void sample(const GameEngine& engine);
    // Discrete events (built, lost, mined, settled day...) from the engine event queue
    void onInfoEvent(const InfoEvent& ev);

    const std::vector<Sample>& getSamples() const { return samples; }
    const std::vector<DayRecord>& getDays() const { return days; }
    const PlayerTotals& getTotals(int player) const { return totals[player == 2 ? 1 : 0]; }
    float getCurrentHours() const { return lastHours; }
    // Live output by source for a player right now (MW)
    float getLiveSourceMW(int player, int source) const { return liveMW[player == 2 ? 1 : 0][source]; }

    static float matchHours(const GameEngine& engine);

private:
    struct Snapshot {
        float p1Share = 0.5f;
        long money[2] = { 0, 0 };
        long gold[2] = { 0, 0 };
    };

    Snapshot takeSnapshot(const GameEngine& engine) const;

    std::vector<Sample> samples;
    std::vector<DayRecord> days;
    PlayerTotals totals[2];
    float liveMW[2][SOURCE_COUNT] = {};
    Snapshot prev;
    bool hasPrev = false;
    float lastHours = 0.0f;
    float nextSampleHours = 0.0f;
    float lastSettledShare = 0.5f; // P1 share after the previous settlement (DayRecord::shareBefore)
};

#endif // UI_MATCH_STATS_H
