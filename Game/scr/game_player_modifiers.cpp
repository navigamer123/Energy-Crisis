// =============================================================================
// [AI team] Minimal per-player modifiers (see Game/includes/game_player_modifiers.h)
// Used by the НЕВЪЗМОЖНО (Impossible) bot: faster mining, more income, cheaper buildings,
// no mining cooldown and a daily city-share bonus. Defaults change nothing.
// =============================================================================
#include "../includes/game_main.h"
#include <algorithm>
#include <cmath>

namespace {

PlayerModifiers sanitized(const PlayerModifiers& m) {
    PlayerModifiers s = m;
    s.incomeMult = std::max(0.0f, s.incomeMult);
    s.mineYieldMult = std::max(0.0f, s.mineYieldMult);
    s.costMult = std::max(0.0f, s.costMult);
    s.cooldownMult = std::max(0.0f, s.cooldownMult);
    s.shareBonus = std::min(1.0f, std::max(0.0f, s.shareBonus));
    return s;
}

// A non-zero recipe entry never becomes free: scaled, rounded up, at least 1
int scaledCost(int cost, float mult) {
    if (cost <= 0) return cost;
    if (mult == 1.0f) return cost;
    int scaled = static_cast<int>(std::ceil(static_cast<float>(cost) * mult - 1e-4f));
    return std::max(1, scaled);
}

} // namespace

void GameEngine::setPlayerModifiers(int player, const PlayerModifiers& mods) {
    playerMods[(player == 1) ? 0 : 1] = sanitized(mods);
}

const PlayerModifiers& GameEngine::getPlayerModifiers(int player) const {
    return playerMods[(player == 1) ? 0 : 1];
}

BuildingCost GameEngine::getBuildingCost(int player, BuildingType type) const {
    BuildingCost c = getBuildingCost(type);
    float m = getPlayerModifiers(player).costMult;
    if (m != 1.0f) {
        c.woodCost = scaledCost(c.woodCost, m);
        c.ironCost = scaledCost(c.ironCost, m);
        c.copperCost = scaledCost(c.copperCost, m);
        c.coalCost = scaledCost(c.coalCost, m);
        c.siliconCost = scaledCost(c.siliconCost, m);
        c.silverCost = scaledCost(c.silverCost, m);
        c.oreCost = scaledCost(c.oreCost, m);
    }
    return c;
}

float GameEngine::dailyShareBonusShift(bool p1Met, bool p2Met) const {
    float shift = 0.0f;
    if (p1Met) shift += playerMods[0].shareBonus;
    if (p2Met) shift -= playerMods[1].shareBonus;
    return shift;
}
