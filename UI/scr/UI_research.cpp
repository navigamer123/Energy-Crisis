// =============================================================================
// F-33 Research lab UI [team b-options]: lab buildings + research panels
// =============================================================================
#include "../includes/UI_research.h"
#include "../includes/UI_optionsText.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

// --- Colours (integrator: map these to UI_theme.h tokens) ---------------------------------
const sf::Color COL_P1(0, 229, 255);
const sf::Color COL_P2(255, 120, 200);
const sf::Color COL_BRANCH[TECH_BRANCHES] = { sf::Color(255, 205, 60), sf::Color(90, 200, 255), sf::Color(255, 145, 70) };
const sf::Color COL_PANEL(14, 20, 32, 248);
const sf::Color COL_HEADER(24, 34, 52);
const sf::Color COL_TEXT(232, 240, 252);
const sf::Color COL_MUTED(150, 168, 192);
const sf::Color COL_DIM(95, 108, 128);
const sf::Color COL_GOLD(255, 214, 96);
const sf::Color COL_GOOD(120, 238, 150);
const sf::Color COL_BAD(255, 120, 110);
const sf::Color COL_FOCUS(255, 215, 0);
const sf::Color COL_CARD(28, 38, 56);
const sf::Color COL_CARD_DONE(26, 82, 52);
const sf::Color COL_CARD_OFF(20, 25, 34);
const sf::Color COL_WINDOW_OFF(44, 56, 76);
const sf::Color COL_BODY(32, 42, 60);
const sf::Color COL_GLASS(70, 140, 185, 210);

// --- Layout -------------------------------------------------------------------------------
constexpr float LAB_W = 160.0f, LAB_H = 146.0f, LAB_Y = 606.0f;
constexpr float LAB_X[2] = { 626.0f, 814.0f };
constexpr float PANEL_X[2] = { 16.0f, 816.0f };
constexpr float PANEL_Y = 118.0f, PANEL_W = 768.0f, PANEL_H = 520.0f;
constexpr float PAD = 16.0f, COL_GAP = 12.0f;
constexpr float COLUMN_HEADER_Y = 72.0f, TIERS_Y = 104.0f, TIER_H = 121.0f, TIER_GAP = 8.0f;
constexpr float CARD_H = 46.0f, CARD_GAP = 5.0f, TIER_LABEL_H = 24.0f;

sf::Color playerColor(int player) { return player == 2 ? COL_P2 : COL_P1; }

std::string groupThousands(int value) {
    std::string digits = std::to_string(std::abs(value));
    std::string out;
    int n = static_cast<int>(digits.size());
    for (int i = 0; i < n; ++i) {
        out += digits[static_cast<size_t>(i)];
        int left = n - 1 - i;
        if (left > 0 && left % 3 == 0) out += ' ';
    }
    return (value < 0 ? "-" : "") + out;
}

// Cached label: x by alignment (0 left, 1 centre, 2 right), centred on the caps of its size
void drawText(OptionsTextCache& cache, int slot, sf::RenderWindow& window, const sf::Font& font, const std::string& s,
              unsigned int size, sf::Color c, float x, float centerY, float maxW, int align = 0) {
    cache.draw(window, slot, font, s, size, c, x, centerY, maxW, static_cast<OptionsTextCache::Align>(align));
}

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

void drawCheck(sf::RenderWindow& window, float x, float y, sf::Color c) {
    // Thick check mark from two rotated bars
    sf::RectangleShape a({ 7.0f, 3.0f });
    a.setOrigin({ 0.0f, 1.5f });
    a.setPosition({ x, y });
    a.setRotation(sf::degrees(45.0f));
    a.setFillColor(c);
    window.draw(a);
    sf::RectangleShape b({ 13.0f, 3.0f });
    b.setOrigin({ 0.0f, 1.5f });
    b.setPosition({ x + 4.0f, y + 5.0f });
    b.setRotation(sf::degrees(-50.0f));
    b.setFillColor(c);
    window.draw(b);
}

