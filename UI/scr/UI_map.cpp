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

void UI_map::spawnNotice(const std::string& text, sf::Vector2f pos, sf::Color color) {
    FloatingNotice n;
    n.text = text;
    n.pos = pos;
    n.timer = 1.8f;
    n.maxTimer = 1.8f;
    n.color = color;
    notices.push_back(n);
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

    // Dynamic atmospheric tint based on current hour
    float hour = engine.getHour24();
    sf::RectangleShape skyOverlay({ screenWidth, screenHeight });
    skyOverlay.setPosition({ 0.0f, 0.0f });

    if (hour >= 18.0f && hour <= 21.0f) {
        // Sunset / Dusk warm tint
        float sunsetAlpha = (hour - 18.0f) / 3.0f;
        skyOverlay.setFillColor(sf::Color(180, 80, 20, static_cast<std::uint8_t>(60 * (1.0f - sunsetAlpha))));
        window.draw(skyOverlay);
    } else if (hour > 21.0f || hour < 5.0f) {
        // Nighttime dark blue tint
        skyOverlay.setFillColor(sf::Color(10, 15, 30, 100));
        window.draw(skyOverlay);
    }
}

void UI_map::drawPlayerCursors(sf::RenderWindow& window) {
    float animTime = animClock.getElapsedTime().asSeconds();

    // -------------------------------------------------------------------------
    // PLAYER 1 CURSOR (WEST SECTOR - CYAN)
    // -------------------------------------------------------------------------
    if (p1Pulse > 0.0f) {
        float radius = 24.0f + (1.0f - p1Pulse) * 60.0f;
        sf::CircleShape pulseCircle(radius);
        pulseCircle.setOrigin({ radius, radius });
        pulseCircle.setPosition(p1Pos);
        std::uint8_t alpha = static_cast<std::uint8_t>(p1Pulse * 220);
        pulseCircle.setFillColor(sf::Color(0, 229, 255, alpha / 4));
        pulseCircle.setOutlineThickness(2.0f);
        pulseCircle.setOutlineColor(sf::Color(0, 255, 200, alpha));
        window.draw(pulseCircle);
    }

    sf::CircleShape p1Core(8.0f);
    p1Core.setOrigin({ 8.0f, 8.0f });
    p1Core.setPosition(p1Pos);
    p1Core.setFillColor(sf::Color(0, 229, 255, 220));
    p1Core.setOutlineThickness(2.0f);
    p1Core.setOutlineColor(sf::Color::White);
    window.draw(p1Core);

    float rot1 = animTime * 90.0f;
    for (int i = 0; i < 4; i++) {
        sf::RectangleShape bracket({ 14.0f, 2.5f });
        bracket.setOrigin({ 20.0f, 1.25f });
        bracket.setPosition(p1Pos);
        bracket.setRotation(sf::degrees(rot1 + i * 90.0f));
        bracket.setFillColor(sf::Color(0, 255, 220));
        window.draw(bracket);
    }

    if (resourcesLoaded) {
        sf::Text p1Tag(font, "P1", 13);
        p1Tag.setFillColor(sf::Color(0, 255, 255));
        p1Tag.setPosition({ p1Pos.x - 8.0f, p1Pos.y - 28.0f });
        window.draw(p1Tag);
    }

    // -------------------------------------------------------------------------
    // PLAYER 2 CURSOR (EAST SECTOR - MAGENTA / GOLD)
    // -------------------------------------------------------------------------
    if (p2Pulse > 0.0f) {
        float radius = 24.0f + (1.0f - p2Pulse) * 60.0f;
        sf::CircleShape pulseCircle(radius);
        pulseCircle.setOrigin({ radius, radius });
        pulseCircle.setPosition(p2Pos);
        std::uint8_t alpha = static_cast<std::uint8_t>(p2Pulse * 220);
        pulseCircle.setFillColor(sf::Color(255, 120, 200, alpha / 4));
        pulseCircle.setOutlineThickness(2.0f);
        pulseCircle.setOutlineColor(sf::Color(255, 215, 0, alpha));
        window.draw(pulseCircle);
    }

    sf::CircleShape p2Core(8.0f);
    p2Core.setOrigin({ 8.0f, 8.0f });
    p2Core.setPosition(p2Pos);
    p2Core.setFillColor(sf::Color(255, 120, 200, 220));
    p2Core.setOutlineThickness(2.0f);
    p2Core.setOutlineColor(sf::Color::White);
    window.draw(p2Core);

    float rot2 = -animTime * 90.0f;
    for (int i = 0; i < 4; i++) {
        sf::RectangleShape bracket({ 14.0f, 2.5f });
        bracket.setOrigin({ 20.0f, 1.25f });
        bracket.setPosition(p2Pos);
        bracket.setRotation(sf::degrees(rot2 + i * 90.0f));
        bracket.setFillColor(sf::Color(255, 204, 0));
        window.draw(bracket);
    }

    if (resourcesLoaded) {
        sf::Text p2Tag(font, "P2", 13);
        p2Tag.setFillColor(sf::Color(255, 140, 220));
        p2Tag.setPosition({ p2Pos.x - 8.0f, p2Pos.y - 28.0f });
        window.draw(p2Tag);
    }
}

void UI_map::drawFloatingNotices(sf::RenderWindow& window) {
    if (!resourcesLoaded) return;

    for (const auto& n : notices) {
        float alphaFrac = n.timer / n.maxTimer;
        std::uint8_t alpha = static_cast<std::uint8_t>(alphaFrac * 255);
        sf::Text t(font, toUtf8(n.text), 14);
        sf::Color c = n.color;
        c.a = alpha;
        t.setFillColor(c);
        sf::FloatRect b = t.getLocalBounds();
        t.setPosition(sf::Vector2f(n.pos.x - b.size.x / 2.0f, n.pos.y));
        window.draw(t);
    }
}

void UI_map::triggerPlayerPopup(int player, const std::string& badge, const std::string& title,
                                const std::string& detail, const std::string& action, sf::Color accent) {
    PlayerPopup& pop = (player == 1) ? p1Popup : p2Popup;
    pop.badge = badge;
    pop.title = title;
    pop.detail = detail;
    pop.action = action;
    pop.accentColor = accent;
    pop.timer = 4.0f;
    pop.maxTimer = 4.0f;
    pop.active = true;
}

void UI_map::drawPlayerPopups(sf::RenderWindow& window) {
    if (!resourcesLoaded) return;

    auto drawOnePopup = [&](const PlayerPopup& pop, float x, float y) {
        if (!pop.active) return;

        float alphaRatio = std::min(1.0f, pop.timer / 0.8f);
        std::uint8_t alpha = static_cast<std::uint8_t>(alphaRatio * 245);

        // Glassmorphic container box
        sf::RectangleShape box({ 225.0f, 132.0f });
        box.setPosition({ x, y });
        box.setFillColor(sf::Color(16, 22, 34, alpha));
        box.setOutlineThickness(1.5f);
        sf::Color outColor = pop.accentColor;
        outColor.a = alpha;
        box.setOutlineColor(outColor);
        window.draw(box);

        // Badge pill
        sf::RectangleShape badgeBox({ 65.0f, 18.0f });
        badgeBox.setPosition({ x + 8.0f, y + 8.0f });
        sf::Color bColor = pop.accentColor;
        bColor.a = static_cast<std::uint8_t>(alpha * 0.65f);
        badgeBox.setFillColor(bColor);
        window.draw(badgeBox);

        sf::Text tBadge(font, toUtf8(pop.badge), 10);
        tBadge.setFillColor(sf::Color(255, 255, 255, alpha));
        sf::FloatRect bb = tBadge.getLocalBounds();
        tBadge.setPosition({ x + 8.0f + (65.0f - bb.size.x) / 2.0f, y + 9.0f });
        window.draw(tBadge);

        // Title
        sf::Text tTitle(font, toUtf8(pop.title), 12);
        tTitle.setFillColor(sf::Color(255, 255, 255, alpha));
        tTitle.setPosition({ x + 78.0f, y + 9.0f });
        window.draw(tTitle);

        // Separator
        sf::RectangleShape sep({ 209.0f, 1.0f });
        sep.setPosition({ x + 8.0f, y + 31.0f });
        sep.setFillColor(sf::Color(60, 85, 120, alpha));
        window.draw(sep);

        // Detailed Explanation
        sf::Text tDetail(font, toUtf8(pop.detail), 11);
        tDetail.setFillColor(sf::Color(195, 225, 255, alpha));
        tDetail.setPosition({ x + 8.0f, y + 36.0f });
        window.draw(tDetail);

        // Action instructions
        if (!pop.action.empty()) {
            sf::Text tAct(font, toUtf8(pop.action), 10);
            tAct.setFillColor(sf::Color(255, 215, 80, alpha));
            tAct.setPosition({ x + 8.0f, y + 92.0f });
            window.draw(tAct);
        }

        // Timer progress bar at the bottom
        float pWidth = 209.0f * (pop.timer / pop.maxTimer);
        sf::RectangleShape prog({ std::max(0.0f, pWidth), 2.5f });
        prog.setPosition({ x + 8.0f, y + 122.0f });
        prog.setFillColor(outColor);
        window.draw(prog);
    };

    drawOnePopup(p1Popup, 20.0f, 520.0f);
    drawOnePopup(p2Popup, 1600.0f - 245.0f, 520.0f);
}

void UI_map::triggerPlayerModal(int player, const std::string& badge, const std::string& title,
                                const std::string& detail, const std::string& tip, sf::Color accent) {
    PlayerModalDialog& m = (player == 1) ? p1Modal : p2Modal;
    m.active = true;
    m.badge = badge;
    m.title = title;
    m.detail = detail;
    m.tip = tip;
    m.accentColor = accent;

    float w = 340.0f;
    float h = 210.0f;
    float x = (player == 1) ? (800.0f - w) / 2.0f : 800.0f + (800.0f - w) / 2.0f;
    float y = 250.0f;

    m.box = sf::FloatRect({ x, y }, { w, h });
    m.okBtn = sf::FloatRect({ x + (w - 160.0f) / 2.0f, y + h - 42.0f }, { 160.0f, 32.0f });
}

