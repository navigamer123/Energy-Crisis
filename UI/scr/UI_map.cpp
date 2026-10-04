#include "../includes/UI_map.h"
#include "../includes/UI_text.h"
#include "../includes/UI_shot.h"
#include "../includes/UI_theme.h"
#include <cmath>
#include <iostream>
#include <algorithm>

UI_map::UI_map()
    : resourcesLoaded(false),
      controlScheme(ControlScheme::BOTH_KEYBOARD),
      requestMenu(false),
      p1Clock(1),
      p2Clock(2),
      p1Buildings(1, { 18.0f, 115.0f }, { 230.0f, 395.0f }, theme::P1),
      p2Buildings(2, { 1600.0f - 248.0f, 115.0f }, { 230.0f, 395.0f }, theme::P2),
      p1Pos(450.0f, 450.0f),
      p2Pos(1150.0f, 450.0f),
      p1Pulse(0.0f),
      p2Pulse(0.0f),
      requestFullscreenToggle(false),
      showHelpOverlay(false),
      lightningFlashTimer(0.0f) {
    if (backgroundTexture.loadFromFile("assets/background.png")) {
        backgroundTexture.setSmooth(true); // 1920x1080 artwork scaled down to the canvas
    } else {
        std::cerr << "[UI_map] Warning: Failed to load assets/background.png (plain ground colour shown instead)\n";
    }

    if (font.openFromFile("assets/font.ttf")) {
        resourcesLoaded = true;
    } else {
        std::cerr << "[UI_map] ERROR: Failed to load assets/font.ttf - all text will be missing; "
                     "the tutorial and modal dialogs are disabled so they cannot block the game.\n";
    }

    // Initialize weather particles
    particles.resize(130);
    for (size_t i = 0; i < particles.size(); ++i) {
        particles[i].pos = sf::Vector2f(static_cast<float>(rand() % 1600), static_cast<float>(rand() % 900));
        particles[i].vel = sf::Vector2f(-60.0f, 520.0f);
        particles[i].alpha = 140.0f + (rand() % 100);
        particles[i].size = 2.0f + (rand() % 3);
        particles[i].type = 0;
    }

    // Initialize backend game engine at 1600x900
    engine.init(1600.0f, 900.0f);
    // Real-time host: a stalled frame drops its backlog instead of running hundreds of steps
    engine.setMaxStepsPerUpdate(GameEngine::RECOMMENDED_MAX_STEPS_PER_UPDATE);
    std::cout << "[UI_map] 1600x900 map orchestrator with backend GameEngine ready.\n";
}

UI_map::~UI_map() {
    std::cout << "[UI_map] SFML Map component destroyed.\n";
}

void UI_map::setControlScheme(ControlScheme scheme) {
    controlScheme = scheme;
    p1Pos = { 450.0f, 450.0f };
    p2Pos = { 1150.0f, 450.0f };
    p1Pulse = 0.0f;
    p2Pulse = 0.0f;
    requestMenu = false;
    std::cout << "[UI_map] Active Control Scheme set to: " << static_cast<int>(scheme) << "\n";
}

void UI_map::setBotDifficulty(BotDifficulty diff) {
    bot.init(diff);
    // Team b-session (F-06): the tutorial policy from НАСТРОЙКИ decides (АВТОМАТИЧНО: until it was
    // finished or skipped once per mode, never on HARD; ВИНАГИ; НИКОГА). Co-op / single-player hints.
    tutorial.setCoop(diff == BotDifficulty::NONE);
    if (resourcesLoaded && shouldShowTutorial(diff)) {
        tutorial.start();
        tutorialPolicyWatch = true;
        tutorialWatchCoop = (diff == BotDifficulty::NONE);
    } else {
        // Also without a font: the tutorial cannot be drawn, so never leave it active (it would
        // invisibly swallow input and keep the bot frozen).
        tutorial.skip();
        tutorialPolicyWatch = false;
    }
    std::cout << "[UI_map] Bot difficulty set to: " << static_cast<int>(diff) << "\n";
}

