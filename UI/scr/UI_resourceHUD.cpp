#include "../includes/UI_resourceHUD.h"
#include "../includes/UI_types.h"
#include <cmath>
#include <string>

UI_resourceHUD::UI_resourceHUD()
    : p1BuyLandBtn({ 14.0f, 852.0f }, { 190.0f, 28.0f }),
      p2BuyLandBtn({ 1600.0f - 204.0f, 852.0f }, { 190.0f, 28.0f }) {
}

void UI_resourceHUD::drawQuarterCircle(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                                       const PlayerEconomy& econ, bool isWest) {
    float screenWidth = VIRTUAL_WIDTH;
    float screenHeight = VIRTUAL_HEIGHT;
    float R = 250.0f; // Radius of the corner quarter-circle

    // -------------------------------------------------------------------------
    // Quarter circle geometry
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
    // Mini Vector Icon Drawing Helpers
    // -------------------------------------------------------------------------
    auto drawIcon = [&](ResourceType type, float x, float y) {
        if (type == ResourceType::WOOD) {
            sf::RectangleShape log({ 14.0f, 7.0f });
            log.setPosition({ x + 2.0f, y + 3.0f });
            log.setFillColor(sf::Color(140, 90, 50));
            window.draw(log);
            sf::CircleShape leaf(3.0f, 3);
            leaf.setPosition({ x + 10.0f, y });
            leaf.setFillColor(sf::Color(70, 220, 90));
            window.draw(leaf);
        } else if (type == ResourceType::IRON) {
            sf::RectangleShape bar({ 12.0f, 8.0f });
            bar.setPosition({ x + 2.0f, y + 2.0f });
            bar.setFillColor(sf::Color(170, 190, 215));
            window.draw(bar);
        } else if (type == ResourceType::COPPER) {
            sf::CircleShape coil(5.5f);
            coil.setPosition({ x + 2.0f, y + 2.0f });
            coil.setFillColor(sf::Color::Transparent);
            coil.setOutlineThickness(2.0f);
            coil.setOutlineColor(sf::Color(235, 140, 70));
            window.draw(coil);
        } else if (type == ResourceType::COAL) {
            sf::CircleShape lump(5.5f, 5);
            lump.setPosition({ x + 2.0f, y + 2.0f });
            lump.setFillColor(sf::Color(100, 105, 115));
            window.draw(lump);
        } else if (type == ResourceType::SILICON) {
            sf::ConvexShape gem(4);
            gem.setPoint(0, { x + 7.0f, y });
            gem.setPoint(1, { x + 13.0f, y + 6.0f });
            gem.setPoint(2, { x + 7.0f, y + 12.0f });
            gem.setPoint(3, { x + 1.0f, y + 6.0f });
            gem.setFillColor(sf::Color(0, 220, 255));
            window.draw(gem);
        } else if (type == ResourceType::SILVER) {
            sf::RectangleShape bar({ 13.0f, 7.0f });
            bar.setPosition({ x + 1.0f, y + 3.0f });
            bar.setFillColor(sf::Color(225, 235, 245));
            window.draw(bar);
        } else if (type == ResourceType::GOLD) {
            sf::CircleShape coin(6.0f);
            coin.setPosition({ x + 2.0f, y + 2.0f });
            coin.setFillColor(sf::Color(255, 215, 0));
            window.draw(coin);
        } else if (type == ResourceType::MONEY) {
            sf::RectangleShape note({ 13.0f, 8.0f });
            note.setPosition({ x + 1.0f, y + 2.0f });
            note.setFillColor(sf::Color(70, 220, 130));
            window.draw(note);
        } else if (type == ResourceType::ENERGY) {
            sf::ConvexShape bolt(6);
            bolt.setPoint(0, { x + 8.0f, y });
            bolt.setPoint(1, { x + 2.0f, y + 6.0f });
            bolt.setPoint(2, { x + 7.0f, y + 6.0f });
            bolt.setPoint(3, { x + 5.0f, y + 13.0f });
            bolt.setPoint(4, { x + 13.0f, y + 5.0f });
            bolt.setPoint(5, { x + 8.0f, y + 5.0f });
            bolt.setFillColor(sf::Color(255, 225, 40));
            window.draw(bolt);
        }
    };

    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    if (fontLoaded) {
        if (isWest) {
            // Player 1 (West Corner)
            // Left Column (Wood, Iron, Copper, Coal)
            float col1X = 18.0f;
            float col2X = 96.0f;
            float row0Y = screenHeight - 215.0f;
            float row1Y = screenHeight - 185.0f;
            float row2Y = screenHeight - 155.0f;
            float row3Y = screenHeight - 125.0f;

            // Wood
            drawIcon(ResourceType::WOOD, col1X, row0Y);
            sf::Text tWood(font, std::to_string(econ.wood), 14);
            tWood.setFillColor(sf::Color(140, 255, 170));
            tWood.setPosition({ col1X + 20.0f, row0Y - 2.0f });
            window.draw(tWood);

            // Iron
            drawIcon(ResourceType::IRON, col1X, row1Y);
            sf::Text tIron(font, std::to_string(econ.iron), 14);
            tIron.setFillColor(sf::Color(170, 210, 245));
            tIron.setPosition({ col1X + 20.0f, row1Y - 2.0f });
            window.draw(tIron);

            // Copper
            drawIcon(ResourceType::COPPER, col1X, row2Y);
            sf::Text tCopper(font, std::to_string(econ.copper), 14);
            tCopper.setFillColor(sf::Color(255, 170, 100));
            tCopper.setPosition({ col1X + 20.0f, row2Y - 2.0f });
            window.draw(tCopper);

            // Coal
            drawIcon(ResourceType::COAL, col1X, row3Y);
            sf::Text tCoal(font, std::to_string(econ.coal), 14);
            tCoal.setFillColor(sf::Color(180, 185, 195));
            tCoal.setPosition({ col1X + 20.0f, row3Y - 2.0f });
            window.draw(tCoal);

            // Right Column (Silicon, Silver, Gold, Money)
            // Silicon
            drawIcon(ResourceType::SILICON, col2X, row0Y);
            sf::Text tSilicon(font, std::to_string(econ.silicon), 14);
            tSilicon.setFillColor(sf::Color(0, 230, 255));
            tSilicon.setPosition({ col2X + 20.0f, row0Y - 2.0f });
            window.draw(tSilicon);

            // Silver
            drawIcon(ResourceType::SILVER, col2X, row1Y);
            sf::Text tSilver(font, std::to_string(econ.silver), 14);
            tSilver.setFillColor(sf::Color(230, 240, 250));
            tSilver.setPosition({ col2X + 20.0f, row1Y - 2.0f });
            window.draw(tSilver);

            // Gold
            drawIcon(ResourceType::GOLD, col2X, row2Y);
            sf::Text tGold(font, std::to_string(econ.gold) + "G", 14);
            tGold.setFillColor(sf::Color(255, 215, 0));
            tGold.setPosition({ col2X + 20.0f, row2Y - 2.0f });
            window.draw(tGold);

            // Money
            drawIcon(ResourceType::MONEY, col2X, row3Y);
            sf::Text tMoney(font, std::to_string(econ.money) + "$", 14);
            tMoney.setFillColor(sf::Color(80, 255, 160));
            tMoney.setPosition({ col2X + 20.0f, row3Y - 2.0f });
            window.draw(tMoney);

            // Energy & Share Banner
            sf::RectangleShape energyPlaque({ 186.0f, 26.0f });
            energyPlaque.setPosition({ 14.0f, screenHeight - 88.0f });
            energyPlaque.setFillColor(sf::Color(25, 36, 52, 230));
            energyPlaque.setOutlineThickness(1.0f);
            energyPlaque.setOutlineColor(sf::Color(0, 229, 255));
            window.draw(energyPlaque);

            drawIcon(ResourceType::ENERGY, 18.0f, screenHeight - 82.0f);
            int p1SharePct = static_cast<int>(econ.cityInfluence * 100.0f);
            std::string pStr = std::to_string(econ.energyMW) + " MW (" + std::to_string(p1SharePct) + "% ток)";
            sf::Text tPwr(font, toUtf8(pStr), 12);
            tPwr.setFillColor(sf::Color(255, 235, 100));
            tPwr.setPosition({ 40.0f, screenHeight - 83.0f });
            window.draw(tPwr);

            // Land Expansion Button
            p1BuyLandBtn = sf::FloatRect({ 14.0f, screenHeight - 52.0f }, { 186.0f, 26.0f });
            bool hoverLand = p1BuyLandBtn.contains(mousePos);
            sf::RectangleShape landBtn(p1BuyLandBtn.size);
            landBtn.setPosition(p1BuyLandBtn.position);
            landBtn.setFillColor(hoverLand ? sf::Color(35, 85, 115) : sf::Color(18, 40, 60));
            landBtn.setOutlineThickness(1.0f);
            landBtn.setOutlineColor(hoverLand ? sf::Color(255, 215, 0) : sf::Color(0, 200, 255));
            window.draw(landBtn);

            sf::Text tLand(font, toUtf8("+ КУПИ ЗЕМЯ"), 11);
            tLand.setFillColor(hoverLand ? sf::Color(255, 240, 150) : sf::Color::White);
            sf::FloatRect tb = tLand.getLocalBounds();
            tLand.setPosition({ p1BuyLandBtn.position.x + (p1BuyLandBtn.size.x - tb.size.x) / 2.0f, screenHeight - 47.0f });
            window.draw(tLand);
        } else {
            // Player 2 (East Corner)
            float col1X = screenWidth - 190.0f;
            float col2X = screenWidth - 105.0f;
            float row0Y = screenHeight - 215.0f;
            float row1Y = screenHeight - 185.0f;
            float row2Y = screenHeight - 155.0f;
            float row3Y = screenHeight - 125.0f;

            // Wood
            drawIcon(ResourceType::WOOD, col1X, row0Y);
            sf::Text tWood(font, std::to_string(econ.wood), 14);
            tWood.setFillColor(sf::Color(140, 255, 170));
            tWood.setPosition({ col1X + 20.0f, row0Y - 2.0f });
            window.draw(tWood);

            // Iron
            drawIcon(ResourceType::IRON, col1X, row1Y);
            sf::Text tIron(font, std::to_string(econ.iron), 14);
            tIron.setFillColor(sf::Color(170, 210, 245));
            tIron.setPosition({ col1X + 20.0f, row1Y - 2.0f });
            window.draw(tIron);

            // Copper
            drawIcon(ResourceType::COPPER, col1X, row2Y);
            sf::Text tCopper(font, std::to_string(econ.copper), 14);
            tCopper.setFillColor(sf::Color(255, 170, 100));
            tCopper.setPosition({ col1X + 20.0f, row2Y - 2.0f });
            window.draw(tCopper);

            // Coal
            drawIcon(ResourceType::COAL, col1X, row3Y);
            sf::Text tCoal(font, std::to_string(econ.coal), 14);
            tCoal.setFillColor(sf::Color(180, 185, 195));
            tCoal.setPosition({ col1X + 20.0f, row3Y - 2.0f });
            window.draw(tCoal);

            // Silicon
            drawIcon(ResourceType::SILICON, col2X, row0Y);
            sf::Text tSilicon(font, std::to_string(econ.silicon), 14);
            tSilicon.setFillColor(sf::Color(0, 230, 255));
            tSilicon.setPosition({ col2X + 20.0f, row0Y - 2.0f });
            window.draw(tSilicon);

            // Silver
            drawIcon(ResourceType::SILVER, col2X, row1Y);
            sf::Text tSilver(font, std::to_string(econ.silver), 14);
            tSilver.setFillColor(sf::Color(230, 240, 250));
            tSilver.setPosition({ col2X + 20.0f, row1Y - 2.0f });
            window.draw(tSilver);

            // Gold
            drawIcon(ResourceType::GOLD, col2X, row2Y);
            sf::Text tGold(font, std::to_string(econ.gold) + "G", 14);
            tGold.setFillColor(sf::Color(255, 215, 0));
            tGold.setPosition({ col2X + 20.0f, row2Y - 2.0f });
            window.draw(tGold);

            // Money
            drawIcon(ResourceType::MONEY, col2X, row3Y);
            sf::Text tMoney(font, std::to_string(econ.money) + "$", 14);
            tMoney.setFillColor(sf::Color(80, 255, 160));
            tMoney.setPosition({ col2X + 20.0f, row3Y - 2.0f });
            window.draw(tMoney);

            // Energy & Share Banner
            sf::RectangleShape energyPlaque({ 186.0f, 26.0f });
            energyPlaque.setPosition({ screenWidth - 200.0f, screenHeight - 88.0f });
            energyPlaque.setFillColor(sf::Color(42, 25, 45, 230));
            energyPlaque.setOutlineThickness(1.0f);
            energyPlaque.setOutlineColor(sf::Color(255, 120, 200));
            window.draw(energyPlaque);

            drawIcon(ResourceType::ENERGY, screenWidth - 196.0f, screenHeight - 82.0f);
            int p2SharePct = static_cast<int>(econ.cityInfluence * 100.0f);
            std::string pStr = std::to_string(econ.energyMW) + " MW (" + std::to_string(p2SharePct) + "% ток)";
            sf::Text tPwr(font, toUtf8(pStr), 12);
            tPwr.setFillColor(sf::Color(255, 235, 100));
            tPwr.setPosition({ screenWidth - 174.0f, screenHeight - 83.0f });
            window.draw(tPwr);

            // Land Expansion Button
            p2BuyLandBtn = sf::FloatRect({ screenWidth - 200.0f, screenHeight - 52.0f }, { 186.0f, 26.0f });
            bool hoverLand = p2BuyLandBtn.contains(mousePos);
            sf::RectangleShape landBtn(p2BuyLandBtn.size);
            landBtn.setPosition(p2BuyLandBtn.position);
            landBtn.setFillColor(hoverLand ? sf::Color(85, 35, 80) : sf::Color(45, 20, 40));
            landBtn.setOutlineThickness(1.0f);
            landBtn.setOutlineColor(hoverLand ? sf::Color(255, 215, 0) : sf::Color(255, 120, 200));
            window.draw(landBtn);

            sf::Text tLand(font, toUtf8("+ КУПИ ЗЕМЯ"), 11);
            tLand.setFillColor(hoverLand ? sf::Color(255, 240, 150) : sf::Color::White);
            sf::FloatRect tb = tLand.getLocalBounds();
            tLand.setPosition({ p2BuyLandBtn.position.x + (p2BuyLandBtn.size.x - tb.size.x) / 2.0f, screenHeight - 47.0f });
            window.draw(tLand);
        }
    }
}
