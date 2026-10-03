#ifndef UI_BOTPROFILES_H
#define UI_BOTPROFILES_H

// =============================================================================
// [AI team] Data-driven bot profiles (F-17 personalities, BAL-01 governor, НЕВЪЗМОЖНО)
//
// A BotProfile = the difficulty's SKILL (speed, reactions, governor margin, slips, engine edge)
// + the rival's STYLE (building scores, weather bonuses, battery/lamp caps, upgrade priority,
// expansion threshold). The tables live in UI/scr/UI_botProfiles.cpp; UI/scr/UI_bot.cpp only
// reads fields, so a new rival or a retuned difficulty is a data change.
// =============================================================================

#include <SFML/Graphics.hpp>
#include <string>
#include "UI_types.h"
#include "../../Game/includes/game_main.h"

// How the governor measures its own supply against the city demand
enum class BotSupplyView {
    NAMEPLATE, // sum of the "+MW" printed on its buildings (blind to night, weather and lamps)
    INSTANT,   // MW delivered right now (fooled by the noon sun, blind to the night)
    PROJECTED, // projected average of the day being settled with today's weather
    WORST_CASE // projected average + tomorrow under the worst possible weather (perfect foresight)
};

struct BotProfile {
    // ---- Identity -----------------------------------------------------------
    int personalityId = 0;             // 0..BOT_PERSONALITY_COUNT-1, BOT_PERSONALITY_OMEGA for НЕВЪЗМОЖНО
    std::string nameBg;                // shown on the P2 tag, the rival picker and the intro banner
    std::string taglineBg;             // one-line motto
    std::string styleBg;               // how it plays (and its learnable weakness)
    std::string signatureBg;           // two-word play style chip in the rival picker
    sf::Color accent = sf::Color(255, 215, 0);

    // ---- Skill (difficulty) -------------------------------------------------
    float moveSpeed = 420.0f;          // px per real second
    float decisionInterval = 0.35f;    // real seconds of "thinking" after an action
    float mineHitInterval = 1.05f;     // real seconds between mining hits
    float afterMineThink = 0.15f;      // pause after a mining quota is met
    float actionCooldown = 0.15f;      // UI debounce between two actions
    float slipChance = 0.0f;           // chance per plan to waste the decision (blunder)
    float demandMargin = 1.2f;         // governor: start generators only while supply < margin x demand
    float floorMW = 30.0f;             // ... or while supply < floorMW (grace-period income)
    BotSupplyView supplyView = BotSupplyView::PROJECTED;
    float lookaheadHours = 6.0f;       // prepares for tomorrow's demand this many game-hours before 06:00
    int maxMineLevel = 3;              // highest mine level it upgrades to
    int upgradeChancePct = 50;         // chance to take an affordable upgrade
    int expandChancePct = 100;         // chance to buy land when the threshold is reached
    PlayerModifiers engineEdge;        // engine-side advantages (only НЕВЪЗМОЖНО)

    // ---- Style (personality) ------------------------------------------------
    float hydroScore = 105.0f;
    float windScore = 100.0f;
    float solarScore = 80.0f;          // daytime only (never at night)
    float rainHydroBonus = 45.0f;
    float windyWindBonus = 60.0f;
    float stormWindBonus = 90.0f;
    float nightWindBonus = 35.0f;
    bool neverSolar = false;
    int batteryCap = 2;
    int batteryMinMW = 95;             // builds a battery only above this output
    float batteryScore = 65.0f;
    int lampCap = 1;
    bool preferRiverPlots = false;     // buys the river-bank column first (hydro)
    int expandFreeSlots = 2;           // buys land when this many free slots or fewer are left
    ResourceType upgradeOrder[4] = { ResourceType::WOOD, ResourceType::IRON, ResourceType::COPPER, ResourceType::SILICON };
    int graceMineTarget = 0;           // grace period: mines gold to upgrade the first mine to this level
    float marginScale = 1.0f;          // multiplies demandMargin before surgeDay
    int surgeDay = 0;                  // from this day on marginScale = surgeMarginScale (0 = never)
    float surgeMarginScale = 1.0f;
    int stockpileQuota[5] = { 25, 25, 18, 12, 10 }; // wood, iron, copper, silicon, coal kept in stock
};

constexpr int BOT_PERSONALITY_COUNT = 5;
constexpr int BOT_PERSONALITY_RANDOM = -1; // menu choice "СЛУЧАЕН"
constexpr int BOT_PERSONALITY_OMEGA = 99;  // НЕВЪЗМОЖНО's own profile

// Full profile for a difficulty and a rival (НЕВЪЗМОЖНО ignores the rival and plays ОМЕГА)
BotProfile makeBotProfile(BotDifficulty difficulty, int personalityId);

// Rival identity only (picker cards)
const BotProfile& botPersonality(int personalityId);

// Display helpers
const char* botDifficultyNameBg(BotDifficulty difficulty);     // "ЛЕСНО", ..., "НЕВЪЗМОЖНО"
const char* botDifficultyTagBg(BotDifficulty difficulty);      // "ЛЕСЕН", ..., "НЕВЪЗМОЖЕН"
sf::Color botDifficultyColor(BotDifficulty difficulty);        // НЕВЪЗМОЖНО = red

#endif // UI_BOTPROFILES_H
