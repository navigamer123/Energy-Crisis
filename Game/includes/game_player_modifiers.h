#pragma once

// =============================================================================
// [AI team] Minimal per-player modifiers (used by the НЕВЪЗМОЖНО / Impossible bot)
//
// Wave A (engine) ships the full PlayerModifiers system with the same field names and the same
// GameEngine::setPlayerModifiers / getPlayerModifiers / getBuildingCost(player, type) API.
// When both branches meet, keep wave A's struct and drop this header plus the matching
// definitions in Game/scr/game_player_modifiers.cpp.
//
// Defaults are neutral: a player without modifiers plays by the normal rules.
// GameEngine::init() resets both players to the defaults (a new match starts fair); the UI
// re-applies the bot's modifiers after every (re)start.
// =============================================================================
struct PlayerModifiers {
    float incomeMult = 1.0f;    // city money payout and gold dividend
    float mineYieldMult = 1.0f; // every mining hit (on top of the mine level multiplier)
    float costMult = 1.0f;      // building recipes: each non-zero cost is scaled and rounded up
    float cooldownMult = 1.0f;  // mining cooldown (applied by the UI; 0 = no cooldown)
    float shareBonus = 0.0f;    // extra city share (0..1) won at every settled day end where the
                                // player met the city demand, on top of the normal daily shift
};
