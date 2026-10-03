// =============================================================================
// ENERGY CRISIS - CITY CONTRACT BOARD (F-16)  [team b-politics]
// Every day from day 3 the city posts 2 contracts (plus a sealed-bid tender from day 4).
// Contracts are judged automatically from the simulation:
//   window contracts  - average MW delivered inside a time window (evening / morning / night)
//   point contracts   - batteries stored at sunset, powered lamps at 22:00
//   record race       - the first player to deliver X MW at once wins (exclusive)
//   tender            - sealed bids (escrowed money) until 12:00; only the winner may deliver
// Rewards: gold and city share; a failed tender costs 1% of the city.
// =============================================================================
#include "../includes/game_main.h"
#include <algorithm>
#include <cmath>

using namespace Politics;

namespace Politics {

const char* getContractTitle(ContractKind kind) {
    switch (kind) {
        case ContractKind::EVENING_PEAK:   return "ВЕЧЕРЕН ПИК";
        case ContractKind::MORNING_PEAK:   return "СУТРЕШЕН ПИК";
        case ContractKind::STORAGE_SUNSET: return "РЕЗЕРВ ЗА НОЩТА";
        case ContractKind::LAMPS_22:       return "ОСВЕТЕНИ УЛИЦИ";
        case ContractKind::RECORD_RACE:    return "РЕКОРДНА МОЩНОСТ";
        case ContractKind::TENDER_NIGHT:   return "ТЪРГ: НОЩНА БОЛНИЦА";
        default:                           return "ДОГОВОР";
    }
}

void getContractWindow(ContractKind kind, int seasonIndex, float& startDayHour, float& endDayHour) {
    const float h0 = Balance::CLOCK_HOUR_AT_ZERO; // day-hours count from 06:00
    switch (kind) {
        case ContractKind::EVENING_PEAK:   startDayHour = 18.0f - h0; endDayHour = 22.0f - h0; break;
        case ContractKind::MORNING_PEAK:   startDayHour = 6.0f - h0;  endDayHour = 9.0f - h0;  break;
        case ContractKind::STORAGE_SUNSET: {
            float s = Balance::getSunsetHour(static_cast<SeasonType>(std::max(0, std::min(seasonIndex, 3)))) - h0;
            startDayHour = endDayHour = s;
            break;
        }
        case ContractKind::LAMPS_22:       startDayHour = endDayHour = 22.0f - h0; break;
        case ContractKind::RECORD_RACE:    startDayHour = 6.0f - h0;  endDayHour = 18.0f - h0; break;
        case ContractKind::TENDER_NIGHT:   startDayHour = 22.0f - h0; endDayHour = 28.0f - h0; break; // 22:00-04:00
        default:                           startDayHour = 0.0f;       endDayHour = 0.0f;       break;
    }
}

} // namespace Politics

namespace {
int round5(float v) { return std::max(5, static_cast<int>(std::lround(v / 5.0f)) * 5); }
int round10(float v) { return std::max(10, static_cast<int>(std::lround(v / 10.0f)) * 10); }
const char* tag(int player) { return player == 1 ? "ИГРАЧ 1" : "ИГРАЧ 2"; }
} // namespace

int GameEngine::getTenderBidStep() const {
    return std::max(50, static_cast<int>(std::lround(500.0f * priceScale(currentDay) / 50.0f)) * 50);
}

int GameEngine::getPoweredLampCount(int player) const {
    int n = 0;
    for (const auto& b : buildings) {
        if (b.playerOwner == player && b.type == BuildingType::LAMP && b.currentOutputMW < 0.0f) ++n;
    }
    return n;
}

float GameEngine::getStoredBatteryMWh(int player) const {
    float s = 0.0f;
    for (const auto& b : buildings) {
        if (b.playerOwner == player && b.type == BuildingType::BATTERY) s += b.energyStored;
    }
    return s;
}

