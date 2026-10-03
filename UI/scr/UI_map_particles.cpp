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
    SeasonType season = engine.getSeason();
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

    // 3. Lightning Strike schedule & strike chance
    lightningStrikeCooldown -= dt;
    if (lightningStrikeCooldown <= 0.0f) {
        bool isStormy = (w1 == WeatherType::STORMY || w2 == WeatherType::STORMY);
        bool isRainy = (w1 == WeatherType::RAINY || w2 == WeatherType::RAINY);

        if (isStormy) {
            lightningStrikeCooldown = 6.0f + static_cast<float>(rand() % 6);
        } else if (isRainy) {
            lightningStrikeCooldown = 18.0f + static_cast<float>(rand() % 12);
        } else {
            lightningStrikeCooldown = 45.0f + static_cast<float>(rand() % 35);
        }

        // Lightning strikes!
        // Very small chance to hit a facility ("много малък шанс да чупят съоръжението")
        // "ако бъдат счупени да изчезват за да може да бъде построен друг ВЕЦ"
        const auto& allBuildings = engine.getBuildings();
        if (!allBuildings.empty() && (rand() % 12 == 0)) {
            size_t chosen = rand() % allBuildings.size();
            sf::Vector2f strikePos = allBuildings[chosen].position;
            int owner = allBuildings[chosen].playerOwner;
            BuildingCost cost = engine.getBuildingCost(allBuildings[chosen].type);

            // Destroy and remove building so it disappears immediately and frees the grid slot!
            const_cast<GameEngine&>(engine).breakBuildingAt(strikePos);

            triggerPlayerPopup(owner, "МЪЛНИЯ!", "Унищожено съоръжение!",
                               "Мълния унищожи " + cost.nameBg + "!\nКлетката се освободи за нов строеж (ВЕЦ/друг).",
                               "[SPACE/Клик]: Постройте ново съоръжение", sf::Color(255, 230, 80));

            spawnNotice("СЪОРЪЖЕНИЕТО Е УНИЩОЖЕНО!", strikePos + sf::Vector2f(0.0f, -32.0f), sf::Color(255, 80, 80));
            triggerLightningStrike(strikePos, true);
        } else {
            // Harmless strike into open ground
            float sx = 80.0f + static_cast<float>(rand() % 1440);
            float sy = 160.0f + static_cast<float>(rand() % 650);
            triggerLightningStrike(sf::Vector2f(sx, sy), false);
        }
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
    // 1. Screen Lightning flash
    if (lightningFlashTimer > 0.0f) {
        sf::RectangleShape flash({ VIRTUAL_WIDTH, VIRTUAL_HEIGHT });
        flash.setPosition({ 0.0f, 0.0f });
        std::uint8_t a = static_cast<std::uint8_t>(std::min(220.0f, lightningFlashTimer * 750.0f));
        flash.setFillColor(sf::Color(220, 240, 255, a));
        window.draw(flash);
    }

    // 2. Active electrifying lightning bolts
    for (const auto& bolt : activeLightnings) {
        float alphaRatio = 1.0f - (bolt.lifetime / bolt.maxLifetime);
        std::uint8_t alpha = static_cast<std::uint8_t>(std::clamp(alphaRatio * 255.0f, 0.0f, 255.0f));

        if (bolt.mainBolt.size() >= 2) {
            for (size_t i = 0; i < bolt.mainBolt.size() - 1; ++i) {
                // Outer cyan electric glow
                sf::Vertex glow[2];
                glow[0].position = bolt.mainBolt[i];
                glow[0].color = sf::Color(110, 215, 255, static_cast<std::uint8_t>(alpha * 0.65f));
                glow[1].position = bolt.mainBolt[i + 1];
                glow[1].color = sf::Color(110, 215, 255, static_cast<std::uint8_t>(alpha * 0.65f));
                window.draw(glow, 2, sf::PrimitiveType::Lines);

                // Core brilliant hot white bolt
                sf::Vertex core[2];
                core[0].position = bolt.mainBolt[i] + sf::Vector2f(1.0f, 0.0f);
                core[0].color = sf::Color(255, 255, 255, alpha);
                core[1].position = bolt.mainBolt[i + 1] + sf::Vector2f(1.0f, 0.0f);
                core[1].color = sf::Color(255, 255, 255, alpha);
                window.draw(core, 2, sf::PrimitiveType::Lines);
            }
        }

        // Side fork branches
        for (const auto& branch : bolt.branches) {
            if (branch.size() >= 2) {
                for (size_t i = 0; i < branch.size() - 1; ++i) {
                    sf::Vertex bLine[2];
                    bLine[0].position = branch[i];
                    bLine[0].color = sf::Color(140, 225, 255, static_cast<std::uint8_t>(alpha * 0.55f));
                    bLine[1].position = branch[i + 1];
                    bLine[1].color = sf::Color(180, 240, 255, static_cast<std::uint8_t>(alpha * 0.35f));
                    window.draw(bLine, 2, sf::PrimitiveType::Lines);
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

    // 3. Particles
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
