// =============================================================================
// [b-politics] City politics UI: state, input, bot tick and the centre city column
// (event chips, news line, exchange ticker, contract board). The player overlays
// (council card, city hall panel) are drawn in UI_politics_panels.cpp.
// =============================================================================
#include "../includes/UI_politics.h"
#include "../includes/UI_politics_draw.h"
#include <algorithm>
#include <cmath>

using namespace Politics;
using namespace PolUi;

namespace {

// Centre column under the city (x 615..985). The mines end at x 607 / start at x 997.
constexpr float COL_X = 615.0f;
constexpr float COL_W = 370.0f;
constexpr float CHIPS_Y = 430.0f;
constexpr float CHIPS_H = 48.0f;
constexpr float NEWS_Y = 482.0f;
constexpr float NEWS_H = 32.0f;
constexpr float TICKER_Y = 518.0f;
constexpr float TICKER_H = 62.0f;
constexpr float BOARD_Y = 584.0f;
constexpr float BOARD_H = 280.0f;
constexpr float TOAST_SEC = 4.5f;

const char* kResShort[MARKET_SLOTS] = { "", "Дърво", "Желязо", "Мед", "Въгл.", "Силиц.", "Сребро", "Злато" };

float dayHourOf(float hour24) { return std::fmod(hour24 - 6.0f + 24.0f, 24.0f); }

std::string hhmm(float hour24) { return Balance::formatHourMinute(std::fmod(hour24 + 24.0f, 24.0f)); }

} // namespace

// =============================================================================
// State & per-frame update
// =============================================================================
void UI_politics::reset() {
    PolUi::clearTextCache();
    for (int i = 0; i < 2; ++i) {
        panelOpen[i] = false;
        tab[i] = TAB_MARKET;
        for (int k = 0; k < TAB_COUNT; ++k) row[i][k] = 0;
        councilSel[i] = 2;
        hits[i].clear();
        overlayRect[i] = sf::FloatRect();
        flash[i].clear();
        flashTimer[i] = 0.0f;
    }
    boardHits.clear();
    toasts.clear();
    councilWasActive = false;
    botThinkTimer = 0.0f;
}

void UI_politics::update(float dt, GameEngine& engine, bool botActive, BotDifficulty botDiff) {
    lastBotActive = botActive;
    for (auto& n : engine.drainPoliticsNotices()) {
        Toast t;
        t.text = n.text;
        t.tone = n.tone;
        t.timer = TOAST_SEC;
        toasts.push_back(t);
    }
    while (toasts.size() > 6) toasts.pop_front();
    if (!toasts.empty()) {
        // A long queue scrolls faster so the news never lags far behind the game
        toasts.front().timer -= dt * (1.0f + 0.6f * static_cast<float>(toasts.size() - 1));
        if (toasts.front().timer <= 0.0f) toasts.pop_front();
    }
    for (int i = 0; i < 2; ++i) {
        if (flashTimer[i] > 0.0f) flashTimer[i] -= dt;
    }

    // A new council card starts on its free default option, so a stray key costs nothing
    const CouncilState& c = engine.getPolitics().council;
    if (c.active && !councilWasActive) {
        int def = getCouncilCard(c.cardId).defaultOption;
        councilSel[0] = councilSel[1] = def;
    }
    councilWasActive = c.active;

    if (botActive) {
        botThinkTimer += dt;
        if (botThinkTimer >= 1.0f) {
            botThinkTimer = 0.0f;
            int skill = (botDiff == BotDifficulty::HARD) ? 3 : (botDiff == BotDifficulty::MEDIUM ? 2 : 1);
            engine.politicsBotThink(2, skill);
        }
    }
}

bool UI_politics::blocksPlayer(int player, const GameEngine& engine, bool botActive) const {
    if (player != 1 && player != 2) return false;
    if (player == 2 && botActive) return false;
    const CouncilState& c = engine.getPolitics().council;
    return panelOpen[player - 1] || (c.active && c.choice[player - 1] < 0);
}

