#include "../includes/UI_resourceNodes.h"
#include "../includes/UI_types.h"
#include <cmath>
#include <string>
#include <algorithm>

UI_resourceNodes::UI_resourceNodes() {
    stations.clear();

    // -------------------------------------------------------------------------
    // Player 1 Stations (West: 4 in Row 0, 3 in Row 1 under Land Plots)
    // -------------------------------------------------------------------------
    float p1StartX = 244.0f;
    float row0Y = 582.0f;
    float row1Y = 668.0f;
    float cardW0 = 86.0f;
    float cardH = 80.0f;
    float gapX0 = 6.0f;

    auto makeUpgradeBtn = [](const sf::FloatRect& b) {
        float upW = b.size.x * 0.94f;
        float upH = b.size.y * 0.25f;
        float upX = b.position.x + (b.size.x - upW) / 2.0f;
        float upY = b.position.y + b.size.y - upH - (b.size.y * 0.025f);
        return sf::FloatRect({ upX, upY }, { upW, upH });
    };

    // Row 0: Wood, Iron, Copper, Coal (86px cards)
    sf::FloatRect bWood1({ p1StartX + 0 * (cardW0 + gapX0), row0Y }, { cardW0, cardH });
    stations.push_back({ ResourceType::WOOD, 1, bWood1, "ГОРА", "+12 Дърво", sf::Color(75, 210, 110), makeUpgradeBtn(bWood1) });

    sf::FloatRect bIron1({ p1StartX + 1 * (cardW0 + gapX0), row0Y }, { cardW0, cardH });
    stations.push_back({ ResourceType::IRON, 1, bIron1, "ЖЕЛЯЗО", "+8 Жел", sf::Color(170, 195, 220), makeUpgradeBtn(bIron1) });

    sf::FloatRect bCop1({ p1StartX + 2 * (cardW0 + gapX0), row0Y }, { cardW0, cardH });
    stations.push_back({ ResourceType::COPPER, 1, bCop1, "МЕД", "+6 Мед", sf::Color(230, 140, 70), makeUpgradeBtn(bCop1) });

    sf::FloatRect bCoal1({ p1StartX + 3 * (cardW0 + gapX0), row0Y }, { cardW0, cardH });
    stations.push_back({ ResourceType::COAL, 1, bCoal1, "ВЪГЛИЩА", "+6 Въгл", sf::Color(115, 125, 140), makeUpgradeBtn(bCoal1) });

    // Row 1: Silicon, Silver, Gold (116px cards, perfectly centered)
    float cardW1 = 116.0f;
    float gapX1 = 8.0f;
    sf::FloatRect bSil1({ p1StartX + 0 * (cardW1 + gapX1), row1Y }, { cardW1, cardH });
    stations.push_back({ ResourceType::SILICON, 1, bSil1, "СИЛИЦИЙ", "+6 Сил", sf::Color(0, 220, 255), makeUpgradeBtn(bSil1) });

    sf::FloatRect bSilv1({ p1StartX + 1 * (cardW1 + gapX1), row1Y }, { cardW1, cardH });
    stations.push_back({ ResourceType::SILVER, 1, bSilv1, "СРЕБРО", "+4 Среб", sf::Color(225, 235, 245), makeUpgradeBtn(bSilv1) });

    sf::FloatRect bGold1({ p1StartX + 2 * (cardW1 + gapX1), row1Y }, { cardW1, cardH });
    stations.push_back({ ResourceType::GOLD, 1, bGold1, "ЗЛАТО", "+3 Злато", sf::Color(255, 215, 0), makeUpgradeBtn(bGold1) });

    // -------------------------------------------------------------------------
    // Player 2 Stations (East: 4 in Row 0, 3 in Row 1, mirrored)
    // -------------------------------------------------------------------------
    float p2StartX = 998.0f;

    // Row 0: Coal, Copper, Iron, Wood
    sf::FloatRect bCoal2({ p2StartX + 0 * (cardW0 + gapX0), row0Y }, { cardW0, cardH });
    stations.push_back({ ResourceType::COAL, 2, bCoal2, "ВЪГЛИЩА", "+6 Въгл", sf::Color(115, 125, 140), makeUpgradeBtn(bCoal2) });

    sf::FloatRect bCop2({ p2StartX + 1 * (cardW0 + gapX0), row0Y }, { cardW0, cardH });
    stations.push_back({ ResourceType::COPPER, 2, bCop2, "МЕД", "+6 Мед", sf::Color(230, 140, 70), makeUpgradeBtn(bCop2) });

    sf::FloatRect bIron2({ p2StartX + 2 * (cardW0 + gapX0), row0Y }, { cardW0, cardH });
    stations.push_back({ ResourceType::IRON, 2, bIron2, "ЖЕЛЯЗО", "+8 Жел", sf::Color(170, 195, 220), makeUpgradeBtn(bIron2) });

    sf::FloatRect bWood2({ p2StartX + 3 * (cardW0 + gapX0), row0Y }, { cardW0, cardH });
    stations.push_back({ ResourceType::WOOD, 2, bWood2, "ГОРА", "+12 Дърво", sf::Color(75, 210, 110), makeUpgradeBtn(bWood2) });

    // Row 1: Gold, Silver, Silicon
    sf::FloatRect bGold2({ p2StartX + 0 * (cardW1 + gapX1), row1Y }, { cardW1, cardH });
    stations.push_back({ ResourceType::GOLD, 2, bGold2, "ЗЛАТО", "+3 Злато", sf::Color(255, 215, 0), makeUpgradeBtn(bGold2) });

    sf::FloatRect bSilv2({ p2StartX + 1 * (cardW1 + gapX1), row1Y }, { cardW1, cardH });
    stations.push_back({ ResourceType::SILVER, 2, bSilv2, "СРЕБРО", "+4 Среб", sf::Color(225, 235, 245), makeUpgradeBtn(bSilv2) });

    sf::FloatRect bSil2({ p2StartX + 2 * (cardW1 + gapX1), row1Y }, { cardW1, cardH });
    stations.push_back({ ResourceType::SILICON, 2, bSil2, "СИЛИЦИЙ", "+6 Сил", sf::Color(0, 220, 255), makeUpgradeBtn(bSil2) });
}

