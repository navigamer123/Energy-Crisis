#include "../includes/UI_mainMenu.h"
#include <iostream>
#include <cmath>
#include <algorithm>

UI_mainMenu::UI_mainMenu()
    : state(MenuState::MAIN),
      requestPlay(false),
      requestQuit(false),
      selectedMainIndex(0),
      selectedModeIndex(0),
      selectedDifficultyIndex(1),
      selectedSettingsIndex(0),
      selectedBotDifficulty(BotDifficulty::NONE),
      volume(80),
      soundEffects(true),
      settingsDifficultyIndex(1),
      fontLoaded(false) {
    if (font.openFromFile("assets/font.ttf")) {
        fontLoaded = true;
    } else {
        std::cerr << "[UI_mainMenu] Warning: Failed to load assets/font.ttf\n";
    }
    std::cout << "[UI_mainMenu] SFML Main Menu with Mode & Difficulty selection ready.\n";
}

UI_mainMenu::~UI_mainMenu() {
    std::cout << "[UI_mainMenu] SFML Main Menu destroyed.\n";
}

bool UI_mainMenu::isPointInside(sf::FloatRect bounds, sf::Vector2f pt) const {
    return bounds.contains(pt);
}

void UI_mainMenu::drawButton(sf::RenderWindow& window, sf::FloatRect bounds, const sf::String& text,
                             sf::Color baseColor, sf::Color hoverColor, sf::Color textColor,
                             bool isSelected) {
    sf::RectangleShape shape(bounds.size);
    shape.setPosition(bounds.position);
    shape.setFillColor(isSelected ? hoverColor : baseColor);
    shape.setOutlineThickness(isSelected ? 3.0f : 1.5f);
    shape.setOutlineColor(isSelected ? sf::Color(255, 215, 0) : sf::Color(70, 90, 120));
    window.draw(shape);

    if (fontLoaded) {
        sf::String displayText = isSelected ? (toUtf8("> ") + text + toUtf8(" <")) : text;
        sf::Text label(font, displayText, 18);
        label.setFillColor(isSelected ? sf::Color(255, 240, 150) : textColor);

        sf::FloatRect textBounds = label.getLocalBounds();
        label.setPosition({
            bounds.position.x + (bounds.size.x - textBounds.size.x) / 2.0f - textBounds.position.x,
            bounds.position.y + (bounds.size.y - textBounds.size.y) / 2.0f - textBounds.position.y
        });
        window.draw(label);
    }
}

void UI_mainMenu::drawHeader(sf::RenderWindow& window) {
    (void)window;
    float screenWidth = VIRTUAL_WIDTH;

    if (fontLoaded) {
        // Drop shadow
        sf::Text titleShadow(font, "ENERGY CRISIS", 54);
        titleShadow.setFillColor(sf::Color(180, 100, 0, 180));
        sf::FloatRect sBounds = titleShadow.getLocalBounds();
        titleShadow.setPosition({ (screenWidth - sBounds.size.x) / 2.0f + 2.0f, 62.0f });
        window.draw(titleShadow);

        // Main Title
        sf::Text title(font, "ENERGY CRISIS", 54);
        title.setFillColor(sf::Color(255, 204, 0));
        title.setPosition({ (screenWidth - sBounds.size.x) / 2.0f, 60.0f });
        window.draw(title);

        // Subtitle
        sf::Text subtitle(font, toUtf8("УПРАВЛЕНИЕ НА ЕНЕРГИЙНАТА МРЕЖА И РЕСУРСИТЕ"), 16);
        subtitle.setFillColor(sf::Color(140, 180, 220));
        sf::FloatRect subBounds = subtitle.getLocalBounds();
        subtitle.setPosition({ (screenWidth - subBounds.size.x) / 2.0f, 124.0f });
        window.draw(subtitle);

        // Decorative line
        sf::RectangleShape line({ subBounds.size.x + 80.0f, 2.0f });
        line.setPosition({ (screenWidth - (subBounds.size.x + 80.0f)) / 2.0f, 152.0f });
        line.setFillColor(sf::Color(60, 85, 120, 180));
        window.draw(line);
    }
}

