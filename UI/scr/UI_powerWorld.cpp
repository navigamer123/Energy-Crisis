// =============================================================================
// Team b-power: world layer for terrain plots (F-15), the nuclear reactor
// (F-32), hazard damage (F-34) and mega-projects (HX-10).
// =============================================================================
#include "../includes/UI_power.h"
#include "../includes/UI_types.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

// Colours (the integrator maps these to UI_theme tokens)
const sf::Color PW_P1(0, 229, 255);
const sf::Color PW_P2(255, 120, 200);
const sf::Color PW_TEXT(230, 240, 252);
const sf::Color PW_MUTED(160, 182, 210);
const sf::Color PW_PANEL(14, 20, 32, 236);
const sf::Color PW_PANEL_BORDER(70, 95, 125);
const sf::Color PW_GOOD(90, 230, 140);
const sf::Color PW_BAD(255, 90, 90);
const sf::Color PW_WARN(255, 200, 60);
const sf::Color PW_HAZARD_STRIPE(255, 205, 40);
const sf::Color PW_SCAFFOLD(200, 170, 90);
const sf::Color PW_SMOKE(70, 74, 82);

constexpr float PI = 3.14159265f;

sf::Color owner(int player) { return (player == 1) ? PW_P1 : PW_P2; }
sf::Color withAlpha(sf::Color c, float a) { c.a = static_cast<std::uint8_t>(std::max(0.0f, std::min(255.0f, a))); return c; }

void fillRect(sf::RenderTarget& t, sf::Vector2f pos, sf::Vector2f size, sf::Color fill, sf::Color line = sf::Color::Transparent, float th = 0.0f) {
    sf::RectangleShape r(size);
    r.setPosition(pos);
    r.setFillColor(fill);
    if (th != 0.0f) {
        r.setOutlineThickness(th);
        r.setOutlineColor(line);
    }
    t.draw(r);
}

void disc(sf::RenderTarget& t, sf::Vector2f c, float r, sf::Color fill, sf::Color line = sf::Color::Transparent, float th = 0.0f) {
    sf::CircleShape s(r, (r > 8.0f) ? 24 : 12);
    s.setOrigin({ r, r });
    s.setPosition(c);
    s.setFillColor(fill);
    if (th != 0.0f) {
        s.setOutlineThickness(th);
        s.setOutlineColor(line);
    }
    t.draw(s);
}

void segment(sf::RenderTarget& t, sf::Vector2f a, sf::Vector2f b, float th, sf::Color c) {
    sf::Vector2f d = b - a;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 0.01f) return;
    sf::RectangleShape r({ len, th });
    r.setOrigin({ 0.0f, th * 0.5f });
    r.setPosition(a);
    r.setRotation(sf::radians(std::atan2(d.y, d.x)));
    r.setFillColor(c);
    t.draw(r);
}

sf::Text makeText(const sf::Font& f, const std::string& s, unsigned size, sf::Color c) {
    sf::Text t(f, toUtf8(s), size);
    t.setFillColor(c);
    return t;
}

void textAt(sf::RenderTarget& w, const sf::Font& f, const std::string& s, unsigned size, sf::Color c, sf::Vector2f pos, bool centred) {
    sf::Text t = makeText(f, s, size, c);
    sf::FloatRect b = t.getLocalBounds();
    t.setPosition({ centred ? std::round(pos.x - b.size.x * 0.5f - b.position.x) : std::round(pos.x), std::round(pos.y) });
    w.draw(t);
}

// Text with a dark plate behind it (stays readable on any terrain)
void plateText(sf::RenderTarget& w, const sf::Font& f, const std::string& s, unsigned size, sf::Color c, sf::Vector2f centreTop,
               sf::Color plate = sf::Color(10, 14, 22, 210)) {
    sf::Text t = makeText(f, s, size, c);
    sf::FloatRect b = t.getLocalBounds();
    float x = std::round(centreTop.x - b.size.x * 0.5f - b.position.x);
    fillRect(w, { x + b.position.x - 3.0f, centreTop.y + b.position.y - 2.0f }, { b.size.x + 6.0f, b.size.y + 4.0f }, plate);
    t.setPosition({ x, std::round(centreTop.y) });
    w.draw(t);
}

void progressBar(sf::RenderTarget& w, sf::Vector2f pos, sf::Vector2f size, float frac, sf::Color fill) {
    fillRect(w, pos, size, sf::Color(8, 12, 20, 230), sf::Color(90, 110, 140), 1.0f);
    float f = std::max(0.0f, std::min(1.0f, frac));
    if (f > 0.0f) fillRect(w, { pos.x + 1.0f, pos.y + 1.0f }, { (size.x - 2.0f) * f, size.y - 2.0f }, fill);
}

// Deterministic 0..1 noise for decoration positions
float hash01(int a, int b) {
    unsigned h = static_cast<unsigned>(a) * 73856093u ^ static_cast<unsigned>(b) * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return static_cast<float>(h % 10000u) / 10000.0f;
}

const LandPlot* plotUnder(const GameEngine& e, const PlacedBuilding& b) { return e.findPlotAt(b.playerOwner, b.position); }

