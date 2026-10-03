// =============================================================================
// F-21 Practice sandbox control panel [team b-options]
// =============================================================================
#include "../includes/UI_sandbox.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

// --- Colours (integrator: map these to UI_theme.h tokens) ---------------------------------
const sf::Color COL_PANEL(14, 20, 32, 250);
const sf::Color COL_EDGE(255, 190, 70);
const sf::Color COL_HEADER(40, 32, 16);
const sf::Color COL_TITLE(255, 205, 90);
const sf::Color COL_TEXT(232, 240, 252);
const sf::Color COL_MUTED(150, 168, 192);
const sf::Color COL_BTN(30, 42, 62);
const sf::Color COL_BTN_EDGE(70, 92, 124);
const sf::Color COL_BTN_ON(150, 104, 28);
const sf::Color COL_BTN_ON_EDGE(255, 215, 90);
const sf::Color COL_CHART_BG(10, 14, 22);
const sf::Color COL_DAYLIGHT(90, 80, 30, 70);
const sf::Color COL_GRID(70, 85, 110, 110);
const sf::Color COL_GEN(0, 229, 255);
const sf::Color COL_GEN_FILL(0, 229, 255, 55);
const sf::Color COL_DEMAND(255, 205, 70);
const sf::Color COL_GOOD(120, 238, 150);
const sf::Color COL_BAD(255, 120, 110);

// --- Layout -------------------------------------------------------------------------------
constexpr float PX = 996.0f, PY = 118.0f, PW = 592.0f, PH = 522.0f;
constexpr float PAD = 16.0f;
constexpr float LABEL_W = 100.0f;
constexpr float BTN_X0 = PX + PAD + LABEL_W;
constexpr float BTN_AREA_W = PW - 2.0f * PAD - LABEL_W;
constexpr float ROW_Y0 = PY + 60.0f;     // centre of the first row
constexpr float ROW_STEP = 40.0f;
constexpr float BTN_H = 30.0f;
constexpr float CHART_X = PX + 56.0f, CHART_Y = PY + 306.0f, CHART_W = PW - 56.0f - 18.0f, CHART_H = 140.0f;

const float SPEEDS[] = { 0.0f, 1.0f, 4.0f, 16.0f };
const char* WEATHER_LABELS[] = { "СЛЪНЦЕ", "ВЯТЪР", "ДЪЖД", "БУРЯ", "СНЯГ", "ОБЛАЦИ" };
const WeatherType WEATHERS[] = { WeatherType::SUNNY, WeatherType::WINDY, WeatherType::RAINY,
                                 WeatherType::STORMY, WeatherType::SNOWY, WeatherType::CLOUDY };
const char* SEASON_LABELS[] = { "ПРОЛЕТ", "ЛЯТО", "ЕСЕН", "ЗИМА" };

float rowCenter(int row) { return ROW_Y0 + static_cast<float>(row) * ROW_STEP; }

std::string hourText(float h) { return Balance::formatHourMinute(h); }

void drawRect(sf::RenderWindow& window, sf::FloatRect r, sf::Color fill, float outline = 0.0f, sf::Color edge = sf::Color::Transparent) {
    sf::RectangleShape s(r.size);
    s.setPosition(r.position);
    s.setFillColor(fill);
    if (outline > 0.0f) {
        s.setOutlineThickness(outline);
        s.setOutlineColor(edge);
    }
    window.draw(s);
}

} // namespace

