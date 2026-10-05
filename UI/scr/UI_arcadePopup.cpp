#include "../includes/UI_arcadePopup.h"
#include "../includes/UI_types.h"
#include <iostream>
#include <sstream>
#include <vector>

ArcadePopup& ArcadePopup::get() {
    static ArcadePopup instance;
    return instance;
}

ArcadePopup::ArcadePopup() {
    init();
}

void ArcadePopup::init() {
    if (!fontLoaded) {
        if (font.openFromFile("assets/PressStart2P.ttf")) {
            fontLoaded = true;
            std::cout << "[ArcadePopup] PressStart2P font loaded successfully.\n";
        } else if (font.openFromFile("assets/font.ttf")) {
            fontLoaded = true;
            std::cout << "[ArcadePopup] Fallback font loaded.\n";
        } else {
            std::cerr << "[ArcadePopup] Failed to load font for arcade popup.\n";
        }
    }
}

void ArcadePopup::show(const std::string& message, float durationSeconds) {
    init();
    currentMessage = message;
    timer = durationSeconds;
    maxDuration = durationSeconds;
}

void ArcadePopup::update(float dt) {
    if (timer > 0.0f) {
        timer -= dt;
        if (timer < 0.0f) timer = 0.0f;
    }
}

void ArcadePopup::draw(sf::RenderWindow& window) {
    if (timer <= 0.0f || currentMessage.empty()) return;

    const float boxWidth = 340.0f;
    const float boxHeight = 84.0f;
    const float boxX = VIRTUAL_WIDTH - boxWidth - 35.0f;
    const float boxY = 24.0f;

    // Outer glow / shadow
    sf::RectangleShape shadow({ boxWidth + 4.0f, boxHeight + 4.0f });
    shadow.setPosition({ boxX - 2.0f, boxY - 2.0f });
    shadow.setFillColor(sf::Color(0, 0, 0, 160));
    window.draw(shadow);

    // Dark translucent background box
    sf::RectangleShape box({ boxWidth, boxHeight });
    box.setPosition({ boxX, boxY });
    box.setFillColor(sf::Color(12, 16, 24, 235));
    box.setOutlineThickness(2.8f);
    box.setOutlineColor(sf::Color::White);
    window.draw(box);

    if (!fontLoaded) return;

    // Split message by newline
    std::vector<std::string> lines;
    std::stringstream ss(currentMessage);
    std::string line;
    while (std::getline(ss, line)) {
        lines.push_back(line);
    }

    if (lines.empty()) return;

    const unsigned int charSize = 13;
    const float lineHeight = 26.0f;
    float totalTextHeight = lines.size() * lineHeight;
    float startY = boxY + (boxHeight - totalTextHeight) / 2.0f;

    for (size_t i = 0; i < lines.size(); ++i) {
        sf::Text t(font, toUtf8(lines[i]), charSize);
        t.setFillColor(sf::Color::White);
        t.setStyle(sf::Text::Bold);
        sf::FloatRect b = t.getLocalBounds();
        float textX = boxX + (boxWidth - b.size.x) / 2.0f - b.position.x;
        float textY = startY + i * lineHeight - b.position.y;
        t.setPosition({ textX, textY });
        window.draw(t);
    }
}