void UI_map::closePlayerModal(int player) {
    PlayerModalDialog& m = (player == 1) ? p1Modal : p2Modal;
    m.active = false;
}

void UI_map::drawPlayerModals(sf::RenderWindow& window) {
    if (!resourcesLoaded) return;
    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    auto drawOneModal = [&](const PlayerModalDialog& m, int pIdx) {
        if (!m.active) return;

        // Dim background overlay over that player's half of the screen
        float overlayX = (pIdx == 1) ? 0.0f : 800.0f;
        sf::RectangleShape overlay({ 800.0f, 900.0f });
        overlay.setPosition({ overlayX, 0.0f });
        overlay.setFillColor(sf::Color(0, 0, 0, 140));
        window.draw(overlay);

        // Modal main box
        sf::RectangleShape card(m.box.size);
        card.setPosition(m.box.position);
        card.setFillColor(sf::Color(16, 22, 34, 252));
        card.setOutlineThickness(2.5f);
        card.setOutlineColor(m.accentColor);
        window.draw(card);

        // Header bar
        sf::RectangleShape hBar({ m.box.size.x, 30.0f });
        hBar.setPosition(m.box.position);
        hBar.setFillColor(sf::Color(28, 38, 54, 250));
        window.draw(hBar);

        // Badge / Alert Icon
        sf::Text tBadge(font, toUtf8("! " + m.badge), 12);
        tBadge.setFillColor(m.accentColor);
        tBadge.setPosition({ m.box.position.x + 10.0f, m.box.position.y + 6.0f });
        window.draw(tBadge);

        // Title
        sf::Text tTitle(font, toUtf8(m.title), 13);
        tTitle.setFillColor(sf::Color::White);
        tTitle.setPosition({ m.box.position.x + 12.0f, m.box.position.y + 38.0f });
        window.draw(tTitle);

        // Detail explanation
        sf::Text tDetail(font, toUtf8(m.detail), 11);
        tDetail.setFillColor(sf::Color(200, 225, 250));
        tDetail.setPosition({ m.box.position.x + 12.0f, m.box.position.y + 64.0f });
        window.draw(tDetail);

        // Tip text
        if (!m.tip.empty()) {
            sf::Text tTip(font, toUtf8("СЪВЕТ: " + m.tip), 10);
            tTip.setFillColor(sf::Color(255, 225, 110));
            tTip.setPosition({ m.box.position.x + 12.0f, m.box.position.y + 115.0f });
            window.draw(tTip);
        }

        // [ OK - РАЗБРАХ ] Button
        bool btnHover = m.okBtn.contains(mousePos);
        sf::RectangleShape btn(m.okBtn.size);
        btn.setPosition(m.okBtn.position);
        btn.setFillColor(btnHover ? sf::Color(55, 160, 95) : sf::Color(35, 110, 65));
        btn.setOutlineThickness(1.5f);
        btn.setOutlineColor(btnHover ? sf::Color(100, 255, 180) : sf::Color(70, 210, 110));
        window.draw(btn);

        sf::Text tOk(font, toUtf8("OK  (РАЗБРАХ)"), 12);
        tOk.setFillColor(sf::Color::White);
        sf::FloatRect ob = tOk.getLocalBounds();
        tOk.setPosition({ m.okBtn.position.x + (m.okBtn.size.x - ob.size.x) / 2.0f, m.okBtn.position.y + 6.0f });
        window.draw(tOk);
    };

    drawOneModal(p1Modal, 1);
    drawOneModal(p2Modal, 2);
}

void UI_map::updateWeatherParticles(float dt) {
    WeatherType w1 = engine.getPlayerWeather(1);
    WeatherType w2 = engine.getPlayerWeather(2);
    SeasonType season = engine.getSeason();
    bool isNight = !engine.isDaylight();

    // Lightning strike timer for stormy conditions
    if ((w1 == WeatherType::STORMY || w2 == WeatherType::STORMY) && (rand() % 350 == 0) && lightningFlashTimer <= 0.0f) {
        lightningFlashTimer = 0.28f;
    }
    if (lightningFlashTimer > 0.0f) {
        lightningFlashTimer -= dt;
        if (lightningFlashTimer < 0.0f) lightningFlashTimer = 0.0f;
    }

    for (size_t i = 0; i < particles.size(); ++i) {
        auto& p = particles[i];
        WeatherType w = (p.pos.x < 800.0f) ? w1 : w2;

        if (w == WeatherType::RAINY || w == WeatherType::STORMY) {
            p.type = 0; // Rain
            p.vel = sf::Vector2f((w == WeatherType::STORMY ? -140.0f : -60.0f), (w == WeatherType::STORMY ? 750.0f : 550.0f));
        } else if (season == SeasonType::WINTER) {
            p.type = 1; // Snow
            float drift = std::sin(p.pos.y * 0.02f + static_cast<float>(i)) * 40.0f;
            p.vel = sf::Vector2f(drift, 65.0f);
        } else if (w == WeatherType::WINDY) {
            p.type = 2; // Wind streak / leaf
            p.vel = sf::Vector2f(320.0f, std::sin(p.pos.x * 0.015f) * 35.0f);
        } else if (isNight) {
            p.type = 3; // Night star / firefly
            p.vel = sf::Vector2f(std::cos(p.pos.y * 0.03f) * 12.0f, std::sin(p.pos.x * 0.03f) * 12.0f);
        } else {
            // Calm daylight atmospheric dust
            p.type = 3;
            p.vel = sf::Vector2f(std::cos(p.pos.y * 0.01f) * 8.0f, -15.0f);
        }

        p.pos += p.vel * dt;

        // Wrap around virtual screen bounds
        if (p.pos.y > VIRTUAL_HEIGHT + 10.0f) {
            p.pos.y = -10.0f;
            p.pos.x = static_cast<float>(rand() % static_cast<int>(VIRTUAL_WIDTH));
        } else if (p.pos.y < -15.0f) {
            p.pos.y = VIRTUAL_HEIGHT + 5.0f;
            p.pos.x = static_cast<float>(rand() % static_cast<int>(VIRTUAL_WIDTH));
        }
        if (p.pos.x > VIRTUAL_WIDTH + 15.0f) {
            p.pos.x = -10.0f;
        } else if (p.pos.x < -15.0f) {
            p.pos.x = VIRTUAL_WIDTH + 10.0f;
        }
    }
}

