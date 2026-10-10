#ifndef UI_MAINMENU_H
#define UI_MAINMENU_H

#include <SFML/Graphics.hpp>
#include <string>
#include "UI_types.h"
#include "UI_playControls.h"

enum class MenuState {
    PRESS_A_TO_START,
    CALIBRATE_BLUE,
    CALIBRATE_RED,
    MAIN,
    MODE_SELECT,
    PLAY_CONTROLS,
    BOT_DIFFICULTY,
    SETTINGS,
    SETTINGS_CONTROLS
};

class UI_mainMenu {
private:
    MenuState state;
    bool requestPlay;
    bool requestQuit;

    int selectedMainIndex;       // 0: Play, 1: Settings, 2: Quit
    int selectedModeIndex;       // 0: Co-op (2P), 1: Single Player (VS Bot), 2: Back
    int selectedDifficultyIndex; // 0: Easy, 1: Medium, 2: Hard, 3: Back
    int selectedSettingsIndex;   // 0: Language, 1: Volume, 2: SoundFX, 3: Difficulty, 4: Controls, 5: Back
    sf::Vector2f lastMenuMousePos = { -999.0f, -999.0f };

    BotDifficulty selectedBotDifficulty;

    // Controls remapping state (SETTINGS_CONTROLS)
    int remapSelectedPlayer = 1; // 1: Player 1, 2: Player 2
    int remapSelectedRow = 0;    // 0..8: Control action, 9: Reset defaults, 10: Back
    bool isRebinding = false;
    int rebindPlayer = 1;
    int rebindActionIndex = 0;
    sf::Clock rebindPulseClock;

    // Settings state
    int volume;                  // 0..100 % master volume ([b-effects] applied by UI_main to UI_audio)
    bool soundEffects;           // sound effects on/off ([b-effects] applied by UI_main to UI_audio)
    int settingsDifficultyIndex; // 0..2: default bot difficulty preselected in the single-player menu

    // Enter/Space must be released before they can select again (filters key auto-repeat)
    bool enterHeld = false;
    bool spaceHeld = false;

    sf::Font font;
    bool fontLoaded;
    sf::Font arcadeFont;
    bool arcadeFontLoaded = false;

    sf::Texture logoTexture;     // assets/logo.png (title artwork)
    bool logoLoaded = false;
    sf::Texture blueControllerTexture;
    bool blueControllerLoaded = false;
    sf::Texture redControllerTexture;
    bool redControllerLoaded = false;
    sf::Texture arrowTexture;
    bool arrowLoaded = false;

    sf::Clock animClock;         // Logo glow pulse

    UI_playControls playControls;

    // Logo title: large on the top-level menu, compact above the submenus
    void drawHeader(sf::RenderWindow& window, bool large = false);
    void drawPressAToStart(sf::RenderWindow& window);
    void drawCalibration(sf::RenderWindow& window, bool isBlue);
    void drawMainMenu(sf::RenderWindow& window);
    void drawModeSelectMenu(sf::RenderWindow& window);
    void drawBotDifficultyMenu(sf::RenderWindow& window);
    void drawSettingsMenu(sf::RenderWindow& window);
    void drawControlsRemapMenu(sf::RenderWindow& window);

    void drawButton(sf::RenderWindow& window, sf::FloatRect bounds, const sf::String& text,
                    sf::Color baseColor, sf::Color hoverColor, sf::Color textColor,
                    bool isSelected);
    bool isPointInside(sf::FloatRect bounds, sf::Vector2f pt) const;

public:
    UI_mainMenu();
    ~UI_mainMenu();

    void handleEvent(const sf::Event& event, const sf::RenderWindow& window);
    void render(sf::RenderWindow& window);

    void onPlay();
    void onSettings();
    void onQuit();

    // Show the top-level menu again (call when returning from a match); keys still held are ignored
    void returnToMain();

    // Screenshot mode: open one menu screen directly (no input needed)
    void showState(MenuState s);

    bool isPlayRequested() const { return requestPlay; }
    bool isQuitRequested() const { return requestQuit; }
    void resetPlayRequest() { requestPlay = false; }
    ControlScheme getSelectedControlScheme() const {
        return playControls.getSelectedScheme();
    }
    BotDifficulty getSelectedBotDifficulty() const {
        return selectedBotDifficulty;
    }
    bool isBotVsBot() const { return botVsBotMode; }
    void setBotVsBot(bool enabled) { botVsBotMode = enabled; }
    // [b-effects] Audio settings read every frame by UI_main
    int getVolume() const { return volume; }
    bool isSoundEffectsEnabled() const { return soundEffects; }
private:
    bool botVsBotMode = false;
};

#endif // UI_MAINMENU_H
