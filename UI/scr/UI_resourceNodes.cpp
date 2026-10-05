#include "../includes/UI_resourceNodes.h"
#include "../includes/UI_text.h"
#include "../includes/UI_shot.h"
#include "../includes/UI_theme.h"
#include "../includes/UI_icons.h"
#include "../includes/UI_buildings.h"
#include <cstdio>
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
    stations.push_back({ ResourceType::WOOD, 1, bWood1, "ГОРА", "+12 дърво", theme::Wood, makeUpgradeBtn(bWood1) });

    sf::FloatRect bIron1({ p1StartX + 1 * (cardW0 + gapX0), row0Y }, { cardW0, cardH });
    stations.push_back({ ResourceType::IRON, 1, bIron1, "ЖЕЛЯЗО", "+8 желязо", theme::Iron, makeUpgradeBtn(bIron1) });

    sf::FloatRect bCop1({ p1StartX + 2 * (cardW0 + gapX0), row0Y }, { cardW0, cardH });
    stations.push_back({ ResourceType::COPPER, 1, bCop1, "МЕД", "+6 мед", theme::Copper, makeUpgradeBtn(bCop1) });

    sf::FloatRect bCoal1({ p1StartX + 3 * (cardW0 + gapX0), row0Y }, { cardW0, cardH });
    stations.push_back({ ResourceType::COAL, 1, bCoal1, "ВЪГЛИЩА", "+6 въглища", theme::Coal, makeUpgradeBtn(bCoal1) });

    // Row 1: Silicon, Silver, Gold (116px cards, perfectly centered)
    float cardW1 = 116.0f;
    float gapX1 = 8.0f;
    sf::FloatRect bSil1({ p1StartX + 0 * (cardW1 + gapX1), row1Y }, { cardW1, cardH });
    stations.push_back({ ResourceType::SILICON, 1, bSil1, "СИЛИЦИЙ", "+6 силиций", theme::Silicon, makeUpgradeBtn(bSil1) });

    sf::FloatRect bSilv1({ p1StartX + 1 * (cardW1 + gapX1), row1Y }, { cardW1, cardH });
    stations.push_back({ ResourceType::SILVER, 1, bSilv1, "СРЕБРО", "+4 сребро", theme::Silver, makeUpgradeBtn(bSilv1) });

    sf::FloatRect bGold1({ p1StartX + 2 * (cardW1 + gapX1), row1Y }, { cardW1, cardH });
    stations.push_back({ ResourceType::GOLD, 1, bGold1, "ЗЛАТО", "+3 злато", theme::Gold, makeUpgradeBtn(bGold1) });

    // -------------------------------------------------------------------------
    // Player 2 Stations (East: 4 in Row 0, 3 in Row 1, mirrored)
    // -------------------------------------------------------------------------
    float p2StartX = 998.0f;

    // Row 0: Coal, Copper, Iron, Wood
    sf::FloatRect bCoal2({ p2StartX + 0 * (cardW0 + gapX0), row0Y }, { cardW0, cardH });
    stations.push_back({ ResourceType::COAL, 2, bCoal2, "ВЪГЛИЩА", "+6 въглища", theme::Coal, makeUpgradeBtn(bCoal2) });

    sf::FloatRect bCop2({ p2StartX + 1 * (cardW0 + gapX0), row0Y }, { cardW0, cardH });
    stations.push_back({ ResourceType::COPPER, 2, bCop2, "МЕД", "+6 мед", theme::Copper, makeUpgradeBtn(bCop2) });

    sf::FloatRect bIron2({ p2StartX + 2 * (cardW0 + gapX0), row0Y }, { cardW0, cardH });
    stations.push_back({ ResourceType::IRON, 2, bIron2, "ЖЕЛЯЗО", "+8 желязо", theme::Iron, makeUpgradeBtn(bIron2) });

    sf::FloatRect bWood2({ p2StartX + 3 * (cardW0 + gapX0), row0Y }, { cardW0, cardH });
    stations.push_back({ ResourceType::WOOD, 2, bWood2, "ГОРА", "+12 дърво", theme::Wood, makeUpgradeBtn(bWood2) });

    // Row 1: Gold, Silver, Silicon
    sf::FloatRect bGold2({ p2StartX + 0 * (cardW1 + gapX1), row1Y }, { cardW1, cardH });
    stations.push_back({ ResourceType::GOLD, 2, bGold2, "ЗЛАТО", "+3 злато", theme::Gold, makeUpgradeBtn(bGold2) });

    sf::FloatRect bSilv2({ p2StartX + 1 * (cardW1 + gapX1), row1Y }, { cardW1, cardH });
    stations.push_back({ ResourceType::SILVER, 2, bSilv2, "СРЕБРО", "+4 сребро", theme::Silver, makeUpgradeBtn(bSilv2) });

    sf::FloatRect bSil2({ p2StartX + 2 * (cardW1 + gapX1), row1Y }, { cardW1, cardH });
    stations.push_back({ ResourceType::SILICON, 2, bSil2, "СИЛИЦИЙ", "+6 силиций", theme::Silicon, makeUpgradeBtn(bSil2) });
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
                                     const std::vector<LandPlot>& plots, sf::Vector2f mousePos,
                                     const std::vector<PlacedBuilding>& buildings) {
    for (const auto& plot : plots) {
        ui::lint::ContainerScope plotScope(plot.bounds);
        bool hover = plot.bounds.contains(mousePos);
        sf::Color ownerAccent = theme::player(plot.playerOwner);

        sf::RectangleShape box(plot.bounds.size);
        box.setPosition(plot.bounds.position);

        if (plot.isPurchased) {
            box.setFillColor(theme::withAlpha(theme::playerDark(plot.playerOwner), 150));
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
                hLine.setFillColor(theme::withAlpha(ownerAccent, 65));
                window.draw(hLine);
            }

            // Vertical grid dividers (2 lines)
            for (int divC = 1; divC < 3; ++divC) {
                float x = plot.bounds.position.x + divC * colW;
                sf::RectangleShape vLine({ 1.0f, plot.bounds.size.y - 6.0f });
                vLine.setPosition({ x, plot.bounds.position.y + 3.0f });
                vLine.setFillColor(theme::withAlpha(ownerAccent, 65));
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
                    crossH.setFillColor(theme::withAlpha(ownerAccent, 75));
                    window.draw(crossH);

                    sf::RectangleShape crossV({ 1.0f, 5.0f });
                    crossV.setOrigin({ 0.5f, 2.5f });
                    crossV.setPosition({ cx, cy });
                    crossV.setFillColor(theme::withAlpha(ownerAccent, 75));
                    window.draw(crossV);
                }
            }

            // Boundary posts on corners
            sf::CircleShape post(3.0f);
            post.setOrigin({ 3.0f, 3.0f });
            post.setFillColor(theme::playerLight(plot.playerOwner));
            for (float px : { plot.bounds.position.x, plot.bounds.position.x + plot.bounds.size.x }) {
                for (float py : { plot.bounds.position.y, plot.bounds.position.y + plot.bounds.size.y }) {
                    post.setPosition({ px, py });
                    window.draw(post);
                }
            }

            // Owner label only while the plot is still empty: buildings in the top row would cover it
            bool plotEmpty = std::none_of(buildings.begin(), buildings.end(),
                                          [&](const PlacedBuilding& b) { return plot.bounds.contains(b.position); });
            if (fontLoaded && plotEmpty) {
                std::string tag = (plot.playerOwner == 1) ? "ЗЕМЯ НА P1" : "ЗЕМЯ НА P2";
                sf::Text& t = ui::pooledText(font, toUtf8(tag), fontsize::Caption);
                t.setFillColor(theme::playerLight(plot.playerOwner));
                t.setPosition({ plot.bounds.position.x + 6.0f, plot.bounds.position.y + 4.0f });
                ui::drawText(window, t);
            }
        } else {
            // Unpurchased, available for purchase!
            box.setFillColor(hover ? theme::withAlpha(theme::CardHover, 200) : theme::withAlpha(theme::Window, 165));
            box.setOutlineThickness(hover ? 2.5f : 1.0f);
            box.setOutlineColor(hover ? theme::Focus : theme::withAlpha(theme::LineStrong, 200));
            window.draw(box);

            if (fontLoaded) {
                sf::Text& t = ui::pooledText(font, toUtf8("+ КУПИ ЗЕМЯ"), fontsize::Caption);
                t.setFillColor(hover ? theme::TextPrimary : theme::TextSecondary);
                sf::FloatRect tb = t.getLocalBounds();
                t.setPosition({ plot.bounds.position.x + (plot.bounds.size.x - tb.size.x) / 2.0f, plot.bounds.position.y + 28.0f });
                ui::drawText(window, t);

                std::string cStr = std::to_string(plot.costGold) + " $";
                sf::Text& tCost = ui::pooledText(font, toUtf8(cStr), fontsize::Body);
                tCost.setStyle(sf::Text::Bold);
                tCost.setFillColor(theme::Good);
                sf::FloatRect cb = tCost.getLocalBounds();
                tCost.setPosition({ plot.bounds.position.x + (plot.bounds.size.x - cb.size.x) / 2.0f, plot.bounds.position.y + 48.0f });
                ui::drawText(window, tCost);
            }
        }
    }
}