void UI_map::drawWeatherParticles(sf::RenderWindow& window) {
    // 1. Lightning flash during storms
    if (lightningFlashTimer > 0.0f) {
        sf::RectangleShape flash({ VIRTUAL_WIDTH, VIRTUAL_HEIGHT });
        flash.setPosition({ 0.0f, 0.0f });
        std::uint8_t a = static_cast<std::uint8_t>(std::min(240.0f, lightningFlashTimer * 850.0f));
        flash.setFillColor(sf::Color(220, 240, 255, a));
        window.draw(flash);
    }

    // 2. Particles
    for (const auto& p : particles) {
        if (p.type == 0) {
            // Rain streak
            sf::Vertex line[2];
            line[0].position = p.pos;
            line[0].color = sf::Color(160, 210, 255, 160);
            line[1].position = p.pos + sf::Vector2f(p.vel.x * 0.025f, p.vel.y * 0.025f);
            line[1].color = sf::Color(200, 235, 255, 220);
            window.draw(line, 2, sf::PrimitiveType::Lines);
        } else if (p.type == 1) {
            // Snow flake
            sf::CircleShape flake(p.size);
            flake.setPosition(p.pos);
            flake.setFillColor(sf::Color(255, 255, 255, static_cast<std::uint8_t>(p.alpha * 0.85f)));
            window.draw(flake);
        } else if (p.type == 2) {
            // Wind leaf / amber petal
            sf::RectangleShape leaf({ 5.0f, 2.5f });
            leaf.setPosition(p.pos);
            leaf.setRotation(sf::degrees(p.pos.x * 0.5f));
            leaf.setFillColor(sf::Color(210, 170, 70, 180));
            window.draw(leaf);
        } else if (p.type == 3) {
            // Night star or firefly
            sf::CircleShape glow(p.size);
            glow.setPosition(p.pos);
            glow.setFillColor(sf::Color(180, 255, 120, static_cast<std::uint8_t>(120 + 80 * std::sin(p.pos.x * 0.05f))));
            window.draw(glow);
        }
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

void UI_map::drawHelpOverlay(sf::RenderWindow& window) {
    if (!showHelpOverlay) return;

    // Dim backdrop
    sf::RectangleShape backdrop({ VIRTUAL_WIDTH, VIRTUAL_HEIGHT });
    backdrop.setPosition({ 0.0f, 0.0f });
    backdrop.setFillColor(sf::Color(5, 10, 18, 205));
    window.draw(backdrop);

    // Dialog card
    sf::FloatRect card({ 220.0f, 80.0f }, { 1160.0f, 740.0f });
    sf::RectangleShape cardBox(card.size);
    cardBox.setPosition(card.position);
    cardBox.setFillColor(sf::Color(14, 22, 36, 250));
    cardBox.setOutlineThickness(2.5f);
    cardBox.setOutlineColor(sf::Color(0, 229, 255, 200));
    window.draw(cardBox);

    // Header strip
    sf::RectangleShape headerStrip({ card.size.x, 52.0f });
    headerStrip.setPosition(card.position);
    headerStrip.setFillColor(sf::Color(22, 35, 58));
    window.draw(headerStrip);

    if (resourcesLoaded) {
        // Title
        sf::Text title(font, toUtf8("⚡ НАСТОЛЕН НАРЪЧНИК: ENERGY CRISIS"), 20);
        title.setStyle(sf::Text::Bold);
        title.setFillColor(sf::Color(0, 229, 255));
        title.setPosition({ card.position.x + 25.0f, card.position.y + 12.0f });
        window.draw(title);

        // Close button at top right
        sf::FloatRect closeBtn({ card.position.x + card.size.x - 170.0f, card.position.y + 11.0f }, { 150.0f, 30.0f });
        sf::Vector2f mPos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        bool hClose = closeBtn.contains(mPos);

        sf::RectangleShape cb(closeBtn.size);
        cb.setPosition(closeBtn.position);
        cb.setFillColor(hClose ? sf::Color(255, 75, 75) : sf::Color(180, 50, 50));
        cb.setOutlineThickness(1.0f);
        cb.setOutlineColor(sf::Color::White);
        window.draw(cb);

        sf::Text cbText(font, toUtf8("[X] ЗАТВОРИ (H)"), 12);
        cbText.setFillColor(sf::Color::White);
        sf::FloatRect cbb = cbText.getLocalBounds();
        cbText.setPosition({ closeBtn.position.x + (closeBtn.size.x - cbb.size.x) / 2.0f, closeBtn.position.y + 6.0f });
        window.draw(cbText);

        // Content Sections
        float y = card.position.y + 70.0f;
        auto drawSection = [&](const std::string& h, const std::string& body, sf::Color accent) {
            sf::Text st(font, toUtf8(h), 15);
            st.setStyle(sf::Text::Bold);
            st.setFillColor(accent);
            st.setPosition({ card.position.x + 35.0f, y });
            window.draw(st);
            y += 24.0f;

            sf::Text bt(font, toUtf8(body), 12);
            bt.setFillColor(sf::Color(220, 235, 255));
            bt.setLineSpacing(1.25f);
            bt.setPosition({ card.position.x + 45.0f, y });
            window.draw(bt);
            y += bt.getLocalBounds().size.y + 24.0f;
        };

        drawSection("1. ЦЕЛ НА ИГРАТА И ДОМИНИРАНЕ НА ГРАДА",
                    "• Централният Метрополис изисква постоянно нарастваща мощност (MW) всеки изминал ден.\n"
                    "• Ако в края на денонощието не покриете своята квота, опонентът завзема част от вашия град!\n"
                    "• Победител е играчът, който постигне 100% териториален контрол над Метрополиса.",
                    sf::Color(255, 215, 0));

        drawSection("2. СТРОЕЖ, ЗЕМЯ И ДОБИВ НА РЕСУРСИ",
                    "• За да строите, първо трябва да закупите свободен парцел (ЗЕМЯ) в своята територия.\n"
                    "• Добивайте Дървесина (Wood) от горите и Руда (Ore) от мините чрез курсора или клик.\n"
                    "• Всеки тип централа има предимства: Солар (денем), Вятър (бури), ВЕЦ (дъжд), Батерия (буфер).",
                    sf::Color(0, 229, 255));

        drawSection("3. НОЩНИ ПРАВИЛА И ОСВЕТЛИТЕЛНИ ЛАМПИ",
                    "• През нощта (21:00 - 05:00) работниците не строят на тъмно, освен ако няма поставена ЛАМПА!\n"
                    "• Батериите се зареждат през деня от излишната енергия и я отдават нощем, за да спасят града ви от срив.",
                    sf::Color(255, 140, 220));

        drawSection("4. УПРАВЛЕНИЕ И БЪРЗИ КЛАВИШИ",
                    "• ИГРАЧ 1 (Син): [W/A/S/D] - Движение  |  [E] - Избор сграда  |  [X] - Разруши  |  [Q] - Отказ  |  [SPACE/Клик] - Действие\n"
                    "• ИГРАЧ 2 (Розов): [Стрелки] - Движение | [PgDn] - Избор сграда | [Del] - Разруши | [PgUp] - Отказ | [ENTER] - Действие\n"
                    "• СИСТЕМНИ: [F11] - Цял екран (Fullscreen)  |  [H] или [F1] - Този наръчник  |  [ESC/M] - Главно меню",
                    sf::Color(100, 255, 150));
    }
}

void UI_map::drawHUD(sf::RenderWindow& window) {
    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    // 1. Menu Button
    sf::FloatRect menuBtn({ 1600.0f - 130.0f, 900.0f - 34.0f }, { 120.0f, 28.0f });
    bool hoverMenu = menuBtn.contains(mousePos);
    sf::RectangleShape mBox(menuBtn.size);
    mBox.setPosition(menuBtn.position);
    mBox.setFillColor(hoverMenu ? sf::Color(55, 75, 105) : sf::Color(32, 42, 58));
    mBox.setOutlineThickness(hoverMenu ? 1.5f : 1.0f);
    mBox.setOutlineColor(hoverMenu ? sf::Color(255, 204, 0) : sf::Color(80, 110, 150));
    window.draw(mBox);

    // 2. Fullscreen Button [ ⛶ ЦЯЛ ЕКРАН (F11) ]
    sf::FloatRect fsBtn({ 1600.0f - 275.0f, 900.0f - 34.0f }, { 135.0f, 28.0f });
    bool hoverFs = fsBtn.contains(mousePos);
    sf::RectangleShape fsBox(fsBtn.size);
    fsBox.setPosition(fsBtn.position);
    fsBox.setFillColor(hoverFs ? sf::Color(0, 120, 180) : sf::Color(22, 48, 75));
    fsBox.setOutlineThickness(hoverFs ? 1.5f : 1.0f);
    fsBox.setOutlineColor(hoverFs ? sf::Color(0, 229, 255) : sf::Color(60, 100, 145));
    window.draw(fsBox);

    // 3. Help Button [ ? ПОМОЩ (H) ]
    sf::FloatRect helpBtn({ 1600.0f - 405.0f, 900.0f - 34.0f }, { 120.0f, 28.0f });
    bool hoverHelp = helpBtn.contains(mousePos);
    sf::RectangleShape hBox(helpBtn.size);
    hBox.setPosition(helpBtn.position);
    hBox.setFillColor(hoverHelp ? sf::Color(100, 70, 150) : sf::Color(40, 32, 65));
    hBox.setOutlineThickness(hoverHelp ? 1.5f : 1.0f);
    hBox.setOutlineColor(hoverHelp ? sf::Color(220, 150, 255) : sf::Color(90, 75, 130));
    window.draw(hBox);

    if (resourcesLoaded) {
        sf::Text mt(font, toUtf8("ESC / МЕНЮ"), 12);
        mt.setFillColor(hoverMenu ? sf::Color(255, 240, 150) : sf::Color::White);
        sf::FloatRect mb = mt.getLocalBounds();
        mt.setPosition({ menuBtn.position.x + (menuBtn.size.x - mb.size.x) / 2.0f, menuBtn.position.y + 5.0f });
        window.draw(mt);

        sf::Text fst(font, toUtf8("⛶ ЦЯЛ ЕКРАН (F11)"), 11);
        fst.setFillColor(hoverFs ? sf::Color::White : sf::Color(180, 235, 255));
        sf::FloatRect fsb = fst.getLocalBounds();
        fst.setPosition({ fsBtn.position.x + (fsBtn.size.x - fsb.size.x) / 2.0f, fsBtn.position.y + 6.0f });
        window.draw(fst);

        sf::Text htBtn(font, toUtf8("? ПОМОЩ (H)"), 12);
        htBtn.setFillColor(hoverHelp ? sf::Color::White : sf::Color(230, 200, 255));
        sf::FloatRect htbBtn = htBtn.getLocalBounds();
        htBtn.setPosition({ helpBtn.position.x + (helpBtn.size.x - htbBtn.size.x) / 2.0f, helpBtn.position.y + 5.0f });
        window.draw(htBtn);

        // Persistent Controls Reminder Bar
        sf::RectangleShape helpBar({ 930.0f, 26.0f });
        helpBar.setPosition({ 250.0f, 900.0f - 30.0f });
        helpBar.setFillColor(sf::Color(15, 20, 30, 220));
        helpBar.setOutlineThickness(1.0f);
        helpBar.setOutlineColor(sf::Color(60, 85, 120));
        window.draw(helpBar);

        std::string helpText = "P1: [E] Сграда | [X] Разруши | [Q] Отказ | [SPACE/Клик] Действие  ///  P2: [PgDn] Сграда | [Del] Разруши | [PgUp] Отказ | [ENTER] Действие";
        sf::Text ht(font, toUtf8(helpText), 11);
        ht.setFillColor(sf::Color(210, 230, 255));
        sf::FloatRect htb = ht.getLocalBounds();
        ht.setPosition({ 250.0f + (930.0f - htb.size.x) / 2.0f, 900.0f - 26.0f });
        window.draw(ht);
    }
}

void UI_map::spawnMiningParticles(sf::Vector2f pos, sf::Color color, int count) {
    for (int i = 0; i < count; ++i) {
        MiningParticle p;
        p.pos = pos + sf::Vector2f(static_cast<float>((rand() % 30) - 15), static_cast<float>((rand() % 30) - 15));
        float angle = static_cast<float>(rand() % 360) * 3.14159f / 180.0f;
        float speed = 60.0f + static_cast<float>(rand() % 140);
        p.vel = sf::Vector2f(std::cos(angle) * speed, std::sin(angle) * speed - 60.0f);
        p.color = color;
        p.life = 0.5f + static_cast<float>(rand() % 40) / 100.0f;
        p.maxLife = p.life;
        p.size = 2.5f + static_cast<float>(rand() % 3);
        miningParticles.push_back(p);
    }
}

void UI_map::updateMiningParticles(float dt) {
    for (auto it = miningParticles.begin(); it != miningParticles.end();) {
        it->life -= dt;
        it->pos += it->vel * dt;
        it->vel.y += 180.0f * dt;
        if (it->life <= 0.0f) {
            it = miningParticles.erase(it);
        } else {
            ++it;
        }
    }
}

void UI_map::drawMiningParticles(sf::RenderWindow& window) {
    for (const auto& p : miningParticles) {
        float alphaRatio = std::max(0.0f, p.life / p.maxLife);
        sf::CircleShape sp(p.size);
        sp.setPosition(p.pos);
        sf::Color c = p.color;
        c.a = static_cast<std::uint8_t>(alphaRatio * 255);
        sp.setFillColor(c);
        window.draw(sp);
    }
}

void UI_map::drawMiningZonesAndBadges(sf::RenderWindow& window) {
    float animTime = animClock.getElapsedTime().asSeconds();

    ResourceType p1Res = nodes.getP1ResourceAt(p1Pos);
    ResourceType p2Res = nodes.getP2ResourceAt(p2Pos);

    auto drawPrompt = [&](sf::Vector2f pos, const std::string& title, const std::string& keyStr, sf::Color col, float cd) {
        if (!resourcesLoaded) return;
        sf::RectangleShape tagBox({ 260.0f, 26.0f });
        tagBox.setPosition({ pos.x - 130.0f, pos.y - 44.0f });
        tagBox.setFillColor(sf::Color(10, 15, 25, 235));
        tagBox.setOutlineThickness(1.5f);
        tagBox.setOutlineColor(cd > 0.05f ? sf::Color(255, 180, 50) : col);
        window.draw(tagBox);

        std::string promptText = title + " | " + keyStr;
        if (cd > 0.05f) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), " (%.1fs)", cd);
            promptText += buf;
        } else {
            promptText += " [Добив: 2с]";
        }

        sf::Text t(font, toUtf8(promptText), 11);
        t.setFillColor(cd > 0.05f ? sf::Color(255, 210, 100) : col);
        sf::FloatRect tb = t.getLocalBounds();
        t.setPosition({ tagBox.getPosition().x + (260.0f - tb.size.x) / 2.0f, tagBox.getPosition().y + 5.0f });
        window.draw(t);

        if (cd > 0.05f) {
            float fillRatio = 1.0f - std::max(0.0f, std::min(1.0f, cd / 2.0f));
            sf::RectangleShape cdBar({ 256.0f * fillRatio, 3.0f });
            cdBar.setPosition({ tagBox.getPosition().x + 2.0f, tagBox.getPosition().y + 24.0f });
            cdBar.setFillColor(sf::Color(0, 255, 180));
            window.draw(cdBar);
        }
    };

    if (p1Res != ResourceType::NONE) {
        const auto* st = nodes.getStation(1, p1Res);
        std::string name = st ? st->nameBg + " (" + st->yieldStr + ")" : "ДОБИВ";
        sf::Color c = st ? st->themeColor : sf::Color(0, 229, 255);
        drawPrompt(p1Pos, name, "[SPACE]", c, p1ResourceCooldown);
    }
    if (p2Res != ResourceType::NONE) {
        const auto* st = nodes.getStation(2, p2Res);
        std::string name = st ? st->nameBg + " (" + st->yieldStr + ")" : "ДОБИВ";
        sf::Color c = st ? st->themeColor : sf::Color(255, 120, 200);
        drawPrompt(p2Pos, name, "[ENTER]", c, p2ResourceCooldown);
    }

    // High-speed 6x time badges under the top clocks when active
    if (engine.getTimeScale() > 1.5f && resourcesLoaded) {
        float pulse = (std::sin(animTime * 8.0f) + 1.0f) * 0.5f;
        std::uint8_t glowAlpha = static_cast<std::uint8_t>(180 + pulse * 75);

        auto drawClockSpeedBadge = [&](float x, float y) {
            sf::RectangleShape badge({ 230.0f, 24.0f });
            badge.setPosition({ x, y });
            badge.setFillColor(sf::Color(45, 30, 8, 230));
            badge.setOutlineThickness(1.5f);
            badge.setOutlineColor(sf::Color(255, 215, 0, glowAlpha));
            window.draw(badge);

            sf::Text bt(font, toUtf8("⏩ 6x СКОРОСТ НА ВРЕМЕТО (ДОБИВ)"), 10);
            bt.setStyle(sf::Text::Bold);
            bt.setFillColor(sf::Color(255, 235, 120));
            sf::FloatRect btb = bt.getLocalBounds();
            bt.setPosition({ x + (230.0f - btb.size.x) / 2.0f, y + 5.0f });
            window.draw(bt);
        };

        drawClockSpeedBadge(20.0f, 115.0f);
        drawClockSpeedBadge(1600.0f - 250.0f, 115.0f);
    }
}

