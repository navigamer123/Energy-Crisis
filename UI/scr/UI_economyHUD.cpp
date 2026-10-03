// =============================================================================
// City economy HUD                                             [team b-economy]
// Grid control dashboard (UX-01 forecast chip, BAL-03 demand curve, F-37 frequency
// gauges, F-11 energy mix + CO2, BAL-04 badges), F-36 district strip, F-37 sector
// alerts and the UX-01 Day Report card. See UI_economyHUD.h.
// =============================================================================
#include "../includes/UI_economyHUD.h"
#include "../includes/UI_types.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

// -----------------------------------------------------------------------------
// Colours (integrator: map onto the UI_theme.h tokens)
// -----------------------------------------------------------------------------
const sf::Color COL_PANEL_BG(13, 19, 31, 238);
const sf::Color COL_PANEL_EDGE(70, 95, 130);
const sf::Color COL_SECTION_BG(20, 28, 44, 235);
const sf::Color COL_TITLE(140, 195, 255);
const sf::Color COL_TEXT(225, 233, 245);
const sf::Color COL_DIM(150, 165, 190);
const sf::Color COL_P1(0, 229, 255);
const sf::Color COL_P2(255, 120, 200);
const sf::Color COL_OK(90, 240, 140);
const sf::Color COL_WARN(255, 190, 60);
const sf::Color COL_BAD(255, 85, 85);
const sf::Color COL_GRACE(90, 255, 190);
const sf::Color COL_BAR_LOW(55, 95, 150);
const sf::Color COL_BAR_MID(85, 140, 200);
const sf::Color COL_BAR_PEAK(255, 165, 60);
const sf::Color COL_SOLAR(255, 210, 70);
const sf::Color COL_WIND(150, 235, 255);
const sf::Color COL_HYDRO(70, 135, 255);
const sf::Color COL_BATTERY(120, 255, 150);
const sf::Color COL_CO2(140, 230, 120);

// -----------------------------------------------------------------------------
// Layout (virtual 1600 x 900 canvas)
// -----------------------------------------------------------------------------
const float DASH_X = 616.0f;      // free column under the city, between the two land grids
const float DASH_Y = 432.0f;
const float DASH_W = 368.0f;
const float SECTION_GAP = 5.0f;
const sf::FloatRect DISTRICT_STRIP({ 612.0f, 349.0f }, { 376.0f, 44.0f }); // city footer, one cell per tower pair
const float DISTRICT_TOWERS_TOP = 92.0f; // below the city banner
const sf::FloatRect P1_LAND({ 256.0f, 103.0f }, { 343.0f, 414.0f });
const sf::FloatRect P2_LAND({ 1001.0f, 103.0f }, { 343.0f, 414.0f });
const float ALERT_Y = 521.0f;     // banner just under each land grid
const float ALERT_H = 26.0f;
const sf::FloatRect REPORT_BOX({ 622.0f, 104.0f }, { 356.0f, 216.0f }); // over the city skyline
const float REPORT_SECONDS = 5.0f;

// Player clock cards (drawn by UI_clock at these rectangles in UI_map::render)
const sf::FloatRect CLOCK_P1({ 20.0f, 10.0f }, { 230.0f, 100.0f });
const sf::FloatRect CLOCK_P2({ 1350.0f, 10.0f }, { 230.0f, 100.0f });
const float CLOCK_BADGE_W = 68.0f;
const float CLOCK_BADGE_H = 18.0f;

const float FREQ_MIN_HZ = 47.5f;
const float FREQ_MAX_HZ = 50.5f;

// -----------------------------------------------------------------------------
// Small drawing helpers
// -----------------------------------------------------------------------------
enum class Align { Left, Center, Right };

sf::Color withAlpha(sf::Color c, float alpha01) {
    c.a = static_cast<std::uint8_t>(std::max(0.0f, std::min(1.0f, alpha01)) * c.a);
    return c;
}

sf::Color lerpColor(sf::Color a, sf::Color b, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    auto mix = [t](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(std::lround(x + (static_cast<float>(y) - x) * t));
    };
    return sf::Color(mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a));
}

// Draw queue: while a panel is being built, every rectangle / triangle goes into ONE vertex
// array and every text is deferred; flushQueue() draws the shapes first and the texts on top.
// This turns ~200 draw calls per frame into a handful (texts are always above shapes here).
struct DrawQueue {
    sf::VertexArray shapes{ sf::PrimitiveType::Triangles };
    std::vector<sf::Text> texts;
    bool active = false;
};
DrawQueue g_queue;

void beginQueue() {
    g_queue.shapes.clear();
    g_queue.texts.clear();
    g_queue.active = true;
}

void flushQueue(sf::RenderTarget& t) {
    if (g_queue.shapes.getVertexCount() > 0) t.draw(g_queue.shapes);
    for (const auto& text : g_queue.texts) t.draw(text);
    g_queue.shapes.clear();
    g_queue.texts.clear();
    g_queue.active = false;
}

void pushQuad(float x, float y, float w, float h, sf::Color c) {
    if (w <= 0.0f || h <= 0.0f || c.a == 0) return;
    const sf::Vector2f p0(x, y), p1(x + w, y), p2(x + w, y + h), p3(x, y + h);
    g_queue.shapes.append(sf::Vertex{ p0, c, {} });
    g_queue.shapes.append(sf::Vertex{ p1, c, {} });
    g_queue.shapes.append(sf::Vertex{ p2, c, {} });
    g_queue.shapes.append(sf::Vertex{ p0, c, {} });
    g_queue.shapes.append(sf::Vertex{ p2, c, {} });
    g_queue.shapes.append(sf::Vertex{ p3, c, {} });
}

void drawTriangle(sf::RenderTarget& t, sf::Vector2f a, sf::Vector2f b, sf::Vector2f c, sf::Color col) {
    if (g_queue.active) {
        g_queue.shapes.append(sf::Vertex{ a, col, {} });
        g_queue.shapes.append(sf::Vertex{ b, col, {} });
        g_queue.shapes.append(sf::Vertex{ c, col, {} });
        return;
    }
    sf::ConvexShape tri(3);
    tri.setPoint(0, a);
    tri.setPoint(1, b);
    tri.setPoint(2, c);
    tri.setFillColor(col);
    t.draw(tri);
}

