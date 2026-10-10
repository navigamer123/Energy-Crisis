#include "../includes/UI_postmatch.h"
#include "../includes/UI_infoCharts.h"
#include "../includes/UI_infoText.h"
#include "../includes/UI_types.h"
#include "../includes/UI_arcadeMode.h"
#include "../includes/UI_settings.h"
#include "../includes/UI_theme.h"
#include <algorithm>
#include <cmath>

// =============================================================================
// team info: post-match report (awards, P1/P2 comparison, charts, energy mix)
// =============================================================================

namespace {

using namespace infoCharts;

// Colours (named here so a theme pass can map them)
const sf::Color COL_BACKDROP(8, 12, 20, 238);
const sf::Color COL_CARD(14, 20, 32, 252);
const sf::Color COL_HEADER(24, 34, 54);
const sf::Color COL_P1(0, 229, 255);
const sf::Color COL_P2(255, 120, 200);
const sf::Color COL_GOLD(255, 215, 0);
const sf::Color COL_TAB(26, 38, 58);
const sf::Color COL_TAB_ACTIVE(40, 64, 96);
const sf::Color COL_ROW(22, 32, 50);
const sf::Color COL_DIM(150, 175, 210);
const sf::Color COL_BEST(255, 255, 255);
const sf::Color COL_SOURCE[UI_matchStats::SOURCE_COUNT] = {
    sf::Color(255, 205, 60), sf::Color(190, 230, 255), sf::Color(60, 150, 255), sf::Color(0, 230, 140)
};
const char* sourceName(int s, bool isEn) {
    static const char* EN_NAMES[UI_matchStats::SOURCE_COUNT] = { "Solar", "Wind", "Hydro", "Batteries" };
    static const char* BG_NAMES[UI_matchStats::SOURCE_COUNT] = { "Слънце", "Вятър", "ВЕЦ", "Батерии" };
    if (s >= 0 && s < UI_matchStats::SOURCE_COUNT) return isEn ? EN_NAMES[s] : BG_NAMES[s];
    return "";
}

const char* tabName(int t, bool isEn) {
    static const char* EN_TABS[UI_postmatch::TAB_COUNT] = { "1 · SUMMARY", "2 · CHARTS", "3 · ENERGY MIX" };
    static const char* BG_TABS[UI_postmatch::TAB_COUNT] = { "1 · ОБОБЩЕНИЕ", "2 · ГРАФИКИ", "3 · ЕНЕРГИЕН МИКС" };
    if (t >= 0 && t < UI_postmatch::TAB_COUNT) return isEn ? EN_TABS[t] : BG_TABS[t];
    return "";
}

// Layout
const sf::FloatRect CARD({ 110.0f, 44.0f }, { 1380.0f, 812.0f });
constexpr float PAD = 26.0f;

sf::Color playerCol(int p) { return p == 1 ? COL_P1 : (p == 2 ? COL_P2 : COL_GOLD); }
std::string playerName(int p, bool isEn) {
    if (isEn) return p == 1 ? "Player 1 (West)" : "Player 2 (East)";
    return p == 1 ? "Играч 1 (Запад)" : "Играч 2 (Изток)";
}

double totalMWh(const UI_matchStats::PlayerTotals& t) {
    double s = 0.0;
    for (double v : t.mwh) s += v;
    return s;
}

int settledDays(const UI_matchStats& stats) {
    int n = 0;
    for (const auto& d : stats.getDays()) if (d.outcome != UI_matchStats::DayOutcome::GRACE) ++n;
    return n;
}

int plotsOwned(const GameEngine& e, int p) {
    int n = 0;
    for (const auto& plot : e.getLandPlots()) if (plot.playerOwner == p && plot.isPurchased) ++n;
    return n;
}

void medal(sf::RenderTarget& t, sf::Vector2f c, sf::Color ribbon) {
    sf::ConvexShape left(3), right(3);
    left.setPoint(0, { c.x - 9.0f, c.y + 2.0f });
    left.setPoint(1, { c.x - 2.0f, c.y + 2.0f });
    left.setPoint(2, { c.x - 12.0f, c.y + 22.0f });
    right.setPoint(0, { c.x + 2.0f, c.y + 2.0f });
    right.setPoint(1, { c.x + 9.0f, c.y + 2.0f });
    right.setPoint(2, { c.x + 12.0f, c.y + 22.0f });
    left.setFillColor(ribbon);
    right.setFillColor(ribbon);
    t.draw(left);
    t.draw(right);
    sf::CircleShape coin(13.0f);
    coin.setOrigin({ 13.0f, 13.0f });
    coin.setPosition({ c.x, c.y - 4.0f });
    coin.setFillColor(COL_GOLD);
    coin.setOutlineThickness(2.0f);
    coin.setOutlineColor(sf::Color(180, 130, 0));
    t.draw(coin);
    sf::CircleShape star(7.0f, 5); // simple pentagon "star" mark
    star.setOrigin({ 7.0f, 7.0f });
    star.setPosition({ c.x, c.y - 4.0f });
    star.setFillColor(sf::Color(255, 245, 190));
    t.draw(star);
}

} // namespace