void UI_map::drawBackground(sf::RenderWindow& window) {
    float screenWidth = VIRTUAL_WIDTH;
    float screenHeight = VIRTUAL_HEIGHT;

    if (backgroundTexture.getSize().x > 0) {
        // Stretch the artwork over the canvas and darken it (the source is a bright lime green)
        // so the river, the city, the plots and their labels stay readable on top.
        sf::Sprite sprite(backgroundTexture);
        sf::Vector2f texSize(backgroundTexture.getSize());
        sprite.setScale({ screenWidth / texSize.x, screenHeight / texSize.y });
        sprite.setPosition({ 0.0f, 0.0f });
        sprite.setColor(theme::GroundTint);
        window.draw(sprite);
    } else {
        sf::RectangleShape ground({ screenWidth, screenHeight });
        ground.setPosition({ 0.0f, 0.0f });
        ground.setFillColor(theme::GroundFallback);
        window.draw(ground);
    }

    // Dynamic atmospheric tint based on seasonal adaptive sunrise & sunset
    float hour = engine.getHour24();
    float sunrise = engine.getSunriseHour();
    float sunset = engine.getSunsetHour();

    sf::RectangleShape skyOverlay({ screenWidth, screenHeight });
    skyOverlay.setPosition({ 0.0f, 0.0f });

    // 1. Dawn / Sunrise transition (morning warm rose/peach glow)
    if (hour >= (sunrise - 0.75f) && hour < (sunrise + 0.5f)) {
        float t = (hour - (sunrise - 0.75f)) / 1.25f;
        float intensity = std::sin(t * 3.14159f);
        skyOverlay.setFillColor(theme::withAlpha(theme::Dawn, static_cast<std::uint8_t>(50 * intensity)));
        window.draw(skyOverlay);
    }
    // 2. Dusk / Sunset transition (warm amber/crimson evening glow)
    else if (hour >= (sunset - 0.75f) && hour <= (sunset + 0.85f)) {
        float t = (hour - (sunset - 0.75f)) / 1.6f;
        float intensity = std::sin(t * 3.14159f);
        skyOverlay.setFillColor(theme::withAlpha(theme::Dusk, static_cast<std::uint8_t>(65 * intensity)));
        window.draw(skyOverlay);
    }
    // 3. Nighttime (deep midnight indigo overlay)
    else if (hour > (sunset + 0.85f) || hour < (sunrise - 0.75f)) {
        std::uint8_t nightAlpha = 110;
        if (engine.getSeason() == SeasonType::WINTER) {
            nightAlpha = 130; // Darker winter nights
        }
        skyOverlay.setFillColor(theme::withAlpha(theme::Night, nightAlpha));
        window.draw(skyOverlay);
    }
}

void UI_map::drawEnergyConduits(sf::RenderWindow& window, float animTime) {
    const auto& bList = engine.getBuildings();
    if (bList.empty()) return;

    // Conduits end at the city's bottom edge (not on the border tag below it)
    sf::Vector2f cityEntranceP1(730.0f, 392.0f);
    sf::Vector2f cityEntranceP2(870.0f, 392.0f);

    for (size_t i = 0; i < bList.size(); ++i) {
        const auto& b = bList[i];
        if (b.type == BuildingType::LAMP) continue;

        sf::Vector2f dest = (b.playerOwner == 1) ? cityEntranceP1 : cityEntranceP2;
        sf::Color conduitColor = theme::withAlpha(theme::player(b.playerOwner), 90);
        sf::Color packetColor = theme::withAlpha(theme::playerLight(b.playerOwner), 230);

        // Draw base conduit line
        sf::Vertex conduitLine[2];
        conduitLine[0].position = b.position;
        conduitLine[0].color = conduitColor;
        conduitLine[1].position = dest;
        conduitLine[1].color = conduitColor;
        window.draw(conduitLine, 2, sf::PrimitiveType::Lines);

        // Draw animated energy pulse packets traveling along the conduit
        sf::Vector2f delta = dest - b.position;
        float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        if (dist > 10.0f) {
            int numPackets = std::max(1, static_cast<int>(dist / 140.0f));
            for (int k = 0; k < numPackets; ++k) {
                float offset = static_cast<float>(k) / static_cast<float>(numPackets);
                float progress = std::fmod(animTime * 0.8f + offset + (static_cast<float>(i) * 0.17f), 1.0f);
                sf::Vector2f packetPos = b.position + delta * progress;

                sf::CircleShape packet(3.5f);
                packet.setOrigin({ 3.5f, 3.5f });
                packet.setPosition(packetPos);
                packet.setFillColor(packetColor);
                window.draw(packet);

                // Subtle energy packet aura
                sf::CircleShape aura(7.0f);
                aura.setOrigin({ 7.0f, 7.0f });
                aura.setPosition(packetPos);
                aura.setFillColor(theme::withAlpha(packetColor, 65));
                window.draw(aura);
            }
        }
    }
}

