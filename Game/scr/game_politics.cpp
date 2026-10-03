// =============================================================================
// ENERGY CRISIS - CITY POLITICS: event deck, festivals, council decisions  [team b-politics]
// Contracts live in game_contracts.cpp, the exchange and power import in game_market.cpp.
// =============================================================================
#include "../includes/game_main.h"
#include <algorithm>
#include <cmath>

namespace Politics {

// -----------------------------------------------------------------------------
// F-09 Event table. Multipliers: demand, solar, wind, hydro, payout, gold, other mines
// -----------------------------------------------------------------------------
namespace {

const EventDef kEvents[] = {
    { EventId::NONE, "СПОКОЕН ДЕН", "Без събития", "Градът живее спокойно.", Tone::NEUTRAL,
      1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f },
    { EventId::HEATWAVE, "ГОРЕЩА ВЪЛНА", "Нужда +35%, Слънце +10%",
      "Климатиците работят денонощно: градът иска повече ток.", Tone::BAD,
      1.35f, 1.10f, 1.00f, 0.90f, 1.00f, 1.00f, 1.00f },
    { EventId::COLD_SNAP, "СТУДЕНА ВЪЛНА", "Нужда +25%, Вятър +15%",
      "Студът вдига потреблението, но вятърът се усилва.", Tone::BAD,
      1.25f, 0.85f, 1.15f, 1.00f, 1.00f, 1.00f, 1.00f },
    { EventId::DROUGHT, "СУША", "ВЕЦ -50%",
      "Реката пресъхва: водните централи губят половината мощност.", Tone::BAD,
      1.00f, 1.05f, 1.00f, 0.50f, 1.00f, 1.00f, 1.00f },
    { EventId::SUBSIDY, "ДЪРЖАВНА СУБСИДИЯ", "Приходи +50%",
      "Държавата доплаща за всеки доставен мегават.", Tone::GOOD,
      1.00f, 1.00f, 1.00f, 1.00f, 1.50f, 1.00f, 1.00f },
    { EventId::PROTEST, "ПРОТЕСТ В ГРАДА", "Приходи -30%",
      "Жителите протестират срещу сметките за ток.", Tone::BAD,
      1.00f, 1.00f, 1.00f, 1.00f, 0.70f, 1.00f, 1.00f },
    { EventId::MINER_STRIKE, "СТАЧКА НА МИНЬОРИТЕ", "Добив -40%",
      "Миньорите стачкуват: мините дават по-малко суровини.", Tone::BAD,
      1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 0.60f },
    { EventId::GOLD_RUSH, "ЗЛАТНА ТРЕСКА", "Злато x2",
      "Нова жила: златото от мини и дивиденти се удвоява.", Tone::GOOD,
      1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 2.00f, 1.00f },
    { EventId::GRID_REPAIR, "РЕМОНТ НА МРЕЖАТА", "Нужда -20%",
      "Квартали са изключени за ремонт: градът иска по-малко ток.", Tone::GOOD,
      0.80f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f },
    { EventId::NEIGHBOUR_BLACKOUT, "АВАРИЯ В СЪСЕДЕН ГРАД", "Нужда +15%, Приходи +30%",
      "Съседите купуват ток на висока цена.", Tone::NEUTRAL,
      1.15f, 1.00f, 1.00f, 1.00f, 1.30f, 1.00f, 1.00f },
    { EventId::FESTIVAL_SPRING, "ПРАЗНИК НА МАРТЕНИЦАТА", "Нужда +15%, Приходи +60%",
      "Пролетен празник: площадите светят до късно.", Tone::FESTIVAL,
      1.15f, 1.00f, 1.00f, 1.00f, 1.60f, 1.00f, 1.00f },
    { EventId::FESTIVAL_SUMMER, "ФЕСТИВАЛ НА РОЗАТА", "Нужда +20%, Приходи +60%",
      "Казанлък празнува: туристи пълнят града.", Tone::FESTIVAL,
      1.20f, 1.10f, 1.00f, 1.00f, 1.60f, 1.00f, 1.00f },
    { EventId::FESTIVAL_AUTUMN, "ПРАЗНИК НА ВИНОТО", "Нужда +20%, Приходи +60%",
      "Есенен празник с концерти до полунощ.", Tone::FESTIVAL,
      1.20f, 1.00f, 1.00f, 1.00f, 1.60f, 1.00f, 1.00f },
    { EventId::FESTIVAL_WINTER, "КОЛЕДНИ СВЕТЛИНИ", "Нужда +30%, Приходи +80%",
      "Коледният базар иска светлини навсякъде.", Tone::FESTIVAL,
      1.30f, 1.00f, 1.00f, 1.00f, 1.80f, 1.00f, 1.00f },
};

// One fixed festival per season (seasons last 5 days: 1-5, 6-10, 11-15, 16-20)
const int kFestivalDay[4] = { 4, 9, 14, 19 };
const EventId kFestivalId[4] = { EventId::FESTIVAL_SPRING, EventId::FESTIVAL_SUMMER,
                                 EventId::FESTIVAL_AUTUMN, EventId::FESTIVAL_WINTER };

// Random deck (festivals are never drawn)
const EventId kDeck[] = { EventId::HEATWAVE, EventId::COLD_SNAP, EventId::DROUGHT, EventId::SUBSIDY,
                          EventId::PROTEST, EventId::MINER_STRIKE, EventId::GOLD_RUSH,
                          EventId::GRID_REPAIR, EventId::NEIGHBOUR_BLACKOUT };

// -----------------------------------------------------------------------------
// F-13 Council cards. Costs are base money (x priceScale(day)).
// -----------------------------------------------------------------------------
const CouncilCardDef kCards[] = {
    { 0, "ПРОТЕСТ ПРЕД ОБЩИНАТА", "Жителите искат обезщетение за спирането на тока.", 3, 2,
      { { "Плати обезщетение", "Протестът утихва", 1500, CouncilEffect::NONE, 0, 0 },
        { "Организирай концерт", "Приходи -10% за 1 ден", 600, CouncilEffect::PAYOUT_BUFF, -10, 1 },
        { "Игнорирай", "Приходи -30% за 1 ден", 0, CouncilEffect::PAYOUT_BUFF, -30, 1 } } },
    { 1, "ЗЕЛЕНИ ОБЛИГАЦИИ", "Общината продава облигации с падеж след 2 дни.", 3, 2,
      { { "Голям пакет", "+150% обратно след 2 дни", 3000, CouncilEffect::BOND, 4500, 2 },
        { "Малък пакет", "+140% обратно след 2 дни", 1200, CouncilEffect::BOND, 1700, 2 },
        { "Откажи", "Нищо не се променя", 0, CouncilEffect::NONE, 0, 0 } } },
    { 2, "ДАРЕНИЕ ЗА БОЛНИЦАТА", "Болницата търси спонсор за резервен генератор.", 3, 2,
      { { "Голямо дарение", "+3% от града", 4000, CouncilEffect::SHARE, 30, 0 },
        { "Малко дарение", "+1% от града", 1500, CouncilEffect::SHARE, 10, 0 },
        { "Откажи", "Нищо не се променя", 0, CouncilEffect::NONE, 0, 0 } } },
    { 3, "ТЪРГ ЗА СУБСИДИЯ", "По-високата оферта печели +40% приходи за 2 дни.", 3, 2,
      { { "Висока оферта", "Плащате само ако спечелите", 3000, CouncilEffect::AUCTION_BID, 2, 2 },
        { "Ниска оферта", "Плащате само ако спечелите", 1200, CouncilEffect::AUCTION_BID, 1, 2 },
        { "Без оферта", "Нищо не се променя", 0, CouncilEffect::NONE, 0, 0 } } },
    { 4, "НОВА МИННА ТЕХНИКА", "Синдикатът предлага модерни машини за мините.", 3, 2,
      { { "Нова техника", "Добив +40% за 2 дни", 2500, CouncilEffect::MINE_BUFF, 40, 2 },
        { "Ремонт", "Добив +15% за 2 дни", 1000, CouncilEffect::MINE_BUFF, 15, 2 },
        { "Откажи", "Нищо не се променя", 0, CouncilEffect::NONE, 0, 0 } } },
    { 5, "НАУЧЕН ГРАНТ", "Университетът тества нови слънчеви клетки.", 3, 2,
      { { "Финансирай", "Слънце +25% за 3 дни", 2500, CouncilEffect::SOLAR_BUFF, 25, 3 },
        { "Частично", "Слънце +10% за 3 дни", 1000, CouncilEffect::SOLAR_BUFF, 10, 3 },
        { "Откажи", "Нищо не се променя", 0, CouncilEffect::NONE, 0, 0 } } },
    { 6, "ЗЛАТНИЯТ РЕЗЕРВ", "Хазната разпродава част от златото си.", 3, 2,
      { { "Купи 40 злато", "+40 G веднага", 3000, CouncilEffect::GOLD, 40, 0 },
        { "Купи 15 злато", "+15 G веднага", 1300, CouncilEffect::GOLD, 15, 0 },
        { "Откажи", "Нищо не се променя", 0, CouncilEffect::NONE, 0, 0 } } },
    { 7, "ПРОВЕРКА НА МРЕЖАТА", "Инспекция: градът иска по-сигурни централи.", 3, 2,
      { { "Модернизирай", "Вятър и ВЕЦ +20% за 2 дни", 2000, CouncilEffect::WIND_HYDRO_BUFF, 20, 2 },
        { "Малък ремонт", "Нищо не се променя", 700, CouncilEffect::NONE, 0, 0 },
        { "Откажи", "-1% от града", 0, CouncilEffect::SHARE, -10, 0 } } },
};

const int kCardCount = static_cast<int>(sizeof(kCards) / sizeof(kCards[0]));

} // namespace

const EventDef& getEventDef(EventId id) {
    for (const auto& e : kEvents) {
        if (e.id == id) return e;
    }
    return kEvents[0];
}

int getFestivalDay(int seasonIndex) {
    return (seasonIndex >= 0 && seasonIndex < 4) ? kFestivalDay[seasonIndex] : 0;
}

EventId getFestivalOnDay(int day) {
    for (int s = 0; s < 4; ++s) {
        if (kFestivalDay[s] == day) return kFestivalId[s];
    }
    return EventId::NONE;
}

int getCouncilCardCount() { return kCardCount; }

const CouncilCardDef& getCouncilCard(int id) {
    if (id < 0 || id >= kCardCount) return kCards[0];
    return kCards[id];
}

} // namespace Politics

