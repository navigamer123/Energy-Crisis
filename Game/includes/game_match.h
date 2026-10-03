#pragma once
#include <cstdint>
#include "game_weather.h"

// =============================================================================
// ENERGY CRISIS - MATCH OPTIONS & PROGRESSION            [team b-options]
//
//   F-03  MatchRules: match length, win threshold and pacing presets
//   F-24  Mutators: toggleable match modifiers (bit mask, at most 3 at once)
//   F-35  Starting charters: asymmetric per-player perks
//   F-33  Research lab: 3 branches x 3 tiers, "pick 1 of 2" per tier, paid in money
//   F-21  Practice sandbox: infinite resources, clock / weather / season / demand control
//
// Integrator note: Wave A adds MatchConfig and PlayerModifiers on another branch. The names
// here differ on purpose (MatchRules, PlayerPerks) so both branches compile side by side;
// MatchRules uses the same field names as MatchConfig where they overlap (finalDay,
// victoryShare, daySeconds, graceDays, seed, sandbox) so folding one into the other is a rename.
// Every engine edit that reads these values is marked "[b-options]" in game_main.cpp.
// =============================================================================

// -----------------------------------------------------------------------------
// F-03 Pacing presets
// -----------------------------------------------------------------------------
enum class MatchPreset {
    BLITZ = 0,   // ~10 minutes: 45 s days, 1 grace day, 10 days, 75% wins
    STANDARD,    // README rules: 90 s days, 2 grace days, 20 days, 85% wins
    MARATHON,    // long game: 120 s days, 3 grace days, 35 days, 90% wins
    ENDLESS,     // no day limit: only dominance ends the match
    CUSTOM,      // values edited in the Match Setup screen
    COUNT
};

// -----------------------------------------------------------------------------
// F-24 Mutators (bit flags in MatchRules::mutators)
// -----------------------------------------------------------------------------
enum MutatorFlag : std::uint32_t {
    MUT_NONE           = 0u,
    MUT_ETERNAL_WINTER = 1u << 0, // season locked to winter (short days, snow)
    MUT_MIRROR_WEATHER = 1u << 1, // both sectors always share the same weather
    MUT_RICH_VEINS     = 1u << 2, // mining yields x2
    MUT_NO_GRACE       = 1u << 3, // the city demands power from day 1
    MUT_BUILDING_BOOM  = 1u << 4, // all buildings cost 40% less
    MUT_VOLATILE_CITY  = 1u << 5, // daily territory shift x2
    MUT_HUNGRY_CITY    = 1u << 6, // city demand grows twice as fast
    MUT_HEAD_START     = 1u << 7  // both players start with a resource stockpile
};
constexpr int MUTATOR_COUNT = 8;
constexpr int MAX_ACTIVE_MUTATORS = 3;

// -----------------------------------------------------------------------------
// F-35 Starting charters (one per player)
// -----------------------------------------------------------------------------
enum class CharterType {
    NONE = 0,
    SOLAR_COOP,        // Соларен кооператив
    HYDRO_HOLDING,     // Хидро холдинг
    MINING_SYNDICATE,  // Минен синдикат
    CITY_INSIDER,      // Градски инсайдер
    NIGHT_SHIFT,       // Нощна смяна
    COUNT
};

// -----------------------------------------------------------------------------
// MatchRules: everything chosen before a match (survives restartGame())
// -----------------------------------------------------------------------------
struct MatchRules {
    MatchPreset preset = MatchPreset::STANDARD;
    float daySeconds = 90.0f;      // real seconds per in-game day at 1x speed
    int graceDays = 2;             // days during which the city demands 0 MW
    int finalDay = 20;             // last day of the match; 0 = endless (dominance only)
    int startDemandMW = 30;        // demand on the first day after the grace period
    int demandGrowthMW = 15;       // extra demand per following day
    float victoryShare = 0.85f;    // city share that wins at once
    float miningMult = 1.0f;       // multiplies every mining yield (Blitz compensates short days)
    std::uint32_t mutators = 0u;   // MutatorFlag bits
    CharterType charter[2] = { CharterType::NONE, CharterType::NONE }; // [0] = P1, [1] = P2
    bool sandbox = false;          // F-21 practice mode: P2 idle, infinite resources, never ends
    std::uint32_t seed = 0u;       // 0 = EC_SEED or the clock

    static MatchRules fromPreset(MatchPreset p); // keeps mutators/charters at their defaults
    void applyPreset(MatchPreset p);             // changes only the pacing values
    void validate();                             // clamps every value into its legal range

    bool hasMutator(std::uint32_t flag) const { return (mutators & flag) != 0u; }
    int activeMutatorCount() const;
    bool toggleMutator(std::uint32_t flag);      // false when a 4th mutator would be enabled