void UI_politics::setFlash(int player, const std::string& msg, bool ok) {
    flash[player - 1] = msg;
    flashTimer[player - 1] = 3.0f;
    flashOk[player - 1] = ok;
}

int UI_politics::tenderId(const GameEngine& engine) const {
    for (const auto& k : engine.getPolitics().contracts) {
        if (k.kind == ContractKind::TENDER_NIGHT) return k.id;
    }
    return 0;
}

// =============================================================================
// Input
// =============================================================================
UI_politics::Role UI_politics::keyRole(int player, sf::Keyboard::Key code, bool botActive) const {
    using K = sf::Keyboard::Key;
    auto p1Keys = [](K k) -> Role {
        switch (k) {
            case K::W: return R_UP;
            case K::S: return R_DOWN;
            case K::A: case K::Q: return R_LEFT;
            case K::D: case K::E: return R_RIGHT;
            case K::Space: return R_PRIMARY;
            case K::X: return R_SECONDARY;
            case K::C: return R_TOGGLE;
            default: return R_NONE;
        }
    };
    auto p2Keys = [](K k) -> Role {
        switch (k) {
            case K::Up: return R_UP;
            case K::Down: return R_DOWN;
            case K::Left: case K::PageUp: return R_LEFT;
            case K::Right: case K::PageDown: return R_RIGHT;
            case K::Enter: return R_PRIMARY;
            case K::Delete: return R_SECONDARY;
            case K::Home: return R_TOGGLE;
            default: return R_NONE;
        }
    };
    if (player == 1) {
        Role r = p1Keys(code);
        if (r == R_NONE && botActive) { // single player: P1 also owns P2's keys
            r = p2Keys(code);
            if (r == R_NONE && code == K::Backspace) r = R_SECONDARY;
        }
        return r;
    }
    return p2Keys(code);
}

int UI_politics::handleEvent(const sf::Event& event, GameEngine& engine, sf::Vector2f eventPos, int mouseOwner,
                             bool botActive, const bool modalOpen[2]) {
    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        for (int p = 1; p <= 2; ++p) {
            if (p == 2 && botActive) continue;
            if (modalOpen[p - 1]) continue;
            Role r = keyRole(p, key->code, botActive);
            if (r == R_NONE) continue;
            const int i = p - 1;

            const CouncilState& c = engine.getPolitics().council;
            if (c.active && c.choice[i] < 0) {
                const CouncilCardDef& card = getCouncilCard(c.cardId);
                int n = card.optionCount;
                if (r == R_UP || r == R_LEFT) councilSel[i] = (councilSel[i] + n - 1) % n;
                else if (r == R_DOWN || r == R_RIGHT) councilSel[i] = (councilSel[i] + 1) % n;
                else if (r == R_PRIMARY) doAction(p, ACT_COUNCIL, councilSel[i], engine);
                else if (r == R_SECONDARY) doAction(p, ACT_COUNCIL, card.defaultOption, engine);
                return p;
            }
            if (r == R_TOGGLE) {
                panelOpen[i] = !panelOpen[i];
                return p;
            }
            if (!panelOpen[i]) continue;

            const int rows = (tab[i] == TAB_MARKET) ? 7 : (tab[i] == TAB_IMPORT ? 2 : 1);
            int& sel = row[i][tab[i]];
            switch (r) {
                case R_UP: sel = (sel + rows - 1) % rows; break;
                case R_DOWN: sel = (sel + 1) % rows; break;
                case R_LEFT: tab[i] = (tab[i] + TAB_COUNT - 1) % TAB_COUNT; break;
                case R_RIGHT: tab[i] = (tab[i] + 1) % TAB_COUNT; break;
                case R_PRIMARY:
                case R_SECONDARY: {
                    bool primary = (r == R_PRIMARY);
                    if (tab[i] == TAB_MARKET) doAction(p, primary ? ACT_BUY : ACT_SELL, sel + 1, engine);
                    else if (tab[i] == TAB_IMPORT) doAction(p, sel == 0 ? ACT_TOGGLE_IMPORT : ACT_TOGGLE_EXPORT, 0, engine);
                    else doAction(p, primary ? ACT_BID_UP : ACT_BID_DOWN, 0, engine);
                    break;
                }
                default: break;
            }
            return p;
        }
        return 0;
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button != sf::Mouse::Button::Left) return 0;
        const int owner = mouseOwner;
        if (owner != 1 && owner != 2) return 0;
        if (owner == 2 && botActive) return 0;
        if (modalOpen[owner - 1]) return 0;
        for (const auto& h : hits[owner - 1]) {
            if (h.rect.contains(eventPos)) {
                doAction(owner, h.action, h.arg, engine);
                return owner;
            }
        }
        if (overlayRect[owner - 1].size.x > 0.0f && overlayRect[owner - 1].contains(eventPos)) return owner;
        for (const auto& h : boardHits) {
            if (h.arg == owner && h.rect.contains(eventPos)) {
                doAction(owner, ACT_OPEN_PANEL, owner, engine);
                return owner;
            }
        }
    }
    return 0;
}