using namespace Politics;

namespace {
int rollPct(std::mt19937& rng) { return std::uniform_int_distribution<int>(0, 99)(rng); }
const char* playerTag(int player) { return player == 1 ? "ИГРАЧ 1" : "ИГРАЧ 2"; }
} // namespace

// =============================================================================
// Shared helpers
// =============================================================================
void GameEngine::politicsNotice(const std::string& text, int player, Tone tone) {
    PoliticsNotice n;
    n.text = text;
    n.player = player;
    n.tone = tone;
    politics.notices.push_back(n);
    if (politics.notices.size() > 40) politics.notices.erase(politics.notices.begin());
}

std::vector<PoliticsNotice> GameEngine::drainPoliticsNotices() {
    std::vector<PoliticsNotice> out;
    out.swap(politics.notices);
    return out;
}

void GameEngine::addCityShare(int player, int tenths) {
    float delta = static_cast<float>(tenths) / 1000.0f;
    if (player == 2) delta = -delta;
    city.p1CityShare = std::clamp(city.p1CityShare + delta, 0.0f, 1.0f);
    p1.cityInfluence = city.p1CityShare;
    p2.cityInfluence = 1.0f - city.p1CityShare;
}

float GameEngine::dayHourNow() const {
    return std::fmod(hour24 - Balance::CLOCK_HOUR_AT_ZERO + 24.0f, 24.0f);
}

