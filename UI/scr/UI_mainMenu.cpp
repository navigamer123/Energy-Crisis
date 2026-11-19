#include "../includes/UI_mainMenu.h"
#include "../includes/UI_controlsConfig.h"
#include "../includes/UI_text.h"
#include "../includes/UI_shot.h"
#include "../includes/UI_theme.h"
#include "../includes/UI_settings.h"
#include "../includes/UI_lang.h"
#include "../includes/UI_arcadeMode.h"
#include "../includes/UI_credits.h"
#include "../includes/UI_arcadePopup.h"
#include "../includes/UI_ads.h"
#include <iostream>
#include <cmath>
#include <algorithm>

#if defined(__ANDROID__)
#include <android/log.h>
#define LOG_TAG "EnergyCrisisAds"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...)
#endif

namespace {
// Header logo layout (px on the 1600x900 canvas)
constexpr float MAIN_LOGO_TOP = 58.0f;
constexpr float MAIN_LOGO_HEIGHT = 280.0f;
constexpr float SUB_LOGO_TOP = 10.0f;
constexpr float SUB_LOGO_HEIGHT = 116.0f;

std::string toStdUtf8(const sf::String& s) {
    auto u8 = s.toUtf8();
    return std::string(u8.begin(), u8.end());
}

// Top-level menu buttons (420x68, drawn and clicked with the same rectangles)
constexpr float MAIN_BTN_W = 420.0f;
constexpr float MAIN_BTN_H = 68.0f;
constexpr float MAIN_BTN_Y[3] = { 414.0f, 502.0f, 590.0f };

sf::FloatRect mainButtonRect(int index) {
    return sf::FloatRect({ (VIRTUAL_WIDTH - MAIN_BTN_W) / 2.0f, MAIN_BTN_Y[index] }, { MAIN_BTN_W, MAIN_BTN_H });
}

// Match type menu (600x64 primary, 280x54 back)
sf::FloatRect matchTypeButtonRect(int index) {
    constexpr float btnW = 600.0f;
    constexpr float btnH = 64.0f;
    constexpr float backW = 280.0f;
    constexpr float backH = 54.0f;
    constexpr float startY = 230.0f;
    constexpr float spacing = 86.0f;
    if (index == 2) {
        return sf::FloatRect({ (VIRTUAL_WIDTH - backW) / 2.0f, startY + 2.0f * spacing + 14.0f }, { backW, backH });
    }
    return sf::FloatRect({ (VIRTUAL_WIDTH - btnW) / 2.0f, startY + index * spacing }, { btnW, btnH });
}

// Ad unlock prompt (600x64 primary, 280x54 back)
sf::FloatRect adUnlockButtonRect(int index) {
    constexpr float btnW = 600.0f;
    constexpr float btnH = 64.0f;
    constexpr float backW = 280.0f;
    constexpr float backH = 54.0f;
    constexpr float startY = 292.0f;
    constexpr float spacing = 84.0f;
    if (index == 2) {
        return sf::FloatRect({ (VIRTUAL_WIDTH - backW) / 2.0f, startY + 2.0f * spacing + 14.0f }, { backW, backH });
    }
    return sf::FloatRect({ (VIRTUAL_WIDTH - btnW) / 2.0f, startY + index * spacing }, { btnW, btnH });
}

// Mode select menu (600x64 primary, 280x54 back)
sf::FloatRect modeButtonRect(int index) {
    constexpr float btnW = 600.0f;
    constexpr float btnH = 64.0f;
    constexpr float backW = 280.0f;
    constexpr float backH = 54.0f;
    constexpr float startY = 210.0f;
    constexpr float spacing = 82.0f;
    if (index == 3) {
        return sf::FloatRect({ (VIRTUAL_WIDTH - backW) / 2.0f, startY + 3.0f * spacing + 14.0f }, { backW, backH });
    }
    return sf::FloatRect({ (VIRTUAL_WIDTH - btnW) / 2.0f, startY + index * spacing }, { btnW, btnH });
}

// Bot difficulty menu (520x64 primary, 280x54 back)
sf::FloatRect botDifficultyButtonRect(int index) {
    constexpr float btnW = 520.0f;
    constexpr float btnH = 64.0f;
    constexpr float backW = 280.0f;
    constexpr float backH = 54.0f;
    constexpr float startY = 210.0f;
    constexpr float spacing = 82.0f;
    if (index == 3) {
        return sf::FloatRect({ (VIRTUAL_WIDTH - backW) / 2.0f, startY + 3.0f * spacing + 14.0f }, { backW, backH });
    }
    return sf::FloatRect({ (VIRTUAL_WIDTH - btnW) / 2.0f, startY + index * spacing }, { btnW, btnH });
}

// Arcade start screen (480x66 buttons, controller slots below compact header)
sf::FloatRect arcadeSlotRect(int player) {
    float x = (player == 1) ? 60.0f : 1100.0f;
    return sf::FloatRect({ x, 220.0f }, { 440.0f, 460.0f });
}

sf::FloatRect arcadeStartButtonRect(int index) {
    constexpr float btnW = 480.0f;
    constexpr float btnH = 66.0f;
    constexpr float startY = 252.0f;
    constexpr float spacing = 88.0f;
    return sf::FloatRect({ (VIRTUAL_WIDTH - btnW) / 2.0f, startY + index * spacing }, { btnW, btnH });
}

// Arcade bot difficulty screen (480x66 buttons, 300x54 back)
sf::FloatRect arcadeBotButtonRect(int index) {
    constexpr float btnW = 480.0f;
    constexpr float btnH = 66.0f;
    constexpr float backW = 300.0f;
    constexpr float backH = 54.0f;
    constexpr float startY = 258.0f;
    constexpr float spacing = 82.0f;
    if (index == 3) {
        return sf::FloatRect({ (VIRTUAL_WIDTH - backW) / 2.0f, startY + 3.0f * spacing + 10.0f }, { backW, backH });
    }
    return sf::FloatRect({ (VIRTUAL_WIDTH - btnW) / 2.0f, startY + index * spacing }, { btnW, btnH });
}

// Settings panel (680x510, 48px rows, 220x40 controls, 48x40 volume -/+, 260x54 back)
sf::FloatRect settingsPanelRect() {
    constexpr float panelW = 680.0f;
    constexpr float panelH = 510.0f;
    return sf::FloatRect({ (VIRTUAL_WIDTH - panelW) / 2.0f, 180.0f }, { panelW, panelH });
}

sf::FloatRect settingsRowRect(int row) {
    sf::FloatRect panel = settingsPanelRect();
    constexpr float rowH = 48.0f;
    constexpr float spacing = 54.0f;
    return sf::FloatRect({ panel.position.x + 28.0f, panel.position.y + 66.0f + row * spacing },
                         { panel.size.x - 56.0f, rowH });
}

sf::FloatRect settingsRowButtonRect(int row) {
    sf::FloatRect r = settingsRowRect(row);
    constexpr float btnW = 220.0f;
    constexpr float btnH = 40.0f;
    return sf::FloatRect({ r.position.x + r.size.x - btnW - 12.0f, r.position.y + (r.size.y - btnH) / 2.0f },
                         { btnW, btnH });
}

sf::FloatRect settingsVolDownRect() {
    sf::FloatRect area = settingsRowButtonRect(1);
    return sf::FloatRect(area.position, { 48.0f, 40.0f });
}

sf::FloatRect settingsVolUpRect() {
    sf::FloatRect area = settingsRowButtonRect(1);
    return sf::FloatRect({ area.position.x + area.size.x - 48.0f, area.position.y }, { 48.0f, 40.0f });
}

sf::FloatRect settingsBackButtonRect() {
    sf::FloatRect panel = settingsPanelRect();
    constexpr float backW = 260.0f;
    constexpr float backH = 54.0f;
    return sf::FloatRect({ panel.position.x + (panel.size.x - backW) / 2.0f,
                           panel.position.y + panel.size.y - backH - 22.0f },
                         { backW, backH });
}

// Controls remap panel (1280x614, 40px rows, 380x54 Reset, 220x54 Back)
sf::FloatRect remapPanelRect() {
    constexpr float panelW = 1280.0f;
    constexpr float panelH = 614.0f;
    return sf::FloatRect({ (VIRTUAL_WIDTH - panelW) / 2.0f, 176.0f }, { panelW, panelH });
}

sf::FloatRect remapRowRect(int player, int actionIdx) {
    sf::FloatRect panel = remapPanelRect();
    float colWidth = (panel.size.x - 90.0f) / 2.0f;
    float colX = (player == 1) ? (panel.position.x + 30.0f) : (panel.position.x + 60.0f + colWidth);
    float rowsStartY = panel.position.y + 112.0f;
    constexpr float rowHeight = 40.0f;
    constexpr float rowSpacing = 45.0f;
    return sf::FloatRect({ colX, rowsStartY + actionIdx * rowSpacing }, { colWidth, rowHeight });
}

sf::FloatRect remapResetButtonRect() {
    sf::FloatRect panel = remapPanelRect();
    constexpr float resetW = 380.0f;
    constexpr float backW = 220.0f;
    constexpr float gap = 24.0f;
    constexpr float btnH = 54.0f;
    float startX = panel.position.x + (panel.size.x - (resetW + gap + backW)) / 2.0f;
    float btnY = panel.position.y + panel.size.y - btnH - 22.0f;
    return sf::FloatRect({ startX, btnY }, { resetW, btnH });
}

sf::FloatRect remapBackButtonRect() {
    sf::FloatRect reset = remapResetButtonRect();
    return sf::FloatRect({ reset.position.x + reset.size.x + 24.0f, reset.position.y }, { 220.0f, 54.0f });
}
} // namespace

UI_mainMenu::UI_mainMenu()
    : state(ArcadeMode::isEnabled() ? MenuState::PRESS_A_TO_START : MenuState::MAIN),
      requestPlay(false),
      requestQuit(false),
      selectedMainIndex(0),
      selectedMatchTypeIndex(0),
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
#if defined(__ANDROID__)
    sf::FloatRect padded({ bounds.position.x - 12.0f, bounds.position.y - 12.0f },
                         { bounds.size.x + 24.0f, bounds.size.y + 24.0f });
    return padded.contains(pt);
