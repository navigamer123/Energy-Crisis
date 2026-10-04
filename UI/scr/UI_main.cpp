#include "../includes/UI_main.h"
#include "../includes/UI_types.h"
#include "../includes/UI_text.h"
#include "../includes/UI_theme.h"
#include <algorithm>
#include <iostream>

UI_main::UI_main(const ShotOptions& shotOptions)
    : window(sf::VideoMode({ 1600, 900 }), "Energy Crisis"),
      currentState(UIState::MAIN_MENU),
      isFullscreen(false),
      shot(shotOptions) {
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false); // A held key must not re-trigger menu/pause/hotkey events
    updateViewport();
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

    while (window.isOpen() && currentState != UIState::QUIT) {
        ui::lint::beginFrame();
        ui::shot::tickFrame();
        while (const auto event = window.pollEvent()) {
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

            if (currentState == UIState::MAIN_MENU) {
                mainMenu.handleEvent(*event, window);
            } else if (currentState == UIState::PLAYING) {
                map.handleEvent(*event, window);
            }
        }

        if (currentState == UIState::MAIN_MENU) {
            if (mainMenu.isPlayRequested()) {
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
                mainMenu.returnToMain(); // Show the top-level menu, not the last submenu
                currentState = UIState::MAIN_MENU;
            }
            if (map.isFullscreenRequested()) {
                map.resetFullscreenRequest();
                toggleFullscreen();
                map.primeInputEdges();
            }
        }

        window.setView(gameView);
        window.clear(theme::Window);

        if (currentState == UIState::MAIN_MENU) {
            mainMenu.render(window);
        } else if (currentState == UIState::PLAYING) {
            map.render(window);
        }

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