// Yellow/black dashes around a plot: the reactor exclusion zone
void hazardBorder(sf::RenderTarget& w, sf::FloatRect r, float animTime) {
    const float dash = 8.0f;
    float shift = std::fmod(animTime * 10.0f, dash * 2.0f);
    auto edge = [&](sf::Vector2f a, sf::Vector2f dir, float len) {
        for (float s = -shift; s < len; s += dash * 2.0f) {
            float s0 = std::max(0.0f, s), s1 = std::min(len, s + dash);
            if (s1 <= s0) continue;
            segment(w, a + dir * s0, a + dir * s1, 2.5f, PW_HAZARD_STRIPE);
        }
    };
    sf::Vector2f p = r.position + sf::Vector2f(1.5f, 1.5f);
    float W = r.size.x - 3.0f, H = r.size.y - 3.0f;
    edge(p, { 1.0f, 0.0f }, W);
    edge(p + sf::Vector2f(W, 0.0f), { 0.0f, 1.0f }, H);
    edge(p + sf::Vector2f(W, H), { -1.0f, 0.0f }, W);
    edge(p + sf::Vector2f(0.0f, H), { 0.0f, -1.0f }, H);
}

void steamPuffs(sf::RenderTarget& w, sf::Vector2f base, float strength, float animTime, float scale, int seed) {
    if (strength <= 0.01f) return;
    for (int i = 0; i < 4; ++i) {
        float ph = std::fmod(animTime * 0.55f + i * 0.25f + seed * 0.13f, 1.0f);
        float r = (2.5f + ph * 7.0f) * scale;
        sf::Vector2f c(base.x + std::sin(ph * 5.0f + seed) * 4.0f * scale, base.y - ph * 26.0f * scale);
        disc(w, c, r, withAlpha(sf::Color(238, 242, 248), (1.0f - ph) * 170.0f * strength));
    }
}

void crane(sf::RenderTarget& w, sf::FloatRect r, float animTime, float progress) {
    float mastX = r.position.x + r.size.x * 0.86f;
    float top = r.position.y + 21.0f; // stays inside the plot, under the status band
    float bottom = r.position.y + r.size.y - 6.0f;
    // Lattice mast
    segment(w, { mastX - 2.0f, bottom }, { mastX - 2.0f, top }, 1.6f, PW_WARN);
    segment(w, { mastX + 2.0f, bottom }, { mastX + 2.0f, top }, 1.6f, PW_WARN);
    for (float y = bottom; y > top + 6.0f; y -= 8.0f) segment(w, { mastX - 2.0f, y }, { mastX + 2.0f, y - 8.0f }, 1.0f, PW_WARN);
    // Jib and counter-jib
    float jibL = r.size.x * 0.62f;
    segment(w, { mastX + 10.0f, top }, { mastX - jibL, top }, 2.0f, PW_WARN);
    disc(w, { mastX + 10.0f, top + 2.0f }, 3.0f, sf::Color(90, 96, 110));
    // Trolley + swinging hook carrying a beam
    float trolley = mastX - jibL * (0.35f + 0.25f * std::sin(animTime * 0.6f));
    float swing = std::sin(animTime * 2.1f) * 3.0f;
    float hookY = top + 14.0f + (1.0f - progress) * r.size.y * 0.35f;
    segment(w, { trolley, top }, { trolley + swing, hookY }, 1.0f, sf::Color(40, 44, 52));
    fillRect(w, { trolley + swing - 9.0f, hookY }, { 18.0f, 3.0f }, sf::Color(170, 120, 60));
}

} // namespace

// -----------------------------------------------------------------------------
// Particles
// -----------------------------------------------------------------------------
void UI_powerLayer::reset() {
    particles.clear();
    rings.clear();
    shake = 0.0f;
}

void UI_powerLayer::spawn(sf::Vector2f pos, sf::Vector2f vel, float life, float size, sf::Color c, int kind) {
    if (particles.size() > 900) return;
    Particle p;
    p.pos = pos;
    p.vel = vel;
    p.life = p.maxLife = life;
    p.size = size;
    p.color = c;
    p.kind = kind;
    particles.push_back(p);
}

void UI_powerLayer::spawnRing(sf::Vector2f pos, float maxRadius, float life, sf::Color c) {
    Ring r;
    r.pos = pos;
    r.maxRadius = maxRadius;
    r.life = r.maxLife = life;
    r.color = c;
    rings.push_back(r);
}

void UI_powerLayer::update(float dt) {
    for (auto& p : particles) {
        p.life -= dt;
        p.pos += p.vel * dt;
        if (p.kind == 1) p.vel.y -= 30.0f * dt;     // flames rise faster
        if (p.kind == 2 || p.kind == 3) p.vel.y += 420.0f * dt; // hail and water fall
    }
    particles.erase(std::remove_if(particles.begin(), particles.end(), [](const Particle& p) { return p.life <= 0.0f; }), particles.end());
    for (auto& r : rings) r.life -= dt;
    rings.erase(std::remove_if(rings.begin(), rings.end(), [](const Ring& r) { return r.life <= 0.0f; }), rings.end());
    shake = std::max(0.0f, shake - dt);
}

