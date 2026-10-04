// =============================================================================
// [b-effects] UI_fx core: event handling, audio hooks and the DS-08 juice pack
// (build pop-in, dust, flying resources, HUD bump, screen shake, influence tween).
// =============================================================================
#include "../includes/UI_fx.h"
#include "../includes/UI_audio.h"
#include "../includes/UI_city.h"
#include "../includes/UI_ease.h"
#include "../includes/UI_resourceNodes.h"
#include "../includes/UI_types.h"

#include <algorithm>
#include <cmath>

using AudioSynth::Sfx;

namespace {

// --- Colours (integrator: map to UI_theme tokens) ---
const sf::Color FX_P1(0, 229, 255);
const sf::Color FX_P2(255, 120, 200);
const sf::Color FX_DUST(190, 170, 135);
const sf::Color FX_DEBRIS(255, 140, 60);
const sf::Color FX_GOLD(255, 215, 0);
const sf::Color FX_TRAIL(255, 246, 180);
const sf::Color FX_ICON_OUTLINE(10, 14, 22);

// --- Timing ---
const float POP_DURATION = 0.45f;
const float FLY_DURATION = 0.72f;
const float FLY_STAGGER = 0.06f;
const float BUMP_DURATION = 0.55f;
const float STRIKE_MEMORY = 0.6f;     // a building removed this soon after a nearby strike was destroyed by it
const float SHARE_SMOOTH_TIME = 0.55f;
const float TRAIL_HOLD = 0.6f;

sf::Color withAlpha(sf::Color c, float a) {
    c.a = static_cast<std::uint8_t>(std::max(0.0f, std::min(255.0f, a)));
    return c;
}

sf::Color resourceColor(ResourceType r) {
    switch (r) {
        case ResourceType::WOOD: return sf::Color(75, 210, 110);
        case ResourceType::IRON: return sf::Color(170, 195, 220);
        case ResourceType::COPPER: return sf::Color(230, 140, 70);
        case ResourceType::COAL: return sf::Color(130, 140, 155);
        case ResourceType::SILICON: return sf::Color(0, 220, 255);
        case ResourceType::SILVER: return sf::Color(225, 235, 245);
        case ResourceType::GOLD: return sf::Color(255, 215, 0);
        case ResourceType::MONEY: return sf::Color(70, 220, 130);
        default: return sf::Color::White;
    }
}

Sfx mineSound(ResourceType r) {
    switch (r) {
        case ResourceType::WOOD: return Sfx::MineWood;
        case ResourceType::IRON: return Sfx::MineIron;
        case ResourceType::COPPER: return Sfx::MineCopper;
        case ResourceType::COAL: return Sfx::MineCoal;
        case ResourceType::SILICON: return Sfx::MineSilicon;
        case ResourceType::SILVER: return Sfx::MineSilver;
        case ResourceType::GOLD: return Sfx::MineGold;
        default: return Sfx::MineIron;
    }
}

// Centre of the resource icon in the corner HUD (mirrors UI_resourceHUD::drawQuarterCircle layout)
sf::Vector2f hudSlot(int player, ResourceType r) {
    const float rows[4] = { VIRTUAL_HEIGHT - 215.0f, VIRTUAL_HEIGHT - 185.0f, VIRTUAL_HEIGHT - 155.0f, VIRTUAL_HEIGHT - 125.0f };
    float col1 = (player == 1) ? 18.0f : VIRTUAL_WIDTH - 190.0f;
    float col2 = (player == 1) ? 96.0f : VIRTUAL_WIDTH - 105.0f;
    float x = col1, y = rows[0];
    switch (r) {
        case ResourceType::WOOD: x = col1; y = rows[0]; break;
        case ResourceType::IRON: x = col1; y = rows[1]; break;
        case ResourceType::COPPER: x = col1; y = rows[2]; break;
        case ResourceType::COAL: x = col1; y = rows[3]; break;
        case ResourceType::SILICON: x = col2; y = rows[0]; break;
        case ResourceType::SILVER: x = col2; y = rows[1]; break;
        case ResourceType::GOLD: x = col2; y = rows[2]; break;
        default: x = col2; y = rows[3]; break;
    }
    return { x + 8.0f, y + 6.0f };
}

sf::Vector2f bezier(sf::Vector2f a, sf::Vector2f c, sf::Vector2f b, float t) {
    float u = 1.0f - t;
    return a * (u * u) + c * (2.0f * u * t) + b * (t * t);
}

// Small resource glyph centred at p (radius ~6 * s)
void drawResourceGlyph(sf::RenderTarget& target, ResourceType r, sf::Vector2f p, float s, float alpha) {
    sf::Color c = withAlpha(resourceColor(r), alpha);
    sf::Color outline = withAlpha(FX_ICON_OUTLINE, alpha * 0.8f);
    switch (r) {
        case ResourceType::WOOD: {
            sf::RectangleShape log({ 13.0f * s, 6.0f * s });
            log.setOrigin({ 6.5f * s, 3.0f * s });
            log.setPosition(p);
            log.setRotation(sf::degrees(-20.0f));
            log.setFillColor(withAlpha(sf::Color(150, 98, 55), alpha));
            log.setOutlineThickness(1.0f);
            log.setOutlineColor(outline);
            target.draw(log);
            sf::CircleShape leaf(3.0f * s, 3);
            leaf.setOrigin({ 3.0f * s, 3.0f * s });
            leaf.setPosition(p + sf::Vector2f(4.0f * s, -4.0f * s));
            leaf.setFillColor(c);
            target.draw(leaf);
            break;
        }
        case ResourceType::SILICON: {
            sf::CircleShape gem(6.5f * s, 4);
            gem.setOrigin({ 6.5f * s, 6.5f * s });
            gem.setPosition(p);
            gem.setFillColor(c);
            gem.setOutlineThickness(1.0f);
            gem.setOutlineColor(outline);
            target.draw(gem);
            break;
        }
        case ResourceType::GOLD: case ResourceType::COPPER: {
            sf::CircleShape coin(5.5f * s);
            coin.setOrigin({ 5.5f * s, 5.5f * s });
            coin.setPosition(p);
            coin.setFillColor(r == ResourceType::GOLD ? c : withAlpha(sf::Color(0, 0, 0), 0.0f));
            coin.setOutlineThickness(r == ResourceType::GOLD ? 1.0f : 2.2f * s);
            coin.setOutlineColor(r == ResourceType::GOLD ? outline : c);
            target.draw(coin);
            break;
        }
        case ResourceType::COAL: {
            sf::CircleShape lump(6.0f * s, 5);
            lump.setOrigin({ 6.0f * s, 6.0f * s });
            lump.setPosition(p);
            lump.setFillColor(c);
            lump.setOutlineThickness(1.0f);
            lump.setOutlineColor(outline);
            target.draw(lump);
            break;
        }
        default: { // iron, silver: ingots
            sf::RectangleShape bar({ 12.0f * s, 7.0f * s });
            bar.setOrigin({ 6.0f * s, 3.5f * s });
            bar.setPosition(p);
            bar.setFillColor(c);
            bar.setOutlineThickness(1.0f);
            bar.setOutlineColor(outline);
            target.draw(bar);
            break;
        }
    }
}

} // namespace

