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

            // 2x2 Building Placement Grid Dividers & Slot Crosshairs
            float midX = plot.bounds.position.x + plot.bounds.size.x * 0.5f;
            float midY = plot.bounds.position.y + plot.bounds.size.y * 0.5f;

            // Horizontal grid divider
            sf::RectangleShape hLine({ plot.bounds.size.x - 8.0f, 1.0f });
            hLine.setPosition({ plot.bounds.position.x + 4.0f, midY });
            hLine.setFillColor(sf::Color(ownerAccent.r, ownerAccent.g, ownerAccent.b, 65));
            window.draw(hLine);

            // Vertical grid divider
            sf::RectangleShape vLine({ 1.0f, plot.bounds.size.y - 8.0f });
            vLine.setPosition({ midX, plot.bounds.position.y + 4.0f });
            vLine.setFillColor(sf::Color(ownerAccent.r, ownerAccent.g, ownerAccent.b, 65));
            window.draw(vLine);

            // Slot center crosshairs '+'
            float colW = plot.bounds.size.x * 0.5f;
            float rowH = plot.bounds.size.y * 0.5f;
            for (int r = 0; r < 2; r++) {
                for (int c = 0; c < 2; c++) {
                    float cx = plot.bounds.position.x + (c + 0.5f) * colW;
                    float cy = plot.bounds.position.y + (r + 0.5f) * rowH;

                    sf::RectangleShape crossH({ 6.0f, 1.0f });
                    crossH.setOrigin({ 3.0f, 0.5f });
                    crossH.setPosition({ cx, cy });
                    crossH.setFillColor(sf::Color(ownerAccent.r, ownerAccent.g, ownerAccent.b, 75));
                    window.draw(crossH);

                    sf::RectangleShape crossV({ 1.0f, 6.0f });
                    crossV.setOrigin({ 0.5f, 3.0f });
                    crossV.setPosition({ cx, cy });
                    crossV.setFillColor(sf::Color(ownerAccent.r, ownerAccent.g, ownerAccent.b, 75));
                    window.draw(crossV);
                }
            }

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
            // Battery outer casing
            sf::RectangleShape box({ 26.0f, 34.0f });
            box.setOrigin({ 13.0f, 17.0f });
            box.setPosition(b.position);
            box.setFillColor(sf::Color(20, 26, 36));
            box.setOutlineThickness(1.5f);
            box.setOutlineColor(ownerColor);
            window.draw(box);

            // Positive terminal on top
            sf::RectangleShape term({ 10.0f, 4.0f });
            term.setOrigin({ 5.0f, 4.0f });
            term.setPosition({ b.position.x, b.position.y - 17.0f });
            term.setFillColor(sf::Color(255, 215, 0));
            window.draw(term);

            // Dark inner glass chamber
            sf::RectangleShape glass({ 18.0f, 24.0f });
            glass.setOrigin({ 9.0f, 12.0f });
            glass.setPosition({ b.position.x, b.position.y + 1.0f });
            glass.setFillColor(sf::Color(10, 15, 20));
            glass.setOutlineThickness(1.0f);
            glass.setOutlineColor(sf::Color(60, 75, 95));
            window.draw(glass);

            // Dynamic fluid fill level
            float pct = std::max(0.0f, std::min(1.0f, b.energyStored / b.maxCapacity));
            float fluidH = 22.0f * pct;
            if (fluidH > 1.0f) {
                sf::Color fluidColor = (pct > 0.5f) ? sf::Color(0, 255, 160) :
                                      ((pct > 0.2f) ? sf::Color(255, 210, 40) : sf::Color(255, 75, 75));
                sf::RectangleShape fluid({ 16.0f, fluidH });
                fluid.setPosition({ b.position.x - 8.0f, b.position.y + 12.0f - fluidH });
                fluid.setFillColor(fluidColor);
                window.draw(fluid);
            }

            // Segment tick marks
            for (int seg = 1; seg <= 3; seg++) {
                sf::RectangleShape tick({ 16.0f, 1.0f });
                tick.setPosition({ b.position.x - 8.0f, b.position.y - 10.0f + seg * 5.5f });
                tick.setFillColor(sf::Color(40, 55, 75, 180));
                window.draw(tick);
            }
        } else if (b.type == BuildingType::LAMP) {
            bool isPowered = (b.lightRadius > 0.0f);

            if (isPowered) {
                // Illuminated light circle on ground
                sf::CircleShape lightGlow(b.lightRadius);
                lightGlow.setOrigin({ b.lightRadius, b.lightRadius });
                lightGlow.setPosition(b.position);
                lightGlow.setFillColor(sf::Color(255, 235, 140, 38));
                lightGlow.setOutlineThickness(1.5f);
                lightGlow.setOutlineColor(sf::Color(255, 220, 100, 90));
                window.draw(lightGlow);
            }

            // Base pedestal
            sf::CircleShape base(6.0f);
            base.setOrigin({ 6.0f, 6.0f });
            base.setPosition(b.position);
            base.setFillColor(sf::Color(35, 42, 54));
            base.setOutlineThickness(1.0f);
            base.setOutlineColor(ownerColor);
            window.draw(base);

            // Pole
            sf::RectangleShape pole({ 3.0f, 26.0f });
            pole.setOrigin({ 1.5f, 26.0f });
            pole.setPosition(b.position);
            pole.setFillColor(sf::Color(180, 195, 215));
            window.draw(pole);

            // Lantern head
            sf::CircleShape lantern(7.0f);
            lantern.setOrigin({ 7.0f, 7.0f });
            lantern.setPosition({ b.position.x, b.position.y - 26.0f });
            if (isPowered) {
                lantern.setFillColor(sf::Color(255, 235, 120));
                lantern.setOutlineThickness(2.0f);
                lantern.setOutlineColor(sf::Color::White);
            } else {
                // Unpowered lamp: dark gray head, no illumination
                lantern.setFillColor(sf::Color(65, 70, 80));
                lantern.setOutlineThickness(1.5f);
                lantern.setOutlineColor(sf::Color(120, 125, 135));
            }
            window.draw(lantern);
        }

        // Crisp, high-contrast labels without overlapping clutter
        if (fontLoaded) {
            if (b.type == BuildingType::BATTERY) {
                int pct = static_cast<int>((b.energyStored / b.maxCapacity) * 100.0f);
                std::string bStr = std::to_string(pct) + "% (" + std::to_string(static_cast<int>(b.energyStored)) + "MWh)";
                sf::Text t(font, toUtf8(bStr), 9);
                t.setFillColor(pct > 0 ? sf::Color(160, 255, 200) : sf::Color(200, 200, 200));
                sf::FloatRect tb = t.getLocalBounds();

                sf::RectangleShape pill({ tb.size.x + 8.0f, 14.0f });
                pill.setOrigin({ (tb.size.x + 8.0f) / 2.0f, 7.0f });
                pill.setPosition({ b.position.x, b.position.y + 22.0f });
                pill.setFillColor(sf::Color(12, 16, 24, 235));
                pill.setOutlineThickness(1.0f);
                pill.setOutlineColor(pct > 0 ? sf::Color(60, 90, 120) : sf::Color(100, 100, 100));
                window.draw(pill);

                t.setPosition({ b.position.x - tb.size.x / 2.0f, b.position.y + 15.0f });
                window.draw(t);
            } else if (b.type == BuildingType::LAMP) {
                bool isPowered = (b.lightRadius > 0.0f);
                std::string lStr = isPowered ? "ЛАМПА (-10 MW)" : "БЕЗ ТОК (-10 MW)";
                sf::Text t(font, toUtf8(lStr), 9);
                t.setFillColor(isPowered ? sf::Color(255, 235, 120) : sf::Color(255, 95, 95));
                sf::FloatRect tb = t.getLocalBounds();

                sf::RectangleShape pill({ tb.size.x + 8.0f, 14.0f });
                pill.setOrigin({ (tb.size.x + 8.0f) / 2.0f, 7.0f });
                pill.setPosition({ b.position.x, b.position.y + 12.0f });
                pill.setFillColor(sf::Color(12, 16, 24, 235));
                pill.setOutlineThickness(1.0f);
                pill.setOutlineColor(isPowered ? sf::Color(110, 95, 40) : sf::Color(160, 45, 45));
                window.draw(pill);

                t.setPosition({ b.position.x - tb.size.x / 2.0f, b.position.y + 5.0f });
                window.draw(t);
            } else if (b.currentOutputMW > 0.0f) {
                std::string pStr = "+" + std::to_string(static_cast<int>(b.currentOutputMW)) + " MW";
                sf::Text t(font, toUtf8(pStr), 9);
                t.setFillColor(sf::Color(255, 220, 80));
                sf::FloatRect tb = t.getLocalBounds();

                sf::RectangleShape pill({ tb.size.x + 8.0f, 14.0f });
                pill.setOrigin({ (tb.size.x + 8.0f) / 2.0f, 7.0f });
                pill.setPosition({ b.position.x, b.position.y + 20.0f });
                pill.setFillColor(sf::Color(12, 16, 24, 235));
                pill.setOutlineThickness(1.0f);
                pill.setOutlineColor(sf::Color(70, 95, 130));
                window.draw(pill);

                t.setPosition({ b.position.x - tb.size.x / 2.0f, b.position.y + 13.0f });
                window.draw(t);
            }
        }
    }
}

