// =============================================================================
// Team b-session (F-05, F-10): keyboard + gamepad input layer (see UI_input.h)
// =============================================================================
#include "../includes/UI_input.h"
#include "../includes/UI_settings.h"
#include <algorithm>
#include <cmath>

// The SFML-free InputMap stores sf::Keyboard::Scancode values as plain ints: prove they match
#define EC_CHECK_SCAN(name) static_assert(static_cast<int>(sf::Keyboard::Scan::name) == KeyCode::name, "KeyCode::" #name " != sf::Keyboard::Scan::" #name);
EC_CHECK_SCAN(Unknown) EC_CHECK_SCAN(A) EC_CHECK_SCAN(B) EC_CHECK_SCAN(C) EC_CHECK_SCAN(D) EC_CHECK_SCAN(E) EC_CHECK_SCAN(F)
EC_CHECK_SCAN(G) EC_CHECK_SCAN(H) EC_CHECK_SCAN(I) EC_CHECK_SCAN(J) EC_CHECK_SCAN(K) EC_CHECK_SCAN(L) EC_CHECK_SCAN(M)
EC_CHECK_SCAN(N) EC_CHECK_SCAN(O) EC_CHECK_SCAN(P) EC_CHECK_SCAN(Q) EC_CHECK_SCAN(R) EC_CHECK_SCAN(S) EC_CHECK_SCAN(T)
EC_CHECK_SCAN(U) EC_CHECK_SCAN(V) EC_CHECK_SCAN(W) EC_CHECK_SCAN(X) EC_CHECK_SCAN(Y) EC_CHECK_SCAN(Z)
EC_CHECK_SCAN(Num1) EC_CHECK_SCAN(Num2) EC_CHECK_SCAN(Num3) EC_CHECK_SCAN(Num4) EC_CHECK_SCAN(Num5)
EC_CHECK_SCAN(Num6) EC_CHECK_SCAN(Num7) EC_CHECK_SCAN(Num8) EC_CHECK_SCAN(Num9) EC_CHECK_SCAN(Num0)
EC_CHECK_SCAN(Enter) EC_CHECK_SCAN(Escape) EC_CHECK_SCAN(Backspace) EC_CHECK_SCAN(Tab) EC_CHECK_SCAN(Space)
EC_CHECK_SCAN(Hyphen) EC_CHECK_SCAN(Equal) EC_CHECK_SCAN(LBracket) EC_CHECK_SCAN(RBracket) EC_CHECK_SCAN(Backslash)
EC_CHECK_SCAN(Semicolon) EC_CHECK_SCAN(Apostrophe) EC_CHECK_SCAN(Grave) EC_CHECK_SCAN(Comma) EC_CHECK_SCAN(Period)
EC_CHECK_SCAN(Slash) EC_CHECK_SCAN(F1) EC_CHECK_SCAN(F2) EC_CHECK_SCAN(F3) EC_CHECK_SCAN(F4) EC_CHECK_SCAN(F5)
EC_CHECK_SCAN(F6) EC_CHECK_SCAN(F7) EC_CHECK_SCAN(F8) EC_CHECK_SCAN(F9) EC_CHECK_SCAN(F10) EC_CHECK_SCAN(F11)
EC_CHECK_SCAN(F12) EC_CHECK_SCAN(CapsLock) EC_CHECK_SCAN(Insert) EC_CHECK_SCAN(Home) EC_CHECK_SCAN(PageUp)
EC_CHECK_SCAN(Delete) EC_CHECK_SCAN(End) EC_CHECK_SCAN(PageDown) EC_CHECK_SCAN(Right) EC_CHECK_SCAN(Left)
EC_CHECK_SCAN(Down) EC_CHECK_SCAN(Up) EC_CHECK_SCAN(NumpadDivide) EC_CHECK_SCAN(NumpadMultiply)
EC_CHECK_SCAN(NumpadMinus) EC_CHECK_SCAN(NumpadPlus) EC_CHECK_SCAN(NumpadEnter) EC_CHECK_SCAN(NumpadDecimal)
EC_CHECK_SCAN(Numpad1) EC_CHECK_SCAN(Numpad2) EC_CHECK_SCAN(Numpad3) EC_CHECK_SCAN(Numpad4) EC_CHECK_SCAN(Numpad5)
EC_CHECK_SCAN(Numpad6) EC_CHECK_SCAN(Numpad7) EC_CHECK_SCAN(Numpad8) EC_CHECK_SCAN(Numpad9) EC_CHECK_SCAN(Numpad0)
EC_CHECK_SCAN(LControl) EC_CHECK_SCAN(LShift) EC_CHECK_SCAN(LAlt) EC_CHECK_SCAN(RControl) EC_CHECK_SCAN(RShift)
EC_CHECK_SCAN(RAlt)
static_assert(static_cast<int>(sf::Keyboard::ScancodeCount) <= KeyCode::Max, "KeyCode::Max too small");
#undef EC_CHECK_SCAN

