#include "../includes/UI_main.h"
#include "../includes/UI_types.h"
#include "../includes/UI_text.h"
#include "../includes/UI_theme.h"
#include "../includes/UI_settings.h"
#include "../includes/UI_arcadePopup.h"
#include "../includes/UI_credits.h"
#include "../includes/UI_arcadeMode.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include "../includes/UI_audio.h" // [b-effects]
#include <iostream>

UI_main::UI_main(const ShotOptions& shotOptions)
#if defined(__ANDROID__)
    : window(sf::VideoMode::getDesktopMode(), "Energy Crisis", sf::State::Fullscreen),
      currentState(UIState::MAIN_MENU),
      isFullscreen(true),
      shot(shotOptions) {
#else
    : window(sf::VideoMode({ 1600, 900 }), "Energy Crisis"),
      currentState(UIState::MAIN_MENU),
      isFullscreen(false),
      shot(shotOptions) {
#endif
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false); // A held key must not re-trigger menu/pause/hotkey events
    updateViewport();
    if (!shot.enabled) UI_audio::get().init(); // [b-effects] synthesised sound and music (screenshots stay silent)
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
        else if (shot.scene == "remap" || shot.scene == "controls_remap") s = MenuState::SETTINGS_CONTROLS;
        else if (shot.scene == "arcade_start") s = MenuState::PRESS_A_TO_START;
        else if (shot.scene == "calibrate_blue") s = MenuState::CALIBRATE_BLUE;
        else if (shot.scene == "calibrate_red") s = MenuState::CALIBRATE_RED;
        else if (shot.scene == "popup_exit") {
            s = MenuState::PRESS_A_TO_START;
            bool isEn = (UI_settings::get().getLanguage() == "en");
            ArcadePopup::get().show(isEn ? "PRESS AGAIN\nTO EXIT" : "НАТИСНИ ОТНОВО\nДА ИЗЛЕЗЕШ", 10.0f);
        }
        mainMenu.showState(s);
        currentState = UIState::MAIN_MENU;
        return;
    }

    // Game scenes: a single-player match against the bot, then the scene's state on top
    map.restartMatch();
    map.setControlScheme(ControlScheme::BOTH_KEYBOARD);
    map.setBotDifficulty(BotDifficulty::MEDIUM);
    map.resetMatchInputState();
    // A recording shows the scene's one-off moment (the storm bolt) at 60% of the clip, not at its end
    map.setupDebugScene(shot.scene, shot.recordDir.empty() ? shot.frames : (shot.frames * 3) / 5);
    currentState = UIState::PLAYING;
}

bool UI_main::saveRecordFrame(int index) {
    sf::Texture capture;
    if (!capture.resize(window.getSize())) return false;
    capture.update(window);
    char name[32];
    std::snprintf(name, sizeof(name), "frame_%05d.png", index);
    return capture.copyToImage().saveToFile((std::filesystem::path(shot.recordDir) / name).string());
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
    UI_audio::get().shutdown(); // [b-effects] release audio before SFML tears down
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
    isFullscreen = !isFullscreen;
    if (isFullscreen) {
        auto mode = sf::VideoMode::getDesktopMode();
        window.create(mode, "Energy Crisis", sf::State::Fullscreen);
    } else {
        window.create(sf::VideoMode({ 1600, 900 }), "Energy Crisis", sf::Style::Default);
    }
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false); // window.create() restores the default (repeat on)
    updateViewport();
    std::cout << "[UI_main] Fullscreen toggled: " << (isFullscreen ? "ENABLED" : "DISABLED")
              << " (" << window.getSize().x << "x" << window.getSize().y << ")\n";
}

