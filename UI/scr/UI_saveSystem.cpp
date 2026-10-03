// =============================================================================
// Team b-session (F-18, F-02): save slot files and their descriptions
// =============================================================================
#include "../includes/UI_saveSystem.h"
#include "../includes/UI_paths.h"
#include "../includes/UI_settings.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sstream>

namespace saves {

std::string dir() {
    std::string base = userDataDir();
    if (base.empty()) return std::string();
    std::string d = ecfs::join(base, "saves");
    ecfs::ensureDir(d);
    return d;
}

std::string pathFor(int slot) {
    static const char* files[SLOT_COUNT] = { "slot1.ecsave", "slot2.ecsave", "slot3.ecsave", "quick.ecsave",
                                             "auto1.ecsave", "auto2.ecsave", "auto3.ecsave" };
    if (slot < 0 || slot >= SLOT_COUNT) return std::string();
    std::string d = dir();
    return d.empty() ? std::string() : ecfs::join(d, files[slot]);
}

std::string slotName(int slot) {
    if (slot >= 0 && slot < QUICK_SLOT) return "СЛОТ " + std::to_string(slot + 1);
    if (slot == QUICK_SLOT) return "БЪРЗ ЗАПИС";
    if (slot >= FIRST_AUTO_SLOT && slot < SLOT_COUNT) return "АВТОЗАПИС " + std::to_string(slot - FIRST_AUTO_SLOT + 1);
    return "?";
}

bool isAuto(int slot) { return slot >= FIRST_AUTO_SLOT && slot < SLOT_COUNT; }
bool canSaveTo(int slot) { return slot >= 0 && slot <= QUICK_SLOT; }

SaveInfo readInfo(const std::string& path) {
    SaveInfo info;
    info.path = path;
    std::string text;
    if (path.empty() || !ecfs::readText(path, text)) return info;
    info.exists = true;
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line)) return info;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != "ECSAVE 1") return info;
    bool sawDay = false, sawEngine = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == "[engine]") { sawEngine = true; break; }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "meta.savedAt") info.savedAt = std::atoll(v.c_str());
        else if (k == "meta.day") { info.day = std::atoi(v.c_str()); sawDay = true; }
        else if (k == "meta.hour") info.hour = static_cast<float>(std::atof(v.c_str()));
        else if (k == "meta.difficulty") info.difficulty = std::atoi(v.c_str());
        else if (k == "meta.scheme") info.scheme = std::atoi(v.c_str());
        else if (k == "meta.p1Share") info.p1Share = static_cast<float>(std::atof(v.c_str()));
        else if (k == "meta.finished") info.finished = (std::atoi(v.c_str()) != 0);
    }
    info.valid = sawDay && sawEngine && info.day >= 1;
    return info;
}

std::string modeText(int difficulty) {
    switch (difficulty) {
        case 1: return "Срещу бот (Лесен)";
        case 2: return "Срещу бот (Среден)";
        case 3: return "Срещу бот (Труден)";
        default: return "Двама играчи";
    }
}

std::string describe(const SaveInfo& info, bool withDate) {
    if (!info.exists) return "Празен";
    if (!info.valid) return "Повреден файл";
    if (info.finished) return "Завършен мач · ден " + std::to_string(info.day) + " · " + modeText(info.difficulty);
    int h = static_cast<int>(info.hour);
    int m = static_cast<int>((info.hour - static_cast<float>(h)) * 60.0f);
    char clock[16];
    std::snprintf(clock, sizeof(clock), "%02d:%02d", h % 24, std::max(0, std::min(59, m)));
    int p1 = static_cast<int>(std::lround(info.p1Share * 100.0f));
    std::string s = "Ден " + std::to_string(info.day) + " · " + clock + " · " + modeText(info.difficulty) + " · " +
                    std::to_string(p1) + "% / " + std::to_string(100 - p1) + "%";
    if (withDate && info.savedAt > 0) {
        std::time_t t = static_cast<std::time_t>(info.savedAt);
        std::tm* lt = std::localtime(&t);
        if (lt != nullptr) {
            char date[32];
            std::strftime(date, sizeof(date), "%d.%m %H:%M", lt);
            s += std::string(" · ") + date;
        }
    }
    return s;
}

std::string shortLabel(const SaveInfo& info) {
    int h = static_cast<int>(info.hour);
    int m = static_cast<int>((info.hour - static_cast<float>(h)) * 60.0f);
    char clock[16];
    std::snprintf(clock, sizeof(clock), "%02d:%02d", h % 24, std::max(0, std::min(59, m)));
    return "Ден " + std::to_string(info.day) + " · " + clock + " · " + modeText(info.difficulty);
}

int nextAutosaveSlot() {
    int best = FIRST_AUTO_SLOT;
    long long oldest = -1;
    for (int s = FIRST_AUTO_SLOT; s < SLOT_COUNT; ++s) {
        SaveInfo i = readInfo(pathFor(s));
        if (!i.exists || !i.valid) return s;
        if (oldest < 0 || i.savedAt < oldest) {
            oldest = i.savedAt;
            best = s;
        }
    }
    return best;
}

std::string newestSave(SaveInfo* out) {
    SaveInfo best;
    for (int s = 0; s < SLOT_COUNT; ++s) {
        SaveInfo i = readInfo(pathFor(s));
        if (i.valid && (!best.valid || i.savedAt > best.savedAt)) best = i;
    }
    if (out != nullptr) *out = best;
    // A decided match is not "continued" (its earlier saves stay loadable from the save panel)
    return (best.valid && !best.finished) ? best.path : std::string();
}

std::string headerText(const SaveInfo& meta) {
    std::ostringstream o;
    o << "ECSAVE 1\n";
    o << "meta.savedAt=" << meta.savedAt << "\n";
    o << "meta.day=" << meta.day << "\n";
    o << "meta.hour=" << meta.hour << "\n";
    o << "meta.difficulty=" << meta.difficulty << "\n";
    o << "meta.scheme=" << meta.scheme << "\n";
    o << "meta.p1Share=" << meta.p1Share << "\n";
    o << "meta.finished=" << (meta.finished ? 1 : 0) << "\n";
    return o.str();
}

} // namespace saves