// =============================================================================
// Engine hooks
// =============================================================================
void GameEngine::resetCityPolitics(unsigned int seed) {
    politics = CityPolitics();
    politics.rng.seed(seed ^ 0xC17E5EEDu);
    politics.baseDemandMW = city.cityEnergyDemand;
    politics.lastDayHour = dayHourNow();
}

void GameEngine::tickCityPoliticsRealTime(float realDt) {
    CouncilState& c = politics.council;
    if (c.hasResult) {
        c.resultTimer -= realDt;
        if (c.resultTimer <= 0.0f) c.hasResult = false;
    }
    if (!c.active) return;
    c.elapsed += realDt;
    c.timeLeft -= realDt;
    if (c.timeLeft <= 0.0f || (c.choice[0] >= 0 && c.choice[1] >= 0)) {
        resolveCouncil();
    }
}

void GameEngine::updateCityPolitics(float dt) {
    float prevDayHour = politics.lastDayHour;
    float dayHour = dayHourNow();
    if (dayHour < prevDayHour) prevDayHour = 0.0f; // the 06:00 rollover already reset the board
    politics.lastDayHour = dayHour;

    // Timed buffs and bonds tick on game time
    for (auto& b : politics.buffs) b.secondsLeft -= dt;
    politics.buffs.erase(std::remove_if(politics.buffs.begin(), politics.buffs.end(),
                                        [](const PlayerBuff& b) { return b.secondsLeft <= 0.0f; }),
                         politics.buffs.end());
    for (auto& bond : politics.bonds) {
        bond.secondsLeft -= dt;
        if (bond.secondsLeft <= 0.0f) {
            PlayerEconomy& e = (bond.player == 1) ? p1 : p2;
            e.money += bond.money;
            e.data.money = e.money;
            politicsNotice(std::string(playerTag(bond.player)) + ": ОБЛИГАЦИИТЕ ПАДЕЖИРАХА: +" +
                               std::to_string(bond.money) + "$", bond.player, Tone::GOOD);
        }
    }
    politics.bonds.erase(std::remove_if(politics.bonds.begin(), politics.bonds.end(),
                                        [](const PendingPayment& p) { return p.secondsLeft <= 0.0f; }),
                         politics.bonds.end());

    if (!cityPoliticsEnabled) return;

    // Council card appears at its scheduled hour
    CouncilState& c = politics.council;
    float appearDayHour = std::fmod(c.appearHour - Balance::CLOCK_HOUR_AT_ZERO + 24.0f, 24.0f);
    if (c.scheduled && !c.active && dayHour >= appearDayHour) {
        c.scheduled = false;
        startCouncilCard();
    }

    updateContracts(dt, prevDayHour, dayHour);
}

