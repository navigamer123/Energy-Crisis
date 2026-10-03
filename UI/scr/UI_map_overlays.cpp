#include "../includes/UI_map.h"
#include <cmath>
#include <vector>
#include <string>

// =============================================================================
// UI_map Screen Overlays (HUD, Help Overlay, Pause Menu, Victory Screen)
// =============================================================================

void UI_map::drawHUD(sf::RenderWindow& window) {
    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    // 1. Menu Button
    sf::FloatRect menuBtn({ 1600.0f - 130.0f, 900.0f - 34.0f }, { 120.0f, 28.0f });
    bool hoverMenu = menuBtn.contains(mousePos);
    sf::RectangleShape mBox(menuBtn.size);
    mBox.setPosition(menuBtn.position);
    mBox.setFillColor(hoverMenu ? sf::Color(55, 75, 105) : sf::Color(32, 42, 58));
    mBox.setOutlineThickness(hoverMenu ? 1.5f : 1.0f);
    mBox.setOutlineColor(hoverMenu ? sf::Color(255, 204, 0) : sf::Color(80, 110, 150));
    window.draw(mBox);

    // 2. Fullscreen Button [ ⛶ ЦЯЛ ЕКРАН (F11) ]
    sf::FloatRect fsBtn({ 1600.0f - 275.0f, 900.0f - 34.0f }, { 135.0f, 28.0f });
    bool hoverFs = fsBtn.contains(mousePos);
    sf::RectangleShape fsBox(fsBtn.size);
    fsBox.setPosition(fsBtn.position);
    fsBox.setFillColor(hoverFs ? sf::Color(0, 120, 180) : sf::Color(22, 48, 75));
    fsBox.setOutlineThickness(hoverFs ? 1.5f : 1.0f);
    fsBox.setOutlineColor(hoverFs ? sf::Color(0, 229, 255) : sf::Color(60, 100, 145));
    window.draw(fsBox);

    // 3. Help Button [ ? ПОМОЩ (H) ]
    sf::FloatRect helpBtn({ 1600.0f - 405.0f, 900.0f - 34.0f }, { 120.0f, 28.0f });
    bool hoverHelp = helpBtn.contains(mousePos);
    sf::RectangleShape hBox(helpBtn.size);
    hBox.setPosition(helpBtn.position);
    hBox.setFillColor(hoverHelp ? sf::Color(100, 70, 150) : sf::Color(40, 32, 65));
    hBox.setOutlineThickness(hoverHelp ? 1.5f : 1.0f);
    hBox.setOutlineColor(hoverHelp ? sf::Color(220, 150, 255) : sf::Color(90, 75, 130));
    window.draw(hBox);

    if (resourcesLoaded) {
        sf::Text mt(font, toUtf8("ESC / МЕНЮ"), 12);
        mt.setFillColor(hoverMenu ? sf::Color(255, 240, 150) : sf::Color::White);
        sf::FloatRect mb = mt.getLocalBounds();
        mt.setPosition({ menuBtn.position.x + (menuBtn.size.x - mb.size.x) / 2.0f, menuBtn.position.y + 5.0f });
        window.draw(mt);

        sf::Text fst(font, toUtf8("ЦЯЛ ЕКРАН (F11)"), 11);
        fst.setFillColor(hoverFs ? sf::Color::White : sf::Color(180, 235, 255));
        sf::FloatRect fsb = fst.getLocalBounds();
        fst.setPosition({ fsBtn.position.x + (fsBtn.size.x - fsb.size.x) / 2.0f, fsBtn.position.y + 6.0f });
        window.draw(fst);

        sf::Text htBtn(font, toUtf8("? ПОМОЩ (H)"), 12);
        htBtn.setFillColor(hoverHelp ? sf::Color::White : sf::Color(230, 200, 255));
        sf::FloatRect htbBtn = htBtn.getLocalBounds();
        htBtn.setPosition({ helpBtn.position.x + (helpBtn.size.x - htbBtn.size.x) / 2.0f, helpBtn.position.y + 5.0f });
        window.draw(htBtn);

        // Persistent Controls Reminder Bar
        sf::RectangleShape helpBar({ 930.0f, 26.0f });
        helpBar.setPosition({ 250.0f, 900.0f - 30.0f });
        helpBar.setFillColor(sf::Color(15, 20, 30, 220));
        helpBar.setOutlineThickness(1.0f);
        helpBar.setOutlineColor(sf::Color(60, 85, 120));
        window.draw(helpBar);

        std::string helpText = "P1: [E] Сграда | [X] Разруши | [Q] Отказ | [SPACE/Клик] Действие  ///  P2: [PgDn] Сграда | [Del] Разруши | [PgUp] Отказ | [ENTER] Действие";
        sf::Text ht(font, toUtf8(helpText), 11);
        ht.setFillColor(sf::Color(210, 230, 255));
        sf::FloatRect htb = ht.getLocalBounds();
        ht.setPosition({ 250.0f + (930.0f - htb.size.x) / 2.0f, 900.0f - 26.0f });
        window.draw(ht);
    }
}

