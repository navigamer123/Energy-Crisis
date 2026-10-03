#include "../includes/game_random.h"
#include <random>

static std::mt19937& sharedGenerator()
{
    // Default seed only matters until GameEngine::init() calls seedRandom()
    static std::mt19937 gen(std::random_device{}());
    return gen;
}

void seedRandom(unsigned int seed)
{
    sharedGenerator().seed(seed);
}

int randomInt(int min, int max)
{
    std::uniform_int_distribution<int> dist(min, max);
    return dist(sharedGenerator());
}
