#include "../includes/UI_mainMenu.h"
#include "../includes/UI_controlsConfig.h"
#include "../includes/UI_text.h"
#include "../includes/UI_shot.h"
#include "../includes/UI_theme.h"
#include "../includes/UI_settings.h"
#include "../includes/UI_lang.h"
#include "../includes/UI_arcadeMode.h"
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
    : state(ArcadeMode::isEnabled() ? MenuState::PRESS_A_TO_START : MenuState::MAIN),
      requestPlay(false),
      requestQuit(false),
      selectedMainIndex(0),
      selectedModeIndex(0),
      selectedDifficultyIndex(1),
      selectedSettingsIndex(0),
      selectedBotDifficulty(BotDifficulty::NONE),
      volume(UI_settings::get().getVolume()),
      soundEffects(UI_settings::get().isSoundEffectsEnabled()),
      settingsDifficultyIndex(UI_settings::get().getBotDifficultyIndex()),
      fontLoaded(false) {
    Lang::load(UI_settings::get().getLanguage());
    if (font.openFromFile("assets/font.ttf") || font.openFromFile("font.ttf")) {
        fontLoaded = true;
    } else {
        std::cerr << "[UI_mainMenu] Warning: Failed to load assets/font.ttf\n";
    }
    if (arcadeFont.openFromFile("assets/PressStart2P.ttf")) {
        arcadeFontLoaded = true;
    }
    if (logoTexture.loadFromFile("assets/logo.png") || logoTexture.loadFromFile("logo.png")) {
        logoTexture.setSmooth(true);
        logoLoaded = true;
    } else {
        std::cerr << "[UI_mainMenu] Warning: Failed to load assets/logo.png (text title shown instead)\n";
    }
    if (blueControllerTexture.loadFromFile("assets/controller_blue.png") || blueControllerTexture.loadFromFile("assets/natisni_sin_buton.png")) {
        blueControllerTexture.setSmooth(true);
        blueControllerLoaded = true;
    }
    if (redControllerTexture.loadFromFile("assets/controller_red.png") || redControllerTexture.loadFromFile("assets/natisni_cherven_buton.png")) {
        redControllerTexture.setSmooth(true);
        redControllerLoaded = true;
    }
    if (arrowTexture.loadFromFile("assets/arrow.png") || arrowTexture.loadFromFile("assets/strelka.png")) {
        arrowTexture.setSmooth(true);
        arrowLoaded = true;
    }
    std::cout << "[UI_mainMenu] SFML Main Menu with Arcade Start & Joystick Calibration ready.\n";
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
        sf::Text& subtitle = ui::pooledText(font, toUtf8(Lang::tr("menu.subtitle")), fontsize::H2);
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

    // Arcade version returns to the attract screen (PRESS A TO START) after match; PC returns to MAIN
    state = ArcadeMode::isEnabled() ? MenuState::PRESS_A_TO_START : MenuState::MAIN;
    selectedMainIndex = 0;
    enterHeld = false;
    spaceHeld = false;
}