void UI_map::drawHelpOverlay(sf::RenderWindow& window) {
    if (!showHelpOverlay) return;

    // Dim backdrop
    sf::RectangleShape backdrop({ VIRTUAL_WIDTH, VIRTUAL_HEIGHT });
    backdrop.setPosition({ 0.0f, 0.0f });
    backdrop.setFillColor(sf::Color(5, 10, 18, 205));
    window.draw(backdrop);

    // Dialog card
    sf::FloatRect card({ 220.0f, 80.0f }, { 1160.0f, 740.0f });
    sf::RectangleShape cardBox(card.size);
    cardBox.setPosition(card.position);
    cardBox.setFillColor(sf::Color(14, 22, 36, 250));
    cardBox.setOutlineThickness(2.5f);
    cardBox.setOutlineColor(sf::Color(0, 229, 255, 200));
    window.draw(cardBox);

    // Header strip
    sf::RectangleShape headerStrip({ card.size.x, 52.0f });
    headerStrip.setPosition(card.position);
    headerStrip.setFillColor(sf::Color(22, 35, 58));
    window.draw(headerStrip);

    if (resourcesLoaded) {
        // Title
        sf::Text title(font, toUtf8("НАСТОЛЕН НАРЪЧНИК: ENERGY CRISIS"), 20);
        title.setStyle(sf::Text::Bold);
        title.setFillColor(sf::Color(0, 229, 255));
        title.setPosition({ card.position.x + 25.0f, card.position.y + 12.0f });
        window.draw(title);

        // Close button at top right
        sf::FloatRect closeBtn({ card.position.x + card.size.x - 170.0f, card.position.y + 11.0f }, { 150.0f, 30.0f });
        sf::Vector2f mPos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        bool hClose = closeBtn.contains(mPos);

        sf::RectangleShape cb(closeBtn.size);
        cb.setPosition(closeBtn.position);
        cb.setFillColor(hClose ? sf::Color(255, 75, 75) : sf::Color(180, 50, 50));
        cb.setOutlineThickness(1.0f);
        cb.setOutlineColor(sf::Color::White);
        window.draw(cb);

        sf::Text cbText(font, toUtf8("[X] ЗАТВОРИ (H)"), 12);
        cbText.setFillColor(sf::Color::White);
        sf::FloatRect cbb = cbText.getLocalBounds();
        cbText.setPosition({ closeBtn.position.x + (closeBtn.size.x - cbb.size.x) / 2.0f, closeBtn.position.y + 6.0f });
        window.draw(cbText);

        // Content Sections
        float y = card.position.y + 70.0f;
        auto drawSection = [&](const std::string& h, const std::string& body, sf::Color accent) {
            sf::Text st(font, toUtf8(h), 15);
            st.setStyle(sf::Text::Bold);
            st.setFillColor(accent);
            st.setPosition({ card.position.x + 35.0f, y });
            window.draw(st);
            y += 24.0f;

            sf::Text bt(font, toUtf8(body), 12);
            bt.setFillColor(sf::Color(220, 235, 255));
            bt.setLineSpacing(1.25f);
            bt.setPosition({ card.position.x + 45.0f, y });
            window.draw(bt);
            y += bt.getLocalBounds().size.y + 24.0f;
        };

        drawSection("1. ЦЕЛ НА ИГРАТА И ДОМИНИРАНЕ НА ГРАДА",
                    "- Всеки играч започва с начален свободен парцел и 50% териториален дял в града.\n"
                    "- Първите 2 дена са ГРАТИСЕН ПЕРИОД: Градът иска 0 MW за спокойно първоначално развитие!\n"
                    "- Захранването на града носи пари ($) от договори и златен дивидент (Gold, лимитиран до нуждите на града!).\n"
                    "- В края на всеки ден се отчита доставката: превесът носи максимум 10-15% дневно завладяване за пълен баланс!\n"
                    "- Победител е играчът, който постигне 100% териториален контрол над Метрополиса.",
                    sf::Color(255, 215, 0));

        drawSection("2. СЕЗОНЕН ДЕН/НОЩ ЦИКЪЛ И СЛЪНЧЕВ ГРАФИК",
                    "- Пролет: 06:00 - 19:00 (13ч ден) | Лято: 05:00 - 21:00 (16ч ден, +15% соларна мощност!)\n"
                    "- Есен: 07:00 - 18:00 (11ч ден)    | Зима: 08:00 - 16:30 (само 8.5ч ден, снежни бури!)\n"
                    "- Соларните панели работят единствено между изгрева и залеза на слънцето за съответния сезон.\n"
                    "- Нощем строежът изисква Осветителна лампа, а батериите отдават събраната през деня енергия.",
                    sf::Color(0, 229, 255));

        drawSection("3. РЕСУРСИ И ЪПГРЕЙД НА МИНИ С ЗЛАТО",
                    "- 7 суровини: Дърво, Желязо, Мед, Въглища, Силиций, Сребро и Злато (парите са само от ток!).\n"
                    "- Добивните станции се надграждат до Ниво 6 със Злато (30G, 300G, 500G, 800G, 1500G) за +75% добив на ниво!\n"
                    "- Ъпгрейдвайте с бутона [+1 НИВО] на мината или клавиш [F] (Играч 1) / [RShift] (Играч 2).\n"
                    "- Престоят върху ресурсна станция забързва денонощието 6 пъти за светкавичен добив.",
                    sf::Color(255, 140, 220));

        drawSection("4. УПРАВЛЕНИЕ И БЪРЗИ КЛАВИШИ",
                    "- ИГРАЧ 1 (Запад/Син): [W/A/S/D] - Движение  |  [SPACE/Клик] - Строеж/Добив  |  [E]/[Q] - Сграда  |  [F] - Ъпгрейд мина  |  [X] - Разруши\n"
                    "- ИГРАЧ 2 (Изток/Розов): [Стрелки] - Движение | [ENTER/Клик] - Строеж/Добив | [PgDn]/[PgUp] - Сграда | [RShift/End] - Ъпгрейд | [Del] - Разруши\n"
                    "- СИСТЕМНИ: [ESC] - Меню Пауза  |  [R] - Нова игра  |  [H]/[F1] - Помощ  |  [F11] - Цял екран",
                    sf::Color(100, 255, 150));
    }
}