#else
    return bounds.contains(pt);
#endif
}

void UI_mainMenu::drawButton(sf::RenderTarget& window, sf::FloatRect bounds, const sf::String& text,
                             sf::Color baseColor, sf::Color hoverColor, sf::Color textColor,
                             bool isSelected) {
    sf::RectangleShape shadow(bounds.size);
    shadow.setPosition({ bounds.position.x + 2.0f, bounds.position.y + 3.0f });
    shadow.setFillColor(sf::Color(0, 0, 0, 110));
    window.draw(shadow);

    sf::RectangleShape shape(bounds.size);
    shape.setPosition(bounds.position);
    shape.setFillColor(isSelected ? hoverColor : baseColor);
    shape.setOutlineThickness(isSelected ? 3.5f : 2.0f);
    shape.setOutlineColor(isSelected ? theme::Focus : theme::LineStrong);
    window.draw(shape);

    if (fontLoaded) {
        sf::String displayText = isSelected ? (toUtf8("> ") + text + toUtf8(" <")) : text;
        float pad = (bounds.size.x < 80.0f) ? 12.0f : 28.0f;
        float maxTextW = std::max(16.0f, bounds.size.x - pad);
        unsigned int textSize = ui::fitTextSize(font, toStdUtf8(displayText), 20, 14, maxTextW, true);
        sf::Text& label = ui::pooledText(font, displayText, textSize);
        label.setStyle(sf::Text::Bold);
        label.setFillColor(isSelected ? theme::TextPrimary : textColor);

        sf::FloatRect textBounds = label.getLocalBounds();
        label.setPosition({
            bounds.position.x + (bounds.size.x - textBounds.size.x) / 2.0f - textBounds.position.x,
            bounds.position.y + (bounds.size.y - textBounds.size.y) / 2.0f - textBounds.position.y
        });
        ui::drawText(window, label, bounds);
    }
}

void UI_mainMenu::drawHeader(sf::RenderTarget& window, bool large) {
    const float screenWidth = VIRTUAL_WIDTH;
    // Large logo on the top-level menu; compact on submenus (submenus start below y = 164)
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
        subtitle.setPosition({ (screenWidth - subBounds.size.x) / 2.0f - subBounds.position.x, subtitleY - subBounds.position.y });
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
    arcadeSelectedModeIndex = 0;
    arcadeSelectedDifficultyIndex = 1;
    enterHeld = false;
    spaceHeld = false;
}