namespace {
// Contract terms scale with the day and with today's (event-adjusted) city demand
CityContract makeContract(ContractKind kind, int day, int demand) {
    CityContract k;
    k.kind = kind;
    k.state = ContractState::OPEN;
    k.rewardGold = 25 + 6 * day;
    k.rewardShareTenths = 10;
    switch (kind) {
        case ContractKind::EVENING_PEAK: k.target = round5(demand * 0.8f + 10.0f); break;
        case ContractKind::MORNING_PEAK: k.target = round5(demand * 0.7f + 10.0f); break;
        case ContractKind::STORAGE_SUNSET: k.target = std::min(600, round10(100.0f + 25.0f * (day - 3))); break;
        case ContractKind::LAMPS_22: k.target = 2 + (day - 3) / 5; break;
        case ContractKind::RECORD_RACE:
            k.target = round10(std::max(120.0f, demand * 2.2f));
            k.rewardGold = 40 + 8 * day;
            k.rewardShareTenths = 20;
            break;
        case ContractKind::TENDER_NIGHT:
            k.state = ContractState::BIDDING;
            k.target = round5(demand * 0.6f + 20.0f);
            k.rewardGold = 60 + 10 * day;
            k.rewardShareTenths = 30;
            break;
        default: break;
    }
    return k;
}
} // namespace

int GameEngine::debugPostContract(ContractKind kind) {
    CityContract k = makeContract(kind, currentDay, std::max(Balance::STARTING_CITY_DEMAND_MW, city.cityEnergyDemand));
    k.id = politics.nextContractId++;
    politics.contracts.push_back(k);
    return k.id;
}

void GameEngine::postDailyContracts(int day) {
    politics.contracts.clear();
    if (day < FIRST_CONTRACT_DAY || day > Balance::FINAL_DAY) return;

    const int demand = std::max(Balance::STARTING_CITY_DEMAND_MW, city.cityEnergyDemand);
    auto make = [&](ContractKind kind) {
        CityContract k = makeContract(kind, day, demand);
        k.id = politics.nextContractId++;
        return k;
    };

    // Two different daily contracts; a festival day always brings festival lights
    std::vector<ContractKind> pool = { ContractKind::EVENING_PEAK, ContractKind::MORNING_PEAK,
                                       ContractKind::STORAGE_SUNSET, ContractKind::LAMPS_22,
                                       ContractKind::RECORD_RACE };
    std::shuffle(pool.begin(), pool.end(), politics.rng);
    bool festival = getFestivalOnDay(day) != EventId::NONE;
    if (festival) {
        CityContract k = make(ContractKind::LAMPS_22);
        k.festival = true;
        k.target += 1;
        k.rewardGold = k.rewardGold * 3 / 2;
        k.rewardShareTenths = 20;
        politics.contracts.push_back(k);
        pool.erase(std::remove(pool.begin(), pool.end(), ContractKind::LAMPS_22), pool.end());
    }
    while (politics.contracts.size() < 2 && !pool.empty()) {
        politics.contracts.push_back(make(pool.back()));
        pool.pop_back();
    }

    if (day >= FIRST_TENDER_DAY && std::uniform_int_distribution<int>(0, 99)(politics.rng) < TENDER_CHANCE_PCT) {
        politics.contracts.push_back(make(ContractKind::TENDER_NIGHT));
    }

    politicsNotice("ГРАДЪТ ПУБЛИКУВА " + std::to_string(politics.contracts.size()) + " НОВИ ДОГОВОРА ЗА ДЕН " +
                       std::to_string(day) + "!", 0, Tone::NEUTRAL);
}