void UI_powerLayer::addFx(const PowerFx& fx, const GameEngine& e) {
    auto rnd = [](float lo, float hi) { return lo + (hi - lo) * static_cast<float>(std::rand() % 1000) / 1000.0f; };
    switch (fx.kind) {
        case PowerFxKind::HAZARD_HIT: {
            if (fx.hazard == HazardKind::HAIL) {
                for (int i = 0; i < 140; ++i) {
                    sf::Vector2f p(fx.pos.x + rnd(-fx.radius, fx.radius), fx.pos.y - fx.radius * rnd(0.3f, 0.9f));
                    spawn(p, { rnd(-30.0f, -10.0f), rnd(120.0f, 260.0f) }, rnd(0.7f, 1.3f), rnd(1.6f, 2.8f), sf::Color(235, 245, 255), 2);
                }
            } else if (fx.hazard == HazardKind::FLOOD) {
                spawnRing(fx.pos, fx.radius * 1.4f, 1.6f, sf::Color(70, 150, 240));
                for (int i = 0; i < 90; ++i) {
                    sf::Vector2f p(fx.pos.x + rnd(-40.0f, 40.0f), fx.pos.y + rnd(-fx.radius, fx.radius));
                    spawn(p, { rnd(-60.0f, 60.0f), rnd(-160.0f, -40.0f) }, rnd(0.6f, 1.1f), rnd(1.8f, 3.2f), sf::Color(110, 185, 250), 3);
                }
            } else if (fx.hazard == HazardKind::WILDFIRE) {
                for (int i = 0; i < 160; ++i) {
                    sf::Vector2f p(fx.pos.x + rnd(-fx.radius, fx.radius) * 0.8f, fx.pos.y + rnd(-fx.radius, fx.radius) * 0.6f);
                    sf::Color c = (i % 3 == 0) ? sf::Color(255, 230, 120) : ((i % 3 == 1) ? sf::Color(255, 140, 40) : sf::Color(220, 60, 30));
                    spawn(p, { rnd(-12.0f, 12.0f), rnd(-50.0f, -15.0f) }, rnd(0.8f, 2.2f), rnd(2.5f, 5.5f), c, 1);
                }
                for (int i = 0; i < 40; ++i) {
                    sf::Vector2f p(fx.pos.x + rnd(-fx.radius, fx.radius) * 0.7f, fx.pos.y + rnd(-20.0f, 20.0f));
                    spawn(p, { rnd(-8.0f, 8.0f), rnd(-35.0f, -15.0f) }, rnd(1.5f, 3.0f), rnd(5.0f, 9.0f), withAlpha(PW_SMOKE, 160), 4);
                }
            } else if (fx.hazard == HazardKind::QUAKE) {
                shake = 0.9f;
                spawnRing(fx.pos, fx.radius * 1.6f, 1.2f, sf::Color(210, 170, 110));
                spawnRing(fx.pos, fx.radius * 1.0f, 0.9f, sf::Color(255, 220, 150));
                for (int i = 0; i < 70; ++i) {
                    float a = rnd(0.0f, 2.0f * PI), s = rnd(30.0f, 120.0f);
                    spawn(fx.pos, { std::cos(a) * s, std::sin(a) * s * 0.6f }, rnd(0.6f, 1.4f), rnd(2.5f, 5.0f), sf::Color(150, 120, 90, 200), 4);
                }
            }
            for (const auto& h : fx.hits) {
                spawnRing(h, 26.0f, 0.7f, PW_BAD);
                for (int i = 0; i < 10; ++i) {
                    float a = rnd(0.0f, 2.0f * PI), s = rnd(40.0f, 110.0f);
                    spawn(h, { std::cos(a) * s, std::sin(a) * s }, rnd(0.3f, 0.6f), rnd(1.5f, 2.5f), sf::Color(255, 200, 90), 5);
                }
            }
            break;
        }
        case PowerFxKind::REACTOR_SCRAM:
        case PowerFxKind::REACTOR_NO_FUEL:
            spawnRing(fx.pos, 70.0f, 1.0f, PW_BAD);
            spawnRing(fx.pos, 45.0f, 0.7f, PW_WARN);
            break;
        case PowerFxKind::REACTOR_FULL:
        case PowerFxKind::REACTOR_RESTART:
            spawnRing(fx.pos, 55.0f, 0.9f, PW_GOOD);
            break;
        case PowerFxKind::MEGA_STARTED:
        case PowerFxKind::MEGA_SETBACK:
            spawnRing(fx.pos, 60.0f, 0.9f, fx.kind == PowerFxKind::MEGA_SETBACK ? PW_BAD : PW_WARN);
            break;
        case PowerFxKind::MEGA_COMPLETE:
            for (int k = 0; k < 3; ++k) spawnRing(fx.pos, 70.0f + k * 40.0f, 1.0f + k * 0.4f, k == 1 ? PW_WARN : PW_GOOD);
            for (int i = 0; i < 120; ++i) {
                float a = rnd(0.0f, 2.0f * PI), s = rnd(60.0f, 220.0f);
                sf::Color c = (i % 2) ? sf::Color(255, 220, 90) : sf::Color(120, 240, 255);
                spawn(fx.pos, { std::cos(a) * s, std::sin(a) * s }, rnd(0.6f, 1.4f), rnd(2.0f, 3.5f), c, 5);
            }
            break;
        case PowerFxKind::HAZARD_WARNING:
        case PowerFxKind::MEGA_UNLOCKED:
            break;
    }
    (void)e;
}

void UI_powerLayer::drawEffects(sf::RenderWindow& w, float animTime) {
    (void)animTime;
    for (const auto& r : rings) {
        float t = 1.0f - r.life / r.maxLife;
        float rad = r.radius + (r.maxRadius - r.radius) * t;
        disc(w, r.pos, rad, sf::Color::Transparent, withAlpha(r.color, (1.0f - t) * 220.0f), 3.0f);
    }
    for (const auto& p : particles) {
        float a = p.life / p.maxLife;
        switch (p.kind) {
            case 1: disc(w, p.pos, p.size * (0.5f + 0.5f * a), withAlpha(p.color, 230.0f * a)); break;
            case 2: disc(w, p.pos, p.size, withAlpha(p.color, 255.0f * std::min(1.0f, a * 2.0f)), sf::Color(150, 170, 200, 200), 0.6f); break;
            case 3: disc(w, p.pos, p.size, withAlpha(p.color, 220.0f * a)); break;
            case 4: disc(w, p.pos, p.size * (1.6f - 0.6f * a), withAlpha(p.color, static_cast<float>(p.color.a) * a)); break;
            default: disc(w, p.pos, p.size, withAlpha(p.color, 255.0f * a)); break;
        }
    }
    if (shake > 0.0f) {
        // Quake: a brief dusty flash over the whole map
        fillRect(w, { 0.0f, 0.0f }, { VIRTUAL_WIDTH, VIRTUAL_HEIGHT }, withAlpha(sf::Color(150, 110, 70), 60.0f * shake));
    }
}