// Filled rectangle with an optional outline drawn OUTSIDE it (same as sf::Shape outlines)
void drawRect(sf::RenderTarget& t, float x, float y, float w, float h, sf::Color fill,
              sf::Color outline = sf::Color::Transparent, float thickness = 0.0f) {
    if (w <= 0.0f || h <= 0.0f) return;
    if (g_queue.active) {
        pushQuad(x, y, w, h, fill);
        if (thickness > 0.0f && outline.a > 0) {
            const float k = thickness;
            pushQuad(x - k, y - k, w + 2.0f * k, k, outline); // top
            pushQuad(x - k, y + h, w + 2.0f * k, k, outline); // bottom
            pushQuad(x - k, y, k, h, outline);                // left
            pushQuad(x + w, y, k, h, outline);                // right
        }
        return;
    }
    sf::RectangleShape r({ w, h });
    r.setPosition({ x, y });
    r.setFillColor(fill);
    if (thickness > 0.0f) {
        r.setOutlineThickness(thickness);
        r.setOutlineColor(outline);
    }
    t.draw(r);
}

// Draws a text with its left / centre / right edge at x and its line top at y.
// The size shrinks (down to minSize) until the text fits maxWidth. Returns the drawn width.
float drawText(sf::RenderTarget& t, const sf::Font& font, const std::string& s, unsigned size, sf::Color color, float x,
               float y, Align align = Align::Left, float maxWidth = 0.0f, unsigned minSize = 9, bool bold = false) {
    sf::Text text(font, toUtf8(s), size);
    if (bold) text.setStyle(sf::Text::Bold);
    if (maxWidth > 0.0f) {
        while (size > minSize && text.getLocalBounds().size.x > maxWidth) {
            --size;
            text.setCharacterSize(size);
        }
    }
    text.setFillColor(color);
    const sf::FloatRect b = text.getLocalBounds();
    // Last resort: condense horizontally so the text never leaves its box
    float scaleX = 1.0f;
    if (maxWidth > 0.0f && b.size.x > maxWidth) scaleX = maxWidth / b.size.x;
    text.setScale({ scaleX, 1.0f });
    const float width = b.size.x * scaleX;
    float left = x;
    if (align == Align::Center) left = x - width / 2.0f;
    if (align == Align::Right) left = x - width;
    text.setPosition({ std::round(left - b.position.x * scaleX), std::round(y) });
    if (g_queue.active) g_queue.texts.push_back(text);
    else t.draw(text);
    return width;
}

float textWidth(const sf::Font& font, const std::string& s, unsigned size, bool bold = false) {
    sf::Text text(font, toUtf8(s), size);
    if (bold) text.setStyle(sf::Text::Bold);
    return text.getLocalBounds().size.x;
}

// Greedy word wrap to at most maxLines lines; the last line ends with "..." when cut
std::vector<std::string> wrapText(const sf::Font& font, const std::string& s, unsigned size, float maxWidth, int maxLines) {
    std::vector<std::string> words;
    std::string cur;
    for (char ch : s) {
        if (ch == ' ') {
            if (!cur.empty()) words.push_back(cur);
            cur.clear();
        } else {
            cur += ch;
        }
    }
    if (!cur.empty()) words.push_back(cur);

    std::vector<std::string> lines;
    std::string line;
    for (size_t i = 0; i < words.size(); ++i) {
        std::string candidate = line.empty() ? words[i] : line + " " + words[i];
        if (line.empty() || textWidth(font, candidate, size) <= maxWidth) {
            line = candidate;
        } else {
            lines.push_back(line);
            line = words[i];
            if (static_cast<int>(lines.size()) == maxLines) {
                std::string& last = lines.back();
                while (!last.empty() && textWidth(font, last + "...", size) > maxWidth) {
                    // drop the last UTF-8 character
                    size_t cut = last.size() - 1;
                    while (cut > 0 && (static_cast<unsigned char>(last[cut]) & 0xC0) == 0x80) --cut;
                    last.erase(cut);
                }
                last += "...";
                return lines;
            }
        }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

std::string fmtThousands(long long v) {
    bool neg = v < 0;
    std::string digits = std::to_string(neg ? -v : v);
    std::string out;
    int n = 0;
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i) {
        out.insert(out.begin(), digits[static_cast<size_t>(i)]);
        if (++n % 3 == 0 && i > 0) out.insert(out.begin(), ' ');
    }
    return neg ? "-" + out : out;
}

std::string fmtOneDecimal(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", v);
    return buf;
}

std::string fmtMWh(double mwh) {
    if (mwh >= 10000.0) return fmtOneDecimal(mwh / 1000.0) + " GWh";
    return fmtThousands(static_cast<long long>(std::llround(mwh))) + " MWh";
}

std::string fmtTonnes(double t) {
    if (t >= 10000.0) return fmtOneDecimal(t / 1000.0) + " хил. т";
    return fmtThousands(static_cast<long long>(std::llround(t))) + " т";
}

std::string fmtClock(float seconds) {
    int s = static_cast<int>(std::ceil(std::max(0.0f, seconds)));
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d:%02d", s / 60, s % 60);
    return buf;
}

std::string pct(float fraction) {
    return std::to_string(static_cast<int>(std::lround(std::max(0.0f, std::min(1.0f, fraction)) * 100.0f))) + "%";
}

void drawSection(sf::RenderTarget& t, sf::FloatRect box) {
    drawRect(t, box.position.x, box.position.y, box.size.x, box.size.y, COL_SECTION_BG, COL_PANEL_EDGE, 1.0f);
}

// Hour of the game day (06:00 -> 06:00) as a bar index 0..23
int daySlotOfHour(float hour24) {
    int h = static_cast<int>(std::floor(hour24));
    return ((h - 6) % 24 + 24) % 24;
}

const char* gridStateLabel(int state) {
    switch (state) {
        case Econ::GRID_BROWNOUT: return "ПОНИЖЕНО";
        case Econ::GRID_BLACKOUT: return "ЗАТЪМНЕНИЕ";
        default: return "НОРМА";
    }
}

} // namespace

// =============================================================================
UI_economyHUD::UI_economyHUD() : reportTimer(0.0f), reportAge(0.0f), lastShownDay(0), historyDay(-1) {
    reset();
}

void UI_economyHUD::reset() {
    reportTimer = 0.0f;
    reportAge = 0.0f;
    lastShownDay = 0;
    historyDay = -1;
    shownReport = Econ::DayReport();
    for (int i = 0; i < 2; ++i) {
        for (int h = 0; h < 24; ++h) {
            hourCovered[i][h] = 0.0f;
            hourCount[i][h] = 0.0f;
        }
    }
}

void UI_economyHUD::showLastReport(const GameEngine& engine) {
    const Econ::DayReport& r = engine.getCityEconomy().lastReport;
    if (!r.valid) return;
    shownReport = r;
    lastShownDay = r.day;
    reportTimer = REPORT_SECONDS;
    reportAge = 2.0f; // the bar animation is finished
}

