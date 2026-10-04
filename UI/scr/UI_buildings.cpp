#include "../includes/UI_buildings.h"
#include "../includes/UI_input.h" // Team b-session (F-10): key hints from the real bindings
#include "../includes/UI_types.h"
#include "../includes/UI_text.h"
#include "../includes/UI_icons.h"
#include "../includes/UI_theme.h"
#include "../../Game/includes/game_balance.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

// -----------------------------------------------------------------------------
// Recipe helpers (also used for the "Недостигат: ..." build error)
// -----------------------------------------------------------------------------
std::vector<ResourceNeed> buildingNeeds(const PlayerEconomy& econ, const BuildingCost& cost) {
    std::vector<ResourceNeed> needs;
    auto add = [&](ResourceType t, int need, int have) {
        if (need > 0) needs.push_back({ t, need, have });
    };
    add(ResourceType::WOOD, cost.woodCost, econ.wood);
    add(ResourceType::IRON, cost.ironCost, econ.iron);
    add(ResourceType::COPPER, cost.copperCost, econ.copper);
    add(ResourceType::COAL, cost.coalCost, econ.coal);
    add(ResourceType::SILICON, cost.siliconCost, econ.silicon);
    add(ResourceType::SILVER, cost.silverCost, econ.silver);
    return needs;
}

const char* resourceNameBg(ResourceType type) {
    switch (type) {
        case ResourceType::WOOD:    return "дърво";
        case ResourceType::IRON:    return "желязо";
        case ResourceType::COPPER:  return "мед";
        case ResourceType::COAL:    return "въглища";
        case ResourceType::SILICON: return "силиций";
        case ResourceType::SILVER:  return "сребро";
        case ResourceType::GOLD:    return "злато";
        case ResourceType::MONEY:   return "пари";
        case ResourceType::ENERGY:  return "ток";
        default:                    return "";
    }
}

std::string recipeText(const BuildingCost& cost) {
    std::string list;
    for (const auto& n : buildingNeeds(PlayerEconomy(), cost)) {
        if (!list.empty()) list += ", ";
        list += std::to_string(n.need) + " " + resourceNameBg(n.type);
    }
    return list;
}

std::string missingResourcesText(const PlayerEconomy& econ, const BuildingCost& cost) {
    std::string list;
    for (const auto& n : buildingNeeds(econ, cost)) {
        if (n.have >= n.need) continue;
        if (!list.empty()) list += ", ";
        list += std::to_string(n.need - n.have) + " " + resourceNameBg(n.type);
    }
    return list.empty() ? std::string() : "Недостигат: " + list;
}

// -----------------------------------------------------------------------------
// Panel
// -----------------------------------------------------------------------------
namespace {

constexpr float HEADER_FRAC = 0.071f; // header strip height as a fraction of the panel height
constexpr float ICON_BOX = 40.0f;     // building icon box on the left of each card

std::string formatMultiplier(float m) {
    char buf[16];
    if (m <= 0.0f) return "×0";
    std::snprintf(buf, sizeof(buf), "×%.1f", static_cast<double>(m));
    return buf;
}

} // namespace

