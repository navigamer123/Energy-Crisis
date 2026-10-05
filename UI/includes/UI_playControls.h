#ifndef UI_PLAYCONTROLS_H
#define UI_PLAYCONTROLS_H

#include <SFML/Graphics.hpp>
#include "UI_types.h"

class UI_playControls {
private:
    int selectedIndex;   // 0..4: Options, 5: Start, 6: Back
    int activeScheme;    // 0..4 ControlScheme
    bool requestStart;
    bool requestBack;
    bool requestQuit = false;
    sf::Vector2f lastMousePos = { -999.0f, -999.0f };

    void drawButton(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                    sf::FloatRect bounds, const sf::String& text,
                    sf::Color baseColor, sf::Color hoverColor, sf::Color textColor,
                    bool isSelected);

public:
    UI_playControls();
    void handleEvent(const sf::Event& event, const sf::RenderWindow& window);
    void draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded);

    ControlScheme getSelectedScheme() const { return static_cast<ControlScheme>(activeScheme); }
    int getActiveSchemeIndex() const { return activeScheme; }
    void setActiveSchemeIndex(int index) { activeScheme = index; selectedIndex = index; }

    bool isStartRequested() const { return requestStart; }
    bool isBackRequested() const { return requestBack; }
    bool isQuitRequested() const { return requestQuit; }
    void resetRequests() { requestStart = false; requestBack = false; requestQuit = false; }
};

#endif // UI_PLAYCONTROLS_H
