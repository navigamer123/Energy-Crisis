#include "../includes/UI_main.h"
#include "../includes/UI_types.h"
#include "../includes/UI_text.h"
#include "../includes/UI_theme.h"
#include "../includes/UI_layout.h"     // Team b-session (UX-12): menu zoom
#include "../includes/UI_saveSystem.h" // Team b-session (F-02): continue from disk
#include "../includes/UI_settings.h"   // Team b-session (F-06): persistent settings
#include <algorithm>
#include <iostream>
#include <vector>

UI_main::UI_main(const ShotOptions& shotOptions)
    : currentState(UIState::MAIN_MENU),
      isFullscreen(false),
      shot(shotOptions) {
    // Screenshot mode never reads or writes the player's settings and saves: captures stay reproducible
    if (shot.enabled) disableUserData();
    // Team b-session (F-06): the settings file decides size, fullscreen, frame cap and vsync
    loadGameSettings();
    applyWindowSettings(true); // also disables key repeat (a held key must not re-trigger events)
    refreshContinueInfo();     // F-02: ПРОДЪЛЖИ for the newest save on disk
    std::cout << "[UI_main] SFML RenderWindow (1600x900 virtual canvas) initialized.\n";
}

// Screenshot mode: open the requested scene directly, without any input
void UI_main::setupShotScene() {
    ui::shot::setActive(true);
    ui::lint::setEnabled(shot.lint);
    ui::shot::setSeedEnv(shot.seed); // every match of this run replays the same seed

    if (ui::shot::isMenuScene(shot.scene)) {
        MenuState s = MenuState::MAIN;
        if (shot.scene == "modes") s = MenuState::MODE_SELECT;
        else if (shot.scene == "bots") s = MenuState::BOT_DIFFICULTY;
        else if (shot.scene == "controls") s = MenuState::PLAY_CONTROLS;
        else if (shot.scene == "settings") s = MenuState::SETTINGS;
        mainMenu.showState(s);
        currentState = UIState::MAIN_MENU;
        return;
    }

    // Game scenes: a single-player match against the bot, then the scene's state on top
    map.restartMatch();
    map.setControlScheme(ControlScheme::BOTH_KEYBOARD);
    map.setBotDifficulty(BotDifficulty::MEDIUM);
    map.resetMatchInputState();
    map.setupDebugScene(shot.scene, shot.frames);
    currentState = UIState::PLAYING;
}

int UI_main::finishShot() {
    int exitCode = 0;
    if (!shot.outPath.empty()) {
        sf::Texture capture;
        if (capture.resize(window.getSize())) {
            capture.update(window);
            if (capture.copyToImage().saveToFile(shot.outPath)) {
                std::cout << "[Shot] Saved " << shot.outPath << " (" << window.getSize().x << "x"
                          << window.getSize().y << ", scene " << shot.scene << ", " << shot.frames << " frames)\n";
            } else {
                std::cerr << "[Shot] ERROR: could not write " << shot.outPath << "\n";
                exitCode = 1;
            }
        } else {
            std::cerr << "[Shot] ERROR: could not create the capture texture\n";
            exitCode = 1;
        }
        if (window.getSize() != sf::Vector2u(1600, 900)) {
            std::cerr << "[Shot] Warning: the window is " << window.getSize().x << "x" << window.getSize().y
                      << ", not 1600x900; the capture is scaled.\n";
        }
    }
    if (shot.lint) {
        std::vector<std::string> problems = ui::lint::report();
        for (const auto& line : problems) std::cout << line << "\n";
        std::cout << "[Lint] scene " << shot.scene << ": " << problems.size() << " problem(s)\n";
        exitCode = static_cast<int>(std::min<std::size_t>(problems.size(), 255));
    }
    return exitCode;
}

UI_main::~UI_main() {
    saveGameSettings(); // b-session: keeps a window size changed by dragging
    if (window.isOpen()) {
        window.close();
    }
    std::cout << "[UI_main] SFML window closed.\n";
}

void UI_main::updateViewport() {
    float windowWidth = static_cast<float>(window.getSize().x);
    float windowHeight = static_cast<float>(window.getSize().y);
    float targetAspect = VIRTUAL_WIDTH / VIRTUAL_HEIGHT; // 16:9 = 1.777778
    float windowAspect = windowWidth / windowHeight;

    float vpX = 0.0f;
    float vpY = 0.0f;
    float vpW = 1.0f;
    float vpH = 1.0f;

    if (windowAspect > targetAspect) {
        // Window is wider than 16:9 -> pillarbox (black bars on left and right)
        vpW = targetAspect / windowAspect;
        vpX = (1.0f - vpW) / 2.0f;
    } else {
        // Window is taller than 16:9 (e.g. 1920x1200 is 1.6, 4:3 is 1.33) -> letterbox (top and bottom black bars)
        vpH = windowAspect / targetAspect;
        vpY = (1.0f - vpH) / 2.0f;
    }

    gameView.setSize({ VIRTUAL_WIDTH, VIRTUAL_HEIGHT });
    gameView.setCenter({ VIRTUAL_WIDTH / 2.0f, VIRTUAL_HEIGHT / 2.0f });
    gameView.setViewport(sf::FloatRect({ vpX, vpY }, { vpW, vpH }));
    window.setView(gameView);
}

