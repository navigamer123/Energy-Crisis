// =============================================================================
// [b-politics] Player overlays: council decision card (F-13) and the city hall panel
// (КМЕТСТВО) with the exchange (F-31), power import (F-31) and tender (F-16) tabs.
// Each overlay sits over its own player's land plots (P1 x 255..600, P2 x 1000..1345).
// =============================================================================
#include "../includes/UI_politics.h"
#include "../includes/UI_politics_draw.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace Politics;
using namespace PolUi;

namespace {

constexpr float AREA_W = 345.0f;
constexpr float PANEL_Y = 102.0f;
constexpr float PANEL_H = 412.0f;
constexpr float CARD_Y = 102.0f;
constexpr float CARD_H = 412.0f;
constexpr float WAIT_Y = 60.0f;
constexpr float WAIT_H = 36.0f;

float areaX(int player) { return player == 1 ? 255.0f : 1000.0f; }

const char* kResName[MARKET_SLOTS] = { "", "Дърво", "Желязо", "Мед", "Въглища", "Силиций", "Сребро", "Злато" };

int stockOf(const PlayerEconomy& e, int slot) {
    switch (slot) {
        case 1: return e.wood;
        case 2: return e.iron;
        case 3: return e.copper;
        case 4: return e.coal;
        case 5: return e.silicon;
        case 6: return e.silver;
        case 7: return e.gold;
        default: return 0;
    }
}

// A clickable button; records its hit box and returns whether the mouse hovers it
bool button(sf::RenderTarget& t, const sf::Font& font, sf::FloatRect r, const std::string& label, sf::Color accent,
            bool enabled, sf::Vector2f mouse, bool mouseMine, unsigned size = 11) {
    bool hover = enabled && mouseMine && r.contains(mouse);
    sf::Color fill = enabled ? withAlpha(accent, hover ? 110 : 45) : sf::Color(34, 40, 52);
    sf::Color edge = enabled ? withAlpha(accent, hover ? 255 : 170) : sf::Color(60, 66, 80);
    drawRect(t, r, fill, edge, 1.0f);
    drawFit(t, font, label, size, { r.position.x + r.size.x / 2.0f, r.position.y + (r.size.y - size - 4.0f) / 2.0f },
            enabled ? TEXT : TEXT_DIM, r.size.x - 8.0f, Align::CENTER, true, 9);
    return hover;
}

std::string keyHint(int player, bool botActive, const char* p1, const char* p2, const char* sp) {
    if (player == 2) return p2;
    return botActive ? sp : p1;
}

} // namespace

// =============================================================================
// Overlay dispatcher
// =============================================================================
void UI_politics::drawPlayerOverlays(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine, float animTime,
                                     sf::Vector2f mouse, int mouseOwner, bool botActive) {
    const CouncilState& c = engine.getPolitics().council;
    for (int p = 1; p <= 2; ++p) {
        const int i = p - 1;
        hits[i].clear();
        overlayRect[i] = sf::FloatRect();
        const bool isBot = (p == 2 && botActive);
        const bool mine = (mouseOwner == p);

        if (c.active && c.choice[i] < 0) {
            if (isBot) drawCouncilWaiting(t, font, engine, p, true);
            else drawCouncilCard(t, font, engine, p, animTime, mouse, mine);
            continue;
        }
        if (panelOpen[i] && !isBot) drawPanel(t, font, engine, p, mouse, mine, botActive);
        if (c.active) drawCouncilWaiting(t, font, engine, p, isBot);
    }
}