void UI_economyHUD::update(GameEngine& engine, float dt) {
    // Hourly delivered-power history of the current game day (06:00 -> 06:00)
    const int day = engine.getCurrentDay();
    if (day != historyDay) {
        for (int i = 0; i < 2; ++i) {
            for (int h = 0; h < 24; ++h) {
                hourCovered[i][h] = 0.0f;
                hourCount[i][h] = 0.0f;
            }
        }
        historyDay = day;
    }
    if (dt > 0.0f) {
        const int slot = daySlotOfHour(engine.getHour24());
        for (int i = 0; i < 2; ++i) {
            const float quota = engine.getPlayerLoadTargetMW(i + 1);
            if (engine.getCityEconomy().lastDeliveredMW[i] + 0.5f >= quota) hourCovered[i][slot] += dt;
            hourCount[i][slot] += dt;
        }
    }

    // A settlement happened: open the Day Report card
    if (engine.consumeDayCut()) {
        const Econ::DayReport& r = engine.getCityEconomy().lastReport;
        if (r.valid && r.day != lastShownDay) {
            shownReport = r;
            lastShownDay = r.day;
            reportTimer = REPORT_SECONDS;
            reportAge = 0.0f;
        }
    }
    if (reportTimer > 0.0f) {
        reportTimer = std::max(0.0f, reportTimer - dt);
        reportAge += dt;
    }
    // The victory screen owns the end of the match: no report card behind it
    if (engine.getCityState().winner != 0) reportTimer = 0.0f;
}

// =============================================================================
// Dashboard
// =============================================================================
void UI_economyHUD::drawDashboard(sf::RenderTarget& target, const sf::Font& font, bool fontLoaded, const GameEngine& engine,
                                  float animTime, float maxBottom) {
    if (!fontLoaded) return;
    beginQueue(); // one batched draw for all shapes, texts on top
    const auto& ce = engine.getCityEconomy();
    const float share1 = engine.getCityState().p1CityShare;
    const bool badges = Econ::hasDominanceLevy(ce, 1, share1) || Econ::hasDominanceLevy(ce, 2, share1) ||
                        Econ::hasUnderdogSubsidy(ce, 1, share1) || Econ::hasUnderdogSubsidy(ce, 2, share1) ||
                        (std::max(share1, 1.0f - share1) >= Econ::DAMPING_START_SHARE && !engine.isGracePeriod());

    // Sections top to bottom; the ones that would pass maxBottom (e.g. above the tutorial card) are left out
    enum Section { CHIP, CURVE, FREQ, MIX, MSG, SECTION_COUNT };
    const float heights[SECTION_COUNT] = { badges ? 88.0f : 72.0f, 116.0f, 56.0f, 76.0f, 36.0f };
    int shown = 0;
    float totalH = 26.0f;
    for (int s = 0; s < SECTION_COUNT; ++s) {
        const float next = totalH + (s > 0 ? SECTION_GAP : 0.0f) + heights[s];
        if (DASH_Y + next + 4.0f > maxBottom) break;
        totalH = next;
        shown = s + 1;
    }

    // Panel and title bar
    drawRect(target, DASH_X - 4.0f, DASH_Y - 4.0f, DASH_W + 8.0f, totalH + 8.0f, COL_PANEL_BG, COL_PANEL_EDGE, 1.5f);
    drawText(target, font, "ЦЕНТЪР ЗА УПРАВЛЕНИЕ НА МРЕЖАТА", 12, COL_TITLE, DASH_X + DASH_W / 2.0f, DASH_Y + 3.0f,
             Align::Center, DASH_W - 16.0f, 10, true);
    drawRect(target, DASH_X + 8.0f, DASH_Y + 22.0f, DASH_W - 16.0f, 1.0f, withAlpha(COL_PANEL_EDGE, 0.9f));

    float y = DASH_Y + 26.0f;
    for (int s = 0; s < shown; ++s) {
        const sf::FloatRect box({ DASH_X, y }, { DASH_W, heights[s] });
        switch (s) {
            case CHIP: drawForecastChip(target, font, engine, box, animTime); break;
            case CURVE: drawDemandCurve(target, font, engine, box); break;
            case FREQ: drawFrequency(target, font, engine, box, animTime); break;
            case MIX: drawEnergyMix(target, font, engine, box); break;
            case MSG: drawLastMessage(target, font, engine, box); break;
            default: break;
        }
        y += heights[s] + SECTION_GAP;
    }
    flushQueue(target);
}

