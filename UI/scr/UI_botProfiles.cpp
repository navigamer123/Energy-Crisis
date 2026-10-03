// =============================================================================
// [AI team] Bot profile tables (see UI/includes/UI_botProfiles.h)
// Difficulty = skill (how fast and how carefully it plays), rival = style (what it builds).
// Tuned with the headless match simulation scratch/sim_bot_difficulty.cpp.
// =============================================================================
#include "../includes/UI_botProfiles.h"
#include <algorithm>

namespace {

// ---- Colours (the integrator maps these to UI_theme.h tokens) ---------------
const sf::Color kColEasy(90, 220, 130);
const sf::Color kColMedium(255, 190, 60);
const sf::Color kColHard(255, 125, 60);
const sf::Color kColImpossible(255, 55, 55);

struct Skill {
    float moveSpeed, decisionInterval, mineHitInterval, afterMineThink, actionCooldown, slipChance;
    float demandMargin, floorMW;
    BotSupplyView supplyView;
    float lookaheadHours;
    int maxMineLevel, upgradeChancePct, expandChancePct;
};

// BAL-01 governor: every tier projects the day average exactly (game_forecast), but EASY aims 10%
// BELOW the city demand, MEDIUM right at it (and slips more), HARD keeps a 5% reserve, prepares
// the next day 2 game-hours ahead and reacts at once. Building output is coarse (one plant covers
// several days of demand growth), so a margin under 1.0 really does miss days. Tuned with scratch/sim_bot_difficulty.cpp.
// НЕВЪЗМОЖНО: see omegaProfile.
const Skill kSkills[3] = {
    // move  think  mine  after  actCd  slip    margin floor  view                       look  lvl upg exp
    { 260.f, 0.70f, 1.25f, 0.30f, 0.20f, 0.12f,  0.90f, 15.f, BotSupplyView::PROJECTED, 0.0f,  2, 25,  50 }, // EASY
    { 420.f, 0.25f, 1.08f, 0.12f, 0.15f, 0.07f,  0.975f, 25.f, BotSupplyView::PROJECTED, 0.0f, 3, 50, 100 }, // MEDIUM
    { 650.f, 0.06f, 1.00f, 0.04f, 0.15f, 0.04f,  1.05f, 40.f, BotSupplyView::PROJECTED, 2.0f, Balance::MINE_MAX_LEVEL, 85, 100 }, // HARD
};

BotProfile hydroBaron() {
    BotProfile p;
    p.personalityId = 0;
    p.nameBg = "Хидро Барон";
    p.taglineBg = "Реката работи за мен денонощно.";
    p.styleBg = "ВЕЦ по брега и ранна земя. Слаб в сухи слънчеви дни.";
    p.signatureBg = "ВЕЦ + ЗЕМЯ";
    p.accent = sf::Color(80, 200, 255);
    p.hydroScore = 150.0f; p.windScore = 90.0f; p.solarScore = 55.0f;
    p.rainHydroBonus = 50.0f; p.windyWindBonus = 40.0f; p.stormWindBonus = 60.0f; p.nightWindBonus = 25.0f;
    p.batteryCap = 1; p.batteryMinMW = 120; p.batteryScore = 60.0f;
    p.lampCap = 2;
    p.preferRiverPlots = true;
    p.expandFreeSlots = 4;
    p.upgradeOrder[0] = ResourceType::IRON; p.upgradeOrder[1] = ResourceType::WOOD;
    p.upgradeOrder[2] = ResourceType::COPPER; p.upgradeOrder[3] = ResourceType::SILICON;
    return p;
}

BotProfile solarSprinter() {
    BotProfile p;
    p.personalityId = 1;
    p.nameBg = "Слънчев Спринтер";
    p.taglineBg = "Слънцето изгрява за победителите.";
    p.styleBg = "Соларни панели и батерии. Отслабва нощем и в облачни дни.";
    p.signatureBg = "СЛЪНЦЕ + БАТЕРИИ";
    p.accent = sf::Color(255, 205, 70);
    p.hydroScore = 60.0f; p.windScore = 65.0f; p.solarScore = 165.0f;
    p.rainHydroBonus = 30.0f; p.windyWindBonus = 30.0f; p.stormWindBonus = 40.0f; p.nightWindBonus = 20.0f;
    p.batteryCap = 4; p.batteryMinMW = 60; p.batteryScore = 125.0f;
    p.lampCap = 1;
    p.expandFreeSlots = 2;
    p.upgradeOrder[0] = ResourceType::SILICON; p.upgradeOrder[1] = ResourceType::COPPER;
    p.upgradeOrder[2] = ResourceType::WOOD; p.upgradeOrder[3] = ResourceType::IRON;
    p.stockpileQuota[3] = 24; // silicon for the next panels
    return p;
}

BotProfile stormChaser() {
    BotProfile p;
    p.personalityId = 2;
    p.nameBg = "Буреносец";
    p.taglineBg = "Колкото по-силна бурята, толкова по-добре.";
    p.styleBg = "Само вятърни мелници, без панели. Тихият ден го забавя.";
    p.signatureBg = "ВЯТЪР";
    p.accent = sf::Color(175, 145, 255);
    p.hydroScore = 70.0f; p.windScore = 165.0f; p.solarScore = 0.0f; p.neverSolar = true;
    p.rainHydroBonus = 30.0f; p.windyWindBonus = 75.0f; p.stormWindBonus = 110.0f; p.nightWindBonus = 40.0f;
    p.batteryCap = 1; p.batteryMinMW = 150; p.batteryScore = 50.0f;
    p.lampCap = 2;
    p.expandFreeSlots = 2;
    p.upgradeOrder[0] = ResourceType::IRON; p.upgradeOrder[1] = ResourceType::COAL;
    p.upgradeOrder[2] = ResourceType::COPPER; p.upgradeOrder[3] = ResourceType::WOOD;
    p.stockpileQuota[4] = 20; // coal for the next turbines
    return p;
}

BotProfile miser() {
    BotProfile p;
    p.personalityId = 3;
    p.nameBg = "Скъперника";
    p.taglineBg = "Всяка стотинка се брои.";
    p.styleBg = "Пести до ден 8 и надгражда мини, после удря с пълна сила.";
    p.signatureBg = "ПЕСТИ, ПОСЛЕ УДРЯ";
    p.accent = sf::Color(120, 230, 140);
    p.hydroScore = 110.0f; p.windScore = 105.0f; p.solarScore = 80.0f;
    p.batteryCap = 2; p.batteryMinMW = 110; p.batteryScore = 70.0f;
    p.lampCap = 1;
    p.expandFreeSlots = 1;
    p.marginScale = 0.85f; p.surgeDay = 8; p.surgeMarginScale = 1.35f;
    return p;
}

BotProfile miningMagnate() {
    BotProfile p;
    p.personalityId = 4;
    p.nameBg = "Минен Магнат";
    p.taglineBg = "Първо мините, после светът.";
    p.styleBg = "Надгражда мините в гратисния период. Бавен старт, силен финал.";
    p.signatureBg = "МИНИ";
    p.accent = sf::Color(255, 150, 70);
    p.hydroScore = 100.0f; p.windScore = 110.0f; p.solarScore = 70.0f;
    p.batteryCap = 1; p.batteryMinMW = 120; p.batteryScore = 55.0f;
    p.lampCap = 1;
    p.expandFreeSlots = 2;
    p.graceMineTarget = 3;
    p.stockpileQuota[0] = 40; p.stockpileQuota[1] = 40;
    return p;
}

// НЕВЪЗМОЖНО: the strongest strategy the bot knows (24/7 sources, river first, aggressive land,
// worst-case weather planning) plus instant reactions and engine advantages no human has.
BotProfile omegaProfile() {
    BotProfile p;
    p.personalityId = BOT_PERSONALITY_OMEGA;
    p.nameBg = "ОМЕГА";
    p.taglineBg = "Вижда бурята, преди да е дошла.";
    p.styleBg = "×3 добив, ×2 доход, ½ цена, без пауза при добив, +6% град на ден.";
    p.signatureBg = "ВСИЧКО";
    p.accent = kColImpossible;

    p.moveSpeed = 950.0f;
    p.decisionInterval = 0.02f;
    p.mineHitInterval = 0.15f;
    p.afterMineThink = 0.0f;
    p.actionCooldown = 0.0f;
    p.slipChance = 0.0f;
    p.demandMargin = 4.0f;
    p.floorMW = 5000.0f; // never "satisfied": keeps building until every plot is full
    p.supplyView = BotSupplyView::WORST_CASE;
    p.lookaheadHours = 24.0f;
    p.maxMineLevel = Balance::MINE_MAX_LEVEL;
    p.upgradeChancePct = 100;
    p.expandChancePct = 100;
    p.engineEdge.incomeMult = 2.0f;
    p.engineEdge.mineYieldMult = 3.0f;
    p.engineEdge.costMult = 0.5f;
    p.engineEdge.cooldownMult = 0.0f;
    p.engineEdge.shareBonus = 0.06f;

    p.hydroScore = 150.0f; p.windScore = 140.0f; p.solarScore = 40.0f;
    p.rainHydroBonus = 40.0f; p.windyWindBonus = 50.0f; p.stormWindBonus = 70.0f; p.nightWindBonus = 30.0f;
    p.batteryCap = 3; p.batteryMinMW = 150; p.batteryScore = 90.0f;
    p.lampCap = 12;
    p.preferRiverPlots = true;
    p.expandFreeSlots = 6;
    p.stockpileQuota[0] = 60; p.stockpileQuota[1] = 60; p.stockpileQuota[2] = 40;
    p.stockpileQuota[3] = 30; p.stockpileQuota[4] = 30;
    return p;
}

const BotProfile& personalityTable(int id) {
    static const BotProfile table[BOT_PERSONALITY_COUNT] = { hydroBaron(), solarSprinter(), stormChaser(), miser(),
                                                             miningMagnate() };
    static const BotProfile omega = omegaProfile();
    if (id == BOT_PERSONALITY_OMEGA) return omega;
    if (id < 0 || id >= BOT_PERSONALITY_COUNT) return table[0];
    return table[id];
}

} // namespace