ResourceType UI_resourceNodes::getP1ResourceAt(sf::Vector2f pt) const {
    for (const auto& s : stations) {
        if (s.playerOwner == 1 && s.bounds.contains(pt)) {
            // If clicking/cursor in upgrade area (bottom 26px), don't trigger mining
            if (pt.y >= s.bounds.position.y + s.bounds.size.y - 26.0f) {
                return ResourceType::NONE;
            }
            return s.type;
        }
    }
    return ResourceType::NONE;
}

ResourceType UI_resourceNodes::getP2ResourceAt(sf::Vector2f pt) const {
    for (const auto& s : stations) {
        if (s.playerOwner == 2 && s.bounds.contains(pt)) {
            // If clicking/cursor in upgrade area (bottom 26px), don't trigger mining
            if (pt.y >= s.bounds.position.y + s.bounds.size.y - 26.0f) {
                return ResourceType::NONE;
            }
            return s.type;
        }
    }
    return ResourceType::NONE;
}

ResourceType UI_resourceNodes::getP1UpgradeAt(sf::Vector2f pt) const {
    for (const auto& s : stations) {
        if (s.playerOwner == 1) {
            // Generous hit box: either inside button bounds or within bottom 26px of station card
            if (s.upgradeBtnBounds.contains(pt) ||
                (s.bounds.contains(pt) && pt.y >= s.bounds.position.y + s.bounds.size.y - 26.0f)) {
                return s.type;
            }
        }
    }
    return ResourceType::NONE;
}

ResourceType UI_resourceNodes::getP2UpgradeAt(sf::Vector2f pt) const {
    for (const auto& s : stations) {
        if (s.playerOwner == 2) {
            // Generous hit box: either inside button bounds or within bottom 26px of station card
            if (s.upgradeBtnBounds.contains(pt) ||
                (s.bounds.contains(pt) && pt.y >= s.bounds.position.y + s.bounds.size.y - 26.0f)) {
                return s.type;
            }
        }
    }
    return ResourceType::NONE;
}