// ---- UX-01 settlement forecast chip ------------------------------------------------
void UI_economyHUD::drawForecastChip(sf::RenderTarget& target, const sf::Font& font, const GameEngine& engine,
                                     sf::FloatRect box, float animTime) {
    drawSection(target, box);
    const float x = box.position.x;
    const float y = box.position.y;
    const float w = box.size.x;
    const Econ::Forecast f = engine.projectPlayerOutput();
    const float realSeconds = f.secondsToSettlement / std::max(0.1f, engine.getTimeScale());

    if (!f.active) {
        drawText(target, font, "ГРАТИСЕН ПЕРИОД: ГРАДЪТ ИСКА 0 MW", 12, COL_GRACE, x + w / 2.0f, y + 6.0f, Align::Center,
                 w - 16.0f, 10, true);
        const int daysLeft = Balance::GRACE_PERIOD_DAYS - engine.getCurrentDay() + 1;
        std::string sub = "ПЪРВИЯТ ОТЧЕТ Е СЛЕД ДЕН " + std::to_string(Balance::GRACE_PERIOD_DAYS + 1) + " · ОЩЕ " +
                          std::to_string(std::max(1, daysLeft)) + (daysLeft == 1 ? " ДЕН" : " ДНИ") + " ЗА СТРОЕЖ";
        drawText(target, font, sub, 11, COL_DIM, x + w / 2.0f, y + 26.0f, Align::Center, w - 16.0f);
        drawText(target, font, "СЛЕДВАЩ ДЕН СЛЕД " + fmtClock(realSeconds), 11, COL_TEXT, x + w / 2.0f, y + 40.0f,
                 Align::Center, w - 16.0f);
    } else {
        // Row 1: countdown and projected shift
        const bool soon = realSeconds < 10.0f;
        const bool blink = soon && std::sin(animTime * 8.0f) > 0.0f;
        drawText(target, font, "ОТЧЕТ СЛЕД " + fmtClock(realSeconds), 13, blink ? COL_WARN : COL_TEXT, x + 10.0f, y + 5.0f,
                 Align::Left, 0.0f, 9, true);
        const int movePct = static_cast<int>(std::lround(std::abs(f.projectedShift) * 100.0f));
        std::string proj;
        sf::Color projCol = COL_DIM;
        if (movePct == 0) {
            proj = "ПРОГНОЗА: РАВЕН ДЕН";
        } else {
            const int gainer = (f.projectedShift > 0.0f) ? 1 : 2;
            proj = "ПРОГНОЗА: P" + std::to_string(gainer) + " +" + std::to_string(movePct) + "%";
            projCol = (gainer == 1) ? COL_P1 : COL_P2;
        }
        drawText(target, font, proj, 12, projCol, x + w - 10.0f, y + 6.0f, Align::Right, 170.0f, 9, true);

        // Row 2: one pill per player: delivered now / quota now and status
        const float pillW = (w - 30.0f) / 2.0f;
        for (int i = 0; i < 2; ++i) {
            const float px = x + 10.0f + i * (pillW + 10.0f);
            const float py = y + 27.0f;
            const sf::Color pc = (i == 0) ? COL_P1 : COL_P2;
            const float now = f.nowMW[i];
            const float quota = std::max(0.01f, f.quotaNowMW[i]);
            const float ratio = now / quota;
            sf::Color statusCol = COL_OK;
            std::string status = "OK";
            if (ratio < 1.0f - 1e-3f) {
                statusCol = COL_BAD;
                status = "НЕДОСТИГ";
            } else if (ratio < 1.15f) {
                statusCol = COL_WARN;
                status = "РИСК";
            }
            drawRect(target, px, py, pillW, 24.0f, sf::Color(10, 15, 25, 230), withAlpha(statusCol, 0.85f), 1.0f);
            drawRect(target, px, py, 4.0f, 24.0f, pc);
            const std::string mw = "P" + std::to_string(i + 1) + " " + std::to_string(static_cast<int>(std::lround(now))) + "/" +
                                   std::to_string(static_cast<int>(std::lround(f.quotaNowMW[i]))) + " MW";
            drawText(target, font, mw, 11, pc, px + 9.0f, py + 4.0f, Align::Left, pillW - 70.0f, 9);
            drawText(target, font, status, 11, statusCol, px + pillW - 7.0f, py + 4.0f, Align::Right, 64.0f, 9, true);
        }
        // Served so far today: exactly what the 06:00 settlement measures
        const std::string servedLine = "ОБСЛУЖЕНА НУЖДА ДОСЕГА: P1 " + pct(f.served[0]) + " · P2 " + pct(f.served[1]);
        drawText(target, font, servedLine, 10, COL_DIM, x + w / 2.0f, y + 55.0f, Align::Center, w - 16.0f);
    }

    // BAL-04 badges
    const auto& ce = engine.getCityEconomy();
    const float share1 = engine.getCityState().p1CityShare;
    std::string badge;
    sf::Color badgeCol = COL_WARN;
    for (int p = 1; p <= 2; ++p) {
        if (Econ::hasDominanceLevy(ce, p, share1)) {
            if (!badge.empty()) badge += " · ";
            badge += "P" + std::to_string(p) + ": ДАНЪК -15% ДОХОД";
        }
        if (Econ::hasUnderdogSubsidy(ce, p, share1)) {
            if (!badge.empty()) badge += " · ";
            badge += "P" + std::to_string(p) + ": СУБСИДИЯ -15% ЦЕНИ";
        }
    }
    if (badge.empty() && !engine.isGracePeriod() && std::max(share1, 1.0f - share1) >= Econ::DAMPING_START_SHARE) {
        const int leader = (share1 >= 0.5f) ? 1 : 2;
        badge = "P" + std::to_string(leader) + " Е НАД 70%: ПЕЧАЛБИТЕ МУ СЕ ПОЛОВЯТ";
        badgeCol = COL_DIM;
    }
    if (!badge.empty() && box.size.y > 80.0f) {
        drawText(target, font, badge, 10, badgeCol, x + w / 2.0f, y + 70.0f, Align::Center, w - 16.0f, 9);
    }
}

