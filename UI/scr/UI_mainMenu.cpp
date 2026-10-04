#include "../includes/UI_mainMenu.h"
#include "../includes/UI_text.h"
#include "../includes/UI_shot.h"
#include "../includes/UI_theme.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace {
// Header logo layout (px on the 1600x900 canvas)
constexpr float MAIN_LOGO_TOP = 58.0f;
constexpr float MAIN_LOGO_HEIGHT = 280.0f;
constexpr float SUB_LOGO_TOP = 10.0f;
constexpr float SUB_LOGO_HEIGHT = 116.0f;
// Top-level menu buttons (drawn and clicked with the same rectangles)
constexpr float MAIN_BTN_W = 320.0f;
constexpr float MAIN_BTN_H = 54.0f;
constexpr float MAIN_BTN_Y[3] = { 404.0f, 480.0f, 556.0f };

sf::FloatRect mainButtonRect(int index) {
    return sf::FloatRect({ (VIRTUAL_WIDTH - MAIN_BTN_W) / 2.0f, MAIN_BTN_Y[index] }, { MAIN_BTN_W, MAIN_BTN_H });
}
} // namespace

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
    if (logoTexture.loadFromFile("assets/logo.png")) {
        logoTexture.setSmooth(true);
        logoLoaded = true;
    } else {
        std::cerr << "[UI_mainMenu] Warning: Failed to load assets/logo.png (text title shown instead)\n";
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
    shape.setOutlineColor(isSelected ? theme::Focus : theme::Line);
    window.draw(shape);

    if (fontLoaded) {
        sf::String displayText = isSelected ? (toUtf8("> ") + text + toUtf8(" <")) : text;
        sf::Text& label = ui::pooledText(font, displayText, fontsize::H2);
        label.setFillColor(isSelected ? theme::TextPrimary : textColor);
        if (isSelected) label.setStyle(sf::Text::Bold);

        sf::FloatRect textBounds = label.getLocalBounds();
        label.setPosition({
            bounds.position.x + (bounds.size.x - textBounds.size.x) / 2.0f - textBounds.position.x,
            bounds.position.y + (bounds.size.y - textBounds.size.y) / 2.0f - textBounds.position.y
        });
        ui::drawText(window, label, bounds);
    }
}

