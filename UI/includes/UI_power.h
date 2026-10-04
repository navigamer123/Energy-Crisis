#ifndef UI_POWER_H
#define UI_POWER_H

// =============================================================================
// Team b-power: UI for terrain plots, the nuclear reactor, hazards and
// mega-projects. Procedural icons (no image files) for the new building types
// use the same signature style as UI_icons::drawBuildingIcon so the integrator
// can route NUCLEAR / GEOTHERMAL / MEGA_* there.
// =============================================================================

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include "../../Game/includes/game_main.h"

// Icons for NUCLEAR, GEOTHERMAL, MEGA_FUSION, MEGA_SPACE_SOLAR, MEGA_PUMPED_HYDRO (others: nothing).
// Fits a size x size box centred on `center`, readable from 16 px.
void drawPowerBuildingIcon(sf::RenderTarget& t, BuildingType b, sf::Vector2f center, float size);
// Small terrain badges: river waves, vent steam, hill peaks, meadow flower (PLAIN: nothing)
void drawTerrainIcon(sf::RenderTarget& t, TerrainType terrain, sf::Vector2f center, float size);
sf::Color terrainAccentColor(TerrainType terrain);
// Hazard symbols: hail stones, flood wave, flame, quake crack
void drawHazardIcon(sf::RenderTarget& t, HazardKind k, sf::Vector2f center, float size);

// World layer: terrain decoration, new building sprites, damage, construction,
// hazard effects and the shared mega-project HUD. Owned by UI_map.
class UI_powerLayer {
public:
    void reset();                                   // new match
    void update(float dt);                          // particles and flashes (real time)
    void addFx(const PowerFx& fx, const GameEngine& engine); // visual part of an engine event

    // Terrain badges and decoration on the plots (after the land plots are drawn)
    void drawTerrain(sf::RenderWindow& w, const sf::Font& font, bool fontLoaded, const GameEngine& e, float animTime);
    // Reactors, geothermal plants, mega-projects, damage marks (after the ordinary buildings)
    void drawBuildings(sf::RenderWindow& w, const sf::Font& font, bool fontLoaded, const GameEngine& e, float animTime);
    // Whole-plot footprint / suitable plots while a player has an advanced building selected
    void drawPlacementHint(sf::RenderWindow& w, const GameEngine& e, int player, BuildingType sel, sf::Vector2f cursor, float animTime);
    // Hazard particles above the world
    void drawEffects(sf::RenderWindow& w, float animTime);
    // Shared mega-project progress strip under the city (visible to both players)
    void drawMegaHud(sf::RenderWindow& w, const sf::Font& font, bool fontLoaded, const GameEngine& e, float animTime,
                     bool p2IsBot = false);
    // "[ACTION]: repair" prompt when a cursor rests on its own broken building
    void drawRepairPrompt(sf::RenderWindow& w, const sf::Font& font, bool fontLoaded, const GameEngine& e, int player,
                          sf::Vector2f cursor, const std::string& keyLabel);

private:
    struct Particle {
        sf::Vector2f pos;
        sf::Vector2f vel;
        float life = 1.0f;
        float maxLife = 1.0f;
        float size = 3.0f;
        sf::Color color;
        int kind = 0; // 0 dot, 1 flame, 2 hail stone, 3 water drop, 4 dust, 5 spark
    };
    struct Ring {
        sf::Vector2f pos;
        float radius = 10.0f;
        float maxRadius = 80.0f;
        float life = 1.0f;
        float maxLife = 1.0f;
        sf::Color color;
    };
    std::vector<Particle> particles;
    std::vector<Ring> rings;
    float shake = 0.0f;

    void spawn(sf::Vector2f pos, sf::Vector2f vel, float life, float size, sf::Color c, int kind);
    void spawnRing(sf::Vector2f pos, float maxRadius, float life, sf::Color c);
};

#endif // UI_POWER_H