void UI_politics::doAction(int player, int action, int arg, GameEngine& engine) {
    const int i = player - 1;
    std::string msg;
    switch (action) {
        case ACT_CLOSE: panelOpen[i] = false; break;
        case ACT_OPEN_PANEL: panelOpen[i] = !panelOpen[i]; break;
        case ACT_TAB: tab[i] = std::max(0, std::min(arg, TAB_COUNT - 1)); break;
        case ACT_BUY:
        case ACT_SELL: {
            bool ok = engine.tradeResource(player, static_cast<ResourceType>(arg), action == ACT_BUY, msg);
            row[i][TAB_MARKET] = std::max(0, std::min(arg - 1, 6));
            setFlash(player, msg, ok);
            break;
        }
        case ACT_TOGGLE_IMPORT: {
            bool on = !engine.getPolitics().imports.request[i];
            engine.setImportRequest(player, on);
            row[i][TAB_IMPORT] = 0;
            setFlash(player, on ? "ЗАЯВКАТА ЗА ВНОС Е ВКЛЮЧЕНА" : "ЗАЯВКАТА ЗА ВНОС Е СПРЯНА", true);
            break;
        }
        case ACT_TOGGLE_EXPORT: {
            bool on = !engine.getPolitics().imports.exportAllowed[i];
            engine.setExportAllowed(player, on);
            row[i][TAB_IMPORT] = 1;
            setFlash(player, on ? "ИЗНОСЪТ КЪМ СЪПЕРНИКА Е РАЗРЕШЕН" : "ИЗНОСЪТ КЪМ СЪПЕРНИКА Е БЛОКИРАН", true);
            break;
        }
        case ACT_BID_UP:
        case ACT_BID_DOWN: {
            int id = tenderId(engine);
            int current = 0;
            for (const auto& k : engine.getPolitics().contracts) {
                if (k.id == id) current = k.bid[i];
            }
            int step = engine.getTenderBidStep();
            int next = (action == ACT_BID_UP) ? current + step : std::max(0, current - step);
            if (id == 0) {
                setFlash(player, "ДНЕС НЯМА ТЪРГ", false);
            } else if (next == current) {
                setFlash(player, "НЯМАТЕ ОФЕРТА ЗА НАМАЛЯВАНЕ", false);
            } else {
                bool ok = engine.placeBid(player, id, next, msg);
                setFlash(player, msg, ok);
            }
            break;
        }
        case ACT_COUNCIL: {
            bool ok = engine.chooseCouncilOption(player, arg, msg);
            councilSel[i] = arg;
            setFlash(player, msg, ok);
            break;
        }
        default: break;
    }
}