// =============================================================================
// Council card
// =============================================================================
void UI_politics::drawCouncilCard(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine, int player,
                                  float animTime, sf::Vector2f mouse, bool mouseMine) {
    const int i = player - 1;
    const CouncilState& c = engine.getPolitics().council;
    const CouncilCardDef& card = getCouncilCard(c.cardId);
    const PlayerEconomy& econ = engine.getPlayerEconomy(player);
    const sf::Color pc = playerColor(player);
    const float x = areaX(player);
    sf::FloatRect box({ x, CARD_Y }, { AREA_W, CARD_H });
    overlayRect[i] = box;

    float pulse = 0.5f + 0.5f * std::sin(animTime * 5.0f);
    drawRect(t, sf::FloatRect({ box.position.x - 4.0f, box.position.y - 4.0f }, { box.size.x + 8.0f, box.size.y + 8.0f }),
             withAlpha(GOLD, static_cast<std::uint8_t>(30 + 40 * pulse)));
    drawRect(t, box, PANEL_BG, GOLD, 2.0f);
    drawRect(t, sf::FloatRect(box.position, { AREA_W, 30.0f }), HEADER_BG);
    drawFit(t, font, "ГРАДСКИЯТ СЪВЕТ · ГЛАСУВАНЕ", 13, { x + 10.0f, box.position.y + 6.0f }, GOLD, 240.0f, Align::LEFT, true);
    int secs = static_cast<int>(std::ceil(std::max(0.0f, c.timeLeft)));
    drawFit(t, font, std::to_string(secs) + " с", 14, { x + AREA_W - 10.0f, box.position.y + 5.0f },
            secs <= 5 ? BAD : TEXT, 60.0f, Align::RIGHT, true);
    drawBar(t, sf::FloatRect({ x, box.position.y + 30.0f }, { AREA_W, 4.0f }), c.timeLeft / COUNCIL_DECISION_SEC,
            secs <= 5 ? BAD : GOLD);

    // Title + description
    drawFit(t, font, card.titleBg, 16, { x + 12.0f, box.position.y + 42.0f }, TEXT, AREA_W - 24.0f, Align::LEFT, true, 12);
    std::vector<std::string> desc = wrapText(font, card.descBg, 12, AREA_W - 24.0f);
    for (size_t k = 0; k < desc.size() && k < 2; ++k) {
        drawFit(t, font, desc[k], 12, { x + 12.0f, box.position.y + 66.0f + 16.0f * k }, TEXT_DIM, AREA_W - 24.0f);
    }

    // Options
    for (int o = 0; o < card.optionCount; ++o) {
        const CouncilOption& opt = card.options[o];
        sf::FloatRect r({ x + 10.0f, box.position.y + 112.0f + o * 62.0f }, { AREA_W - 20.0f, 54.0f });
        int cost = c.cost[o];
        bool afford = econ.money >= cost;
        bool sel = (councilSel[i] == o);
        bool hover = mouseMine && r.contains(mouse);
        drawRect(t, r, sel ? ROW_SEL_BG : ROW_BG, sel ? pc : (hover ? withAlpha(pc, 160) : PANEL_EDGE), sel ? 2.0f : 1.0f);
        drawFit(t, font, std::to_string(o + 1) + ". " + opt.labelBg, 13, { r.position.x + 10.0f, r.position.y + 7.0f },
                afford ? TEXT : TEXT_DIM, 200.0f, Align::LEFT, true);
        drawFit(t, font, opt.effectBg, 11, { r.position.x + 10.0f, r.position.y + 31.0f }, afford ? GOOD : TEXT_DIM, 185.0f);
        std::string price = (cost > 0) ? money(cost) + "$" : "БЕЗПЛАТНО";
        drawFit(t, font, price, 13, { r.position.x + r.size.x - 10.0f, r.position.y + 7.0f }, afford ? GOLD : BAD, 100.0f,
                Align::RIGHT, true);
        if (!afford) {
            drawFit(t, font, "НЯМАТЕ ПАРИ", 10, { r.position.x + r.size.x - 10.0f, r.position.y + 32.0f }, BAD, 120.0f,
                    Align::RIGHT);
        } else if (o == card.defaultOption) {
            drawFit(t, font, "ПО ПОДРАЗБИРАНЕ", 10, { r.position.x + r.size.x - 10.0f, r.position.y + 32.0f }, TEXT_DIM,
                    125.0f, Align::RIGHT);
        }
        if (afford) hits[i].push_back({ r, ACT_COUNCIL, o });
    }

    // Wallet + keys + last message
    float fy = box.position.y + 314.0f;
    drawFit(t, font, "Вашите пари: " + money(econ.money) + "$", 12, { x + 12.0f, fy }, TEXT, AREA_W - 24.0f);
    std::string keys = keyHint(player, false, "[W/S] ИЗБОР · [SPACE] ГЛАСУВАЙ · [X] ОТКАЖИ",
                               "[↑/↓] ИЗБОР · [ENTER] ГЛАСУВАЙ · [DEL] ОТКАЖИ", "");
    drawFit(t, font, keys, 11, { x + AREA_W / 2.0f, fy + 26.0f }, GOLD, AREA_W - 20.0f, Align::CENTER);
    if (flashTimer[i] > 0.0f && !flashOk[i]) {
        drawFit(t, font, flash[i], 11, { x + AREA_W / 2.0f, fy + 52.0f }, BAD, AREA_W - 20.0f, Align::CENTER);
    } else {
        drawFit(t, font, "Съперникът гласува тайно в същото време.", 11, { x + AREA_W / 2.0f, fy + 52.0f }, TEXT_DIM,
                AREA_W - 20.0f, Align::CENTER);
    }
    drawFit(t, font, "Ако не гласувате: " + std::string(card.options[card.defaultOption].labelBg), 11,
            { x + AREA_W / 2.0f, fy + 72.0f }, TEXT_DIM, AREA_W - 20.0f, Align::CENTER);
}

