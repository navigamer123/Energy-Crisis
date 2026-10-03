#ifndef UI_BOT_H
#define UI_BOT_H

#include <SFML/Graphics.hpp>
#include "UI_types.h"
#include "../../Game/includes/game_main.h"
#include "UI_resourceNodes.h"
#include "UI_botProfiles.h"
#include <string>
#include <vector>

enum class BotActionState {
    THINKING,
    MOVING_TO_MINE,
    MINING_RESOURCE,
    MOVING_TO_BUILD,
    MOVING_TO_BUY_LAND,
    MOVING_TO_UPGRADE
};

class UIBot {
private:
    BotDifficulty difficulty = BotDifficulty::NONE;
    BotActionState actionState = BotActionState::THINKING;
    BotProfile profile;            // [AI team] skill + style (UI_botProfiles.cpp)
    int personalityId = 0;         // chosen rival, kept across restarts
    int playerId = 2;              // sector the bot plays (2 in the game; the simulation can drive P1)

    sf::Vector2f targetPos = { 1150.0f, 450.0f };
    float stateTimer = 0.0f;
    float mineCooldown = 0.0f;
    float stateWatchdog = 0.0f;

    // Strategic targets
    BuildingType plannedBuilding = BuildingType::NONE;
    sf::Vector2f plannedBuildSlot = { 0.0f, 0.0f };
    ResourceType plannedResource = ResourceType::NONE;
    int targetResourceQuota = 0;
    int plannedPlotId = -1;
    ResourceType plannedUpgradeRes = ResourceType::NONE;
    BuildingType buildGoal = BuildingType::NONE; // generator the current mining run is for (intent text)

    // Governor readout (BAL-01), refreshed at every plan
    float lastSupplyMW = 0.0f;
    float lastTargetMW = 0.0f;
    bool lastSatisfied = false;
    std::string intent;            // short Bulgarian line describing the current plan

    int getResourceCount(const PlayerEconomy& econ, ResourceType type) const;
    void planNextAction(GameEngine& engine, const UI_resourceNodes& nodes, sf::Vector2f curPos);
    bool evaluateGovernor(const GameEngine& engine);
    bool startMining(const UI_resourceNodes& nodes, ResourceType res, int quota);
    void setRest(float seconds, const std::string& why);

public:
    UIBot() = default;

    void init(BotDifficulty diff);                         // keeps the chosen rival
    void initWithProfile(BotDifficulty diff, const BotProfile& custom); // simulation / tests
    void reset();

    // [AI team] Rival choice (0..BOT_PERSONALITY_COUNT-1); applied by the next init()
    void setPersonality(int id) { personalityId = id; }
    int getPersonality() const { return personalityId; }
    void setPlayerId(int player) { playerId = (player == 1) ? 1 : 2; }
    int getPlayerId() const { return playerId; }
    const BotProfile& getProfile() const { return profile; }

    // Engine / UI hooks
    const PlayerModifiers& getEngineModifiers() const { return profile.engineEdge; }
    float getActionCooldown() const { return profile.actionCooldown; }
    const std::string& getIntentText() const { return intent; }
    bool isGovernorSatisfied() const { return lastSatisfied; }
    float getGovernorSupplyMW() const { return lastSupplyMW; }
    float getGovernorTargetMW() const { return lastTargetMW; }
    BotActionState getActionState() const { return actionState; }

    BotDifficulty getDifficulty() const { return difficulty; }
    bool isActive() const { return difficulty != BotDifficulty::NONE; }

    void update(float dt, GameEngine& engine, const UI_resourceNodes& nodes, sf::Vector2f& botPos,
                bool& outTriggerAction, bool& outTriggerUpgrade, BuildingType& outSelectedBuilding);
};

#endif // UI_BOT_H