// =============================================================================
// Centre city column
// =============================================================================
void UI_politics::drawCityColumn(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine, float animTime,
                                 sf::Vector2f mouse, int mouseOwner) {
    drawEventChips(t, font, engine, animTime);
    drawNewsLine(t, font, engine);
    drawTicker(t, font, engine);
    drawBoard(t, font, engine, mouse, mouseOwner);
}

void UI_politics::drawEventChips(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine, float animTime) {
    const float chipW = (COL_W - 6.0f) / 2.0f;
    for (int k = 0; k < 2; ++k) {
        const EventDef& ev = (k == 0) ? engine.getActiveCityEvent() : engine.getForecastCityEvent();
        const bool none = (ev.id == EventId::NONE);
        sf::FloatRect r({ COL_X + k * (chipW + 6.0f), CHIPS_Y }, { chipW, CHIPS_H });
        sf::Color tone = none ? TEXT_DIM : toneColor(ev.tone);
        sf::Color edge = PANEL_EDGE;
        if (!none && ev.tone == Tone::FESTIVAL) {
            float pulse = 0.5f + 0.5f * std::sin(animTime * 4.0f);
            edge = withAlpha(FESTIVAL, static_cast<std::uint8_t>(140 + 115 * pulse));
        } else if (!none) {
            edge = withAlpha(tone, 170);
        }
        drawRect(t, r, PANEL_BG, edge, 1.5f);
        drawRect(t, sf::FloatRect(r.position, { 4.0f, r.size.y }), tone);

        std::string label = (k == 0) ? "ДНЕС · ДЕН " + std::to_string(engine.getCurrentDay()) : "УТРЕ · ПРОГНОЗА";
        drawFit(t, font, label, 10, { r.position.x + 11.0f, r.position.y + 3.0f }, TEXT_DIM, chipW - 16.0f);
        drawFit(t, font, ev.nameBg, 13, { r.position.x + 11.0f, r.position.y + 15.0f }, tone, chipW - 16.0f,
                Align::LEFT, true, 10);
        drawFit(t, font, ev.effectBg, 11, { r.position.x + 11.0f, r.position.y + 31.0f }, TEXT, chipW - 16.0f);
    }
}

