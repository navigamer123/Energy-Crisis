#ifndef UI_ICONS_H
#define UI_ICONS_H

#include <SFML/Graphics.hpp>
#include "../../Game/includes/game_main.h"

// -----------------------------------------------------------------------------
// Procedural vector icons for resources and buildings (no image files).
// Every icon fits a size x size box centred on `center` and is readable from
// 16 px upwards. Each icon has its own silhouette (tree, ingot, spool, rock,
// chip, gem, coin, note, bolt / panel, turbine, wheel, cell, lamp, hammer),
// so they stay distinct for colour-blind players, not only by colour.
// -----------------------------------------------------------------------------

// Resources: WOOD, IRON, COPPER, COAL, SILICON, SILVER, GOLD, plus MONEY and
// ENERGY (MW bolt). NONE and ORE draw nothing.
void drawResourceIcon(sf::RenderTarget& t, ResourceType r, sf::Vector2f center, float size);

// Buildings: SOLAR_PANEL, WIND_TURBINE, HYDRO_PLANT, BATTERY, LAMP, plus
// DEMOLISH (hammer). NONE draws nothing.
void drawBuildingIcon(sf::RenderTarget& t, BuildingType b, sf::Vector2f center, float size);

// Main colour of each resource icon, e.g. for a matching label or highlight.
sf::Color resourceIconColor(ResourceType r);

#endif // UI_ICONS_H
