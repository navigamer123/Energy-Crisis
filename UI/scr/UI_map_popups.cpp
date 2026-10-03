#include "../includes/UI_map.h"
#include "../includes/UI_infoText.h" // team info
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace {
// team info (UX-03): modal text sizes and line limits
constexpr unsigned MODAL_DETAIL_SIZE = 11;
constexpr unsigned MODAL_TIP_SIZE = 10;
constexpr int MODAL_DETAIL_MAX_LINES = 5;
constexpr int MODAL_TIP_MAX_LINES = 3;
} // namespace

// =============================================================================
// UI_map Popups, Notices, Interactive Modals & Mining Prompts
// =============================================================================

void UI_map::spawnNotice(const std::string& text, sf::Vector2f pos, sf::Color color) {
    FloatingNotice n;
    n.text = text;
    n.pos = pos;
    n.timer = 1.8f;
    n.maxTimer = 1.8f;
    n.color = color;
    notices.push_back(n);
}

void UI_map::drawFloatingNotices(sf::RenderWindow& window) {
    if (!resourcesLoaded) return;

    for (const auto& n : notices) {
        float alphaFrac = n.timer / n.maxTimer;
        std::uint8_t alpha = static_cast<std::uint8_t>(alphaFrac * 255);
        sf::Text t(font, toUtf8(n.text), 14);
        sf::Color c = n.color;
        c.a = alpha;
        t.setFillColor(c);
        sf::FloatRect b = t.getLocalBounds();
        t.setPosition(sf::Vector2f(n.pos.x - b.size.x / 2.0f, n.pos.y));
        window.draw(t);
    }
}

// team info: player popups are toasts of the notification system (UX-06). The badge decides the
// priority and the channel (a newer hint replaces the older one on the same channel). Routine
// mining results are not toasts any more: the floating "+12 Дърво" notice already shows them.
void UI_map::triggerPlayerPopup(int player, const std::string& badge, const std::string& title,
                                const std::string& detail, const std::string& action, sf::Color accent) {
    if (badge == "ДОБИВ") return;
    ToastPriority prio = ToastPriority::INFO;
    std::string channel = badge;
    if (badge == "МЪЛНИЯ!") {
        prio = ToastPriority::CRITICAL;
        channel.clear();
    } else if (badge == "ГРЕШКА" || badge.rfind("НЕДОСТИГ", 0) == 0 || badge.rfind("ГРЕШКА", 0) == 0) {
        prio = ToastPriority::WARNING;
        channel = "error";
    } else if (badge == "СТРОЕЖ" || badge == "ПРЕМАХВАНЕ" || badge == "ОСВЕТЛЕНИЕ" || badge == "ОТКАЗ") {
        channel = "select";
    }
    notifications.push(player, prio, channel, badge, title, detail, action, accent, false);
}

void UI_map::drawPlayerPopups(sf::RenderWindow& window) {
    if (!resourcesLoaded) return;
    notifications.draw(window, font); // team info
}

void UI_map::triggerPlayerModal(int player, const std::string& badge, const std::string& title,
                                const std::string& detail, const std::string& tip, sf::Color accent) {
    if (player == 2 && bot.isActive()) {
        triggerPlayerPopup(2, badge, title, detail, tip, accent);
        return;
    }
    PlayerModalDialog& m = (player == 1) ? p1Modal : p2Modal;
    m.active = true;
    m.badge = badge;
    m.title = title;
    m.detail = detail;
    m.tip = tip;
    m.accentColor = accent;

    // team info (UX-03): the card grows with the wrapped detail and tip text instead of letting it overflow
    float w = 340.0f;
    std::size_t detailLines = 2, tipLines = tip.empty() ? 0 : 1;
    if (resourcesLoaded) {
        detailLines = infoText::wrap(font, detail, MODAL_DETAIL_SIZE, w - 24.0f, MODAL_DETAIL_MAX_LINES).size();
        tipLines = tip.empty() ? 0 : infoText::wrap(font, "СЪВЕТ: " + tip, MODAL_TIP_SIZE, w - 24.0f, MODAL_TIP_MAX_LINES).size();
    }
    float h = 64.0f + 14.0f * static_cast<float>(detailLines) + (tipLines ? 8.0f + 13.0f * static_cast<float>(tipLines) : 0.0f) +
              16.0f + 32.0f + 10.0f;
    h = std::max(h, 170.0f);
    float x = (player == 1) ? (800.0f - w) / 2.0f : 800.0f + (800.0f - w) / 2.0f;
    float y = 355.0f - h / 2.0f; // same centre as the old fixed-size card

    m.box = sf::FloatRect({ x, y }, { w, h });
    m.okBtn = sf::FloatRect({ x + (w - 200.0f) / 2.0f, y + h - 42.0f }, { 200.0f, 32.0f });
}

