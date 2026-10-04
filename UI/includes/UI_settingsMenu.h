#ifndef UI_SETTINGSMENU_H
#define UI_SETTINGSMENU_H

// =============================================================================
// Team b-session (F-06, F-10, UX-12, HX-15): the НАСТРОЙКИ screen, shared by the
// main menu and the in-match pause menu. Four tabs (ЕКРАН, ЗВУК, ИГРА, УПРАВЛЕНИЕ);
// every change goes straight into gameSettings() (UI_main re-applies the window),
// and the file is written when the screen closes. Keyboard, mouse and gamepad.
// =============================================================================

#include <SFML/Graphics.hpp>
#include <functional>
#include <string>
#include <vector>
#include "UI_keybindMenu.h"

class UI_settingsMenu {
public:
    void open(bool inMatch);
    bool isOpen() const { return active; }
    // Closing saves the settings file
    void close();

    // `window`'s current view must be the 1600x900 base view (the screen applies its own UI scale)
    void handleEvent(const sf::Event& event, const sf::RenderWindow& window);
    void draw(sf::RenderWindow& window, const sf::Font& font);
    sf::FloatRect panelRect() const;

private:
    struct Row {
        std::string label;
        std::function<std::string()> value;     // text in the value box ("" = button row)
        std::function<void(int)> change;         // -1 / +1 from arrows or clicks on the arrows
        std::function<void()> activate;          // Enter / click on the value
        std::string help;                        // description shown for the focused row
    };

    bool active = false;
    bool inMatch = false;
    int tab = 0;
    int focus = -1;  // -1 = tab bar, 0..rows-1 = rows, rows = НАЗАД button
    std::string flash;   // short confirmation ("Готово")
    float flashTimer = 0.0f;
    sf::Clock flashClock;
    sf::Vector2f lastMouse{ -999.0f, -999.0f };
    UI_keybindMenu keybinds;

    std::vector<Row> buildRows();
    sf::View viewFor(const sf::RenderWindow& window) const;
    sf::FloatRect tabRect(int i) const;
    sf::FloatRect rowRect(int i) const;
    sf::FloatRect valueRect(int i) const;
    sf::FloatRect backRect() const;
    void setFlash(const std::string& text);
};

#endif // UI_SETTINGSMENU_H
