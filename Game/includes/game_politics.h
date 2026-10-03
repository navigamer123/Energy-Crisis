#ifndef GAME_POLITICS_H
#define GAME_POLITICS_H

// =============================================================================
// ENERGY CRISIS - CITY POLITICS  [team b-politics]
// Data and tuning for four connected city systems that live inside GameEngine:
//   F-09  City Event Deck: one active event + a one-day forecast, seasonal festivals
//   F-13  Council Decisions: timed cards with up to 3 money options for each player
//   F-16  City Contract Board: daily contracts, races and sealed-bid tenders
//   F-31  Commodity Exchange and emergency cross-river power import
// The engine owns one CityPolitics value (GameEngine::politics). The logic is in
// game_politics.cpp (events, council, buffs), game_contracts.cpp and game_market.cpp.
// Gcc 6 compatible on purpose (headless tests): no std::optional, no structured bindings.
// =============================================================================

#include <random>
#include <string>
#include <vector>

namespace Politics {

// -----------------------------------------------------------------------------
// Tuning
// -----------------------------------------------------------------------------
constexpr int FIRST_EVENT_DAY = 4;          // Random city events can start on this day
constexpr int EVENT_CHANCE_PCT = 40;        // Daily chance of a random event (rolled one day ahead)
constexpr int FIRST_COUNCIL_DAY = 4;        // Council cards can start on this day
constexpr int COUNCIL_CHANCE_PCT = 25;      // Daily chance of a council card
constexpr float COUNCIL_DECISION_SEC = 20.0f; // Real seconds players get to decide
constexpr float COUNCIL_EARLIEST_HOUR = 8.5f; // A council card appears between these hours
constexpr float COUNCIL_LATEST_HOUR = 14.0f;
constexpr int FIRST_CONTRACT_DAY = 3;       // The contract board opens with the city demand
constexpr int FIRST_TENDER_DAY = 4;         // Sealed-bid tenders start here
constexpr int TENDER_CHANCE_PCT = 60;       // Daily chance of a tender
constexpr float TENDER_BID_CLOSE_HOUR = 12.0f; // Sealed bids close at 12:00
constexpr int IMPORT_MAX_MW = 60;           // Cross-river emergency import cap

// Money prices grow with the match: city income grows with the grid, so do the stakes.
// Day 4 = 1.0x, day 8 = 2.0x, day 12 = 3.0x, day 20 = 5.0x
inline float priceScale(int day) {
    return 1.0f + 0.25f * static_cast<float>(day > 4 ? day - 4 : 0);
}

// -----------------------------------------------------------------------------
// F-09 City events
// -----------------------------------------------------------------------------
enum class EventId {
    NONE = 0,
    HEATWAVE,
    COLD_SNAP,
    DROUGHT,
    SUBSIDY,
    PROTEST,
    MINER_STRIKE,
    GOLD_RUSH,
    GRID_REPAIR,
    NEIGHBOUR_BLACKOUT,
    FESTIVAL_SPRING,
    FESTIVAL_SUMMER,
    FESTIVAL_AUTUMN,
    FESTIVAL_WINTER,
    COUNT
};

// Tone drives the chip colour in the UI
enum class Tone { NEUTRAL = 0, GOOD, BAD, FESTIVAL };

struct EventDef {
    EventId id;
    const char* nameBg;    // "ГОРЕЩА ВЪЛНА"
    const char* effectBg;  // short effect line for the chip
    const char* descBg;    // one sentence for notices
    Tone tone;
    float demandMult;      // city demand
    float solarMult;
    float windMult;
    float hydroMult;
    float payoutMult;      // money paid by the city
    float goldMult;        // gold dividend and gold mining
    float mineMult;        // every other mine
};

const EventDef& getEventDef(EventId id);
// Fixed festival of a season (one per season, known from the start)
int getFestivalDay(int seasonIndex);            // 0 = spring .. 3 = winter
EventId getFestivalOnDay(int day);              // EventId::NONE when that day has no festival

// -----------------------------------------------------------------------------
// Player buffs (council results, auctions): timed multipliers on one player
// -----------------------------------------------------------------------------
enum class BuffKind { PAYOUT = 0, MINE, SOLAR, WIND_HYDRO };

struct PlayerBuff {
    int player = 1;
    BuffKind kind = BuffKind::PAYOUT;
    float mult = 1.0f;
    float secondsLeft = 0.0f; // game seconds
    std::string labelBg;
};

struct PendingPayment {     // green bonds
    int player = 1;
    int money = 0;
    float secondsLeft = 0.0f;
};

// -----------------------------------------------------------------------------
// F-13 Council decisions
// -----------------------------------------------------------------------------
enum class CouncilEffect {
    NONE = 0,
    SHARE,          // magnitude = city share in tenths of a percent (+10 = +1%)
    PAYOUT_BUFF,    // magnitude = percent, days = duration
    MINE_BUFF,
    SOLAR_BUFF,
    WIND_HYDRO_BUFF,
    BOND,           // magnitude = money paid back after `days`
    GOLD,           // magnitude = gold granted now
    AUCTION_BID     // magnitude = bid rank; the highest bid wins the card's prize
};

struct CouncilOption {
    const char* labelBg;
    const char* effectBg;
    int baseCost;           // money, multiplied by priceScale(day)
    CouncilEffect effect;
    int magnitude;
    int days;
};

struct CouncilCardDef {
    int id;
    const char* titleBg;
    const char* descBg;
    int optionCount;
    int defaultOption;      // used when time runs out or the player cannot pay
    CouncilOption options[3];
};

int getCouncilCardCount();
const CouncilCardDef& getCouncilCard(int id);

struct CouncilState {
    bool scheduled = false;     // a card will appear today
    float appearHour = 12.0f;
    bool active = false;
    int cardId = 0;
    int day = 0;
    float timeLeft = 0.0f;      // real seconds
    float elapsed = 0.0f;       // real seconds since it appeared
    int choice[2] = { -1, -1 }; // -1 = undecided
    int cost[3] = { 0, 0, 0 };  // option costs for today (priceScale applied)
    // Last resolution, kept for the UI result line
    bool hasResult = false;
    int resultCardId = 0;
    int resultChoice[2] = { -1, -1 };
    std::string resultBg[2];
    float resultTimer = 0.0f;   // real seconds the result stays visible
};

// -----------------------------------------------------------------------------
// F-16 City contracts
// -----------------------------------------------------------------------------
enum class ContractKind {
    EVENING_PEAK = 0, // average MW delivered 18:00-22:00
    MORNING_PEAK,     // average MW delivered 06:00-09:00
    STORAGE_SUNSET,   // MWh stored in batteries at sunset
    LAMPS_22,         // powered lamps at 22:00
    RECORD_RACE,      // first to deliver X MW at once, 06:00-18:00 (exclusive)
    TENDER_NIGHT,     // sealed bids until 12:00, winner must average X MW 22:00-04:00
    COUNT
};

enum class ContractState { OPEN = 0, BIDDING, ACTIVE, DONE, EXPIRED };

struct CityContract {
    int id = 0;
    ContractKind kind = ContractKind::EVENING_PEAK;
    ContractState state = ContractState::OPEN;
    bool festival = false;      // posted by a festival (bigger reward)
    int target = 0;             // MW, MWh or lamps
    int rewardGold = 0;
    int rewardShareTenths = 0;  // +10 = +1% city share
    bool completed[2] = { false, false };
    bool failed[2] = { false, false };
    float sum[2] = { 0.0f, 0.0f };     // MW x game-seconds inside the window
    float seconds = 0.0f;              // game-seconds of the window seen so far
    float best[2] = { 0.0f, 0.0f };    // best value seen (UI progress)
    // Tender
    int bid[2] = { 0, 0 };             // money held in escrow
    float bidTime[2] = { 0.0f, 0.0f }; // day-hour of the latest raise (earlier bid wins a tie)
    int tenderWinner = 0;              // 0 = none yet
    int paidBid = 0;
};

const char* getContractTitle(ContractKind kind);
// Window in hours after 06:00 (0..24). For point checks start == end.
void getContractWindow(ContractKind kind, int seasonIndex, float& startDayHour, float& endDayHour);

// -----------------------------------------------------------------------------
// F-31 Exchange and import
// -----------------------------------------------------------------------------
constexpr int MARKET_SLOTS = 8;  // indexed by ResourceType 1..7 (WOOD..GOLD); 0 unused

struct MarketState {
    float mult[MARKET_SLOTS] = { 1, 1, 1, 1, 1, 1, 1, 1 }; // demand-driven price multipliers (0.5..3)
    int boughtToday[MARKET_SLOTS] = { 0 };
    int soldToday[MARKET_SLOTS] = { 0 };
};

struct ImportState {
    bool request[2] = { false, false };      // player asks to import
    bool exportAllowed[2] = { true, true };  // player lets the rival import from them
    int flowMW[2] = { 0, 0 };                // MW currently imported by each player
    float moneyAccum[2] = { 0.0f, 0.0f };    // fractional money owed by each importer
    int paidToday[2] = { 0, 0 };             // money paid for imports today
    int earnedToday[2] = { 0, 0 };           // money earned from exports today
};

// -----------------------------------------------------------------------------
// Notices for the UI (drained every frame). A small local queue until the engine
// event queue from wave A lands; the integrator can forward these into it.
// -----------------------------------------------------------------------------
struct PoliticsNotice {
    std::string text;
    int player = 0;    // 0 = both / city-wide
    Tone tone = Tone::NEUTRAL;
};

} // namespace Politics

// Everything the engine keeps for the city politics systems
struct CityPolitics {
    std::mt19937 rng;                        // own stream: weather sequences stay as before
    // F-09
    Politics::EventId activeEvent = Politics::EventId::NONE;
    Politics::EventId forecastEvent = Politics::EventId::NONE;
    int baseDemandMW = 0;                    // demand before the event multiplier
    // F-13
    Politics::CouncilState council;
    std::vector<Politics::PlayerBuff> buffs;
    std::vector<Politics::PendingPayment> bonds;
    // F-16
    std::vector<Politics::CityContract> contracts;
    int nextContractId = 1;
    float lastDayHour = 0.0f;                // previous step's hour after 06:00 (window crossing)
    // F-31
    Politics::MarketState market;
    Politics::ImportState imports;
    // UI
    std::vector<Politics::PoliticsNotice> notices;
};

#endif // GAME_POLITICS_H
