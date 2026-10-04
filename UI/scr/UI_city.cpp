#include "../includes/UI_city.h"
#include "../includes/UI_text.h"
#include "../includes/UI_shot.h"
#include "../includes/UI_theme.h"
#include "../includes/UI_types.h"
#include "../../Game/includes/game_balance.h"
#include <cmath>
#include <string>
#include <vector>

UI_city::UI_city() {
}

void UI_city::drawDividingRiver(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded, float animTime, bool isDaylight) {
    float screenHeight = VIRTUAL_HEIGHT;
    float midX = VIRTUAL_WIDTH / 2.0f; // 800.0f

    // Full Central Dividing Line from Y=0 to Y=900
    sf::RectangleShape centerLine({ 3.0f, screenHeight });
    centerLine.setPosition({ midX - 1.5f, 0.0f });
    centerLine.setFillColor(theme::withAlpha(theme::Neutral, 200));
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
        sf::RectangleShape bridge({ 40.0f, 20.0f });
        bridge.setPosition({ midX - 20.0f, bridgeY });
        bridge.setFillColor(sf::Color(45, 52, 65, 245));
        bridge.setOutlineThickness(1.0f);
        bridge.setOutlineColor(sf::Color(255, 230, 120, 180));
        window.draw(bridge);

        sf::RectangleShape lane({ 16.0f, 2.0f });
        lane.setPosition({ midX - 8.0f, bridgeY + 9.0f });
        lane.setFillColor(sf::Color(255, 240, 100));
        window.draw(lane);

        // Animated Traffic / Electric Cars crossing the bridges!
        float carOffset1 = std::fmod(animTime * 40.0f + bridgeY, 50.0f) - 25.0f;
        sf::RectangleShape car1({ 8.0f, 4.5f });
        car1.setPosition({ midX + carOffset1, bridgeY + 3.0f });
        car1.setFillColor(theme::P1);
        window.draw(car1);

        float carOffset2 = 25.0f - std::fmod(animTime * 32.0f + bridgeY * 1.5f, 50.0f);
        sf::RectangleShape car2({ 8.0f, 4.5f });
        car2.setPosition({ midX + carOffset2, bridgeY + 12.0f });
        car2.setFillColor(theme::P2);
        window.draw(car2);

        // Night bridge streetlights and car headlights
        if (!isDaylight) {
            sf::CircleShape lampW(3.5f);
            lampW.setPosition({ midX - 22.0f, bridgeY + 1.0f });
            lampW.setFillColor(sf::Color(255, 240, 160, 240));
            window.draw(lampW);

            sf::CircleShape lampE(3.5f);
            lampE.setPosition({ midX + 16.0f, bridgeY + 1.0f });
            lampE.setFillColor(sf::Color(255, 240, 160, 240));
            window.draw(lampE);

            // Headlight beams
            sf::RectangleShape beam1({ 12.0f, 3.0f });
            beam1.setPosition({ midX + carOffset1 + 8.0f, bridgeY + 3.8f });
            beam1.setFillColor(sf::Color(255, 255, 200, 140));
            window.draw(beam1);

            sf::RectangleShape beam2({ 12.0f, 3.0f });
            beam2.setPosition({ midX + carOffset2 - 12.0f, bridgeY + 12.8f });
            beam2.setFillColor(sf::Color(255, 255, 200, 140));
            window.draw(beam2);
        }
    }

    // Border marker tag below city
    sf::RectangleShape tag({ 200.0f, 26.0f });
    tag.setPosition({ midX - 100.0f, 400.0f });
    tag.setFillColor(theme::withAlpha(theme::Panel, 245));
    tag.setOutlineThickness(1.0f);
    tag.setOutlineColor(theme::withAlpha(theme::Neutral, 200));
    window.draw(tag);

    if (fontLoaded) {
        sf::Text& bText = ui::pooledText(font, toUtf8("ЦЕНТРАЛНА ГРАНИЦА"), fontsize::Label);
        bText.setStyle(sf::Text::Bold);
        bText.setFillColor(theme::TextPrimary);
        sf::FloatRect tb = bText.getLocalBounds();
        bText.setPosition({ midX - tb.size.x / 2.0f, 404.0f });
        ui::drawText(window, bText, sf::FloatRect(tag.getPosition(), tag.getSize()));
    }
}