namespace {

constexpr float STICK_DEADZONE = 0.30f;   // fraction of full deflection ignored (worn sticks drift)
constexpr float STICK_DIGITAL = 0.55f;    // deflection that counts as a digital direction
constexpr float MENU_REPEAT_DELAY = 0.38f;
constexpr float MENU_REPEAT_RATE = 0.11f;

bool padButton(unsigned int j, unsigned int button) {
    return button < sf::Joystick::getButtonCount(j) && sf::Joystick::isButtonPressed(j, button);
}

float padAxis(unsigned int j, sf::Joystick::Axis axis) {
    if (!sf::Joystick::hasAxis(j, axis)) return 0.0f;
    return sf::Joystick::getAxisPosition(j, axis) / 100.0f;
}

// Left stick with a rescaled dead zone, plus the D-pad (point-of-view hat)
sf::Vector2f padStick(unsigned int j) {
    sf::Vector2f v(padAxis(j, sf::Joystick::Axis::X), padAxis(j, sf::Joystick::Axis::Y));
    auto dz = [](float a) {
        float m = std::fabs(a);
        if (m < STICK_DEADZONE) return 0.0f;
        float r = (m - STICK_DEADZONE) / (1.0f - STICK_DEADZONE);
        return (a < 0.0f ? -1.0f : 1.0f) * std::min(1.0f, r);
    };
    v.x = dz(v.x);
    v.y = dz(v.y);
    float povX = padAxis(j, sf::Joystick::Axis::PovX);
    float povY = padAxis(j, sf::Joystick::Axis::PovY);
#ifdef _WIN32
    povY = -povY; // SFML on Windows reports the hat's "up" as positive Y
#endif
    if (std::fabs(povX) > 0.5f) v.x = (povX > 0.0f) ? 1.0f : -1.0f;
    if (std::fabs(povY) > 0.5f) v.y = (povY > 0.0f) ? 1.0f : -1.0f;
    return v;
}

int dominantDir(sf::Vector2f v) {
    if (std::fabs(v.x) < STICK_DIGITAL && std::fabs(v.y) < STICK_DIGITAL) return -1;
    if (std::fabs(v.y) >= std::fabs(v.x)) return (v.y < 0.0f) ? 0 : 1;
    return (v.x < 0.0f) ? 2 : 3;
}

int padButtonFor(InputAction a) {
    switch (a) {
        case InputAction::Action: return GamepadButton::A;
        case InputAction::Cancel: return GamepadButton::B;
        case InputAction::Upgrade: return GamepadButton::X;
        case InputAction::NextBuilding: return GamepadButton::RB;
        case InputAction::PrevBuilding: return GamepadButton::LB;
        case InputAction::Quick1: return GamepadButton::Y;
        default: return -1;
    }
}

sf::Event keyEvent(bool pressed, sf::Keyboard::Key code, sf::Keyboard::Scancode scan) {
    if (pressed) {
        sf::Event::KeyPressed k;
        k.code = code;
        k.scancode = scan;
        return sf::Event(k);
    }
    sf::Event::KeyReleased k;
    k.code = code;
    k.scancode = scan;
    return sf::Event(k);
}

void emitKey(std::vector<sf::Event>& out, sf::Keyboard::Key code, sf::Keyboard::Scancode scan) {
    out.push_back(keyEvent(true, code, scan));
    out.push_back(keyEvent(false, code, scan));
}

} // namespace

