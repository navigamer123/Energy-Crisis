#include "../includes/UI_map.h"
#include "../includes/UI_text.h"
#include "../includes/UI_shot.h"
#include "../includes/UI_theme.h"
#include "../includes/UI_icons.h"
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
        sf::Text t(font, toUtf8(n.text), fontsize::Body);
        t.setStyle(sf::Text::Bold);
        t.setFillColor(theme::withAlpha(n.color, alpha));
        sf::FloatRect b = t.getLocalBounds();
        // Dark pill behind the rising text so it stays readable over the map and the panels
        sf::Vector2f pillSize(b.size.x + 12.0f, b.size.y + 8.0f);
        sf::Vector2f pillPos(std::max(2.0f, std::min(n.pos.x - pillSize.x / 2.0f, VIRTUAL_WIDTH - pillSize.x - 2.0f)), n.pos.y);
        sf::RectangleShape pill(pillSize);
        pill.setPosition(pillPos);
        pill.setFillColor(theme::withAlpha(theme::Window, static_cast<std::uint8_t>(alphaFrac * 200)));
        window.draw(pill);
        if (alpha >= 40) ui::lint::occlude(sf::FloatRect(pillPos, pillSize));
        t.setPosition({ pillPos.x + 6.0f - b.position.x, pillPos.y + 4.0f - b.position.y });
        ui::drawText(window, t);
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
    m.accentColor = accent;

    // Wrap every text to the box and grow the box to fit, so long engine messages stay inside
    const float w = 360.0f;
    const float textW = w - 24.0f;
    auto textHeight = [this](const std::string& s, unsigned int size) {
        if (s.empty() || !resourcesLoaded) return 0.0f;
        sf::Text t(font, toUtf8(s), size);
        return t.getLocalBounds().position.y + t.getLocalBounds().size.y;
    };
    m.title = resourcesLoaded ? ui::wrapText(font, title, fontsize::Body, textW, true) : title;
    m.detail = resourcesLoaded ? ui::wrapText(font, detail, fontsize::Label, textW) : detail;
    std::string tipText = tip.empty() ? std::string() : "СЪВЕТ: " + tip;
    m.tip = resourcesLoaded ? ui::wrapText(font, tipText, fontsize::Caption, textW) : tipText;

    m.detailY = 40.0f + textHeight(m.title, fontsize::Body) + 10.0f;
    m.tipY = m.detailY + textHeight(m.detail, fontsize::Label) + 12.0f;
    float okY = m.tipY + (m.tip.empty() ? 0.0f : textHeight(m.tip, fontsize::Caption) + 14.0f);
    float h = std::max(190.0f, okY + 32.0f + 12.0f);

    // Centred in the player's free map area (between the building panel and the centre line)
    float x = (player == 1) ? (248.0f + 800.0f - w) / 2.0f : (800.0f + 1352.0f - w) / 2.0f;
    float y = std::max(120.0f, 355.0f - h / 2.0f);

    m.box = sf::FloatRect({ x, y }, { w, h });
    m.okBtn = sf::FloatRect({ x + (w - 200.0f) / 2.0f, y + h - 44.0f }, { 200.0f, 32.0f });
}

void UI_map::reportBuildFailure(int player, BuildingType sel, const std::string& engineMsg) {
    const sf::Color errorColor = theme::Bad;
    if (sel != BuildingType::DEMOLISH && sel != BuildingType::NONE) {
        BuildingCost cost = engine.getBuildingCost(sel);
        std::string missing = missingResourcesText(engine.getPlayerEconomy(player), cost);
        if (!missing.empty()) {
            std::string key = (player == 1) ? "[SPACE]" : "[ENTER]";
            triggerPlayerModal(player, "НЕДОСТИГ НА РЕСУРСИ", "Не стигат ресурси за " + cost.nameBg, missing,
                               "Добийте ги от станциите долу: застанете върху станция и натиснете " + key +
                                   " (или кликнете върху нея).",
                               errorColor);
            return;
        }
    }
    triggerPlayerModal(player, "ГРЕШКА ПРИ СТРОЕЖ", "Строежът е невъзможен!", engineMsg,
                       !engine.isDaylight() ? "Поставете и захранете Осветителна лампа за работа нощем!"
                                            : "Изберете свободна клетка от ваш закупен парцел.",
                       errorColor);
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
        overlay.setFillColor(theme::withAlpha(theme::Dim, 140));
        window.draw(overlay);

        // Modal main box
        sf::RectangleShape card(m.box.size);
        card.setPosition(m.box.position);
        card.setFillColor(theme::withAlpha(theme::Panel, 252));
        card.setOutlineThickness(2.5f);
        card.setOutlineColor(m.accentColor);
        window.draw(card);
        ui::lint::occlude(m.box);
        ui::lint::ContainerScope modalScope(m.box);

        // Header bar
        sf::RectangleShape hBar({ m.box.size.x, 30.0f });
        hBar.setPosition(m.box.position);
        hBar.setFillColor(theme::PanelHeader);
        window.draw(hBar);

        // Badge / Alert Icon
        sf::Text tBadge(font, toUtf8("! " + m.badge), fontsize::Label);
        tBadge.setStyle(sf::Text::Bold);
        tBadge.setFillColor(m.accentColor);
        tBadge.setPosition({ m.box.position.x + 10.0f, m.box.position.y + 6.0f });
        ui::drawText(window, tBadge);

        // Title
        sf::Text tTitle(font, toUtf8(m.title), fontsize::Body);
        tTitle.setStyle(sf::Text::Bold);
        tTitle.setFillColor(theme::TextPrimary);
        tTitle.setPosition({ m.box.position.x + 12.0f, m.box.position.y + 40.0f });
        ui::drawText(window, tTitle);

        // Detail explanation
        sf::Text tDetail(font, toUtf8(m.detail), fontsize::Label);
        tDetail.setFillColor(theme::TextPrimary);
        tDetail.setPosition({ m.box.position.x + 12.0f, m.box.position.y + m.detailY });
        ui::drawText(window, tDetail);

        if (!m.tip.empty()) {
            sf::Text tTip(font, toUtf8(m.tip), fontsize::Caption);
            tTip.setFillColor(theme::Warn);
            tTip.setPosition({ m.box.position.x + 12.0f, m.box.position.y + m.tipY });
            ui::drawText(window, tTip);
        }

        // [ OK - РАЗБРАХ ] Button (clickable only by the player who owns the mouse)
        bool btnHover = (mouseOwnerAt(mousePos) == pIdx) && m.okBtn.contains(mousePos);
        sf::RectangleShape btn(m.okBtn.size);
        btn.setPosition(m.okBtn.position);
        btn.setFillColor(theme::GoodFill);
        btn.setOutlineThickness(btnHover ? 2.0f : 1.5f);
        btn.setOutlineColor(btnHover ? theme::Focus : theme::Good);
        window.draw(btn);

        sf::Text tOk(font, toUtf8(pIdx == 1 ? "РАЗБРАХ [SPACE]" : "РАЗБРАХ [ENTER]"), fontsize::Label);
        tOk.setStyle(sf::Text::Bold);
        tOk.setFillColor(theme::TextPrimary);
        sf::FloatRect ob = tOk.getLocalBounds();
        tOk.setPosition({ m.okBtn.position.x + (m.okBtn.size.x - ob.size.x) / 2.0f - ob.position.x,
                          m.okBtn.position.y + (m.okBtn.size.y - ob.size.y) / 2.0f - ob.position.y });
        ui::drawText(window, tOk, m.okBtn);
    };

    drawOneModal(p1Modal, 1);
    drawOneModal(p2Modal, 2);
}

