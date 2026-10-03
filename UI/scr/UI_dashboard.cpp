#include "../includes/UI_dashboard.h"
#include "../includes/UI_infoCharts.h"
#include "../includes/UI_infoText.h"
#include "../includes/UI_types.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

// =============================================================================
// team info: real-time energy dashboard (hold [Tab])
// =============================================================================

namespace {

using namespace infoCharts;

// Colours (named here so a theme pass can map them)
const sf::Color COL_BACKDROP(5, 8, 14, 215);
const sf::Color COL_CARD(14, 20, 32, 250);
const sf::Color COL_CARD_EDGE(0, 229, 255);
const sf::Color COL_TILE(22, 32, 50);
const sf::Color COL_P1(0, 229, 255);
const sf::Color COL_P2(255, 120, 200);
const sf::Color COL_DEMAND(255, 215, 0);
const sf::Color COL_GOOD(90, 235, 160);
const sf::Color COL_BAD(255, 120, 120);
const sf::Color COL_NIGHT(4, 8, 18, 120);
const sf::Color COL_LIVE(255, 80, 80);
const sf::Color COL_SOURCE[UI_matchStats::SOURCE_COUNT] = {
    sf::Color(255, 205, 60),   // solar
    sf::Color(190, 230, 255),  // wind
    sf::Color(60, 150, 255),   // hydro
    sf::Color(0, 230, 140)     // battery
};
const char* SOURCE_NAME[UI_matchStats::SOURCE_COUNT] = { "Слънце", "Вятър", "ВЕЦ", "Батерии" };

// Layout
const sf::FloatRect CARD({ 80.0f, 56.0f }, { 1440.0f, 790.0f });
constexpr float PAD = 24.0f;

const char* seasonShort(SeasonType s) {
    switch (s) {
        case SeasonType::SPRING: return "Пролет";
        case SeasonType::SUMMER: return "Лято";
        case SeasonType::AUTUMN: return "Есен";
        case SeasonType::WINTER: return "Зима";
    }
    return "Пролет";
}

std::string hoursMinutes(float h) {
    int total = static_cast<int>(std::round(h * 60.0f));
    return std::to_string(total / 60) + "ч " + std::to_string(total % 60) + "м";
}

void liveDot(sf::RenderTarget& t, sf::Vector2f p, sf::Color c, float animTime) {
    float pulse = 0.5f + 0.5f * std::sin(animTime * 6.0f);
    sf::CircleShape halo(7.0f + 3.0f * pulse);
    halo.setOrigin({ halo.getRadius(), halo.getRadius() });
    halo.setPosition(p);
    halo.setFillColor(sf::Color(c.r, c.g, c.b, static_cast<std::uint8_t>(60 + 60 * (1.0f - pulse))));
    t.draw(halo);
    sf::CircleShape dot(3.5f);
    dot.setOrigin({ 3.5f, 3.5f });
    dot.setPosition(p);
    dot.setFillColor(c);
    t.draw(dot);
}

// Headline tile: label, big value (one or two coloured parts) and a sub line
void tile(sf::RenderTarget& t, const sf::Font& f, sf::FloatRect r, const std::string& label,
          const std::string& value, sf::Color valueCol, const std::string& sub, sf::Color subCol,
          const std::string& value2 = "", sf::Color value2Col = sf::Color::White) {
    rect(t, r, COL_TILE, PANEL_EDGE, 1.0f);
    text(t, f, label, 11, { r.position.x + 14.0f, r.position.y + 10.0f }, AXIS_TEXT, true, r.size.x - 28.0f);
    sf::Text v(f, toUtf8(value), 28);
    v.setStyle(sf::Text::Bold);
    v.setFillColor(valueCol);
    v.setPosition({ r.position.x + 14.0f, r.position.y + 26.0f });
    t.draw(v);
    if (!value2.empty()) {
        text(t, f, value2, 28, { r.position.x + 14.0f + infoText::width(v) + 6.0f, r.position.y + 26.0f }, value2Col, true);
    }
    text(t, f, sub, 12, { r.position.x + 14.0f, r.position.y + r.size.y - 22.0f }, subCol, false, r.size.x - 28.0f);
}

} // namespace