int UI_main::render() {
    if (shot.enabled) setupShotScene();
    int frame = 0;
    int exitCode = 0;
    int recordErrors = 0;
    if (!shot.recordDir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(shot.recordDir, ec);
    }
    sf::Clock audioClock; // [b-effects] frame time for the audio crossfades
    sf::Clock changeGameClock;
    int changeGamePressCount = 0;
    sf::Clock arcadeUpdateClock;
    bool matchWasCompleted = false;

    while (window.isOpen() && currentState != UIState::QUIT) {
        ui::beginTextFrame();
        ui::lint::beginFrame();
        ui::shot::tickFrame();

        float dt = arcadeUpdateClock.restart().asSeconds();
        ArcadePopup::get().update(dt);
        if (ArcadeMode::isEnabled()) {
            CreditsManager::get().update(dt);

            if (currentState == UIState::PLAYING) {
                if (map.getEngine().getCityState().winner != 0 && !matchWasCompleted) {
                    matchWasCompleted = true;
                    CreditsManager::get().onMatchCompleted();
                    std::cout << "[Credits] Match completed. Total completed matches: "
                              << CreditsManager::get().getMatchesCompletedCount() << "\n";
                }
            }
        }

        while (const auto event = window.pollEvent()) {
            // Global controller exit: BTN_BASE3 / Button 8 or 9 pressed 2 times within 2 seconds
            if (const auto* jb = event->getIf<sf::Event::JoystickButtonPressed>()) {
                if (jb->button == 8 || jb->button == 9) {
                    float elapsed = changeGameClock.getElapsedTime().asSeconds();
                    if (changeGamePressCount >= 1 && elapsed <= 2.0f) {
                        std::cout << "[UI_main] Controller exit confirmed (2x within 2s). Exiting game...\n";
                        currentState = UIState::QUIT;
                        window.close();
                        break;
                    } else {
                        changeGamePressCount = 1;
                        changeGameClock.restart();
                        std::cout << "[UI_main] Controller exit button pressed (1/2). Press again within 2s to exit.\n";
                        bool isEn = (UI_settings::get().getLanguage() == "en");
                        std::string msg = isEn ? "PRESS AGAIN\nTO EXIT" : "НАТИСНИ ОТНОВО\nДА ИЗЛЕЗЕШ";
                        ArcadePopup::get().show(msg, 2.0f);
                    }
                    continue;
                }
            }

            // [b-effects] Menu navigation clicks
            if (currentState == UIState::MAIN_MENU && !shot.enabled &&
                (event->is<sf::Event::KeyPressed>() || event->is<sf::Event::MouseButtonPressed>() || event->is<sf::Event::JoystickButtonPressed>())) {
                UI_audio::get().play(AudioSynth::Sfx::UiClick);
            }
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                (void)resized;
                updateViewport();
            }

            if (shot.enabled) continue; // Screenshot mode: no player input, no focus auto-pause

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

            // Touch input support for mobile / touchscreens
            if (const auto* touchBegan = event->getIf<sf::Event::TouchBegan>()) {
                if (currentState == UIState::MAIN_MENU && !shot.enabled) {
                    UI_audio::get().play(AudioSynth::Sfx::UiClick);
                }
                // Finger 0 is mapped to Left Click; Finger 1 (two-finger tap) maps to Right Click (Cancel)
                sf::Mouse::Button btn = (touchBegan->finger == 0) ? sf::Mouse::Button::Left : sf::Mouse::Button::Right;
                sf::Event mouseEv = sf::Event::MouseButtonPressed{ btn, touchBegan->position };
                if (currentState == UIState::MAIN_MENU) {
                    mainMenu.handleEvent(mouseEv, window);
                } else if (currentState == UIState::PLAYING) {
                    map.handleEvent(mouseEv, window);
                }
                continue;
            }

            if (const auto* touchMoved = event->getIf<sf::Event::TouchMoved>()) {
                if (touchMoved->finger == 0) {
                    sf::Event mouseEv = sf::Event::MouseMoved{ touchMoved->position };
                    if (currentState == UIState::MAIN_MENU) {
                        mainMenu.handleEvent(mouseEv, window);
                    } else if (currentState == UIState::PLAYING) {
                        map.handleEvent(mouseEv, window);
                    }
                }
                continue;
            }

            if (const auto* touchEnded = event->getIf<sf::Event::TouchEnded>()) {
                sf::Mouse::Button btn = (touchEnded->finger == 0) ? sf::Mouse::Button::Left : sf::Mouse::Button::Right;
                sf::Event mouseEv = sf::Event::MouseButtonReleased{ btn, touchEnded->position };
                if (currentState == UIState::MAIN_MENU) {
                    mainMenu.handleEvent(mouseEv, window);
                } else if (currentState == UIState::PLAYING) {
                    map.handleEvent(mouseEv, window);
                }
                continue;
            }

            if (currentState == UIState::MAIN_MENU) {
                mainMenu.handleEvent(*event, window);
            } else if (currentState == UIState::PLAYING) {
                map.handleEvent(*event, window);
            }
        }

        if (currentState == UIState::MAIN_MENU) {
            if (mainMenu.isPlayRequested()) {
                if (ArcadeMode::isEnabled() && CreditsManager::get().requiresCreditForNewGame()) {
                    if (!CreditsManager::get().tryConsumeCredits(1)) {
                        mainMenu.resetPlayRequest();
                    } else {
                        mainMenu.resetPlayRequest();
                        matchWasCompleted = false;
                        map.restartMatch();
                        map.setControlScheme(mainMenu.getSelectedControlScheme());
                        map.setBotDifficulty(mainMenu.getSelectedBotDifficulty());
                        map.resetMatchInputState();
                        UI_audio::get().play(AudioSynth::Sfx::UiConfirm);
                        currentState = UIState::PLAYING;
                    }
                } else {
                    mainMenu.resetPlayRequest();
                    matchWasCompleted = false;
                    map.restartMatch();
                    map.setControlScheme(mainMenu.getSelectedControlScheme());
                    map.setBotDifficulty(mainMenu.getSelectedBotDifficulty());
                    map.resetMatchInputState();
                    UI_audio::get().play(AudioSynth::Sfx::UiConfirm);
                    currentState = UIState::PLAYING;
                }
            } else if (mainMenu.isQuitRequested()) {
                currentState = UIState::QUIT;
                window.close();
                break;
            }
        } else if (currentState == UIState::PLAYING) {
            if (map.isMenuRequested()) {
                map.resetMenuRequest();
                mainMenu.returnToMain(); // Show the top-level menu, not the last submenu
                currentState = UIState::MAIN_MENU;
            }
            if (map.isFullscreenRequested()) {
                map.resetFullscreenRequest();
                toggleFullscreen();
                map.primeInputEdges();
            }
        }

        // [b-effects] Audio: apply the Settings values and crossfade day/night music
        {
            UI_audio& audio = UI_audio::get();
            audio.setMasterVolume(mainMenu.getVolume());
            audio.setSfxEnabled(mainMenu.isSoundEffectsEnabled());
            UI_audio::Scene scene = UI_audio::Scene::Menu;
            if (currentState == UIState::PLAYING) {
                if (map.getEngine().getCityState().winner != 0) scene = UI_audio::Scene::Silent;
                else if (map.isMatchPaused()) scene = UI_audio::Scene::Paused;
                else scene = UI_audio::Scene::Match;
            }
            audio.update(audioClock.restart().asSeconds(), scene, map.getNightAmount());
        }

        window.setView(gameView);
        window.clear(theme::Window);

        if (currentState == UIState::MAIN_MENU) {
            mainMenu.render(window);
        } else if (currentState == UIState::PLAYING) {
            map.render(window);
        }

        // Top-right arcade popup notification (credits, exit confirmation)
        ArcadePopup::get().draw(window);

        // Screenshot mode: capture the finished frame before display() (the back buffer is still valid)
        if (shot.enabled) ++frame;
        if (!shot.recordDir.empty() && frame % shot.recordEvery == 0 && !saveRecordFrame(frame / shot.recordEvery)) {
            std::cerr << "[Shot] ERROR: could not write frame " << frame << " to " << shot.recordDir << "\n";
            recordErrors++;
        }
        if (shot.enabled && frame >= shot.frames) {
            exitCode = std::max(finishShot(), recordErrors > 0 ? 1 : 0);
            if (!shot.recordDir.empty()) {
                std::cout << "[Shot] Recorded " << frame / shot.recordEvery << " frames of scene " << shot.scene
                          << " to " << shot.recordDir << "\n";
            }
            window.close();
            break;
        }

        window.display();
    }
    return exitCode;
}
