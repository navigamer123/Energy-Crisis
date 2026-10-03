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
    stats.sample(engine); // discrete events arrive through dispatchEngineEvents() (UI_map_events.cpp)
    // Toast timers and alerts only run while the match itself runs
    const bool running = !isPaused && !showHelpOverlay && engine.getCityState().winner == 0;
    notifications.update(running ? dt : 0.0f, engine, bot.isActive());
}

void UI_map::resetInfoUI() {
    stats.reset();
    notifications.reset();
    showEventLog = false;
    dashboardOpenedAt = -1.0f;
    postMatch.reset();
}

// -----------------------------------------------------------------------------
// Energy dashboard: shown while [Tab] is held during the running match
// -----------------------------------------------------------------------------

void UI_map::drawDashboardIfHeld(sf::RenderWindow& window) {
    const bool tabDown = forceDashboard || (window.hasFocus() && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Tab));
    const bool held = resourcesLoaded && tabDown && !isPaused && !showHelpOverlay && engine.getCityState().winner == 0;
    const float now = animClock.getElapsedTime().asSeconds();
    if (!held) {
        dashboardOpenedAt = -1.0f;
        return;
    }
    // Screenshot mode opens it "long ago", so the reveal animation has already finished
    if (dashboardOpenedAt < 0.0f) dashboardOpenedAt = forceDashboard ? now - 10.0f : now;
    dashboard.draw(window, font, engine, stats, now - dashboardOpenedAt, now);
}

// -----------------------------------------------------------------------------
// Developer overlay ([F3])
// -----------------------------------------------------------------------------

void UI_map::drawDevOverlay(sf::RenderWindow& window) {
    if (!devOverlay.isVisible() || !resourcesLoaded) return;
    UI_devOverlay::Counts c;
    for (const auto& b : engine.getBuildings()) {
        if (b.playerOwner == 1) ++c.buildingsP1;
        else if (b.playerOwner == 2) ++c.buildingsP2;
    }
    c.weatherParticles = static_cast<int>(particles.size());
    c.miningParticles = static_cast<int>(miningParticles.size());
    c.notices = static_cast<int>(notices.size());
    c.lightnings = static_cast<int>(activeLightnings.size());
    c.toasts = notifications.toastCount();
    c.samples = stats.getSamples().size();
    c.logEntries = notifications.logSize();
    devOverlay.draw(window, font, engine, c, isPaused || engine.getCityState().winner != 0);
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
