// =============================================================================
// Match Setup screen [team b-options] - F-03 presets/rules, F-24 mutators, F-35 charters
// =============================================================================
#include "../includes/UI_matchSetup.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

// --- Colours (integrator: map these to UI_theme.h tokens) ---------------------------------
const sf::Color COL_PANEL(18, 25, 38, 245);
const sf::Color COL_PANEL_EDGE(60, 85, 120);
const sf::Color COL_TITLE(0, 229, 255);
const sf::Color COL_TEXT(232, 240, 252);
const sf::Color COL_MUTED(160, 180, 205);
const sf::Color COL_FOCUS_BG(40, 56, 84, 235);
const sf::Color COL_FOCUS_EDGE(255, 215, 0);
const sf::Color COL_VALUE(255, 214, 96);
const sf::Color COL_GOOD(120, 238, 150);
const sf::Color COL_BAD(255, 160, 115);
const sf::Color COL_CHIP(30, 42, 62);
const sf::Color COL_CHIP_ON(32, 118, 74);
const sf::Color COL_ARROW(150, 190, 235);
const sf::Color COL_P1(0, 229, 255);
const sf::Color COL_P2(255, 130, 205);
const sf::Color COL_START(32, 125, 72);
const sf::Color COL_START_HOVER(45, 160, 92);
const sf::Color COL_BACK(52, 40, 58);
const sf::Color COL_BACK_HOVER(92, 60, 82);
const sf::Color COL_REFUSED(255, 90, 90);

// --- Layout (1600x900 virtual canvas) -----------------------------------------------------
constexpr float LEFT_X = 110.0f;
constexpr float RIGHT_X = 830.0f;
constexpr float PANEL_W = 660.0f;
constexpr float PANEL_Y = 172.0f;
constexpr float PANEL_H = 572.0f;
constexpr float INNER_PAD = 20.0f;
constexpr float VALUE_ROW_Y = 292.0f;
constexpr float VALUE_ROW_STEP = 42.0f;
constexpr float VALUE_ROW_H = 36.0f;
constexpr float MUT_ROW_Y = 212.0f;
constexpr float MUT_ROW_STEP = 34.0f;
constexpr float MUT_ROW_H = 30.0f;
constexpr float CHARTER_Y[2] = { 562.0f, 644.0f };
constexpr float CHARTER_H = 34.0f;
constexpr float BUTTON_Y = 764.0f;
constexpr float BUTTON_W = 260.0f;
constexpr float BUTTON_H = 50.0f;

// Steps of the custom editor
const float DAY_SECONDS_STEPS[] = { 30.0f, 45.0f, 60.0f, 75.0f, 90.0f, 120.0f, 150.0f, 180.0f, 240.0f };
const int FINAL_DAY_STEPS[] = { 5, 8, 10, 12, 15, 20, 25, 30, 35, 40, 50, 60, 0 }; // 0 = endless (last)

template <typename T, size_t N>
int nearestIndex(const T (&steps)[N], T value) {
    int best = 0;
    for (size_t i = 0; i < N; ++i) {
        if (std::abs(static_cast<double>(steps[i]) - static_cast<double>(value)) <
            std::abs(static_cast<double>(steps[best]) - static_cast<double>(value))) {
            best = static_cast<int>(i);
        }
    }
    return best;
}

std::string fmtFloat(float v, int decimals) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), decimals == 0 ? "%.0f" : (decimals == 1 ? "%.1f" : "%.2f"), static_cast<double>(v));
    return buf;
}

// Shrinks a text until it fits maxWidth (never below minSize)
void drawPanel(sf::RenderWindow& window, float x, float y, float w, float h) {
    sf::RectangleShape box({ w, h });
    box.setPosition({ x, y });
    box.setFillColor(COL_PANEL);
    box.setOutlineThickness(2.0f);
    box.setOutlineColor(COL_PANEL_EDGE);
    window.draw(box);
}