void UI_city::drawCity(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded, float animTime,
                      float p1Share, const std::string& cutMessage, bool isDaylight,
                      float currentHour, SeasonType season) {
    (void)currentHour;
    (void)season;
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
    base.setFillColor(isDaylight ? theme::withAlpha(theme::Card, 245) : theme::withAlpha(theme::Well, 250));
    base.setOutlineThickness(2.0f);
    base.setOutlineColor(theme::withAlpha(theme::Neutral, 220));
    window.draw(base);
    ui::lint::solid(sf::FloatRect({ cityLeft, cityTop }, { cityWidth, cityHeight })); // nothing may hide under the city

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

    // Predefined colors for Player 1 (Blue/Cyan) and Player 2 (Magenta/Red)
    const sf::Color p1Outline = theme::P1;
    const sf::Color p2Outline = theme::P2;
    const sf::Color p1Roof = theme::P1Light;
    const sf::Color p2Roof = theme::P2Light;
    const sf::Color p1ColorDay(32, 44, 62);
    const sf::Color p2ColorDay(48, 34, 52);
    const sf::Color p1ColorNight(18, 26, 38);
    const sf::Color p2ColorNight(30, 20, 32);

    const sf::Color p1Fill = isDaylight ? p1ColorDay : p1ColorNight;
    const sf::Color p2Fill = isDaylight ? p2ColorDay : p2ColorNight;

    for (const auto& b : buildings) {
        float bLeft = b.x;
        float bRight = b.x + b.w;

        // 1. Building Body: Sliced if captureX divides this building!
        if (captureX <= bLeft) {
            // Entirely in Player 2 territory
            sf::RectangleShape bShape({ b.w, b.h });
            bShape.setPosition({ b.x, b.y });
            bShape.setFillColor(p2Fill);
            bShape.setOutlineThickness(1.5f);
            bShape.setOutlineColor(p2Outline);
            window.draw(bShape);
        } else if (captureX >= bRight) {
            // Entirely in Player 1 territory
            sf::RectangleShape bShape({ b.w, b.h });
            bShape.setPosition({ b.x, b.y });
            bShape.setFillColor(p1Fill);
            bShape.setOutlineThickness(1.5f);
            bShape.setOutlineColor(p1Outline);
            window.draw(bShape);
        } else {
            // Sliced Building: Left portion is Player 1 (Blue), Right portion is Player 2 (Red)
            float w1 = captureX - bLeft;
            float w2 = bRight - captureX;

            sf::RectangleShape p1Slice({ w1, b.h });
            p1Slice.setPosition({ bLeft, b.y });
            p1Slice.setFillColor(p1Fill);
            p1Slice.setOutlineThickness(1.5f);
            p1Slice.setOutlineColor(p1Outline);
            window.draw(p1Slice);

            sf::RectangleShape p2Slice({ w2, b.h });
            p2Slice.setPosition({ captureX, b.y });
            p2Slice.setFillColor(p2Fill);
            p2Slice.setOutlineThickness(1.5f);
            p2Slice.setOutlineColor(p2Outline);
            window.draw(p2Slice);
        }

        // 2. Rooftop Structure (Sliced)
        float rLeft = b.x + 4.0f;
        float rRight = b.x + b.w - 4.0f;
        if (captureX <= rLeft) {
            sf::RectangleShape roof({ rRight - rLeft, 6.0f });
            roof.setPosition({ rLeft, b.y - 6.0f });
            roof.setFillColor(p2Roof);
            window.draw(roof);
        } else if (captureX >= rRight) {
            sf::RectangleShape roof({ rRight - rLeft, 6.0f });
            roof.setPosition({ rLeft, b.y - 6.0f });
            roof.setFillColor(p1Roof);
            window.draw(roof);
        } else {
            // Roof cut right at captureX
            sf::RectangleShape roof1({ captureX - rLeft, 6.0f });
            roof1.setPosition({ rLeft, b.y - 6.0f });
            roof1.setFillColor(p1Roof);
            window.draw(roof1);

            sf::RectangleShape roof2({ rRight - captureX, 6.0f });
            roof2.setPosition({ captureX, b.y - 6.0f });
            roof2.setFillColor(p2Roof);
            window.draw(roof2);
        }

        // 3. Spire & Blinking Beacon
        if (b.h > 250.0f) {
            float spireX = b.x + b.w / 2.0f;
            bool spireInP1 = (spireX < captureX);
            sf::Color beaconColor = spireInP1 ? p1Outline : p2Outline;

            sf::RectangleShape spire({ 2.5f, 16.0f });
            spire.setPosition({ spireX - 1.25f, b.y - 22.0f });
            spire.setFillColor(sf::Color(200, 215, 230));
            window.draw(spire);

            bool blink = (std::sin(animTime * 5.0f + b.x) > 0.0f);
            sf::CircleShape beacon(3.0f);
            beacon.setPosition({ spireX - 3.0f, b.y - 25.0f });
            beacon.setFillColor(blink ? beaconColor : sf::Color(60, 20, 30));
            window.draw(beacon);
        }

        // 4. Window Matrix: adapts to day (reflective glass) vs night (electric neon glow)
        // Each individual window receives its owner's color depending on which side of captureX it sits on!
        int rows = static_cast<int>(b.h / 16.0f);
        int cols = static_cast<int>(b.w / 11.0f);
        for (int r = 2; r < rows; r++) {
            for (int c = 1; c < cols; c++) {
                float winX = b.x + c * 10.0f;
                float winY = b.y + r * 15.0f;
                bool winInP1 = (winX + 2.5f < captureX);
                bool winCutOff = (b.originalWest && !winInP1) || (!b.originalWest && winInP1);

                int hash = (r * 11 + c * 17 + static_cast<int>(b.x * 3)) % 100;
                bool isPowered = hash < 65;
                if (winCutOff) {
                    isPowered = (std::sin(animTime * 4.0f + r) > 0.0f) && (hash < 35);
                }

                sf::RectangleShape win({ 5.0f, 7.0f });
                win.setPosition({ winX, winY });

                if (isPowered) {
                    if (isDaylight) {
                        win.setFillColor(winInP1 ? sf::Color(90, 160, 200, 190) : sf::Color(210, 180, 130, 190));
                    } else {
                        win.setFillColor(theme::withAlpha(winInP1 ? theme::P1 : theme::P2, 245));
                    }
                } else {
                    win.setFillColor(sf::Color(14, 18, 26, 240));
                }
                window.draw(win);
            }
        }

        // 5. Visual Cutoff / Hazard Striping only on the captured portion of this building
        float stripMinX = b.x + 2.0f;
        float stripMaxX = b.x + b.w - 2.0f;
        if (b.originalWest) {
            // Conquered part of West building is x >= captureX
            float startX = std::max(stripMinX, captureX);
            float endX = stripMaxX;
            if (endX > startX + 2.0f) {
                for (float hy = b.y + 10.0f; hy < b.y + b.h - 10.0f; hy += 24.0f) {
                    sf::RectangleShape stripe({ endX - startX, 3.0f });
                    stripe.setPosition({ startX, hy });
                    stripe.setFillColor(theme::withAlpha(theme::Warn, 150));
                    window.draw(stripe);
                }
            }
        } else {
            // Conquered part of East building is x <= captureX
            float startX = stripMinX;
            float endX = std::min(stripMaxX, captureX);
            if (endX > startX + 2.0f) {
                for (float hy = b.y + 10.0f; hy < b.y + b.h - 10.0f; hy += 24.0f) {
                    sf::RectangleShape stripe({ endX - startX, 3.0f });
                    stripe.setPosition({ startX, hy });
                    stripe.setFillColor(theme::withAlpha(theme::Warn, 150));
                    window.draw(stripe);
                }
            }
        }
    }

    // Dynamic capture line through the city
    sf::RectangleShape capNeedle({ 3.0f, cityHeight });
    capNeedle.setPosition({ captureX - 1.5f, cityTop });
    capNeedle.setFillColor(theme::Focus);
    window.draw(capNeedle);

    // City Header banner
    if (fontLoaded) {
        sf::RectangleShape banner({ cityWidth, 26.0f });
        banner.setPosition({ cityLeft, cityTop });
        banner.setFillColor(theme::withAlpha(theme::Panel, 250));
        banner.setOutlineThickness(1.0f);
        banner.setOutlineColor(theme::Neutral);
        window.draw(banner);

        int p1Pct = static_cast<int>(std::lround(p1Share * 100.0f));
        int p2Pct = 100 - p1Pct;
        std::string phaseStr = isDaylight ? "ДЕН" : "НОЩ";
        std::string titleStr = "ГРАД · " + phaseStr + " · P1 " + std::to_string(p1Pct) + "% · P2 " + std::to_string(p2Pct) + "%";
        sf::Text& cLabel = ui::pooledText(font, toUtf8(titleStr), fontsize::Label);
        cLabel.setStyle(sf::Text::Bold);
        cLabel.setFillColor(theme::TextPrimary);
        sf::FloatRect lb = cLabel.getLocalBounds();
        cLabel.setPosition({ midX - lb.size.x / 2.0f, cityTop + 5.0f });
        ui::drawText(window, cLabel, sf::FloatRect(banner.getPosition(), banner.getSize()));

        // Cut notification banner at bottom of city if conquest occurred
        if (!cutMessage.empty()) {
            // The engine's day messages are long: wrap them into centred lines and size the bar to fit
            const unsigned int cutSize = fontsize::Caption;
            const float lineH = 14.0f;
            std::string wrapped = ui::wrapText(font, cutMessage, cutSize, cityWidth - 16.0f);
            std::vector<std::string> lines;
            std::size_t start = 0;
            while (true) {
                std::size_t nl = wrapped.find('\n', start);
                lines.push_back(wrapped.substr(start, nl == std::string::npos ? std::string::npos : nl - start));
                if (nl == std::string::npos) break;
                start = nl + 1;
            }
            const float barH = 8.0f + lineH * static_cast<float>(lines.size());
            sf::RectangleShape cutBar({ cityWidth, barH });
            cutBar.setPosition({ cityLeft, cityTop + cityHeight - barH - 2.0f });
            cutBar.setFillColor(theme::withAlpha(theme::Panel, 240));
            cutBar.setOutlineThickness(1.0f);
            cutBar.setOutlineColor(theme::Warn);
            window.draw(cutBar);
            const sf::FloatRect barRect(cutBar.getPosition(), cutBar.getSize());

            for (std::size_t i = 0; i < lines.size(); ++i) {
                sf::Text& cutText = ui::pooledText(font, toUtf8(lines[i]), cutSize);
                cutText.setFillColor(theme::TextPrimary);
                sf::FloatRect cb = cutText.getLocalBounds();
                cutText.setPosition({ midX - cb.size.x / 2.0f - cb.position.x,
                                      barRect.position.y + 3.0f + lineH * static_cast<float>(i) });
                ui::drawText(window, cutText, barRect);
            }
        }
    }
}