InputRouter& inputRouter() {
    static InputRouter router;
    return router;
}

void InputRouter::setMatchContext(bool single, ControlScheme s) {
    singlePlayer = single;
    controlScheme = s;
}

std::vector<unsigned int> InputRouter::connectedPads() {
    std::vector<unsigned int> pads;
    for (unsigned int j = 0; j < sf::Joystick::Count; ++j) {
        // Ignore devices that are not game controllers (some mice/keyboards register as joysticks)
        if (sf::Joystick::isConnected(j) && sf::Joystick::getButtonCount(j) >= 4) pads.push_back(j);
    }
    return pads;
}

std::string InputRouter::padName(unsigned int joystickId) {
    if (!sf::Joystick::isConnected(joystickId)) return std::string();
    sf::String name = sf::Joystick::getIdentification(joystickId).name;
    auto u8 = name.toUtf8();
    std::string s(u8.begin(), u8.end());
    if (s.empty()) s = "Gamepad " + std::to_string(joystickId + 1);
    if (s.size() > 34) s = s.substr(0, 31) + "...";
    return s;
}

int InputRouter::padFor(int player) const {
    if (!gameSettings().gamepadEnabled || (player != 1 && player != 2)) return -1;
    std::vector<unsigned int> pads = connectedPads();
    if (pads.empty()) return -1;
    if (singlePlayer) return (player == 1) ? static_cast<int>(pads[0]) : -1;
    if (pads.size() >= 2) return static_cast<int>(pads[player - 1]);
    return (player == gameSettings().singlePadOwner) ? static_cast<int>(pads[0]) : -1;
}

int InputRouter::playerForPad(unsigned int joystickId) const {
    for (int p = 1; p <= 2; ++p)
        if (padFor(p) == static_cast<int>(joystickId)) return p;
    return 0;
}

void InputRouter::refreshAssignment() {
    cachedPad[1] = padFor(1);
    cachedPad[2] = padFor(2);
}

bool InputRouter::keyHeld(int player, InputAction a) const {
    const InputMap& map = gameSettings().keys;
    auto down = [](int code) {
        return code != KeyCode::Unknown && sf::Keyboard::isKeyPressed(static_cast<sf::Keyboard::Scancode>(code));
    };
    const KeyBinding& own = map.get(player, a);
    if (down(own.primary) || down(own.secondary)) return true;
    if (singlePlayer && player == 1) {
        const KeyBinding& other = map.get(2, a);
        if (down(other.primary) || down(other.secondary)) return true;
    }
    return false;
}

bool InputRouter::keyMatches(int player, InputAction a, sf::Keyboard::Scancode sc) const {
    int code = static_cast<int>(sc);
    const InputMap& map = gameSettings().keys;
    if (map.isBound(code, player, a)) return true;
    return singlePlayer && player == 1 && map.isBound(code, 2, a);
}