// ---- BAL-03 demand curve "НУЖДА СЕГА" ----------------------------------------------
void UI_economyHUD::drawDemandCurve(sf::RenderTarget& target, const sf::Font& font, const GameEngine& engine,
                                    sf::FloatRect box) {
    drawSection(target, box);
    const float x = box.position.x;
    const float y = box.position.y;
    const float w = box.size.x;
    const float hour = engine.getHour24();
    const int baseDemand = engine.getCityState().cityEnergyDemand;
    const bool grace = baseDemand <= 0;
    const float profileNow = Econ::getCityDemandProfileAt(hour);
    const bool peak = Econ::isPeakHour(hour);

    // Title row
    std::string title = grace ? "НУЖДА СЕГА: 0 MW" : "НУЖДА СЕГА: " + std::to_string(static_cast<int>(std::lround(engine.getCurrentDemandMW()))) + " MW";
    drawText(target, font, title, 12, grace ? COL_GRACE : COL_TEXT, x + 10.0f, y + 4.0f, Align::Left, 0.0f, 9, true);
    std::string price = "ЦЕНА x" + fmtOneDecimal(Econ::getPeakPriceFactor(hour));
    if (peak) price = "ВЕЧЕРЕН ПИК · " + price;
    if (grace) price = "ФОРМА НА НУЖДАТА ОТ ДЕН 3";
    drawText(target, font, price, 11, grace ? COL_GRACE : (peak ? COL_BAR_PEAK : COL_DIM), x + w - 10.0f, y + 5.0f,
             Align::Right, 190.0f);

    // Chart geometry: demand bars, then one coverage row per player, axis labels and legend
    const float chartX = x + 10.0f;
    const float chartW = w - 20.0f;
    const float chartTop = y + 24.0f;
    const float chartH = 48.0f;
    const float chartBottom = chartTop + chartH;
    const float slotW = chartW / 24.0f;
    float maxProfile = 0.0f;
    for (int h = 0; h < 24; ++h) maxProfile = std::max(maxProfile, Econ::getCityDemandProfile(h));
    const int nowSlot = daySlotOfHour(hour);

    drawRect(target, chartX, chartTop, chartW, chartH, sf::Color(8, 12, 20, 200));
    drawRect(target, chartX, chartTop + 4.0f, chartW, 1.0f, sf::Color(255, 255, 255, 26)); // peak level

    for (int k = 0; k < 24; ++k) {
        const int h = (6 + k) % 24;
        const float prof = Econ::getCityDemandProfile(h);
        const float bh = (chartH - 4.0f) * prof / maxProfile;
        sf::Color c = (prof >= Econ::PEAK_PROFILE_THRESHOLD) ? COL_BAR_PEAK : (prof >= 0.9f ? COL_BAR_MID : COL_BAR_LOW);
        if (grace) c = withAlpha(c, 0.45f);
        if (k == nowSlot) c = lerpColor(c, sf::Color::White, 0.45f);
        else if (k > nowSlot) c = withAlpha(c, 0.7f);
        drawRect(target, chartX + k * slotW + 1.5f, chartBottom - bh, slotW - 3.0f, bh, c);
    }

    // "Now" needle
    const float frac = std::fmod(hour - 6.0f + 24.0f, 24.0f) / 24.0f;
    drawRect(target, chartX + chartW * frac - 1.0f, chartTop - 2.0f, 2.0f, chartH + 4.0f, sf::Color(255, 255, 255, 210));

    // Coverage rows: did each player deliver its full quota in that hour? (elapsed hours only)
    for (int i = 0; i < 2; ++i) {
        const float ry = chartBottom + 3.0f + i * 6.0f;
        const sf::Color pc = (i == 0) ? COL_P1 : COL_P2;
        for (int k = 0; k < 24; ++k) {
            sf::Color c = sf::Color(40, 50, 66, 200); // future hour
            if (!grace && k <= nowSlot && hourCount[i][k] > 0.0f) {
                const float covered = hourCovered[i][k] / hourCount[i][k];
                c = (covered >= 0.99f) ? pc : (covered >= 0.5f ? withAlpha(pc, 0.45f) : COL_BAD);
            }
            drawRect(target, chartX + k * slotW + 1.5f, ry, slotW - 3.0f, 4.0f, c);
        }
    }

    // Axis labels (the game day runs 06:00 -> 06:00)
    const float axisY = chartBottom + 15.0f;
    const char* labels[5] = { "06", "12", "18", "00", "06" };
    for (int i = 0; i < 5; ++i) {
        const float lx = chartX + chartW * (i / 4.0f);
        Align a = (i == 0) ? Align::Left : (i == 4 ? Align::Right : Align::Center);
        drawText(target, font, labels[i], 10, COL_DIM, lx, axisY, a);
    }

    // Legend: P1 / P2 covered hours, shortfall, peak
    struct LegendItem {
        sf::Color c;
        const char* label;
    };
    const LegendItem items[4] = { { COL_P1, "P1 покрит час" }, { COL_P2, "P2 покрит час" }, { COL_BAD, "недостиг" },
                                  { COL_BAR_PEAK, "пик" } };
    float legendW = 0.0f;
    static float labelW[4] = { -1.0f, -1.0f, -1.0f, -1.0f }; // measured once
    for (int i = 0; i < 4; ++i) {
        if (labelW[i] < 0.0f) labelW[i] = textWidth(font, items[i].label, 10);
        legendW += 12.0f + labelW[i] + 10.0f;
    }
    float lx = x + (w - legendW) / 2.0f;
    const float ly = axisY + 14.0f;
    for (const auto& it : items) {
        drawRect(target, lx, ly + 4.0f, 8.0f, 8.0f, it.c);
        lx += 12.0f;
        lx += drawText(target, font, it.label, 10, COL_DIM, lx, ly) + 10.0f;
    }
    (void)profileNow;
}


// ---- F-37 grid-frequency gauges ----------------------------------------------------
void UI_economyHUD::drawFrequency(sf::RenderTarget& target, const sf::Font& font, const GameEngine& engine, sf::FloatRect box,
                                  float animTime) {
    drawSection(target, box);
    const float x = box.position.x;
    const float y = box.position.y;
    const float w = box.size.x;
    drawText(target, font, "ЧЕСТОТА НА МРЕЖАТА", 12, COL_TITLE, x + 10.0f, y + 4.0f, Align::Left, 0.0f, 9, true);
    drawText(target, font, "под 49.2 Hz: x0.7 · под 48 Hz: 0 MW", 10, COL_DIM, x + w - 10.0f, y + 6.0f, Align::Right, 180.0f);

    const float colW = (w - 30.0f) / 2.0f;
    for (int i = 0; i < 2; ++i) {
        const float cx = x + 10.0f + i * (colW + 10.0f);
        const float cy = y + 24.0f;
        const int player = i + 1;
        const float hz = engine.getGridFrequencyHz(player);
        const int state = engine.getGridState(player);
        const sf::Color pc = (i == 0) ? COL_P1 : COL_P2;
        sf::Color stateCol = (state == Econ::GRID_BLACKOUT) ? COL_BAD : (state == Econ::GRID_BROWNOUT ? COL_WARN : COL_OK);
        if (state != Econ::GRID_NORMAL && std::sin(animTime * 7.0f) < -0.3f) stateCol = withAlpha(stateCol, 0.55f);

        drawText(target, font, "P" + std::to_string(player), 12, pc, cx, cy, Align::Left, 0.0f, 9, true);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.1f Hz", hz);
        sf::Color hzCol = (hz < Econ::BLACKOUT_HZ) ? COL_BAD : (hz < Econ::BROWNOUT_HZ ? COL_WARN : COL_TEXT);
        drawText(target, font, buf, 13, hzCol, cx + 24.0f, cy - 1.0f, Align::Left, 0.0f, 9, true);
        drawText(target, font, gridStateLabel(state), 10, stateCol, cx + colW, cy + 1.0f, Align::Right, colW - 100.0f, 9, true);

        // Scale bar with zones and needle
        const float bx = cx;
        const float by = cy + 21.0f;
        const float bw = colW;
        const float bh = 7.0f;
        auto xOf = [&](float v) { return bx + bw * (std::max(FREQ_MIN_HZ, std::min(FREQ_MAX_HZ, v)) - FREQ_MIN_HZ) / (FREQ_MAX_HZ - FREQ_MIN_HZ); };
        drawRect(target, bx, by, xOf(Econ::BLACKOUT_HZ) - bx, bh, withAlpha(COL_BAD, 0.75f));
        drawRect(target, xOf(Econ::BLACKOUT_HZ), by, xOf(Econ::BROWNOUT_HZ) - xOf(Econ::BLACKOUT_HZ), bh, withAlpha(COL_WARN, 0.75f));
        drawRect(target, xOf(Econ::BROWNOUT_HZ), by, bx + bw - xOf(Econ::BROWNOUT_HZ), bh, withAlpha(COL_OK, 0.6f));
        drawRect(target, xOf(Econ::NOMINAL_HZ) - 0.5f, by - 2.0f, 1.0f, bh + 4.0f, sf::Color(255, 255, 255, 120));
        const float nx = xOf(hz);
        drawTriangle(target, { nx - 4.0f, by - 5.0f }, { nx + 4.0f, by - 5.0f }, { nx, by + 1.0f }, sf::Color::White);
        drawRect(target, nx - 1.0f, by, 2.0f, bh, sf::Color::White);
    }
}

