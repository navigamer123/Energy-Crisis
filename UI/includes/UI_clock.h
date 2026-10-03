#ifndef UI_CLOCK_H
#define UI_CLOCK_H

#include <SFML/Graphics.hpp>
#include <string>
#include "../../Game/includes/game_weather.h"
#include "../../Game/includes/game_balance.h"

class UI_clock {
private:
    int playerIndex;
    int currentDay;
    float currentHour; // 0.0f to 24.0f
    WeatherType weather;
    SeasonType season;
    // [b-options] match rules shown on the clock (MatchRules via UI_map::syncMatchOptionsUI)
    int dayLimit = Balance::FINAL_DAY;          // 0 = endless
    int graceDays = Balance::GRACE_PERIOD_DAYS;
    sf::Clock pulseClock;                       // last-days pulse

public:
    UI_clock();
    UI_clock(int playerIdx);

    void setPlayerIndex(int idx) { playerIndex = idx; }
    void advanceTime(float hours = 3.0f);

    void setHour(float h) { currentHour = h; }
    void setDay(int d) { currentDay = d; }
    void setWeather(WeatherType w) { weather = w; }
    void setSeason(SeasonType s) { season = s; }
    void setRules(int finalDay, int grace) { dayLimit = finalDay; graceDays = grace; } // [b-options]

    int getCurrentDay() const { return currentDay; }
    float getHour24() const { return currentHour; }
    bool isDaylight() const { return Balance::isDaylightAt(currentHour, season); }
    float getSunriseHour() const { return Balance::getSunriseHour(season); }
    float getSunsetHour() const { return Balance::getSunsetHour(season); }
    bool isGracePeriod() const { return currentDay <= graceDays; } // [b-options]
    WeatherType getWeather() const { return weather; }
    SeasonType getSeason() const { return season; }

    void draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
              sf::Vector2f pos, sf::Vector2f size, sf::Color accentColor);
};

#endif // UI_CLOCK_H
