#include "../Game/includes/game_main.h"
#include "../Game/includes/game_balance.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "========================================================\n";
    std::cout << " RUNNING BLITZ FEATURES & WINDMILL HIGHLIGHT UNIT TESTS\n";
    std::cout << "========================================================\n";

    // -------------------------------------------------------------------------
    // Test 1: Blitz Land Plot Costs vs Standard Costs
    // -------------------------------------------------------------------------
    std::cout << "[Test 1] Verifying Blitz Land Plot Costs...\n";
    int stdBase = Balance::getLandPlotCost(0, 0, false);
    int blitzBase = Balance::getLandPlotCost(0, 0, true);
    assert(blitzBase >= 400);
    assert(blitzBase > stdBase);
    std::cout << "  Standard base cost: " << stdBase << " G, Blitz base cost: " << blitzBase << " G\n";

    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 3; ++c) {
            int blitzCost = Balance::getLandPlotCost(r, c, true);
            int standardCost = Balance::getLandPlotCost(r, c, false);
            assert(blitzCost > standardCost);
        }
    }
    std::cout << "  -> PASS: Blitz land plots are consistently more expensive.\n";

    // -------------------------------------------------------------------------
    // Test 2: Blitz Mode Max 4 Land Plots Hard Cap
    // -------------------------------------------------------------------------
    std::cout << "[Test 2] Verifying Max 4 Land Purchases in Blitz Mode...\n";
    {
        GameEngine engine;
        MatchConfig cfg;
        cfg.isBlitz = true;
        cfg.finalDay = 5;
        cfg.daySeconds = 60.0f;
        cfg.graceDays = 0;
        engine.init(cfg);

        // Give Player 1 realistic 5m blitz cash (4000 money)
        engine.getPlayerEconomyMut(1).money = 4000;
        engine.getPlayerEconomyMut(1).gold = 0;

        int purchasedCount = 0;
        std::string msg;
        // Try to buy 6 land plots with 4000 cash
        for (int i = 0; i < 6; ++i) {
            bool ok = engine.buyNextLandTier(1, msg);
            if (ok) {
                purchasedCount++;
                std::cout << "  Purchased land tier " << purchasedCount << " successfully.\n";
            } else {
                std::cout << "  Purchase rejected at attempt " << (i + 1) << " with message: " << msg << "\n";
                // Rejected due to cash shortage for the 5th plot (cost has a zero added: 11500 $)
                assert(msg.find("ПАРИ") != std::string::npos || msg.find("MONEY") != std::string::npos);
                break;
            }
        }
        assert(purchasedCount == 4);
        assert(engine.getPlayerEconomy(1).landTier == 5);
        std::cout << "  -> PASS: Blitz naturally limits purchased land plots to 4 max with cash.\n";

        // Verify that there is NO artificial hard limit: if player has 15000 cash, they CAN buy plot 5
        engine.getPlayerEconomyMut(1).money = 15000;
        bool ok5 = engine.buyNextLandTier(1, msg);
        assert(ok5 == true);
        assert(engine.getPlayerEconomy(1).landTier == 6);
        std::cout << "  -> PASS: No hard limit: player with sufficient cash (15000 $) can buy the escalated 5th plot.\n";
    }

    // -------------------------------------------------------------------------
    // Test 3: Standard 30m Mode Allows Expanding Beyond 4 Plots
    // -------------------------------------------------------------------------
    std::cout << "[Test 3] Verifying Standard Mode Expansion (Beyond 4 Plots)...\n";
    {
        GameEngine engine;
        MatchConfig cfg;
        cfg.isBlitz = false;
        cfg.finalDay = 20;
        cfg.daySeconds = 90.0f;
        cfg.graceDays = 2;
        engine.init(cfg);

        engine.getPlayerEconomyMut(1).gold = 50000;

        int purchasedCount = 0;
        std::string msg;
        for (int i = 0; i < 6; ++i) {
            bool ok = engine.buyNextLandTier(1, msg);
            if (ok) purchasedCount++;
        }
        assert(purchasedCount > 4);
        std::cout << "  -> PASS: Standard mode successfully purchased " << purchasedCount << " plots (> 4).\n";
    }

    // -------------------------------------------------------------------------
    // Test 4: Windmill 1x1 Radius Placement Highlighting
    // -------------------------------------------------------------------------
    std::cout << "[Test 4] Verifying Windmill 1x1 Radius Highlighting (Red Orthogonal, Yellow Corners)...\n";
    {
        GameEngine engine;
        MatchConfig cfg;
        engine.init(cfg);
        engine.getPlayerEconomyMut(1).gold = 50000;
        engine.getPlayerEconomyMut(1).wood = 5000;
        engine.getPlayerEconomyMut(1).iron = 5000;
        engine.getPlayerEconomyMut(1).copper = 5000;
        engine.getPlayerEconomyMut(1).coal = 5000;
        engine.getPlayerEconomyMut(1).silicon = 5000;
        engine.getPlayerEconomyMut(1).silver = 5000;

        // Buy all land plots for P1
        std::string msg;
        while (engine.buyNextLandTier(1, msg)) {}

        // Place a Windmill at slot (col 1, row 1)
        sf::Vector2f pos11 = engine.getGridSlot(1, 1, 1);
        bool ok = engine.placeBuilding(1, BuildingType::WIND_TURBINE, pos11, msg);
        if (!ok) std::cerr << "placeBuilding failed: " << msg << "\n";
        assert(ok);

        // Check 1x1 Orthogonal Neighbors -> Red sf::Color(255, 45, 45)
        sf::Color upColor = engine.getPlacementTileColor(1, BuildingType::WIND_TURBINE, 1, 0);
        sf::Color downColor = engine.getPlacementTileColor(1, BuildingType::WIND_TURBINE, 1, 2);
        sf::Color leftColor = engine.getPlacementTileColor(1, BuildingType::WIND_TURBINE, 0, 1);
        sf::Color rightColor = engine.getPlacementTileColor(1, BuildingType::WIND_TURBINE, 2, 1);

        assert(upColor == sf::Color(255, 45, 45));
        assert(downColor == sf::Color(255, 45, 45));
        assert(leftColor == sf::Color(255, 45, 45));
        assert(rightColor == sf::Color(255, 45, 45));
        std::cout << "  1x1 Orthogonal neighbors (1,0), (1,2), (0,1), (2,1) highlighted in RED.\n";

        // Check 1x1 Corner Diagonal Neighbors -> Yellow sf::Color(255, 215, 0)
        sf::Color tlColor = engine.getPlacementTileColor(1, BuildingType::WIND_TURBINE, 0, 0);
        sf::Color trColor = engine.getPlacementTileColor(1, BuildingType::WIND_TURBINE, 2, 0);
        sf::Color blColor = engine.getPlacementTileColor(1, BuildingType::WIND_TURBINE, 0, 2);
        sf::Color brColor = engine.getPlacementTileColor(1, BuildingType::WIND_TURBINE, 2, 2);

        assert(tlColor == sf::Color(255, 215, 0));
        assert(trColor == sf::Color(255, 215, 0));
        assert(blColor == sf::Color(255, 215, 0));
        assert(brColor == sf::Color(255, 215, 0));
        std::cout << "  1x1 Diagonal corner neighbors (0,0), (2,0), (0,2), (2,2) highlighted in YELLOW.\n";

        // Check Row 3 (col 1, row 3) - distance 2 in same column
        // Previously this buggy whole-column check turned it red!
        // Now it must NOT be Red and NOT be Yellow!
        sf::Color distantColColor = engine.getPlacementTileColor(1, BuildingType::WIND_TURBINE, 1, 3);
        assert(distantColColor != sf::Color(255, 45, 45));
        assert(distantColColor != sf::Color(255, 215, 0));
        assert(distantColColor == sf::Color(45, 230, 85)); // Green: valid placement without 1x1 interference
        std::cout << "  Distant slot (1, 3) at distance 2 is GREEN (no whole-column red bug!).\n";
        std::cout << "  -> PASS: Windmill 1x1 highlighting is completely fixed.\n";
    }

    std::cout << "========================================================\n";
    std::cout << " ALL BLITZ & WINDMILL UNIT TESTS PASSED SUCCESSFULLY!\n";
    std::cout << "========================================================\n";
    return 0;
}
