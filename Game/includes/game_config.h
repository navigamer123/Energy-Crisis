#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "game_balance.h"

// =============================================================================
// ENERGY CRISIS - MATCH CONFIGURATION
// Rules chosen before a match starts (GameEngine::init(const MatchConfig&)).
// A default-constructed MatchConfig reproduces the standard rules exactly.
// =============================================================================
struct MatchConfig {
    int finalDay = Balance::FINAL_DAY;              // the larger share wins when this day ends
    float victoryShare = Balance::VICTORY_SHARE;    // instant win at this city share (checked at day end)
    float daySeconds = Balance::SECONDS_PER_DAY;    // game-seconds per 24 h day (90 s)
    int graceDays = Balance::GRACE_PERIOD_DAYS;     // first days without city demand (at most finalDay - 1)
    uint32_t seed = 0;                              // 0 = EC_SEED environment variable, else the clock
    std::vector<std::string> mutators;              // rule variations by name, read by later features
    int mapPreset = 0;                              // 0 = standard map (the only layout so far)
    bool sandbox = false;                           // no victory: the match never ends

    // Valid ranges the engine enforces (init() clamps out-of-range values)
    static constexpr int MIN_FINAL_DAY = 1;
    static constexpr int MAX_FINAL_DAY = 999;
    static constexpr float MIN_VICTORY_SHARE = 0.51f;
    static constexpr float MAX_VICTORY_SHARE = 1.0f;
    static constexpr float MIN_DAY_SECONDS = 10.0f;
    static constexpr float MAX_DAY_SECONDS = 3600.0f;

    bool hasMutator(const std::string& name) const {
        for (const auto& m : mutators) {
            if (m == name) return true;
        }
        return false;
    }
};

// =============================================================================
// Per-player modifiers (handicaps, charters, events). Neutral values change nothing.
// Set them after init() / restartGame(): a new match starts with neutral modifiers.
// =============================================================================
struct PlayerModifiers {
    float incomeMult = 1.0f;     // city money payout and gold dividend
    float mineYieldMult = 1.0f;  // resources per mining action
    float costMult = 1.0f;       // resource cost of buildings (also scales the demolition refund)
    float cooldownMult = 1.0f;   // mining cooldown (GameEngine::getMineCooldown, enforced by the caller)
    float shareBonus = 0.0f;     // extra city share added to a day this player wins (e.g. 0.02 = +2%)

    static constexpr float MAX_MULT = 100.0f;
    static constexpr float MAX_SHARE_BONUS = 0.5f;
};
