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
    showEventLog = false;
}

// -----------------------------------------------------------------------------
// Event log overlay (on top of the pause menu)
// -----------------------------------------------------------------------------

void UI_map::openEventLog() {
    showEventLog = true;
    notifications.resetLogScroll();
}

void UI_map::handleEventLogInput(const sf::Event& event) {
    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        switch (key->code) {
            case sf::Keyboard::Key::Escape:
            case sf::Keyboard::Key::L:
            case sf::Keyboard::Key::Enter:
            case sf::Keyboard::Key::Space:
            case sf::Keyboard::Key::Backspace:
                showEventLog = false; // back to the pause menu
                break;
            case sf::Keyboard::Key::Up:
            case sf::Keyboard::Key::W:
                notifications.scrollLog(-1);
                break;
            case sf::Keyboard::Key::Down:
            case sf::Keyboard::Key::S:
                notifications.scrollLog(1);
                break;
            case sf::Keyboard::Key::PageUp:
                notifications.scrollLog(-10);
                break;
            case sf::Keyboard::Key::PageDown:
                notifications.scrollLog(10);
                break;
            case sf::Keyboard::Key::Home:
                notifications.resetLogScroll();
                break;
            default:
                break;
        }
        return;
    }
    if (const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>()) {
        notifications.scrollLog(wheel->delta > 0.0f ? -3 : 3);
        return;
    }
    if (event.is<sf::Event::MouseButtonPressed>()) {
        showEventLog = false;
    }
}