void drawLock(sf::RenderWindow& window, float x, float y, sf::Color c) {
    sf::CircleShape shackle(5.0f);
    shackle.setPosition({ x + 1.0f, y });
    shackle.setFillColor(sf::Color::Transparent);
    shackle.setOutlineThickness(2.0f);
    shackle.setOutlineColor(c);
    window.draw(shackle);
    drawRect(window, { { x - 1.0f, y + 6.0f }, { 14.0f, 10.0f } }, c);
}

} // namespace

UI_research::UI_research() {}

sf::FloatRect UI_research::labRect(int player) {
    return { { LAB_X[idx(player)], LAB_Y }, { LAB_W, LAB_H } };
}

sf::FloatRect UI_research::panelRect(int player) {
    return { { PANEL_X[idx(player)], PANEL_Y }, { PANEL_W, PANEL_H } };
}

sf::FloatRect UI_research::cardRect(int player, int branch, int tier, int option) {
    const float colW = (PANEL_W - 2.0f * PAD - 2.0f * COL_GAP) / 3.0f;
    float x = PANEL_X[idx(player)] + PAD + static_cast<float>(branch) * (colW + COL_GAP);
    float tierTop = PANEL_Y + TIERS_Y + static_cast<float>(tier) * (TIER_H + TIER_GAP);
    float y = tierTop + TIER_LABEL_H + static_cast<float>(option) * (CARD_H + CARD_GAP);
    return { { x, y }, { colW, CARD_H } };
}

void UI_research::open(int player) {
    openState[idx(player)] = true;
    message[idx(player)].clear();
}

void UI_research::move(int player, int dBranch, int dSlot) {
    int i = idx(player);
    focusB[i] = (focusB[i] + dBranch + TECH_BRANCHES) % TECH_BRANCHES;
    const int slots = TECH_TIERS * TECH_OPTIONS;
    focusS[i] = (focusS[i] + dSlot + slots) % slots;
}

void UI_research::setFocus(int player, int branch, int tier, int option) {
    int i = idx(player);
    focusB[i] = std::max(0, std::min(TECH_BRANCHES - 1, branch));
    focusS[i] = std::max(0, std::min(TECH_TIERS - 1, tier)) * TECH_OPTIONS + std::max(0, std::min(TECH_OPTIONS - 1, option));
}

bool UI_research::cardAt(int player, sf::Vector2f p, int& branch, int& tier, int& option) const {
    if (!isOpen(player)) return false;
    for (int b = 0; b < TECH_BRANCHES; ++b)
        for (int t = 0; t < TECH_TIERS; ++t)
            for (int o = 0; o < TECH_OPTIONS; ++o)
                if (cardRect(player, b, t, o).contains(p)) {
                    branch = b; tier = t; option = o;
                    return true;
                }
    return false;
}

void UI_research::flash(int player, const std::string& msg, bool ok) {
    int i = idx(player);
    message[i] = msg;
    messageOk[i] = ok;
    messageClock[i].restart();
}