// -----------------------------------------------------------------------------
// Terrain (F-15)
// -----------------------------------------------------------------------------
void UI_powerLayer::drawTerrain(sf::RenderWindow& w, const sf::Font& font, bool fontLoaded, const GameEngine& e, float animTime) {
    const MapLayout& L = e.getMapLayout();

    // Impassable cells (holes in the layout): a rocky peak
    for (int player = 1; player <= 2; ++player) {
        for (int r = 0; r < L.plotRows; ++r) {
            for (int sc = 0; sc < L.plotCols; ++sc) {
                if (L.hasPlot(L.westCol(player, sc), r)) continue;
                sf::FloatRect cell = L.plotRect(player, sc, r);
                float cx = cell.position.x + cell.size.x * 0.5f, by = cell.position.y + cell.size.y - 6.0f;
                sf::ConvexShape m1(3), m2(3), cap(3);
                m1.setPoint(0, { cx - cell.size.x * 0.45f, by });
                m1.setPoint(1, { cx - cell.size.x * 0.08f, cell.position.y + 10.0f });
                m1.setPoint(2, { cx + cell.size.x * 0.30f, by });
                m1.setFillColor(sf::Color(104, 100, 108));
                m1.setOutlineThickness(1.0f);
                m1.setOutlineColor(sf::Color(40, 40, 46));
                m2.setPoint(0, { cx - cell.size.x * 0.05f, by });
                m2.setPoint(1, { cx + cell.size.x * 0.22f, cell.position.y + cell.size.y * 0.38f });
                m2.setPoint(2, { cx + cell.size.x * 0.46f, by });
                m2.setFillColor(sf::Color(84, 82, 90));
                m2.setOutlineThickness(1.0f);
                m2.setOutlineColor(sf::Color(40, 40, 46));
                float capH = cell.size.y * 0.22f;
                cap.setPoint(0, { cx - cell.size.x * 0.08f - capH * 0.45f, cell.position.y + 10.0f + capH });
                cap.setPoint(1, { cx - cell.size.x * 0.08f, cell.position.y + 10.0f });
                cap.setPoint(2, { cx - cell.size.x * 0.08f + capH * 0.55f, cell.position.y + 10.0f + capH });
                cap.setFillColor(sf::Color(240, 244, 250));
                w.draw(m1);
                w.draw(m2);
                w.draw(cap);
                if (fontLoaded) plateText(w, font, "ВРЪХ", 9, PW_MUTED, { cx, by - 12.0f });
            }
        }
    }

    // Ground decoration of every terrain plot in ONE vertex array (few draw calls), under the buildings
    sf::VertexArray deco(sf::PrimitiveType::Triangles);
    auto quadLine = [&](sf::Vector2f a, sf::Vector2f b, float th, sf::Color c) {
        sf::Vector2f d = b - a;
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len < 0.01f) return;
        sf::Vector2f n(-d.y / len * th * 0.5f, d.x / len * th * 0.5f);
        sf::Vector2f p[4] = { a + n, b + n, b - n, a - n };
        const int idx[6] = { 0, 1, 2, 0, 2, 3 };
        for (int i : idx) deco.append(sf::Vertex{ p[i], c });
    };
    auto triangle = [&](sf::Vector2f a, sf::Vector2f b, sf::Vector2f c, sf::Color col) {
        deco.append(sf::Vertex{ a, col });
        deco.append(sf::Vertex{ b, col });
        deco.append(sf::Vertex{ c, col });
    };
    for (const auto& plot : e.getLandPlots()) {
        TerrainType t = static_cast<TerrainType>(plot.terrain);
        const sf::FloatRect& r = plot.bounds;
        sf::Color acc = terrainAccentColor(t);
        if (t == TerrainType::RIVER) {
            for (int k = 0; k < 3; ++k) {
                float y = r.position.y + r.size.y * (0.30f + 0.25f * k);
                const int segs = 8;
                for (int s = 0; s < segs; ++s) {
                    float x0 = r.position.x + 6.0f + (r.size.x - 12.0f) * s / segs;
                    float x1 = r.position.x + 6.0f + (r.size.x - 12.0f) * (s + 1) / segs;
                    float y0 = y + std::sin(animTime * 1.6f + s * 0.9f + k) * 2.2f;
                    float y1 = y + std::sin(animTime * 1.6f + (s + 1) * 0.9f + k) * 2.2f;
                    quadLine({ x0, y0 }, { x1, y1 }, 1.6f, withAlpha(acc, 85.0f));
                }
            }
        } else if (t == TerrainType::VENT) {
            sf::Vector2f c(r.position.x + r.size.x * 0.5f, r.position.y + r.size.y * 0.62f);
            const sf::Color crack(255, 110, 40, 150);
            quadLine({ c.x - 16.0f, c.y + 4.0f }, { c.x - 4.0f, c.y - 2.0f }, 2.0f, crack);
            quadLine({ c.x - 4.0f, c.y - 2.0f }, { c.x + 8.0f, c.y + 3.0f }, 2.0f, crack);
            quadLine({ c.x + 8.0f, c.y + 3.0f }, { c.x + 17.0f, c.y - 1.0f }, 2.0f, crack);
        } else if (t == TerrainType::HILL) {
            for (int k = 0; k < 2; ++k) {
                float bx = r.position.x + r.size.x * (0.18f + 0.42f * k);
                float by = r.position.y + r.size.y * (0.88f - 0.12f * k);
                float hw = r.size.x * 0.22f;
                triangle({ bx - hw, by }, { bx, by - r.size.y * 0.30f }, { bx + hw, by }, withAlpha(sf::Color(150, 120, 80), 70.0f));
            }
        } else if (t == TerrainType::MEADOW) {
            for (int k = 0; k < 14; ++k) {
                float fx = r.position.x + 6.0f + hash01(plot.id, k) * (r.size.x - 12.0f);
                float fy = r.position.y + 16.0f + hash01(k, plot.id) * (r.size.y - 22.0f);
                sf::Color fc = (k % 3 == 0) ? sf::Color(255, 150, 200, 150) : ((k % 3 == 1) ? sf::Color(255, 235, 120, 150) : sf::Color(250, 250, 255, 140));
                quadLine({ fx - 1.6f, fy }, { fx + 1.6f, fy }, 3.2f, fc);
            }
        }
    }
    w.draw(deco);

    for (const auto& plot : e.getLandPlots()) {
        TerrainType t = static_cast<TerrainType>(plot.terrain);
        if (t == TerrainType::PLAIN) continue;
        const sf::FloatRect& r = plot.bounds;
        sf::Color acc = terrainAccentColor(t);
        if (t == TerrainType::VENT) {
            sf::Vector2f c(r.position.x + r.size.x * 0.5f, r.position.y + r.size.y * 0.62f);
            steamPuffs(w, { c.x - 6.0f, c.y - 2.0f }, 0.55f, animTime, 0.9f, plot.id);
            steamPuffs(w, { c.x + 10.0f, c.y + 1.0f }, 0.45f, animTime * 0.8f, 0.7f, plot.id + 7);
        }

        // Badge: top-left icon on land for sale, bottom-right on owned land (the owner tag sits top-left)
        if (!plot.isPurchased) {
            disc(w, { r.position.x + 12.0f, r.position.y + 12.0f }, 9.0f, sf::Color(10, 14, 22, 200), withAlpha(acc, 220.0f), 1.0f);
            drawTerrainIcon(w, t, { r.position.x + 12.0f, r.position.y + 12.0f }, 13.0f);
            if (fontLoaded) {
                std::string label = std::string(getTerrainNameBg(t)) + ": " + getTerrainEffectBg(t);
                sf::Text probe = makeText(font, label, 9, acc);
                bool shortPlot = (r.size.y < 90.0f); // crowded / valley: keep clear of the price above
                unsigned size = (shortPlot || probe.getLocalBounds().size.x > r.size.x - 8.0f) ? 8u : 9u;
                plateText(w, font, label, size, acc, { r.position.x + r.size.x * 0.5f, r.position.y + r.size.y - (shortPlot ? 13.0f : 15.0f) });
            }
        } else {
            sf::Vector2f c(r.position.x + r.size.x - 10.0f, r.position.y + r.size.y - 10.0f);
            disc(w, c, 7.5f, sf::Color(10, 14, 22, 190), withAlpha(acc, 220.0f), 1.0f);
            drawTerrainIcon(w, t, c, 11.0f);
        }
    }
}

