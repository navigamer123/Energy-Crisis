// =============================================================================
// Team b-session (F-10): SFML-free key binding table (see UI_inputmap.h)
// =============================================================================
#include "../includes/UI_inputmap.h"
#include <cctype>
#include <cstdio>

namespace {

struct KeyName {
    int code;
    const char* id;     // settings-file id (ASCII)
    const char* label;  // on-screen label (UTF-8)
};

// Every key that can be bound. Letters and digits are generated below.
const KeyName kNamedKeys[] = {
    { KeyCode::Enter, "Enter", "ENTER" },
    { KeyCode::Escape, "Escape", "ESC" },
    { KeyCode::Backspace, "Backspace", "BACKSPACE" },
    { KeyCode::Tab, "Tab", "TAB" },
    { KeyCode::Space, "Space", "SPACE" },
    { KeyCode::Hyphen, "Hyphen", "-" },
    { KeyCode::Equal, "Equal", "=" },
    { KeyCode::LBracket, "LBracket", "[" },
    { KeyCode::RBracket, "RBracket", "]" },
    { KeyCode::Backslash, "Backslash", "\\" },
    { KeyCode::Semicolon, "Semicolon", ";" },
    { KeyCode::Apostrophe, "Apostrophe", "'" },
    { KeyCode::Grave, "Grave", "`" },
    { KeyCode::Comma, "Comma", "," },
    { KeyCode::Period, "Period", "." },
    { KeyCode::Slash, "Slash", "/" },
    { KeyCode::CapsLock, "CapsLock", "CAPS" },
    { KeyCode::Insert, "Insert", "Ins" },
    { KeyCode::Home, "Home", "Home" },
    { KeyCode::PageUp, "PageUp", "PgUp" },
    { KeyCode::Delete, "Delete", "Del" },
    { KeyCode::End, "End", "End" },
    { KeyCode::PageDown, "PageDown", "PgDn" },
    { KeyCode::Right, "Right", "\xE2\x86\x92" }, // →
    { KeyCode::Left, "Left", "\xE2\x86\x90" },   // ←
    { KeyCode::Down, "Down", "\xE2\x86\x93" },   // ↓
    { KeyCode::Up, "Up", "\xE2\x86\x91" },       // ↑
    { KeyCode::NumpadDivide, "NumpadDivide", "Num /" },
    { KeyCode::NumpadMultiply, "NumpadMultiply", "Num *" },
    { KeyCode::NumpadMinus, "NumpadMinus", "Num -" },
    { KeyCode::NumpadPlus, "NumpadPlus", "Num +" },
    { KeyCode::NumpadEnter, "NumpadEnter", "Num ENTER" },
    { KeyCode::NumpadDecimal, "NumpadDecimal", "Num ." },
    { KeyCode::LControl, "LControl", "LCtrl" },
    { KeyCode::LShift, "LShift", "LShift" },
    { KeyCode::LAlt, "LAlt", "LAlt" },
    { KeyCode::RControl, "RControl", "RCtrl" },
    { KeyCode::RShift, "RShift", "RShift" },
    { KeyCode::RAlt, "RAlt", "RAlt" },
};

// Lazily built lookup tables for ids / labels of all codes 0..KeyCode::Max-1
struct KeyTables {
    std::string id[KeyCode::Max];
    std::string label[KeyCode::Max];
    KeyTables() {
        for (int c = KeyCode::A; c <= KeyCode::Z; ++c) {
            id[c] = std::string(1, static_cast<char>('A' + (c - KeyCode::A)));
            label[c] = id[c];
        }
        for (int d = 0; d < 10; ++d) {
            // Num1..Num9 then Num0
            int code = (d == 0) ? KeyCode::Num0 : KeyCode::Num1 + (d - 1);
            id[code] = "Num" + std::to_string(d);
            label[code] = std::to_string(d);
            int pad = (d == 0) ? KeyCode::Numpad0 : KeyCode::Numpad1 + (d - 1);
            id[pad] = "Numpad" + std::to_string(d);
            label[pad] = "Num " + std::to_string(d);
        }
        for (int f = 1; f <= 12; ++f) {
            int code = KeyCode::F1 + (f - 1);
            id[code] = "F" + std::to_string(f);
            label[code] = id[code];
        }
        for (const KeyName& k : kNamedKeys) {
            id[k.code] = k.id;
            label[k.code] = k.label;
        }
    }
};

const KeyTables& tables() {
    static const KeyTables t;
    return t;
}

bool validCode(int code) { return code >= 0 && code < KeyCode::Max && !tables().id[code].empty(); }

bool validPlayer(int player) { return player == 1 || player == 2; }

int clampAction(InputAction a) {
    int i = static_cast<int>(a);
    return (i < 0 || i >= INPUT_ACTION_COUNT) ? 0 : i;
}

std::string lowerAscii(std::string s) {
    for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

} // namespace

// -----------------------------------------------------------------------------
// Key names
// -----------------------------------------------------------------------------
const char* keyCodeId(int code) {
    return validCode(code) ? tables().id[code].c_str() : "";
}

int keyCodeFromId(const std::string& rawId) {
    std::string id = lowerAscii(trim(rawId));
    if (id.empty()) return KeyCode::Unknown;
    const KeyTables& t = tables();
    for (int c = 0; c < KeyCode::Max; ++c) {
        if (!t.id[c].empty() && lowerAscii(t.id[c]) == id) return c;
    }
    return KeyCode::Unknown;
}

std::string keyCodeLabel(int code) {
    return validCode(code) ? tables().label[code] : std::string("?");
}

bool isReservedKey(int code) {
    switch (code) {
        case KeyCode::Escape: // pause / back
        case KeyCode::F1:     // help
        case KeyCode::H:      // help
        case KeyCode::M:      // pause menu
        case KeyCode::F5:     // quicksave
        case KeyCode::F9:     // quickload
        case KeyCode::F11:    // fullscreen
            return true;
        default:
            return false;
    }
}

// -----------------------------------------------------------------------------
// InputMap
// -----------------------------------------------------------------------------
InputMap::InputMap() { *this = defaults(); }

InputMap InputMap::defaults() {
    InputMap m{ BlankTag{} }; // not InputMap m; (the default constructor calls defaults())

    auto put = [&m](int player, InputAction a, int primary, int secondary) {
        m.bind[player - 1][static_cast<int>(a)].primary = primary;
        m.bind[player - 1][static_cast<int>(a)].secondary = secondary;
    };
    using namespace KeyCode;
    // Player 1 (west): WASD + Space / F / E / Q / X and the number row
    put(1, InputAction::MoveUp, W, Unknown);
    put(1, InputAction::MoveDown, S, Unknown);
    put(1, InputAction::MoveLeft, A, Unknown);
    put(1, InputAction::MoveRight, D, Unknown);
    put(1, InputAction::Action, Space, Unknown);
    put(1, InputAction::Upgrade, F, Unknown);
    put(1, InputAction::NextBuilding, E, Unknown);
    put(1, InputAction::PrevBuilding, Q, Unknown);
    put(1, InputAction::Cancel, X, Unknown);
    put(1, InputAction::Quick1, Num1, Unknown);
    put(1, InputAction::Quick2, Num2, Unknown);
    put(1, InputAction::Quick3, Num3, Unknown);
    put(1, InputAction::Quick4, Num4, Unknown);
    put(1, InputAction::Quick5, Num5, Unknown);
    put(1, InputAction::Quick6, Num6, Unknown);
    // Player 2 (east): arrows + Enter / RShift / PgDn / PgUp / Del and the numpad
    put(2, InputAction::MoveUp, Up, Unknown);
    put(2, InputAction::MoveDown, Down, Unknown);
    put(2, InputAction::MoveLeft, Left, Unknown);
    put(2, InputAction::MoveRight, Right, Unknown);
    put(2, InputAction::Action, Enter, NumpadEnter);
    put(2, InputAction::Upgrade, RShift, End);
    put(2, InputAction::NextBuilding, PageDown, Unknown);
    put(2, InputAction::PrevBuilding, PageUp, Unknown);
    put(2, InputAction::Cancel, Delete, Backspace);
    put(2, InputAction::Quick1, Numpad1, Unknown);
    put(2, InputAction::Quick2, Numpad2, Unknown);
    put(2, InputAction::Quick3, Numpad3, Unknown);
    put(2, InputAction::Quick4, Numpad4, Unknown);
    put(2, InputAction::Quick5, Numpad5, Unknown);
    put(2, InputAction::Quick6, Numpad6, Unknown);
    return m;
}

const KeyBinding& InputMap::get(int player, InputAction a) const {
    return bind[validPlayer(player) ? player - 1 : 0][clampAction(a)];
}

int InputMap::key(int player, InputAction a, int slot) const {
    const KeyBinding& b = get(player, a);
    return (slot == 1) ? b.secondary : b.primary;
}

void InputMap::set(int player, InputAction a, int slot, int code) {
    if (!validPlayer(player)) return;
    if (code != KeyCode::Unknown && !validCode(code)) code = KeyCode::Unknown;
    KeyBinding& b = bind[player - 1][clampAction(a)];
    if (slot == 1) b.secondary = code;
    else b.primary = code;
}

bool InputMap::isBound(int code, int player, InputAction a) const {
    if (code == KeyCode::Unknown) return false;
    const KeyBinding& b = get(player, a);
    return b.primary == code || b.secondary == code;
}

std::vector<InputMap::Cell> InputMap::conflicts() const {
    int uses[KeyCode::Max] = {};
    for (int p = 0; p < 2; ++p)
        for (int a = 0; a < INPUT_ACTION_COUNT; ++a) {
            const KeyBinding& b = bind[p][a];
            if (validCode(b.primary)) uses[b.primary]++;
            if (validCode(b.secondary)) uses[b.secondary]++;
        }

    std::vector<Cell> out;
    for (int p = 0; p < 2; ++p)
        for (int a = 0; a < INPUT_ACTION_COUNT; ++a)
            for (int slot = 0; slot < 2; ++slot) {
                int code = (slot == 0) ? bind[p][a].primary : bind[p][a].secondary;
                if (!validCode(code)) continue;
                if (uses[code] > 1 || isReservedKey(code)) out.push_back(Cell{ p + 1, a, slot });
            }
    return out;
}

bool InputMap::isConflicted(int player, InputAction a, int slot) const {
    for (const Cell& c : conflicts())
        if (c.player == player && c.action == static_cast<int>(a) && c.slot == slot) return true;
    return false;
}

std::string InputMap::encode(int player, InputAction a) const {
    const KeyBinding& b = get(player, a);
    std::string s = keyCodeId(b.primary);
    if (validCode(b.secondary)) s += std::string(",") + keyCodeId(b.secondary);
    return s;
}

bool InputMap::decode(int player, InputAction a, const std::string& value) {
    if (!validPlayer(player)) return false;
    std::string v = trim(value);
    std::string first = v, second;
    size_t comma = v.find(',');
    if (comma != std::string::npos) {
        first = v.substr(0, comma);
        second = v.substr(comma + 1);
    }
    int p = keyCodeFromId(first);
    int s = keyCodeFromId(second);
    // A non-empty name that is not a key makes the whole value invalid (keep the old binding)
    if ((!trim(first).empty() && p == KeyCode::Unknown) || (!trim(second).empty() && s == KeyCode::Unknown)) return false;
    if (p == KeyCode::Unknown && s != KeyCode::Unknown) { p = s; s = KeyCode::Unknown; }
    set(player, a, 0, p);
    set(player, a, 1, s);
    return true;
}

const char* InputMap::actionId(InputAction a) {
    switch (a) {
        case InputAction::MoveUp: return "move_up";
        case InputAction::MoveDown: return "move_down";
        case InputAction::MoveLeft: return "move_left";
        case InputAction::MoveRight: return "move_right";
        case InputAction::Action: return "action";
        case InputAction::Upgrade: return "upgrade";
        case InputAction::NextBuilding: return "next_building";
        case InputAction::PrevBuilding: return "prev_building";
        case InputAction::Cancel: return "cancel";
        case InputAction::Quick1: return "quick_solar";
        case InputAction::Quick2: return "quick_wind";
        case InputAction::Quick3: return "quick_hydro";
        case InputAction::Quick4: return "quick_battery";
        case InputAction::Quick5: return "quick_lamp";
        case InputAction::Quick6: return "quick_demolish";
        default: return "unknown";
    }
}

const char* InputMap::actionLabelBg(InputAction a) {
    switch (a) {
        case InputAction::MoveUp: return "Движение нагоре";
        case InputAction::MoveDown: return "Движение надолу";
        case InputAction::MoveLeft: return "Движение наляво";
        case InputAction::MoveRight: return "Движение надясно";
        case InputAction::Action: return "Действие (строеж, добив)";
        case InputAction::Upgrade: return "Надграждане на мина";
        case InputAction::NextBuilding: return "Следваща сграда";
        case InputAction::PrevBuilding: return "Предишна сграда";
        case InputAction::Cancel: return "Отказ / разрушаване";
        case InputAction::Quick1: return "Бърз избор: соларен панел";
        case InputAction::Quick2: return "Бърз избор: вятърна турбина";
        case InputAction::Quick3: return "Бърз избор: ВЕЦ";
        case InputAction::Quick4: return "Бърз избор: батерия";
        case InputAction::Quick5: return "Бърз избор: лампа";
        case InputAction::Quick6: return "Бърз избор: разрушаване";
        default: return "?";
    }
}

bool InputMap::operator==(const InputMap& o) const {
    for (int p = 0; p < 2; ++p)
        for (int a = 0; a < INPUT_ACTION_COUNT; ++a)
            if (bind[p][a].primary != o.bind[p][a].primary || bind[p][a].secondary != o.bind[p][a].secondary) return false;
    return true;
}

// -----------------------------------------------------------------------------
// Hints
// -----------------------------------------------------------------------------
std::string bindingHint(const InputMap& map, int player, InputAction a, int mergeWith) {
    std::vector<int> codes;
    auto add = [&codes](int c) {
        if (c == KeyCode::Unknown) return;
        for (int x : codes) if (x == c) return;
        codes.push_back(c);
    };
    const KeyBinding& own = map.get(player, a);
    add(own.primary);
    if (mergeWith > 0 && mergeWith != player) add(map.get(mergeWith, a).primary);
    add(own.secondary);
    if (codes.empty()) return std::string();
    // Keep hints short: at most two keys
    std::string s = "[" + keyCodeLabel(codes[0]);
    if (codes.size() > 1) s += "/" + keyCodeLabel(codes[1]);
    s += "]";
    return s;
}
