#ifndef UI_CONTROLSCONFIG_H
#define UI_CONTROLSCONFIG_H

#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Joystick.hpp>
#include <SFML/System/Vector2.hpp>
#include <string>

enum class ControlAction {
    MOVE_UP = 0,
    MOVE_DOWN,
    MOVE_LEFT,
    MOVE_RIGHT,
    ACTION,
    UPGRADE,
    NEXT_BUILDING,
    PREV_BUILDING,
    CANCEL,
    COUNT
};

struct PlayerBindings {
    sf::Keyboard::Key up;
    sf::Keyboard::Key down;
    sf::Keyboard::Key left;
    sf::Keyboard::Key right;
    sf::Keyboard::Key action;
    sf::Keyboard::Key upgrade;
    sf::Keyboard::Key nextBuilding;
    sf::Keyboard::Key prevBuilding;
    sf::Keyboard::Key cancel;

    sf::Keyboard::Key getKey(ControlAction actionType) const;
    void setKey(ControlAction actionType, sf::Keyboard::Key key);
};

std::string keyToString(sf::Keyboard::Key key);
const char* getControlActionNameBg(ControlAction actionType);

class UI_controlsConfig {
public:
    PlayerBindings p1;
    PlayerBindings p2;

    static UI_controlsConfig& get();

    void resetToDefaults();
    PlayerBindings& getPlayer(int player);
    const PlayerBindings& getPlayer(int player) const;

    bool isActionPressed(int player, ControlAction actionType, bool allowP1Arrows = false) const;

    // DevHub One Arcade Console & Gamepad Support
    bool isJoystickConnected(int player) const;
    bool isAnyJoystickConnected() const;
    int getConnectedJoystickCount() const;
    std::string getJoystickName(int player) const;
    std::string getJoystickStatusBg() const;

    sf::Vector2f getJoystickMoveVector(int player) const;
    bool isJoystickDirectionPressed(int player, ControlAction actionType) const;
    bool isJoystickActionPressed(int player, ControlAction actionType) const;

    int p1JoystickId = 0;
    int p2JoystickId = 1;
    void setPlayerJoystick(int player, int joyId) {
        if (player == 1) p1JoystickId = joyId;
        else if (player == 2) p2JoystickId = joyId;
    }
    int getPlayerJoystick(int player) const {
        return (player == 1) ? p1JoystickId : p2JoystickId;
    }

private:
    UI_controlsConfig();
};

#endif // UI_CONTROLSCONFIG_H
