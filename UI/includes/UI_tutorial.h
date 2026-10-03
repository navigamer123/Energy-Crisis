#ifndef UI_TUTORIAL_H
#define UI_TUTORIAL_H

#include <SFML/Graphics.hpp>
#include <string>
#include "UI_types.h"
#include "../../Game/includes/game_main.h"
#include "UI_resourceNodes.h"

enum class TutorialStep {
    INACTIVE = 0,
    WELCOME,
    GATHER_WOOD,
    GATHER_IRON,
    GATHER_COPPER,
    GATHER_SILICON,
    SELECT_SOLAR,
    PLACE_SOLAR,
    COMPLETED
};

class UI_tutorial {
private:
    TutorialStep step = TutorialStep::WELCOME;
    bool active = true;
    float animTimer = 0.0f;
    float stepDelayTimer = 0.0f;
    int initialP1BuildingCount = 1;

    sf::FloatRect cardBounds;
    sf::FloatRect skipBtnBounds;
    sf::FloatRect nextBtnBounds;

    void drawArrow(sf::RenderWindow& window, sf::Vector2f targetPos, const std::string& label,
                   const sf::Font& font, float animTime, bool pointUp = false);

public:
    UI_tutorial();

    void reset();
    void start();
    void skip();

    bool isActive() const { return active && step != TutorialStep::INACTIVE; }
    TutorialStep getStep() const { return step; }

    void update(float dt, const GameEngine& engine);
    void draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
              const GameEngine& engine, const UI_resourceNodes& nodes, float animTime, sf::Vector2f mousePos);

    bool handleClick(sf::Vector2f mousePos);
    bool handleKey(sf::Keyboard::Key key);
};

#endif // UI_TUTORIAL_H
