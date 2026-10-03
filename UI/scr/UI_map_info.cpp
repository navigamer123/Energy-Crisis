#include "../includes/UI_map.h"

// =============================================================================
// team info: glue between UI_map and the information UI (telemetry, notifications,
// event log, dashboard, post-match report, developer overlay)
// =============================================================================

void UI_map::onInfoEvent(const InfoEvent& ev) {
    stats.onInfoEvent(ev);
    if (ev.type == InfoEventType::LOST_LIGHTNING) {
        triggerPlayerPopup(ev.player, "МЪЛНИЯ!", "Унищожено съоръжение!",
                           "Мълния унищожи " + engine.getBuildingCost(ev.building).nameBg +
                               "!\nКлетката се освободи за нов строеж (ВЕЦ/друг).",
                           "[SPACE/Клик]: Постройте ново съоръжение", sf::Color(255, 230, 80));
    }
}

void UI_map::updateInfoUI(float dt) {
    (void)dt;
    pendingInfoEvents.clear();
    stats.afterEngineUpdate(engine, pendingInfoEvents);
    for (const InfoEvent& ev : pendingInfoEvents) {
        if (ev.type != InfoEventType::LOST_LIGHTNING) onInfoEvent(ev); // lightning arrives via its own hook
    }
}

void UI_map::resetInfoUI() {
    stats.reset();
    pendingInfoEvents.clear();
}