ResourceType UI_resourceNodes::getP1StationAt(sf::Vector2f pt) const {
    for (const auto& s : stations) {
        if (s.playerOwner == 1 && s.bounds.contains(pt)) {
            return s.type;
        }
    }
    return ResourceType::NONE;
}

ResourceType UI_resourceNodes::getP2StationAt(sf::Vector2f pt) const {
    for (const auto& s : stations) {
        if (s.playerOwner == 2 && s.bounds.contains(pt)) {
            return s.type;
        }
    }
    return ResourceType::NONE;
}

const ResourceStation* UI_resourceNodes::getStation(int player, ResourceType type) const {
    for (const auto& s : stations) {
        if (s.playerOwner == player && s.type == type) {
            return &s;
        }
    }
    return nullptr;
}

bool UI_resourceNodes::isNearP1Forest(sf::Vector2f pt) const {
    return getP1ResourceAt(pt) == ResourceType::WOOD || getP1ForestBounds().contains(pt);
}

bool UI_resourceNodes::isNearP1Mine(sf::Vector2f pt) const {
    ResourceType r = getP1ResourceAt(pt);
    return (r != ResourceType::NONE && r != ResourceType::WOOD) || getP1MineBounds().contains(pt);
}

bool UI_resourceNodes::isNearP2Mine(sf::Vector2f pt) const {
    ResourceType r = getP2ResourceAt(pt);
    return (r != ResourceType::NONE && r != ResourceType::WOOD) || getP2MineBounds().contains(pt);
}