// ---- F-11 energy mix and CO2 ledger ----------------------------------------------
void UI_economyHUD::drawEnergyMix(sf::RenderTarget& target, const sf::Font& font, const GameEngine& engine, sf::FloatRect box) {
    drawSection(target, box);
    const float x = box.position.x;
    const float y = box.position.y;
    const float w = box.size.x;
    const auto& ce = engine.getCityEconomy();
    drawText(target, font, "ЕНЕРГИЕН МИКС И CO2", 12, COL_TITLE, x + 10.0f, y + 4.0f, Align::Left, 0.0f, 9, true);
    // Legend
    const sf::Color srcCol[3] = { COL_SOLAR, COL_WIND, COL_HYDRO };
    float lx = x + w - 10.0f;
    for (int s = 2; s >= 0; --s) {
        const std::string name = Econ::getSourceNameBg(s);
        const float tw = drawText(target, font, name, 10, COL_DIM, lx, y + 6.0f, Align::Right);
        lx -= tw + 4.0f;
        drawRect(target, lx - 8.0f, y + 9.0f, 8.0f, 8.0f, srcCol[s]);
        lx -= 16.0f;
    }

    double maxTotal = 1.0;
    for (int i = 0; i < 2; ++i) {
        const auto& led = ce.ledger[i];
        maxTotal = std::max(maxTotal, led.generatedMWh[0] + led.generatedMWh[1] + led.generatedMWh[2]);
    }
    const float barX = x + 34.0f;
    const float barW = w - 34.0f - 118.0f;
    for (int i = 0; i < 2; ++i) {
        const auto& led = ce.ledger[i];
        const float by = y + 25.0f + i * 18.0f;
        const sf::Color pc = (i == 0) ? COL_P1 : COL_P2;
        drawText(target, font, "P" + std::to_string(i + 1), 11, pc, x + 10.0f, by - 1.0f, Align::Left, 0.0f, 9, true);
        drawRect(target, barX, by + 1.0f, barW, 12.0f, sf::Color(8, 12, 20, 220), withAlpha(COL_PANEL_EDGE, 0.8f), 1.0f);
        float bx = barX;
        for (int s = 0; s < 3; ++s) {
            const float segW = static_cast<float>(barW * led.generatedMWh[s] / maxTotal);
            drawRect(target, bx, by + 1.0f, segW, 12.0f, srcCol[s]);
            bx += segW;
        }
        drawText(target, font, "→ " + fmtMWh(led.deliveredMWh), 11, COL_TEXT, x + w - 10.0f, by - 1.0f, Align::Right, 112.0f, 9);
    }
    // CO2 row and battery throughput
    const std::string co2 = "CO2 СПЕСТЕНИ: P1 " + fmtTonnes(Econ::co2AvoidedT(ce.ledger[0])) + " · P2 " +
                            fmtTonnes(Econ::co2AvoidedT(ce.ledger[1]));
    drawText(target, font, co2, 11, COL_CO2, x + 10.0f, y + 59.0f, Align::Left, w - 20.0f, 9, true);
}

// ---- Last settlement message (replaces the old 10 pt city footer) --------------------
float UI_economyHUD::drawLastMessage(sf::RenderTarget& target, const sf::Font& font, const GameEngine& engine, sf::FloatRect box) {
    const std::string& msg = engine.getCityState().lastCutMessage;
    drawSection(target, box);
    if (msg.empty()) return 0.0f;
    if (msg != wrappedFor) {
        wrappedLines = wrapText(font, msg, 10, box.size.x - 20.0f, 2);
        wrappedFor = msg;
    }
    const std::vector<std::string>& lines = wrappedLines;
    float ly = box.position.y + (lines.size() == 1 ? 11.0f : 4.0f);
    for (const auto& l : lines) {
        drawText(target, font, l, 10, sf::Color(255, 220, 110), box.position.x + box.size.x / 2.0f, ly, Align::Center);
        ly += 15.0f;
    }
    return box.size.y;
}

// =============================================================================
// F-36 district strip in the city footer
// =============================================================================
void UI_economyHUD::drawDistricts(sf::RenderTarget& target, const sf::Font& font, bool fontLoaded, const GameEngine& engine,
                                  float animTime) {
    beginQueue();
    const auto& ce = engine.getCityEconomy();
    const sf::FloatRect s = DISTRICT_STRIP;
    drawRect(target, s.position.x, s.position.y, s.size.x, s.size.y, sf::Color(12, 17, 28, 240), COL_PANEL_EDGE, 1.0f);
    const float hour = engine.getHour24();
    const bool grace = engine.getCityState().cityEnergyDemand <= 0;

    // The district with the biggest share of the load this hour is highlighted
    int busiest = 0;
    float busiestLoad = -1.0f;
    for (int d = 0; d < Econ::DISTRICT_COUNT; ++d) {
        float load = Econ::getDistrictDef(d).weight * Econ::getDistrictProfileAt(d, hour);
        if (load > busiestLoad) {
            busiestLoad = load;
            busiest = d;
        }
    }

    const float cellW = s.size.x / Econ::DISTRICT_COUNT;
    for (int d = 0; d < Econ::DISTRICT_COUNT; ++d) {
        const auto& def = Econ::getDistrictDef(d);
        const auto& ds = ce.districts[d];
        const float cx = s.position.x + d * cellW;
        const float cy = s.position.y;
        if (d > 0) drawRect(target, cx, cy + 4.0f, 1.0f, s.size.y - 8.0f, withAlpha(COL_PANEL_EDGE, 0.8f));
        if (d == busiest && !grace) {
            const float pulse = 0.55f + 0.45f * std::sin(animTime * 3.0f);
            drawRect(target, cx + 3.0f, cy + 1.0f, cellW - 6.0f, 2.0f, withAlpha(COL_BAR_PEAK, pulse));
        }

        // Coverage right now: the better of the two players
        float coverage = 1.0f;
        if (!grace && ds.demandNowMW > 0.01f) {
            float best = 0.0f;
            for (int i = 0; i < 2; ++i) {
                if (ds.quotaNowMW[i] > 0.0f) best = std::max(best, ds.allocNowMW[i] / ds.quotaNowMW[i]);
            }
            coverage = best;
        }
        // The two towers above this cell form the district: they go dark while it is underserved
        if (!grace && coverage < 0.999f) {
            const float dark = std::min(1.0f, (1.0f - coverage) * 1.4f);
            drawRect(target, cx, DISTRICT_TOWERS_TOP, cellW, cy - DISTRICT_TOWERS_TOP, sf::Color(4, 6, 12, static_cast<std::uint8_t>(170 * dark)));
        }
        sf::Color nameCol = COL_TEXT;
        if (coverage < 0.5f) nameCol = (std::sin(animTime * 6.0f) > 0.0f) ? COL_BAD : withAlpha(COL_BAD, 0.6f);
        else if (coverage < 0.999f) nameCol = COL_WARN;

        if (fontLoaded) drawText(target, font, def.nameBg, 10, nameCol, cx + cellW / 2.0f, cy + 4.0f, Align::Center, cellW - 8.0f, 8, true);

        // Ownership mini-bar
        const float barW = cellW - 18.0f;
        const float barX = cx + 9.0f;
        const float barY = cy + 19.0f;
        drawRect(target, barX, barY, barW * ds.p1Share, 6.0f, COL_P1);
        drawRect(target, barX + barW * ds.p1Share, barY, barW * (1.0f - ds.p1Share), 6.0f, COL_P2);
        drawRect(target, barX + barW * 0.5f - 0.5f, barY - 2.0f, 1.0f, 10.0f, sf::Color(255, 255, 255, 140));

        if (fontLoaded) {
            const std::string mw = grace ? "0 MW" : std::to_string(static_cast<int>(std::lround(ds.demandNowMW))) + " MW";
            drawText(target, font, mw, 10, COL_DIM, barX, cy + 27.0f, Align::Left);
            drawText(target, font, pct(ds.p1Share), 10, COL_P1, barX + barW, cy + 27.0f, Align::Right);
        }
    }
    flushQueue(target);
}