bool GameEngine::placeBid(int player, int contractId, int newTotalBid, std::string& outMsg) {
    if (player != 1 && player != 2) return false;
    for (auto& k : politics.contracts) {
        if (k.id != contractId) continue;
        if (k.kind != ContractKind::TENDER_NIGHT || k.state != ContractState::BIDDING) {
            outMsg = "ОФЕРТИТЕ ЗА ТОЗИ ТЪРГ СА ЗАТВОРЕНИ!";
            return false;
        }
        const int idx = player - 1;
        newTotalBid = std::max(0, newTotalBid);
        PlayerEconomy& e = (player == 1) ? p1 : p2;
        int delta = newTotalBid - k.bid[idx];
        if (delta > e.money) {
            outMsg = "НЕДОСТИГ НА ПАРИ ЗА ОФЕРТАТА! НУЖНИ: " + std::to_string(delta) + "$";
            return false;
        }
        e.money -= delta; // escrow (a lower bid returns the difference)
        e.data.money = e.money;
        k.bid[idx] = newTotalBid;
        if (delta > 0) k.bidTime[idx] = dayHourNow();
        outMsg = (newTotalBid > 0) ? "ЗАПЕЧАТАНА ОФЕРТА: " + std::to_string(newTotalBid) + "$" : "ОФЕРТАТА Е ОТТЕГЛЕНА";
        return true;
    }
    outMsg = "НЯМА ТАКЪВ ТЪРГ!";
    return false;
}

