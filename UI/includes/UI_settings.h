#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

// =============================================================================
// Team b-session (F-06, F-10, UX-12, HX-15): persistent player settings.
//
// One process-wide GameSettings instance (gameSettings()) is loaded by UI_main at
// start-up from <user data dir>/settings.ini (key=value, UTF-8) and saved whenever
// a settings screen closes. Every change bumps `revision`, so UI_main re-applies the
// window settings (size, fullscreen, frame cap, vsync) exactly once per change.
//
// This header and UI_settings.cpp are SFML-free (headless tests parse/serialize);
// the file system part lives in UI_paths.cpp.
// =============================================================================

#include <string>
#include "UI_inputmap.h"

enum class TutorialPolicy : int {
    Auto = 0,   // first matches of each mode until the tutorial was finished or skipped once (never on Hard)
    Always = 1, // every match, every difficulty
    Never = 2
};

struct GameSettings {
    // --- Window / display ---
    int windowWidth = 1600;
    int windowHeight = 900;
    bool fullscreen = false;
    int fpsLimit = 60;          // 0 = unlimited
    bool vsync = false;
    int uiScalePercent = 100;   // 90 / 100 / 125 / 150: menus, pause, help, dialogs
    bool projectorMode = false; // larger overlays, thicker cursors, high-contrast picture

    // --- Audio (0..100). Read by the audio module through effective*Volume(). ---
    int masterVolume = 80;
    int musicVolume = 70;
    int sfxVolume = 80;

    // --- Gameplay ---
    TutorialPolicy tutorialPolicy = TutorialPolicy::Auto;
    int defaultBotDifficulty = 2; // BotDifficulty as int: 1 easy, 2 medium, 3 hard
    float popupSeconds = 4.0f;    // how long the side popups stay (2..8 s)
    bool autosave = true;         // autosave at every day end and when leaving a match
    bool tutorialDoneSolo = false;
    bool tutorialDoneCoop = false;

    // --- Input ---
    bool gamepadEnabled = true;
    int singlePadOwner = 2;       // 2-player match with ONE gamepad: the player it drives (1 or 2)
    InputMap keys;

    // Bumped by touch() on every change; never saved
    int revision = 0;
    void touch() { ++revision; }

    // Brings every value into its allowed range / allowed steps
    void clampAll();

    // key=value text (one setting per line, '#' comments); parse() ignores unknown keys and
    // keeps the current value for anything malformed. Returns the number of keys applied.
    std::string serialize() const;
    int parse(const std::string& text);

    // Effective scale for overlays/menus: projector mode is never below 125 %
    float overlayScale() const;
    // Multiplier for typography tokens (integrator hook for the shared text helper)
    float textScale() const { return overlayScale(); }

    float effectiveMusicVolume() const { return masterVolume * musicVolume / 100.0f; } // 0..100
    float effectiveSfxVolume() const { return masterVolume * sfxVolume / 100.0f; }     // 0..100
};

// Allowed steps used by the settings screen (and clampAll)
namespace SettingsSteps {
constexpr int RESOLUTION_COUNT = 5;
constexpr int RESOLUTIONS[RESOLUTION_COUNT][2] = { { 1280, 720 }, { 1366, 768 }, { 1600, 900 }, { 1920, 1080 }, { 2560, 1440 } };
constexpr int FPS_COUNT = 5;
constexpr int FPS_VALUES[FPS_COUNT] = { 30, 60, 120, 144, 0 };
constexpr int UI_SCALE_COUNT = 4;
constexpr int UI_SCALES[UI_SCALE_COUNT] = { 90, 100, 125, 150 };
} // namespace SettingsSteps

// The process-wide settings (defaults until loadGameSettings() runs)
GameSettings& gameSettings();

// File system (UI_paths.cpp). The user data dir is %APPDATA%/EnergyCrisis on Windows,
// $XDG_CONFIG_HOME or ~/.config/EnergyCrisis elsewhere; EC_USERDIR overrides it (tests).
std::string userDataDir();     // created on first use; "" when nothing is writable
void disableUserData();       // screenshot mode: no settings or saves are read or written
std::string settingsFilePath();
bool loadGameSettings();       // false when the file is missing or unreadable (defaults stay)
bool saveGameSettings();

#endif // UI_SETTINGS_H
