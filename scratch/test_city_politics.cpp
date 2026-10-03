// =============================================================================
// ENERGY CRISIS - CITY POLITICS TESTS  [team b-politics]
// F-09 event deck & festivals, F-13 council decisions, F-16 contract board,
// F-31 commodity exchange & cross-river power import, plus the politics bot.
// Headless: built from this file + Game/scr/*.cpp only ("make test").
// Every check is reported; the program exits with 1 when any check failed.
// =============================================================================
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include "../Game/includes/game_main.h"

#ifdef _WIN32
extern "C" __declspec(dllimport) int __cdecl _putenv(const char*);
#endif

using namespace Politics;

namespace {

int g_checks = 0;
int g_failures = 0;
int g_groupFailures = 0;

#define CHECK(cond, details)                                                                   \
    do {                                                                                       \
        ++g_checks;                                                                            \
        if (!(cond)) {                                                                         \
            ++g_failures;                                                                      \
            ++g_groupFailures;                                                                 \
            std::cerr << "    FAIL (line " << __LINE__ << "): " << #cond << " | " << details << "\n"; \
        }                                                                                      \
    } while (0)

#define REQUIRE(cond, details)                                                                 \
    do {                                                                                       \
        ++g_checks;                                                                            \
        if (!(cond)) {                                                                         \
            std::cerr << "    FATAL (line " << __LINE__ << "): " << #cond << " | " << details << "\n"; \
            std::exit(1);                                                                      \
        }                                                                                      \
    } while (0)

void beginGroup(const std::string& name) {
    g_groupFailures = 0;
    std::cout << "\n[" << name << "]\n";
}

void endGroup() { std::cout << (g_groupFailures == 0 ? "  -> PASS\n" : "  -> FAIL\n"); }

void setSeedEnv(const char* value) {
    std::string s = std::string("EC_SEED=") + (value ? value : "");
#ifdef _WIN32
    _putenv(s.c_str());
#else
    if (value) setenv("EC_SEED", value, 1); else unsetenv("EC_SEED");
#endif
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void giveResources(GameEngine& e, int player, int amount) {
    PlayerEconomy& p = e.getPlayerEconomyMut(player);
    p.wood = p.iron = p.copper = p.coal = p.silicon = p.silver = amount;
}

int startPlotId(int player) { return (player == 1) ? 1 : 15; }

sf::Vector2f slotOf(const GameEngine& e, int player, int plotId, int sub) {
    int idx = (plotId - 1) % 12;
    int col = (idx % 3) * 3 + sub % 3;
    int row = (idx / 3) * 3 + sub / 3;
    return e.getGridSlot(player, col, row);
}

void place(GameEngine& e, int player, BuildingType type, sf::Vector2f pos) {
    std::string msg;
    bool ok = e.placeBuilding(player, type, pos, msg);
    REQUIRE(ok, "placeBuilding P" << player << " type " << static_cast<int>(type) << " failed: " << msg);
}

void buyPlot(GameEngine& e, int player, int plotId) {
    auto& econ = e.getPlayerEconomyMut(player);
    int goldBefore = econ.gold;
    econ.gold += 100000;
    std::string msg;
    REQUIRE(e.buyLandPlot(player, plotId, msg), "buyLandPlot failed: " << msg);
    econ.gold = goldBefore;
}

// n wind turbines on the starting plot (>= 6 x 85 x 0.68 MW at the weakest hour)
void buildWindFarm(GameEngine& e, int player, int n = 6) {
    giveResources(e, player, 1000);
    for (int i = 0; i < n; ++i) place(e, player, BuildingType::WIND_TURBINE, slotOf(e, player, startPlotId(player), i));
}

GameEngine* newEngine(bool politicsOn, const char* seed = "777") {
    setSeedEnv(seed);
    GameEngine* e = new GameEngine();
    e->setCityPoliticsEnabled(politicsOn);
    e->init(1600.0f, 900.0f);
    return e;
}

void runToDay(GameEngine& e, int day) {
    while (e.getCurrentDay() < day && e.getCityState().winner == 0) e.update(0.25f);
}

// Advances to the given clock hour of the current day (hours after 06:00 never wrap backwards)
void runToHour(GameEngine& e, float hour24) {
    float target = std::fmod(hour24 - 6.0f + 24.0f, 24.0f);
    int day = e.getCurrentDay();
    while (e.getCurrentDay() == day) {
        float dh = std::fmod(e.getHour24() - 6.0f + 24.0f, 24.0f);
        if (dh >= target) break;
        e.update(0.05f);
    }
}

int expectedBaseDemand(int day) {
    if (day <= Balance::GRACE_PERIOD_DAYS) return 0;
    return Balance::STARTING_CITY_DEMAND_MW + (day - Balance::GRACE_PERIOD_DAYS - 1) * Balance::DAILY_DEMAND_INCREASE_MW;
}

const CityContract* findContract(const GameEngine& e, int id) {
    for (const auto& k : e.getPolitics().contracts) {
        if (k.id == id) return &k;
    }
    return nullptr;
}

// ===========================================================================
// F-09 City Event Deck
// ===========================================================================
void testPoliticsOffByDefault() {
    beginGroup("Politics is off by default: no events, council or contracts");
    setSeedEnv("4242");
    GameEngine e;
    e.init(1600.0f, 900.0f);
    CHECK(!e.isCityPoliticsEnabled(), "engine must default to politics off");
    runToDay(e, 9);
    CHECK(e.getPolitics().activeEvent == EventId::NONE, "an event fired with politics off");
    CHECK(e.getPolitics().contracts.empty(), "contracts posted with politics off");
    CHECK(!e.getPolitics().council.active, "council card with politics off");
    CHECK(e.getCityState().cityEnergyDemand == expectedBaseDemand(9),
          "demand " << e.getCityState().cityEnergyDemand << " != " << expectedBaseDemand(9));
    endGroup();
}

void testForecastBecomesToday() {
    beginGroup("Forecast becomes the active event the next day; festivals on fixed days");
    int eventsSeen = 0;
    for (int s = 0; s < 6; ++s) {
        std::string seed = std::to_string(1000 + s);
        GameEngine* e = newEngine(true, seed.c_str());
        EventId lastForecast = EventId::NONE;
        for (int day = 2; day <= 20; ++day) {
            runToDay(*e, day);
            if (e->getCityState().winner != 0) break;
            const CityPolitics& p = e->getPolitics();
            EventId fest = getFestivalOnDay(day);
            if (fest != EventId::NONE) {
                CHECK(p.activeEvent == fest, "seed " << seed << " day " << day << ": festival missing");
            } else {
                CHECK(p.activeEvent == lastForecast, "seed " << seed << " day " << day << ": active "
                                                             << static_cast<int>(p.activeEvent) << " != forecast "
                                                             << static_cast<int>(lastForecast));
            }
            if (day < FIRST_EVENT_DAY) CHECK(p.activeEvent == EventId::NONE, "event before day 4 on day " << day);
            if (p.activeEvent != EventId::NONE && fest == EventId::NONE) ++eventsSeen;
            // demand = base x event multiplier, base grows +15 MW/day regardless of events
            CHECK(e->getBaseCityDemand() == expectedBaseDemand(day),
                  "base demand " << e->getBaseCityDemand() << " != " << expectedBaseDemand(day) << " (day " << day << ")");
            int want = static_cast<int>(std::lround(expectedBaseDemand(day) * getEventDef(p.activeEvent).demandMult));
            CHECK(e->getCityState().cityEnergyDemand == want,
                  "demand " << e->getCityState().cityEnergyDemand << " != " << want << " (day " << day << ")");
            lastForecast = p.forecastEvent;
        }
        delete e;
    }
    // 6 seeds x ~13 eligible non-festival days x 40% ~ 31 events
    CHECK(eventsSeen >= 12 && eventsSeen <= 55, "random events seen: " << eventsSeen);
    CHECK(getFestivalDay(0) == 4 && getFestivalDay(3) == 19, "festival days moved");
    endGroup();
}

void testEventMultipliers() {
    beginGroup("Event multipliers: drought, strike, gold rush, subsidy, heatwave demand");
    GameEngine* e = newEngine(true, "31");
    runToDay(*e, 4);
    buyPlot(*e, 1, 3); // river-bank plot for a hydro plant
    giveResources(*e, 1, 1000);
    place(*e, 1, BuildingType::HYDRO_PLANT, slotOf(*e, 1, 3, 4));

    e->debugForceCityEvent(EventId::NONE, EventId::NONE);
    e->update(0.1f);
    float hydroNormal = 0.0f;
    for (const auto& b : e->getBuildings()) if (b.type == BuildingType::HYDRO_PLANT) hydroNormal = b.currentOutputMW;
    e->debugForceCityEvent(EventId::DROUGHT, EventId::NONE);
    e->update(0.1f);
    float hydroDrought = 0.0f;
    for (const auto& b : e->getBuildings()) if (b.type == BuildingType::HYDRO_PLANT) hydroDrought = b.currentOutputMW;
    CHECK(hydroNormal > 0.0f && std::fabs(hydroDrought - hydroNormal * 0.5f) < 0.5f,
          "drought hydro " << hydroDrought << " vs normal " << hydroNormal);

    std::string msg;
    GameEngine::MineResult r;
    e->debugForceCityEvent(EventId::MINER_STRIKE, EventId::NONE);
    e->mineResource(1, ResourceType::WOOD, r, msg);
    CHECK(r.wood == static_cast<int>(std::round(Balance::WOOD_BASE_YIELD * 0.6f)), "strike wood " << r.wood);
    e->debugForceCityEvent(EventId::GOLD_RUSH, EventId::NONE);
    e->mineResource(1, ResourceType::GOLD, r, msg);
    CHECK(r.gold == Balance::GOLD_BASE_YIELD * 2, "gold rush gold " << r.gold);

    // Subsidy: +50% money for the same grid (one payout second compared with a neutral one)
    e->debugForceCityEvent(EventId::NONE, EventId::NONE);
    int m0 = e->getPlayerEconomy(1).money;
    e->update(1.0f);
    int neutral = e->getPlayerEconomy(1).money - m0;
    e->debugForceCityEvent(EventId::SUBSIDY, EventId::NONE);
    m0 = e->getPlayerEconomy(1).money;
    e->update(1.0f);
    int subsidy = e->getPlayerEconomy(1).money - m0;
    CHECK(neutral > 0 && std::abs(subsidy - static_cast<int>(std::lround(neutral * 1.5f))) <= 2,
          "subsidy " << subsidy << " vs neutral " << neutral);

    e->debugForceCityEvent(EventId::HEATWAVE, EventId::NONE);
    CHECK(e->getCityState().cityEnergyDemand == static_cast<int>(std::lround(e->getBaseCityDemand() * 1.35f)),
          "heatwave demand " << e->getCityState().cityEnergyDemand);
    int base = e->getBaseCityDemand();
    runToDay(*e, 5);
    CHECK(e->getBaseCityDemand() == base + Balance::DAILY_DEMAND_INCREASE_MW,
          "demand compounded the heatwave: base " << e->getBaseCityDemand());
    delete e;
    endGroup();
}

// The weather generator is one global stream, so each engine runs alone from its own init
std::vector<int> weatherLog(bool politicsOn, int& outEvent, int& outForecast, size_t& outContracts) {
    GameEngine* e = newEngine(politicsOn, "9090");
    std::vector<int> log;
    for (int day = 2; day <= 12; ++day) {
        runToDay(*e, day);
        log.push_back(static_cast<int>(e->getPlayerWeather(1)) * 10 + static_cast<int>(e->getPlayerWeather(2)));
    }
    outEvent = static_cast<int>(e->getPolitics().activeEvent);
    outForecast = static_cast<int>(e->getPolitics().forecastEvent);
    outContracts = e->getPolitics().contracts.size();
    delete e;
    return log;
}

void testWeatherUnaffected() {
    beginGroup("Politics uses its own RNG: weather is identical with politics on or off");
    int evA = 0, fcA = 0, evB = 0, fcB = 0, evC = 0, fcC = 0;
    size_t kA = 0, kB = 0, kC = 0;
    std::vector<int> a = weatherLog(true, evA, fcA, kA);
    std::vector<int> b = weatherLog(false, evB, fcB, kB);
    std::vector<int> c = weatherLog(true, evC, fcC, kC);
    CHECK(a == b, "weather diverged when politics was enabled");
    CHECK(a == c && evA == evC && fcA == fcC && kA == kC, "same seed gave a different politics sequence");
    endGroup();
}

// ===========================================================================
// F-13 Council
// ===========================================================================
void testCouncilDonationAndEscrow() {
    beginGroup("Council: paying options escrow money and apply effects; poor players cannot pick them");
    GameEngine* e = newEngine(true, "55");
    runToDay(*e, 4);
    runToHour(*e, 8.0f);
    e->getPlayerEconomyMut(1).money = 100000;
    e->getPlayerEconomyMut(2).money = 100;
    e->debugStartCouncilCard(2); // hospital donation
    const CouncilState& c = e->getPolitics().council;
    REQUIRE(c.active, "card did not open");
    float share0 = e->getCityState().p1CityShare;
    std::string msg;
    CHECK(e->chooseCouncilOption(1, 0, msg), "P1 big donation refused: " << msg);
    CHECK(e->getPlayerEconomy(1).money == 100000 - c.cost[0], "escrow missing");
    CHECK(!e->chooseCouncilOption(1, 1, msg), "P1 voted twice");
    CHECK(!e->chooseCouncilOption(2, 0, msg), "P2 picked an option it cannot pay");
    CHECK(e->getPlayerEconomy(2).money == 100, "failed choice changed money");
    CHECK(e->chooseCouncilOption(2, 2, msg), "P2 decline refused");
    e->update(0.016f); // both decided -> resolves at once
    CHECK(!c.active && c.hasResult, "card not resolved after both votes");
    CHECK(std::fabs(e->getCityState().p1CityShare - (share0 + 0.03f)) < 1e-4f,
          "share " << e->getCityState().p1CityShare << " expected " << share0 + 0.03f);
    delete e;
    endGroup();
}

void testCouncilTimeoutDefaults() {
    beginGroup("Council: undecided players get the free default after 20 real seconds");
    GameEngine* e = newEngine(true, "56");
    runToDay(*e, 5);
    runToHour(*e, 8.0f);
    e->getPlayerEconomyMut(1).money = 50000;
    e->getPlayerEconomyMut(2).money = 50000;
    e->debugStartCouncilCard(7); // grid inspection: default = decline = -1%
    float share0 = e->getCityState().p1CityShare;
    std::string msg;
    CHECK(e->chooseCouncilOption(1, 0, msg), msg);
    for (int i = 0; i < 19; ++i) e->update(1.0f);
    CHECK(e->getPolitics().council.active, "card closed before 20 s");
    e->update(1.5f);
    CHECK(!e->getPolitics().council.active, "card still open after 20 s");
    // P1 modernised (+20% wind/hydro buff), P2 declined (-1% for P2 = +1% for P1)
    CHECK(std::fabs(e->getCityState().p1CityShare - (share0 + 0.01f)) < 1e-4f, "share " << e->getCityState().p1CityShare);
    CHECK(e->getPlayerEconomy(2).money >= 50000, "default option cost money");
    bool buff = false;
    for (const auto& b : e->getPolitics().buffs) buff = buff || (b.player == 1 && b.kind == BuffKind::WIND_HYDRO);
    CHECK(buff, "wind/hydro buff missing");
    delete e;
    endGroup();
}

void testCouncilAuction() {
    beginGroup("Council auction: only the higher bid pays; a tie shares the prize");
    for (int mode = 0; mode < 2; ++mode) {
        GameEngine* e = newEngine(true, "57");
        runToDay(*e, 6);
        runToHour(*e, 8.0f);
        e->getPlayerEconomyMut(1).money = 50000;
        e->getPlayerEconomyMut(2).money = 50000;
        e->debugStartCouncilCard(3);
        const CouncilState& c = e->getPolitics().council;
        int hi = c.cost[0], lo = c.cost[1];
        std::string msg;
        e->chooseCouncilOption(1, 0, msg);
        e->chooseCouncilOption(2, mode == 0 ? 1 : 0, msg);
        e->update(0.016f);
        float m1 = 0.0f, m2 = 0.0f;
        for (const auto& b : e->getPolitics().buffs) {
            if (b.kind != BuffKind::PAYOUT) continue;
            if (b.player == 1) m1 = b.mult;
            if (b.player == 2) m2 = b.mult;
        }
        if (mode == 0) {
            CHECK(e->getPlayerEconomy(1).money <= 50000 - hi + 200, "winner did not pay");
            CHECK(e->getPlayerEconomy(2).money >= 50000 - 1, "loser was not refunded (" << e->getPlayerEconomy(2).money << ", low " << lo << ")");
            CHECK(std::fabs(m1 - 1.4f) < 1e-4f && m2 == 0.0f, "buffs " << m1 << " / " << m2);
        } else {
            CHECK(std::fabs(m1 - 1.2f) < 1e-4f && std::fabs(m2 - 1.2f) < 1e-4f, "tie buffs " << m1 << " / " << m2);
        }
        delete e;
    }
    endGroup();
}

void testCouncilBondAndFrequency() {
    beginGroup("Council: bonds pay back after 2 days; cards appear on ~25% of days");
    GameEngine* e = newEngine(true, "58");
    runToDay(*e, 4);
    runToHour(*e, 8.0f);
    e->getPlayerEconomyMut(1).money = 20000;
    e->debugStartCouncilCard(1);
    int cost = e->getPolitics().council.cost[0];
    std::string msg;
    CHECK(e->chooseCouncilOption(1, 0, msg), msg);
    e->chooseCouncilOption(2, 2, msg);
    e->update(0.016f);
    REQUIRE(e->getPolitics().bonds.size() == 1, "bond not registered");
    int expect = static_cast<int>(std::lround(cost * 4500.0f / 3000.0f));
    CHECK(e->getPolitics().bonds[0].money == expect, "bond payback " << e->getPolitics().bonds[0].money << " != " << expect);
    CHECK(e->getPolitics().bonds[0].money > cost, "bond pays less than it costs");
    delete e;

    int cards = 0, days = 0;
    for (int s = 0; s < 8; ++s) {
        std::string seed = std::to_string(300 + s);
        GameEngine* g = newEngine(true, seed.c_str());
        for (int day = FIRST_COUNCIL_DAY; day <= 18; ++day) {
            runToDay(*g, day);
            ++days;
            if (g->getPolitics().council.scheduled) ++cards;
        }
        delete g;
    }
    float rate = static_cast<float>(cards) / static_cast<float>(days);
    CHECK(rate > 0.12f && rate < 0.40f, "council rate " << rate << " (" << cards << "/" << days << ")");
    endGroup();
}

// ===========================================================================
// F-16 Contracts
// ===========================================================================
void testDailyPosting() {
    beginGroup("Contract board: nothing before day 3, then 2-3 contracts daily, festival lights on festival days");
    GameEngine* e = newEngine(true, "61");
    runToDay(*e, 2);
    CHECK(e->getPolitics().contracts.empty(), "contracts before day 3");
    int tenders = 0;
    for (int day = 3; day <= 12; ++day) {
        runToDay(*e, day);
        const auto& ks = e->getPolitics().contracts;
        CHECK(ks.size() >= 2 && ks.size() <= 3, "day " << day << ": " << ks.size() << " contracts");
        bool fest = false;
        for (const auto& k : ks) {
            if (k.kind == ContractKind::TENDER_NIGHT) ++tenders;
            if (k.festival) fest = true;
            CHECK(k.target > 0 && k.rewardGold > 0, "bad terms day " << day);
        }
        CHECK(fest == (getFestivalOnDay(day) != EventId::NONE), "festival contract mismatch on day " << day);
        if (day == 3) CHECK(ks.size() == 2, "a tender on day 3");
    }
    CHECK(tenders >= 1, "no tender in 9 days");
    delete e;
    endGroup();
}

void testEveningPeakAndRecord() {
    beginGroup("Window contract pays only the player who averaged the target; record race is exclusive");
    GameEngine* e = newEngine(true, "62");
    buildWindFarm(*e, 1, 6);
    runToDay(*e, 5);
    int peak = e->debugPostContract(ContractKind::EVENING_PEAK);
    int race = e->debugPostContract(ContractKind::RECORD_RACE);
    int g1 = e->getPlayerEconomy(1).gold, g2 = e->getPlayerEconomy(2).gold;
    float share0 = e->getCityState().p1CityShare;
    runToHour(*e, 23.0f);
    const CityContract* kp = findContract(*e, peak);
    const CityContract* kr = findContract(*e, race);
    REQUIRE(kp && kr, "contracts vanished");
    CHECK(kp->state == ContractState::DONE && kp->completed[0] && !kp->completed[1], "evening peak result wrong");
    CHECK(kr->state == ContractState::DONE && kr->completed[0] && !kr->completed[1], "record race result wrong");
    int rewards = kp->rewardGold + kr->rewardGold;
    CHECK(e->getPlayerEconomy(1).gold - g1 >= rewards, "gold reward missing");
    CHECK(e->getPlayerEconomy(2).gold - g2 < rewards, "P2 got contract gold");
    // +1% (peak) +2% (race); the day verdict happens only at 06:00, so no other share change yet
    CHECK(e->getCityState().p1CityShare >= share0 + 0.03f - 1e-4f, "share " << e->getCityState().p1CityShare); // the daily board may pay more
    delete e;
    endGroup();
}

void testStorageAndLamps() {
    beginGroup("Point contracts: batteries at sunset and powered lamps at 22:00");
    GameEngine* e = newEngine(true, "63");
    buildWindFarm(*e, 2, 6);
    giveResources(*e, 2, 1000);
    place(*e, 2, BuildingType::BATTERY, slotOf(*e, 2, startPlotId(2), 6));
    place(*e, 2, BuildingType::LAMP, slotOf(*e, 2, startPlotId(2), 7));
    place(*e, 2, BuildingType::LAMP, slotOf(*e, 2, startPlotId(2), 8));
    runToDay(*e, 3);
    int st = e->debugPostContract(ContractKind::STORAGE_SUNSET);
    int lp = e->debugPostContract(ContractKind::LAMPS_22);
    runToHour(*e, 23.0f);
    const CityContract* ks = findContract(*e, st);
    const CityContract* kl = findContract(*e, lp);
    REQUIRE(ks && kl, "contracts vanished");
    CHECK(ks->completed[1] && !ks->completed[0], "storage: P2 " << ks->best[1] << " MWh of " << ks->target);
    CHECK(kl->completed[1] && !kl->completed[0], "lamps: P2 " << kl->best[1] << " of " << kl->target);
    CHECK(e->getPoweredLampCount(2) == 2, "powered lamps " << e->getPoweredLampCount(2));
    delete e;
    endGroup();
}

void testTender() {
    beginGroup("Tender: escrowed sealed bids, loser refunded, winner judged at 04:00");
    for (int mode = 0; mode < 2; ++mode) { // 0 = winner can deliver, 1 = winner fails
        GameEngine* e = newEngine(true, "64");
        if (mode == 0) buildWindFarm(*e, 1, 8);
        runToDay(*e, 5);
        int id = e->debugPostContract(ContractKind::TENDER_NIGHT);
        e->getPlayerEconomyMut(1).money = 20000;
        e->getPlayerEconomyMut(2).money = 20000;
        int step = e->getTenderBidStep();
        std::string msg;
        CHECK(e->placeBid(1, id, 3 * step, msg), msg);
        CHECK(e->placeBid(2, id, 2 * step, msg), msg);
        CHECK(e->getPlayerEconomy(1).money == 20000 - 3 * step, "P1 escrow");
        CHECK(e->placeBid(2, id, step, msg) && e->getPlayerEconomy(2).money == 20000 - step, "lowering a bid refunds");
        CHECK(!e->placeBid(2, id, 1000000, msg), "bid above the wallet accepted");
        runToHour(*e, 12.5f);
        const CityContract* k = findContract(*e, id);
        REQUIRE(k, "tender vanished");
        CHECK(k->state == ContractState::ACTIVE && k->tenderWinner == 1, "winner " << k->tenderWinner);
        CHECK(e->getPlayerEconomy(2).money >= 20000, "loser not refunded: " << e->getPlayerEconomy(2).money);
        CHECK(!e->placeBid(1, id, 4 * step, msg), "bid after 12:00 accepted");
        float share0 = e->getCityState().p1CityShare;
        int g0 = e->getPlayerEconomy(1).gold;
        // run to 05:00 next morning (still the same game day)
        while (std::fmod(e->getHour24() - 6.0f + 24.0f, 24.0f) < 23.0f && e->getCurrentDay() == 5) e->update(0.1f);
        k = findContract(*e, id);
        REQUIRE(k, "tender vanished");
        CHECK(k->state == ContractState::DONE, "tender not judged");
        if (mode == 0) {
            CHECK(k->completed[0], "winner with 8 turbines failed the night: avg " << k->best[0] << " / " << k->target);
            CHECK(e->getPlayerEconomy(1).gold - g0 >= k->rewardGold, "tender gold missing");
            CHECK(e->getCityState().p1CityShare > share0 + 0.029f, "tender share missing");
        } else {
            CHECK(k->failed[0] && !k->completed[0], "winner without power passed");
            CHECK(std::fabs(e->getCityState().p1CityShare - (share0 - 0.01f)) < 1e-4f, "failure penalty missing");
        }
        delete e;
    }
    endGroup();
}

// ===========================================================================
// F-31 Exchange & import
// ===========================================================================
void testExchange() {
    beginGroup("Exchange: shared prices rise on buys, fall on sells, relax 20% per night");
    GameEngine* e = newEngine(true, "71");
    e->getPlayerEconomyMut(1).money = 100000;
    int lot = e->getMarketLotSize(ResourceType::SILICON);
    int price = e->getMarketBuyPrice(ResourceType::SILICON);
    int si0 = e->getPlayerEconomy(1).silicon;
    std::string msg;
    CHECK(e->tradeResource(1, ResourceType::SILICON, true, msg), msg);
    CHECK(e->getPlayerEconomy(1).silicon == si0 + lot, "silicon not added");
    CHECK(e->getPlayerEconomy(1).money == 100000 - price, "money not charged");
    CHECK(std::fabs(e->getMarketMultiplier(ResourceType::SILICON) - 1.08f) < 1e-4f, "buy impact");
    CHECK(e->getMarketBuyPrice(ResourceType::SILICON) > price, "P2 does not see the higher price");
    CHECK(e->getMarketSellPrice(ResourceType::SILICON) < e->getMarketBuyPrice(ResourceType::SILICON) / 2 + 1, "spread");

    e->getPlayerEconomyMut(2).copper = 0;
    CHECK(!e->tradeResource(2, ResourceType::COPPER, false, msg), "sold copper it did not have");
    e->getPlayerEconomyMut(2).copper = 100;
    int m2 = e->getPlayerEconomy(2).money;
    int sell = e->getMarketSellPrice(ResourceType::COPPER);
    CHECK(e->tradeResource(2, ResourceType::COPPER, false, msg), msg);
    CHECK(e->getPlayerEconomy(2).money == m2 + sell && e->getPlayerEconomy(2).copper == 90, "sell result");
    CHECK(e->getMarketMultiplier(ResourceType::COPPER) < 1.0f, "sell impact");
    CHECK(!e->tradeResource(1, ResourceType::MONEY, true, msg), "money is not tradable");

    for (int i = 0; i < 40; ++i) e->tradeResource(1, ResourceType::GOLD, true, msg);
    CHECK(e->getMarketMultiplier(ResourceType::GOLD) <= 3.0f + 1e-4f, "price cap");
    e->getPlayerEconomyMut(2).money = 0;
    CHECK(!e->tradeResource(2, ResourceType::WOOD, true, msg), "bought without money");
    float m = e->getMarketMultiplier(ResourceType::SILICON);
    runToDay(*e, 2);
    CHECK(std::fabs(e->getMarketMultiplier(ResourceType::SILICON) - (1.0f + (m - 1.0f) * 0.8f)) < 1e-4f, "night decay");
    CHECK(e->getPlayerEconomy(1).money >= 0 && e->getPlayerEconomy(2).money >= 0, "negative money");
    delete e;
    endGroup();
}

void testImport() {
    beginGroup("Import: up to 60 MW of the rival's surplus, paid per MW-second, exporter can block");
    GameEngine* e = newEngine(true, "72");
    buildWindFarm(*e, 2, 6);
    runToDay(*e, 3);
    runToHour(*e, 9.0f);
    e->getPlayerEconomyMut(1).money = 50000;
    int demand = e->getCityState().cityEnergyDemand;
    REQUIRE(demand > 0, "no demand on day 3");
    int p2Before = e->getPlayerEconomy(2).energyMW;
    e->setImportRequest(1, true);
    e->update(0.1f);
    int flow = e->getImportFlowMW(1);
    CHECK(flow == std::min(IMPORT_MAX_MW, demand), "flow " << flow << " (demand " << demand << ")");
    CHECK(e->getPlayerEconomy(1).energyMW == flow, "importer MW " << e->getPlayerEconomy(1).energyMW);
    CHECK(e->getPlayerEconomy(2).energyMW <= p2Before - flow + 15, "exporter MW not reduced");
    int m1 = e->getPlayerEconomy(1).money, m2 = e->getPlayerEconomy(2).money;
    e->setTimeScale(1.0f);
    for (int i = 0; i < 10; ++i) e->update(1.0f);
    int paid = m1 - e->getPlayerEconomy(1).money;
    float expect = flow * e->getImportPricePerMWs() * 10.0f;
    // the importer also earns city payouts for the imported MW, so its net cost is below the price
    CHECK(e->getPolitics().imports.paidToday[0] >= static_cast<int>(expect) - 2, "paid " << e->getPolitics().imports.paidToday[0] << " expected ~" << expect);
    CHECK(e->getPolitics().imports.earnedToday[1] == e->getPolitics().imports.paidToday[0], "exporter not paid");
    CHECK(e->getPlayerEconomy(2).money > m2, "exporter money did not grow");
    (void)paid;

    e->setExportAllowed(2, false);
    e->update(0.1f);
    CHECK(e->getImportFlowMW(1) == 0 && e->getPlayerEconomy(1).energyMW == 0, "blocked export still flows");
    e->setExportAllowed(2, true);
    e->setImportRequest(2, true);
    e->update(0.1f);
    CHECK(e->getImportFlowMW(1) == 0 && e->getImportFlowMW(2) == 0, "both requesting still flows");
    e->setImportRequest(2, false);

    // Whole-day effect: importing the demand keeps P1 from losing territory
    runToDay(*e, 4);
    float shareStart = e->getCityState().p1CityShare;
    e->getPlayerEconomyMut(1).money = 500000;
    e->debugForceCityEvent(EventId::NONE, EventId::NONE);
    runToDay(*e, 5);
    CHECK(std::fabs(e->getCityState().p1CityShare - shareStart) < 0.021f,
          "importer lost territory: " << shareStart << " -> " << e->getCityState().p1CityShare);
    delete e;

    GameEngine* off = newEngine(false, "72");
    buildWindFarm(*off, 2, 6);
    runToDay(*off, 3);
    off->getPlayerEconomyMut(1).money = 50000;
    off->setImportRequest(1, true);
    off->update(0.5f);
    CHECK(off->getImportFlowMW(1) == 0, "import works with politics off");
    delete off;
    endGroup();
}

// ===========================================================================
// Bot & full match
// ===========================================================================
void testBotsFullMatch() {
    beginGroup("Two politics bots play a full match: no negative money, valid shares, systems used");
    GameEngine* e = newEngine(true, "81");
    buildWindFarm(*e, 1, 4);
    buildWindFarm(*e, 2, 3);
    giveResources(*e, 2, 1000);
    place(*e, 2, BuildingType::BATTERY, slotOf(*e, 2, startPlotId(2), 6));
    bool negative = false, badShare = false;
    int councilsAnswered = 0, bids = 0;
    float tick = 0.0f;
    while (e->getCityState().winner == 0 && e->getCurrentDay() <= 21) {
        e->update(0.25f);
        tick += 0.25f;
        if (tick >= 1.0f) {
            tick = 0.0f;
            bool wasOpen = e->getPolitics().council.active && e->getPolitics().council.choice[1] < 0;
            e->politicsBotThink(1, 3);
            e->politicsBotThink(2, 2);
            if (wasOpen && e->getPolitics().council.choice[1] >= 0) ++councilsAnswered;
            for (const auto& k : e->getPolitics().contracts) {
                if (k.kind == ContractKind::TENDER_NIGHT && (k.bid[0] > 0 || k.bid[1] > 0)) ++bids;
            }
        }
        for (int p = 1; p <= 2; ++p) negative = negative || e->getPlayerEconomy(p).money < 0 || e->getPlayerEconomy(p).gold < 0;
        float s = e->getCityState().p1CityShare;
        badShare = badShare || s < 0.0f || s > 1.0f;
        e->drainPoliticsNotices();
    }
    CHECK(!negative, "money or gold went negative");
    CHECK(!badShare, "share left [0, 1]");
    CHECK(e->getCityState().winner != 0, "match did not finish");
    std::cout << "  bots answered " << councilsAnswered << " council cards, tender-bid seconds " << bids
              << ", winner " << e->getCityState().winner << "\n";
    delete e;

    // Suggestions are always a valid option, and a broke bot takes the free default
    GameEngine* g = newEngine(true, "82");
    runToDay(*g, 6);
    for (int card = 0; card < getCouncilCardCount(); ++card) {
        g->debugStartCouncilCard(card);
        g->getPlayerEconomyMut(2).money = 0;
        int s = g->suggestCouncilOption(2, 3);
        CHECK(s == getCouncilCard(card).defaultOption, "broke bot picked " << s << " on card " << card);
        g->getPlayerEconomyMut(1).money = 1000000;
        int r = g->suggestCouncilOption(1, 3);
        CHECK(r >= 0 && r < getCouncilCard(card).optionCount, "invalid suggestion");
    }
    delete g;
    endGroup();
}

void testRestartKeepsSwitch() {
    beginGroup("restartGame() keeps the politics switch and clears the board");
    GameEngine* e = newEngine(true, "91");
    runToDay(*e, 6);
    e->restartGame();
    CHECK(e->isCityPoliticsEnabled(), "switch lost on restart");
    CHECK(e->getPolitics().contracts.empty() && e->getPolitics().activeEvent == EventId::NONE, "board not cleared");
    CHECK(e->getMarketMultiplier(ResourceType::WOOD) == 1.0f, "market not reset");
    delete e;
    endGroup();
}

} // namespace

int main() {
    std::cout << "========================================================\n";
    std::cout << " ENERGY CRISIS - CITY POLITICS TESTS (F-09, F-13, F-16, F-31)\n";
    std::cout << "========================================================\n";
    testPoliticsOffByDefault();
    testForecastBecomesToday();
    testEventMultipliers();
    testWeatherUnaffected();
    testCouncilDonationAndEscrow();
    testCouncilTimeoutDefaults();
    testCouncilAuction();
    testCouncilBondAndFrequency();
    testDailyPosting();
    testEveningPeakAndRecord();
    testStorageAndLamps();
    testTender();
    testExchange();
    testImport();
    testBotsFullMatch();
    testRestartKeepsSwitch();
    setSeedEnv(nullptr);
    std::cout << "\n========================================================\n";
    if (g_failures == 0) std::cout << " ALL " << g_checks << " CITY POLITICS CHECKS PASSED\n";
    else std::cout << " " << g_failures << " OF " << g_checks << " CITY POLITICS CHECKS FAILED\n";
    std::cout << "========================================================\n";
    return (g_failures == 0) ? 0 : 1;
}
