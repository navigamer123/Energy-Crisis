// =============================================================================
// Team b-power: page 2 of the building panel — reactor, geothermal plant and
// the three mega-projects (+ Demolish). The cards are drawn by the same
// renderer as page 1 (UI_buildings::draw); this file holds the page data, the
// lock reasons shown on the cards and the "1/2" page tab in the header.
// The page follows the selection (E / PgDn cycle, 7 / 8 / 9) and the tab
// switches pages with the mouse.
// =============================================================================
#include "../includes/UI_buildings.h"
#include "../includes/UI_text.h"
#include "../includes/UI_theme.h"
#include "../includes/UI_types.h"
#include <algorithm>
#include <cmath>

namespace {

bool isAdvancedType(BuildingType t) {
    return t == BuildingType::NUCLEAR || t == BuildingType::GEOTHERMAL || GameEngine::isMegaProject(t);
}

} // namespace

void UI_buildings::setupAdvancedCards() {
    advancedCards = {
        { BuildingType::NUCLEAR, "АЕЦ", {} },
        { BuildingType::GEOTHERMAL, "Геотермална", {} },
        { BuildingType::MEGA_FUSION, "Термояд", {} },
        { BuildingType::MEGA_SPACE_SOLAR, "Косм. СЕЦ", {} },
        { BuildingType::MEGA_PUMPED_HYDRO, "ПАВЕЦ", {} },
        { BuildingType::DEMOLISH, "Събаряне", {} },
    };
    // Same slots as page 1 (both pages have six cards)
    for (size_t i = 0; i < advancedCards.size() && i < buildings.size(); i++) {
        advancedCards[i].btnBounds = buildings[i].btnBounds;
    }
    pageTabBounds = sf::FloatRect({ panelPos.x + panelSize.x - 34.0f, panelPos.y + 4.0f }, { 28.0f, 17.0f });
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

std::string UI_buildings::advancedLockReason(const GameEngine& engine, BuildingType type) const {
    if (type == BuildingType::NUCLEAR) {
        int owned = engine.countOwnedPlots(playerIndex);
        if (owned < PowerBalance::NUCLEAR_UNLOCK_PLOTS) {
            return "Нужни " + std::to_string(PowerBalance::NUCLEAR_UNLOCK_PLOTS) + " парцела (" + std::to_string(owned) + "/" +
                   std::to_string(PowerBalance::NUCLEAR_UNLOCK_PLOTS) + ")";
        }
        if (engine.getReactor(playerIndex) != nullptr) return "Вече имате АЕЦ";
    } else if (GameEngine::isMegaProject(type)) {
        if (engine.getCurrentDay() < PowerBalance::MEGA_UNLOCK_DAY) {
            return "Отключва се от ден " + std::to_string(PowerBalance::MEGA_UNLOCK_DAY);
        }
        const PlacedBuilding* mega = engine.getMegaProject(playerIndex);
        if (mega != nullptr && mega->type != type) return "Вече имате мегапроект";
    }
    return std::string();
}

std::string UI_buildings::advancedHotkey(BuildingType type) const {
    if (hotkeys == BuildHotkeys::DIGITS) {
        if (type == BuildingType::NUCLEAR) return "[7]";
        if (type == BuildingType::GEOTHERMAL) return "[8]";
        if (GameEngine::isMegaProject(type)) return "[9]";
        if (type == BuildingType::DEMOLISH) return "[6]";
    } else if (hotkeys == BuildHotkeys::NUMPAD && type == BuildingType::DEMOLISH) {
        return "[Num6]";
    }
    return std::string();
}

void UI_buildings::drawPageTab(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded, sf::Vector2f mousePos) {
    bool hover = pageTabBounds.contains(mousePos);
    sf::RectangleShape tab(pageTabBounds.size);
    tab.setPosition(pageTabBounds.position);
    tab.setFillColor(theme::withAlpha(hover ? theme::CardHover : theme::Well, 240));
    tab.setOutlineThickness(1.0f);
    tab.setOutlineColor(advancedPage ? theme::Warn : accentColor);
    window.draw(tab);
    if (!fontLoaded) return;
    sf::Text t(font, toUtf8(advancedPage ? "2/2" : "1/2"), fontsize::Caption);
    t.setStyle(sf::Text::Bold);
    t.setFillColor(hover ? theme::TextPrimary : (advancedPage ? theme::Warn : accentColor));
    sf::FloatRect b = t.getLocalBounds();
    t.setPosition({ std::round(pageTabBounds.position.x + (pageTabBounds.size.x - b.size.x) * 0.5f - b.position.x),
                    std::round(pageTabBounds.position.y + (pageTabBounds.size.y - b.size.y) * 0.5f - b.position.y) });
    ui::drawText(window, t, pageTabBounds);
}
