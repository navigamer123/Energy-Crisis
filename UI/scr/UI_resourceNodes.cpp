#include "../includes/UI_resourceNodes.h"
#include "../includes/UI_types.h"
#include <cmath>
#include <string>

UI_resourceNodes::UI_resourceNodes() {
}

bool UI_resourceNodes::isNearP1Forest(sf::Vector2f pt) const {
    return getP1ForestBounds().contains(pt);
}

bool UI_resourceNodes::isNearP1Mine(sf::Vector2f pt) const {
    return getP1MineBounds().contains(pt);
}

bool UI_resourceNodes::isNearP2Mine(sf::Vector2f pt) const {
    return getP2MineBounds().contains(pt);
}

bool UI_resourceNodes::isNearP2Forest(sf::Vector2f pt) const {
    return getP2ForestBounds().contains(pt);
}

void UI_resourceNodes::drawLandPlots(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                                     const std::vector<LandPlot>& plots, sf::Vector2f mousePos) {
    for (const auto& plot : plots) {
        bool hover = plot.bounds.contains(mousePos);
        sf::Color ownerAccent = (plot.playerOwner == 1) ? sf::Color(0, 220, 255) : sf::Color(255, 120, 200);

        sf::RectangleShape box(plot.bounds.size);
        box.setPosition(plot.bounds.position);

        if (plot.isPurchased) {
            box.setFillColor(plot.playerOwner == 1 ? sf::Color(22, 38, 30, 160) : sf::Color(36, 26, 36, 160));
            box.setOutlineThickness(2.0f);
            box.setOutlineColor(ownerAccent);
            window.draw(box);

            // Boundary posts on corners
            sf::CircleShape post(3.0f);
            post.setOrigin({ 3.0f, 3.0f });
            post.setFillColor(sf::Color(255, 215, 0));
            for (float px : { plot.bounds.position.x, plot.bounds.position.x + plot.bounds.size.x }) {
                for (float py : { plot.bounds.position.y, plot.bounds.position.y + plot.bounds.size.y }) {
                    post.setPosition({ px, py });
                    window.draw(post);
                }
            }

            if (fontLoaded) {
                std::string tag = (plot.playerOwner == 1) ? "КУПЕНА ЗЕМЯ (P1)" : "КУПЕНА ЗЕМЯ (P2)";
                sf::Text t(font, toUtf8(tag), 10);
                t.setFillColor(ownerAccent);
                t.setPosition({ plot.bounds.position.x + 6.0f, plot.bounds.position.y + 4.0f });
                window.draw(t);
            }
        } else {
            // Unpurchased, available for purchase!
            box.setFillColor(hover ? sf::Color(45, 55, 35, 190) : sf::Color(20, 25, 22, 160));
            box.setOutlineThickness(hover ? 2.5f : 1.0f);
            box.setOutlineColor(hover ? sf::Color(255, 215, 0) : sf::Color(100, 120, 90));
            window.draw(box);

            if (fontLoaded) {
                sf::Text t(font, toUtf8("+ КУПИ ЗЕМЯ"), 11);
                t.setFillColor(hover ? sf::Color(255, 240, 150) : sf::Color(180, 205, 160));
                sf::FloatRect tb = t.getLocalBounds();
                t.setPosition({ plot.bounds.position.x + (plot.bounds.size.x - tb.size.x) / 2.0f, plot.bounds.position.y + 28.0f });
                window.draw(t);

                std::string cStr = std::to_string(plot.costGold) + " G";
                sf::Text tCost(font, toUtf8(cStr), 13);
                tCost.setFillColor(sf::Color(255, 215, 0));
                sf::FloatRect cb = tCost.getLocalBounds();
                tCost.setPosition({ plot.bounds.position.x + (plot.bounds.size.x - cb.size.x) / 2.0f, plot.bounds.position.y + 48.0f });
                window.draw(tCost);
            }
        }
    }
}

