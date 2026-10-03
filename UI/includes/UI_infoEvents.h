#ifndef UI_INFO_EVENTS_H
#define UI_INFO_EVENTS_H

#include "../../Game/includes/game_main.h"

// -----------------------------------------------------------------------------
// team info: internal event hook for the information UI (toasts, event log, match
// statistics, post-match report). Today the events are produced by UI_matchStats by
// comparing engine snapshots every frame, plus one explicit call from the lightning code.
// When the engine event queue (GameEngine::pollEvents) lands, the integrator can map
// those engine events onto InfoEvent and feed them to UI_map::onInfoEvent() instead.
// -----------------------------------------------------------------------------
enum class InfoEventType {
    BUILT,            // player, building, value = how many
    DEMOLISHED,       // player, value = how many
    LOST_LIGHTNING,   // player, building (one building destroyed by a lightning strike)
    PLOT_BOUGHT,      // player, value = plots now owned
    MINE_UPGRADED,    // player, resource, value = new level
    DAY_SETTLED,      // value = ended day, value2 = outcome (see UI_matchStats::DayOutcome), share = P1 share after
    GRACE_ENDED,      // value = first day with a real city demand
    SEASON_CHANGED,   // value = static_cast<int>(SeasonType)
    WEATHER_CHANGED,  // player, value = static_cast<int>(WeatherType)
    MATCH_ENDED       // value = winner (1, 2, 3 = draw)
};

struct InfoEvent {
    InfoEventType type = InfoEventType::BUILT;
    int player = 0;                         // 0 = both / nobody, 1 = West, 2 = East
    int value = 0;
    int value2 = 0;
    float share = 0.0f;
    BuildingType building = BuildingType::NONE;
    ResourceType resource = ResourceType::NONE;
};

#endif // UI_INFO_EVENTS_H
