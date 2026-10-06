#include "../includes/UI_clock.h"
#include "../includes/UI_text.h"
#include "../includes/UI_shot.h"
#include "../includes/UI_theme.h"
#include <cstdio>
#include "../includes/UI_types.h"
#include "../includes/UI_infoText.h" // team info (UX-03)
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

#include "../includes/UI_settings.h"

namespace {

// Bulgarian names; a sunny night is "Ясно"
const char* weatherNameBg(WeatherType w, bool daylight) {
    switch (w) {
        case WeatherType::SUNNY:  return daylight ? "Слънчево" : "Ясно";
        case WeatherType::WINDY:  return "Ветровито";
        case WeatherType::RAINY:  return "Дъждовно";
        case WeatherType::STORMY: return "Буря";
        case WeatherType::SNOWY:  return "Снежно";
        case WeatherType::CLOUDY: return "Облачно";
    }
    return "Слънчево";
}

const char* weatherNameEn(WeatherType w, bool daylight) {
    switch (w) {
        case WeatherType::SUNNY:  return daylight ? "Sunny" : "Clear";
        case WeatherType::WINDY:  return "Windy";
        case WeatherType::RAINY:  return "Rainy";
        case WeatherType::STORMY: return "Storm";
        case WeatherType::SNOWY:  return "Snowy";
        case WeatherType::CLOUDY: return "Cloudy";
    }
    return "Sunny";
}

const char* seasonNameBg(SeasonType s) {
    switch (s) {
        case SeasonType::SPRING: return "Пролет";
        case SeasonType::SUMMER: return "Лято";
        case SeasonType::AUTUMN: return "Есен";
        case SeasonType::WINTER: return "Зима";
    }
    return "Пролет";
}

const char* seasonNameEn(SeasonType s) {
    switch (s) {
        case SeasonType::SPRING: return "Spring";
        case SeasonType::SUMMER: return "Summer";
        case SeasonType::AUTUMN: return "Autumn";
        case SeasonType::WINTER: return "Winter";
    }
    return "Spring";
}

// Weather colours never use a player colour (both cards show weather)
sf::Color weatherColor(WeatherType w, bool daylight) {
    switch (w) {
        case WeatherType::SUNNY:  return daylight ? theme::Energy : theme::TextPrimary;
        case WeatherType::WINDY:  return theme::TextPrimary;
        case WeatherType::RAINY:  return theme::Info;
        case WeatherType::STORMY: return theme::Warn;
        case WeatherType::SNOWY:  return theme::TextPrimary;
        case WeatherType::CLOUDY: return theme::TextSecondary;
    }
    return theme::TextPrimary;
}

// Drawn "fast forward" mark (two triangles); the font has no U+23E9 glyph
void drawFastForward(sf::RenderWindow& window, sf::Vector2f pos, float h, sf::Color color) {
    for (int i = 0; i < 2; ++i) {
        sf::ConvexShape tri(3);
        float x = pos.x + i * h * 0.55f;
        tri.setPoint(0, { x, pos.y });
        tri.setPoint(1, { x + h * 0.6f, pos.y + h / 2.0f });
        tri.setPoint(2, { x, pos.y + h });
        tri.setFillColor(color);
        window.draw(tri);
    }
}

} // namespace