bool UI_resourceNodes::isNearP2Forest(sf::Vector2f pt) const {
    return getP2ResourceAt(pt) == ResourceType::WOOD || getP2ForestBounds().contains(pt);
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

            // 3x3 Building Placement Grid Dividers & Slot Crosshairs (9 slots per slab)
            float colW = plot.bounds.size.x / 3.0f;
            float rowH = plot.bounds.size.y / 3.0f;

            // Horizontal grid dividers (2 lines)
            for (int divR = 1; divR < 3; ++divR) {
                float y = plot.bounds.position.y + divR * rowH;
                sf::RectangleShape hLine({ plot.bounds.size.x - 6.0f, 1.0f });
                hLine.setPosition({ plot.bounds.position.x + 3.0f, y });
                hLine.setFillColor(sf::Color(ownerAccent.r, ownerAccent.g, ownerAccent.b, 65));
                window.draw(hLine);
            }

            // Vertical grid dividers (2 lines)
            for (int divC = 1; divC < 3; ++divC) {
                float x = plot.bounds.position.x + divC * colW;
                sf::RectangleShape vLine({ 1.0f, plot.bounds.size.y - 6.0f });
                vLine.setPosition({ x, plot.bounds.position.y + 3.0f });
                vLine.setFillColor(sf::Color(ownerAccent.r, ownerAccent.g, ownerAccent.b, 65));
                window.draw(vLine);
            }

            // 9 Slot center crosshairs '+'
            for (int r = 0; r < 3; r++) {
                for (int c = 0; c < 3; c++) {
                    float cx = plot.bounds.position.x + (c + 0.5f) * colW;
                    float cy = plot.bounds.position.y + (r + 0.5f) * rowH;

                    sf::RectangleShape crossH({ 5.0f, 1.0f });
                    crossH.setOrigin({ 2.5f, 0.5f });
                    crossH.setPosition({ cx, cy });
                    crossH.setFillColor(sf::Color(ownerAccent.r, ownerAccent.g, ownerAccent.b, 75));
                    window.draw(crossH);

                    sf::RectangleShape crossV({ 1.0f, 5.0f });
                    crossV.setOrigin({ 0.5f, 2.5f });
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
            sf::RectangleShape frame({ 26.0f, 20.0f });
            frame.setOrigin({ 13.0f, 10.0f });
            frame.setPosition(b.position);
            frame.setFillColor(sf::Color(25, 35, 55));
            frame.setOutlineThickness(1.2f);
            frame.setOutlineColor(ownerColor);
            window.draw(frame);

            for (int r = 0; r < 2; r++) {
                for (int c = 0; c < 3; c++) {
                    sf::RectangleShape cell({ 6.0f, 6.0f });
                    cell.setPosition({ b.position.x - 10.0f + c * 7.5f, b.position.y - 7.0f + r * 7.5f });
                    cell.setFillColor(sf::Color(20, 100, 220, 220));
                    cell.setOutlineThickness(0.5f);
                    cell.setOutlineColor(sf::Color(100, 180, 255, 180));
                    window.draw(cell);
                }
            }
        } else if (b.type == BuildingType::WIND_TURBINE) {
            sf::RectangleShape mast({ 3.0f, 22.0f });
            mast.setOrigin({ 1.5f, 22.0f });
            mast.setPosition(b.position);
            mast.setFillColor(sf::Color(210, 225, 240));
            mast.setOutlineThickness(1.0f);
            mast.setOutlineColor(sf::Color(140, 160, 185));
            window.draw(mast);

            sf::Vector2f hubPos = { b.position.x, b.position.y - 22.0f };
            float angleDeg = b.animTimer * 180.0f;
            for (int blade = 0; blade < 3; blade++) {
                float a = (angleDeg + blade * 120.0f) * 3.14159265f / 180.0f;
                sf::VertexArray bladeGeom(sf::PrimitiveType::Triangles, 3);
                bladeGeom[0].position = hubPos;
                bladeGeom[0].color = sf::Color::White;
                bladeGeom[1].position = { hubPos.x + 15.0f * std::cos(a), hubPos.y + 15.0f * std::sin(a) };
                bladeGeom[1].color = sf::Color(220, 235, 250);
                bladeGeom[2].position = { hubPos.x + 12.0f * std::cos(a + 0.15f), hubPos.y + 12.0f * std::sin(a + 0.15f) };
                bladeGeom[2].color = ownerColor;
                window.draw(bladeGeom);
            }

            sf::CircleShape hub(3.0f);
            hub.setOrigin({ 3.0f, 3.0f });
            hub.setPosition(hubPos);
            hub.setFillColor(ownerColor);
            window.draw(hub);
        } else if (b.type == BuildingType::HYDRO_PLANT) {
            sf::RectangleShape station({ 26.0f, 22.0f });
            station.setOrigin({ 13.0f, 11.0f });
            station.setPosition(b.position);
            station.setFillColor(sf::Color(30, 48, 70));
            station.setOutlineThickness(1.2f);
            station.setOutlineColor(ownerColor);
            window.draw(station);

            sf::CircleShape wheel(6.0f);
            wheel.setOrigin({ 6.0f, 6.0f });
            wheel.setPosition(b.position);
            wheel.setFillColor(sf::Color(45, 120, 180, 180));
            wheel.setOutlineThickness(1.0f);
            wheel.setOutlineColor(sf::Color(100, 220, 255));
            window.draw(wheel);
        } else if (b.type == BuildingType::BATTERY) {
            sf::RectangleShape caseBox({ 22.0f, 26.0f });
            caseBox.setOrigin({ 11.0f, 13.0f });
            caseBox.setPosition(b.position);
            caseBox.setFillColor(sf::Color(18, 25, 36));
            caseBox.setOutlineThickness(1.2f);
            caseBox.setOutlineColor(ownerColor);
            window.draw(caseBox);

            sf::RectangleShape term({ 8.0f, 3.0f });
            term.setOrigin({ 4.0f, 3.0f });
            term.setPosition({ b.position.x, b.position.y - 13.0f });
            term.setFillColor(sf::Color(210, 215, 225));
            window.draw(term);

            float pct = std::min(1.0f, std::max(0.0f, b.energyStored / b.maxCapacity));
            float fillH = 18.0f * pct;
            if (fillH > 0.5f) {
                sf::RectangleShape fluid({ 16.0f, fillH });
                fluid.setOrigin({ 8.0f, fillH });
                fluid.setPosition({ b.position.x, b.position.y + 9.0f });
                sf::Color fluidCol = (pct > 0.6f) ? sf::Color(0, 230, 140) : ((pct > 0.25f) ? sf::Color(255, 210, 40) : sf::Color(255, 90, 60));
                fluid.setFillColor(fluidCol);
                window.draw(fluid);
            }

            for (int seg = 1; seg <= 3; seg++) {
                sf::RectangleShape div({ 16.0f, 1.0f });
                div.setOrigin({ 8.0f, 0.5f });
                div.setPosition({ b.position.x, b.position.y + 9.0f - seg * 4.5f });
                div.setFillColor(sf::Color(60, 75, 95, 150));
                window.draw(div);
            }

            if (fontLoaded) {
                int pctInt = static_cast<int>(pct * 100.0f);
                sf::Text tPct(font, std::to_string(pctInt) + "%", 8);
                tPct.setFillColor(sf::Color::White);
                sf::FloatRect tb = tPct.getLocalBounds();
                tPct.setPosition({ b.position.x - tb.size.x / 2.0f, b.position.y - 5.0f });
                window.draw(tPct);
            }
        } else if (b.type == BuildingType::LAMP) {
            sf::RectangleShape pole({ 3.0f, 20.0f });
            pole.setOrigin({ 1.5f, 20.0f });
            pole.setPosition(b.position);
            pole.setFillColor(sf::Color(170, 185, 205));
            window.draw(pole);

            sf::CircleShape head(6.5f);
            head.setOrigin({ 6.5f, 6.5f });
            head.setPosition({ b.position.x, b.position.y - 24.0f });
            bool isPowered = (b.lightRadius > 0.0f);
            head.setFillColor(isPowered ? sf::Color(255, 235, 120) : sf::Color(65, 70, 80));
            head.setOutlineThickness(1.0f);
            head.setOutlineColor(ownerColor);
            window.draw(head);

            if (isPowered) {
                sf::CircleShape glow(b.lightRadius);
                glow.setOrigin({ b.lightRadius, b.lightRadius });
                glow.setPosition(b.position);
                glow.setFillColor(sf::Color(255, 230, 120, 22));
                glow.setOutlineThickness(1.0f);
                glow.setOutlineColor(sf::Color(255, 220, 100, 50));
                window.draw(glow);
            }
        }
    }
}