// -----------------------------------------------------------------------------
// Laboratory building on the map
// -----------------------------------------------------------------------------
void UI_research::drawLab(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded, const GameEngine& engine,
                          int player, float animTime, bool cursorOver, const std::string& keyHint, bool interactive) const {
    sf::FloatRect r = labRect(player);
    const sf::Color pc = playerColor(player);
    const float x = r.position.x, y = r.position.y;

    bool anyAvailable = false;
    // Only a human player gets the "you can research" hints (the bot researches by itself)
    for (int b = 0; interactive && b < TECH_BRANCHES && !anyAvailable; ++b)
        for (int t = 0; t < TECH_TIERS && !anyAvailable; ++t)
            for (int o = 0; o < TECH_OPTIONS; ++o)
                if (engine.getTechStatus(player, b, t, o) == TechStatus::AVAILABLE) { anyAvailable = true; break; }

    // Ground strip and soft player-coloured glow
    float pulse = 0.5f + 0.5f * std::sin(animTime * 4.0f);
    if (anyAvailable || cursorOver || isOpen(player)) {
        std::uint8_t a = static_cast<std::uint8_t>(cursorOver || isOpen(player) ? 70 : 30 + 40 * pulse);
        drawRect(window, { { x + 8.0f, y + 20.0f }, { LAB_W - 16.0f, 104.0f } }, sf::Color(pc.r, pc.g, pc.b, a));
    }
    drawRect(window, { { x + 6.0f, y + 118.0f }, { LAB_W - 12.0f, 6.0f } }, sf::Color(38, 48, 40));

    // Antenna with a blinking beacon
    drawRect(window, { { x + 79.0f, y + 8.0f }, { 2.0f, 20.0f } }, sf::Color(170, 180, 195));
    sf::CircleShape beacon(3.5f);
    beacon.setOrigin({ 3.5f, 3.5f });
    beacon.setPosition({ x + 80.0f, y + 8.0f });
    beacon.setFillColor(std::fmod(animTime, 1.2f) < 0.6f ? pc : sf::Color(255, 80, 80));
    window.draw(beacon);

    // Glass dome (lower half hidden behind the body)
    sf::CircleShape dome(34.0f);
    dome.setOrigin({ 34.0f, 34.0f });
    dome.setPosition({ x + 80.0f, y + 60.0f });
    dome.setFillColor(COL_GLASS);
    dome.setOutlineThickness(1.5f);
    dome.setOutlineColor(sf::Color(185, 230, 255, 170));
    window.draw(dome);
    sf::CircleShape shine(9.0f);
    shine.setPosition({ x + 60.0f, y + 33.0f });
    shine.setFillColor(sf::Color(255, 255, 255, 60));
    window.draw(shine);

    // Body
    drawRect(window, { { x + 22.0f, y + 58.0f }, { 116.0f, 64.0f } }, COL_BODY, cursorOver || isOpen(player) ? 2.5f : 1.5f,
             cursorOver || isOpen(player) ? COL_FOCUS : sf::Color(pc.r, pc.g, pc.b, 170));

    // Windows = research progress: column per branch, bottom row = tier 1
    for (int b = 0; b < TECH_BRANCHES; ++b) {
        float cx = x + 80.0f + static_cast<float>(b - 1) * 36.0f;
        for (int t = 0; t < TECH_TIERS; ++t) {
            float wy = y + 94.0f - static_cast<float>(t) * 14.0f;
            bool done = engine.getTechChoice(player, b, t) >= 0;
            drawRect(window, { { cx - 9.0f, wy }, { 18.0f, 9.0f } }, done ? COL_BRANCH[b] : COL_WINDOW_OFF);
        }
    }
    // Door
    drawRect(window, { { x + 73.0f, y + 107.0f }, { 14.0f, 15.0f } }, sf::Color(20, 26, 38), 1.0f, sf::Color(pc.r, pc.g, pc.b, 140));

    // "New research affordable" badge
    if (anyAvailable && !isOpen(player)) {
        float rad = 9.0f + 1.5f * pulse;
        sf::CircleShape badge(rad);
        badge.setOrigin({ rad, rad });
        badge.setPosition({ x + 134.0f, y + 44.0f });
        badge.setFillColor(sf::Color(255, 200, 40));
        badge.setOutlineThickness(1.5f);
        badge.setOutlineColor(sf::Color(90, 60, 0));
        window.draw(badge);
        if (fontLoaded) drawText(texts, 1000 * player + 2, window, font, "!", 14, sf::Color(40, 25, 0), x + 134.0f, y + 44.0f, 20.0f, 1);
    }

    if (fontLoaded) {
        drawRect(window, { { x + 4.0f, y + 126.0f }, { LAB_W - 8.0f, 18.0f } }, sf::Color(12, 18, 28, 225));
        drawText(texts, 1000 * player + 1, window, font, "ЛАБОРАТОРИЯ " + keyHint, 12, pc, x + LAB_W / 2.0f, y + 135.0f, LAB_W - 14.0f, 1);
    }
}