void UI_map::restartMatch() {
    engine.restartGame();

    // --- Reset every per-match UI state (same result for menu starts and in-match restarts) ---
    // Overlays
    isPaused = false;
    pauseSelectedIdx = 0;
    lastPauseMousePos = { -999.0f, -999.0f };
    showHelpOverlay = false;
    requestMenu = false;

    // Cursors spawn exactly like a menu start (see setControlScheme)
    p1Pos = { 450.0f, 450.0f };
    p2Pos = { 1150.0f, 450.0f };
    p1Pulse = 0.0f;
    p2Pulse = 0.0f;
    p1GridStepCooldown = 0.0f;
    p2GridStepCooldown = 0.0f;
    engine.getClosestGridIndex(1, p1Pos, p1GridCol, p1GridRow);
    engine.getClosestGridIndex(2, p2Pos, p2GridCol, p2GridRow);

    // Cooldowns
    p1ResourceCooldown = 0.0f;
    p2ResourceCooldown = 0.0f;
    p1ActionCooldown = 0.0f;
    p2ActionCooldown = 0.0f;
    p1SelectCooldown = 0.0f;
    p2SelectCooldown = 0.0f;

    // Dialogs, popups & effects
    p1Modal.active = false;
    p2Modal.active = false;
    p1Popup.active = false;
    p2Popup.active = false;
    notices.clear();
    miningParticles.clear();

    // Lightning
    activeLightnings.clear();
    lightningFlashTimer = 0.0f;
    lightningStrikeCooldown = 5.0f;
    lightningStrikeCooldownP2 = 5.0f;

    // Re-apply the chosen mode & difficulty: re-inits the bot and shows the tutorial only where
    // that mode wants it (never on HARD), with the matching co-op / single-player hints.
    tutorial.reset();
    setBotDifficulty(bot.getDifficulty());

    // Per-match input state owned by UI_map_controls.cpp: bot/tutorial hold timer, help origin, and
    // every key edge flag primed, so the Enter/Space/R that started this match (menu, pause menu or
    // victory screen) is not seen as a fresh in-game press. Idempotent, so callers may repeat it.
    resetMatchInputState();

    // Do not let the time spent in menus/pause leak into the first frame of the new match
    deltaClock.restart();

    // Team b-session (F-02, F-18): a real match is now in memory (ПРОДЪЛЖИ, autosaves)
    matchStarted = true;
    autosaveDay = engine.getCurrentDay();
    settingsOverlay.close();
    savePanelOpen = false;

    spawnNotice("НОВА ИГРА СТАРТИРАНА!", { 800.0f, 450.0f }, theme::Good);
}

bool UI_map::isPosOnPurchasedLand(int player, sf::Vector2f pos) const {
    for (const auto& plot : engine.getLandPlots()) {
        if (plot.playerOwner == player && plot.isPurchased && plot.bounds.contains(pos)) {
            return true;
        }
    }
    return false;
}

