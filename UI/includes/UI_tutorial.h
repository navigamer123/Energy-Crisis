#ifndef UI_TUTORIAL_H
#define UI_TUTORIAL_H

#include <SFML/Graphics.hpp>
#include <string>
#include "UI_types.h"
#include "../../Game/includes/game_balance.h"
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
    UPGRADE_MINE,
    COMPLETED
};

enum class ArrowDir {
    DOWN,
    UP,
    LEFT,
    RIGHT
};

class UI_tutorial {
private:
    TutorialStep p1Step = TutorialStep::WELCOME;
    TutorialStep p2Step = TutorialStep::WELCOME;
    bool p1Active = true;
    bool p2Active = true;
    bool isCoop = false;
    float animTimer = 0.0f;
    float p1StepDelayTimer = 0.0f;
    float p2StepDelayTimer = 0.0f;
    int initialP1BuildingCount = 0;
    int initialP2BuildingCount = 0;
    bool p1ExitBoostGranted = false;
    bool p2ExitBoostGranted = false;
    bool p1MineFunded = false;
    bool p2MineFunded = false;

    sf::FloatRect cardBounds; // Single-player centered card
    sf::FloatRect skipBtnBounds;
    sf::FloatRect nextBtnBounds;

    sf::FloatRect p1CardBounds; // Co-op Player 1 card (West)
    sf::FloatRect p1SkipBtnBounds;
    sf::FloatRect p1NextBtnBounds;

    sf::FloatRect p2CardBounds; // Co-op Player 2 card (East)
    sf::FloatRect p2SkipBtnBounds;
    sf::FloatRect p2NextBtnBounds;

    void drawSpotlight(sf::RenderWindow& window, sf::FloatRect targetRect, float animTime);
    void drawArrow(sf::RenderWindow& window, sf::Vector2f targetPos, const std::string& label,
                   const sf::Font& font, float animTime, ArrowDir dir = ArrowDir::DOWN,
                   sf::Color color = sf::Color(0, 240, 255));
    void drawPlayerCard(sf::RenderWindow& window, const sf::Font& font, int player,
                        TutorialStep step, const sf::FloatRect& bounds,
                        const sf::FloatRect& skipBtn, const sf::FloatRect& nextBtn,
                        const GameEngine& engine, float animTime);

public:
    UI_tutorial();

    void reset();
    void start();
    void skip(GameEngine* engine = nullptr);
    void skipP1(GameEngine* engine = nullptr);
    void skipP2(GameEngine* engine = nullptr);
    void grantExitBoost(int player, GameEngine& engine);
    void setCoop(bool coop) { isCoop = coop; }

    bool isActive() const {
        return (p1Active && p1Step != TutorialStep::INACTIVE) ||
               (isCoop && p2Active && p2Step != TutorialStep::INACTIVE);
    }
    bool isTutorialBlockingTime() const;
    TutorialStep getStep() const { return p1Step; }
    TutorialStep getP1Step() const { return p1Step; }
    TutorialStep getP2Step() const { return p2Step; }
    void setStep(TutorialStep step) { p1Step = step; p2Step = step; }
    std::string getStepDescription(int player, TutorialStep step, const GameEngine& engine) const;

    void update(float dt, GameEngine& engine);
    void draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
              const GameEngine& engine, const UI_resourceNodes& nodes, float animTime,
              sf::Vector2f mousePos, sf::Vector2f p1Pos, sf::Vector2f p2Pos);

    bool handleClick(sf::Vector2f mousePos, GameEngine* engine = nullptr);
    bool handleKey(sf::Keyboard::Key key, GameEngine* engine = nullptr);
    bool handleAction(int player, GameEngine* engine = nullptr);
    bool handleSkip(int player, GameEngine* engine = nullptr);
};

#endif // UI_TUTORIAL_H
