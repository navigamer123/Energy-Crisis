// =============================================================================
// Team b-session (F-06, F-18): user data directory, settings file, file helpers.
// =============================================================================
#include "../includes/UI_paths.h"
#include "../includes/UI_settings.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace {

fs::path toPath(const std::string& utf8) {
    // u8path is deprecated in C++20 but is exactly "UTF-8 string -> native path" in C++17
    return fs::u8path(utf8);
}

std::string fromPath(const fs::path& p) {
    return p.u8string();
}

std::string envUtf8(const char* name) {
#ifdef _WIN32
    std::wstring wname(name, name + std::char_traits<char>::length(name));
    const wchar_t* w = _wgetenv(wname.c_str());
    if (w == nullptr || *w == L'\0') return std::string();
    return fs::path(w).u8string();
#else
    const char* v = std::getenv(name);
    return (v != nullptr) ? std::string(v) : std::string();
#endif
}

} // namespace

namespace ecfs {

std::string join(const std::string& dir, const std::string& name) {
    if (dir.empty()) return name;
    return fromPath(toPath(dir) / toPath(name));
}

bool ensureDir(const std::string& dir) {
    if (dir.empty()) return false;
    std::error_code ec;
    fs::path p = toPath(dir);
    if (fs::is_directory(p, ec)) return true;
    ec.clear();
    fs::create_directories(p, ec);
    return !ec && fs::is_directory(p, ec);
}

bool exists(const std::string& path) {
    std::error_code ec;
    return fs::exists(toPath(path), ec);
}

bool readText(const std::string& path, std::string& out) {
    std::ifstream in(toPath(path), std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

bool writeTextAtomic(const std::string& path, const std::string& text) {
    fs::path target = toPath(path);
    fs::path tmp = target;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << text;
        out.flush();
        if (!out) return false;
    }
    std::error_code ec;
    fs::rename(tmp, target, ec);
    if (ec) {
        // Some file systems refuse to rename over an existing file: replace it explicitly
        ec.clear();
        fs::remove(target, ec);
        ec.clear();
        fs::rename(tmp, target, ec);
    }
    return !ec;
}

bool remove(const std::string& path) {
    std::error_code ec;
    return fs::remove(toPath(path), ec);
}

} // namespace ecfs

std::string userDataDir() {
    static std::string cached;
    static bool resolved = false;
    if (resolved) return cached;
    resolved = true;

    std::string candidates[3];
    candidates[0] = envUtf8("EC_USERDIR");
#ifdef _WIN32
    std::string appData = envUtf8("APPDATA");
    if (!appData.empty()) candidates[1] = ecfs::join(appData, "EnergyCrisis");
#else
    std::string xdg = envUtf8("XDG_CONFIG_HOME");
    std::string home = envUtf8("HOME");
    if (!xdg.empty()) candidates[1] = ecfs::join(xdg, "EnergyCrisis");
    else if (!home.empty()) candidates[1] = ecfs::join(ecfs::join(home, ".config"), "EnergyCrisis");
#endif
    candidates[2] = "EnergyCrisisData"; // next to the executable (main.cpp makes it the working dir)

    for (const std::string& c : candidates) {
        if (!c.empty() && ecfs::ensureDir(c)) {
            cached = c;
            std::cout << "[Settings] User data folder: " << cached << "\n";
            return cached;
        }
    }
    std::cerr << "[Settings] Warning: no writable user data folder; settings and saves are not kept.\n";
    return cached;
}

std::string settingsFilePath() {
    std::string dir = userDataDir();
    return dir.empty() ? std::string() : ecfs::join(dir, "settings.ini");
}

bool loadGameSettings() {
    std::string path = settingsFilePath();
    std::string text;
    if (path.empty() || !ecfs::readText(path, text)) {
        std::cout << "[Settings] No settings file yet; using defaults.\n";
        gameSettings().clampAll();
        return false;
    }
    int applied = gameSettings().parse(text);
    std::cout << "[Settings] Loaded " << applied << " settings from " << path << "\n";
    return true;
}

bool saveGameSettings() {
    std::string path = settingsFilePath();
    if (path.empty()) return false;
    bool ok = ecfs::writeTextAtomic(path, gameSettings().serialize());
    if (!ok) std::cerr << "[Settings] Warning: could not write " << path << "\n";
    return ok;
}
