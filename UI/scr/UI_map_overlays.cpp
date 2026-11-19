#include "../includes/UI_map.h"
#include "../includes/UI_text.h"
#include "../includes/UI_shot.h"
#include "../includes/UI_theme.h"
#include "../includes/UI_arcadeMode.h"
#include "../includes/UI_settings.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>

// =============================================================================
// UI_map Screen Overlays (HUD, Help Overlay, Pause Menu, Victory Screen)
// =============================================================================

sf::FloatRect UI_map::hudMenuBtnRect() const {
#if defined(__ANDROID__)
    return sf::FloatRect({ 1600.0f - 10.0f - 145.0f, 900.0f - 44.0f }, { 145.0f, 38.0f });
#else
    return sf::FloatRect({ 1600.0f - 10.0f - 150.0f, 900.0f - 44.0f }, { 150.0f, 38.0f });
#endif
}

sf::FloatRect UI_map::hudFullscreenBtnRect() const {
    sf::FloatRect menu = hudMenuBtnRect();
    return sf::FloatRect({ menu.position.x - 8.0f - 180.0f, 900.0f - 44.0f }, { 180.0f, 38.0f });
}

sf::FloatRect UI_map::hudHelpBtnRect() const {
#if defined(__ANDROID__)
    sf::FloatRect menu = hudMenuBtnRect();
    return sf::FloatRect({ menu.position.x - 8.0f - 145.0f, 900.0f - 44.0f }, { 145.0f, 38.0f });
#else
    sf::FloatRect fs = hudFullscreenBtnRect();
    return sf::FloatRect({ fs.position.x - 8.0f - 145.0f, 900.0f - 44.0f }, { 145.0f, 38.0f });
#endif
}

sf::FloatRect UI_map::helpCardRect() const {
    return sf::FloatRect({ 200.0f, 65.0f }, { 1200.0f, 770.0f });
}

sf::FloatRect UI_map::helpCloseBtnRect() const {
    sf::FloatRect card = helpCardRect();
    return sf::FloatRect({ card.position.x + card.size.x - 190.0f - 16.0f, card.position.y + 9.0f }, { 190.0f, 38.0f });
}

