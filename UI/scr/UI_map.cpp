#include "../includes/UI_map.h"
#include <cmath>
#include <iostream>
#include <algorithm>

UI_map::UI_map()
    : resourcesLoaded(false),
      controlScheme(ControlScheme::BOTH_KEYBOARD),
      requestMenu(false),
      p1Clock(1),
      p2Clock(2),
      p1Buildings(1, { 18.0f, 115.0f }, { 230.0f, 395.0f }, sf::Color(0, 229, 255)),
      p2Buildings(2, { 1600.0f - 248.0f, 115.0f }, { 230.0f, 395.0f }, sf::Color(255, 120, 200)),
      p1Pos(450.0f, 450.0f),
      p2Pos(1150.0f, 450.0f),
      p1Pulse(0.0f),
      p2Pulse(0.0f),
      requestFullscreenToggle(false),
      showHelpOverlay(false),
      lightningFlashTimer(0.0f) {
    if (grassTexture.loadFromFile("assets/grass.png")) {
        grassTexture.setRepeated(true);
    } else {
        std::cerr << "[UI_map] Warning: Failed to load assets/grass.png\n";
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
    if (!resourcesLoaded) {
        // No font: the tutorial cannot be drawn, so never leave it active (it would invisibly
        // swallow input and keep the bot frozen).
        tutorial.setCoop(diff == BotDifficulty::NONE);
        tutorial.skip();
    } else if (diff == BotDifficulty::HARD) {
        tutorial.setCoop(false);
        tutorial.skip(); // Hard mode: skip tutorial for advanced players
    } else if (diff == BotDifficulty::NONE) {
        tutorial.setCoop(true);
        tutorial.start(); // Co-op mode: show tutorial for 2 players
    } else {
        tutorial.setCoop(false);
        tutorial.start(); // Easy / Medium: show single player tutorial
    }
    std::cout << "[UI_map] Bot difficulty set to: " << static_cast<int>(diff) << "\n";
}

void UI_map::drawGrassBackground(sf::RenderWindow& window) {
    float screenWidth = VIRTUAL_WIDTH;
    float screenHeight = VIRTUAL_HEIGHT;

    if (grassTexture.getSize().x > 0) {
        sf::Sprite sprite(grassTexture);
        sprite.setTextureRect(sf::IntRect({ 0, 0 }, { (int)screenWidth, (int)screenHeight }));
        sprite.setPosition({ 0.0f, 0.0f });
        window.draw(sprite);
    } else {
        sf::RectangleShape ground({ screenWidth, screenHeight });
        ground.setPosition({ 0.0f, 0.0f });
        ground.setFillColor(sf::Color(60, 115, 40));
        window.draw(ground);
    }
    // [b-effects] The dawn/dusk/night tint moved to the lighting pass (UI_fx::applyLighting, DS-11)
    // and the straight energy conduits became the visible power grid (UI_fx::drawPowerGrid, HX-07).
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

    fx.reset(engine); // [b-effects] clear effects and resync the feedback event tracker

    spawnNotice("НОВА ИГРА СТАРТИРАНА!", { 800.0f, 450.0f }, sf::Color(0, 255, 180));
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
    float dt = deltaClock.restart().asSeconds();
    if (dt > 0.05f) dt = 0.05f;

    // 1. Advance continuous backend simulation (only when NOT paused and game not won)
    bool simulationRunning = !isPaused && engine.getCityState().winner == 0;
    if (simulationRunning) {
        engine.update(dt);
        updateControls(window, dt);
        updateWeatherParticles(dt);
        tutorial.update(dt, engine);
    }

    // [b-effects] Effects + audio feedback, after the simulation so this frame's actions are seen
    fx.setSinglePlayer(bot.isActive());
    fx.update(dt, engine, nodes, simulationRunning);

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

    p2Clock.setHour(engine.getHour24());
    p2Clock.setDay(engine.getCurrentDay());
    p2Clock.setWeather(engine.getPlayerWeather(2));
    p2Clock.setSeason(engine.getSeason());

    float animTime = animClock.getElapsedTime().asSeconds();
    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    // ===== [b-effects] WORLD LAYER: screen shake applies only here; the UI layer below stays still =====
    const sf::View uiView = window.getView();
    window.setView(fx.worldView(uiView));

    // 3. Render terrain & atmosphere (seasonal ground: snow, leaves, blossoms, heat shimmer)
    drawGrassBackground(window);
    fx.drawGround(window);

    // 4. Full-height river with canals to the hydro bank, bridges and traffic (DS-10)
    fx.drawRiver(window);

    // Day/night light map over the terrain (DS-11)
    fx.applyLighting(window, false);

    // 5. Purchasable Land Plots Grid
    nodes.drawLandPlots(window, font, resourcesLoaded, engine.getLandPlots(), mousePos);

    // Visible power grid: hydro pipes, catenary cables, pylons, transformers (HX-07)
    fx.drawPowerGrid(window);

    // 6. Placed Buildings on the Map (new ones pop in, DS-08)
    fx.drawBuildings(window, nodes, font, resourcesLoaded, engine.getBuildings());

    // Mild light pass over plots and buildings, then emissive power pulses and lamp halos
    fx.applyLighting(window, true);
    fx.drawEmissive(window);
    fx.drawWorldFx(window);
    fx.drawBorderTag(window, font, resourcesLoaded);

    // 7. Holographic ghost preview if building is selected (snapped to plot grid, hidden if outside purchased land)
    BuildingType p1Sel = engine.getSelectedBuilding(1);
    if (p1Sel != BuildingType::NONE) {
        sf::Vector2f targetPos = (p1Sel == BuildingType::DEMOLISH) ? p1Pos : engine.snapToBuildingGrid(1, p1Pos);
        if (p1Sel == BuildingType::DEMOLISH || isPosOnPurchasedLand(1, targetPos)) {
            std::string reason;
            bool valid = engine.canPlaceBuilding(1, p1Sel, targetPos, reason);
            nodes.drawBuildingGhost(window, font, resourcesLoaded, p1Sel, targetPos, valid, engine.getBuildingCost(p1Sel));
        }
    }
    BuildingType p2Sel = engine.getSelectedBuilding(2);
    if (p2Sel != BuildingType::NONE) {
        sf::Vector2f targetPos = (p2Sel == BuildingType::DEMOLISH) ? p2Pos : engine.snapToBuildingGrid(2, p2Pos);
        if (p2Sel == BuildingType::DEMOLISH || isPosOnPurchasedLand(2, targetPos)) {
            std::string reason;
            bool valid = engine.canPlaceBuilding(2, p2Sel, targetPos, reason);
            nodes.drawBuildingGhost(window, font, resourcesLoaded, p2Sel, targetPos, valid, engine.getBuildingCost(p2Sel));
        }
    }

    // 8. Compact Metropolis City Center with territorial slicing & conquest ([b-effects] tweened share)
    city.drawCity(window, font, resourcesLoaded, animTime, fx.displayedShare(),
                  engine.getCityState().lastCutMessage, engine.isDaylight(), engine.getHour24(), engine.getSeason());

    // 10. Resource Mines & Timber Forests
    nodes.drawNodes(window, font, resourcesLoaded, &engine, p1ResourceCooldown, p2ResourceCooldown);

    // Interactive mining extraction prompts & 6x speed badges
    drawMiningZonesAndBadges(window);

    // [b-effects] UX-09: weather (rain, snow, leaves, petals, fireflies) and lightning stay in the world
    // layer, below every HUD panel and popup
    drawWeatherParticles(window);

    // Dynamic Mining sparks and wood chips
    drawMiningParticles(window);

    window.setView(uiView);
    // ===== [b-effects] UI LAYER =====

    // 9. City Demand & Influence Tug-of-War Bar (Above City), animated needle + trailing change segment
    city.drawInfluenceBar(window, font, resourcesLoaded, engine.getCityState().cityEnergyDemand,
                          engine.getPlayerEconomy(1).energyMW, engine.getPlayerEconomy(2).energyMW,
                          fx.displayedShare(), engine.getCurrentDay());
    fx.drawInfluenceTrail(window);

    // 11. Top-Left & Top-Right Clocks (Continuous 24h cycle & weather)
    p1Clock.draw(window, font, resourcesLoaded, { 20.0f, 10.0f }, { 230.0f, 100.0f }, sf::Color(0, 229, 255));
    p2Clock.draw(window, font, resourcesLoaded, { 1600.0f - 250.0f, 10.0f }, { 230.0f, 100.0f }, sf::Color(255, 120, 200));

    // 12. Left & Right Building Menus
    p1Buildings.draw(window, font, resourcesLoaded, mousePos, engine.getPlayerEconomy(1), p1Sel);
    p2Buildings.draw(window, font, resourcesLoaded, mousePos, engine.getPlayerEconomy(2), p2Sel);

    // 13. Bottom Corner Quarter-Circles (Pure icons and numbers, gold at bottom)
    resourceHUD.drawQuarterCircle(window, font, resourcesLoaded, engine.getPlayerEconomy(1), true);
    resourceHUD.drawQuarterCircle(window, font, resourcesLoaded, engine.getPlayerEconomy(2), false);

    // [b-effects] Mined resources fly from the station into the HUD counter
    fx.drawFlyingResources(window, font, resourcesLoaded);

    // 14. Player Side Popups (rendered on player's sector)
    drawPlayerPopups(window);

    // 15. Menu button & persistent HUD
    drawHUD(window);

    // 17. Interactive Modal Dialogs (Requires player to click OK or confirm)
    drawPlayerModals(window);

    // 17.5 Interactive Beginner Tutorial Prompts and Guide Arrows
    tutorial.draw(window, font, resourcesLoaded, engine, nodes, animTime, mousePos, p1Pos, p2Pos);

    // 18. Player targeting cursors (RENDERED ON TOP OF EVERYTHING!)
    drawPlayerCursors(window);

    // 19. Floating Notices
    drawFloatingNotices(window);

    // 20. Pause Menu (drawn before help so help is layered on top)
    if (engine.getCityState().winner != 0) {
        drawVictoryScreen(window);
    } else if (isPaused) {
        drawPauseMenu(window);
    }

    // 21. Help & Rules Manual Overlay — ALWAYS on top of everything (including pause menu)
    drawHelpOverlay(window);
}