void UI_map::drawMiningZonesAndBadges(sf::RenderWindow& window) {
    ResourceType p1Res = nodes.getP1ResourceAt(p1Pos);
    ResourceType p2Res = nodes.getP2ResourceAt(p2Pos);

    // Mining prompt above the cursor's name tag: resource icon, station, current yield and key,
    // or the remaining cooldown with a progress bar
    auto drawPrompt = [&](int player, sf::Vector2f pos, ResourceType res, const std::string& keyStr, float cd) {
        if (!resourcesLoaded) return;
        const auto* st = nodes.getStation(player, res);
        const int yield = UI_resourceNodes::mineYield(res, engine.getMineLevel(player, res));
        const bool cooling = cd > 0.05f;
        std::string name = st ? st->nameBg : std::string("ДОБИВ");
        std::string promptText;
        if (cooling) {
            char buf[48];
            std::snprintf(buf, sizeof(buf), " · изчакайте %.1fс", static_cast<double>(cd));
            promptText = name + buf;
        } else {
            promptText = name + " +" + std::to_string(yield) + " " + resourceNameBg(res) + " · " + keyStr;
        }

        sf::Text t(font, toUtf8(promptText), fontsize::Label);
        t.setFillColor(cooling ? theme::Warn : theme::TextPrimary);
        sf::FloatRect tb = t.getLocalBounds();
        const float iconSize = 16.0f;
        const float w = tb.size.x + iconSize + 22.0f;
        const float h = 24.0f;
        float x = std::max(4.0f, std::min(pos.x - w / 2.0f, VIRTUAL_WIDTH - w - 4.0f));
        float y = std::max(4.0f, pos.y - 74.0f);

        sf::RectangleShape tagBox({ w, h });
        tagBox.setPosition({ x, y });
        tagBox.setFillColor(theme::withAlpha(theme::Panel, 240));
        tagBox.setOutlineThickness(1.5f);
        tagBox.setOutlineColor(cooling ? theme::Warn : theme::player(player));
        window.draw(tagBox);
        const sf::FloatRect tagRect({ x, y }, { w, h });
        ui::lint::occlude(tagRect);

        drawResourceIcon(window, res, { x + 6.0f + iconSize / 2.0f, y + h / 2.0f }, iconSize);
        t.setPosition({ x + iconSize + 14.0f - tb.position.x, y + (h - tb.size.y) / 2.0f - tb.position.y });
        ui::drawText(window, t, tagRect);

        if (cooling) {
            float fillRatio = 1.0f - std::max(0.0f, std::min(1.0f, cd / Balance::MINE_COOLDOWN_SEC));
            sf::RectangleShape cdBar({ (w - 4.0f) * fillRatio, 3.0f });
            cdBar.setPosition({ x + 2.0f, y + h - 4.0f });
            cdBar.setFillColor(theme::Good);
            window.draw(cdBar);
        }
    };

    if (p1Res != ResourceType::NONE) drawPrompt(1, p1Pos, p1Res, "[SPACE]", p1ResourceCooldown);
    if (p2Res != ResourceType::NONE) drawPrompt(2, p2Pos, p2Res, "[ENTER]", p2ResourceCooldown);

    // The 6x mining speed-up is shown inside each player's clock card (UI_clock)
}
