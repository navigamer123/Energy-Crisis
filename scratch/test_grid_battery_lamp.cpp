#include <iostream>
#include <cassert>
#include <cmath>
#include "../Game/includes/game_main.h"

int main() {
    std::cout << "=== Running Test: Grid Snapping, Battery Charging & Lamp Rules ===\n";

    GameEngine engine;
    engine.init(1600.0f, 900.0f);

    // -------------------------------------------------------------------------
    // Test 1: Grid Snapping
    // -------------------------------------------------------------------------
    std::cout << "[Test 1] Testing Grid Snapping...\n";
    const auto& plots = engine.getLandPlots();
    assert(!plots.empty());

    // Find Player 1's starting purchased plot
    const LandPlot* p1Plot = nullptr;
    for (const auto& plot : plots) {
        if (plot.playerOwner == 1 && plot.isPurchased) {
            p1Plot = &plot;
            break;
        }
    }
    assert(p1Plot != nullptr);

    float left = p1Plot->bounds.position.x;
    float top = p1Plot->bounds.position.y;
    float colW = p1Plot->bounds.size.x * 0.5f;
    float rowH = p1Plot->bounds.size.y * 0.5f;

    // Test snapping inside slot (0, 0)
    sf::Vector2f rawPos1(left + 10.0f, top + 10.0f);
    sf::Vector2f snapped1 = engine.snapToBuildingGrid(1, rawPos1);
    sf::Vector2f expected1(left + 0.5f * colW, top + 0.5f * rowH);
    assert(std::abs(snapped1.x - expected1.x) < 0.01f);
    assert(std::abs(snapped1.y - expected1.y) < 0.01f);

    // Test snapping inside slot (1, 1)
    sf::Vector2f rawPos2(left + colW + 15.0f, top + rowH + 15.0f);
    sf::Vector2f snapped2 = engine.snapToBuildingGrid(1, rawPos2);
    sf::Vector2f expected2(left + 1.5f * colW, top + 1.5f * rowH);
    assert(std::abs(snapped2.x - expected2.x) < 0.01f);
    assert(std::abs(snapped2.y - expected2.y) < 0.01f);
    std::cout << "  -> Grid snapping works perfectly! Slot (0,0) and Slot (1,1) match expected centers.\n";

    // -------------------------------------------------------------------------
    // Test 2: Battery Spawns with 0% Battery Charge
    // -------------------------------------------------------------------------
    std::cout << "[Test 2] Testing Battery Spawn Charge (must be 0%)...\n";
    auto& p1Econ = engine.getPlayerEconomyMut(1);
    p1Econ.wood = 500;
    p1Econ.ore = 500;

    std::string msg;
    bool placedBat = engine.placeBuilding(1, BuildingType::BATTERY, rawPos1, msg);
    assert(placedBat);

    const auto& buildings = engine.getBuildings();
    assert(buildings.size() == 1);
    const auto& batBuilding = buildings[0];
    assert(batBuilding.type == BuildingType::BATTERY);
    assert(batBuilding.energyStored == 0.0f);
    std::cout << "  -> Battery spawned with energyStored = " << batBuilding.energyStored << " MWh (0% charge)!\n";

    // -------------------------------------------------------------------------
    // Test 3: Battery Does NOT Charge from Thin Air (requires player generation)
    // -------------------------------------------------------------------------
    std::cout << "[Test 3] Testing Battery Idle without Generation...\n";
    // Advance simulation time by 5 seconds
    for (int i = 0; i < 50; i++) {
        engine.update(0.1f);
    }
    const auto& batAfterIdle = engine.getBuildings()[0];
    assert(batAfterIdle.energyStored == 0.0f);
    std::cout << "  -> Battery remained at 0% when player generated 0 power. No magic energy!\n";

    // Now place a Solar Panel on slot (1,0)
    sf::Vector2f rawSolarPos(left + colW + 10.0f, top + 10.0f);
    bool placedSolar = engine.placeBuilding(1, BuildingType::SOLAR_PANEL, rawSolarPos, msg);
    assert(placedSolar);

    // Update simulation during daytime: Solar panel produces power, charging the battery
    if (engine.isDaylight()) {
        for (int i = 0; i < 50; i++) {
            engine.update(0.1f);
        }
        const auto& batAfterGen = engine.getBuildings()[0];
        std::cout << "  -> After solar generation, battery energyStored = " << batAfterGen.energyStored << " MWh (> 0%)\n";
        assert(batAfterGen.energyStored > 0.0f);
    }

    // -------------------------------------------------------------------------
    // Test 4: Lamp Energy Consumption & Light Illumination
    // -------------------------------------------------------------------------
    std::cout << "[Test 4] Testing Lamp Placement and 10 MW Consumption...\n";
    sf::Vector2f rawLampPos(left + 10.0f, top + rowH + 10.0f);
    bool placedLamp = engine.placeBuilding(1, BuildingType::LAMP, rawLampPos, msg);
    assert(placedLamp);
    engine.update(0.1f);

    bool lampFound = false;
    for (const auto& b : engine.getBuildings()) {
        if (b.type == BuildingType::LAMP) {
            lampFound = true;
            assert(b.currentOutputMW == -10.0f); // Lamp consumes 10 MW
            assert(b.lightRadius == 150.0f);     // Powered lamp emits 150px light cone
            std::cout << "  -> Lamp is powered! Consumption = " << b.currentOutputMW << " MW, Light radius = " << b.lightRadius << " px\n";
        }
    }
    assert(lampFound);

    // -------------------------------------------------------------------------
    // Test 5: Night Construction Rule
    // -------------------------------------------------------------------------
    std::cout << "[Test 5] Testing Night Construction Rules...\n";
    // Advance time until night
    while (engine.isDaylight()) {
        engine.update(1.0f);
    }
    assert(!engine.isDaylight());
    std::cout << "  -> Night has fallen (hour24 = " << engine.getHour24() << ")\n";

    // Inside illuminated area of active lamp, placement should succeed!
    sf::Vector2f insideLightSlot(left + colW + 10.0f, top + rowH + 10.0f); // Slot (1, 1) in same plot is within 150px
    std::string placeReason;
    bool canBuildInsideLight = engine.canPlaceBuilding(1, BuildingType::SOLAR_PANEL, insideLightSlot, placeReason);
    std::cout << "  -> Building inside active lamp illumination: " << (canBuildInsideLight ? "ALLOWED (OK)" : "REJECTED (" + placeReason + ")") << "\n";
    assert(canBuildInsideLight);

    // Outside illuminated area on Plot 3 (Col 2, distance > 200px), placement must be REJECTED!
    p1Econ.gold = 10000;
    std::string buyPlotMsg;
    engine.buyLandPlot(1, 3, buyPlotMsg); // Col 2, Row 0 (x = 492px)
    const LandPlot* plot3 = nullptr;
    for (const auto& p : engine.getLandPlots()) {
        if (p.id == 3) plot3 = &p;
    }
    assert(plot3 != nullptr && plot3->isPurchased);

    sf::Vector2f unilluminatedPos(plot3->bounds.position.x + 20.0f, plot3->bounds.position.y + 20.0f);
    bool isIlluminated = engine.isAreaIlluminated(1, unilluminatedPos);
    assert(!isIlluminated);

    bool canBuildInDark = engine.canPlaceBuilding(1, BuildingType::SOLAR_PANEL, unilluminatedPos, placeReason);
    assert(!canBuildInDark);
    assert(placeReason.find("НОЩЕН МРАК") != std::string::npos);
    std::cout << "  -> Building in the dark outside light cone: REJECTED with message: \"" << placeReason.substr(0, 45) << "...\"\n";

    // But placing a LAMP in the dark to set up lighting IS allowed
    bool canPlaceLampInDark = engine.canPlaceBuilding(1, BuildingType::LAMP, unilluminatedPos, placeReason);
    assert(canPlaceLampInDark);
    std::cout << "  -> Placing a LAMP in the dark: ALLOWED so player can establish lighting!\n";

    // -------------------------------------------------------------------------
    // Test 6: Unpowered Lamp (0 Power Generation + Depleted Battery) Turns Off
    // -------------------------------------------------------------------------
    std::cout << "[Test 6] Testing Unpowered Lamp blackout when power is depleted...\n";
    // Place a fresh engine with 1 lamp and 0 generators, at night
    GameEngine darkEngine;
    darkEngine.init(1600.0f, 900.0f);
    // Move to night
    while (darkEngine.isDaylight()) darkEngine.update(1.0f);
    auto& darkP1 = darkEngine.getPlayerEconomyMut(1);
    darkP1.wood = 500;
    darkP1.ore = 500;

    // Place lamp on starting plot
    sf::Vector2f startPlotSlot(258.0f + 20.0f, 120.0f + 20.0f);
    bool placedDarkLamp = darkEngine.placeBuilding(1, BuildingType::LAMP, startPlotSlot, msg);
    assert(placedDarkLamp);

    // Update tick: with 0 generators and 0 battery, lamp should have 0 power and lightRadius = 0
    darkEngine.update(0.1f);
    const auto& darkBuildings = darkEngine.getBuildings();
    assert(darkBuildings.size() == 1);
    assert(darkBuildings[0].type == BuildingType::LAMP);
    assert(darkBuildings[0].lightRadius == 0.0f); // UNPOWERED!
    std::cout << "  -> Lamp with 0 available electricity went dark: lightRadius = " << darkBuildings[0].lightRadius << " px\n";

    // Because lamp is unpowered, the area is NOT illuminated
    assert(!darkEngine.isAreaIlluminated(1, startPlotSlot));

    // Trying to place a solar panel right next to unpowered lamp must be REJECTED!
    sf::Vector2f adjacentSlot(258.0f + 70.0f, 120.0f + 20.0f);
    bool canBuildNearUnpoweredLamp = darkEngine.canPlaceBuilding(1, BuildingType::SOLAR_PANEL, adjacentSlot, placeReason);
    assert(!canBuildNearUnpoweredLamp);
    std::cout << "  -> Building next to unpowered dark lamp: REJECTED (unpowered lamp cannot illuminate)!\n";

    std::cout << "\n>>> ALL 6 TESTS PASSED SUCCESSFULLY! <<<\n";
    return 0;
}
