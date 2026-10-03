#include "../includes/UI_map.h"
#include "../includes/UI_text.h"
#include "../includes/UI_shot.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

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
        ui::drawText(window, t);
    }
}

void UI_map::triggerPlayerPopup(int player, const std::string& badge, const std::string& title,
                                const std::string& detail, const std::string& action, sf::Color accent) {
    PlayerPopup& pop = (player == 1) ? p1Popup : p2Popup;
    pop.badge = badge;
    pop.title = title;
    pop.detail = detail;
    pop.action = action;
    pop.accentColor = accent;
    pop.timer = 4.0f;
    pop.maxTimer = 4.0f;
    pop.active = true;
}

void UI_map::drawPlayerPopups(sf::RenderWindow& window) {
    if (!resourcesLoaded) return;

    auto drawOnePopup = [&](const PlayerPopup& pop, float x, float y) {
        if (!pop.active) return;

        float alphaRatio = std::min(1.0f, pop.timer / 0.8f);
        std::uint8_t alpha = static_cast<std::uint8_t>(alphaRatio * 245);

        // Glassmorphic container box
        sf::RectangleShape box({ 225.0f, 132.0f });
        box.setPosition({ x, y });
        box.setFillColor(sf::Color(16, 22, 34, alpha));
        box.setOutlineThickness(1.5f);
        sf::Color outColor = pop.accentColor;
        outColor.a = alpha;
        box.setOutlineColor(outColor);
        window.draw(box);
        const sf::FloatRect popupRect({ x, y }, { 225.0f, 132.0f });
        ui::lint::occlude(popupRect);
        ui::lint::ContainerScope popupScope(popupRect);

        // Badge pill
        sf::RectangleShape badgeBox({ 65.0f, 18.0f });
        badgeBox.setPosition({ x + 8.0f, y + 8.0f });
        sf::Color bColor = pop.accentColor;
        bColor.a = static_cast<std::uint8_t>(alpha * 0.65f);
        badgeBox.setFillColor(bColor);
        window.draw(badgeBox);

        sf::Text tBadge(font, toUtf8(pop.badge), 10);
        tBadge.setFillColor(sf::Color(255, 255, 255, alpha));
        sf::FloatRect bb = tBadge.getLocalBounds();
        tBadge.setPosition({ x + 8.0f + (65.0f - bb.size.x) / 2.0f, y + 9.0f });
        ui::drawText(window, tBadge, sf::FloatRect(badgeBox.getPosition(), badgeBox.getSize()));

        // Title
        sf::Text tTitle(font, toUtf8(pop.title), 12);
        tTitle.setFillColor(sf::Color(255, 255, 255, alpha));
        tTitle.setPosition({ x + 78.0f, y + 9.0f });
        ui::drawText(window, tTitle);

        // Separator
        sf::RectangleShape sep({ 209.0f, 1.0f });
        sep.setPosition({ x + 8.0f, y + 31.0f });
        sep.setFillColor(sf::Color(60, 85, 120, alpha));
        window.draw(sep);

        // Detailed Explanation
        sf::Text tDetail(font, toUtf8(pop.detail), 11);
        tDetail.setFillColor(sf::Color(195, 225, 255, alpha));
        tDetail.setPosition({ x + 8.0f, y + 36.0f });
        ui::drawText(window, tDetail);

        // Action instructions
        if (!pop.action.empty()) {
            sf::Text tAct(font, toUtf8(pop.action), 10);
            tAct.setFillColor(sf::Color(255, 215, 80, alpha));
            tAct.setPosition({ x + 8.0f, y + 92.0f });
            ui::drawText(window, tAct);
        }

        // Timer progress bar at the bottom
        float pWidth = 209.0f * (pop.timer / pop.maxTimer);
        sf::RectangleShape prog({ std::max(0.0f, pWidth), 2.5f });
        prog.setPosition({ x + 8.0f, y + 122.0f });
        prog.setFillColor(outColor);
        window.draw(prog);
    };

    drawOnePopup(p1Popup, 20.0f, 520.0f);
    drawOnePopup(p2Popup, 1600.0f - 245.0f, 520.0f);
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
    m.accentColor = accent;

    // Wrap every text to the box and grow the box to fit, so long engine messages stay inside
    const float w = 360.0f;
    const float textW = w - 24.0f;
    auto textHeight = [this](const std::string& s, unsigned int size) {
        if (s.empty() || !resourcesLoaded) return 0.0f;
        sf::Text t(font, toUtf8(s), size);
        return t.getLocalBounds().position.y + t.getLocalBounds().size.y;
    };
    m.title = resourcesLoaded ? ui::wrapText(font, title, 13, textW) : title;
    m.detail = resourcesLoaded ? ui::wrapText(font, detail, 11, textW) : detail;
    std::string tipText = tip.empty() ? std::string() : "СЪВЕТ: " + tip;
    m.tip = resourcesLoaded ? ui::wrapText(font, tipText, 11, textW) : tipText;

    m.detailY = 38.0f + textHeight(m.title, 13) + 10.0f;
    m.tipY = m.detailY + textHeight(m.detail, 11) + 12.0f;
    float okY = m.tipY + (m.tip.empty() ? 0.0f : textHeight(m.tip, 11) + 14.0f);
    float h = std::max(190.0f, okY + 32.0f + 12.0f);

    // Centred in the player's free map area (between the building panel and the centre line)
    float x = (player == 1) ? (248.0f + 800.0f - w) / 2.0f : (800.0f + 1352.0f - w) / 2.0f;
    float y = std::max(120.0f, 355.0f - h / 2.0f);

    m.box = sf::FloatRect({ x, y }, { w, h });
    m.okBtn = sf::FloatRect({ x + (w - 200.0f) / 2.0f, y + h - 44.0f }, { 200.0f, 32.0f });
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
    sf::Vector2f mousePos = ui::pointerPos(window);

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
        ui::lint::occlude(m.box);
        ui::lint::ContainerScope modalScope(m.box);

        // Header bar
        sf::RectangleShape hBar({ m.box.size.x, 30.0f });
        hBar.setPosition(m.box.position);
        hBar.setFillColor(sf::Color(28, 38, 54, 250));
        window.draw(hBar);

        // Badge / Alert Icon
        sf::Text tBadge(font, toUtf8("! " + m.badge), 12);
        tBadge.setFillColor(m.accentColor);
        tBadge.setPosition({ m.box.position.x + 10.0f, m.box.position.y + 6.0f });
        ui::drawText(window, tBadge);

        // Title
        sf::Text tTitle(font, toUtf8(m.title), 13);
        tTitle.setFillColor(sf::Color::White);
        tTitle.setPosition({ m.box.position.x + 12.0f, m.box.position.y + 38.0f });
        ui::drawText(window, tTitle);

        // Detail explanation
        sf::Text tDetail(font, toUtf8(m.detail), 11);
        tDetail.setFillColor(sf::Color(200, 225, 250));
        tDetail.setPosition({ m.box.position.x + 12.0f, m.box.position.y + m.detailY });
        ui::drawText(window, tDetail);

        // Tip text
        if (!m.tip.empty()) {
            sf::Text tTip(font, toUtf8(m.tip), 11);
            tTip.setFillColor(sf::Color(255, 225, 110));
            tTip.setPosition({ m.box.position.x + 12.0f, m.box.position.y + m.tipY });
            ui::drawText(window, tTip);
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
        ui::drawText(window, tOk, m.okBtn);
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
        const sf::FloatRect tagRect(tagBox.getPosition(), tagBox.getSize());
        ui::lint::occlude(tagRect);

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
        t.setFillColor(cd > 0.05f ? sf::Color(255, 210, 100) : col);
        sf::FloatRect tb = t.getLocalBounds();
        t.setPosition({ tagBox.getPosition().x + (260.0f - tb.size.x) / 2.0f, tagBox.getPosition().y + 5.0f });
        ui::drawText(window, t, tagRect);

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

        auto drawClockSpeedBadge = [&](float x, float y) {
            sf::RectangleShape badge({ 230.0f, 24.0f });
            badge.setPosition({ x, y });
            badge.setFillColor(sf::Color(45, 30, 8, 230));
            badge.setOutlineThickness(1.5f);
            badge.setOutlineColor(sf::Color(255, 215, 0, glowAlpha));
            window.draw(badge);
            const sf::FloatRect badgeRect({ x, y }, { 230.0f, 24.0f });
            ui::lint::occlude(badgeRect);

            sf::Text bt(font, toUtf8("⏩ 6x СКОРОСТ НА ВРЕМЕТО (ДОБИВ)"), 10);
            bt.setStyle(sf::Text::Bold);
            bt.setFillColor(sf::Color(255, 235, 120));
            sf::FloatRect btb = bt.getLocalBounds();
            bt.setPosition({ x + (230.0f - btb.size.x) / 2.0f, y + 5.0f });
            ui::drawText(window, bt, badgeRect);
        };

        drawClockSpeedBadge(20.0f, 115.0f);
        drawClockSpeedBadge(1600.0f - 250.0f, 115.0f);
    }
}