// -----------------------------------------------------------------------------
// Research panel
// -----------------------------------------------------------------------------
void UI_research::drawPanel(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded, const GameEngine& engine,
                            int player, float animTime, const std::string& researchKey, const std::string& navHint) {
    (void)animTime;
    if (!isOpen(player)) return;
    const int i = idx(player);
    const sf::FloatRect pr = panelRect(player);
    const sf::Color pc = playerColor(player);
    const PlayerEconomy& econ = engine.getPlayerEconomy(player);
    const int base = 10000 * player; // text cache slots of this panel
    const float colW = (PANEL_W - 2.0f * PAD - 2.0f * COL_GAP) / 3.0f;

    drawRect(window, pr, COL_PANEL, 2.0f, pc);
    drawRect(window, { pr.position, { pr.size.x, 46.0f } }, COL_HEADER);
    drawRect(window, { { pr.position.x, pr.position.y + 46.0f }, { pr.size.x, 2.0f } }, sf::Color(pc.r, pc.g, pc.b, 160));

    if (fontLoaded) {
        drawText(texts, base + 1, window, font, player == 1 ? "ЛАБОРАТОРИЯ · ИГРАЧ 1" : "ЛАБОРАТОРИЯ · ИГРАЧ 2", 19, pc,
                 pr.position.x + PAD, pr.position.y + 23.0f, 400.0f);
        drawText(texts, base + 2, window, font, "Пари: " + groupThousands(econ.money) + " $", 18, COL_GOLD,
                 pr.position.x + pr.size.x - PAD, pr.position.y + 23.0f, 300.0f, 2);
        drawText(texts, base + 3, window, font, "Във всяко ниво изберете 1 от 2 технологии. Изборът е за постоянно.", 12, COL_MUTED,
                 pr.position.x + pr.size.x / 2.0f, pr.position.y + 59.0f, pr.size.x - 2.0f * PAD, 1);
    }

    for (int b = 0; b < TECH_BRANCHES; ++b) {
        float cx = pr.position.x + PAD + static_cast<float>(b) * (colW + COL_GAP);
        const sf::Color bc = COL_BRANCH[b];
        drawRect(window, { { cx, pr.position.y + COLUMN_HEADER_Y }, { colW, 26.0f } },
                 sf::Color(bc.r / 5, bc.g / 5, bc.b / 5, 255), 1.0f, sf::Color(bc.r, bc.g, bc.b, 150));
        if (fontLoaded) {
            drawText(texts, base + 10 + b * 2, window, font, MatchInfo::techBranchName(b), 15, bc, cx + 10.0f, pr.position.y + COLUMN_HEADER_Y + 13.0f, colW - 60.0f);
            drawText(texts, base + 11 + b * 2, window, font, std::to_string(engine.getResearchedTierCount(player, b)) + "/" + std::to_string(TECH_TIERS), 14,
                     COL_TEXT, cx + colW - 10.0f, pr.position.y + COLUMN_HEADER_Y + 13.0f, 50.0f, 2);
        }

        for (int t = 0; t < TECH_TIERS; ++t) {
            float tierTop = pr.position.y + TIERS_Y + static_cast<float>(t) * (TIER_H + TIER_GAP);
            int chosen = engine.getTechChoice(player, b, t);
            bool locked = (chosen < 0 && t > 0 && engine.getTechChoice(player, b, t - 1) < 0);
            int cost = MatchInfo::techTierCost(t);
            if (fontLoaded) {
                std::string label = "НИВО " + std::to_string(t + 1) + " · ";
                sf::Color lc;
                if (chosen >= 0) { label += "ИЗБРАНО"; lc = COL_GOOD; }
                else if (locked) { label += groupThousands(cost) + " $"; lc = COL_DIM; }
                else { label += groupThousands(cost) + " $"; lc = (econ.money >= cost) ? COL_GOLD : COL_BAD; }
                drawText(texts, base + 100 + b * 10 + t, window, font, label, 12, lc, cx + 2.0f, tierTop + 10.0f, colW - 4.0f);
            }
            for (int o = 0; o < TECH_OPTIONS; ++o) {
                sf::FloatRect cr = cardRect(player, b, t, o);
                TechStatus st = engine.getTechStatus(player, b, t, o);
                bool focused = (focusB[i] == b && focusS[i] == t * TECH_OPTIONS + o);
                sf::Color fill = COL_CARD, nameC = COL_TEXT, effC = COL_MUTED;
                switch (st) {
                    case TechStatus::RESEARCHED:   fill = COL_CARD_DONE; nameC = COL_TEXT; effC = COL_GOOD; break;
                    case TechStatus::EXCLUDED:     fill = COL_CARD_OFF; nameC = COL_DIM; effC = COL_DIM; break;
                    case TechStatus::LOCKED:       fill = COL_CARD_OFF; nameC = COL_DIM; effC = COL_DIM; break;
                    case TechStatus::AVAILABLE:    fill = COL_CARD; nameC = COL_TEXT; effC = bc; break;
                    case TechStatus::UNAFFORDABLE: fill = COL_CARD; nameC = COL_MUTED; effC = COL_MUTED; break;
                }
                drawRect(window, cr, fill, focused ? 2.5f : 1.0f,
                         focused ? COL_FOCUS : (st == TechStatus::RESEARCHED ? COL_GOOD : sf::Color(60, 78, 104)));
                const TechInfo& info = MatchInfo::techInfo(b, t, o);
                if (fontLoaded) {
                    float textW = cr.size.x - 34.0f;
                    drawText(texts, base + 200 + b * 100 + t * 10 + o * 2, window, font, info.name, 14, nameC, cr.position.x + 9.0f, cr.position.y + 14.0f, textW);
                    drawText(texts, base + 201 + b * 100 + t * 10 + o * 2, window, font, info.effect, 12, effC, cr.position.x + 9.0f, cr.position.y + 33.0f, textW);
                }
                if (st == TechStatus::RESEARCHED) {
                    drawCheck(window, cr.position.x + cr.size.x - 24.0f, cr.position.y + 22.0f, COL_GOOD);
                } else if (st == TechStatus::LOCKED) {
                    drawLock(window, cr.position.x + cr.size.x - 22.0f, cr.position.y + 14.0f, COL_DIM);
                } else if (st == TechStatus::EXCLUDED) {
                    // Struck through: the other card of this tier was chosen
                    drawRect(window, { { cr.position.x + 6.0f, cr.position.y + cr.size.y / 2.0f }, { cr.size.x - 12.0f, 1.5f } },
                             sf::Color(120, 130, 150, 160));
                }
            }
        }
    }

    // Footer: feedback message or what the focused card needs
    if (fontLoaded) {
        float fy = pr.position.y + pr.size.y - 18.0f;
        drawRect(window, { { pr.position.x + 2.0f, fy - 13.0f }, { pr.size.x - 4.0f, 28.0f } }, COL_HEADER);
        std::string status;
        sf::Color sc = COL_MUTED;
        if (!message[i].empty() && messageClock[i].getElapsedTime().asSeconds() < 3.0f) {
            status = message[i];
            sc = messageOk[i] ? COL_GOOD : COL_BAD;
        } else {
            int b = focusB[i], t = focusS[i] / TECH_OPTIONS, o = focusS[i] % TECH_OPTIONS;
            int cost = MatchInfo::techTierCost(t);
            switch (engine.getTechStatus(player, b, t, o)) {
                case TechStatus::RESEARCHED:
                    status = "ИЗСЛЕДВАНО: ефектът е активен."; sc = COL_GOOD; break;
                case TechStatus::EXCLUDED:
                    status = std::string("В това ниво вече е избрано: ") +
                             MatchInfo::techInfo(b, t, engine.getTechChoice(player, b, t)).name;
                    break;
                case TechStatus::LOCKED:
                    status = "ЗАКЛЮЧЕНО: първо изследвайте ниво " + std::to_string(t) + " в този клон."; break;
                case TechStatus::AVAILABLE:
                    status = researchKey + " ИЗСЛЕДВАЙ ЗА " + groupThousands(cost) + " $"; sc = COL_GOLD; break;
                case TechStatus::UNAFFORDABLE:
                    status = "Нужни още " + groupThousands(cost - econ.money) + " $ (парите идват от тока за града)."; sc = COL_BAD; break;
            }
        }
        drawText(texts, base + 900, window, font, status, 13, sc, pr.position.x + PAD, fy, pr.size.x * 0.62f);
        drawText(texts, base + 901, window, font, navHint, 12, COL_MUTED, pr.position.x + pr.size.x - PAD, fy, pr.size.x * 0.34f, 2);
    }
}