const BotProfile& botPersonality(int personalityId) {
    return personalityTable(personalityId);
}

BotProfile makeBotProfile(BotDifficulty difficulty, int personalityId) {
    if (difficulty == BotDifficulty::IMPOSSIBLE) return personalityTable(BOT_PERSONALITY_OMEGA);

    BotProfile p = personalityTable(personalityId);
    int tier = (difficulty == BotDifficulty::EASY) ? 0 : (difficulty == BotDifficulty::MEDIUM ? 1 : 2);
    const Skill& s = kSkills[tier];
    p.moveSpeed = s.moveSpeed;
    p.decisionInterval = s.decisionInterval;
    p.mineHitInterval = s.mineHitInterval;
    p.afterMineThink = s.afterMineThink;
    p.actionCooldown = s.actionCooldown;
    p.slipChance = s.slipChance;
    p.demandMargin = s.demandMargin;
    p.floorMW = s.floorMW;
    p.supplyView = s.supplyView;
    p.lookaheadHours = s.lookaheadHours;
    p.maxMineLevel = s.maxMineLevel;
    p.upgradeChancePct = s.upgradeChancePct;
    p.expandChancePct = s.expandChancePct;
    p.engineEdge = PlayerModifiers(); // only НЕВЪЗМОЖНО bends the rules

    // Rival skill tweaks
    if (p.personalityId == 4) { // Минен Магнат: one mine level more, always takes an upgrade
        p.maxMineLevel = std::min(Balance::MINE_MAX_LEVEL, p.maxMineLevel + 1);
        p.upgradeChancePct = 100;
    } else if (p.personalityId == 3) { // Скъперника: upgrades eagerly while it saves
        p.upgradeChancePct = std::min(100, p.upgradeChancePct + 25);
    } else if (p.personalityId == 0) { // Хидро Барон: never hesitates over land
        p.expandChancePct = 100;
    }
    return p;
}

const char* botDifficultyNameBg(BotDifficulty difficulty) {
    switch (difficulty) {
        case BotDifficulty::EASY: return "ЛЕСНО";
        case BotDifficulty::MEDIUM: return "СРЕДНО";
        case BotDifficulty::HARD: return "ТРУДНО";
        case BotDifficulty::IMPOSSIBLE: return "НЕВЪЗМОЖНО";
        default: return "";
    }
}

const char* botDifficultyTagBg(BotDifficulty difficulty) {
    switch (difficulty) {
        case BotDifficulty::EASY: return "ЛЕСЕН";
        case BotDifficulty::MEDIUM: return "СРЕДЕН";
        case BotDifficulty::HARD: return "ТРУДЕН";
        case BotDifficulty::IMPOSSIBLE: return "НЕВЪЗМОЖЕН";
        default: return "";
    }
}

sf::Color botDifficultyColor(BotDifficulty difficulty) {
    switch (difficulty) {
        case BotDifficulty::EASY: return kColEasy;
        case BotDifficulty::MEDIUM: return kColMedium;
        case BotDifficulty::HARD: return kColHard;
        case BotDifficulty::IMPOSSIBLE: return kColImpossible;
        default: return sf::Color(255, 215, 0);
    }
}