PlayerIntent InputRouter::poll(int player, bool keyboardMovement) const {
    PlayerIntent in;
    for (int a = 0; a < INPUT_ACTION_COUNT; ++a) {
        InputAction act = static_cast<InputAction>(a);
        bool isMove = (act == InputAction::MoveUp || act == InputAction::MoveDown ||
                       act == InputAction::MoveLeft || act == InputAction::MoveRight);
        if (isMove && !keyboardMovement) continue;
        in.held[a] = keyHeld(player, act);
    }
    in.move.x = (in.is(InputAction::MoveRight) ? 1.0f : 0.0f) - (in.is(InputAction::MoveLeft) ? 1.0f : 0.0f);
    in.move.y = (in.is(InputAction::MoveDown) ? 1.0f : 0.0f) - (in.is(InputAction::MoveUp) ? 1.0f : 0.0f);

    int pad = padFor(player);
    if (pad >= 0) {
        unsigned int j = static_cast<unsigned int>(pad);
        in.padActive = true;
        sf::Vector2f stick = padStick(j);
        in.move.x = std::max(-1.0f, std::min(1.0f, in.move.x + stick.x));
        in.move.y = std::max(-1.0f, std::min(1.0f, in.move.y + stick.y));
        if (stick.y <= -STICK_DIGITAL) in.held[static_cast<int>(InputAction::MoveUp)] = true;
        if (stick.y >= STICK_DIGITAL) in.held[static_cast<int>(InputAction::MoveDown)] = true;
        if (stick.x <= -STICK_DIGITAL) in.held[static_cast<int>(InputAction::MoveLeft)] = true;
        if (stick.x >= STICK_DIGITAL) in.held[static_cast<int>(InputAction::MoveRight)] = true;
        for (int a = 0; a < INPUT_ACTION_COUNT; ++a) {
            int b = padButtonFor(static_cast<InputAction>(a));
            if (b >= 0 && padButton(j, static_cast<unsigned int>(b))) in.held[a] = true;
        }
    }
    in.up = in.is(InputAction::MoveUp);
    in.down = in.is(InputAction::MoveDown);
    in.left = in.is(InputAction::MoveLeft);
    in.right = in.is(InputAction::MoveRight);
    return in;
}

std::string keyLabelLocalized(int keyCode) {
    if (keyCode >= KeyCode::A && keyCode <= KeyCode::Z) {
        // Letters follow the active layout (AZERTY: the "W" position types Z)
        sf::Keyboard::Key k = sf::Keyboard::localize(static_cast<sf::Keyboard::Scancode>(keyCode));
        int ki = static_cast<int>(k);
        int ka = static_cast<int>(sf::Keyboard::Key::A);
        int kz = static_cast<int>(sf::Keyboard::Key::Z);
        if (ki >= ka && ki <= kz) return std::string(1, static_cast<char>('A' + (ki - ka)));
    }
    return keyCodeLabel(keyCode);
}

std::string InputRouter::padLabel(InputAction a) {
    switch (padButtonFor(a)) {
        case GamepadButton::A: return "A";
        case GamepadButton::B: return "B";
        case GamepadButton::X: return "X";
        case GamepadButton::Y: return "Y";
        case GamepadButton::LB: return "LB";
        case GamepadButton::RB: return "RB";
        default: return std::string();
    }
}

std::string InputRouter::hint(int player, InputAction a, bool withPad, int maxKeys) const {
    const InputMap& map = gameSettings().keys;
    std::vector<int> codes;
    auto add = [&codes](int c) {
        if (c == KeyCode::Unknown) return;
        for (int x : codes) if (x == c) return;
        codes.push_back(c);
    };
    add(map.get(player, a).primary);
    if (singlePlayer && player == 1) add(map.get(2, a).primary);
    add(map.get(player, a).secondary);

    std::string s;
    if (!codes.empty()) {
        s = "[" + keyLabelLocalized(codes[0]);
        if (codes.size() > 1 && maxKeys > 1) s += "/" + keyLabelLocalized(codes[1]);
        s += "]";
    }
    if (withPad && padFor(player) >= 0) {
        std::string pl = padLabel(a);
        if (!pl.empty()) s += (s.empty() ? "" : " ") + std::string("(") + pl + ")";
    }
    return s.empty() ? std::string("[-]") : s;
}

