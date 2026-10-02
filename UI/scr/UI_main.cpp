#include "../includes/UI_main.h"
#include "../includes/UI_types.h"
#include <iostream>

UI_main::UI_main()
    : window(sf::VideoMode({ 1600, 900 }), "Energy Crisis"),
      currentState(UIState::MAIN_MENU),
      isFullscreen(false) {
    window.setFramerateLimit(60);
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

            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::F11 ||
                    (key->code == sf::Keyboard::Key::Enter && key->alt)) {
                    toggleFullscreen();
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
                currentState = UIState::PLAYING;
            } else if (mainMenu.isQuitRequested()) {
                currentState = UIState::QUIT;
                window.close();
                break;
            }
        } else if (currentState == UIState::PLAYING) {
            if (map.isMenuRequested()) {
                map.resetMenuRequest();
                currentState = UIState::MAIN_MENU;
            }
            if (map.isFullscreenRequested()) {
                map.resetFullscreenRequest();
                toggleFullscreen();
            }
        }

        window.setView(gameView);
        window.clear(sf::Color(10, 14, 22));

        if (currentState == UIState::MAIN_MENU) {
            mainMenu.render(window);
        } else if (currentState == UIState::PLAYING) {
            map.render(window);
        }

        window.display();
    }
}
