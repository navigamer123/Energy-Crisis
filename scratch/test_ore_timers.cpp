#include <iostream>
#include <cassert>
#include "../Game/includes/game_main.h"

int main() {
    std::cout << "=== Running Test: Different Ore Mine Timers ===\n";

    GameEngine engine;
    engine.init(1600.0f, 900.0f);

    float cdWood = Balance::getResourceMineCooldown(ResourceType::WOOD);
    float cdCoal = Balance::getResourceMineCooldown(ResourceType::COAL);
    float cdIron = Balance::getResourceMineCooldown(ResourceType::IRON);
    float cdCopper = Balance::getResourceMineCooldown(ResourceType::COPPER);
    float cdSilicon = Balance::getResourceMineCooldown(ResourceType::SILICON);
    float cdSilver = Balance::getResourceMineCooldown(ResourceType::SILVER);
    float cdGold = Balance::getResourceMineCooldown(ResourceType::GOLD);

    std::cout << "  Wood:    " << cdWood << "s\n";
    std::cout << "  Coal:    " << cdCoal << "s\n";
    std::cout << "  Iron:    " << cdIron << "s\n";
    std::cout << "  Copper:  " << cdCopper << "s\n";
    std::cout << "  Silicon: " << cdSilicon << "s\n";
    std::cout << "  Silver:  " << cdSilver << "s\n";
    std::cout << "  Gold:    " << cdGold << "s\n";

    assert(cdWood == 0.8f);
    assert(cdCoal == 1.0f);
    assert(cdIron == 1.2f);
    assert(cdCopper == 1.2f);
    assert(cdSilicon == 1.5f);
    assert(cdSilver == 1.8f);
    assert(cdGold == 2.5f);

    // Verify engine.getMineCooldown returns resource-specific cooldown
    assert(engine.getMineCooldown(1, ResourceType::WOOD) == 0.8f);
    assert(engine.getMineCooldown(1, ResourceType::GOLD) == 2.5f);
    assert(engine.getMineCooldown(1, ResourceType::NONE) == Balance::MINE_COOLDOWN_SEC);

    std::cout << ">>> ALL DIFFERENT ORE TIMERS VERIFIED SUCCESSFULLY! <<<\n";
    return 0;
}
