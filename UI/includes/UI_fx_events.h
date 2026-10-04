#ifndef UI_FX_EVENTS_H
#define UI_FX_EVENTS_H

// =============================================================================
// [b-effects] FxEventTracker: turns engine state changes into feedback events
// (mining, building, land, upgrades, settlements, victory, day/night, seasons).
// It compares snapshots of the public engine state once per frame, so it reacts
// to the human players AND the bot without hooks in every call site.
// Pure engine-level code (no drawing): covered by scratch/test_fx_events.cpp.
// When the wave-A engine event queue (pollEvents) lands, this class can be fed
// from it instead; the FxEvent consumers stay the same.
// =============================================================================

#include <SFML/Graphics.hpp>
#include <vector>
#include "../../Game/includes/game_main.h"

struct FxEvent {
    enum class Type {
        Mined,         // player, resource, amount
        Built,         // player, building, pos
        Removed,       // player, building, pos (demolished or destroyed)
        LandBought,    // player, area (plot bounds), pos = plot centre
        MineUpgraded,  // player, resource, amount = new level
        Settlement,    // player = side that gained city share (0 = unchanged), value = share delta for that side
        Victory,       // player = winner (3 = draw)
        DayBreak,      // daylight started
        Nightfall,     // night started
        SeasonChanged  // season
    };
    Type type = Type::Mined;
    int player = 0;
    ResourceType resource = ResourceType::NONE;
    BuildingType building = BuildingType::NONE;
    int amount = 0;
    float value = 0.0f;
    sf::Vector2f pos;
    sf::FloatRect area;
    SeasonType season = SeasonType::SPRING;
};

class FxEventTracker {
public:
    // Take a fresh snapshot without emitting events (match start / restart)
    void reset(const GameEngine& engine);
    // Compare with the previous snapshot, append events, keep the new snapshot
    void poll(const GameEngine& engine, std::vector<FxEvent>& out);
    bool hasSnapshot() const { return valid; }

private:
    struct BuildingKey {
        BuildingType type;
        int owner;
        sf::Vector2f pos;
    };
    struct Snapshot {
        int mineCount[2] = { 0, 0 };
        int mineLevels[2][8] = {};
        std::vector<BuildingKey> buildings;
        std::vector<int> purchasedPlotIds;
        float p1Share = 0.5f;
        int day = 1;
        bool daylight = true;
        SeasonType season = SeasonType::SPRING;
        int winner = 0;
    };
    Snapshot prev;
    bool valid = false;

    static Snapshot capture(const GameEngine& engine);
};

#endif // UI_FX_EVENTS_H
