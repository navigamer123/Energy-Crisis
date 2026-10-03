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
        std::cerr << "[UI_map] Warning: Failed to load assets/font.ttf\n";
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
    if (diff == BotDifficulty::HARD) {
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
        skyOverlay.setFillColor(sf::Color(240, 150, 80, static_cast<std::uint8_t>(50 * intensity)));
        window.draw(skyOverlay);
    }
    // 2. Dusk / Sunset transition (warm amber/crimson evening glow)
    else if (hour >= (sunset - 0.75f) && hour <= (sunset + 0.85f)) {
        float t = (hour - (sunset - 0.75f)) / 1.6f;
        float intensity = std::sin(t * 3.14159f);
        skyOverlay.setFillColor(sf::Color(215, 80, 25, static_cast<std::uint8_t>(65 * intensity)));
        window.draw(skyOverlay);
    }
    // 3. Nighttime (deep midnight indigo overlay)
    else if (hour > (sunset + 0.85f) || hour < (sunrise - 0.75f)) {
        std::uint8_t nightAlpha = 110;
        if (engine.getSeason() == SeasonType::WINTER) {
            nightAlpha = 130; // Darker winter nights
        }
        skyOverlay.setFillColor(sf::Color(8, 14, 28, nightAlpha));
        window.draw(skyOverlay);
    }
}

void UI_map::drawEnergyConduits(sf::RenderWindow& window, float animTime) {
    const auto& bList = engine.getBuildings();
    if (bList.empty()) return;

    sf::Vector2f cityEntranceP1(730.0f, 410.0f);
    sf::Vector2f cityEntranceP2(870.0f, 410.0f);

    for (size_t i = 0; i < bList.size(); ++i) {
        const auto& b = bList[i];
        if (b.type == BuildingType::LAMP) continue;

        sf::Vector2f dest = (b.playerOwner == 1) ? cityEntranceP1 : cityEntranceP2;
        sf::Color conduitColor = (b.playerOwner == 1) ? sf::Color(0, 229, 255, 90) : sf::Color(255, 120, 200, 90);
        sf::Color packetColor = (b.playerOwner == 1) ? sf::Color(160, 250, 255, 230) : sf::Color(255, 190, 240, 230);

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
                aura.setFillColor(sf::Color(packetColor.r, packetColor.g, packetColor.b, 65));
                window.draw(aura);
            }
        }
    }
}

void UI_map::restartMatch() {
    isPaused = false;
    engine.restartGame();
    p1Pos = { 420.0f, 320.0f };
    p2Pos = { 1180.0f, 320.0f };
    p1ResourceCooldown = 0.0f;
    p2ResourceCooldown = 0.0f;
    p1ActionCooldown = 0.0f;
    p2ActionCooldown = 0.0f;
    p1SelectCooldown = 0.0f;
    p2SelectCooldown = 0.0f;
    p1Modal.active = false;
    p2Modal.active = false;
    p1Popup.active = false;
    p2Popup.active = false;
    notices.clear();
    miningParticles.clear();
    bot.init(bot.getDifficulty());
    tutorial.reset();
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
    if (!isPaused && engine.getCityState().winner == 0) {
        engine.update(dt);
        updateControls(window, dt);
        updateWeatherParticles(dt);
        tutorial.update(dt, engine);
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

    // 3. Render terrain & atmosphere
    drawGrassBackground(window);

    // 4. Central dividing line & river
    city.drawDividingRiver(window, font, resourcesLoaded, animTime, engine.isDaylight());

    // 5. Purchasable Land Plots Grid
    nodes.drawLandPlots(window, font, resourcesLoaded, engine.getLandPlots(), mousePos);

    // Dynamic glowing energy conduit lines connecting generators to metropolis
    drawEnergyConduits(window, animTime);

    // 6. Placed Buildings on the Map
    nodes.drawPlacedBuildings(window, font, resourcesLoaded, engine.getBuildings());

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

    // 11. Top-Left & Top-Right Clocks (Continuous 24h cycle & weather)
    p1Clock.draw(window, font, resourcesLoaded, { 20.0f, 10.0f }, { 230.0f, 100.0f }, sf::Color(0, 229, 255));
    p2Clock.draw(window, font, resourcesLoaded, { 1600.0f - 250.0f, 10.0f }, { 230.0f, 100.0f }, sf::Color(255, 120, 200));

    // 12. Left & Right Building Menus
    p1Buildings.draw(window, font, resourcesLoaded, mousePos, engine.getPlayerEconomy(1), p1Sel);
    p2Buildings.draw(window, font, resourcesLoaded, mousePos, engine.getPlayerEconomy(2), p2Sel);

    // 13. Bottom Corner Quarter-Circles (Pure icons and numbers, gold at bottom)
    resourceHUD.drawQuarterCircle(window, font, resourcesLoaded, engine.getPlayerEconomy(1), true);
    resourceHUD.drawQuarterCircle(window, font, resourcesLoaded, engine.getPlayerEconomy(2), false);

    // 14. Player Side Popups (rendered on player's sector)
    drawPlayerPopups(window);

    // 15. Menu button & persistent HUD
    drawHUD(window);

    // 16. Dynamic Weather Particles (rain, snow, wind leaves, night stars/fireflies) & Lightning
    drawWeatherParticles(window);

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
    if (engine.getCityState().winner != 0) {
        drawVictoryScreen(window);
    } else if (isPaused) {
        drawPauseMenu(window);
    }

    // 21. Help & Rules Manual Overlay — ALWAYS on top of everything (including pause menu)
    drawHelpOverlay(window);
}
