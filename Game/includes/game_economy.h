#pragma once
// =============================================================================
// ENERGY CRISIS - CITY ECONOMY SIMULATION                     [team b-economy]
//
//  BAL-03  Hourly demand curve: low night, morning ramp, evening peak, peak pricing
//  F-36    Four city districts with their own hourly demand profiles
//  BAL-02  Proportional daily verdict (served fraction + supply share, max 12 %/day)
//  BAL-04  Anti-snowball: territory is load (quota = D x (0.5 + share)), leader damping
//          above 70 %, optional dominance levy / underdog subsidy
//  F-37    Grid frequency: hourly stability check, brownout (x0.7) and blackout (0 MW)
//  F-11    Energy-mix and CO2 ledger (MWh by source, CO2 avoided)
//  UX-01   Settlement forecast and Day Report data
//
// Pure engine code (no SFML drawing). GameEngine owns one CityEconomy and calls the
// hooks below from game_main.cpp; the GameEngine accessors live in game_economy.cpp.
// Must stay compatible with the headless gcc 6 test build (no C++17-only syntax).
// =============================================================================
#include <string>

namespace Econ {

// -----------------------------------------------------------------------------
// Districts (F-36). Allocation order = index order: a player's power first serves
// the hospital, then industry, then the business district and last the homes.
// -----------------------------------------------------------------------------
constexpr int DISTRICT_COUNT = 4;
enum DistrictId {
    DISTRICT_HOSPITAL = 0, // Болница: flat 24/7 critical load (hydro's job)
    DISTRICT_INDUSTRY = 1, // Индустрия: steady base load, slightly lower at night
    DISTRICT_BUSINESS = 2, // Бизнес: office hours 08-17 (solar's job)
    DISTRICT_HOMES = 3     // Домове: evening peak 17-22 (batteries and wind)
};

struct DistrictDef {
    const char* nameBg;  // short upper-case name for the HUD
    const char* roleBg;  // one-line description of the load
    float weight;        // share of the city demand (weights sum to 1)
};

const DistrictDef& getDistrictDef(int district);
// Hourly profile of one district, normalised to a daily mean of exactly 1.0 (hour 0..23)
float getDistrictProfile(int district, int hour);
float getDistrictProfileAt(int district, float hour24);

// -----------------------------------------------------------------------------
// City demand curve (BAL-03): weighted sum of the district profiles, mean 1.0.
// About x0.67 at night (00-06), x1.0 by day and x1.4 in the evening peak (17-22).
// -----------------------------------------------------------------------------
float getCityDemandProfile(int hour);
float getCityDemandProfileAt(float hour24);
// Peak pricing: the city's per-MW payout scales with the demand curve
float getPeakPriceFactor(float hour24);
constexpr float PEAK_PROFILE_THRESHOLD = 1.25f; // profile at or above this is the "evening peak"
bool isPeakHour(float hour24);

// -----------------------------------------------------------------------------
// Daily verdict (BAL-02) and anti-snowball (BAL-04)
//   served_i  = energy that counted against the player's quota / quota energy (0..1)
//   supply_1  = P1 share of all energy delivered to the city today (pulled towards 50 % while the
//               two players together delivered less than the city's daily demand)
//   shift(P1) = clamp(0.08 x (served_1 - served_2) + 0.05 x (2 x supply_1 - 1), -0.12, +0.12)
//   quota_i   = demand x (0.5 + share_i): the more of the city you own, the more you must power
//   gains that push the leader above 70 % count only half
// -----------------------------------------------------------------------------
constexpr float VERDICT_SERVED_WEIGHT = 0.08f;
constexpr float VERDICT_SUPPLY_WEIGHT = 0.05f;
constexpr float VERDICT_MAX_SHIFT = 0.12f;
constexpr float QUOTA_BASE = 0.5f;
constexpr float DAMPING_START_SHARE = 0.70f;
constexpr float DAMPING_FACTOR = 0.5f;

// Optional rule (off by default; Match Setup toggle / EC_UNDERDOG_AID=1)
constexpr float LEVY_ABOVE_SHARE = 0.65f;    // leader above 65 %: city payout x0.85
constexpr float LEVY_PAYOUT_FACTOR = 0.85f;
constexpr float SUBSIDY_BELOW_SHARE = 0.40f; // underdog below 40 %: buildings cost x0.85
constexpr float SUBSIDY_COST_FACTOR = 0.85f;

float quotaFactor(float playerShare);                                  // 0.5 + share
float computeVerdictShift(float served1, float served2, float supplyShare1);
float applyLeaderDamping(float p1Share, float p1Shift);                // damped P1 shift

// -----------------------------------------------------------------------------
// Grid frequency (F-37). Once per game-hour every player's grid is checked:
//   drop       = generation lost since the last check
//   uncovered  = part of the drop that pushes the player below its load, minus the
//                battery power still available (fast reserve)
//   inertia    = hydro share of the generation (synchronous machines resist the change)
//   delta Hz   = 2.5 x uncovered / load / (1 + 2 x inertia)
// Below 49.2 Hz: brownout (output x0.7 for 1 h). Below 48.0 Hz: blackout (0 MW, lamps dark).
// No events during the grace period (the city asks for 0 MW).
// -----------------------------------------------------------------------------
constexpr float NOMINAL_HZ = 50.0f;
constexpr float BROWNOUT_HZ = 49.2f;
constexpr float BLACKOUT_HZ = 48.0f;
constexpr float BROWNOUT_FACTOR = 0.7f;
constexpr float FREQ_DROP_SCALE_HZ = 2.5f;
constexpr float HYDRO_INERTIA_BONUS = 2.0f;
constexpr float GRID_EVENT_HOURS = 1.0f;
constexpr float FREQ_RECOVERY_HZ_PER_HOUR = 1.5f;
constexpr float FREQ_SAG_HZ = 0.35f;     // steady sag of a grid that cannot meet its load
constexpr float FREQ_RISE_HZ = 0.08f;    // steady rise of a grid with a big surplus

enum GridState { GRID_NORMAL = 0, GRID_BROWNOUT = 1, GRID_BLACKOUT = 2 };

// -----------------------------------------------------------------------------
// Energy mix and CO2 ledger (F-11)
// Approximate CO2 intensity of the Bulgarian grid mix that clean energy displaces
// (Ember / ESO data put it at roughly 0.35-0.45 t CO2 per MWh; value to be re-verified).
// -----------------------------------------------------------------------------
constexpr float GRID_CO2_T_PER_MWH = 0.40f;

enum Source { SRC_SOLAR = 0, SRC_WIND = 1, SRC_HYDRO = 2, SRC_BATTERY = 3, SRC_COUNT = 4 };
const char* getSourceNameBg(int source);

// -----------------------------------------------------------------------------
// State
// -----------------------------------------------------------------------------
// One simulation step of one player's grid, filled by GameEngine::updateBuildingsEnergy
struct GridSample {
    float solarMW = 0.0f;
    float windMW = 0.0f;
    float hydroMW = 0.0f;
    float batteryOutMW = 0.0f;   // battery discharge this step
    float batteryReadyMW = 0.0f; // discharge power the batteries could still add (fast reserve)
    float deliveredMW = 0.0f;    // power that reached the city (after lamps and grid events)
    float loadTargetMW = 0.0f;   // own lamps + city quota
};

struct PlayerGrid {
    float frequencyHz = NOMINAL_HZ;
    float lastCheckGenMW = -1.0f; // generation at the previous hourly check (-1 = none yet)
    long lastCheckHour = -1;      // absolute game-hour index of the previous check
    int state = GRID_NORMAL;
    float stateHoursLeft = 0.0f;
    int brownoutsToday = 0;
    int blackoutsToday = 0;
    int brownoutsTotal = 0;
    int blackoutsTotal = 0;
    float lowestHzToday = NOMINAL_HZ;
    float lastDropMW = 0.0f;      // last hourly check: generation lost
    float lastUncoveredMW = 0.0f; // last hourly check: part of it nobody covered
};

struct PlayerLedger {
    double generatedMWh[SRC_COUNT] = { 0.0, 0.0, 0.0, 0.0 }; // battery = discharged energy
    double deliveredMWh = 0.0;       // whole match, energy that reached the city
    double servedMWh = 0.0;          // whole match, energy that counted against the quota (tie-break)
    double deliveredTodayMWh = 0.0;
    double servedTodayMWh = 0.0;
};

struct DistrictState {
    float p1Share = 0.5f;                     // P1 share of this district
    float quotaEnergy[2] = { 0.0f, 0.0f };    // today, MW x game-seconds
    float servedEnergy[2] = { 0.0f, 0.0f };   // today, MW x game-seconds (min(allocation, quota))
    float demandNowMW = 0.0f;                 // district demand right now
    float quotaNowMW[2] = { 0.0f, 0.0f };     // each player's quota in this district right now
    float allocNowMW[2] = { 0.0f, 0.0f };     // each player's power serving this district right now
    float lastShift = 0.0f;                   // P1 shift at the last settlement
};

struct DayReport {
    bool valid = false;
    int day = 0;
    int demandMW = 0;                         // base demand of the settled day
    float shareBefore = 0.5f;
    float shareAfter = 0.5f;
    float rawShift = 0.0f;                    // P1, before damping
    float appliedShift = 0.0f;                // P1, what really moved
    bool damped = false;
    float served[2] = { 0.0f, 0.0f };         // 0..1
    float supplyShare1 = 0.5f;
    float deliveredMWh[2] = { 0.0f, 0.0f };
    float co2AvoidedT[2] = { 0.0f, 0.0f };
    float districtServed[DISTRICT_COUNT][2] = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };
    float districtShift[DISTRICT_COUNT] = { 0.0f, 0.0f, 0.0f, 0.0f };
    int brownouts[2] = { 0, 0 };
    int blackouts[2] = { 0, 0 };
};

