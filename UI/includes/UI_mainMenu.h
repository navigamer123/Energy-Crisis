#ifndef UI_MAINMENU_H
#define UI_MAINMENU_H

#include <SFML/Graphics.hpp>
#include <string>
#include "UI_types.h"
#include "UI_playControls.h"
#include "UI_settingsMenu.h" // Team b-session: shared НАСТРОЙКИ screen

enum class MenuState {
    MAIN,
    MODE_SELECT,
    PLAY_CONTROLS,
    BOT_DIFFICULTY,
    SETTINGS
};

class UI_mainMenu {
private:
    MenuState state;
    bool requestPlay;
    bool requestQuit;

    int selectedMainIndex;       // index into the main buttons (ПРОДЪЛЖИ first when a match can be continued)
    int selectedModeIndex;       // 0: Co-op (2P), 1: Single Player (VS Bot), 2: Back
    int selectedDifficultyIndex; // 0: Easy, 1: Medium, 2: Hard, 3: Back
    sf::Vector2f lastMenuMousePos = { -999.0f, -999.0f };

    BotDifficulty selectedBotDifficulty;

    // --- Team b-session (F-02, F-06): continue + persistent settings screen ---
    UI_settingsMenu settingsScreen;   // values live in gameSettings() (UI_settings.h)
    bool continueAvailable = false;   // a match in memory or a save on disk can be continued
    bool continueInMemory = false;    // the match is still in memory (starting a new one asks first)
    std::string continueLabel;        // "Ден 5 · 13:30 · Срещу бот (Среден)"
    bool requestContinue = false;
    bool confirmNewOpen = false;      // "start a new match?" dialog
    int confirmIndex = 1;             // 0 = start new, 1 = cancel (default)
    enum class MainAction { CONTINUE, PLAY, SETTINGS, QUIT };
    int mainButtonCount() const { return continueAvailable ? 4 : 3; }
    MainAction mainButtonAction(int index) const;
    void activateMainButton(int index);
    void drawContinueButton(sf::RenderWindow& window, sf::FloatRect bounds, bool selected);
    void drawConfirmNew(sf::RenderWindow& window);
    bool handleConfirmNewEvent(const sf::Event& event, const sf::RenderWindow& window);

    // Enter/Space must be released before they can select again (filters key auto-repeat)
    bool enterHeld = false;
    bool spaceHeld = false;

    sf::Font font;
    bool fontLoaded;

    UI_playControls playControls;

    void drawHeader(sf::RenderWindow& window);
    void drawMainMenu(sf::RenderWindow& window);
    void drawModeSelectMenu(sf::RenderWindow& window);
    void drawBotDifficultyMenu(sf::RenderWindow& window);
    void drawSettingsMenu(sf::RenderWindow& window);

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

    bool isPlayRequested() const { return requestPlay; }
    bool isQuitRequested() const { return requestQuit; }
    void resetPlayRequest() { requestPlay = false; }
    ControlScheme getSelectedControlScheme() const {
        return playControls.getSelectedScheme();
    }
    BotDifficulty getSelectedBotDifficulty() const {
        return selectedBotDifficulty;
    }

    // --- Team b-session (F-02): continue an unfinished match ---
    // available: show ПРОДЪЛЖИ (preselected); inMemory: the match is still running, so ИГРА asks before
    // replacing it; label: one-line description of the match shown on the button
    void setContinueInfo(bool available, bool inMemory, const std::string& label);
    bool isContinueRequested() const { return requestContinue; }
    void resetContinueRequest() { requestContinue = false; }
    // Rect the current screen draws into (UI_main zooms it by the UI scale); the settings screen
    // zooms itself, so UI_main must then draw the menu with the plain base view
    sf::FloatRect contentBounds() const;
    bool usesOwnZoom() const { return state == MenuState::SETTINGS; }
};

#endif // UI_MAINMENU_H