void UI_map::drawVictoryScreen(sf::RenderWindow& window) {
    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    // 1. Dark frosted backdrop
    sf::RectangleShape backdrop({ 1600.0f, 900.0f });
    backdrop.setFillColor(sf::Color(10, 14, 24, 235));
    window.draw(backdrop);

    int winner = engine.getCityState().winner;
    sf::Color winColor = (winner == 1) ? sf::Color(0, 229, 255) : sf::Color(255, 120, 200);
    std::string winPlayerStr = (winner == 1) ? "ИГРАЧ 1 (ЗАПАД) СПЕЧЕЛИ!" : "ИГРАЧ 2 (ИЗТОК) СПЕЧЕЛИ!";

    // 2. Victory Modal Box
    float boxW = 740.0f;
    float boxH = 480.0f;
    float boxX = (1600.0f - boxW) / 2.0f;
    float boxY = (900.0f - boxH) / 2.0f;

    sf::RectangleShape box({ boxW, boxH });
    box.setPosition({ boxX, boxY });
    box.setFillColor(sf::Color(18, 24, 38, 252));
    box.setOutlineThickness(3.0f);
    box.setOutlineColor(winColor);
    window.draw(box);

    // Top Header Banner
    sf::RectangleShape header({ boxW, 52.0f });
    header.setPosition({ boxX, boxY });
    header.setFillColor(sf::Color(26, 36, 56));
    window.draw(header);

    // Glowing accent line
    sf::RectangleShape topGlow({ boxW, 3.0f });
    topGlow.setPosition({ boxX, boxY + 52.0f });
    topGlow.setFillColor(winColor);
    window.draw(topGlow);

    if (resourcesLoaded) {
        // Header Text
        sf::Text tHeader(font, toUtf8("ЕНЕРГИЙНА КРИЗА: ПОБЕДА 100%"), 16);
        tHeader.setFillColor(sf::Color(255, 215, 0));
        sf::FloatRect hb = tHeader.getLocalBounds();
        tHeader.setPosition({ boxX + (boxW - hb.size.x) / 2.0f, boxY + 14.0f });
        window.draw(tHeader);

        // Huge Winner Title
        sf::Text tWinner(font, toUtf8(winPlayerStr), 26);
        tWinner.setFillColor(winColor);
        sf::FloatRect wb = tWinner.getLocalBounds();
        tWinner.setPosition({ boxX + (boxW - wb.size.x) / 2.0f, boxY + 80.0f });
        window.draw(tWinner);

        // Subtitle
        sf::Text tSub(font, toUtf8("Играчът постигна 100% контрол и захрани целия град с чиста енергия!"), 13);
        tSub.setFillColor(sf::Color(200, 220, 245));
        sf::FloatRect sb = tSub.getLocalBounds();
        tSub.setPosition({ boxX + (boxW - sb.size.x) / 2.0f, boxY + 125.0f });
        window.draw(tSub);

        // Stats Box
        sf::RectangleShape statsBox({ boxW - 60.0f, 180.0f });
        statsBox.setPosition({ boxX + 30.0f, boxY + 165.0f });
        statsBox.setFillColor(sf::Color(24, 32, 48, 230));
        statsBox.setOutlineThickness(1.0f);
        statsBox.setOutlineColor(sf::Color(60, 80, 115));
        window.draw(statsBox);

        const auto& winEcon = engine.getPlayerEconomy(winner);
        int ownedPlots = 0;
        for (const auto& p : engine.getLandPlots()) {
            if (p.playerOwner == winner && p.isPurchased) ownedPlots++;
        }
        int totalBuildings = 0;
        for (const auto& b : engine.getBuildings()) {
            if (b.playerOwner == winner) totalBuildings++;
        }

        std::vector<std::string> statLines = {
            "Ден на победата: Ден " + std::to_string(engine.getCurrentDay()),
            "Произведена мощност: " + std::to_string(winEcon.energyMW) + " MW",
            "Закупени парцели земя: " + std::to_string(ownedPlots) + " / 12 парцела",
            "Построени съоръжения: " + std::to_string(totalBuildings) + " сгради",
            "Налично злато: " + std::to_string(winEcon.gold) + " G | Градска хазна: " + std::to_string(winEcon.money) + " $"
        };

        for (size_t i = 0; i < statLines.size(); i++) {
            sf::Text tStat(font, toUtf8(statLines[i]), 13);
            tStat.setFillColor(sf::Color(220, 235, 255));
            tStat.setPosition({ boxX + 50.0f, boxY + 180.0f + i * 28.0f });
            window.draw(tStat);
        }

        // Action Buttons
        victoryRestartBtn = sf::FloatRect({ boxX + 60.0f, boxY + boxH - 65.0f }, { 280.0f, 44.0f });
        victoryMenuBtn = sf::FloatRect({ boxX + boxW - 340.0f, boxY + boxH - 65.0f }, { 280.0f, 44.0f });

        bool hoverRestart = victoryRestartBtn.contains(mousePos);
        sf::RectangleShape btnR(victoryRestartBtn.size);
        btnR.setPosition(victoryRestartBtn.position);
        btnR.setFillColor(hoverRestart ? sf::Color(0, 200, 130) : sf::Color(0, 150, 95));
        btnR.setOutlineThickness(1.5f);
        btnR.setOutlineColor(sf::Color(100, 255, 180));
        window.draw(btnR);

        sf::Text tR(font, toUtf8("[ R ]  НОВА ИГРА"), 13);
        tR.setFillColor(sf::Color::White);
        sf::FloatRect rb = tR.getLocalBounds();
        tR.setPosition({ victoryRestartBtn.position.x + (victoryRestartBtn.size.x - rb.size.x) / 2.0f,
                         victoryRestartBtn.position.y + (victoryRestartBtn.size.y - rb.size.y) / 2.0f - 2.0f });
        window.draw(tR);

        bool hoverMenu = victoryMenuBtn.contains(mousePos);
        sf::RectangleShape btnM(victoryMenuBtn.size);
        btnM.setPosition(victoryMenuBtn.position);
        btnM.setFillColor(hoverMenu ? sf::Color(70, 90, 120) : sf::Color(45, 60, 85));
        btnM.setOutlineThickness(1.5f);
        btnM.setOutlineColor(sf::Color(130, 160, 205));
        window.draw(btnM);

        sf::Text tM(font, toUtf8("[ ESC / M ]  ГЛАВНО МЕНЮ"), 13);
        tM.setFillColor(sf::Color::White);
        sf::FloatRect mb = tM.getLocalBounds();
        tM.setPosition({ victoryMenuBtn.position.x + (victoryMenuBtn.size.x - mb.size.x) / 2.0f,
                         victoryMenuBtn.position.y + (victoryMenuBtn.size.y - mb.size.y) / 2.0f - 2.0f });
        window.draw(tM);
    }
}

