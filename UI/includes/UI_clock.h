#ifndef UI_CLOCK_H
#define UI_CLOCK_H

#include <SFML/Graphics.hpp>
#include <string>
#include "../../Game/includes/game_weather.h"

class UI_clock {
private:
    int playerIndex;
    int currentDay;
    float currentHour; // 0.0f to 24.0f
    WeatherType weather;
    SeasonType season;

public:
    UI_clock();
    UI_clock(int playerIdx);

    void setPlayerIndex(int idx) { playerIndex = idx; }
    void advanceTime(float hours = 3.0f);

    void setHour(float h) { currentHour = h; }
    void setDay(int d) { currentDay = d; }
    void setWeather(WeatherType w) { weather = w; }
    void setSeason(SeasonType s) { season = s; }

    int getCurrentDay() const { return currentDay; }
    float getHour24() const { return currentHour; }
    bool isDaylight() const { return currentHour >= 6.0f && currentHour < 18.0f; }
    WeatherType getWeather() const { return weather; }
    SeasonType getSeason() const { return season; }

    void draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
              sf::Vector2f pos, sf::Vector2f size, sf::Color accentColor);
};

#endif // UI_CLOCK_H
