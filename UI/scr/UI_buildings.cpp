#include "../includes/UI_buildings.h"
#include "../includes/UI_types.h"
#include "../../Game/includes/game_balance.h"
#include <iostream>

UI_buildings::UI_buildings()
    : playerIndex(1), panelPos(18.0f, 115.0f), panelSize(230.0f, 395.0f),
      accentColor(sf::Color(0, 229, 255)) {
  setPlayer(1, panelPos, panelSize, accentColor);
}

UI_buildings::UI_buildings(int playerIdx, sf::Vector2f pos, sf::Vector2f size,
                           sf::Color accent) {
  setPlayer(playerIdx, pos, size, accent);
}

void UI_buildings::setPlayer(int playerIdx, sf::Vector2f pos, sf::Vector2f size,
                             sf::Color accent) {
  playerIndex = playerIdx;
  panelPos = pos;
  panelSize = size;
  accentColor = accent;

  buildings.clear();
  const auto& sp = Balance::SOLAR_PANEL;
  buildings.push_back({BuildingType::SOLAR_PANEL, sp.nameEn, sp.nameBg,
                       sp.woodCost, sp.ironCost, sp.copperCost, sp.coalCost,
                       sp.siliconCost, sp.silverCost,
                       sp.ironCost + sp.copperCost + sp.siliconCost,
                       sp.basePowerMW, 0, {}});

  const auto& wt = Balance::WIND_TURBINE;
  buildings.push_back({BuildingType::WIND_TURBINE, wt.nameEn, wt.nameBg,
                       wt.woodCost, wt.ironCost, wt.copperCost, wt.coalCost,
                       wt.siliconCost, wt.silverCost,
                       wt.ironCost + wt.copperCost + wt.coalCost,
                       wt.basePowerMW, 0, {}});

  const auto& hp = Balance::HYDRO_PLANT;
  buildings.push_back({BuildingType::HYDRO_PLANT, hp.nameEn, hp.nameBg,
                       hp.woodCost, hp.ironCost, hp.copperCost, hp.coalCost,
                       hp.siliconCost, hp.silverCost,
                       hp.ironCost + hp.copperCost + hp.siliconCost,
                       hp.basePowerMW, 0, {}});

  const auto& bs = Balance::BATTERY;
  buildings.push_back({BuildingType::BATTERY, bs.nameEn, bs.nameBg,
                       bs.woodCost, bs.ironCost, bs.copperCost, bs.coalCost,
                       bs.siliconCost, bs.silverCost,
                       bs.ironCost + bs.copperCost + bs.coalCost + bs.silverCost,
                       bs.basePowerMW, 0, {}});

  const auto& sl = Balance::STREET_LAMP;
  buildings.push_back({BuildingType::LAMP, sl.nameEn, sl.nameBg,
                       sl.woodCost, sl.ironCost, sl.copperCost, sl.coalCost,
                       sl.siliconCost, sl.silverCost,
                       sl.ironCost + sl.copperCost,
                       sl.basePowerMW, 0, {}});

  buildings.push_back({BuildingType::DEMOLISH, "Demolish Tool",
                       "Премахване / Разруши", 0, 0, 0, 0, 0, 0, 0, 0, 0, {}});
}

BuildingType UI_buildings::handleClick(sf::Vector2f clickPos) {
  for (auto &b : buildings) {
    if (b.btnBounds.contains(clickPos)) {
      return b.type;
    }
  }
  return BuildingType::NONE;
}