void UI_main::toggleFullscreen() {
    // Team b-session (F-06): F11 / Alt+Enter / the HUD button change the saved setting too
    gameSettings().fullscreen = !isFullscreen;
    gameSettings().touch();
    saveGameSettings();
    applyWindowSettings(false);
    std::cout << "[UI_main] Fullscreen toggled: " << (isFullscreen ? "ENABLED" : "DISABLED")
              << " (" << window.getSize().x << "x" << window.getSize().y << ")\n";
}

// -----------------------------------------------------------------------------
// Team b-session (F-02, F-05, F-06, UX-12)
// -----------------------------------------------------------------------------
void UI_main::applyWindowSettings(bool recreate) {
    GameSettings& s = gameSettings();
    const sf::VideoMode desk = sf::VideoMode::getDesktopMode();
    const unsigned int w = std::min(static_cast<unsigned int>(s.windowWidth), desk.size.x);
    const unsigned int h = std::min(static_cast<unsigned int>(s.windowHeight), desk.size.y);
    auto centre = [this, &desk, w, h]() {
        window.setPosition({ std::max(0, static_cast<int>(desk.size.x) - static_cast<int>(w)) / 2,
                             std::max(0, static_cast<int>(desk.size.y) - static_cast<int>(h)) / 2 });
    };
    if (recreate || s.fullscreen != isFullscreen || !window.isOpen()) {
        isFullscreen = s.fullscreen;
        if (isFullscreen) {
            window.create(desk, "Energy Crisis", sf::State::Fullscreen);
        } else {
            window.create(sf::VideoMode({ w, h }), "Energy Crisis", sf::Style::Default);
            centre();
        }
        window.setKeyRepeatEnabled(false); // window.create() restores the default (repeat on)
        padBridge.reset();
    } else if (!isFullscreen && (window.getSize().x != w || window.getSize().y != h)) {
        window.setSize({ w, h });
        centre();
    }
    // SFML: use either vsync or a frame cap, never both
    window.setVerticalSyncEnabled(s.vsync);
    window.setFramerateLimit(s.vsync ? 0u : static_cast<unsigned int>(std::max(0, s.fpsLimit)));
    updateViewport();
    appliedRevision = s.revision;
    std::cout << "[UI_main] Window " << window.getSize().x << "x" << window.getSize().y
              << (isFullscreen ? " fullscreen" : " windowed") << ", "
              << (s.vsync ? std::string("vsync") : (s.fpsLimit == 0 ? std::string("no FPS cap") : std::to_string(s.fpsLimit) + " FPS cap"))
              << ", UI scale " << s.uiScalePercent << "%" << (s.projectorMode ? ", projector mode" : "") << "\n";
}

void UI_main::refreshContinueInfo() {
    if (map.hasMatchInProgress()) {
        mainMenu.setContinueInfo(true, true, map.matchSummary());
        return;
    }
    SaveInfo info;
    std::string path = saves::newestSave(&info);
    mainMenu.setContinueInfo(!path.empty(), false, path.empty() ? std::string() : "Запис: " + saves::shortLabel(info));
}

bool UI_main::continueMatch() {
    if (!map.hasMatchInProgress()) {
        // Fresh start of the game: continue from the newest save on disk
        SaveInfo info;
        std::string path = saves::newestSave(&info);
        std::string err;
        if (path.empty() || !map.loadFromFile(path, err)) {
            std::cerr << "[UI_main] Continue failed: " << (path.empty() ? std::string("no save") : err) << "\n";
            refreshContinueInfo();
            return false;
        }
    }
    map.openPauseMenu(); // everybody gets ready first; ПРОДЪЛЖИ / Esc resumes
    return true;
}

sf::View UI_main::currentView() const {
    if (currentState == UIState::MAIN_MENU && !mainMenu.usesOwnZoom())
        return ui::overlayView(gameView, mainMenu.contentBounds(), ui::overlayScale());
    return gameView;
}

GamepadMenuBridge::Mode UI_main::bridgeMode() const {
    if (currentState != UIState::PLAYING) return GamepadMenuBridge::Mode::MainMenu;
    return map.wantsMenuInput() ? GamepadMenuBridge::Mode::MatchMenu : GamepadMenuBridge::Mode::Game;
}

