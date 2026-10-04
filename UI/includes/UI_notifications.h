#ifndef UI_NOTIFICATIONS_H
#define UI_NOTIFICATIONS_H

#include <SFML/Graphics.hpp>
#include <deque>
#include <string>
#include <vector>
#include "UI_infoEvents.h"

// -----------------------------------------------------------------------------
// team info: notification system (UX-06)
//  - per-player stack of up to 3 toasts with priorities (critical toasts are never pushed
//    out by less important ones); the most important / newest toast is shown expanded
//  - proactive alerts (nightfall without lamps, settlement risk, low batteries, season change)
//  - an event log of the whole match (opened from the pause menu)
// -----------------------------------------------------------------------------
enum class ToastPriority { INFO = 0, WARNING = 1, CRITICAL = 2 };

class UI_notifications {
public:
    static constexpr int MAX_TOASTS = 3;
    static constexpr std::size_t MAX_LOG = 400;

    struct LogEntry {
        int day = 1;
        float hour = 0.0f;
        int player = 0;        // 0 = both / city
        ToastPriority priority = ToastPriority::INFO;
        std::string text;
    };

    void reset();

    // Show a toast in a player's corner. A non-empty channel replaces the toast already on that channel.
    // With addToLog the toast text is also written to the event log.
    void push(int player, ToastPriority priority, const std::string& channel, const std::string& badge,
              const std::string& title, const std::string& detail, const std::string& action,
              sf::Color accent, bool addToLog);
    void log(int player, ToastPriority priority, const std::string& text);

    void update(float dt, const GameEngine& engine, bool botActive);
    void onInfoEvent(const InfoEvent& ev, const GameEngine& engine, bool botActive);

    void draw(sf::RenderTarget& target, const sf::Font& font) const;
    void drawLog(sf::RenderTarget& target, const sf::Font& font) const;
    void scrollLog(int rows);
    void resetLogScroll() { logScroll = 0; }

    int toastCount() const { return static_cast<int>(toasts[0].size() + toasts[1].size()); }
    std::size_t logSize() const { return entries.size(); }
    const std::deque<LogEntry>& getLog() const { return entries; }

private:
    struct Toast {
        ToastPriority priority = ToastPriority::INFO;
        std::string channel;
        std::string badge;
        std::string title;
        std::string detail;
        std::string action;
        sf::Color accent = sf::Color::White;
        float timer = 4.0f;
        float maxTimer = 4.0f;
        float age = 0.0f;
        unsigned serial = 0;
        // Wrapped text, computed on first draw (the text never changes after push)
        mutable bool wrapped = false;
        mutable std::vector<sf::String> detailLines;
        mutable std::vector<sf::String> actionLines;
    };

    enum Alert { NIGHT_SOON = 0, SETTLEMENT_RISK, BATTERY_LOW, SEASON_TOMORROW, ALERT_COUNT };

    void evaluateAlerts(const GameEngine& engine, bool botActive);
    bool alertOnce(int player, Alert a, int day);
    bool isHuman(int player, bool botActive) const { return player == 1 || !botActive; }
    void drawPlayerStack(sf::RenderTarget& target, const sf::Font& font, int player) const;

    std::deque<Toast> toasts[2];
    std::deque<LogEntry> entries;
    int logScroll = 0;
    unsigned serialCounter = 0;
    int alertDay[2][ALERT_COUNT] = {};
    int opponentDay[2][3] = {}; // once-per-day memory for opponent alerts (land, hydro, mine level)
    int curDay = 1;
    float curHour = 8.0f;
};

#endif // UI_NOTIFICATIONS_H
