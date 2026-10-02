#include "game_expedition.h"
#include <random>

int randomInt(int min, int max)
{
    static std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<int> dist(min, max);
    return dist(gen);
}

std::vector<int> Expedition(int expedition_time, const std::string& expedition_type)
{
    std::vector<int> resources;

    if (expedition_type == "cave")
    {
        int silicon = randomInt(1, 10) * expedition_time;
        int copper  = randomInt(1, 20) * expedition_time;
        int silver  = randomInt(1, 10) * expedition_time;
        int iron    = randomInt(1, 15) * expedition_time;
        int gold    = randomInt(1, 5)  * expedition_time;
        int coal    = randomInt(1, 30) * expedition_time;

        resources = {silicon, copper, silver, iron, gold, coal};
    }
    else if (expedition_type == "forest")
    {
        int wood   = randomInt(1, 40) * expedition_time * 4;
        int sticks = randomInt(1, 60) * expedition_time * 4;

        resources = {wood, sticks};
    }

    return resources;
}