#pragma once

int randomInt(int min, int max);

// Re-seeds the shared generator used by randomInt (called once per match by GameEngine::init)
void seedRandom(unsigned int seed);
