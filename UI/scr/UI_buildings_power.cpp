// =============================================================================
// Team b-power: page 2 of the building panel — reactor, geothermal plant and
// the three mega-projects (+ Demolish). Same card layout as page 1, with a
// procedural icon, the lock reason on the button and auto-fitted cost lines.
// The page follows the selection (E / PgDn cycle, 7 / 8 / 9) and the "1/2"
// tab in the header switches pages with the mouse.
// =============================================================================
#include "../includes/UI_buildings.h"
#include "../includes/UI_power.h"
#include "../includes/UI_types.h"
#include <algorithm>

namespace {

// Colours (the integrator maps these to UI_theme tokens)
const sf::Color BP_PANEL(16, 22, 34, 252);
const sf::Color BP_DIVIDER(65, 88, 120);
const sf::Color BP_CARD(22, 30, 44, 220);
const sf::Color BP_CARD_SELECTED(42, 68, 92, 250);
const sf::Color BP_GOLD(255, 215, 0);
const sf::Color BP_TEXT(220, 235, 250);
const sf::Color BP_COST_OK(140, 210, 250);
const sf::Color BP_COST_SHORT(255, 130, 130);
const sf::Color BP_LOCKED(255, 170, 90);
const sf::Color BP_MEGA(255, 200, 80);

bool isAdvancedType(BuildingType t) {
    return t == BuildingType::NUCLEAR || t == BuildingType::GEOTHERMAL || GameEngine::isMegaProject(t);
}

std::string costLine(const BuildingTypeInfo& b) {
    std::string s;
    auto add = [&](int v, const char* tag) {
        if (v <= 0) return;
        if (!s.empty()) s += " ";
        s += std::to_string(v) + " " + tag;
    };
    add(b.woodCost, "Дър");
    add(b.ironCost, "Жел");
    add(b.copperCost, "Мед");
    add(b.siliconCost, "Сил");
    add(b.coalCost, "Въгл");
    add(b.silverCost, "Среб");
    return s;
}

// Text that shrinks (down to 8 pt) until it fits `maxWidth`
sf::Text fittedText(const sf::Font& font, const std::string& s, unsigned size, float maxWidth) {
    sf::Text t(font, toUtf8(s), size);
    while (size > 8 && t.getLocalBounds().size.x > maxWidth) {
        --size;
        t.setCharacterSize(size);
    }
    return t;
}

} // namespace

void UI_buildings::setupAdvancedCards() {
    advancedCards.clear();
    auto add = [&](BuildingType t, const Balance::BuildingDef& d) {
        BuildingTypeInfo info;
        info.type = t;
        info.name = d.nameEn;
        info.bgName = d.nameBg;
        info.woodCost = d.woodCost;
        info.ironCost = d.ironCost;
        info.copperCost = d.copperCost;
        info.coalCost = d.coalCost;
        info.siliconCost = d.siliconCost;
        info.silverCost = d.silverCost;
        info.oreCost = d.ironCost + d.copperCost + d.siliconCost;
        info.powerOutputMW = d.basePowerMW;
        advancedCards.push_back(info);
    };
    add(BuildingType::NUCLEAR, PowerBalance::NUCLEAR);
    add(BuildingType::GEOTHERMAL, PowerBalance::GEOTHERMAL);
    add(BuildingType::MEGA_FUSION, PowerBalance::MEGA_FUSION);
    add(BuildingType::MEGA_SPACE_SOLAR, PowerBalance::MEGA_SPACE_SOLAR);
    add(BuildingType::MEGA_PUMPED_HYDRO, PowerBalance::MEGA_PUMPED_HYDRO);
    BuildingTypeInfo demolish;
    demolish.type = BuildingType::DEMOLISH;
    demolish.name = "Demolish Tool";
    demolish.bgName = "Премахване / Разруши";
    advancedCards.push_back(demolish);

    float marginX = panelSize.x * 0.026f;
    float itemX = panelPos.x + marginX;
    float itemW = panelSize.x - 2.0f * marginX;
    float headerH = panelSize.y * 0.071f;
    float itemStartY = panelPos.y + headerH;
    float availH = panelSize.y - headerH - panelSize.y * 0.018f;
    float spacing = availH / static_cast<float>(std::max<size_t>(1, advancedCards.size()));
    float itemH = spacing * 0.916f;
    for (size_t i = 0; i < advancedCards.size(); i++) {
        advancedCards[i].btnBounds = sf::FloatRect({ itemX, itemStartY + i * spacing }, { itemW, itemH });
    }
    pageTabBounds = sf::FloatRect({ panelPos.x + panelSize.x - 30.0f, panelPos.y + 4.0f }, { 26.0f, 18.0f });
}

void UI_buildings::syncPageWithSelection(BuildingType activeSelection) {
    if (activeSelection == lastSelection) return;
    lastSelection = activeSelection;
    if (isAdvancedType(activeSelection)) advancedPage = true;
    else if (activeSelection != BuildingType::NONE && activeSelection != BuildingType::DEMOLISH) advancedPage = false;
}