void GameEngine::restoreBaseCityDemand() {
    city.cityEnergyDemand = politics.baseDemandMW;
}

void GameEngine::onCityPoliticsNewDay(int endedDay) {
    (void)endedDay;
    // Market prices relax 20% towards normal every night; import counters restart
    for (int i = 0; i < MARKET_SLOTS; ++i) {
        politics.market.mult[i] = 1.0f + (politics.market.mult[i] - 1.0f) * 0.8f;
        politics.market.boughtToday[i] = 0;
        politics.market.soldToday[i] = 0;
    }
    for (int i = 0; i < 2; ++i) {
        politics.imports.paidToday[i] = 0;
        politics.imports.earnedToday[i] = 0;
    }

    politics.baseDemandMW = city.cityEnergyDemand;
    politics.lastDayHour = 0.0f;

    if (!cityPoliticsEnabled) {
        politics.activeEvent = EventId::NONE;
        politics.forecastEvent = EventId::NONE;
        politics.contracts.clear();
        return;
    }

    // A council card still open at the day end is decided with the defaults
    if (politics.council.active) resolveCouncil();
    settleContractsAtDayEnd();

    // F-09: yesterday's forecast becomes today's event, then forecast tomorrow
    const int today = currentDay;
    politics.activeEvent = politics.forecastEvent;
    EventId festivalToday = getFestivalOnDay(today);
    if (festivalToday != EventId::NONE) politics.activeEvent = festivalToday;

    const int tomorrow = today + 1;
    EventId next = getFestivalOnDay(tomorrow);
    if (next == EventId::NONE && tomorrow >= FIRST_EVENT_DAY && tomorrow <= Balance::FINAL_DAY &&
        rollPct(politics.rng) < EVENT_CHANCE_PCT) {
        const int deckSize = static_cast<int>(sizeof(kDeck) / sizeof(kDeck[0]));
        int pick = std::uniform_int_distribution<int>(0, deckSize - 1)(politics.rng);
        if (kDeck[pick] == politics.activeEvent) pick = (pick + 1) % deckSize; // never the same two days running
        next = kDeck[pick];
    }
    politics.forecastEvent = next;

    const EventDef& ev = getEventDef(politics.activeEvent);
    city.cityEnergyDemand = static_cast<int>(std::lround(politics.baseDemandMW * ev.demandMult));
    if (politics.activeEvent != EventId::NONE) {
        politicsNotice("ДНЕС: " + std::string(ev.nameBg) + " - " + ev.effectBg, 0, ev.tone);
    }

    // F-13: maybe schedule a council card for today
    politics.council.scheduled = false;
    if (today >= FIRST_COUNCIL_DAY && rollPct(politics.rng) < COUNCIL_CHANCE_PCT) {
        politics.council.scheduled = true;
        std::uniform_real_distribution<float> hour(COUNCIL_EARLIEST_HOUR, COUNCIL_LATEST_HOUR);
        politics.council.appearHour = hour(politics.rng);
    }

    // F-16: today's contracts
    postDailyContracts(today);
}