void UI_resourceNodes::drawBuildingGhost(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                                        BuildingType type, sf::Vector2f pos, bool isValidPlacement,
                                        const BuildingCost& cost) {
    if (type == BuildingType::NONE) return;

    if (type == BuildingType::DEMOLISH) {
        sf::Color tint = isValidPlacement ? sf::Color(255, 80, 80, 220) : sf::Color(180, 180, 180, 160);
        sf::RectangleShape ghost({ 42.0f, 42.0f });
        ghost.setOrigin({ 21.0f, 21.0f });
        ghost.setPosition(pos);
        ghost.setFillColor(isValidPlacement ? sf::Color(255, 60, 60, 90) : sf::Color(100, 100, 100, 60));
        ghost.setOutlineThickness(2.5f);
        ghost.setOutlineColor(tint);
        window.draw(ghost);

        if (fontLoaded) {
            std::string label = isValidPlacement ? "ПРЕМАХНИ СГРАДА [КЛИК]" : "ИЗБЕРЕТЕ ВАША СГРАДА";
            sf::Text t(font, toUtf8(label), 12);
            t.setFillColor(tint);
            sf::FloatRect tb = t.getLocalBounds();
            t.setPosition({ pos.x - tb.size.x / 2.0f, pos.y - 34.0f });
            window.draw(t);

            std::string hint = isValidPlacement ? "Връща 50% от вложените ресурси" : "Посочете сграда за разрушаване";
            sf::Text th(font, toUtf8(hint), 10);
            th.setFillColor(sf::Color(255, 210, 210));
            sf::FloatRect thb = th.getLocalBounds();
            th.setPosition({ pos.x - thb.size.x / 2.0f, pos.y + 24.0f });
            window.draw(th);
        }
        return;
    }

    sf::Color tint = isValidPlacement ? sf::Color(0, 255, 180, 220) : sf::Color(255, 60, 60, 220);

    // 1. Grid Cell Snapping Reticle Frame
    sf::RectangleShape cellSlot({ 50.0f, 44.0f });
    cellSlot.setOrigin({ 25.0f, 22.0f });
    cellSlot.setPosition(pos);
    cellSlot.setFillColor(isValidPlacement ? sf::Color(0, 255, 180, 35) : sf::Color(255, 60, 60, 35));
    cellSlot.setOutlineThickness(1.5f);
    cellSlot.setOutlineColor(tint);
    window.draw(cellSlot);

    // Grid snap corner ticks
    float cw = 6.0f;
    for (float ox : { -25.0f, 25.0f }) {
        for (float oy : { -22.0f, 22.0f }) {
            sf::RectangleShape c1({ (ox < 0 ? cw : -cw), 2.0f });
            c1.setPosition({ pos.x + ox, pos.y + oy });
            c1.setFillColor(tint);
            window.draw(c1);

            sf::RectangleShape c2({ 2.0f, (oy < 0 ? cw : -cw) });
            c2.setPosition({ pos.x + ox, pos.y + oy });
            c2.setFillColor(tint);
            window.draw(c2);
        }
    }

    // 2. If placing a Lamp, show illumination coverage circle!
    if (type == BuildingType::LAMP) {
        sf::CircleShape lampCone(150.0f);
        lampCone.setOrigin({ 150.0f, 150.0f });
        lampCone.setPosition(pos);
        lampCone.setFillColor(isValidPlacement ? sf::Color(255, 235, 120, 28) : sf::Color(255, 80, 80, 20));
        lampCone.setOutlineThickness(1.5f);
        lampCone.setOutlineColor(isValidPlacement ? sf::Color(255, 220, 100, 120) : sf::Color(255, 80, 80, 100));
        window.draw(lampCone);
    }

    // 3. Mini holographic preview of the building
    if (type == BuildingType::SOLAR_PANEL) {
        sf::RectangleShape frame({ 34.0f, 24.0f });
        frame.setOrigin({ 17.0f, 12.0f });
        frame.setPosition(pos);
        frame.setFillColor(sf::Color(20, 35, 55, 180));
        frame.setOutlineThickness(1.2f);
        frame.setOutlineColor(tint);
        window.draw(frame);
    } else if (type == BuildingType::WIND_TURBINE) {
        sf::RectangleShape mast({ 4.0f, 24.0f });
        mast.setOrigin({ 2.0f, 24.0f });
        mast.setPosition(pos);
        mast.setFillColor(sf::Color(200, 220, 240, 180));
        window.draw(mast);

        sf::CircleShape hub(3.5f);
        hub.setOrigin({ 3.5f, 3.5f });
        hub.setPosition({ pos.x, pos.y - 24.0f });
        hub.setFillColor(tint);
        window.draw(hub);
    } else if (type == BuildingType::HYDRO_PLANT) {
        sf::RectangleShape station({ 34.0f, 26.0f });
        station.setOrigin({ 17.0f, 13.0f });
        station.setPosition(pos);
        station.setFillColor(sf::Color(25, 45, 65, 180));
        station.setOutlineThickness(1.2f);
        station.setOutlineColor(tint);
        window.draw(station);
    } else if (type == BuildingType::BATTERY) {
        sf::RectangleShape box({ 24.0f, 30.0f });
        box.setOrigin({ 12.0f, 15.0f });
        box.setPosition(pos);
        box.setFillColor(sf::Color(18, 24, 34, 180));
        box.setOutlineThickness(1.2f);
        box.setOutlineColor(tint);
        window.draw(box);
    } else if (type == BuildingType::LAMP) {
        sf::CircleShape lantern(6.0f);
        lantern.setOrigin({ 6.0f, 6.0f });
        lantern.setPosition({ pos.x, pos.y - 20.0f });
        lantern.setFillColor(sf::Color(255, 235, 120, 220));
        window.draw(lantern);

        sf::RectangleShape pole({ 2.5f, 20.0f });
        pole.setOrigin({ 1.25f, 20.0f });
        pole.setPosition(pos);
        pole.setFillColor(sf::Color(180, 195, 215, 180));
        window.draw(pole);
    }

    if (fontLoaded) {
        std::string label = cost.nameBg + (isValidPlacement ? " [ПОСТАВИ В ГРИДА]" : " [НЕДОПУСТИМО]");
        sf::Text t(font, toUtf8(label), 12);
        t.setFillColor(tint);
        sf::FloatRect tb = t.getLocalBounds();
        t.setPosition({ pos.x - tb.size.x / 2.0f, pos.y - 38.0f });
        window.draw(t);

        std::string costStr = "Нужно: " + std::to_string(cost.woodCost) + " Дърво, " + std::to_string(cost.oreCost) + " Руда";
        if (type == BuildingType::LAMP) costStr += " | Консумация: 10 MW";
        else if (type == BuildingType::BATTERY) costStr += " | Заряд: 0%";
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
