#include "../includes/UI_controlsConfig.h"
#include <SFML/Window/Keyboard.hpp>

sf::Keyboard::Key PlayerBindings::getKey(ControlAction actionType) const {
    switch (actionType) {
        case ControlAction::MOVE_UP:       return up;
        case ControlAction::MOVE_DOWN:     return down;
        case ControlAction::MOVE_LEFT:     return left;
        case ControlAction::MOVE_RIGHT:    return right;
        case ControlAction::ACTION:        return action;
        case ControlAction::UPGRADE:       return upgrade;
        case ControlAction::NEXT_BUILDING: return nextBuilding;
        case ControlAction::PREV_BUILDING: return prevBuilding;
        case ControlAction::CANCEL:        return cancel;
        default:                           return sf::Keyboard::Key::Unknown;
    }
}

void PlayerBindings::setKey(ControlAction actionType, sf::Keyboard::Key key) {
    switch (actionType) {
        case ControlAction::MOVE_UP:       up = key; break;
        case ControlAction::MOVE_DOWN:     down = key; break;
        case ControlAction::MOVE_LEFT:     left = key; break;
        case ControlAction::MOVE_RIGHT:    right = key; break;
        case ControlAction::ACTION:        action = key; break;
        case ControlAction::UPGRADE:       upgrade = key; break;
        case ControlAction::NEXT_BUILDING: nextBuilding = key; break;
        case ControlAction::PREV_BUILDING: prevBuilding = key; break;
        case ControlAction::CANCEL:        cancel = key; break;
        default: break;
    }
}

const char* getControlActionNameBg(ControlAction actionType) {
    switch (actionType) {
        case ControlAction::MOVE_UP:       return "Движение Нагоре";
        case ControlAction::MOVE_DOWN:     return "Движение Надолу";
        case ControlAction::MOVE_LEFT:     return "Движение Наляво";
        case ControlAction::MOVE_RIGHT:    return "Движение Надясно";
        case ControlAction::ACTION:        return "Действие / Строеж";
        case ControlAction::UPGRADE:       return "Надграждане на мина";
        case ControlAction::NEXT_BUILDING: return "Следваща сграда";
        case ControlAction::PREV_BUILDING: return "Предишна сграда";
        case ControlAction::CANCEL:        return "Отказ / Разрушаване";
        default:                           return "";
    }
}

std::string keyToString(sf::Keyboard::Key key) {
    // Letters A..Z
    if (key >= sf::Keyboard::Key::A && key <= sf::Keyboard::Key::Z) {
        int offset = static_cast<int>(key) - static_cast<int>(sf::Keyboard::Key::A);
        return std::string(1, static_cast<char>('A' + offset));
    }
    // Number keys 0..9
    if (key >= sf::Keyboard::Key::Num0 && key <= sf::Keyboard::Key::Num9) {
        int offset = static_cast<int>(key) - static_cast<int>(sf::Keyboard::Key::Num0);
        return std::string(1, static_cast<char>('0' + offset));
    }
    // Numpad 0..9
    if (key >= sf::Keyboard::Key::Numpad0 && key <= sf::Keyboard::Key::Numpad9) {
        int offset = static_cast<int>(key) - static_cast<int>(sf::Keyboard::Key::Numpad0);
        return "Num " + std::string(1, static_cast<char>('0' + offset));
    }
    // Function keys F1..F12
    if (key >= sf::Keyboard::Key::F1 && key <= sf::Keyboard::Key::F12) {
        int offset = static_cast<int>(key) - static_cast<int>(sf::Keyboard::Key::F1);
        return "F" + std::to_string(offset + 1);
    }

    switch (key) {
        case sf::Keyboard::Key::Up:        return "UP";
        case sf::Keyboard::Key::Down:      return "DOWN";
        case sf::Keyboard::Key::Left:      return "LEFT";
        case sf::Keyboard::Key::Right:     return "RIGHT";
        case sf::Keyboard::Key::Space:     return "SPACE";
        case sf::Keyboard::Key::Enter:     return "ENTER";
        case sf::Keyboard::Key::Escape:    return "ESC";
        case sf::Keyboard::Key::LControl:  return "L-CTRL";
        case sf::Keyboard::Key::RControl:  return "R-CTRL";
        case sf::Keyboard::Key::LShift:    return "L-SHIFT";
        case sf::Keyboard::Key::RShift:    return "R-SHIFT";
        case sf::Keyboard::Key::LAlt:      return "L-ALT";
        case sf::Keyboard::Key::RAlt:      return "R-ALT";
        case sf::Keyboard::Key::Tab:       return "TAB";
        case sf::Keyboard::Key::Backspace: return "BACKSPACE";
        case sf::Keyboard::Key::Delete:    return "DELETE";
        case sf::Keyboard::Key::PageUp:    return "PG UP";
        case sf::Keyboard::Key::PageDown:  return "PG DN";
        case sf::Keyboard::Key::Home:      return "HOME";
        case sf::Keyboard::Key::End:       return "END";
        case sf::Keyboard::Key::Insert:    return "INSERT";
        case sf::Keyboard::Key::Comma:     return ",";
        case sf::Keyboard::Key::Period:    return ".";
        case sf::Keyboard::Key::Slash:     return "/";
        case sf::Keyboard::Key::Backslash: return "\\";
        case sf::Keyboard::Key::Semicolon: return ";";
        case sf::Keyboard::Key::Apostrophe: return "'";
        case sf::Keyboard::Key::Hyphen:    return "-";
        case sf::Keyboard::Key::Equal:     return "=";
        case sf::Keyboard::Key::LBracket:  return "[";
        case sf::Keyboard::Key::RBracket:  return "]";
        case sf::Keyboard::Key::Grave:     return "`";
        case sf::Keyboard::Key::Add:       return "Num +";
        case sf::Keyboard::Key::Subtract:  return "Num -";
        case sf::Keyboard::Key::Multiply:  return "Num *";
        case sf::Keyboard::Key::Divide:    return "Num /";
        default:                           return "?";
    }
}

