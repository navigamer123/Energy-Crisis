#ifndef UI_PATHS_H
#define UI_PATHS_H

// =============================================================================
// Team b-session (F-06, F-18): small file helpers for settings and save games.
// Paths are UTF-8 strings; on Windows they are converted to wide paths, so a
// Cyrillic user name in %APPDATA% works.
// =============================================================================

#include <string>

namespace ecfs {
std::string join(const std::string& dir, const std::string& name);
bool ensureDir(const std::string& dir);
bool exists(const std::string& path);
bool readText(const std::string& path, std::string& out);
// Writes to "<path>.tmp" first and renames it over the target, so a crash never leaves half a file
bool writeTextAtomic(const std::string& path, const std::string& text);
bool remove(const std::string& path);
} // namespace ecfs

#endif // UI_PATHS_H
