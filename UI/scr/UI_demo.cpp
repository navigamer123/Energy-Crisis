// =============================================================================
// ENERGY CRISIS - JUDGE DEMO MODE (HX-02), UI side                 [Team Demo]
// UI_demo is a friend of UI_map: it drives the map's engine, cursors and effects
// while UI_map::demoDriven keeps render() from running input, bot or simulation.
// =============================================================================
#include "../includes/UI_demo.h"
#include "../includes/UI_map.h"
#include "../includes/UI_types.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>

namespace {

const sf::Color kGold(255, 204, 0);
const sf::Color kP1(0, 229, 255);
const sf::Color kP2(255, 120, 200);
const sf::Color kPanel(8, 12, 22);

sf::Color playerColor(int player) {
    return (player == 1) ? kP1 : (player == 2 ? kP2 : kGold);
}

sf::Color withAlpha(sf::Color c, float a) {
    c.a = static_cast<std::uint8_t>(std::max(0.0f, std::min(1.0f, a)) * static_cast<float>(c.a));
    return c;
}

float smooth01(float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    return t * t * (3.0f - 2.0f * t);
}

// Text that shrinks until it fits maxWidth, centred on centreX with its top at topY
void drawFittedText(sf::RenderWindow& window, const sf::Font& font, const std::string& utf8,
                    unsigned int size, unsigned int minSize, float centreX, float topY, float maxWidth,
                    sf::Color fill, float outline, float alpha) {
    if (utf8.empty()) return;
    sf::Text text(font, toUtf8(utf8), size);
    while (text.getLocalBounds().size.x > maxWidth && size > minSize) {
        size -= 2;
        text.setCharacterSize(size);
    }
    text.setFillColor(withAlpha(fill, alpha));
    text.setOutlineColor(withAlpha(sf::Color(0, 0, 0, 230), alpha));
    text.setOutlineThickness(outline);
    sf::FloatRect b = text.getLocalBounds();
    text.setPosition({ std::round(centreX - b.size.x / 2.0f - b.position.x), std::round(topY - b.position.y) });
    window.draw(text);
}

std::string clockString(float seconds) {
    int s = std::max(0, static_cast<int>(seconds));
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d:%02d", s / 60, s % 60);
    return buf;
}

} // namespace

UI_demo::UI_demo() {
    for (auto& t : cursorTarget) t = sf::Vector2f(0.0f, 0.0f);
}

void UI_demo::start(UI_map& map) {
    // Full per-match UI reset first (overlays, cooldowns, popups, lightning, notices)
    map.restartMatch();

    // Nobody but the script plays: no bot, no tutorial, no dialogs, no pause
    map.bot.init(BotDifficulty::NONE);
    map.tutorial.skip();
    map.isPaused = false;
    map.showHelpOverlay = false;
    map.p1Modal.active = false;
    map.p2Modal.active = false;
    map.p1Popup.active = false;
    map.p2Popup.active = false;
    map.notices.clear();
    map.demoDriven = true;

    speed = 1.0f;
    if (const char* env = std::getenv("EC_DEMO_SPEED")) {
        float v = static_cast<float>(std::atof(env));
        if (v >= 0.25f && v <= 8.0f) speed = v;
    }
    shotDir.clear();
    if (const char* env = std::getenv("EC_DEMO_SHOTS")) shotDir = env;
    lastShotSerial = -1;
    finalShotTaken = false;

    director.setScript(Demo::defaultDemoScript());
    director.start(map.engine, Demo::DEMO_SEED);
    for (const auto& problem : Demo::validateScript(director.getScript())) {
        std::cerr << "[UI_demo] Script problem: " << problem << "\n";
    }

    cursorTarget[1] = map.p1Pos;
    cursorTarget[2] = map.p2Pos;
    active = true;
    exitRequested = false;
    statFrames = 0;
    statTime = 0.0f;
    realTime = 0.0f;
    shownSerial = -1;
    captionShownAt = 0.0f;
    frameClock.restart();

    for (const auto& cue : director.takeCues()) applyCue(map, cue);
    std::cout << "[UI_demo] Judge demo started (" << director.getScript().size() << " beats, "
              << static_cast<int>(director.getDuration()) << " s, speed x" << speed << ").\n";
}

void UI_demo::stop(UI_map& map) {
    director.stop();
    map.demoDriven = false;
    map.engine.setTimeScale(1.0f);
    map.notices.clear();
    active = false;
    exitRequested = false;
    std::cout << "[UI_demo] Judge demo stopped.\n";
}