// =============================================================================
// Multipliers used by the core simulation (all 1.0 when nothing is active)
// =============================================================================
float GameEngine::politicsGenMult(int player, BuildingType type) const {
    const EventDef& ev = getEventDef(politics.activeEvent);
    float m = 1.0f;
    if (type == BuildingType::SOLAR_PANEL) m = ev.solarMult;
    else if (type == BuildingType::WIND_TURBINE) m = ev.windMult;
    else if (type == BuildingType::HYDRO_PLANT) m = ev.hydroMult;
    for (const auto& b : politics.buffs) {
        if (b.player != player) continue;
        if (b.kind == BuffKind::SOLAR && type == BuildingType::SOLAR_PANEL) m *= b.mult;
        if (b.kind == BuffKind::WIND_HYDRO &&
            (type == BuildingType::WIND_TURBINE || type == BuildingType::HYDRO_PLANT)) m *= b.mult;
    }
    return m;
}

float GameEngine::politicsPayoutMult(int player) const {
    float m = getEventDef(politics.activeEvent).payoutMult;
    for (const auto& b : politics.buffs) {
        if (b.player == player && b.kind == BuffKind::PAYOUT) m *= b.mult;
    }
    return std::max(0.0f, m);
}

float GameEngine::politicsGoldMult(int player) const {
    (void)player;
    return getEventDef(politics.activeEvent).goldMult;
}

float GameEngine::politicsMineMult(int player, ResourceType type) const {
    const EventDef& ev = getEventDef(politics.activeEvent);
    float m = (type == ResourceType::GOLD) ? ev.goldMult : ev.mineMult;
    for (const auto& b : politics.buffs) {
        if (b.player == player && b.kind == BuffKind::MINE) m *= b.mult;
    }
    return m;
}

// =============================================================================
// F-09 public helpers
// =============================================================================
int GameEngine::getNextFestivalDay() const {
    for (int s = 0; s < 4; ++s) {
        int d = getFestivalDay(s);
        if (d >= currentDay) return d;
    }
    return 0;
}

void GameEngine::debugForceCityEvent(EventId today, EventId tomorrow) {
    politics.activeEvent = today;
    politics.forecastEvent = tomorrow;
    city.cityEnergyDemand = static_cast<int>(std::lround(politics.baseDemandMW * getEventDef(today).demandMult));
}

// =============================================================================
// F-13 Council decisions
// =============================================================================
namespace {
int roundCost(float v) { return static_cast<int>(std::lround(v / 50.0f)) * 50; }
} // namespace

void GameEngine::debugStartCouncilCard(int cardId) {
    CouncilState& c = politics.council;
    if (c.active) resolveCouncil();
    c.scheduled = false;
    c.active = true;
    c.cardId = std::max(0, std::min(cardId, getCouncilCardCount() - 1));
    c.day = currentDay;
    c.timeLeft = COUNCIL_DECISION_SEC;
    c.elapsed = 0.0f;
    c.choice[0] = c.choice[1] = -1;
    const CouncilCardDef& card = getCouncilCard(c.cardId);
    for (int i = 0; i < 3; ++i) {
        c.cost[i] = (i < card.optionCount) ? roundCost(card.options[i].baseCost * priceScale(currentDay)) : 0;
    }
    politicsNotice("ГРАДСКИЯТ СЪВЕТ РЕШАВА: " + std::string(card.titleBg), 0, Tone::NEUTRAL);
}

void GameEngine::startCouncilCard() {
    int count = getCouncilCardCount();
    int pick = std::uniform_int_distribution<int>(0, count - 1)(politics.rng);
    if (pick == politics.council.resultCardId && count > 1) pick = (pick + 1) % count; // no direct repeat
    debugStartCouncilCard(pick);
}

