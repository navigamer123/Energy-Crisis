#include "../includes/UI_map.h"
#include "../includes/UI_text.h"
#include "../includes/UI_shot.h"
#include "../includes/UI_theme.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>

// =============================================================================
// UI_map Screen Overlays (HUD, Help Overlay, Pause Menu, Victory Screen)
// =============================================================================

void UI_map::drawHUD(sf::RenderWindow& window) {
    sf::Vector2f mousePos = ui::pointerPos(window);

    // 1. Menu Button
    sf::FloatRect menuBtn({ 1600.0f - 130.0f, 900.0f - 34.0f }, { 120.0f, 28.0f });
    bool hoverMenu = menuBtn.contains(mousePos);
    sf::RectangleShape mBox(menuBtn.size);
    mBox.setPosition(menuBtn.position);
    mBox.setFillColor(hoverMenu ? theme::ButtonHover : theme::Button);
    mBox.setOutlineThickness(hoverMenu ? 1.5f : 1.0f);
    mBox.setOutlineColor(hoverMenu ? theme::Focus : theme::LineStrong);
    window.draw(mBox);

    // 2. Fullscreen Button [ ⛶ ЦЯЛ ЕКРАН (F11) ]
    sf::FloatRect fsBtn({ 1600.0f - 275.0f, 900.0f - 34.0f }, { 135.0f, 28.0f });
    bool hoverFs = fsBtn.contains(mousePos);
    sf::RectangleShape fsBox(fsBtn.size);
    fsBox.setPosition(fsBtn.position);
    fsBox.setFillColor(hoverFs ? theme::ButtonHover : theme::Button);
    fsBox.setOutlineThickness(hoverFs ? 1.5f : 1.0f);
    fsBox.setOutlineColor(hoverFs ? theme::Focus : theme::LineStrong);
    window.draw(fsBox);

    // 3. Help Button [ ? ПОМОЩ (H) ]
    sf::FloatRect helpBtn({ 1600.0f - 405.0f, 900.0f - 34.0f }, { 120.0f, 28.0f });
    bool hoverHelp = helpBtn.contains(mousePos);
    sf::RectangleShape hBox(helpBtn.size);
    hBox.setPosition(helpBtn.position);
    hBox.setFillColor(hoverHelp ? theme::ButtonHover : theme::Button);
    hBox.setOutlineThickness(hoverHelp ? 1.5f : 1.0f);
    hBox.setOutlineColor(hoverHelp ? theme::Focus : theme::LineStrong);
    window.draw(hBox);

    if (resourcesLoaded) {
        sf::Text mt(font, toUtf8("ESC / МЕНЮ"), fontsize::Label);
        mt.setStyle(sf::Text::Bold);
        mt.setFillColor(theme::TextPrimary);
        sf::FloatRect mb = mt.getLocalBounds();
        mt.setPosition({ menuBtn.position.x + (menuBtn.size.x - mb.size.x) / 2.0f, menuBtn.position.y + 5.0f });
        ui::drawText(window, mt, menuBtn);

        sf::Text fst(font, toUtf8("ЦЯЛ ЕКРАН (F11)"), fontsize::Label);
        fst.setStyle(sf::Text::Bold);
        fst.setFillColor(theme::TextPrimary);
        sf::FloatRect fsb = fst.getLocalBounds();
        fst.setPosition({ fsBtn.position.x + (fsBtn.size.x - fsb.size.x) / 2.0f, fsBtn.position.y + 6.0f });
        ui::drawText(window, fst, fsBtn);

        sf::Text htBtn(font, toUtf8("? ПОМОЩ (H)"), fontsize::Label);
        htBtn.setStyle(sf::Text::Bold);
        htBtn.setFillColor(theme::TextPrimary);
        sf::FloatRect htbBtn = htBtn.getLocalBounds();
        htBtn.setPosition({ helpBtn.position.x + (helpBtn.size.x - htbBtn.size.x) / 2.0f, helpBtn.position.y + 5.0f });
        ui::drawText(window, htBtn, helpBtn);

        // Persistent Controls Reminder Bar
        sf::RectangleShape helpBar({ 930.0f, 26.0f });
        helpBar.setPosition({ 250.0f, 900.0f - 30.0f });
        helpBar.setFillColor(theme::withAlpha(theme::Panel, 225));
        helpBar.setOutlineThickness(1.0f);
        helpBar.setOutlineColor(theme::Line);
        window.draw(helpBar);

        // Q / PgUp step back through buildings; X / Del cancel a selection (or enter demolish mode)
        std::string helpText = "P1: [E]/[Q] Сграда | [X] Разруши/Отказ | [SPACE/Клик] Действие  ///  P2: [PgDn]/[PgUp] Сграда | [Del] Разруши/Отказ | [ENTER] Действие";
        sf::Text ht(font, toUtf8(helpText), fontsize::Caption);
        ht.setFillColor(theme::TextSecondary);
        sf::FloatRect htb = ht.getLocalBounds();
        ht.setPosition({ 250.0f + (930.0f - htb.size.x) / 2.0f, 900.0f - 26.0f });
        ui::drawText(window, ht, sf::FloatRect(helpBar.getPosition(), helpBar.getSize()));
    }
}