// =============================================================================
// F-37 brownout / blackout banners on the player sectors
// =============================================================================
void UI_economyHUD::drawGridAlerts(sf::RenderTarget& target, const sf::Font& font, bool fontLoaded, const GameEngine& engine,
                                   float animTime) {
    beginQueue();
    for (int player = 1; player <= 2; ++player) {
        const int state = engine.getGridState(player);
        if (state == Econ::GRID_NORMAL) continue;
        const sf::FloatRect land = (player == 1) ? P1_LAND : P2_LAND;
        const bool blackout = (state == Econ::GRID_BLACKOUT);
        if (blackout) {
            // The sector goes dark
            drawRect(target, land.position.x, land.position.y, land.size.x, land.size.y, sf::Color(0, 0, 8, 95));
        }
        const sf::Color col = blackout ? COL_BAD : COL_WARN;
        const float pulse = 0.65f + 0.35f * std::sin(animTime * 7.0f);
        drawRect(target, land.position.x, ALERT_Y, land.size.x, ALERT_H, sf::Color(30, 8, 10, 235), withAlpha(col, pulse), 2.0f);
        if (!fontLoaded) continue;
        const auto& g = engine.getCityEconomy().grid[player - 1];
        char hz[16];
        std::snprintf(hz, sizeof(hz), "%.1f Hz", g.frequencyHz);
        const int minutesLeft = static_cast<int>(std::ceil(g.stateHoursLeft * 60.0f)); // in-game minutes
        std::string text = blackout ? "ЗАТЪМНЕНИЕ! " + std::string(hz) + " · 0 MW"
                                    : "ПОНИЖЕНО НАПРЕЖЕНИЕ " + std::string(hz) + " · x0.7";
        text += " · " + std::to_string(std::max(1, minutesLeft)) + " мин";
        drawText(target, font, text, 12, withAlpha(col, 0.75f + 0.25f * pulse), land.position.x + land.size.x / 2.0f,
                 ALERT_Y + 5.0f, Align::Center, land.size.x - 14.0f, 10, true);
    }
    flushQueue(target);
}

