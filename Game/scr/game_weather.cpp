#include "game_weather.h"
#include "game_random.h"   // for randomInt

std::string weather_state = "clear";
bool wind = false;
std::string wind_direction = "none";

std::vector<std::string> weather_report(const std::string& season)
{
    // 0 = cloud, 1 = precipitation, 2 = wind direction, 3 = wind speed
    std::vector<std::string> Weather = {"clear", "clear", "none", "0"};

    int rain_roll         = randomInt(1, 10);
    int wind_roll         = randomInt(1, 10);
    int thunder_storm_roll = randomInt(1, 10);
    int snow_roll         = randomInt(1, 10);
    int hail_roll         = randomInt(1, 10);
    int cloudy_roll       = randomInt(1, 10);

    int cloudy_num, wind_num, rain_num, thunder_storm_num, snow_num, hail_num;

    if (season == "spring")      { cloudy_num = 6; wind_num = 4; rain_num = 4; thunder_storm_num = 8; snow_num = 10; hail_num = 10; }
    else if (season == "summer") { cloudy_num = 9; wind_num = 6; rain_num = 2; thunder_storm_num = 7; snow_num = 10; hail_num = 10; }
    else if (season == "fall")   { cloudy_num = 4; wind_num = 4; rain_num = 2; thunder_storm_num = 7; snow_num = 9;  hail_num = 9;  }
    else if (season == "winter") { cloudy_num = 5; wind_num = 3; rain_num = 4; thunder_storm_num = 6; snow_num = 3;  hail_num = 6;  }
    else                         { return Weather; } // unknown season

    if (cloudy_roll > cloudy_num)
        Weather[0] = "cloudy";

    if (wind_roll > wind_num)
    {
        int knots = randomInt(1, 50);
        double wind_speed = knots * 1.9;
        Weather[2] = (randomInt(1, 2) == 1) ? "right" : "left";
        Weather[3] = std::to_string(wind_speed);
    }

    if (snow_roll > snow_num)
    {
        Weather[0] = "cloudy";
        Weather[1] = (hail_roll > hail_num) ? "hail" : "snow";
    }
    else if (rain_roll > rain_num)
    {
        Weather[0] = "cloudy";
        Weather[1] = (thunder_storm_roll > thunder_storm_num) ? "thunder_storm" : "rain";
    }

    return Weather;
}