UI_controlsConfig::UI_controlsConfig() {
    resetToDefaults();
}

UI_controlsConfig& UI_controlsConfig::get() {
    static UI_controlsConfig instance;
    return instance;
}

void UI_controlsConfig::resetToDefaults() {
    // Player 1 defaults
    p1.up           = sf::Keyboard::Key::W;
    p1.down         = sf::Keyboard::Key::S;
    p1.left         = sf::Keyboard::Key::A;
    p1.right        = sf::Keyboard::Key::D;
    p1.action       = sf::Keyboard::Key::Space;
    p1.upgrade      = sf::Keyboard::Key::F;
    p1.nextBuilding = sf::Keyboard::Key::E;
    p1.prevBuilding = sf::Keyboard::Key::Q;
    p1.cancel       = sf::Keyboard::Key::X;

    // Player 2 defaults
    p2.up           = sf::Keyboard::Key::Up;
    p2.down         = sf::Keyboard::Key::Down;
    p2.left         = sf::Keyboard::Key::Left;
    p2.right        = sf::Keyboard::Key::Right;
    p2.action       = sf::Keyboard::Key::Enter;
    p2.upgrade      = sf::Keyboard::Key::RShift;
    p2.nextBuilding = sf::Keyboard::Key::PageDown;
    p2.prevBuilding = sf::Keyboard::Key::PageUp;
    p2.cancel       = sf::Keyboard::Key::Delete;
}

PlayerBindings& UI_controlsConfig::getPlayer(int player) {
    return (player == 1 ? p1 : p2);
}

const PlayerBindings& UI_controlsConfig::getPlayer(int player) const {
    return (player == 1 ? p1 : p2);
}

#include <cmath>
#include <algorithm>

bool UI_controlsConfig::isActionPressed(int player, ControlAction actionType, bool allowP1Arrows) const {
    const PlayerBindings& b = getPlayer(player);
    sf::Keyboard::Key k = b.getKey(actionType);
    if (k != sf::Keyboard::Key::Unknown && sf::Keyboard::isKeyPressed(k)) {
        return true;
    }
    if (player == 1 && allowP1Arrows) {
        switch (actionType) {
            case ControlAction::MOVE_UP:       if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up)) return true; break;
            case ControlAction::MOVE_DOWN:     if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down)) return true; break;
            case ControlAction::MOVE_LEFT:     if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)) return true; break;
            case ControlAction::MOVE_RIGHT:    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) return true; break;
            case ControlAction::ACTION:        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter)) return true; break;
            case ControlAction::UPGRADE:       if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::End)) return true; break;
            case ControlAction::NEXT_BUILDING: if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::PageDown)) return true; break;
            case ControlAction::PREV_BUILDING: if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::PageUp)) return true; break;
            case ControlAction::CANCEL:        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Delete) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Backspace)) return true; break;
            default: break;
        }
    }
    if (isJoystickActionPressed(player, actionType) || isJoystickDirectionPressed(player, actionType)) {
        return true;
    }
    return false;
}

