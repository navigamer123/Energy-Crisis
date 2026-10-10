#ifndef UI_BOT_H
#define UI_BOT_H

#include <SFML/Graphics.hpp>
#include "UI_types.h"
#include "../../Game/includes/game_main.h"
#include "UI_resourceNodes.h"
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

    // Movement speed & decision parameters tuned per difficulty
    float moveSpeed = 420.0f;
    float decisionInterval = 0.35f;
    float mineHitInterval = 1.05f;

    int botPlayer = 2;
    int getResourceCount(const PlayerEconomy& econ, ResourceType type) const;
    void planNextAction(GameEngine& engine, const UI_resourceNodes& nodes, sf::Vector2f curPos);

public:
    UIBot() = default;

    void init(BotDifficulty diff, int player = 2);
    void reset();

    BotDifficulty getDifficulty() const { return difficulty; }
    int getPlayer() const { return botPlayer; }
    bool isActive() const { return difficulty != BotDifficulty::NONE; }

    void update(float dt, GameEngine& engine, const UI_resourceNodes& nodes, sf::Vector2f& botPos,
                bool& outTriggerAction, bool& outTriggerUpgrade, BuildingType& outSelectedBuilding);
};

#endif // UI_BOT_H