UI_sandboxPanel::UI_sandboxPanel() {
    auto addRow = [&](int row, float x0, float totalW, int count, Action a, const std::vector<int>& values,
                      const std::vector<std::string>& labels) {
        const float gap = 5.0f;
        float w = (totalW - gap * static_cast<float>(count - 1)) / static_cast<float>(count);
        for (int i = 0; i < count; ++i) {
            Button b;
            b.rect = sf::FloatRect({ x0 + static_cast<float>(i) * (w + gap), rowCenter(row) - BTN_H / 2.0f }, { w, BTN_H });
            b.action = a;
            b.value = values[static_cast<size_t>(i)];
            b.label = labels[static_cast<size_t>(i)];
            buttons.push_back(b);
        }
    };
    // Row 0: clock - the current time takes the first 64 px, then -1 h / +1 h and four preset hours
    {
        float start = BTN_X0 + 64.0f;
        float w = (BTN_AREA_W - 64.0f - 5.0f * 5.0f) / 6.0f;
        const int hours[] = { 6, 12, 18, 0 };
        const char* labels[] = { "06:00", "12:00", "18:00", "00:00" };
        for (int i = 0; i < 6; ++i) {
            Button b;
            b.rect = sf::FloatRect({ start + static_cast<float>(i) * (w + 5.0f), rowCenter(0) - BTN_H / 2.0f }, { w, BTN_H });
            if (i < 2) { b.action = HOUR_DELTA; b.value = (i == 0) ? -1 : +1; b.label = (i == 0) ? "-1 ч" : "+1 ч"; }
            else { b.action = HOUR_SET; b.value = hours[i - 2]; b.label = labels[i - 2]; }
            buttons.push_back(b);
        }
    }
    addRow(1, BTN_X0, BTN_AREA_W, 4, SPEED, { 0, 1, 2, 3 }, { "ПАУЗА", "1x", "4x", "16x" });
    addRow(2, BTN_X0, BTN_AREA_W, 7, WEATHER, { -1, 0, 1, 2, 3, 4, 5 },
           { "АВТО", WEATHER_LABELS[0], WEATHER_LABELS[1], WEATHER_LABELS[2], WEATHER_LABELS[3], WEATHER_LABELS[4], WEATHER_LABELS[5] });
    addRow(3, BTN_X0, BTN_AREA_W, 5, SEASON, { -1, 0, 1, 2, 3 },
           { "АВТО", SEASON_LABELS[0], SEASON_LABELS[1], SEASON_LABELS[2], SEASON_LABELS[3] });
    addRow(4, BTN_X0 + 96.0f, BTN_AREA_W - 96.0f, 4, DEMAND_DELTA, { -50, -10, 10, 50 }, { "-50", "-10", "+10", "+50" });
    addRow(5, PX + PAD, PW - 2.0f * PAD, 2, CLEAR, { 0, 0 }, { "ИЗЧИСТИ СГРАДИТЕ", "КУПИ ЦЯЛАТА ЗЕМЯ" });
    buttons.back().action = BUY_LAND;
}

sf::FloatRect UI_sandboxPanel::panelRect() {
    return { { PX, PY }, { PW, PH } };
}

bool UI_sandboxPanel::isActive(const Button& b, const GameEngine& engine) const {
    switch (b.action) {
        case SPEED:   return std::fabs(engine.getSandboxClockSpeed() - SPEEDS[b.value]) < 0.01f;
        case WEATHER:
            if (b.value < 0) return !engine.isSandboxWeatherLocked(1);
            return engine.isSandboxWeatherLocked(1) && engine.getPlayerWeather(1) == WEATHERS[b.value];
        case SEASON:  return engine.getSandboxSeasonOverride() == b.value;
        default:      return false;
    }
}

