#pragma once

#include <vector>
#include <string>

// Globals: declared here with "extern", defined once in Weather.cpp
extern std::string weather_state;
extern bool wind;
extern std::string wind_direction;

// Returns {cloud, precipitation, wind direction, wind speed}
std::vector<std::string> weather_report(const std::string& season);