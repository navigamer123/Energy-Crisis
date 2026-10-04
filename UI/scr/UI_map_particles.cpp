#include "../includes/UI_map.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

// =============================================================================
// UI_map Particle Systems (Weather VFX & Mining Sparks)
// =============================================================================

void UI_map::triggerLightningStrike(sf::Vector2f targetPos, bool hitBuilding) {
    ActiveLightning bolt;
    bolt.startPos = sf::Vector2f(targetPos.x + static_cast<float>((rand() % 160) - 80), 0.0f);
    bolt.targetPos = targetPos;
    bolt.hitBuilding = hitBuilding;
    bolt.lifetime = 0.0f;
    bolt.maxLifetime = 0.32f;

    // Generate zigzag points from sky to target
    int segments = 12 + (rand() % 6);
    bolt.mainBolt.push_back(bolt.startPos);
    sf::Vector2f current = bolt.startPos;

    for (int i = 1; i < segments; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(segments);
        sf::Vector2f ideal = bolt.startPos + (targetPos - bolt.startPos) * t;
        float jitterX = static_cast<float>((rand() % 44) - 22);
        float jitterY = static_cast<float>((rand() % 20) - 10);
        current = sf::Vector2f(ideal.x + jitterX, ideal.y + jitterY);
        bolt.mainBolt.push_back(current);

        // Branching fork
        if (i > 2 && i < segments - 2 && (rand() % 3 == 0)) {
            std::vector<sf::Vector2f> branch;
            branch.push_back(current);
            sf::Vector2f bCurrent = current;
            int bSteps = 3 + (rand() % 3);
            for (int b = 0; b < bSteps; ++b) {
                bCurrent += sf::Vector2f(static_cast<float>((rand() % 60) - 30), static_cast<float>(18 + rand() % 24));
                branch.push_back(bCurrent);
            }
            bolt.branches.push_back(branch);
        }
    }
    bolt.mainBolt.push_back(targetPos);
    activeLightnings.push_back(bolt);

    lightningFlashTimer = 0.32f;
    fx.onLightning(targetPos, hitBuilding); // [b-effects] thunder (panned), light flash, screen shake

    // Strike sparks
    spawnMiningParticles(targetPos, sf::Color(210, 240, 255), 24);

    if (hitBuilding) {
        spawnMiningParticles(targetPos, sf::Color(255, 130, 60), 20);
        spawnNotice("МЪЛНИЯ УДАРИ СЪОРЪЖЕНИЕТО!", targetPos + sf::Vector2f(0.0f, -32.0f), sf::Color(255, 230, 80));
    }
}