// -----------------------------------------------------------------------------
// Buildings: geothermal, reactor, mega-projects, damage
// -----------------------------------------------------------------------------
void UI_powerLayer::drawBuildings(sf::RenderWindow& w, const sf::Font& font, bool fontLoaded, const GameEngine& e, float animTime) {
    for (const auto& b : e.getBuildings()) {
        sf::Color oc = owner(b.playerOwner);

        if (b.type == BuildingType::GEOTHERMAL) {
            fillRect(w, { b.position.x - 14.0f, b.position.y - 12.0f }, { 28.0f, 24.0f }, sf::Color(30, 26, 30, 200), oc, 1.2f);
            drawPowerBuildingIcon(w, b.type, { b.position.x, b.position.y }, 24.0f);
            if (!b.isBroken) steamPuffs(w, { b.position.x + 5.5f, b.position.y - 9.0f }, 0.8f, animTime, 0.6f, static_cast<int>(b.position.x));
        }

        if (GameEngine::isPlotWideBuilding(b.type)) {
            const LandPlot* plot = plotUnder(e, b);
            if (plot == nullptr) continue;
            sf::FloatRect r = plot->bounds;
            // Layout inside the plot: status band (top), structure (middle), progress bar (bottom)
            float iconSize = std::min(r.size.x * 0.70f, r.size.y - 38.0f);
            sf::Vector2f c(r.position.x + r.size.x * 0.5f, r.position.y + 20.0f + (r.size.y - 36.0f) * 0.5f);
            fillRect(w, { r.position.x + 4.0f, r.position.y + 4.0f }, { r.size.x - 8.0f, r.size.y - 8.0f }, sf::Color(26, 30, 38), oc, 1.5f);

            if (b.type == BuildingType::NUCLEAR) {
                hazardBorder(w, r, animTime);
                bool running = (b.scramTimer <= 0.0f && !b.needsFuel);
                drawPowerBuildingIcon(w, b.type, c, iconSize);
                float u = iconSize / 24.0f;
                if (running) steamPuffs(w, { c.x - 3.0f * u, c.y - 9.0f * u }, 0.3f + 0.7f * b.rampProgress, animTime, u * 0.35f, 3);
                // Status line
                std::string status;
                sf::Color sc = PW_GOOD;
                float frac = b.rampProgress;
                if (b.scramTimer > 0.0f) {
                    int secs = static_cast<int>(std::ceil(b.scramTimer));
                    char buf[48];
                    std::snprintf(buf, sizeof(buf), "SCRAM %d:%02d", secs / 60, secs % 60);
                    status = buf;
                    sc = (std::fmod(animTime, 0.8f) < 0.4f) ? PW_BAD : sf::Color(255, 160, 160);
                    frac = 1.0f - b.scramTimer / PowerBalance::NUCLEAR_SCRAM_COOLDOWN_SEC;
                } else if (b.needsFuel) {
                    status = "НЯМА ГОРИВО";
                    sc = PW_WARN;
                    frac = 0.0f;
                } else {
                    status = "АЕЦ " + std::to_string(static_cast<int>(std::lround(b.currentOutputMW))) + " MW";
                }
                progressBar(w, { r.position.x + 10.0f, r.position.y + r.size.y - 13.0f }, { r.size.x - 20.0f, 6.0f }, frac,
                            b.scramTimer > 0.0f ? sf::Color(200, 70, 70) : PW_GOOD);
                if (fontLoaded) plateText(w, font, status, 9, sc, { c.x, r.position.y + 6.0f });
                if (b.scramTimer > 0.0f || b.needsFuel) {
                    float pulse = 0.5f + 0.5f * std::sin(animTime * 7.0f);
                    disc(w, { c.x + 7.5f * u, c.y + 3.6f * u }, 1.8f * u + pulse * 2.0f, withAlpha(PW_BAD, 120.0f + 120.0f * pulse));
                }
                continue;
            }

            // Mega-projects
            float progress = e.getConstructionProgress(b);
            bool building = (b.constructionLeft > 0.0f);
            if (building) {
                // The structure rises from the foundation: icon, then the unbuilt part covered by scaffolding
                drawPowerBuildingIcon(w, b.type, c, iconSize);
                float top = c.y - iconSize * 0.5f;
                float coverH = iconSize * (1.0f - progress);
                fillRect(w, { c.x - iconSize * 0.5f, top }, { iconSize, coverH }, sf::Color(26, 30, 38, 235));
                for (float gx = c.x - iconSize * 0.5f; gx <= c.x + iconSize * 0.5f + 0.1f; gx += iconSize / 6.0f) {
                    segment(w, { gx, top }, { gx, top + iconSize }, 1.0f, withAlpha(PW_SCAFFOLD, 170.0f));
                }
                for (float gy = top; gy <= top + iconSize + 0.1f; gy += iconSize / 6.0f) {
                    segment(w, { c.x - iconSize * 0.5f, gy }, { c.x + iconSize * 0.5f, gy }, 1.0f, withAlpha(PW_SCAFFOLD, 170.0f));
                }
                crane(w, r, animTime, progress);
                progressBar(w, { r.position.x + 10.0f, r.position.y + r.size.y - 14.0f }, { r.size.x - 20.0f, 7.0f }, progress, PW_WARN);
                if (fontLoaded) {
                    plateText(w, font, "СТРОИ СЕ " + std::to_string(static_cast<int>(progress * 100.0f)) + "%", 9, PW_WARN,
                              { r.position.x + r.size.x * 0.5f, r.position.y + 6.0f });
                }
                continue;
            }

            // Finished mega-project with its own animation
            if (b.type == BuildingType::MEGA_FUSION) {
                float pulse = 0.5f + 0.5f * std::sin(animTime * 3.0f);
                disc(w, c, iconSize * 0.48f + pulse * 4.0f, withAlpha(sf::Color(200, 110, 255), 40.0f + 40.0f * pulse));
                drawPowerBuildingIcon(w, b.type, c, iconSize);
                float a = animTime * 2.4f;
                for (int k = 0; k < 3; ++k) {
                    float ang = a + k * 2.094f;
                    float rr = iconSize * 0.26f;
                    disc(w, { c.x + std::cos(ang) * rr, c.y + std::sin(ang) * rr }, 2.4f, sf::Color(255, 240, 255));
                }
            } else if (b.type == BuildingType::MEGA_SPACE_SOLAR) {
                float flick = 0.75f + 0.25f * std::sin(animTime * 9.0f);
                sf::ConvexShape beam(4);
                beam.setPoint(0, { c.x - 3.0f, r.position.y + 19.0f });
                beam.setPoint(1, { c.x + 3.0f, r.position.y + 19.0f });
                beam.setPoint(2, { c.x + iconSize * 0.28f, c.y + iconSize * 0.33f });
                beam.setPoint(3, { c.x - iconSize * 0.28f, c.y + iconSize * 0.33f });
                beam.setFillColor(withAlpha(sf::Color(255, 235, 140), 70.0f * flick));
                w.draw(beam);
                drawPowerBuildingIcon(w, b.type, c, iconSize);
            } else if (b.type == BuildingType::MEGA_PUMPED_HYDRO) {
                drawPowerBuildingIcon(w, b.type, c, iconSize);
                float u = iconSize / 24.0f;
                float frac = (b.maxCapacity > 0.0f) ? b.energyStored / b.maxCapacity : 0.0f;
                // Upper reservoir level = stored energy
                float h = 15.0f * u * std::max(0.0f, std::min(1.0f, frac));
                fillRect(w, { c.x - 11.0f * u, c.y + 8.0f * u - h }, { 8.5f * u, h }, sf::Color(120, 200, 255, 150));
                if (fontLoaded) {
                    plateText(w, font, std::to_string(static_cast<int>(b.energyStored)) + " MWh", 9, sf::Color(150, 210, 255),
                              { c.x, r.position.y + 6.0f });
                }
            }
            if (fontLoaded && b.type != BuildingType::MEGA_PUMPED_HYDRO) {
                plateText(w, font, std::to_string(static_cast<int>(std::lround(b.currentOutputMW))) + " MW", 9, PW_GOOD,
                          { c.x, r.position.y + 6.0f });
            }
            continue;
        }

        // Damage marks (F-34): smoke, kind-specific mark and a red repair badge
        if (b.isBroken) {
            sf::Vector2f p = b.position;
            disc(w, p, 15.0f, sf::Color(20, 10, 10, 120));
            switch (static_cast<HazardKind>(b.damageKind)) {
                case HazardKind::HAIL:
                    for (int k = 0; k < 6; ++k) disc(w, { p.x - 9.0f + hash01(k, 3) * 18.0f, p.y - 8.0f + hash01(3, k) * 16.0f }, 1.6f, sf::Color(235, 245, 255));
                    segment(w, { p.x - 8.0f, p.y - 6.0f }, { p.x + 2.0f, p.y + 2.0f }, 1.2f, sf::Color(230, 240, 255, 200));
                    segment(w, { p.x + 2.0f, p.y + 2.0f }, { p.x + 8.0f, p.y - 3.0f }, 1.2f, sf::Color(230, 240, 255, 200));
                    break;
                case HazardKind::FLOOD:
                    disc(w, { p.x, p.y + 8.0f }, 13.0f, sf::Color(60, 140, 230, 140));
                    break;
                case HazardKind::WILDFIRE: {
                    float fl = 0.5f + 0.5f * std::sin(animTime * 12.0f + p.x);
                    disc(w, p, 11.0f, sf::Color(30, 20, 16, 170));
                    disc(w, { p.x - 4.0f, p.y + 2.0f }, 3.0f + fl * 1.5f, sf::Color(255, 140, 40, 220));
                    disc(w, { p.x + 3.0f, p.y }, 2.5f + (1.0f - fl) * 1.5f, sf::Color(255, 220, 110, 220));
                    break;
                }
                case HazardKind::QUAKE:
                    segment(w, { p.x - 12.0f, p.y - 4.0f }, { p.x - 3.0f, p.y + 3.0f }, 2.0f, sf::Color(30, 20, 14, 230));
                    segment(w, { p.x - 3.0f, p.y + 3.0f }, { p.x + 4.0f, p.y - 3.0f }, 2.0f, sf::Color(30, 20, 14, 230));
                    segment(w, { p.x + 4.0f, p.y - 3.0f }, { p.x + 12.0f, p.y + 5.0f }, 2.0f, sf::Color(30, 20, 14, 230));
                    break;
                case HazardKind::NONE:
                    break;
            }
            for (int k = 0; k < 3; ++k) {
                float ph = std::fmod(animTime * 0.5f + k * 0.33f + hash01(static_cast<int>(p.x), k), 1.0f);
                disc(w, { p.x + std::sin(ph * 6.0f + k) * 3.0f, p.y - 8.0f - ph * 22.0f }, 3.0f + ph * 5.0f, withAlpha(PW_SMOKE, (1.0f - ph) * 170.0f));
            }
            sf::Vector2f badge(p.x + 11.0f, p.y - 11.0f);
            float pulse = 0.5f + 0.5f * std::sin(animTime * 5.0f);
            disc(w, badge, 6.5f + pulse, sf::Color(220, 40, 40), sf::Color::White, 1.0f);
            fillRect(w, { badge.x - 1.0f, badge.y - 4.5f }, { 2.0f, 5.5f }, sf::Color::White); // "!"
            fillRect(w, { badge.x - 1.0f, badge.y + 2.0f }, { 2.0f, 2.0f }, sf::Color::White);
        }
    }
}