void UI_mainMenu::onPlay() {
    std::cout << "[UI_mainMenu] Game launching with Control Scheme "
              << static_cast<int>(playControls.getSelectedScheme())
              << ", Bot Difficulty: " << static_cast<int>(selectedBotDifficulty) << "...\n";
    requestPlay = true;

    // Coming back from the match ('ГЛАВНО МЕНЮ') must show the top-level menu, not the
    // submenu that started this match (one Enter there would start a new match at once).
    // The chosen scheme / difficulty stay stored for getSelectedControlScheme/BotDifficulty().
    state = MenuState::MAIN;
    selectedMainIndex = 0;
    enterHeld = false;
    spaceHeld = false;
}

void UI_mainMenu::returnToMain() {
    state = MenuState::MAIN;
    selectedMainIndex = 0;
    mapSelect.reroll(); // Team b-power: a fresh terrain roll for the next match
    // A key still held from the match (e.g. Enter on 'ГЛАВНО МЕНЮ') must be released first
    enterHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter);
    spaceHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
}

void UI_mainMenu::onSettings() {
    std::cout << "[UI_mainMenu] Settings opened.\n";
    state = MenuState::SETTINGS;
    selectedSettingsIndex = 0;
}

void UI_mainMenu::onQuit() {
    std::cout << "[UI_mainMenu] Exiting game...\n";
    requestQuit = true;
}

void UI_mainMenu::drawMainMenu(sf::RenderWindow& window) {
    drawHeader(window);

    float screenWidth = VIRTUAL_WIDTH;
    float btnWidth = 320.0f;
    float btnHeight = 54.0f;
    float btnX = (screenWidth - btnWidth) / 2.0f;

    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    sf::FloatRect playBtn({ btnX, 230.0f }, { btnWidth, btnHeight });
    sf::FloatRect settingsBtn({ btnX, 306.0f }, { btnWidth, btnHeight });
    sf::FloatRect quitBtn({ btnX, 382.0f }, { btnWidth, btnHeight });

    bool mouseMoved = (std::abs(mousePos.x - lastMenuMousePos.x) > 2.0f || std::abs(mousePos.y - lastMenuMousePos.y) > 2.0f);
    if (mouseMoved) {
        lastMenuMousePos = mousePos;
        if (playBtn.contains(mousePos)) selectedMainIndex = 0;
        else if (settingsBtn.contains(mousePos)) selectedMainIndex = 1;
        else if (quitBtn.contains(mousePos)) selectedMainIndex = 2;
    }

    sf::Color defaultBtn(30, 40, 56);
    sf::Color whiteText(240, 245, 255);

    drawButton(window, playBtn, toUtf8("ИГРА / PLAY"), defaultBtn, sf::Color(35, 120, 70), whiteText, selectedMainIndex == 0);
    drawButton(window, settingsBtn, toUtf8("НАСТРОЙКИ / SETTINGS"), defaultBtn, sf::Color(35, 80, 140), whiteText, selectedMainIndex == 1);
    drawButton(window, quitBtn, toUtf8("ИЗХОД / QUIT"), defaultBtn, sf::Color(140, 40, 40), whiteText, selectedMainIndex == 2);

    if (fontLoaded) {
        sf::Text hint(font, toUtf8("Навигация: [Стрелки / W,S] | Избор: [Enter]"), 14);
        hint.setFillColor(sf::Color(130, 155, 185));
        sf::FloatRect hb = hint.getLocalBounds();
        hint.setPosition({ (screenWidth - hb.size.x) / 2.0f, 470.0f });
        window.draw(hint);
    }
}