void UI_map::updateWeatherParticles(float dt) {
    WeatherType w1 = engine.getPlayerWeather(1);
    WeatherType w2 = engine.getPlayerWeather(2);
    bool isNight = !engine.isDaylight();

    // 1. Update existing active lightnings
    for (auto it = activeLightnings.begin(); it != activeLightnings.end();) {
        it->lifetime += dt;
        if (it->lifetime >= it->maxLifetime) {
            it = activeLightnings.erase(it);
        } else {
            ++it;
        }
    }

    // 2. Flash timer decay
    if (lightningFlashTimer > 0.0f) {
        lightningFlashTimer -= dt;
        if (lightningFlashTimer < 0.0f) lightningFlashTimer = 0.0f;
    }

    // 3. Lightning Strike schedule & strike chance.
    //    Each sector rolls on its own: strikes happen ONLY while that sector's weather is STORMY,
    //    they can only hit buildings standing in that sector (West = P1, East = P2), and the
    //    schedule runs on game time (dt * time scale) so strikes per game day do not depend on
    //    the mining speed-up. Bolt/flash animations above stay on real time.
    float gameDt = dt * engine.getTimeScale();
    for (int sector = 1; sector <= 2; ++sector) {
        float& strikeCooldown = (sector == 1) ? lightningStrikeCooldown : lightningStrikeCooldownP2;
        WeatherType sectorWeather = (sector == 1) ? w1 : w2;
        if (sectorWeather != WeatherType::STORMY) {
            continue; // Calm sky in this sector: no lightning, schedule paused
        }

        strikeCooldown -= gameDt;
        if (strikeCooldown > 0.0f) {
            continue;
        }
        strikeCooldown = 6.0f + static_cast<float>(rand() % 6);

        // Lightning strikes!
        // Very small chance to hit a facility ("много малък шанс да чупят съоръжението")
        // "ако бъдат счупени да изчезват за да може да бъде построен друг ВЕЦ"
        // Facilities are safe during the grace period.
        const auto& allBuildings = engine.getBuildings();
        std::vector<size_t> sectorBuildings;
        for (size_t i = 0; i < allBuildings.size(); ++i) {
            if (allBuildings[i].playerOwner == sector) {
                sectorBuildings.push_back(i);
            }
        }

        if (!sectorBuildings.empty() && !engine.isGracePeriod() && (rand() % 12 == 0)) {
            size_t chosen = sectorBuildings[static_cast<size_t>(rand()) % sectorBuildings.size()];
            sf::Vector2f strikePos = allBuildings[chosen].position;
            BuildingType hitType = allBuildings[chosen].type;

            // Destroy and remove building so it disappears immediately and frees the grid slot!
            // (allBuildings must not be used after this call)
            engine.breakBuildingAt(strikePos);

            // team info: the notification system shows the critical toast and logs the loss
            InfoEvent lost;
            lost.type = InfoEventType::LOST_LIGHTNING;
            lost.player = sector;
            lost.building = hitType;
            onInfoEvent(lost);

            spawnNotice("СЪОРЪЖЕНИЕТО Е УНИЩОЖЕНО!", strikePos + sf::Vector2f(0.0f, -32.0f), sf::Color(255, 80, 80));
            triggerLightningStrike(strikePos, true);
        } else {
            // Harmless strike into open ground of the stormy sector
            float minX = (sector == 1) ? 80.0f : 820.0f;
            float sx = minX + static_cast<float>(rand() % 700);
            float sy = 160.0f + static_cast<float>(rand() % 650);
            triggerLightningStrike(sf::Vector2f(sx, sy), false);
        }
    }

    // [b-effects] UX-09 / HX-11: calm days get their own seasonal particles (warm dust, spring petals,
    // autumn leaves) instead of the night firefly type; fireflies only on warm-season nights.
    SeasonType season = engine.getSeason();
    for (size_t i = 0; i < particles.size(); ++i) {
        auto& p = particles[i];
        WeatherType w = (p.pos.x < 800.0f) ? w1 : w2;
        bool seasonal = (i % 3 == 0); // a third of the particles carry the season's petals / leaves

        if (w == WeatherType::RAINY || w == WeatherType::STORMY) {
            p.type = 0; // Rain
            p.vel = sf::Vector2f((w == WeatherType::STORMY ? -140.0f : -60.0f), (w == WeatherType::STORMY ? 750.0f : 550.0f));
        } else if (w == WeatherType::SNOWY) {
            p.type = 1; // Snow (snow / hail days only, not every dry winter day)
            float drift = std::sin(p.pos.y * 0.02f + static_cast<float>(i)) * 40.0f;
            p.vel = sf::Vector2f(drift, 65.0f);
        } else if (w == WeatherType::WINDY) {
            p.type = (season == SeasonType::AUTUMN && seasonal) ? 6 : 2; // Autumn leaves ride the wind, else streaks
            p.vel = sf::Vector2f(320.0f, std::sin(p.pos.x * 0.015f) * 35.0f);
        } else if (isNight) {
            bool warm = (season == SeasonType::SPRING || season == SeasonType::SUMMER || (season == SeasonType::AUTUMN && i % 2 == 0));
            p.type = warm ? 3 : 4; // Firefly on warm nights, faint frost glitter otherwise
            p.vel = sf::Vector2f(std::cos(p.pos.y * 0.03f) * 12.0f, std::sin(p.pos.x * 0.03f) * 12.0f);
        } else if (season == SeasonType::SPRING && seasonal) {
            p.type = 5; // Blossom petal drifting down
            p.vel = sf::Vector2f(18.0f + std::sin(p.pos.y * 0.03f + static_cast<float>(i)) * 22.0f, 38.0f);
        } else if (season == SeasonType::AUTUMN && seasonal) {
            p.type = 6; // Falling leaf, swaying
            p.vel = sf::Vector2f(std::sin(p.pos.y * 0.025f + static_cast<float>(i)) * 45.0f, 48.0f);
        } else {
            p.type = 4; // Calm daylight dust mote (warm white, low alpha)
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

// Thick line segment (SFML lines are 1 px, too thin for a bolt on the bright ground)
static void drawThickSegment(sf::RenderWindow& window, sf::Vector2f a, sf::Vector2f b, float width, sf::Color color) {
    sf::Vector2f d = b - a;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len <= 0.0f) return;
    sf::Vector2f n(-d.y / len * width * 0.5f, d.x / len * width * 0.5f);
    sf::ConvexShape quad(4);
    quad.setPoint(0, a + n);
    quad.setPoint(1, b + n);
    quad.setPoint(2, b - n);
    quad.setPoint(3, a - n);
    quad.setFillColor(color);
    window.draw(quad);
}

void UI_map::drawWeatherParticles(sf::RenderWindow& window) {
    // [b-effects] Drawn in the world layer (UX-09): particles first, lit by the time of day, then
    // lightning on top. The screen flash is softer now that the light map also flashes.
    auto lit = [this](sf::Color c, float x) {
        sf::Color amb = fx.ambientColorAt(x);
        return sf::Color(static_cast<std::uint8_t>(c.r * (amb.r + 60) / 315), static_cast<std::uint8_t>(c.g * (amb.g + 60) / 315),
                         static_cast<std::uint8_t>(c.b * (amb.b + 60) / 315), c.a);
    };
    const sf::FloatRect cityRect = city.getCityBounds();
    auto overGrass = [&](sf::Vector2f pos) {
        if (cityRect.contains(pos)) return false;
        for (const auto& plot : engine.getLandPlots()) {
            if (plot.bounds.contains(pos)) return false;
        }
        return nodes.getP1StationAt(pos) == ResourceType::NONE && nodes.getP2StationAt(pos) == ResourceType::NONE;
    };

    for (const auto& p : particles) {
        // Dust, petals and leaves never drift over the city banner or the mine cards (keeps their text clean)
        if (p.type >= 4 && (cityRect.contains(p.pos) || nodes.getP1StationAt(p.pos) != ResourceType::NONE ||
                            nodes.getP2StationAt(p.pos) != ResourceType::NONE)) {
            continue;
        }
        if (p.type == 0) {
            // Rain streak
            sf::Vertex line[2];
            line[0].position = p.pos;
            line[0].color = lit(sf::Color(160, 210, 255, 160), p.pos.x);
            line[1].position = p.pos + sf::Vector2f(p.vel.x * 0.025f, p.vel.y * 0.025f);
            line[1].color = lit(sf::Color(200, 235, 255, 220), p.pos.x);
            window.draw(line, 2, sf::PrimitiveType::Lines);
        } else if (p.type == 1) {
            // Snow flake
            sf::CircleShape flake(p.size);
            flake.setPosition(p.pos);
            flake.setFillColor(lit(sf::Color(255, 255, 255, static_cast<std::uint8_t>(p.alpha * 0.85f)), p.pos.x));
            window.draw(flake);
        } else if (p.type == 2) {
            // Wind streak
            sf::Vertex streak[2];
            streak[0].position = p.pos;
            streak[0].color = lit(sf::Color(235, 245, 255, 0), p.pos.x);
            streak[1].position = p.pos + sf::Vector2f(22.0f, p.vel.y * 0.05f);
            streak[1].color = lit(sf::Color(235, 245, 255, 120), p.pos.x);
            window.draw(streak, 2, sf::PrimitiveType::Lines);
        } else if (p.type == 3) {
            // Firefly: only over open grass (never over the city, plots or mines), softly pulsing
            if (!overGrass(p.pos)) continue;
            float pulse = 0.5f + 0.5f * std::sin(animClock.getElapsedTime().asSeconds() * 2.3f + p.pos.x * 0.05f);
            sf::CircleShape halo(p.size + 2.5f);
            halo.setOrigin({ p.size + 2.5f, p.size + 2.5f });
            halo.setPosition(p.pos);
            halo.setFillColor(sf::Color(210, 255, 140, static_cast<std::uint8_t>(45.0f * pulse)));
            window.draw(halo);
            sf::CircleShape glow(p.size * 0.6f);
            glow.setOrigin({ p.size * 0.6f, p.size * 0.6f });
            glow.setPosition(p.pos);
            glow.setFillColor(sf::Color(235, 255, 170, static_cast<std::uint8_t>(90.0f + 140.0f * pulse)));
            window.draw(glow);
        } else if (p.type == 4) {
            // Dust mote (day) / frost glitter (cold nights): warm white, low alpha, tiny
            sf::CircleShape mote(1.2f);
            mote.setPosition(p.pos);
            mote.setFillColor(lit(sf::Color(255, 246, 225, 70), p.pos.x));
            window.draw(mote);
        } else if (p.type == 5) {
            // Spring blossom petal
            sf::CircleShape petal(2.6f, 6);
            petal.setOrigin({ 2.6f, 2.6f });
            petal.setScale({ 1.0f, 0.6f });
            petal.setRotation(sf::degrees(p.pos.y * 1.3f));
            petal.setPosition(p.pos);
            petal.setFillColor(lit(sf::Color(255, 190, 215, 220), p.pos.x));
            window.draw(petal);
        } else if (p.type == 6) {
            // Autumn leaf
            sf::RectangleShape leaf({ 6.0f, 3.0f });
            leaf.setOrigin({ 3.0f, 1.5f });
            leaf.setPosition(p.pos);
            leaf.setRotation(sf::degrees(p.pos.y * 2.0f + p.pos.x * 0.5f));
            static const sf::Color leafCols[3] = { sf::Color(214, 110, 36, 230), sf::Color(236, 170, 50, 230), sf::Color(170, 70, 30, 230) };
            leaf.setFillColor(lit(leafCols[static_cast<int>(p.size) % 3], p.pos.x));
            window.draw(leaf);
        }
    }

    // 1. Screen Lightning flash
    if (lightningFlashTimer > 0.0f) {
        sf::RectangleShape flash({ VIRTUAL_WIDTH, VIRTUAL_HEIGHT });
        flash.setPosition({ 0.0f, 0.0f });
        std::uint8_t a = static_cast<std::uint8_t>(std::min(150.0f, lightningFlashTimer * 520.0f));
        flash.setFillColor(sf::Color(220, 240, 255, a));
        window.draw(flash);
    }

    // 2. Active electrifying lightning bolts
    for (const auto& bolt : activeLightnings) {
        float alphaRatio = 1.0f - (bolt.lifetime / bolt.maxLifetime);
        std::uint8_t alpha = static_cast<std::uint8_t>(std::clamp(alphaRatio * 255.0f, 0.0f, 255.0f));

        if (bolt.mainBolt.size() >= 2) {
            for (size_t i = 0; i < bolt.mainBolt.size() - 1; ++i) {
                // Outer cyan electric glow, then the brilliant hot white core
                drawThickSegment(window, bolt.mainBolt[i], bolt.mainBolt[i + 1], 7.0f,
                                 sf::Color(110, 215, 255, static_cast<std::uint8_t>(alpha * 0.35f)));
                drawThickSegment(window, bolt.mainBolt[i], bolt.mainBolt[i + 1], 3.5f,
                                 sf::Color(150, 230, 255, static_cast<std::uint8_t>(alpha * 0.75f)));
                drawThickSegment(window, bolt.mainBolt[i], bolt.mainBolt[i + 1], 1.6f,
                                 sf::Color(255, 255, 255, alpha));
            }
        }

        // Side fork branches
        for (const auto& branch : bolt.branches) {
            if (branch.size() >= 2) {
                for (size_t i = 0; i < branch.size() - 1; ++i) {
                    drawThickSegment(window, branch[i], branch[i + 1], 2.0f,
                                     sf::Color(160, 230, 255, static_cast<std::uint8_t>(alpha * 0.55f)));
                }
            }
        }

        // Ground/facility impact burst
        sf::CircleShape impact(bolt.hitBuilding ? 16.0f : 10.0f);
        impact.setOrigin({ impact.getRadius(), impact.getRadius() });
        impact.setPosition(bolt.targetPos);
        impact.setFillColor(sf::Color(255, 255, 255, static_cast<std::uint8_t>(alpha * 0.75f)));
        window.draw(impact);
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