void UI_mainMenu::drawHeader(sf::RenderWindow& window, bool large) {
    const float screenWidth = VIRTUAL_WIDTH;
    // Large logo on the top-level menu; compact on submenus (the control-scheme card starts at y = 170)
    const float logoTop = large ? MAIN_LOGO_TOP : SUB_LOGO_TOP;
    const float logoH = large ? MAIN_LOGO_HEIGHT : SUB_LOGO_HEIGHT;
    float subtitleY = logoTop + logoH + 10.0f;

    if (logoLoaded) {
        sf::Vector2f texSize(logoTexture.getSize());
        float scale = logoH / texSize.y;
        sf::Vector2f center(screenWidth / 2.0f, logoTop + logoH / 2.0f);

        // Subtle breathing glow: two slightly larger, warm additive copies whose strength pulses
        float t = ui::shot::clockSeconds(animClock.getElapsedTime().asSeconds());
        float pulse = 0.5f + 0.5f * std::sin(t * 2.0f);
        for (int layer = 0; layer < 2; ++layer) {
            float grow = (layer == 0 ? 1.03f : 1.07f) + 0.012f * pulse;
            float alpha = (layer == 0 ? 34.0f : 16.0f) + (layer == 0 ? 30.0f : 16.0f) * pulse;
            sf::Sprite glow(logoTexture);
            glow.setOrigin(texSize / 2.0f);
            glow.setPosition(center);
            glow.setScale({ scale * grow, scale * grow });
            glow.setColor(theme::withAlpha(theme::Energy, static_cast<std::uint8_t>(alpha)));
            window.draw(glow, sf::RenderStates(sf::BlendAdd));
        }

        sf::Sprite logo(logoTexture);
        logo.setOrigin(texSize / 2.0f);
        logo.setPosition(center);
        logo.setScale({ scale, scale });
        window.draw(logo);
    } else if (fontLoaded) {
        // Fallback when assets/logo.png is missing: the old text title
        sf::Text& title = ui::pooledText(font, "ENERGY CRISIS", fontsize::Display);
        title.setFillColor(theme::Energy);
        sf::FloatRect tb = title.getLocalBounds();
        title.setPosition({ (screenWidth - tb.size.x) / 2.0f - tb.position.x, logoTop + (logoH - tb.size.y) / 2.0f - tb.position.y });
        ui::drawText(window, title);
    }

    if (fontLoaded) {
        // Subtitle
        sf::Text& subtitle = ui::pooledText(font, toUtf8("УПРАВЛЕНИЕ НА ЕНЕРГИЙНАТА МРЕЖА И РЕСУРСИТЕ"), fontsize::H2);
        subtitle.setFillColor(theme::TextSecondary);
        sf::FloatRect subBounds = subtitle.getLocalBounds();
        subtitle.setPosition({ (screenWidth - subBounds.size.x) / 2.0f, subtitleY });
        ui::drawText(window, subtitle);

        // Decorative line
        sf::RectangleShape line({ subBounds.size.x + 80.0f, 2.0f });
        line.setPosition({ (screenWidth - (subBounds.size.x + 80.0f)) / 2.0f, subtitleY + 26.0f });
        line.setFillColor(theme::withAlpha(theme::Line, 180));
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
    selectedSettingsIndex = 0;
}

void UI_mainMenu::onQuit() {
    std::cout << "[UI_mainMenu] Exiting game...\n";
    requestQuit = true;
}

void UI_mainMenu::drawMainMenu(sf::RenderWindow& window) {
    drawHeader(window, true);

    float screenWidth = VIRTUAL_WIDTH;

    sf::Vector2f mousePos = ui::pointerPos(window);
    sf::FloatRect playBtn = mainButtonRect(0);
    sf::FloatRect settingsBtn = mainButtonRect(1);
    sf::FloatRect quitBtn = mainButtonRect(2);

    bool mouseMoved = (std::abs(mousePos.x - lastMenuMousePos.x) > 2.0f || std::abs(mousePos.y - lastMenuMousePos.y) > 2.0f);
    if (mouseMoved) {
        lastMenuMousePos = mousePos;
        if (playBtn.contains(mousePos)) selectedMainIndex = 0;
        else if (settingsBtn.contains(mousePos)) selectedMainIndex = 1;
        else if (quitBtn.contains(mousePos)) selectedMainIndex = 2;
    }

    const sf::Color defaultBtn = theme::Button;
    const sf::Color whiteText = theme::TextPrimary;

    drawButton(window, playBtn, toUtf8("ИГРАЙ"), defaultBtn, theme::GoodFill, whiteText, selectedMainIndex == 0);
    drawButton(window, settingsBtn, toUtf8("НАСТРОЙКИ"), defaultBtn, theme::InfoFill, whiteText, selectedMainIndex == 1);
    drawButton(window, quitBtn, toUtf8("ИЗХОД"), defaultBtn, theme::BadFill, whiteText, selectedMainIndex == 2);

    if (fontLoaded) {
        sf::Text& hint = ui::pooledText(font, toUtf8("Навигация: [Стрелки / W,S] | Избор: [Enter]"), fontsize::Label);
        hint.setFillColor(theme::TextMuted);
        sf::FloatRect hb = hint.getLocalBounds();
        hint.setPosition({ (screenWidth - hb.size.x) / 2.0f, MAIN_BTN_Y[2] + MAIN_BTN_H + 34.0f });
        ui::drawText(window, hint);
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

    sf::Vector2f mousePos = ui::pointerPos(window);
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

    const sf::Color defaultBtn = theme::Button;
    const sf::Color whiteText = theme::TextPrimary;

    drawButton(window, coopBtn, toUtf8("ДВАМА ИГРАЧИ (1 СРЕЩУ 1)"), defaultBtn, theme::GoodFill, whiteText, selectedModeIndex == 0);
    drawButton(window, singleBtn, toUtf8("ЕДИН ИГРАЧ (СРЕЩУ БОТ)"), defaultBtn, theme::InfoFill, whiteText, selectedModeIndex == 1);
    drawButton(window, backBtn, toUtf8("НАЗАД"), defaultBtn, theme::ButtonHover, whiteText, selectedModeIndex == 2);

    if (fontLoaded) {
        std::string desc = (selectedModeIndex == 0)
            ? "Двама играчи се състезават на един компютър: западен и източен сектор"
            : (selectedModeIndex == 1)
                ? "Играйте срещу компютърен противник (ботът управлява източния сектор)"
                : "Връщане към главното меню";

        sf::Text& tDesc = ui::pooledText(font, toUtf8(desc), fontsize::Body);
        tDesc.setFillColor(theme::TextSecondary);
        sf::FloatRect db = tDesc.getLocalBounds();
        tDesc.setPosition({ (screenWidth - db.size.x) / 2.0f, startY + 2.0f * spacing + 78.0f });
        ui::drawText(window, tDesc);

        sf::Text& hint = ui::pooledText(font, toUtf8("Навигация: [Стрелки / W,S] | Избор: [Enter / Space] | Отказ: [ESC]"), fontsize::Label);
        hint.setFillColor(theme::TextMuted);
        sf::FloatRect hb = hint.getLocalBounds();
        hint.setPosition({ (screenWidth - hb.size.x) / 2.0f, startY + 2.0f * spacing + 112.0f });
        ui::drawText(window, hint);
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

    sf::Vector2f mousePos = ui::pointerPos(window);
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

    const sf::Color defaultBtn = theme::Button;
    const sf::Color whiteText = theme::TextPrimary;

    drawButton(window, easyBtn, toUtf8("ЛЕСЕН БОТ"), defaultBtn, theme::GoodFill, whiteText, selectedDifficultyIndex == 0);
    drawButton(window, medBtn, toUtf8("СРЕДЕН БОТ"), defaultBtn, theme::WarnFill, whiteText, selectedDifficultyIndex == 1);
    drawButton(window, hardBtn, toUtf8("ТРУДЕН БОТ"), defaultBtn, theme::BadFill, whiteText, selectedDifficultyIndex == 2);
    drawButton(window, backBtn, toUtf8("НАЗАД"), defaultBtn, theme::ButtonHover, whiteText, selectedDifficultyIndex == 3);

    if (fontLoaded) {
        std::string desc = (selectedDifficultyIndex == 0)
            ? "По-бавен бот; строи базови солари и турбини (подходящ за учене)"
            : (selectedDifficultyIndex == 1)
                ? "Балансиран бот; събира ресурси, строи батерии и нощни лампи"
                : (selectedDifficultyIndex == 2)
                    ? "Бърз и агресивен бот; купува земя, ъпгрейдва мини и оптимизира ток"
                    : "Връщане към избор на режим";

        sf::Text& tDesc = ui::pooledText(font, toUtf8(desc), fontsize::Body);
        tDesc.setFillColor(theme::TextSecondary);
        sf::FloatRect db = tDesc.getLocalBounds();
        tDesc.setPosition({ (screenWidth - db.size.x) / 2.0f, startY + 3.0f * spacing + 70.0f });
        ui::drawText(window, tDesc);

        sf::Text& hint = ui::pooledText(font, toUtf8("Навигация: [Стрелки / W,S] | Старт: [Enter / Space] | Отказ: [ESC]"), fontsize::Label);
        hint.setFillColor(theme::TextMuted);
        sf::FloatRect hb = hint.getLocalBounds();
        hint.setPosition({ (screenWidth - hb.size.x) / 2.0f, startY + 3.0f * spacing + 102.0f });
        ui::drawText(window, hint);
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
    panel.setFillColor(theme::withAlpha(theme::Panel, 245));
    panel.setOutlineThickness(2.0f);
    panel.setOutlineColor(theme::LineStrong);
    window.draw(panel);
    ui::lint::occlude(sf::FloatRect({ panelX, panelY }, { panelWidth, panelHeight }));

    sf::Vector2f mousePos = ui::pointerPos(window);

    if (fontLoaded) {
        sf::Text& sTitle = ui::pooledText(font, toUtf8("НАСТРОЙКИ"), fontsize::H1);
        sTitle.setStyle(sf::Text::Bold);
        sTitle.setFillColor(theme::TextPrimary);
        sf::FloatRect tb = sTitle.getLocalBounds();
        sTitle.setPosition({ panelX + (panelWidth - tb.size.x) / 2.0f, panelY + 16.0f });
        ui::drawText(window, sTitle);
    }

    // Row 0: Volume
    sf::FloatRect volRow({ panelX + 30.0f, panelY + 70.0f }, { panelWidth - 60.0f, 42.0f });
    bool volSelected = (selectedSettingsIndex == 0);
    sf::RectangleShape volHighlight(volRow.size);
    volHighlight.setPosition(volRow.position);
    volHighlight.setFillColor(volSelected ? theme::withAlpha(theme::CardSelected, 200) : sf::Color::Transparent);
    volHighlight.setOutlineThickness(volSelected ? 1.5f : 0.0f);
    volHighlight.setOutlineColor(theme::Focus);
    window.draw(volHighlight);

    if (fontLoaded) {
        sf::Text& tVol = ui::pooledText(font, toUtf8(volSelected ? "> Сила на звука:" : "  Сила на звука:"), fontsize::H2);
        tVol.setFillColor(volSelected ? theme::TextPrimary : theme::TextSecondary);
        tVol.setPosition({ panelX + 45.0f, panelY + 78.0f });
        ui::drawText(window, tVol);

        sf::Text& volVal = ui::pooledText(font, toUtf8(std::to_string(volume) + "%"), fontsize::H2);
        volVal.setStyle(sf::Text::Bold);
        volVal.setFillColor(theme::TextPrimary);
        volVal.setPosition({ panelX + 345.0f, panelY + 78.0f });
        ui::drawText(window, volVal);
    }
    sf::FloatRect volDown({ panelX + 280.0f, panelY + 74.0f }, { 36.0f, 32.0f });
    sf::FloatRect volUp({ panelX + 420.0f, panelY + 74.0f }, { 36.0f, 32.0f });
    drawButton(window, volDown, "-", theme::Button, theme::ButtonHover, theme::TextPrimary, volDown.contains(mousePos));
    drawButton(window, volUp, "+", theme::Button, theme::ButtonHover, theme::TextPrimary, volUp.contains(mousePos));

    // Row 1: Sound FX
    sf::FloatRect sfxRow({ panelX + 30.0f, panelY + 125.0f }, { panelWidth - 60.0f, 42.0f });
    bool sfxSelected = (selectedSettingsIndex == 1);
    sf::RectangleShape sfxHighlight(sfxRow.size);
    sfxHighlight.setPosition(sfxRow.position);
    sfxHighlight.setFillColor(sfxSelected ? theme::withAlpha(theme::CardSelected, 200) : sf::Color::Transparent);
    sfxHighlight.setOutlineThickness(sfxSelected ? 1.5f : 0.0f);
    sfxHighlight.setOutlineColor(theme::Focus);
    window.draw(sfxHighlight);

    if (fontLoaded) {
        sf::Text& tSfx = ui::pooledText(font, toUtf8(sfxSelected ? "> Звукови ефекти:" : "  Звукови ефекти:"), fontsize::H2);
        tSfx.setFillColor(sfxSelected ? theme::TextPrimary : theme::TextSecondary);
        tSfx.setPosition({ panelX + 45.0f, panelY + 133.0f });
        ui::drawText(window, tSfx);
    }
    sf::FloatRect sfxBtn({ panelX + 280.0f, panelY + 129.0f }, { 180.0f, 34.0f });
    drawButton(window, sfxBtn, toUtf8(soundEffects ? "ВКЛЮЧЕНИ" : "ИЗКЛЮЧЕНИ"),
               soundEffects ? theme::GoodFill : theme::BadFill,
               soundEffects ? theme::GoodFill : theme::BadFill, theme::TextPrimary, sfxSelected || sfxBtn.contains(mousePos));

    // Row 2: Difficulty
    sf::FloatRect diffRow({ panelX + 30.0f, panelY + 180.0f }, { panelWidth - 60.0f, 42.0f });
    bool diffSelected = (selectedSettingsIndex == 2);
    sf::RectangleShape diffHighlight(diffRow.size);
    diffHighlight.setPosition(diffRow.position);
    diffHighlight.setFillColor(diffSelected ? theme::withAlpha(theme::CardSelected, 200) : sf::Color::Transparent);
    diffHighlight.setOutlineThickness(diffSelected ? 1.5f : 0.0f);
    diffHighlight.setOutlineColor(theme::Focus);
    window.draw(diffHighlight);

    if (fontLoaded) {
        sf::Text& tDiff = ui::pooledText(font, toUtf8(diffSelected ? "> Трудност на бота:" : "  Трудност на бота:"), fontsize::H2);
        tDiff.setFillColor(diffSelected ? theme::TextPrimary : theme::TextSecondary);
        tDiff.setPosition({ panelX + 45.0f, panelY + 188.0f });
        ui::drawText(window, tDiff);
    }
    const char* diffLabels[] = { "ЛЕСЕН", "СРЕДЕН", "ТРУДЕН" };
    sf::FloatRect diffBtn({ panelX + 280.0f, panelY + 184.0f }, { 180.0f, 34.0f });
    drawButton(window, diffBtn, toUtf8(diffLabels[settingsDifficultyIndex]),
               theme::Button, theme::ButtonHover, theme::TextPrimary, diffSelected || diffBtn.contains(mousePos));

    // Notes: audio is synthesised in-game ([b-effects]); the difficulty is only the default bot choice
    if (fontLoaded) {
        const char* notes[] = {
            "Звукът и музиката се синтезират в играта; промените важат веднага.",
            "Трудността на бота е избраната по подразбиране в ЕДИН ИГРАЧ."
        };
        for (int i = 0; i < 2; ++i) {
            sf::Text& tNote = ui::pooledText(font, toUtf8(notes[i]), fontsize::Label);
            tNote.setFillColor(theme::TextMuted);
            sf::FloatRect nb = tNote.getLocalBounds();
            tNote.setPosition({ panelX + (panelWidth - nb.size.x) / 2.0f, panelY + 234.0f + i * 20.0f });
            ui::drawText(window, tNote);
        }
    }

    // Row 3: Back button
    sf::FloatRect backBtn({ panelX + (panelWidth - 220.0f) / 2.0f, panelY + 285.0f }, { 220.0f, 46.0f });
    bool backSelected = (selectedSettingsIndex == 3);
    drawButton(window, backBtn, toUtf8("НАЗАД"), theme::Button, theme::ButtonHover, theme::TextPrimary, backSelected || backBtn.contains(mousePos));

    // Hints
    if (fontLoaded) {
        sf::Text& hint = ui::pooledText(font, toUtf8("Навигация: [W/S или Стрелки] | Промяна: [A/D или Enter]"), fontsize::Label);
        hint.setFillColor(theme::TextMuted);
        sf::FloatRect hb = hint.getLocalBounds();
        hint.setPosition({ (screenWidth - hb.size.x) / 2.0f, panelY + panelHeight + 14.0f });
        ui::drawText(window, hint);
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
                if (isPointInside(mainButtonRect(0), clickPos)) {
                    state = MenuState::MODE_SELECT;
                    selectedModeIndex = 0;
                } else if (isPointInside(mainButtonRect(1), clickPos)) {
                    onSettings();
                } else if (isPointInside(mainButtonRect(2), clickPos)) {
                    onQuit();
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

void UI_mainMenu::showState(MenuState s) {
    state = s;
    selectedMainIndex = 0;
    selectedModeIndex = 0;
    selectedDifficultyIndex = settingsDifficultyIndex;
    selectedSettingsIndex = 0;
    if (s == MenuState::PLAY_CONTROLS) playControls.resetRequests();
}