// =============================================================================
// UX-01 Day Report card (non-modal)
// =============================================================================
void UI_economyHUD::drawDayReport(sf::RenderTarget& target, const sf::Font& font, bool fontLoaded) {
    if (reportTimer <= 0.0f || !shownReport.valid || !fontLoaded) return;
    beginQueue();
    const Econ::DayReport& r = shownReport;

    // Slide in for 0.3 s, fade out over the last 0.6 s
    const float appear = std::min(1.0f, reportAge / 0.3f);
    const float fade = std::min(1.0f, reportTimer / 0.6f);
    const float alpha = std::min(appear, fade);
    const float slide = (1.0f - appear) * -18.0f;

    const float x = REPORT_BOX.position.x;
    const float y = REPORT_BOX.position.y + slide;
    const float w = REPORT_BOX.size.x;
    const float h = REPORT_BOX.size.y;
    const int moved = static_cast<int>(std::lround(std::abs(r.appliedShift) * 100.0f));
    const int gainer = (moved == 0) ? 0 : (r.appliedShift > 0.0f ? 1 : 2);
    const sf::Color accent = (gainer == 1) ? COL_P1 : (gainer == 2 ? COL_P2 : COL_TITLE);

    drawRect(target, x, y, w, h, withAlpha(sf::Color(10, 15, 26, 248), alpha), withAlpha(accent, alpha), 2.0f);
    drawRect(target, x, y, w, 28.0f, withAlpha(sf::Color(22, 34, 56, 250), alpha));
    drawText(target, font, "ДЕН " + std::to_string(r.day) + " · ОТЧЕТ НА ГРАДА", 14, withAlpha(COL_TEXT, alpha), x + 12.0f,
             y + 5.0f, Align::Left, w - 142.0f, 10, true); // never reaches the demand label on the right
    drawText(target, font, "СР. НУЖДА " + std::to_string(r.demandMW) + " MW", 11, withAlpha(COL_DIM, alpha), x + w - 12.0f,
             y + 8.0f, Align::Right, 110.0f);

    // Table: label | P1 | P2
    const float colP1 = x + 196.0f;
    const float colP2 = x + w - 14.0f;
    float ry = y + 36.0f;
    drawText(target, font, "ИГРАЧ 1", 11, withAlpha(COL_P1, alpha), colP1, ry, Align::Right, 0.0f, 9, true);
    drawText(target, font, "ИГРАЧ 2", 11, withAlpha(COL_P2, alpha), colP2, ry, Align::Right, 0.0f, 9, true);
    ry += 18.0f;

    auto gridText = [](int brown, int black) {
        if (brown == 0 && black == 0) return std::string("без аварии");
        std::string s;
        if (black > 0) s += std::to_string(black) + " затъмн.";
        if (brown > 0) s += (s.empty() ? "" : " ") + std::to_string(brown) + " пониж.";
        return s;
    };
    struct Row {
        std::string label, a, b;
        sf::Color ca, cb;
    };
    const Row rows[4] = {
        { "Обслужена нужда", pct(r.served[0]), pct(r.served[1]), r.served[0] >= r.served[1] ? COL_OK : COL_TEXT,
          r.served[1] >= r.served[0] ? COL_OK : COL_TEXT },
        { "Доставено", fmtMWh(r.deliveredMWh[0]), fmtMWh(r.deliveredMWh[1]), COL_TEXT, COL_TEXT },
        { "CO2 спестени", fmtTonnes(r.co2AvoidedT[0]), fmtTonnes(r.co2AvoidedT[1]), COL_CO2, COL_CO2 },
        { "Мрежа", gridText(r.brownouts[0], r.blackouts[0]), gridText(r.brownouts[1], r.blackouts[1]),
          (r.brownouts[0] + r.blackouts[0]) ? COL_WARN : COL_OK, (r.brownouts[1] + r.blackouts[1]) ? COL_WARN : COL_OK },
    };
    for (const Row& row : rows) {
        drawText(target, font, row.label, 11, withAlpha(COL_DIM, alpha), x + 12.0f, ry, Align::Left, 110.0f);
        drawText(target, font, row.a, 11, withAlpha(row.ca, alpha), colP1, ry, Align::Right, 72.0f);
        drawText(target, font, row.b, 11, withAlpha(row.cb, alpha), colP2, ry, Align::Right, 120.0f);
        ry += 17.0f;
    }

    // Territory bar animating from the old to the new share
    ry += 4.0f;
    const float t = std::min(1.0f, std::max(0.0f, (reportAge - 0.4f) / 1.4f));
    const float ease = 1.0f - (1.0f - t) * (1.0f - t);
    const float share = r.shareBefore + (r.shareAfter - r.shareBefore) * ease;
    const int p1Before = static_cast<int>(std::lround(r.shareBefore * 100.0f));
    const int p1After = static_cast<int>(std::lround(r.shareAfter * 100.0f));
    std::string terr = "ТЕРИТОРИЯ P1 " + std::to_string(p1Before) + "% → " + std::to_string(p1After) + "%";
    if (gainer != 0) terr += "   (P" + std::to_string(gainer) + " +" + std::to_string(moved) + "%)";
    else terr += "   (без промяна)";
    drawText(target, font, terr, 12, withAlpha(accent, alpha), x + w / 2.0f, ry, Align::Center, w - 24.0f, 9, true);
    ry += 19.0f;
    const float barX = x + 14.0f;
    const float barW = w - 28.0f;
    drawRect(target, barX, ry, barW * share, 10.0f, withAlpha(COL_P1, alpha));
    drawRect(target, barX + barW * share, ry, barW * (1.0f - share), 10.0f, withAlpha(COL_P2, alpha));
    drawRect(target, barX + barW * r.shareBefore - 0.5f, ry - 3.0f, 1.0f, 16.0f, withAlpha(sf::Color(255, 255, 255, 110), alpha));
    drawRect(target, barX + barW * share - 1.5f, ry - 3.0f, 3.0f, 16.0f, withAlpha(sf::Color::White, alpha));
    for (float tick : { Balance::VICTORY_SHARE, 1.0f - Balance::VICTORY_SHARE }) {
        drawRect(target, barX + barW * tick - 1.0f, ry - 2.0f, 2.0f, 14.0f, withAlpha(sf::Color(255, 215, 0, 220), alpha));
    }
    ry += 18.0f;

    // District shifts
    const float cellW = (w - 24.0f) / Econ::DISTRICT_COUNT;
    for (int d = 0; d < Econ::DISTRICT_COUNT; ++d) {
        const float shift = r.districtShift[d];
        const int sp = static_cast<int>(std::lround(std::abs(shift) * 100.0f));
        sf::Color c = (sp == 0) ? COL_DIM : (shift > 0.0f ? COL_P1 : COL_P2);
        std::string txt = std::string(Econ::getDistrictDef(d).nameBg);
        drawText(target, font, txt, 10, withAlpha(COL_DIM, alpha), x + 12.0f + cellW * (d + 0.5f), ry, Align::Center, cellW - 4.0f, 8);
        std::string val = (sp == 0) ? "0%" : ((shift > 0.0f ? "P1 +" : "P2 +") + std::to_string(sp) + "%");
        drawText(target, font, val, 11, withAlpha(c, alpha), x + 12.0f + cellW * (d + 0.5f), ry + 13.0f, Align::Center, cellW - 4.0f, 8, true);
    }
    if (r.damped) {
        drawText(target, font, "Лидерът е над 70%: печалбата му е наполовина", 10, withAlpha(COL_DIM, alpha), x + w / 2.0f,
                 y + h - 19.0f, Align::Center, w - 24.0f);
    }
    flushQueue(target);
}

// =============================================================================
// F-37 "50.0 Hz" badges on the player clocks
// =============================================================================
void UI_economyHUD::drawClockFrequencyBadges(sf::RenderTarget& target, const sf::Font& font, bool fontLoaded,
                                             const GameEngine& engine, float animTime) {
    // During the grace period the clock's sun line is longer and the grid cannot fail: no badge
    if (!fontLoaded || engine.isGracePeriod()) return;
    beginQueue();
    for (int player = 1; player <= 2; ++player) {
        const sf::FloatRect clock = (player == 1) ? CLOCK_P1 : CLOCK_P2;
        const float bx = clock.position.x + clock.size.x - CLOCK_BADGE_W - 6.0f;
        const float by = clock.position.y + clock.size.y - CLOCK_BADGE_H - 5.0f;
        const float hz = engine.getGridFrequencyHz(player);
        const int state = engine.getGridState(player);
        sf::Color c = (state == Econ::GRID_BLACKOUT || hz < Econ::BLACKOUT_HZ)
                          ? COL_BAD
                          : ((state == Econ::GRID_BROWNOUT || hz < Econ::BROWNOUT_HZ) ? COL_WARN : COL_OK);
        if (state != Econ::GRID_NORMAL && std::sin(animTime * 7.0f) < -0.3f) c = withAlpha(c, 0.5f);
        drawRect(target, bx, by, CLOCK_BADGE_W, CLOCK_BADGE_H, sf::Color(8, 12, 20, 235), withAlpha(c, 0.9f), 1.0f);
        drawRect(target, bx + 5.0f, by + CLOCK_BADGE_H / 2.0f - 3.0f, 6.0f, 6.0f, c);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.1f Hz", hz);
        drawText(target, font, buf, 11, c, bx + CLOCK_BADGE_W - 5.0f, by + 2.0f, Align::Right, CLOCK_BADGE_W - 16.0f, 9, true);
    }
    flushQueue(target);
}