void drawTriangle(sf::RenderWindow& window, sf::FloatRect r, int dir, sf::Color c) {
    sf::ConvexShape tri(3);
    float cx = r.position.x + r.size.x / 2.0f;
    float cy = r.position.y + r.size.y / 2.0f;
    float s = std::min(r.size.x, r.size.y) * 0.32f;
    if (dir < 0) {
        tri.setPoint(0, { cx - s, cy });
        tri.setPoint(1, { cx + s * 0.8f, cy - s });
        tri.setPoint(2, { cx + s * 0.8f, cy + s });
    } else {
        tri.setPoint(0, { cx + s, cy });
        tri.setPoint(1, { cx - s * 0.8f, cy - s });
        tri.setPoint(2, { cx - s * 0.8f, cy + s });
    }
    tri.setFillColor(c);
    window.draw(tri);
}

} // namespace

UI_matchSetup::UI_matchSetup() {
    rules = MatchRules::fromPreset(MatchPreset::STANDARD);
}

void UI_matchSetup::open(bool botMatch) {
    vsBot = botMatch;
    focus = ITEM_START;
    requestStart = false;
    requestBack = false;
    lastMouse = { -999.0f, -999.0f };
    rules.sandbox = false;
    rules.validate();
}

// -----------------------------------------------------------------------------
// Layout
// -----------------------------------------------------------------------------
sf::FloatRect UI_matchSetup::itemRect(int item) const {
    const float rowW = PANEL_W - 2.0f * INNER_PAD;
    if (item == ITEM_PRESET) return { { LEFT_X + INNER_PAD, 212.0f }, { rowW, 40.0f } };
    if (item >= ITEM_DAY_SECONDS && item <= ITEM_MINING) {
        float y = VALUE_ROW_Y + static_cast<float>(item - ITEM_DAY_SECONDS) * VALUE_ROW_STEP;
        return { { LEFT_X + INNER_PAD, y }, { rowW, VALUE_ROW_H } };
    }
    if (item >= ITEM_MUTATOR_FIRST && item < ITEM_MUTATOR_RANDOM) {
        float y = MUT_ROW_Y + static_cast<float>(item - ITEM_MUTATOR_FIRST) * MUT_ROW_STEP;
        return { { RIGHT_X + INNER_PAD, y }, { rowW, MUT_ROW_H } };
    }
    if (item == ITEM_MUTATOR_RANDOM) return { { RIGHT_X + (PANEL_W - 300.0f) / 2.0f, 488.0f }, { 300.0f, 30.0f } };
    if (item == ITEM_CHARTER_P1) return { { RIGHT_X + INNER_PAD, CHARTER_Y[0] }, { rowW, CHARTER_H } };
    if (item == ITEM_CHARTER_P2) return { { RIGHT_X + INNER_PAD, CHARTER_Y[1] }, { rowW, CHARTER_H } };
    if (item == ITEM_START) return { { VIRTUAL_WIDTH / 2.0f - BUTTON_W - 12.0f, BUTTON_Y }, { BUTTON_W, BUTTON_H } };
    if (item == ITEM_BACK) return { { VIRTUAL_WIDTH / 2.0f + 12.0f, BUTTON_Y }, { BUTTON_W, BUTTON_H } };
    return { { 0.0f, 0.0f }, { 0.0f, 0.0f } };
}

sf::FloatRect UI_matchSetup::presetChipRect(int preset) const {
    sf::FloatRect row = itemRect(ITEM_PRESET);
    const float gap = 8.0f;
    const int n = static_cast<int>(MatchPreset::COUNT);
    float w = (row.size.x - gap * static_cast<float>(n - 1)) / static_cast<float>(n);
    return { { row.position.x + static_cast<float>(preset) * (w + gap), row.position.y }, { w, row.size.y } };
}

sf::FloatRect UI_matchSetup::arrowRect(int item, int dir) const {
    sf::FloatRect r = itemRect(item);
    const float aw = 30.0f;
    float boxLeft;
    if (item == ITEM_CHARTER_P1 || item == ITEM_CHARTER_P2) {
        boxLeft = r.position.x + 196.0f;
    } else {
        boxLeft = r.position.x + r.size.x - 240.0f;
    }
    float boxRight = r.position.x + r.size.x - 6.0f;
    float x = (dir < 0) ? boxLeft : boxRight - aw;
    return { { x, r.position.y + 3.0f }, { aw, r.size.y - 6.0f } };
}

int UI_matchSetup::itemAt(sf::Vector2f p) const {
    for (int i = 0; i < ITEM_COUNT; ++i) {
        if (itemRect(i).contains(p)) return i;
    }
    return -1;
}

