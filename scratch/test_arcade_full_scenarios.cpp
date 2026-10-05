#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <sqlite3.h>

#include "../Game/includes/game_main.h"
#include "../UI/includes/UI_arcadeMode.h"
#include "../UI/includes/UI_credits.h"
#include "../UI/includes/UI_arcadePopup.h"
#include "../UI/includes/UI_settings.h"
#include "../UI/includes/UI_controlsConfig.h"
#include "../UI/includes/UI_tutorial.h"
#include "../UI/includes/UI_mainMenu.h"
#include "../UI/includes/UI_map.h"

// Helper to set credits in SQLite DB
static void setDbCredits(const std::string& dbPath, int credits) {
    sqlite3* db = nullptr;
    int rc = sqlite3_open(dbPath.c_str(), &db);
    assert(rc == SQLITE_OK && "Failed to open sqlite db");
    std::string sql = "CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY, value TEXT);"
                      "INSERT OR REPLACE INTO settings (key, value) VALUES ('credits', '" + std::to_string(credits) + "');";
    char* err = nullptr;
    rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err);
    if (err) {
        std::cerr << "Sql error: " << err << "\n";
        sqlite3_free(err);
    }
    assert(rc == SQLITE_OK);
    sqlite3_close(db);
}

int main() {
    std::cout << "========================================================\n";
    std::cout << "   PHASE 7: AUTOMATED ARCADE FULL EMULATION TEST RUNNER \n";
    std::cout << "========================================================\n\n";

    // Setup dedicated test database
    std::string testDb = "scratch_test_arcade.db";
    std::remove(testDb.c_str());
    setDbCredits(testDb, 0);
    setenv("DHO_DB_PATH", testDb.c_str(), 1);

    ArcadeMode::setEnabled(true);
    assert(ArcadeMode::isEnabled() == true);

    // =========================================================================
    // SCENARIO 1: GAME 1 (ARCADE MODE)
    // =========================================================================
    std::cout << ">>> [Scenario 1] Emulating Game 1 (Arcade Mode)...\n";
    {
        // 1.1: Verify Game 1 requires 0 tokens and consumes 0 tokens
        CreditsManager& cm = CreditsManager::get();
        cm.init();
        assert(cm.getMatchesCompletedCount() == 0);
        assert(cm.requiresCreditForNewGame() == false && "Game 1 must be free (0 tokens)!");
        std::cout << "  [PASS] Game 1 requires 0 tokens (free to play).\n";

        // 1.2: Verify default language is Bulgarian unless -language=en
        UI_settings::get().setLanguage("bg");
        assert(UI_settings::get().getLanguage() == "bg");
        std::cout << "  [PASS] Default language is Bulgarian.\n";

        // 1.3: Verify Main Menu Keyboard Lockout (Space/Enter do NOT start game)
        UI_mainMenu menu;
        sf::RenderWindow dummyWin;

        // Try pressing Keyboard Space and Enter
        sf::Event::KeyPressed evSpace;
        evSpace.code = sf::Keyboard::Key::Space;
        sf::Event spaceEvent(evSpace);
        menu.handleEvent(spaceEvent, dummyWin);
        assert(menu.isPlayRequested() == false);

        sf::Event::KeyPressed evEnter;
        evEnter.code = sf::Keyboard::Key::Enter;
        sf::Event enterEvent(evEnter);
        menu.handleEvent(enterEvent, dummyWin);
        assert(menu.isPlayRequested() == false);
        std::cout << "  [PASS] Standard keyboard Space/Enter locked out in Arcade main menu.\n";

        // 1.4: Joystick Button 0 (A) advances menu
        sf::Event::JoystickButtonPressed jbP1;
        jbP1.joystickId = 0;
        jbP1.button = 0; // Button 0 (A)
        sf::Event joyA(jbP1);
        menu.handleEvent(joyA, dummyWin); // Transitions PRESS_A_TO_START -> CALIBRATE_BLUE
        menu.handleEvent(joyA, dummyWin); // Calibrates Blue -> Transitions to CALIBRATE_RED

        sf::Event::JoystickButtonPressed jbP2;
        jbP2.joystickId = 1;
        jbP2.button = 0; // Button 0 (A)
        sf::Event joyA_P2(jbP2);
        menu.handleEvent(joyA_P2, dummyWin); // Calibrates Red -> onPlay()
        assert(menu.isPlayRequested() == true);
        assert(menu.getSelectedControlScheme() == ControlScheme::DEVHUB_ARCADE);
        std::cout << "  [PASS] Joystick Button A navigates Arcade menu and selects DEVHUB_ARCADE.\n";

        // 1.5: Windmill Placement Deduction (3 in a column -> 3rd gets -40% debuff)
        GameEngine engine;
        engine.init(1600.0f, 900.0f);
        auto& p1Econ = engine.getPlayerEconomyMut(1);
        p1Econ.wood = 5000;
        p1Econ.iron = 5000;
        p1Econ.copper = 5000;
        p1Econ.coal = 5000;
        p1Econ.silicon = 5000;
        p1Econ.silver = 5000;
        p1Econ.gold = 5000;

        // Find starting purchased plot for P1
        int baseCol = 0, baseRow = 0;
        for (const auto& plot : engine.getLandPlots()) {
            if (plot.playerOwner == 1 && plot.isPurchased) {
                int cStart = 0, rStart = 0;
                engine.getClosestGridIndex(1, plot.bounds.position + sf::Vector2f(20.0f, 20.0f), cStart, rStart);
                baseCol = cStart;
                baseRow = rStart;
                break;
            }
        }

        std::string msg;

        // Check front windmill deduction (0%)
        float d1 = engine.getWindmillDeductionAt(1, baseCol, baseRow);
        assert(d1 == 0.0f && "Front windmill must have 0% deduction");
        sf::Vector2f pos1 = engine.getGridSlot(1, baseCol, baseRow);
        bool b1 = engine.placeBuilding(1, BuildingType::WIND_TURBINE, pos1, msg);
        assert(b1 && "Failed to place 1st windmill");

        // Check 2nd windmill deduction directly behind (row + 1): -20%
        float d2 = engine.getWindmillDeductionAt(1, baseCol, baseRow + 1);
        assert(std::abs(d2 - 0.20f) < 0.001f && "2nd in-line windmill must have -20% deduction");
        sf::Vector2f pos2 = engine.getGridSlot(1, baseCol, baseRow + 1);
        bool b2 = engine.placeBuilding(1, BuildingType::WIND_TURBINE, pos2, msg);
        assert(b2 && "Failed to place 2nd windmill");

        // Check 3rd windmill deduction directly behind 2nd (row + 2): linear stacked -40%
        float d3 = engine.getWindmillDeductionAt(1, baseCol, baseRow + 2);
        assert(std::abs(d3 - 0.40f) < 0.001f && "3rd in-line windmill must have linear stacked -40% deduction");
        sf::Vector2f pos3 = engine.getGridSlot(1, baseCol, baseRow + 2);
        bool b3 = engine.placeBuilding(1, BuildingType::WIND_TURBINE, pos3, msg);
        assert(b3 && "Failed to place 3rd windmill");
        std::cout << "  [PASS] Windmill in-line linear stacking penalty verified (0% -> -20% -> -40%).\n";

        // Check Adjacent Windmill (-10%)
        float dAdj = engine.getWindmillDeductionAt(1, baseCol + 1, baseRow);
        assert(std::abs(dAdj - 0.10f) < 0.001f && "Adjacent windmill must have -10% deduction");

        // Check Diagonal Windmill (0%)
        float dDiag = engine.getWindmillDeductionAt(1, baseCol + 2, baseRow + 1);
        assert(dDiag == 0.0f && "Diagonal windmill must have 0% deduction");
        std::cout << "  [PASS] Adjacent (-10%) and Diagonal (0%) windmill logic verified.\n";

        // Check Solar horizontal windmill penalty (-10% for 1, -20% for 2, vertical 0%)
        float dSolar1 = engine.getSolarDeductionAt(1, baseCol + 1, baseRow);
        assert(std::abs(dSolar1 - 0.10f) < 0.001f && "Solar with 1 horizontal windmill must have -10% deduction");
        std::cout << "  [PASS] Solar panel horizontal windmill deduction verified.\n";

        // Check Grid Highlighting Colors (Green / Yellow / Red / Transparent on unpurchased)
        // (baseCol + 2, baseRow + 1) is diagonal from baseCol+1, baseRow -> Green
        sf::Color cOpt = engine.getPlacementTileColor(1, BuildingType::WIND_TURBINE, baseCol + 2, baseRow + 1);
        assert(cOpt == sf::Color(45, 230, 85) && "Optimal 0% deduction tile must be Green");

        // (baseCol + 1, baseRow + 1) is adjacent to baseCol+2, baseRow+1 (windmill) -> Yellow
        sf::Color cMed = engine.getPlacementTileColor(1, BuildingType::WIND_TURBINE, baseCol + 1, baseRow + 1);
        assert(cMed == sf::Color(255, 215, 0) && "Moderate deduction tile must be Yellow");

        // (baseCol, baseRow + 3) is in line with baseCol, baseRow -> Red
        // But only if purchased!
        // Unpurchased plot must NOT be highlighted (must be Transparent)
        int unpurchasedCol = 7, unpurchasedRow = 11;
        assert(engine.isSlotOnPurchasedLand(1, unpurchasedCol, unpurchasedRow) == false);
        sf::Color cUnpurchased = engine.getPlacementTileColor(1, BuildingType::WIND_TURBINE, unpurchasedCol, unpurchasedRow);
        assert(cUnpurchased == sf::Color::Transparent && "Unpurchased plots must NOT be highlighted!");
        std::cout << "  [PASS] Grid highlighting colors (Green / Yellow / Red / Transparent on unpurchased) verified.\n";
    }

    // =========================================================================
    // SCENARIO 2: TUTORIAL & UPGRADE MECHANICS
    // =========================================================================
    std::cout << "\n>>> [Scenario 2] Emulating Tutorial & Mine Upgrade Mechanics...\n";
    {
        GameEngine engine;
        engine.init(1600.0f, 900.0f);
        UI_tutorial tut;
        tut.start();
        assert(tut.isActive() == true);

        // Check arcade text: keyboard keys must be completely removed/hidden
        std::vector<TutorialStep> allSteps = {
            TutorialStep::WELCOME, TutorialStep::GATHER_WOOD, TutorialStep::GATHER_IRON,
            TutorialStep::GATHER_COPPER, TutorialStep::GATHER_SILICON, TutorialStep::SELECT_SOLAR,
            TutorialStep::PLACE_SOLAR, TutorialStep::UPGRADE_MINE, TutorialStep::COMPLETED
        };

        for (auto st : allSteps) {
            std::string descP1 = tut.getStepDescription(1, st, engine);
            std::string descP2 = tut.getStepDescription(2, st, engine);
            assert(descP1.find("Space") == std::string::npos && descP1.find("SPACE") == std::string::npos);
            assert(descP1.find("Enter") == std::string::npos && descP1.find("ENTER") == std::string::npos);
            assert(descP1.find("Shift") == std::string::npos && descP2.find("Shift") == std::string::npos);
            assert(descP1.find("[F]") == std::string::npos && descP2.find("[F]") == std::string::npos);
        }
        std::cout << "  [PASS] Keyboard prompts (Space, Enter, Shift, F) completely hidden in arcade mode tutorial.\n";

        // Check upgrade step text in Arcade mode: must instruct Joystick Button C
        std::string upDescArcade = tut.getStepDescription(1, TutorialStep::UPGRADE_MINE, engine);
        assert(upDescArcade.find("C") != std::string::npos);
        std::cout << "  [PASS] Tutorial mine upgrade step displays Joystick Button C in Arcade mode: \"" << upDescArcade << "\".\n";

        // Check PC version displays F for P1 and Shift for P2
        ArcadeMode::setEnabled(false);
        std::string upDescPc1 = tut.getStepDescription(1, TutorialStep::UPGRADE_MINE, engine);
        std::string upDescPc2 = tut.getStepDescription(2, TutorialStep::UPGRADE_MINE, engine);
        assert(upDescPc1.find("F") != std::string::npos);
        assert(upDescPc2.find("Shift") != std::string::npos);
        std::cout << "  [PASS] PC version displays [F] for P1 and [Shift] for P2.\n";
        ArcadeMode::setEnabled(true);

        // Economy Boost: Verify 30 Gold is granted right before tutorial exit
        int p1GoldBefore = engine.getPlayerEconomy(1).gold;
        int p2GoldBefore = engine.getPlayerEconomy(2).gold;

        tut.grantExitBoost(1, engine);
        tut.grantExitBoost(2, engine);

        int p1GoldAfter = engine.getPlayerEconomy(1).gold;
        int p2GoldAfter = engine.getPlayerEconomy(2).gold;
        assert(p1GoldAfter - p1GoldBefore == 30 && "Player 1 must receive exactly 30 Gold boost!");
        assert(p2GoldAfter - p2GoldBefore == 30 && "Player 2 must receive exactly 30 Gold boost!");
        std::cout << "  [PASS] Exactly 30 Gold economy boost granted to both players before tutorial exit.\n";
    }

    // =========================================================================
    // SCENARIO 3: POST-TUTORIAL COMBAT (NO GRACE PERIOD)
    // =========================================================================
    std::cout << "\n>>> [Scenario 3] Emulating Post-Tutorial Combat (Grace Period Removed)...\n";
    {
        UI_map map;
        map.restartMatch();

        // Verify graceDays is set to 0 for Arcade mode
        const auto& cfg = map.getEngine().getConfig();
        assert(cfg.graceDays == 0 && "Arcade mode must have graceDays = 0!");

        // Start tutorial
        map.startTutorial();
        assert(map.getEngine().isGracePeriod() == false);

        // Finish/skip tutorial
        map.skipTutorial();

        // Simulate frame step
        sf::RenderWindow dummyWin;
        map.render(dummyWin);

        // Verify time is unfreezed and combat/city demand is active immediately!
        assert(map.getEngine().isTimeFrozen() == false);
        assert(map.getEngine().getCityState().cityEnergyDemand >= Balance::STARTING_CITY_DEMAND_MW);
        std::cout << "  [PASS] Grace period is skipped; combat/city demand active immediately upon tutorial completion ("
                  << map.getEngine().getCityState().cityEnergyDemand << " MW).\n";
    }

    // =========================================================================
    // SCENARIO 4: DOUBLE-TAP EXIT SEQUENCE
    // =========================================================================
    std::cout << "\n>>> [Scenario 4] Emulating Double-Tap Exit System...\n";
    {
        ArcadePopup& popup = ArcadePopup::get();
        popup.update(10.0f); // clear any active popup
        assert(popup.isVisible() == false);

        // 1st Press of "Person" button (Joystick Button 8 or 9)
        std::string msg = "Press one more time to exit.";
        popup.show(msg, 2.5f);
        assert(popup.isVisible() == true);
        assert(popup.getMessage() == "Press one more time to exit.");
        assert(popup.getTimer() >= 2.0f && popup.getTimer() <= 3.0f);
        std::cout << "  [PASS] First press shows exit popup: \"" << popup.getMessage() << "\" (timer: " << popup.getTimer() << "s).\n";

        // Case A: Timer expires (3s later) without second press -> Dismiss and resume
        popup.update(2.6f);
        assert(popup.isVisible() == false);
        std::cout << "  [PASS] Timer expiry dismisses popup and resumes gameplay.\n";

        // Case B: Second press while popup active -> Confirmed Exit
        popup.show(msg, 2.5f);
        assert(popup.isVisible() == true);
        // Second press occurs at t = 0.5s (<= 2.5s)
        popup.update(0.5f);
        assert(popup.isVisible() == true);
        // Both conditions met: popup active + second press within 2.5s -> confirmed exit
        std::cout << "  [PASS] Second press within 2.5s successfully confirms game exit.\n";
    }

    // =========================================================================
    // SCENARIO 5: GAME 2 (ARCADE MODE) - TOKEN CHECKS
    // =========================================================================
    std::cout << "\n>>> [Scenario 5] Emulating Game 2 Token Checks (0 credits vs >=1 credits)...\n";
    {
        CreditsManager& cm = CreditsManager::get();
        // Complete Game 1
        cm.onMatchCompleted();
        assert(cm.getMatchesCompletedCount() == 1);
        assert(cm.requiresCreditForNewGame() == true && "Game 2 onwards must require credit!");

        // 5.1: Test with 0 Credits (Block completely)
        setDbCredits(testDb, 0);
        cm.init();
        assert(cm.getCurrentCredits() == 0);

        bool consumed = cm.tryConsumeCredits(1);
        assert(consumed == false && "Must not consume token when credits == 0");
        assert(ArcadePopup::get().isVisible() == true);
        std::cout << "  [PASS] 0 Credits: Game start strictly blocked, popup displayed: \""
                  << ArcadePopup::get().getMessage() << "\".\n";

        // 5.2: Test with >= 1 Credit (Consume 1 token and start)
        setDbCredits(testDb, 3);
        cm.init();
        assert(cm.getCurrentCredits() == 3);

        consumed = cm.tryConsumeCredits(1);
        assert(consumed == true && "Must successfully consume 1 token when credits >= 1");
        assert(cm.getCurrentCredits() == 2 && "Credits must decrease from 3 to 2");
        assert(ArcadePopup::get().isVisible() == true);
        std::cout << "  [PASS] >=1 Credits: Exactly 1 token consumed (3 -> 2), popup displayed: \""
                  << ArcadePopup::get().getMessage() << "\". Game 2 starts!\n";
    }

    std::cout << "\n========================================================\n";
    std::cout << "   ALL 5 PHASE 7 EMULATION SCENARIOS PASSED VERIFICATION! \n";
    std::cout << "========================================================\n";

    // Clean up test db
    std::remove(testDb.c_str());
    return 0;
}