void UI_politics::drawCouncilWaiting(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine, int player,
                                     bool isBot) {
    const int i = player - 1;
    const CouncilState& c = engine.getPolitics().council;
    const CouncilCardDef& card = getCouncilCard(c.cardId);
    const float x = areaX(player);
    sf::FloatRect r({ x, WAIT_Y }, { AREA_W, WAIT_H });
    drawRect(t, r, PANEL_BG, withAlpha(GOLD, 200), 1.5f);
    int secs = static_cast<int>(std::ceil(std::max(0.0f, c.timeLeft)));
    std::string head;
    if (isBot) {
        head = (c.choice[i] >= 0) ? "БОТЪТ ГЛАСУВА ТАЙНО" : "БОТЪТ ОБМИСЛЯ РЕШЕНИЕТО...";
    } else {
        head = "ГЛАСУВАХТЕ: " + std::string(card.options[std::max(0, c.choice[i])].labelBg);
    }
    drawFit(t, font, head, 12, { x + 10.0f, r.position.y + 3.0f }, isBot ? playerColor(player) : TEXT, AREA_W - 70.0f,
            Align::LEFT, true);
    drawFit(t, font, std::string(card.titleBg) + " · изчакване на съперника", 10, { x + 10.0f, r.position.y + 20.0f },
            TEXT_DIM, AREA_W - 70.0f);
    drawFit(t, font, std::to_string(secs) + " с", 13, { x + AREA_W - 10.0f, r.position.y + 9.0f }, GOLD, 50.0f,
            Align::RIGHT, true);
}