void UI_map::render(sf::RenderWindow& window) {
    float dt = ui::shot::frameDt(deltaClock.restart().asSeconds());
    if (dt > 0.05f) dt = 0.05f;

    // 1. Advance continuous backend simulation (only when NOT paused and game not won)
    if (!isPaused && engine.getCityState().winner == 0) {
        engine.update(dt);
        updateControls(window, dt);
        updateWeatherParticles(dt);
        tutorial.update(dt, engine);
    }
    updateSession(dt); // Team b-session: day-end autosave, tutorial policy, gamepad assignment

    // Screenshot storm scene: fire one harmless bolt into the stormy sector just before the capture
    if (debugBoltCountdown >= 0 && debugBoltCountdown-- == 0) {
        bool westStorm = engine.getPlayerWeather(1) == WeatherType::STORMY;
        triggerLightningStrike(westStorm ? sf::Vector2f(430.0f, 470.0f) : sf::Vector2f(1170.0f, 520.0f), false);
    }

    // Without a font, modal dialogs and the tutorial cannot be drawn: never leave an invisible
    // dialog/tutorial blocking input (or freezing the bot).
    if (!resourcesLoaded) {
        p1Modal.active = false;
        p2Modal.active = false;
        if (tutorial.isActive()) tutorial.skip();
    }

    // 2. Synchronize clock displays with continuous time and dynamic weather
    p1Clock.setHour(engine.getHour24());
    p1Clock.setDay(engine.getCurrentDay());
    p1Clock.setWeather(engine.getPlayerWeather(1));
    p1Clock.setSeason(engine.getSeason());
    p1Clock.setTimeScale(engine.getTimeScale());

    p2Clock.setHour(engine.getHour24());
    p2Clock.setDay(engine.getCurrentDay());
    p2Clock.setWeather(engine.getPlayerWeather(2));
    p2Clock.setSeason(engine.getSeason());
    p2Clock.setTimeScale(engine.getTimeScale());

    float animTime = ui::shot::clockSeconds(animClock.getElapsedTime().asSeconds());
    sf::Vector2f mousePos = ui::pointerPos(window);

    // 3. Render terrain & atmosphere
    drawBackground(window);

    // 4. Central dividing line & river
    city.drawDividingRiver(window, font, resourcesLoaded, animTime, engine.isDaylight());

    // 5. Purchasable Land Plots Grid
    nodes.drawLandPlots(window, font, resourcesLoaded, engine.getLandPlots(), mousePos, engine.getBuildings());

    // Dynamic glowing energy conduit lines connecting generators to metropolis
    drawEnergyConduits(window, animTime);

    // 6. Placed Buildings on the Map
    nodes.drawPlacedBuildings(window, font, resourcesLoaded, engine.getBuildings());

    // 7. Holographic ghost preview if building is selected (snapped to plot grid, hidden if outside purchased land).
    //    The ghost's tooltip is drawn later (after the city and the mines) so nothing covers it.
    struct GhostInfo { bool shown = false; BuildingType type = BuildingType::NONE; sf::Vector2f pos; bool valid = false; };
    GhostInfo ghosts[2];
    BuildingType p1Sel = engine.getSelectedBuilding(1);
    BuildingType p2Sel = engine.getSelectedBuilding(2);
    for (int player = 1; player <= 2; ++player) {
        BuildingType sel = (player == 1) ? p1Sel : p2Sel;
        sf::Vector2f cursor = (player == 1) ? p1Pos : p2Pos;
        if (sel == BuildingType::NONE) continue;
        sf::Vector2f targetPos = (sel == BuildingType::DEMOLISH) ? cursor : engine.snapToBuildingGrid(player, cursor);
        if (sel == BuildingType::DEMOLISH || isPosOnPurchasedLand(player, targetPos)) {
            std::string reason;
            bool valid = engine.canPlaceBuilding(player, sel, targetPos, reason);
            nodes.drawBuildingGhost(window, font, resourcesLoaded, sel, targetPos, valid, engine.getBuildingCost(sel));
            ghosts[player - 1] = { true, sel, targetPos, valid };
        }
    }

    // 8. Compact Metropolis City Center with territorial slicing & conquest
    city.drawCity(window, font, resourcesLoaded, animTime, engine.getCityState().p1CityShare,
                  engine.getCityState().lastCutMessage, engine.isDaylight(), engine.getHour24(), engine.getSeason());

    // 9. City Demand & Influence Tug-of-War Bar (Above City)
    city.drawInfluenceBar(window, font, resourcesLoaded, engine.getCityState().cityEnergyDemand,
                          engine.getPlayerEconomy(1).energyMW, engine.getPlayerEconomy(2).energyMW,
                          engine.getCityState().p1CityShare, engine.getCurrentDay());

    // 10. Resource Mines & Timber Forests
    nodes.drawNodes(window, font, resourcesLoaded, &engine, p1ResourceCooldown, p2ResourceCooldown);

    // Interactive mining extraction prompts & 6x speed badges
    drawMiningZonesAndBadges(window);

    // Ghost tooltips (name, cost, keys) on top of the city and the mines
    for (const auto& g : ghosts) {
        if (!g.shown) continue;
        int owner = (&g == &ghosts[0]) ? 1 : 2;
        std::string missing = (g.type == BuildingType::DEMOLISH) ? std::string()
                            : missingResourcesText(engine.getPlayerEconomy(owner), engine.getBuildingCost(g.type));
        nodes.drawBuildingGhostInfo(window, font, resourcesLoaded, g.type, g.pos, g.valid, engine.getBuildingCost(g.type), missing);
    }

    // 10.5 Dynamic Weather Particles (rain, snow, wind leaves, night stars/fireflies) & Lightning.
    //      Drawn over the map but under the HUD panels, so no speck or flash hides panel text.
    drawWeatherParticles(window);

    // 11. Top-Left & Top-Right Clocks (Continuous 24h cycle & weather)
    p1Clock.draw(window, font, resourcesLoaded, { 20.0f, 10.0f }, { 230.0f, 100.0f }, theme::P1);
    p2Clock.draw(window, font, resourcesLoaded, { 1600.0f - 250.0f, 10.0f }, { 230.0f, 100.0f }, theme::P2);

    // 12. Left & Right Building Menus
    p1Buildings.setHotkeys(BuildHotkeys::DIGITS);
    p2Buildings.setHotkeys(bot.isActive() ? BuildHotkeys::NONE : BuildHotkeys::NUMPAD);
    p1Buildings.draw(window, font, resourcesLoaded, mousePos, engine, p1Sel);
    p2Buildings.draw(window, font, resourcesLoaded, mousePos, engine, p2Sel);

    // 13. Bottom Corner Quarter-Circles (Pure icons and numbers, gold at bottom)
    resourceHUD.drawQuarterCircle(window, font, resourcesLoaded, engine.getPlayerEconomy(1), true);
    resourceHUD.drawQuarterCircle(window, font, resourcesLoaded, engine.getPlayerEconomy(2), false);

    // 14. Player Side Popups (rendered on player's sector)
    drawPlayerPopups(window);

    // 15. Menu button & persistent HUD
    drawHUD(window);


    // Dynamic Mining sparks and wood chips
    drawMiningParticles(window);

    // 17. Interactive Modal Dialogs (Requires player to click OK or confirm)
    drawPlayerModals(window);

    // 17.5 Interactive Beginner Tutorial Prompts and Guide Arrows
    tutorial.draw(window, font, resourcesLoaded, engine, nodes, animTime, mousePos, p1Pos, p2Pos);

    // 18. Player targeting cursors (RENDERED ON TOP OF EVERYTHING!)
    drawPlayerCursors(window);

    // 19. Floating Notices
    drawFloatingNotices(window);

    // 20. Pause Menu (drawn before help so help is layered on top)
    // Team b-session (UX-12): the cards are drawn with the UI-scale zoom (hit tests use the same views)
    const sf::View baseView = window.getView();
    if (engine.getCityState().winner != 0) {
        window.setView(victoryView(window));
        drawVictoryScreen(window);
    } else if (isPaused) {
        window.setView(pauseView(window));
        drawPauseMenu(window);
    }
    window.setView(baseView);

    // 21. Help & Rules Manual Overlay — ALWAYS on top of everything (including pause menu)
    if (showHelpOverlay) window.setView(helpView(window));
    drawHelpOverlay(window);
    window.setView(baseView);

    // 22. Team b-session: settings / save panel / save toast on top of everything
    drawSessionOverlays(window);
}