void UI_map::closePlayerModal(int player) {
    if (player == 1) {
        p1Modal.active = false;
        p1ActionCooldown = 0.35f;
    } else {
        p2Modal.active = false;
        p2ActionCooldown = 0.35f;
    }
    primeInputEdges(player); // The dismissing key (Space/X/Enter/Del...) must not also act in-game
}

void UI_map::drawPlayerModals(sf::RenderWindow& window) {
    if (!resourcesLoaded) return;
    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    auto drawOneModal = [&](const PlayerModalDialog& m, int pIdx) {
        if (!m.active) return;

        // Dim background overlay over that player's half of the screen
        float overlayX = (pIdx == 1) ? 0.0f : 800.0f;
        sf::RectangleShape overlay({ 800.0f, 900.0f });
        overlay.setPosition({ overlayX, 0.0f });
        overlay.setFillColor(sf::Color(0, 0, 0, 140));
        window.draw(overlay);

        // Modal main box
        sf::RectangleShape card(m.box.size);
        card.setPosition(m.box.position);
        card.setFillColor(sf::Color(16, 22, 34, 252));
        card.setOutlineThickness(2.5f);
        card.setOutlineColor(m.accentColor);
        window.draw(card);

        // Header bar
        sf::RectangleShape hBar({ m.box.size.x, 30.0f });
        hBar.setPosition(m.box.position);
        hBar.setFillColor(sf::Color(28, 38, 54, 250));
        window.draw(hBar);

        // Badge / Alert Icon
        sf::Text tBadge(font, toUtf8("! " + m.badge), 12);
        tBadge.setFillColor(m.accentColor);
        tBadge.setPosition({ m.box.position.x + 10.0f, m.box.position.y + 6.0f });
        window.draw(tBadge);

        // team info (UX-03): title, detail and tip are fitted / wrapped to the card width
        const float innerW = m.box.size.x - 24.0f;
        sf::Text tTitle(font, infoText::ellipsize(font, toUtf8(m.title), 13, innerW), 13);
        tTitle.setFillColor(sf::Color::White);
        tTitle.setPosition({ m.box.position.x + 12.0f, m.box.position.y + 38.0f });
        window.draw(tTitle);

        float ly = m.box.position.y + 64.0f;
        for (const sf::String& line : infoText::wrap(font, m.detail, MODAL_DETAIL_SIZE, innerW, MODAL_DETAIL_MAX_LINES)) {
            sf::Text tDetail(font, line, MODAL_DETAIL_SIZE);
            tDetail.setFillColor(sf::Color(200, 225, 250));
            tDetail.setPosition({ m.box.position.x + 12.0f, ly });
            window.draw(tDetail);
            ly += 14.0f;
        }

        if (!m.tip.empty()) {
            ly += 8.0f;
            for (const sf::String& line : infoText::wrap(font, "СЪВЕТ: " + m.tip, MODAL_TIP_SIZE, innerW, MODAL_TIP_MAX_LINES)) {
                sf::Text tTip(font, line, MODAL_TIP_SIZE);
                tTip.setFillColor(sf::Color(255, 225, 110));
                tTip.setPosition({ m.box.position.x + 12.0f, ly });
                window.draw(tTip);
                ly += 13.0f;
            }
        }

        // [ OK - РАЗБРАХ ] Button (clickable only by the player who owns the mouse)
        bool btnHover = (mouseOwnerAt(mousePos) == pIdx) && m.okBtn.contains(mousePos);
        sf::RectangleShape btn(m.okBtn.size);
        btn.setPosition(m.okBtn.position);
        btn.setFillColor(btnHover ? sf::Color(55, 160, 95) : sf::Color(35, 110, 65));
        btn.setOutlineThickness(1.5f);
        btn.setOutlineColor(btnHover ? sf::Color(100, 255, 180) : sf::Color(70, 210, 110));
        window.draw(btn);

        sf::Text tOk(font, toUtf8(pIdx == 1 ? "OK [SPACE] - РАЗБРАХ" : "OK [ENTER] - РАЗБРАХ"), 12);
        tOk.setFillColor(sf::Color::White);
        sf::FloatRect ob = tOk.getLocalBounds();
        tOk.setPosition({ m.okBtn.position.x + (m.okBtn.size.x - ob.size.x) / 2.0f, m.okBtn.position.y + 6.0f });
        window.draw(tOk);
    };

    drawOneModal(p1Modal, 1);
    drawOneModal(p2Modal, 2);
}

