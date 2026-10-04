#include "../includes/UI_lang.h"

#include <fstream>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#if defined(__ANDROID__)
#include <SFML/System/FileInputStream.hpp>
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <limits.h>
#include <unistd.h>
#endif

namespace Lang {

namespace {

using Table = std::unordered_map<std::string, std::string>;

struct State {
    std::string current = "bg";
    Table active;              // the selected language (empty while it is Bulgarian)
    Table fallback;            // Bulgarian, the reference language
    bool fallbackTried = false;
    std::unordered_set<std::string> missing; // keys returned as-is (node-based: stable references)
};

State& state() {
    static State s;
    return s;
}

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) return std::string();
    const size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

bool isValidKey(const std::string& key) {
    if (key.empty()) return false;
    for (char c : key) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
        if (!ok) return false;
    }
    return key.front() != '.' && key.back() != '.';
}

bool isValidCode(const std::string& code) {
    if (code.empty() || code.size() > 16) return false;
    for (char c : code) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

// "\n" -> newline, "\\" -> backslash; any other escape is kept as written
std::string unescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            const char n = s[i + 1];
            if (n == 'n') { out += '\n'; ++i; continue; }
            if (n == '\\') { out += '\\'; ++i; continue; }
        }
        out += s[i];
    }
    return out;
}

bool readWholeFile(const std::string& path, std::string& out) {
#if defined(__ANDROID__)
    sf::FileInputStream stream;
    if (stream.open(path)) {
        auto size = stream.getSize();
        if (size.has_value() && *size > 0) {
            out.resize(static_cast<std::size_t>(*size));
            auto readBytes = stream.read(out.data(), *size);
            if (readBytes.has_value() && *readBytes > 0) {
                out.resize(static_cast<std::size_t>(*readBytes));
                return true;
            }
        }
    }
#endif
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

#if defined(_WIN32)
// Wide-path variant so an executable folder with Cyrillic letters still works
// (Win32 API instead of _wfopen, which strict -std=c++17 hides on some MinGW runtimes)
bool readWholeFileW(const std::wstring& path, std::string& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    out.clear();
    char buf[4096];
    DWORD n = 0;
    while (ReadFile(h, buf, static_cast<DWORD>(sizeof(buf)), &n, nullptr) && n > 0) out.append(buf, n);
    CloseHandle(h);
    return true;
}

std::wstring exeDirW() {
    std::wstring buf(1024, L'\0');
    for (int attempt = 0; attempt < 4; ++attempt) {
        const DWORD n = GetModuleFileNameW(nullptr, &buf[0], static_cast<DWORD>(buf.size()));
        if (n == 0) return std::wstring();
        if (n < buf.size()) {
            buf.resize(n);
            const size_t slash = buf.find_last_of(L"\\/");
            return (slash == std::wstring::npos) ? std::wstring() : buf.substr(0, slash);
        }
        buf.resize(buf.size() * 2);
    }
    return std::wstring();
}

std::wstring widenAscii(const std::string& s) {
    return std::wstring(s.begin(), s.end()); // codes are validated ASCII
}
#else
std::string exeDir() {
#if defined(__linux__)
    char buf[PATH_MAX];
    const ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        std::string p(buf);
        const size_t slash = p.find_last_of('/');
        if (slash != std::string::npos) return p.substr(0, slash);
    }
#endif
    return std::string();
}
#endif

std::string relativePath(const std::string& code) {
    return "assets/lang/" + code + ".lang";
}

// Reads the raw text of <code>.lang; sets where to the path that was used
bool readLangFile(const std::string& code, std::string& text, std::string& where) {
    const std::string rel = relativePath(code);
    if (readWholeFile(rel, text)) {
        where = rel;
        return true;
    }
#if defined(_WIN32)
    const std::wstring dir = exeDirW();
    if (!dir.empty()) {
        const std::wstring full = dir + L"\\assets\\lang\\" + widenAscii(code) + L".lang";
        if (readWholeFileW(full, text)) {
            where = "<exe>/" + rel;
            return true;
        }
    }
#else
    const std::string dir = exeDir();
    if (!dir.empty()) {
        const std::string full = dir + "/" + rel;
        if (readWholeFile(full, text)) {
            where = full;
            return true;
        }
    }
#endif
    return false;
}