void UI_buildings::draw(sf::RenderWindow &window, const sf::Font &font,
                        bool fontLoaded, sf::Vector2f mousePos,
                        const PlayerEconomy &econ,
                        BuildingType activeSelection) {
  // Panel container
  sf::RectangleShape panel(panelSize);
  panel.setPosition(panelPos);
  panel.setFillColor(sf::Color(16, 22, 34, 252));
  panel.setOutlineThickness(2.0f);
  panel.setOutlineColor(accentColor);
  window.draw(panel);

  if (fontLoaded) {
    std::string pTag = (playerIndex == 1) ? "ПОСТРОЙКИ (ИГРАЧ 1) [E]"
                                          : "ПОСТРОЙКИ (ИГРАЧ 2) [PgDn]";
    sf::Text tHeader(font, toUtf8(pTag), 12);
    tHeader.setFillColor(accentColor);
    tHeader.setPosition({panelPos.x + 8.0f, panelPos.y + 6.0f});
    window.draw(tHeader);

    sf::RectangleShape div({panelSize.x - 16.0f, 1.5f});
    div.setPosition({panelPos.x + 8.0f, panelPos.y + 24.0f});
    div.setFillColor(sf::Color(65, 88, 120));
    window.draw(div);
  }

  float itemX = panelPos.x + 6.0f;
  float itemStartY = panelPos.y + 28.0f;
  float itemW = panelSize.x - 12.0f;
  float itemH = 55.0f;
  float spacing = 60.0f;

  for (size_t i = 0; i < buildings.size(); i++) {
    auto &b = buildings[i];
    float y = itemStartY + i * spacing;
    b.btnBounds = sf::FloatRect({itemX, y}, {itemW, itemH});

    bool isSelected = (b.type == activeSelection);
    bool canAfford =
        (b.type == BuildingType::DEMOLISH)
            ? true
            : (econ.wood >= b.woodCost &&
               (econ.iron >= b.ironCost || econ.ore >= b.oreCost) &&
               (b.copperCost == 0 || econ.copper >= b.copperCost ||
                econ.ore >= b.oreCost) &&
               (b.coalCost == 0 || econ.coal >= b.coalCost ||
                econ.ore >= b.oreCost) &&
               (b.siliconCost == 0 || econ.silicon >= b.siliconCost ||
                econ.ore >= b.oreCost) &&
               (b.silverCost == 0 || econ.silver >= b.silverCost ||
                econ.ore >= b.oreCost));
    bool hover = b.btnBounds.contains(mousePos);

    // Card background
    sf::RectangleShape card(b.btnBounds.size);
    card.setPosition(b.btnBounds.position);
    if (isSelected) {
      card.setFillColor(sf::Color(42, 68, 92, 250));
      card.setOutlineThickness(2.0f);
      card.setOutlineColor(sf::Color(255, 215, 0));
    } else if (hover) {
      card.setFillColor(canAfford ? sf::Color(30, 56, 78, 240)
                                  : sf::Color(55, 28, 38, 240));
      card.setOutlineThickness(1.5f);
      card.setOutlineColor(canAfford ? sf::Color(100, 255, 180)
                                     : sf::Color(240, 90, 90));
    } else {
      card.setFillColor(sf::Color(22, 30, 44, 220));
      card.setOutlineThickness(1.0f);
      card.setOutlineColor(canAfford ? sf::Color(70, 95, 125)
                                     : sf::Color(70, 45, 55));
    }
    window.draw(card);

    if (fontLoaded) {
      // Line 1: Building name (clear 11pt, never overlaps badge)
      sf::Text tName(font, toUtf8(b.bgName), 11);
      if (b.type == BuildingType::DEMOLISH) {
        tName.setFillColor(isSelected ? sf::Color(255, 215, 0)
                                      : sf::Color(255, 140, 140));
      } else if (isSelected) {
        tName.setFillColor(sf::Color(255, 235, 120));
      } else {
        tName.setFillColor(hover ? sf::Color::White : sf::Color(220, 235, 250));
      }
      tName.setPosition({itemX + 6.0f, y + 3.0f});
      window.draw(tName);

      // Line 1 Right: Output or Type Badge
      std::string pStr;
      sf::Color badgeColor = sf::Color(255, 215, 0);
      if (b.type == BuildingType::DEMOLISH) {
        pStr = "[-50% Връща]";
        badgeColor = sf::Color(255, 120, 120);
      } else if (b.type == BuildingType::LAMP) {
        pStr = "[Нощ: 150px]";
        badgeColor = sf::Color(255, 230, 120);
      } else if (b.type == BuildingType::BATTERY) {
        pStr = "[200 MWh]";
        badgeColor = sf::Color(100, 240, 180);
      } else {
        pStr = "+" + std::to_string(b.powerOutputMW) + " MW";
      }
      sf::Text tPwr(font, toUtf8(pStr), 11);
      tPwr.setFillColor(badgeColor);
      sf::FloatRect pb = tPwr.getLocalBounds();
      tPwr.setPosition({itemX + itemW - pb.size.x - 6.0f, y + 3.0f});
      window.draw(tPwr);

      // Line 2: Cost description
      std::string cStr;
      if (b.type == BuildingType::DEMOLISH) {
        cStr = "Кликнете върху ваша сграда";
      } else {
        cStr = std::to_string(b.woodCost) + " Дър";
        if (b.ironCost > 0)
          cStr += " " + std::to_string(b.ironCost) + " Жел";
        if (b.copperCost > 0)
          cStr += " " + std::to_string(b.copperCost) + " Мед";
        if (b.siliconCost > 0)
          cStr += " " + std::to_string(b.siliconCost) + " Сил";
        if (b.coalCost > 0)
          cStr += " " + std::to_string(b.coalCost) + " Въгл";
        if (b.silverCost > 0)
          cStr += " " + std::to_string(b.silverCost) + " Среб";
        if (b.type == BuildingType::LAMP)
          cStr += " (10MW)";
      }
      sf::Text tCost(font, toUtf8(cStr), 10);
      tCost.setFillColor(canAfford ? sf::Color(140, 210, 250)
                                   : sf::Color(255, 130, 130));
      tCost.setPosition({itemX + 6.0f, y + 19.0f});
      window.draw(tCost);

      // Line 3: Compact Action Button Indicator
      sf::RectangleShape bBtn({itemW - 12.0f, 16.0f});
      bBtn.setPosition({itemX + 6.0f, y + 35.0f});
      if (isSelected) {
        bBtn.setFillColor(sf::Color(210, 150, 15));
        bBtn.setOutlineThickness(1.0f);
        bBtn.setOutlineColor(sf::Color::White);
      } else if (b.type == BuildingType::DEMOLISH) {
        bBtn.setFillColor(hover ? sf::Color(160, 45, 55)
                                : sf::Color(110, 30, 40));
        bBtn.setOutlineThickness(1.0f);
        bBtn.setOutlineColor(sf::Color(220, 80, 90));
      } else {
        bBtn.setFillColor(
            canAfford ? (hover ? sf::Color(40, 130, 70) : sf::Color(28, 90, 48))
                      : sf::Color(50, 32, 40));
        bBtn.setOutlineThickness(1.0f);
        bBtn.setOutlineColor(canAfford ? sf::Color(65, 210, 105)
                                       : sf::Color(80, 48, 58));
      }
      window.draw(bBtn);

      std::string btnText;
      if (isSelected) {
        btnText = (b.type == BuildingType::DEMOLISH)
                      ? "ИЗБРАНО - КЛИКНИ СГРАДА ЗА МАХАНЕ"
                      : "ИЗБРАНА - КЛИКНИ ЗА СТРОЕЖ";
      } else if (b.type == BuildingType::DEMOLISH) {
        btnText = "ИЗБЕРИ ЗА РАЗРУШАВАНЕ";
      } else {
        btnText = canAfford ? "ИЗБЕРИ ЗА СТРОЕЖ" : "НЕДОСТИГ НА РЕСУРСИ";
      }
      sf::Text tBtn(font, toUtf8(btnText), 9);
      tBtn.setFillColor(sf::Color::White);
      sf::FloatRect bb = tBtn.getLocalBounds();
      tBtn.setPosition(
          {itemX + 6.0f + (itemW - 12.0f - bb.size.x) / 2.0f, y + 36.0f});
      window.draw(tBtn);
    }
  }
}