// -----------------------------------------------------------------------------
// Values
// -----------------------------------------------------------------------------
std::string UI_matchSetup::labelText(int item) const {
    switch (item) {
        case ITEM_DAY_SECONDS:  return "Дължина на деня";
        case ITEM_GRACE:        return "Гратисни дни (0 MW)";
        case ITEM_FINAL_DAY:    return "Последен ден";
        case ITEM_VICTORY:      return "Победа при дял от града";
        case ITEM_START_DEMAND: return "Начална нужда на града";
        case ITEM_GROWTH:       return "Ръст на нуждата";
        case ITEM_MINING:       return "Добив на ресурси";
        default:                return "";
    }
}

std::string UI_matchSetup::valueText(int item) const {
    switch (item) {
        case ITEM_DAY_SECONDS:  return fmtFloat(rules.daySeconds, 0) + " с";
        case ITEM_GRACE:        return std::to_string(rules.graceDays) + (rules.graceDays == 1 ? " ден" : " дни");
        case ITEM_FINAL_DAY:    return rules.finalDay == 0 ? "БЕЗ КРАЙ" : ("ден " + std::to_string(rules.finalDay));
        case ITEM_VICTORY:      return std::to_string(static_cast<int>(std::lround(rules.victoryShare * 100.0f))) + "%";
        case ITEM_START_DEMAND: return std::to_string(rules.startDemandMW) + " MW";
        case ITEM_GROWTH:       return "+" + std::to_string(rules.demandGrowthMW) + " MW/ден";
        case ITEM_MINING:       return "x" + fmtFloat(rules.miningMult, 2);
        case ITEM_CHARTER_P1:   return MatchInfo::charterName(rules.charter[0]);
        case ITEM_CHARTER_P2:   return MatchInfo::charterName(rules.charter[1]);
        default:                return "";
    }
}

void UI_matchSetup::adjust(int item, int dir) {
    if (dir == 0) return;
    using namespace MatchLimits;
    const int presetCount = static_cast<int>(MatchPreset::COUNT);
    switch (item) {
        case ITEM_PRESET: {
            int p = (static_cast<int>(rules.preset) + dir + presetCount) % presetCount;
            rules.applyPreset(static_cast<MatchPreset>(p));
            return;
        }
        case ITEM_DAY_SECONDS: {
            const int n = static_cast<int>(sizeof(DAY_SECONDS_STEPS) / sizeof(DAY_SECONDS_STEPS[0]));
            int i = std::max(0, std::min(n - 1, nearestIndex(DAY_SECONDS_STEPS, rules.daySeconds) + dir));
            rules.daySeconds = DAY_SECONDS_STEPS[i];
            break;
        }
        case ITEM_GRACE:
            rules.graceDays = std::max(GRACE_MIN, std::min(GRACE_MAX, rules.graceDays + dir));
            break;
        case ITEM_FINAL_DAY: {
            const int n = static_cast<int>(sizeof(FINAL_DAY_STEPS) / sizeof(FINAL_DAY_STEPS[0]));
            int cur = 0;
            for (int i = 0; i < n; ++i) if (FINAL_DAY_STEPS[i] == rules.finalDay) cur = i;
            if (rules.finalDay != 0 && FINAL_DAY_STEPS[cur] != rules.finalDay) cur = nearestIndex(FINAL_DAY_STEPS, rules.finalDay);
            int i = std::max(0, std::min(n - 1, cur + dir));
            // Never offer a final day inside the grace period
            while (FINAL_DAY_STEPS[i] != 0 && FINAL_DAY_STEPS[i] <= rules.graceDays && i + 1 < n) ++i;
            rules.finalDay = FINAL_DAY_STEPS[i];
            break;
        }
        case ITEM_VICTORY: {
            int pct = static_cast<int>(std::lround(rules.victoryShare * 100.0f)) + dir * 5;
            pct = std::max(static_cast<int>(VICTORY_MIN * 100.0f + 0.5f), std::min(static_cast<int>(VICTORY_MAX * 100.0f + 0.5f), pct));
            rules.victoryShare = static_cast<float>(pct) / 100.0f;
            break;
        }
        case ITEM_START_DEMAND:
            rules.startDemandMW = std::max(DEMAND_START_MIN, std::min(DEMAND_START_MAX, rules.startDemandMW + dir * 10));
            break;
        case ITEM_GROWTH:
            rules.demandGrowthMW = std::max(DEMAND_GROWTH_MIN, std::min(DEMAND_GROWTH_MAX, rules.demandGrowthMW + dir * 5));
            break;
        case ITEM_MINING:
            rules.miningMult = std::max(MINING_MIN, std::min(MINING_MAX, rules.miningMult + dir * 0.25f));
            break;
        case ITEM_CHARTER_P1:
        case ITEM_CHARTER_P2: {
            int idx = (item == ITEM_CHARTER_P1) ? 0 : 1;
            const int n = static_cast<int>(CharterType::COUNT);
            rules.charter[idx] = static_cast<CharterType>((static_cast<int>(rules.charter[idx]) + dir + n) % n);
            return; // charters are not part of the pacing preset
        }
        default:
            return;
    }
    rules.preset = MatchPreset::CUSTOM; // any hand-edited pacing value makes the preset "СВОЯ"
    rules.validate();
}