void UI_sandboxPanel::apply(Action action, int value, GameEngine& engine) {
    char buf[96];
    switch (action) {
        case HOUR_DELTA:
            engine.sandboxSetHour(engine.getHour24() + static_cast<float>(value));
            feedback = "Часът е " + hourText(engine.getHour24());
            break;
        case HOUR_SET:
            engine.sandboxSetHour(static_cast<float>(value));
            feedback = "Часът е " + hourText(engine.getHour24());
            break;
        case SPEED:
            engine.sandboxSetClockSpeed(SPEEDS[value]);
            feedback = (value == 0) ? "Часовникът е спрян" : ("Скорост на времето: " + std::string(value == 1 ? "1x" : (value == 2 ? "4x" : "16x")));
            break;
        case WEATHER:
            if (value < 0) {
                engine.sandboxUnlockWeather();
                feedback = "Времето отново се сменя всеки ден";
            } else {
                engine.sandboxSetWeather(1, WEATHERS[value]);
                engine.sandboxSetWeather(2, WEATHERS[value]);
                feedback = std::string("Време: ") + WEATHER_LABELS[value];
            }
            break;
        case SEASON:
            engine.sandboxSetSeason(value);
            feedback = (value < 0) ? std::string("Сезоните се сменят нормално") : (std::string("Сезон: ") + SEASON_LABELS[value]);
            break;
        case DEMAND_DELTA:
            engine.sandboxSetDemand(engine.getCityState().cityEnergyDemand + value);
            std::snprintf(buf, sizeof(buf), "Нужда на града: %d MW", engine.getCityState().cityEnergyDemand);
            feedback = buf;
            break;
        case CLEAR: {
            int n = engine.sandboxClearBuildings(1);
            feedback = "Премахнати сгради: " + std::to_string(n);
            break;
        }
        case BUY_LAND: {
            int bought = 0;
            std::string msg;
            std::vector<int> ids;
            for (const auto& plot : engine.getLandPlots()) {
                if (plot.playerOwner == 1 && !plot.isPurchased) ids.push_back(plot.id);
            }
            for (int id : ids) {
                if (engine.buyLandPlot(1, id, msg)) ++bought;
            }
            feedback = (bought > 0) ? "Купени парцели: " + std::to_string(bought) : std::string("Цялата земя вече е ваша");
            break;
        }
    }
    feedbackClock.restart();
}

bool UI_sandboxPanel::handleClick(sf::Vector2f p, GameEngine& engine) {
    if (!visible || !panelRect().contains(p)) return false;
    for (const auto& b : buttons) {
        if (b.rect.contains(p)) {
            apply(b.action, b.value, engine);
            break;
        }
    }
    return true; // clicks on the panel never reach the map below
}

bool UI_sandboxPanel::handleKey(sf::Keyboard::Key key, GameEngine& engine) {
    using K = sf::Keyboard::Key;
    switch (key) {
        case K::F3: { // cycle speed
            int cur = 1;
            for (int i = 0; i < 4; ++i) if (std::fabs(engine.getSandboxClockSpeed() - SPEEDS[i]) < 0.01f) cur = i;
            apply(SPEED, (cur + 1) % 4, engine);
            return true;
        }
        case K::F4: { // cycle weather: auto -> sunny -> ... -> cloudy -> auto
            int cur = -1;
            if (engine.isSandboxWeatherLocked(1)) {
                for (int i = 0; i < 6; ++i) if (engine.getPlayerWeather(1) == WEATHERS[i]) cur = i;
            }
            apply(WEATHER, (cur + 1 >= 6) ? -1 : cur + 1, engine);
            return true;
        }
        case K::F5: { // cycle season: auto -> spring -> ... -> winter -> auto
            int cur = engine.getSandboxSeasonOverride();
            apply(SEASON, (cur + 1 >= 4) ? -1 : cur + 1, engine);
            return true;
        }
        case K::F6: apply(DEMAND_DELTA, -10, engine); return true;
        case K::F7: apply(DEMAND_DELTA, +10, engine); return true;
        case K::F8: apply(HOUR_DELTA, +1, engine); return true;
        default: return false;
    }
}

