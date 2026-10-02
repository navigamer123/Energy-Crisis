#ifndef UI_MAINMENU_H
#define UI_MAINMENU_H

#include <SFML/Graphics.hpp>
#include <string>
#include "UI_types.h"
#include "UI_playControls.h"

enum class MenuState {
    MAIN,
    PLAY_CONTROLS,
    SETTINGS
};

class UI_mainMenu {
private:
    MenuState state;
    bool requestPlay;
    bool requestQuit;

    int selectedMainIndex;       // 0: Play, 1: Settings, 2: Quit
    int selectedSettingsIndex;   // 0: Volume, 1: SoundFX, 2: Difficulty, 3: Back
    sf::Vector2f lastMenuMousePos = { -999.0f, -999.0f };

    // Settings state
    int volume;
    bool soundEffects;
    int difficultyIndex; // 0 = Easy, 1 = Normal, 2 = Hard

    sf::Font font;
    bool fontLoaded;

    UI_playControls playControls;

    void drawHeader(sf::RenderWindow& window);
    void drawMainMenu(sf::RenderWindow& window);
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

    bool isPlayRequested() const { return requestPlay; }
    bool isQuitRequested() const { return requestQuit; }
    void resetPlayRequest() { requestPlay = false; }
    ControlScheme getSelectedControlScheme() const {
        return playControls.getSelectedScheme();
    }
};

#endif // UI_MAINMENU_H