UI_fx::UI_fx() {
    initTraffic();
}

UI_fx::~UI_fx() = default;

float UI_fx::frand() {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return static_cast<float>(rng & 0xFFFFFFu) / 16777215.0f; // own RNG: never disturbs the match seed
}

void UI_fx::reset(const GameEngine& engine) {
    tracker.reset(engine);
    popIns.clear();
    rings.clear();
    puffs.clear();
    flyIcons.clear();
    bumps.clear();
    rectFlashes.clear();
    recentStrikes.clear();
    shakeTime = shakeDur = shakeAmp = 0.0f;
    shareReal = shareShown = shareTrail = engine.getCityState().p1CityShare;
    shareVel = 0.0f;
    trailHold = 0.0f;
    lightningLight = 0.0f;
    pulsePhase.clear();
    // Seasons snap to the new match instead of fading from the last one
    for (int s = 0; s < 4; ++s) seasonW[s] = (static_cast<int>(engine.getSeason()) == s) ? 1.0f : 0.0f;
}

void UI_fx::addShake(float amp, float dur) {
    if (!shakeEnabled) return;
    float remaining = (shakeDur > 0.0f) ? shakeAmp * std::max(0.0f, 1.0f - shakeTime / shakeDur) : 0.0f;
    if (amp >= remaining) {
        shakeAmp = amp;
        shakeDur = dur;
        shakeTime = 0.0f;
    }
}

