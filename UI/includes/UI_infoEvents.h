#ifndef UI_INFO_EVENTS_H
#define UI_INFO_EVENTS_H

#include <string>
#include "../../Game/includes/game_main.h"

// -----------------------------------------------------------------------------
// team info: internal event hook for the information UI (toasts, event log, match
// statistics, post-match report). UI_map::dispatchEngineEvents() maps the engine's
// GameEvents (GameEngine::pollEvents) onto InfoEvents and feeds them to UI_map::onInfoEvent().
// -----------------------------------------------------------------------------
enum class InfoEventType {
    BUILT,            // player, building, value = how many
    DEMOLISHED,       // player, building, value = how many
    LOST_LIGHTNING,   // player, building (one building destroyed by a lightning strike)
    PLOT_BOUGHT,      // player, value = plots now owned
    MINE_UPGRADED,    // player, resource, value = new level
    MINED,            // player, resource, value = amount gathered (statistics only)
    DAY_SETTLED,      // value = ended day, value2 = outcome (see UI_matchStats::DayOutcome), share = P1 share after,
                      // demand, avgMW, text = the engine's day-end message
    GRACE_ENDED,      // value = first day with a real city demand, demand = that demand
    SEASON_CHANGED,   // value = static_cast<int>(SeasonType)
    WEATHER_CHANGED,  // player, value = static_cast<int>(WeatherType)
    MATCH_ENDED       // value = winner (1, 2, 3 = draw), share = final P1 share, text = the engine's message
};

struct InfoEvent {
    InfoEventType type = InfoEventType::BUILT;
    int player = 0;                         // 0 = both / nobody, 1 = West, 2 = East
    int value = 0;
    int value2 = 0;
    float share = 0.0f;
    int demand = 0;                         // DAY_SETTLED: the day's city demand (MW)
    int avgMW[2] = { 0, 0 };                // DAY_SETTLED: average MW delivered by P1 / P2 that day
    BuildingType building = BuildingType::NONE;
    ResourceType resource = ResourceType::NONE;
    std::string text;                       // DAY_SETTLED / MATCH_ENDED: Bulgarian message from the engine
};

#endif // UI_INFO_EVENTS_H
