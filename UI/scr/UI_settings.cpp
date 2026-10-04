#include "../includes/UI_settings.h"
#include "../includes/UI_lang.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif

namespace {

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

} // namespace

UI_settings& UI_settings::get() {
    static UI_settings instance;
    return instance;
}

UI_settings::UI_settings() {
    resolvedPath = resolvePath();
}

std::string UI_settings::resolvePath() {
    const char* envPath = std::getenv("EC_SETTINGS");
    if (envPath && envPath[0] != '\0') {
        return std::string(envPath);
    }

    // Default directly to settings.ini in the game working directory
    return "settings.ini";
}

std::string UI_settings::getSettingsPath() const {
    return resolvedPath.empty() ? "settings.ini" : resolvedPath;
}

void UI_settings::load() {
    std::string path = getSettingsPath();
    std::ifstream file(path.c_str());
    if (!file.is_open()) {
#if !defined(_WIN32) && !defined(__ANDROID__)
        const char* home = std::getenv("HOME");
        if (home && home[0] != '\0') {
            std::string userPath = std::string(home) + "/.config/energy-crisis/settings.ini";
            file.open(userPath.c_str());
            if (file.is_open()) {
                path = userPath;
            }
        }
#endif
    }
    if (!file.is_open()) {
        std::cout << "[UI_settings] No settings file found at " << path << "; using defaults.\n";
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        if (key == "language" || key == "ui.language") {
            if (val == "en" || val == "bg") {
                current.language = val;
            }
        } else if (key == "volume" || key == "audio.volume" || key == "audio.master" || key == "master_volume") {
            try {
                current.volume = std::clamp(std::stoi(val), 0, 100);
            } catch (...) {}
        } else if (key == "sound_effects" || key == "audio.sfx" || key == "sfx") {
            current.soundEffects = (val == "true" || val == "1" || val == "yes" || val == "on");
        } else if (key == "bot_difficulty" || key == "difficulty" || key == "bot.difficulty") {
            try {
                current.botDifficultyIndex = std::clamp(std::stoi(val), 0, 2);
            } catch (...) {}
        }
    }
    std::cout << "[UI_settings] Loaded settings: language=" << current.language
              << ", volume=" << current.volume << "%, sfx=" << (current.soundEffects ? "on" : "off")
              << ", botDifficulty=" << current.botDifficultyIndex << " (" << path << ")\n";
}

void UI_settings::save() {
    std::string path = getSettingsPath();
    namespace fs = std::filesystem;
    try {
        fs::path p(path);
        if (p.has_parent_path()) {
            std::error_code ec;
            fs::create_directories(p.parent_path(), ec);
        }
    } catch (...) {}

    std::ofstream file(path.c_str());
    if (!file.is_open()) {
        // If writing to local game directory failed, attempt user config directory
#if !defined(_WIN32) && !defined(__ANDROID__)
        const char* home = std::getenv("HOME");
        if (home && home[0] != '\0') {
            fs::path userDir = fs::path(home) / ".config" / "energy-crisis";
            std::error_code ec;
            fs::create_directories(userDir, ec);
            resolvedPath = (userDir / "settings.ini").string();
            file.open(resolvedPath.c_str());
        }
#endif
        if (!file.is_open()) {
            std::cerr << "[UI_settings] ERROR: Could not save settings to " << path << "\n";
            return;
        }
    }

    file << "# Energy Crisis - Configuration File / Настройки\n\n";
    file << "[ui]\n";
    file << "language = " << current.language << "\n\n";
    file << "[audio]\n";
    file << "master_volume = " << current.volume << "\n";
    file << "sound_effects = " << (current.soundEffects ? "true" : "false") << "\n\n";
    file << "[game]\n";
    file << "bot_difficulty = " << current.botDifficultyIndex << "\n";

    file.close();
    std::cout << "[UI_settings] Settings successfully saved to " << getSettingsPath() << ".\n";
}

void UI_settings::setLanguage(const std::string& lang) {
    if (lang == "en" || lang == "bg") {
        current.language = lang;
        Lang::load(lang);
        save();
    }
}

void UI_settings::setVolume(int vol) {
    current.volume = std::clamp(vol, 0, 100);
    save();
}

void UI_settings::setSoundEffectsEnabled(bool on) {
    current.soundEffects = on;
    save();
}

void UI_settings::setBotDifficultyIndex(int idx) {
    current.botDifficultyIndex = std::clamp(idx, 0, 2);
    save();
}