bool GameEngine::chooseCouncilOption(int player, int option, std::string& outMsg) {
    CouncilState& c = politics.council;
    if (player != 1 && player != 2) return false;
    if (!c.active) {
        outMsg = "НЯМА ОТВОРЕНО РЕШЕНИЕ НА СЪВЕТА!";
        return false;
    }
    const CouncilCardDef& card = getCouncilCard(c.cardId);
    if (option < 0 || option >= card.optionCount) {
        outMsg = "НЕВАЛИДЕН ИЗБОР!";
        return false;
    }
    int idx = player - 1;
    if (c.choice[idx] >= 0) {
        outMsg = "ВЕЧЕ ГЛАСУВАХТЕ!";
        return false;
    }
    PlayerEconomy& e = (player == 1) ? p1 : p2;
    int cost = c.cost[option];
    if (e.money < cost) {
        outMsg = "НЕДОСТИГ НА ПАРИ! НУЖНИ: " + std::to_string(cost) + "$ (ИМАТЕ " + std::to_string(e.money) + "$)";
        return false;
    }
    // Money goes into escrow now (auction losers get it back at the resolution)
    e.money -= cost;
    e.data.money = e.money;
    c.choice[idx] = option;
    outMsg = "ИЗБРАНО: " + std::string(card.options[option].labelBg);
    return true;
}

void GameEngine::resolveCouncil() {
    CouncilState& c = politics.council;
    if (!c.active) return;
    const CouncilCardDef& card = getCouncilCard(c.cardId);
    for (int i = 0; i < 2; ++i) {
        if (c.choice[i] < 0) c.choice[i] = card.defaultOption; // defaults never cost money
    }

    // Auction cards compare the two bids; only the winner pays
    int rank[2] = { 0, 0 };
    for (int i = 0; i < 2; ++i) {
        const CouncilOption& o = card.options[c.choice[i]];
        if (o.effect == CouncilEffect::AUCTION_BID) rank[i] = o.magnitude;
    }
    int auctionWinner = 0; // 0 none, 1, 2, 3 = tie (both win half)
    if (rank[0] > 0 || rank[1] > 0) {
        if (rank[0] > rank[1]) auctionWinner = 1;
        else if (rank[1] > rank[0]) auctionWinner = 2;
        else auctionWinner = 3;
    }

    for (int i = 0; i < 2; ++i) {
        int player = i + 1;
        PlayerEconomy& e = (player == 1) ? p1 : p2;
        const CouncilOption& o = card.options[c.choice[i]];
        int cost = c.cost[c.choice[i]];
        float sc = priceScale(c.day);
        std::string res = o.labelBg;
        switch (o.effect) {
            case CouncilEffect::SHARE:
                addCityShare(player, o.magnitude);
                res += (o.magnitude >= 0 ? " (+" : " (") + std::to_string(o.magnitude / 10) + "% от града)";
                break;
            case CouncilEffect::PAYOUT_BUFF:
            case CouncilEffect::MINE_BUFF:
            case CouncilEffect::SOLAR_BUFF:
            case CouncilEffect::WIND_HYDRO_BUFF: {
                PlayerBuff b;
                b.player = player;
                b.kind = (o.effect == CouncilEffect::PAYOUT_BUFF) ? BuffKind::PAYOUT
                       : (o.effect == CouncilEffect::MINE_BUFF)   ? BuffKind::MINE
                       : (o.effect == CouncilEffect::SOLAR_BUFF)  ? BuffKind::SOLAR
                                                                  : BuffKind::WIND_HYDRO;
                b.mult = 1.0f + static_cast<float>(o.magnitude) / 100.0f;
                b.secondsLeft = static_cast<float>(o.days) * Balance::SECONDS_PER_DAY;
                b.labelBg = o.effectBg;
                politics.buffs.push_back(b);
                break;
            }
            case CouncilEffect::BOND: {
                PendingPayment p;
                p.player = player;
                p.money = (o.baseCost > 0) ? static_cast<int>(std::lround(static_cast<float>(cost) * o.magnitude / o.baseCost)) : 0;
                p.secondsLeft = static_cast<float>(o.days) * Balance::SECONDS_PER_DAY;
                politics.bonds.push_back(p);
                res += " (+" + std::to_string(p.money) + "$ след " + std::to_string(o.days) + " дни)";
                break;
            }
            case CouncilEffect::GOLD: {
                int g = static_cast<int>(std::lround(o.magnitude * sc));
                e.gold += g;
                e.data.gold = e.gold;
                res = "+" + std::to_string(g) + " злато";
                break;
            }
            case CouncilEffect::AUCTION_BID: {
                bool won = (auctionWinner == player) || (auctionWinner == 3);
                if (won) {
                    PlayerBuff b;
                    b.player = player;
                    b.kind = BuffKind::PAYOUT;
                    b.mult = (auctionWinner == 3) ? 1.20f : 1.40f;
                    b.secondsLeft = static_cast<float>(o.days) * Balance::SECONDS_PER_DAY;
                    b.labelBg = (auctionWinner == 3) ? "Субсидия +20% (поделена)" : "Субсидия +40%";
                    politics.buffs.push_back(b);
                    res = (auctionWinner == 3) ? "Поделена субсидия +20%" : "Спечели субсидията +40%";
                } else {
                    e.money += cost; // outbid: escrow refunded
                    e.data.money = e.money;
                    res = "Загуби търга (парите са върнати)";
                }
                break;
            }
            case CouncilEffect::NONE:
            default:
                break;
        }
        c.resultChoice[i] = c.choice[i];
        c.resultBg[i] = res;
    }

    c.hasResult = true;
    c.resultCardId = c.cardId;
    c.resultTimer = 6.0f;
    c.active = false;
    politicsNotice(std::string("СЪВЕТЪТ РЕШИ: P1 - ") + c.resultBg[0] + " | P2 - " + c.resultBg[1], 0, Tone::NEUTRAL);
}