// =============================================================================
// City hall panel
// =============================================================================
void UI_politics::drawPanel(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine, int player,
                            sf::Vector2f mouse, bool mouseMine, bool botActive) {
    const int i = player - 1;
    const sf::Color pc = playerColor(player);
    const PlayerEconomy& econ = engine.getPlayerEconomy(player);
    const float x = areaX(player);
    sf::FloatRect box({ x, PANEL_Y }, { AREA_W, PANEL_H });
    overlayRect[i] = box;

    drawRect(t, box, PANEL_BG, pc, 2.0f);
    drawRect(t, sf::FloatRect(box.position, { AREA_W, 30.0f }), HEADER_BG);
    drawFit(t, font, player == 1 ? "КМЕТСТВО · ИГРАЧ 1" : "КМЕТСТВО · ИГРАЧ 2", 14, { x + 10.0f, box.position.y + 6.0f },
            pc, 200.0f, Align::LEFT, true);
    sf::FloatRect closeR({ x + AREA_W - 126.0f, box.position.y + 5.0f }, { 118.0f, 20.0f });
    button(t, font, closeR, player == 1 ? "ЗАТВОРИ [C]" : "ЗАТВОРИ [Home]", BAD, true, mouse,
           mouseMine, 10);
    hits[i].push_back({ closeR, ACT_CLOSE, 0 });

    // Wallet and active bonuses
    float wy = box.position.y + 36.0f;
    drawFit(t, font, "Пари: " + money(econ.money) + "$", 13, { x + 10.0f, wy }, GOLD, 160.0f, Align::LEFT, true);
    drawFit(t, font, "Злато: " + money(econ.gold) + " G", 13, { x + AREA_W - 10.0f, wy }, GOLD, 150.0f, Align::RIGHT, true);
    std::string bonus;
    for (const auto& b : engine.getPolitics().buffs) {
        if (b.player != player) continue;
        int pct = static_cast<int>(std::lround((b.mult - 1.0f) * 100.0f));
        const char* what = (b.kind == BuffKind::PAYOUT) ? "Приходи" : (b.kind == BuffKind::MINE ? "Добив"
                         : (b.kind == BuffKind::SOLAR ? "Слънце" : "Вятър/ВЕЦ"));
        float days = b.secondsLeft / Balance::SECONDS_PER_DAY;
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.1f", days);
        if (!bonus.empty()) bonus += " · ";
        bonus += std::string(what) + (pct >= 0 ? " +" : " ") + std::to_string(pct) + "% (" + buf + " д.)";
    }
    for (const auto& bond : engine.getPolitics().bonds) {
        if (bond.player != player) continue;
        if (!bonus.empty()) bonus += " · ";
        bonus += "Облигация +" + money(bond.money) + "$";
    }
    drawFit(t, font, bonus.empty() ? "Няма активни бонуси от съвета" : "Бонуси: " + bonus, 10, { x + 10.0f, wy + 19.0f },
            bonus.empty() ? TEXT_DIM : GOOD, AREA_W - 20.0f);

    // Tabs
    const char* tabNames[TAB_COUNT] = { "БОРСА", "ВНОС НА ТОК", "ТЪРГ" };
    const float tabW = (AREA_W - 20.0f - 8.0f) / 3.0f;
    for (int k = 0; k < TAB_COUNT; ++k) {
        sf::FloatRect tr({ x + 10.0f + k * (tabW + 4.0f), box.position.y + 72.0f }, { tabW, 24.0f });
        bool selTab = (tab[i] == k);
        bool hover = mouseMine && tr.contains(mouse);
        drawRect(t, tr, selTab ? withAlpha(pc, 120) : (hover ? withAlpha(pc, 50) : ROW_BG), selTab ? pc : PANEL_EDGE, 1.0f);
        std::string label = tabNames[k];
        if (k == TAB_TENDER && tenderId(engine) != 0) label += " •";
        drawFit(t, font, label, 12, { tr.position.x + tabW / 2.0f, tr.position.y + 4.0f }, selTab ? sf::Color::White : TEXT,
                tabW - 8.0f, Align::CENTER, true);
        hits[i].push_back({ tr, ACT_TAB, k });
    }

    sf::FloatRect area({ x + 10.0f, box.position.y + 102.0f }, { AREA_W - 20.0f, 262.0f });
    if (tab[i] == TAB_MARKET) drawMarketTab(t, font, engine, player, area, mouse, mouseMine);
    else if (tab[i] == TAB_IMPORT) drawImportTab(t, font, engine, player, area, mouse, mouseMine);
    else drawTenderTab(t, font, engine, player, area, mouse, mouseMine);

    // Last action result and key help
    float fy = box.position.y + PANEL_H - 44.0f;
    drawRect(t, sf::FloatRect({ x + 1.0f, fy - 4.0f }, { AREA_W - 2.0f, 1.0f }), PANEL_EDGE);
    if (flashTimer[i] > 0.0f) {
        drawFit(t, font, flash[i], 11, { x + AREA_W / 2.0f, fy }, flashOk[i] ? GOOD : BAD, AREA_W - 20.0f, Align::CENTER);
    }
    std::string keys;
    if (tab[i] == TAB_MARKET) {
        keys = keyHint(player, botActive, "[W/S] РЕСУРС · [SPACE] КУПИ · [X] ПРОДАЙ · [A/D] РАЗДЕЛ",
                       "[↑/↓] РЕСУРС · [ENTER] КУПИ · [DEL] ПРОДАЙ · [←/→] РАЗДЕЛ",
                       "[W/S] РЕСУРС · [SPACE] КУПИ · [X] ПРОДАЙ · [A/D] РАЗДЕЛ");
    } else if (tab[i] == TAB_IMPORT) {
        keys = keyHint(player, botActive, "[W/S] ИЗБОР · [SPACE] ПРЕВКЛЮЧИ · [A/D] РАЗДЕЛ",
                       "[↑/↓] ИЗБОР · [ENTER] ПРЕВКЛЮЧИ · [←/→] РАЗДЕЛ",
                       "[W/S] ИЗБОР · [SPACE] ПРЕВКЛЮЧИ · [A/D] РАЗДЕЛ");
    } else {
        keys = keyHint(player, botActive, "[SPACE] +ОФЕРТА · [X] -ОФЕРТА · [A/D] РАЗДЕЛ",
                       "[ENTER] +ОФЕРТА · [DEL] -ОФЕРТА · [←/→] РАЗДЕЛ",
                       "[SPACE] +ОФЕРТА · [X] -ОФЕРТА · [A/D] РАЗДЕЛ");
    }
    drawFit(t, font, keys, 10, { x + AREA_W / 2.0f, fy + 20.0f }, TEXT_DIM, AREA_W - 16.0f, Align::CENTER);
}