void UI_mainMenu::returnToMain() {
    botVsBotMode = false;
    state = ArcadeMode::isEnabled() ? MenuState::PRESS_A_TO_START : MenuState::MAIN;
    selectedMainIndex = 0;
    selectedMatchTypeIndex = 0;
    arcadeSelectedModeIndex = 0;
    arcadeSelectedDifficultyIndex = 1;
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

void UI_mainMenu::drawPressAToStart(sf::RenderTarget& window) {
    drawHeader(window, false);

    const float screenWidth = VIRTUAL_WIDTH;
    const sf::Font& textFont = arcadeFontLoaded ? arcadeFont : font;

    bool isEn = (UI_settings::get().getLanguage() == "en");

    float t = ui::shot::clockSeconds(animClock.getElapsedTime().asSeconds());
    float pulse = 0.5f + 0.5f * std::sin(t * 4.5f);
    std::uint8_t alpha = static_cast<std::uint8_t>(140 + 115 * pulse);

    // 1. Player 1 (Blue Player) Layout Guide on Left
    {
        sf::FloatRect p1Slot = arcadeSlotRect(1);
        sf::RectangleShape p1Box(p1Slot.size);
        p1Box.setPosition(p1Slot.position);
        p1Box.setFillColor(theme::withAlpha(theme::Window, 225));
        p1Box.setOutlineThickness(2.0f);
        p1Box.setOutlineColor(theme::withAlpha(sf::Color(80, 180, 255), 200));
        window.draw(p1Box);
        ui::lint::ContainerScope p1Scope(p1Slot);

        std::string p1Title = isEn ? "PLAYER 1 (BLUE)" : "ИГРАЧ 1 (СИН)";
        sf::Text& tP1 = ui::pooledText(fontLoaded ? font : textFont, toUtf8(p1Title), fontsize::H2);
        tP1.setStyle(sf::Text::Bold);
        tP1.setFillColor(sf::Color(80, 180, 255));
        sf::FloatRect tb1 = tP1.getLocalBounds();
        tP1.setPosition({ p1Slot.position.x + (p1Slot.size.x - tb1.size.x) / 2.0f - tb1.position.x, p1Slot.position.y + 18.0f - tb1.position.y });
        ui::drawText(window, tP1);

        if (blueControllerLoaded) {
            sf::Sprite p1Spr(blueControllerTexture);
            float scale = 390.0f / blueControllerTexture.getSize().x;
            p1Spr.setScale({ scale, scale });
            p1Spr.setPosition({ p1Slot.position.x + 25.0f, p1Slot.position.y + 72.0f });
            window.draw(p1Spr);
        }

        std::string p1Desc = isEn ? "STICK: Move  |  A: Action\nC: Upgrade  |  D: Building  |  B: Cancel"
                                  : "СТИК: Движение  |  A: Действие\nC: Ъпгрейд  |  D: Сграда  |  B: Отказ";
        sf::Text& tDesc1 = ui::pooledText(fontLoaded ? font : textFont, toUtf8(p1Desc), fontsize::Label);
        tDesc1.setFillColor(theme::TextSecondary);
        sf::FloatRect db1 = tDesc1.getLocalBounds();
        tDesc1.setPosition({ p1Slot.position.x + (p1Slot.size.x - db1.size.x) / 2.0f - db1.position.x,
                             p1Slot.position.y + 315.0f + (125.0f - db1.size.y) / 2.0f - db1.position.y });
        ui::drawText(window, tDesc1);
    }

    // 2. Player 2 (Red Player / Bot) Layout Guide on Right
    {
        sf::FloatRect p2Slot = arcadeSlotRect(2);
        sf::RectangleShape p2Box(p2Slot.size);
        p2Box.setPosition(p2Slot.position);
        p2Box.setFillColor(theme::withAlpha(theme::Window, 225));
        p2Box.setOutlineThickness(2.0f);
        p2Box.setOutlineColor(theme::withAlpha(sf::Color(255, 100, 100), 200));
        window.draw(p2Box);
        ui::lint::ContainerScope p2Scope(p2Slot);

        std::string p2Title = (arcadeSelectedModeIndex == 1)
            ? (isEn ? "PLAYER 2 (BOT)" : "ИГРАЧ 2 (РОБОТ)")
            : (isEn ? "PLAYER 2 (RED)" : "ИГРАЧ 2 (ЧЕРВЕН)");
        sf::Text& tP2 = ui::pooledText(fontLoaded ? font : textFont, toUtf8(p2Title), fontsize::H2);
        tP2.setStyle(sf::Text::Bold);
        tP2.setFillColor(sf::Color(255, 100, 100));
        sf::FloatRect tb2 = tP2.getLocalBounds();
        tP2.setPosition({ p2Slot.position.x + (p2Slot.size.x - tb2.size.x) / 2.0f - tb2.position.x, p2Slot.position.y + 18.0f - tb2.position.y });
        ui::drawText(window, tP2);

        if (redControllerLoaded) {
            sf::Sprite p2Spr(redControllerTexture);
            float scale = 390.0f / redControllerTexture.getSize().x;
            p2Spr.setScale({ scale, scale });
            p2Spr.setPosition({ p2Slot.position.x + 25.0f, p2Slot.position.y + 72.0f });
            window.draw(p2Spr);
        }

        std::string p2Desc = (arcadeSelectedModeIndex == 1)
            ? (isEn ? "AI OPPONENT (BOT)\n3 DIFFICULTIES AVAILABLE" : "АВТОМАТИЧЕН БОТ (ИИ)\n3 НАЛИЧНИ ТРУДНОСТИ")
            : (isEn ? "STICK: Move  |  A: Action\nC: Upgrade  |  D: Building  |  B: Cancel"
                    : "СТИК: Движение  |  A: Действие\nC: Ъпгрейд  |  D: Сграда  |  B: Отказ");
        sf::Text& tDesc2 = ui::pooledText(fontLoaded ? font : textFont, toUtf8(p2Desc), fontsize::Label);
        tDesc2.setFillColor(theme::TextSecondary);
        sf::FloatRect db2 = tDesc2.getLocalBounds();
        tDesc2.setPosition({ p2Slot.position.x + (p2Slot.size.x - db2.size.x) / 2.0f - db2.position.x,
                             p2Slot.position.y + 315.0f + (125.0f - db2.size.y) / 2.0f - db2.position.y });
        ui::drawText(window, tDesc2);
    }

    // 3. Center Arcade Menu (Player vs Player / Player vs Bot)
    if (arcadeFontLoaded || fontLoaded) {
        sf::FloatRect pvpBtn = arcadeStartButtonRect(0);
        sf::FloatRect pvbBtn = arcadeStartButtonRect(1);
        sf::FloatRect centerBounds({ 510.0f, 220.0f }, { 580.0f, 480.0f });

        std::string pvpText = isEn ? "Player vs Player" : "Играч срещу играч";
        std::string pvbText = isEn ? "Player vs Bot" : "Играч срещу робот";

        drawButton(window, pvpBtn, toUtf8(pvpText), theme::Button, theme::GoodFill, theme::TextPrimary, arcadeSelectedModeIndex == 0);
        drawButton(window, pvbBtn, toUtf8(pvbText), theme::Button, theme::InfoFill, theme::TextPrimary, arcadeSelectedModeIndex == 1);

        // Blinking text below them: "press a to start"
        std::string promptText = isEn ? "PRESS A TO START" : "НАТИСНИ A ЗА ДА СТАРТИРАШ";
        unsigned int promptSz = ui::fitTextSize(textFont, promptText, arcadeFontLoaded ? 20 : fontsize::H1, 12, 560.0f, true);
        sf::Text& prompt = ui::pooledText(textFont, toUtf8(promptText), promptSz);
        prompt.setFillColor(sf::Color(255, 235, 70, alpha));
        prompt.setStyle(sf::Text::Bold);
        sf::FloatRect pb = prompt.getLocalBounds();
        prompt.setPosition({ (screenWidth - pb.size.x) / 2.0f - pb.position.x, 456.0f - pb.position.y });
        ui::drawText(window, prompt, centerBounds);

        const sf::Font& bodyFont = fontLoaded ? font : textFont;
        if (ArcadeMode::isEnabled()) {
            int creds = CreditsManager::get().getCurrentCredits();
            std::string credStr = isEn ? ("CREDITS: " + std::to_string(creds)) : ("КРЕДИТИ: " + std::to_string(creds));
            unsigned int credSz = ui::fitTextSize(bodyFont, credStr, fontsize::Body, 11, 560.0f, true);
            sf::Text& crText = ui::pooledText(bodyFont, toUtf8(credStr), credSz);
            crText.setStyle(sf::Text::Bold);
            crText.setFillColor(creds > 0 ? theme::Gold : theme::Bad);
            sf::FloatRect crb = crText.getLocalBounds();
            crText.setPosition({ (screenWidth - crb.size.x) / 2.0f - crb.position.x, 524.0f - crb.position.y });
            ui::drawText(window, crText, centerBounds);
        } else {
            std::string sub = isEn ? "OR PRESS [ENTER] / [SPACE] ON KEYBOARD" : "ИЛИ НАТИСНЕТЕ [ENTER] / [SPACE] НА КЛАВИАТУРАТА";
            unsigned int subSz = ui::fitTextSize(bodyFont, sub, fontsize::Label, 11, 560.0f, false);
            sf::Text& subPrompt = ui::pooledText(bodyFont, toUtf8(sub), subSz);
            subPrompt.setFillColor(theme::TextMuted);
            sf::FloatRect sb = subPrompt.getLocalBounds();
            subPrompt.setPosition({ (screenWidth - sb.size.x) / 2.0f - sb.position.x, 524.0f - sb.position.y });
            ui::drawText(window, subPrompt, centerBounds);
        }

        std::string hintStr = isEn ? "[STICK]: Select Mode   |   [A]: Start" : "[СТИК]: Избор на режим   |   [A]: Старт";
        unsigned int hintSz = ui::fitTextSize(bodyFont, hintStr, fontsize::Label, 11, 560.0f, false);
        sf::Text& hintText = ui::pooledText(bodyFont, toUtf8(hintStr), hintSz);
        hintText.setFillColor(theme::TextMuted);
        sf::FloatRect hb = hintText.getLocalBounds();
        hintText.setPosition({ (screenWidth - hb.size.x) / 2.0f - hb.position.x, 584.0f - hb.position.y });
        ui::drawText(window, hintText, centerBounds);
    }
}

void UI_mainMenu::drawArcadeBotDifficulty(sf::RenderTarget& window) {
    drawHeader(window, false);

    const float screenWidth = VIRTUAL_WIDTH;
    const sf::Font& textFont = arcadeFontLoaded ? arcadeFont : font;
    const sf::Font& bodyFont = fontLoaded ? font : textFont;
    bool isEn = (UI_settings::get().getLanguage() == "en");

    float t = ui::shot::clockSeconds(animClock.getElapsedTime().asSeconds());
    float pulse = 0.5f + 0.5f * std::sin(t * 4.5f);
    std::uint8_t alpha = static_cast<std::uint8_t>(140 + 115 * pulse);

    // Title
    std::string titleStr = isEn ? "SELECT BOT DIFFICULTY" : "ИЗБЕРЕТЕ ТРУДНОСТ НА БОТА";
    unsigned int titleSz = ui::fitTextSize(bodyFont, titleStr, fontsize::H1, 16, 800.0f, true);
    sf::Text& titleText = ui::pooledText(bodyFont, toUtf8(titleStr), titleSz);
    titleText.setStyle(sf::Text::Bold);
    titleText.setFillColor(theme::Energy);
    sf::FloatRect tb = titleText.getLocalBounds();
    titleText.setPosition({ (screenWidth - tb.size.x) / 2.0f - tb.position.x, 200.0f - tb.position.y });
    ui::drawText(window, titleText);

    // 3 difficulty buttons + back button
    sf::FloatRect easyBtn = arcadeBotButtonRect(0);
    sf::FloatRect medBtn = arcadeBotButtonRect(1);
    sf::FloatRect hardBtn = arcadeBotButtonRect(2);
    sf::FloatRect backBtn = arcadeBotButtonRect(3);

    std::string easyText = isEn ? "EASY" : "ЛЕСЕН";
    std::string medText = isEn ? "MEDIUM" : "СРЕДЕН";
    std::string hardText = isEn ? "HARD" : "ТРУДЕН";
    std::string backText = isEn ? "BACK" : "НАЗАД";

    drawButton(window, easyBtn, toUtf8(easyText), theme::Button, theme::GoodFill, theme::TextPrimary, arcadeSelectedDifficultyIndex == 0);
    drawButton(window, medBtn, toUtf8(medText), theme::Button, theme::WarnFill, theme::TextPrimary, arcadeSelectedDifficultyIndex == 1);
    drawButton(window, hardBtn, toUtf8(hardText), theme::Button, theme::BadFill, theme::TextPrimary, arcadeSelectedDifficultyIndex == 2);
    drawButton(window, backBtn, toUtf8(backText), theme::Button, theme::ButtonHover, theme::TextPrimary, arcadeSelectedDifficultyIndex == 3);

    // Blinking prompt below them
    std::string promptText = isEn ? "PRESS A TO START" : "НАТИСНИ A ЗА ДА СТАРТИРАШ";
    unsigned int promptSz = ui::fitTextSize(textFont, promptText, arcadeFontLoaded ? 20 : fontsize::H1, 12, 720.0f, true);
    sf::Text& prompt = ui::pooledText(textFont, toUtf8(promptText), promptSz);
    prompt.setFillColor(sf::Color(255, 235, 70, alpha));
    prompt.setStyle(sf::Text::Bold);
    sf::FloatRect pb = prompt.getLocalBounds();
    prompt.setPosition({ (screenWidth - pb.size.x) / 2.0f - pb.position.x, 614.0f - pb.position.y });
    ui::drawText(window, prompt);

    // Hint
    std::string hintStr = isEn ? "[STICK]: Change   |   [A]: Start   |   [B]: Back" : "[СТИК]: Смяна   |   [A]: Старт   |   [B]: Назад";
    sf::Text& hintText = ui::pooledText(bodyFont, toUtf8(hintStr), fontsize::Label);
    hintText.setFillColor(theme::TextMuted);
    sf::FloatRect hb = hintText.getLocalBounds();
    hintText.setPosition({ (screenWidth - hb.size.x) / 2.0f - hb.position.x, 668.0f - hb.position.y });
    ui::drawText(window, hintText);
}

void UI_mainMenu::drawCalibration(sf::RenderTarget& window, bool isBlue) {
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
        unsigned int titleSz = ui::fitTextSize(textFont, titleStr, arcadeFontLoaded ? 22 : fontsize::H1, 14, 1400.0f, true);
        sf::Text& title = ui::pooledText(textFont, toUtf8(titleStr), titleSz);
        title.setFillColor(titleColor);
        title.setStyle(sf::Text::Bold);
        sf::FloatRect tb = title.getLocalBounds();
        title.setPosition({ (screenWidth - tb.size.x) / 2.0f - tb.position.x, 192.0f - tb.position.y });
        ui::drawText(window, title);

        sf::Text& sub = ui::pooledText(fontLoaded ? font : textFont, toUtf8(subStr), fontsize::Body);
        sub.setFillColor(theme::TextSecondary);
        sf::FloatRect sb = sub.getLocalBounds();
        sub.setPosition({ (screenWidth - sb.size.x) / 2.0f - sb.position.x, 236.0f - sb.position.y });
        ui::drawText(window, sub);
    }

    const sf::Texture& ctrlTex = isBlue ? blueControllerTexture : redControllerTexture;
    bool ctrlLoaded = isBlue ? blueControllerLoaded : redControllerLoaded;

    float ctrlX = 0.0f;
    float ctrlY = 278.0f;
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
            float arrowScale = 0.085f;
            sf::Sprite arrow(arrowTexture);
            arrow.setScale({ arrowScale, arrowScale });
            sf::Vector2u arrSize = arrowTexture.getSize();
            arrow.setOrigin({ arrSize.x * 0.49f, arrSize.y * 0.953f });

            // Point directly down at Button B (center-top of the A-F action button cluster)
            float targetX = ctrlX + 690.0f * ctrlScale;
            float targetY = ctrlY + 234.0f * ctrlScale;

            float bob = std::sin(ui::shot::clockSeconds(animClock.getElapsedTime().asSeconds()) * 6.0f) * 10.0f;
            arrow.setPosition({ targetX, targetY + bob });
            window.draw(arrow);
        }
    }

    if (fontLoaded && !ArcadeMode::isEnabled()) {
        std::string skipStr = isEn ? "[ENTER] / [SPACE] to skip (use keyboard)" : "[ENTER] / [SPACE] за пропускане (клавиатура)";
        sf::Text& skip = ui::pooledText(font, toUtf8(skipStr), fontsize::H2);
        skip.setFillColor(theme::TextSecondary);
        sf::FloatRect skb = skip.getLocalBounds();
        skip.setPosition({ (screenWidth - skb.size.x) / 2.0f - skb.position.x, 810.0f - skb.position.y });
        ui::drawText(window, skip);
    }
}

void UI_mainMenu::drawMainMenu(sf::RenderTarget& window) {
    drawHeader(window, true);

    const sf::Color defaultBtn = theme::Button;
    const sf::Color whiteText = theme::TextPrimary;

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

    drawButton(window, playBtn, toUtf8(Lang::tr("menu.play")), defaultBtn, theme::GoodFill, whiteText, selectedMainIndex == 0);
    drawButton(window, settingsBtn, toUtf8(Lang::tr("menu.settings")), defaultBtn, theme::InfoFill, whiteText, selectedMainIndex == 1);
    drawButton(window, quitBtn, toUtf8(Lang::tr("menu.quit")), defaultBtn, theme::BadFill, whiteText, selectedMainIndex == 2);

    if (fontLoaded) {
#if defined(__ANDROID__)
        sf::Text& hint = ui::pooledText(font, toUtf8(UI_settings::get().getLanguage() == "en" ? "Tap an option to select" : "Докоснете опция за избор"), fontsize::Label);
#else
        sf::Text& hint = ui::pooledText(font, toUtf8(Lang::tr("menu.hint_main")), fontsize::Label);
#endif
        hint.setFillColor(theme::TextMuted);
        sf::FloatRect hb = hint.getLocalBounds();
        hint.setPosition({ (screenWidth - hb.size.x) / 2.0f - hb.position.x, MAIN_BTN_Y[2] + MAIN_BTN_H + 36.0f });
        ui::drawText(window, hint);
    }
}