void UI_mainMenu::drawModeSelectMenu(sf::RenderWindow& window) {
    drawHeader(window);

    float screenWidth = VIRTUAL_WIDTH;
    float btnWidth = 540.0f;
    float btnHeight = 58.0f;
    float btnX = (screenWidth - btnWidth) / 2.0f;
    float startY = 220.0f;
    float spacing = 78.0f;

    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    sf::FloatRect coopBtn({ btnX, startY }, { btnWidth, btnHeight });
    sf::FloatRect singleBtn({ btnX, startY + spacing }, { btnWidth, btnHeight });
    sf::FloatRect backBtn({ btnX + (btnWidth - 240.0f) / 2.0f, startY + 2.0f * spacing + 12.0f }, { 240.0f, 48.0f });

    bool mouseMoved = (std::abs(mousePos.x - lastMenuMousePos.x) > 2.0f || std::abs(mousePos.y - lastMenuMousePos.y) > 2.0f);
    if (mouseMoved) {
        lastMenuMousePos = mousePos;
        if (coopBtn.contains(mousePos)) selectedModeIndex = 0;
        else if (singleBtn.contains(mousePos)) selectedModeIndex = 1;
        else if (backBtn.contains(mousePos)) selectedModeIndex = 2;
    }

    sf::Color defaultBtn(30, 40, 56);
    sf::Color whiteText(240, 245, 255);

    drawButton(window, coopBtn, toUtf8("ДВАМА ИГРАЧИ / CO-OP (1v1)"), defaultBtn, sf::Color(35, 120, 70), whiteText, selectedModeIndex == 0);
    drawButton(window, singleBtn, toUtf8("САМОСТОЯТЕЛНА ИГРА / SINGLE PLAYER (VS BOT)"), defaultBtn, sf::Color(35, 95, 150), whiteText, selectedModeIndex == 1);
    drawButton(window, backBtn, toUtf8("НАЗАД / BACK"), defaultBtn, sf::Color(80, 50, 60), whiteText, selectedModeIndex == 2);

    if (fontLoaded) {
        std::string desc = (selectedModeIndex == 0)
            ? "Двама играчи се състезават на една машина (Разделен екран / Сектори)"
            : (selectedModeIndex == 1)
                ? "Играйте срещу автономен изкуствен интелект (Бот в East сектора)"
                : "Връщане към главното меню";

        sf::Text tDesc(font, toUtf8(desc), 14);
        tDesc.setFillColor(sf::Color(170, 205, 240));
        sf::FloatRect db = tDesc.getLocalBounds();
        tDesc.setPosition({ (screenWidth - db.size.x) / 2.0f, startY + 2.0f * spacing + 78.0f });
        window.draw(tDesc);

        sf::Text hint(font, toUtf8("Навигация: [Стрелки / W,S] | Избор: [Enter / Space] | Отказ: [ESC]"), 13);
        hint.setFillColor(sf::Color(120, 145, 175));
        sf::FloatRect hb = hint.getLocalBounds();
        hint.setPosition({ (screenWidth - hb.size.x) / 2.0f, startY + 2.0f * spacing + 112.0f });
        window.draw(hint);
    }

    // Team b-power (F-39): map layout selector with mini-map preview
    mapSelect.draw(window, font, fontLoaded, startY + 2.0f * spacing + 142.0f, mousePos);
}

