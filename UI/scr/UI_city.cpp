#include "../includes/UI_city.h"
#include "../includes/UI_types.h"
#include <cmath>
#include <string>

UI_city::UI_city() {
}

void UI_city::drawDividingRiver(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded, float animTime) {
    float screenHeight = static_cast<float>(window.getSize().y);
    float midX = 800.0f;

    // Full Central Dividing Line from Y=0 to Y=900
    sf::RectangleShape centerLine({ 3.0f, screenHeight });
    centerLine.setPosition({ midX - 1.5f, 0.0f });
    centerLine.setFillColor(sf::Color(0, 220, 100, 200));
    window.draw(centerLine);

    // River channel through the city area (Y = 65 to Y = 395)
    sf::RectangleShape river({ 22.0f, 330.0f });
    river.setPosition({ midX - 11.0f, 65.0f });
    river.setFillColor(sf::Color(20, 48, 80, 240));
    river.setOutlineThickness(1.5f);
    river.setOutlineColor(sf::Color(0, 180, 240, 150));
    window.draw(river);

    // Animated water shimmer
    for (float y = 75.0f; y < 385.0f; y += 35.0f) {
        float waveOff = std::sin(animTime * 3.0f + y * 0.1f) * 3.5f;
        sf::RectangleShape wave({ 10.0f, 2.5f });
        wave.setPosition({ midX - 5.0f + waveOff, y });
        wave.setFillColor(sf::Color(120, 220, 255, 160));
        window.draw(wave);
    }

    // Road bridges connecting West and East across river
    for (float bridgeY : { 170.0f, 290.0f }) {
        sf::RectangleShape bridge({ 36.0f, 18.0f });
        bridge.setPosition({ midX - 18.0f, bridgeY });
        bridge.setFillColor(sf::Color(45, 52, 65, 245));
        bridge.setOutlineThickness(1.0f);
        bridge.setOutlineColor(sf::Color(255, 215, 0, 180));
        window.draw(bridge);

        sf::RectangleShape lane({ 12.0f, 2.0f });
        lane.setPosition({ midX - 6.0f, bridgeY + 8.0f });
        lane.setFillColor(sf::Color(255, 240, 100));
        window.draw(lane);
    }

    // Border marker tag below city
    sf::RectangleShape tag({ 200.0f, 26.0f });
    tag.setPosition({ midX - 100.0f, 400.0f });
    tag.setFillColor(sf::Color(15, 20, 32, 245));
    tag.setOutlineThickness(1.0f);
    tag.setOutlineColor(sf::Color(0, 220, 100, 200));
    window.draw(tag);

    if (fontLoaded) {
        sf::Text bText(font, toUtf8("ЦЕНТРАЛНА ГРАНИЦА"), 12);
        bText.setFillColor(sf::Color(140, 255, 180));
        sf::FloatRect tb = bText.getLocalBounds();
        bText.setPosition({ midX - tb.size.x / 2.0f, 404.0f });
        window.draw(bText);
    }
}

