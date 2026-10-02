#include "game_main.h"

Game::Game()
    : window(sf::VideoMode::getDesktopMode(), "Game", sf::Style::Fullscreen)
{
    window.setFramerateLimit(60);

    sf::Vector2u size = window.getSize();
    w_window = size.x;
    h_window = size.y;
}

void Game::run()
{
    while (window.isOpen())
    {
        handleEvents();
        update();
        render();
    }
}

void Game::handleEvents()
{
    sf::Event event;
    while (window.pollEvent(event))
    {
        if (event.type == sf::Event::Closed ||
            (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape))
        {
            window.close();
        }

        if (event.type == sf::Event::MouseButtonPressed &&
            event.mouseButton.button == sf::Mouse::Left)
        {
            int mouse_x = event.mouseButton.x;
            int mouse_y = event.mouseButton.y;
            // handle click at (mouse_x, mouse_y)
        }
    }
}

void Game::update()
{
    if (day_night_cycle == 0 || day_night_cycle == 1800 || day_night_cycle == -1800)
        new_weather = true;

    time_of_day = (day_night_cycle < 0) ? "night" : "day";

    day_night_cycle--;
    if (day_night_cycle < -3600)
        day_night_cycle = 3600;
}

void Game::render()
{
    window.clear(sf::Color(3, 85, 9));
    // draw things here
    window.display();
}