void UI_mainMenu::drawBotDifficultyMenu(sf::RenderWindow& window) {
    drawHeader(window);

    float screenWidth = VIRTUAL_WIDTH;
    float btnWidth = 460.0f;
    float btnHeight = 54.0f;
    float btnX = (screenWidth - btnWidth) / 2.0f;
    float startY = 205.0f;
    float spacing = 66.0f;

    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    sf::FloatRect easyBtn({ btnX, startY }, { btnWidth, btnHeight });
    sf::FloatRect medBtn({ btnX, startY + spacing }, { btnWidth, btnHeight });
    sf::FloatRect hardBtn({ btnX, startY + 2.0f * spacing }, { btnWidth, btnHeight });
    sf::FloatRect backBtn({ btnX + (btnWidth - 240.0f) / 2.0f, startY + 3.0f * spacing + 10.0f }, { 240.0f, 46.0f });

    bool mouseMoved = (std::abs(mousePos.x - lastMenuMousePos.x) > 2.0f || std::abs(mousePos.y - lastMenuMousePos.y) > 2.0f);
    if (mouseMoved) {
        lastMenuMousePos = mousePos;
        if (easyBtn.contains(mousePos)) selectedDifficultyIndex = 0;
        else if (medBtn.contains(mousePos)) selectedDifficultyIndex = 1;
        else if (hardBtn.contains(mousePos)) selectedDifficultyIndex = 2;
        else if (backBtn.contains(mousePos)) selectedDifficultyIndex = 3;
    }

    sf::Color defaultBtn(30, 40, 56);
    sf::Color whiteText(240, 245, 255);

    drawButton(window, easyBtn, toUtf8("ЛЕСНО / EASY BOT"), defaultBtn, sf::Color(35, 130, 80), whiteText, selectedDifficultyIndex == 0);
    drawButton(window, medBtn, toUtf8("СРЕДНО / MEDIUM BOT"), defaultBtn, sf::Color(170, 110, 30), whiteText, selectedDifficultyIndex == 1);
    drawButton(window, hardBtn, toUtf8("ТРУДНО / HARD BOT"), defaultBtn, sf::Color(160, 45, 45), whiteText, selectedDifficultyIndex == 2);
    drawButton(window, backBtn, toUtf8("НАЗАД / BACK"), defaultBtn, sf::Color(70, 45, 60), whiteText, selectedDifficultyIndex == 3);

    if (fontLoaded) {
        std::string desc = (selectedDifficultyIndex == 0)
            ? "По-бавен бот; строи базови солари и турбини (подходящ за учене)"
            : (selectedDifficultyIndex == 1)
                ? "Балансиран бот; събира ресурси, строи батерии и нощни лампи"
                : (selectedDifficultyIndex == 2)
                    ? "Бърз и агресивен бот; купува земя, ъпгрейдва мини и оптимизира ток"
                    : "Връщане към избор на режим";

        sf::Text tDesc(font, toUtf8(desc), 14);
        tDesc.setFillColor(sf::Color(170, 205, 240));
        sf::FloatRect db = tDesc.getLocalBounds();
        tDesc.setPosition({ (screenWidth - db.size.x) / 2.0f, startY + 3.0f * spacing + 70.0f });
        window.draw(tDesc);

        sf::Text hint(font, toUtf8("Навигация: [Стрелки / W,S] | Старт: [Enter / Space] | Отказ: [ESC]"), 13);
        hint.setFillColor(sf::Color(120, 145, 175));
        sf::FloatRect hb = hint.getLocalBounds();
        hint.setPosition({ (screenWidth - hb.size.x) / 2.0f, startY + 3.0f * spacing + 102.0f });
        window.draw(hint);
    }
}