// -----------------------------------------------------------------------------
// Placement hints
// -----------------------------------------------------------------------------
void UI_powerLayer::drawPlacementHint(sf::RenderWindow& w, const GameEngine& e, int player, BuildingType sel, sf::Vector2f cursor, float animTime) {
    float pulse = 0.5f + 0.5f * std::sin(animTime * 5.0f);
    auto outlinePlots = [&](TerrainType t) {
        for (const auto& p : e.getLandPlots()) {
            if (p.playerOwner != player || p.terrain != static_cast<int>(t)) continue;
            fillRect(w, p.bounds.position, p.bounds.size, sf::Color::Transparent, withAlpha(terrainAccentColor(t), 120.0f + 120.0f * pulse), 2.5f);
        }
    };
    if (sel == BuildingType::GEOTHERMAL) outlinePlots(TerrainType::VENT);
    if (sel == BuildingType::HYDRO_PLANT || sel == BuildingType::MEGA_PUMPED_HYDRO) outlinePlots(TerrainType::RIVER);
    if (GameEngine::isPlotWideBuilding(sel)) {
        const LandPlot* plot = e.findPlotAt(player, e.snapToBuildingGrid(player, cursor));
        if (plot != nullptr && plot->isPurchased) {
            std::string reason;
            bool ok = e.canPlaceBuilding(player, sel, cursor, reason);
            sf::Color c = ok ? sf::Color(0, 255, 180) : sf::Color(255, 70, 70);
            fillRect(w, plot->bounds.position, plot->bounds.size, withAlpha(c, 28.0f + 20.0f * pulse), withAlpha(c, 230.0f), 3.0f);
            drawPowerBuildingIcon(w, sel, { plot->bounds.position.x + plot->bounds.size.x * 0.5f, plot->bounds.position.y + plot->bounds.size.y * 0.5f },
                                  std::min(plot->bounds.size.x, plot->bounds.size.y) * 0.6f);
        }
    }
}