void UI_city::drawInfluenceBar(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                              int demand, int p1Energy, int p2Energy, float p1Share, int currentDay) {
    float screenWidth = VIRTUAL_WIDTH;

    float panelW = 560.0f;
    float panelH = 42.0f;
    float panelX = (screenWidth - panelW) / 2.0f; // 520.0f
    float panelY = 10.0f;

    sf::RectangleShape cPanel({ panelW, panelH });
    cPanel.setPosition({ panelX, panelY });
    cPanel.setFillColor(theme::withAlpha(theme::Panel, 245));
    cPanel.setOutlineThickness(1.5f);
    cPanel.setOutlineColor(theme::Line);
    window.draw(cPanel);
    ui::lint::ContainerScope panelScope(sf::FloatRect({ panelX, panelY }, { panelW, panelH }));

    if (fontLoaded) {
        int totalSupplied = p1Energy + p2Energy;
        std::string dStr;
        sf::Color demandColor;

        if (currentDay <= Balance::GRACE_PERIOD_DAYS) {
            dStr = "ГРАТИСЕН ПЕРИОД (ДЕН " + std::to_string(currentDay) + "/" +
                   std::to_string(Balance::GRACE_PERIOD_DAYS) + "): ГРАДЪТ ИСКА 0 MW · ДОСТАВКА " + std::to_string(totalSupplied) + " MW";
            demandColor = theme::Good;
        } else {
            dStr = "НУЖДА НА ГРАДА " + std::to_string(demand) + " MW · ДОСТАВКА " + std::to_string(totalSupplied) +
                   " MW · ПОБЕДА ПРИ " + std::to_string(static_cast<int>(std::lround(Balance::VICTORY_SHARE * 100.0f))) + "%";
            demandColor = theme::TextPrimary;
        }

        sf::Text& tDemand = ui::pooledText(font, toUtf8(dStr), fontsize::Label);
        tDemand.setStyle(sf::Text::Bold);
        tDemand.setFillColor(demandColor);
        sf::FloatRect db = tDemand.getLocalBounds();
        tDemand.setPosition({ panelX + (panelW - db.size.x) / 2.0f, panelY + 4.0f });
        ui::drawText(window, tDemand);

        const sf::FloatRect barRect = influenceBarRect(); // [b-effects] shared with the UI_fx tween overlay
        float barW = barRect.size.x;
        float barH = barRect.size.y;
        float barX = barRect.position.x;
        float barY = barRect.position.y;

        sf::RectangleShape baseBar({ barW, barH });
        baseBar.setPosition({ barX, barY });
        baseBar.setFillColor(theme::Well);
        window.draw(baseBar);

        // P1 Cyan Portion
        sf::RectangleShape p1Bar({ barW * p1Share, barH });
        p1Bar.setPosition({ barX, barY });
        p1Bar.setFillColor(theme::P1);
        window.draw(p1Bar);

        // P2 Magenta Portion
        sf::RectangleShape p2Bar({ barW * (1.0f - p1Share), barH });
        p2Bar.setPosition({ barX + barW * p1Share, barY });
        p2Bar.setFillColor(theme::P2);
        window.draw(p2Bar);

        // Victory threshold ticks: P1 wins when the needle reaches the right tick, P2 at the left one
        for (float tickShare : { Balance::VICTORY_SHARE, 1.0f - Balance::VICTORY_SHARE }) {
            sf::RectangleShape tick({ 2.0f, barH + 6.0f });
            tick.setPosition({ barX + barW * tickShare - 1.0f, barY - 3.0f });
            tick.setFillColor(theme::withAlpha(theme::TextPrimary, 230));
            window.draw(tick);
        }

        // Dividing needle
        sf::RectangleShape needle({ 3.0f, barH + 4.0f });
        needle.setPosition({ barX + barW * p1Share - 1.5f, barY - 2.0f });
        needle.setFillColor(sf::Color::White);
        window.draw(needle);

        // Percent tags on edges
        int p1Pct = static_cast<int>(std::lround(p1Share * 100.0f));
        int p2Pct = 100 - p1Pct;

        sf::Text& p1Tag = ui::pooledText(font, toUtf8("P1 " + std::to_string(p1Pct) + "%"), fontsize::Caption);
        p1Tag.setStyle(sf::Text::Bold);
        p1Tag.setFillColor(theme::P1Light);
        p1Tag.setPosition({ barX - 48.0f, barY - 2.0f });
        ui::drawText(window, p1Tag);

        sf::Text& p2Tag = ui::pooledText(font, toUtf8("P2 " + std::to_string(p2Pct) + "%"), fontsize::Caption);
        p2Tag.setStyle(sf::Text::Bold);
        p2Tag.setFillColor(theme::P2Light);
        p2Tag.setPosition({ barX + barW + 8.0f, barY - 2.0f });
        ui::drawText(window, p2Tag);
    }
}