void UI_map::drawHUD(sf::RenderTarget& window) {
    bool isArcade = ArcadeMode::isEnabled() || (controlScheme == ControlScheme::DEVHUB_ARCADE);
    // In Arcade mode, hide bottom mouse buttons and help reminder bar for clean minimal arcade UI
    if (isArcade) {
        return;
    }

    sf::Vector2f mousePos = ui::pointerPos(window);
    bool isEn = (UI_settings::get().getLanguage() == "en");

    auto drawHudBtn = [&](const sf::FloatRect& rect, const std::string& label) {
        bool hover = rect.contains(mousePos);
        sf::RectangleShape box(rect.size);
        box.setPosition(rect.position);
        box.setFillColor(hover ? theme::ButtonHover : theme::Button);
        box.setOutlineThickness(hover ? 2.0f : 1.2f);
        box.setOutlineColor(hover ? theme::Focus : theme::LineStrong);
        window.draw(box);

        if (resourcesLoaded) {
            unsigned int sz = ui::fitTextSize(font, label, 15, fontsize::Caption, rect.size.x - 14.0f, true);
            sf::Text& t = ui::pooledText(font, toUtf8(label), sz);
            t.setStyle(sf::Text::Bold);
            t.setFillColor(theme::TextPrimary);
            sf::FloatRect tb = t.getLocalBounds();
            t.setPosition({ rect.position.x + (rect.size.x - tb.size.x) / 2.0f - tb.position.x,
                            rect.position.y + (rect.size.y - tb.size.y) / 2.0f - tb.position.y });
            ui::drawText(window, t, rect);
        }
    };

    // 1. Menu Button
    sf::FloatRect menuBtn = hudMenuBtnRect();
#if defined(__ANDROID__)
    drawHudBtn(menuBtn, isEn ? "MENU" : "МЕНЮ");
#else
    drawHudBtn(menuBtn, isEn ? "ESC / MENU" : "ESC / МЕНЮ");
#endif

#if !defined(__ANDROID__)
    // 2. Fullscreen Button [ FULLSCREEN (F11) ] - not needed on mobile Android
    sf::FloatRect fsBtn = hudFullscreenBtnRect();
    drawHudBtn(fsBtn, isEn ? "FULLSCREEN (F11)" : "ЦЯЛ ЕКРАН (F11)");
#endif

    // 3. Help Button
    sf::FloatRect helpBtn = hudHelpBtnRect();
#if defined(__ANDROID__)
    drawHudBtn(helpBtn, isEn ? "? HELP" : "? ПОМОЩ");
#else
    drawHudBtn(helpBtn, isEn ? "? HELP (H)" : "? ПОМОЩ (H)");
#endif

    if (resourcesLoaded && !tutorial.isActive()) {
        // Persistent Controls Reminder Bar cleanly positioned between P1 quarter circle (x=250) and helpBtn
        const float barX = 258.0f;
        const float barW = helpBtn.position.x - barX - 8.0f;
        const float barH = 34.0f;
        const float barY = 900.0f - 42.0f;

        sf::RectangleShape helpBar({ barW, barH });
        helpBar.setPosition({ barX, barY });
        helpBar.setFillColor(theme::withAlpha(theme::Panel, 228));
        helpBar.setOutlineThickness(1.0f);
        helpBar.setOutlineColor(theme::Line);
        window.draw(helpBar);

#if defined(__ANDROID__)
        std::string helpText = isEn
            ? "Tap station: Gather  |  Tap card: Select  |  Tap plot: Place  |  Two-finger tap: Cancel"
            : "Докоснете станция: Добив  |  Докоснете карта: Избор  |  Докоснете парцел: Постави  |  2 пръста: Отказ";
#else
        std::string helpText = isEn
            ? "P1: [E]/[Q] Building | [X] Demolish/Cancel | [SPACE/Click] Action  ///  P2: [PgDn]/[PgUp] Building | [Del] Demolish/Cancel | [ENTER] Action"
            : "P1: [E]/[Q] Сграда | [X] Разруши/Отказ | [SPACE/Клик] Действие  ///  P2: [PgDn]/[PgUp] Сграда | [Del] Разруши/Отказ | [ENTER] Действие";
        if (ui::measureText(font, helpText, fontsize::Caption, false) > barW - 14.0f) {
            helpText = isEn
                ? "P1: [E]/[Q] Build | [X] Cancel | [SPACE] Act  ///  P2: [PgDn]/[PgUp] Build | [Del] Cancel | [ENTER] Act"
                : "P1: [E]/[Q] Сграда | [X] Отказ | [SPACE] Действие  |  P2: [PgDn]/[PgUp] Сграда | [Del] Отказ | [ENTER] Действие";
        }
#endif
        unsigned int helpSize = ui::fitTextSize(font, helpText, fontsize::Label, fontsize::Caption, barW - 16.0f, false);
        sf::Text& ht = ui::pooledText(font, toUtf8(helpText), helpSize);
        ht.setFillColor(theme::TextSecondary);
        sf::FloatRect htb = ht.getLocalBounds();
        ht.setPosition({ barX + (barW - htb.size.x) / 2.0f - htb.position.x,
                         barY + (barH - htb.size.y) / 2.0f - htb.position.y });
        ui::drawText(window, ht, sf::FloatRect(helpBar.getPosition(), helpBar.getSize()));
    }
}