void UI_matchSetup::activate(int item) {
    if (item >= ITEM_MUTATOR_FIRST && item < ITEM_MUTATOR_RANDOM) {
        std::uint32_t flag = MatchInfo::mutatorFlag(item - ITEM_MUTATOR_FIRST);
        if (!rules.toggleMutator(flag)) {
            refusedFlash = 1.0f; // already 3 active
            flashClock.restart();
        }
    } else if (item == ITEM_MUTATOR_RANDOM) {
        randomizeMutators();
    } else if (item == ITEM_START) {
        rules.validate();
        requestStart = true;
    } else if (item == ITEM_BACK) {
        requestBack = true;
    } else if (item == ITEM_PRESET || item == ITEM_CHARTER_P1 || item == ITEM_CHARTER_P2 ||
               (item >= ITEM_DAY_SECONDS && item <= ITEM_MINING)) {
        adjust(item, +1);
    }
}

void UI_matchSetup::randomizeMutators() {
    rules.mutators = 0u;
    int want = 2 + std::rand() % 2; // 2 or 3 mutators
    int guard = 0;
    while (rules.activeMutatorCount() < want && guard++ < 64) {
        rules.mutators |= MatchInfo::mutatorFlag(std::rand() % MUTATOR_COUNT);
    }
}

// -----------------------------------------------------------------------------
// Input
// -----------------------------------------------------------------------------
void UI_matchSetup::handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        using K = sf::Keyboard::Key;
        switch (key->code) {
            case K::Up: case K::W:
                focus = (focus + ITEM_COUNT - 1) % ITEM_COUNT;
                break;
            case K::Down: case K::S: case K::Tab:
                focus = (focus + 1) % ITEM_COUNT;
                break;
            case K::Left: case K::A:
                if (focus == ITEM_BACK) focus = ITEM_START;
                else adjust(focus, -1);
                break;
            case K::Right: case K::D:
                if (focus == ITEM_START) focus = ITEM_BACK;
                else adjust(focus, +1);
                break;
            case K::Enter: case K::Space:
                activate(focus);
                break;
            case K::Escape: case K::Backspace:
                requestBack = true;
                break;
            default:
                break;
        }
        return;
    }

    if (const auto* moved = event.getIf<sf::Event::MouseMoved>()) {
        sf::Vector2f p = window.mapPixelToCoords(moved->position);
        if (std::abs(p.x - lastMouse.x) > 2.0f || std::abs(p.y - lastMouse.y) > 2.0f) {
            lastMouse = p;
            int hit = itemAt(p);
            if (hit >= 0) focus = hit;
        }
        return;
    }

    if (const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>()) {
        sf::Vector2f p = window.mapPixelToCoords(wheel->position);
        int hit = itemAt(p);
        if (hit >= 0 && hit != ITEM_START && hit != ITEM_BACK && !(hit >= ITEM_MUTATOR_FIRST && hit <= ITEM_MUTATOR_RANDOM)) {
            focus = hit;
            adjust(hit, wheel->delta > 0.0f ? +1 : -1);
        }
        return;
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button != sf::Mouse::Button::Left) {
            if (mb->button == sf::Mouse::Button::Right) requestBack = true;
            return;
        }
        sf::Vector2f p = window.mapPixelToCoords(mb->position);
        // Preset chips
        for (int i = 0; i < static_cast<int>(MatchPreset::COUNT); ++i) {
            if (presetChipRect(i).contains(p)) {
                focus = ITEM_PRESET;
                rules.applyPreset(static_cast<MatchPreset>(i));
                return;
            }
        }
        // Arrows of value / charter rows
        for (int item = ITEM_DAY_SECONDS; item <= ITEM_CHARTER_P2; ++item) {
            bool hasArrows = (item <= ITEM_MINING) || item == ITEM_CHARTER_P1 || item == ITEM_CHARTER_P2;
            if (!hasArrows) continue;
            if (arrowRect(item, -1).contains(p)) { focus = item; adjust(item, -1); return; }
            if (arrowRect(item, +1).contains(p)) { focus = item; adjust(item, +1); return; }
        }
        int hit = itemAt(p);
        if (hit < 0) return;
        focus = hit;
        if ((hit >= ITEM_MUTATOR_FIRST && hit <= ITEM_MUTATOR_RANDOM) || hit == ITEM_START || hit == ITEM_BACK) {
            activate(hit);
        }
    }
}

