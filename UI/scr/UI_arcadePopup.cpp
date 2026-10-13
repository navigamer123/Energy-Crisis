#include "../includes/UI_arcadePopup.h"
#include "../includes/UI_types.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>

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

    if (!fontLoaded) return;

    // Split message by newline, with word wrapping if any individual line exceeds max width
    const unsigned int charSize = 13;
    const float lineHeight = 26.0f;
    const float maxAllowedContentWidth = 520.0f;

    std::vector<std::string> rawLines;
    std::stringstream ss(currentMessage);
    std::string rawLine;
    while (std::getline(ss, rawLine)) {
        rawLines.push_back(rawLine);
    }
    if (rawLines.empty()) return;

    std::vector<std::string> lines;
    for (const auto& rLine : rawLines) {
        sf::Text testText(font, toUtf8(rLine), charSize);
        if (testText.getLocalBounds().size.x <= maxAllowedContentWidth) {
            lines.push_back(rLine);
        } else {
            // Word wrap
            std::stringstream lineStream(rLine);
            std::string word;
            std::string currentWrapped;
            while (lineStream >> word) {
                std::string candidate = currentWrapped.empty() ? word : (currentWrapped + " " + word);
                sf::Text candidateText(font, toUtf8(candidate), charSize);
                if (candidateText.getLocalBounds().size.x > maxAllowedContentWidth && !currentWrapped.empty()) {
                    lines.push_back(currentWrapped);
                    currentWrapped = word;
                } else {
                    currentWrapped = candidate;
                }
            }
            if (!currentWrapped.empty()) {
                lines.push_back(currentWrapped);
            }
        }
    }
    if (lines.empty()) return;

    // Calculate maximum width across all lines
    float maxLineWidth = 0.0f;
    for (const auto& l : lines) {
        sf::Text t(font, toUtf8(l), charSize);
        maxLineWidth = std::max(maxLineWidth, t.getLocalBounds().size.x);
    }

    const float totalTextHeight = static_cast<float>(lines.size()) * lineHeight;
    const float boxWidth = std::clamp(maxLineWidth + 50.0f, 360.0f, VIRTUAL_WIDTH - 60.0f);
    const float boxHeight = std::max(84.0f, totalTextHeight + 36.0f);
    const float boxX = std::clamp(VIRTUAL_WIDTH - boxWidth - 35.0f, 20.0f, VIRTUAL_WIDTH - boxWidth - 20.0f);
    const float boxY = 24.0f;

    // Outer glow / shadow
    sf::RectangleShape shadow({ boxWidth + 6.0f, boxHeight + 6.0f });
    shadow.setPosition({ boxX - 3.0f, boxY - 3.0f });
    shadow.setFillColor(sf::Color(0, 0, 0, 160));
    window.draw(shadow);

    // Dark translucent background box
    sf::RectangleShape box({ boxWidth, boxHeight });
    box.setPosition({ boxX, boxY });
    box.setFillColor(sf::Color(12, 16, 24, 235));
    box.setOutlineThickness(2.8f);
    box.setOutlineColor(sf::Color::White);
    window.draw(box);

    const float startY = boxY + (boxHeight - totalTextHeight) / 2.0f;

    for (size_t i = 0; i < lines.size(); ++i) {
        sf::Text t(font, toUtf8(lines[i]), charSize);
        t.setFillColor(sf::Color::White);
        t.setStyle(sf::Text::Bold);
        sf::FloatRect b = t.getLocalBounds();
        float textX = boxX + (boxWidth - b.size.x) / 2.0f - b.position.x;
        float textY = startY + static_cast<float>(i) * lineHeight - b.position.y;
        t.setPosition({ textX, textY });
        window.draw(t);
    }
}