void UI_politics::drawNewsLine(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine) {
    sf::FloatRect r({ COL_X, NEWS_Y }, { COL_W, NEWS_H });
    const float textW = COL_W - 20.0f;

    if (!toasts.empty()) {
        const Toast& n = toasts.front();
        sf::Color tone = toneColor(n.tone);
        float a = std::min(1.0f, n.timer / 0.4f);
        drawRect(t, r, withAlpha(sf::Color(20, 28, 44), 246), withAlpha(tone, 200), 1.5f);
        drawRect(t, sf::FloatRect(r.position, { 4.0f, r.size.y }), tone);
        (void)a;
        // One line when it fits at 11 px, otherwise two lines at 11 px
        if (textWidth(font, toUtf8(n.text), 11) <= textW) {
            drawFit(t, font, n.text, 12, { r.position.x + 12.0f, r.position.y + 8.0f }, TEXT, textW, Align::LEFT, false, 11);
        } else {
            std::vector<std::string> lines = wrapText(font, n.text, 11, textW);
            for (size_t k = 0; k < lines.size() && k < 2; ++k) {
                drawFit(t, font, lines[k], 11, { r.position.x + 12.0f, r.position.y + 2.0f + 14.0f * k }, TEXT, textW);
            }
        }
        return;
    }

    drawRect(t, r, PANEL_BG, PANEL_EDGE, 1.0f);
    const CouncilState& c = engine.getPolitics().council;
    // Candidates from the most to the least detailed: the first one that fits at 10 px is shown whole
    std::vector<std::string> lines;
    sf::Color col = TEXT_DIM;
    if (c.active) {
        std::string secs = std::to_string(static_cast<int>(std::ceil(std::max(0.0f, c.timeLeft)))) + " с";
        lines.push_back("СЪВЕТЪТ ГЛАСУВА: " + std::string(getCouncilCard(c.cardId).titleBg) + " · " + secs);
        lines.push_back("СЪВЕТ: " + std::string(getCouncilCard(c.cardId).titleBg) + " · " + secs);
        col = GOLD;
    } else {
        int fd = engine.getNextFestivalDay();
        EventId fe = getFestivalOnDay(fd);
        std::string name = (fe != EventId::NONE) ? getEventDef(fe).nameBg : "";
        if (fd == engine.getCurrentDay() && fe != EventId::NONE) {
            lines.push_back("ДНЕС Е ПРАЗНИК: " + name + "! ОСВЕТЕТЕ ГРАДА");
            lines.push_back("ДНЕС: " + name + "!");
            col = FESTIVAL;
        } else if (fe != EventId::NONE) {
            int left = fd - engine.getCurrentDay();
            std::string when = (left == 1) ? "УТРЕ" : "СЛЕД " + std::to_string(left) + " ДНИ";
            lines.push_back("СЛЕДВАЩ ПРАЗНИК: " + name + " · ДЕН " + std::to_string(fd) + " (" + when + ")");
            lines.push_back("ПРАЗНИК: " + name + " · ДЕН " + std::to_string(fd) + " (" + when + ")");
            lines.push_back("ДЕН " + std::to_string(fd) + ": " + name);
            col = withAlpha(FESTIVAL, 230);
        } else {
            lines.push_back("ПОСЛЕДНИТЕ ДНИ НА КРИЗАТА - ВСЕКИ МЕГАВАТ Е ВАЖЕН");
        }
    }
    std::string line = lines.back();
    for (const auto& cand : lines) {
        if (textWidth(font, toUtf8(cand), 10) <= textW) {
            line = cand;
            break;
        }
    }
    drawFit(t, font, line, 12, { r.position.x + COL_W / 2.0f, r.position.y + 8.0f }, col, textW, Align::CENTER, false, 10);
}

void UI_politics::drawTicker(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine) {
    sf::FloatRect r({ COL_X, TICKER_Y }, { COL_W, TICKER_H });
    drawRect(t, r, PANEL_BG, PANEL_EDGE, 1.0f);
    drawFit(t, font, "БОРСА · ЦЕНА ЗА ПАРТИДА", 10, { r.position.x + 8.0f, r.position.y + 3.0f }, TEXT_DIM, 170.0f);

    // Import status on the right of the header
    int f1 = engine.getImportFlowMW(1), f2 = engine.getImportFlowMW(2);
    std::string imp = "ВНОС НА ТОК: НЯМА";
    sf::Color impCol = TEXT_DIM;
    if (f1 > 0) { imp = "ВНОС: P1 КУПУВА " + std::to_string(f1) + " MW ОТ P2"; impCol = P1; }
    else if (f2 > 0) { imp = "ВНОС: P2 КУПУВА " + std::to_string(f2) + " MW ОТ P1"; impCol = P2; }
    drawFit(t, font, imp, 10, { r.position.x + COL_W - 8.0f, r.position.y + 3.0f }, impCol, 180.0f, Align::RIGHT);

    const float cellW = (COL_W - 8.0f) / 7.0f;
    for (int k = 1; k <= 7; ++k) {
        ResourceType rt = static_cast<ResourceType>(k);
        float cx = r.position.x + 4.0f + (k - 1) * cellW + cellW / 2.0f;
        float y = r.position.y + 18.0f;
        float m = engine.getMarketMultiplier(rt);
        int pct = static_cast<int>(std::lround((m - 1.0f) * 100.0f));
        sf::Color pc = (pct > 2) ? FESTIVAL : (pct < -2 ? P1 : TEXT);
        if (k > 1) drawRect(t, sf::FloatRect({ r.position.x + 4.0f + (k - 1) * cellW, y + 2.0f }, { 1.0f, 38.0f }), PANEL_EDGE);
        drawFit(t, font, kResShort[k], 10, { cx, y }, TEXT_DIM, cellW - 9.0f, Align::CENTER, false, 9);
        drawFit(t, font, moneyShort(engine.getMarketBuyPrice(rt)) + "$", 11, { cx, y + 13.0f }, pc, cellW - 8.0f,
                Align::CENTER, true, 9);
        std::string trend = (pct > 0) ? "▲" + std::to_string(pct) + "%" : (pct < 0 ? "▼" + std::to_string(-pct) + "%" : "0%");
        drawFit(t, font, trend, 10, { cx, y + 28.0f }, pc, cellW - 4.0f, Align::CENTER);
    }
}

