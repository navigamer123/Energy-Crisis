// =============================================================================
// ENERGY CRISIS - UI SESSION FLOW CHECKS (team b-session: F-02, F-05, F-06, F-10, F-18)
// Needs SFML (not part of "make test"): drives the real UI components with synthetic
// key events and checks continue, the confirm dialog, the settings screen, the key
// table, the tutorial policy, the pause menu, save/load, autosave rotation and F5/F9.
// Build: compile this file with every UI/scr and Game/scr object except main.o and
// link sfml-graphics/window/system, e.g. (from the repo root, objects in BUILD):
//   g++ -std=c++17 -IUI/includes -IGame/includes scratch/ui_session_flow.cpp \
//       $(find BUILD/UI BUILD/Game -name '*.o') -lsfml-graphics -lsfml-window -lsfml-system
// Run with EC_USERDIR=<empty scratch folder> (it deletes the saves in that folder).
// =============================================================================
#include <SFML/Graphics.hpp>
#include <cstdio>
#include <iostream>
#include <string>
#include "UI_mainMenu.h"
#include "UI_map.h"
#include "UI_input.h"
#include "UI_paths.h"
#include "UI_saveSystem.h"
#include "UI_settings.h"
#include "UI_settingsMenu.h"
#include "UI_keybindMenu.h"

static int g_fail = 0, g_checks = 0;
#define CHECK(c, msg) do { ++g_checks; if (!(c)) { ++g_fail; std::cerr << "FAIL line " << __LINE__ << ": " << #c << " | " << msg << "\n"; } } while (0)

static sf::Event kp(sf::Keyboard::Key k, sf::Keyboard::Scan s) { sf::Event::KeyPressed e; e.code = k; e.scancode = s; return sf::Event(e); }
static sf::Event kr(sf::Keyboard::Key k, sf::Keyboard::Scan s) { sf::Event::KeyReleased e; e.code = k; e.scancode = s; return sf::Event(e); }
using K = sf::Keyboard::Key;
using S = sf::Keyboard::Scan;