void UI_politics::drawMarketTab(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine, int player,
                                sf::FloatRect area, sf::Vector2f mouse, bool mouseMine) {
    const int i = player - 1;
    const sf::Color pc = playerColor(player);
    const PlayerEconomy& econ = engine.getPlayerEconomy(player);
    const float rowH = 36.0f;
    drawFit(t, font, "Цените са общи: покупката поскъпва и за съперника.", 10, { area.position.x, area.position.y },
            TEXT_DIM, area.size.x);
    for (int k = 1; k <= 7; ++k) {
        ResourceType rt = static_cast<ResourceType>(k);
        sf::FloatRect r({ area.position.x, area.position.y + 14.0f + (k - 1) * rowH }, { area.size.x, rowH - 3.0f });
        bool sel = (row[i][TAB_MARKET] == k - 1);
        drawRect(t, r, sel ? ROW_SEL_BG : ROW_BG, sel ? pc : sf::Color::Transparent, sel ? 1.0f : 0.0f);
        int lot = engine.getMarketLotSize(rt);
        drawFit(t, font, kResName[k], 12, { r.position.x + 8.0f, r.position.y + 2.0f }, TEXT, 76.0f, Align::LEFT, true);
        drawFit(t, font, "имате " + money(stockOf(econ, k)), 10, { r.position.x + 8.0f, r.position.y + 17.0f }, TEXT_DIM,
                76.0f);
        int pct = static_cast<int>(std::lround((engine.getMarketMultiplier(rt) - 1.0f) * 100.0f));
        std::string trend = (pct > 0) ? "▲" + std::to_string(pct) + "%" : (pct < 0 ? "▼" + std::to_string(-pct) + "%" : "—");
        drawFit(t, font, "x" + std::to_string(lot), 10, { r.position.x + 103.0f, r.position.y + 2.0f }, TEXT_DIM, 34.0f,
                Align::CENTER);
        drawFit(t, font, trend, 10, { r.position.x + 103.0f, r.position.y + 17.0f },
                pct > 2 ? FESTIVAL : (pct < -2 ? P1 : TEXT_DIM), 36.0f, Align::CENTER);

        int buy = engine.getMarketBuyPrice(rt);
        int sell = engine.getMarketSellPrice(rt);
        const float btnW = (r.size.x - 124.0f - 4.0f - 4.0f) / 2.0f;
        sf::FloatRect bb({ r.position.x + 124.0f, r.position.y + 4.0f }, { btnW, rowH - 11.0f });
        sf::FloatRect sb({ r.position.x + 124.0f + btnW + 4.0f, r.position.y + 4.0f }, { btnW, rowH - 11.0f });
        bool canBuy = econ.money >= buy;
        bool canSell = stockOf(econ, k) >= lot;
        button(t, font, bb, "КУПИ " + money(buy) + "$", GOOD, canBuy, mouse, mouseMine, 10);
        button(t, font, sb, "ПРОДАЙ " + money(sell) + "$", FESTIVAL, canSell, mouse, mouseMine, 10);
        hits[i].push_back({ bb, ACT_BUY, k });
        hits[i].push_back({ sb, ACT_SELL, k });
    }
}