void UI_fx::spawnDust(sf::Vector2f pos, sf::Color color, int count, float speed) {
    for (int i = 0; i < count; ++i) {
        float a = frand() * 6.2831853f;
        float v = speed * (0.4f + 0.6f * frand());
        Puff p;
        p.pos = pos + sf::Vector2f(std::cos(a) * 4.0f, std::sin(a) * 2.0f);
        p.vel = { std::cos(a) * v, std::sin(a) * v * 0.55f - 10.0f };
        p.age = 0.0f;
        p.life = 0.45f + 0.35f * frand();
        p.size = 2.0f + 2.5f * frand();
        p.color = color;
        puffs.push_back(p);
    }
}

void UI_fx::onLightning(sf::Vector2f pos, bool hitBuilding) {
    UI_audio::get().playAtX(Sfx::Thunder, pos.x, hitBuilding ? 1.0f : 0.7f, 0.92f + 0.16f * frand());
    lightningLight = 1.0f;
    recentStrikes.push_back({ pos, 0.0f });
    if (hitBuilding) {
        addShake(6.0f, 0.45f);
    } else {
        addShake(2.0f, 0.25f);
    }
}

void UI_fx::onPlayerError(int player) {
    if (singlePlayerMode && player == 2) return; // the bot's failed attempts stay silent
    UI_audio::get().play(Sfx::BuildDenied, player);
}