void UI_resourceNodes::drawPlacedBuildings(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                                          const std::vector<PlacedBuilding>& buildings) {
    for (const auto& b : buildings) {
        sf::Color ownerColor = (b.playerOwner == 1) ? sf::Color(0, 229, 255) : sf::Color(255, 120, 200);

        if (b.type == BuildingType::SOLAR_PANEL) {
            // Solar panel base
            sf::RectangleShape frame({ 36.0f, 26.0f });
            frame.setOrigin({ 18.0f, 13.0f });
            frame.setPosition(b.position);
            frame.setFillColor(sf::Color(25, 35, 55));
            frame.setOutlineThickness(1.5f);
            frame.setOutlineColor(ownerColor);
            window.draw(frame);

            // Blue cells
            for (int r = 0; r < 2; r++) {
                for (int c = 0; c < 3; c++) {
                    sf::RectangleShape cell({ 8.0f, 8.0f });
                    cell.setPosition({ b.position.x - 14.0f + c * 10.0f, b.position.y - 10.0f + r * 10.0f });
                    cell.setFillColor(sf::Color(30, 90, 160));
                    window.draw(cell);
                }
            }
        } else if (b.type == BuildingType::WIND_TURBINE) {
            // Mast
            sf::RectangleShape mast({ 4.0f, 28.0f });
            mast.setOrigin({ 2.0f, 28.0f });
            mast.setPosition(b.position);
            mast.setFillColor(sf::Color(200, 215, 230));
            window.draw(mast);

            // Hub
            sf::CircleShape hub(4.0f);
            hub.setOrigin({ 4.0f, 4.0f });
            hub.setPosition({ b.position.x, b.position.y - 28.0f });
            hub.setFillColor(ownerColor);
            window.draw(hub);

            // Rotating blades
            float angle = b.animTimer * 180.0f;
            for (int i = 0; i < 3; i++) {
                sf::RectangleShape blade({ 16.0f, 2.5f });
                blade.setOrigin({ 0.0f, 1.25f });
                blade.setPosition({ b.position.x, b.position.y - 28.0f });
                blade.setRotation(sf::degrees(angle + i * 120.0f));
                blade.setFillColor(sf::Color::White);
                window.draw(blade);
            }
        } else if (b.type == BuildingType::HYDRO_PLANT) {
            // Station box
            sf::RectangleShape station({ 38.0f, 30.0f });
            station.setOrigin({ 19.0f, 15.0f });
            station.setPosition(b.position);
            station.setFillColor(sf::Color(30, 48, 70));
            station.setOutlineThickness(1.5f);
            station.setOutlineColor(ownerColor);
            window.draw(station);

            // Water intake animation
            float wave = std::sin(b.animTimer * 6.0f) * 3.0f;
            sf::RectangleShape intake({ 14.0f, 6.0f });
            intake.setOrigin({ 7.0f, 3.0f });
            intake.setPosition({ b.position.x, b.position.y + wave });
            intake.setFillColor(sf::Color(80, 200, 255));
            window.draw(intake);
        } else if (b.type == BuildingType::BATTERY) {
            // Battery container
            sf::RectangleShape box({ 26.0f, 32.0f });
            box.setOrigin({ 13.0f, 16.0f });
            box.setPosition(b.position);
            box.setFillColor(sf::Color(25, 30, 38));
            box.setOutlineThickness(1.5f);
            box.setOutlineColor(ownerColor);
            window.draw(box);

            // Terminal
            sf::RectangleShape term({ 8.0f, 4.0f });
            term.setOrigin({ 4.0f, 4.0f });
            term.setPosition({ b.position.x, b.position.y - 16.0f });
            term.setFillColor(sf::Color(255, 215, 0));
            window.draw(term);

            // Glowing charge level
            sf::RectangleShape charge({ 18.0f, 18.0f });
            charge.setOrigin({ 9.0f, 9.0f });
            charge.setPosition(b.position);
            charge.setFillColor(sf::Color(0, 255, 120, 200));
            window.draw(charge);
        }

        // Power Output label
        if (fontLoaded) {
            std::string pStr = "+" + std::to_string(static_cast<int>(b.currentOutputMW)) + " MW";
            sf::Text t(font, toUtf8(pStr), 10);
            t.setFillColor(sf::Color(255, 215, 0));
            sf::FloatRect tb = t.getLocalBounds();
            t.setPosition({ b.position.x - tb.size.x / 2.0f, b.position.y + 16.0f });
            window.draw(t);
        }
    }
}

