#include "../includes/UI_clock.h"
#include "../includes/UI_types.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

UI_clock::UI_clock()
    : playerIndex(1),
      currentDay(1),
      currentHour(8.0f),
      weather(WeatherType::SUNNY),
      season(SeasonType::SPRING) {
}

UI_clock::UI_clock(int playerIdx)
    : playerIndex(playerIdx),
      currentDay(1),
      currentHour(8.0f),
      weather(playerIdx == 1 ? WeatherType::SUNNY : WeatherType::WINDY),
      season(SeasonType::SPRING) {
}

void UI_clock::advanceTime(float hours) {
    currentHour += hours;
    while (currentHour >= 24.0f) {
        currentHour -= 24.0f;
        currentDay++;
        int sIdx = ((currentDay - 1) / 5) % 4;
        season = static_cast<SeasonType>(sIdx);
    }

    int wHash = (static_cast<int>(currentHour) * 3 + currentDay * 7 + playerIndex * 11) % 100;
    if (wHash < 35) {
        weather = WeatherType::SUNNY;
    } else if (wHash < 65) {
        weather = WeatherType::WINDY;
    } else if (wHash < 85) {
        weather = WeatherType::RAINY;
    } else {
        weather = WeatherType::STORMY;
    }
}

void UI_clock::draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                    sf::Vector2f pos, sf::Vector2f size, sf::Color accentColor) {
    // Background card with glowing outline
    sf::RectangleShape card(size);
    card.setPosition(pos);
    card.setFillColor(sf::Color(16, 22, 34, 250));
    card.setOutlineThickness(2.0f);
    card.setOutlineColor(accentColor);
    window.draw(card);

    // Format 12h/24h AM/PM
    int hour = static_cast<int>(currentHour);
    int minute = static_cast<int>(std::fmod(currentHour * 60.0f, 60.0f));
    bool isPM = (hour >= 12);
    int hour12 = hour % 12;
    if (hour12 == 0) hour12 = 12;

    std::ostringstream oss;
    oss << (hour12 < 10 ? "0" : "") << hour12 << ":" << (minute < 10 ? "0" : "") << minute << (isPM ? " PM" : " AM");
    std::string timeStr = oss.str();

    // Celestial Sun/Moon dial
    bool daylight = isDaylight();
    sf::CircleShape celestial(9.0f);
    celestial.setOrigin({ 9.0f, 9.0f });
    celestial.setPosition({ pos.x + size.x - 22.0f, pos.y + 20.0f });
    if (daylight) {
        celestial.setFillColor(sf::Color(255, 215, 0));
        celestial.setOutlineThickness(2.0f);
        celestial.setOutlineColor(sf::Color(255, 245, 180));
    } else {
        celestial.setFillColor(sf::Color(170, 205, 255));
        celestial.setOutlineThickness(2.0f);
        celestial.setOutlineColor(sf::Color(220, 240, 255));
    }
    window.draw(celestial);

    if (fontLoaded) {
        // Header: Player Name & Day (ДЕН N/FINAL_DAY). The grace period is shown by the green
        // title colour and the "(0 MW Гратис)" sun line below (a "[ГРАТИС]" tag no longer fits).
        // After the final day ends the engine is already on the next day; never show e.g. 21/20.
        const int finalDay = static_cast<int>(Balance::FINAL_DAY);
        const int shownDay = std::min(currentDay, finalDay);
        std::string pTitle = (playerIndex == 1) ? "ИГРАЧ 1 (ЗАПАД)" : "ИГРАЧ 2 (ИЗТОК)";
        std::string dayStr = pTitle + " | ДЕН " + std::to_string(shownDay) + "/" + std::to_string(finalDay);
        unsigned int titleSize = 13;
        sf::Text tTitle(font, toUtf8(dayStr), titleSize);
        // Keep the title clear of the sun/moon dial on the right edge of the card
        const float maxTitleW = size.x - 20.0f - 26.0f;
        while (titleSize > 10 && tTitle.getLocalBounds().size.x > maxTitleW) {
            --titleSize;
            tTitle.setCharacterSize(titleSize);
        }
        tTitle.setFillColor(currentDay <= Balance::GRACE_PERIOD_DAYS ? sf::Color(90, 255, 190) : accentColor);
        tTitle.setPosition({ pos.x + 10.0f, pos.y + 5.0f });
        window.draw(tTitle);

        // Divider
        sf::RectangleShape div({ size.x - 20.0f, 1.5f });
        div.setPosition({ pos.x + 10.0f, pos.y + 24.0f });
        div.setFillColor(sf::Color(70, 95, 130));
        window.draw(div);

        // Line 1: Weather (Време) - font size 12
        std::string wStr = "Време: " + std::string(getWeatherName(weather));
        sf::Text tWeather(font, toUtf8(wStr), 12);
        tWeather.setFillColor(weather == WeatherType::SUNNY ? sf::Color(255, 225, 110) :
                             (weather == WeatherType::WINDY ? sf::Color(130, 245, 255) :
                             (weather == WeatherType::RAINY ? sf::Color(150, 190, 255) :
                             (weather == WeatherType::SNOWY ? sf::Color(220, 235, 255) :
                             (weather == WeatherType::CLOUDY ? sf::Color(180, 185, 200) : sf::Color(255, 160, 140))))));
        tWeather.setPosition({ pos.x + 10.0f, pos.y + 27.0f });
        window.draw(tWeather);

        // Line 2: Season (Сезон) - font size 12
        std::string sStr = "Сезон: " + std::string(getSeasonName(season)) + (daylight ? " [ДЕН]" : " [НОЩ]");
        sf::Text tSeason(font, toUtf8(sStr), 12);
        tSeason.setFillColor(season == SeasonType::SPRING ? sf::Color(140, 255, 160) :
                            (season == SeasonType::SUMMER ? sf::Color(255, 235, 120) :
                            (season == SeasonType::AUTUMN ? sf::Color(255, 185, 110) : sf::Color(210, 235, 255))));
        tSeason.setPosition({ pos.x + 10.0f, pos.y + 44.0f });
        window.draw(tSeason);

        // Line 3: Hour (Час) - font size 13
        std::string hStr = "Час: " + timeStr;
        sf::Text tHour(font, toUtf8(hStr), 13);
        tHour.setFillColor(daylight ? sf::Color(255, 255, 255) : sf::Color(190, 220, 255));
        tHour.setPosition({ pos.x + 10.0f, pos.y + 61.0f });
        window.draw(tHour);

        // Line 4: Adaptive Sun Schedule (Sunrise & Sunset) - font size 11
        std::string riseStr = Balance::formatHourMinute(Balance::getSunriseHour(season));
        std::string setStr = Balance::formatHourMinute(Balance::getSunsetHour(season));
        std::string sLine = "Слънце: " + riseStr + " - " + setStr;
        if (currentDay <= Balance::GRACE_PERIOD_DAYS) {
            sLine += " (0 MW Гратис)";
        }
        sf::Text tSun(font, toUtf8(sLine), 11);
        tSun.setFillColor(currentDay <= Balance::GRACE_PERIOD_DAYS ? sf::Color(90, 255, 190) : sf::Color(255, 215, 120));
        tSun.setPosition({ pos.x + 10.0f, pos.y + 79.0f });
        window.draw(tSun);
    }
}