void UI_dashboard::draw(sf::RenderTarget& target, const sf::Font& font, const GameEngine& engine,
                        const UI_matchStats& stats, float openAge, float animTime) const {
    const float reveal = std::min(1.0f, openAge / 0.35f); // lines grow from the left when the board opens
    const float nowH = stats.getCurrentHours();
    const auto& samples = stats.getSamples();
    const auto& city = engine.getCityState();

    rect(target, sf::FloatRect({ 0.0f, 0.0f }, { VIRTUAL_WIDTH, VIRTUAL_HEIGHT }), COL_BACKDROP);
    rect(target, CARD, COL_CARD, COL_CARD_EDGE, 2.5f);

    // ---------------------------------------------------------------- header
    const float hx = CARD.position.x + PAD;
    const float hy = CARD.position.y + 14.0f;
    text(target, font, "ЕНЕРГИЙНО ТАБЛО", 22, { hx, hy }, COL_CARD_EDGE, true);
    {
        sf::Text title(font, toUtf8("ЕНЕРГИЙНО ТАБЛО"), 22);
        title.setStyle(sf::Text::Bold);
        float lx = hx + infoText::width(title) + 18.0f;
        float pulse = 0.5f + 0.5f * std::sin(animTime * 5.0f);
        sf::CircleShape dot(5.0f);
        dot.setPosition({ lx, hy + 10.0f });
        dot.setFillColor(sf::Color(COL_LIVE.r, COL_LIVE.g, COL_LIVE.b, static_cast<std::uint8_t>(140 + 115 * pulse)));
        target.draw(dot);
        text(target, font, "НА ЖИВО", 13, { lx + 16.0f, hy + 6.0f }, COL_LIVE, true);
    }
    const std::string when = "Ден " + std::to_string(engine.getCurrentDay()) + " · " +
                             Balance::formatHourMinute(engine.getHour24()) + " · " + seasonShort(engine.getSeason()) +
                             (engine.isDaylight() ? " · ден" : " · нощ");
    textRight(target, font, when, 15, { CARD.position.x + CARD.size.x - PAD, hy + 2.0f }, TITLE_TEXT, true);
    textRight(target, font, "Пуснете [Tab], за да се върнете в играта", 11,
              { CARD.position.x + CARD.size.x - PAD, hy + 24.0f }, AXIS_TEXT);

    // ---------------------------------------------------------------- headline tiles
    const float tilesY = CARD.position.y + 62.0f;
    const float tileH = 92.0f;
    const float tileW = (CARD.size.x - 2.0f * PAD - 3.0f * 16.0f) / 4.0f;
    auto tileRect = [&](int i) {
        return sf::FloatRect({ CARD.position.x + PAD + static_cast<float>(i) * (tileW + 16.0f), tilesY }, { tileW, tileH });
    };

    const int demand = city.cityEnergyDemand;
    const float hoursToCut = std::fmod(Balance::CLOCK_HOUR_AT_ZERO - engine.getHour24() + 24.0f, 24.0f);
    if (engine.isGracePeriod()) {
        tile(target, font, tileRect(0), "НУЖДА НА ГРАДА", "0 MW", COL_DEMAND,
             "Гратисен период · отчет след " + hoursMinutes(hoursToCut), AXIS_TEXT);
    } else {
        tile(target, font, tileRect(0), "НУЖДА НА ГРАДА (СРЕДНО ЗА ДЕНЯ)", std::to_string(demand) + " MW", COL_DEMAND,
             "Отчет в 06:00 · след " + hoursMinutes(hoursToCut), AXIS_TEXT);
    }
    for (int p = 1; p <= 2; ++p) {
        const int mw = engine.getPlayerEconomy(p).energyMW;
        const int avg = static_cast<int>(engine.getTodayAverageMW(p));
        std::string sub = "Средно днес: " + std::to_string(avg) + " MW";
        sf::Color subCol = AXIS_TEXT;
        if (!engine.isGracePeriod()) {
            bool ok = avg >= demand;
            sub += ok ? " · покрива нуждата" : " · под нуждата";
            subCol = ok ? COL_GOOD : COL_BAD;
        }
        tile(target, font, tileRect(p), p == 1 ? "ИГРАЧ 1 · ЗАПАД · СЕГА" : "ИГРАЧ 2 · ИЗТОК · СЕГА", std::to_string(mw) + " MW",
             p == 1 ? COL_P1 : COL_P2, sub, subCol);
    }
    {
        const int p1Pct = static_cast<int>(std::lround(city.p1CityShare * 100.0f));
        const sf::FloatRect r = tileRect(3);
        const int winPct = static_cast<int>(std::lround(Balance::VICTORY_SHARE * 100.0f));
        tile(target, font, r, "ДЯЛ ОТ ГРАДА", std::to_string(p1Pct) + "%", COL_P1,
             "Победа при " + std::to_string(winPct) + "% · ден " + std::to_string(Balance::FINAL_DAY) + " е последен",
             AXIS_TEXT, "/ " + std::to_string(100 - p1Pct) + "%", COL_P2);
        // mini tug-of-war bar
        const float bx = r.position.x + r.size.x - 128.0f, by = r.position.y + 40.0f, bw = 112.0f, bh = 8.0f;
        rect(target, sf::FloatRect({ bx, by }, { bw * city.p1CityShare, bh }), COL_P1);
        rect(target, sf::FloatRect({ bx + bw * city.p1CityShare, by }, { bw * (1.0f - city.p1CityShare), bh }), COL_P2);
        for (float tick : { Balance::VICTORY_SHARE, 1.0f - Balance::VICTORY_SHARE }) {
            rect(target, sf::FloatRect({ bx + bw * tick - 1.0f, by - 3.0f }, { 2.0f, bh + 6.0f }), COL_DEMAND);
        }
    }

    // ---------------------------------------------------------------- charts
    const float chartsY = tilesY + tileH + 16.0f;
    const float chartH = (CARD.position.y + CARD.size.y - PAD - chartsY - 16.0f) / 2.0f;
    const float wideW = 800.0f;
    const float narrowW = CARD.size.x - 2.0f * PAD - 16.0f - wideW;
    const float leftX = CARD.position.x + PAD;
    const float rightX = leftX + wideW + 16.0f;

    // A. Power vs demand, last 48 game hours
    {
        const float x0 = std::max(0.0f, nowH - WINDOW_HOURS);
        const float x1 = std::max(x0 + 24.0f, nowH);
        float maxV = 50.0f;
        for (const auto& s : samples) {
            if (s.hours < x0) continue;
            maxV = std::max({ maxV, static_cast<float>(s.mw[0]), static_cast<float>(s.mw[1]), static_cast<float>(s.demand) });
        }
        const float yMax = niceCeil(maxV * 1.1f);
        sf::FloatRect box({ leftX, chartsY }, { wideW, chartH });
        Frame fr = chartPanel(target, font, box, "МОЩНОСТ КЪМ ГРАДА (MW) · ПОСЛЕДНИТЕ 48 ЧАСА", yMax, niceTicks(yMax),
                              [](float v) { return fmtInt(v); });
        fr.x0 = x0;
        fr.x1 = x1;

        // Night shading: darker columns while the sun is down
        float runStart = -1.0f;
        for (std::size_t i = 0; i < samples.size(); ++i) {
            const auto& s = samples[i];
            if (s.hours < x0) continue;
            bool dark = !Balance::isDaylightAt(s.hour24, Balance::getSeasonForDay(s.day));
            bool last = (i + 1 == samples.size());
            if (dark && runStart < 0.0f) runStart = s.hours;
            if ((!dark || last) && runStart >= 0.0f) {
                sf::Vector2f a = fr.map(runStart, yMax), b = fr.map(s.hours, 0.0f);
                rect(target, sf::FloatRect({ a.x, a.y }, { std::max(1.0f, b.x - a.x), b.y - a.y }), COL_NIGHT);
                runStart = -1.0f;
            }
        }
        // Day boundaries (06:00 settlements). Labels stay inside the plot and never touch the "сега" label.
        const float labelY = fr.r.position.y + fr.r.size.y + 4.0f;
        const float nowLabelLeft = fr.r.position.x + fr.r.size.x - infoText::advance(font, toUtf8("сега"), 10) - 8.0f;
        for (int d = 1; d <= engine.getCurrentDay(); ++d) {
            float h = static_cast<float>(d - 1) * 24.0f;
            if (h < x0 || h > x1) continue;
            sf::Vector2f p = fr.map(h, 0.0f);
            rect(target, sf::FloatRect({ p.x, fr.r.position.y }, { 1.0f, fr.r.size.y }), sf::Color(90, 110, 140, 160));
            const std::string lbl = "Ден " + std::to_string(d);
            const float w = infoText::advance(font, toUtf8(lbl), 10);
            const float lx = std::max(fr.r.position.x, p.x - w / 2.0f);
            if (lx + w > nowLabelLeft) continue;
            text(target, font, lbl, 10, { lx, labelY }, AXIS_TEXT);
        }
        textRight(target, font, "сега", 10, { fr.r.position.x + fr.r.size.x, labelY }, AXIS_TEXT);

        const float revealX = x0 + (x1 - x0) * reveal;
        std::vector<sf::Vector2f> dem, l1, l2;
        int prevDemand = -1;
        for (const auto& s : samples) {
            if (s.hours < x0 || s.hours > revealX) continue;
            // the demand changes in steps at the 06:00 settlement
            if (prevDemand >= 0 && s.demand != prevDemand) dem.push_back(fr.map(s.hours, static_cast<float>(prevDemand)));
            prevDemand = s.demand;
            dem.push_back(fr.map(s.hours, static_cast<float>(s.demand)));
            l1.push_back(fr.map(s.hours, static_cast<float>(s.mw[0])));
            l2.push_back(fr.map(s.hours, static_cast<float>(s.mw[1])));
        }
        polyline(target, dem, sf::Color(COL_DEMAND.r, COL_DEMAND.g, COL_DEMAND.b, 220), 2.0f);
        polyline(target, l2, COL_P2, 2.5f);
        polyline(target, l1, COL_P1, 2.5f);
        if (reveal >= 1.0f) {
            if (!l1.empty()) liveDot(target, l1.back(), COL_P1, animTime);
            if (!l2.empty()) liveDot(target, l2.back(), COL_P2, animTime);
        }
        float lx = box.position.x + box.size.x - 300.0f;
        lx = legend(target, font, lx, box.position.y + 10.0f, COL_P1, "Играч 1");
        lx = legend(target, font, lx, box.position.y + 10.0f, COL_P2, "Играч 2");
        legend(target, font, lx, box.position.y + 10.0f, COL_DEMAND, "Нужда");
    }

    // B. City share tug-of-war across the match
    {
        sf::FloatRect box({ rightX, chartsY }, { narrowW, chartH });
        Frame fr = chartPanel(target, font, box, "ДЯЛ ОТ ГРАДА ПО ДНИ", 100.0f, 4,
                              [](float v) { return std::to_string(static_cast<int>(std::lround(v))) + "%"; });
        const float nowDays = nowH / 24.0f;
        fr.x0 = 0.0f;
        fr.x1 = std::max(5.0f, std::ceil(nowDays + 0.01f));

        std::vector<sf::Vector2f> line;
        line.push_back(fr.map(0.0f, 50.0f));
        for (const auto& rec : stats.getDays()) {
            line.push_back(fr.map(static_cast<float>(rec.day), rec.shareAfter * 100.0f));
        }
        line.push_back(fr.map(nowDays, city.p1CityShare * 100.0f));
        // reveal
        std::vector<sf::Vector2f> shown;
        const float revealX = fr.r.position.x + fr.r.size.x * reveal;
        for (const auto& p : line) if (p.x <= revealX + 0.5f) shown.push_back(p);

        area(target, shown, fr.r.position.y + fr.r.size.y, sf::Color(COL_P1.r, COL_P1.g, COL_P1.b, 70));
        area(target, shown, fr.r.position.y, sf::Color(COL_P2.r, COL_P2.g, COL_P2.b, 70));
        for (float v : { Balance::VICTORY_SHARE * 100.0f, (1.0f - Balance::VICTORY_SHARE) * 100.0f }) {
            sf::Vector2f p = fr.map(0.0f, v);
            dashedH(target, fr.r.position.x, fr.r.position.x + fr.r.size.x, p.y, COL_DEMAND);
        }
        polyline(target, shown, sf::Color(240, 246, 255), 2.0f);
        for (std::size_t i = 0; i < stats.getDays().size(); ++i) {
            const auto& rec = stats.getDays()[i];
            sf::Vector2f p = fr.map(static_cast<float>(rec.day), rec.shareAfter * 100.0f);
            if (p.x > revealX) break;
            sf::Color c = (rec.outcome == UI_matchStats::DayOutcome::P1_TOOK) ? COL_P1
                        : (rec.outcome == UI_matchStats::DayOutcome::P2_TOOK) ? COL_P2
                        : (rec.outcome == UI_matchStats::DayOutcome::NONE_MET ? sf::Color(150, 150, 160) : COL_DEMAND);
            sf::CircleShape dot(3.5f);
            dot.setOrigin({ 3.5f, 3.5f });
            dot.setPosition(p);
            dot.setFillColor(c);
            dot.setOutlineThickness(1.0f);
            dot.setOutlineColor(sf::Color(10, 14, 22));
            target.draw(dot);
        }
        int step = (fr.x1 > 12.0f) ? 4 : (fr.x1 > 6.0f ? 2 : 1);
        for (int d = 0; d <= static_cast<int>(fr.x1); d += step) {
            sf::Vector2f p = fr.map(static_cast<float>(d), 0.0f);
            textCentered(target, font, std::to_string(d), 10, { p.x, fr.r.position.y + fr.r.size.y + 4.0f }, AXIS_TEXT);
        }
        textRight(target, font, "ден", 10, { box.position.x + box.size.x - 10.0f, fr.r.position.y + fr.r.size.y + 4.0f }, AXIS_TEXT);
        float lx = box.position.x + box.size.x - 210.0f;
        lx = legend(target, font, lx, box.position.y + 10.0f, COL_P1, "Запад");
        legend(target, font, lx, box.position.y + 10.0f, COL_P2, "Изток");
    }

    // C. Energy mix right now
    {
        sf::FloatRect box({ leftX, chartsY + chartH + 16.0f }, { wideW, chartH });
        rect(target, box, PANEL, PANEL_EDGE, 1.0f);
        text(target, font, "ЕНЕРГИЕН МИКС СЕГА (MW ПО ИЗТОЧНИК)", 13, { box.position.x + 12.0f, box.position.y + 8.0f },
             TITLE_TEXT, true);
        float totals[2] = { 0.0f, 0.0f };
        for (int p = 0; p < 2; ++p)
            for (int s = 0; s < UI_matchStats::SOURCE_COUNT; ++s) totals[p] += stats.getLiveSourceMW(p + 1, s);
        const float maxT = niceCeil(std::max({ totals[0], totals[1], 50.0f }));
        const float barX = box.position.x + 110.0f;
        const float barW = box.size.x - 110.0f - 120.0f;
        for (int p = 0; p < 2; ++p) {
            const float by = box.position.y + 46.0f + static_cast<float>(p) * 58.0f;
            text(target, font, p == 0 ? "Играч 1" : "Играч 2", 13, { box.position.x + 16.0f, by + 9.0f }, p == 0 ? COL_P1 : COL_P2, true);
            rect(target, sf::FloatRect({ barX, by }, { barW, 36.0f }), sf::Color(30, 42, 62));
            float x = barX;
            for (int s = 0; s < UI_matchStats::SOURCE_COUNT; ++s) {
                float w = barW * (stats.getLiveSourceMW(p + 1, s) / maxT) * reveal;
                if (w <= 0.5f) continue;
                rect(target, sf::FloatRect({ x, by }, { w, 36.0f }), COL_SOURCE[s]);
                std::string lbl = fmtInt(stats.getLiveSourceMW(p + 1, s));
                sf::Text tl(font, toUtf8(lbl), 12);
                tl.setStyle(sf::Text::Bold);
                if (infoText::width(tl) + 8.0f < w) {
                    tl.setFillColor(sf::Color(12, 16, 24));
                    tl.setPosition({ x + (w - infoText::width(tl)) / 2.0f, by + 10.0f });
                    target.draw(tl);
                }
                x += w;
            }
            text(target, font, fmtInt(totals[p]) + " MW", 14, { barX + barW + 12.0f, by + 8.0f }, TITLE_TEXT, true);
        }
        // legend + produced energy so far
        float lx = box.position.x + 16.0f;
        const float ly = box.position.y + 168.0f;
        for (int s = 0; s < UI_matchStats::SOURCE_COUNT; ++s) lx = legend(target, font, lx, ly, COL_SOURCE[s], SOURCE_NAME[s]);
        for (int p = 0; p < 2; ++p) {
            const auto& tot = stats.getTotals(p + 1);
            double sum = 0.0;
            for (double v : tot.mwh) sum += v;
            std::string line = std::string(p == 0 ? "Играч 1" : "Играч 2") + " е произвел общо " + fmtInt(sum) + " MWh";
            if (sum > 0.5) {
                line += ":";
                for (int s = 0; s < UI_matchStats::SOURCE_COUNT; ++s) {
                    int pct = static_cast<int>(std::lround(100.0 * tot.mwh[s] / sum));
                    line += std::string(" ") + SOURCE_NAME[s] + " " + std::to_string(pct) + "%" +
                            (s + 1 < UI_matchStats::SOURCE_COUNT ? "," : "");
                }
            }
            text(target, font, line, 12, { box.position.x + 16.0f, ly + 26.0f + static_cast<float>(p) * 20.0f },
                 p == 0 ? COL_P1 : COL_P2, false, box.size.x - 32.0f);
        }
    }

    // D. CO2 avoided (cumulative)
    {
        float maxV = 10.0f;
        for (const auto& s : samples) maxV = std::max({ maxV, s.co2t[0], s.co2t[1] });
        const float yMax = niceCeil(maxV * 1.1f);
        sf::FloatRect box({ rightX, chartsY + chartH + 16.0f }, { narrowW, chartH });
        Frame fr = chartPanel(target, font, box, "СПЕСТЕН CO2 (ТОНОВЕ)", yMax, niceTicks(yMax),
                              [](float v) { return fmtInt(v); });
        fr.x0 = 0.0f;
        fr.x1 = std::max(24.0f, nowH);
        const float revealX = fr.x1 * reveal;
        std::vector<sf::Vector2f> c1, c2;
        for (const auto& s : samples) {
            if (s.hours > revealX) break;
            c1.push_back(fr.map(s.hours, s.co2t[0]));
            c2.push_back(fr.map(s.hours, s.co2t[1]));
        }
        polyline(target, c2, COL_P2, 2.5f);
        polyline(target, c1, COL_P1, 2.5f);
        if (reveal >= 1.0f) {
            if (!c1.empty()) liveDot(target, c1.back(), COL_P1, animTime);
            if (!c2.empty()) liveDot(target, c2.back(), COL_P2, animTime);
        }
        const auto& t1 = stats.getTotals(1);
        const auto& t2 = stats.getTotals(2);
        const bool kt = std::max(t1.co2t, t2.co2t) >= 10000.0; // same unit on both sides
        textRight(target, font, "Запад " + fmtTonnes(t1.co2t, kt) + " · Изток " + fmtTonnes(t2.co2t, kt), 11,
                  { box.position.x + box.size.x - 12.0f, box.position.y + 10.0f }, AXIS_TEXT);
        char factor[16];
        std::snprintf(factor, sizeof(factor), "%.2f", static_cast<double>(UI_matchStats::CO2_T_PER_MWH));
        std::string factorStr(factor);
        std::replace(factorStr.begin(), factorStr.end(), '.', ',');
        textCentered(target, font, "приблизително " + factorStr + " т CO2 на всеки MWh чиста енергия", 10,
                     { box.position.x + box.size.x / 2.0f, fr.r.position.y + fr.r.size.y + 4.0f }, AXIS_TEXT);
    }
}
