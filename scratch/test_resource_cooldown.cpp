#include <iostream>
#include <cassert>
#include "../Game/includes/game_main.h"

int main() {
    std::cout << "=== Running Test: 2-Second Resource Harvest Cooldown ===\n";

    GameEngine engine;
    engine.init(1600.0f, 900.0f);

    float p1ResourceCooldown = 0.0f;
    int harvestedCount = 0;

    auto attemptHarvest = [&](float dtAdvance) {
        if (p1ResourceCooldown > 0.0f) {
            p1ResourceCooldown -= dtAdvance;
            if (p1ResourceCooldown < 0.0f) p1ResourceCooldown = 0.0f;
        }

        if (p1ResourceCooldown <= 0.0f) {
            GameEngine::MineResult res;
            std::string msg;
            if (engine.mineResource(1, ResourceType::ORE, res, msg)) {
                harvestedCount++;
                p1ResourceCooldown = 2.0f;
                return true;
            }
        }
        return false;
    };

    // First attempt at t=0
    bool h1 = attemptHarvest(0.0f);
    assert(h1);
    assert(harvestedCount == 1);
    assert(p1ResourceCooldown == 2.0f);
    std::cout << "  -> First harvest at t=0.0s: SUCCEEDED. Cooldown set to 2.0s.\n";

    // Immediate second attempt (holding or spamming at t=0.1s)
    bool h2 = attemptHarvest(0.1f);
    assert(!h2);
    assert(harvestedCount == 1);
    std::cout << "  -> Second harvest attempt at t=0.1s (cooldown active): BLOCKED.\n";

    // Attempt at t=1.0s (still within 2.0s cooldown)
    bool h3 = attemptHarvest(0.9f);
    assert(!h3);
    assert(harvestedCount == 1);
    std::cout << "  -> Third harvest attempt at t=1.0s (cooldown active): BLOCKED.\n";

    // Attempt at t=1.9s (still within cooldown)
    bool h4 = attemptHarvest(0.9f);
    assert(!h4);
    assert(harvestedCount == 1);
    std::cout << "  -> Fourth harvest attempt at t=1.9s (cooldown active): BLOCKED.\n";

    // Attempt after full 2.0s (at t=2.1s)
    bool h5 = attemptHarvest(0.2f);
    assert(h5);
    assert(harvestedCount == 2);
    assert(p1ResourceCooldown == 2.0f);
    std::cout << "  -> Fifth harvest attempt at t=2.1s (cooldown expired): SUCCEEDED. New 2.0s cooldown started.\n";

    std::cout << "\n>>> 2-SECOND COOLDOWN TEST PASSED! <<<\n";
    return 0;
}