// -----------------------------------------------------------------------------
// Awards: the three clearest leads of the match
// -----------------------------------------------------------------------------

std::vector<UI_postmatch::Award> UI_postmatch::pickAwards(const UI_matchStats& stats, const GameEngine& engine, bool isEn) {
    const auto& a = stats.getTotals(1);
    const auto& b = stats.getTotals(2);
    std::vector<Award> all;

    auto lead = [&](const std::string& title, double va, double vb, const std::string& unit, double minValue) {
        double best = std::max(va, vb);
        if (best < minValue || std::fabs(va - vb) < 1e-6) return;
        Award aw;
        aw.title = title;
        aw.player = (va > vb) ? 1 : 2;
        aw.detail = fmtInt(best) + unit;
        aw.score = static_cast<float>(std::fabs(va - vb) / best);
        all.push_back(aw);
    };
    lead(isEn ? "ENERGY TITAN" : "ЕНЕРГИЕН ТИТАН", a.peakMW, b.peakMW, isEn ? " MW peak power" : " MW най-висока мощност", 1.0);
    lead(isEn ? "CLEAN AIR" : "ЧИСТ ВЪЗДУХ", a.co2t, b.co2t, isEn ? " t avoided CO2" : " т спестен CO2", 1.0);
    lead(isEn ? "SOLAR KING" : "СЛЪНЧЕВ КРАЛ", a.mwh[UI_matchStats::SOLAR], b.mwh[UI_matchStats::SOLAR], isEn ? " MWh from solar" : " MWh от слънцето", 1.0);
    lead(isEn ? "WIND MASTER" : "ГОСПОДАР НА ВЯТЪРА", a.mwh[UI_matchStats::WIND], b.mwh[UI_matchStats::WIND], isEn ? " MWh from wind" : " MWh от вятъра", 1.0);
    lead(isEn ? "HYDRO BARON" : "ХИДРО БАРОН", a.mwh[UI_matchStats::HYDRO], b.mwh[UI_matchStats::HYDRO], isEn ? " MWh from hydro" : " MWh от ВЕЦ", 1.0);
    lead(isEn ? "NIGHT WATCHMAN" : "НОЩЕН ПАЗАЧ", a.mwh[UI_matchStats::BATTERY], b.mwh[UI_matchStats::BATTERY], isEn ? " MWh from batteries" : " MWh от батерии", 1.0);
    lead(isEn ? "BUILDER" : "СТРОИТЕЛ", a.builtTotal, b.builtTotal, isEn ? " buildings built" : " построени сгради", 1.0);
    lead(isEn ? "MINER" : "МИНЬОР", static_cast<double>(a.mined), static_cast<double>(b.mined), isEn ? " raw resources mined" : " добити суровини", 1.0);
    lead(isEn ? "LANDLORD" : "ЗЕМЕВЛАДЕЛЕЦ", plotsOwned(engine, 1), plotsOwned(engine, 2), isEn ? " land plots" : " парцела земя", 2.0);
    lead(isEn ? "RELIABLE SUPPLIER" : "НАДЕЖДЕН ДОСТАВЧИК", a.daysMet, b.daysMet, isEn ? " days meeting demand" : " дни с покрита нужда", 1.0);

    // Comeback: the winner once held 35% of the city or less
    const int winner = engine.getCityState().winner;
    if (winner == 1 || winner == 2) {
        const auto& w = stats.getTotals(winner);
        if (w.minShare <= 0.35f) {
            Award aw;
            aw.title = isEn ? "GREAT COMEBACK" : "ВЕЛИКО ЗАВРЪЩАНЕ";
            aw.player = winner;
            aw.detail = isEn ? ("Won after holding only " + std::to_string(static_cast<int>(std::lround(w.minShare * 100.0f))) + "% city control")
                             : ("Спечели след " + std::to_string(static_cast<int>(std::lround(w.minShare * 100.0f))) + "% от града");
            aw.score = 2.0f;
            all.push_back(aw);
        }
    }
    std::stable_sort(all.begin(), all.end(), [](const Award& x, const Award& y) { return x.score > y.score; });
    if (all.size() > 3) all.resize(3);
    return all;
}

// -----------------------------------------------------------------------------
// Input
// -----------------------------------------------------------------------------

bool UI_postmatch::handleKey(sf::Keyboard::Key key) {
    switch (key) {
        case sf::Keyboard::Key::Num1: case sf::Keyboard::Key::Numpad1: tab = SUMMARY; return true;
        case sf::Keyboard::Key::Num2: case sf::Keyboard::Key::Numpad2: tab = CHARTS; return true;
        case sf::Keyboard::Key::Num3: case sf::Keyboard::Key::Numpad3: tab = MIX; return true;
        case sf::Keyboard::Key::Left: case sf::Keyboard::Key::Q:
            tab = (tab + TAB_COUNT - 1) % TAB_COUNT; return true;
        case sf::Keyboard::Key::Right: case sf::Keyboard::Key::E: case sf::Keyboard::Key::Tab:
            tab = (tab + 1) % TAB_COUNT; return true;
        default: return false;
    }
}