namespace {
// Rough money a player earns per day at the current grid (bot heuristics only)
float estimateDailyIncome(const PlayerEconomy& me, const PlayerEconomy& other, float payoutMult, float scale) {
    float total = static_cast<float>(std::max(0, me.energyMW) + std::max(0, other.energyMW));
    float est = 0.0f;
    if (total > 0.0f) {
        float pool = static_cast<float>(Balance::calculateContractPool(total));
        est = pool * (static_cast<float>(std::max(0, me.energyMW)) / total) * Balance::SECONDS_PER_DAY * payoutMult;
    }
    return std::max(est, 1500.0f * scale);
}
} // namespace

int GameEngine::suggestCouncilOption(int player, int skill) const {
    const CouncilState& c = politics.council;
    if (!c.active) return -1;
    const CouncilCardDef& card = getCouncilCard(c.cardId);
    const PlayerEconomy& me = (player == 1) ? p1 : p2;
    const PlayerEconomy& other = (player == 1) ? p2 : p1;
    skill = std::max(1, std::min(skill, 3));
    float sc = priceScale(c.day);
    float daily = estimateDailyIncome(me, other, politicsPayoutMult(player), sc);
    float spendCap = me.money * (0.25f + 0.1f * skill);     // never bet the whole wallet
    float shareValuePct = (500.0f + 500.0f * skill) * sc;   // what +1% of the city is worth to this bot

    // Share of the installed power that is solar / wind+hydro (for generation grants)
    float solarMW = 0.0f, windHydroMW = 0.0f, allMW = 0.0f;
    for (const auto& b : buildings) {
        if (b.playerOwner != player) continue;
        float base = static_cast<float>(getBuildingCost(b.type).basePowerMW);
        if (b.type == BuildingType::SOLAR_PANEL) solarMW += base;
        if (b.type == BuildingType::WIND_TURBINE || b.type == BuildingType::HYDRO_PLANT) windHydroMW += base;
        allMW += base;
    }
    float solarFrac = (allMW > 0.0f) ? solarMW / allMW : 0.0f;
    float windFrac = (allMW > 0.0f) ? windHydroMW / allMW : 0.0f;

    int best = card.defaultOption;
    float bestScore = -1e9f;
    for (int i = 0; i < card.optionCount; ++i) {
        const CouncilOption& o = card.options[i];
        float cost = static_cast<float>(c.cost[i]);
        if (cost > spendCap && cost > 0.0f) continue;
        float benefit = 0.0f;
        float days = static_cast<float>(o.days);
        float mag = static_cast<float>(o.magnitude);
        switch (o.effect) {
            case CouncilEffect::SHARE: benefit = shareValuePct * mag / 10.0f; break;
            case CouncilEffect::PAYOUT_BUFF: benefit = daily * days * mag / 100.0f; break;
            case CouncilEffect::MINE_BUFF: benefit = 0.35f * daily * days * mag / 100.0f * skill; break;
            case CouncilEffect::SOLAR_BUFF: benefit = daily * days * mag / 100.0f * solarFrac; break;
            case CouncilEffect::WIND_HYDRO_BUFF: benefit = daily * days * mag / 100.0f * windFrac; break;
            case CouncilEffect::BOND:
                benefit = (o.baseCost > 0 ? cost * mag / o.baseCost : 0.0f) * (me.money > 3.0f * cost ? 1.0f : 0.8f);
                break;
            case CouncilEffect::GOLD: benefit = mag * sc * (60.0f + 15.0f * skill); break;
            case CouncilEffect::AUCTION_BID: {
                // Win chance grows with the bid; the price is paid only on a win
                float win = (o.magnitude >= 2) ? 0.7f : 0.35f;
                benefit = win * (daily * days * 0.40f - cost);
                cost = 0.0f;
                break;
            }
            case CouncilEffect::NONE:
            default: break;
        }
        float score = benefit - cost;
        if (score > bestScore + 1.0f) {
            bestScore = score;
            best = i;
        }
    }
    return best;
}

