#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

#include <string>

struct GameSettings {
    std::string language = "bg";      // "bg" or "en"
    int volume = 80;                  // 0..100 %
    bool soundEffects = true;         // true / false
    int botDifficultyIndex = 1;       // 0: Easy / Лесно, 1: Medium / Средно, 2: Hard / Трудно
};

class UI_settings {
public:
    static UI_settings& get();

    void load();
    void save();

    const GameSettings& getSettings() const { return current; }
    const std::string& getLanguage() const { return current.language; }
    void setLanguage(const std::string& lang);

    int getVolume() const { return current.volume; }
    void setVolume(int vol);

    bool isSoundEffectsEnabled() const { return current.soundEffects; }
    void setSoundEffectsEnabled(bool on);

    int getBotDifficultyIndex() const { return current.botDifficultyIndex; }
    void setBotDifficultyIndex(int idx);

    std::string getSettingsPath() const;

private:
    UI_settings();
    ~UI_settings() = default;

    GameSettings current;
    std::string resolvedPath;

    std::string resolvePath();
};

#endif // UI_SETTINGS_H