void UI_politics::drawImportTab(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine, int player,
                                sf::FloatRect area, sf::Vector2f mouse, bool mouseMine) {
    const int i = player - 1, j = 1 - i;
    const sf::Color pc = playerColor(player);
    const ImportState& im = engine.getPolitics().imports;
    const int demand = engine.getCityState().cityEnergyDemand;
    const int myMW = engine.getPlayerEconomy(player).energyMW - im.flowMW[i] + im.flowMW[j];
    const int rivalMW = engine.getPlayerEconomy(3 - player).energyMW - im.flowMW[j] + im.flowMW[i];
    float x = area.position.x, y = area.position.y;

    // Situation
    drawFit(t, font, "Нужда на града: " + std::to_string(demand) + " MW", 12, { x, y }, TEXT, area.size.x);
    std::string mine = "Вашата мощност: " + std::to_string(myMW) + " MW";
    if (demand > 0) mine += (myMW >= demand) ? " (покривате нуждата)" : " (не достигат " + std::to_string(demand - myMW) + ")";
    drawFit(t, font, mine, 12, { x, y + 18.0f }, (demand > 0 && myMW < demand) ? BAD : GOOD, area.size.x);
    int surplus = std::max(0, rivalMW - demand);
    drawFit(t, font, "Съперник: " + std::to_string(rivalMW) + " MW (излишък " + std::to_string(surplus) + " MW)", 12,
            { x, y + 36.0f }, TEXT_DIM, area.size.x);

    // Toggles
    const char* labels[2] = { "ЗАЯВКА ЗА ВНОС (до 60 MW)", "РАЗРЕШИ ИЗНОС КЪМ СЪПЕРНИКА" };
    const bool states[2] = { im.request[i], im.exportAllowed[i] };
    for (int k = 0; k < 2; ++k) {
        sf::FloatRect r({ x, y + 60.0f + k * 40.0f }, { area.size.x, 34.0f });
        bool sel = (row[i][TAB_IMPORT] == k);
        drawRect(t, r, sel ? ROW_SEL_BG : ROW_BG, sel ? pc : sf::Color::Transparent, sel ? 1.0f : 0.0f);
        drawFit(t, font, labels[k], 12, { r.position.x + 8.0f, r.position.y + 9.0f }, TEXT, 220.0f, Align::LEFT, true);
        sf::FloatRect pill({ r.position.x + r.size.x - 86.0f, r.position.y + 6.0f }, { 78.0f, 22.0f });
        std::string st = (k == 0) ? (states[k] ? "ВКЛ" : "ИЗКЛ") : (states[k] ? "ДА" : "НЕ");
        button(t, font, pill, st, states[k] ? GOOD : BAD, true, mouse, mouseMine, 11);
        hits[i].push_back({ pill, k == 0 ? ACT_TOGGLE_IMPORT : ACT_TOGGLE_EXPORT, 0 });
        hits[i].push_back({ sf::FloatRect(r.position, { r.size.x - 90.0f, r.size.y }), k == 0 ? ACT_TOGGLE_IMPORT : ACT_TOGGLE_EXPORT, 0 });
    }

    // Live flow
    float price = engine.getImportPricePerMWs();
    char pbuf[24];
    std::snprintf(pbuf, sizeof(pbuf), "%.2f", price);
    std::string flow;
    sf::Color flowCol = TEXT_DIM;
    if (im.flowMW[i] > 0) {
        flow = "ВНАСЯТЕ " + std::to_string(im.flowMW[i]) + " MW · ПЛАЩАТЕ ~" +
               std::to_string(static_cast<int>(std::lround(im.flowMW[i] * price))) + "$/с";
        flowCol = pc;
    } else if (im.flowMW[j] > 0) {
        flow = "ИЗНАСЯТЕ " + std::to_string(im.flowMW[j]) + " MW · ПОЛУЧАВАТЕ ~" +
               std::to_string(static_cast<int>(std::lround(im.flowMW[j] * price))) + "$/с";
        flowCol = GOOD;
    } else if (im.request[i] && !im.exportAllowed[j]) {
        flow = "СЪПЕРНИКЪТ БЛОКИРА ИЗНОСА";
        flowCol = BAD;
    } else if (im.request[i] && im.request[j]) {
        flow = "И ДВАМАТА ИСКАТЕ ВНОС - НЯМА ИЗЛИШЪК";
        flowCol = BAD;
    } else if (im.request[i]) {
        flow = demand <= 0 ? "ГРАДЪТ НЕ ИСКА ТОК - НЯМА ВНОС" : "НЯМА ИЗЛИШЪК ИЛИ НЕДОСТИГ ЗА ВНОС";
    } else {
        flow = "НЯМА ПОТОК НА ТОК МЕЖДУ СЕКТОРИТЕ";
    }
    drawFit(t, font, flow, 12, { x, y + 148.0f }, flowCol, area.size.x, Align::LEFT, true);
    drawFit(t, font, "Цена: " + std::string(pbuf) + "$ за MW в секунда", 11, { x, y + 168.0f }, TEXT_DIM, area.size.x);
    drawFit(t, font, "Днес: платени " + money(im.paidToday[i]) + "$ · получени " + money(im.earnedToday[i]) + "$", 11,
            { x, y + 184.0f }, TEXT_DIM, area.size.x);
    std::vector<std::string> help = wrapText(font,
        "Внасяте до 60 MW от излишъка на съперника над нуждата на града. Съперникът решава дали да продаде: "
        "парите срещу шанса да ви остави без ток.", 10, area.size.x);
    for (size_t k = 0; k < help.size() && k < 4; ++k) {
        drawFit(t, font, help[k], 10, { x, y + 206.0f + 13.0f * k }, TEXT_DIM, area.size.x);
    }
}