// Live projection of the coming settlement (UX-01)
struct Forecast {
    bool active = false;                  // false during the grace period
    float served[2] = { 0.0f, 0.0f };     // served fraction so far today (0..1)
    float nowMW[2] = { 0.0f, 0.0f };      // power delivered right now
    float quotaNowMW[2] = { 0.0f, 0.0f }; // quota right now
    float projectedShift = 0.0f;          // P1 shift if the day ended now (after damping)
    float secondsToSettlement = 0.0f;     // game-seconds until 06:00
};

struct Rules {
    bool levyAndSubsidy = false;          // BAL-04 optional dominance levy + underdog subsidy
};

struct CityEconomy {
    DistrictState districts[DISTRICT_COUNT];
    PlayerGrid grid[2];
    PlayerLedger ledger[2];
    float deliveredEnergyToday[2] = { 0.0f, 0.0f }; // MW x game-seconds, uncapped
    float demandEnergyToday = 0.0f;                  // MW x game-seconds of D x profile
    float lastDeliveredMW[2] = { 0.0f, 0.0f };
    DayReport lastReport;
    Rules rules;
};

// Result of judging the day so far (used by the settlement and by the forecast)
struct Settlement {
    float served[2] = { 0.0f, 0.0f };
    float supplyShare1 = 0.5f;
    float districtServed[DISTRICT_COUNT][2] = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };
    float districtShift[DISTRICT_COUNT] = { 0.0f, 0.0f, 0.0f, 0.0f }; // after damping
    float rawShift = 0.0f;     // city-level P1 shift before damping
    float appliedShift = 0.0f; // city-level P1 shift after damping and clamping
    bool damped = false;
};

