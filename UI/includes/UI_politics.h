#ifndef UI_POLITICS_H
#define UI_POLITICS_H

// =============================================================================
// [b-politics] City politics UI
//   City column (centre, under the city): today/tomorrow event chips (F-09), a news line,
//   the exchange ticker (F-31) and the contract board (F-16).
//   Player overlays (over each player's land): the council decision card (F-13) and the
//   city hall panel - КМЕТСТВО - with three tabs: exchange, power import, tender bids.
// Keys: P1 [C] opens the city hall, then W/S choose, A/D (or Q/E) switch tabs, SPACE = buy /
//       toggle / raise bid, X = sell / lower bid.  P2 [Home], arrows, ENTER, DEL, PgUp/PgDn.
//       In single player P1 may also use P2's keys. Council card: choose + SPACE / ENTER.
// Mouse: the player who owns the mouse clicks tabs, buttons, options, and the КМЕТСТВО buttons.
// =============================================================================

#include <SFML/Graphics.hpp>
#include <deque>
#include <string>
#include <vector>
#include "UI_types.h"
#include "../../Game/includes/game_main.h"

class UI_politics {
public:
    void reset();

    // Bot politics (single player) and news; call once per frame while the match runs
    void update(float dt, GameEngine& engine, bool botActive, BotDifficulty botDiff);

    void drawCityColumn(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine, float animTime,
                        sf::Vector2f mouse, int mouseOwner);
    void drawPlayerOverlays(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine, float animTime,
                            sf::Vector2f mouse, int mouseOwner, bool botActive);

    // Returns the player (1 or 2) whose key/click was used, or 0 when the event is not ours.
    // modalOpen[i]: that player's error dialog is open (it keeps priority over our overlays).
    int handleEvent(const sf::Event& event, GameEngine& engine, sf::Vector2f eventPos, int mouseOwner,
                    bool botActive, const bool modalOpen[2]);

    // True while the player's keys belong to a council card or the city hall panel
    // (their cursor must not move and their world actions must not fire)
    bool blocksPlayer(int player, const GameEngine& engine, bool botActive) const;
    bool isPanelOpen(int player) const { return (player == 1 || player == 2) && panelOpen[player - 1]; }

private:
    enum Action {
        ACT_NONE = 0,
        ACT_CLOSE,
        ACT_TAB,          // arg = tab
        ACT_BUY,          // arg = ResourceType
        ACT_SELL,         // arg = ResourceType
        ACT_TOGGLE_IMPORT,
        ACT_TOGGLE_EXPORT,
        ACT_BID_UP,
        ACT_BID_DOWN,
        ACT_COUNCIL,      // arg = option
        ACT_OPEN_PANEL,   // arg = player (board buttons)
        ACT_SWALLOW       // click inside an overlay that hit nothing
    };
    enum Role { R_NONE = 0, R_UP, R_DOWN, R_LEFT, R_RIGHT, R_PRIMARY, R_SECONDARY, R_TOGGLE };
    static constexpr int TAB_MARKET = 0;
    static constexpr int TAB_IMPORT = 1;
    static constexpr int TAB_TENDER = 2;
    static constexpr int TAB_COUNT = 3;

    struct HitBox {
        sf::FloatRect rect;
        int action;
        int arg;
    };
    struct Toast {
        std::string text;
        Politics::Tone tone;
        float timer;
    };

    bool panelOpen[2] = { false, false };
    int tab[2] = { 0, 0 };
    int row[2][TAB_COUNT] = { { 0, 0, 0 }, { 0, 0, 0 } };
    int councilSel[2] = { 2, 2 };
    bool councilWasActive = false;
    std::vector<HitBox> hits[2];   // overlay buttons drawn last frame, per player
    sf::FloatRect overlayRect[2];  // the overlay box itself (clicks inside are ours)
    std::vector<HitBox> boardHits; // КМЕТСТВО buttons under the contract board
    std::deque<Toast> toasts;
    float botThinkTimer = 0.0f;
    bool lastBotActive = false;    // single player: P2 is the bot (board shows one button)
    std::string flash[2];
    float flashTimer[2] = { 0.0f, 0.0f };
    bool flashOk[2] = { true, true };

    Role keyRole(int player, sf::Keyboard::Key code, bool botActive) const;
    void doAction(int player, int action, int arg, GameEngine& engine);
    void setFlash(int player, const std::string& msg, bool ok);
    int tenderId(const GameEngine& engine) const;

    void drawEventChips(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine, float animTime);
    void drawNewsLine(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine);
    void drawTicker(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine);
    void drawBoard(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine, sf::Vector2f mouse, int mouseOwner);
    void drawCouncilCard(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine, int player, float animTime,
                         sf::Vector2f mouse, bool mouseMine);
    void drawCouncilWaiting(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine, int player, bool isBot);
    void drawPanel(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine, int player, sf::Vector2f mouse,
                   bool mouseMine, bool botActive);
    void drawMarketTab(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine, int player, sf::FloatRect area,
                       sf::Vector2f mouse, bool mouseMine);
    void drawImportTab(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine, int player, sf::FloatRect area,
                       sf::Vector2f mouse, bool mouseMine);
    void drawTenderTab(sf::RenderTarget& t, const sf::Font& font, const GameEngine& engine, int player, sf::FloatRect area,
                       sf::Vector2f mouse, bool mouseMine);
};

#endif // UI_POLITICS_H