void UI_clock::draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                    sf::Vector2f pos, sf::Vector2f size, sf::Color accentColor) {
    // Background card with the player's outline
    sf::RectangleShape card(size);
    card.setPosition(pos);
    card.setFillColor(theme::withAlpha(theme::Panel, 250));
    card.setOutlineThickness(2.0f);
    card.setOutlineColor(accentColor);
    window.draw(card);
    ui::lint::solid(sf::FloatRect(pos, size));
    ui::lint::ContainerScope cardScope(sf::FloatRect(pos, size));

    // Celestial Sun/Moon dial
    bool daylight = isDaylight();
    sf::CircleShape celestial(9.0f);
    celestial.setOrigin({ 9.0f, 9.0f });
    celestial.setPosition({ pos.x + size.x - 22.0f, pos.y + 20.0f });
    if (daylight) {
        celestial.setFillColor(theme::Energy);
        celestial.setOutlineThickness(2.0f);
        celestial.setOutlineColor(sf::Color(255, 245, 180));
    } else {
        celestial.setFillColor(sf::Color(170, 205, 255));
        celestial.setOutlineThickness(2.0f);
        celestial.setOutlineColor(sf::Color(220, 240, 255));
    }
    window.draw(celestial);
    ui::lint::icon(celestial.getGlobalBounds());

    if (!fontLoaded) return;

    const bool grace = currentDay <= graceDays;
    const float textX = pos.x + 10.0f;

    // Header: player and day (ДЕН N/FINAL_DAY). After the final day ends the engine is already on
    // the next day; never show e.g. 21/20.
    bool isEn = (UI_settings::get().getLanguage() == "en");
    const int shownDay = std::min(currentDay, finalDay);
    std::string pTitle = isEn ? ((playerIndex == 1) ? "PLAYER 1" : "PLAYER 2")
                              : ((playerIndex == 1) ? "ИГРАЧ 1" : "ИГРАЧ 2");
    std::string dayStr = pTitle + (isEn ? " · DAY " : " · ДЕН ") + std::to_string(shownDay) + "/" + std::to_string(finalDay);
    // Keep the title clear of the sun/moon dial on the right edge of the card
    const float maxTitleW = size.x - 20.0f - 26.0f;
    unsigned int titleSize = ui::fitTextSize(font, dayStr, fontsize::Label, fontsize::Caption, maxTitleW, true);
    sf::Text& tTitle = ui::pooledText(font, toUtf8(dayStr), titleSize);
    tTitle.setStyle(sf::Text::Bold);
    tTitle.setFillColor(accentColor);
    tTitle.setPosition({ textX, pos.y + 6.0f });
    ui::drawText(window, tTitle);

    // Divider under the title; it stops before the dial instead of crossing it
    sf::RectangleShape div({ size.x - 20.0f - 26.0f, 1.0f });
    div.setPosition({ textX, pos.y + 25.0f });
    div.setFillColor(theme::Line);
    window.draw(div);

    // Weather (colour by weather), season and day / night
    std::string wStr = isEn ? ("Weather: " + std::string(weatherNameEn(weather, daylight)))
                            : ("Време: " + std::string(weatherNameBg(weather, daylight)));
    sf::Text& tWeather = ui::pooledText(font, toUtf8(wStr), fontsize::Label);
    tWeather.setFillColor(weatherColor(weather, daylight));
    tWeather.setPosition({ textX, pos.y + 29.0f });
    ui::drawText(window, tWeather);

    std::string sStr = isEn ? ("Season: " + std::string(seasonNameEn(season)) + (daylight ? " · Day" : " · Night"))
                            : ("Сезон: " + std::string(seasonNameBg(season)) + (daylight ? " · ден" : " · нощ"));
    sf::Text& tSeason = ui::pooledText(font, toUtf8(sStr), fontsize::Label);
    tSeason.setFillColor(theme::TextSecondary);
    tSeason.setPosition({ textX, pos.y + 45.0f });
    ui::drawText(window, tSeason);

    // 24-hour clock; while the mining speed-up runs: a fast-forward mark and the factor
    std::string hStr = (isEn ? "Time: " : "Час: ") + Balance::formatHourMinute(currentHour);
    sf::Text& tHour = ui::pooledText(font, toUtf8(hStr), fontsize::Body);
    tHour.setStyle(sf::Text::Bold);
    tHour.setFillColor(daylight ? theme::TextPrimary : theme::Info);
    tHour.setPosition({ textX, pos.y + 60.0f });
    ui::drawText(window, tHour);

    if (timeScale > 1.5f) {
        char buf[24];
        std::snprintf(buf, sizeof(buf), isEn ? "%dx speed" : "%dx добив", static_cast<int>(std::lround(timeScale)));
        sf::Text& tFast = ui::pooledText(font, toUtf8(buf), fontsize::Label);
        tFast.setStyle(sf::Text::Bold);
        tFast.setFillColor(theme::Warn);
        sf::FloatRect fb = tFast.getLocalBounds();
        float fx = pos.x + size.x - 10.0f - fb.size.x - fb.position.x;
        tFast.setPosition({ fx, pos.y + 62.0f });
        drawFastForward(window, { fx - 20.0f, pos.y + 65.0f }, 10.0f, theme::Warn);
        ui::drawText(window, tFast);
    }

    // Adaptive sun schedule (sunrise - sunset), grace period note
    std::string sLine = (isEn ? "Sun: " : "Слънце: ") + Balance::formatHourMinute(Balance::getSunriseHour(season)) + " - " +
                        Balance::formatHourMinute(Balance::getSunsetHour(season));
    if (grace) sLine += isEn ? " · grace" : " · гратис";
    sf::Text& tSun = ui::pooledText(font, toUtf8(sLine), fontsize::Caption);
    tSun.setFillColor(grace ? theme::Good : theme::TextSecondary);
    tSun.setPosition({ textX, pos.y + 81.0f });
    ui::drawText(window, tSun);
}
