// =============================================================================
// ENERGY CRISIS - CITY COMMODITY EXCHANGE & CROSS-RIVER POWER IMPORT (F-31)  [team b-politics]
// Exchange: both players trade lots of the 7 resources for money on ONE shared market.
//   Buying pushes the price up for both players (+8% per lot, up to 3x), selling pushes it
//   down (-6% per lot, down to 0.5x); every night prices relax 20% towards normal.
//   Prices also follow the match inflation (+8% per day) because city income grows.
// Import: a player who cannot cover the city demand may request up to 60 MW of the rival's
//   SURPLUS (above the demand) for money per MW per game-second. The exporter decides with a
//   toggle: take the money, or block it and keep the rival short of the city demand.
// =============================================================================
#include "../includes/game_main.h"
#include <algorithm>
#include <cmath>

using namespace Politics;

namespace {

// Indexed by ResourceType (WOOD = 1 .. GOLD = 7)
const float kBasePrice[MARKET_SLOTS] = { 0.0f, 20.0f, 30.0f, 36.0f, 30.0f, 44.0f, 60.0f, 100.0f };
const int kLotSize[MARKET_SLOTS] = { 0, 10, 10, 10, 10, 10, 5, 5 };
constexpr float BUY_IMPACT = 1.08f;
constexpr float SELL_IMPACT = 0.94f;
constexpr float MULT_MIN = 0.5f;
constexpr float MULT_MAX = 3.0f;
constexpr float SELL_SPREAD = 0.45f; // the city buys back at 45% of its selling price

bool tradable(ResourceType t) {
    int i = static_cast<int>(t);
    return i >= static_cast<int>(ResourceType::WOOD) && i <= static_cast<int>(ResourceType::GOLD);
}

int* resourceSlot(PlayerEconomy& e, ResourceType t) {
    switch (t) {
        case ResourceType::WOOD:    return &e.wood;
        case ResourceType::IRON:    return &e.iron;
        case ResourceType::COPPER:  return &e.copper;
        case ResourceType::COAL:    return &e.coal;
        case ResourceType::SILICON: return &e.silicon;
        case ResourceType::SILVER:  return &e.silver;
        case ResourceType::GOLD:    return &e.gold;
        default:                    return nullptr;
    }
}

void syncData(PlayerEconomy& e) {
    e.data.money = e.money;
    e.data.wood = e.wood;
    e.data.iron = e.iron;
    e.data.copper = e.copper;
    e.data.coal = e.coal;
    e.data.silicon = e.silicon;
    e.data.silver = e.silver;
    e.data.gold = e.gold;
}

const char* resourceNameBg(ResourceType t) {
    switch (t) {
        case ResourceType::WOOD:    return "Дърво";
        case ResourceType::IRON:    return "Желязо";
        case ResourceType::COPPER:  return "Мед";
        case ResourceType::COAL:    return "Въглища";
        case ResourceType::SILICON: return "Силиций";
        case ResourceType::SILVER:  return "Сребро";
        case ResourceType::GOLD:    return "Злато";
        default:                    return "?";
    }
}

float inflation(int day) { return 1.0f + 0.08f * static_cast<float>(std::max(0, day - 1)); }

} // namespace

// =============================================================================
// Exchange
// =============================================================================
int GameEngine::getMarketLotSize(ResourceType type) const {
    return tradable(type) ? kLotSize[static_cast<int>(type)] : 0;
}

float GameEngine::getMarketMultiplier(ResourceType type) const {
    return tradable(type) ? politics.market.mult[static_cast<int>(type)] : 1.0f;
}

int GameEngine::getMarketBuyPrice(ResourceType type) const {
    if (!tradable(type)) return 0;
    int i = static_cast<int>(type);
    float p = kBasePrice[i] * politics.market.mult[i] * inflation(currentDay) * static_cast<float>(kLotSize[i]);
    return std::max(1, static_cast<int>(std::lround(p)));
}

int GameEngine::getMarketSellPrice(ResourceType type) const {
    if (!tradable(type)) return 0;
    return std::max(1, static_cast<int>(std::lround(getMarketBuyPrice(type) * SELL_SPREAD)));
}