// -----------------------------------------------------------------------------
// Shared mega-project HUD (HX-10): both players see both projects
// -----------------------------------------------------------------------------
void UI_powerLayer::drawMegaHud(sf::RenderWindow& w, const sf::Font& font, bool fontLoaded, const GameEngine& e, float animTime,
                                bool p2IsBot) {
    const PlacedBuilding* m1 = e.getMegaProject(1);
    const PlacedBuilding* m2 = e.getMegaProject(2);
    int day = e.getCurrentDay();
    if (m1 == nullptr && m2 == nullptr && day < PowerBalance::MEGA_UNLOCK_DAY - 2) return;

    const float x = 632.0f, y = 600.0f, W = 336.0f, H = 104.0f;
    fillRect(w, { x, y }, { W, H }, PW_PANEL, PW_PANEL_BORDER, 1.5f);
    if (!fontLoaded) return;
    textAt(w, font, "МЕГАПРОЕКТИ", 13, PW_WARN, { x + W * 0.5f, y + 5.0f }, true);
    segment(w, { x + 10.0f, y + 25.0f }, { x + W - 10.0f, y + 25.0f }, 1.0f, PW_PANEL_BORDER);

    for (int player = 1; player <= 2; ++player) {
        const PlacedBuilding* m = (player == 1) ? m1 : m2;
        float ry = y + 31.0f + (player - 1) * 35.0f;
        sf::Color oc = owner(player);
        textAt(w, font, player == 1 ? "P1" : "P2", 12, oc, { x + 10.0f, ry + 8.0f }, false);
        if (m == nullptr) {
            std::string msg = (day < PowerBalance::MEGA_UNLOCK_DAY)
                ? "Отключват се от ден " + std::to_string(PowerBalance::MEGA_UNLOCK_DAY)
                : (player == 1) ? "Няма проект: [9] или страница 2"
                                : (p2IsBot ? "Няма проект" : "Няма проект: [PgDn] до страница 2");
            textAt(w, font, msg, 11, PW_MUTED, { x + 40.0f, ry + 9.0f }, false);
            continue;
        }
        drawPowerBuildingIcon(w, m->type, { x + 52.0f, ry + 15.0f }, 26.0f);
        std::string name = e.getBuildingCost(m->type).nameBg;
        textAt(w, font, name, 11, PW_TEXT, { x + 72.0f, ry + 1.0f }, false);
        float prog = e.getConstructionProgress(*m);
        if (m->constructionLeft > 0.0f) {
            float daysLeft = m->constructionLeft / Balance::SECONDS_PER_DAY;
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%d%% | %.1f дни", static_cast<int>(prog * 100.0f), daysLeft);
            sf::Text t = makeText(font, buf, 10, PW_WARN);
            float tw = t.getLocalBounds().size.x;
            t.setPosition({ std::round(x + W - 10.0f - tw), std::round(ry + 2.0f) });
            w.draw(t);
            progressBar(w, { x + 72.0f, ry + 18.0f }, { W - 82.0f, 9.0f }, prog, PW_WARN);
        } else {
            float pulse = 0.85f + 0.15f * std::sin(animTime * 3.0f);
            std::string done = (m->type == BuildingType::MEGA_PUMPED_HYDRO)
                ? "ГОТОВ | " + std::to_string(static_cast<int>(m->energyStored)) + " MWh"
                : "ГОТОВ | +" + std::to_string(static_cast<int>(std::lround(m->currentOutputMW))) + " MW";
            sf::Text t = makeText(font, done, 10, withAlpha(PW_GOOD, 255.0f * pulse));
            float tw = t.getLocalBounds().size.x;
            t.setPosition({ std::round(x + W - 10.0f - tw), std::round(ry + 2.0f) });
            w.draw(t);
            progressBar(w, { x + 72.0f, ry + 18.0f }, { W - 82.0f, 9.0f }, 1.0f, PW_GOOD);
        }
    }
}