void UI_fx::handleEvents(const UI_resourceNodes& nodes) {
    UI_audio& audio = UI_audio::get();
    for (const FxEvent& e : events) {
        // In single player the bot (P2) is heard a little quieter than the human player
        const float vol = (singlePlayerMode && e.player == 2) ? 0.6f : 1.0f;
        switch (e.type) {
            case FxEvent::Type::Mined: {
                audio.play(mineSound(e.resource), e.player, vol);
                const ResourceStation* st = nodes.getStation(e.player, e.resource);
                if (!st) break;
                sf::Vector2f from = st->bounds.position + sf::Vector2f(st->bounds.size.x * 0.5f, st->bounds.size.y * 0.35f);
                sf::Vector2f to = hudSlot(e.player, e.resource);
                int count = std::max(3, std::min(7, e.amount / 3 + 2));
                for (int i = 0; i < count; ++i) {
                    FlyIcon f;
                    f.from = from + sf::Vector2f((frand() - 0.5f) * 26.0f, (frand() - 0.5f) * 14.0f);
                    sf::Vector2f mid = (f.from + to) * 0.5f;
                    f.ctrl = mid + sf::Vector2f((frand() - 0.5f) * 90.0f, -110.0f - 50.0f * frand());
                    f.to = to;
                    f.delay = static_cast<float>(i) * FLY_STAGGER;
                    f.age = 0.0f;
                    f.dur = FLY_DURATION * (0.9f + 0.2f * frand());
                    f.res = e.resource;
                    f.player = e.player;
                    f.last = (i == count - 1);
                    f.amount = e.amount;
                    flyIcons.push_back(f);
                }
                break;
            }
            case FxEvent::Type::Built: {
                audio.play(Sfx::BuildOk, e.player, vol);
                popIns.push_back({ e.pos, e.player, 0.0f });
                rings.push_back({ e.pos + sf::Vector2f(0.0f, 6.0f), 0.0f, 0.5f, 30.0f, FX_DUST });
                spawnDust(e.pos + sf::Vector2f(0.0f, 8.0f), FX_DUST, 10, 55.0f);
                break;
            }
            case FxEvent::Type::Removed: {
                bool struck = false;
                for (const auto& s : recentStrikes) {
                    float dx = s.first.x - e.pos.x, dy = s.first.y - e.pos.y;
                    if (s.second < STRIKE_MEMORY && dx * dx + dy * dy < 40.0f * 40.0f) struck = true;
                }
                if (struck) {
                    audio.play(Sfx::Destroyed, e.player, vol);
                    rings.push_back({ e.pos, 0.0f, 0.6f, 46.0f, FX_DEBRIS });
                    spawnDust(e.pos, FX_DEBRIS, 14, 110.0f);
                    spawnDust(e.pos, sf::Color(90, 90, 100), 12, 70.0f);
                } else {
                    audio.play(Sfx::Demolish, e.player, vol);
                    rings.push_back({ e.pos + sf::Vector2f(0.0f, 6.0f), 0.0f, 0.45f, 26.0f, FX_DUST });
                    spawnDust(e.pos, FX_DUST, 12, 70.0f);
                }
                break;
            }
            case FxEvent::Type::LandBought: {
                audio.play(Sfx::LandBuy, e.player, vol);
                rectFlashes.push_back({ e.area, 0.0f, 0.75f, FX_GOLD });
                for (int k = 0; k < 4; ++k) {
                    sf::Vector2f corner(e.area.position.x + ((k & 1) ? e.area.size.x : 0.0f),
                                        e.area.position.y + ((k & 2) ? e.area.size.y : 0.0f));
                    spawnDust(corner, FX_GOLD, 5, 60.0f);
                }
                break;
            }
            case FxEvent::Type::MineUpgraded: {
                audio.play(Sfx::Upgrade, e.player, vol);
                if (const ResourceStation* st = nodes.getStation(e.player, e.resource)) {
                    rectFlashes.push_back({ st->bounds, 0.0f, 0.7f, FX_GOLD });
                    spawnDust(st->bounds.position + st->bounds.size * 0.5f, FX_GOLD, 12, 80.0f);
                }
                break;
            }
            case FxEvent::Type::Settlement: {
                if (e.player == 0) break;
                // The day's verdict: the city outline flashes in the winner's colour with two shock rings
                {
                    const sf::FloatRect cityRect = UI_city().getCityBounds();
                    sf::Color c = (e.player == 1) ? FX_P1 : FX_P2;
                    sf::Vector2f centre = cityRect.position + cityRect.size * 0.5f;
                    rectFlashes.push_back({ cityRect, 0.0f, 1.1f, c });
                    rings.push_back({ centre + sf::Vector2f(0.0f, cityRect.size.y * 0.5f), 0.0f, 0.9f, 260.0f, c });
                    rings.push_back({ centre + sf::Vector2f(0.0f, cityRect.size.y * 0.5f), -0.25f, 0.9f, 200.0f, c });
                }
                if (singlePlayerMode) {
                    audio.play(e.player == 1 ? Sfx::SettleWon : Sfx::SettleLost, 0);
                } else {
                    audio.play(Sfx::SettleWon, e.player);
                }
                break;
            }
            case FxEvent::Type::Victory:
                audio.play(Sfx::Victory, 0);
                break;
            case FxEvent::Type::DayBreak:
                audio.play(Sfx::Sunrise, 0);
                break;
            case FxEvent::Type::Nightfall:
                audio.play(Sfx::Nightfall, 0);
                break;
            case FxEvent::Type::SeasonChanged:
                break;
        }
    }
}

