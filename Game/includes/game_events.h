#pragma once
#include <string>

// =============================================================================
// ENERGY CRISIS - ENGINE EVENTS
// The engine records what happened (building placed, day settled, mine upgraded...) in a queue.
// The UI, audio, statistics or achievements drain it once per frame with GameEngine::pollEvents(),
// so they react to engine results without polling flags or parsing the Bulgarian outMsg strings.
// Bot actions go through the same engine calls, so they produce the same events as human input.
// =============================================================================

enum class GameEventType {
    DAY_END,            // value = day that just ended; text = empty
    DAY_RESULT,         // player = who won the day's territory (0 = nobody / both); value = P1 share change; text = day-end message
    BUILDING_PLACED,    // player, subtype = BuildingType, x/y = cell centre, value = base MW; text = building name
    BUILDING_REMOVED,   // player demolished it: player, subtype = BuildingType, x/y, value = refund fraction; text = name
    BUILDING_DESTROYED, // lightning / hazard: player = owner, subtype = BuildingType, x/y; text = name
    LAND_BOUGHT,        // player, subtype = plot id, x/y = plot centre, value = price in gold
    MINE_UPGRADED,      // player, subtype = ResourceType, value = new mine level
    RESOURCE_MINED,     // player, subtype = ResourceType, value = amount gained
    WEATHER_CHANGED,    // player = sector (1 West, 2 East), subtype = WeatherType, value = wind speed
    SEASON_CHANGED,     // subtype = SeasonType (flips at midnight before the new season's first day)
    VICTORY,            // player = winner code (1, 2, or 3 = draw), value = final P1 share; text = message
    MESSAGE             // player = 0 for both / 1 / 2; text = Bulgarian message for the HUD
};

struct GameEvent {
    GameEventType type;
    int player;         // 0 = both / none, 1 = West (P1), 2 = East (P2)
    float value;
    std::string text;
    int subtype = 0;    // BuildingType / ResourceType / WeatherType / SeasonType / plot id, see above
    float x = 0.0f;     // world position on the 1600x900 map when the event has one
    float y = 0.0f;
};