int main() {
    // Fresh user dir per run
    std::string dir = std::getenv("EC_USERDIR") ? std::getenv("EC_USERDIR") : "";
    if (dir.empty()) { std::cerr << "set EC_USERDIR\n"; return 2; }
    for (int s = 0; s < saves::SLOT_COUNT; ++s) ecfs::remove(saves::pathFor(s));
    ecfs::remove(settingsFilePath());
    loadGameSettings();

    sf::RenderWindow window(sf::VideoMode({ 1600, 900 }), "flow");
    window.setVisible(false);
    sf::View gameView({ 800.0f, 450.0f }, { 1600.0f, 900.0f });
    window.setView(gameView);

    // --- Main menu: continue + confirm new match ---
    {
        UI_mainMenu menu;
        auto send = [&](K k, S s) { menu.handleEvent(kp(k, s), window); menu.handleEvent(kr(k, s), window); };
        menu.setContinueInfo(true, true, "Ден 5 · 13:30 · Двама играчи");
        send(K::Enter, S::Enter);
        CHECK(menu.isContinueRequested(), "Enter on the preselected ПРОДЪЛЖИ");
        menu.resetContinueRequest();
        send(K::Down, S::Down);         // НОВА ИГРА
        send(K::Enter, S::Enter);       // -> confirm dialog (default ОТКАЗ)
        send(K::Enter, S::Enter);       // ОТКАЗ
        CHECK(!menu.isPlayRequested() && !menu.isContinueRequested(), "cancel keeps the match");
        CHECK(menu.contentBounds().size.y < 600.0f, "still on the main screen");
        send(K::Enter, S::Enter);       // dialog again
        send(K::Left, S::Left);         // ЗАПОЧНИ НОВА
        send(K::Enter, S::Enter);
        CHECK(menu.contentBounds().size.y < 600.0f, "mode select bounds");
        send(K::Enter, S::Enter);       // co-op -> play controls
        CHECK(menu.contentBounds().size.y > 600.0f, "reached the control-scheme screen after confirming");
        // continue without memory: no confirm
        UI_mainMenu m2;
        m2.setContinueInfo(true, false, "Запис: Ден 2");
        m2.handleEvent(kp(K::Down, S::Down), window);
        m2.handleEvent(kp(K::Enter, S::Enter), window);
        CHECK(m2.contentBounds().size.y < 600.0f && !m2.isPlayRequested(), "disk-only continue: ИГРА goes straight to mode select");
    }

    // --- Settings screen changes real values and saves ---
    {
        UI_settingsMenu sm;
        sm.open(false);
        auto send = [&](K k, S s) { sm.handleEvent(kp(k, s), window); };
        int before = gameSettings().fpsLimit;
        send(K::Down, S::Down); send(K::Down, S::Down); // row 2 = FPS
        send(K::Right, S::Right);
        CHECK(gameSettings().fpsLimit != before, "FPS row changed the setting: " << gameSettings().fpsLimit);
        send(K::Down, S::Down); send(K::Down, S::Down); // UI scale
        send(K::Right, S::Right);
        CHECK(gameSettings().uiScalePercent == 125, "UI scale 100 -> 125: " << gameSettings().uiScalePercent);
        send(K::PageDown, S::PageDown); send(K::PageDown, S::PageDown); // ИГРА tab
        // focus stays on row 4 (Автоматичен запис) -> toggle off and on
        bool autosaveBefore = gameSettings().autosave;
        send(K::Right, S::Right);
        CHECK(gameSettings().autosave != autosaveBefore, "autosave toggled");
        send(K::Left, S::Left);
        send(K::Escape, S::Escape);
        CHECK(!sm.isOpen(), "Esc closes");
        std::string text;
        CHECK(ecfs::readText(settingsFilePath(), text) && text.find("ui_scale=125") != std::string::npos, "settings.ini written on close");
        gameSettings().uiScalePercent = 100;
    }

    // --- Key table: capture, conflict refusal, save ---
    {
        UI_keybindMenu kb;
        kb.open();
        auto send = [&](K k, S s) { kb.handleEvent(kp(k, s), window); };
        // P1 action primary -> K
        for (int i = 0; i < 4; ++i) send(K::Down, S::Down);
        send(K::Enter, S::Enter);
        CHECK(kb.isCapturing(), "Enter starts the key capture");
        send(K::K, S::K);
        CHECK(!kb.isCapturing(), "a key ends the capture");
        // conflict: P1 upgrade primary -> E (already next building)
        send(K::Down, S::Down);
        send(K::Enter, S::Enter);
        send(K::E, S::E);
        // try to save: focus the button row, SAVE (col 1)
        for (int i = 0; i < 10; ++i) send(K::Down, S::Down);
        send(K::Right, S::Right);
        send(K::Enter, S::Enter);
        CHECK(kb.isOpen(), "ЗАПАЗИ refused while keys repeat");
        // fix the conflict: back to the upgrade row, clear it, then set F5 (reserved) -> refused
        for (int i = 0; i < 10; ++i) send(K::Up, S::Up);
        send(K::Left, S::Left);
        send(K::Enter, S::Enter);
        send(K::F5, S::F5);
        CHECK(gameSettings().keys.key(1, InputAction::Upgrade, 0) == KeyCode::F, "live bindings untouched before saving");
        send(K::Enter, S::Enter);
        send(K::G, S::G);
        for (int i = 0; i < 10; ++i) send(K::Down, S::Down);
        send(K::Right, S::Right); // col 0 -> 1 = ЗАПАЗИ
        send(K::Enter, S::Enter);
        CHECK(!kb.isOpen(), "ЗАПАЗИ closes once the conflict is gone");
        CHECK(gameSettings().keys.key(1, InputAction::Action, 0) == KeyCode::K, "P1 action rebound to K");
        CHECK(gameSettings().keys.key(1, InputAction::Upgrade, 0) == KeyCode::G, "P1 upgrade rebound to G");
        CHECK(!gameSettings().keys.hasConflicts(), "saved bindings conflict-free");
        inputRouter().setMatchContext(false, ControlScheme::BOTH_KEYBOARD);
        CHECK(inputRouter().hint(1, InputAction::Action, false) == "[K]", "hint follows rebinding: " << inputRouter().hint(1, InputAction::Action, false));
        gameSettings().keys = InputMap::defaults();
    }

    // --- Match: tutorial policy, pause menu, save/load, autosave rotation ---
    {
        UI_map map;
        auto send = [&](K k, S s) { map.handleEvent(kp(k, s), window); map.handleEvent(kr(k, s), window); };
        CHECK(!map.hasMatchInProgress(), "no match before the first start");
        gameSettings().tutorialPolicy = TutorialPolicy::Never;
        map.restartMatch();
        map.setBotDifficulty(BotDifficulty::EASY);
        CHECK(!map.isTutorialActive(), "НИКОГА: no tutorial");
        gameSettings().tutorialPolicy = TutorialPolicy::Always;
        map.setBotDifficulty(BotDifficulty::HARD);
        CHECK(map.isTutorialActive(), "ВИНАГИ: tutorial even on HARD");
        gameSettings().tutorialPolicy = TutorialPolicy::Auto;
        gameSettings().tutorialDoneSolo = true;
        map.setBotDifficulty(BotDifficulty::MEDIUM);
        CHECK(!map.isTutorialActive(), "АВТОМАТИЧНО after finishing once: no tutorial");
        gameSettings().tutorialDoneCoop = false;
        map.setBotDifficulty(BotDifficulty::NONE);
        CHECK(map.isTutorialActive(), "АВТОМАТИЧНО co-op not done: tutorial");
        map.skipTutorial();
        window.clear();
        map.render(window); // updateSession marks the co-op tutorial as done
        CHECK(gameSettings().tutorialDoneCoop, "skipping marks the tutorial as seen");

        map.setBotDifficulty(BotDifficulty::MEDIUM);
        CHECK(map.hasMatchInProgress(), "match in progress");
        CHECK(map.matchSummary().find("Среден") != std::string::npos, map.matchSummary());

        // Pause menu -> НАСТРОЙКИ
        send(K::Escape, S::Escape);
        CHECK(map.wantsMenuInput(), "pause = menu input");
        send(K::Down, S::Down); send(K::Down, S::Down);
        send(K::Enter, S::Enter);
        send(K::Escape, S::Escape); // close settings
        send(K::Escape, S::Escape); // resume
        CHECK(!map.wantsMenuInput(), "resumed");

        // Save to a file, play on, load back
        std::string err;
        std::string path = saves::pathFor(0);
        CHECK(map.saveToFile(path, err), err);
        int savedDay = map.getEngine().getCurrentDay();
        int goldBefore = map.getEngine().getPlayerEconomy(1).gold;
        const_cast<GameEngine&>(map.getEngine()).getPlayerEconomyMut(1).gold += 777;
        CHECK(map.loadFromFile(path, err), err);
        CHECK(map.getEngine().getPlayerEconomy(1).gold == goldBefore, "gold restored");
        CHECK(map.getEngine().getCurrentDay() == savedDay, "day restored");
        CHECK(map.getBotDifficulty() == BotDifficulty::MEDIUM && map.isBotActive(), "bot difficulty restored");
        CHECK(!map.isTutorialActive(), "no tutorial after loading");

        // Damaged file: refused, match untouched
        ecfs::writeTextAtomic(saves::pathFor(1), "ECSAVE 1\nmeta.day=3\n[engine]\nengine_version=1\n");
        const_cast<GameEngine&>(map.getEngine()).getPlayerEconomyMut(1).gold = 4242;
        CHECK(!map.loadFromFile(saves::pathFor(1), err), "damaged save refused");
        CHECK(map.getEngine().getPlayerEconomy(1).gold == 4242, "match untouched by a failed load");
        std::cout << "damaged save error: " << err << "\n";

        // Autosave rotation: four autosaves keep three files, the oldest is replaced
        for (int i = 0; i < 3; ++i) {
            CHECK(map.writeAutosave(), "autosave " << i);
            const_cast<GameEngine&>(map.getEngine()).update(2.0f); // later moment = new autosave
        }
        int filesBefore = 0;
        for (int s = saves::FIRST_AUTO_SLOT; s < saves::SLOT_COUNT; ++s) filesBefore += saves::readInfo(saves::pathFor(s)).valid ? 1 : 0;
        CHECK(filesBefore == 3, "three autosave files: " << filesBefore);
        CHECK(map.writeAutosave(), "fourth autosave");
        CHECK(map.writeAutosave(), "same moment again: reported as saved");
        int filesAfter = 0;
        for (int s = saves::FIRST_AUTO_SLOT; s < saves::SLOT_COUNT; ++s) filesAfter += saves::readInfo(saves::pathFor(s)).valid ? 1 : 0;
        CHECK(filesAfter == 3, "still three autosave files");
        SaveInfo a1 = saves::readInfo(saves::pathFor(saves::FIRST_AUTO_SLOT));
        CHECK(a1.valid, "auto1 valid");
        // make auto2 the oldest
        std::string t2;
        ecfs::readText(saves::pathFor(saves::FIRST_AUTO_SLOT + 1), t2);
        size_t p = t2.find("meta.savedAt=");
        t2.replace(p, t2.find('\n', p) - p, "meta.savedAt=1");
        ecfs::writeTextAtomic(saves::pathFor(saves::FIRST_AUTO_SLOT + 1), t2);
        CHECK(saves::nextAutosaveSlot() == saves::FIRST_AUTO_SLOT + 1, "oldest autosave is replaced next");
        gameSettings().autosave = false;
        CHECK(!map.writeAutosave(), "autosave off: nothing written");
        gameSettings().autosave = true;

        // Newest save drives ПРОДЪЛЖИ after a restart of the game
        SaveInfo newest;
        std::string np = saves::newestSave(&newest);
        CHECK(!np.empty() && newest.valid, "newest save found");
        CHECK(saves::shortLabel(newest).find("Ден ") == 0, saves::shortLabel(newest));

        // Quicksave / quickload keys
        const_cast<GameEngine&>(map.getEngine()).getPlayerEconomyMut(2).wood = 123;
        send(K::F5, S::F5);
        const_cast<GameEngine&>(map.getEngine()).getPlayerEconomyMut(2).wood = 5;
        send(K::F9, S::F9);
        CHECK(map.getEngine().getPlayerEconomy(2).wood == 123, "F9 restores the F5 state");

        // Loading the OLDEST autosave from the panel must load it, not the safety autosave written first
        for (int s = saves::FIRST_AUTO_SLOT; s < saves::SLOT_COUNT; ++s) ecfs::remove(saves::pathFor(s));
        for (int i = 0; i < 3; ++i) {
            const_cast<GameEngine&>(map.getEngine()).getPlayerEconomyMut(1).gold = 100 + i;
            const_cast<GameEngine&>(map.getEngine()).update(2.0f);
            CHECK(map.writeAutosave(), "rotation autosave " << i);
        }
        {
            std::string t1;
            ecfs::readText(saves::pathFor(saves::FIRST_AUTO_SLOT), t1);
            size_t q = t1.find("meta.savedAt=");
            t1.replace(q, t1.find('\n', q) - q, "meta.savedAt=1");
            ecfs::writeTextAtomic(saves::pathFor(saves::FIRST_AUTO_SLOT), t1);
        }
        const_cast<GameEngine&>(map.getEngine()).getPlayerEconomyMut(1).gold = 999;
        const_cast<GameEngine&>(map.getEngine()).update(2.0f);
        send(K::Escape, S::Escape);
        send(K::Down, S::Down);
        send(K::Enter, S::Enter); // ЗАПИС / ЗАРЕЖДАНЕ
        for (int i = 0; i < 4; ++i) send(K::Down, S::Down); // АВТОЗАПИС 1 (oldest), column ЗАРЕДИ
        send(K::Enter, S::Enter);
        CHECK(map.getEngine().getPlayerEconomy(1).gold == 100, "oldest autosave loaded intact: gold " << map.getEngine().getPlayerEconomy(1).gold);
        int holds999 = 0;
        for (int s = saves::FIRST_AUTO_SLOT; s < saves::SLOT_COUNT; ++s) {
            std::string t;
            ecfs::readText(saves::pathFor(s), t);
            holds999 += (t.find("p1.gold=999") != std::string::npos) ? 1 : 0;
        }
        CHECK(holds999 == 1, "the replaced match (gold 999) is in the autosaves: " << holds999);
        send(K::Escape, S::Escape); // resume

        CHECK(map.hasMatchInProgress(), "still running");
    }

    // --- A finished match is not offered as ПРОДЪЛЖИ ---
    {
        std::string t;
        CHECK(ecfs::readText(saves::pathFor(0), t) && t.find("meta.finished=0") != std::string::npos, "saves carry meta.finished");
        size_t q = t.find("meta.finished=0");
        t.replace(q, 15, "meta.finished=1");
        q = t.find("meta.savedAt=");
        t.replace(q, t.find('\n', q) - q, "meta.savedAt=99999999999");
        ecfs::writeTextAtomic(saves::pathFor(2), t);
        SaveInfo fin = saves::readInfo(saves::pathFor(2));
        CHECK(fin.valid && fin.finished, "finished flag read back");
        CHECK(saves::newestSave().empty(), "newest save is a finished match: no ПРОДЪЛЖИ");
        CHECK(saves::describe(fin).find("Завършен") == 0, saves::describe(fin));
        ecfs::remove(saves::pathFor(2));
        CHECK(!saves::newestSave().empty(), "older unfinished save offered again");
    }

    // --- Gamepad bridge without pads: nothing happens ---
    {
        GamepadMenuBridge bridge;
        std::vector<sf::Event> ev;
        std::vector<GamepadMenuBridge::Press> pr;
        bridge.update(0.016f, GamepadMenuBridge::Mode::MainMenu, ev, pr);
        CHECK(ev.empty() && pr.empty(), "no pad -> no synthetic events (pads connected: " << InputRouter::connectedPads().size() << ")");
    }

    std::cout << (g_fail == 0 ? "ALL " : "") << (g_checks - g_fail) << "/" << g_checks << " UI FLOW CHECKS PASSED\n";
    return g_fail == 0 ? 0 : 1;
}
