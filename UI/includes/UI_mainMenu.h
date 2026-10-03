#ifndef UI_MAINMENU_H
#define UI_MAINMENU_H

#include <SFML/Graphics.hpp>
#include <string>
#include "UI_types.h"
#include "UI_playControls.h"

enum class MenuState {
    MAIN,
    MODE_SELECT,
    PLAY_CONTROLS,
    BOT_DIFFICULTY,
    SETTINGS,
    BOT_RIVAL      // [AI team] rival picker after Easy/Medium/Hard (UI_mainMenu_rivals.cpp)
};

class UI_mainMenu {
private:
    MenuState state;
    bool requestPlay;
    bool requestQuit;

    int selectedMainIndex;       // 0: Play, 1: Settings, 2: Quit
    int selectedModeIndex;       // 0: Co-op (2P), 1: Single Player (VS Bot), 2: Back
    int selectedDifficultyIndex; // 0: Easy, 1: Medium, 2: Hard, 3: Back
    int selectedSettingsIndex;   // 0: Volume, 1: SoundFX, 2: Difficulty, 3: Back
    sf::Vector2f lastMenuMousePos = { -999.0f, -999.0f };

    BotDifficulty selectedBotDifficulty;

    // Settings state
    int volume;                  // 0..100 % (no audio subsystem yet: stored only)
    bool soundEffects;           // no audio subsystem yet: stored only
    int settingsDifficultyIndex; // 0..2: default bot difficulty preselected in the single-player menu

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

    // [AI team] НЕВЪЗМОЖНО button and rival picker (UI/scr/UI_mainMenu_rivals.cpp)
    int selectedRivalIndex = 0;       // 0..4 rivals, 5 = random, 6 = back
    int selectedBotPersonality = 0;   // rival passed to the match (BOT_PERSONALITY_OMEGA for НЕВЪЗМОЖНО)
    BotDifficulty pendingDifficulty = BotDifficulty::MEDIUM; // tier chosen before the rival picker
    static constexpr int DIFFICULTY_ROWS = 5; // Easy, Medium, Hard, Impossible, Back
    sf::FloatRect difficultyButtonRect(int index) const;
    sf::FloatRect rivalRowRect(int index) const;
    void drawImpossibleButton(sf::RenderWindow& window, sf::FloatRect bounds, bool isSelected);
    void drawDifficultyNote(sf::RenderWindow& window, float y);
    void drawRivalMenu(sf::RenderWindow& window);
    void chooseDifficulty(int index);  // acts on a difficulty row (keyboard or mouse)
    void chooseRival(int index);       // acts on a rival row (keyboard or mouse)
    bool handleRivalEvent(const sf::Event& event, const sf::RenderWindow& window);

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
    int getSelectedBotPersonality() const { return selectedBotPersonality; } // [AI team]
};

#endif // UI_MAINMENU_H
