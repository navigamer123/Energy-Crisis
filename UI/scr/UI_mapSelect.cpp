// =============================================================================
// Team b-power (F-39): map layout selector with a live mini-map preview
// =============================================================================
#include "../includes/UI_mapSelect.h"
#include "../includes/UI_types.h"
#include <chrono>
#include <cstdlib>
#include <string>

namespace {

// Colours (the integrator maps these to UI_theme tokens)
const sf::Color MS_PANEL(16, 22, 34, 235);
const sf::Color MS_BORDER(70, 95, 125);
const sf::Color MS_TITLE(255, 215, 0);
const sf::Color MS_TEXT(220, 235, 250);
const sf::Color MS_MUTED(150, 175, 205);
const sf::Color MS_ARROW(0, 229, 255);
const sf::Color MS_ARROW_HOVER(160, 250, 255);
const sf::Color MS_P1(0, 229, 255);
const sf::Color MS_P2(255, 120, 200);
const sf::Color MS_CITY(34, 44, 66);
const sf::Color MS_RIVER_LINE(70, 150, 230);

constexpr float MS_WIDTH = 660.0f;
// World region shown in the preview (plots + city) and its scale
constexpr float MS_WORLD_X0 = 240.0f;
constexpr float MS_WORLD_Y0 = 60.0f;
constexpr float MS_WORLD_W = 1120.0f;
constexpr float MS_WORLD_H = 520.0f;
constexpr float MS_SCALE = 0.30f;

sf::Color terrainColor(int terrain) {
    switch (static_cast<TerrainType>(terrain)) {
        case TerrainType::RIVER:  return sf::Color(45, 120, 205);
        case TerrainType::VENT:   return sf::Color(230, 120, 40);
        case TerrainType::HILL:   return sf::Color(140, 110, 75);
        case TerrainType::MEADOW: return sf::Color(120, 200, 90);
        case TerrainType::PLAIN:
        default:                  return sf::Color(70, 95, 70);
    }
}

void drawTriangle(sf::RenderWindow& w, sf::FloatRect box, bool pointRight, sf::Color c) {
    sf::ConvexShape tri(3);
    float cx = box.position.x + box.size.x * 0.5f, cy = box.position.y + box.size.y * 0.5f;
    float hw = box.size.x * 0.22f, hh = box.size.y * 0.30f;
    if (pointRight) {
        tri.setPoint(0, { cx - hw, cy - hh });
        tri.setPoint(1, { cx + hw, cy });
        tri.setPoint(2, { cx - hw, cy + hh });
    } else {
        tri.setPoint(0, { cx + hw, cy - hh });
        tri.setPoint(1, { cx - hw, cy });
        tri.setPoint(2, { cx + hw, cy + hh });
    }
    tri.setFillColor(c);
    w.draw(tri);
}

void drawLabel(sf::RenderWindow& w, const sf::Font& f, const std::string& s, unsigned size, sf::Color c,
               sf::Vector2f pos, bool centered) {
    sf::Text t(f, toUtf8(s), size);
    t.setFillColor(c);
    sf::FloatRect b = t.getLocalBounds();
    t.setPosition({ centered ? pos.x - b.size.x * 0.5f - b.position.x : pos.x, pos.y });
    w.draw(t);
}

} // namespace

UI_mapSelect::UI_mapSelect() {
    reroll();
}

void UI_mapSelect::step(int delta) {
    int n = static_cast<int>(MapPreset::COUNT);
    presetIdx = ((presetIdx + delta) % n + n) % n;
    rebuildPreview();
}

void UI_mapSelect::reroll() {
    unsigned long long t = static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count());
    seed = static_cast<unsigned>((t ^ (t >> 29)) % 900000ULL) + 100000u; // six digits, easy to read out
    rebuildPreview();
}

void UI_mapSelect::rebuildPreview() {
    preview = buildMapLayout(getPreset(), seed);
}

unsigned UI_mapSelect::getSeed() const {
    return std::getenv("EC_SEED") ? 0u : seed;
}

bool UI_mapSelect::handleKey(sf::Keyboard::Key code) {
    if (code == sf::Keyboard::Key::Left || code == sf::Keyboard::Key::A) { step(-1); return true; }
    if (code == sf::Keyboard::Key::Right || code == sf::Keyboard::Key::D) { step(+1); return true; }
    if (code == sf::Keyboard::Key::R) { reroll(); return true; }
    return false;
}

bool UI_mapSelect::handleClick(sf::Vector2f pos) {
    if (prevBtn.contains(pos)) { step(-1); return true; }
    if (nextBtn.contains(pos)) { step(+1); return true; }
    if (rerollBtn.contains(pos)) { reroll(); return true; }
    return false;
}