void UI_resourceNodes::drawBuildingGhost(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                                        BuildingType type, sf::Vector2f pos, bool isValidPlacement,
                                        const BuildingCost& cost) {
    if (type == BuildingType::NONE) return;

    sf::Color tint = isValidPlacement ? sf::Color(0, 255, 150, 180) : sf::Color(255, 60, 60, 200);

    sf::RectangleShape ghost({ 40.0f, 40.0f });
    ghost.setOrigin({ 20.0f, 20.0f });
    ghost.setPosition(pos);
    ghost.setFillColor(isValidPlacement ? sf::Color(0, 255, 150, 60) : sf::Color(255, 60, 60, 80));
    ghost.setOutlineThickness(2.5f);
    ghost.setOutlineColor(tint);
    window.draw(ghost);

    if (fontLoaded) {
        std::string label = cost.nameBg + (isValidPlacement ? " [ПОСТАВИ]" : " [НЕДОПУСТИМО]");
        sf::Text t(font, toUtf8(label), 12);
        t.setFillColor(tint);
        sf::FloatRect tb = t.getLocalBounds();
        t.setPosition({ pos.x - tb.size.x / 2.0f, pos.y - 36.0f });
        window.draw(t);

        std::string costStr = "Нужно: " + std::to_string(cost.woodCost) + " Дърво, " + std::to_string(cost.oreCost) + " Руда";
        sf::Text tc(font, toUtf8(costStr), 10);
        tc.setFillColor(sf::Color::White);
        sf::FloatRect tcb = tc.getLocalBounds();
        tc.setPosition({ pos.x - tcb.size.x / 2.0f, pos.y + 24.0f });
        window.draw(tc);

        std::string hint = isValidPlacement ? "[SPACE/КЛИК]: Постави  |  [Q]: Отказ  |  [E]: Смени" 
                                            : "[Q]: Отказ  |  [E]: Смени сграда";
        sf::Text th(font, toUtf8(hint), 10);
        th.setFillColor(isValidPlacement ? sf::Color(255, 230, 100) : sf::Color(255, 130, 130));
        sf::FloatRect thb = th.getLocalBounds();
        th.setPosition({ pos.x - thb.size.x / 2.0f, pos.y + 38.0f });
        window.draw(th);
    }
}

