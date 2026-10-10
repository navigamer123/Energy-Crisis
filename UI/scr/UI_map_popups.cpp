#include "../includes/UI_map.h"
#include "../includes/UI_text.h"
#include "../includes/UI_shot.h"
#include "../includes/UI_theme.h"
#include "../includes/UI_icons.h"
#include "../includes/UI_arcadeMode.h"
#include "../includes/UI_settings.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

// =============================================================================
// UI_map Popups, Notices, Interactive Modals & Mining Prompts
// =============================================================================

void UI_map::spawnNotice(const std::string& text, sf::Vector2f pos, sf::Color color) {
    std::string finalText = text;
    if (UI_settings::get().getLanguage() == "en") {
        if (finalText == "ЗАКУПЕН ПАРЦЕЛ!") finalText = "PLOT PURCHASED!";
        else if (finalText == "ПОСТРОЕНА СГРАДА!") finalText = "BUILDING COMPLETE!";
        else if (finalText == "РЕЖИМ ДОБИВ") finalText = "GATHER MODE";
        else if (finalText == "НЕЗАКУПЕНА ТЕРИТОРИЯ!") finalText = "UNPURCHASED LAND!";
        else if (finalText == "МЪЛНИЯ УДАРИ СЪОРЪЖЕНИЕТО!") finalText = "LIGHTNING STRUCK FACILITY!";
        else if (finalText == "СЪОРЪЖЕНИЕТО Е УНИЩОЖЕНО!") finalText = "FACILITY DESTROYED!";
        else if (finalText.find("ИЗБРАНА СГРАДА: ") == 0) {
            std::string sub = finalText.substr(std::string("ИЗБРАНА СГРАДА: ").length());
            if (sub == "Слънчев панел") sub = "Solar Panel";
            else if (sub == "Вятърна мелница") sub = "Wind Turbine";
            else if (sub == "ВЕЦ / Хидро") sub = "Hydro Plant";
            else if (sub == "Батерия") sub = "Battery Storage";
            else if (sub == "Осветителна лампа") sub = "Work Lamp";
            finalText = "SELECTED: " + sub;
        } else if (finalText.find("ИЗЧАКАЙТЕ: ") == 0) {
            finalText = "WAIT: " + finalText.substr(std::string("ИЗЧАКАЙТЕ: ").length());
        }
    }
    FloatingNotice n;
    n.text = finalText;
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
        sf::Text& t = ui::pooledText(font, toUtf8(n.text), fontsize::Body);
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
    if (badge == "ДОБИВ" || badge == "GATHER") return;
    ToastPriority prio = ToastPriority::INFO;
    std::string channel = badge;
    if (badge == "МЪЛНИЯ!" || badge == "LIGHTNING!") {
        prio = ToastPriority::CRITICAL;
        channel.clear();
    } else if (badge == "ГРЕШКА" || badge == "ERROR" || badge.rfind("НЕДОСТИГ", 0) == 0 || badge.rfind("NOT ENOUGH", 0) == 0) {
        prio = ToastPriority::WARNING;
        channel = "error";
        fx.onPlayerError(player); // [b-effects] error popups buzz for that player
    } else if (badge == "СТРОЕЖ" || badge == "BUILD" || badge == "ПРЕМАХВАНЕ" || badge == "DEMOLISH" ||
               badge == "ОСВЕТЛЕНИЕ" || badge == "LIGHTING" || badge == "ОТКАЗ" || badge == "CANCELLED") {
        channel = "select";
    }

    std::string finalBadge = badge;
    std::string finalTitle = title;
    std::string finalDetail = detail;
    std::string finalAction = action;

    if (UI_settings::get().getLanguage() == "en") {
        auto translateStr = [](const std::string& in) -> std::string {
            if (in == "СТРОЕЖ") return "BUILD";
            if (in == "ЗЕМЯ") return "LAND";
            if (in == "ПРЕМАХВАНЕ") return "DEMOLISH";
            if (in == "ОСВЕТЛЕНИЕ") return "LIGHTING";
            if (in == "ОТКАЗ") return "CANCELLED";
            if (in == "УСПЕХ") return "SUCCESS";
            if (in == "ГРЕШКА") return "ERROR";
            if (in == "ИНФО") return "INFO";
            if (in == "НАДГРАЖДАНЕ") return "UPGRADE";
            if (in == "НЕЗАКУПЕНА ТЕРИТОРИЯ") return "UNOWNED PLOT";
            if (in == "НЕДОСТИГ НА ПАРИ") return "NOT ENOUGH MONEY";
            if (in == "НЕДОСТИГ НА РЕСУРСИ") return "NOT ENOUGH RESOURCES";
            if (in == "Закупен парцел!") return "Plot purchased!";
            if (in == "Купихте парцел!") return "Plot purchased!";
            if (in == "Разширена земя!") return "Territory expanded!";
            if (in == "Ваш парцел") return "Your plot";
            if (in == "Действието е успешно!") return "Action successful!";
            if (in == "Земята не е закупена!") return "Plot is unpurchased!";
            if (in == "Изборът е прекратен") return "Selection cancelled";
            if (in == "Режим Разрушаване") return "Demolish Mode";
            if (in == "Отменен строеж") return "Placement cancelled";
            if (in == "Слънчев панел") return "Solar Panel";
            if (in == "Вятърна мелница") return "Wind Turbine";
            if (in == "ВЕЦ / Хидро") return "Hydro Plant";
            if (in == "Батерия") return "Battery Storage";
            if (in == "Осветителна лампа") return "Work Lamp";
            return in;
        };
        finalBadge = translateStr(finalBadge);
        finalTitle = translateStr(finalTitle);

        if (finalDetail == "Парцелът е ваш. Вече можете да строите върху него.") finalDetail = "The plot is yours. You can now build on it.";
        else if (finalDetail == "Не може да строите върху незакупена земя.") finalDetail = "You cannot build on unowned land.";
        else if (finalDetail == "Земята е свободна за строителство.") finalDetail = "Land is ready for construction.";
        else if (finalDetail == "Свободен режим.") finalDetail = "Free mode.";
        else if (finalDetail == "Посочете сградата, която искате да махнете.") finalDetail = "Select the building you want to demolish.";
        else if (finalDetail == "Режимът за поставяне е прекратен.") finalDetail = "Placement mode has been cancelled.";
        else if (finalDetail.find("Кликнете върху ваша сграда за разрушаване.\nВръща 50% от ресурсите.") != std::string::npos)
            finalDetail = "Click on your building to demolish.\nRefunds 50% of construction resources.";

        if (finalAction == "[SPACE НА ЗЕМЯ]: Постави | [E]: Друга сграда | [X]: Отказ")
            finalAction = "[SPACE ON LAND]: Place | [E]: Cycle | [X]: Cancel";
        else if (finalAction == "[ENTER НА ЗЕМЯ]: Постави | [PgDn]: Друга сграда | [Del]: Отказ")
            finalAction = "[ENTER ON LAND]: Place | [PgDn]: Cycle | [Del]: Cancel";
        else if (finalAction == "[E]: Постави отново същата")
            finalAction = "[E]: Place another";
        else if (finalAction == "[PgDn]: Постави отново същата")
            finalAction = "[PgDn]: Place another";
        else if (finalAction == "[A]: Постави сграда")
            finalAction = "[A]: Place building";
        else if (finalAction == "[SPACE]: Постави сграда")
            finalAction = "[SPACE]: Place building";
        else if (finalAction == "[ENTER]: Постави сграда")
            finalAction = "[ENTER]: Place building";
        else if (finalAction == "[КЛИК]: Постави сграда")
            finalAction = "[CLICK]: Place building";
        else if (finalAction == "[E]: Избери сграда")
            finalAction = "[E]: Choose building";
        else if (finalAction == "[PgDn]: Избери сграда")
            finalAction = "[PgDn]: Choose building";
        else if (finalAction == "[E]: Изберете сграда за строеж")
            finalAction = "[E]: Choose building";
        else if (finalAction == "[PgDn]: Изберете сграда за строеж")
            finalAction = "[PgDn]: Choose building";
    }

    notifications.push(player, prio, channel, finalBadge, finalTitle, finalDetail, finalAction, accent, false);
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
    fx.onPlayerError(player); // [b-effects] every modal is a refusal: build denied / not enough gold
    PlayerModalDialog& m = (player == 1) ? p1Modal : p2Modal;
    m.active = true;
    m.accentColor = accent;

    std::string modalBadge = badge;
    std::string modalTitle = title;
    std::string modalDetail = detail;
    std::string modalTip = tip;
    bool isEn = (UI_settings::get().getLanguage() == "en");

    if (isEn) {
        if (modalBadge == "НЕДОСТИГ НА ПАРИ") modalBadge = "NOT ENOUGH MONEY";
        else if (modalBadge == "НЕДОСТИГ НА РЕСУРСИ") modalBadge = "NOT ENOUGH RESOURCES";
        else if (modalBadge == "ГРЕШКА ПРИ СТРОЕЖ") modalBadge = "BUILD ERROR";
        if (modalTitle == "Не можете да купите парцела!") modalTitle = "Cannot purchase plot!";
        if (modalDetail.find("НЕДОСТИГ НА ПАРИ! НУЖНО: ") == 0) {
            modalDetail = "NOT ENOUGH MONEY! NEED: " + modalDetail.substr(std::string("НЕДОСТИГ НА ПАРИ! НУЖНО: ").length());
        } else if (modalDetail == "ТОЗИ ПАРЦЕЛ ВЕЧЕ Е ЗАКУПЕН!") {
            modalDetail = "THIS PLOT IS ALREADY PURCHASED!";
        } else if (modalDetail == "НЕВАЛИДЕН ПАРЦЕЛ!") {
            modalDetail = "INVALID PLOT!";
        } else if (modalDetail == "ВСИЧКИ ПАРЦЕЛИ СА ЗАКУПЕНИ!") {
            modalDetail = "ALL PLOTS PURCHASED!";
        }
        if (modalTip == "Продавайте ток на града за да печелите пари ($)!")
            modalTip = "Sell electricity to the city to earn money ($)!";
    }

    m.badge = modalBadge;

    // Wrap every text to the box and grow the box to fit, so long engine messages stay inside
    const float w = 360.0f;
    const float textW = w - 24.0f;
    auto textHeight = [this](const std::string& s, unsigned int size) {
        if (s.empty() || !resourcesLoaded) return 0.0f;
        sf::Text& t = ui::pooledText(font, toUtf8(s), size);
        return t.getLocalBounds().position.y + t.getLocalBounds().size.y;
    };
    m.title = resourcesLoaded ? ui::wrapText(font, modalTitle, fontsize::Body, textW, true) : modalTitle;
    m.detail = resourcesLoaded ? ui::wrapText(font, modalDetail, fontsize::Label, textW) : modalDetail;
    std::string tipPrefix = isEn ? "TIP: " : "СЪВЕТ: ";
    std::string tipText = modalTip.empty() ? std::string() : tipPrefix + modalTip;
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
    bool isEn = (UI_settings::get().getLanguage() == "en");
    if (sel != BuildingType::DEMOLISH && sel != BuildingType::NONE) {
        BuildingCost cost = engine.getBuildingCost(sel);
        std::string missing = missingResourcesText(engine.getPlayerEconomy(player), cost);
        if (!missing.empty()) {
            bool isArcade = ArcadeMode::isEnabled() || (controlScheme == ControlScheme::DEVHUB_ARCADE);
            std::string key = isArcade ? "[A]" : ((player == 1) ? "[SPACE]" : "[ENTER]");
            std::string tipStr = isEn
                ? (isArcade ? ("Gather them from stations: stand over a station and press " + key + ".")
                            : ("Gather them from stations below: stand over a station and press " + key + " (or click it)."))
                : (isArcade ? ("Добийте ги от станциите: застанете върху станция и натиснете " + key + ".")
                            : ("Добийте ги от станциите долу: застанете върху станция и натиснете " + key + " (или кликнете върху нея)."));
            std::string badge = isEn ? "NOT ENOUGH RESOURCES" : "НЕДОСТИГ НА РЕСУРСИ";
            std::string title = isEn ? ("Missing resources for " + cost.nameEn) : ("Не стигат ресурси за " + cost.nameBg);
            triggerPlayerModal(player, badge, title, missing, tipStr, errorColor);
            return;
        }
    }
    std::string badge = isEn ? "BUILD ERROR" : "ГРЕШКА ПРИ СТРОЕЖ";
    std::string title = isEn ? "Cannot build here!" : "Строежът е невъзможен!";
    std::string tip = isEn
        ? (!engine.isDaylight() ? "Place and power a Work Lamp to build at night!"
                                : "Choose an empty cell on your purchased plot.")
        : (!engine.isDaylight() ? "Поставете и захранете Осветителна лампа за работа нощем!"
                                : "Изберете свободна клетка от ваш закупен парцел.");
    triggerPlayerModal(player, badge, title, engineMsg, tip, errorColor);
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
        sf::Text& tBadge = ui::pooledText(font, toUtf8("! " + m.badge), fontsize::Label);
        tBadge.setStyle(sf::Text::Bold);
        tBadge.setFillColor(m.accentColor);
        tBadge.setPosition({ m.box.position.x + 10.0f, m.box.position.y + 6.0f });
        ui::drawText(window, tBadge);

        // Title
        sf::Text& tTitle = ui::pooledText(font, toUtf8(m.title), fontsize::Body);
        tTitle.setStyle(sf::Text::Bold);
        tTitle.setFillColor(theme::TextPrimary);
        tTitle.setPosition({ m.box.position.x + 12.0f, m.box.position.y + 40.0f });
        ui::drawText(window, tTitle);

        // Detail explanation
        sf::Text& tDetail = ui::pooledText(font, toUtf8(m.detail), fontsize::Label);
        tDetail.setFillColor(theme::TextPrimary);
        tDetail.setPosition({ m.box.position.x + 12.0f, m.box.position.y + m.detailY });
        ui::drawText(window, tDetail);

        if (!m.tip.empty()) {
            sf::Text& tTip = ui::pooledText(font, toUtf8(m.tip), fontsize::Caption);
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

        bool isArcade = ArcadeMode::isEnabled() || (controlScheme == ControlScheme::DEVHUB_ARCADE);
        std::string okLabel = isArcade ? "РАЗБРАХ [A]" : (pIdx == 1 ? "РАЗБРАХ [SPACE]" : "РАЗБРАХ [ENTER]");
        sf::Text& tOk = ui::pooledText(font, toUtf8(okLabel), fontsize::Label);
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

        sf::Text& t = ui::pooledText(font, toUtf8(promptText), fontsize::Label);
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
            float maxCd = Balance::getResourceMineCooldown(res);
            float fillRatio = 1.0f - std::max(0.0f, std::min(1.0f, cd / maxCd));
            sf::RectangleShape cdBar({ (w - 4.0f) * fillRatio, 3.0f });
            cdBar.setPosition({ x + 2.0f, y + h - 4.0f });
            cdBar.setFillColor(theme::Good);
            window.draw(cdBar);
        }
    };

    bool isArcade = ArcadeMode::isEnabled() || (controlScheme == ControlScheme::DEVHUB_ARCADE);
    if (p1Res != ResourceType::NONE) drawPrompt(1, p1Pos, p1Res, isArcade ? "[A]" : "[SPACE]", getP1ResourceCooldown(p1Res));
    if (p2Res != ResourceType::NONE) drawPrompt(2, p2Pos, p2Res, isArcade ? "[A]" : "[ENTER]", getP2ResourceCooldown(p2Res));

    // The 6x mining speed-up is shown inside each player's clock card (UI_clock)
}