void UI_map::executeP1Action() {
    p1Pulse = 1.0f;
    BuildingType sel = engine.getSelectedBuilding(1);

    if (sel != BuildingType::NONE) {
        for (const auto& plot : engine.getLandPlots()) {
            if (plot.playerOwner == 1 && plot.bounds.contains(p1Pos)) {
                if (!plot.isPurchased) {
                    std::string buyMsg;
                    if (engine.buyLandPlot(1, plot.id, buyMsg)) {
                        triggerPlayerPopup(1, "ЗЕМЯ", "Купихте парцел!", "Парцелът е ваш. Натиснете пак SPACE за строеж.", "[SPACE]: Постави сградата", sf::Color(255, 215, 0));
                        spawnNotice("ЗАКУПЕН ПАРЦЕЛ!", p1Pos, sf::Color(255, 215, 0));
                    } else {
                        triggerPlayerModal(1, "НЕДОСТИГ НА ЗЛАТО", "Не можете да купите земята!", buyMsg, "Продавайте ток на града за да печелите пари и злато!", sf::Color(255, 180, 50));
                    }
                    return;
                }
                break;
            }
        }
        sf::Vector2f targetPos = (sel == BuildingType::DEMOLISH) ? p1Pos : engine.snapToBuildingGrid(1, p1Pos);
        std::string msg;
        if (engine.placeBuilding(1, sel, targetPos, msg)) {
            triggerPlayerPopup(1, "УСПЕХ", "Действието е успешно!", msg, "[E]: Постави отново същата", sf::Color(0, 255, 180));
            spawnNotice("ПОСТРОЕНА СГРАДА!", targetPos, sf::Color(0, 255, 180));
            if (sel != BuildingType::DEMOLISH) engine.clearBuildingSelection(1);
        } else {
            triggerPlayerModal(1, "ГРЕШКА ПРИ СТРОЕЖ", "Строежът е невъзможен!", msg,
                               (!engine.isDaylight() ? "Поставете и захранете Осветителна лампа за работа нощем!" : "Проверете ресурсите си или изберете друго място!"), sf::Color(255, 75, 75));
        }
    } else {
        ResourceType resType = nodes.getP1ResourceAt(p1Pos);
        if (resType != ResourceType::NONE) {
            if (p1ResourceCooldown > 0.0f) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "ИЗЧАКАЙТЕ: %.1fs", p1ResourceCooldown);
                spawnNotice(buf, p1Pos + sf::Vector2f(0.0f, -25.0f), sf::Color(255, 180, 50));
                return;
            }
            GameEngine::MineResult res;
            std::string msg;
            if (engine.mineResource(1, resType, res, msg)) {
                p1ResourceCooldown = 2.0f;
                const auto* st = nodes.getStation(1, resType);
                sf::Color c = st ? st->themeColor : sf::Color(0, 229, 255);
                spawnMiningParticles(p1Pos, c, 18);
                triggerPlayerPopup(1, "ДОБИВ", msg, "Ресурсът е добавен в склада.", "[SPACE]: Добив (на 2 сек)", c);
                spawnNotice(msg, p1Pos + sf::Vector2f(0.0f, -25.0f), c);
            }
        } else {
            for (const auto& plot : engine.getLandPlots()) {
                if (plot.playerOwner == 1 && plot.bounds.contains(p1Pos)) {
                    if (!plot.isPurchased) {
                        std::string msg;
                        if (engine.buyLandPlot(1, plot.id, msg)) {
                            triggerPlayerPopup(1, "ЗЕМЯ", "Закупен парцел!", "Парцелът е ваш. Натиснете E за избор на сграда.", "[E]: Избери сграда", sf::Color(255, 215, 0));
                            spawnNotice("ЗАКУПЕН ПАРЦЕЛ!", p1Pos, sf::Color(255, 215, 0));
                        } else {
                            triggerPlayerModal(1, "НЕДОСТИГ НА ЗЛАТО", "Не можете да купите парцела!", msg, "Продавайте ток на града за да печелите пари и злато!", sf::Color(255, 180, 50));
                        }
                    } else {
                        triggerPlayerPopup(1, "ИНФО", "Ваш парцел", "Земята е свободна за строителство.", "[E]: Изберете сграда за строеж", sf::Color(0, 229, 255));
                    }
                    break;
                }
            }
        }
    }
}