BuildingType UI_buildings::handleAdvancedClick(sf::Vector2f clickPos) {
    for (const auto& b : advancedCards) {
        if (b.btnBounds.contains(clickPos)) return b.type;
    }
    return BuildingType::NONE;
}

void UI_buildings::drawPageTab(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded, sf::Vector2f mousePos) {
    bool hover = pageTabBounds.contains(mousePos);
    sf::RectangleShape tab(pageTabBounds.size);
    tab.setPosition(pageTabBounds.position);
    tab.setFillColor(hover ? sf::Color(40, 70, 95) : sf::Color(24, 36, 52));
    tab.setOutlineThickness(1.0f);
    tab.setOutlineColor(advancedPage ? BP_MEGA : accentColor);
    window.draw(tab);
    if (!fontLoaded) return;
    sf::Text t(font, toUtf8(advancedPage ? "2/2" : "1/2"), 10);
    t.setFillColor(hover ? sf::Color::White : (advancedPage ? BP_MEGA : accentColor));
    sf::FloatRect b = t.getLocalBounds();
    t.setPosition({ std::round(pageTabBounds.position.x + (pageTabBounds.size.x - b.size.x) * 0.5f - b.position.x),
                    std::round(pageTabBounds.position.y + 2.0f) });
    window.draw(t);
}