void UI_mainMenu::returnToMain() {
    state = ArcadeMode::isEnabled() ? MenuState::PRESS_A_TO_START : MenuState::MAIN;
    selectedMainIndex = 0;
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

void UI_mainMenu::drawPressAToStart(sf::RenderWindow& window) {
    drawHeader(window, true);

    const float screenWidth = VIRTUAL_WIDTH;
    const sf::Font& textFont = arcadeFontLoaded ? arcadeFont : font;

    bool isEn = (UI_settings::get().getLanguage() == "en");
    std::string promptText = isEn ? "PRESS [A] TO START" : "НАТИСНЕТЕ [A] ЗА СТАРТ";

    float t = animClock.getElapsedTime().asSeconds();
    float pulse = 0.5f + 0.5f * std::sin(t * 4.5f);
    std::uint8_t alpha = static_cast<std::uint8_t>(140 + 115 * pulse);

    if (arcadeFontLoaded || fontLoaded) {
        sf::Text& prompt = ui::pooledText(textFont, toUtf8(promptText), arcadeFontLoaded ? 26 : fontsize::H1);
        prompt.setFillColor(sf::Color(255, 235, 70, alpha));
        prompt.setStyle(sf::Text::Bold);
        sf::FloatRect pb = prompt.getLocalBounds();
        prompt.setPosition({ (screenWidth - pb.size.x) / 2.0f - pb.position.x, 460.0f - pb.position.y });
        ui::drawText(window, prompt);

        if (!ArcadeMode::isEnabled()) {
            std::string sub = isEn ? "OR PRESS [ENTER] / [SPACE] ON KEYBOARD" : "ИЛИ НАТИСНЕТЕ [ENTER] / [SPACE] НА КЛАВИАТУРАТА";
            sf::Text& subPrompt = ui::pooledText(fontLoaded ? font : textFont, toUtf8(sub), fontsize::Label);
            subPrompt.setFillColor(theme::TextMuted);
            sf::FloatRect sb = subPrompt.getLocalBounds();
            subPrompt.setPosition({ (screenWidth - sb.size.x) / 2.0f - sb.position.x, 530.0f - sb.position.y });
            ui::drawText(window, subPrompt);
        }
    }
}

void UI_mainMenu::drawCalibration(sf::RenderWindow& window, bool isBlue) {
    drawHeader(window, false);

    const float screenWidth = VIRTUAL_WIDTH;
    const sf::Font& textFont = arcadeFontLoaded ? arcadeFont : font;
    bool isEn = (UI_settings::get().getLanguage() == "en");

    std::string titleStr = isBlue
        ? (isEn ? "PLAYER 1: PRESS ANY BLUE BUTTON" : "ИГРАЧ 1: НАТИСНЕТЕ СИНИЯ БУТОН")
        : (isEn ? "PLAYER 2: PRESS ANY RED BUTTON" : "ИГРАЧ 2: НАТИСНЕТЕ ЧЕРВЕНИЯ БУТОН");

    std::string subStr = isBlue
        ? (isEn ? "Press a button to calibrate Player 1 joystick" : "Натиснете бутон за настройка на Играч 1 (син стик)")
        : (isEn ? "Press a button to calibrate Player 2 joystick" : "Натиснете бутон за настройка на Играч 2 (червен стик)");

    sf::Color titleColor = isBlue ? sf::Color(80, 180, 255) : sf::Color(255, 100, 100);

    if (arcadeFontLoaded || fontLoaded) {
        sf::Text& title = ui::pooledText(textFont, toUtf8(titleStr), arcadeFontLoaded ? 22 : fontsize::H1);
        title.setFillColor(titleColor);
        title.setStyle(sf::Text::Bold);
        sf::FloatRect tb = title.getLocalBounds();
        title.setPosition({ (screenWidth - tb.size.x) / 2.0f - tb.position.x, 185.0f - tb.position.y });
        ui::drawText(window, title);

        sf::Text& sub = ui::pooledText(fontLoaded ? font : textFont, toUtf8(subStr), fontsize::Body);
        sub.setFillColor(theme::TextSecondary);
        sf::FloatRect sb = sub.getLocalBounds();
        sub.setPosition({ (screenWidth - sb.size.x) / 2.0f - sb.position.x, 230.0f - sb.position.y });
        ui::drawText(window, sub);
    }

    const sf::Texture& ctrlTex = isBlue ? blueControllerTexture : redControllerTexture;
    bool ctrlLoaded = isBlue ? blueControllerLoaded : redControllerLoaded;

    float ctrlX = 0.0f;
    float ctrlY = 275.0f;
    float ctrlScale = 1.0f;

    if (ctrlLoaded) {
        sf::Vector2u size = ctrlTex.getSize();
        ctrlScale = 0.85f;
        float drawW = size.x * ctrlScale;
        ctrlX = (screenWidth - drawW) / 2.0f;
        sf::Sprite sprite(ctrlTex);
        sprite.setScale({ ctrlScale, ctrlScale });
        sprite.setPosition({ ctrlX, ctrlY });
        window.draw(sprite);

        if (arrowLoaded) {
            float arrowScale = 0.13f;
            sf::Sprite arrow(arrowTexture);
            arrow.setScale({ arrowScale, arrowScale });
            sf::Vector2u arrSize = arrowTexture.getSize();
            arrow.setOrigin({ arrSize.x / 2.0f, static_cast<float>(arrSize.y) });

            float targetX = ctrlX + 670.0f * ctrlScale;
            float targetY = ctrlY + 180.0f * ctrlScale;

            float bob = std::sin(animClock.getElapsedTime().asSeconds() * 6.0f) * 14.0f;
            arrow.setPosition({ targetX, targetY + bob });
            window.draw(arrow);
        }
    }

    if (fontLoaded && !ArcadeMode::isEnabled()) {
        std::string skipStr = isEn ? "[ENTER] / [SPACE] to skip (use keyboard)" : "[ENTER] / [SPACE] за пропускане (клавиатура)";
        sf::Text& skip = ui::pooledText(font, toUtf8(skipStr), fontsize::Label);
        skip.setFillColor(theme::TextMuted);
        sf::FloatRect skb = skip.getLocalBounds();
        skip.setPosition({ (screenWidth - skb.size.x) / 2.0f - skb.position.x, 810.0f - skb.position.y });
        ui::drawText(window, skip);
    }
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

    drawButton(window, playBtn, toUtf8(Lang::tr("menu.play")), defaultBtn, theme::GoodFill, whiteText, selectedMainIndex == 0);
    drawButton(window, settingsBtn, toUtf8(Lang::tr("menu.settings")), defaultBtn, theme::InfoFill, whiteText, selectedMainIndex == 1);
    drawButton(window, quitBtn, toUtf8(Lang::tr("menu.quit")), defaultBtn, theme::BadFill, whiteText, selectedMainIndex == 2);

    if (fontLoaded) {
        sf::Text& hint = ui::pooledText(font, toUtf8(Lang::tr("menu.hint_main")), fontsize::Label);
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

    drawButton(window, coopBtn, toUtf8(Lang::tr("menu.mode_coop")), defaultBtn, theme::GoodFill, whiteText, selectedModeIndex == 0);
    drawButton(window, singleBtn, toUtf8(Lang::tr("menu.mode_single")), defaultBtn, theme::InfoFill, whiteText, selectedModeIndex == 1);
    drawButton(window, backBtn, toUtf8(Lang::tr("common.back")), defaultBtn, theme::ButtonHover, whiteText, selectedModeIndex == 2);

    if (fontLoaded) {
        std::string desc = (selectedModeIndex == 0)
            ? Lang::tr("menu.mode_coop_desc")
            : (selectedModeIndex == 1)
                ? Lang::tr("menu.mode_single_desc")
                : Lang::tr("menu.mode_back_desc");

        sf::Text& tDesc = ui::pooledText(font, toUtf8(desc), fontsize::Body);
        tDesc.setFillColor(theme::TextSecondary);
        sf::FloatRect db = tDesc.getLocalBounds();
        tDesc.setPosition({ (screenWidth - db.size.x) / 2.0f, startY + 2.0f * spacing + 78.0f });
        ui::drawText(window, tDesc);

        sf::Text& hint = ui::pooledText(font, toUtf8(Lang::tr("menu.hint_mode")), fontsize::Label);
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

    drawButton(window, easyBtn, toUtf8(Lang::tr("menu.bot_easy")), defaultBtn, theme::GoodFill, whiteText, selectedDifficultyIndex == 0);
    drawButton(window, medBtn, toUtf8(Lang::tr("menu.bot_medium")), defaultBtn, theme::WarnFill, whiteText, selectedDifficultyIndex == 1);
    drawButton(window, hardBtn, toUtf8(Lang::tr("menu.bot_hard")), defaultBtn, theme::BadFill, whiteText, selectedDifficultyIndex == 2);
    drawButton(window, backBtn, toUtf8(Lang::tr("common.back")), defaultBtn, theme::ButtonHover, whiteText, selectedDifficultyIndex == 3);

    if (fontLoaded) {
        std::string desc = (selectedDifficultyIndex == 0)
            ? Lang::tr("menu.bot_easy_desc")
            : (selectedDifficultyIndex == 1)
                ? Lang::tr("menu.bot_medium_desc")
                : (selectedDifficultyIndex == 2)
                    ? Lang::tr("menu.bot_hard_desc")
                    : Lang::tr("menu.bot_back_desc");

        sf::Text& tDesc = ui::pooledText(font, toUtf8(desc), fontsize::Body);
        tDesc.setFillColor(theme::TextSecondary);
        sf::FloatRect db = tDesc.getLocalBounds();
        tDesc.setPosition({ (screenWidth - db.size.x) / 2.0f, startY + 3.0f * spacing + 70.0f });
        ui::drawText(window, tDesc);

        sf::Text& hint = ui::pooledText(font, toUtf8(Lang::tr("menu.hint_difficulty")), fontsize::Label);
        hint.setFillColor(theme::TextMuted);
        sf::FloatRect hb = hint.getLocalBounds();
        hint.setPosition({ (screenWidth - hb.size.x) / 2.0f, startY + 3.0f * spacing + 102.0f });
        ui::drawText(window, hint);
    }
}

void UI_mainMenu::drawSettingsMenu(sf::RenderWindow& window) {
    drawHeader(window);

    float screenWidth = VIRTUAL_WIDTH;
    float panelWidth = 580.0f;
    float panelHeight = 460.0f;
    float panelX = (screenWidth - panelWidth) / 2.0f;
    float panelY = 155.0f;

    sf::RectangleShape panel({ panelWidth, panelHeight });
    panel.setPosition({ panelX, panelY });
    panel.setFillColor(theme::withAlpha(theme::Panel, 245));
    panel.setOutlineThickness(2.0f);
    panel.setOutlineColor(theme::LineStrong);
    window.draw(panel);
    ui::lint::occlude(sf::FloatRect({ panelX, panelY }, { panelWidth, panelHeight }));

    sf::Vector2f mousePos = ui::pointerPos(window);

    if (fontLoaded) {
        sf::Text& sTitle = ui::pooledText(font, toUtf8(Lang::tr("settings.title")), fontsize::H1);
        sTitle.setStyle(sf::Text::Bold);
        sTitle.setFillColor(theme::TextPrimary);
        sf::FloatRect tb = sTitle.getLocalBounds();
        sTitle.setPosition({ panelX + (panelWidth - tb.size.x) / 2.0f, panelY + 16.0f });
        ui::drawText(window, sTitle);
    }

    std::string curLang = UI_settings::get().getLanguage();

    // Row 0: Language
    sf::FloatRect langRow({ panelX + 30.0f, panelY + 62.0f }, { panelWidth - 60.0f, 40.0f });
    bool langSelected = (selectedSettingsIndex == 0);
    sf::RectangleShape langHighlight(langRow.size);
    langHighlight.setPosition(langRow.position);
    langHighlight.setFillColor(langSelected ? theme::withAlpha(theme::CardSelected, 200) : sf::Color::Transparent);
    langHighlight.setOutlineThickness(langSelected ? 1.5f : 0.0f);
    langHighlight.setOutlineColor(theme::Focus);
    window.draw(langHighlight);

    if (fontLoaded) {
        std::string langLabel = (langSelected ? "> " : "  ") + Lang::tr("settings.language");
        sf::Text& tLang = ui::pooledText(font, toUtf8(langLabel), fontsize::H2);
        tLang.setFillColor(langSelected ? theme::TextPrimary : theme::TextSecondary);
        tLang.setPosition({ panelX + 45.0f, panelY + 68.0f });
        ui::drawText(window, tLang);
    }
    sf::FloatRect langBtn({ panelX + 280.0f, panelY + 66.0f }, { 190.0f, 32.0f });
    std::string langValText = (curLang == "bg") ? Lang::tr("settings.lang_bg") : Lang::tr("settings.lang_en");
    drawButton(window, langBtn, toUtf8(langValText),
               theme::Button, theme::ButtonHover, theme::TextPrimary, langSelected || langBtn.contains(mousePos));

    // Row 1: Volume
    sf::FloatRect volRow({ panelX + 30.0f, panelY + 110.0f }, { panelWidth - 60.0f, 40.0f });
    bool volSelected = (selectedSettingsIndex == 1);
    sf::RectangleShape volHighlight(volRow.size);
    volHighlight.setPosition(volRow.position);
    volHighlight.setFillColor(volSelected ? theme::withAlpha(theme::CardSelected, 200) : sf::Color::Transparent);
    volHighlight.setOutlineThickness(volSelected ? 1.5f : 0.0f);
    volHighlight.setOutlineColor(theme::Focus);
    window.draw(volHighlight);

    if (fontLoaded) {
        std::string volLabel = (volSelected ? "> " : "  ") + Lang::tr("settings.volume");
        sf::Text& tVol = ui::pooledText(font, toUtf8(volLabel), fontsize::H2);
        tVol.setFillColor(volSelected ? theme::TextPrimary : theme::TextSecondary);
        tVol.setPosition({ panelX + 45.0f, panelY + 116.0f });
        ui::drawText(window, tVol);

        sf::Text& volVal = ui::pooledText(font, toUtf8(std::to_string(volume) + "%"), fontsize::H2);
        volVal.setStyle(sf::Text::Bold);
        volVal.setFillColor(theme::TextPrimary);
        volVal.setPosition({ panelX + 345.0f, panelY + 116.0f });
        ui::drawText(window, volVal);
    }
    sf::FloatRect volDown({ panelX + 280.0f, panelY + 114.0f }, { 36.0f, 30.0f });
    sf::FloatRect volUp({ panelX + 434.0f, panelY + 114.0f }, { 36.0f, 30.0f });
    drawButton(window, volDown, "-", theme::Button, theme::ButtonHover, theme::TextPrimary, volDown.contains(mousePos));
    drawButton(window, volUp, "+", theme::Button, theme::ButtonHover, theme::TextPrimary, volUp.contains(mousePos));

    // Row 2: Sound FX
    sf::FloatRect sfxRow({ panelX + 30.0f, panelY + 158.0f }, { panelWidth - 60.0f, 40.0f });
    bool sfxSelected = (selectedSettingsIndex == 2);
    sf::RectangleShape sfxHighlight(sfxRow.size);
    sfxHighlight.setPosition(sfxRow.position);
    sfxHighlight.setFillColor(sfxSelected ? theme::withAlpha(theme::CardSelected, 200) : sf::Color::Transparent);
    sfxHighlight.setOutlineThickness(sfxSelected ? 1.5f : 0.0f);
    sfxHighlight.setOutlineColor(theme::Focus);
    window.draw(sfxHighlight);

    if (fontLoaded) {
        std::string sfxLabel = (sfxSelected ? "> " : "  ") + Lang::tr("settings.sfx");
        sf::Text& tSfx = ui::pooledText(font, toUtf8(sfxLabel), fontsize::H2);
        tSfx.setFillColor(sfxSelected ? theme::TextPrimary : theme::TextSecondary);
        tSfx.setPosition({ panelX + 45.0f, panelY + 164.0f });
        ui::drawText(window, tSfx);
    }
    sf::FloatRect sfxBtn({ panelX + 280.0f, panelY + 162.0f }, { 190.0f, 32.0f });
    drawButton(window, sfxBtn, toUtf8(soundEffects ? Lang::tr("settings.on") : Lang::tr("settings.off")),
               soundEffects ? theme::GoodFill : theme::BadFill,
               soundEffects ? theme::GoodFill : theme::BadFill, theme::TextPrimary, sfxSelected || sfxBtn.contains(mousePos));

    // Row 3: Difficulty
    sf::FloatRect diffRow({ panelX + 30.0f, panelY + 206.0f }, { panelWidth - 60.0f, 40.0f });
    bool diffSelected = (selectedSettingsIndex == 3);
    sf::RectangleShape diffHighlight(diffRow.size);
    diffHighlight.setPosition(diffRow.position);
    diffHighlight.setFillColor(diffSelected ? theme::withAlpha(theme::CardSelected, 200) : sf::Color::Transparent);
    diffHighlight.setOutlineThickness(diffSelected ? 1.5f : 0.0f);
    diffHighlight.setOutlineColor(theme::Focus);
    window.draw(diffHighlight);

    if (fontLoaded) {
        std::string diffLabel = (diffSelected ? "> " : "  ") + Lang::tr("settings.bot_difficulty");
        sf::Text& tDiff = ui::pooledText(font, toUtf8(diffLabel), fontsize::H2);
        tDiff.setFillColor(diffSelected ? theme::TextPrimary : theme::TextSecondary);
        tDiff.setPosition({ panelX + 45.0f, panelY + 212.0f });
        ui::drawText(window, tDiff);
    }
    std::string diffKeys[] = { "settings.diff_easy", "settings.diff_normal", "settings.diff_hard" };
    sf::FloatRect diffBtn({ panelX + 280.0f, panelY + 210.0f }, { 190.0f, 32.0f });
    drawButton(window, diffBtn, toUtf8(Lang::tr(diffKeys[settingsDifficultyIndex])),
               theme::Button, theme::ButtonHover, theme::TextPrimary, diffSelected || diffBtn.contains(mousePos));

    // Row 4: Controls Remapping
    sf::FloatRect ctrlRow({ panelX + 30.0f, panelY + 254.0f }, { panelWidth - 60.0f, 40.0f });
    bool ctrlSelected = (selectedSettingsIndex == 4);
    sf::RectangleShape ctrlHighlight(ctrlRow.size);
    ctrlHighlight.setPosition(ctrlRow.position);
    ctrlHighlight.setFillColor(ctrlSelected ? theme::withAlpha(theme::CardSelected, 200) : sf::Color::Transparent);
    ctrlHighlight.setOutlineThickness(ctrlSelected ? 1.5f : 0.0f);
    ctrlHighlight.setOutlineColor(theme::Focus);
    window.draw(ctrlHighlight);

    if (fontLoaded) {
        std::string ctrlLabel = (ctrlSelected ? "> " : "  ") + Lang::tr("settings.controls");
        sf::Text& tCtrl = ui::pooledText(font, toUtf8(ctrlLabel), fontsize::H2);
        tCtrl.setFillColor(ctrlSelected ? theme::TextPrimary : theme::TextSecondary);
        tCtrl.setPosition({ panelX + 45.0f, panelY + 260.0f });
        ui::drawText(window, tCtrl);
    }
    sf::FloatRect ctrlBtn({ panelX + 280.0f, panelY + 258.0f }, { 190.0f, 32.0f });
    drawButton(window, ctrlBtn, toUtf8(Lang::tr("settings.remap")),
               theme::Button, theme::ButtonHover, theme::TextPrimary, ctrlSelected || ctrlBtn.contains(mousePos));

    // Notes
    if (fontLoaded) {
        std::string notes[2] = {
            Lang::tr("settings.note_audio"),
            Lang::tr("settings.note_difficulty")
        };
        for (int i = 0; i < 2; ++i) {
            sf::Text& tNote = ui::pooledText(font, toUtf8(notes[i]), fontsize::Label);
            tNote.setFillColor(theme::TextMuted);
            sf::FloatRect nb = tNote.getLocalBounds();
            tNote.setPosition({ panelX + (panelWidth - nb.size.x) / 2.0f, panelY + 308.0f + i * 20.0f });
            ui::drawText(window, tNote);
        }
    }

    // Row 5: Back button
    sf::FloatRect backBtn({ panelX + (panelWidth - 220.0f) / 2.0f, panelY + 395.0f }, { 220.0f, 44.0f });
    bool backSelected = (selectedSettingsIndex == 5);
    drawButton(window, backBtn, toUtf8(Lang::tr("common.back")), theme::Button, theme::ButtonHover, theme::TextPrimary, backSelected || backBtn.contains(mousePos));

    // Hints
    if (fontLoaded) {
        sf::Text& hint = ui::pooledText(font, toUtf8(Lang::tr("settings.hint")), fontsize::Label);
        hint.setFillColor(theme::TextMuted);
        sf::FloatRect hb = hint.getLocalBounds();
        hint.setPosition({ (screenWidth - hb.size.x) / 2.0f, panelY + panelHeight + 12.0f });
        ui::drawText(window, hint);
    }
}

void UI_mainMenu::drawControlsRemapMenu(sf::RenderWindow& window) {
    drawHeader(window);

    float screenWidth = VIRTUAL_WIDTH;
    float panelWidth = 1260.0f;
    float panelHeight = 580.0f;
    float panelX = (screenWidth - panelWidth) / 2.0f;
    float panelY = 160.0f;

    sf::RectangleShape panel({ panelWidth, panelHeight });
    panel.setPosition({ panelX, panelY });
    panel.setFillColor(theme::withAlpha(theme::Panel, 245));
    panel.setOutlineThickness(2.0f);
    panel.setOutlineColor(theme::LineStrong);
    window.draw(panel);
    ui::lint::occlude(sf::FloatRect({ panelX, panelY }, { panelWidth, panelHeight }));

    sf::Vector2f mousePos = ui::pointerPos(window);

    if (fontLoaded) {
        sf::Text& title = ui::pooledText(font, toUtf8("НАСТРОЙКИ: ПРЕНАЗНАЧАВАНЕ НА УПРАВЛЕНИЕТО"), fontsize::H1);
        title.setStyle(sf::Text::Bold);
        title.setFillColor(theme::TextPrimary);
        sf::FloatRect tb = title.getLocalBounds();
        title.setPosition({ panelX + (panelWidth - tb.size.x) / 2.0f, panelY + 14.0f });
        ui::drawText(window, title);

        sf::Text& sub = ui::pooledText(font, toUtf8(isRebinding ? "НАТИСНЕТЕ ЖЕЛАН КЛАВИШ ОТ КЛАВИАТУРАТА (ИЛИ ESC ЗА ОТКАЗ)" : "Изберете контрол с [Enter] или клик, след което натиснете новия клавиш"), fontsize::Body);
        sub.setFillColor(isRebinding ? theme::Gold : theme::TextSecondary);
        if (isRebinding) sub.setStyle(sf::Text::Bold);
        sf::FloatRect sb = sub.getLocalBounds();
        sub.setPosition({ panelX + (panelWidth - sb.size.x) / 2.0f, panelY + 46.0f });
        ui::drawText(window, sub);
    }

    // Divider
    sf::RectangleShape div({ panelWidth - 60.0f, 2.0f });
    div.setPosition({ panelX + 30.0f, panelY + 72.0f });
    div.setFillColor(theme::Line);
    window.draw(div);

    float colWidth = (panelWidth - 90.0f) / 2.0f; // 585.0f
    float startY = panelY + 84.0f;

    UI_controlsConfig& cfg = UI_controlsConfig::get();

    for (int p = 1; p <= 2; ++p) {
        float colX = (p == 1) ? (panelX + 30.0f) : (panelX + 60.0f + colWidth);
        sf::Color playerColor = (p == 1) ? theme::P1 : theme::P2;
        sf::Color playerColorLight = (p == 1) ? theme::P1Light : theme::P2Light;

        // Player section title
        if (fontLoaded) {
            std::string pTitle = (p == 1) ? "ИГРАЧ 1 (ЗАПАДЕН СЕКТОР)" : "ИГРАЧ 2 (ИЗТОЧЕН СЕКТОР)";
            sf::Text& colTitle = ui::pooledText(font, toUtf8(pTitle), fontsize::H2);
            colTitle.setStyle(sf::Text::Bold);
            colTitle.setFillColor(playerColorLight);
            colTitle.setPosition({ colX + 10.0f, startY });
            ui::drawText(window, colTitle);
        }

        float rowsStartY = startY + 34.0f;
        float rowHeight = 33.0f;
        float rowSpacing = 37.0f;

        const PlayerBindings& binds = cfg.getPlayer(p);

        for (int a = 0; a < static_cast<int>(ControlAction::COUNT); ++a) {
            ControlAction actionType = static_cast<ControlAction>(a);
            sf::FloatRect rowRect({ colX, rowsStartY + a * rowSpacing }, { colWidth, rowHeight });

            bool isRowSelected = (!isRebinding && remapSelectedPlayer == p && remapSelectedRow == a);
            bool isRowHovered = (!isRebinding && rowRect.contains(mousePos));
            bool isCurrentRebinding = (isRebinding && rebindPlayer == p && rebindActionIndex == a);

            sf::RectangleShape rowBg(rowRect.size);
            rowBg.setPosition(rowRect.position);
            if (isCurrentRebinding) {
                float pulse = 0.5f + 0.5f * std::sin(rebindPulseClock.getElapsedTime().asSeconds() * 8.0f);
                rowBg.setFillColor(theme::withAlpha(theme::Gold, static_cast<std::uint8_t>(40 + 50 * pulse)));
                rowBg.setOutlineThickness(2.0f);
                rowBg.setOutlineColor(theme::Gold);
            } else if (isRowSelected || isRowHovered) {
                rowBg.setFillColor(theme::withAlpha(playerColor, 40));
                rowBg.setOutlineThickness(1.5f);
                rowBg.setOutlineColor(playerColorLight);
            } else {
                rowBg.setFillColor(theme::withAlpha(theme::Card, 110));
                rowBg.setOutlineThickness(1.0f);
                rowBg.setOutlineColor(theme::withAlpha(theme::Line, 120));
            }
            window.draw(rowBg);

            // Action label
            if (fontLoaded) {
                std::string actionName = getControlActionNameBg(actionType);
                if (isRowSelected) actionName = "> " + actionName;
                sf::Text& actText = ui::pooledText(font, toUtf8(actionName), fontsize::Body);
                actText.setFillColor((isRowSelected || isCurrentRebinding) ? theme::TextPrimary : theme::TextSecondary);
                if (isRowSelected || isCurrentRebinding) actText.setStyle(sf::Text::Bold);
                actText.setPosition({ rowRect.position.x + 12.0f, rowRect.position.y + 5.0f });
                ui::drawText(window, actText);
            }

            // Key badge on the right
            sf::Keyboard::Key assignedKey = binds.getKey(actionType);
            std::string keyName = keyToString(assignedKey);

            float badgeWidth = isCurrentRebinding ? 175.0f : 115.0f;
            float badgeHeight = 25.0f;
            sf::FloatRect badgeRect({ rowRect.position.x + rowRect.size.x - badgeWidth - 8.0f, rowRect.position.y + 4.0f },
                                    { badgeWidth, badgeHeight });

            sf::RectangleShape badge(badgeRect.size);
            badge.setPosition(badgeRect.position);
            badge.setFillColor(isCurrentRebinding ? theme::Gold : (isRowSelected ? playerColor : theme::Button));
            badge.setOutlineThickness(1.0f);
            badge.setOutlineColor(isCurrentRebinding ? sf::Color::White : theme::Line);
            window.draw(badge);

            if (fontLoaded) {
                std::string badgeLabel = isCurrentRebinding ? "[ ВЪВЕДЕТЕ... ]" : ("[" + keyName + "]");
                sf::Text& bText = ui::pooledText(font, toUtf8(badgeLabel), fontsize::Label);
                bText.setStyle(sf::Text::Bold);
                bText.setFillColor(isCurrentRebinding ? sf::Color::Black : theme::TextPrimary);
                sf::FloatRect bb = bText.getLocalBounds();
                bText.setPosition({ badgeRect.position.x + (badgeWidth - bb.size.x) / 2.0f - bb.position.x,
                                    badgeRect.position.y + (badgeHeight - bb.size.y) / 2.0f - bb.position.y });
                ui::drawText(window, bText);
            }
        }
    }

    // Bottom action buttons
    float btnY = panelY + panelHeight - 64.0f;
    sf::FloatRect resetBtn({ panelX + (panelWidth - 520.0f) / 2.0f, btnY }, { 320.0f, 44.0f });
    sf::FloatRect backBtn({ panelX + (panelWidth - 520.0f) / 2.0f + 340.0f, btnY }, { 180.0f, 44.0f });

    bool resetSelected = (!isRebinding && remapSelectedRow == 9);
    bool backSelected = (!isRebinding && remapSelectedRow == 10);

    drawButton(window, resetBtn, toUtf8("ВЪЗСТАНОВИ ПО ПОДРАЗБИРАНЕ"),
               theme::Button, theme::ButtonHover, theme::TextPrimary, resetSelected || (!isRebinding && resetBtn.contains(mousePos)));
    drawButton(window, backBtn, toUtf8("НАЗАД"),
               theme::Button, theme::ButtonHover, theme::TextPrimary, backSelected || (!isRebinding && backBtn.contains(mousePos)));

    if (fontLoaded) {
        std::string joyStatus = cfg.getJoystickStatusBg();
        sf::Text& joyText = ui::pooledText(font, toUtf8(joyStatus), fontsize::Label);
        joyText.setFillColor(cfg.isAnyJoystickConnected() ? theme::Good : theme::TextSecondary);
        sf::FloatRect jb = joyText.getLocalBounds();
        joyText.setPosition({ (screenWidth - jb.size.x) / 2.0f, panelY + panelHeight + 8.0f });
        ui::drawText(window, joyText);

        sf::Text& hint = ui::pooledText(font, toUtf8("Навигация: [W/S/A/D, Стрелки, Аркаден стик] | Избор: [Enter / Бутон 1] | Отказ: [Esc / Бутон 2]"), fontsize::Label);
        hint.setFillColor(theme::TextMuted);
        sf::FloatRect hb = hint.getLocalBounds();
        hint.setPosition({ (screenWidth - hb.size.x) / 2.0f, panelY + panelHeight + 26.0f });
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
        } else if (playControls.isQuitRequested()) {
            playControls.resetRequests();
            onQuit();
        }
        return;
    }

    bool isUp = false;
    bool isDown = false;
    bool isLeft = false;
    bool isRight = false;
    bool isSelect = false;
    bool isEscape = false;

    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        isUp = (key->code == sf::Keyboard::Key::Up || key->code == sf::Keyboard::Key::W);
        isDown = (key->code == sf::Keyboard::Key::Down || key->code == sf::Keyboard::Key::S);
        isLeft = (key->code == sf::Keyboard::Key::Left || key->code == sf::Keyboard::Key::A);
        isRight = (key->code == sf::Keyboard::Key::Right || key->code == sf::Keyboard::Key::D);
        isSelect = (key->code == sf::Keyboard::Key::Enter || key->code == sf::Keyboard::Key::Space);
        isEscape = (key->code == sf::Keyboard::Key::Escape);
    }
    if (const auto* jb = event.getIf<sf::Event::JoystickButtonPressed>()) {
        if (jb->button == 0 || jb->button == 7) isSelect = true; // Button 1 (A) or Start
        if (jb->button == 1 || jb->button == 6) isEscape = true; // Button 2 (B) or Back / Coin
    }
    if (const auto* jm = event.getIf<sf::Event::JoystickMoved>()) {
        if (jm->axis == sf::Joystick::Axis::Y || jm->axis == sf::Joystick::Axis::PovY) {
            if (jm->position < -55.0f) isUp = true;
            else if (jm->position > 55.0f) isDown = true;
        } else if (jm->axis == sf::Joystick::Axis::X || jm->axis == sf::Joystick::Axis::PovX) {
            if (jm->position < -55.0f) isLeft = true;
            else if (jm->position > 55.0f) isRight = true;
        }
    }

    if (isUp || isDown || isLeft || isRight || isSelect || isEscape) {

        if (state == MenuState::PRESS_A_TO_START) {
            state = MenuState::CALIBRATE_BLUE;
            enterHeld = true;
            spaceHeld = true;
            return;
        } else if (state == MenuState::CALIBRATE_BLUE) {
            if (const auto* jb = event.getIf<sf::Event::JoystickButtonPressed>()) {
                UI_controlsConfig::get().setPlayerJoystick(1, jb->joystickId);
                std::cout << "[Calibration] Player 1 assigned to Joystick " << jb->joystickId << "\n";
            } else {
                UI_controlsConfig::get().setPlayerJoystick(1, 0);
            }
            state = MenuState::CALIBRATE_RED;
            enterHeld = true;
            spaceHeld = true;
            return;
        } else if (state == MenuState::CALIBRATE_RED) {
            if (const auto* jb = event.getIf<sf::Event::JoystickButtonPressed>()) {
                UI_controlsConfig::get().setPlayerJoystick(2, jb->joystickId);
                std::cout << "[Calibration] Player 2 assigned to Joystick " << jb->joystickId << "\n";
            } else {
                UI_controlsConfig::get().setPlayerJoystick(2, 1);
            }
            enterHeld = true;
            spaceHeld = true;

            // Arcade version: directly enter the game (2P Co-op match) without options or menus!
            selectedBotDifficulty = BotDifficulty::NONE;
            playControls.setActiveSchemeIndex(static_cast<int>(ControlScheme::DEVHUB_ARCADE));
            onPlay();
            return;
        } else if (state == MenuState::MAIN) {
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
                selectedSettingsIndex = (selectedSettingsIndex + 5) % 6;
            } else if (isDown) {
                selectedSettingsIndex = (selectedSettingsIndex + 1) % 6;
            } else if (isLeft) {
                if (selectedSettingsIndex == 0) {
                    std::string nextLang = (UI_settings::get().getLanguage() == "bg") ? "en" : "bg";
                    UI_settings::get().setLanguage(nextLang);
                } else if (selectedSettingsIndex == 1) {
                    if (volume >= 10) {
                        volume -= 10;
                        UI_settings::get().setVolume(volume);
                    }
                } else if (selectedSettingsIndex == 2) {
                    soundEffects = !soundEffects;
                    UI_settings::get().setSoundEffectsEnabled(soundEffects);
                } else if (selectedSettingsIndex == 3) {
                    settingsDifficultyIndex = (settingsDifficultyIndex + 2) % 3;
                    UI_settings::get().setBotDifficultyIndex(settingsDifficultyIndex);
                }
            } else if (isRight) {
                if (selectedSettingsIndex == 0) {
                    std::string nextLang = (UI_settings::get().getLanguage() == "bg") ? "en" : "bg";
                    UI_settings::get().setLanguage(nextLang);
                } else if (selectedSettingsIndex == 1) {
                    if (volume <= 90) {
                        volume += 10;
                        UI_settings::get().setVolume(volume);
                    }
                } else if (selectedSettingsIndex == 2) {
                    soundEffects = !soundEffects;
                    UI_settings::get().setSoundEffectsEnabled(soundEffects);
                } else if (selectedSettingsIndex == 3) {
                    settingsDifficultyIndex = (settingsDifficultyIndex + 1) % 3;
                    UI_settings::get().setBotDifficultyIndex(settingsDifficultyIndex);
                }
            } else if (isSelect) {
                if (selectedSettingsIndex == 0) {
                    std::string nextLang = (UI_settings::get().getLanguage() == "bg") ? "en" : "bg";
                    UI_settings::get().setLanguage(nextLang);
                } else if (selectedSettingsIndex == 1) {
                    volume = (volume >= 100) ? 0 : std::min(100, volume + 10);
                    UI_settings::get().setVolume(volume);
                } else if (selectedSettingsIndex == 2) {
                    soundEffects = !soundEffects;
                    UI_settings::get().setSoundEffectsEnabled(soundEffects);
                } else if (selectedSettingsIndex == 3) {
                    settingsDifficultyIndex = (settingsDifficultyIndex + 1) % 3;
                    UI_settings::get().setBotDifficultyIndex(settingsDifficultyIndex);
                } else if (selectedSettingsIndex == 4) {
                    state = MenuState::SETTINGS_CONTROLS;
                    remapSelectedPlayer = 1;
                    remapSelectedRow = 0;
                    isRebinding = false;
                } else if (selectedSettingsIndex == 5) {
                    state = MenuState::MAIN;
                }
            }
        } else if (state == MenuState::SETTINGS_CONTROLS) {
            if (isRebinding) {
                if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
                    if (key->code == sf::Keyboard::Key::Escape) {
                        isRebinding = false;
                    } else if (key->code != sf::Keyboard::Key::Unknown) {
                        UI_controlsConfig::get().getPlayer(rebindPlayer).setKey(static_cast<ControlAction>(rebindActionIndex), key->code);
                        isRebinding = false;
                    }
                }
                return;
            }
            if (isEscape) {
                state = MenuState::SETTINGS;
                selectedSettingsIndex = 3;
                return;
            }
            if (isUp) {
                if (remapSelectedRow > 0) remapSelectedRow--;
            } else if (isDown) {
                if (remapSelectedRow < 10) remapSelectedRow++;
            } else if (isLeft) {
                if (remapSelectedRow < 9) {
                    remapSelectedPlayer = 1;
                } else if (remapSelectedRow == 10) {
                    remapSelectedRow = 9;
                }
            } else if (isRight) {
                if (remapSelectedRow < 9) {
                    remapSelectedPlayer = 2;
                } else if (remapSelectedRow == 9) {
                    remapSelectedRow = 10;
                }
            } else if (isSelect) {
                if (remapSelectedRow < 9) {
                    isRebinding = true;
                    rebindPlayer = remapSelectedPlayer;
                    rebindActionIndex = remapSelectedRow;
                    rebindPulseClock.restart();
                } else if (remapSelectedRow == 9) {
                    UI_controlsConfig::get().resetToDefaults();
                } else if (remapSelectedRow == 10) {
                    state = MenuState::SETTINGS;
                    selectedSettingsIndex = 3;
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
                float panelWidth = 580.0f;
                float panelX = (screenWidth - panelWidth) / 2.0f;
                float panelY = 155.0f;

                if (isPointInside({ { panelX + 280.0f, panelY + 66.0f }, { 190.0f, 32.0f } }, clickPos) ||
                    isPointInside({ { panelX + 30.0f, panelY + 62.0f }, { panelWidth - 60.0f, 40.0f } }, clickPos)) {
                    std::string nextLang = (UI_settings::get().getLanguage() == "bg") ? "en" : "bg";
                    UI_settings::get().setLanguage(nextLang);
                    selectedSettingsIndex = 0;
                } else if (isPointInside({ { panelX + 280.0f, panelY + 114.0f }, { 36.0f, 30.0f } }, clickPos)) {
                    if (volume >= 10) {
                        volume -= 10;
                        UI_settings::get().setVolume(volume);
                    }
                    selectedSettingsIndex = 1;
                } else if (isPointInside({ { panelX + 434.0f, panelY + 114.0f }, { 36.0f, 30.0f } }, clickPos)) {
                    if (volume <= 90) {
                        volume += 10;
                        UI_settings::get().setVolume(volume);
                    }
                    selectedSettingsIndex = 1;
                } else if (isPointInside({ { panelX + 280.0f, panelY + 162.0f }, { 190.0f, 32.0f } }, clickPos) ||
                           isPointInside({ { panelX + 30.0f, panelY + 158.0f }, { panelWidth - 60.0f, 40.0f } }, clickPos)) {
                    soundEffects = !soundEffects;
                    UI_settings::get().setSoundEffectsEnabled(soundEffects);
                    selectedSettingsIndex = 2;
                } else if (isPointInside({ { panelX + 280.0f, panelY + 210.0f }, { 190.0f, 32.0f } }, clickPos) ||
                           isPointInside({ { panelX + 30.0f, panelY + 206.0f }, { panelWidth - 60.0f, 40.0f } }, clickPos)) {
                    settingsDifficultyIndex = (settingsDifficultyIndex + 1) % 3;
                    UI_settings::get().setBotDifficultyIndex(settingsDifficultyIndex);
                    selectedSettingsIndex = 3;
                } else if (isPointInside({ { panelX + 280.0f, panelY + 258.0f }, { 190.0f, 32.0f } }, clickPos) ||
                           isPointInside({ { panelX + 30.0f, panelY + 254.0f }, { panelWidth - 60.0f, 40.0f } }, clickPos)) {
                    state = MenuState::SETTINGS_CONTROLS;
                    remapSelectedPlayer = 1;
                    remapSelectedRow = 0;
                    isRebinding = false;
                    selectedSettingsIndex = 4;
                } else if (isPointInside({ { panelX + (panelWidth - 220.0f) / 2.0f, panelY + 395.0f }, { 220.0f, 44.0f } }, clickPos)) {
                    state = MenuState::MAIN;
                }
            } else if (state == MenuState::SETTINGS_CONTROLS) {
                float panelWidth = 1260.0f;
                float panelHeight = 580.0f;
                float panelX = (screenWidth - panelWidth) / 2.0f;
                float panelY = 160.0f;
                float colWidth = (panelWidth - 90.0f) / 2.0f;
                float startY = panelY + 84.0f;
                float rowsStartY = startY + 34.0f;
                float rowHeight = 33.0f;
                float rowSpacing = 37.0f;

                // Check clicks on control rows
                for (int p = 1; p <= 2; ++p) {
                    float colX = (p == 1) ? (panelX + 30.0f) : (panelX + 60.0f + colWidth);
                    for (int a = 0; a < static_cast<int>(ControlAction::COUNT); ++a) {
                        sf::FloatRect rowRect({ colX, rowsStartY + a * rowSpacing }, { colWidth, rowHeight });
                        if (isPointInside(rowRect, clickPos)) {
                            isRebinding = true;
                            rebindPlayer = p;
                            rebindActionIndex = a;
                            remapSelectedPlayer = p;
                            remapSelectedRow = a;
                            rebindPulseClock.restart();
                            return;
                        }
                    }
                }

                // Check bottom buttons
                float btnY = panelY + panelHeight - 64.0f;
                sf::FloatRect resetBtn({ panelX + (panelWidth - 520.0f) / 2.0f, btnY }, { 320.0f, 44.0f });
                sf::FloatRect backBtn({ panelX + (panelWidth - 520.0f) / 2.0f + 340.0f, btnY }, { 180.0f, 44.0f });

                if (isPointInside(resetBtn, clickPos)) {
                    UI_controlsConfig::get().resetToDefaults();
                    remapSelectedRow = 9;
                } else if (isPointInside(backBtn, clickPos)) {
                    state = MenuState::SETTINGS;
                    selectedSettingsIndex = 3;
                }
            }
        }
    }
}

void UI_mainMenu::render(sf::RenderWindow& window) {
    if (state == MenuState::PRESS_A_TO_START) {
        drawPressAToStart(window);
    } else if (state == MenuState::CALIBRATE_BLUE) {
        drawCalibration(window, true);
    } else if (state == MenuState::CALIBRATE_RED) {
        drawCalibration(window, false);
    } else if (state == MenuState::MAIN) {
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
    } else if (state == MenuState::SETTINGS_CONTROLS) {
        drawControlsRemapMenu(window);
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
