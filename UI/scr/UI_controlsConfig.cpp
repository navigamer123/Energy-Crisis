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

bool UI_controlsConfig::isActionPressed(int player, ControlAction actionType, bool allowP1Arrows) const {
    const PlayerBindings& b = getPlayer(player);
    sf::Keyboard::Key k = b.getKey(actionType);
    if (k != sf::Keyboard::Key::Unknown && sf::Keyboard::isKeyPressed(k)) {
        return true;
    }
    if (player == 1 && allowP1Arrows) {
        switch (actionType) {
            case ControlAction::MOVE_UP:       return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up);
            case ControlAction::MOVE_DOWN:     return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down);
            case ControlAction::MOVE_LEFT:     return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left);
            case ControlAction::MOVE_RIGHT:    return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right);
            case ControlAction::ACTION:        return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter);
            case ControlAction::UPGRADE:       return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::End);
            case ControlAction::NEXT_BUILDING: return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::PageDown);
            case ControlAction::PREV_BUILDING: return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::PageUp);
            case ControlAction::CANCEL:        return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Delete) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Backspace);
            default: break;
        }
    }
    return false;
}