// -----------------------------------------------------------------------------
// Hooks used by GameEngine
// -----------------------------------------------------------------------------
// City share implied by the district shares (weighted sum)
float cityShareFromDistricts(const CityEconomy& ce);
// Moves every district by the same amount so their weighted sum equals p1CityShare again
// (keeps districts in sync when another system changed the city share directly)
void syncDistrictsToCityShare(CityEconomy& ce, float p1CityShare);

// Quota of one player right now: sum over districts of D x w x profile x (0.5 + share)
float playerQuotaMW(const CityEconomy& ce, int player, int demandMW, float hour24);

// Output multiplier of the player's grid right now (1, 0.7 brownout, 0 blackout)
float gridOutputFactor(const CityEconomy& ce, int player);

// Integrates one simulation step: district allocation, served energy, ledger, frequency
void onSimStep(CityEconomy& ce, const GridSample samples[2], int demandMW, float hour24,
               float gameSeconds, float dt);

// Judges the day so far without changing anything
Settlement evaluateDay(const CityEconomy& ce, float p1CityShare);

// Settles the ended day: moves the districts and the city share, writes the Day Report and a
// Bulgarian result message. Resets the daily counters. Returns the applied P1 shift.
float settleDay(CityEconomy& ce, int endedDay, int demandMW, float& p1CityShare, std::string& outMessage);

// Grace-period day end: only resets the daily counters (no verdict)
void resetDay(CityEconomy& ce);

// Day-20 tie-break: 1 or 2 = more served MWh over the whole match, 0 = still equal
int tieBreakWinner(const CityEconomy& ce);

// Optional rule helpers
float payoutFactor(const CityEconomy& ce, int player, float p1CityShare);   // levy
bool hasUnderdogSubsidy(const CityEconomy& ce, int player, float p1CityShare);
bool hasDominanceLevy(const CityEconomy& ce, int player, float p1CityShare);

// Pretty helpers for the HUD and messages
std::string formatPercentSigned(float fraction); // +6% / -3% / 0%
double co2AvoidedT(const PlayerLedger& ledger);

} // namespace Econ