void UI_map::executeP2Action() {
    p2Pulse = 1.0f;
    BuildingType sel = engine.getSelectedBuilding(2);

    if (sel != BuildingType::NONE) {
        for (const auto& plot : engine.getLandPlots()) {
            if (plot.playerOwner == 2 && plot.bounds.contains(p2Pos)) {
                if (!plot.isPurchased) {
                    std::string buyMsg;
                    if (engine.buyLandPlot(2, plot.id, buyMsg)) {
                        triggerPlayerPopup(2, "ЗЕМЯ", "Купихте парцел!", "Парцелът е ваш. Натиснете пак ENTER за строеж.", "[ENTER]: Постави сградата", sf::Color(255, 215, 0));
                        spawnNotice("ЗАКУПЕН ПАРЦЕЛ!", p2Pos, sf::Color(255, 215, 0));
                    } else {
                        triggerPlayerModal(2, "НЕДОСТИГ НА ЗЛАТО", "Не можете да купите земята!", buyMsg, "Продавайте ток на града за да печелите пари и злато!", sf::Color(255, 180, 50));
                    }
                    return;
                }
                break;
            }
        }
        sf::Vector2f targetPos = (sel == BuildingType::DEMOLISH) ? p2Pos : engine.snapToBuildingGrid(2, p2Pos);
        std::string msg;
        if (engine.placeBuilding(2, sel, targetPos, msg)) {
            triggerPlayerPopup(2, "УСПЕХ", "Действието е успешно!", msg, "[PgDn]: Постави отново същата", sf::Color(255, 120, 200));
            spawnNotice("ПОСТРОЕНА СГРАДА!", targetPos, sf::Color(255, 120, 200));
            if (sel != BuildingType::DEMOLISH) engine.clearBuildingSelection(2);
        } else {
            triggerPlayerModal(2, "ГРЕШКА ПРИ СТРОЕЖ", "Строежът е невъзможен!", msg,
                               (!engine.isDaylight() ? "Поставете и захранете Осветителна лампа за работа нощем!" : "Проверете ресурсите си или изберете друго място!"), sf::Color(255, 75, 75));
        }
    } else {
        ResourceType resType = nodes.getP2ResourceAt(p2Pos);
        if (resType != ResourceType::NONE) {
            if (p2ResourceCooldown > 0.0f) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "ИЗЧАКАЙТЕ: %.1fs", p2ResourceCooldown);
                spawnNotice(buf, p2Pos + sf::Vector2f(0.0f, -25.0f), sf::Color(255, 180, 50));
                return;
            }
            GameEngine::MineResult res;
            std::string msg;
            if (engine.mineResource(2, resType, res, msg)) {
                p2ResourceCooldown = 2.0f;
                const auto* st = nodes.getStation(2, resType);
                sf::Color c = st ? st->themeColor : sf::Color(255, 140, 210);
                spawnMiningParticles(p2Pos, c, 18);
                triggerPlayerPopup(2, "ДОБИВ", msg, "Ресурсът е добавен в склада.", "[ENTER]: Добив (на 2 сек)", c);
                spawnNotice(msg, p2Pos + sf::Vector2f(0.0f, -25.0f), c);
            }
        } else {
            for (const auto& plot : engine.getLandPlots()) {
                if (plot.playerOwner == 2 && plot.bounds.contains(p2Pos)) {
                    if (!plot.isPurchased) {
                        std::string msg;
                        if (engine.buyLandPlot(2, plot.id, msg)) {
                            triggerPlayerPopup(2, "ЗЕМЯ", "Закупен парцел!", "Парцелът е ваш. Натиснете PgDn за избор.", "[PgDn]: Избери сграда", sf::Color(255, 215, 0));
                            spawnNotice("ЗАКУПЕН ПАРЦЕЛ!", p2Pos, sf::Color(255, 215, 0));
                        } else {
                            triggerPlayerPopup(2, "ГРЕШКА", "Няма злато!", msg, "[PgUp]: Отказ", sf::Color(255, 90, 90));
                        }
                    } else {
                        triggerPlayerPopup(2, "ИНФО", "Ваш парцел", "Земята е свободна за строителство.", "[PgDn]: Изберете сграда за строеж", sf::Color(255, 140, 220));
                    }
                    break;
                }
            }
        }
    }
}

void UI_map::updateControls(const sf::RenderWindow& window, float dt) {
    float speed = 360.0f;
    sf::Vector2f mPos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    // 1. Update floating notices & mining particles
    for (auto it = notices.begin(); it != notices.end();) {
        it->timer -= dt;
        it->pos.y -= 40.0f * dt;
        if (it->timer <= 0.0f) it = notices.erase(it);
        else ++it;
    }
    updateMiningParticles(dt);

    // 2. Resource zone detection -> 6x time speedup!
    bool p1InRes = (nodes.getP1ResourceAt(p1Pos) != ResourceType::NONE);
    bool p2InRes = (nodes.getP2ResourceAt(p2Pos) != ResourceType::NONE);
    if (p1InRes || p2InRes) {
        engine.setTimeScale(6.0f);
    } else {
        engine.setTimeScale(1.0f);
    }

    // Decrement grid step cooldowns
    if (p1GridStepCooldown > 0.0f) p1GridStepCooldown -= dt;
    if (p2GridStepCooldown > 0.0f) p2GridStepCooldown -= dt;

    // Helper to format building costs
    auto formatCost = [](const BuildingCost& c) {
        std::string s = "Нужно: " + std::to_string(c.woodCost) + " Дърво";
        if (c.ironCost > 0) s += ", " + std::to_string(c.ironCost) + " Жел";
        if (c.copperCost > 0) s += ", " + std::to_string(c.copperCost) + " Мед";
        if (c.siliconCost > 0) s += ", " + std::to_string(c.siliconCost) + " Сил";
        if (c.coalCost > 0) s += ", " + std::to_string(c.coalCost) + " Въгл";
        if (c.silverCost > 0) s += ", " + std::to_string(c.silverCost) + " Среб";
        return s;
    };

    // 3. Player 1 Movement (Precision Grid during placement, smooth analog otherwise)
    bool p1BuildingMode = (engine.getSelectedBuilding(1) != BuildingType::NONE);
    if (p1BuildingMode) {
        if (controlScheme == ControlScheme::BOTH_KEYBOARD || controlScheme == ControlScheme::P1_KEYBOARD_P2_MOUSE) {
            if (p1GridStepCooldown <= 0.0f) {
                bool moved = false;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
                    p1GridRow = std::max(0, p1GridRow - 1);
                    moved = true;
                } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
                    p1GridRow = std::min(5, p1GridRow + 1);
                    moved = true;
                }
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
                    p1GridCol = std::max(0, p1GridCol - 1);
                    moved = true;
                } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
                    p1GridCol = std::min(5, p1GridCol + 1);
                    moved = true;
                }
                if (moved) {
                    p1Pos = engine.getGridSlot(1, p1GridCol, p1GridRow);
                    p1GridStepCooldown = 0.14f;
                }
            }
        } else {
            engine.getClosestGridIndex(1, mPos, p1GridCol, p1GridRow);
            p1Pos = engine.getGridSlot(1, p1GridCol, p1GridRow);
        }
    } else {
        if (controlScheme == ControlScheme::BOTH_KEYBOARD || controlScheme == ControlScheme::P1_KEYBOARD_P2_MOUSE) {
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) p1Pos.y -= speed * dt;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) p1Pos.y += speed * dt;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) p1Pos.x -= speed * dt;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) p1Pos.x += speed * dt;
        } else if (controlScheme == ControlScheme::P1_MOUSE_P2_KEYBOARD) {
            p1Pos = mPos;
        } else if (controlScheme == ControlScheme::BOTH_MOUSE) {
            if (mPos.x <= 800.0f) p1Pos = mPos;
        }
        p1Pos.x = std::max(30.0f, std::min(p1Pos.x, 780.0f));
        p1Pos.y = std::max(40.0f, std::min(p1Pos.y, 860.0f));
        engine.getClosestGridIndex(1, p1Pos, p1GridCol, p1GridRow);
    }

    // 4. Player 2 Movement (Precision Grid during placement, smooth analog otherwise)
    bool p2BuildingMode = (engine.getSelectedBuilding(2) != BuildingType::NONE);
    if (p2BuildingMode) {
        if (controlScheme == ControlScheme::BOTH_KEYBOARD || controlScheme == ControlScheme::P1_MOUSE_P2_KEYBOARD) {
            if (p2GridStepCooldown <= 0.0f) {
                bool moved = false;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up)) {
                    p2GridRow = std::max(0, p2GridRow - 1);
                    moved = true;
                } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down)) {
                    p2GridRow = std::min(5, p2GridRow + 1);
                    moved = true;
                }
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)) {
                    p2GridCol = std::max(0, p2GridCol - 1);
                    moved = true;
                } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) {
                    p2GridCol = std::min(5, p2GridCol + 1);
                    moved = true;
                }
                if (moved) {
                    p2Pos = engine.getGridSlot(2, p2GridCol, p2GridRow);
                    p2GridStepCooldown = 0.14f;
                }
            }
        } else {
            engine.getClosestGridIndex(2, mPos, p2GridCol, p2GridRow);
            p2Pos = engine.getGridSlot(2, p2GridCol, p2GridRow);
        }
    } else {
        if (controlScheme == ControlScheme::BOTH_KEYBOARD || controlScheme == ControlScheme::P1_MOUSE_P2_KEYBOARD) {
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up)) p2Pos.y -= speed * dt;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down)) p2Pos.y += speed * dt;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)) p2Pos.x -= speed * dt;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) p2Pos.x += speed * dt;
        } else if (controlScheme == ControlScheme::P1_KEYBOARD_P2_MOUSE) {
            p2Pos = mPos;
        } else if (controlScheme == ControlScheme::BOTH_MOUSE) {
            if (mPos.x >= 800.0f) p2Pos = mPos;
        }
        p2Pos.x = std::max(820.0f, std::min(p2Pos.x, 1570.0f));
        p2Pos.y = std::max(40.0f, std::min(p2Pos.y, 860.0f));
        engine.getClosestGridIndex(2, p2Pos, p2GridCol, p2GridRow);
    }

    // 5. Action cooldown decrement
    if (p1ActionCooldown > 0.0f) p1ActionCooldown -= dt;
    if (p2ActionCooldown > 0.0f) p2ActionCooldown -= dt;
    if (p1ResourceCooldown > 0.0f) p1ResourceCooldown -= dt;
    if (p2ResourceCooldown > 0.0f) p2ResourceCooldown -= dt;

    // 6. Player 1 Action Input (Single Press only, NO continuous hold-to-mine!)
    bool p1PressingAction = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F);
    bool p1JustPressed = p1PressingAction && !p1PrevAction;
    p1PrevAction = p1PressingAction;

    if (p1JustPressed && p1ActionCooldown <= 0.0f && !p1Modal.active && !showHelpOverlay) {
        executeP1Action();
        p1ActionCooldown = 0.20f;
    }

    bool curE = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E);
    if (curE && !p1PrevE && !p1Modal.active && !showHelpOverlay) {
        engine.cycleBuildingSelection(1);
        BuildingType newSel = engine.getSelectedBuilding(1);
        BuildingCost c = engine.getBuildingCost(newSel);
        p1Pos = engine.getGridSlot(1, p1GridCol, p1GridRow);
        if (newSel == BuildingType::DEMOLISH) {
            triggerPlayerPopup(1, "ПРЕМАХВАНЕ", c.nameBg, "Кликнете върху ваша сграда за разрушаване.\nВръща 50% от ресурсите.", "[SPACE]: Премахни | [E]: Смени | [Q]: Отказ", sf::Color(255, 80, 80));
        } else if (newSel == BuildingType::LAMP) {
            triggerPlayerPopup(1, "ОСВЕТЛЕНИЕ", c.nameBg, formatCost(c) + ".\nОсветява нощем за строителство.", "[SPACE]: Постави | [E]: Смени | [Q]: Отказ", sf::Color(255, 220, 100));
        } else {
            triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg,
                               formatCost(c) + ".\nДобив: +" + std::to_string(c.basePowerMW) + " MW ток.",
                               "[SPACE]: Постави в грида | [E]: Смени | [Q]: Отказ", sf::Color(0, 229, 255));
        }
    }
    p1PrevE = curE;

    bool curQ = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q);
    if (curQ && !p1PrevQ) {
        if (engine.getSelectedBuilding(1) != BuildingType::NONE) {
            engine.clearBuildingSelection(1);
            triggerPlayerPopup(1, "ОТКАЗ", "Отменен строеж", "Режимът за поставяне е прекратен.", "[E]: Постави отново същата", sf::Color(180, 180, 180));
        }
    }
    p1PrevQ = curQ;

    bool curX = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::X);
    if (curX && !p1PrevX) {
        if (engine.getSelectedBuilding(1) == BuildingType::DEMOLISH) {
            engine.clearBuildingSelection(1);
            triggerPlayerPopup(1, "ОТКАЗ", "Премахването е отменено", "Свободен режим.", "[E]: Избери сграда", sf::Color(180, 180, 180));
        } else {
            engine.getPlayerEconomyMut(1).selectedBuilding = static_cast<int>(BuildingType::DEMOLISH);
            p1Pos = engine.getGridSlot(1, p1GridCol, p1GridRow);
            triggerPlayerPopup(1, "ПРЕМАХВАНЕ", "Режим Разрушаване", "Посочете сградата, която искате да махнете.", "[SPACE]: Премахни | [X]: Отказ", sf::Color(255, 80, 80));
        }
    }
    p1PrevX = curX;

    // Direct Hotkeys 1..6 for P1
    for (int k = 1; k <= 6; ++k) {
        sf::Keyboard::Key numKey = static_cast<sf::Keyboard::Key>(static_cast<int>(sf::Keyboard::Key::Num1) + (k - 1));
        bool curNum = sf::Keyboard::isKeyPressed(numKey);
        if (curNum && !p1PrevNum[k] && !p1Modal.active && !showHelpOverlay) {
            engine.getPlayerEconomyMut(1).selectedBuilding = k;
            BuildingCost c = engine.getBuildingCost(static_cast<BuildingType>(k));
            p1Pos = engine.getGridSlot(1, p1GridCol, p1GridRow);
            triggerPlayerPopup(1, (k == 6 ? "ПРЕМАХВАНЕ" : "СТРОЕЖ"), c.nameBg,
                               (k == 6 ? "Посочете сграда за разрушаване." : formatCost(c)),
                               "[SPACE]: Постави в грида | [Q]: Отказ", (k == 6 ? sf::Color(255, 80, 80) : sf::Color(0, 229, 255)));
        }
        p1PrevNum[k] = curNum;
    }

    // 7. Player 2 Action Input (Single Press only, NO continuous hold-to-mine!)
    bool p2PressingAction = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Numpad0) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
    bool p2JustPressed = p2PressingAction && !p2PrevAction;
    p2PrevAction = p2PressingAction;

    if (p2JustPressed && p2ActionCooldown <= 0.0f && !p2Modal.active && !showHelpOverlay) {
        executeP2Action();
        p2ActionCooldown = 0.20f;
    }

    bool curPgDn = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::PageDown);
    if (curPgDn && !p2PrevPgDn && !p2Modal.active && !showHelpOverlay) {
        engine.cycleBuildingSelection(2);
        BuildingType newSel = engine.getSelectedBuilding(2);
        BuildingCost c = engine.getBuildingCost(newSel);
        p2Pos = engine.getGridSlot(2, p2GridCol, p2GridRow);
        if (newSel == BuildingType::DEMOLISH) {
            triggerPlayerPopup(2, "ПРЕМАХВАНЕ", c.nameBg, "Кликнете върху ваша сграда за разрушаване.\nВръща 50% от ресурсите.", "[ENTER]: Премахни | [PgDn]: Смени | [PgUp]: Отказ", sf::Color(255, 80, 80));
        } else if (newSel == BuildingType::LAMP) {
            triggerPlayerPopup(2, "ОСВЕТЛЕНИЕ", c.nameBg, formatCost(c) + ".\nОсветява нощем за строителство.", "[ENTER]: Постави | [PgDn]: Смени | [PgUp]: Отказ", sf::Color(255, 220, 100));
        } else {
            triggerPlayerPopup(2, "СТРОЕЖ", c.nameBg,
                               formatCost(c) + ".\nДобив: +" + std::to_string(c.basePowerMW) + " MW ток.",
                               "[ENTER]: Постави в грида | [PgDn]: Смени | [PgUp]: Отказ", sf::Color(255, 120, 200));
        }
    }
    p2PrevPgDn = curPgDn;

    bool curPgUp = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::PageUp);
    if (curPgUp && !p2PrevPgUp) {
        if (engine.getSelectedBuilding(2) != BuildingType::NONE) {
            engine.clearBuildingSelection(2);
            triggerPlayerPopup(2, "ОТКАЗ", "Отменен строеж", "Режимът за поставяне е прекратен.", "[PgDn]: Постави отново същата", sf::Color(180, 180, 180));
        }
    }
    p2PrevPgUp = curPgUp;

    bool curDel = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Delete) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::End);
    if (curDel && !p2PrevDel) {
        if (engine.getSelectedBuilding(2) == BuildingType::DEMOLISH) {
            engine.clearBuildingSelection(2);
            triggerPlayerPopup(2, "ОТКАЗ", "Премахването е отменено", "Свободен режим.", "[PgDn]: Избери сграда", sf::Color(180, 180, 180));
        } else {
            engine.getPlayerEconomyMut(2).selectedBuilding = static_cast<int>(BuildingType::DEMOLISH);
            p2Pos = engine.getGridSlot(2, p2GridCol, p2GridRow);
            triggerPlayerPopup(2, "ПРЕМАХВАНЕ", "Режим Разрушаване", "Посочете сградата, която искате да махнете.", "[ENTER]: Премахни | [Del]: Отказ", sf::Color(255, 80, 80));
        }
    }
    p2PrevDel = curDel;

    // Pulse decay
    if (p1Pulse > 0.0f) {
        p1Pulse -= dt * 2.2f;
        if (p1Pulse < 0.0f) p1Pulse = 0.0f;
    }
    if (p2Pulse > 0.0f) {
        p2Pulse -= dt * 2.2f;
        if (p2Pulse < 0.0f) p2Pulse = 0.0f;
    }

    // Player Side Popup Timers
    if (p1Popup.active) {
        p1Popup.timer -= dt;
        if (p1Popup.timer <= 0.0f) p1Popup.active = false;
    }
    if (p2Popup.active) {
        p2Popup.timer -= dt;
        if (p2Popup.timer <= 0.0f) p2Popup.active = false;
    }
}

