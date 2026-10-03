#include "../includes/UI_map.h"

// =============================================================================
// team info: glue between UI_map and the information UI (telemetry, notifications,
// event log, dashboard, post-match report, developer overlay)
// =============================================================================

void UI_map::onInfoEvent(const InfoEvent& ev) {
    stats.onInfoEvent(ev);
    notifications.onInfoEvent(ev, engine, bot.isActive());
}

void UI_map::updateInfoUI(float dt) {
    pendingInfoEvents.clear();
    stats.afterEngineUpdate(engine, pendingInfoEvents);
    for (const InfoEvent& ev : pendingInfoEvents) {
        if (ev.type != InfoEventType::LOST_LIGHTNING) onInfoEvent(ev); // lightning arrives via its own hook
    }
    // Toast timers and alerts only run while the match itself runs
    const bool running = !isPaused && !showHelpOverlay && engine.getCityState().winner == 0;
    notifications.update(running ? dt : 0.0f, engine, bot.isActive());
}

void UI_map::resetInfoUI() {
    stats.reset();
    notifications.reset();
    pendingInfoEvents.clear();
}
