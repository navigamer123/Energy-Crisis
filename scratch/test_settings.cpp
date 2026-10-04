#include "../UI/includes/UI_settings.h"
#include "../UI/scr/UI_settings.cpp"
#include "../UI/includes/UI_lang.h"
#include "../UI/scr/UI_lang.cpp"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>

int main() {
    std::cout << "========================================\n";
    std::cout << "    Testing UI_settings & Persistence   \n";
    std::cout << "========================================\n";

    // Test settings file in local directory
    const std::string testIni = "test_settings_custom.ini";
    setenv("EC_SETTINGS", testIni.c_str(), 1);

    // Clean up any old test file
    std::filesystem::remove(testIni);

    UI_settings& settings = UI_settings::get();
    settings.load();

    // 1. Check default settings
    assert(settings.getLanguage() == "bg" || settings.getLanguage() == "en");

    // 2. Switch language to English
    std::cout << "[Test] Setting language to 'en'...\n";
    settings.setLanguage("en");
    assert(settings.getLanguage() == "en");
    assert(Lang::current() == "en");
    assert(Lang::tr("settings.title") == "SETTINGS");
    assert(Lang::tr("common.back") == "BACK");

    // 3. Set volume, sfx, difficulty
    std::cout << "[Test] Setting volume to 65%, sfx off, bot difficulty hard (2)...\n";
    settings.setVolume(65);
    settings.setSoundEffectsEnabled(false);
    settings.setBotDifficultyIndex(2);

    assert(settings.getVolume() == 65);
    assert(!settings.isSoundEffectsEnabled());
    assert(settings.getBotDifficultyIndex() == 2);

    // 4. Verify file was saved to disk
    assert(std::filesystem::exists(testIni));

    // Verify content of the saved file
    std::ifstream in(testIni);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    std::cout << "[Test] Saved INI contents:\n" << content << "\n";
    assert(content.find("language = en") != std::string::npos);
    assert(content.find("master_volume = 65") != std::string::npos);
    assert(content.find("sound_effects = false") != std::string::npos);
    assert(content.find("bot_difficulty = 2") != std::string::npos);

    // 5. Test reloading from disk when file changes externally
    {
        std::ofstream out(testIni);
        out << "[ui]\nlanguage = bg\n[audio]\nmaster_volume = 42\nsound_effects = true\n[game]\nbot_difficulty = 0\n";
        out.close();
    }
    settings.load();
    assert(settings.getLanguage() == "bg");
    assert(settings.getVolume() == 42);
    assert(settings.isSoundEffectsEnabled() == true);
    assert(settings.getBotDifficultyIndex() == 0);

    // 6. Switch back to Bulgarian and verify
    std::cout << "[Test] Switching back to 'bg'...\n";
    settings.setLanguage("bg");
    assert(settings.getLanguage() == "bg");
    assert(Lang::current() == "bg");
    assert(Lang::tr("settings.title") == "НАСТРОЙКИ");
    assert(Lang::tr("common.back") == "НАЗАД");

    // Clean up test file
    std::filesystem::remove(testIni);

    std::cout << "[Test] ALL UI_settings tests PASSED!\n";
    return 0;
}