void parseText(const std::string& text, const std::string& name, std::map<std::string, std::string>& out,
               std::vector<std::string>* errors) {
    size_t pos = 0;
    // Skip a UTF-8 byte order mark
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        pos = 3;
    }
    int lineNo = 0;
    while (pos <= text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        const std::string line = trim(text.substr(pos, end - pos));
        ++lineNo;
        pos = end + 1;

        if (line.empty() || line[0] == '#') {
            if (end == text.size()) break;
            continue;
        }
        const size_t eq = line.find('=');
        const std::string key = (eq == std::string::npos) ? std::string() : trim(line.substr(0, eq));
        if (eq == std::string::npos || !isValidKey(key)) {
            if (errors) errors->push_back(name + ":" + std::to_string(lineNo) + ": malformed line");
        } else {
            if (out.count(key) && errors) {
                errors->push_back(name + ":" + std::to_string(lineNo) + ": duplicate key '" + key + "'");
            }
            out[key] = unescape(trim(line.substr(eq + 1)));
        }
        if (end == text.size()) break;
    }
}

bool loadTable(const std::string& code, Table& table) {
    std::string text;
    std::string where;
    if (!readLangFile(code, text, where)) return false;
    std::map<std::string, std::string> parsed;
    std::vector<std::string> errors;
    parseText(text, where, parsed, &errors);
    for (const auto& e : errors) std::cerr << "[Lang] Warning: " << e << "\n";
    table.clear();
    table.insert(parsed.begin(), parsed.end());
    std::cout << "[Lang] Loaded " << table.size() << " strings from " << where << "\n";
    return true;
}

void ensureFallback() {
    State& s = state();
    if (s.fallbackTried) return;
    s.fallbackTried = true;
    if (!loadTable(fallbackCode(), s.fallback)) {
        std::cerr << "[Lang] Warning: " << relativePath(fallbackCode())
                  << " was not found; untranslated keys will be shown.\n";
    }
}

} // namespace

const std::string& fallbackCode() {
    static const std::string code = "bg";
    return code;
}

bool load(const std::string& code) {
    State& s = state();
    ensureFallback();
    if (!isValidCode(code)) {
        std::cerr << "[Lang] Warning: invalid language code '" << code << "'.\n";
        return false;
    }
    if (code == fallbackCode()) {
        s.active.clear();
        s.missing.clear();
        s.current = code;
        return !s.fallback.empty();
    }
    Table table;
    if (!loadTable(code, table)) {
        std::cerr << "[Lang] Warning: " << relativePath(code) << " was not found; keeping '" << s.current
                  << "'.\n";
        return false;
    }
    s.active.swap(table);
    s.missing.clear();
    s.current = code;
    return true;
}

const std::string& current() {
    return state().current;
}

const std::string& tr(const std::string& key) {
    State& s = state();
    ensureFallback();
    auto it = s.active.find(key);
    if (it != s.active.end()) return it->second;
    it = s.fallback.find(key);
    if (it != s.fallback.end()) return it->second;
    auto ins = s.missing.insert(key);
    if (ins.second) std::cerr << "[Lang] Warning: missing key '" << key << "'.\n";
    return *ins.first;
}

bool has(const std::string& key) {
    State& s = state();
    ensureFallback();
    return s.active.count(key) > 0 || s.fallback.count(key) > 0;
}

std::string format(const std::string& pattern, const std::vector<std::string>& args) {
    std::string out;
    out.reserve(pattern.size() + 16);
    for (size_t i = 0; i < pattern.size(); ++i) {
        const char c = pattern[i];
        if (c == '{' && i + 1 < pattern.size() && pattern[i + 1] == '{') { out += '{'; ++i; continue; }
        if (c == '}' && i + 1 < pattern.size() && pattern[i + 1] == '}') { out += '}'; ++i; continue; }
        if (c == '{') {
            size_t j = i + 1;
            size_t index = 0;
            while (j < pattern.size() && pattern[j] >= '0' && pattern[j] <= '9' && j - i <= 3) {
                index = index * 10 + static_cast<size_t>(pattern[j] - '0');
                ++j;
            }
            if (j > i + 1 && j < pattern.size() && pattern[j] == '}' && index < args.size()) {
                out += args[index];
                i = j;
                continue;
            }
        }
        out += c;
    }
    return out;
}

bool parseFile(const std::string& path, std::map<std::string, std::string>& out, std::vector<std::string>* errors) {
    std::string text;
    if (!readWholeFile(path, text)) return false;
    parseText(text, path, out, errors);
    return true;
}

std::string findFile(const std::string& code) {
    if (!isValidCode(code)) return std::string();
    std::string text;
    std::string where;
    return readLangFile(code, text, where) ? where : std::string();
}

} // namespace Lang
