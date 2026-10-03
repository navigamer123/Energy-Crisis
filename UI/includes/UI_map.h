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
#include "UI_bot.h"
#include "UI_tutorial.h"
#include "UI_economyHUD.h" // [b-economy] city economy dashboard, districts, grid alerts, Day Report
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
    UIBot bot;
    UI_tutorial tutorial;
    UI_economyHUD economyHUD; // [b-economy]

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
    float lightningStrikeCooldown = 5.0f;   // West sector (P1) strike schedule, game seconds
    float lightningStrikeCooldownP2 = 5.0f; // East sector (P2) strike schedule, game seconds

    struct ActiveLightning {
        sf::Vector2f startPos;
        sf::Vector2f targetPos;
        std::vector<sf::Vector2f> mainBolt;
        std::vector<std::vector<sf::Vector2f>> branches;
        float lifetime = 0.0f;
        float maxLifetime = 0.32f;
        bool hitBuilding = false;
        sf::Color color = sf::Color(220, 245, 255);
    };
    std::vector<ActiveLightning> activeLightnings;
    void triggerLightningStrike(sf::Vector2f targetPos, bool hitBuilding);

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
    float p1ResourceCooldown = 0.0f; // 1.0s cooldown between resource harvests
    float p2ResourceCooldown = 0.0f; // 1.0s cooldown between resource harvests
    float p1SelectCooldown = 0.0f;   // Debounce cooldown for building selection
    float p2SelectCooldown = 0.0f;   // Debounce cooldown for building selection
    bool p1PrevAction = false;       // Single-press edge trigger (no continuous holding)
    bool p2PrevAction = false;       // Single-press edge trigger (no continuous holding)
    bool p1PrevE = false;
    bool p1PrevQ = false;
    bool p1PrevX = false;
    bool p1PrevNum[7] = {false, false, false, false, false, false, false};
    bool p2PrevPgDn = false;
    bool p2PrevPgUp = false;
    bool p2PrevDel = false;

    // Discrete grid movement stepping for building placement
    float p1GridStepCooldown = 0.0f;
    float p2GridStepCooldown = 0.0f;
    int p1GridCol = 0, p1GridRow = 0;
    int p2GridCol = 0, p2GridRow = 0;

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
    void executeP1Upgrade();
    void executeP2Upgrade();

    void updateWeatherParticles(float dt);
    void drawWeatherParticles(sf::RenderWindow& window);
    void drawEnergyConduits(sf::RenderWindow& window, float animTime);
    void drawHelpOverlay(sf::RenderWindow& window);
    void drawVictoryScreen(sf::RenderWindow& window);
    void drawPauseMenu(sf::RenderWindow& window);
    bool isPosOnPurchasedLand(int player, sf::Vector2f pos) const;

    sf::FloatRect victoryRestartBtn;
    sf::FloatRect victoryMenuBtn;

    // Pause Menu state & button bounds
    bool isPaused = false;
    int pauseSelectedIdx = 0;
    sf::Vector2f lastPauseMousePos = { -999.0f, -999.0f };
    sf::FloatRect pauseResumeBtn;
    sf::FloatRect pauseRestartBtn;
    sf::FloatRect pauseHelpBtn;
    sf::FloatRect pauseMenuBtn;

    // Input ownership & per-match input state (UI_map_controls.cpp)
    static constexpr float TUTORIAL_BOT_HOLD_SEC = 60.0f; // Max real seconds an unfinished tutorial keeps the bot idle
    float tutorialBotHoldLeft = TUTORIAL_BOT_HOLD_SEC;
    bool p1PrevUpgrade = false;         // Upgrade-key edge flags (were function-local statics)
    bool p2PrevUpgrade = false;
    bool helpOpenedFromPause = false;   // Closing help returns to this pause state
    int mouseOwnerAt(sf::Vector2f pos) const;                          // 0 = nobody, 1 = P1, 2 = P2
    bool isModalDismissKey(int player, sf::Keyboard::Key code) const;  // That player's own confirm/cancel keys

public:
    UI_map();
    ~UI_map();

    void setControlScheme(ControlScheme scheme);
    void setBotDifficulty(BotDifficulty diff);
    BotDifficulty getBotDifficulty() const { return bot.getDifficulty(); }
    bool isBotActive() const { return bot.isActive(); }
    bool isMenuRequested() const { return requestMenu; }
    void resetMenuRequest() { requestMenu = false; }

    bool isFullscreenRequested() const { return requestFullscreenToggle; }
    void resetFullscreenRequest() { requestFullscreenToggle = false; }

    bool isTutorialActive() const { return tutorial.isActive(); }
    void startTutorial() { tutorial.start(); }
    void skipTutorial() { tutorial.skip(); }

    const GameEngine& getEngine() const { return engine; }

    void handleEvent(const sf::Event& event, const sf::RenderWindow& window);
    void render(sf::RenderWindow& window);
    void restartMatch();

    // Input resync (UI_map_controls.cpp)
    void primeInputEdges(int player = 0); // Keys held right now are not fresh presses (0 = both players)
    void resetMatchInputState();          // Call after restartMatch()/setBotDifficulty() when a match starts
    void onFocusLost();                   // Auto-pause when the window loses focus
};

#endif // UI_MAP_H