bool UI_controlsConfig::isJoystickConnected(int player) const {
    if (player == 1) {
        return sf::Joystick::isConnected(0);
    }
    if (sf::Joystick::isConnected(1)) {
        return true;
    }
    // Check if Joystick 0 is a dual-player arcade encoder board (e.g. XinMo 2-Player USB)
    if (sf::Joystick::isConnected(0)) {
        unsigned int btnCount = sf::Joystick::getButtonCount(0);
        if (btnCount >= 12 || sf::Joystick::hasAxis(0, sf::Joystick::Axis::U) || sf::Joystick::hasAxis(0, sf::Joystick::Axis::Z)) {
            return true;
        }
    }
    return false;
}

bool UI_controlsConfig::isAnyJoystickConnected() const {
    for (unsigned int i = 0; i < sf::Joystick::Count; ++i) {
        if (sf::Joystick::isConnected(i)) return true;
    }
    return false;
}

int UI_controlsConfig::getConnectedJoystickCount() const {
    int count = 0;
    for (unsigned int i = 0; i < sf::Joystick::Count; ++i) {
        if (sf::Joystick::isConnected(i)) count++;
    }
    return count;
}

std::string UI_controlsConfig::getJoystickName(int player) const {
    if (player == 1) {
        if (sf::Joystick::isConnected(0)) {
            auto id = sf::Joystick::getIdentification(0);
            std::string name = id.name.toAnsiString();
            return name.empty() ? "DevHub Joy 1" : name;
        }
        return "Няма";
    }
    if (sf::Joystick::isConnected(1)) {
        auto id = sf::Joystick::getIdentification(1);
        std::string name = id.name.toAnsiString();
        return name.empty() ? "DevHub Joy 2" : name;
    }
    if (sf::Joystick::isConnected(0) && isJoystickConnected(2)) {
        return "DevHub Dual (P2)";
    }
    return "Няма";
}

std::string UI_controlsConfig::getJoystickStatusBg() const {
    bool p1Connected = isJoystickConnected(1);
    bool p2Connected = isJoystickConnected(2);
    if (p1Connected && p2Connected) {
        return "DevHub One Аркада: ИГРАЧ 1 [СВЪРЗАН]  ·  ИГРАЧ 2 [СВЪРЗАН]";
    } else if (p1Connected) {
        return "DevHub One Аркада: ИГРАЧ 1 [СВЪРЗАН]  ·  ИГРАЧ 2 [КЛАВИАТУРА]";
    } else if (p2Connected) {
        return "DevHub One Аркада: ИГРАЧ 1 [КЛАВИАТУРА]  ·  ИГРАЧ 2 [СВЪРЗАН]";
    }
    return "DevHub One Аркада: В готовност (Включете контролери/аркадни стикове)";
}