void UI_mainMenu::drawMatchTypeMenu(sf::RenderTarget& window) {
    drawHeader(window);

    float screenWidth = VIRTUAL_WIDTH;
    sf::Vector2f mousePos = ui::pointerPos(window);
    sf::FloatRect stdBtn = matchTypeButtonRect(0);
    sf::FloatRect blitzBtn = matchTypeButtonRect(1);
    sf::FloatRect backBtn = matchTypeButtonRect(2);

    bool mouseMoved = (std::abs(mousePos.x - lastMenuMousePos.x) > 2.0f || std::abs(mousePos.y - lastMenuMousePos.y) > 2.0f);
    if (mouseMoved) {
        lastMenuMousePos = mousePos;
        if (stdBtn.contains(mousePos)) selectedMatchTypeIndex = 0;
        else if (blitzBtn.contains(mousePos)) selectedMatchTypeIndex = 1;
        else if (backBtn.contains(mousePos)) selectedMatchTypeIndex = 2;
    }

    const sf::Color defaultBtn = theme::Button;
    const sf::Color whiteText = theme::TextPrimary;

    bool is30mUnlocked = AdManager::get().is30mUnlocked();
    std::string match30mLabel = is30mUnlocked ? Lang::tr("menu.match_30m") : Lang::tr("menu.match_30m_locked");

    drawButton(window, stdBtn, toUtf8(match30mLabel), defaultBtn, theme::GoodFill, whiteText, selectedMatchTypeIndex == 0);
    drawButton(window, blitzBtn, toUtf8(Lang::tr("menu.match_5m")), defaultBtn, sf::Color(140, 100, 20), whiteText, selectedMatchTypeIndex == 1);
    drawButton(window, backBtn, toUtf8(Lang::tr("common.back")), defaultBtn, theme::ButtonHover, whiteText, selectedMatchTypeIndex == 2);

    if (fontLoaded) {
        std::string desc = (selectedMatchTypeIndex == 0)
            ? (is30mUnlocked ? Lang::tr("menu.match_30m_desc") : Lang::tr("menu.ad_unlock_desc"))
            : (selectedMatchTypeIndex == 1)
                ? Lang::tr("menu.match_5m_desc")
                : Lang::tr("menu.mode_back_desc");

        sf::Text& descText = ui::pooledText(font, toUtf8(desc), fontsize::Body);
        descText.setFillColor(theme::TextSecondary);
        sf::FloatRect db = descText.getLocalBounds();
        descText.setPosition({ (screenWidth - db.size.x) / 2.0f - db.position.x, backBtn.position.y + backBtn.size.y + 36.0f });
        ui::drawText(window, descText);

#if defined(__ANDROID__)
        sf::Text& hint = ui::pooledText(font, toUtf8(UI_settings::get().getLanguage() == "en" ? "Tap a match type to continue" : "Докоснете тип игра за продължаване"), fontsize::Label);
#else
        sf::Text& hint = ui::pooledText(font, toUtf8(Lang::tr("menu.hint_mode")), fontsize::Label);
#endif
        hint.setFillColor(theme::TextMuted);
        sf::FloatRect hb = hint.getLocalBounds();
        hint.setPosition({ (screenWidth - hb.size.x) / 2.0f - hb.position.x, backBtn.position.y + backBtn.size.y + 74.0f });
        ui::drawText(window, hint);
    }
}

void UI_mainMenu::drawAdUnlockPrompt(sf::RenderTarget& window) {
    if (AdManager::get().pollRewardEarned()) {
        blitzMode = false;
        state = MenuState::MODE_SELECT;
        selectedModeIndex = 0;
        return;
    }

    drawHeader(window);

    float screenWidth = VIRTUAL_WIDTH;
    sf::Vector2f mousePos = ui::pointerPos(window);
    sf::FloatRect watchBtn = adUnlockButtonRect(0);
    sf::FloatRect blitzBtn = adUnlockButtonRect(1);
    sf::FloatRect backBtn = adUnlockButtonRect(2);

    bool mouseMoved = (std::abs(mousePos.x - lastMenuMousePos.x) > 2.0f || std::abs(mousePos.y - lastMenuMousePos.y) > 2.0f);
    if (mouseMoved) {
        lastMenuMousePos = mousePos;
        if (watchBtn.contains(mousePos)) selectedAdPromptIndex = 0;
        else if (blitzBtn.contains(mousePos)) selectedAdPromptIndex = 1;
        else if (backBtn.contains(mousePos)) selectedAdPromptIndex = 2;
    }

    if (fontLoaded) {
        // Title
        sf::Text& titleText = ui::pooledText(font, toUtf8(Lang::tr("menu.ad_unlock_title")), fontsize::H1);
        titleText.setStyle(sf::Text::Bold);
        titleText.setFillColor(theme::Gold);
        sf::FloatRect tb = titleText.getLocalBounds();
        titleText.setPosition({ (screenWidth - tb.size.x) / 2.0f - tb.position.x, 192.0f });
        ui::drawText(window, titleText);

        // Description
        sf::Text& descText = ui::pooledText(font, toUtf8(Lang::tr("menu.ad_unlock_desc")), fontsize::Body);
        descText.setFillColor(theme::TextSecondary);
        sf::FloatRect db = descText.getLocalBounds();
        descText.setPosition({ (screenWidth - db.size.x) / 2.0f - db.position.x, 238.0f });
        ui::drawText(window, descText);
    }

    const sf::Color defaultBtn = theme::Button;
    const sf::Color whiteText = theme::TextPrimary;

    drawButton(window, watchBtn, toUtf8(Lang::tr("menu.ad_watch_btn")), defaultBtn, theme::GoodFill, whiteText, selectedAdPromptIndex == 0);
    drawButton(window, blitzBtn, toUtf8(Lang::tr("menu.ad_free_blitz_btn")), defaultBtn, sf::Color(140, 100, 20), whiteText, selectedAdPromptIndex == 1);
    drawButton(window, backBtn, toUtf8(Lang::tr("common.back")), defaultBtn, theme::ButtonHover, whiteText, selectedAdPromptIndex == 2);

    if (fontLoaded) {
        const std::string& statusMsg = AdManager::get().getStatusMessage();
        if (!statusMsg.empty()) {
            sf::Text& statusText = ui::pooledText(font, toUtf8(statusMsg), fontsize::Body);
            statusText.setFillColor(theme::Warn);
            sf::FloatRect sb = statusText.getLocalBounds();
            statusText.setPosition({ (screenWidth - sb.size.x) / 2.0f - sb.position.x, backBtn.position.y + backBtn.size.y + 30.0f });
            ui::drawText(window, statusText);
        }
    }
}

void UI_mainMenu::drawModeSelectMenu(sf::RenderTarget& window) {
    drawHeader(window);

    float screenWidth = VIRTUAL_WIDTH;
    sf::Vector2f mousePos = ui::pointerPos(window);
    sf::FloatRect coopBtn = modeButtonRect(0);
    sf::FloatRect singleBtn = modeButtonRect(1);
    sf::FloatRect emuBtn = modeButtonRect(2);
    sf::FloatRect backBtn = modeButtonRect(3);

    bool mouseMoved = (std::abs(mousePos.x - lastMenuMousePos.x) > 2.0f || std::abs(mousePos.y - lastMenuMousePos.y) > 2.0f);
    if (mouseMoved) {
        lastMenuMousePos = mousePos;
        if (coopBtn.contains(mousePos)) selectedModeIndex = 0;
        else if (singleBtn.contains(mousePos)) selectedModeIndex = 1;
        else if (emuBtn.contains(mousePos)) selectedModeIndex = 2;
        else if (backBtn.contains(mousePos)) selectedModeIndex = 3;
    }

    const sf::Color defaultBtn = theme::Button;
    const sf::Color whiteText = theme::TextPrimary;

    drawButton(window, coopBtn, toUtf8(Lang::tr("menu.mode_coop")), defaultBtn, theme::GoodFill, whiteText, selectedModeIndex == 0);
    drawButton(window, singleBtn, toUtf8(Lang::tr("menu.mode_single")), defaultBtn, theme::InfoFill, whiteText, selectedModeIndex == 1);
    drawButton(window, emuBtn, toUtf8(Lang::tr("menu.mode_emulation")), defaultBtn, sf::Color(140, 100, 20), whiteText, selectedModeIndex == 2);
    drawButton(window, backBtn, toUtf8(Lang::tr("common.back")), defaultBtn, theme::ButtonHover, whiteText, selectedModeIndex == 3);

    if (fontLoaded) {
        std::string desc = (selectedModeIndex == 0)
            ? Lang::tr("menu.mode_coop_desc")
            : (selectedModeIndex == 1)
                ? Lang::tr("menu.mode_single_desc")
                : (selectedModeIndex == 2)
                    ? Lang::tr("menu.mode_emulation_desc")
                    : Lang::tr("menu.mode_back_desc");

        sf::Text& tDesc = ui::pooledText(font, toUtf8(desc), fontsize::Body);
        tDesc.setFillColor(theme::TextSecondary);
        sf::FloatRect db = tDesc.getLocalBounds();
        tDesc.setPosition({ (screenWidth - db.size.x) / 2.0f - db.position.x, backBtn.position.y + backBtn.size.y + 32.0f });
        ui::drawText(window, tDesc);

#if defined(__ANDROID__)
        sf::Text& hint = ui::pooledText(font, toUtf8(UI_settings::get().getLanguage() == "en" ? "Tap a mode to continue" : "Докоснете режим за продължаване"), fontsize::Label);
#else
        sf::Text& hint = ui::pooledText(font, toUtf8(Lang::tr("menu.hint_mode")), fontsize::Label);
#endif
        hint.setFillColor(theme::TextMuted);
        sf::FloatRect hb = hint.getLocalBounds();
        hint.setPosition({ (screenWidth - hb.size.x) / 2.0f - hb.position.x, backBtn.position.y + backBtn.size.y + 70.0f });
        ui::drawText(window, hint);
    }
}