void UI_buildings::drawAdvancedPage(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded, sf::Vector2f mousePos,
                                    const PlayerEconomy& econ, BuildingType activeSelection) {
    sf::RectangleShape panel(panelSize);
    panel.setPosition(panelPos);
    panel.setFillColor(BP_PANEL);
    panel.setOutlineThickness(2.0f);
    panel.setOutlineColor(accentColor);
    window.draw(panel);

    if (fontLoaded) {
        std::string tag = (playerIndex == 1) ? "НАПРЕДНАЛИ (ИГРАЧ 1) [7-9]" : "НАПРЕДНАЛИ (ИГРАЧ 2) [PgDn]";
        sf::Text header = fittedText(font, tag, 12, panelSize.x - 46.0f);
        header.setFillColor(BP_MEGA);
        header.setPosition({ panelPos.x + 8.0f, panelPos.y + 6.0f });
        window.draw(header);
        sf::RectangleShape div({ panelSize.x - 16.0f, 1.5f });
        div.setPosition({ panelPos.x + 8.0f, panelPos.y + 24.0f });
        div.setFillColor(BP_DIVIDER);
        window.draw(div);
    }

    for (auto& b : advancedCards) {
        const sf::FloatRect& r = b.btnBounds;
        float x = r.position.x, y = r.position.y, w = r.size.x;
        bool isSelected = (b.type == activeSelection);
        bool hover = r.contains(mousePos);
        bool isDemolish = (b.type == BuildingType::DEMOLISH);
        bool canAfford = isDemolish || (econ.wood >= b.woodCost && econ.iron >= b.ironCost && econ.copper >= b.copperCost &&
                                        econ.coal >= b.coalCost && econ.silicon >= b.siliconCost && econ.silver >= b.silverCost);

        // Lock reason (short, fits the button)
        std::string lock;
        if (engineView != nullptr && !isDemolish) {
            if (b.type == BuildingType::NUCLEAR) {
                int owned = engineView->countOwnedPlots(playerIndex);
                if (owned < PowerBalance::NUCLEAR_UNLOCK_PLOTS) {
                    lock = "НУЖНИ " + std::to_string(PowerBalance::NUCLEAR_UNLOCK_PLOTS) + " ПАРЦЕЛА (" + std::to_string(owned) + "/" +
                           std::to_string(PowerBalance::NUCLEAR_UNLOCK_PLOTS) + ")";
                } else if (engineView->getReactor(playerIndex) != nullptr) {
                    lock = "ВЕЧЕ ИМАТЕ АЕЦ";
                }
            } else if (GameEngine::isMegaProject(b.type)) {
                if (engineView->getCurrentDay() < PowerBalance::MEGA_UNLOCK_DAY) {
                    lock = "ОТКЛЮЧВА СЕ ОТ ДЕН " + std::to_string(PowerBalance::MEGA_UNLOCK_DAY);
                } else if (engineView->getMegaProject(playerIndex) != nullptr) {
                    lock = "ВЕЧЕ ИМАТЕ МЕГАПРОЕКТ";
                }
            }
        }
        bool locked = !lock.empty();

        sf::RectangleShape card(r.size);
        card.setPosition(r.position);
        if (isSelected) {
            card.setFillColor(BP_CARD_SELECTED);
            card.setOutlineThickness(2.0f);
            card.setOutlineColor(BP_GOLD);
        } else if (hover) {
            bool ok = canAfford && !locked;
            card.setFillColor(ok ? sf::Color(30, 56, 78, 240) : sf::Color(55, 28, 38, 240));
            card.setOutlineThickness(1.5f);
            card.setOutlineColor(ok ? sf::Color(100, 255, 180) : sf::Color(240, 90, 90));
        } else {
            card.setFillColor(BP_CARD);
            card.setOutlineThickness(1.0f);
            card.setOutlineColor((canAfford && !locked) ? sf::Color(70, 95, 125) : sf::Color(70, 45, 55));
        }
        window.draw(card);

        float textX = x + 6.0f;
        if (!isDemolish) {
            // Icon tile on the left of the first two lines
            sf::RectangleShape tile({ 26.0f, 28.0f });
            tile.setPosition({ x + 4.0f, y + 4.0f });
            tile.setFillColor(sf::Color(12, 18, 28, 230));
            window.draw(tile);
            drawPowerBuildingIcon(window, b.type, { x + 17.0f, y + 18.0f }, 24.0f);
            textX = x + 35.0f;
        }
        if (!fontLoaded) continue;

        // Line 1: name + output badge
        std::string badge;
        sf::Color badgeColor = BP_GOLD;
        if (isDemolish) {
            badge = "[-50% Връща]";
            badgeColor = sf::Color(255, 120, 120);
        } else if (b.type == BuildingType::MEGA_PUMPED_HYDRO) {
            badge = "[" + std::to_string(PowerBalance::MEGA_PUMPED_HYDRO.batteryCapacityMWh) + " MWh]";
            badgeColor = sf::Color(100, 240, 180);
        } else {
            badge = "+" + std::to_string(b.powerOutputMW) + " MW";
        }
        sf::Text tPwr(font, toUtf8(badge), 11);
        tPwr.setFillColor(badgeColor);
        float pw = tPwr.getLocalBounds().size.x;
        tPwr.setPosition({ x + w - pw - 6.0f, y + 3.0f });
        window.draw(tPwr);

        sf::Text tName = fittedText(font, b.bgName, 11, (x + w - pw - 10.0f) - textX);
        if (isDemolish) tName.setFillColor(isSelected ? BP_GOLD : sf::Color(255, 140, 140));
        else tName.setFillColor(isSelected ? sf::Color(255, 235, 120) : (hover ? sf::Color::White : BP_TEXT));
        tName.setPosition({ textX, y + 3.0f });
        window.draw(tName);

        // Line 2: cost (auto-fitted)
        std::string cost = isDemolish ? "Кликнете върху ваша сграда" : costLine(b);
        sf::Text tCost = fittedText(font, cost, 10, (x + w - 6.0f) - textX);
        tCost.setFillColor((canAfford || isDemolish) ? BP_COST_OK : BP_COST_SHORT);
        tCost.setPosition({ textX, y + 19.0f });
        window.draw(tCost);

        // Line 3: action / lock button
        sf::RectangleShape btn({ w - 12.0f, 16.0f });
        btn.setPosition({ x + 6.0f, y + 35.0f });
        std::string btnText;
        if (isSelected) {
            btn.setFillColor(sf::Color(210, 150, 15));
            btn.setOutlineThickness(1.0f);
            btn.setOutlineColor(sf::Color::White);
            btnText = isDemolish ? "ИЗБРАНО - КЛИКНИ СГРАДА ЗА МАХАНЕ" : (locked ? lock : "ИЗБРАНА - КЛИКНИ ЗА СТРОЕЖ");
        } else if (isDemolish) {
            btn.setFillColor(hover ? sf::Color(160, 45, 55) : sf::Color(110, 30, 40));
            btn.setOutlineThickness(1.0f);
            btn.setOutlineColor(sf::Color(220, 80, 90));
            btnText = "ИЗБЕРИ ЗА РАЗРУШАВАНЕ";
        } else if (locked) {
            btn.setFillColor(sf::Color(58, 40, 22));
            btn.setOutlineThickness(1.0f);
            btn.setOutlineColor(sf::Color(150, 100, 50));
            btnText = lock;
        } else {
            btn.setFillColor(canAfford ? (hover ? sf::Color(40, 130, 70) : sf::Color(28, 90, 48)) : sf::Color(50, 32, 40));
            btn.setOutlineThickness(1.0f);
            btn.setOutlineColor(canAfford ? sf::Color(65, 210, 105) : sf::Color(80, 48, 58));
            btnText = canAfford ? "ИЗБЕРИ ЗА СТРОЕЖ" : "НЕДОСТИГ НА РЕСУРСИ";
        }
        window.draw(btn);
        sf::Text tBtn = fittedText(font, btnText, 9, w - 20.0f);
        tBtn.setFillColor((locked && !isSelected) ? BP_LOCKED : sf::Color::White);
        sf::FloatRect bb = tBtn.getLocalBounds();
        tBtn.setPosition({ std::round(x + 6.0f + (w - 12.0f - bb.size.x) * 0.5f - bb.position.x), y + 36.0f });
        window.draw(tBtn);
    }
    drawPageTab(window, font, fontLoaded, mousePos);
}