void UI_demo::handleEvent(const sf::Event& event) {
    if (!active) return;
    // The key that started the demo (F9 / Enter) must not end it at once
    if (realTime < 0.6f) return;

    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        switch (key->code) {
            // Modifiers (Alt-Tab, Ctrl...) and fullscreen keys never end the show
            case sf::Keyboard::Key::LAlt:
            case sf::Keyboard::Key::RAlt:
            case sf::Keyboard::Key::LControl:
            case sf::Keyboard::Key::RControl:
            case sf::Keyboard::Key::LShift:
            case sf::Keyboard::Key::RShift:
            case sf::Keyboard::Key::LSystem:
            case sf::Keyboard::Key::RSystem:
            case sf::Keyboard::Key::F11:
            case sf::Keyboard::Key::Unknown:
                return;
            default:
                exitRequested = true;
                return;
        }
    }
    if (event.is<sf::Event::MouseButtonPressed>() || event.is<sf::Event::JoystickButtonPressed>()) {
        exitRequested = true;
    }
}

void UI_demo::update(UI_map& map) {
    float dt = frameClock.restart().asSeconds();
    if (dt > 0.5f) dt = 0.5f; // a window drag or hitch skips at most half a second
    if (!active) return;
    realTime += dt;

    director.advance(map.engine, dt * speed);
    for (const auto& cue : director.takeCues()) applyCue(map, cue);
    animateMap(map, dt);

    // Presenter console: progress and frame rate every 10 s (a slow machine plays the show slower)
    ++statFrames;
    statTime += dt;
    if (statTime >= 10.0f) {
        std::cout << "[UI_demo] " << clockString(director.getTime()) << " / " << clockString(director.getDuration())
                  << ", " << static_cast<int>(statFrames / statTime + 0.5f) << " FPS\n";
        statFrames = 0;
        statTime = 0.0f;
    }

    if (director.getCaptionSerial() != shownSerial) {
        shownSerial = director.getCaptionSerial();
        captionShownAt = realTime;
    }
}

void UI_demo::animateMap(UI_map& map, float dt) {
    // Cursors glide to where the script acts, so the audience can follow every move
    float k = std::min(1.0f, dt * 5.0f);
    map.p1Pos += (cursorTarget[1] - map.p1Pos) * k;
    map.p2Pos += (cursorTarget[2] - map.p2Pos) * k;

    // Same visual timers UI_map::updateControls runs in a normal match
    for (auto it = map.notices.begin(); it != map.notices.end();) {
        it->timer -= dt;
        it->pos.y -= 40.0f * dt;
        if (it->timer <= 0.0f) it = map.notices.erase(it);
        else ++it;
    }
    map.updateMiningParticles(dt);
    map.p1Pulse = std::max(0.0f, map.p1Pulse - dt * 2.2f);
    map.p2Pulse = std::max(0.0f, map.p2Pulse - dt * 2.2f);
    if (map.p1Popup.active) {
        map.p1Popup.timer -= dt;
        if (map.p1Popup.timer <= 0.0f) map.p1Popup.active = false;
    }
    if (map.p2Popup.active) {
        map.p2Popup.timer -= dt;
        if (map.p2Popup.timer <= 0.0f) map.p2Popup.active = false;
    }
}

void UI_demo::applyCue(UI_map& map, const Demo::DemoCue& cue) {
    const sf::Color color = playerColor(cue.player);
    const bool hasPlayer = (cue.player == 1 || cue.player == 2);
    auto pulse = [&map](int player) {
        if (player == 1) map.p1Pulse = 1.0f;
        if (player == 2) map.p2Pulse = 1.0f;
    };

    if (cue.kind == "build") {
        if (hasPlayer) cursorTarget[cue.player] = cue.pos;
        pulse(cue.player);
        map.spawnMiningParticles(cue.pos, color, 16);
        map.spawnNotice(cue.text, cue.pos + sf::Vector2f(0.0f, -26.0f), color);
    } else if (cue.kind == "plot") {
        if (hasPlayer) cursorTarget[cue.player] = cue.pos;
        pulse(cue.player);
        map.spawnNotice("ЗАКУПЕН ПАРЦЕЛ!", cue.pos, sf::Color(255, 215, 0));
    } else if (cue.kind == "mine") {
        const ResourceStation* station = map.nodes.getStation(cue.player, static_cast<ResourceType>(cue.value));
        if (station) {
            sf::Vector2f centre = station->bounds.getCenter();
            if (hasPlayer) cursorTarget[cue.player] = centre;
            pulse(cue.player);
            map.spawnMiningParticles(centre, station->themeColor, 18);
            map.spawnNotice(cue.text, centre + sf::Vector2f(0.0f, -30.0f), station->themeColor);
        }
    } else if (cue.kind == "lightning") {
        map.triggerLightningStrike(cue.pos, cue.value != 0);
        if (cue.value != 0 && hasPlayer) {
            map.triggerPlayerPopup(cue.player, "МЪЛНИЯ!", "Унищожено съоръжение!",
                                   "Мълния унищожи " + cue.text + "!\nКлетката се освободи за нов строеж.",
                                   "", sf::Color(255, 230, 80));
            map.spawnNotice("СЪОРЪЖЕНИЕТО Е УНИЩОЖЕНО!", cue.pos + sf::Vector2f(0.0f, -32.0f), sf::Color(255, 80, 80));
        }
    } else if (cue.kind == "weather") {
        sf::Vector2f sector = (cue.player == 2) ? sf::Vector2f(1170.0f, 470.0f) : sf::Vector2f(430.0f, 470.0f);
        map.spawnNotice(cue.text, sector, color);
    } else if (cue.kind == "jump") {
        map.spawnNotice(cue.text, sf::Vector2f(800.0f, 430.0f), sf::Color(255, 255, 255));
    } else if (cue.kind == "settle" || cue.kind == "winner") {
        // The caption shows the result; the victory screen comes from the engine itself
    } else if (!cue.text.empty()) {
        // "notice" and any custom cue of a later feature
        sf::Vector2f pos = cue.hasPos ? cue.pos : sf::Vector2f(800.0f, 450.0f);
        map.spawnNotice(cue.text, pos, color);
    }
}

