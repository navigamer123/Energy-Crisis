#ifndef UI_TUTORIAL_CHAPTERS_H
#define UI_TUTORIAL_CHAPTERS_H

// =============================================================================
// [b-showcase] Tutorial chapters 2-3 (F-07): step logic, clock policy and resource grants
//
//   Chapter 2 "night and storage": build a battery by day, time-lapse to dusk while it charges,
//   place a lamp at night, build inside its light, time-lapse through the night to the 06:00
//   settlement.
//   Chapter 3 "land and mines": earn 30 gold, upgrade a mine, buy the river-bank plot, build hydro.
//
// Pure logic on top of GameEngine (no drawing), so scratch/test_showcase.cpp tests it headless.
// UI_tutorial owns one instance, draws the cards and forwards Space/Enter on dialog steps.
// Both players receive the same material grants, so the tutorial never unbalances the match.
// =============================================================================

#include <string>
#include <vector>
#include "../../Game/includes/game_main.h"

enum class ChapterStep {
    NONE = 0,
    // Chapter 2: night and storage
    C2_INTRO,
    C2_SELECT_BATTERY,
    C2_PLACE_BATTERY,
    C2_DUSK,
    C2_SELECT_LAMP,
    C2_PLACE_LAMP,
    C2_BUILD_IN_LIGHT,
    C2_WATCH_NIGHT,
    C2_DONE,
    // Chapter 3: land and mines
    C3_INTRO,
    C3_EARN_GOLD,
    C3_UPGRADE_MINE,
    C3_BUY_PLOT,
    C3_SELECT_HYDRO,
    C3_PLACE_HYDRO,
    C3_DONE
};

struct ResourceGrant {
    int wood = 0, iron = 0, copper = 0, coal = 0, silicon = 0, silver = 0, gold = 0;
};

class TutorialChapters {
public:
    static constexpr float TIMELAPSE_SCALE = 12.0f; // Game time runs this much faster in the watch steps
    static constexpr int GOLD_TARGET = 30;          // Chapter 3, step 1
    static constexpr float STEP_DELAY_SEC = 0.6f;   // Short pause on a finished step before the next one

    void reset();
    void startChapter(int chapter); // 2 or 3 (anything else ends the chapters)
    void requestConfirm() { confirmRequested = true; } // [SPACE]/[ENTER]/button on a dialog step

    // Advances the steps from the engine state (and handles a pending confirm)
    void update(float dt, const GameEngine& engine);
    // Applies queued grants to both players (call with the mutable engine once per frame)
    void applyPending(GameEngine& engine);

    ChapterStep getStep() const { return step; }
    int getChapter() const;
    bool isRunning() const { return step != ChapterStep::NONE; }
    bool isDialog() const; // INTRO / DONE cards that wait for [SPACE]
    bool isFinalCard() const { return step == ChapterStep::C3_DONE; }
    // 0 = clock held, 1 = normal time, TIMELAPSE_SCALE = time-lapse
    float clockScale(const GameEngine& engine) const;
    // Step number inside the chapter (1-based) and the chapter's step count, for the card badge
    int stepNumber() const;
    int stepCount() const;
    float stepDelayLeft() const { return stepDelay; }

    // Day-end message of the night watched in chapter 2 (shown on the C2_DONE card)
    const std::string& getWatchedSettlement() const { return watchedSettlement; }

    // --- Engine queries used by the logic and the cards (Player 1 = the learner) ---
    static int countBuildings(const GameEngine& engine, int player, BuildingType type);
    static int countNonLampBuildings(const GameEngine& engine, int player);
    static int mineLevelSum(const GameEngine& engine, int player);
    static float batteryCharge(const GameEngine& engine, int player); // 0..1 over all batteries
    static int riverPlotId(int player);                               // Cheapest river-bank plot
    static const LandPlot* findPlot(const GameEngine& engine, int plotId);
    static bool ownsPlot(const GameEngine& engine, int plotId);
    // A free building slot on the player's purchased land, preferring `preferredPlotId`; when
    // mustBeLit is set the slot must lie inside the light of one of the player's powered lamps,
    // when riverBankOnly is set it must be a river-bank slot (hydro).
    // Returns false when there is none.
    static bool findFreeSlot(const GameEngine& engine, int player, int preferredPlotId, bool mustBeLit,
                             bool riverBankOnly, sf::Vector2f& outPos);

    // Material packages (identical for both players)
    static ResourceGrant chapter2Grant();  // battery + lamp + solar panel
    static ResourceGrant plotGrant();      // price of the river-bank plot
    static ResourceGrant hydroGrant();     // one hydro plant
    static void applyGrant(GameEngine& engine, int player, const ResourceGrant& g);

private:
    ChapterStep step = ChapterStep::NONE;
    bool confirmRequested = false;
    float stepDelay = 0.0f;   // > 0: the current step is finished, the next one starts when it runs out
    ChapterStep nextStep = ChapterStep::NONE;
    int baseline = 0;         // Count captured when the current step started
    int watchDay = 0;         // Day whose end chapter 2 watches
    std::string watchedSettlement;
    std::vector<ResourceGrant> pendingGrants;
    bool chapter2Granted = false; // Each package is handed out once per chapter run
    bool plotGranted = false;
    bool hydroGranted = false;

    void enter(ChapterStep s, const GameEngine& engine);
    void finishStep(ChapterStep next); // Starts the short delay before `next`
};

#endif // UI_TUTORIAL_CHAPTERS_H
