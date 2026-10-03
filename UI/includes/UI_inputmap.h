#ifndef UI_INPUTMAP_H
#define UI_INPUTMAP_H

// =============================================================================
// Team b-session (F-10): key bindings for both players, SFML-free so the headless
// tests can check defaults, conflicts and the settings-file encoding.
//
// Keys are stored as physical scancodes (layout independent: the "W" position is
// the same key on QWERTY and AZERTY). The numeric values below are identical to
// sf::Keyboard::Scancode; UI_input.cpp static_asserts every one of them.
// =============================================================================

#include <string>
#include <vector>

namespace KeyCode {
enum : int {
    Unknown = -1,
    A = 0, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z, // 0..25
    Num1 = 26, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9, Num0,                  // 26..35
    Enter = 36, Escape = 37, Backspace = 38, Tab = 39, Space = 40,
    Hyphen = 41, Equal = 42, LBracket = 43, RBracket = 44, Backslash = 45,
    Semicolon = 46, Apostrophe = 47, Grave = 48, Comma = 49, Period = 50, Slash = 51,
    F1 = 52, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,                           // 52..63
    CapsLock = 76,
    Insert = 80, Home = 81, PageUp = 82, Delete = 83, End = 84, PageDown = 85,
    Right = 86, Left = 87, Down = 88, Up = 89,
    NumpadDivide = 91, NumpadMultiply = 92, NumpadMinus = 93, NumpadPlus = 94,
    NumpadEnter = 96, NumpadDecimal = 97,
    Numpad1 = 98, Numpad2, Numpad3, Numpad4, Numpad5, Numpad6, Numpad7, Numpad8, Numpad9, // 98..106
    Numpad0 = 107,
    LControl = 127, LShift = 128, LAlt = 129, RControl = 131, RShift = 132, RAlt = 133,
    Max = 160 // exclusive upper bound for table lookups
};
} // namespace KeyCode

// One entry per thing a player can do with the keyboard (and a gamepad, see UI_input.h)
enum class InputAction : int {
    MoveUp = 0,
    MoveDown,
    MoveLeft,
    MoveRight,
    Action,        // build / mine / buy land / confirm a dialog
    Upgrade,       // upgrade the mine under the cursor
    NextBuilding,  // cycle the building selection forward
    PrevBuilding,  // cycle backward
    Cancel,        // cancel the selection / demolish mode / close a dialog
    Quick1,        // direct building hotkeys: solar, wind, hydro, battery, lamp, demolish
    Quick2,
    Quick3,
    Quick4,
    Quick5,
    Quick6,
    Count
};

constexpr int INPUT_ACTION_COUNT = static_cast<int>(InputAction::Count);

struct KeyBinding {
    int primary = KeyCode::Unknown;
    int secondary = KeyCode::Unknown;
};

class InputMap {
public:
    struct Cell {
        int player;   // 1 or 2
        int action;   // InputAction as int
        int slot;     // 0 = primary, 1 = secondary
    };

    InputMap(); // starts with defaults()
    static InputMap defaults();

    const KeyBinding& get(int player, InputAction a) const;
    int key(int player, InputAction a, int slot) const;
    void set(int player, InputAction a, int slot, int code);
    void clear(int player, InputAction a, int slot) { set(player, a, slot, KeyCode::Unknown); }

    // True when `code` is one of the keys bound to (player, action)
    bool isBound(int code, int player, InputAction a) const;

    // Every cell whose key is bound more than once (across BOTH players: Single Player merges the
    // two key sets) or is reserved for a system shortcut. Empty cells never conflict.
    std::vector<Cell> conflicts() const;
    bool isConflicted(int player, InputAction a, int slot) const;
    bool hasConflicts() const { return !conflicts().empty(); }

    // Settings file encoding: "W" or "PageDown,End" (empty = unbound)
    std::string encode(int player, InputAction a) const;
    bool decode(int player, InputAction a, const std::string& value);

    static const char* actionId(InputAction a);      // stable file key, e.g. "move_up"
    static const char* actionLabelBg(InputAction a);  // Bulgarian on-screen label

    bool operator==(const InputMap& o) const;
    bool operator!=(const InputMap& o) const { return !(*this == o); }

private:
    struct BlankTag {};
    explicit InputMap(BlankTag) {} // every key unbound (used by defaults())
    KeyBinding bind[2][INPUT_ACTION_COUNT];
};

// Stable key id used in the settings file ("W", "Space", "PageDown"); "" for unknown codes
const char* keyCodeId(int code);
// Inverse of keyCodeId (case-insensitive); KeyCode::Unknown when not found
int keyCodeFromId(const std::string& id);
// Short on-screen label: "W", "SPACE", "ENTER", "PgDn", "↑", "Num 1" (UTF-8)
std::string keyCodeLabel(int code);
// Keys owned by system shortcuts (pause, help, menu, quicksave/quickload, fullscreen)
bool isReservedKey(int code);

// "[E]" / "[E/PgDn]" for one action. mergeWith > 0 also lists that player's keys
// (Single Player: the human uses both key sets). Empty string when nothing is bound.
std::string bindingHint(const InputMap& map, int player, InputAction a, int mergeWith = 0);

#endif // UI_INPUTMAP_H