void UI_mainMenu::drawBotDifficultyMenu(sf::RenderTarget& window) {
    drawHeader(window);

    float screenWidth = VIRTUAL_WIDTH;
    sf::Vector2f mousePos = ui::pointerPos(window);
    sf::FloatRect easyBtn = botDifficultyButtonRect(0);
    sf::FloatRect medBtn = botDifficultyButtonRect(1);
    sf::FloatRect hardBtn = botDifficultyButtonRect(2);
    sf::FloatRect backBtn = botDifficultyButtonRect(3);

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
        tDesc.setPosition({ (screenWidth - db.size.x) / 2.0f - db.position.x, backBtn.position.y + backBtn.size.y + 32.0f });
        ui::drawText(window, tDesc);

#if defined(__ANDROID__)
        sf::Text& hint = ui::pooledText(font, toUtf8(UI_settings::get().getLanguage() == "en" ? "Tap difficulty to start match" : "Докоснете трудност за старт на мача"), fontsize::Label);
#else
        sf::Text& hint = ui::pooledText(font, toUtf8(Lang::tr("menu.hint_difficulty")), fontsize::Label);
#endif
        hint.setFillColor(theme::TextMuted);
        sf::FloatRect hb = hint.getLocalBounds();
        hint.setPosition({ (screenWidth - hb.size.x) / 2.0f - hb.position.x, backBtn.position.y + backBtn.size.y + 70.0f });
        ui::drawText(window, hint);
    }
}

void UI_mainMenu::drawSettingsMenu(sf::RenderTarget& window) {
    drawHeader(window);

    const sf::FloatRect panelRect = settingsPanelRect();
    const float panelX = panelRect.position.x;
    const float panelY = panelRect.position.y;
    const float panelWidth = panelRect.size.x;
    const float panelHeight = panelRect.size.y;

    sf::RectangleShape panel(panelRect.size);
    panel.setPosition(panelRect.position);
    panel.setFillColor(theme::withAlpha(theme::Panel, 248));
    panel.setOutlineThickness(2.0f);
    panel.setOutlineColor(theme::LineStrong);
    window.draw(panel);
    ui::lint::occlude(panelRect);
    ui::lint::ContainerScope panelScope(panelRect);

    sf::Vector2f mousePos = ui::pointerPos(window);

    if (fontLoaded) {
        sf::Text& sTitle = ui::pooledText(font, toUtf8(Lang::tr("settings.title")), fontsize::H1);
        sTitle.setStyle(sf::Text::Bold);
        sTitle.setFillColor(theme::TextPrimary);
        sf::FloatRect tb = sTitle.getLocalBounds();
        sTitle.setPosition({ panelX + (panelWidth - tb.size.x) / 2.0f - tb.position.x, panelY + 18.0f });
        ui::drawText(window, sTitle);
    }

    auto drawRowBackdrop = [&](sf::FloatRect rowRect, bool selected) {
        sf::RectangleShape hl(rowRect.size);
        hl.setPosition(rowRect.position);
        hl.setFillColor(selected ? theme::withAlpha(theme::CardSelected, 210) : theme::withAlpha(theme::Card, 110));
        hl.setOutlineThickness(selected ? 2.0f : 1.0f);
        hl.setOutlineColor(selected ? theme::Focus : theme::withAlpha(theme::Line, 120));
        window.draw(hl);
    };

    auto drawRowLabel = [&](sf::FloatRect rowRect, sf::FloatRect rightCtrl, const std::string& rawLabel, bool selected) {
        if (!fontLoaded) return;
        std::string text = (selected ? "> " : "  ") + rawLabel;
        float maxW = std::max(80.0f, rightCtrl.position.x - (rowRect.position.x + 16.0f) - 16.0f);
        unsigned int sz = ui::fitTextSize(font, text, 18, 14, maxW, selected);
        sf::Text& lbl = ui::pooledText(font, toUtf8(text), sz);
        if (selected) lbl.setStyle(sf::Text::Bold);
        lbl.setFillColor(selected ? theme::TextPrimary : theme::TextSecondary);
        sf::FloatRect lb = lbl.getLocalBounds();
        lbl.setPosition({ rowRect.position.x + 16.0f - lb.position.x,
                          rowRect.position.y + (rowRect.size.y - lb.size.y) / 2.0f - lb.position.y });
        ui::drawText(window, lbl, rowRect);
    };

    std::string curLang = UI_settings::get().getLanguage();

    // Row 0: Language
    sf::FloatRect langRow = settingsRowRect(0);
    sf::FloatRect langBtn = settingsRowButtonRect(0);
    bool langSelected = (selectedSettingsIndex == 0);
    drawRowBackdrop(langRow, langSelected);
    drawRowLabel(langRow, langBtn, Lang::tr("settings.language"), langSelected);
    std::string langValText = (curLang == "bg") ? Lang::tr("settings.lang_bg") : Lang::tr("settings.lang_en");
    drawButton(window, langBtn, toUtf8(langValText),
               theme::Button, theme::ButtonHover, theme::TextPrimary, langSelected || langBtn.contains(mousePos));

    // Row 1: Volume
    sf::FloatRect volRow = settingsRowRect(1);
    sf::FloatRect volDown = settingsVolDownRect();
    sf::FloatRect volUp = settingsVolUpRect();
    bool volSelected = (selectedSettingsIndex == 1);
    drawRowBackdrop(volRow, volSelected);
    drawRowLabel(volRow, volDown, Lang::tr("settings.volume"), volSelected);
    if (fontLoaded) {
        sf::Text& volVal = ui::pooledText(font, toUtf8(std::to_string(volume) + "%"), 18);
        volVal.setStyle(sf::Text::Bold);
        volVal.setFillColor(theme::TextPrimary);
        sf::FloatRect vb = volVal.getLocalBounds();
        float valX = volDown.position.x + volDown.size.x;
        float valW = volUp.position.x - valX;
        sf::FloatRect valBounds({ valX, volRow.position.y }, { valW, volRow.size.y });
        volVal.setPosition({ valX + (valW - vb.size.x) / 2.0f - vb.position.x,
                             volRow.position.y + (volRow.size.y - vb.size.y) / 2.0f - vb.position.y });
        ui::drawText(window, volVal, valBounds);
    }
    drawButton(window, volDown, "-", theme::Button, theme::ButtonHover, theme::TextPrimary, volDown.contains(mousePos));
    drawButton(window, volUp, "+", theme::Button, theme::ButtonHover, theme::TextPrimary, volUp.contains(mousePos));

    // Row 2: Sound FX
    sf::FloatRect sfxRow = settingsRowRect(2);
    sf::FloatRect sfxBtn = settingsRowButtonRect(2);
    bool sfxSelected = (selectedSettingsIndex == 2);
    drawRowBackdrop(sfxRow, sfxSelected);
    drawRowLabel(sfxRow, sfxBtn, Lang::tr("settings.sfx"), sfxSelected);
    drawButton(window, sfxBtn, toUtf8(soundEffects ? Lang::tr("settings.on") : Lang::tr("settings.off")),
               soundEffects ? theme::GoodFill : theme::BadFill,
               soundEffects ? sf::Color(40, 148, 86) : sf::Color(156, 48, 60),
               theme::TextPrimary, sfxSelected || sfxBtn.contains(mousePos));

    // Row 3: Difficulty
    sf::FloatRect diffRow = settingsRowRect(3);
    sf::FloatRect diffBtn = settingsRowButtonRect(3);
    bool diffSelected = (selectedSettingsIndex == 3);
    drawRowBackdrop(diffRow, diffSelected);
    drawRowLabel(diffRow, diffBtn, Lang::tr("settings.bot_difficulty"), diffSelected);
    std::string diffKeys[] = { "settings.diff_easy", "settings.diff_normal", "settings.diff_hard" };
    drawButton(window, diffBtn, toUtf8(Lang::tr(diffKeys[settingsDifficultyIndex])),
               theme::Button, theme::ButtonHover, theme::TextPrimary, diffSelected || diffBtn.contains(mousePos));

    // Row 4: Controls Remapping
    sf::FloatRect ctrlRow = settingsRowRect(4);
    sf::FloatRect ctrlBtn = settingsRowButtonRect(4);
    bool ctrlSelected = (selectedSettingsIndex == 4);
    drawRowBackdrop(ctrlRow, ctrlSelected);
    drawRowLabel(ctrlRow, ctrlBtn, Lang::tr("settings.controls"), ctrlSelected);
    drawButton(window, ctrlBtn, toUtf8(Lang::tr("settings.remap")),
               theme::Button, theme::ButtonHover, theme::TextPrimary, ctrlSelected || ctrlBtn.contains(mousePos));

    // Notes
    if (fontLoaded) {
        std::string notes[2] = {
            Lang::tr("settings.note_audio"),
            Lang::tr("settings.note_difficulty")
        };
        for (int i = 0; i < 2; ++i) {
            unsigned int noteSz = ui::fitTextSize(font, notes[i], 13, 11, panelWidth - 56.0f, false);
            sf::Text& tNote = ui::pooledText(font, toUtf8(notes[i]), noteSz);
            tNote.setFillColor(theme::TextMuted);
            sf::FloatRect nb = tNote.getLocalBounds();
            tNote.setPosition({ panelX + (panelWidth - nb.size.x) / 2.0f - nb.position.x, panelY + 346.0f + i * 24.0f });
            ui::drawText(window, tNote);
        }
    }

    // Row 5: Back button
    sf::FloatRect backBtn = settingsBackButtonRect();
    bool backSelected = (selectedSettingsIndex == 5);
    drawButton(window, backBtn, toUtf8(Lang::tr("common.back")), theme::Button, theme::ButtonHover, theme::TextPrimary, backSelected || backBtn.contains(mousePos));

    // Hints
    if (fontLoaded) {
#if defined(__ANDROID__)
        sf::Text& hint = ui::pooledText(font, toUtf8(UI_settings::get().getLanguage() == "en" ? "Tap settings to adjust" : "Докоснете настройките за промяна"), fontsize::Label);
#else
        sf::Text& hint = ui::pooledText(font, toUtf8(Lang::tr("settings.hint")), fontsize::Label);
#endif
        hint.setFillColor(theme::TextMuted);
        sf::FloatRect hb = hint.getLocalBounds();
        hint.setPosition({ (VIRTUAL_WIDTH - hb.size.x) / 2.0f - hb.position.x, panelY + panelHeight + 16.0f });
        ui::drawText(window, hint, sf::FloatRect({ 0.0f, 0.0f }, { VIRTUAL_WIDTH, VIRTUAL_HEIGHT }));
    }
}

