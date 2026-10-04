// Headless check of the language files and the Lang loader (F-20, English option).
//
//   make test                      (builds and runs every scratch/test_*.cpp from the repository root)
//   g++ -std=c++17 scratch/test_lang.cpp -o test_lang && ./test_lang [lang-dir]
//
// Checks that assets/lang/bg.lang and en.lang parse cleanly, have identical key sets and the same
// {N} placeholders per key, then exercises Lang::load / tr / fmt / format and the fallbacks.
// The loader has no SFML dependency, so it is compiled straight into this program.
#include "../UI/includes/UI_lang.h"
#include "../UI/scr/UI_lang.cpp"

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        ++g_failures;
        std::cout << "FAIL: " << what << "\n";
    }
}

// Set of {N} placeholders in a value; badBrace is set when a brace is not part of {N}, {{ or }}
std::set<std::string> placeholders(const std::string& s, bool& badBrace) {
    std::set<std::string> out;
    badBrace = false;
    for (size_t i = 0; i < s.size(); ++i) {
        if ((s[i] == '{' || s[i] == '}') && i + 1 < s.size() && s[i + 1] == s[i]) { ++i; continue; }
        if (s[i] == '{') {
            size_t j = i + 1;
            while (j < s.size() && s[j] >= '0' && s[j] <= '9') ++j;
            if (j > i + 1 && j < s.size() && s[j] == '}') {
                out.insert(s.substr(i, j - i + 1));
                i = j;
                continue;
            }
            badBrace = true;
        } else if (s[i] == '}') {
            badBrace = true;
        }
    }
    return out;
}

std::string join(const std::set<std::string>& s) {
    std::string r;
    for (const auto& x : s) r += (r.empty() ? "" : " ") + x;
    return r.empty() ? "(none)" : r;
}

} // namespace

int main(int argc, char* argv[]) {
    const std::string dir = (argc > 1) ? argv[1] : "assets/lang";

    // 1. Both files parse without malformed lines or duplicate keys
    std::map<std::string, std::string> bg, en;
    std::vector<std::string> errors;
    check(Lang::parseFile(dir + "/bg.lang", bg, &errors), "cannot read " + dir + "/bg.lang (run from the repository root)");
    check(Lang::parseFile(dir + "/en.lang", en, &errors), "cannot read " + dir + "/en.lang");
    for (const auto& e : errors) check(false, e);
    check(!bg.empty(), "bg.lang has no keys");
    check(bg.count("lang.name") == 1, "bg.lang has no lang.name");
    check(en.count("lang.name") == 1, "en.lang has no lang.name");

    // 2. Identical key sets, matching placeholders, no empty values, no stray braces
    for (const auto& kv : bg) {
        bool badBg = false;
        const std::set<std::string> phBg = placeholders(kv.second, badBg);
        check(!kv.second.empty(), "bg.lang: empty value for " + kv.first);
        check(!badBg, "bg.lang: stray brace in " + kv.first);
        auto it = en.find(kv.first);
        if (it == en.end()) {
            check(false, "en.lang: missing key " + kv.first);
            continue;
        }
        bool badEn = false;
        const std::set<std::string> phEn = placeholders(it->second, badEn);
        check(!it->second.empty(), "en.lang: empty value for " + kv.first);
        check(!badEn, "en.lang: stray brace in " + kv.first);
        check(phBg == phEn, "placeholders differ for " + kv.first + ": bg " + join(phBg) + " / en " + join(phEn));
    }
    for (const auto& kv : en) check(bg.count(kv.first) == 1, "en.lang: extra key " + kv.first);

    // 3. Formatting
    check(Lang::format("{0} MW ({1}%)", { "120", "55" }) == "120 MW (55%)", "format: basic placeholders");
    check(Lang::format("{1} / {0}", { "a", "b" }) == "b / a", "format: reordered placeholders");
    check(Lang::format("{{0}} {1} {5}", { "a", "b" }) == "{0} b {5}", "format: escaped braces and missing argument");
    check(Lang::format("no args", {}) == "no args", "format: no placeholders");

    // 4. Loader (reads assets/lang relative to the working directory)
    if (dir == "assets/lang") {
        check(Lang::current() == "bg", "default language is not bg");
        check(Lang::tr("menu.play") == bg["menu.play"], "tr: Bulgarian text before any load()");
        check(Lang::tr("no.such.key") == "no.such.key", "tr: missing key does not fall back to the key");
        check(!Lang::findFile("en").empty(), "findFile(en) found nothing");

        check(Lang::load("en"), "load(en) failed");
        check(Lang::current() == "en", "current() is not en after load(en)");
        check(Lang::tr("menu.play") == en["menu.play"], "tr: English text after load(en)");
        check(Lang::tr("lang.name") == "English", "tr: lang.name is not English");
        check(Lang::fmt("hud.power", 120, 55) == Lang::format(en["hud.power"], { "120", "55" }), "fmt: numbers");
        check(Lang::fmt("msg.built", std::string("X"), 60) == Lang::format(en["msg.built"], { "X", "60" }), "fmt: string + number");
        check(Lang::fmt("popup.tip", "abc") == Lang::format(en["popup.tip"], { "abc" }), "fmt: string literal");
        check(Lang::has("menu.play") && !Lang::has("no.such.key"), "has()");

        check(!Lang::load("xx"), "load(xx) should fail (no such file)");
        check(Lang::current() == "en", "a failed load() changed the language");
        check(!Lang::load("../bg"), "load() accepted a path instead of a language code");
        check(!Lang::load(""), "load() accepted an empty code");

        check(Lang::load("bg"), "load(bg) failed");
        check(Lang::current() == "bg" && Lang::tr("menu.play") == bg["menu.play"], "switching back to bg");
    }

    std::cout << "[test_lang] bg.lang: " << bg.size() << " keys, en.lang: " << en.size() << " keys\n";
    if (g_failures == 0) {
        std::cout << "[test_lang] PASS\n";
        return 0;
    }
    std::cout << "[test_lang] FAIL (" << g_failures << " problems)\n";
    return 1;
}