void UI_resourceNodes::drawPlacedBuildings(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                                          const std::vector<PlacedBuilding>& buildings) {
    (void)font;
    (void)fontLoaded;
    for (const auto& b : buildings) {
        sf::Color ownerColor = theme::player(b.playerOwner);
        ui::lint::solid(sf::FloatRect({ b.position.x - 13.0f, b.position.y - 13.0f }, { 26.0f, 26.0f })); // no text may hide under it

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

            if (fontLoaded && b.maxCapacity > 0.0f) {
                int pctInt = static_cast<int>(std::round(pct * 100.0f));
                std::string pctStr = std::to_string(pctInt) + "%";
                sf::Text& tPct = ui::pooledText(font, toUtf8(pctStr), fontsize::Caption);
                tPct.setStyle(sf::Text::Bold);
                sf::Color txtCol = (pct > 0.5f) ? sf::Color(100, 255, 180) : ((pct > 0.2f) ? sf::Color(255, 230, 100) : sf::Color(255, 120, 100));
                tPct.setFillColor(txtCol);
                sf::FloatRect tb = tPct.getLocalBounds();
                sf::RectangleShape pill({ tb.size.x + 4.0f, tb.size.y + 3.0f });
                pill.setOrigin({ (tb.size.x + 4.0f) * 0.5f, (tb.size.y + 3.0f) * 0.5f });
                pill.setPosition({ b.position.x, b.position.y - 0.5f });
                pill.setFillColor(sf::Color(12, 18, 28, 220));
                window.draw(pill);
                tPct.setPosition({ b.position.x - tb.size.x * 0.5f - tb.position.x,
                                   b.position.y - 0.5f - tb.size.y * 0.5f - tb.position.y });
                ui::drawText(window, tPct);
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
                                       const BuildingCost& cost, sf::Color overrideColor) {
    if (type == BuildingType::NONE) return;
    (void)font;
    (void)fontLoaded;
    (void)cost;

    sf::Color tint = (isValidPlacement && overrideColor != sf::Color::Transparent)
                         ? overrideColor
                         : (isValidPlacement ? theme::Good : theme::Bad);

    // Grid box footprint
    sf::RectangleShape footprint({ 48.0f, 42.0f });
    footprint.setOrigin({ 24.0f, 21.0f });
    footprint.setPosition(pos);
    footprint.setFillColor(theme::withAlpha(tint, isValidPlacement ? 35 : 45));
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
}

void UI_resourceNodes::drawBuildingGhostInfo(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                                             BuildingType type, sf::Vector2f pos, bool isValidPlacement,
                                             const BuildingCost& cost,
                                             const std::string& missing) {
    if (type == BuildingType::NONE) return;
    sf::Color tint = isValidPlacement ? theme::Good : theme::Bad;

    if (fontLoaded) {
        std::string label = cost.nameBg + (isValidPlacement ? " [ПОСТАВИ В ГРИДА]" : " [НЕДОПУСТИМО]");

        std::string costStr = (type == BuildingType::DEMOLISH) ? std::string("Връща половината ресурси")
                                                               : "Нужно: " + recipeText(cost);
        if (type == BuildingType::LAMP) costStr += " · консумира " + std::to_string(static_cast<int>(GameEngine::LAMP_POWER_MW)) + " MW";
        else if (type == BuildingType::BATTERY) costStr += " · започва от 0%";
        if (!missing.empty()) costStr = missing; // exactly what the player still has to mine

        // Cancel keys are X (P1) / Del (P2); Q / PgUp only step back through the buildings
        std::string hint = isValidPlacement ? "[SPACE/КЛИК]: Постави  |  [X/Del]: Отказ  |  [E]: Смени"
                                            : "[X/Del]: Отказ  |  [E]: Смени сграда";

        // Smart text wrapping (Diagram fix): Wrap long recipe/hints to fit neatly inside tooltip
        const float maxTextW = 340.0f;
        std::string wrappedCost = ui::wrapText(font, costStr, fontsize::Caption, maxTextW);
        std::string wrappedHint = ui::wrapText(font, hint, fontsize::Caption, maxTextW);

        struct TooltipLine {
            std::string text;
            unsigned int size;
            bool bold;
            sf::Color color;
        };
        std::vector<TooltipLine> lines;
        lines.push_back({ label, fontsize::Label, true, tint });

        auto appendWrapped = [&](const std::string& str, sf::Color col) {
            std::string cur;
            for (char ch : str) {
                if (ch == '\n') {
                    if (!cur.empty()) { lines.push_back({ cur, fontsize::Caption, false, col }); cur.clear(); }
                } else {
                    cur += ch;
                }
            }
            if (!cur.empty()) lines.push_back({ cur, fontsize::Caption, false, col });
        };
        appendWrapped(wrappedCost, missing.empty() ? theme::TextPrimary : theme::Bad);
        appendWrapped(wrappedHint, theme::TextSecondary);

        const float lineGap = 16.0f;
        float maxMeasured = 0.0f;
        for (const auto& l : lines) {
            float lw = ui::measureText(font, l.text, l.size, l.bold);
            if (lw > maxMeasured) maxMeasured = lw;
        }
        float w = maxMeasured + 20.0f;
        float h = 10.0f + static_cast<float>(lines.size()) * lineGap + 4.0f;

        // CLAMP: Keep tooltip entirely in the playfield, NEVER under the left/right building panels!
        float px = std::clamp(pos.x - w / 2.0f, 256.0f, 1344.0f - w);
        float py = pos.y + 26.0f;
        if (py + h > VIRTUAL_HEIGHT - 4.0f) py = pos.y - 26.0f - h;

        sf::RectangleShape panel({ w, h });
        panel.setPosition({ px, py });
        panel.setFillColor(theme::withAlpha(theme::Panel, 240));
        panel.setOutlineThickness(1.2f);
        panel.setOutlineColor(theme::withAlpha(tint, 190));
        window.draw(panel);
        const sf::FloatRect panelRect({ px, py }, { w, h });
        ui::lint::occlude(panelRect);

        for (std::size_t i = 0; i < lines.size(); ++i) {
            sf::Text& txt = ui::pooledText(font, toUtf8(lines[i].text), lines[i].size);
            txt.setStyle(lines[i].bold ? sf::Text::Bold : sf::Text::Regular);
            txt.setFillColor(lines[i].color);
            sf::FloatRect lb = txt.getLocalBounds();
            txt.setPosition({ px + (w - lb.size.x) / 2.0f - lb.position.x, py + 6.0f + static_cast<float>(i) * lineGap });
            ui::drawText(window, txt, panelRect);
        }
    }
}

int UI_resourceNodes::mineYield(ResourceType type, int level) {
    int base = 0;
    switch (type) {
        case ResourceType::WOOD:    base = Balance::WOOD_BASE_YIELD; break;
        case ResourceType::IRON:    base = Balance::IRON_BASE_YIELD; break;
        case ResourceType::COPPER:  base = Balance::COPPER_BASE_YIELD; break;
        case ResourceType::COAL:    base = Balance::COAL_BASE_YIELD; break;
        case ResourceType::SILICON: base = Balance::SILICON_BASE_YIELD; break;
        case ResourceType::SILVER:  base = Balance::SILVER_BASE_YIELD; break;
        case ResourceType::GOLD:    base = Balance::GOLD_BASE_YIELD; break;
        default: break;
    }
    return static_cast<int>(std::round(base * Balance::getMineYieldMultiplier(level)));
}

void UI_resourceNodes::drawNodes(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                                 const GameEngine* engine,
                                 const float* p1Cooldowns, const float* p2Cooldowns,
                                 float p1Cooldown, float p2Cooldown) {
    sf::Vector2f mousePos = ui::pointerPos(window);

    for (const auto& s : stations) {
        const float x = s.bounds.position.x;
        const float y = s.bounds.position.y;
        const float w = s.bounds.size.x;
        bool hover = s.bounds.contains(mousePos);
        int typeIdx = static_cast<int>(s.type);
        float cd = 0.0f;
        if (s.playerOwner == 1) {
            cd = (p1Cooldowns && typeIdx >= 0 && typeIdx < 12) ? p1Cooldowns[typeIdx] : p1Cooldown;
        } else {
            cd = (p2Cooldowns && typeIdx >= 0 && typeIdx < 12) ? p2Cooldowns[typeIdx] : p2Cooldown;
        }
        bool onCooldown = (cd > 0.0f);

        int lvl = engine ? engine->getMineLevel(s.playerOwner, s.type) : 1;
        int upCost = engine ? engine->getMineUpgradeCost(s.playerOwner, s.type) : 15;
        bool maxed = lvl >= Balance::MINE_MAX_LEVEL;

        // Station card: the resource's own hue on the outline and the top bar
        sf::RectangleShape card(s.bounds.size);
        card.setPosition(s.bounds.position);
        card.setFillColor(hover ? theme::withAlpha(theme::CardHover, 245) : theme::withAlpha(theme::Card, 235));
        card.setOutlineThickness(hover ? 2.0f : 1.2f);
        card.setOutlineColor(hover ? theme::Focus : s.themeColor);
        window.draw(card);
        ui::lint::solid(s.bounds);
        ui::lint::ContainerScope cardScope(s.bounds);

        sf::RectangleShape topBar({ w, 3.0f });
        topBar.setPosition(s.bounds.position);
        topBar.setFillColor(s.themeColor);
        window.draw(topBar);

        drawResourceIcon(window, s.type, { x + 12.0f, y + 13.0f }, 14.0f);

        // Upgrade button frame (drawn with or without a font so it stays visible)
        bool upHover = s.upgradeBtnBounds.contains(mousePos) ||
                       (s.bounds.contains(mousePos) && mousePos.y >= y + s.bounds.size.y - 26.0f);
        sf::RectangleShape upBtn(s.upgradeBtnBounds.size);
        upBtn.setPosition(s.upgradeBtnBounds.position);
        if (maxed) {
            upBtn.setFillColor(theme::withAlpha(theme::Well, 220));
            upBtn.setOutlineThickness(1.0f);
            upBtn.setOutlineColor(theme::Line);
        } else {
            upBtn.setFillColor(upHover ? theme::withAlpha(theme::Gold, 70) : theme::withAlpha(theme::Gold, 28));
            upBtn.setOutlineThickness(upHover ? 2.0f : 1.0f);
            upBtn.setOutlineColor(upHover ? theme::Focus : theme::withAlpha(theme::Gold, 200));
        }
        window.draw(upBtn);

        if (!fontLoaded) continue;

        // Row 1: station name (primary text; the colour is carried by the icon and the outline)
        sf::Text& nameText = ui::pooledText(font, toUtf8(s.nameBg), fontsize::Caption);
        nameText.setFillColor(theme::TextPrimary);
        nameText.setPosition({ x + 23.0f, y + 6.0f });
        ui::drawText(window, nameText);

        // Row 2: what one mining action gives at the current level
        std::string yieldStr = "+" + std::to_string(mineYield(s.type, lvl)) + " " + resourceNameBg(s.type);
        sf::Text& yieldText = ui::pooledText(font, toUtf8(yieldStr), fontsize::Label);
        yieldText.setFillColor(theme::TextPrimary);
        yieldText.setPosition({ x + 6.0f, y + 23.0f });
        ui::drawText(window, yieldText);

        // Row 3: ready / cooldown (left) and the mine level (right)
        std::string lvlStr = "Н" + std::to_string(lvl); // Н = ниво (level)
        sf::Text& tLvl = ui::pooledText(font, toUtf8(lvlStr), fontsize::Caption);
        tLvl.setFillColor(lvl > 1 ? theme::Info : theme::TextSecondary);
        sf::FloatRect lb = tLvl.getLocalBounds();
        tLvl.setPosition({ x + w - lb.size.x - 6.0f - lb.position.x, y + 40.0f });
        ui::drawText(window, tLvl);

        if (onCooldown) {
            char cdbuf[16];
            std::snprintf(cdbuf, sizeof(cdbuf), "%.1fс", cd);
            sf::Text& cdText = ui::pooledText(font, toUtf8(cdbuf), fontsize::Caption);
            cdText.setFillColor(theme::Warn);
            cdText.setPosition({ x + 6.0f, y + 40.0f });
            ui::drawText(window, cdText);

            float maxCd = Balance::getResourceMineCooldown(s.type);
            float cdRatio = std::min(1.0f, std::max(0.0f, cd / maxCd));
            sf::RectangleShape cdBar({ (w - 12.0f) * (1.0f - cdRatio), 2.0f });
            cdBar.setPosition({ x + 6.0f, y + 55.0f });
            cdBar.setFillColor(theme::Warn);
            window.draw(cdBar);
        } else {
            // The mining key is shown by the prompt tag over the station; the card only shows the state
            sf::Text& actText = ui::pooledText(font, toUtf8("готово"), fontsize::Caption);
            actText.setFillColor(theme::Good);
            actText.setPosition({ x + 6.0f, y + 40.0f });
            ui::drawText(window, actText);
        }

        // Upgrade button: next level and its gold price (gold coin icon)
        const sf::FloatRect& ub = s.upgradeBtnBounds;
        std::string upLabel = maxed ? "МАКС. НИВО" : ("Н" + std::to_string(lvl + 1) + " за " + std::to_string(upCost));
        sf::Text& tUp = ui::pooledText(font, toUtf8(upLabel), fontsize::Caption);
        tUp.setStyle(sf::Text::Bold);
        tUp.setFillColor(maxed ? theme::TextSecondary : theme::TextPrimary);
        sf::FloatRect upb = tUp.getLocalBounds();
        const float coin = maxed ? 0.0f : 12.0f;
        float contentW = upb.size.x + (maxed ? 0.0f : coin + 3.0f);
        float tx = ub.position.x + (ub.size.x - contentW) / 2.0f;
        tUp.setPosition({ tx - upb.position.x, ub.position.y + (ub.size.y - upb.size.y) / 2.0f - upb.position.y });
        ui::drawText(window, tUp, ub);
        if (!maxed) {
            drawResourceIcon(window, ResourceType::GOLD, { tx + upb.size.x + 3.0f + coin / 2.0f, ub.position.y + ub.size.y / 2.0f }, coin);
        }
    }
}
