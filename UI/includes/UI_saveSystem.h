#ifndef UI_SAVESYSTEM_H
#define UI_SAVESYSTEM_H

// =============================================================================
// Team b-session (F-18, F-02): save slots on disk.
//
// <user data dir>/saves/ holds 7 files: slot1-3 (manual), quick (F5 / F9) and
// auto1-3 (written at every day end and when a match is left; the oldest is
// replaced). A save file is:
//   ECSAVE 1
//   meta.<key>=...   (day, hour, mode, time stamp: read by the slot list)
//   ui.<key>=...     (cursors, lightning timers, control scheme)
//   [engine]
//   <GameEngine::saveSnapshot block>
// =============================================================================

#include <string>

struct SaveInfo {
    bool exists = false;   // file is there
    bool valid = false;    // header could be read
    std::string path;
    long long savedAt = 0; // seconds since 1970 (std::time)
    int day = 0;
    float hour = 0.0f;
    int difficulty = 0;    // BotDifficulty as int (0 = two players)
    int scheme = 0;        // ControlScheme as int
    float p1Share = 0.5f;
};

namespace saves {
constexpr int SLOT_COUNT = 7;
constexpr int QUICK_SLOT = 3;      // index of quick.ecsave
constexpr int FIRST_AUTO_SLOT = 4; // auto1..3 are slots 4..6

std::string dir();                  // <user data dir>/saves (created on demand)
std::string pathFor(int slot);      // 0..6
std::string slotName(int slot);     // "СЛОТ 1", "БЪРЗ ЗАПИС", "АВТОЗАПИС 2"
bool isAuto(int slot);
bool canSaveTo(int slot);           // manual slots and the quick slot

SaveInfo readInfo(const std::string& path);
std::string modeText(int difficulty);                  // "Двама играчи" / "Срещу бот (Среден)"
std::string describe(const SaveInfo& info, bool withDate = true); // "Ден 5 · 13:30 · Срещу бот (Среден) · 03.10 14:22"
std::string shortLabel(const SaveInfo& info);                       // "Ден 5 · 13:30 · Срещу бот (Среден)"
int nextAutosaveSlot();             // the missing or oldest autosave slot
std::string newestSave(SaveInfo* out = nullptr); // newest valid save of any slot, "" when none

// The "ECSAVE 1" line and the meta.* lines (the caller appends ui.* lines, "[engine]" and the snapshot)
std::string headerText(const SaveInfo& meta);
} // namespace saves

#endif // UI_SAVESYSTEM_H
