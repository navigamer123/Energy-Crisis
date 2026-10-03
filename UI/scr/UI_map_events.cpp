#include "../includes/UI_map.h"
#include <algorithm>

// =============================================================================
// Engine events: the single engine.pollEvents() drain of the UI.
// render() calls dispatchEngineEvents() once per frame, after the simulation step (and outside
// it, so the frame that ends the match or follows a restart is drained too). Every consumer
// (information UI, effects, audio...) is fed from here, so nothing reacts twice to one event and
// nobody has to diff engine snapshots. The acting player's own feedback (popups, notices) stays
// at the call site; the consumers here log, alert the opponent, count and add juice.
// =============================================================================

namespace {
int ownedPlots(const GameEngine& engine, int player) {
    int n = 0;
    for (const auto& plot : engine.getLandPlots()) n += (plot.isPurchased && plot.playerOwner == player) ? 1 : 0;
    return n;
}
} // namespace

void UI_map::resetEventConsumers() {
    evCtx = EngineEventCtx();
    evCtx.share = engine.getCityState().p1CityShare;
    resetInfoUI(); // team info: telemetry, toasts, event log, report
}

void UI_map::settleSetupEvents() {
    dispatchEngineEvents(EventDispatch::Silent);
}

void UI_map::dispatchEngineEvents(EventDispatch mode) {
    const std::vector<GameEvent> events = engine.pollEvents();
    if (events.empty()) return;
    const bool live = (mode == EventDispatch::Live);
    // On the final day the victory message announces the result; the day verdict only goes to the statistics
    const bool matchOver = std::any_of(events.begin(), events.end(),
                                       [](const GameEvent& e) { return e.type == GameEventType::VICTORY; });

    // team info: announce = toasts and log lines; otherwise statistics only
    auto info = [&](const InfoEvent& ie, bool announce) {
        if (live && announce) onInfoEvent(ie);
        else stats.onInfoEvent(ie);
    };

    for (const GameEvent& ev : events) {
        const int p = ev.player;
        InfoEvent ie;
        ie.player = p;
        switch (ev.type) {
            case GameEventType::BUILDING_PLACED:
                ie.type = InfoEventType::BUILT;
                ie.value = 1;
                ie.building = static_cast<BuildingType>(ev.subtype);
                info(ie, true);
                break;
            case GameEventType::BUILDING_REMOVED:
                ie.type = InfoEventType::DEMOLISHED;
                ie.value = 1;
                ie.building = static_cast<BuildingType>(ev.subtype);
                info(ie, true);
                break;
            case GameEventType::BUILDING_DESTROYED: // lightning (breakBuildingAt)
                ie.type = InfoEventType::LOST_LIGHTNING;
                ie.building = static_cast<BuildingType>(ev.subtype);
                info(ie, true);
                break;
            case GameEventType::LAND_BOUGHT:
                ie.type = InfoEventType::PLOT_BOUGHT;
                ie.value = ownedPlots(engine, p);
                info(ie, true);
                break;
            case GameEventType::MINE_UPGRADED:
                ie.type = InfoEventType::MINE_UPGRADED;
                ie.resource = static_cast<ResourceType>(ev.subtype);
                ie.value = static_cast<int>(ev.value);
                info(ie, true);
                break;
            case GameEventType::RESOURCE_MINED:
                ie.type = InfoEventType::MINED;
                ie.resource = static_cast<ResourceType>(ev.subtype);
                ie.value = static_cast<int>(ev.value);
                info(ie, false);
                break;
            case GameEventType::WEATHER_CHANGED: // re-rolled every day: announce real changes only
                if (p < 1 || p > 2 || evCtx.weather[p - 1] == ev.subtype) break;
                evCtx.weather[p - 1] = ev.subtype;
                ie.type = InfoEventType::WEATHER_CHANGED;
                ie.value = ev.subtype;
                info(ie, true);
                break;
            case GameEventType::SEASON_CHANGED:
                ie.type = InfoEventType::SEASON_CHANGED;
                ie.player = 0;
                ie.value = ev.subtype;
                info(ie, true);
                break;
            case GameEventType::DAY_END:
                evCtx.endedDay = static_cast<int>(ev.value);
                break;
            case GameEventType::DAY_RESULT: {
                using DO = UI_matchStats::DayOutcome;
                const int day = evCtx.endedDay;
                const int demand = ev.subtype; // 0 on a grace day
                const int avg1 = static_cast<int>(ev.x), avg2 = static_cast<int>(ev.y);
                const bool grace = (demand <= 0);
                const bool p1Met = grace || avg1 >= demand, p2Met = grace || avg2 >= demand;
                evCtx.share = std::clamp(evCtx.share + ev.value, 0.0f, 1.0f); // the engine clamps the same way
                ie.type = InfoEventType::DAY_SETTLED;
                ie.player = 0;
                ie.value = day;
                ie.value2 = static_cast<int>(grace ? DO::GRACE
                                             : p == 1 ? DO::P1_TOOK
                                             : p == 2 ? DO::P2_TOOK
                                             : (p1Met && p2Met) ? DO::BOTH_MET : DO::NONE_MET);
                ie.share = evCtx.share;
                ie.demand = demand;
                ie.avgMW[0] = avg1;
                ie.avgMW[1] = avg2;
                ie.text = ev.text;
                info(ie, !matchOver);
                if (day > 0 && day == engine.getConfig().graceDays && !matchOver) {
                    InfoEvent g;
                    g.type = InfoEventType::GRACE_ENDED;
                    g.value = day + 1;
                    g.demand = engine.getCityState().cityEnergyDemand;
                    info(g, true);
                }
                break;
            }
            case GameEventType::VICTORY:
                ie.type = InfoEventType::MATCH_ENDED;
                ie.player = 0;
                ie.value = p;
                ie.share = ev.value;
                ie.text = ev.text;
                info(ie, true);
                break;
            case GameEventType::MESSAGE:
                if (live && !ev.text.empty()) notifications.log(p, ToastPriority::INFO, ev.text);
                break;
        }
    }
}