void UI_politics::drawBoard(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine, sf::Vector2f mouse,
                            int mouseOwner) {
    sf::FloatRect r({ COL_X, BOARD_Y }, { COL_W, BOARD_H });
    drawRect(t, r, PANEL_BG, PANEL_EDGE, 1.5f);
    drawRect(t, sf::FloatRect(r.position, { COL_W, 24.0f }), HEADER_BG);
    drawFit(t, font, "ДОГОВОРИ НА ГРАДА · ДЕН " + std::to_string(engine.getCurrentDay()), 13,
            { r.position.x + 8.0f, r.position.y + 4.0f }, GOLD, 240.0f, Align::LEFT, true);
    drawFit(t, font, "ЧАС " + hhmm(engine.getHour24()), 11, { r.position.x + COL_W - 8.0f, r.position.y + 6.0f }, TEXT_DIM,
            110.0f, Align::RIGHT);

    const auto& ks = engine.getPolitics().contracts;
    const float dh = dayHourOf(engine.getHour24());
    const int season = static_cast<int>(engine.getSeason());
    const float cardX = r.position.x + 8.0f;
    const float cardW = COL_W - 16.0f;

    if (ks.empty()) {
        std::string head = (engine.getCurrentDay() < FIRST_CONTRACT_DAY) ? "ГРАДЪТ ПУБЛИКУВА ДОГОВОРИ ОТ ДЕН 3"
                                                                          : "ДНЕС НЯМА ДОГОВОРИ";
        drawFit(t, font, head, 13, { r.position.x + COL_W / 2.0f, r.position.y + 44.0f }, TEXT, cardW, Align::CENTER, true);
        const char* lines[] = { "Всеки ден: 2 договора и понякога търг.",
                                "Изпълнените договори носят злато и",
                                "проценти от града. Търговете се печелят",
                                "със запечатани оферти в Кметството." };
        for (int k = 0; k < 4; ++k) {
            drawFit(t, font, lines[k], 12, { r.position.x + COL_W / 2.0f, r.position.y + 76.0f + 20.0f * k }, TEXT_DIM,
                    cardW, Align::CENTER);
        }
    }

    for (size_t n = 0; n < ks.size() && n < 3; ++n) {
        const CityContract& k = ks[n];
        sf::FloatRect cr({ cardX, r.position.y + 28.0f + n * 74.0f }, { cardW, 70.0f });
        const bool tender = (k.kind == ContractKind::TENDER_NIGHT);
        sf::Color edge = k.festival ? FESTIVAL : (tender ? sf::Color(170, 140, 255) : PANEL_EDGE);
        if (k.state == ContractState::DONE) edge = withAlpha(GOOD, 200);
        if (k.state == ContractState::EXPIRED) edge = withAlpha(TEXT_DIM, 120);
        drawRect(t, cr, ROW_BG, edge, 1.0f);

        float start = 0.0f, end = 0.0f;
        getContractWindow(k.kind, season, start, end);
        const float x = cr.position.x, y = cr.position.y;

        // Line 1: title + when
        std::string title = getContractTitle(k.kind);
        if (k.festival) title = "ПРАЗНИЧНИ СВЕТЛИНИ";
        drawFit(t, font, title, 12, { x + 8.0f, y + 4.0f }, k.festival ? FESTIVAL : TEXT, 200.0f, Align::LEFT, true);
        std::string when;
        switch (k.kind) {
            case ContractKind::STORAGE_SUNSET: when = "ПРИ ЗАЛЕЗ " + hhmm(start + 6.0f); break;
            case ContractKind::LAMPS_22: when = "В 22:00"; break;
            default: when = hhmm(start + 6.0f) + "–" + hhmm(end + 6.0f); break;
        }
        drawFit(t, font, when, 11, { x + cardW - 8.0f, y + 5.0f }, TEXT_DIM, 130.0f, Align::RIGHT);

        // Line 2: condition + reward
        std::string cond, unit;
        switch (k.kind) {
            case ContractKind::EVENING_PEAK:
            case ContractKind::MORNING_PEAK: cond = "Средно ≥ " + std::to_string(k.target) + " MW за града"; break;
            case ContractKind::STORAGE_SUNSET: cond = "≥ " + std::to_string(k.target) + " MWh в батериите"; break;
            case ContractKind::LAMPS_22: cond = "≥ " + std::to_string(k.target) + " светещи лампи"; break;
            case ContractKind::RECORD_RACE: cond = "Пръв с ≥ " + std::to_string(k.target) + " MW наведнъж"; break;
            case ContractKind::TENDER_NIGHT: cond = "Победителят: средно ≥ " + std::to_string(k.target) + " MW"; break;
            default: break;
        }
        drawFit(t, font, cond, 11, { x + 8.0f, y + 22.0f }, TEXT, 220.0f);
        std::string rew = "+" + std::to_string(k.rewardGold) + " G · +" + std::to_string(k.rewardShareTenths / 10) + "%";
        drawFit(t, font, rew, 11, { x + cardW - 8.0f, y + 22.0f }, GOLD, 115.0f, Align::RIGHT, true);

        // Line 3: progress per player, or the sealed-bid state
        const float barY = y + 42.0f;
        auto playerBar = [&](int p, float bx, float bw) {
            const int i = p - 1;
            float value = k.best[i];
            if (k.state == ContractState::OPEN && k.kind == ContractKind::STORAGE_SUNSET) value = engine.getStoredBatteryMWh(p);
            if (k.state == ContractState::OPEN && k.kind == ContractKind::LAMPS_22) value = static_cast<float>(engine.getPoweredLampCount(p));
            sf::Color pc = playerColor(p);
            drawFit(t, font, p == 1 ? "P1" : "P2", 11, { bx, barY - 2.0f }, pc, 20.0f, Align::LEFT, true);
            sf::Color fill = k.completed[i] ? GOOD : (k.failed[i] ? BAD : pc);
            float frac = (k.target > 0) ? value / static_cast<float>(k.target) : 0.0f;
            if (k.completed[i]) frac = 1.0f;
            drawBar(t, sf::FloatRect({ bx + 22.0f, barY + 3.0f }, { bw - 84.0f, 8.0f }), frac, fill);
            std::string v = k.completed[i] ? "✓ ГОТОВО" : (k.failed[i] ? "ПРОВАЛ"
                            : std::to_string(static_cast<int>(std::floor(value))) + "/" + std::to_string(k.target));
            drawFit(t, font, v, 10, { bx + bw - 2.0f, barY - 1.0f }, k.completed[i] ? GOOD : TEXT, 58.0f, Align::RIGHT);
        };

        std::string status;
        sf::Color statusCol = TEXT_DIM;
        if (tender && k.state == ContractState::BIDDING) {
            std::string b1 = k.bid[0] > 0 ? "P1: ✓ ОФЕРТА" : "P1: —";
            std::string b2 = k.bid[1] > 0 ? "P2: ✓ ОФЕРТА" : "P2: —";
            drawFit(t, font, b1, 11, { x + 8.0f, barY - 2.0f }, P1, cardW / 2.0f - 12.0f, Align::LEFT, true);
            drawFit(t, font, b2, 11, { x + cardW / 2.0f + 4.0f, barY - 2.0f }, P2, cardW / 2.0f - 12.0f, Align::LEFT, true);
            status = "ЗАПЕЧАТАНИ ОФЕРТИ ДО 12:00 · ПРОВАЛ: -1%";
        } else if (tender && k.tenderWinner > 0) {
            playerBar(k.tenderWinner, x + 8.0f, cardW - 16.0f);
            status = std::string(k.tenderWinner == 1 ? "СПЕЧЕЛЕН ОТ P1" : "СПЕЧЕЛЕН ОТ P2") + " ЗА " + money(k.paidBid) + "$";
            if (k.state == ContractState::DONE) {
                bool ok = k.completed[k.tenderWinner - 1];
                status += ok ? " · ИЗПЪЛНЕН" : " · НЕИЗПЪЛНЕН (-1%)";
                statusCol = ok ? GOOD : BAD;
            }
        } else if (tender) {
            status = "НИКОЙ НЕ ПОДАДЕ ОФЕРТА";
        } else {
            playerBar(1, x + 8.0f, cardW / 2.0f - 12.0f);
            playerBar(2, x + cardW / 2.0f + 4.0f, cardW / 2.0f - 12.0f);
        }

        // Line 4: status
        if (status.empty()) {
            if (k.state == ContractState::DONE) {
                bool both = k.completed[0] && k.completed[1];
                status = both ? "ИЗПЪЛНЕН ОТ P1 И P2" : (k.completed[0] ? "ИЗПЪЛНЕН ОТ P1" : "ИЗПЪЛНЕН ОТ P2");
                statusCol = GOOD;
            } else if (k.state == ContractState::EXPIRED) {
                status = "ИЗТЕКЪЛ · НИКОЙ НЕ ГО ИЗПЪЛНИ";
            } else if (k.kind == ContractKind::RECORD_RACE) {
                status = "ПЪРВИЯТ ПЕЧЕЛИ · ДО 18:00";
                statusCol = GOLD;
            } else if (start == end) {
                status = "ПРОВЕРКА В " + hhmm(start + 6.0f);
            } else if (dh < start) {
                status = "ЗАПОЧВА В " + hhmm(start + 6.0f) + " · И ДВАМАТА МОГАТ ДА ГО ИЗПЪЛНЯТ";
            } else {
                status = "ТЕЧЕ СЕГА · ДО " + hhmm(end + 6.0f);
                statusCol = GOLD;
            }
        }
        drawFit(t, font, status, 10, { x + 8.0f, y + 56.0f }, statusCol, cardW - 16.0f);
    }

    // Footer: city hall buttons (mouse) with the keyboard shortcut on them
    boardHits.clear();
    const float by = r.position.y + BOARD_H - 26.0f;
    const int buttons = lastBotActive ? 1 : 2; // single player: one button, both shortcuts
    const float bw = (buttons == 1) ? COL_W - 16.0f : (COL_W - 24.0f) / 2.0f;
    for (int p = 1; p <= buttons; ++p) {
        sf::FloatRect br({ r.position.x + 8.0f + (p - 1) * (bw + 8.0f), by }, { bw, 21.0f });
        bool hover = (mouseOwner == p) && br.contains(mouse);
        sf::Color pc = playerColor(p);
        drawRect(t, br, hover ? withAlpha(pc, 90) : withAlpha(pc, 35), withAlpha(pc, hover ? 255 : 160), 1.0f);
        std::string label = (buttons == 1) ? "КМЕТСТВО: БОРСА · ВНОС · ТЪРГ  [C] / [Home]"
                          : (p == 1) ? "P1 КМЕТСТВО [C]" : "P2 КМЕТСТВО [Home]";
        drawFit(t, font, label, 11, { br.position.x + bw / 2.0f, br.position.y + 3.0f }, TEXT, bw - 8.0f, Align::CENTER, true);
        boardHits.push_back({ br, ACT_OPEN_PANEL, p });
    }
}