    int effectiveGraceDays() const;              // MUT_NO_GRACE -> 0
    int effectiveDemandGrowthMW() const;         // MUT_HUNGRY_CITY -> x2
    float effectiveShiftMult() const;            // MUT_VOLATILE_CITY -> x2
    CharterType charterOf(int player) const { return charter[(player == 2) ? 1 : 0]; }
};

// Legal ranges used by validate() and by the Custom editor in the Match Setup screen
namespace MatchLimits {
constexpr float DAY_SECONDS_MIN = 30.0f, DAY_SECONDS_MAX = 240.0f;
constexpr int GRACE_MIN = 0, GRACE_MAX = 5;
constexpr int FINAL_DAY_MIN = 5, FINAL_DAY_MAX = 60;   // or 0 = endless
constexpr int DEMAND_START_MIN = 10, DEMAND_START_MAX = 150;
constexpr int DEMAND_GROWTH_MIN = 0, DEMAND_GROWTH_MAX = 60;
constexpr float VICTORY_MIN = 0.60f, VICTORY_MAX = 1.00f;
constexpr float MINING_MIN = 0.5f, MINING_MAX = 3.0f;
} // namespace MatchLimits

// -----------------------------------------------------------------------------
// PlayerPerks: combined multipliers from charter (F-35), research (F-33) and mutators (F-24)
// -----------------------------------------------------------------------------
struct PlayerPerks {
    float solarOutputMult = 1.0f;
    float windOutputMult = 1.0f;
    float hydroOutputMult = 1.0f;
    float buildCostMult[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f }; // index = (int)BuildingType
    float batteryCapacityMult = 1.0f;
    float batteryPowerMult = 1.0f;
    float lampDrawMult = 1.0f;
    float lampRadiusMult = 1.0f;
    float miningYieldMult = 1.0f;   // charter x research x mutator x MatchRules::miningMult
    float miningCooldownMult = 1.0f;
    float incomeMult = 1.0f;        // city money payout
    float landCostMult = 1.0f;
    float mineUpgradeCostMult = 1.0f;
};

// -----------------------------------------------------------------------------
// F-33 Research lab: 3 branches x 3 tiers, each tier offers 2 options and you keep one
// -----------------------------------------------------------------------------
constexpr int TECH_BRANCHES = 3;   // 0 = Generation, 1 = Storage & night, 2 = Extraction
constexpr int TECH_TIERS = 3;
constexpr int TECH_OPTIONS = 2;

struct TechState {
    // -1 = tier not researched yet, 0/1 = chosen option (permanent)
    signed char choice[TECH_BRANCHES][TECH_TIERS] = { { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } };
};

enum class TechStatus {
    RESEARCHED,   // this option was bought
    EXCLUDED,     // the other option of this tier was bought
    AVAILABLE,    // previous tier done and enough money
    UNAFFORDABLE, // previous tier done, not enough money
    LOCKED        // the previous tier of this branch is not researched yet
};

struct TechInfo {
    const char* name;    // Bulgarian, short (fits a research card)
    const char* effect;  // Bulgarian, one line
};

// -----------------------------------------------------------------------------
// F-21 Practice sandbox runtime state (reset with every match)
// -----------------------------------------------------------------------------
struct SandboxState {
    float clockSpeed = 1.0f;          // 0 = clock paused
    int seasonOverride = -1;          // -1 = natural seasons, else (int)SeasonType
    bool weatherLocked[2] = { false, false };
    WeatherType lockedWeather[2] = { WeatherType::SUNNY, WeatherType::SUNNY };
    bool demandLocked = false;
    int lockedDemandMW = 0;
};

namespace MatchInfo {
// Display names / one-line descriptions (Bulgarian, player-facing)
const char* presetName(MatchPreset p);
const char* presetDescription(MatchPreset p);
std::uint32_t mutatorFlag(int index);              // index 0..MUTATOR_COUNT-1 -> flag
const char* mutatorName(std::uint32_t flag);
const char* mutatorDescription(std::uint32_t flag);
const char* charterName(CharterType c);
const char* charterShortName(CharterType c);
const char* charterDescription(CharterType c);     // bonus line
const char* charterDrawback(CharterType c);        // drawback line ("" for NONE)
const char* techBranchName(int branch);
const TechInfo& techInfo(int branch, int tier, int option);
int techTierCost(int tier);                        // money ($): 4 000 / 15 000 / 40 000

// Perks of one player from the match rules and that player's research
PlayerPerks computePlayerPerks(const MatchRules& rules, int player, const TechState& tech);
} // namespace MatchInfo