void UI_demo::drawOverlay(sf::RenderWindow& window, const UI_map& map) {
    if (!active || !map.resourcesLoaded) {
        captureShotIfDue(window);
        return;
    }
    const sf::Font& font = map.font;
    float appear = smooth01((realTime - captionShownAt) / 0.35f);
    bool finished = director.isFinished();
    bool showCaption = director.hasCaption() || finished;

    if (showCaption && director.isTitleCard()) {
        drawTitleCard(window, font, appear);
    } else if (showCaption) {
        drawCaptionPanel(window, font, appear);
    } else {
        drawProgress(window, font, 864.0f);
    }

    if (finished) {
        float blink = 0.55f + 0.45f * std::sin(realTime * 3.0f);
        drawFittedText(window, font, "НАТИСНЕТЕ КЛАВИШ ЗА ВРЪЩАНЕ В МЕНЮТО", 24, 14, 800.0f, 640.0f, 1100.0f,
                       sf::Color(255, 240, 150), 2.0f, blink);
    }
    captureShotIfDue(window);
}

void UI_demo::drawCaptionPanel(sf::RenderWindow& window, const sf::Font& font, float appear) {
    const float panelW = 1140.0f;
    const float panelH = 122.0f;
    const float panelX = (VIRTUAL_WIDTH - panelW) / 2.0f;
    const float panelY = 746.0f + (1.0f - appear) * 24.0f;

    sf::RectangleShape panel({ panelW, panelH });
    panel.setPosition({ panelX, panelY });
    panel.setFillColor(withAlpha(sf::Color(kPanel.r, kPanel.g, kPanel.b, 228), appear));
    panel.setOutlineThickness(2.5f);
    panel.setOutlineColor(withAlpha(kGold, appear));
    window.draw(panel);

    // Accent stripe on the left
    sf::RectangleShape stripe({ 8.0f, panelH });
    stripe.setPosition({ panelX, panelY });
    stripe.setFillColor(withAlpha(kGold, appear));
    window.draw(stripe);

    const float textW = panelW - 60.0f;
    const std::string& caption = director.getCaption();
    const std::string& subtitle = director.getSubtitle();
    if (subtitle.empty()) {
        drawFittedText(window, font, caption, 50, 26, VIRTUAL_WIDTH / 2.0f, panelY + 30.0f, textW, kGold, 3.0f, appear);
    } else {
        drawFittedText(window, font, caption, 46, 26, VIRTUAL_WIDTH / 2.0f, panelY + 12.0f, textW, kGold, 3.0f, appear);
        drawFittedText(window, font, subtitle, 26, 14, VIRTUAL_WIDTH / 2.0f, panelY + 70.0f, textW,
                       sf::Color(235, 242, 255), 2.0f, appear);
    }
    drawProgress(window, font, panelY + panelH - 5.0f);
}