// -----------------------------------------------------------------------------
// Drawing
// -----------------------------------------------------------------------------
void UI_matchSetup::draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded) {
    // Cached labels: numbered in drawing order, rebuilt only when their text changes
    int slot = 0;
    auto L = [&](const std::string& s, unsigned int size, sf::Color c, float x, float cy, float maxW = 100000.0f) {
        texts.draw(window, ++slot, font, s, size, c, x, cy, maxW, OptionsTextCache::LEFT, 10);
    };
    auto C = [&](const std::string& s, unsigned int size, sf::Color c, sf::FloatRect box) {
        texts.draw(window, ++slot, font, s, size, c, box.position.x + box.size.x / 2.0f, box.position.y + box.size.y / 2.0f,
                   box.size.x - 8.0f, OptionsTextCache::CENTER, 10);
    };
    const float rowW = PANEL_W - 2.0f * INNER_PAD;
    drawPanel(window, LEFT_X, PANEL_Y, PANEL_W, PANEL_H);
    drawPanel(window, RIGHT_X, PANEL_Y, PANEL_W, PANEL_H);

    auto drawFocus = [&](int item) {
        if (focus != item) return;
        sf::FloatRect r = itemRect(item);
        sf::RectangleShape hl(r.size);
        hl.setPosition(r.position);
        hl.setFillColor(COL_FOCUS_BG);
        hl.setOutlineThickness(1.5f);
        hl.setOutlineColor(COL_FOCUS_EDGE);
        window.draw(hl);
    };

    // ---------------- Left panel: preset + rule values ----------------
    if (fontLoaded) {
        L("ПРАВИЛА НА МАЧА", 20, COL_TITLE, LEFT_X + INNER_PAD, PANEL_Y + 20.0f);
    }
    for (int i = 0; i < static_cast<int>(MatchPreset::COUNT); ++i) {
        sf::FloatRect chip = presetChipRect(i);
        bool selected = (static_cast<int>(rules.preset) == i);
        sf::RectangleShape c(chip.size);
        c.setPosition(chip.position);
        c.setFillColor(selected ? COL_CHIP_ON : COL_CHIP);
        c.setOutlineThickness(selected && focus == ITEM_PRESET ? 2.5f : 1.5f);
        c.setOutlineColor(selected ? (focus == ITEM_PRESET ? COL_FOCUS_EDGE : COL_GOOD) : COL_PANEL_EDGE);
        window.draw(c);
        if (fontLoaded) {
            C(MatchInfo::presetName(static_cast<MatchPreset>(i)), 15,
                             selected ? COL_TEXT : COL_MUTED, chip);
        }
    }
    if (fontLoaded) {
        L(MatchInfo::presetDescription(rules.preset), 13, COL_MUTED,
                     LEFT_X + INNER_PAD, 270.0f, rowW);
    }

    for (int item = ITEM_DAY_SECONDS; item <= ITEM_MINING; ++item) {
        drawFocus(item);
        sf::FloatRect r = itemRect(item);
        float cy = r.position.y + r.size.y / 2.0f;
        drawTriangle(window, arrowRect(item, -1), -1, focus == item ? COL_FOCUS_EDGE : COL_ARROW);
        drawTriangle(window, arrowRect(item, +1), +1, focus == item ? COL_FOCUS_EDGE : COL_ARROW);
        if (fontLoaded) {
            L(labelText(item), 15, focus == item ? COL_TEXT : COL_MUTED, r.position.x + 12.0f, cy,
                         arrowRect(item, -1).position.x - r.position.x - 24.0f);
            sf::FloatRect left = arrowRect(item, -1), right = arrowRect(item, +1);
            sf::FloatRect valueBox({ left.position.x + left.size.x, r.position.y },
                                   { right.position.x - (left.position.x + left.size.x), r.size.y });
            C(valueText(item), 16, COL_VALUE, valueBox);
        }
    }

    if (fontLoaded) {
        std::string length;
        if (rules.finalDay == 0) {
            length = "Продължителност: без ограничение (до превземане на града)";
        } else {
            int minutes = std::max(1, static_cast<int>(std::lround(rules.finalDay * rules.daySeconds / 60.0f)));
            length = "Продължителност: до " + std::to_string(minutes) + " мин (" + std::to_string(rules.finalDay) +
                     " дни x " + fmtFloat(rules.daySeconds, 0) + " с)";
        }
        L(length, 14, COL_TEXT, LEFT_X + INNER_PAD, 604.0f, rowW);
        L("Ускорението при добив съкращава реалното време.", 12, COL_MUTED,
                     LEFT_X + INNER_PAD, 626.0f, rowW);
        std::string mode = vsBot ? "Режим: срещу бот (ботът е Играч 2)" : "Режим: двама играчи на една машина";
        L(mode, 13, COL_MUTED, LEFT_X + INNER_PAD, 660.0f, rowW);
        L("Изборът се пази за следващите мачове и при [R] рестарт.", 12, COL_MUTED,
                     LEFT_X + INNER_PAD, 682.0f, rowW);
    }

    // ---------------- Right panel: mutators ----------------
    if (fontLoaded) {
        L("МУТАТОРИ", 20, COL_TITLE, RIGHT_X + INNER_PAD, PANEL_Y + 20.0f);
        float flash = 0.0f;
        if (refusedFlash > 0.0f) {
            flash = std::max(0.0f, 1.0f - flashClock.getElapsedTime().asSeconds() / 1.2f);
            if (flash <= 0.0f) refusedFlash = 0.0f;
        }
        std::string count = "избрани " + std::to_string(rules.activeMutatorCount()) + " от " + std::to_string(MAX_ACTIVE_MUTATORS);
        texts.draw(window, 9000, font, count, 14, flash > 0.0f ? COL_REFUSED : COL_VALUE, RIGHT_X + PANEL_W - INNER_PAD,
                   PANEL_Y + 20.0f, 300.0f, OptionsTextCache::RIGHT, 10);
    }
    for (int i = 0; i < MUTATOR_COUNT; ++i) {
        int item = ITEM_MUTATOR_FIRST + i;
        std::uint32_t flag = MatchInfo::mutatorFlag(i);
        bool on = rules.hasMutator(flag);
        drawFocus(item);
        sf::FloatRect r = itemRect(item);
        float cy = r.position.y + r.size.y / 2.0f;
        sf::RectangleShape box({ 18.0f, 18.0f });
        box.setPosition({ r.position.x + 10.0f, cy - 9.0f });
        box.setFillColor(on ? COL_CHIP_ON : COL_CHIP);
        box.setOutlineThickness(1.5f);
        box.setOutlineColor(on ? COL_GOOD : COL_ARROW);
        window.draw(box);
        if (on) {
            sf::Vertex check[3];
            check[0].position = { r.position.x + 13.0f, cy };
            check[1].position = { r.position.x + 18.0f, cy + 5.0f };
            check[2].position = { r.position.x + 25.0f, cy - 5.0f };
            for (auto& v : check) v.color = COL_TEXT;
            window.draw(check, 3, sf::PrimitiveType::LineStrip);
        }
        if (fontLoaded) {
            L(MatchInfo::mutatorName(flag), 15, on ? COL_GOOD : COL_TEXT, r.position.x + 38.0f, cy, 190.0f);
            L(MatchInfo::mutatorDescription(flag), 12, COL_MUTED, r.position.x + 236.0f, cy,
                         r.size.x - 236.0f - 8.0f);
        }
    }
    {
        sf::FloatRect r = itemRect(ITEM_MUTATOR_RANDOM);
        sf::RectangleShape b(r.size);
        b.setPosition(r.position);
        b.setFillColor(focus == ITEM_MUTATOR_RANDOM ? COL_FOCUS_BG : COL_CHIP);
        b.setOutlineThickness(focus == ITEM_MUTATOR_RANDOM ? 2.0f : 1.5f);
        b.setOutlineColor(focus == ITEM_MUTATOR_RANDOM ? COL_FOCUS_EDGE : COL_PANEL_EDGE);
        window.draw(b);
        if (fontLoaded) C("СЛУЧАЙНИ МУТАТОРИ", 14, COL_TEXT, r);
    }

    // ---------------- Right panel: charters ----------------
    if (fontLoaded) {
        L("СТАРТОВИ ХАРТИ", 20, COL_TITLE, RIGHT_X + INNER_PAD, 538.0f);
    }
    for (int idx = 0; idx < 2; ++idx) {
        int item = (idx == 0) ? ITEM_CHARTER_P1 : ITEM_CHARTER_P2;
        drawFocus(item);
        sf::FloatRect r = itemRect(item);
        float cy = r.position.y + r.size.y / 2.0f;
        drawTriangle(window, arrowRect(item, -1), -1, focus == item ? COL_FOCUS_EDGE : COL_ARROW);
        drawTriangle(window, arrowRect(item, +1), +1, focus == item ? COL_FOCUS_EDGE : COL_ARROW);
        if (fontLoaded) {
            std::string who = (idx == 0) ? "ИГРАЧ 1 (ЗАПАД)" : (vsBot ? "БОТ (ИЗТОК)" : "ИГРАЧ 2 (ИЗТОК)");
            L(who, 15, idx == 0 ? COL_P1 : COL_P2, r.position.x + 12.0f, cy, 176.0f);
            sf::FloatRect left = arrowRect(item, -1), right = arrowRect(item, +1);
            sf::FloatRect valueBox({ left.position.x + left.size.x, r.position.y },
                                   { right.position.x - (left.position.x + left.size.x), r.size.y });
            C(valueText(item), 16, COL_VALUE, valueBox);
            CharterType ct = rules.charter[idx];
            L(MatchInfo::charterDescription(ct), 13, ct == CharterType::NONE ? COL_MUTED : COL_GOOD,
                         r.position.x + 12.0f, r.position.y + r.size.y + 12.0f, r.size.x - 24.0f);
            const char* minus = MatchInfo::charterDrawback(ct);
            if (minus[0] != '\0') {
                L(minus, 13, COL_BAD, r.position.x + 12.0f, r.position.y + r.size.y + 30.0f, r.size.x - 24.0f);
            }
        }
    }

    // ---------------- Buttons + hint ----------------
    for (int item : { static_cast<int>(ITEM_START), static_cast<int>(ITEM_BACK) }) {
        sf::FloatRect r = itemRect(item);
        bool f = (focus == item);
        sf::RectangleShape b(r.size);
        b.setPosition(r.position);
        if (item == ITEM_START) b.setFillColor(f ? COL_START_HOVER : COL_START);
        else b.setFillColor(f ? COL_BACK_HOVER : COL_BACK);
        b.setOutlineThickness(f ? 3.0f : 1.5f);
        b.setOutlineColor(f ? COL_FOCUS_EDGE : COL_PANEL_EDGE);
        window.draw(b);
        if (fontLoaded) {
            C(item == ITEM_START ? "СТАРТ НА МАЧА" : "НАЗАД", 19, COL_TEXT, r);
        }
    }
    if (fontLoaded) {
        texts.draw(window, 9001, font, "[W/S] Ред   |   [A/D] Промяна   |   [Enter/Space] Избор   |   [Esc] Назад   |   Мишка: клик / колелце", 13, COL_MUTED, VIRTUAL_WIDTH / 2.0f, 845.0f, 1400.0f, OptionsTextCache::CENTER, 10);
    }
}
