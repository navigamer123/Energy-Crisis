// =============================================================================
// Team b-session (F-06): GameSettings value handling and key=value text format.
// SFML-free; the file system part is in UI_paths.cpp.
// =============================================================================
#include "../includes/UI_settings.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>

GameSettings& gameSettings() {
    static GameSettings instance;
    return instance;
}

namespace {

std::string trimValue(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

bool parseInt(const std::string& v, int& out) {
    std::string t = trimValue(v);
    if (t.empty()) return false;
    char* end = nullptr;
    long value = std::strtol(t.c_str(), &end, 10);
    if (end == t.c_str() || *end != '\0') return false;
    out = static_cast<int>(value);
    return true;
}

bool parseFloat(const std::string& v, float& out) {
    std::string t = trimValue(v);
    if (t.empty()) return false;
    char* end = nullptr;
    double value = std::strtod(t.c_str(), &end);
    if (end == t.c_str() || *end != '\0') return false;
    out = static_cast<float>(value);
    return true;
}

bool parseBool(const std::string& v, bool& out) {
    std::string t = trimValue(v);
    for (char& ch : t) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (t == "1" || t == "true" || t == "yes" || t == "on") { out = true; return true; }
    if (t == "0" || t == "false" || t == "no" || t == "off") { out = false; return true; }
    return false;
}

template <size_t N>
int nearestStep(int value, const int (&steps)[N]) {
    int best = steps[0];
    for (size_t i = 0; i < N; ++i)
        if (std::abs(steps[i] - value) < std::abs(best - value)) best = steps[i];
    return best;
}

const char* policyId(TutorialPolicy p) {
    switch (p) {
        case TutorialPolicy::Always: return "always";
        case TutorialPolicy::Never: return "never";
        default: return "auto";
    }
}

} // namespace

void GameSettings::clampAll() {
    windowWidth = std::max(800, std::min(windowWidth, 7680));
    windowHeight = std::max(450, std::min(windowHeight, 4320));
    if (fpsLimit != 0) fpsLimit = std::max(15, std::min(fpsLimit, 360));
    uiScalePercent = nearestStep(uiScalePercent, SettingsSteps::UI_SCALES);
    masterVolume = std::max(0, std::min(masterVolume, 100));
    musicVolume = std::max(0, std::min(musicVolume, 100));
    sfxVolume = std::max(0, std::min(sfxVolume, 100));
    int tp = static_cast<int>(tutorialPolicy);
    if (tp < 0 || tp > 2) tutorialPolicy = TutorialPolicy::Auto;
    defaultBotDifficulty = std::max(1, std::min(defaultBotDifficulty, 3));
    popupSeconds = std::max(2.0f, std::min(popupSeconds, 8.0f));
    singlePadOwner = (singlePadOwner == 1) ? 1 : 2;
}

float GameSettings::overlayScale() const {
    float s = uiScalePercent / 100.0f;
    if (projectorMode) s = std::max(s, 1.25f);
    return s;
}

std::string GameSettings::serialize() const {
    std::ostringstream o;
    o << "# Energy Crisis settings (key=value). Delete this file to restore the defaults.\n";
    o << "version=1\n";
    o << "window_width=" << windowWidth << "\n";
    o << "window_height=" << windowHeight << "\n";
    o << "fullscreen=" << (fullscreen ? 1 : 0) << "\n";
    o << "fps_limit=" << fpsLimit << "\n";
    o << "vsync=" << (vsync ? 1 : 0) << "\n";
    o << "ui_scale=" << uiScalePercent << "\n";
    o << "projector_mode=" << (projectorMode ? 1 : 0) << "\n";
    o << "master_volume=" << masterVolume << "\n";
    o << "music_volume=" << musicVolume << "\n";
    o << "sfx_volume=" << sfxVolume << "\n";
    o << "tutorial_policy=" << policyId(tutorialPolicy) << "\n";
    o << "default_bot_difficulty=" << defaultBotDifficulty << "\n";
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", popupSeconds);
    o << "popup_seconds=" << buf << "\n";
    o << "autosave=" << (autosave ? 1 : 0) << "\n";
    o << "tutorial_done_solo=" << (tutorialDoneSolo ? 1 : 0) << "\n";
    o << "tutorial_done_coop=" << (tutorialDoneCoop ? 1 : 0) << "\n";
    o << "gamepad_enabled=" << (gamepadEnabled ? 1 : 0) << "\n";
    o << "single_pad_owner=" << singlePadOwner << "\n";
    for (int p = 1; p <= 2; ++p)
        for (int a = 0; a < INPUT_ACTION_COUNT; ++a) {
            InputAction act = static_cast<InputAction>(a);
            o << "key.p" << p << "." << InputMap::actionId(act) << "=" << keys.encode(p, act) << "\n";
        }
    return o.str();
}

int GameSettings::parse(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    int applied = 0;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string t = trimValue(line);
        if (t.empty() || t[0] == '#' || t[0] == ';') continue;
        size_t eq = t.find('=');
        if (eq == std::string::npos) continue;
        std::string k = trimValue(t.substr(0, eq));
        std::string v = trimValue(t.substr(eq + 1));
        bool ok = false;
        int i = 0;
        float f = 0.0f;
        bool b = false;

        if (k == "window_width") { if ((ok = parseInt(v, i))) windowWidth = i; }
        else if (k == "window_height") { if ((ok = parseInt(v, i))) windowHeight = i; }
        else if (k == "fullscreen") { if ((ok = parseBool(v, b))) fullscreen = b; }
        else if (k == "fps_limit") { if ((ok = parseInt(v, i))) fpsLimit = std::max(0, i); }
        else if (k == "vsync") { if ((ok = parseBool(v, b))) vsync = b; }
        else if (k == "ui_scale") { if ((ok = parseInt(v, i))) uiScalePercent = i; }
        else if (k == "projector_mode") { if ((ok = parseBool(v, b))) projectorMode = b; }
        else if (k == "master_volume") { if ((ok = parseInt(v, i))) masterVolume = i; }
        else if (k == "music_volume") { if ((ok = parseInt(v, i))) musicVolume = i; }
        else if (k == "sfx_volume") { if ((ok = parseInt(v, i))) sfxVolume = i; }
        else if (k == "tutorial_policy") {
            ok = true;
            if (v == "auto") tutorialPolicy = TutorialPolicy::Auto;
            else if (v == "always") tutorialPolicy = TutorialPolicy::Always;
            else if (v == "never") tutorialPolicy = TutorialPolicy::Never;
            else ok = false;
        }
        else if (k == "default_bot_difficulty") { if ((ok = parseInt(v, i))) defaultBotDifficulty = i; }
        else if (k == "popup_seconds") { if ((ok = parseFloat(v, f))) popupSeconds = f; }
        else if (k == "autosave") { if ((ok = parseBool(v, b))) autosave = b; }
        else if (k == "tutorial_done_solo") { if ((ok = parseBool(v, b))) tutorialDoneSolo = b; }
        else if (k == "tutorial_done_coop") { if ((ok = parseBool(v, b))) tutorialDoneCoop = b; }
        else if (k == "gamepad_enabled") { if ((ok = parseBool(v, b))) gamepadEnabled = b; }
        else if (k == "single_pad_owner") { if ((ok = parseInt(v, i))) singlePadOwner = i; }
        else if (k.compare(0, 6, "key.p1") == 0 || k.compare(0, 6, "key.p2") == 0) {
            int player = (k[5] == '1') ? 1 : 2;
            std::string id = (k.size() > 7) ? k.substr(7) : std::string();
            for (int a = 0; a < INPUT_ACTION_COUNT; ++a) {
                InputAction act = static_cast<InputAction>(a);
                if (id == InputMap::actionId(act)) {
                    ok = keys.decode(player, act, v);
                    break;
                }
            }
        }
        if (ok) ++applied;
    }
    clampAll();
    // A hand-edited file with clashing keys would make a player's key do two things: fall back
    if (keys.hasConflicts()) keys = InputMap::defaults();
    touch();
    return applied;
}
