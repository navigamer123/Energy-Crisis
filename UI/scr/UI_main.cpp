#include "../includes/UI_main.h"
#include <iostream>

UI_main::UI_main()
    : window(sf::VideoMode({ 1600, 900 }), "Energy Crisis"),
      currentState(UIState::MAIN_MENU) {
    window.setFramerateLimit(60);
    std::cout << "[UI_main] SFML RenderWindow (1600x900) initialized.\n";
}

UI_main::~UI_main() {
    if (window.isOpen()) {
        window.close();
    }
    std::cout << "[UI_main] SFML window closed.\n";
}

void UI_main::render() {
    while (window.isOpen() && currentState != UIState::QUIT) {
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Escape || key->code == sf::Keyboard::Key::M) {
                    if (currentState == UIState::PLAYING) {
                        currentState = UIState::MAIN_MENU;
                    }
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
        }

        window.clear(sf::Color(15, 20, 28));

        if (currentState == UIState::MAIN_MENU) {
            mainMenu.render(window);
        } else if (currentState == UIState::PLAYING) {
            map.render(window);
        }

        window.display();
    }
}