void UI_resourceNodes::drawBuildingGhost(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                                       BuildingType type, sf::Vector2f pos, bool isValidPlacement,
                                       const BuildingCost& cost) {
    if (type == BuildingType::NONE) return;

    sf::Color tint = isValidPlacement ? sf::Color(0, 255, 180, 220) : sf::Color(255, 60, 60, 220);

    // Grid box footprint
    sf::RectangleShape footprint({ 48.0f, 42.0f });
    footprint.setOrigin({ 24.0f, 21.0f });
    footprint.setPosition(pos);
    footprint.setFillColor(isValidPlacement ? sf::Color(0, 255, 180, 35) : sf::Color(255, 60, 60, 45));
    footprint.setOutlineThickness(2.0f);
    footprint.setOutlineColor(tint);
    window.draw(footprint);

    if (type == BuildingType::LAMP) {
        sf::CircleShape lampCone(150.0f);
        lampCone.setOrigin({ 150.0f, 150.0f });
        lampCone.setPosition(pos);
        lampCone.setFillColor(isValidPlacement ? sf::Color(255, 235, 120, 28) : sf::Color(255, 80, 80, 20));
        lampCone.setOutlineThickness(1.5f);
        lampCone.setOutlineColor(isValidPlacement ? sf::Color(255, 220, 100, 120) : sf::Color(255, 80, 80, 100));
        window.draw(lampCone);
    }

    if (fontLoaded) {
        std::string label = cost.nameBg + (isValidPlacement ? " [ПОСТАВИ В ГРИДА]" : " [НЕДОПУСТИМО]");
        sf::Text t(font, toUtf8(label), 12);
        t.setFillColor(tint);
        sf::FloatRect tb = t.getLocalBounds();
        t.setPosition({ pos.x - tb.size.x / 2.0f, pos.y - 38.0f });
        window.draw(t);

        std::string costStr = "Нужно: " + std::to_string(cost.woodCost) + " Дърво";
        if (cost.ironCost > 0) costStr += ", " + std::to_string(cost.ironCost) + " Жел";
        if (cost.copperCost > 0) costStr += ", " + std::to_string(cost.copperCost) + " Мед";
        if (cost.siliconCost > 0) costStr += ", " + std::to_string(cost.siliconCost) + " Сил";
        if (cost.coalCost > 0) costStr += ", " + std::to_string(cost.coalCost) + " Въгл";
        if (cost.silverCost > 0) costStr += ", " + std::to_string(cost.silverCost) + " Среб";

        if (type == BuildingType::LAMP) costStr += " | Консумация: 10 MW";
        else if (type == BuildingType::BATTERY) costStr += " | Заряд: 0%";
        sf::Text tc(font, toUtf8(costStr), 10);
        tc.setFillColor(sf::Color::White);
        sf::FloatRect tcb = tc.getLocalBounds();
        tc.setPosition({ pos.x - tcb.size.x / 2.0f, pos.y + 24.0f });
        window.draw(tc);

        // Cancel keys are X (P1) / Del (P2); Q / PgUp only step back through the buildings
        std::string hint = isValidPlacement ? "[SPACE/КЛИК]: Постави  |  [X/Del]: Отказ  |  [E]: Смени"
                                            : "[X/Del]: Отказ  |  [E]: Смени сграда";
        sf::Text th(font, toUtf8(hint), 10);
        th.setFillColor(isValidPlacement ? sf::Color(255, 230, 100) : sf::Color(255, 130, 130));
        sf::FloatRect thb = th.getLocalBounds();
        th.setPosition({ pos.x - thb.size.x / 2.0f, pos.y + 38.0f });
        window.draw(th);
    }
}