void UI_map::drawPauseMenu(sf::RenderWindow& window) {
    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    bool mouseMoved = (std::abs(mousePos.x - lastPauseMousePos.x) > 2.0f ||
                       std::abs(mousePos.y - lastPauseMousePos.y) > 2.0f);
    if (mouseMoved) {
        lastPauseMousePos = mousePos;
    }

    // 1. Frosted dim backdrop
    sf::RectangleShape backdrop({ 1600.0f, 900.0f });
    backdrop.setFillColor(sf::Color(8, 12, 20, 215));
    window.draw(backdrop);

    // 2. Pause Card
    float boxW = 500.0f;
    float boxH = 430.0f;
    float boxX = (1600.0f - boxW) / 2.0f;
    float boxY = (900.0f - boxH) / 2.0f;

    sf::RectangleShape box({ boxW, boxH });
    box.setPosition({ boxX, boxY });
    box.setFillColor(sf::Color(16, 22, 34, 252));
    box.setOutlineThickness(3.0f);
    box.setOutlineColor(sf::Color(0, 229, 255));
    window.draw(box);

    // Header banner
    sf::RectangleShape header({ boxW, 56.0f });
    header.setPosition({ boxX, boxY });
    header.setFillColor(sf::Color(24, 34, 52));
    window.draw(header);

    sf::RectangleShape glowLine({ boxW, 3.0f });
    glowLine.setPosition({ boxX, boxY + 56.0f });
    glowLine.setFillColor(sf::Color(0, 229, 255));
    window.draw(glowLine);

    if (resourcesLoaded) {
        sf::Text tHeader(font, toUtf8("[ ПАУЗА ]  ИГРАТА Е НА ПАУЗА"), 18);
        tHeader.setFillColor(sf::Color(255, 215, 0));
        sf::FloatRect hb = tHeader.getLocalBounds();
        tHeader.setPosition({ boxX + (boxW - hb.size.x) / 2.0f, boxY + 16.0f });
        window.draw(tHeader);

        sf::Text tSub(font, toUtf8("Използвайте [Стрелки] / [Enter] или мишката за избор"), 12);
        tSub.setFillColor(sf::Color(150, 185, 220));
        sf::FloatRect sb = tSub.getLocalBounds();
        tSub.setPosition({ boxX + (boxW - sb.size.x) / 2.0f, boxY + 70.0f });
        window.draw(tSub);

        // 4 Menu Options
        float btnW = 390.0f;
        float btnH = 52.0f;
        float btnX = boxX + (boxW - btnW) / 2.0f;
        float startY = boxY + 105.0f;
        float spacing = 62.0f;

        pauseResumeBtn  = sf::FloatRect({ btnX, startY }, { btnW, btnH });
        pauseRestartBtn = sf::FloatRect({ btnX, startY + spacing }, { btnW, btnH });
        pauseHelpBtn    = sf::FloatRect({ btnX, startY + 2.0f * spacing }, { btnW, btnH });
        pauseMenuBtn    = sf::FloatRect({ btnX, startY + 3.0f * spacing }, { btnW, btnH });

        struct PauseOption {
            sf::FloatRect bounds;
            std::string label;
            sf::Color normalColor;
            sf::Color hoverColor;
            sf::Color outlineColor;
        };

        PauseOption opts[4] = {
            { pauseResumeBtn,  "ПРОДЪЛЖИ  [ ESC / ENTER ]", sf::Color(20, 120, 85),  sf::Color(30, 175, 120), sf::Color(0, 255, 180) },
            { pauseRestartBtn, "НОВА ИГРА  [ R ]",          sf::Color(45, 90, 130),  sf::Color(65, 130, 185), sf::Color(0, 229, 255) },
            { pauseHelpBtn,    "ПОМОЩ И ПРАВИЛА  [ H ]",    sf::Color(80, 75, 45),   sf::Color(135, 125, 60), sf::Color(255, 215, 0) },
            { pauseMenuBtn,    "ГЛАВНО МЕНЮ  [ M ]",        sf::Color(70, 45, 55),   sf::Color(120, 65, 80),  sf::Color(255, 120, 140) }
        };

        for (int i = 0; i < 4; i++) {
            if (mouseMoved && opts[i].bounds.contains(mousePos)) {
                pauseSelectedIdx = i;
            }
            bool isSel = (pauseSelectedIdx == i);

            sf::RectangleShape bShape(opts[i].bounds.size);
            bShape.setPosition(opts[i].bounds.position);
            bShape.setFillColor(isSel ? opts[i].hoverColor : opts[i].normalColor);
            bShape.setOutlineThickness(isSel ? 2.5f : 1.0f);
            bShape.setOutlineColor(isSel ? sf::Color::White : opts[i].outlineColor);
            window.draw(bShape);

            sf::Text tBtn(font, toUtf8(opts[i].label), 14);
            tBtn.setFillColor(isSel ? sf::Color::White : sf::Color(225, 240, 255));
            sf::FloatRect bb = tBtn.getLocalBounds();
            tBtn.setPosition({ opts[i].bounds.position.x + (opts[i].bounds.size.x - bb.size.x) / 2.0f,
                               opts[i].bounds.position.y + (opts[i].bounds.size.y - bb.size.y) / 2.0f - 2.0f });
            window.draw(tBtn);
        }
    }
}