// =============================================================================
// Bot: one call per second of real time from the UI (skill 1 = easy .. 3 = hard)
// =============================================================================
void GameEngine::politicsBotThink(int player, int skill) {
    if (player != 1 && player != 2) return;
    skill = std::max(1, std::min(skill, 3));
    PlayerEconomy& me = (player == 1) ? p1 : p2;
    const PlayerEconomy& rival = (player == 1) ? p2 : p1;
    const int idx = player - 1;
    const float sc = priceScale(currentDay);
    std::string msg;

    // 1. Council: answer after a human-like pause
    const CouncilState& c = politics.council;
    float reaction = (skill == 1) ? 7.0f : (skill == 2 ? 4.5f : 2.5f);
    if (c.active && c.choice[idx] < 0 && c.elapsed >= reaction) {
        int pick = suggestCouncilOption(player, skill);
        if (pick < 0 || !chooseCouncilOption(player, pick, msg)) {
            chooseCouncilOption(player, getCouncilCard(c.cardId).defaultOption, msg);
        }
    }

    // 2. Sealed-bid tenders: bid once when the night capacity looks good enough
    for (const auto& k : politics.contracts) {
        if (k.kind != ContractKind::TENDER_NIGHT || k.state != ContractState::BIDDING || k.bid[idx] > 0) continue;
        float nightMW = 0.0f;
        for (const auto& b : buildings) {
            if (b.playerOwner != player) continue;
            if (b.type == BuildingType::BATTERY) nightMW += 30.0f;
            else if (b.type == BuildingType::WIND_TURBINE) nightMW += 0.75f * Balance::WIND_TURBINE.basePowerMW;
            else if (b.type == BuildingType::HYDRO_PLANT) nightMW += 0.9f * Balance::HYDRO_PLANT.basePowerMW;
        }
        float need = static_cast<float>(k.target) * (skill == 3 ? 1.0f : 1.2f);
        if (nightMW < need) continue;
        int step = getTenderBidStep();
        int steps = skill + static_cast<int>(politics.rng() % 2u);
        int amount = std::min(steps * step, static_cast<int>(me.money * 0.35f) / step * step);
        if (amount >= step) placeBid(player, k.id, amount, msg);
    }

    // 3. Emergency import / export policy (base = own generation without today's flows)
    int demand = city.cityEnergyDemand;
    int myBase = me.energyMW - politics.imports.flowMW[idx] + politics.imports.flowMW[1 - idx];
    int rivalBase = rival.energyMW - politics.imports.flowMW[1 - idx] + politics.imports.flowMW[idx];
    bool wantImport = demand > 0 && myBase < demand && me.money > static_cast<int>(800.0f * sc);
    if (wantImport != politics.imports.request[idx]) setImportRequest(player, wantImport);
    bool allow = true;
    if (skill >= 2 && demand > 0 && rivalBase < demand && myBase >= demand) allow = false; // keep the edge
    if (allow != politics.imports.exportAllowed[idx]) setExportAllowed(player, allow);

    // 4. Exchange: spend idle money on the scarcest building material (and gold when rich)
    if (skill >= 2 && me.money > static_cast<int>(6000.0f * sc)) {
        const ResourceType mats[5] = { ResourceType::WOOD, ResourceType::IRON, ResourceType::COPPER,
                                       ResourceType::SILICON, ResourceType::COAL };
        const int stock[5] = { me.wood, me.iron, me.copper, me.silicon, me.coal };
        int lowest = 0;
        for (int i = 1; i < 5; ++i) {
            if (stock[i] < stock[lowest]) lowest = i;
        }
        if (stock[lowest] < 40 && getMarketMultiplier(mats[lowest]) < 1.6f) {
            tradeResource(player, mats[lowest], true, msg);
        }
    }
    if (skill >= 3 && me.money > static_cast<int>(12000.0f * sc) && getMarketMultiplier(ResourceType::GOLD) < 1.5f) {
        tradeResource(player, ResourceType::GOLD, true, msg);
    }
}