void UI_demo::drawTitleCard(sf::RenderWindow& window, const sf::Font& font, float appear) {
    sf::RectangleShape dim({ VIRTUAL_WIDTH, VIRTUAL_HEIGHT });
    dim.setFillColor(sf::Color(4, 8, 16, static_cast<std::uint8_t>(190.0f * appear)));
    window.draw(dim);

    float y = 300.0f + (1.0f - appear) * 30.0f;
    drawFittedText(window, font, director.getCaption(), 92, 40, VIRTUAL_WIDTH / 2.0f, y, 1400.0f, kGold, 4.0f, appear);

    sf::RectangleShape line({ 640.0f * appear, 3.0f });
    line.setPosition({ (VIRTUAL_WIDTH - 640.0f * appear) / 2.0f, y + 128.0f });
    line.setFillColor(withAlpha(kGold, appear));
    window.draw(line);

    drawFittedText(window, font, director.getSubtitle(), 32, 16, VIRTUAL_WIDTH / 2.0f, y + 150.0f, 1400.0f,
                   sf::Color(225, 235, 250), 2.0f, appear);
    drawProgress(window, font, 864.0f);
}

void UI_demo::drawProgress(sf::RenderWindow& window, const sf::Font& font, float y) {
    const float barW = 1140.0f;
    const float barX = (VIRTUAL_WIDTH - barW) / 2.0f;
    float duration = std::max(1.0f, director.getDuration());
    float t = std::min(director.getTime(), duration);

    sf::RectangleShape back({ barW, 4.0f });
    back.setPosition({ barX, y });
    back.setFillColor(sf::Color(40, 50, 70, 200));
    window.draw(back);

    sf::RectangleShape fill({ barW * (t / duration), 4.0f });
    fill.setPosition({ barX, y });
    fill.setFillColor(kGold);
    window.draw(fill);

    // Small presenter line just above the bar
    sf::Text left(font, toUtf8("ДЕМО РЕЖИМ  •  Esc / клавиш: изход"), 13);
    left.setFillColor(sf::Color(170, 185, 210));
    left.setOutlineColor(sf::Color(0, 0, 0, 200));
    left.setOutlineThickness(1.0f);
    left.setPosition({ barX + 14.0f, y - 19.0f });
    window.draw(left);

    sf::Text right(font, toUtf8(clockString(t) + " / " + clockString(duration)), 13);
    right.setFillColor(sf::Color(170, 185, 210));
    right.setOutlineColor(sf::Color(0, 0, 0, 200));
    right.setOutlineThickness(1.0f);
    sf::FloatRect rb = right.getLocalBounds();
    right.setPosition({ barX + barW - rb.size.x - 14.0f, y - 19.0f });
    window.draw(right);
}

void UI_demo::drawMenuHint(sf::RenderWindow& window, const UI_map& map) const {
    if (!map.resourcesLoaded) return;
    const sf::Font& font = map.font;
    sf::Text hint(font, toUtf8("[F9]  ДЕМО ЗА ЖУРИТО (3 мин.)"), 18);
    hint.setFillColor(kGold);
    hint.setOutlineColor(sf::Color(0, 0, 0, 220));
    hint.setOutlineThickness(1.5f);
    sf::FloatRect b = hint.getLocalBounds();
    float x = (VIRTUAL_WIDTH - b.size.x) / 2.0f;
    float y = VIRTUAL_HEIGHT - 58.0f;

    sf::RectangleShape pill({ b.size.x + 36.0f, 34.0f });
    pill.setPosition({ x - 18.0f, y - 6.0f });
    pill.setFillColor(sf::Color(20, 26, 40, 220));
    pill.setOutlineThickness(1.5f);
    pill.setOutlineColor(sf::Color(255, 204, 0, 160));
    window.draw(pill);

    hint.setPosition({ x - b.position.x, y + (22.0f - b.size.y) / 2.0f - b.position.y });
    window.draw(hint);
}

void UI_demo::captureShotIfDue(sf::RenderWindow& window) {
    if (shotDir.empty() || !active) return;
    int serial = director.getCaptionSerial();
    bool captionShot = serial > 0 && serial != lastShotSerial && director.getCaptionAge() >= 1.2f;
    bool finalShot = director.isFinished() && !finalShotTaken && realTime - captionShownAt > 1.0f;
    if (!captionShot && !finalShot) return;
    if (captionShot) lastShotSerial = serial;
    if (finalShot) finalShotTaken = true;

    char name[64];
    std::snprintf(name, sizeof(name), "/demo_%02d_%03ds%s.png", serial, static_cast<int>(director.getTime()),
                  finalShot && !captionShot ? "_end" : "");
    try {
        sf::Texture shot(window.getSize());
        shot.update(window);
        if (!shot.copyToImage().saveToFile(shotDir + name)) {
            std::cerr << "[UI_demo] Could not save " << shotDir << name << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "[UI_demo] Screenshot failed: " << e.what() << "\n";
    }
}