void UI_mainMenu::drawControlsRemapMenu(sf::RenderTarget& window) {
    drawHeader(window);

    const sf::FloatRect panelRect = remapPanelRect();
    const float panelX = panelRect.position.x;
    const float panelY = panelRect.position.y;
    const float panelWidth = panelRect.size.x;
    const float panelHeight = panelRect.size.y;

    sf::RectangleShape panel(panelRect.size);
    panel.setPosition(panelRect.position);
    panel.setFillColor(theme::withAlpha(theme::Panel, 248));
    panel.setOutlineThickness(2.0f);
    panel.setOutlineColor(theme::LineStrong);
    window.draw(panel);
    ui::lint::occlude(panelRect);

    sf::Vector2f mousePos = ui::pointerPos(window);
    bool isEn = (UI_settings::get().getLanguage() == "en");

    {
        ui::lint::ContainerScope panelScope(panelRect);

        if (fontLoaded) {
            std::string titleStr = isEn ? "SETTINGS: REMAP CONTROLS" : "НАСТРОЙКИ: ПРЕНАЗНАЧАВАНЕ НА УПРАВЛЕНИЕТО";
            sf::Text& title = ui::pooledText(font, toUtf8(titleStr), fontsize::H1);
            title.setStyle(sf::Text::Bold);
            title.setFillColor(theme::TextPrimary);
            sf::FloatRect tb = title.getLocalBounds();
            title.setPosition({ panelX + (panelWidth - tb.size.x) / 2.0f - tb.position.x, panelY + 14.0f });
            ui::drawText(window, title);

            std::string subText;
            if (isRebinding) {
                subText = isEn ? "PRESS DESIRED KEY ON KEYBOARD (OR ESC TO CANCEL)"
                               : "НАТИСНЕТЕ ЖЕЛАН КЛАВИШ ОТ КЛАВИАТУРАТА (ИЛИ ESC ЗА ОТКАЗ)";
            } else {
                subText = isEn ? "Select control with [Enter] or click, then press new key"
                               : "Изберете контрол с [Enter] или клик, след което натиснете новия клавиш";
            }
            sf::Text& sub = ui::pooledText(font, toUtf8(subText), fontsize::Body);
            sub.setFillColor(isRebinding ? theme::Gold : theme::TextSecondary);
            if (isRebinding) sub.setStyle(sf::Text::Bold);
            sf::FloatRect sb = sub.getLocalBounds();
            sub.setPosition({ panelX + (panelWidth - sb.size.x) / 2.0f - sb.position.x, panelY + 46.0f });
            ui::drawText(window, sub);
        }

        // Divider
        sf::RectangleShape div({ panelWidth - 60.0f, 2.0f });
        div.setPosition({ panelX + 30.0f, panelY + 72.0f });
        div.setFillColor(theme::Line);
        window.draw(div);

        float startY = panelY + 84.0f;
        const UI_controlsConfig& cfg = UI_controlsConfig::get();

        for (int p = 1; p <= 2; ++p) {
            sf::FloatRect firstRow = remapRowRect(p, 0);
            float colX = firstRow.position.x;
            sf::Color playerColor = (p == 1) ? theme::P1 : theme::P2;
            sf::Color playerColorLight = (p == 1) ? theme::P1Light : theme::P2Light;

            // Player section title
            if (fontLoaded) {
                std::string pTitle = (p == 1) ? (isEn ? "PLAYER 1 (WEST SECTOR)" : "ИГРАЧ 1 (ЗАПАДЕН СЕКТОР)")
                                              : (isEn ? "PLAYER 2 (EAST SECTOR)" : "ИГРАЧ 2 (ИЗТОЧЕН СЕКТОР)");
                sf::Text& colTitle = ui::pooledText(font, toUtf8(pTitle), fontsize::H2);
                colTitle.setStyle(sf::Text::Bold);
                colTitle.setFillColor(playerColorLight);
                colTitle.setPosition({ colX + 10.0f, startY });
                ui::drawText(window, colTitle);
            }

            const PlayerBindings& binds = cfg.getPlayer(p);

            for (int a = 0; a < static_cast<int>(ControlAction::COUNT); ++a) {
                ControlAction actionType = static_cast<ControlAction>(a);
                sf::FloatRect rowRect = remapRowRect(p, a);

                bool isRowSelected = (!isRebinding && remapSelectedPlayer == p && remapSelectedRow == a);
                bool isRowHovered = (!isRebinding && rowRect.contains(mousePos));
                bool isCurrentRebinding = (isRebinding && rebindPlayer == p && rebindActionIndex == a);

                sf::RectangleShape rowBg(rowRect.size);
                rowBg.setPosition(rowRect.position);
                if (isCurrentRebinding) {
                    float pulse = 0.5f + 0.5f * std::sin(ui::shot::clockSeconds(rebindPulseClock.getElapsedTime().asSeconds()) * 8.0f);
                    rowBg.setFillColor(theme::withAlpha(theme::Gold, static_cast<std::uint8_t>(40 + 50 * pulse)));
                    rowBg.setOutlineThickness(2.0f);
                    rowBg.setOutlineColor(theme::Gold);
                } else if (isRowSelected || isRowHovered) {
                    rowBg.setFillColor(theme::withAlpha(playerColor, 45));
                    rowBg.setOutlineThickness(1.5f);
                    rowBg.setOutlineColor(playerColorLight);
                } else {
                    rowBg.setFillColor(theme::withAlpha(theme::Card, 130));
                    rowBg.setOutlineThickness(1.0f);
                    rowBg.setOutlineColor(theme::withAlpha(theme::Line, 140));
                }
                window.draw(rowBg);

                // Action label
                if (fontLoaded) {
                    std::string actionName = getControlActionName(actionType, isEn);
                    if (isRowSelected) actionName = "> " + actionName;
                    bool boldAct = (isRowSelected || isCurrentRebinding);
                    unsigned int actSz = ui::fitTextSize(font, actionName, 15, 13, rowRect.size.x - 195.0f - 28.0f, boldAct);
                    sf::Text& actText = ui::pooledText(font, toUtf8(actionName), actSz);
                    actText.setFillColor(boldAct ? theme::TextPrimary : theme::TextSecondary);
                    if (boldAct) actText.setStyle(sf::Text::Bold);
                    sf::FloatRect ab = actText.getLocalBounds();
                    actText.setPosition({ rowRect.position.x + 14.0f - ab.position.x,
                                          rowRect.position.y + (rowRect.size.y - ab.size.y) / 2.0f - ab.position.y });
                    ui::drawText(window, actText, rowRect);
                }

                // Key badge on the right (130x30, or 195x30 when rebinding)
                sf::Keyboard::Key assignedKey = binds.getKey(actionType);
                std::string keyName = keyToString(assignedKey);

                float badgeWidth = isCurrentRebinding ? 195.0f : 130.0f;
                float badgeHeight = 30.0f;
                sf::FloatRect badgeRect({ rowRect.position.x + rowRect.size.x - badgeWidth - 10.0f,
                                          rowRect.position.y + (rowRect.size.y - badgeHeight) / 2.0f },
                                        { badgeWidth, badgeHeight });

                sf::RectangleShape badge(badgeRect.size);
                badge.setPosition(badgeRect.position);
                badge.setFillColor(isCurrentRebinding ? theme::Gold : (isRowSelected ? playerColor : theme::Button));
                badge.setOutlineThickness(1.5f);
                badge.setOutlineColor(isCurrentRebinding ? sf::Color::White : theme::LineStrong);
                window.draw(badge);

                if (fontLoaded) {
                    std::string badgeLabel = isCurrentRebinding ? (isEn ? "[ PRESS KEY... ]" : "[ ВЪВЕДЕТЕ... ]") : ("[" + keyName + "]");
                    unsigned int badgeSz = ui::fitTextSize(font, badgeLabel, 14, 11, badgeWidth - 10.0f, true);
                    sf::Text& bText = ui::pooledText(font, toUtf8(badgeLabel), badgeSz);
                    bText.setStyle(sf::Text::Bold);
                    bText.setFillColor(isCurrentRebinding ? sf::Color::Black : (isRowSelected ? theme::TextOnLight : theme::TextPrimary));
                    sf::FloatRect bb = bText.getLocalBounds();
                    bText.setPosition({ badgeRect.position.x + (badgeWidth - bb.size.x) / 2.0f - bb.position.x,
                                        badgeRect.position.y + (badgeHeight - bb.size.y) / 2.0f - bb.position.y });
                    ui::drawText(window, bText, badgeRect);
                }
            }
        }

        // Bottom action buttons
        sf::FloatRect resetBtn = remapResetButtonRect();
        sf::FloatRect backBtn = remapBackButtonRect();

        bool resetSelected = (!isRebinding && remapSelectedRow == 9);
        bool backSelected = (!isRebinding && remapSelectedRow == 10);

        drawButton(window, resetBtn, toUtf8(isEn ? "RESET TO DEFAULT" : "ВЪЗСТАНОВИ ПО ПОДРАЗБИРАНЕ"),
                   theme::Button, theme::ButtonHover, theme::TextPrimary, resetSelected || (!isRebinding && resetBtn.contains(mousePos)));
        drawButton(window, backBtn, toUtf8(isEn ? "BACK" : "НАЗАД"),
                   theme::Button, theme::ButtonHover, theme::TextPrimary, backSelected || (!isRebinding && backBtn.contains(mousePos)));
    }

    if (fontLoaded) {
        const UI_controlsConfig& cfg = UI_controlsConfig::get();
        std::string joyStatus = cfg.getJoystickStatusBg();
        sf::Text& joyText = ui::pooledText(font, toUtf8(joyStatus), fontsize::Label);
        joyText.setFillColor(cfg.isAnyJoystickConnected() ? theme::Good : theme::TextSecondary);
        sf::FloatRect jb = joyText.getLocalBounds();
        joyText.setPosition({ (VIRTUAL_WIDTH - jb.size.x) / 2.0f - jb.position.x, panelY + panelHeight + 14.0f });
        ui::drawText(window, joyText);

#if defined(__ANDROID__)
        sf::Text& hint = ui::pooledText(font, toUtf8(isEn ? "Tap control options to adjust" : "Докоснете опциите за управление"), fontsize::Label);
#else
        sf::Text& hint = ui::pooledText(font, toUtf8(isEn ? "Navigation: [W/S/A/D, Arrows, Arcade Stick] | Select: [Enter / Btn 1] | Cancel: [Esc / Btn 2]"
                                                          : "Навигация: [W/S/A/D, Стрелки, Аркаден стик] | Избор: [Enter / Бутон 1] | Отказ: [Esc / Бутон 2]"), fontsize::Label);
#endif
        hint.setFillColor(theme::TextMuted);
        sf::FloatRect hb = hint.getLocalBounds();
        hint.setPosition({ (VIRTUAL_WIDTH - hb.size.x) / 2.0f - hb.position.x, panelY + panelHeight + 38.0f });
        ui::drawText(window, hint);
    }
}