void UI_map::drawHelpOverlay(sf::RenderTarget& window) {
    if (!showHelpOverlay) return;

    // Dim backdrop
    sf::RectangleShape backdrop({ VIRTUAL_WIDTH, VIRTUAL_HEIGHT });
    backdrop.setPosition({ 0.0f, 0.0f });
    backdrop.setFillColor(theme::withAlpha(theme::Dim, 215));
    window.draw(backdrop);
    ui::lint::occlude(sf::FloatRect({ 0.0f, 0.0f }, { VIRTUAL_WIDTH, VIRTUAL_HEIGHT }));

    // Dialog card
    sf::FloatRect card = helpCardRect();
    sf::RectangleShape cardBox(card.size);
    cardBox.setPosition(card.position);
    cardBox.setFillColor(theme::withAlpha(theme::Panel, 250));
    cardBox.setOutlineThickness(2.5f);
    cardBox.setOutlineColor(theme::LineStrong);
    window.draw(cardBox);
    ui::lint::ContainerScope cardScope(card);

    // Header strip
    const float headerH = 56.0f;
    sf::RectangleShape headerStrip({ card.size.x, headerH });
    headerStrip.setPosition(card.position);
    headerStrip.setFillColor(theme::PanelHeader);
    window.draw(headerStrip);

    if (resourcesLoaded) {
        bool isEn = (UI_settings::get().getLanguage() == "en");
        bool isArcade = ArcadeMode::isEnabled() || (controlScheme == ControlScheme::DEVHUB_ARCADE);

        // Close button at top right
        sf::FloatRect closeBtn = helpCloseBtnRect();
        sf::Vector2f mPos = ui::pointerPos(window);
        bool hClose = closeBtn.contains(mPos);

        // Title
        std::string titleStr = isEn ? "HANDBOOK: RULES & CONTROLS" : "НАРЪЧНИК: ПРАВИЛА И УПРАВЛЕНИЕ";
        float maxTitleW = closeBtn.position.x - card.position.x - 40.0f;
        unsigned int titleSz = ui::fitTextSize(font, titleStr, fontsize::H1, fontsize::H2, maxTitleW, true);
        sf::Text& title = ui::pooledText(font, toUtf8(titleStr), titleSz);
        title.setStyle(sf::Text::Bold);
        title.setFillColor(theme::TextPrimary);
        sf::FloatRect tb = title.getLocalBounds();
        title.setPosition({ card.position.x + 25.0f - tb.position.x,
                            card.position.y + (headerH - tb.size.y) / 2.0f - tb.position.y });
        ui::drawText(window, title);

        sf::RectangleShape cb(closeBtn.size);
        cb.setPosition(closeBtn.position);
        cb.setFillColor(hClose ? sf::Color(172, 54, 68) : theme::BadFill);
        cb.setOutlineThickness(hClose ? 2.0f : 1.2f);
        cb.setOutlineColor(hClose ? theme::Focus : theme::Bad);
        window.draw(cb);

        std::string closeLbl = isEn ? (isArcade ? "CLOSE [BUTTON]" : "CLOSE [H]")
                                    : (isArcade ? "ЗАТВОРИ [БУТОН]" : "ЗАТВОРИ [H]");
        unsigned int closeSz = ui::fitTextSize(font, closeLbl, 15, fontsize::Label, closeBtn.size.x - 16.0f, true);
        sf::Text& cbText = ui::pooledText(font, toUtf8(closeLbl), closeSz);
        cbText.setStyle(sf::Text::Bold);
        cbText.setFillColor(theme::TextPrimary);
        sf::FloatRect cbb = cbText.getLocalBounds();
        cbText.setPosition({ closeBtn.position.x + (closeBtn.size.x - cbb.size.x) / 2.0f - cbb.position.x,
                             closeBtn.position.y + (closeBtn.size.y - cbb.size.y) / 2.0f - cbb.position.y });
        ui::drawText(window, cbText, closeBtn);

        // Content Sections
        float y = card.position.y + headerH + 18.0f;
        const float maxBodyW = card.size.x - 75.0f;
        auto drawSection = [&](const std::string& h, const std::string& body, sf::Color accent) {
            sf::Text& st = ui::pooledText(font, toUtf8(h), fontsize::H2);
            st.setStyle(sf::Text::Bold);
            st.setFillColor(accent);
            st.setPosition({ card.position.x + 35.0f, y });
            ui::drawText(window, st);
            y += 24.0f;

            std::string wrappedBody = ui::wrapText(font, body, fontsize::Label, maxBodyW);
            sf::Text& bt = ui::pooledText(font, toUtf8(wrappedBody), fontsize::Label);
            bt.setFillColor(theme::TextPrimary);
            bt.setLineSpacing(1.22f);
            bt.setPosition({ card.position.x + 45.0f, y });
            ui::drawText(window, bt);
            sf::FloatRect btb = bt.getLocalBounds();
            y += btb.position.y + btb.size.y + 20.0f;
        };

        const std::string victoryPctStr = std::to_string(static_cast<int>(std::lround(Balance::VICTORY_SHARE * 100.0f)));
        const std::string finalDayStr = std::to_string(engine.getConfig().finalDay);
        const int graceDays = engine.getConfig().graceDays;
        const std::string speedupStr = std::to_string(static_cast<int>(std::lround(Balance::MINE_SPEEDUP_MULT)));

        if (isEn) {
            std::string graceLineEn = (graceDays == 1)
                ? "- Day 1 is a GRACE PERIOD: The city requests 0 MW for initial buildup!\n"
                : ("- The first " + std::to_string(graceDays) + " days are a GRACE PERIOD: The city requests 0 MW!\n");
            drawSection("1. GAME OBJECTIVE & CITY DOMINANCE",
                        "- Each player begins with starting land plots and 50% territory share in the city.\n" +
                        graceLineEn +
                        "- Powering the city earns money ($) from contracts and gold dividends (capped to city demand!).\n"
                        "- At the end of each day, average delivered power (MW) is evaluated: majority earns 10-15% territory shift!\n"
                        "- The winner is the first player reaching at least " + victoryPctStr + "% city control. After day " + finalDayStr +
                        ", the larger share wins.",
                        theme::Energy);
            drawSection("2. SEASONAL DAY/NIGHT CYCLE & SOLAR SCHEDULE",
                        "- Spring: 06:00 - 19:00 (13h day) | Summer: 05:00 - 21:00 (16h day, +15% solar power!)\n"
                        "- Autumn: 07:00 - 18:00 (11h day) | Winter: 08:00 - 16:30 (only 8.5h day, snowstorms!)\n"
                        "- Solar panels only generate power between sunrise and sunset of the active season.\n"
                        "- At night, construction requires a Work Lamp, and battery banks discharge stored daylight energy.\n"
                        "- Stormy weather brings lightning strikes in stormy sectors.",
                        theme::Info);
            drawSection("3. RESOURCES & GOLD MINE UPGRADES",
                        "- 7 resources: Wood, Iron, Copper, Coal, Silicon, Silver, and Gold (money comes only from power!).\n"
                        "- Mining stations can be upgraded up to Level 6 with Gold for +75% yield per level!\n"
                        "- Upgrade using the button below the mine or key [F] (P1) / [RShift] (P2) / Joystick Button [C].\n"
                        "- When ALL human players stand on resource stations, time advances " + speedupStr + "x faster.",
                        theme::Gold);
#if defined(__ANDROID__)
            drawSection("4. TOUCH CONTROLS",
                        "- TAP STATION: Gather resource instantly\n"
                        "- TAP BUILDING CARD: Select building to construct\n"
                        "- TAP OWNED PLOT: Place selected building / buy unowned plot\n"
                        "- TAP UPGRADE BUTTON: Upgrade mine yield with Gold\n"
                        "- TWO PLAYERS: Left screen half = Player 1 (West), Right half = Player 2 (East)\n"
                        "- SYSTEM: [MENU] button at bottom right to Pause / Exit to Main Menu",
                        theme::Good);
#else
            drawSection("4. CONTROLS & SHORTCUTS",
                        isArcade ? "- ARCADE CONTROLS: [Joystick] - Move | [A] - Action/Mine | [D] - Building | [C] - Upgrade | [B] - Cancel\n"
                                   "- SYSTEM: [Top CG Button (2x)] - Exit Game | [Top Person Button] - Help Handbook"
                                 : "- PLAYER 1 (West/Blue): [W/A/S/D] - Move | [SPACE/Click] - Build/Mine | [E]/[Q] or [1-6] - Building | [F] - Upgrade | [X] - Demolish\n"
                                   "- PLAYER 2 (East/Pink): [Arrows] - Move | [ENTER/Click] - Build/Mine | [PgDn]/[PgUp] or [Num1-6] - Building | [RShift] - Upgrade | [Del] - Demolish\n"
                                   "- SYSTEM: [ESC] - Pause Menu | [H]/[F1] - Help | [F11] - Fullscreen | [Tab] - Energy Dashboard",
                        theme::Good);
#endif
        } else {
            const std::string graceLine = (graceDays == 1)
                ? "- Ден 1 е ГРАТИСЕН ПЕРИОД: Градът иска 0 MW за спокойно първоначално развитие!\n"
                : ("- Първите " + std::to_string(graceDays) + " дена са ГРАТИСЕН ПЕРИОД: Градът иска 0 MW за спокойно първоначално развитие!\n");
            drawSection("1. ЦЕЛ НА ИГРАТА И ДОМИНИРАНЕ НА ГРАДА",
                        "- Всеки играч започва с начален свободен парцел и 50% териториален дял в града.\n" +
                        graceLine +
                        "- Захранването на града носи пари ($) от договори и златен дивидент (злато, ограничено до нуждите на града!).\n"
                        "- В края на всеки ден се отчита средната доставена мощност (MW) за целия ден: превесът носи 10-15% дневно завладяване!\n"
                        "- Победител е първият играч с поне " + victoryPctStr + "% от града в края на ден. След края на ден " + finalDayStr +
                        " печели по-големият дял (равен дял = равенство).",
                        theme::Energy);

            drawSection("2. СЕЗОНЕН ДЕН/НОЩ ЦИКЪЛ И СЛЪНЧЕВ ГРАФИК",
                        "- Пролет: 06:00 - 19:00 (13ч ден) | Лято: 05:00 - 21:00 (16ч ден, +15% соларна мощност!)\n"
                        "- Есен: 07:00 - 18:00 (11ч ден)    | Зима: 08:00 - 16:30 (само 8.5ч ден, снежни бури!)\n"
                        "- Соларните панели работят единствено между изгрева и залеза на слънцето за съответния сезон.\n"
                        "- Нощем строежът изисква Осветителна лампа, а батериите отдават събраната през деня енергия.\n"
                        "- Бурно време носи мълнии: те падат само в бурния сектор и могат да унищожат съоръжение там (не и в гратисния период).",
                        theme::Info);

            drawSection("3. РЕСУРСИ И ЪПГРЕЙД НА МИНИ С ЗЛАТО",
                        "- 7 суровини: Дърво, Желязо, Мед, Въглища, Силиций, Сребро и Злато (парите са само от ток!).\n"
                        "- Добивните станции се надграждат до Ниво 6 със Злато (30G, 300G, 500G, 800G, 1500G) за +75% добив на ниво!\n"
                        "- Надграждайте с бутона за ниво под мината или клавиш [F] (Играч 1) / [RShift] (Играч 2).\n"
                        "- Когато ВСИЧКИ играчи-хора стоят върху ресурсни станции, денонощието тече " + speedupStr +
                        " пъти по-бързо (ботът не ускорява времето).",
                        theme::Gold);

#if defined(__ANDROID__)
            drawSection("4. СЕНЗОРНО УПРАВЛЕНИЕ",
                        "- ДОКОСВАНЕ НА СТАНЦИЯ: Мигновен добив на ресурс\n"
                        "- ДОКОСВАНЕ НА КАРТА: Избор на сграда за строеж\n"
                        "- ДОКОСВАНЕ НА ПАРЦЕЛ: Поставяне на избраната сграда / покупка на нов парцел\n"
                        "- ДОКОСВАНЕ НА ЪПГРЕЙД: Надграждане на мината със Злато\n"
                        "- ДВАМА ИГРАЧИ: Лява половина = Играч 1 (Запад), Дясна половина = Играч 2 (Изток)\n"
                        "- СИСТЕМНИ: Бутон [МЕНЮ] долу вдясно за Пауза / Изход към Главно меню",
                        theme::Good);
#else
            drawSection("4. УПРАВЛЕНИЕ И БЪРЗИ КЛАВИШИ",
                        isArcade ? "- АРКАДНО УПРАВЛЕНИЕ: [Стик] - Движение | [A] - Действие/Добив | [D] - Сграда | [C] - Ъпгрейд | [B] - Отказ\n"
                                   "- СИСТЕМНИ: [Горен бутон CG (2x)] - Изход от играта | [Горен бутон Човече] - Наръчник/Помощ"
                                 : "- ИГРАЧ 1 (Запад/Син): [W/A/S/D] - Движение  |  [SPACE/Клик] - Строеж/Добив  |  [E]/[Q] или [1-6] - Сграда  |  [F] - Ъпгрейд мина  |  [X] - Разруши\n"
                                   "- ИГРАЧ 2 (Изток/Розов): [Стрелки] - Движение | [ENTER/Клик] - Строеж/Добив | [PgDn]/[PgUp] или [Num1-6] - Сграда | [RShift/End] - Ъпгрейд | [Del] - Разруши\n"
                                   "- СИСТЕМНИ: [ESC] - Меню Пауза (там [R] - Нова игра, [M] - Главно меню)  |  [H]/[F1] - Помощ  |  [F11] - Цял екран\n"
                                   "- ИНФОРМАЦИЯ: задръжте [Tab] - Енергийно табло  |  [L] в паузата - Дневник на събитията  |  [F3] - Панел за разработчици",
                        theme::Good);
#endif
        }
    }
}