void UI_map::handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    // -------------------------------------------------------------------------
    // 0. If Help overlay is active, any dismiss key or click closes it
    // -------------------------------------------------------------------------
    if (showHelpOverlay) {
        if (event.is<sf::Event::MouseButtonPressed>()) {
            showHelpOverlay = false;
            return;
        }
        if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
            if (key->code == sf::Keyboard::Key::H || key->code == sf::Keyboard::Key::F1 ||
                key->code == sf::Keyboard::Key::Escape || key->code == sf::Keyboard::Key::Enter ||
                key->code == sf::Keyboard::Key::Space) {
                showHelpOverlay = false;
                return;
            }
        }
    }

    // -------------------------------------------------------------------------
    // 1. Check if interactive player modal dialog is active
    // -------------------------------------------------------------------------
    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        sf::Vector2f clickPos = window.mapPixelToCoords(mb->position);
        if (p1Modal.active) {
            if (p1Modal.okBtn.contains(clickPos) || p1Modal.box.contains(clickPos) || clickPos.x <= 800.0f) {
                closePlayerModal(1);
                return;
            }
        }
        if (p2Modal.active) {
            if (p2Modal.okBtn.contains(clickPos) || p2Modal.box.contains(clickPos) || clickPos.x > 800.0f) {
                closePlayerModal(2);
                return;
            }
        }
    }

    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        if (key->code == sf::Keyboard::Key::F11) {
            requestFullscreenToggle = true;
            return;
        }
        if (key->code == sf::Keyboard::Key::H || key->code == sf::Keyboard::Key::F1) {
            showHelpOverlay = !showHelpOverlay;
            return;
        }

        if (p1Modal.active) {
            if (key->code == sf::Keyboard::Key::Space || key->code == sf::Keyboard::Key::Enter ||
                key->code == sf::Keyboard::Key::E || key->code == sf::Keyboard::Key::Q) {
                closePlayerModal(1);
                return;
            }
        }
        if (p2Modal.active) {
            if (key->code == sf::Keyboard::Key::Enter || key->code == sf::Keyboard::Key::PageDown ||
                key->code == sf::Keyboard::Key::PageUp) {
                closePlayerModal(2);
                return;
            }
        }

        if (key->code == sf::Keyboard::Key::Escape || key->code == sf::Keyboard::Key::M) {
            requestMenu = true;
        }

        // =====================================================================
        // PLAYER 1 ACTIONS (WEST)
        // =====================================================================

        // [X]: Quick Demolish mode toggle
        if (key->code == sf::Keyboard::Key::X) {
            if (engine.getSelectedBuilding(1) == BuildingType::DEMOLISH) {
                engine.clearBuildingSelection(1);
                triggerPlayerPopup(1, "ОТКАЗ", "Премахването е отменено", "Свободен режим.", "[E]: Избери сграда", sf::Color(180, 180, 180));
            } else {
                engine.getPlayerEconomyMut(1).selectedBuilding = static_cast<int>(BuildingType::DEMOLISH);
                triggerPlayerPopup(1, "ПРЕМАХВАНЕ", "Режим Разрушаване", "Кликнете сградата, която искате да махнете.", "[КЛИК/SPACE]: Премахни | [Q/X]: Отказ", sf::Color(255, 80, 80));
            }
        }

        // [E]: Choose what to do (Cycle buildings: Solar -> Wind -> Hydro -> Battery -> Lamp -> Demolish)
        if (key->code == sf::Keyboard::Key::E) {
            engine.cycleBuildingSelection(1);
            BuildingType newSel = engine.getSelectedBuilding(1);
            BuildingCost c = engine.getBuildingCost(newSel);
            if (newSel == BuildingType::DEMOLISH) {
                triggerPlayerPopup(1, "ПРЕМАХВАНЕ", c.nameBg, "Кликнете върху ваша сграда за разрушаване.\nВръща 50% от дърво и руда.", "[SPACE/КЛИК]: Премахни | [E]: Смени | [Q]: Отказ", sf::Color(255, 80, 80));
            } else if (newSel == BuildingType::LAMP) {
                triggerPlayerPopup(1, "ОСВЕТЛЕНИЕ", c.nameBg, "Нужно: 15 Дърво, 10 Руда.\nОсветява нощем за строителство.", "[SPACE/КЛИК]: Постави | [E]: Смени | [Q]: Отказ", sf::Color(255, 220, 100));
            } else {
                triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg,
                                   "Нужно: " + std::to_string(c.woodCost) + " Дърво, " + std::to_string(c.oreCost) + " Руда.\nДобив: +" + std::to_string(c.basePowerMW) + " MW ток.",
                                   "[SPACE/КЛИК]: Постави | [E]: Смени | [Q]: Отказ", sf::Color(0, 229, 255));
            }
        }

        // [Q]: CANCEL / КЕНСЕЛИРАЙ
        if (key->code == sf::Keyboard::Key::Q) {
            if (engine.getSelectedBuilding(1) != BuildingType::NONE) {
                engine.clearBuildingSelection(1);
                triggerPlayerPopup(1, "ОТКАЗ", "Отменен строеж", "Режимът за поставяне е прекратен.", "[E]: Избери нова сграда", sf::Color(180, 180, 180));
            } else {
                triggerPlayerPopup(1, "ИНФО", "Свободен режим", "Няма активен избор на сграда.", "[E]: Избери сграда", sf::Color(180, 180, 180));
            }
        }

        // Direct Hotkeys 1..6 for P1
        if (key->code == sf::Keyboard::Key::Num1) {
            engine.getPlayerEconomyMut(1).selectedBuilding = 1;
            BuildingCost c = engine.getBuildingCost(BuildingType::SOLAR_PANEL);
            triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg, "Нужно: 35 Дърво, 30 Руда.\nДобив: +25 MW при слънце.", "[SPACE/КЛИК]: Постави | [Q]: Отказ", sf::Color(0, 229, 255));
        } else if (key->code == sf::Keyboard::Key::Num2) {
            engine.getPlayerEconomyMut(1).selectedBuilding = 2;
            BuildingCost c = engine.getBuildingCost(BuildingType::WIND_TURBINE);
            triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg, "Нужно: 50 Дърво, 45 Руда.\nДобив: +45 MW при вятър.", "[SPACE/КЛИК]: Постави | [Q]: Отказ", sf::Color(0, 229, 255));
        } else if (key->code == sf::Keyboard::Key::Num3) {
            engine.getPlayerEconomyMut(1).selectedBuilding = 3;
            BuildingCost c = engine.getBuildingCost(BuildingType::HYDRO_PLANT);
            triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg, "Нужно: 85 Дърво, 90 Руда.\nДобив: +90 MW при дъжд.", "[SPACE/КЛИК]: Постави | [Q]: Отказ", sf::Color(0, 229, 255));
        } else if (key->code == sf::Keyboard::Key::Num4) {
            engine.getPlayerEconomyMut(1).selectedBuilding = 4;
            BuildingCost c = engine.getBuildingCost(BuildingType::BATTERY);
            triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg, "Нужно: 30 Дърво, 60 Руда.\nЗарежда се денем, отдава нощем.", "[SPACE/КЛИК]: Постави | [Q]: Отказ", sf::Color(0, 229, 255));
        } else if (key->code == sf::Keyboard::Key::Num5) {
            engine.getPlayerEconomyMut(1).selectedBuilding = 5;
            BuildingCost c = engine.getBuildingCost(BuildingType::LAMP);
            triggerPlayerPopup(1, "ОСВЕТЛЕНИЕ", c.nameBg, "Нужно: 15 Дърво, 10 Руда.\nОсветява нощем за строителство.", "[SPACE/КЛИК]: Постави | [Q]: Отказ", sf::Color(255, 220, 100));
        } else if (key->code == sf::Keyboard::Key::Num6) {
            engine.getPlayerEconomyMut(1).selectedBuilding = 6;
            triggerPlayerPopup(1, "ПРЕМАХВАНЕ", "Режим Разрушаване", "Кликнете сграда за премахване.\nВръща 50% от дърво и руда.", "[SPACE/КЛИК]: Премахни | [Q]: Отказ", sf::Color(255, 80, 80));
        }

        // [Space] or [F]: Confirm / Place Building / Buy Land / Mine Resource
        if (key->code == sf::Keyboard::Key::Space || key->code == sf::Keyboard::Key::F) {
            executeP1Action();
        }

        // =====================================================================
        // =====================================================================
        // PLAYER 2 ACTIONS (EAST)
        // =====================================================================

        // [Delete] / [End]: Quick Demolish mode toggle for P2
        if (key->code == sf::Keyboard::Key::Delete || key->code == sf::Keyboard::Key::End) {
            if (engine.getSelectedBuilding(2) == BuildingType::DEMOLISH) {
                engine.clearBuildingSelection(2);
                triggerPlayerPopup(2, "ОТКАЗ", "Премахването е отменено", "Свободен режим.", "[PgDn]: Избери сграда", sf::Color(180, 180, 180));
            } else {
                engine.getPlayerEconomyMut(2).selectedBuilding = static_cast<int>(BuildingType::DEMOLISH);
                triggerPlayerPopup(2, "ПРЕМАХВАНЕ", "Режим Разрушаване", "Кликнете сградата, която искате да махнете.", "[ENTER]: Премахни | [PgUp]: Отказ", sf::Color(255, 80, 80));
            }
        }

        // [PgDn]: Choose what to do (Cycle buildings: Solar -> Wind -> Hydro -> Battery -> Lamp -> Demolish)
        if (key->code == sf::Keyboard::Key::PageDown) {
            engine.cycleBuildingSelection(2);
            BuildingType newSel = engine.getSelectedBuilding(2);
            BuildingCost c = engine.getBuildingCost(newSel);
            if (newSel == BuildingType::DEMOLISH) {
                triggerPlayerPopup(2, "ПРЕМАХВАНЕ", c.nameBg, "Кликнете върху ваша сграда за разрушаване.\nВръща 50% от дърво и руда.", "[ENTER/КЛИК]: Премахни | [PgDn]: Смени | [PgUp]: Отказ", sf::Color(255, 80, 80));
            } else if (newSel == BuildingType::LAMP) {
                triggerPlayerPopup(2, "ОСВЕТЛЕНИЕ", c.nameBg, "Нужно: 15 Дърво, 10 Руда.\nОсветява нощем за строителство.", "[ENTER/КЛИК]: Постави | [PgDn]: Смени | [PgUp]: Отказ", sf::Color(255, 220, 100));
            } else {
                triggerPlayerPopup(2, "СТРОЕЖ", c.nameBg,
                                   "Нужно: " + std::to_string(c.woodCost) + " Дърво, " + std::to_string(c.oreCost) + " Руда.\nДобив: +" + std::to_string(c.basePowerMW) + " MW ток.",
                                   "[ENTER/КЛИК]: Постави | [PgDn]: Смени | [PgUp]: Отказ", sf::Color(255, 120, 200));
            }
        }

        // [PgUp]: CANCEL / КЕНСЕЛИРАЙ
        if (key->code == sf::Keyboard::Key::PageUp) {
            if (engine.getSelectedBuilding(2) != BuildingType::NONE) {
                engine.clearBuildingSelection(2);
                triggerPlayerPopup(2, "ОТКАЗ", "Отменен строеж", "Режимът за поставяне е прекратен.", "[PgDn]: Избери нова сграда", sf::Color(180, 180, 180));
            } else {
                triggerPlayerPopup(2, "ИНФО", "Свободен режим", "Няма активен избор на сграда.", "[PgDn]: Избери сграда", sf::Color(180, 180, 180));
            }
        }

        // [Enter] or [Num0] or [RCtrl]: Confirm / Place / Buy / Mine
        if (key->code == sf::Keyboard::Key::Enter || key->code == sf::Keyboard::Key::Numpad0 || key->code == sf::Keyboard::Key::RControl) {
            executeP2Action();
        }
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        sf::Vector2f clickPos = window.mapPixelToCoords(mb->position);

        // Right-Click: CANCEL / КЕНСЕЛИРАЙ
        if (mb->button == sf::Mouse::Button::Right) {
            bool was1 = (engine.getSelectedBuilding(1) != BuildingType::NONE);
            bool was2 = (engine.getSelectedBuilding(2) != BuildingType::NONE);
            engine.clearBuildingSelection(1);
            engine.clearBuildingSelection(2);
            if (was1) triggerPlayerPopup(1, "ОТКАЗ", "Отменен строеж", "Режимът за поставяне е прекратен.", "", sf::Color(180, 180, 180));
            if (was2) triggerPlayerPopup(2, "ОТКАЗ", "Отменен строеж", "Режимът за поставяне е прекратен.", "", sf::Color(180, 180, 180));
            return;
        }

        // Left-Click:
        // Click on ESC / MENU
        if (sf::FloatRect({ 1600.0f - 130.0f, 900.0f - 34.0f }, { 120.0f, 28.0f }).contains(clickPos)) {
            requestMenu = true;
            return;
        }

        // Click on Fullscreen button
        if (sf::FloatRect({ 1600.0f - 275.0f, 900.0f - 34.0f }, { 135.0f, 28.0f }).contains(clickPos)) {
            requestFullscreenToggle = true;
            return;
        }

        // Click on Help button
        if (sf::FloatRect({ 1600.0f - 405.0f, 900.0f - 34.0f }, { 120.0f, 28.0f }).contains(clickPos)) {
            showHelpOverlay = !showHelpOverlay;
            return;
        }

        // 1. Building Menu Clicks
        BuildingType clickedP1 = p1Buildings.handleClick(clickPos);
        if (clickedP1 != BuildingType::NONE) {
            engine.getPlayerEconomyMut(1).selectedBuilding = static_cast<int>(clickedP1);
            BuildingCost c = engine.getBuildingCost(clickedP1);
            triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg,
                               "Нужно: " + std::to_string(c.woodCost) + " Дърво, " + std::to_string(c.oreCost) + " Руда.\nДобив: +" + std::to_string(c.basePowerMW) + " MW.",
                               "[КЛИК НА ЗЕМЯ]: Постави | [ДЕСЕН КЛИК]: Отказ", sf::Color(0, 229, 255));
            return;
        }

        BuildingType clickedP2 = p2Buildings.handleClick(clickPos);
        if (clickedP2 != BuildingType::NONE) {
            engine.getPlayerEconomyMut(2).selectedBuilding = static_cast<int>(clickedP2);
            BuildingCost c = engine.getBuildingCost(clickedP2);
            triggerPlayerPopup(2, "СТРОЕЖ", c.nameBg,
                               "Нужно: " + std::to_string(c.woodCost) + " Дърво, " + std::to_string(c.oreCost) + " Руда.\nДобив: +" + std::to_string(c.basePowerMW) + " MW.",
                               "[КЛИК НА ЗЕМЯ]: Постави | [ДЕСЕН КЛИК]: Отказ", sf::Color(255, 120, 200));
            return;
        }

        // 2. Buy Land HUD button clicks
        if (resourceHUD.getP1BuyLandButton().contains(clickPos)) {
            std::string msg;
            if (engine.buyNextLandTier(1, msg)) {
                triggerPlayerPopup(1, "ЗЕМЯ", "Разширена земя!", msg, "[E]: Избери сграда за строеж", sf::Color(255, 215, 0));
            } else {
                triggerPlayerPopup(1, "ГРЕШКА", "Няма злато!", msg, "[Q]: Отказ", sf::Color(255, 90, 90));
            }
            return;
        }
        if (resourceHUD.getP2BuyLandButton().contains(clickPos)) {
            std::string msg;
            if (engine.buyNextLandTier(2, msg)) {
                triggerPlayerPopup(2, "ЗЕМЯ", "Разширена земя!", msg, "[PgDn]: Избери сграда", sf::Color(255, 215, 0));
            } else {
                triggerPlayerPopup(2, "ГРЕШКА", "Няма злато!", msg, "[PgUp]: Отказ", sf::Color(255, 90, 90));
            }
            return;
        }

        // 3. Click on Land Plots directly (Buy Plot or Place Building on it)
        for (const auto& plot : engine.getLandPlots()) {
            if (plot.bounds.contains(clickPos)) {
                int owner = plot.playerOwner;
                if (!plot.isPurchased) {
                    std::string msg;
                    if (engine.buyLandPlot(owner, plot.id, msg)) {
                        triggerPlayerPopup(owner, "ЗЕМЯ", "Купихте парцел!", msg + "\nВече можете да строите тук.", "[КЛИК]: Постави сграда", sf::Color(255, 215, 0));
                    } else {
                        triggerPlayerModal(owner, "НЕДОСТИГ НА ЗЛАТО", "Не можете да купите парцела!", msg, "Продавайте ток на града за да печелите злато!", sf::Color(255, 180, 50));
                    }
                    return;
                } else {
                    BuildingType sel = engine.getSelectedBuilding(owner);
                    if (sel != BuildingType::NONE) {
                        sf::Vector2f targetPos = (sel == BuildingType::DEMOLISH) ? clickPos : engine.snapToBuildingGrid(owner, clickPos);
                        std::string msg;
                        if (engine.placeBuilding(owner, sel, targetPos, msg)) {
                            triggerPlayerPopup(owner, "УСПЕХ", "Действието е успешно!", msg, "", sf::Color(0, 255, 180));
                            if (sel != BuildingType::DEMOLISH) engine.clearBuildingSelection(owner);
                        } else {
                            triggerPlayerModal(owner, "ГРЕШКА ПРИ СТРОЕЖ", "Строежът е невъзможен!", msg,
                                               (!engine.isDaylight() ? "Поставете и захранете Осветителна лампа за работа нощем!" : "Проверете вашите ресурси и парцели!"), sf::Color(255, 75, 75));
                        }
                        return;
                    }
                }
            }
        }

        // 4. Click on Resource Stations
        ResourceType p1Res = nodes.getP1ResourceAt(clickPos);
        if (p1Res != ResourceType::NONE) {
            if (p1ResourceCooldown > 0.0f) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "ИЗЧАКАЙТЕ: %.1fs", p1ResourceCooldown);
                spawnNotice(buf, clickPos + sf::Vector2f(0.0f, -25.0f), sf::Color(255, 180, 50));
                return;
            }
            GameEngine::MineResult res;
            std::string msg;
            if (engine.mineResource(1, p1Res, res, msg)) {
                p1ResourceCooldown = 2.0f;
                p1Pulse = 1.0f;
                const auto* st = nodes.getStation(1, p1Res);
                sf::Color c = st ? st->themeColor : sf::Color(0, 229, 255);
                spawnMiningParticles(clickPos, c, 18);
                triggerPlayerPopup(1, "ДОБИВ", msg, "Ресурсът е добавен в склада.", "[E]: Избери сграда", c);
                spawnNotice(msg, clickPos + sf::Vector2f(0.0f, -25.0f), c);
            }
            return;
        }

        ResourceType p2Res = nodes.getP2ResourceAt(clickPos);
        if (p2Res != ResourceType::NONE) {
            if (p2ResourceCooldown > 0.0f) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "ИЗЧАКАЙТЕ: %.1fs", p2ResourceCooldown);
                spawnNotice(buf, clickPos + sf::Vector2f(0.0f, -25.0f), sf::Color(255, 180, 50));
                return;
            }
            GameEngine::MineResult res;
            std::string msg;
            if (engine.mineResource(2, p2Res, res, msg)) {
                p2ResourceCooldown = 2.0f;
                p2Pulse = 1.0f;
                const auto* st = nodes.getStation(2, p2Res);
                sf::Color c = st ? st->themeColor : sf::Color(255, 140, 210);
                spawnMiningParticles(clickPos, c, 18);
                triggerPlayerPopup(2, "ДОБИВ", msg, "Ресурсът е добавен в склада.", "[PgDn]: Избери сграда", c);
                spawnNotice(msg, clickPos + sf::Vector2f(0.0f, -25.0f), c);
            }
            return;
        }
    }
}

