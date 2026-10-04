#include "../includes/UI_devOverlay.h"
#include "../includes/UI_infoCharts.h"
#include "../includes/UI_types.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

// =============================================================================
// team info: developer overlay ([F3])
// =============================================================================

constexpr float UI_devOverlay::MULTIPLIERS[5];

namespace {
const sf::Color COL_BG(6, 10, 18, 225);
const sf::Color COL_EDGE(120, 255, 120);
const sf::Color COL_HEAD(120, 255, 120);
const sf::Color COL_KEY(160, 185, 215);
const sf::Color COL_VAL(240, 246, 255);
const sf::Color COL_WARN(255, 200, 90);
const sf::FloatRect DEV_PANEL({ 622.0f, 100.0f }, { 356.0f, 286.0f }); // over the city, below the influence bar

std::string fmt1(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(v));
    return buf;
}
} // namespace

void UI_devOverlay::toggle() {
    visible = !visible;
    if (!visible) multIdx = 0; // never leave the game running fast after hiding the panel
}

bool UI_devOverlay::handleKey(sf::Keyboard::Key key) {
    if (!visible) return false;
    switch (key) {
        case sf::Keyboard::Key::F6: multIdx = std::max(0, multIdx - 1); return true;
        case sf::Keyboard::Key::F7: multIdx = std::min(4, multIdx + 1); return true;
        case sf::Keyboard::Key::F8: multIdx = 0; return true;
        default: return false;
    }
}

void UI_devOverlay::recordFrame(float rawDt) {
    frameTimes[frameIdx] = rawDt;
    frameIdx = (frameIdx + 1) % HISTORY;
    frameCount = std::min(frameCount + 1, HISTORY);
}

void UI_devOverlay::draw(sf::RenderTarget& target, const sf::Font& font, const GameEngine& engine, const Counts& c,
                         bool paused) const {
    if (!visible) return;
    using namespace infoCharts;

    float avg = 0.0f, worst = 0.0f;
    for (std::size_t i = 0; i < frameCount; ++i) {
        avg += frameTimes[i];
        worst = std::max(worst, frameTimes[i]);
    }
    avg = (frameCount > 0) ? avg / static_cast<float>(frameCount) : 0.0f;
    const float fps = (avg > 0.0f) ? 1.0f / avg : 0.0f;

    rect(target, DEV_PANEL, COL_BG, COL_EDGE, 1.5f);
    const float x = DEV_PANEL.position.x + 12.0f;
    const float vx = DEV_PANEL.position.x + DEV_PANEL.size.x - 12.0f;
    float y = DEV_PANEL.position.y + 8.0f;
    text(target, font, "РАЗРАБОТЧИК  [F3]", 13, { x, y }, COL_HEAD, true);
    textRight(target, font, paused ? "ПАУЗА" : "", 12, { vx, y + 1.0f }, COL_WARN, true);
    y += 24.0f;

    auto row = [&](const std::string& k, const std::string& v, sf::Color vc = COL_VAL) {
        text(target, font, k, 12, { x, y }, COL_KEY);
        textRight(target, font, v, 12, { vx, y }, vc, true);
        y += 17.0f;
    };
    row("FPS", fmt1(fps), fps < 50.0f ? COL_WARN : COL_VAL);
    row("Кадър: средно / най-бавен", fmt1(avg * 1000.0f) + " / " + fmt1(worst * 1000.0f) + " ms",
        worst > 0.034f ? COL_WARN : COL_VAL);
    row("Seed (EC_SEED)", std::to_string(engine.getSeed()));
    row("Ден / час", std::to_string(engine.getCurrentDay()) + " / " + Balance::formatHourMinute(engine.getHour24()));
    const float mult = timeMultiplier();
    row("Скорост: игра x разработчик", "x" + fmt1(engine.getTimeScale()) + " x" + fmt1(mult), mult > 1.0f ? COL_WARN : COL_VAL);
    y += 4.0f;
    row("Сгради P1 / P2", std::to_string(c.buildingsP1) + " / " + std::to_string(c.buildingsP2));
    row("Частици: време / добив", std::to_string(c.weatherParticles) + " / " + std::to_string(c.miningParticles));
    row("Надписи / мълнии / известия", std::to_string(c.notices) + " / " + std::to_string(c.lightnings) + " / " +
                                           std::to_string(c.toasts));
    row("Проби в телеметрията", std::to_string(c.samples));
    row("Записи в дневника", std::to_string(c.logEntries));
    y += 6.0f;
    text(target, font, "[F6] по-бавно   [F7] по-бързо   [F8] x1", 11, { x, y }, COL_KEY);
}
