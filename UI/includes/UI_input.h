#ifndef UI_INPUT_H
#define UI_INPUT_H

// =============================================================================
// Team b-session (F-05, F-10): one input layer for keyboard and gamepads.
//
// Every frame UI_map asks inputRouter().poll(player, ...) for a PlayerIntent: the
// held state of each InputAction from that player's rebindable keys (InputMap in
// gameSettings().keys) merged with that player's gamepad (sf::Joystick). The map
// then runs the same code for both sources, so a pad plays exactly like the keys.
//
// GamepadMenuBridge turns pad presses into ordinary key events for menus, the pause
// menu and dialogs (A = Enter, B = Esc, D-pad/stick = arrows with auto-repeat), so
// every screen is usable from the sofa without per-screen gamepad code.
//
// Default pad layout (XInput order, as SFML reports Xbox-style pads on Windows):
//   stick / D-pad move   A action   B cancel   X upgrade   LB/RB building -/+
//   Y quick-select solar   Start pause   Back help
// =============================================================================

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include "UI_inputmap.h"
#include "UI_types.h"

namespace GamepadButton {
enum : unsigned int { A = 0, B = 1, X = 2, Y = 3, LB = 4, RB = 5, Back = 6, Start = 7 };
}

struct PlayerIntent {
    sf::Vector2f move{ 0.0f, 0.0f };   // analog movement, -1..1 per axis (keys give -1/0/1)
    bool up = false, down = false, left = false, right = false; // digital directions (grid stepping)
    bool held[INPUT_ACTION_COUNT] = {}; // held state of every action (keys OR pad)
    bool padActive = false;              // a gamepad drives this player

    bool is(InputAction a) const { return held[static_cast<int>(a)]; }
};

class InputRouter {
public:
    // Match context (UI_map keeps it current): Single Player lets the human use both key sets
    void setMatchContext(bool singlePlayer, ControlScheme scheme);
    bool isSinglePlayer() const { return singlePlayer; }
    ControlScheme scheme() const { return controlScheme; }

    // Joystick id driving `player` (1/2), or -1. Uses connected pads and the settings:
    // Single Player -> first pad = P1; two pads -> P1/P2; one pad in a 2-player match ->
    // gameSettings().singlePadOwner.
    int padFor(int player) const;
    int playerForPad(unsigned int joystickId) const; // 0 = none
    int lastPadFor(int player) const { return (player == 1 || player == 2) ? cachedPad[player] : -1; }
    void refreshAssignment(); // caches padFor() so a disconnect can still tell whose pad it was
    static std::vector<unsigned int> connectedPads();
    static std::string padName(unsigned int joystickId);

    // Keyboard state of an action for a player in the current context
    bool keyHeld(int player, InputAction a) const;
    bool keyMatches(int player, InputAction a, sf::Keyboard::Scancode sc) const;

    // keyboardMovement = false: the control scheme gives this player's movement to the mouse,
    // so only the pad (if any) may move them. Action keys always count.
    PlayerIntent poll(int player, bool keyboardMovement) const;

    // "[SPACE]", "[SPACE/ENTER]" (Single Player), plus " (A)" when the player has a gamepad
    // maxKeys: how many keys to list (1 = primary only; Single Player P1 lists P1 + P2 primaries first)
    std::string hint(int player, InputAction a, bool withPad = true, int maxKeys = 2) const;
    // "[W/A/S/D]" / "[↑/←/↓/→]" style hint for the four movement keys
    std::string moveHint(int player) const;
    static std::string padLabel(InputAction a); // "A", "LB", ... or "" when the pad has no button for it

private:
    bool singlePlayer = false;
    ControlScheme controlScheme = ControlScheme::BOTH_KEYBOARD;
    int cachedPad[3] = { -1, -1, -1 };
};

InputRouter& inputRouter();

// On-screen label of a physical key on the CURRENT keyboard layout (AZERTY shows "Z" for the
// W position); falls back to keyCodeLabel()
std::string keyLabelLocalized(int keyCode);

class GamepadMenuBridge {
public:
    enum class Mode {
        MainMenu,  // A = Enter, B = Esc, Start = Enter
        MatchMenu, // pause menu, help, settings, dialogs: A = Enter, B = Esc, Start = Esc
        Game       // playing: only Start = Esc (pause) and Back = F1 (help); A/B go to presses
    };
    struct Press {
        unsigned int joystickId;
        unsigned int button;
    };

    // Call once per frame. Appends synthetic key events (press + release pairs) to `keys`
    // and, in Game mode, raw button presses to `presses`.
    void update(float dt, Mode mode, std::vector<sf::Event>& keys, std::vector<Press>& presses);
    void reset();

private:
    struct PadState {
        unsigned int buttons = 0; // bit per button, previous frame
        int dir = -1;             // 0 up, 1 down, 2 left, 3 right, -1 none
        float repeat = 0.0f;
    };
    PadState pads[8];
};

#endif // UI_INPUT_H
