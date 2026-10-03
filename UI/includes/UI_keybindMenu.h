#ifndef UI_KEYBINDMENU_H
#define UI_KEYBINDMENU_H

// =============================================================================
// Team b-session (F-10): "КЛАВИШИ" screen. A table of every action with a primary
// and a secondary key for each player. Enter (or a click) on a cell waits for the
// next key; Backspace / right-click clears it. Keys used twice (across both players)
// or reserved for system shortcuts turn red and block ЗАПАЗИ.
// Works on a draft copy: ОТКАЗ / Esc throws the changes away.
// =============================================================================

#include <SFML/Graphics.hpp>
#include <string>
#include "UI_inputmap.h"

class UI_keybindMenu {
public:
    void open();
    bool isOpen() const { return active; }
    bool isCapturing() const { return capturing; }

    // `window`'s current view must be the 1600x900 base view (the menu applies its own UI scale)
    void handleEvent(const sf::Event& event, const sf::RenderWindow& window);
    void draw(sf::RenderWindow& window, const sf::Font& font);
    sf::FloatRect panelRect() const;

private:
    enum Button { BTN_DEFAULTS = 0, BTN_SAVE = 1, BTN_CANCEL = 2, BTN_COUNT = 3 };

    bool active = false;
    bool capturing = false;
    InputMap draft;
    int row = 0;  // 0..INPUT_ACTION_COUNT-1 = actions, INPUT_ACTION_COUNT = button row
    int col = 0;  // cells: 0 = P1 primary, 1 = P1 secondary, 2 = P2 primary, 3 = P2 secondary; buttons: 0..2
    std::string status;
    bool statusIsError = false;
    sf::Vector2f lastMouse{ -999.0f, -999.0f };

    sf::View viewFor(const sf::RenderWindow& window) const;
    sf::FloatRect cellRect(int r, int c) const;
    sf::FloatRect buttonRect(int b) const;
    void activateButton(int b);
    void assignCapturedKey(int code);
    void setStatus(const std::string& text, bool error);
    void finish(bool save);
};

#endif // UI_KEYBINDMENU_H