bool GameEngine::tradeResource(int player, ResourceType type, bool buy, std::string& outMsg) {
    if ((player != 1 && player != 2) || !tradable(type)) {
        outMsg = "ТОЗИ РЕСУРС НЕ СЕ ТЪРГУВА!";
        return false;
    }
    PlayerEconomy& e = (player == 1) ? p1 : p2;
    int* stock = resourceSlot(e, type);
    const int i = static_cast<int>(type);
    const int lot = kLotSize[i];
    if (buy) {
        int price = getMarketBuyPrice(type);
        if (e.money < price) {
            outMsg = "НЕДОСТИГ НА ПАРИ! " + std::to_string(lot) + " " + resourceNameBg(type) + " СТРУВАТ " +
                     std::to_string(price) + "$";
            return false;
        }
        e.money -= price;
        *stock += lot;
        politics.market.mult[i] = std::min(MULT_MAX, politics.market.mult[i] * BUY_IMPACT);
        politics.market.boughtToday[i] += lot;
        outMsg = "КУПИХТЕ " + std::to_string(lot) + " " + resourceNameBg(type) + " ЗА " + std::to_string(price) + "$";
    } else {
        if (*stock < lot) {
            outMsg = "НЯМАТЕ " + std::to_string(lot) + " " + resourceNameBg(type) + " ЗА ПРОДАЖБА!";
            return false;
        }
        int price = getMarketSellPrice(type);
        *stock -= lot;
        e.money += price;
        politics.market.mult[i] = std::max(MULT_MIN, politics.market.mult[i] * SELL_IMPACT);
        politics.market.soldToday[i] += lot;
        outMsg = "ПРОДАДОХТЕ " + std::to_string(lot) + " " + resourceNameBg(type) + " ЗА " + std::to_string(price) + "$";
    }
    syncData(e);
    return true;
}

// =============================================================================
// Cross-river emergency import
// =============================================================================
float GameEngine::getImportPricePerMWs() const {
    return 0.5f * (1.0f + 0.1f * static_cast<float>(std::max(0, currentDay - 3)));
}

int GameEngine::getImportFlowMW(int player) const {
    return (player == 1 || player == 2) ? politics.imports.flowMW[player - 1] : 0;
}

void GameEngine::setImportRequest(int player, bool on) {
    if (player != 1 && player != 2) return;
    bool& r = politics.imports.request[player - 1];
    if (r == on) return;
    r = on;
    politicsNotice(std::string(player == 1 ? "ИГРАЧ 1" : "ИГРАЧ 2") +
                       (on ? " ЗАЯВИ АВАРИЕН ВНОС НА ТОК ОТ СЪПЕРНИКА!" : " СПРЯ ВНОСА НА ТОК."),
                   0, on ? Tone::BAD : Tone::NEUTRAL);
}

void GameEngine::setExportAllowed(int player, bool on) {
    if (player != 1 && player != 2) return;
    bool& a = politics.imports.exportAllowed[player - 1];
    if (a == on) return;
    a = on;
    politicsNotice(std::string(player == 1 ? "ИГРАЧ 1" : "ИГРАЧ 2") +
                       (on ? " РАЗРЕШИ ИЗНОС НА ТОК КЪМ СЪПЕРНИКА." : " БЛОКИРА ИЗНОСА НА ТОК КЪМ СЪПЕРНИКА!"),
                   0, on ? Tone::GOOD : Tone::BAD);
}

void GameEngine::applyPowerImport(float dt) {
    ImportState& im = politics.imports;
    im.flowMW[0] = im.flowMW[1] = 0;
    if (!cityPoliticsEnabled) return;
    const int demand = city.cityEnergyDemand;
    if (demand <= 0) return;

    for (int i = 0; i < 2; ++i) {
        const int j = 1 - i;
        // Both asking means nobody has surplus to spare; the exporter's switch has the last word
        if (!im.request[i] || im.request[j] || !im.exportAllowed[j]) continue;
        PlayerEconomy& importer = (i == 0) ? p1 : p2;
        PlayerEconomy& exporter = (i == 0) ? p2 : p1;
        if (importer.money <= 0) continue;
        int deficit = demand - importer.energyMW;
        int surplus = exporter.energyMW - demand;
        int x = std::min(IMPORT_MAX_MW, std::min(deficit, surplus));
        if (x <= 0) continue;

        importer.energyMW += x;
        exporter.energyMW -= x;
        float& impDelivered = (i == 0) ? city.p1DailyDelivered : city.p2DailyDelivered;
        float& expDelivered = (i == 0) ? city.p2DailyDelivered : city.p1DailyDelivered;
        impDelivered += static_cast<float>(x) * dt;
        expDelivered -= static_cast<float>(x) * dt;

        im.moneyAccum[i] += static_cast<float>(x) * getImportPricePerMWs() * dt;
        int pay = static_cast<int>(std::floor(im.moneyAccum[i]));
        if (pay > 0) {
            pay = std::min(pay, importer.money);
            importer.money -= pay;
            exporter.money += pay;
            im.moneyAccum[i] -= static_cast<float>(pay);
            im.paidToday[i] += pay;
            im.earnedToday[j] += pay;
            syncData(importer);
            syncData(exporter);
        }
        im.flowMW[i] = x;
    }
}