void UI_fx::update(float dt, const GameEngine& engine, const UI_resourceNodes& nodes, bool simulationRunning) {
    dt = std::max(0.0f, std::min(dt, 0.1f));
    time += dt;
    float wdt = simulationRunning ? dt : 0.0f;
    worldTime += wdt;

    // Cache the engine state the draw calls need
    buildings = engine.getBuildings();
    plots = engine.getLandPlots();
    weather[0] = engine.getPlayerWeather(1);
    weather[1] = engine.getPlayerWeather(2);
    season = engine.getSeason();
    hour = engine.getHour24();
    sunrise = engine.getSunriseHour();
    sunset = engine.getSunsetHour();
    daylight = engine.isDaylight();
    winner = engine.getCityState().winner;

    events.clear();
    tracker.poll(engine, events);
    handleEvents(nodes);

    // --- Juice timers ---
    for (auto& p : popIns) p.age += dt;
    popIns.erase(std::remove_if(popIns.begin(), popIns.end(), [](const PopIn& p) { return p.age >= POP_DURATION; }), popIns.end());
    for (auto& r : rings) r.age += dt;
    rings.erase(std::remove_if(rings.begin(), rings.end(), [](const Ring& r) { return r.age >= r.life; }), rings.end());
    for (auto& p : puffs) {
        p.age += dt;
        p.pos += p.vel * dt;
        p.vel *= std::exp(-3.5f * dt);
    }
    puffs.erase(std::remove_if(puffs.begin(), puffs.end(), [](const Puff& p) { return p.age >= p.life; }), puffs.end());
    for (auto& f : flyIcons) {
        f.age += dt;
        if (f.last && f.age >= f.delay + f.dur) bumps.push_back({ f.to, 0.0f, f.res, f.amount, f.player });
    }
    flyIcons.erase(std::remove_if(flyIcons.begin(), flyIcons.end(), [](const FlyIcon& f) { return f.age >= f.delay + f.dur; }), flyIcons.end());
    for (auto& b : bumps) b.age += dt;
    bumps.erase(std::remove_if(bumps.begin(), bumps.end(), [](const Bump& b) { return b.age >= BUMP_DURATION; }), bumps.end());
    for (auto& r : rectFlashes) r.age += dt;
    rectFlashes.erase(std::remove_if(rectFlashes.begin(), rectFlashes.end(), [](const RectFlash& r) { return r.age >= r.life; }), rectFlashes.end());
    for (auto& s : recentStrikes) s.second += dt;
    recentStrikes.erase(std::remove_if(recentStrikes.begin(), recentStrikes.end(),
                                       [](const std::pair<sf::Vector2f, float>& s) { return s.second > STRIKE_MEMORY; }),
                        recentStrikes.end());
    if (shakeDur > 0.0f) {
        shakeTime += dt;
        if (shakeTime >= shakeDur) shakeDur = shakeAmp = shakeTime = 0.0f;
    }
    lightningLight = std::max(0.0f, lightningLight - dt * 3.0f);

    // --- Influence tween: smooth needle plus a trailing segment that shows the change ---
    float real = engine.getCityState().p1CityShare;
    if (std::fabs(real - shareReal) > 0.0005f) {
        if (std::fabs(shareTrail - shareShown) < 0.002f) shareTrail = shareShown;
        trailHold = TRAIL_HOLD;
        shareReal = real;
    }
    shareShown = Ease::smoothDamp(shareShown, shareReal, shareVel, SHARE_SMOOTH_TIME, dt);
    if (trailHold > 0.0f) {
        trailHold -= dt;
    } else {
        shareTrail = Ease::approach(shareTrail, shareShown, 4.0f, dt);
    }

    // --- Other systems ---
    rebuildGrid();
    advancePulses(wdt);
    advanceTraffic(wdt);
    computeAmbient();

    float targetW[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    targetW[static_cast<int>(season)] = 1.0f;
    for (int s = 0; s < 4; ++s) seasonW[s] = Ease::approach(seasonW[s], targetW[s], 0.5f, dt); // ~4 s season fade
}

sf::View UI_fx::worldView(const sf::View& base) const {
    if (!shakeEnabled || shakeDur <= 0.0f) return base;
    float k = 1.0f - shakeTime / shakeDur;
    float amp = shakeAmp * k * k;
    sf::Vector2f off(std::sin(time * 61.0f) * std::cos(time * 23.0f) * amp, std::cos(time * 53.0f) * std::sin(time * 37.0f) * amp);
    sf::View v = base;
    float zoom = 1.0f + (2.0f * amp + 2.0f) / VIRTUAL_HEIGHT; // zoom in a hair so the edges never show
    v.setSize(base.getSize() / zoom);
    v.setCenter(base.getCenter() + off);
    return v;
}

void UI_fx::drawBuildings(sf::RenderWindow& window, UI_resourceNodes& nodes, const sf::Font& font, bool fontLoaded,
                          const std::vector<PlacedBuilding>& list) const {
    if (popIns.empty()) {
        nodes.drawPlacedBuildings(window, font, fontLoaded, list);
        return;
    }
    std::vector<PlacedBuilding> steady;
    std::vector<std::pair<PlacedBuilding, float>> popping;
    steady.reserve(list.size());
    for (const auto& b : list) {
        float age = -1.0f;
        for (const auto& p : popIns) {
            if (p.owner == b.playerOwner && std::fabs(p.pos.x - b.position.x) < 0.5f && std::fabs(p.pos.y - b.position.y) < 0.5f) {
                age = p.age;
                break;
            }
        }
        if (age < 0.0f) steady.push_back(b);
        else popping.push_back({ b, age });
    }
    nodes.drawPlacedBuildings(window, font, fontLoaded, steady);

    // Scale each new building about its base with a view transform (the building art itself is untouched)
    const sf::View base = window.getView();
    for (const auto& pb : popping) {
        float t = pb.second / POP_DURATION;
        float s = std::max(0.05f, Ease::easeOutBack(t, 2.4f));
        float wobble = 0.14f * std::sin(t * 6.2831853f) * (1.0f - t);
        float sx = std::max(0.05f, s * (1.0f + wobble));
        float sy = std::max(0.05f, s * (1.0f - wobble));
        sf::Vector2f pivot = pb.first.position + sf::Vector2f(0.0f, 9.0f);
        sf::Vector2f c = base.getCenter();
        sf::View v = base;
        v.setSize({ base.getSize().x / sx, base.getSize().y / sy });
        v.setCenter({ pivot.x + (c.x - pivot.x) / sx, pivot.y + (c.y - pivot.y) / sy });
        window.setView(v);
        std::vector<PlacedBuilding> one(1, pb.first);
        nodes.drawPlacedBuildings(window, font, fontLoaded, one);
    }
    window.setView(base);
}

void UI_fx::drawWorldFx(sf::RenderTarget& target) const {
    for (const auto& r : rectFlashes) {
        float t = r.age / r.life;
        float grow = 2.0f + 10.0f * Ease::easeOutCubic(t);
        sf::RectangleShape box(r.rect.size + sf::Vector2f(grow * 2.0f, grow * 2.0f));
        box.setPosition(r.rect.position - sf::Vector2f(grow, grow));
        box.setFillColor(withAlpha(r.color, 70.0f * (1.0f - t) * (1.0f - t)));
        box.setOutlineThickness(2.5f);
        box.setOutlineColor(withAlpha(r.color, 230.0f * (1.0f - t)));
        target.draw(box);
    }
    for (const auto& r : rings) {
        if (r.age < 0.0f) continue; // delayed ring
        float t = r.age / r.life;
        float rad = 4.0f + r.radius * Ease::easeOutCubic(t);
        sf::CircleShape ring(rad, 40);
        ring.setOrigin({ rad, rad });
        ring.setScale({ 1.0f, 0.55f }); // lies on the ground
        ring.setPosition(r.pos);
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineThickness(2.5f);
        ring.setOutlineColor(withAlpha(r.color, 210.0f * (1.0f - t)));
        target.draw(ring);
    }
    for (const auto& p : puffs) {
        float t = p.age / p.life;
        float rad = p.size * (1.0f + 1.2f * t);
        sf::CircleShape c(rad, 10);
        c.setOrigin({ rad, rad });
        c.setPosition(p.pos);
        c.setFillColor(withAlpha(p.color, 190.0f * (1.0f - t)));
        target.draw(c);
    }
}

void UI_fx::drawInfluenceTrail(sf::RenderTarget& target) const {
    const sf::FloatRect bar = UI_city::influenceBarRect();
    float a = std::min(shareTrail, shareShown), b = std::max(shareTrail, shareShown);
    if (b - a > 0.002f) {
        float alpha = (trailHold > 0.0f) ? 190.0f : 190.0f * std::min(1.0f, (b - a) * 25.0f);
        sf::RectangleShape seg({ bar.size.x * (b - a), bar.size.y });
        seg.setPosition({ bar.position.x + bar.size.x * a, bar.position.y });
        seg.setFillColor(withAlpha(FX_TRAIL, alpha));
        target.draw(seg);
    }
    // Needle glow while the bar is moving
    float speed = std::fabs(shareVel);
    if (speed > 0.005f) {
        float x = bar.position.x + bar.size.x * shareShown;
        float glowA = std::min(160.0f, speed * 1500.0f);
        sf::RectangleShape glow({ 9.0f, bar.size.y + 10.0f });
        glow.setOrigin({ 4.5f, 0.0f });
        glow.setPosition({ x, bar.position.y - 5.0f });
        glow.setFillColor(withAlpha(sf::Color::White, glowA * 0.45f));
        target.draw(glow);
    }
}

void UI_fx::drawFlyingResources(sf::RenderTarget& target, const sf::Font& font, bool fontLoaded) const {
    (void)font;
    (void)fontLoaded;
    for (const auto& f : flyIcons) {
        if (f.age < f.delay) continue;
        float t = Ease::clamp01((f.age - f.delay) / f.dur);
        float e = Ease::easeInOutSine(t);
        sf::Vector2f p = bezier(f.from, f.ctrl, f.to, e);
        // Faint trail
        for (int k = 1; k <= 3; ++k) {
            float tk = Ease::easeInOutSine(std::max(0.0f, t - 0.035f * static_cast<float>(k)));
            sf::Vector2f q = bezier(f.from, f.ctrl, f.to, tk);
            float r = 3.2f - 0.7f * static_cast<float>(k);
            sf::CircleShape dot(r, 8);
            dot.setOrigin({ r, r });
            dot.setPosition(q);
            dot.setFillColor(withAlpha(resourceColor(f.res), 90.0f - 25.0f * static_cast<float>(k)));
            target.draw(dot);
        }
        float s = 1.25f - 0.45f * t;
        float alpha = (t < 0.1f) ? 255.0f * (t / 0.1f) : 255.0f;
        sf::CircleShape glow(9.0f * s, 16);
        glow.setOrigin({ 9.0f * s, 9.0f * s });
        glow.setPosition(p);
        glow.setFillColor(withAlpha(resourceColor(f.res), 55.0f));
        target.draw(glow);
        drawResourceGlyph(target, f.res, p, s, alpha);
    }
    for (const auto& b : bumps) {
        float t = b.age / BUMP_DURATION;
        float r = 9.0f + 14.0f * Ease::easeOutCubic(t);
        sf::CircleShape ring(r, 28);
        ring.setOrigin({ r, r });
        ring.setPosition(b.pos);
        ring.setFillColor(withAlpha(resourceColor(b.res), 70.0f * (1.0f - t)));
        ring.setOutlineThickness(2.0f);
        ring.setOutlineColor(withAlpha(resourceColor(b.res), 230.0f * (1.0f - t)));
        target.draw(ring);
    }
}
