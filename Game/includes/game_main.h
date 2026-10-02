#pragma once

#include <SFML/Graphics.hpp>
#include <string>

struct PlayerData
{
    int money = 0;
    int iron = 0;
    int coal = 0;
    int gold = 0;
    int copper = 0;
    int silver = 0;
    int silicon = 0;
    int wood = 0;
    int sticks = 0;

    std::string weather;
    std::string wind_speed;
};

class Game
{
public:
    Game();
    void run();

private:
    void handleEvents();
    void update();
    void render();

    sf::RenderWindow window;
    sf::Clock clock;

    unsigned int w_window;
    unsigned int h_window;

    int day_night_cycle = 3600;
    bool new_weather = false;
    std::string time_of_day = "day";

    PlayerData player1;
    PlayerData player2;
};