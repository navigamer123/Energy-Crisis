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

    // Interactive Modal Popups with required [OK] dismissal
    struct PlayerModalDialog {
        bool active = false;
        std::string badge;
        std::string title;
        std::string detail;
        std::string tip;
        sf::FloatRect box;
        sf::FloatRect okBtn;
        sf::Color accentColor = sf::Color(255, 75, 75);
    };

    PlayerModalDialog p1Modal;
    PlayerModalDialog p2Modal;

    std::vector<FloatingNotice> notices;

    void drawGrassBackground(sf::RenderWindow& window);
    void drawPlayerCursors(sf::RenderWindow& window);
    void drawHUD(sf::RenderWindow& window);
    void drawFloatingNotices(sf::RenderWindow& window);
    void drawPlayerPopups(sf::RenderWindow& window);
    void drawPlayerModals(sf::RenderWindow& window);

    void updateControls(const sf::RenderWindow& window, float dt);
    void spawnNotice(const std::string& text, sf::Vector2f pos, sf::Color color);
    void triggerPlayerPopup(int player, const std::string& badge, const std::string& title,
                            const std::string& detail, const std::string& action, sf::Color accent);
    void triggerPlayerModal(int player, const std::string& badge, const std::string& title,
                            const std::string& detail, const std::string& tip, sf::Color accent = sf::Color(255, 75, 75));
    void closePlayerModal(int player);

    bool requestFullscreenToggle = false;
    bool showHelpOverlay = false;
    float lightningFlashTimer = 0.0f;

    struct WeatherParticle {
        sf::Vector2f pos;
        sf::Vector2f vel;
        float alpha;
        float size;
        int type; // 0 = Rain, 1 = Snow, 2 = Wind leaf, 3 = Star/Firefly
    };
    std::vector<WeatherParticle> particles;

    // Multi-input cooldown timers & edge-detection key states
    float p1ActionCooldown = 0.0f;
    float p2ActionCooldown = 0.0f;
    bool p1PrevE = false;
    bool p1PrevQ = false;
    bool p1PrevX = false;
    bool p1PrevNum[7] = {false, false, false, false, false, false, false};
    bool p2PrevPgDn = false;
    bool p2PrevPgUp = false;
    bool p2PrevDel = false;

    // Mining FX particles
    struct MiningParticle {
        sf::Vector2f pos;
        sf::Vector2f vel;
        sf::Color color;
        float life = 0.6f;
        float maxLife = 0.6f;
        float size = 3.0f;
    };
    std::vector<MiningParticle> miningParticles;

    void spawnMiningParticles(sf::Vector2f pos, sf::Color color, int count);
    void updateMiningParticles(float dt);
    void drawMiningParticles(sf::RenderWindow& window);
    void drawMiningZonesAndBadges(sf::RenderWindow& window);

    void executeP1Action();
    void executeP2Action();

    void updateWeatherParticles(float dt);
    void drawWeatherParticles(sf::RenderWindow& window);
    void drawEnergyConduits(sf::RenderWindow& window, float animTime);
    void drawHelpOverlay(sf::RenderWindow& window);

public:
    UI_map();
    ~UI_map();

    void setControlScheme(ControlScheme scheme);
    bool isMenuRequested() const { return requestMenu; }
    void resetMenuRequest() { requestMenu = false; }

    bool isFullscreenRequested() const { return requestFullscreenToggle; }
    void resetFullscreenRequest() { requestFullscreenToggle = false; }

    void handleEvent(const sf::Event& event, const sf::RenderWindow& window);
    void render(sf::RenderWindow& window);
};

#endif // UI_MAP_H