void UI_city::drawCity(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded, float animTime,
                      float p1Share, const std::string& cutMessage) {
    float midX = 800.0f;
    float cityWidth = 380.0f;
    float cityLeft = midX - cityWidth / 2.0f; // 610.0f
    float cityTop = 65.0f;
    float cityHeight = 330.0f;

    // Dynamic territorial border inside city based on energy share
    float captureX = cityLeft + cityWidth * p1Share;

    // City base platform
    sf::RectangleShape base({ cityWidth, cityHeight });
    base.setPosition({ cityLeft, cityTop });
    base.setFillColor(sf::Color(24, 30, 42, 245));
    base.setOutlineThickness(2.0f);
    base.setOutlineColor(sf::Color(0, 220, 100, 220));
    window.draw(base);

    // High-rise Skyscraper Buildings
    struct CityBuilding {
        float x, y, w, h;
        bool originalWest;
    };

    CityBuilding buildings[] = {
        // --- WEST SIDE BUILDINGS ---
        { 618.0f, 150.0f, 38.0f, 240.0f, true },
        { 660.0f, 105.0f, 44.0f, 285.0f, true },
        { 708.0f, 175.0f, 36.0f, 215.0f, true },
        { 748.0f, 120.0f, 38.0f, 270.0f, true },

        // --- EAST SIDE BUILDINGS ---
        { 814.0f, 120.0f, 38.0f, 270.0f, false },
        { 856.0f, 175.0f, 36.0f, 215.0f, false },
        { 896.0f, 105.0f, 44.0f, 285.0f, false },
        { 944.0f, 150.0f, 38.0f, 240.0f, false },
    };

    for (const auto& b : buildings) {
        // Building belongs to whichever player controls its coordinate
        bool controlledByP1 = (b.x + b.w / 2.0f < captureX);
        bool isCutOff = (b.originalWest && !controlledByP1) || (!b.originalWest && controlledByP1);

        sf::Color outlineColor = controlledByP1 ? sf::Color(0, 200, 255) : sf::Color(255, 120, 200);
        sf::Color roofColor = controlledByP1 ? sf::Color(80, 230, 255) : sf::Color(255, 160, 220);

        sf::RectangleShape bShape({ b.w, b.h });
        bShape.setPosition({ b.x, b.y });
        bShape.setFillColor(controlledByP1 ? sf::Color(28, 38, 54) : sf::Color(44, 30, 48));
        bShape.setOutlineThickness(1.5f);
        bShape.setOutlineColor(outlineColor);
        window.draw(bShape);

        // Rooftop structure
        sf::RectangleShape roof({ b.w - 8.0f, 6.0f });
        roof.setPosition({ b.x + 4.0f, b.y - 6.0f });
        roof.setFillColor(roofColor);
        window.draw(roof);

        // Spire & Blinking Beacon
        if (b.h > 250.0f) {
            sf::RectangleShape spire({ 2.5f, 16.0f });
            spire.setPosition({ b.x + b.w / 2.0f - 1.25f, b.y - 22.0f });
            spire.setFillColor(sf::Color(200, 215, 230));
            window.draw(spire);

            bool blink = (std::sin(animTime * 5.0f + b.x) > 0.0f);
            sf::CircleShape beacon(3.0f);
            beacon.setPosition({ b.x + b.w / 2.0f - 3.0f, b.y - 25.0f });
            beacon.setFillColor(blink ? outlineColor : sf::Color(60, 20, 30));
            window.draw(beacon);
        }

        // Window matrix
        int rows = static_cast<int>(b.h / 16.0f);
        int cols = static_cast<int>(b.w / 11.0f);
        for (int r = 2; r < rows; r++) {
            for (int c = 1; c < cols; c++) {
                int hash = (r * 11 + c * 17 + static_cast<int>(b.x * 3)) % 100;
                bool isPowered = hash < 65;
                // If building is cut off / conquered, windows flicker with blackout alert
                if (isCutOff) {
                    isPowered = (std::sin(animTime * 4.0f + r) > 0.0f) && (hash < 35);
                }

                sf::RectangleShape win({ 5.0f, 7.0f });
                win.setPosition({ b.x + c * 10.0f, b.y + r * 15.0f });

                if (isPowered) {
                    win.setFillColor(controlledByP1 ? sf::Color(100, 230, 255, 220) : sf::Color(255, 215, 120, 220));
                } else {
                    win.setFillColor(sf::Color(16, 20, 28, 240));
                }
                window.draw(win);
            }
        }

        // Visual Cutoff / Hazard Striping if this building was severed from its original owner
        if (isCutOff) {
            for (float hy = b.y + 10.0f; hy < b.y + b.h - 10.0f; hy += 24.0f) {
                sf::RectangleShape stripe({ b.w - 4.0f, 3.0f });
                stripe.setPosition({ b.x + 2.0f, hy });
                stripe.setFillColor(sf::Color(255, 215, 0, 160));
                window.draw(stripe);
            }
        }
    }

    // Dynamic capture line through the city
    sf::RectangleShape capNeedle({ 3.0f, cityHeight });
    capNeedle.setPosition({ captureX - 1.5f, cityTop });
    capNeedle.setFillColor(sf::Color(255, 240, 100));
    window.draw(capNeedle);

    // City Header banner
    if (fontLoaded) {
        sf::RectangleShape banner({ cityWidth, 26.0f });
        banner.setPosition({ cityLeft, cityTop });
        banner.setFillColor(sf::Color(15, 22, 34, 250));
        banner.setOutlineThickness(1.0f);
        banner.setOutlineColor(sf::Color(0, 220, 100));
        window.draw(banner);

        int p1Pct = static_cast<int>(p1Share * 100.0f);
        int p2Pct = 100 - p1Pct;
        std::string titleStr = "ГРАД (METROPOLIS) | P1: " + std::to_string(p1Pct) + "% | P2: " + std::to_string(p2Pct) + "%";
        sf::Text cLabel(font, toUtf8(titleStr), 12);
        cLabel.setFillColor(sf::Color(0, 255, 180));
        sf::FloatRect lb = cLabel.getLocalBounds();
        cLabel.setPosition({ midX - lb.size.x / 2.0f, cityTop + 5.0f });
        window.draw(cLabel);

        // Cut notification banner at bottom of city if conquest occurred
        if (!cutMessage.empty()) {
            sf::RectangleShape cutBar({ cityWidth, 22.0f });
            cutBar.setPosition({ cityLeft, cityTop + cityHeight - 24.0f });
            cutBar.setFillColor(sf::Color(45, 15, 20, 230));
            cutBar.setOutlineThickness(1.0f);
            cutBar.setOutlineColor(sf::Color(255, 100, 100));
            window.draw(cutBar);

            sf::Text cutText(font, toUtf8(cutMessage), 10);
            cutText.setFillColor(sf::Color(255, 220, 100));
            sf::FloatRect cb = cutText.getLocalBounds();
            cutText.setPosition({ midX - cb.size.x / 2.0f, cityTop + cityHeight - 20.0f });
            window.draw(cutText);
        }
    }
}

