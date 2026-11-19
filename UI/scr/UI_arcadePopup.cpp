#include "../includes/UI_arcadePopup.h"
#include "../includes/UI_text.h"
#include "../includes/UI_theme.h"
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

void ArcadePopup::draw(sf::RenderTarget& window) {
    if (timer <= 0.0f || currentMessage.empty()) return;

    if (!fontLoaded) return;

    // Split message by newline, wrapping lines that exceed maxAllowedContentWidth
    const unsigned int charSize = 14;
    const float lineHeight = 28.0f;
    const float maxAllowedContentWidth = 520.0f;

    std::string wrapped = ui::wrapText(font, currentMessage, charSize, maxAllowedContentWidth, true);
    std::vector<std::string> lines;
    std::stringstream ss(wrapped);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    if (lines.empty()) return;

    // Measure actual rendered bold width across all wrapped lines
    float maxLineWidth = 0.0f;
    for (const auto& l : lines) {
        sf::Text& t = ui::pooledText(font, toUtf8(l), charSize);
        t.setStyle(sf::Text::Bold);
        maxLineWidth = std::max(maxLineWidth, t.getLocalBounds().size.x);
    }

    const float totalTextHeight = static_cast<float>(lines.size()) * lineHeight;
    const float boxWidth = std::clamp(maxLineWidth + 60.0f, 380.0f, VIRTUAL_WIDTH - 60.0f);
    const float boxHeight = std::max(88.0f, totalTextHeight + 36.0f);
    const float boxX = std::clamp(VIRTUAL_WIDTH - boxWidth - 35.0f, 20.0f, VIRTUAL_WIDTH - boxWidth - 20.0f);
    const float boxY = 24.0f;
    const sf::FloatRect boxRect({ boxX, boxY }, { boxWidth, boxHeight });

    // Outer glow / shadow
    sf::RectangleShape shadow({ boxWidth + 6.0f, boxHeight + 6.0f });
    shadow.setPosition({ boxX - 3.0f, boxY - 3.0f });
    shadow.setFillColor(sf::Color(0, 0, 0, 170));
    window.draw(shadow);

    // Dark translucent background box with crisp border
    sf::RectangleShape box({ boxWidth, boxHeight });
    box.setPosition({ boxX, boxY });
    box.setFillColor(theme::withAlpha(theme::Panel, 245));
    box.setOutlineThickness(2.8f);
    box.setOutlineColor(theme::Focus);
    window.draw(box);
    ui::lint::occlude(boxRect);

    const float startY = boxY + (boxHeight - totalTextHeight) / 2.0f;

    for (size_t i = 0; i < lines.size(); ++i) {
        sf::Text& t = ui::pooledText(font, toUtf8(lines[i]), charSize);
        t.setFillColor(theme::TextPrimary);
        t.setStyle(sf::Text::Bold);
        sf::FloatRect b = t.getLocalBounds();
        float slotY = startY + static_cast<float>(i) * lineHeight;
        float textX = boxX + (boxWidth - b.size.x) / 2.0f - b.position.x;
        float textY = slotY + (lineHeight - b.size.y) / 2.0f - b.position.y;
        t.setPosition({ textX, textY });
        ui::drawText(window, t, boxRect);
    }
}
