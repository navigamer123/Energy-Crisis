#include "../includes/UI_main.h"
#include "../includes/UI_types.h"
#include <iostream>

UI_main::UI_main()
    : window(sf::VideoMode({ 1600, 900 }), "Energy Crisis"),
      currentState(UIState::MAIN_MENU),
      isFullscreen(false) {
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false); // A held key must not re-trigger menu/pause/hotkey events
    updateViewport();
    std::cout << "[UI_main] SFML RenderWindow (1600x900 virtual canvas) initialized.\n";
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

void UI_main::render() {
    while (window.isOpen() && currentState != UIState::QUIT) {
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                (void)resized;
                updateViewport();
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

            // [Team Demo / HX-02] F9 in the main menu starts the judge demo; inside it any key goes back
            if (currentState == UIState::MAIN_MENU) {
                const auto* demoKey = event->getIf<sf::Event::KeyPressed>();
                if (demoKey && demoKey->code == sf::Keyboard::Key::F9) {
                    startDemo();
                    continue;
                }
            } else if (currentState == UIState::DEMO) {
                demo.handleEvent(*event);
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
        } else if (currentState == UIState::DEMO) {
            // [Team Demo / HX-02] Judge demo: step the script before the map draws, leave on any key
            if (demo.isExitRequested()) {
                demo.stop(map);
                mainMenu.returnToMain();
                currentState = UIState::MAIN_MENU;
            } else {
                demo.update(map);
            }
        }

        window.setView(gameView);
        window.clear(sf::Color(10, 14, 22));

        if (currentState == UIState::MAIN_MENU) {
            mainMenu.render(window);
            demo.drawMenuHint(window, map); // [Team Demo / HX-02] "[F9] ДЕМО ЗА ЖУРИТО"
        } else if (currentState == UIState::PLAYING) {
            map.render(window);
        } else if (currentState == UIState::DEMO) {
            map.render(window);              // [Team Demo / HX-02]
            demo.drawOverlay(window, map);
        }

        window.display();
    }
}

// [Team Demo / HX-02] Judge demo: a seeded, scripted match on the real map (UI/scr/UI_demo.cpp)
void UI_main::startDemo() {
    demo.start(map);
    currentState = UIState::DEMO;
}