std::string InputRouter::moveHint(int player) const {
    const InputMap& map = gameSettings().keys;
    std::string s = "[";
    const InputAction dirs[4] = { InputAction::MoveUp, InputAction::MoveLeft, InputAction::MoveDown, InputAction::MoveRight };
    for (int i = 0; i < 4; ++i) {
        int c = map.get(player, dirs[i]).primary;
        s += (i ? "/" : "") + (c == KeyCode::Unknown ? std::string("-") : keyLabelLocalized(c));
    }
    s += "]";
    if (padFor(player) >= 0) s += " (стик)";
    return s;
}

// -----------------------------------------------------------------------------
// GamepadMenuBridge
// -----------------------------------------------------------------------------
void GamepadMenuBridge::reset() {
    for (PadState& p : pads) p = PadState();
}

void GamepadMenuBridge::update(float dt, Mode mode, std::vector<sf::Event>& keys, std::vector<Press>& presses) {
    if (!gameSettings().gamepadEnabled) {
        reset();
        return;
    }
    using K = sf::Keyboard::Key;
    using S = sf::Keyboard::Scan;
    for (unsigned int j = 0; j < sf::Joystick::Count && j < 8; ++j) {
        PadState& st = pads[j];
        if (!sf::Joystick::isConnected(j) || sf::Joystick::getButtonCount(j) < 4) {
            st = PadState();
            continue;
        }
        unsigned int now = 0;
        unsigned int count = std::min(sf::Joystick::getButtonCount(j), 32u);
        for (unsigned int b = 0; b < count; ++b)
            if (sf::Joystick::isButtonPressed(j, b)) now |= (1u << b);
        unsigned int pressed = now & ~st.buttons;
        st.buttons = now;

        auto was = [pressed](unsigned int b) { return (pressed & (1u << b)) != 0; };
        if (mode == Mode::Game) {
            if (was(GamepadButton::Start)) emitKey(keys, K::Escape, S::Escape);
            if (was(GamepadButton::Back)) emitKey(keys, K::F1, S::F1);
            for (unsigned int b = 0; b < count; ++b)
                if (was(b) && b != GamepadButton::Start && b != GamepadButton::Back) presses.push_back(Press{ j, b });
            st.dir = dominantDir(padStick(j)); // no menu repeat while playing
            st.repeat = MENU_REPEAT_DELAY;
            continue;
        }

        if (was(GamepadButton::A)) emitKey(keys, K::Enter, S::Enter);
        if (was(GamepadButton::B)) emitKey(keys, K::Escape, S::Escape);
        if (was(GamepadButton::X)) emitKey(keys, K::Backspace, S::Backspace);
        if (was(GamepadButton::LB)) emitKey(keys, K::PageUp, S::PageUp);
        if (was(GamepadButton::RB)) emitKey(keys, K::PageDown, S::PageDown);
        if (was(GamepadButton::Start)) {
            if (mode == Mode::MainMenu) emitKey(keys, K::Enter, S::Enter);
            else emitKey(keys, K::Escape, S::Escape);
        }
        if (was(GamepadButton::Back) && mode == Mode::MatchMenu) emitKey(keys, K::F1, S::F1);

        int dir = dominantDir(padStick(j));
        bool fire = false;
        if (dir != st.dir) {
            st.dir = dir;
            st.repeat = MENU_REPEAT_DELAY;
            fire = (dir >= 0);
        } else if (dir >= 0) {
            st.repeat -= dt;
            if (st.repeat <= 0.0f) {
                st.repeat = MENU_REPEAT_RATE;
                fire = true;
            }
        }
        if (fire) {
            static const K codes[4] = { K::Up, K::Down, K::Left, K::Right };
            static const S scans[4] = { S::Up, S::Down, S::Left, S::Right };
            emitKey(keys, codes[dir], scans[dir]);
        }
    }
}