void UI_mainMenu::drawSettingsMenu(sf::RenderWindow& window) {
    drawHeader(window);

    float screenWidth = VIRTUAL_WIDTH;
    float panelWidth = 560.0f;
    float panelHeight = 370.0f;
    float panelX = (screenWidth - panelWidth) / 2.0f;
    float panelY = 200.0f;

    sf::RectangleShape panel({ panelWidth, panelHeight });
    panel.setPosition({ panelX, panelY });
    panel.setFillColor(sf::Color(22, 28, 40, 245));
    panel.setOutlineThickness(2.0f);
    panel.setOutlineColor(sf::Color(0, 200, 255, 200));
    window.draw(panel);

    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    if (fontLoaded) {
        sf::Text sTitle(font, toUtf8("НАСТРОЙКИ / SETTINGS"), 24);
        sTitle.setFillColor(sf::Color(0, 229, 255));
        sf::FloatRect tb = sTitle.getLocalBounds();
        sTitle.setPosition({ panelX + (panelWidth - tb.size.x) / 2.0f, panelY + 16.0f });
        window.draw(sTitle);
    }

    // Row 0: Volume
    sf::FloatRect volRow({ panelX + 30.0f, panelY + 70.0f }, { panelWidth - 60.0f, 42.0f });
    bool volSelected = (selectedSettingsIndex == 0);
    sf::RectangleShape volHighlight(volRow.size);
    volHighlight.setPosition(volRow.position);
    volHighlight.setFillColor(volSelected ? sf::Color(40, 55, 80, 200) : sf::Color::Transparent);
    volHighlight.setOutlineThickness(volSelected ? 1.5f : 0.0f);
    volHighlight.setOutlineColor(sf::Color(255, 215, 0));
    window.draw(volHighlight);

    if (fontLoaded) {
        sf::Text tVol(font, toUtf8(volSelected ? "> Сила на звука:" : "  Сила на звука:"), 17);
        tVol.setFillColor(volSelected ? sf::Color(255, 240, 150) : sf::Color::White);
        tVol.setPosition({ panelX + 45.0f, panelY + 78.0f });
        window.draw(tVol);

        sf::Text volVal(font, toUtf8(std::to_string(volume) + "%"), 18);
        volVal.setFillColor(sf::Color(255, 204, 0));
        volVal.setPosition({ panelX + 345.0f, panelY + 78.0f });
        window.draw(volVal);
    }
    sf::FloatRect volDown({ panelX + 280.0f, panelY + 74.0f }, { 36.0f, 32.0f });
    sf::FloatRect volUp({ panelX + 420.0f, panelY + 74.0f }, { 36.0f, 32.0f });
    drawButton(window, volDown, "-", sf::Color(40, 50, 70), sf::Color(60, 80, 110), sf::Color::White, volDown.contains(mousePos));
    drawButton(window, volUp, "+", sf::Color(40, 50, 70), sf::Color(60, 80, 110), sf::Color::White, volUp.contains(mousePos));

    // Row 1: Sound FX
    sf::FloatRect sfxRow({ panelX + 30.0f, panelY + 125.0f }, { panelWidth - 60.0f, 42.0f });
    bool sfxSelected = (selectedSettingsIndex == 1);
    sf::RectangleShape sfxHighlight(sfxRow.size);
    sfxHighlight.setPosition(sfxRow.position);
    sfxHighlight.setFillColor(sfxSelected ? sf::Color(40, 55, 80, 200) : sf::Color::Transparent);
    sfxHighlight.setOutlineThickness(sfxSelected ? 1.5f : 0.0f);
    sfxHighlight.setOutlineColor(sf::Color(255, 215, 0));
    window.draw(sfxHighlight);

    if (fontLoaded) {
        sf::Text tSfx(font, toUtf8(sfxSelected ? "> Звукови ефекти:" : "  Звукови ефекти:"), 17);
        tSfx.setFillColor(sfxSelected ? sf::Color(255, 240, 150) : sf::Color::White);
        tSfx.setPosition({ panelX + 45.0f, panelY + 133.0f });
        window.draw(tSfx);
    }
    sf::FloatRect sfxBtn({ panelX + 280.0f, panelY + 129.0f }, { 180.0f, 34.0f });
    drawButton(window, sfxBtn, toUtf8(soundEffects ? "ВКЛЮЧЕНИ" : "ИЗКЛЮЧЕНИ"),
               soundEffects ? sf::Color(30, 100, 60) : sf::Color(100, 40, 40),
               sf::Color(50, 120, 80), sf::Color::White, sfxSelected || sfxBtn.contains(mousePos));

    // Row 2: Difficulty
    sf::FloatRect diffRow({ panelX + 30.0f, panelY + 180.0f }, { panelWidth - 60.0f, 42.0f });
    bool diffSelected = (selectedSettingsIndex == 2);
    sf::RectangleShape diffHighlight(diffRow.size);
    diffHighlight.setPosition(diffRow.position);
    diffHighlight.setFillColor(diffSelected ? sf::Color(40, 55, 80, 200) : sf::Color::Transparent);
    diffHighlight.setOutlineThickness(diffSelected ? 1.5f : 0.0f);
    diffHighlight.setOutlineColor(sf::Color(255, 215, 0));
    window.draw(diffHighlight);

    if (fontLoaded) {
        sf::Text tDiff(font, toUtf8(diffSelected ? "> Трудност на бота:" : "  Трудност на бота:"), 17);
        tDiff.setFillColor(diffSelected ? sf::Color(255, 240, 150) : sf::Color::White);
        tDiff.setPosition({ panelX + 45.0f, panelY + 188.0f });
        window.draw(tDiff);
    }
    const char* diffLabels[] = { "ЛЕСНО", "НОРМАЛНО", "ТРУДНО" };
    sf::FloatRect diffBtn({ panelX + 280.0f, panelY + 184.0f }, { 180.0f, 34.0f });
    drawButton(window, diffBtn, toUtf8(diffLabels[settingsDifficultyIndex]),
               sf::Color(40, 55, 80), sf::Color(60, 85, 120), sf::Color::White, diffSelected || diffBtn.contains(mousePos));

    // Honest notes: there is no audio yet, and the difficulty is only the default bot choice
    if (fontLoaded) {
        const char* notes[] = {
            "Звукът все още не е реализиран: звуковите настройки нямат ефект.",
            "Трудността на бота е избраната по подразбиране в САМОСТОЯТЕЛНА ИГРА."
        };
        for (int i = 0; i < 2; ++i) {
            sf::Text tNote(font, toUtf8(notes[i]), 12);
            tNote.setFillColor(sf::Color(150, 170, 195));
            sf::FloatRect nb = tNote.getLocalBounds();
            tNote.setPosition({ panelX + (panelWidth - nb.size.x) / 2.0f, panelY + 234.0f + i * 20.0f });
            window.draw(tNote);
        }
    }

    // Row 3: Back button
    sf::FloatRect backBtn({ panelX + (panelWidth - 220.0f) / 2.0f, panelY + 285.0f }, { 220.0f, 46.0f });
    bool backSelected = (selectedSettingsIndex == 3);
    drawButton(window, backBtn, toUtf8("НАЗАД"), sf::Color(35, 45, 65), sf::Color(80, 90, 120), sf::Color::White, backSelected || backBtn.contains(mousePos));

    // Hints
    if (fontLoaded) {
        sf::Text hint(font, toUtf8("Навигация: [W/S или Стрелки] | Промяна: [A/D или Enter]"), 13);
        hint.setFillColor(sf::Color(130, 155, 185));
        sf::FloatRect hb = hint.getLocalBounds();
        hint.setPosition({ (screenWidth - hb.size.x) / 2.0f, panelY + panelHeight + 14.0f });
        window.draw(hint);
    }
}