UI_buildings::UI_buildings()
    : playerIndex(1), panelPos(18.0f, 115.0f), panelSize(230.0f, 395.0f),
      accentColor(theme::P1) {
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
  hotkeys = (playerIdx == 1) ? BuildHotkeys::DIGITS : BuildHotkeys::NUMPAD;

  // Order matches the engine's selection numbers 1..6 (and the hotkeys)
  buildings = {
      { BuildingType::SOLAR_PANEL, "Панел", {} },
      { BuildingType::WIND_TURBINE, "Турбина", {} },
      { BuildingType::HYDRO_PLANT, "ВЕЦ", {} },
      { BuildingType::BATTERY, "Батерия", {} },
      { BuildingType::LAMP, "Лампа", {} },
      { BuildingType::DEMOLISH, "Събаряне", {} },
  };

  // Pre-calculate proportional button bounds for click collision
  float marginX = panelSize.x * 0.026f;
  float itemX = panelPos.x + marginX;
  float itemW = panelSize.x - 2.0f * marginX;
  float headerH = panelSize.y * HEADER_FRAC;
  float itemStartY = panelPos.y + headerH;
  float availH = panelSize.y - headerH - panelSize.y * 0.018f;
  float spacing = availH / static_cast<float>(std::max<size_t>(1, buildings.size()));
  float itemH = spacing * 0.916f;

  for (size_t i = 0; i < buildings.size(); i++) {
    buildings[i].btnBounds = sf::FloatRect({itemX, itemStartY + i * spacing}, {itemW, itemH});
  }
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
                        const GameEngine &engine,
                        BuildingType activeSelection) {
  const PlayerEconomy &econ = engine.getPlayerEconomy(playerIndex);
  const WeatherType weather = engine.getPlayerWeather(playerIndex);

  // Panel container
  sf::RectangleShape panel(panelSize);
  panel.setPosition(panelPos);
  panel.setFillColor(theme::withAlpha(theme::Panel, 252));
  panel.setOutlineThickness(2.0f);
  panel.setOutlineColor(accentColor);
  window.draw(panel);
  ui::lint::solid(sf::FloatRect(panelPos, panelSize)); // no map text may hide under the panel
  ui::lint::ContainerScope panelScope(sf::FloatRect(panelPos, panelSize));

  if (fontLoaded) {
    std::string pTag = std::string(playerIndex == 1 ? "ПОСТРОЙКИ (ИГРАЧ 1) " : "ПОСТРОЙКИ (ИГРАЧ 2) ") +
                       inputRouter().hint(playerIndex, InputAction::NextBuilding, false, 1); // b-session (F-10)
    sf::Text tHeader(font, toUtf8(pTag), fontsize::Label);
    tHeader.setStyle(sf::Text::Bold);
    tHeader.setFillColor(accentColor);
    tHeader.setPosition({panelPos.x + 8.0f, panelPos.y + 6.0f});
    ui::drawText(window, tHeader);

    sf::RectangleShape div({panelSize.x - 16.0f, 1.5f});
    div.setPosition({panelPos.x + 8.0f, panelPos.y + 24.0f});
    div.setFillColor(theme::Line);
    window.draw(div);
  }

  for (size_t i = 0; i < buildings.size(); i++) {
    auto &b = buildings[i];
    const sf::FloatRect &r = b.btnBounds;
    const float x = r.position.x;
    const float y = r.position.y;
    const float w = r.size.x;
    const float h = r.size.y;

    const BuildingCost cost = engine.getBuildingCost(b.type);
    const std::vector<ResourceNeed> needs = buildingNeeds(econ, cost);
    const bool isDemolish = (b.type == BuildingType::DEMOLISH);
    const bool canAfford = std::all_of(needs.begin(), needs.end(),
                                       [](const ResourceNeed &n) { return n.have >= n.need; });
    const bool isSelected = (b.type == activeSelection);
    const bool hover = r.contains(mousePos);

    // Card background: selection = white outline (gold is reserved for money)
    sf::RectangleShape card(r.size);
    card.setPosition(r.position);
    if (isSelected) {
      card.setFillColor(theme::withAlpha(theme::CardSelected, 250));
      card.setOutlineThickness(2.0f);
      card.setOutlineColor(theme::Focus);
    } else if (hover) {
      card.setFillColor(theme::withAlpha(theme::CardHover, 245));
      card.setOutlineThickness(1.5f);
      card.setOutlineColor(canAfford ? theme::Good : theme::Bad);
    } else {
      card.setFillColor(theme::withAlpha(theme::Card, 230));
      card.setOutlineThickness(1.0f);
      card.setOutlineColor(canAfford ? theme::Line : theme::withAlpha(theme::Bad, 110));
    }
    window.draw(card);
    ui::lint::ContainerScope cardScope(r);

    // Building icon in a small well on the left
    sf::RectangleShape well({ICON_BOX, ICON_BOX});
    well.setPosition({x + 5.0f, y + (h - ICON_BOX) / 2.0f});
    well.setFillColor(theme::withAlpha(theme::Well, 230));
    well.setOutlineThickness(1.0f);
    well.setOutlineColor(isSelected ? theme::Focus : theme::Line);
    window.draw(well);
    drawBuildingIcon(window, b.type, {x + 5.0f + ICON_BOX / 2.0f, y + h / 2.0f}, ICON_BOX - 8.0f);

    if (!fontLoaded) continue;

    const float textX = x + ICON_BOX + 12.0f;
    const float rightX = x + w - 6.0f;

    // Row 1: short name + output badge
    sf::Text tName(font, toUtf8(b.shortName), fontsize::Label);
    tName.setStyle(sf::Text::Bold);
    tName.setFillColor(isDemolish ? theme::Bad : theme::TextPrimary);
    tName.setPosition({textX, y + 3.0f});
    ui::drawText(window, tName);

    std::string badge;
    sf::Color badgeColor = theme::Energy;
    if (isDemolish) {
      badge = "-50%";
      badgeColor = theme::Bad;
    } else if (b.type == BuildingType::LAMP) {
      badge = "-" + std::to_string(static_cast<int>(GameEngine::LAMP_POWER_MW)) + " MW";
      badgeColor = theme::Warn;
    } else if (b.type == BuildingType::BATTERY) {
      badge = std::to_string(Balance::BATTERY.batteryCapacityMWh) + " MWh";
      badgeColor = theme::Good;
    } else {
      badge = "+" + std::to_string(cost.basePowerMW) + " MW";
    }
    sf::Text tBadge(font, toUtf8(badge), fontsize::Label);
    tBadge.setStyle(sf::Text::Bold);
    tBadge.setFillColor(badgeColor);
    sf::FloatRect bb = tBadge.getLocalBounds();
    tBadge.setPosition({rightX - bb.size.x - bb.position.x, y + 3.0f});
    ui::drawText(window, tBadge);

    // Row 2: costs as amount + resource icon, each green (enough) or red (missing)
    const float costY = y + 20.0f;
    if (isDemolish) {
      sf::Text tInfo(font, toUtf8("Връща половината ресурси"), fontsize::Caption);
      tInfo.setFillColor(theme::TextSecondary);
      tInfo.setPosition({textX, costY + 1.0f});
      ui::drawText(window, tInfo);
    } else {
      float cx = textX;
      for (const auto &n : needs) {
        sf::Text tNum(font, std::to_string(n.need), fontsize::Label);
        tNum.setFillColor(n.have >= n.need ? theme::Good : theme::Bad);
        tNum.setStyle(sf::Text::Bold);
        tNum.setPosition({cx, costY});
        ui::drawText(window, tNum);
        cx += tNum.getLocalBounds().position.x + tNum.getLocalBounds().size.x + 2.0f;
        drawResourceIcon(window, n.type, {cx + 7.0f, costY + 8.0f}, 14.0f);
        cx += 14.0f + 6.0f;
      }
    }

    // Row 3: how many the player owns and their output right now (+ multiplier) | hotkey
    int count = 0;
    float liveMW = 0.0f;
    float storedMWh = 0.0f;
    for (const auto &pb : engine.getBuildings()) {
      if (pb.playerOwner != playerIndex || pb.type != b.type) continue;
      ++count;
      liveMW += pb.currentOutputMW;
      storedMWh += pb.energyStored;
    }
    float mult = -1.0f;
    if (b.type == BuildingType::SOLAR_PANEL)
      mult = WeatherSystem::getSolarMultiplier(weather, engine.getHour24(), engine.getSeason());
    else if (b.type == BuildingType::WIND_TURBINE)
      mult = WeatherSystem::getWindMultiplier(weather, engine.getHour24());
    else if (b.type == BuildingType::HYDRO_PLANT)
      mult = WeatherSystem::getHydroMultiplier(weather);

    std::string hotkeyStr;
    if (hotkeys == BuildHotkeys::DIGITS) hotkeyStr = "[" + std::to_string(i + 1) + "]";
    else if (hotkeys == BuildHotkeys::NUMPAD) hotkeyStr = "[Num" + std::to_string(i + 1) + "]";

    float hotkeyW = 0.0f;
    const float statusY = y + 37.0f;
    if (!hotkeyStr.empty()) {
      sf::Text tKey(font, toUtf8(hotkeyStr), fontsize::Caption);
      tKey.setFillColor(theme::TextMuted);
      sf::FloatRect kb = tKey.getLocalBounds();
      hotkeyW = kb.size.x + 6.0f;
      tKey.setPosition({rightX - kb.size.x - kb.position.x, statusY});
      ui::drawText(window, tKey);
    }

    std::string status;
    if (!isDemolish) {
      status = std::to_string(count) + " бр.";
      if (b.type == BuildingType::BATTERY) {
        if (count > 0) status += " · " + std::to_string(static_cast<int>(std::lround(storedMWh))) + " MWh";
      } else if (count > 0) {
        status += " · " + std::to_string(static_cast<int>(std::lround(liveMW))) + " MW";
      }
      if (mult >= 0.0f) {
        std::string withMult = status + " · " + formatMultiplier(mult);
        if (ui::measureText(font, withMult, fontsize::Caption) <= (rightX - textX) - hotkeyW) status = withMult;
      }
    }
    if (!status.empty()) {
      sf::Text tStatus(font, toUtf8(status), fontsize::Caption);
      tStatus.setFillColor(count > 0 ? theme::TextSecondary : theme::TextMuted);
      tStatus.setPosition({textX, statusY});
      ui::drawText(window, tStatus);
    }
  }
}