// -----------------------------------------------------------------------------
// Repair prompt
// -----------------------------------------------------------------------------
void UI_powerLayer::drawRepairPrompt(sf::RenderWindow& w, const sf::Font& font, bool fontLoaded, const GameEngine& e, int player,
                                     sf::Vector2f cursor, const std::string& keyLabel) {
    if (!fontLoaded) return;
    const PlacedBuilding* target = nullptr;
    float best = 22.0f;
    for (const auto& b : e.getBuildings()) {
        if (b.playerOwner != player || !b.isBroken) continue;
        float d = std::hypot(b.position.x - cursor.x, b.position.y - cursor.y);
        if (d < best) { best = d; target = &b; }
    }
    if (target == nullptr) return;
    const PlayerEconomy& econ = e.getPlayerEconomy(player);
    bool canPay = econ.wood >= Balance::REPAIR_WOOD_COST && econ.iron >= Balance::REPAIR_IRON_COST;
    std::string s = keyLabel + " РЕМОНТ: " + std::to_string(Balance::REPAIR_WOOD_COST) + " Дър + " +
                    std::to_string(Balance::REPAIR_IRON_COST) + " Жел";
    // Keep the tag inside the player's own half (never over the city)
    const MapLayout& L = e.getMapLayout();
    float left = (player == 1) ? L.westStartX - 6.0f : L.eastStartX() - 6.0f;
    float right = left + L.blockWidth() + 12.0f;
    float halfW = makeText(font, s, 10, PW_GOOD).getLocalBounds().size.x * 0.5f + 4.0f;
    float cx = std::max(left + halfW, std::min(right - halfW, target->position.x));
    plateText(w, font, s, 10, canPay ? PW_GOOD : PW_BAD, { cx, target->position.y + 20.0f }, sf::Color(10, 14, 22, 230));
}