void UI_mainMenu::handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    if (ArcadeMode::isEnabled()) {
        if (event.is<sf::Event::KeyPressed>() || event.is<sf::Event::KeyReleased>() ||
            event.is<sf::Event::MouseButtonPressed>() || event.is<sf::Event::MouseButtonReleased>()) {
            return;
        }
    }

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
        if (!ArcadeMode::isEnabled()) {
            isUp = (key->code == sf::Keyboard::Key::Up || key->code == sf::Keyboard::Key::W);
            isDown = (key->code == sf::Keyboard::Key::Down || key->code == sf::Keyboard::Key::S);
            isLeft = (key->code == sf::Keyboard::Key::Left || key->code == sf::Keyboard::Key::A);
            isRight = (key->code == sf::Keyboard::Key::Right || key->code == sf::Keyboard::Key::D);
            isSelect = (key->code == sf::Keyboard::Key::Enter || key->code == sf::Keyboard::Key::Space);
            isEscape = (key->code == sf::Keyboard::Key::Escape);
        }
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
            if (isUp || isDown) {
                arcadeSelectedModeIndex = (arcadeSelectedModeIndex + 1) % 2;
                return;
            }
            if (isSelect) {
                if (ArcadeMode::isEnabled()) {
                    // Check credits for Game 2 onwards
                    if (CreditsManager::get().requiresCreditForNewGame()) {
                        int curCredits = CreditsManager::get().getCurrentCredits();
                        if (curCredits < 1) {
                            bool isEn = (UI_settings::get().getLanguage() == "en");
                            std::string msg = isEn ? "INSERT COIN\n(0 CREDITS)" : "НЯМА КРЕДИТИ!\nПУСНЕТЕ МОНЕТА";
                            ArcadePopup::get().show(msg, 2.5f);
                            return; // Block completely from starting
                        }
                    }
                }
                if (arcadeSelectedModeIndex == 0) {
                    // Player vs Player
                    selectedBotDifficulty = BotDifficulty::NONE;
                    state = MenuState::CALIBRATE_BLUE;
                    enterHeld = true;
                    spaceHeld = true;
                    return;
                } else {
                    // Player vs Bot: show 3 difficulties
                    state = MenuState::ARCADE_BOT_DIFFICULTY;
                    arcadeSelectedDifficultyIndex = 1; // default Medium
                    enterHeld = true;
                    spaceHeld = true;
                    return;
                }
            }
            return;
        } else if (state == MenuState::ARCADE_BOT_DIFFICULTY) {
            if (isUp) {
                arcadeSelectedDifficultyIndex = (arcadeSelectedDifficultyIndex + 3) % 4;
                return;
            } else if (isDown) {
                arcadeSelectedDifficultyIndex = (arcadeSelectedDifficultyIndex + 1) % 4;
                return;
            } else if (isEscape) {
                state = MenuState::PRESS_A_TO_START;
                enterHeld = true;
                spaceHeld = true;
                return;
            } else if (isSelect) {
                if (arcadeSelectedDifficultyIndex == 3) {
                    state = MenuState::PRESS_A_TO_START;
                    enterHeld = true;
                    spaceHeld = true;
                    return;
                }
                if (ArcadeMode::isEnabled()) {
                    if (CreditsManager::get().requiresCreditForNewGame()) {
                        int curCredits = CreditsManager::get().getCurrentCredits();
                        if (curCredits < 1) {
                            bool isEn = (UI_settings::get().getLanguage() == "en");
                            std::string msg = isEn ? "INSERT COIN\n(0 CREDITS)" : "НЯМА КРЕДИТИ!\nПУСНЕТЕ МОНЕТА";
                            ArcadePopup::get().show(msg, 2.5f);
                            return;
                        }
                    }
                }
                if (arcadeSelectedDifficultyIndex == 0) selectedBotDifficulty = BotDifficulty::EASY;
                else if (arcadeSelectedDifficultyIndex == 1) selectedBotDifficulty = BotDifficulty::MEDIUM;
                else if (arcadeSelectedDifficultyIndex == 2) selectedBotDifficulty = BotDifficulty::HARD;

                state = MenuState::CALIBRATE_BLUE;
                enterHeld = true;
                spaceHeld = true;
                return;
            }
            return;
        } else if (state == MenuState::CALIBRATE_BLUE) {
            if (ArcadeMode::isEnabled() && !event.is<sf::Event::JoystickButtonPressed>()) {
                return;
            }
            if (const auto* jb = event.getIf<sf::Event::JoystickButtonPressed>()) {
                UI_controlsConfig::get().setPlayerJoystick(1, jb->joystickId);
                std::cout << "[Calibration] Player 1 assigned to Joystick " << jb->joystickId << "\n";
            } else {
                UI_controlsConfig::get().setPlayerJoystick(1, 0);
            }
            enterHeld = true;
            spaceHeld = true;

            if (selectedBotDifficulty != BotDifficulty::NONE) {
                // In Player vs Bot mode, Player 2 is BOT! No need to calibrate red controller.
                std::cout << "[Calibration] Bot mode active (" << static_cast<int>(selectedBotDifficulty)
                          << "). Skipping Player 2 calibration and launching match.\n";
                UI_controlsConfig::get().setPlayerJoystick(2, 1);
                playControls.setActiveSchemeIndex(static_cast<int>(ControlScheme::DEVHUB_ARCADE));
                onPlay();
                return;
            }

            state = MenuState::CALIBRATE_RED;
            return;
        } else if (state == MenuState::CALIBRATE_RED) {
            if (ArcadeMode::isEnabled() && !event.is<sf::Event::JoystickButtonPressed>()) {
                return;
            }
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
                    state = MenuState::MATCH_TYPE_SELECT;
                    selectedMatchTypeIndex = 0;
                } else if (selectedMainIndex == 1) {
                    onSettings();
                } else if (selectedMainIndex == 2) {
                    onQuit();
                }
            }
        } else if (state == MenuState::MATCH_TYPE_SELECT) {
            if (isUp) {
                selectedMatchTypeIndex = (selectedMatchTypeIndex + 2) % 3;
            } else if (isDown) {
                selectedMatchTypeIndex = (selectedMatchTypeIndex + 1) % 3;
            } else if (isSelect) {
                if (selectedMatchTypeIndex == 0) {
                    if (AdManager::get().is30mUnlocked()) {
                        blitzMode = false; // 30m Battle
                        state = MenuState::MODE_SELECT;
                        selectedModeIndex = 0;
                    } else {
                        state = MenuState::AD_UNLOCK_PROMPT;
                        selectedAdPromptIndex = 0;
                    }
                } else if (selectedMatchTypeIndex == 1) {
                    blitzMode = true; // 5m Blitz
                    state = MenuState::MODE_SELECT;
                    selectedModeIndex = 0;
                } else if (selectedMatchTypeIndex == 2) {
                    state = MenuState::MAIN;
                }
            } else if (isEscape) {
                state = MenuState::MAIN;
            }
        } else if (state == MenuState::AD_UNLOCK_PROMPT) {
            if (isUp) {
                selectedAdPromptIndex = (selectedAdPromptIndex + 2) % 3;
            } else if (isDown) {
                selectedAdPromptIndex = (selectedAdPromptIndex + 1) % 3;
            } else if (isSelect) {
                if (selectedAdPromptIndex == 0) {
                    AdManager::get().showRewardedAd();
                } else if (selectedAdPromptIndex == 1) {
                    blitzMode = true; // 5m Blitz is free
                    state = MenuState::MODE_SELECT;
                    selectedModeIndex = 0;
                } else if (selectedAdPromptIndex == 2) {
                    state = MenuState::MATCH_TYPE_SELECT;
                }
            } else if (isEscape) {
                state = MenuState::MATCH_TYPE_SELECT;
            }
        } else if (state == MenuState::MODE_SELECT) {
            if (isUp) {
                selectedModeIndex = (selectedModeIndex + 3) % 4;
            } else if (isDown) {
                selectedModeIndex = (selectedModeIndex + 1) % 4;
            } else if (isSelect) {
                if (selectedModeIndex == 0) {
                    botVsBotMode = false;
                    selectedBotDifficulty = BotDifficulty::NONE;
#if defined(__ANDROID__)
                    onPlay();
#else
                    state = MenuState::PLAY_CONTROLS;
                    playControls.resetRequests();
#endif
                } else if (selectedModeIndex == 1) {
                    botVsBotMode = false;
                    state = MenuState::BOT_DIFFICULTY;
                    selectedDifficultyIndex = settingsDifficultyIndex; // default from НАСТРОЙКИ
                } else if (selectedModeIndex == 2) {
                    // Emulation / Test (Bot vs Bot)
                    botVsBotMode = true;
                    selectedBotDifficulty = BotDifficulty::HARD;
                    onPlay();
                } else if (selectedModeIndex == 3) {
                    state = MenuState::MATCH_TYPE_SELECT;
                }
            } else if (isEscape) {
                state = MenuState::MATCH_TYPE_SELECT;
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
                selectedSettingsIndex = 4;
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
                    selectedSettingsIndex = 4;
                }
            }
        }
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        std::cout << "[UI_mainMenu] MouseButtonPressed at (" << mb->position.x << "," << mb->position.y << ") state=" << static_cast<int>(state) << "\n";
        if (mb->button == sf::Mouse::Button::Left) {
            static sf::Clock s_menuTransitionClock;
            if (s_menuTransitionClock.getElapsedTime().asMilliseconds() < 160) {
                std::cout << "[UI_mainMenu] Blocked by transition clock debounce\n";
                return;
            }
            sf::Vector2f clickPos = window.mapPixelToCoords(mb->position);
            std::cout << "[UI_mainMenu] clickPos=(" << clickPos.x << "," << clickPos.y << ")\n";

            if (state == MenuState::MAIN) {
                if (isPointInside(mainButtonRect(0), clickPos)) {
                    std::cout << "[UI_mainMenu] Transitioning MAIN -> MATCH_TYPE_SELECT\n";
                    state = MenuState::MATCH_TYPE_SELECT;
                    selectedMatchTypeIndex = 0;
                    s_menuTransitionClock.restart();
                    return;
                } else if (isPointInside(mainButtonRect(1), clickPos)) {
                    std::cout << "[UI_mainMenu] Transitioning MAIN -> SETTINGS\n";
                    onSettings();
                    s_menuTransitionClock.restart();
                    return;
                } else if (isPointInside(mainButtonRect(2), clickPos)) {
                    std::cout << "[UI_mainMenu] Transitioning MAIN -> QUIT\n";
                    onQuit();
                    s_menuTransitionClock.restart();
                    return;
                }
            } else if (state == MenuState::MATCH_TYPE_SELECT) {
                if (isPointInside(matchTypeButtonRect(0), clickPos)) {
                    if (AdManager::get().is30mUnlocked()) {
                        blitzMode = false; // 30m Battle
                        state = MenuState::MODE_SELECT;
                        selectedModeIndex = 0;
                    } else {
                        state = MenuState::AD_UNLOCK_PROMPT;
                        selectedAdPromptIndex = 0;
                    }
                    s_menuTransitionClock.restart();
                    return;
                } else if (isPointInside(matchTypeButtonRect(1), clickPos)) {
                    blitzMode = true; // 5m Blitz
                    state = MenuState::MODE_SELECT;
                    selectedModeIndex = 0;
                    s_menuTransitionClock.restart();
                    return;
                } else if (isPointInside(matchTypeButtonRect(2), clickPos)) {
                    state = MenuState::MAIN;
                    s_menuTransitionClock.restart();
                    return;
                }
            } else if (state == MenuState::AD_UNLOCK_PROMPT) {
                sf::FloatRect watchBtn = adUnlockButtonRect(0);
                LOGI("AD_UNLOCK_PROMPT clicked at (%.1f, %.1f), watchBtn=[%.1f, %.1f, %.1f, %.1f]",
                     clickPos.x, clickPos.y, watchBtn.position.x, watchBtn.position.y, watchBtn.size.x, watchBtn.size.y);

                if (isPointInside(watchBtn, clickPos)) {
                    LOGI("User clicked 'Watch Ad' button!");
                    AdManager::get().showRewardedAd();
                    return;
                } else if (isPointInside(adUnlockButtonRect(1), clickPos)) {
                    LOGI("User clicked '5m Blitz free' button!");
                    blitzMode = true; // 5m Blitz is free
                    state = MenuState::MODE_SELECT;
                    selectedModeIndex = 0;
                    s_menuTransitionClock.restart();
                    return;
                } else if (isPointInside(adUnlockButtonRect(2), clickPos)) {
                    LOGI("User clicked 'Back' button!");
                    state = MenuState::MATCH_TYPE_SELECT;
                    s_menuTransitionClock.restart();
                    return;
                }
            } else if (state == MenuState::MODE_SELECT) {
                if (isPointInside(modeButtonRect(0), clickPos)) {
                    botVsBotMode = false;
                    selectedBotDifficulty = BotDifficulty::NONE;
#if defined(__ANDROID__)
                    onPlay();
#else
                    state = MenuState::PLAY_CONTROLS;
                    playControls.resetRequests();
#endif
                    s_menuTransitionClock.restart();
                    return;
                } else if (isPointInside(modeButtonRect(1), clickPos)) {
                    botVsBotMode = false;
                    state = MenuState::BOT_DIFFICULTY;
                    selectedDifficultyIndex = settingsDifficultyIndex; // default from НАСТРОЙКИ
                    s_menuTransitionClock.restart();
                    return;
                } else if (isPointInside(modeButtonRect(2), clickPos)) {
                    botVsBotMode = true;
                    selectedBotDifficulty = BotDifficulty::HARD;
                    onPlay();
                    s_menuTransitionClock.restart();
                    return;
                } else if (isPointInside(modeButtonRect(3), clickPos)) {
                    state = MenuState::MATCH_TYPE_SELECT;
                    s_menuTransitionClock.restart();
                    return;
                }
            } else if (state == MenuState::BOT_DIFFICULTY) {
                if (isPointInside(botDifficultyButtonRect(0), clickPos)) {
                    selectedBotDifficulty = BotDifficulty::EASY;
                    onPlay();
                    s_menuTransitionClock.restart();
                    return;
                } else if (isPointInside(botDifficultyButtonRect(1), clickPos)) {
                    selectedBotDifficulty = BotDifficulty::MEDIUM;
                    onPlay();
                    s_menuTransitionClock.restart();
                    return;
                } else if (isPointInside(botDifficultyButtonRect(2), clickPos)) {
                    selectedBotDifficulty = BotDifficulty::HARD;
                    onPlay();
                    s_menuTransitionClock.restart();
                    return;
                } else if (isPointInside(botDifficultyButtonRect(3), clickPos)) {
                    state = MenuState::MODE_SELECT;
                    s_menuTransitionClock.restart();
                    return;
                }
            } else if (state == MenuState::SETTINGS) {
                if (isPointInside(settingsRowButtonRect(0), clickPos) ||
                    isPointInside(settingsRowRect(0), clickPos)) {
                    std::string nextLang = (UI_settings::get().getLanguage() == "bg") ? "en" : "bg";
                    UI_settings::get().setLanguage(nextLang);
                    selectedSettingsIndex = 0;
                } else if (isPointInside(settingsVolDownRect(), clickPos)) {
                    if (volume >= 10) {
                        volume -= 10;
                        UI_settings::get().setVolume(volume);
                    }
                    selectedSettingsIndex = 1;
                } else if (isPointInside(settingsVolUpRect(), clickPos)) {
                    if (volume <= 90) {
                        volume += 10;
                        UI_settings::get().setVolume(volume);
                    }
                    selectedSettingsIndex = 1;
                } else if (isPointInside(settingsRowButtonRect(2), clickPos) ||
                           isPointInside(settingsRowRect(2), clickPos)) {
                    soundEffects = !soundEffects;
                    UI_settings::get().setSoundEffectsEnabled(soundEffects);
                    selectedSettingsIndex = 2;
                } else if (isPointInside(settingsRowButtonRect(3), clickPos) ||
                           isPointInside(settingsRowRect(3), clickPos)) {
                    settingsDifficultyIndex = (settingsDifficultyIndex + 1) % 3;
                    UI_settings::get().setBotDifficultyIndex(settingsDifficultyIndex);
                    selectedSettingsIndex = 3;
                } else if (isPointInside(settingsRowButtonRect(4), clickPos) ||
                           isPointInside(settingsRowRect(4), clickPos)) {
                    state = MenuState::SETTINGS_CONTROLS;
                    remapSelectedPlayer = 1;
                    remapSelectedRow = 0;
                    isRebinding = false;
                    selectedSettingsIndex = 4;
                } else if (isPointInside(settingsBackButtonRect(), clickPos)) {
                    state = MenuState::MAIN;
                }
            } else if (state == MenuState::SETTINGS_CONTROLS) {
                // Check clicks on control rows
                for (int p = 1; p <= 2; ++p) {
                    for (int a = 0; a < static_cast<int>(ControlAction::COUNT); ++a) {
                        if (isPointInside(remapRowRect(p, a), clickPos)) {
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
                if (isPointInside(remapResetButtonRect(), clickPos)) {
                    UI_controlsConfig::get().resetToDefaults();
                    remapSelectedRow = 9;
                } else if (isPointInside(remapBackButtonRect(), clickPos)) {
                    isRebinding = false;
                    state = MenuState::SETTINGS;
                    selectedSettingsIndex = 4;
                }
            }
        }
    }
}

void UI_mainMenu::render(sf::RenderTarget& window) {
    if (state == MenuState::PRESS_A_TO_START) {
        drawPressAToStart(window);
    } else if (state == MenuState::ARCADE_BOT_DIFFICULTY) {
        drawArcadeBotDifficulty(window);
    } else if (state == MenuState::CALIBRATE_BLUE) {
        drawCalibration(window, true);
    } else if (state == MenuState::CALIBRATE_RED) {
        drawCalibration(window, false);
    } else if (state == MenuState::MAIN) {
        drawMainMenu(window);
    } else if (state == MenuState::MATCH_TYPE_SELECT) {
        drawMatchTypeMenu(window);
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
    } else if (state == MenuState::AD_UNLOCK_PROMPT) {
        drawAdUnlockPrompt(window);
    }
}

void UI_mainMenu::showState(MenuState s) {
    state = s;
    selectedMainIndex = 0;
    selectedMatchTypeIndex = 0;
    selectedModeIndex = 0;
    selectedDifficultyIndex = settingsDifficultyIndex;
    selectedSettingsIndex = 0;
    selectedAdPromptIndex = 0;
    if (s == MenuState::PLAY_CONTROLS) playControls.resetRequests();
}