void UI_map::drawMiningZonesAndBadges(sf::RenderWindow& window) {
    float animTime = animClock.getElapsedTime().asSeconds();

    ResourceType p1Res = nodes.getP1ResourceAt(p1Pos);
    ResourceType p2Res = nodes.getP2ResourceAt(p2Pos);

    auto drawPrompt = [&](sf::Vector2f pos, const std::string& title, const std::string& keyStr, sf::Color col, float cd) {
        if (!resourcesLoaded) return;
        sf::RectangleShape tagBox({ 260.0f, 26.0f });
        tagBox.setPosition({ pos.x - 130.0f, pos.y - 44.0f });
        tagBox.setFillColor(sf::Color(10, 15, 25, 235));
        tagBox.setOutlineThickness(1.5f);
        tagBox.setOutlineColor(cd > 0.05f ? sf::Color(255, 180, 50) : col);
        window.draw(tagBox);

        std::string promptText = title + " | " + keyStr;
        if (cd > 0.05f) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), " (%.1fs)", cd);
            promptText += buf;
        } else {
            char cdBuf[48];
            std::snprintf(cdBuf, sizeof(cdBuf), " [Добив: %gс]", static_cast<double>(Balance::MINE_COOLDOWN_SEC));
            promptText += cdBuf;
        }

        sf::Text t(font, toUtf8(promptText), 11);
        infoText::fitSize(t, 250.0f, 9); // team info (UX-03): long mine names stay inside the tag
        t.setFillColor(cd > 0.05f ? sf::Color(255, 210, 100) : col);
        sf::FloatRect tb = t.getLocalBounds();
        t.setPosition({ tagBox.getPosition().x + (260.0f - tb.size.x) / 2.0f, tagBox.getPosition().y + 5.0f });
        window.draw(t);

        if (cd > 0.05f) {
            float fillRatio = 1.0f - std::max(0.0f, std::min(1.0f, cd / Balance::MINE_COOLDOWN_SEC));
            sf::RectangleShape cdBar({ 256.0f * fillRatio, 3.0f });
            cdBar.setPosition({ tagBox.getPosition().x + 2.0f, tagBox.getPosition().y + 24.0f });
            cdBar.setFillColor(sf::Color(0, 255, 180));
            window.draw(cdBar);
        }
    };

    if (p1Res != ResourceType::NONE) {
        const auto* st = nodes.getStation(1, p1Res);
        std::string name = st ? st->nameBg + " (" + st->yieldStr + ")" : "ДОБИВ";
        sf::Color c = st ? st->themeColor : sf::Color(0, 229, 255);
        drawPrompt(p1Pos, name, "[SPACE]", c, p1ResourceCooldown);
    }
    if (p2Res != ResourceType::NONE) {
        const auto* st = nodes.getStation(2, p2Res);
        std::string name = st ? st->nameBg + " (" + st->yieldStr + ")" : "ДОБИВ";
        sf::Color c = st ? st->themeColor : sf::Color(255, 120, 200);
        drawPrompt(p2Pos, name, "[ENTER]", c, p2ResourceCooldown);
    }

    // High-speed 6x time badges under the top clocks when active
    if (engine.getTimeScale() > 1.5f && resourcesLoaded) {
        float pulse = (std::sin(animTime * 8.0f) + 1.0f) * 0.5f;
        std::uint8_t glowAlpha = static_cast<std::uint8_t>(180 + pulse * 75);

        // team info (UX-04): the badge sits in the free strip right of / left of the clock cards
        // (x 262-512 and 1088-1338, y 14-38) instead of on top of the building-panel headers. The
        // "fast forward" mark is drawn as two triangles (the font has no U+23E9 glyph).
        constexpr float BADGE_W = 250.0f;
        constexpr float BADGE_H = 24.0f;
        auto drawClockSpeedBadge = [&](float x, float y) {
            sf::RectangleShape badge({ BADGE_W, BADGE_H });
            badge.setPosition({ x, y });
            badge.setFillColor(sf::Color(45, 30, 8, 230));
            badge.setOutlineThickness(1.5f);
            badge.setOutlineColor(sf::Color(255, 215, 0, glowAlpha));
            window.draw(badge);

            for (int k = 0; k < 2; ++k) {
                sf::ConvexShape tri(3);
                float tx = x + 10.0f + 8.0f * static_cast<float>(k);
                tri.setPoint(0, { tx, y + 6.0f });
                tri.setPoint(1, { tx + 8.0f, y + BADGE_H / 2.0f });
                tri.setPoint(2, { tx, y + BADGE_H - 6.0f });
                tri.setFillColor(sf::Color(255, 215, 0, glowAlpha));
                window.draw(tri);
            }

            sf::Text bt(font, toUtf8(std::to_string(static_cast<int>(std::lround(engine.getTimeScale()))) +
                                     "x СКОРОСТ НА ВРЕМЕТО (ДОБИВ)"), 10);
            bt.setStyle(sf::Text::Bold);
            infoText::fitSize(bt, BADGE_W - 40.0f, 9);
            bt.setFillColor(sf::Color(255, 235, 120));
            sf::FloatRect btb = bt.getLocalBounds();
            bt.setPosition({ x + 32.0f + (BADGE_W - 40.0f - btb.size.x) / 2.0f, y + 5.0f });
            window.draw(bt);
        };

        drawClockSpeedBadge(262.0f, 14.0f);
        drawClockSpeedBadge(1600.0f - 262.0f - BADGE_W, 14.0f);
    }
}