sf::Vector2f UI_controlsConfig::getJoystickMoveVector(int player) const {
    float x = 0.0f;
    float y = 0.0f;

    if (player == 1 && sf::Joystick::isConnected(0)) {
        float rawX = sf::Joystick::getAxisPosition(0, sf::Joystick::Axis::X);
        float rawY = sf::Joystick::getAxisPosition(0, sf::Joystick::Axis::Y);
        float povX = sf::Joystick::getAxisPosition(0, sf::Joystick::Axis::PovX);
        float povY = sf::Joystick::getAxisPosition(0, sf::Joystick::Axis::PovY);

        if (std::abs(rawX) > 20.0f) x = rawX / 100.0f;
        if (std::abs(rawY) > 20.0f) y = rawY / 100.0f;

        if (std::abs(x) < 0.1f && std::abs(povX) > 30.0f) x = (povX > 0.0f ? 1.0f : -1.0f);
        if (std::abs(y) < 0.1f && std::abs(povY) > 30.0f) y = (povY > 0.0f ? 1.0f : -1.0f);

        // Standard D-pad buttons fallback on some DirectInput/arcade encoders
        unsigned int btnCount = sf::Joystick::getButtonCount(0);
        if (btnCount > 14) {
            if (sf::Joystick::isButtonPressed(0, 11)) y = -1.0f;
            if (sf::Joystick::isButtonPressed(0, 12)) y = 1.0f;
            if (sf::Joystick::isButtonPressed(0, 13)) x = -1.0f;
            if (sf::Joystick::isButtonPressed(0, 14)) x = 1.0f;
        }
    } else if (player == 2) {
        if (sf::Joystick::isConnected(1)) {
            float rawX = sf::Joystick::getAxisPosition(1, sf::Joystick::Axis::X);
            float rawY = sf::Joystick::getAxisPosition(1, sf::Joystick::Axis::Y);
            float povX = sf::Joystick::getAxisPosition(1, sf::Joystick::Axis::PovX);
            float povY = sf::Joystick::getAxisPosition(1, sf::Joystick::Axis::PovY);

            if (std::abs(rawX) > 20.0f) x = rawX / 100.0f;
            if (std::abs(rawY) > 20.0f) y = rawY / 100.0f;

            if (std::abs(x) < 0.1f && std::abs(povX) > 30.0f) x = (povX > 0.0f ? 1.0f : -1.0f);
            if (std::abs(y) < 0.1f && std::abs(povY) > 30.0f) y = (povY > 0.0f ? 1.0f : -1.0f);

            unsigned int btnCount = sf::Joystick::getButtonCount(1);
            if (btnCount > 14) {
                if (sf::Joystick::isButtonPressed(1, 11)) y = -1.0f;
                if (sf::Joystick::isButtonPressed(1, 12)) y = 1.0f;
                if (sf::Joystick::isButtonPressed(1, 13)) x = -1.0f;
                if (sf::Joystick::isButtonPressed(1, 14)) x = 1.0f;
            }
        } else if (sf::Joystick::isConnected(0)) {
            // Dual-player single arcade board fallback (XinMo / DragonRise 2-player)
            float rawX = 0.0f;
            float rawY = 0.0f;
            if (sf::Joystick::hasAxis(0, sf::Joystick::Axis::U) && sf::Joystick::hasAxis(0, sf::Joystick::Axis::V)) {
                rawX = sf::Joystick::getAxisPosition(0, sf::Joystick::Axis::U);
                rawY = sf::Joystick::getAxisPosition(0, sf::Joystick::Axis::V);
            } else if (sf::Joystick::hasAxis(0, sf::Joystick::Axis::Z) && sf::Joystick::hasAxis(0, sf::Joystick::Axis::R)) {
                rawX = sf::Joystick::getAxisPosition(0, sf::Joystick::Axis::Z);
                rawY = sf::Joystick::getAxisPosition(0, sf::Joystick::Axis::R);
            } else {
                rawX = sf::Joystick::getAxisPosition(0, sf::Joystick::Axis::PovX);
                rawY = sf::Joystick::getAxisPosition(0, sf::Joystick::Axis::PovY);
            }

            if (std::abs(rawX) > 20.0f) x = rawX / 100.0f;
            if (std::abs(rawY) > 20.0f) y = rawY / 100.0f;
        }
    }

    x = std::max(-1.0f, std::min(1.0f, x));
    y = std::max(-1.0f, std::min(1.0f, y));
    return sf::Vector2f(x, y);
}

bool UI_controlsConfig::isJoystickDirectionPressed(int player, ControlAction actionType) const {
    sf::Vector2f vec = getJoystickMoveVector(player);
    switch (actionType) {
        case ControlAction::MOVE_UP:    return vec.y < -0.40f;
        case ControlAction::MOVE_DOWN:  return vec.y > 0.40f;
        case ControlAction::MOVE_LEFT:  return vec.x < -0.40f;
        case ControlAction::MOVE_RIGHT: return vec.x > 0.40f;
        default: return false;
    }
}

bool UI_controlsConfig::isJoystickActionPressed(int player, ControlAction actionType) const {
    unsigned int joyId = (player == 1) ? 0 : (sf::Joystick::isConnected(1) ? 1 : 0);
    if (!sf::Joystick::isConnected(joyId)) return false;

    unsigned int btnOffset = (player == 2 && joyId == 0) ? 10 : 0;
    unsigned int btnCount = sf::Joystick::getButtonCount(joyId);

    auto isBtn = [&](unsigned int b) -> bool {
        unsigned int actualBtn = b + btnOffset;
        return (actualBtn < btnCount) && sf::Joystick::isButtonPressed(joyId, actualBtn);
    };

    switch (actionType) {
        case ControlAction::ACTION:
            // Arcade Button 1 (A) or Start (Button 7)
            return isBtn(0) || isBtn(7);

        case ControlAction::CANCEL:
            // Arcade Button 2 (B) or Select / Coin (Button 6)
            return isBtn(1) || isBtn(6);

        case ControlAction::UPGRADE:
            // Arcade Button 3 (X) or LB / Button 4
            return isBtn(2) || isBtn(4);

        case ControlAction::NEXT_BUILDING:
            // Arcade Button 4 (Y) or RB / Button 5
            return isBtn(3) || isBtn(5);

        case ControlAction::PREV_BUILDING:
            // LB / Button 4 or Arcade Button 3 (X) if RB is Next
            return isBtn(4);

        default:
            return false;
    }
}
