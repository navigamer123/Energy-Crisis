#include "../includes/UI_buildings.h"
#include "../includes/UI_types.h"
#include <iostream>

UI_buildings::UI_buildings()
    : playerIndex(1),
      panelPos(20.0f, 125.0f),
      panelSize(225.0f, 350.0f),
      accentColor(sf::Color(0, 229, 255)) {
    setPlayer(1, panelPos, panelSize, accentColor);
}

UI_buildings::UI_buildings(int playerIdx, sf::Vector2f pos, sf::Vector2f size, sf::Color accent) {
    setPlayer(playerIdx, pos, size, accent);
}

void UI_buildings::setPlayer(int playerIdx, sf::Vector2f pos, sf::Vector2f size, sf::Color accent) {
    playerIndex = playerIdx;
    panelPos = pos;
    panelSize = size;
    accentColor = accent;

    buildings.clear();
    buildings.push_back({ BuildingType::SOLAR_PANEL, "Solar Panel", "Слънчев панел", 35, 30, 60, 0, {} });
    buildings.push_back({ BuildingType::WIND_TURBINE, "Wind Turbine", "Вятърна мелница", 50, 45, 90, 0, {} });
    buildings.push_back({ BuildingType::HYDRO_PLANT, "Hydro Plant", "ВЕЦ / Хидро", 85, 90, 180, 0, {} });
    buildings.push_back({ BuildingType::BATTERY, "Battery Storage", "Акумулатор / Батерия", 30, 60, 40, 0, {} });
}

BuildingType UI_buildings::handleClick(sf::Vector2f clickPos) {
    for (auto& b : buildings) {
        if (b.btnBounds.contains(clickPos)) {
            return b.type;
        }
    }
    return BuildingType::NONE;
}

void UI_buildings::draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                        sf::Vector2f mousePos, const PlayerEconomy& econ, BuildingType activeSelection) {
    // Panel container
    sf::RectangleShape panel(panelSize);
    panel.setPosition(panelPos);
    panel.setFillColor(sf::Color(18, 24, 36, 250));
    panel.setOutlineThickness(2.0f);
    panel.setOutlineColor(accentColor);
    window.draw(panel);

    if (fontLoaded) {
        std::string pTag = (playerIndex == 1) ? "ПОСТРОЙКИ (ИГРАЧ 1) [E]" : "ПОСТРОЙКИ (ИГРАЧ 2) [PgDn]";
        sf::Text tHeader(font, toUtf8(pTag), 12);
        tHeader.setFillColor(accentColor);
        tHeader.setPosition({ panelPos.x + 10.0f, panelPos.y + 6.0f });
        window.draw(tHeader);

        sf::RectangleShape div({ panelSize.x - 20.0f, 1.5f });
        div.setPosition({ panelPos.x + 10.0f, panelPos.y + 25.0f });
        div.setFillColor(sf::Color(70, 95, 130));
        window.draw(div);
    }

    float itemX = panelPos.x + 8.0f;
    float itemStartY = panelPos.y + 30.0f;
    float itemW = panelSize.x - 16.0f;
    float itemH = 72.0f;
    float spacing = 78.0f;

    for (size_t i = 0; i < buildings.size(); i++) {
        auto& b = buildings[i];
        float y = itemStartY + i * spacing;
        b.btnBounds = sf::FloatRect({ itemX, y }, { itemW, itemH });

        bool isSelected = (b.type == activeSelection);
        bool canAfford = (econ.wood >= b.woodCost && econ.ore >= b.oreCost);
        bool hover = b.btnBounds.contains(mousePos);

        // Card background
        sf::RectangleShape card(b.btnBounds.size);
        card.setPosition(b.btnBounds.position);
        if (isSelected) {
            card.setFillColor(sf::Color(45, 75, 95, 250));
            card.setOutlineThickness(2.5f);
            card.setOutlineColor(sf::Color(255, 215, 0));
        } else if (hover) {
            card.setFillColor(canAfford ? sf::Color(35, 65, 85, 240) : sf::Color(65, 30, 40, 240));
            card.setOutlineThickness(1.5f);
            card.setOutlineColor(canAfford ? sf::Color(100, 255, 180) : sf::Color(240, 90, 90));
        } else {
            card.setFillColor(sf::Color(26, 34, 48, 220));
            card.setOutlineThickness(1.0f);
            card.setOutlineColor(canAfford ? sf::Color(80, 110, 140) : sf::Color(80, 50, 60));
        }
        window.draw(card);

        if (fontLoaded) {
            // Building name
            sf::Text tName(font, toUtf8(b.bgName), 13);
            tName.setFillColor(isSelected ? sf::Color(255, 235, 120) : (hover ? sf::Color::White : sf::Color(235, 245, 255)));
            tName.setPosition({ itemX + 6.0f, y + 4.0f });
            window.draw(tName);

            // Power output badge
            std::string pStr = "+" + std::to_string(b.powerOutputMW) + " MW";
            sf::Text tPwr(font, toUtf8(pStr), 12);
            tPwr.setFillColor(sf::Color(255, 215, 0));
            sf::FloatRect pb = tPwr.getLocalBounds();
            tPwr.setPosition({ itemX + itemW - pb.size.x - 6.0f, y + 4.0f });
            window.draw(tPwr);

            // Cost line: Wood & Ore
            std::string cStr = "Дърво: " + std::to_string(b.woodCost) + " | Руда: " + std::to_string(b.oreCost);
            sf::Text tCost(font, toUtf8(cStr), 11);
            tCost.setFillColor(canAfford ? sf::Color(140, 215, 255) : sf::Color(255, 140, 140));
            tCost.setPosition({ itemX + 6.0f, y + 26.0f });
            window.draw(tCost);

            // Action button indicator
            sf::RectangleShape bBtn({ itemW - 12.0f, 20.0f });
            bBtn.setPosition({ itemX + 6.0f, y + 46.0f });
            if (isSelected) {
                bBtn.setFillColor(sf::Color(220, 160, 20));
                bBtn.setOutlineThickness(1.0f);
                bBtn.setOutlineColor(sf::Color::White);
            } else {
                bBtn.setFillColor(canAfford ? (hover ? sf::Color(45, 140, 75) : sf::Color(30, 95, 50)) : sf::Color(55, 38, 45));
                bBtn.setOutlineThickness(1.0f);
                bBtn.setOutlineColor(canAfford ? sf::Color(70, 220, 110) : sf::Color(90, 55, 65));
            }
            window.draw(bBtn);

            std::string btnText = isSelected ? "ИЗБРАНА - ПОСТАВИ НА ЗЕМЯТА" :
                                 (canAfford ? "ИЗБЕРИ ЗА СТРОЕЖ" : "НЕДОСТИГ НА РЕСУРСИ");
            sf::Text tBtn(font, toUtf8(btnText), 10);
            tBtn.setFillColor(sf::Color::White);
            sf::FloatRect bb = tBtn.getLocalBounds();
            tBtn.setPosition({ itemX + 6.0f + (itemW - 12.0f - bb.size.x) / 2.0f, y + 49.0f });
            window.draw(tBtn);
        }
    }
}