void UI_map::drawVictoryScreen(sf::RenderTarget& window) {
    // team info (F-04): the post-match report (awards, P1/P2 table, charts, energy mix) replaces the
    // old victory box. It also publishes victoryRestartBtn / victoryMenuBtn for the click handling.
    sf::Vector2f mousePos = ui::pointerPos(window);
    postMatch.draw(window, font, resourcesLoaded, engine, stats, mousePos, victoryRestartBtn, victoryMenuBtn, matchFinishedTimer);
}

void UI_map::drawPauseMenu(sf::RenderTarget& window) {
    bool isArcade = ArcadeMode::isEnabled() || (controlScheme == ControlScheme::DEVHUB_ARCADE);
    // In arcade mode, pause menu is completely removed; only the arcade popup is shown
    if (isArcade) return;

    sf::Vector2f mousePos = ui::pointerPos(window);

    bool mouseMoved = (std::abs(mousePos.x - lastPauseMousePos.x) > 2.0f ||
                       std::abs(mousePos.y - lastPauseMousePos.y) > 2.0f);
    if (mouseMoved) {
        lastPauseMousePos = mousePos;
    }

    // 1. Frosted dim backdrop
    sf::RectangleShape backdrop({ 1600.0f, 900.0f });
    backdrop.setFillColor(theme::withAlpha(theme::Dim, 215));
    window.draw(backdrop);
    ui::lint::occlude(sf::FloatRect({ 0.0f, 0.0f }, { VIRTUAL_WIDTH, VIRTUAL_HEIGHT }));

    // 2. Pause Card (5 entries + even margins)
    float boxW = 540.0f;
    float boxH = 495.0f;
    float boxX = (1600.0f - boxW) / 2.0f;
    float boxY = (900.0f - boxH) / 2.0f;

    sf::RectangleShape box({ boxW, boxH });
    box.setPosition({ boxX, boxY });
    box.setFillColor(theme::withAlpha(theme::Panel, 252));
    box.setOutlineThickness(3.0f);
    box.setOutlineColor(theme::LineStrong);
    window.draw(box);
    ui::lint::ContainerScope boxScope(sf::FloatRect({ boxX, boxY }, { boxW, boxH }));

    // Header banner
    const float headerH = 58.0f;
    sf::RectangleShape header({ boxW, headerH });
    header.setPosition({ boxX, boxY });
    header.setFillColor(theme::PanelHeader);
    window.draw(header);

    sf::RectangleShape glowLine({ boxW, 3.0f });
    glowLine.setPosition({ boxX, boxY + headerH });
    glowLine.setFillColor(theme::LineStrong);
    window.draw(glowLine);

    if (resourcesLoaded) {
        bool isEn = (UI_settings::get().getLanguage() == "en");
        sf::Text& tHeader = ui::pooledText(font, toUtf8(isEn ? "PAUSE" : "ПАУЗА"), fontsize::H1);
        tHeader.setStyle(sf::Text::Bold);
        tHeader.setFillColor(theme::TextPrimary);
        sf::FloatRect hb = tHeader.getLocalBounds();
        tHeader.setPosition({ boxX + (boxW - hb.size.x) / 2.0f - hb.position.x,
                              boxY + (headerH - hb.size.y) / 2.0f - hb.position.y });
        ui::drawText(window, tHeader);

#if defined(__ANDROID__)
        std::string subTxt = isEn ? "Tap an option to select"
                                  : "Докоснете опция за избор";
#else
        std::string subTxt = isEn ? "Use [Arrows] / [Enter] or Mouse to select"
                                  : "Използвайте [Стрелки] / [Enter] или мишката за избор";
#endif
        unsigned int subSz = ui::fitTextSize(font, subTxt, fontsize::Body, fontsize::Caption, boxW - 36.0f, false);
        sf::Text& tSub = ui::pooledText(font, toUtf8(subTxt), subSz);
        tSub.setFillColor(theme::TextSecondary);
        sf::FloatRect sb = tSub.getLocalBounds();
        tSub.setPosition({ boxX + (boxW - sb.size.x) / 2.0f - sb.position.x,
                           boxY + 73.0f - sb.position.y });
        ui::drawText(window, tSub);
    }

    // 5 Menu Options: rects are computed and boxes drawn even without a font, so they stay clickable
    float btnW = 450.0f;
    float btnH = 62.0f;
    float btnX = boxX + (boxW - btnW) / 2.0f;
    float startY = boxY + 106.0f;
    float spacing = 73.0f;

    pauseResumeBtn  = sf::FloatRect({ btnX, startY }, { btnW, btnH });
    pauseRestartBtn = sf::FloatRect({ btnX, startY + spacing }, { btnW, btnH });
    pauseHelpBtn    = sf::FloatRect({ btnX, startY + 2.0f * spacing }, { btnW, btnH });
    pauseLogBtn     = sf::FloatRect({ btnX, startY + 3.0f * spacing }, { btnW, btnH }); // team info
    pauseMenuBtn    = sf::FloatRect({ btnX, startY + 4.0f * spacing }, { btnW, btnH });

    struct PauseOption {
        sf::FloatRect bounds;
        std::string label;
        sf::Color normalColor;
        sf::Color hoverColor;
        sf::Color outlineColor;
    };

    bool isEn = (UI_settings::get().getLanguage() == "en");

#if defined(__ANDROID__)
    std::string resumeLbl  = isEn ? "RESUME" : "ПРОДЪЛЖИ";
    std::string restartLbl = isEn ? "NEW GAME" : "НОВА ИГРА";
    std::string helpLbl    = isEn ? "HELP & RULES" : "ПОМОЩ И ПРАВИЛА";
    std::string logLbl     = isEn ? "EVENT LOG" : "ДНЕВНИК НА СЪБИТИЯТА";
    std::string menuLbl    = isEn ? "MAIN MENU" : "ГЛАВНО МЕНЮ";
#else
    std::string resumeLbl  = isEn ? (isArcade ? "RESUME  [B / A]" : "RESUME  [ESC / ENTER]")
                                  : (isArcade ? "ПРОДЪЛЖИ  [B / A]" : "ПРОДЪЛЖИ  [ESC / ENTER]");
    std::string restartLbl = isEn ? "NEW GAME  [R]" : "НОВА ИГРА  [R]";
    std::string helpLbl    = isEn ? (isArcade ? "HELP & RULES  [CG]" : "HELP & RULES  [H]")
                                  : (isArcade ? "ПОМОЩ И ПРАВИЛА  [CG]" : "ПОМОЩ И ПРАВИЛА  [H]");
    std::string logLbl     = isEn ? "EVENT LOG  [L]" : "ДНЕВНИК НА СЪБИТИЯТА  [L]";
    std::string menuLbl    = isEn ? "MAIN MENU  [M]" : "ГЛАВНО МЕНЮ  [M]";
#endif

    PauseOption opts[5] = {
        { pauseResumeBtn,  resumeLbl,  theme::GoodFill, sf::Color(46, 160, 96),  theme::Good },
        { pauseRestartBtn, restartLbl, theme::InfoFill, sf::Color(50, 116, 188), theme::Info },
        { pauseHelpBtn,    helpLbl,    theme::Button,   theme::ButtonHover,      theme::LineStrong },
        { pauseLogBtn,     logLbl,     theme::Button,   theme::ButtonHover,      theme::LineStrong }, // team info
        { pauseMenuBtn,    menuLbl,    theme::BadFill,  sf::Color(172, 54, 68),  theme::Bad }
    };

    for (int i = 0; i < 5; i++) {
        if (mouseMoved && opts[i].bounds.contains(mousePos)) {
            pauseSelectedIdx = i;
        }
        bool isSel = (pauseSelectedIdx == i);

        sf::RectangleShape bShape(opts[i].bounds.size);
        bShape.setPosition(opts[i].bounds.position);
        bShape.setFillColor(isSel ? opts[i].hoverColor : opts[i].normalColor);
        bShape.setOutlineThickness(isSel ? 2.5f : 1.2f);
        bShape.setOutlineColor(isSel ? theme::Focus : opts[i].outlineColor);
        window.draw(bShape);

        if (resourcesLoaded) {
            unsigned int btnSz = ui::fitTextSize(font, opts[i].label, 18, fontsize::Label, btnW - 28.0f, true);
            sf::Text& tBtn = ui::pooledText(font, toUtf8(opts[i].label), btnSz);
            tBtn.setStyle(sf::Text::Bold);
            tBtn.setFillColor(theme::TextPrimary);
            sf::FloatRect bb = tBtn.getLocalBounds();
            tBtn.setPosition({ opts[i].bounds.position.x + (opts[i].bounds.size.x - bb.size.x) / 2.0f - bb.position.x,
                               opts[i].bounds.position.y + (opts[i].bounds.size.y - bb.size.y) / 2.0f - bb.position.y });
            ui::drawText(window, tBtn, opts[i].bounds);
        }
    }
}