void UI_sandboxPanel::draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded, const GameEngine& engine,
                           float animTime) {
    (void)animTime;
    if (!visible) return;
    const sf::FloatRect pr = panelRect();
    drawRect(window, pr, COL_PANEL, 2.0f, COL_EDGE);
    drawRect(window, { pr.position, { pr.size.x, 40.0f } }, COL_HEADER);

    int slot = 0;
    auto L = [&](const std::string& s, unsigned int size, sf::Color c, float x, float cy, float maxW,
                 OptionsTextCache::Align a = OptionsTextCache::LEFT) {
        if (fontLoaded) texts.draw(window, ++slot, font, s, size, c, x, cy, maxW, a, 9);
    };

    L("ПЯСЪЧНИК · КОНТРОЛЕН ПАНЕЛ", 17, COL_TITLE, PX + PAD, PY + 20.0f, 380.0f);
    L("[F2] скрий", 13, COL_MUTED, PX + PW - PAD, PY + 20.0f, 150.0f, OptionsTextCache::RIGHT);

    const char* rowLabels[] = { "ЧАС", "СКОРОСТ", "ВРЕМЕ", "СЕЗОН", "НУЖДА", "" };
    for (int r = 0; r < 5; ++r) L(rowLabels[r], 14, COL_MUTED, PX + PAD, rowCenter(r), LABEL_W - 8.0f);
    L(hourText(engine.getHour24()), 16, COL_TEXT, BTN_X0, rowCenter(0), 60.0f);
    L(std::to_string(engine.getCityState().cityEnergyDemand) + " MW", 16, COL_DEMAND, BTN_X0, rowCenter(4), 90.0f);

    for (const auto& b : buttons) {
        bool on = isActive(b, engine);
        drawRect(window, b.rect, on ? COL_BTN_ON : COL_BTN, on ? 2.0f : 1.0f, on ? COL_BTN_ON_EDGE : COL_BTN_EDGE);
        L(b.label, 13, COL_TEXT, b.rect.position.x + b.rect.size.x / 2.0f, b.rect.position.y + b.rect.size.y / 2.0f,
          b.rect.size.x - 6.0f, OptionsTextCache::CENTER);
    }

    // ---------------- 24 h projection chart ----------------
    const int samples = 49; // every 30 minutes, 00:00 .. 24:00
    float gen[samples];
    float maxGen = 0.0f, sum = 0.0f;
    for (int i = 0; i < samples; ++i) {
        gen[i] = engine.projectGenerationMW(1, static_cast<float>(i) * 0.5f);
        maxGen = std::max(maxGen, gen[i]);
        if (i < samples - 1) sum += gen[i];
    }
    const float avg = sum / static_cast<float>(samples - 1);
    const float demand = static_cast<float>(engine.getCityState().cityEnergyDemand);
    float yMax = std::max({ maxGen, demand, 50.0f }) * 1.15f;
    yMax = std::ceil(yMax / 50.0f) * 50.0f;

    L("ПРОГНОЗА ЗА 24 ч (ЗАПАД)", 14, COL_TITLE, PX + PAD, CHART_Y - 16.0f, 260.0f);
    L("производство", 12, COL_GEN, PX + PW - PAD - 110.0f, CHART_Y - 16.0f, 100.0f, OptionsTextCache::RIGHT);
    L("нужда", 12, COL_DEMAND, PX + PW - PAD, CHART_Y - 16.0f, 60.0f, OptionsTextCache::RIGHT);

    drawRect(window, { { CHART_X, CHART_Y }, { CHART_W, CHART_H } }, COL_CHART_BG, 1.0f, COL_GRID);
    auto xOf = [&](float hour) { return CHART_X + hour / 24.0f * CHART_W; };
    auto yOf = [&](float mw) { return CHART_Y + CHART_H - std::min(mw, yMax) / yMax * CHART_H; };

    // Daylight band of the current season
    float rise = engine.getSunriseHour(), set = engine.getSunsetHour();
    drawRect(window, { { xOf(rise), CHART_Y + 1.0f }, { xOf(set) - xOf(rise), CHART_H - 2.0f } }, COL_DAYLIGHT);
    for (int h = 6; h < 24; h += 6) drawRect(window, { { xOf(static_cast<float>(h)), CHART_Y }, { 1.0f, CHART_H } }, COL_GRID);

    // Filled output curve
    sf::VertexArray fill(sf::PrimitiveType::TriangleStrip, static_cast<size_t>(samples) * 2);
    sf::VertexArray line(sf::PrimitiveType::LineStrip, static_cast<size_t>(samples));
    sf::VertexArray line2(sf::PrimitiveType::LineStrip, static_cast<size_t>(samples));
    for (int i = 0; i < samples; ++i) {
        float x = xOf(static_cast<float>(i) * 0.5f);
        float y = yOf(gen[i]);
        fill[static_cast<size_t>(i) * 2].position = { x, CHART_Y + CHART_H };
        fill[static_cast<size_t>(i) * 2].color = COL_GEN_FILL;
        fill[static_cast<size_t>(i) * 2 + 1].position = { x, y };
        fill[static_cast<size_t>(i) * 2 + 1].color = COL_GEN_FILL;
        line[static_cast<size_t>(i)].position = { x, y };
        line[static_cast<size_t>(i)].color = COL_GEN;
        line2[static_cast<size_t>(i)].position = { x, y - 1.0f }; // 2 px thick
        line2[static_cast<size_t>(i)].color = COL_GEN;
    }
    window.draw(fill);
    window.draw(line);
    window.draw(line2);

    // Dashed demand line
    if (demand > 0.0f) {
        float dy = yOf(demand);
        sf::VertexArray dashes(sf::PrimitiveType::Lines);
        for (float x = CHART_X; x < CHART_X + CHART_W; x += 14.0f) {
            float x2 = std::min(x + 8.0f, CHART_X + CHART_W);
            sf::Vertex a; a.position = { x, dy }; a.color = COL_DEMAND;
            sf::Vertex b; b.position = { x2, dy }; b.color = COL_DEMAND;
            dashes.append(a); dashes.append(b);
            a.position.y = dy - 1.0f; b.position.y = dy - 1.0f;
            dashes.append(a); dashes.append(b);
        }
        window.draw(dashes);
    }

    // "Now" marker
    float hNow = engine.getHour24();
    drawRect(window, { { xOf(hNow) - 1.0f, CHART_Y }, { 2.0f, CHART_H } }, sf::Color(255, 255, 255, 150));
    sf::CircleShape dot(4.0f);
    dot.setOrigin({ 4.0f, 4.0f });
    dot.setPosition({ xOf(hNow), yOf(engine.projectGenerationMW(1, hNow)) });
    dot.setFillColor(sf::Color::White);
    window.draw(dot);

    // Axis labels
    L(std::to_string(static_cast<int>(yMax)), 11, COL_MUTED, CHART_X - 6.0f, CHART_Y + 6.0f, 46.0f, OptionsTextCache::RIGHT);
    L("MW", 11, COL_MUTED, CHART_X - 6.0f, CHART_Y + 22.0f, 46.0f, OptionsTextCache::RIGHT);
    L("0", 11, COL_MUTED, CHART_X - 6.0f, CHART_Y + CHART_H - 6.0f, 46.0f, OptionsTextCache::RIGHT);
    const char* xl[] = { "00", "06", "12", "18", "24" };
    for (int i = 0; i < 5; ++i) {
        L(xl[i], 11, COL_MUTED, xOf(static_cast<float>(i * 6)), CHART_Y + CHART_H + 10.0f, 30.0f, OptionsTextCache::CENTER);
    }

    // Verdict line: average raw output vs demand (lamps and batteries not included)
    char buf[160];
    int avgI = static_cast<int>(std::lround(avg));
    int dem = static_cast<int>(demand);
    if (dem <= 0) {
        std::snprintf(buf, sizeof(buf), "Средно за 24 ч: %d MW · градът не иска ток", avgI);
        L(buf, 13, COL_MUTED, PX + PAD, CHART_Y + CHART_H + 30.0f, PW - 2.0f * PAD);
    } else if (avgI >= dem) {
        std::snprintf(buf, sizeof(buf), "Средно за 24 ч: %d MW, нужда %d MW · денят ще бъде спечелен", avgI, dem);
        L(buf, 13, COL_GOOD, PX + PAD, CHART_Y + CHART_H + 30.0f, PW - 2.0f * PAD);
    } else {
        std::snprintf(buf, sizeof(buf), "Средно за 24 ч: %d MW, нужда %d MW · недостиг %d MW", avgI, dem, dem - avgI);
        L(buf, 13, COL_BAD, PX + PAD, CHART_Y + CHART_H + 30.0f, PW - 2.0f * PAD);
    }

    // Footer: last action or the keyboard shortcuts
    bool showFeedback = !feedback.empty() && feedbackClock.getElapsedTime().asSeconds() < 2.5f;
    L(showFeedback ? feedback : std::string("[F3] скорост  [F4] време  [F5] сезон  [F6]/[F7] нужда  [F8] +1 час"), 12,
      showFeedback ? COL_TITLE : COL_MUTED, PX + PW / 2.0f, PY + PH - 14.0f, PW - 2.0f * PAD, OptionsTextCache::CENTER);
}