void UI_resourceNodes::drawNodes(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded) {
    // -------------------------------------------------------------------------
    // Helper to draw a stylized Ore Mine (Мина)
    // -------------------------------------------------------------------------
    auto drawMine = [&](float x, float y, const std::string& label, sf::Color accent) {
        sf::RectangleShape cavern({ 160.0f, 95.0f });
        cavern.setPosition({ x, y });
        cavern.setFillColor(sf::Color(25, 30, 42, 245));
        cavern.setOutlineThickness(2.0f);
        cavern.setOutlineColor(accent);
        window.draw(cavern);

        sf::RectangleShape entry({ 66.0f, 54.0f });
        entry.setPosition({ x + 47.0f, y + 41.0f });
        entry.setFillColor(sf::Color(10, 12, 18));
        entry.setOutlineThickness(2.0f);
        entry.setOutlineColor(sf::Color(120, 100, 70));
        window.draw(entry);

        sf::RectangleShape beamH({ 76.0f, 6.0f });
        beamH.setPosition({ x + 42.0f, y + 38.0f });
        beamH.setFillColor(sf::Color(140, 95, 50));
        window.draw(beamH);

        sf::RectangleShape cart({ 32.0f, 18.0f });
        cart.setPosition({ x + 64.0f, y + 68.0f });
        cart.setFillColor(sf::Color(80, 85, 95));
        window.draw(cart);

        sf::CircleShape ore1(4.0f);
        ore1.setPosition({ x + 70.0f, y + 64.0f });
        ore1.setFillColor(accent);
        window.draw(ore1);

        sf::CircleShape ore2(3.5f);
        ore2.setPosition({ x + 82.0f, y + 63.0f });
        ore2.setFillColor(sf::Color(255, 215, 0));
        window.draw(ore2);

        if (fontLoaded) {
            sf::Text tag(font, toUtf8(label), 13);
            tag.setFillColor(accent);
            tag.setPosition({ x + 10.0f, y + 8.0f });
            window.draw(tag);

            sf::Text act(font, toUtf8("[Клик за Добив на Руда]"), 11);
            act.setFillColor(sf::Color(255, 235, 120));
            act.setPosition({ x + 10.0f, y + 25.0f });
            window.draw(act);
        }
    };

    // -------------------------------------------------------------------------
    // Helper to draw a stylized Timber Forest (Гора)
    // -------------------------------------------------------------------------
    auto drawForest = [&](float x, float y, const std::string& label, sf::Color accent) {
        sf::RectangleShape forestArea({ 160.0f, 105.0f });
        forestArea.setPosition({ x, y });
        forestArea.setFillColor(sf::Color(22, 38, 26, 235));
        forestArea.setOutlineThickness(2.0f);
        forestArea.setOutlineColor(accent);
        window.draw(forestArea);

        struct Tree { float ox, oy, scale; };
        Tree trees[] = {
            { 18.0f, 26.0f, 1.0f },
            { 50.0f, 22.0f, 1.2f },
            { 90.0f, 26.0f, 1.05f },
            { 32.0f, 54.0f, 1.1f },
            { 70.0f, 50.0f, 1.25f },
            { 110.0f, 54.0f, 0.95f }
        };

        for (const auto& t : trees) {
            float tx = x + t.ox;
            float ty = y + t.oy;

            sf::RectangleShape trunk({ 4.0f * t.scale, 9.0f * t.scale });
            trunk.setPosition({ tx + 6.0f * t.scale, ty + 18.0f * t.scale });
            trunk.setFillColor(sf::Color(90, 60, 35));
            window.draw(trunk);

            sf::ConvexShape pine(3);
            pine.setPoint(0, { tx + 8.0f * t.scale, ty });
            pine.setPoint(1, { tx, ty + 16.0f * t.scale });
            pine.setPoint(2, { tx + 16.0f * t.scale, ty + 16.0f * t.scale });
            pine.setFillColor(sf::Color(40, 130, 60));
            window.draw(pine);

            sf::ConvexShape pineTop(3);
            pineTop.setPoint(0, { tx + 8.0f * t.scale, ty - 4.0f * t.scale });
            pineTop.setPoint(1, { tx + 2.0f * t.scale, ty + 9.0f * t.scale });
            pineTop.setPoint(2, { tx + 14.0f * t.scale, ty + 9.0f * t.scale });
            pineTop.setFillColor(sf::Color(60, 165, 80));
            window.draw(pineTop);
        }

        sf::RectangleShape log1({ 18.0f, 5.0f });
        log1.setPosition({ x + 125.0f, y + 84.0f });
        log1.setFillColor(sf::Color(140, 90, 50));
        window.draw(log1);

        sf::RectangleShape log2({ 18.0f, 5.0f });
        log2.setPosition({ x + 127.0f, y + 76.0f });
        log2.setFillColor(sf::Color(160, 105, 60));
        window.draw(log2);

        if (fontLoaded) {
            sf::Text tag(font, toUtf8(label), 13);
            tag.setFillColor(accent);
            tag.setPosition({ x + 10.0f, y + 8.0f });
            window.draw(tag);

            sf::Text act(font, toUtf8("[Клик за Дървен Материал]"), 11);
            act.setFillColor(sf::Color(255, 235, 120));
            act.setPosition({ x + 10.0f, y + 25.0f });
            window.draw(act);
        }
    };

    // -------------------------------------------------------------------------
    // Order strictly matching user diagram:
    // Left-to-right: [ГОРА P1] [МИНА P1] | [МИНА P2] [ГОРА P2]
    // -------------------------------------------------------------------------
    // P1 (West Sector)
    drawForest(320.0f, 620.0f, "ГОРА: ДЪРВО (+20)", sf::Color(100, 255, 140));
    drawMine(540.0f, 620.0f, "МИНА: РУДА (+15)", sf::Color(0, 229, 255));

    // P2 (East Sector)
    drawMine(900.0f, 620.0f, "МИНА: РУДА (+15)", sf::Color(255, 140, 210));
    drawForest(1120.0f, 620.0f, "ГОРА: ДЪРВО (+20)", sf::Color(255, 204, 100));
}