void UI_city::drawInfluenceBar(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                              int demand, int p1Energy, int p2Energy, float p1Share) {
    float screenWidth = static_cast<float>(window.getSize().x);

    float panelW = 560.0f;
    float panelH = 42.0f;
    float panelX = (screenWidth - panelW) / 2.0f; // 520.0f
    float panelY = 10.0f;

    sf::RectangleShape cPanel({ panelW, panelH });
    cPanel.setPosition({ panelX, panelY });
    cPanel.setFillColor(sf::Color(18, 24, 36, 245));
    cPanel.setOutlineThickness(1.5f);
    cPanel.setOutlineColor(sf::Color(70, 95, 130));
    window.draw(cPanel);

    if (fontLoaded) {
        int totalSupplied = p1Energy + p2Energy;
        std::string dStr = "НУЖДА НА ГРАДА: " + std::to_string(demand) + " MW | ДОСТАВКА: " + std::to_string(totalSupplied) + " MW";
        sf::Text tDemand(font, toUtf8(dStr), 12);
        tDemand.setFillColor(sf::Color(255, 215, 0));
        sf::FloatRect db = tDemand.getLocalBounds();
        tDemand.setPosition({ panelX + (panelW - db.size.x) / 2.0f, panelY + 4.0f });
        window.draw(tDemand);

        float barW = 440.0f;
        float barH = 10.0f;
        float barX = panelX + (panelW - barW) / 2.0f;
        float barY = panelY + 24.0f;

        sf::RectangleShape baseBar({ barW, barH });
        baseBar.setPosition({ barX, barY });
        baseBar.setFillColor(sf::Color(40, 50, 65));
        window.draw(baseBar);

        // P1 Cyan Portion
        sf::RectangleShape p1Bar({ barW * p1Share, barH });
        p1Bar.setPosition({ barX, barY });
        p1Bar.setFillColor(sf::Color(0, 220, 255));
        window.draw(p1Bar);

        // P2 Magenta Portion
        sf::RectangleShape p2Bar({ barW * (1.0f - p1Share), barH });
        p2Bar.setPosition({ barX + barW * p1Share, barY });
        p2Bar.setFillColor(sf::Color(255, 120, 200));
        window.draw(p2Bar);

        // Dividing needle
        sf::RectangleShape needle({ 3.0f, barH + 4.0f });
        needle.setPosition({ barX + barW * p1Share - 1.5f, barY - 2.0f });
        needle.setFillColor(sf::Color::White);
        window.draw(needle);

        // Percent tags on edges
        int p1Pct = static_cast<int>(p1Share * 100.0f);
        int p2Pct = 100 - p1Pct;

        sf::Text p1Tag(font, toUtf8("P1: " + std::to_string(p1Pct) + "%"), 11);
        p1Tag.setFillColor(sf::Color(0, 229, 255));
        p1Tag.setPosition({ barX - 48.0f, barY - 2.0f });
        window.draw(p1Tag);

        sf::Text p2Tag(font, toUtf8("P2: " + std::to_string(p2Pct) + "%"), 11);
        p2Tag.setFillColor(sf::Color(255, 140, 210));
        p2Tag.setPosition({ barX + barW + 8.0f, barY - 2.0f });
        window.draw(p2Tag);
    }
}