void UI_map::render(sf::RenderWindow& window) {
    float dt = deltaClock.restart().asSeconds();
    if (dt > 0.05f) dt = 0.05f;

    // 1. Advance continuous backend simulation
    engine.update(dt);
    updateControls(window, dt);
    updateWeatherParticles(dt);

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
    city.drawDividingRiver(window, font, resourcesLoaded, animTime);

    // 5. Purchasable Land Plots Grid
    nodes.drawLandPlots(window, font, resourcesLoaded, engine.getLandPlots(), mousePos);

    // Dynamic glowing energy conduit lines connecting generators to metropolis
    drawEnergyConduits(window, animTime);

    // 6. Placed Buildings on the Map
    nodes.drawPlacedBuildings(window, font, resourcesLoaded, engine.getBuildings());

    // 7. Holographic ghost preview if building is selected
    // 7. Holographic ghost preview if building is selected (snapped to plot grid)
    BuildingType p1Sel = engine.getSelectedBuilding(1);
    if (p1Sel != BuildingType::NONE) {
        sf::Vector2f targetPos = (p1Sel == BuildingType::DEMOLISH) ? p1Pos : engine.snapToBuildingGrid(1, p1Pos);
        std::string reason;
        bool valid = engine.canPlaceBuilding(1, p1Sel, targetPos, reason);
        nodes.drawBuildingGhost(window, font, resourcesLoaded, p1Sel, targetPos, valid, engine.getBuildingCost(p1Sel));
    }
    BuildingType p2Sel = engine.getSelectedBuilding(2);
    if (p2Sel != BuildingType::NONE) {
        sf::Vector2f targetPos = (p2Sel == BuildingType::DEMOLISH) ? p2Pos : engine.snapToBuildingGrid(2, p2Pos);
        std::string reason;
        bool valid = engine.canPlaceBuilding(2, p2Sel, targetPos, reason);
        nodes.drawBuildingGhost(window, font, resourcesLoaded, p2Sel, targetPos, valid, engine.getBuildingCost(p2Sel));
    }

    // 8. Compact Metropolis City Center with territorial slicing & conquest
    city.drawCity(window, font, resourcesLoaded, animTime, engine.getCityState().p1CityShare, engine.getCityState().lastCutMessage);

    // 9. City Demand & Influence Tug-of-War Bar (Above City)
    city.drawInfluenceBar(window, font, resourcesLoaded, engine.getCityState().cityEnergyDemand,
                          engine.getPlayerEconomy(1).energyMW, engine.getPlayerEconomy(2).energyMW,
                          engine.getCityState().p1CityShare);

    // 10. Resource Mines & Timber Forests
    nodes.drawNodes(window, font, resourcesLoaded, p1ResourceCooldown, p2ResourceCooldown);

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

    // 18. Player targeting cursors (RENDERED ON TOP OF EVERYTHING!)
    drawPlayerCursors(window);

    // 19. Floating Notices
    drawFloatingNotices(window);

    // 20. Interactive Help & Rules Manual Overlay (if opened)
    drawHelpOverlay(window);
}
