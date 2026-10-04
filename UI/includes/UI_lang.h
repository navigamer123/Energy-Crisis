#ifndef UI_LANG_H
#define UI_LANG_H

// -----------------------------------------------------------------------------
// Lang - player-facing text in several languages (F-20: English option).
//
// Strings live in assets/lang/<code>.lang ("key = text", UTF-8, see docs/I18N.md).
// Bulgarian (bg) is the reference language and the fallback for missing keys.
//
//   Lang::load("en");                         // switch language (bg is always the fallback)
//   toUtf8(Lang::tr("menu.play"));            // UTF-8 text -> sf::String via toUtf8()
//   toUtf8(Lang::fmt("hud.power", mw, pct));  // "{0} MW ({1}% ток)" -> "120 MW (55% ток)"
//
// No SFML dependency, so the loader also builds headless (scratch/test_lang.cpp).
// Not thread-safe: call it from the UI thread only (the game is single-threaded).
// -----------------------------------------------------------------------------

#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace Lang {

// Code of the reference / fallback language ("bg")
const std::string& fallbackCode();

// Loads assets/lang/<code>.lang (code: lowercase letters, digits, '_' or '-').
// The Bulgarian file is always loaded as the fallback. Looks in assets/lang relative to the
// working directory first (main.cpp already switches to the executable folder), then next to
// the executable. On failure the previous language stays active and false is returned.
bool load(const std::string& code);

// Code of the active language ("bg" until load() succeeds with another code)
const std::string& current();

// Translated UTF-8 text for key: active language -> Bulgarian -> the key itself.
// The returned reference stays valid until the next load().
const std::string& tr(const std::string& key);

// True when the key exists in the active or the fallback language
bool has(const std::string& key);

// Replaces {0}, {1}, ... in pattern with args; "{{" and "}}" give literal braces.
// A placeholder without a matching argument is left unchanged.
std::string format(const std::string& pattern, const std::vector<std::string>& args);

// Parses one .lang file into out (no fallback). Returns false when the file cannot be read.
// Malformed lines and duplicate keys are skipped/overwritten and reported in errors (if given).
bool parseFile(const std::string& path, std::map<std::string, std::string>& out,
               std::vector<std::string>* errors = nullptr);

// Path load() would read for code ("" when no file is found)
std::string findFile(const std::string& code);

namespace detail {
inline std::string toArg(const std::string& s) { return s; }
inline std::string toArg(const char* s) { return s ? std::string(s) : std::string(); }
inline std::string toArg(char c) { return std::string(1, c); }
template <typename T>
std::string toArg(const T& value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}
} // namespace detail

// tr(key) with {0}, {1}, ... replaced by args (numbers via operator<<; pre-format floats
// that need a fixed precision, e.g. "%.1f", before passing them).
template <typename... Args>
std::string fmt(const std::string& key, const Args&... args) {
    return format(tr(key), std::vector<std::string>{ detail::toArg(args)... });
}

} // namespace Lang

#endif // UI_LANG_H