bool UI_postmatch::handleJoystick(int button) {
    // LB (4), X (2) -> previous tab
    if (button == 4 || button == 2) {
        tab = (tab + TAB_COUNT - 1) % TAB_COUNT;
        return true;
    }
    // RB (5), Y (3) -> next tab
    if (button == 5 || button == 3) {
        tab = (tab + 1) % TAB_COUNT;
        return true;
    }
    return false;
}

bool UI_postmatch::handleJoystickAxis(sf::Joystick::Axis axis, float position) {
    if (axis == sf::Joystick::Axis::X || axis == sf::Joystick::Axis::PovX) {
        if (position < -55.0f && lastJoyAxisX >= -55.0f) {
            tab = (tab + TAB_COUNT - 1) % TAB_COUNT;
            lastJoyAxisX = position;
            return true;
        } else if (position > 55.0f && lastJoyAxisX <= 55.0f) {
            tab = (tab + 1) % TAB_COUNT;
            lastJoyAxisX = position;
            return true;
        } else if (std::abs(position) < 30.0f) {
            lastJoyAxisX = position;
        }
    }
    return false;
}

bool UI_postmatch::handleClick(sf::Vector2f pos) {
    for (int i = 0; i < TAB_COUNT; ++i) {
        if (tabRects[i].contains(pos)) {
            tab = i;
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
// Drawing
// -----------------------------------------------------------------------------

void UI_postmatch::draw(sf::RenderTarget& target, const sf::Font& font, bool fontLoaded, const GameEngine& engine,
                        const UI_matchStats& stats, sf::Vector2f mousePos, sf::FloatRect& restartBtn, sf::FloatRect& menuBtn,
                        float finishedTimer) {
    rect(target, sf::FloatRect({ 0.0f, 0.0f }, { VIRTUAL_WIDTH, VIRTUAL_HEIGHT }), COL_BACKDROP);

    const auto& city = engine.getCityState();
    const int winner = city.winner;
    const bool isDraw = (winner == 3);
    const sf::Color winCol = playerCol(isDraw ? 0 : winner);
    rect(target, CARD, COL_CARD, winCol, 3.0f);
    rect(target, sf::FloatRect(CARD.position, { CARD.size.x, 96.0f }), COL_HEADER);
    rect(target, sf::FloatRect({ CARD.position.x, CARD.position.y + 96.0f }, { CARD.size.x, 3.0f }), winCol);

    // Buttons (rectangles published even without a font so they stay clickable)
    restartBtn = sf::FloatRect({ CARD.position.x + PAD, CARD.position.y + CARD.size.y - 62.0f }, { 360.0f, 44.0f });
    menuBtn = sf::FloatRect({ CARD.position.x + CARD.size.x - PAD - 300.0f, CARD.position.y + CARD.size.y - 62.0f }, { 300.0f, 44.0f });

    const bool locked = (finishedTimer < 10.0f);
    const int remain = locked ? static_cast<int>(std::ceil(10.0f - finishedTimer)) : 0;
    const bool hoverR = !locked && restartBtn.contains(mousePos);
    const bool hoverM = !locked && menuBtn.contains(mousePos);

    sf::Color restartCol = locked ? sf::Color(30, 48, 38) : (hoverR ? sf::Color(0, 200, 130) : sf::Color(0, 150, 95));
    sf::Color restartOutline = locked ? sf::Color(55, 75, 65) : sf::Color(100, 255, 180);
    sf::Color menuCol = locked ? sf::Color(38, 44, 52) : (hoverM ? sf::Color(70, 90, 120) : sf::Color(45, 60, 85));
    sf::Color menuOutline = locked ? sf::Color(60, 68, 80) : sf::Color(130, 160, 205);

    rect(target, restartBtn, restartCol, restartOutline, 1.5f);
    rect(target, menuBtn, menuCol, menuOutline, 1.5f);
    if (!fontLoaded) return;

    bool isEn = (UI_settings::get().getLanguage() == "en");
    bool isArcade = ArcadeMode::isEnabled();

    std::string playAgainLabel;
    std::string exitLabel;

    if (locked) {
        if (isArcade) {
            playAgainLabel = isEn ? ("[ A ]  PLAY AGAIN (" + std::to_string(remain) + "s)")
                                  : ("[ A ]  ИГРАЙ ОТНОВО (" + std::to_string(remain) + "с)");
            exitLabel = isEn ? ("[ B ]  EXIT (" + std::to_string(remain) + "s)")
                             : ("[ B ]  ИЗХОД (" + std::to_string(remain) + "с)");
        } else {
            playAgainLabel = isEn ? ("[ R ]  PLAY AGAIN (" + std::to_string(remain) + "s)")
                                  : ("[ R ]  НОВА ИГРА (" + std::to_string(remain) + "с)");
            exitLabel = isEn ? ("[ ESC ]  MAIN MENU (" + std::to_string(remain) + "s)")
                             : ("[ ESC ]  ГЛАВНО МЕНЮ (" + std::to_string(remain) + "с)");
        }
    } else {
        if (isArcade) {
            playAgainLabel = isEn ? "[ A ]  PLAY AGAIN (1 TOKEN)" : "[ A ]  ИГРАЙ ОТНОВО (1 ЖЕТОН)";
            exitLabel = isEn ? "[ B ]  EXIT" : "[ B ]  ИЗХОД";
        } else {
            playAgainLabel = isEn ? "[ R ]  PLAY AGAIN" : "[ R ]  НОВА ИГРА";
            exitLabel = isEn ? "[ ESC / M ]  MAIN MENU" : "[ ESC / M ]  ГЛАВНО МЕНЮ";
        }
    }

    textCentered(target, font, playAgainLabel, 14,
                 { restartBtn.position.x + restartBtn.size.x / 2.0f, restartBtn.position.y + 12.0f },
                 locked ? sf::Color(160, 175, 190) : sf::Color::White, true);
    textCentered(target, font, exitLabel, 14,
                 { menuBtn.position.x + menuBtn.size.x / 2.0f, menuBtn.position.y + 12.0f },
                 locked ? sf::Color(160, 175, 190) : sf::Color::White, true);

    if (locked) {
        std::string lockMsg = isEn ? ("RESULTS SCREEN LOCKED: " + std::to_string(remain) + "s")
                                   : ("ОСТАВАТ " + std::to_string(remain) + " СЕК. ДО ИЗХОД / РЕСТАРТ");
        textCentered(target, font, lockMsg, 13,
                     { CARD.position.x + CARD.size.x / 2.0f, restartBtn.position.y + 14.0f },
                     theme::Warn, true);
    }

    // Header: who won and how
    const int finalDay = engine.getConfig().finalDay;
    const int victoryPct = static_cast<int>(std::lround(Balance::VICTORY_SHARE * 100.0f));
    const int p1Pct = static_cast<int>(std::lround(city.p1CityShare * 100.0f));
    const int winnerPct = (winner == 1) ? p1Pct : 100 - p1Pct;
    const bool wonByShare = !isDraw && winnerPct >= victoryPct;
    const int decidedDay = std::max(1, std::min(engine.getCurrentDay() - 1, finalDay));
    std::string title;
    if (isEn) {
        title = isDraw ? "DRAW!" : (winner == 1 ? "PLAYER 1 (WEST) WON!" : "PLAYER 2 (EAST) WON!");
    } else {
        title = isDraw ? "РАВЕНСТВО!" : (winner == 1 ? "ИГРАЧ 1 (ЗАПАД) СПЕЧЕЛИ!" : "ИГРАЧ 2 (ИЗТОК) СПЕЧЕЛИ!");
    }

    std::string sub;
    if (isEn) {
        if (isDraw) {
            sub = "After day " + std::to_string(finalDay) + " the city is split equally: " + std::to_string(p1Pct) + "% / " +
                  std::to_string(100 - p1Pct) + "%.";
        } else if (wonByShare) {
            sub = "Reached " + std::to_string(winnerPct) + "% city control (needed: " + std::to_string(victoryPct) + "%) on day " +
                  std::to_string(decidedDay) + ".";
        } else {
            sub = "After day " + std::to_string(finalDay) + " holds majority share: " + std::to_string(winnerPct) + "% vs " +
                  std::to_string(100 - winnerPct) + "%.";
        }
    } else {
        if (isDraw) {
            sub = "След ден " + std::to_string(finalDay) + " градът е разделен поравно: " + std::to_string(p1Pct) + "% / " +
                  std::to_string(100 - p1Pct) + "%.";
        } else if (wonByShare) {
            sub = "Достигна " + std::to_string(winnerPct) + "% от града (нужни: " + std::to_string(victoryPct) + "%) на ден " +
                  std::to_string(decidedDay) + ".";
        } else {
            sub = "След ден " + std::to_string(finalDay) + " държи по-голям дял: " + std::to_string(winnerPct) + "% срещу " +
                  std::to_string(100 - winnerPct) + "%.";
        }
    }

    text(target, font, isEn ? "MATCH REPORT" : "ДОКЛАД ЗА МАЧА", 12, { CARD.position.x + PAD, CARD.position.y + 14.0f }, COL_DIM, true);
    textCentered(target, font, title, 30, { CARD.position.x + CARD.size.x / 2.0f, CARD.position.y + 12.0f }, winCol, true);
    textCentered(target, font, sub, 14, { CARD.position.x + CARD.size.x / 2.0f, CARD.position.y + 58.0f }, sf::Color(205, 222, 245));
    std::string dayReports = isEn ? ("Day " + std::to_string(decidedDay) + " · " + std::to_string(settledDays(stats)) + " reports")
                                  : ("Ден " + std::to_string(decidedDay) + " · " + std::to_string(settledDays(stats)) + " отчета");
    textRight(target, font, dayReports, 12,
              { CARD.position.x + CARD.size.x - PAD, CARD.position.y + 14.0f }, COL_DIM);

    // Tabs
    const float tabsY = CARD.position.y + 112.0f;
    float tx = CARD.position.x + PAD;
    for (int i = 0; i < TAB_COUNT; ++i) {
        const char* tabLabel = tabName(i, isEn);
        float w = infoText::advance(font, toUtf8(tabLabel), 14, true) + 40.0f;
        tabRects[i] = sf::FloatRect({ tx, tabsY }, { w, 34.0f });
        bool active = (i == tab);
        bool hover = tabRects[i].contains(mousePos);
        rect(target, tabRects[i], active ? COL_TAB_ACTIVE : (hover ? sf::Color(34, 50, 76) : COL_TAB),
             active ? winCol : PANEL_EDGE, active ? 2.0f : 1.0f);
        textCentered(target, font, tabLabel, 14, { tx + w / 2.0f, tabsY + 8.0f }, active ? sf::Color::White : COL_DIM, true);
        tx += w + 10.0f;
    }
    textRight(target, font, isEn ? "[1-3] or [Arrows / Stick]: switch tabs" : "[1-3] или [Стрелки / Стик]: раздели", 12, { CARD.position.x + CARD.size.x - PAD, tabsY + 10.0f }, COL_DIM);

    const sf::FloatRect area({ CARD.position.x + PAD, tabsY + 50.0f },
                             { CARD.size.x - 2.0f * PAD, restartBtn.position.y - 16.0f - (tabsY + 50.0f) });
    if (tab == SUMMARY) drawSummary(target, font, engine, stats, area, isEn);
    else if (tab == CHARTS) drawCharts(target, font, engine, stats, area, isEn);
    else drawMix(target, font, stats, area, isEn);
}

void UI_postmatch::drawSummary(sf::RenderTarget& t, const sf::Font& f, const GameEngine& engine, const UI_matchStats& stats,
                               sf::FloatRect area, bool isEn) const {
    // Awards row
    const auto awards = pickAwards(stats, engine, isEn);
    const float awardH = 96.0f;
    const float awardW = (area.size.x - 2.0f * 16.0f) / 3.0f;
    for (std::size_t i = 0; i < 3; ++i) {
        sf::FloatRect r({ area.position.x + static_cast<float>(i) * (awardW + 16.0f), area.position.y }, { awardW, awardH });
        rect(t, r, PANEL, PANEL_EDGE, 1.0f);
        if (i >= awards.size()) {
            textCentered(t, f, "—", 20, { r.position.x + r.size.x / 2.0f, r.position.y + 34.0f }, COL_DIM);
            continue;
        }
        const Award& a = awards[i];
        medal(t, { r.position.x + 38.0f, r.position.y + 46.0f }, playerCol(a.player));
        const float textX = r.position.x + 72.0f;
        const float textW = r.size.x - 72.0f - 14.0f;
        text(t, f, a.title, 15, { textX, r.position.y + 14.0f }, COL_GOLD, true, textW);
        text(t, f, playerName(a.player, isEn), 14, { textX, r.position.y + 40.0f }, playerCol(a.player), true, textW);
        text(t, f, a.detail, 12, { textX, r.position.y + 64.0f }, COL_DIM, false, textW);
    }

    // P1 vs P2 table
    const auto& a = stats.getTotals(1);
    const auto& b = stats.getTotals(2);
    const auto& city = engine.getCityState();
    struct Row { std::string label; double v1; double v2; std::string unit; bool higherIsBetter; };
    const int p1Pct = static_cast<int>(std::lround(city.p1CityShare * 100.0f));
    std::vector<Row> rows = {
        { isEn ? "Final city share" : "Краен дял от града", static_cast<double>(p1Pct), static_cast<double>(100 - p1Pct), "%", true },
        { isEn ? "Days won (captured territory)" : "Спечелени дни (отнета територия)", static_cast<double>(a.daysWon), static_cast<double>(b.daysWon), "", true },
        { isEn ? "Days meeting city demand" : "Дни с покрита нужда на града", static_cast<double>(a.daysMet), static_cast<double>(b.daysMet), (isEn ? " of " : " от ") + std::to_string(settledDays(stats)), true },
        { isEn ? "Peak power" : "Най-висока мощност", static_cast<double>(a.peakMW), static_cast<double>(b.peakMW), " MW", true },
        { isEn ? "Energy generated" : "Произведена енергия", totalMWh(a), totalMWh(b), " MWh", true },
        { isEn ? "Avoided CO2" : "Спестен CO2", a.co2t, b.co2t, isEn ? " t" : " т", true },
        { isEn ? "Buildings constructed" : "Построени сгради", static_cast<double>(a.builtTotal), static_cast<double>(b.builtTotal), "", true },
        { isEn ? "Lost to lightning" : "Загубени от мълния", static_cast<double>(a.lostToLightning), static_cast<double>(b.lostToLightning), "", false },
        { isEn ? "Plots owned" : "Притежавани парцели", static_cast<double>(plotsOwned(engine, 1)), static_cast<double>(plotsOwned(engine, 2)),
          (isEn ? " of " : " от ") + std::to_string(Balance::PLOTS_PER_PLAYER), true },
        { isEn ? "Mine upgrades" : "Надграждания на мини", static_cast<double>(a.mineUpgrades), static_cast<double>(b.mineUpgrades), "", true },
        { isEn ? "Raw resources mined" : "Добити суровини", static_cast<double>(a.mined), static_cast<double>(b.mined), "", true },
        { isEn ? "Revenue earned from city" : "Спечелени пари от града", static_cast<double>(a.moneyEarned), static_cast<double>(b.moneyEarned), " $", true },
    };

    const float tableY = area.position.y + awardH + 18.0f;
    const float rowH = std::min(26.0f, (area.position.y + area.size.y - tableY - 30.0f) / static_cast<float>(rows.size()));
    const float labelX = area.position.x + 18.0f;
    const float col1R = area.position.x + area.size.x * 0.62f;
    const float col2R = area.position.x + area.size.x - 30.0f;
    // header
    textRight(t, f, isEn ? "PLAYER 1 · WEST" : "ИГРАЧ 1 · ЗАПАД", 13, { col1R, tableY }, COL_P1, true);
    textRight(t, f, isEn ? "PLAYER 2 · EAST" : "ИГРАЧ 2 · ИЗТОК", 13, { col2R, tableY }, COL_P2, true);
    float y = tableY + 26.0f;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const Row& r = rows[i];
        if (i % 2 == 0) rect(t, sf::FloatRect({ area.position.x, y - 3.0f }, { area.size.x, rowH }), COL_ROW);
        text(t, f, r.label, 13, { labelX, y }, sf::Color(214, 228, 248));
        const bool tie = std::fabs(r.v1 - r.v2) < 0.5;
        const bool p1Better = r.higherIsBetter ? (r.v1 > r.v2) : (r.v1 < r.v2);
        auto val = [&](double v, bool best, float right, sf::Color c) {
            std::string s = (r.unit == " т" || r.unit == " t") ? fmtTonnes(v) : fmtInt(v) + r.unit;
            textRight(t, f, (best && !tie ? "> " : "") + s, 13, { right, y }, best && !tie ? COL_BEST : c, best && !tie);
        };
        val(r.v1, p1Better, col1R, sf::Color(170, 225, 240));
        val(r.v2, !p1Better, col2R, sf::Color(240, 190, 225));
        y += rowH;
    }
}

void UI_postmatch::drawCharts(sf::RenderTarget& t, const sf::Font& f, const GameEngine& engine, const UI_matchStats& stats,
                              sf::FloatRect area, bool isEn) const {
    const auto& days = stats.getDays();
    const float gap = 16.0f;
    const float topH = (area.size.y - gap) * 0.5f;
    const float halfW = (area.size.x - gap) * 0.5f;
    const float lastDay = std::max(5.0f, static_cast<float>(days.empty() ? 1 : days.back().day));

    // 1. City share by day
    {
        sf::FloatRect box(area.position, { halfW, topH });
        Frame fr = chartPanel(t, f, box, isEn ? "CITY SHARE AFTER EACH SETTLEMENT" : "ДЯЛ ОТ ГРАДА СЛЕД ВСЕКИ ОТЧЕТ", 100.0f, 4,
                              [](float v) { return std::to_string(static_cast<int>(std::lround(v))) + "%"; },
                              { { COL_P1, isEn ? "West" : "Запад" }, { COL_P2, isEn ? "East" : "Изток" } });
        fr.x0 = 0.0f;
        fr.x1 = lastDay;
        std::vector<sf::Vector2f> line{ fr.map(0.0f, 50.0f) };
        for (const auto& d : days) line.push_back(fr.map(static_cast<float>(d.day), d.shareAfter * 100.0f));
        fillArea(t, line, fr.r.position.y + fr.r.size.y, sf::Color(COL_P1.r, COL_P1.g, COL_P1.b, 70));
        fillArea(t, line, fr.r.position.y, sf::Color(COL_P2.r, COL_P2.g, COL_P2.b, 70));
        for (float v : { Balance::VICTORY_SHARE * 100.0f, (1.0f - Balance::VICTORY_SHARE) * 100.0f }) {
            dashedH(t, fr.r.position.x, fr.r.position.x + fr.r.size.x, fr.map(0.0f, v).y, COL_GOLD);
        }
        polyline(t, line, sf::Color(240, 246, 255), 2.0f);
        for (const auto& d : days) {
            sf::Color c = (d.outcome == UI_matchStats::DayOutcome::P1_TOOK) ? COL_P1
                        : (d.outcome == UI_matchStats::DayOutcome::P2_TOOK) ? COL_P2
                        : (d.outcome == UI_matchStats::DayOutcome::NONE_MET ? sf::Color(150, 150, 160) : COL_GOLD);
            sf::CircleShape dot(4.0f);
            dot.setOrigin({ 4.0f, 4.0f });
            dot.setPosition(fr.map(static_cast<float>(d.day), d.shareAfter * 100.0f));
            dot.setFillColor(c);
            dot.setOutlineThickness(1.0f);
            dot.setOutlineColor(sf::Color(10, 14, 22));
            t.draw(dot);
        }
        int step = (lastDay > 12.0f) ? 2 : 1;
        for (int d = 0; d <= static_cast<int>(lastDay); d += step) {
            textCentered(t, f, std::to_string(d), 10, { fr.map(static_cast<float>(d), 0.0f).x, fr.r.position.y + fr.r.size.y + 4.0f }, AXIS_TEXT);
        }
    }

    // 2. Average delivery per day vs demand (grouped bars + demand step)
    {
        sf::FloatRect box({ area.position.x + halfW + gap, area.position.y }, { halfW, topH });
        float maxV = 50.0f;
        for (const auto& d : days) maxV = std::max({ maxV, static_cast<float>(d.avgMW[0]), static_cast<float>(d.avgMW[1]),
                                                     static_cast<float>(d.demand) });
        const float yMax = niceCeil(maxV * 1.1f);
        Frame fr = chartPanel(t, f, box, isEn ? "DAILY AVERAGE DELIVERY VS DEMAND (MW)" : "СРЕДНА ДОСТАВКА ЗА ДЕНЯ СРЕЩУ НУЖДАТА (MW)", yMax, niceTicks(yMax),
                              [](float v) { return fmtInt(v); },
                              { { COL_P1, isEn ? "West" : "Запад" }, { COL_P2, isEn ? "East" : "Изток" }, { COL_GOLD, isEn ? "Demand" : "Нужда" } });
        fr.x0 = 0.5f;
        fr.x1 = lastDay + 0.5f;
        const float slot = fr.r.size.x / (fr.x1 - fr.x0);
        const float barW = std::max(2.0f, std::min(14.0f, slot * 0.32f));
        std::vector<sf::Vector2f> dem;
        for (const auto& d : days) {
            sf::Vector2f base = fr.map(static_cast<float>(d.day), 0.0f);
            for (int p = 0; p < 2; ++p) {
                if (d.avgMW[p] < 0) continue;
                sf::Vector2f top = fr.map(static_cast<float>(d.day), static_cast<float>(d.avgMW[p]));
                float x = base.x + (p == 0 ? -barW - 1.0f : 1.0f);
                rect(t, sf::FloatRect({ x, top.y }, { barW, base.y - top.y }), p == 0 ? COL_P1 : COL_P2);
            }
            if (d.demand >= 0) {
                dem.push_back(fr.map(static_cast<float>(d.day) - 0.45f, static_cast<float>(d.demand)));
                dem.push_back(fr.map(static_cast<float>(d.day) + 0.45f, static_cast<float>(d.demand)));
            }
        }
        for (std::size_t i = 0; i + 1 < dem.size(); i += 2) polyline(t, { dem[i], dem[i + 1] }, COL_GOLD, 2.5f);
        int step = (lastDay > 12.0f) ? 2 : 1;
        for (int d = 1; d <= static_cast<int>(lastDay); d += step) {
            textCentered(t, f, std::to_string(d), 10, { fr.map(static_cast<float>(d), 0.0f).x, fr.r.position.y + fr.r.size.y + 4.0f }, AXIS_TEXT);
        }
    }

    // 3. Power to the city over the whole match
    {
        const auto& samples = stats.getSamples();
        sf::FloatRect box({ area.position.x, area.position.y + topH + gap }, { area.size.x, topH });
        float maxV = 50.0f;
        for (const auto& s : samples) maxV = std::max({ maxV, static_cast<float>(s.mw[0]), static_cast<float>(s.mw[1]),
                                                        static_cast<float>(s.demand) });
        const float yMax = niceCeil(maxV * 1.1f);
        Frame fr = chartPanel(t, f, box, isEn ? "POWER TO CITY OVER ENTIRE MATCH (MW)" : "МОЩНОСТ КЪМ ГРАДА ПРЕЗ ЦЕЛИЯ МАЧ (MW)", yMax, niceTicks(yMax),
                              [](float v) { return fmtInt(v); },
                              { { COL_P1, isEn ? "Player 1" : "Играч 1" }, { COL_P2, isEn ? "Player 2" : "Играч 2" }, { COL_GOLD, isEn ? "Demand" : "Нужда" } });
        fr.x0 = 0.0f;
        fr.x1 = std::max(24.0f, stats.getCurrentHours());
        // Thin out to at most ~one point per pixel
        const std::size_t stride = std::max<std::size_t>(1, samples.size() / static_cast<std::size_t>(std::max(1.0f, fr.r.size.x)));
        std::vector<sf::Vector2f> l1, l2, dem;
        int prevDemand = -1;
        for (std::size_t i = 0; i < samples.size(); i += stride) {
            const auto& s = samples[i];
            if (prevDemand >= 0 && s.demand != prevDemand) dem.push_back(fr.map(s.hours, static_cast<float>(prevDemand)));
            prevDemand = s.demand;
            dem.push_back(fr.map(s.hours, static_cast<float>(s.demand)));
            l1.push_back(fr.map(s.hours, static_cast<float>(s.mw[0])));
            l2.push_back(fr.map(s.hours, static_cast<float>(s.mw[1])));
        }
        polyline(t, dem, COL_GOLD, 2.0f);
        polyline(t, l2, COL_P2, 1.5f);
        polyline(t, l1, COL_P1, 1.5f);
        const int dayCount = static_cast<int>(std::ceil(fr.x1 / 24.0f));
        const int step = (dayCount > 12) ? 2 : 1;
        for (int d = 1; d <= dayCount; d += step) {
            float h = static_cast<float>(d - 1) * 24.0f;
            sf::Vector2f p = fr.map(h, 0.0f);
            if (d > 1) rect(t, sf::FloatRect({ p.x, fr.r.position.y }, { 1.0f, fr.r.size.y }), sf::Color(60, 80, 110, 140));
            text(t, f, (isEn ? "D" : "Д") + std::to_string(d), 10, { p.x + 3.0f, fr.r.position.y + fr.r.size.y + 4.0f }, AXIS_TEXT);
        }
    }
    (void)engine;
}

void UI_postmatch::drawMix(sf::RenderTarget& t, const sf::Font& f, const UI_matchStats& stats, sf::FloatRect area,
                           bool isEn) const {
    const float gap = 20.0f;
    const float colW = (area.size.x - gap) * 0.5f;
    for (int p = 1; p <= 2; ++p) {
        const auto& tot = stats.getTotals(p);
        const double sum = totalMWh(tot);
        sf::FloatRect box({ area.position.x + static_cast<float>(p - 1) * (colW + gap), area.position.y }, { colW, area.size.y });
        rect(t, box, PANEL, PANEL_EDGE, 1.0f);
        const float x = box.position.x + 22.0f;
        const float w = box.size.x - 44.0f;
        text(t, f, isEn ? (p == 1 ? "PLAYER 1 · WEST" : "PLAYER 2 · EAST") : (p == 1 ? "ИГРАЧ 1 · ЗАПАД" : "ИГРАЧ 2 · ИЗТОК"), 16, { x, box.position.y + 16.0f }, playerCol(p), true);
        text(t, f, isEn ? "Total match energy generated" : "Произведена енергия за мача", 12, { x, box.position.y + 50.0f }, COL_DIM);
        text(t, f, fmtInt(sum) + " MWh", 30, { x, box.position.y + 68.0f }, sf::Color::White, true);

        // 100% stacked bar
        const float barY = box.position.y + 124.0f;
        rect(t, sf::FloatRect({ x, barY }, { w, 34.0f }), sf::Color(30, 42, 62));
        float bx = x;
        for (int s = 0; s < UI_matchStats::SOURCE_COUNT; ++s) {
            if (sum <= 0.0) break;
            float sw = w * static_cast<float>(tot.mwh[s] / sum);
            if (sw <= 0.5f) continue;
            rect(t, sf::FloatRect({ bx, barY }, { sw, 34.0f }), COL_SOURCE[s]);
            bx += sw;
        }

        // Per-source rows
        float ry = barY + 54.0f;
        for (int s = 0; s < UI_matchStats::SOURCE_COUNT; ++s) {
            rect(t, sf::FloatRect({ x, ry + 4.0f }, { 14.0f, 14.0f }), COL_SOURCE[s]);
            text(t, f, sourceName(s, isEn), 14, { x + 24.0f, ry }, sf::Color(214, 228, 248));
            const int pct = (sum > 0.0) ? static_cast<int>(std::lround(100.0 * tot.mwh[s] / sum)) : 0;
            textRight(t, f, std::to_string(pct) + "%", 14, { x + w * 0.62f, ry }, sf::Color::White, true);
            textRight(t, f, fmtInt(tot.mwh[s]) + " MWh", 14, { x + w, ry }, COL_DIM);
            ry += 32.0f;
        }

        // CO2 and a short explanation
        ry += 12.0f;
        rect(t, sf::FloatRect({ x, ry }, { w, 1.0f }), PANEL_EDGE);
        ry += 14.0f;
        text(t, f, isEn ? "Avoided CO2" : "Спестен CO2", 12, { x, ry }, COL_DIM);
        text(t, f, fmtTonnes(tot.co2t), 24, { x, ry + 18.0f }, sf::Color(120, 235, 170), true);
        text(t, f, isEn ? "compared to average grid electricity (~0.4 t CO2 per MWh)" : "спрямо ток от средната мрежа (около 0,4 т CO2 на MWh)", 12, { x, ry + 54.0f }, COL_DIM, false, w);
        text(t, f, isEn ? "Batteries do not generate power: they discharge energy stored from solar, wind, and hydro." : "Батериите не произвеждат ток: те връщат енергия, събрана от слънце, вятър и ВЕЦ.", 12,
             { x, ry + 74.0f }, COL_DIM, false, w);
    }
}
