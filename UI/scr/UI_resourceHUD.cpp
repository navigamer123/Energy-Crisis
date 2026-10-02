#include "../includes/UI_resourceHUD.h"
#include "../includes/UI_types.h"
#include <cmath>

UI_resourceHUD::UI_resourceHUD()
    : p1BuyLandBtn({ 12.0f, 842.0f }, { 190.0f, 30.0f }),
      p2BuyLandBtn({ 1600.0f - 202.0f, 842.0f }, { 190.0f, 30.0f }) {
}

void UI_resourceHUD::drawQuarterCircle(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                                       const PlayerEconomy& econ, bool isWest) {
    float screenWidth = static_cast<float>(window.getSize().x);
    float screenHeight = static_cast<float>(window.getSize().y);
    float R = 235.0f; // Radius of the quarter circle

    // -------------------------------------------------------------------------
    // Draw the quarter circle geometry using TriangleFan
    // -------------------------------------------------------------------------
    sf::VertexArray fan(sf::PrimitiveType::TriangleFan);
    sf::VertexArray border(sf::PrimitiveType::LineStrip);

    if (isWest) {
        // Player 1 (Bottom-Left: Center at (0, screenHeight))
        fan.append(sf::Vertex{ { 0.0f, screenHeight }, sf::Color(14, 20, 32, 245), {} });
        for (int deg = -90; deg <= 0; deg += 3) {
            float rad = deg * 3.14159265f / 180.0f;
            sf::Vector2f pt(R * std::cos(rad), screenHeight + R * std::sin(rad));
            fan.append(sf::Vertex{ pt, sf::Color(20, 30, 48, 245), {} });
            border.append(sf::Vertex{ pt, sf::Color(0, 229, 255, 220), {} });
        }
        border.append(sf::Vertex{ { 0.0f, screenHeight }, sf::Color(0, 229, 255, 180), {} });
        border.append(sf::Vertex{ { 0.0f, screenHeight - R }, sf::Color(0, 229, 255, 180), {} });
    } else {
        // Player 2 (Bottom-Right: Center at (screenWidth, screenHeight))
        fan.append(sf::Vertex{ { screenWidth, screenHeight }, sf::Color(24, 18, 32, 245), {} });
        for (int deg = 180; deg <= 270; deg += 3) {
            float rad = deg * 3.14159265f / 180.0f;
            sf::Vector2f pt(screenWidth + R * std::cos(rad), screenHeight + R * std::sin(rad));
            fan.append(sf::Vertex{ pt, sf::Color(36, 24, 46, 245), {} });
            border.append(sf::Vertex{ pt, sf::Color(255, 120, 200, 220), {} });
        }
        border.append(sf::Vertex{ { screenWidth, screenHeight }, sf::Color(255, 120, 200, 180), {} });
        border.append(sf::Vertex{ { screenWidth - R, screenHeight }, sf::Color(255, 120, 200, 180), {} });
    }

    window.draw(fan);
    window.draw(border);

    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    // Resource Items inside the Quarter Circle
    // GOLD IS PROMINENTLY AT THE BOTTOM
    // NO TEXT LABELS ("what is what") - ONLY ICONS AND NUMBERS
    // -------------------------------------------------------------------------
    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    // Vector Icon Drawing Helpers
    auto drawWoodIcon = [&](float x, float y) {
        sf::RectangleShape log({ 18.0f, 10.0f });
        log.setPosition({ x + 4.0f, y + 2.0f });
        log.setFillColor(sf::Color(140, 90, 50));
        log.setOutlineThickness(1.0f);
        log.setOutlineColor(sf::Color(80, 50, 25));
        window.draw(log);

        sf::CircleShape cap(5.0f);
        cap.setPosition({ x, y + 2.0f });
        cap.setFillColor(sf::Color(190, 140, 90));
        cap.setOutlineThickness(1.0f);
        cap.setOutlineColor(sf::Color(80, 50, 25));
        window.draw(cap);

        sf::CircleShape leaf(3.5f, 3);
        leaf.setPosition({ x + 15.0f, y - 2.0f });
        leaf.setFillColor(sf::Color(70, 220, 90));
        window.draw(leaf);
    };

    auto drawOreIcon = [&](float x, float y) {
        sf::ConvexShape gem(4);
        gem.setPoint(0, { x + 9.0f, y });
        gem.setPoint(1, { x + 18.0f, y + 7.0f });
        gem.setPoint(2, { x + 9.0f, y + 15.0f });
        gem.setPoint(3, { x, y + 7.0f });
        gem.setFillColor(sf::Color(0, 225, 255, 230));
        gem.setOutlineThickness(1.2f);
        gem.setOutlineColor(sf::Color(190, 245, 255));
        window.draw(gem);

        sf::ConvexShape facet(3);
        facet.setPoint(0, { x + 9.0f, y + 3.0f });
        facet.setPoint(1, { x + 14.0f, y + 7.0f });
        facet.setPoint(2, { x + 9.0f, y + 12.0f });
        facet.setFillColor(sf::Color(255, 255, 255, 160));
        window.draw(facet);
    };

    auto drawEnergyIcon = [&](float x, float y) {
        sf::ConvexShape bolt(6);
        bolt.setPoint(0, { x + 10.0f, y });
        bolt.setPoint(1, { x + 3.0f, y + 8.0f });
        bolt.setPoint(2, { x + 9.0f, y + 8.0f });
        bolt.setPoint(3, { x + 6.0f, y + 16.0f });
        bolt.setPoint(4, { x + 16.0f, y + 6.0f });
        bolt.setPoint(5, { x + 10.0f, y + 6.0f });
        bolt.setFillColor(sf::Color(255, 225, 40));
        bolt.setOutlineThickness(1.0f);
        bolt.setOutlineColor(sf::Color(255, 255, 180));
        window.draw(bolt);
    };

    auto drawGoldCoin = [&](float x, float y) {
        sf::CircleShape coin(9.0f);
        coin.setPosition({ x, y });
        coin.setFillColor(sf::Color(255, 205, 30));
        coin.setOutlineThickness(1.5f);
        coin.setOutlineColor(sf::Color(180, 130, 0));
        window.draw(coin);

        sf::CircleShape inner(6.5f);
        inner.setPosition({ x + 2.5f, y + 2.5f });
        inner.setFillColor(sf::Color(255, 225, 60));
        inner.setOutlineThickness(1.0f);
        inner.setOutlineColor(sf::Color(210, 160, 20));
        window.draw(inner);
    };

    if (fontLoaded) {
        if (isWest) {
            // Player 1 (West)
            // Wood Row
            drawWoodIcon(22.0f, screenHeight - 195.0f);
            sf::Text tWood(font, std::to_string(econ.wood), 17);
            tWood.setFillColor(sf::Color(140, 255, 170));
            tWood.setPosition({ 52.0f, screenHeight - 200.0f });
            window.draw(tWood);

            // Ore Row
            drawOreIcon(22.0f, screenHeight - 160.0f);
            sf::Text tOre(font, std::to_string(econ.ore), 17);
            tOre.setFillColor(sf::Color(130, 230, 255));
            tOre.setPosition({ 52.0f, screenHeight - 165.0f });
            window.draw(tOre);

            // Energy Row
            drawEnergyIcon(22.0f, screenHeight - 125.0f);
            sf::Text tPwr(font, std::to_string(econ.energyMW) + " MW", 17);
            tPwr.setFillColor(sf::Color(255, 235, 100));
            tPwr.setPosition({ 52.0f, screenHeight - 130.0f });
            window.draw(tPwr);

            // GOLD: PROMINENTLY AT THE BOTTOM
            sf::RectangleShape goldPlaque({ 180.0f, 34.0f });
            goldPlaque.setPosition({ 14.0f, screenHeight - 88.0f });
            goldPlaque.setFillColor(sf::Color(45, 36, 10, 240));
            goldPlaque.setOutlineThickness(1.5f);
            goldPlaque.setOutlineColor(sf::Color(255, 215, 0));
            window.draw(goldPlaque);

            drawGoldCoin(24.0f, screenHeight - 80.0f);
            sf::Text tGold(font, std::to_string(econ.gold) + " G", 18);
            tGold.setFillColor(sf::Color(255, 225, 50));
            tGold.setPosition({ 52.0f, screenHeight - 84.0f });
            window.draw(tGold);

            // Land Expansion Button
            p1BuyLandBtn = sf::FloatRect({ 14.0f, screenHeight - 48.0f }, { 180.0f, 28.0f });
            bool hoverLand = p1BuyLandBtn.contains(mousePos);
            sf::RectangleShape landBtn(p1BuyLandBtn.size);
            landBtn.setPosition(p1BuyLandBtn.position);
            landBtn.setFillColor(hoverLand ? sf::Color(35, 85, 115) : sf::Color(18, 40, 60));
            landBtn.setOutlineThickness(1.0f);
            landBtn.setOutlineColor(hoverLand ? sf::Color(255, 215, 0) : sf::Color(0, 200, 255));
            window.draw(landBtn);

            sf::Text tLand(font, toUtf8("+ КУПИ ЗЕМЯ"), 12);
            tLand.setFillColor(hoverLand ? sf::Color(255, 240, 150) : sf::Color::White);
            sf::FloatRect tb = tLand.getLocalBounds();
            tLand.setPosition({ p1BuyLandBtn.position.x + (p1BuyLandBtn.size.x - tb.size.x) / 2.0f, screenHeight - 44.0f });
            window.draw(tLand);
        } else {
            // Player 2 (East)
            // Wood Row
            drawWoodIcon(screenWidth - 130.0f, screenHeight - 195.0f);
            sf::Text tWood(font, std::to_string(econ.wood), 17);
            tWood.setFillColor(sf::Color(140, 255, 170));
            tWood.setPosition({ screenWidth - 100.0f, screenHeight - 200.0f });
            window.draw(tWood);

            // Ore Row
            drawOreIcon(screenWidth - 130.0f, screenHeight - 160.0f);
            sf::Text tOre(font, std::to_string(econ.ore), 17);
            tOre.setFillColor(sf::Color(130, 230, 255));
            tOre.setPosition({ screenWidth - 100.0f, screenHeight - 165.0f });
            window.draw(tOre);

            // Energy Row
            drawEnergyIcon(screenWidth - 130.0f, screenHeight - 125.0f);
            sf::Text tPwr(font, std::to_string(econ.energyMW) + " MW", 17);
            tPwr.setFillColor(sf::Color(255, 235, 100));
            tPwr.setPosition({ screenWidth - 100.0f, screenHeight - 130.0f });
            window.draw(tPwr);

            // GOLD: PROMINENTLY AT THE BOTTOM
            sf::RectangleShape goldPlaque({ 180.0f, 34.0f });
            goldPlaque.setPosition({ screenWidth - 194.0f, screenHeight - 88.0f });
            goldPlaque.setFillColor(sf::Color(45, 36, 10, 240));
            goldPlaque.setOutlineThickness(1.5f);
            goldPlaque.setOutlineColor(sf::Color(255, 215, 0));
            window.draw(goldPlaque);

            drawGoldCoin(screenWidth - 184.0f, screenHeight - 80.0f);
            sf::Text tGold(font, std::to_string(econ.gold) + " G", 18);
            tGold.setFillColor(sf::Color(255, 225, 50));
            tGold.setPosition({ screenWidth - 156.0f, screenHeight - 84.0f });
            window.draw(tGold);

            // Land Expansion Button
            p2BuyLandBtn = sf::FloatRect({ screenWidth - 194.0f, screenHeight - 48.0f }, { 180.0f, 28.0f });
            bool hoverLand = p2BuyLandBtn.contains(mousePos);
            sf::RectangleShape landBtn(p2BuyLandBtn.size);
            landBtn.setPosition(p2BuyLandBtn.position);
            landBtn.setFillColor(hoverLand ? sf::Color(85, 35, 80) : sf::Color(45, 20, 40));
            landBtn.setOutlineThickness(1.0f);
            landBtn.setOutlineColor(hoverLand ? sf::Color(255, 215, 0) : sf::Color(255, 120, 200));
            window.draw(landBtn);

            sf::Text tLand(font, toUtf8("+ КУПИ ЗЕМЯ"), 12);
            tLand.setFillColor(hoverLand ? sf::Color(255, 240, 150) : sf::Color::White);
            sf::FloatRect tb = tLand.getLocalBounds();
            tLand.setPosition({ p2BuyLandBtn.position.x + (p2BuyLandBtn.size.x - tb.size.x) / 2.0f, screenHeight - 44.0f });
            window.draw(tLand);
        }
    }
}
