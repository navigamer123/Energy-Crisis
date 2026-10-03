#include "../includes/UI_map.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

// =============================================================================
// UI_map Particle Systems (Weather VFX & Mining Sparks)
// =============================================================================

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
