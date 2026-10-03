#include "../includes/UI_mainMenu.h"
#include "../includes/UI_layout.h"   // b-session: shared menu anchors
#include "../includes/UI_settings.h" // b-session: default bot difficulty
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
      selectedBotDifficulty(BotDifficulty::NONE),
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
    // A key still held from the match (e.g. Enter on 'ГЛАВНО МЕНЮ') must be released first
    enterHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter);
    spaceHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
}

void UI_mainMenu::onSettings() {
    std::cout << "[UI_mainMenu] Settings opened.\n";
    state = MenuState::SETTINGS;
    settingsScreen.open(false); // Team b-session: persistent settings screen (UI_settingsMenu)
}

void UI_mainMenu::onQuit() {
    std::cout << "[UI_mainMenu] Exiting game...\n";
    requestQuit = true;
}

void UI_mainMenu::drawMainMenu(sf::RenderWindow& window) {
    drawHeader(window);

    float screenWidth = VIRTUAL_WIDTH;

    // Team b-session (F-02, UX-12): one button list (ui::menu anchors) shared with the click
    // handling; ПРОДЪЛЖИ is added on top while a match can be continued
    const int count = mainButtonCount();
    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    bool mouseMoved = (std::abs(mousePos.x - lastMenuMousePos.x) > 2.0f || std::abs(mousePos.y - lastMenuMousePos.y) > 2.0f);
    if (mouseMoved) {
        lastMenuMousePos = mousePos;
        for (int i = 0; i < count; ++i)
            if (ui::menu::button(i).contains(mousePos)) selectedMainIndex = i;
    }
    if (selectedMainIndex >= count) selectedMainIndex = 0;

    sf::Color defaultBtn(30, 40, 56);
    sf::Color whiteText(240, 245, 255);

    for (int i = 0; i < count; ++i) {
        sf::FloatRect r = ui::menu::button(i);
        switch (mainButtonAction(i)) {
            case MainAction::CONTINUE: drawContinueButton(window, r, selectedMainIndex == i); break;
            case MainAction::PLAY: drawButton(window, r, toUtf8(continueInMemory ? "НОВА ИГРА / NEW GAME" : "ИГРА / PLAY"), defaultBtn, sf::Color(35, 120, 70), whiteText, selectedMainIndex == i); break;
            case MainAction::SETTINGS: drawButton(window, r, toUtf8("НАСТРОЙКИ / SETTINGS"), defaultBtn, sf::Color(35, 80, 140), whiteText, selectedMainIndex == i); break;
            case MainAction::QUIT: drawButton(window, r, toUtf8("ИЗХОД / QUIT"), defaultBtn, sf::Color(140, 40, 40), whiteText, selectedMainIndex == i); break;
        }
    }

    if (fontLoaded) {
        sf::FloatRect last = ui::menu::button(count - 1);
        sf::Text hint(font, toUtf8("Навигация: [Стрелки / W,S] | Избор: [Enter]"), 14);
        hint.setFillColor(sf::Color(130, 155, 185));
        sf::FloatRect hb = hint.getLocalBounds();
        hint.setPosition({ (screenWidth - hb.size.x) / 2.0f, last.position.y + last.size.y + 34.0f });
        window.draw(hint);
    }

    if (confirmNewOpen) drawConfirmNew(window);
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
    // Team b-session (F-06): the persistent settings screen (UI_settingsMenu) replaces the old
    // volume / sound / difficulty panel; it draws its own backdrop, panel and UI-scale zoom.
    if (fontLoaded) settingsScreen.draw(window, font);
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

    // Team b-session (F-06): the settings screen handles its own keys, clicks and gamepad navigation
    if (state == MenuState::SETTINGS) {
        settingsScreen.handleEvent(event, window);
        if (!settingsScreen.isOpen()) state = MenuState::MAIN;
        return;
    }
    // Team b-session (F-02): "start a new match?" dialog owns the input while it is open
    if (confirmNewOpen) {
        handleConfirmNewEvent(event, window);
        return;
    }

    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        bool isUp = (key->code == sf::Keyboard::Key::Up || key->code == sf::Keyboard::Key::W);
        bool isDown = (key->code == sf::Keyboard::Key::Down || key->code == sf::Keyboard::Key::S);
        bool isSelect = (key->code == sf::Keyboard::Key::Enter || key->code == sf::Keyboard::Key::Space);
        bool isEscape = (key->code == sf::Keyboard::Key::Escape);

        if (state == MenuState::MAIN) {
            const int count = mainButtonCount(); // b-session: ПРОДЪЛЖИ adds a button
            if (isUp) {
                selectedMainIndex = (selectedMainIndex + count - 1) % count;
            } else if (isDown) {
                selectedMainIndex = (selectedMainIndex + 1) % count;
            } else if (isSelect) {
                activateMainButton(selectedMainIndex);
            }
        } else if (state == MenuState::MODE_SELECT) {
            if (isUp) {
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
                    selectedDifficultyIndex = std::max(0, std::min(2, gameSettings().defaultBotDifficulty - 1)); // default from НАСТРОЙКИ (b-session)
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
        }
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button == sf::Mouse::Button::Left) {
            sf::Vector2f clickPos = window.mapPixelToCoords(mb->position);
            float screenWidth = VIRTUAL_WIDTH;

            if (state == MenuState::MAIN) {
                // b-session (UX-12): the same ui::menu rects drawMainMenu() draws
                for (int i = 0; i < mainButtonCount(); ++i) {
                    if (isPointInside(ui::menu::button(i), clickPos)) {
                        selectedMainIndex = i;
                        activateMainButton(i);
                        break;
                    }
                }
            } else if (state == MenuState::MODE_SELECT) {
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
                    selectedDifficultyIndex = std::max(0, std::min(2, gameSettings().defaultBotDifficulty - 1)); // default from НАСТРОЙКИ (b-session)
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