void UI_map::drawHelpOverlay(sf::RenderWindow& window) {
    if (!showHelpOverlay) return;

    // Dim backdrop
    sf::RectangleShape backdrop({ VIRTUAL_WIDTH, VIRTUAL_HEIGHT });
    backdrop.setPosition({ 0.0f, 0.0f });
    backdrop.setFillColor(theme::withAlpha(theme::Dim, 215));
    window.draw(backdrop);
    ui::lint::occlude(sf::FloatRect({ 0.0f, 0.0f }, { VIRTUAL_WIDTH, VIRTUAL_HEIGHT }));

    // Dialog card
    sf::FloatRect card({ 220.0f, 80.0f }, { 1160.0f, 740.0f });
    sf::RectangleShape cardBox(card.size);
    cardBox.setPosition(card.position);
    cardBox.setFillColor(theme::withAlpha(theme::Panel, 250));
    cardBox.setOutlineThickness(2.5f);
    cardBox.setOutlineColor(theme::LineStrong);
    window.draw(cardBox);
    ui::lint::ContainerScope cardScope(card);

    // Header strip
    sf::RectangleShape headerStrip({ card.size.x, 52.0f });
    headerStrip.setPosition(card.position);
    headerStrip.setFillColor(theme::PanelHeader);
    window.draw(headerStrip);

    if (resourcesLoaded) {
        // Title
        sf::Text title(font, toUtf8("НАРЪЧНИК: ПРАВИЛА И УПРАВЛЕНИЕ"), fontsize::H1);
        title.setStyle(sf::Text::Bold);
        title.setFillColor(theme::TextPrimary);
        title.setPosition({ card.position.x + 25.0f, card.position.y + 12.0f });
        ui::drawText(window, title);

        // Close button at top right
        sf::FloatRect closeBtn({ card.position.x + card.size.x - 170.0f, card.position.y + 11.0f }, { 150.0f, 30.0f });
        sf::Vector2f mPos = ui::pointerPos(window);
        bool hClose = closeBtn.contains(mPos);

        sf::RectangleShape cb(closeBtn.size);
        cb.setPosition(closeBtn.position);
        cb.setFillColor(theme::BadFill);
        cb.setOutlineThickness(hClose ? 2.0f : 1.0f);
        cb.setOutlineColor(hClose ? theme::Focus : theme::Bad);
        window.draw(cb);

        sf::Text cbText(font, toUtf8("ЗАТВОРИ [H]"), fontsize::Label);
        cbText.setStyle(sf::Text::Bold);
        cbText.setFillColor(theme::TextPrimary);
        sf::FloatRect cbb = cbText.getLocalBounds();
        cbText.setPosition({ closeBtn.position.x + (closeBtn.size.x - cbb.size.x) / 2.0f, closeBtn.position.y + 6.0f });
        ui::drawText(window, cbText, closeBtn);

        // Content Sections
        float y = card.position.y + 70.0f;
        auto drawSection = [&](const std::string& h, const std::string& body, sf::Color accent) {
            sf::Text st(font, toUtf8(h), fontsize::H2);
            st.setStyle(sf::Text::Bold);
            st.setFillColor(accent);
            st.setPosition({ card.position.x + 35.0f, y });
            ui::drawText(window, st);
            y += 24.0f;

            sf::Text bt(font, toUtf8(body), fontsize::Label);
            bt.setFillColor(theme::TextPrimary);
            bt.setLineSpacing(1.25f);
            bt.setPosition({ card.position.x + 45.0f, y });
            ui::drawText(window, bt);
            y += bt.getLocalBounds().size.y + 24.0f;
        };

        const std::string victoryPctStr = std::to_string(static_cast<int>(std::lround(Balance::VICTORY_SHARE * 100.0f)));
        const std::string finalDayStr = std::to_string(static_cast<int>(Balance::FINAL_DAY));
        const std::string speedupStr = std::to_string(static_cast<int>(std::lround(Balance::MINE_SPEEDUP_MULT)));

        drawSection("1. ЦЕЛ НА ИГРАТА И ДОМИНИРАНЕ НА ГРАДА",
                    "- Всеки играч започва с начален свободен парцел и 50% териториален дял в града.\n"
                    "- Първите 2 дена са ГРАТИСЕН ПЕРИОД: Градът иска 0 MW за спокойно първоначално развитие!\n"
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

        drawSection("4. УПРАВЛЕНИЕ И БЪРЗИ КЛАВИШИ",
                    "- ИГРАЧ 1 (Запад/Син): [W/A/S/D] - Движение  |  [SPACE/Клик] - Строеж/Добив  |  [E]/[Q] или [1-6] - Сграда  |  [F] - Ъпгрейд мина  |  [X] - Разруши\n"
                    "- ИГРАЧ 2 (Изток/Розов): [Стрелки] - Движение | [ENTER/Клик] - Строеж/Добив | [PgDn]/[PgUp] или [Num1-6] - Сграда | [RShift/End] - Ъпгрейд | [Del] - Разруши\n"
                    "- СИСТЕМНИ: [ESC] - Меню Пауза (там [R] - Нова игра, [M] - Главно меню)  |  [H]/[F1] - Помощ  |  [F11] - Цял екран\n"
                    "- ИНФОРМАЦИЯ: задръжте [Tab] - Енергийно табло  |  [L] в паузата - Дневник на събитията  |  [F3] - Панел за разработчици", // team info
                    theme::Good);
    }
}

void UI_map::drawVictoryScreen(sf::RenderWindow& window) {
    // team info (F-04): the post-match report (awards, P1/P2 table, charts, energy mix) replaces the
    // old victory box. It also publishes victoryRestartBtn / victoryMenuBtn for the click handling.
    sf::Vector2f mousePos = ui::pointerPos(window);
    postMatch.draw(window, font, resourcesLoaded, engine, stats, mousePos, victoryRestartBtn, victoryMenuBtn);
}

void UI_map::drawPauseMenu(sf::RenderWindow& window) {
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

    // 2. Pause Card (team info: one row taller for the event log entry)
    float boxW = 500.0f;
    float boxH = 433.0f; // 5 entries + an even bottom margin
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
    sf::RectangleShape header({ boxW, 56.0f });
    header.setPosition({ boxX, boxY });
    header.setFillColor(theme::PanelHeader);
    window.draw(header);

    sf::RectangleShape glowLine({ boxW, 3.0f });
    glowLine.setPosition({ boxX, boxY + 56.0f });
    glowLine.setFillColor(theme::LineStrong);
    window.draw(glowLine);

    if (resourcesLoaded) {
        sf::Text tHeader(font, toUtf8("ПАУЗА"), fontsize::H1);
        tHeader.setStyle(sf::Text::Bold);
        tHeader.setFillColor(theme::TextPrimary);
        sf::FloatRect hb = tHeader.getLocalBounds();
        tHeader.setPosition({ boxX + (boxW - hb.size.x) / 2.0f, boxY + 16.0f });
        ui::drawText(window, tHeader);

        sf::Text tSub(font, toUtf8("Използвайте [Стрелки] / [Enter] или мишката за избор"), fontsize::Label);
        tSub.setFillColor(theme::TextSecondary);
        sf::FloatRect sb = tSub.getLocalBounds();
        tSub.setPosition({ boxX + (boxW - sb.size.x) / 2.0f, boxY + 70.0f });
        ui::drawText(window, tSub);
    }

    // 4 Menu Options: rects are computed and boxes drawn even without a font, so they stay clickable
    float btnW = 390.0f;
    float btnH = 52.0f;
    float btnX = boxX + (boxW - btnW) / 2.0f;
    float startY = boxY + 105.0f;
    float spacing = 62.0f;

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

    PauseOption opts[5] = {
        { pauseResumeBtn,  "ПРОДЪЛЖИ  [ESC / ENTER]",    theme::GoodFill, theme::GoodFill, theme::Good },
        { pauseRestartBtn, "НОВА ИГРА  [R]",             theme::InfoFill, theme::InfoFill, theme::Info },
        { pauseHelpBtn,    "ПОМОЩ И ПРАВИЛА  [H]",       theme::Button, theme::ButtonHover, theme::LineStrong },
        { pauseLogBtn,     "ДНЕВНИК НА СЪБИТИЯТА  [L]",  theme::Button, theme::ButtonHover, theme::LineStrong }, // team info
        { pauseMenuBtn,    "ГЛАВНО МЕНЮ  [M]",           theme::BadFill, theme::BadFill, theme::Bad }
    };

    for (int i = 0; i < 5; i++) {
        if (mouseMoved && opts[i].bounds.contains(mousePos)) {
            pauseSelectedIdx = i;
        }
        bool isSel = (pauseSelectedIdx == i);

        sf::RectangleShape bShape(opts[i].bounds.size);
        bShape.setPosition(opts[i].bounds.position);
        bShape.setFillColor(isSel ? opts[i].hoverColor : opts[i].normalColor);
        bShape.setOutlineThickness(isSel ? 2.5f : 1.0f);
        bShape.setOutlineColor(isSel ? theme::Focus : opts[i].outlineColor);
        window.draw(bShape);

        if (resourcesLoaded) {
            sf::Text tBtn(font, toUtf8(opts[i].label), fontsize::Body);
            tBtn.setStyle(sf::Text::Bold);
            tBtn.setFillColor(theme::TextPrimary);
            sf::FloatRect bb = tBtn.getLocalBounds();
            tBtn.setPosition({ opts[i].bounds.position.x + (opts[i].bounds.size.x - bb.size.x) / 2.0f,
                               opts[i].bounds.position.y + (opts[i].bounds.size.y - bb.size.y) / 2.0f - 2.0f });
            ui::drawText(window, tBtn, opts[i].bounds);
        }
    }
}