void UI_resourceNodes::drawNodes(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                                 const GameEngine* engine,
                                 float p1Cooldown, float p2Cooldown) {
    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    for (const auto& s : stations) {
        bool hover = s.bounds.contains(mousePos);
        float cd = (s.playerOwner == 1) ? p1Cooldown : p2Cooldown;
        bool onCooldown = (cd > 0.0f);

        int lvl = engine ? engine->getMineLevel(s.playerOwner, s.type) : 1;
        int upCost = engine ? engine->getMineUpgradeCost(s.playerOwner, s.type) : 15;
        float mult = 1.0f + (lvl - 1) * 0.75f;

        // Station background card
        sf::RectangleShape card(s.bounds.size);
        card.setPosition(s.bounds.position);
        card.setFillColor(hover ? sf::Color(32, 44, 62, 245) : sf::Color(20, 26, 38, 230));
        card.setOutlineThickness(hover ? 2.0f : (lvl > 1 ? 1.5f : 1.2f));
        card.setOutlineColor(hover ? sf::Color(255, 230, 100) : (lvl > 1 ? sf::Color(255, 215, 0, 200) : s.themeColor));
        window.draw(card);

        // Top accent line
        sf::RectangleShape topBar({ s.bounds.size.x, 3.0f });
        topBar.setPosition(s.bounds.position);
        topBar.setFillColor(lvl > 1 ? sf::Color(255, 215, 0) : s.themeColor);
        window.draw(topBar);

        // Vector Icon depending on resource type
        float icX = s.bounds.position.x + 8.0f;
        float icY = s.bounds.position.y + 14.0f;

        if (s.type == ResourceType::WOOD) {
            sf::RectangleShape trunk({ 3.0f, 8.0f });
            trunk.setPosition({ icX + 6.0f, icY + 12.0f });
            trunk.setFillColor(sf::Color(120, 80, 45));
            window.draw(trunk);

            sf::ConvexShape pine(3);
            pine.setPoint(0, { icX + 7.5f, icY });
            pine.setPoint(1, { icX, icY + 12.0f });
            pine.setPoint(2, { icX + 15.0f, icY + 12.0f });
            pine.setFillColor(sf::Color(65, 190, 95));
            window.draw(pine);
        } else if (s.type == ResourceType::IRON) {
            sf::RectangleShape anvil({ 14.0f, 9.0f });
            anvil.setPosition({ icX, icY + 4.0f });
            anvil.setFillColor(sf::Color(170, 190, 215));
            window.draw(anvil);

            sf::RectangleShape horn({ 5.0f, 4.0f });
            horn.setPosition({ icX + 13.0f, icY + 4.0f });
            horn.setFillColor(sf::Color(140, 160, 185));
            window.draw(horn);
        } else if (s.type == ResourceType::COPPER) {
            sf::CircleShape coil(7.0f);
            coil.setPosition({ icX, icY + 2.0f });
            coil.setFillColor(sf::Color::Transparent);
            coil.setOutlineThickness(2.5f);
            coil.setOutlineColor(sf::Color(235, 140, 70));
            window.draw(coil);
        } else if (s.type == ResourceType::COAL) {
            sf::ConvexShape lump(5);
            lump.setPoint(0, { icX + 3.0f, icY });
            lump.setPoint(1, { icX + 14.0f, icY + 2.0f });
            lump.setPoint(2, { icX + 12.0f, icY + 13.0f });
            lump.setPoint(3, { icX + 2.0f, icY + 12.0f });
            lump.setPoint(4, { icX, icY + 6.0f });
            lump.setFillColor(sf::Color(90, 95, 105));
            window.draw(lump);
        } else if (s.type == ResourceType::SILICON) {
            sf::ConvexShape crystal(4);
            crystal.setPoint(0, { icX + 7.0f, icY });
            crystal.setPoint(1, { icX + 14.0f, icY + 7.0f });
            crystal.setPoint(2, { icX + 7.0f, icY + 14.0f });
            crystal.setPoint(3, { icX, icY + 7.0f });
            crystal.setFillColor(sf::Color(0, 229, 255));
            window.draw(crystal);
        } else if (s.type == ResourceType::SILVER) {
            sf::RectangleShape bar({ 15.0f, 8.0f });
            bar.setPosition({ icX, icY + 5.0f });
            bar.setFillColor(sf::Color(225, 235, 245));
            bar.setOutlineThickness(1.0f);
            bar.setOutlineColor(sf::Color(170, 185, 205));
            window.draw(bar);
        } else if (s.type == ResourceType::GOLD) {
            sf::CircleShape coin(7.0f);
            coin.setPosition({ icX, icY + 2.0f });
            coin.setFillColor(sf::Color(255, 215, 0));
            coin.setOutlineThickness(1.2f);
            coin.setOutlineColor(sf::Color(180, 140, 20));
            window.draw(coin);
        }

        // Labels & Upgrade Button
        if (fontLoaded) {
            // Station Name
            sf::Text nameText(font, toUtf8(s.nameBg), 11);
            nameText.setFillColor(s.themeColor);
            nameText.setPosition({ s.bounds.position.x + 28.0f, s.bounds.position.y + 6.0f });
            window.draw(nameText);

            // Level Badge in Top-Right
            std::string lvlStr = "L" + std::to_string(lvl);
            sf::Text tLvl(font, toUtf8(lvlStr), 10);
            tLvl.setFillColor(lvl > 1 ? sf::Color(255, 215, 0) : sf::Color(160, 180, 205));
            sf::FloatRect lb = tLvl.getLocalBounds();
            tLvl.setPosition({ s.bounds.position.x + s.bounds.size.x - lb.size.x - 6.0f, s.bounds.position.y + 6.0f });
            window.draw(tLvl);

            // Dynamic Yield Text
            int curYield = 12;
            std::string unit = "Дърво";
            switch (s.type) {
                case ResourceType::WOOD: curYield = static_cast<int>(std::round(Balance::WOOD_BASE_YIELD * mult)); unit = "Дърво"; break;
                case ResourceType::IRON: curYield = static_cast<int>(std::round(Balance::IRON_BASE_YIELD * mult)); unit = "Жел"; break;
                case ResourceType::COPPER: curYield = static_cast<int>(std::round(Balance::COPPER_BASE_YIELD * mult)); unit = "Мед"; break;
                case ResourceType::COAL: curYield = static_cast<int>(std::round(Balance::COAL_BASE_YIELD * mult)); unit = "Въгл"; break;
                case ResourceType::SILICON: curYield = static_cast<int>(std::round(Balance::SILICON_BASE_YIELD * mult)); unit = "Сил"; break;
                case ResourceType::SILVER: curYield = static_cast<int>(std::round(Balance::SILVER_BASE_YIELD * mult)); unit = "Среб"; break;
                case ResourceType::GOLD: curYield = static_cast<int>(std::round(Balance::GOLD_BASE_YIELD * mult)); unit = "Злато"; break;
                default: break;
            }
            std::string curYieldStr = "+" + std::to_string(curYield) + " " + unit;
            sf::Text yieldText(font, toUtf8(curYieldStr), 11);
            yieldText.setFillColor(sf::Color::White);
            yieldText.setPosition({ s.bounds.position.x + 8.0f, s.bounds.position.y + 34.0f });
            window.draw(yieldText);

            // Status or Cooldown Bar
            if (onCooldown) {
                char cdbuf[16];
                std::snprintf(cdbuf, sizeof(cdbuf), "%.1fs", cd);
                sf::Text cdText(font, toUtf8(cdbuf), 9);
                cdText.setFillColor(sf::Color(255, 170, 70));
                cdText.setPosition({ s.bounds.position.x + 8.0f, s.bounds.position.y + 48.0f });
                window.draw(cdText);

                float cdRatio = std::min(1.0f, std::max(0.0f, cd / 1.0f));
                sf::RectangleShape cdBar({ (s.bounds.size.x - 16.0f) * (1.0f - cdRatio), 2.0f });
                cdBar.setPosition({ s.bounds.position.x + 8.0f, s.bounds.position.y + 58.0f });
                cdBar.setFillColor(sf::Color(255, 180, 50));
                window.draw(cdBar);
            } else {
                std::string actHint = (s.playerOwner == 1) ? "[SPACE: Добив]" : "[ENTER: Добив]";
                sf::Text actText(font, toUtf8(hover ? actHint : "[ГОТОВО]"), 9);
                actText.setFillColor(hover ? sf::Color(255, 235, 120) : sf::Color(140, 240, 180));
                actText.setPosition({ s.bounds.position.x + 8.0f, s.bounds.position.y + 48.0f });
                window.draw(actText);
            }

            // Upgrade Button at bottom of card - snap hover when near bottom
            bool upHover = s.upgradeBtnBounds.contains(mousePos) ||
                           (s.bounds.contains(mousePos) && mousePos.y >= s.bounds.position.y + s.bounds.size.y - 26.0f);
            sf::RectangleShape upBtn(s.upgradeBtnBounds.size);
            upBtn.setPosition(s.upgradeBtnBounds.position);
            if (lvl >= Balance::MINE_MAX_LEVEL) {
                upBtn.setFillColor(sf::Color(40, 45, 55, 200));
                upBtn.setOutlineThickness(1.0f);
                upBtn.setOutlineColor(sf::Color(100, 110, 130));
            } else {
                upBtn.setFillColor(upHover ? sf::Color(95, 75, 25, 255) : sf::Color(35, 30, 15, 220));
                upBtn.setOutlineThickness(upHover ? 2.0f : 1.0f);
                upBtn.setOutlineColor(upHover ? sf::Color(255, 240, 100) : sf::Color(255, 215, 0, 190));
            }
            window.draw(upBtn);

            std::string upLabel = (lvl >= Balance::MINE_MAX_LEVEL) ? "МАКС" : ("+1 НИВО: " + std::to_string(upCost) + "G");
            sf::Text tUp(font, toUtf8(upLabel), 9);
            tUp.setFillColor(lvl >= Balance::MINE_MAX_LEVEL ? sf::Color(160, 170, 185) : (upHover ? sf::Color(255, 250, 180) : sf::Color(255, 215, 0)));
            sf::FloatRect upb = tUp.getLocalBounds();
            tUp.setPosition({ s.upgradeBtnBounds.position.x + (s.upgradeBtnBounds.size.x - upb.size.x) / 2.0f,
                              s.upgradeBtnBounds.position.y + (s.upgradeBtnBounds.size.y - upb.size.y) / 2.0f - 1.0f });
            window.draw(tUp);
        }
    }
}