void GameEngine::updateContracts(float dt, float prevDayHour, float dayHour) {
    const int season = static_cast<int>(currentSeason);
    const int mw[2] = { p1.energyMW, p2.energyMW };

    auto reward = [&](CityContract& k, int player) {
        PlayerEconomy& e = (player == 1) ? p1 : p2;
        k.completed[player - 1] = true;
        e.gold += k.rewardGold;
        e.data.gold = e.gold;
        addCityShare(player, k.rewardShareTenths);
        politicsNotice(std::string(tag(player)) + " ИЗПЪЛНИ \"" + getContractTitle(k.kind) + "\": +" +
                           std::to_string(k.rewardGold) + " G, +" + std::to_string(k.rewardShareTenths / 10) + "% ОТ ГРАДА",
                       player, Tone::GOOD);
    };
    auto crossed = [&](float t) { return prevDayHour < t && dayHour >= t; };

    for (auto& k : politics.contracts) {
        float start = 0.0f, end = 0.0f;
        getContractWindow(k.kind, season, start, end);
        const bool inWindow = dayHour > start && prevDayHour < end;

        switch (k.kind) {
            case ContractKind::EVENING_PEAK:
            case ContractKind::MORNING_PEAK: {
                if (k.state != ContractState::OPEN) break;
                if (inWindow) {
                    k.seconds += dt;
                    for (int i = 0; i < 2; ++i) {
                        k.sum[i] += static_cast<float>(mw[i]) * dt;
                        k.best[i] = k.sum[i] / std::max(0.001f, k.seconds);
                    }
                }
                if (crossed(end)) {
                    for (int i = 0; i < 2; ++i) {
                        float avg = (k.seconds > 0.0f) ? k.sum[i] / k.seconds : 0.0f;
                        if (avg + 0.01f >= static_cast<float>(k.target)) reward(k, i + 1);
                    }
                    k.state = (k.completed[0] || k.completed[1]) ? ContractState::DONE : ContractState::EXPIRED;
                }
                break;
            }
            case ContractKind::STORAGE_SUNSET:
            case ContractKind::LAMPS_22: {
                if (k.state != ContractState::OPEN) break;
                for (int i = 0; i < 2; ++i) {
                    k.best[i] = (k.kind == ContractKind::LAMPS_22) ? static_cast<float>(getPoweredLampCount(i + 1))
                                                                     : getStoredBatteryMWh(i + 1);
                }
                if (crossed(end)) {
                    for (int i = 0; i < 2; ++i) {
                        if (k.best[i] + 0.01f >= static_cast<float>(k.target)) reward(k, i + 1);
                    }
                    k.state = (k.completed[0] || k.completed[1]) ? ContractState::DONE : ContractState::EXPIRED;
                }
                break;
            }
            case ContractKind::RECORD_RACE: {
                if (k.state != ContractState::OPEN) break;
                if (inWindow) {
                    bool hit[2] = { false, false };
                    for (int i = 0; i < 2; ++i) {
                        k.best[i] = std::max(k.best[i], static_cast<float>(mw[i]));
                        hit[i] = mw[i] >= k.target;
                    }
                    if (hit[0] || hit[1]) {
                        // Exclusive: the higher output wins a same-moment finish, a dead heat pays both
                        if (hit[0] && (!hit[1] || mw[0] >= mw[1])) reward(k, 1);
                        if (hit[1] && (!hit[0] || mw[1] >= mw[0])) reward(k, 2);
                        k.state = ContractState::DONE;
                        break;
                    }
                }
                if (crossed(end)) k.state = ContractState::EXPIRED;
                break;
            }
            case ContractKind::TENDER_NIGHT: {
                const float closeAt = TENDER_BID_CLOSE_HOUR - Balance::CLOCK_HOUR_AT_ZERO;
                if (k.state == ContractState::BIDDING && crossed(closeAt)) {
                    int w = 0;
                    if (k.bid[0] > 0 || k.bid[1] > 0) {
                        if (k.bid[0] != k.bid[1]) w = (k.bid[0] > k.bid[1]) ? 1 : 2;
                        else w = (k.bidTime[0] <= k.bidTime[1]) ? 1 : 2; // same amount: the earlier offer wins
                    }
                    if (w == 0) {
                        k.state = ContractState::EXPIRED;
                        politicsNotice("ТЪРГЪТ \"НОЩНА БОЛНИЦА\" НЯМА КАНДИДАТИ И Е ЗАКРИТ.", 0, Tone::NEUTRAL);
                    } else {
                        int loser = 3 - w;
                        PlayerEconomy& le = (loser == 1) ? p1 : p2;
                        le.money += k.bid[loser - 1]; // escrow back to the loser
                        le.data.money = le.money;
                        k.paidBid = k.bid[w - 1];
                        k.tenderWinner = w;
                        k.state = ContractState::ACTIVE;
                        politicsNotice(std::string(tag(w)) + " СПЕЧЕЛИ ТЪРГА С ОФЕРТА " + std::to_string(k.paidBid) + "$ (" +
                                           std::to_string(k.bid[0]) + "$ СРЕЩУ " + std::to_string(k.bid[1]) + "$)",
                                       w, Tone::NEUTRAL);
                    }
                }
                if (k.state == ContractState::ACTIVE) {
                    int w = k.tenderWinner;
                    if (inWindow) {
                        k.seconds += dt;
                        k.sum[w - 1] += static_cast<float>(mw[w - 1]) * dt;
                        k.best[w - 1] = k.sum[w - 1] / std::max(0.001f, k.seconds);
                    }
                    if (crossed(end)) {
                        float avg = (k.seconds > 0.0f) ? k.sum[w - 1] / k.seconds : 0.0f;
                        if (avg + 0.01f >= static_cast<float>(k.target)) {
                            reward(k, w);
                        } else {
                            k.failed[w - 1] = true;
                            addCityShare(w, -10);
                            politicsNotice(std::string(tag(w)) + " НЕ ИЗПЪЛНИ ТЪРГА: -1% ОТ ГРАДА", w, Tone::BAD);
                        }
                        k.state = ContractState::DONE;
                    }
                }
                break;
            }
            default:
                break;
        }
    }
}

void GameEngine::settleContractsAtDayEnd() {
    for (auto& k : politics.contracts) {
        if (k.kind == ContractKind::TENDER_NIGHT && k.state == ContractState::BIDDING) {
            for (int i = 0; i < 2; ++i) { // never closed (should not happen): return every escrow
                PlayerEconomy& e = (i == 0) ? p1 : p2;
                e.money += k.bid[i];
                e.data.money = e.money;
                k.bid[i] = 0;
            }
        }
        if (k.state == ContractState::OPEN || k.state == ContractState::BIDDING || k.state == ContractState::ACTIVE) {
            k.state = ContractState::EXPIRED;
        }
    }
}