int UI_main::render() {
    if (shot.enabled) setupShotScene();
    int frame = 0;
    int exitCode = 0;
    sf::Clock frameClock; // b-session: gamepad menu auto-repeat
    while (window.isOpen() && currentState != UIState::QUIT) {
        ui::lint::beginFrame();
        ui::shot::tickFrame();
        const float frameDt = frameClock.restart().asSeconds();
        // Team b-session (F-06): a changed setting re-applies the window exactly once
        if (gameSettings().revision != appliedRevision) applyWindowSettings(false);
        // Team b-session (UX-12): menus are drawn AND hit-tested with the UI-scale view
        window.setView(currentView());

        // Team b-session (F-05): real events plus gamepad presses translated into key events
        std::vector<sf::Event> frameEvents;
        while (const auto polled = window.pollEvent()) frameEvents.push_back(*polled);
        std::vector<GamepadMenuBridge::Press> padPresses;
        padBridge.update(frameDt, bridgeMode(), frameEvents, padPresses);

        for (const sf::Event& frameEvent : frameEvents) {
            const sf::Event* event = &frameEvent;
            if (event->is<sf::Event::Closed>()) {
                if (currentState == UIState::PLAYING) map.writeAutosave(); // b-session: continue next time
                window.close();
            }

            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                // b-session: remember a window size changed by dragging (saved on exit)
                if (!isFullscreen && resized->size.x >= 800 && resized->size.y >= 450) {
                    gameSettings().windowWidth = static_cast<int>(resized->size.x);
                    gameSettings().windowHeight = static_cast<int>(resized->size.y);
                }
                updateViewport();
            }

            if (shot.enabled) continue; // Screenshot mode: no player input, no focus auto-pause

            // b-session (F-05): gamepad plugged in / pulled out during a match
            if (currentState == UIState::PLAYING) {
                if (const auto* jc = event->getIf<sf::Event::JoystickConnected>()) map.onGamepadConnection(jc->joystickId, true);
                if (const auto* jd = event->getIf<sf::Event::JoystickDisconnected>()) map.onGamepadConnection(jd->joystickId, false);
            }

            // Auto-pause the match when the window loses focus (Alt-Tab, click elsewhere)
            if (event->is<sf::Event::FocusLost>() && currentState == UIState::PLAYING) {
                map.onFocusLost();
            }

            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::F11 ||
                    (key->code == sf::Keyboard::Key::Enter && key->alt)) {
                    toggleFullscreen();
                    // The Enter of Alt+Enter (still held) must not also fire a player action
                    if (currentState == UIState::PLAYING) map.primeInputEdges();
                    continue;
                }
            }

            if (currentState == UIState::MAIN_MENU) {
                mainMenu.handleEvent(*event, window);
            } else if (currentState == UIState::PLAYING) {
                map.handleEvent(*event, window);
            }
        }
        if (currentState == UIState::PLAYING && !shot.enabled) {
            for (const auto& p : padPresses) map.onGamepadPress(p.joystickId, p.button); // b-session
        }

        if (currentState == UIState::MAIN_MENU) {
            if (mainMenu.isContinueRequested()) {
                // Team b-session (F-02): back into the running match, or the newest save
                mainMenu.resetContinueRequest();
                if (continueMatch()) currentState = UIState::PLAYING;
            } else if (mainMenu.isPlayRequested()) {
                mainMenu.resetPlayRequest();
                map.restartMatch();
                map.setControlScheme(mainMenu.getSelectedControlScheme());
                map.setBotDifficulty(mainMenu.getSelectedBotDifficulty());
                map.resetMatchInputState(); // The Enter/Space/click that started the match must not act in it
                currentState = UIState::PLAYING;
            } else if (mainMenu.isQuitRequested()) {
                currentState = UIState::QUIT;
                window.close();
                break;
            }
        } else if (currentState == UIState::PLAYING) {
            if (map.isMenuRequested()) {
                map.resetMenuRequest();
                map.writeAutosave();     // b-session (F-18): the match survives a restart of the game
                mainMenu.returnToMain(); // Show the top-level menu, not the last submenu
                refreshContinueInfo();   // b-session (F-02): ПРОДЪЛЖИ is preselected
                currentState = UIState::MAIN_MENU;
            }
            if (map.isFullscreenRequested()) {
                map.resetFullscreenRequest();
                toggleFullscreen();
                map.primeInputEdges();
            }
        }

        window.setView(currentView()); // b-session (UX-12): zoomed main menu
        window.clear(theme::Window);

        if (currentState == UIState::MAIN_MENU) {
            mainMenu.render(window);
        } else if (currentState == UIState::PLAYING) {
            map.render(window);
        }

        postfx.apply(window); // b-session (HX-15): projector picture
        // Screenshot mode: capture the finished frame before display() (the back buffer is still valid)
        if (shot.enabled && ++frame >= shot.frames) {
            exitCode = finishShot();
            window.close();
            break;
        }

        window.display();
    }
    return exitCode;
}