void UI_politics::drawTenderTab(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine, int player,
                                sf::FloatRect area, sf::Vector2f mouse, bool mouseMine) {
    const int i = player - 1;
    const sf::Color pc = playerColor(player);
    float x = area.position.x, y = area.position.y;
    const CityContract* k = nullptr; // the same tender the bid keys act on (the first one)
    const int id = tenderId(engine);
    for (const auto& c : engine.getPolitics().contracts) {
        if (c.id == id && id != 0) k = &c;
    }
    if (!k) {
        drawFit(t, font, "ДНЕС НЯМА ТЪРГ", 14, { x + area.size.x / 2.0f, y + 40.0f }, TEXT, area.size.x, Align::CENTER, true);
        std::vector<std::string> help = wrapText(font,
            "От ден 4 градът често обявява търг за нощното захранване на болницата. Подавате запечатана оферта до "
            "12:00. По-високата печели, а другата се връща.", 12, area.size.x - 10.0f);
        for (size_t n = 0; n < help.size() && n < 6; ++n) {
            drawFit(t, font, help[n], 12, { x + area.size.x / 2.0f, y + 72.0f + 18.0f * n }, TEXT_DIM, area.size.x,
                    Align::CENTER);
        }
        return;
    }

    drawFit(t, font, getContractTitle(k->kind), 14, { x, y }, sf::Color(190, 170, 255), area.size.x, Align::LEFT, true);
    drawFit(t, font, "Средно ≥ " + std::to_string(k->target) + " MW между 22:00 и 04:00", 12, { x, y + 22.0f }, TEXT,
            area.size.x);
    drawFit(t, font, "Награда: +" + std::to_string(k->rewardGold) + " G и +" + std::to_string(k->rewardShareTenths / 10) +
                         "% от града", 12, { x, y + 40.0f }, GOLD, area.size.x);
    drawFit(t, font, "Провал: офертата се губи и -1% от града", 12, { x, y + 58.0f }, BAD, area.size.x);

    float by = y + 86.0f;
    if (k->state == ContractState::BIDDING) {
        drawFit(t, font, "Вашата оферта: " + money(k->bid[i]) + "$" + (k->bid[i] > 0 ? " (задържани)" : ""), 13,
                { x, by }, pc, area.size.x, Align::LEFT, true);
        drawFit(t, font, std::string("Съперник: ") + (k->bid[1 - i] > 0 ? "подаде оферта (сумата е тайна)" : "още няма оферта"),
                12, { x, by + 20.0f }, TEXT_DIM, area.size.x);
        float dh = std::fmod(engine.getHour24() - 6.0f + 24.0f, 24.0f);
        float left = std::max(0.0f, (TENDER_BID_CLOSE_HOUR - 6.0f) - dh);
        int h = static_cast<int>(left), m = static_cast<int>((left - h) * 60.0f);
        drawFit(t, font, "Офертите се отварят в 12:00 (след " + std::to_string(h) + " ч " + std::to_string(m) + " мин)", 12,
                { x, by + 38.0f }, GOLD, area.size.x);
        int step = engine.getTenderBidStep();
        const PlayerEconomy& econ = engine.getPlayerEconomy(player);
        sf::FloatRect up({ x, by + 66.0f }, { area.size.x / 2.0f - 4.0f, 36.0f });
        sf::FloatRect down({ x + area.size.x / 2.0f + 4.0f, by + 66.0f }, { area.size.x / 2.0f - 4.0f, 36.0f });
        button(t, font, up, "+" + money(step) + "$ ОФЕРТА", GOOD, econ.money >= step, mouse, mouseMine, 12);
        button(t, font, down, "-" + money(step) + "$", FESTIVAL, k->bid[i] > 0, mouse, mouseMine, 12);
        hits[i].push_back({ up, ACT_BID_UP, 0 });
        hits[i].push_back({ down, ACT_BID_DOWN, 0 });
        std::vector<std::string> help = wrapText(font,
            "Само победителят плаща офертата си. Нощната мощност идва от вятър, ВЕЦ и батерии.", 11, area.size.x);
        for (size_t n = 0; n < help.size() && n < 3; ++n) {
            drawFit(t, font, help[n], 11, { x, by + 114.0f + 15.0f * n }, TEXT_DIM, area.size.x);
        }
    } else if (k->tenderWinner > 0) {
        bool mineWin = (k->tenderWinner == player);
        drawFit(t, font, mineWin ? "ВИЕ СПЕЧЕЛИХТЕ ТЪРГА!" : "СЪПЕРНИКЪТ СПЕЧЕЛИ ТЪРГА", 14, { x, by },
                mineWin ? GOOD : BAD, area.size.x, Align::LEFT, true);
        drawFit(t, font, "Оферти: P1 " + money(k->bid[0]) + "$ · P2 " + money(k->bid[1]) + "$", 12, { x, by + 22.0f }, TEXT,
                area.size.x);
        const int w = k->tenderWinner - 1;
        float frac = (k->target > 0) ? k->best[w] / static_cast<float>(k->target) : 0.0f;
        drawFit(t, font, "Нощна доставка: " + std::to_string(static_cast<int>(k->best[w])) + " / " + std::to_string(k->target) + " MW",
                12, { x, by + 44.0f }, TEXT_DIM, area.size.x);
        drawBar(t, sf::FloatRect({ x, by + 64.0f }, { area.size.x, 10.0f }), k->completed[w] ? 1.0f : frac,
                k->completed[w] ? GOOD : (k->failed[w] ? BAD : playerColor(k->tenderWinner)));
        std::string st = k->completed[w] ? "ДОГОВОРЪТ Е ИЗПЪЛНЕН" : (k->failed[w] ? "ДОГОВОРЪТ НЕ Е ИЗПЪЛНЕН (-1%)" : "НОЩТА ПРЕДСТОИ: 22:00 - 04:00");
        drawFit(t, font, st, 12, { x, by + 84.0f }, k->completed[w] ? GOOD : (k->failed[w] ? BAD : GOLD), area.size.x, Align::LEFT, true);
    } else {
        drawFit(t, font, "ТЪРГЪТ Е ЗАКРИТ БЕЗ ОФЕРТИ", 13, { x, by }, TEXT_DIM, area.size.x, Align::LEFT, true);
    }
}