void UI_mainMenu::handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    // Filter auto-repeated Enter/Space: a held select key acts once and must be released before
    // it acts again, so holding it can never walk through the submenus and start a match.
    if (event.is<sf::Event::FocusLost>()) {
        enterHeld = false;
        spaceHeld = false;
    }
    if (const auto* released = event.getIf<sf::Event::KeyReleased>()) {
        if (released->code == sf::Keyboard::Key::Enter) enterHeld = false;
        if (released->code == sf::Keyboard::Key::Space) spaceHeld = false;
    }
    if (const auto* pressed = event.getIf<sf::Event::KeyPressed>()) {
        if (pressed->code == sf::Keyboard::Key::Enter) {
            if (enterHeld) return;
            enterHeld = true;
        } else if (pressed->code == sf::Keyboard::Key::Space) {
            if (spaceHeld) return;
            spaceHeld = true;
        }
    }

    if (state == MenuState::PLAY_CONTROLS) {
        playControls.handleEvent(event, window);
        if (playControls.isStartRequested()) {
            playControls.resetRequests();
            selectedBotDifficulty = BotDifficulty::NONE;
            onPlay();
        } else if (playControls.isBackRequested()) {
            playControls.resetRequests();
            state = MenuState::MODE_SELECT;
        }
        return;
    }

    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        bool isUp = (key->code == sf::Keyboard::Key::Up || key->code == sf::Keyboard::Key::W);
        bool isDown = (key->code == sf::Keyboard::Key::Down || key->code == sf::Keyboard::Key::S);
        bool isLeft = (key->code == sf::Keyboard::Key::Left || key->code == sf::Keyboard::Key::A);
        bool isRight = (key->code == sf::Keyboard::Key::Right || key->code == sf::Keyboard::Key::D);
        bool isSelect = (key->code == sf::Keyboard::Key::Enter || key->code == sf::Keyboard::Key::Space);
        bool isEscape = (key->code == sf::Keyboard::Key::Escape);

        if (state == MenuState::MAIN) {
            if (isUp) {
                selectedMainIndex = (selectedMainIndex + 2) % 3;
            } else if (isDown) {
                selectedMainIndex = (selectedMainIndex + 1) % 3;
            } else if (isSelect) {
                if (selectedMainIndex == 0) {
                    state = MenuState::MODE_SELECT;
                    selectedModeIndex = 0;
                } else if (selectedMainIndex == 1) {
                    onSettings();
                } else if (selectedMainIndex == 2) {
                    onQuit();
                }
            }
        } else if (state == MenuState::MODE_SELECT) {
            if (mapSelect.handleKey(key->code)) {
                // Team b-power: A/D, Left/Right and R change the map layout
            } else if (isUp) {
                selectedModeIndex = (selectedModeIndex + 2) % 3;
            } else if (isDown) {
                selectedModeIndex = (selectedModeIndex + 1) % 3;
            } else if (isSelect) {
                if (selectedModeIndex == 0) {
                    selectedBotDifficulty = BotDifficulty::NONE;
                    state = MenuState::PLAY_CONTROLS;
                    playControls.resetRequests();
                } else if (selectedModeIndex == 1) {
                    state = MenuState::BOT_DIFFICULTY;
                    selectedDifficultyIndex = settingsDifficultyIndex; // default from НАСТРОЙКИ
                } else if (selectedModeIndex == 2) {
                    state = MenuState::MAIN;
                }
            } else if (isEscape) {
                state = MenuState::MAIN;
            }
        } else if (state == MenuState::BOT_DIFFICULTY) {
            if (isUp) {
                selectedDifficultyIndex = (selectedDifficultyIndex + 3) % 4;
            } else if (isDown) {
                selectedDifficultyIndex = (selectedDifficultyIndex + 1) % 4;
            } else if (isSelect) {
                if (selectedDifficultyIndex == 0) {
                    selectedBotDifficulty = BotDifficulty::EASY;
                    onPlay();
                } else if (selectedDifficultyIndex == 1) {
                    selectedBotDifficulty = BotDifficulty::MEDIUM;
                    onPlay();
                } else if (selectedDifficultyIndex == 2) {
                    selectedBotDifficulty = BotDifficulty::HARD;
                    onPlay();
                } else if (selectedDifficultyIndex == 3) {
                    state = MenuState::MODE_SELECT;
                }
            } else if (isEscape) {
                state = MenuState::MODE_SELECT;
            }
        } else if (state == MenuState::SETTINGS) {
            if (isEscape) {
                state = MenuState::MAIN;
                return;
            }
            if (isUp) {
                selectedSettingsIndex = (selectedSettingsIndex + 3) % 4;
            } else if (isDown) {
                selectedSettingsIndex = (selectedSettingsIndex + 1) % 4;
            } else if (isLeft) {
                if (selectedSettingsIndex == 0) {
                    if (volume >= 10) volume -= 10;
                } else if (selectedSettingsIndex == 1) {
                    soundEffects = !soundEffects;
                } else if (selectedSettingsIndex == 2) {
                    settingsDifficultyIndex = (settingsDifficultyIndex + 2) % 3;
                }
            } else if (isRight) {
                if (selectedSettingsIndex == 0) {
                    if (volume <= 90) volume += 10;
                } else if (selectedSettingsIndex == 1) {
                    soundEffects = !soundEffects;
                } else if (selectedSettingsIndex == 2) {
                    settingsDifficultyIndex = (settingsDifficultyIndex + 1) % 3;
                }
            } else if (isSelect) {
                if (selectedSettingsIndex == 0) {
                    // Step up by 10% and wrap to 0% after 100% (never above 100%)
                    volume = (volume >= 100) ? 0 : std::min(100, volume + 10);
                } else if (selectedSettingsIndex == 1) {
                    soundEffects = !soundEffects;
                } else if (selectedSettingsIndex == 2) {
                    settingsDifficultyIndex = (settingsDifficultyIndex + 1) % 3;
                } else if (selectedSettingsIndex == 3) {
                    state = MenuState::MAIN;
                }
            }
        }
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button == sf::Mouse::Button::Left) {
            sf::Vector2f clickPos = window.mapPixelToCoords(mb->position);
            float screenWidth = VIRTUAL_WIDTH;

            if (state == MenuState::MAIN) {
                float btnWidth = 320.0f;
                float btnHeight = 54.0f;
                float btnX = (screenWidth - btnWidth) / 2.0f;

                if (isPointInside({ { btnX, 230.0f }, { btnWidth, btnHeight } }, clickPos)) {
                    state = MenuState::MODE_SELECT;
                    selectedModeIndex = 0;
                } else if (isPointInside({ { btnX, 306.0f }, { btnWidth, btnHeight } }, clickPos)) {
                    onSettings();
                } else if (isPointInside({ { btnX, 382.0f }, { btnWidth, btnHeight } }, clickPos)) {
                    onQuit();
                }
            } else if (state == MenuState::MODE_SELECT) {
                if (mapSelect.handleClick(clickPos)) return; // Team b-power: map selector arrows
                float btnWidth = 540.0f;
                float btnHeight = 58.0f;
                float btnX = (screenWidth - btnWidth) / 2.0f;
                float startY = 220.0f;
                float spacing = 78.0f;

                if (isPointInside({ { btnX, startY }, { btnWidth, btnHeight } }, clickPos)) {
                    selectedBotDifficulty = BotDifficulty::NONE;
                    state = MenuState::PLAY_CONTROLS;
                    playControls.resetRequests();
                } else if (isPointInside({ { btnX, startY + spacing }, { btnWidth, btnHeight } }, clickPos)) {
                    state = MenuState::BOT_DIFFICULTY;
                    selectedDifficultyIndex = settingsDifficultyIndex; // default from НАСТРОЙКИ
                } else if (isPointInside({ { btnX + (btnWidth - 240.0f) / 2.0f, startY + 2.0f * spacing + 12.0f }, { 240.0f, 48.0f } }, clickPos)) {
                    state = MenuState::MAIN;
                }
            } else if (state == MenuState::BOT_DIFFICULTY) {
                float btnWidth = 460.0f;
                float btnHeight = 54.0f;
                float btnX = (screenWidth - btnWidth) / 2.0f;
                float startY = 205.0f;
                float spacing = 66.0f;

                if (isPointInside({ { btnX, startY }, { btnWidth, btnHeight } }, clickPos)) {
                    selectedBotDifficulty = BotDifficulty::EASY;
                    onPlay();
                } else if (isPointInside({ { btnX, startY + spacing }, { btnWidth, btnHeight } }, clickPos)) {
                    selectedBotDifficulty = BotDifficulty::MEDIUM;
                    onPlay();
                } else if (isPointInside({ { btnX, startY + 2.0f * spacing }, { btnWidth, btnHeight } }, clickPos)) {
                    selectedBotDifficulty = BotDifficulty::HARD;
                    onPlay();
                } else if (isPointInside({ { btnX + (btnWidth - 240.0f) / 2.0f, startY + 3.0f * spacing + 10.0f }, { 240.0f, 46.0f } }, clickPos)) {
                    state = MenuState::MODE_SELECT;
                }
            } else if (state == MenuState::SETTINGS) {
                float panelWidth = 560.0f;
                float panelX = (screenWidth - panelWidth) / 2.0f;
                float panelY = 200.0f;

                if (isPointInside({ { panelX + 280.0f, panelY + 74.0f }, { 36.0f, 32.0f } }, clickPos)) {
                    if (volume >= 10) volume -= 10;
                    selectedSettingsIndex = 0;
                } else if (isPointInside({ { panelX + 420.0f, panelY + 74.0f }, { 36.0f, 32.0f } }, clickPos)) {
                    if (volume <= 90) volume += 10;
                    selectedSettingsIndex = 0;
                } else if (isPointInside({ { panelX + 280.0f, panelY + 129.0f }, { 180.0f, 34.0f } }, clickPos)) {
                    soundEffects = !soundEffects;
                    selectedSettingsIndex = 1;
                } else if (isPointInside({ { panelX + 280.0f, panelY + 184.0f }, { 180.0f, 34.0f } }, clickPos)) {
                    settingsDifficultyIndex = (settingsDifficultyIndex + 1) % 3;
                    selectedSettingsIndex = 2;
                } else if (isPointInside({ { panelX + (panelWidth - 220.0f) / 2.0f, panelY + 285.0f }, { 220.0f, 46.0f } }, clickPos)) {
                    state = MenuState::MAIN;
                }
            }
        }
    }
}

void UI_mainMenu::render(sf::RenderWindow& window) {
    if (state == MenuState::MAIN) {
        drawMainMenu(window);
    } else if (state == MenuState::MODE_SELECT) {
        drawModeSelectMenu(window);
    } else if (state == MenuState::PLAY_CONTROLS) {
        drawHeader(window);
        playControls.draw(window, font, fontLoaded);
    } else if (state == MenuState::BOT_DIFFICULTY) {
        drawBotDifficultyMenu(window);
    } else if (state == MenuState::SETTINGS) {
        drawSettingsMenu(window);
    }
}