void UI_mapSelect::draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded, float top, sf::Vector2f mousePos) {
    const float left = (VIRTUAL_WIDTH - MS_WIDTH) * 0.5f;
    const float previewW = MS_WORLD_W * MS_SCALE;
    const float previewH = MS_WORLD_H * MS_SCALE;
    const float height = 44.0f + previewH + 52.0f;

    sf::RectangleShape panel({ MS_WIDTH, height });
    panel.setPosition({ left, top });
    panel.setFillColor(MS_PANEL);
    panel.setOutlineThickness(1.5f);
    panel.setOutlineColor(MS_BORDER);
    window.draw(panel);

    // Row 1: [<]  КАРТА: NAME  [>]
    prevBtn = sf::FloatRect({ left + 10.0f, top + 7.0f }, { 34.0f, 30.0f });
    nextBtn = sf::FloatRect({ left + MS_WIDTH - 44.0f, top + 7.0f }, { 34.0f, 30.0f });
    for (int i = 0; i < 2; ++i) {
        const sf::FloatRect& b = (i == 0) ? prevBtn : nextBtn;
        bool hover = b.contains(mousePos);
        sf::RectangleShape box(b.size);
        box.setPosition(b.position);
        box.setFillColor(hover ? sf::Color(30, 60, 80) : sf::Color(22, 30, 44));
        box.setOutlineThickness(1.0f);
        box.setOutlineColor(hover ? MS_ARROW_HOVER : MS_BORDER);
        window.draw(box);
        drawTriangle(window, b, i == 1, hover ? MS_ARROW_HOVER : MS_ARROW);
    }
    if (fontLoaded) {
        std::string title = std::string("КАРТА: ") + getMapPresetNameBg(getPreset());
        drawLabel(window, font, title, 17, MS_TITLE, { left + MS_WIDTH * 0.5f, top + 10.0f }, true);
    }

    // Row 2: mini-map preview (left) + legend (right)
    const float px = left + 14.0f;
    const float py = top + 44.0f;
    auto toPreview = [&](float wx, float wy) {
        return sf::Vector2f(px + (wx - MS_WORLD_X0) * MS_SCALE, py + (wy - MS_WORLD_Y0) * MS_SCALE);
    };
    sf::RectangleShape bg({ previewW, previewH });
    bg.setPosition({ px, py });
    bg.setFillColor(sf::Color(40, 70, 40));
    bg.setOutlineThickness(1.0f);
    bg.setOutlineColor(MS_BORDER);
    window.draw(bg);

    sf::RectangleShape river({ 3.0f, previewH });
    river.setPosition({ toPreview(800.0f, 0.0f).x - 1.5f, py });
    river.setFillColor(MS_RIVER_LINE);
    window.draw(river);
    sf::RectangleShape cityBox({ 380.0f * MS_SCALE, 330.0f * MS_SCALE });
    cityBox.setPosition(toPreview(610.0f, 65.0f));
    cityBox.setFillColor(MS_CITY);
    cityBox.setOutlineThickness(1.0f);
    cityBox.setOutlineColor(sf::Color(255, 215, 0, 140));
    window.draw(cityBox);

    for (int player = 1; player <= 2; ++player) {
        for (int r = 0; r < preview.plotRows; ++r) {
            for (int sc = 0; sc < preview.plotCols; ++sc) {
                int wc = preview.westCol(player, sc);
                sf::FloatRect rect = preview.plotRect(player, sc, r);
                sf::Vector2f p0 = toPreview(rect.position.x, rect.position.y);
                sf::Vector2f sz(rect.size.x * MS_SCALE, rect.size.y * MS_SCALE);
                if (!preview.hasPlot(wc, r)) {
                    sf::ConvexShape peak(3); // impassable peak
                    peak.setPoint(0, { p0.x + sz.x * 0.5f, p0.y + 3.0f });
                    peak.setPoint(1, { p0.x + sz.x - 3.0f, p0.y + sz.y - 3.0f });
                    peak.setPoint(2, { p0.x + 3.0f, p0.y + sz.y - 3.0f });
                    peak.setFillColor(sf::Color(110, 110, 120));
                    window.draw(peak);
                    continue;
                }
                sf::RectangleShape cell(sz);
                cell.setPosition(p0);
                cell.setFillColor(terrainColor(preview.cellValue(wc, r)));
                bool isStart = (wc == preview.startCol && r == preview.startRow);
                cell.setOutlineThickness(isStart ? 2.0f : 1.0f);
                cell.setOutlineColor(isStart ? (player == 1 ? MS_P1 : MS_P2) : sf::Color(20, 28, 20));
                window.draw(cell);
            }
        }
    }

    if (fontLoaded) {
        // Legend
        float lx = px + previewW + 18.0f;
        float ly = py + 2.0f;
        const TerrainType kinds[5] = { TerrainType::PLAIN, TerrainType::RIVER, TerrainType::VENT, TerrainType::HILL, TerrainType::MEADOW };
        for (int i = 0; i < 5; ++i) {
            sf::RectangleShape sw({ 14.0f, 14.0f });
            sw.setPosition({ lx, ly + i * 24.0f + 2.0f });
            sw.setFillColor(terrainColor(static_cast<int>(kinds[i])));
            window.draw(sw);
            std::string line = getTerrainNameBg(kinds[i]);
            std::string eff = getTerrainEffectBg(kinds[i]);
            if (!eff.empty()) line += " - " + eff;
            drawLabel(window, font, line, 12, MS_TEXT, { lx + 22.0f, ly + i * 24.0f }, false);
        }
        int plots = preview.plotCount();
        drawLabel(window, font, std::to_string(plots) + " парцела на играч, огледални", 12, MS_MUTED,
                  { lx, ly + 5 * 24.0f + 2.0f }, false);

        // Row 3: description + seed controls
        drawLabel(window, font, getMapPresetDescBg(getPreset()), 13, MS_TEXT,
                  { left + MS_WIDTH * 0.5f, py + previewH + 8.0f }, true);
        std::string hint = (getSeed() != 0u)
            ? "Сийд " + std::to_string(seed) + "   [R]: нов терен   [A/D] или [<]/[>]: смяна на картата"
            : "Сийд от EC_SEED   [A/D] или [<]/[>]: смяна на картата";
        sf::Text probe(font, toUtf8(hint), 12);
        float w = probe.getLocalBounds().size.x;
        rerollBtn = sf::FloatRect({ left + (MS_WIDTH - w) * 0.5f - 6.0f, py + previewH + 28.0f }, { w + 12.0f, 18.0f });
        bool hover = rerollBtn.contains(mousePos);
        drawLabel(window, font, hint, 12, hover ? MS_TEXT : MS_MUTED, { left + MS_WIDTH * 0.5f, py + previewH + 29.0f }, true);
    }
}
