#ifndef UI_BOT_H
#define UI_BOT_H

#include <SFML/Graphics.hpp>
#include "UI_types.h"
#include "../../Game/includes/game_main.h"
#include "UI_resourceNodes.h"

enum class BotActionState {
    THINKING,
    MOVING_TO_MINE,
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

    BuildingType plannedBuilding = BuildingType::NONE;
    ResourceType plannedResource = ResourceType::NONE;
    int plannedPlotId = -1;

    // Movement speed & decision parameters tuned per difficulty
    float moveSpeed = 380.0f;
    float decisionInterval = 1.0f;
    float harvestTimeBudget = 2.0f;
    float harvestTimer = 0.0f;

    void planNextAction(GameEngine& engine, const UI_resourceNodes& nodes, sf::Vector2f curPos);

public:
    UIBot() = default;

    void init(BotDifficulty diff);
    void reset();

    BotDifficulty getDifficulty() const { return difficulty; }
    bool isActive() const { return difficulty != BotDifficulty::NONE; }

    void update(float dt, GameEngine& engine, const UI_resourceNodes& nodes, sf::Vector2f& botPos,
                bool& outTriggerAction, bool& outTriggerUpgrade, BuildingType& outSelectedBuilding);
};

#endif // UI_BOT_H
