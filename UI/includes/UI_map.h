#ifndef UI_MAP_H
#define UI_MAP_H

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include "UI_types.h"
#include "UI_clock.h"
#include "UI_city.h"
#include "UI_resourceHUD.h"
#include "UI_resourceNodes.h"
#include "UI_buildings.h"
#include "../../Game/includes/game_main.h"

class UI_map {
private:
    sf::Texture grassTexture;
    sf::Font font;
    bool resourcesLoaded;
    sf::Clock animClock;
    sf::Clock deltaClock;

    ControlScheme controlScheme;
    bool requestMenu;

    // Backend Game Engine
    GameEngine engine;

    // UI Sub-components
    UI_clock p1Clock;
    UI_clock p2Clock;
    UI_buildings p1Buildings;
    UI_buildings p2Buildings;
    UI_city city;
    UI_resourceHUD resourceHUD;
    UI_resourceNodes nodes;

    // Player cursor / drone positions and pulse animations
    sf::Vector2f p1Pos;
    sf::Vector2f p2Pos;
    float p1Pulse;
    float p2Pulse;

    // Player Side Popups (not floating in center of screen)
    struct PlayerPopup {
        std::string badge;      // e.g. "ИНФО", "ГРЕШКА", "СТРОЕЖ", "ДОБИВ", "ЗЕМЯ"
        std::string title;      // e.g. "Соларен панел"
        std::string detail;     // e.g. "Нужно: 35 Дърво, 30 Руда"
        std::string action;     // e.g. "SPACE/Клик: Постави | Q: Отказ"
        sf::Color accentColor;
        float timer = 0.0f;
        float maxTimer = 4.0f;
        bool active = false;
    };

    PlayerPopup p1Popup;
    PlayerPopup p2Popup;

    std::vector<FloatingNotice> notices;

    void drawGrassBackground(sf::RenderWindow& window);
    void drawPlayerCursors(sf::RenderWindow& window);
    void drawHUD(sf::RenderWindow& window);
    void drawFloatingNotices(sf::RenderWindow& window);
    void drawPlayerPopups(sf::RenderWindow& window);

    void updateControls(const sf::RenderWindow& window, float dt);
    void spawnNotice(const std::string& text, sf::Vector2f pos, sf::Color color);
    void triggerPlayerPopup(int player, const std::string& badge, const std::string& title,
                            const std::string& detail, const std::string& action, sf::Color accent);

public:
    UI_map();
    ~UI_map();

    void setControlScheme(ControlScheme scheme);
    bool isMenuRequested() const { return requestMenu; }
    void resetMenuRequest() { requestMenu = false; }

    void handleEvent(const sf::Event& event, const sf::RenderWindow& window);
    void render(sf::RenderWindow& window);
};

#endif // UI_MAP_H
