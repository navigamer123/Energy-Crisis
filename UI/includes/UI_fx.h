#ifndef UI_FX_H
#define UI_FX_H

// =============================================================================
// [b-effects] UI_fx: visual + audio effects director owned by UI_map.
//   DS-08 juice pack   - build pop-in with dust ring, mined resources fly to the HUD with a
//                        counter bump, screen shake on lightning, tweened influence bar
//   HX-07 power grid   - transformers, pylons, sagging catenary cables, pulses scaled by MW
//   DS-11 lighting     - day/night light map (sky gradient, sector weather tint, lamp pools,
//                        city glow, headlights) multiplied over the world
//   DS-10 river        - full-height river with canals feeding the hydro bank, bridges, traffic
//   HX-11 seasons      - snow cover, leaf litter, blossoms, heat shimmer, frozen river
//   F-01  audio hooks  - every feedback event plays its procedural sound (UI_audio)
// Draw calls are grouped by layer; see UI_map::render for the order.
// Colours live in named constants at the top of each UI_fx_*.cpp file.
// =============================================================================

#include <SFML/Graphics.hpp>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "UI_fx_events.h"
#include "../../Game/includes/game_main.h"

class UI_resourceNodes;

class UI_fx {
public:
    UI_fx();
    ~UI_fx();

    // Call when a match (re)starts: clears effects, resyncs the event tracker
    void reset(const GameEngine& engine);
    // Single player: settlement stingers are told from P1's point of view, bot errors stay silent
    void setSinglePlayer(bool singlePlayer) { singlePlayerMode = singlePlayer; }
    void setScreenShakeEnabled(bool on) { shakeEnabled = on; }
    bool isScreenShakeEnabled() const { return shakeEnabled; }

    // Once per frame after the simulation step. simulationRunning = not paused and no winner.
    void update(float dt, const GameEngine& engine, const UI_resourceNodes& nodes, bool simulationRunning);

    // Hooks from UI_map
    void onLightning(sf::Vector2f pos, bool hitBuilding);
    void onPlayerError(int player);

    // ---- World layer -------------------------------------------------------
    sf::View worldView(const sf::View& base) const;              // screen shake (base when calm)
    void drawGround(sf::RenderTarget& target) const;              // HX-11 seasonal ground
    void drawRiver(sf::RenderTarget& target) const;               // DS-10 river, canals, bridges, traffic
    void drawBorderTag(sf::RenderTarget& target, const sf::Font& font, bool fontLoaded) const; // "ЦЕНТРАЛНА ГРАНИЦА"
    void applyLighting(sf::RenderTarget& target, bool mildPass);  // DS-11 (full pass, then mild pass)
    void drawPowerGrid(sf::RenderTarget& target) const;           // HX-07 hydro pipes, cables, pylons
    void drawBuildings(sf::RenderWindow& window, UI_resourceNodes& nodes, const sf::Font& font, bool fontLoaded,
                       const std::vector<PlacedBuilding>& buildings) const; // DS-08 pop-in
    void drawEmissive(sf::RenderTarget& target) const;            // HX-07 pulses, lamp halos
    void drawWorldFx(sf::RenderTarget& target) const;             // dust rings, debris, land/upgrade flashes

    // Queries for other layers (weather particles, city)
    sf::Color ambientColorAt(float x) const;   // multiply colour of the light pass at screen x
    float nightAmount() const { return night; } // 0 = day, 1 = night
    float seasonWeight(SeasonType s) const { return seasonW[static_cast<int>(s)]; }
    float displayedShare() const { return shareShown; }

    // ---- UI layer ----------------------------------------------------------
    void drawInfluenceTrail(sf::RenderTarget& target) const;
    void drawFlyingResources(sf::RenderTarget& target, const sf::Font& font, bool fontLoaded) const;

private:
    // --- cached engine state (draw calls never touch the engine) ---
    std::vector<PlacedBuilding> buildings;
    std::vector<LandPlot> plots;
    WeatherType weather[2] = { WeatherType::SUNNY, WeatherType::SUNNY };
    SeasonType season = SeasonType::SPRING;
    float hour = 8.0f;
    float sunrise = 6.0f;
    float sunset = 19.0f;
    bool daylight = true;
    int winner = 0;

    FxEventTracker tracker;
    std::vector<FxEvent> events;
    bool singlePlayerMode = false;
    bool shakeEnabled = true;
    float time = 0.0f;       // real seconds (UI animation)
    float worldTime = 0.0f;  // stops while paused (traffic, water, pulses)

    // --- DS-08 juice ---
    struct PopIn { sf::Vector2f pos; int owner; float age; };
    struct Ring { sf::Vector2f pos; float age, life, radius; sf::Color color; };
    struct Puff { sf::Vector2f pos, vel; float age, life, size; sf::Color color; };
    struct FlyIcon {
        sf::Vector2f from, ctrl, to;
        float delay, age, dur;
        ResourceType res;
        int player;
        bool last;
        int amount;
    };
    struct Bump { sf::Vector2f pos; float age; ResourceType res; int amount; int player; };
    struct RectFlash { sf::FloatRect rect; float age, life; sf::Color color; };
    std::vector<PopIn> popIns;
    std::vector<Ring> rings;
    std::vector<Puff> puffs;
    std::vector<FlyIcon> flyIcons;
    std::vector<Bump> bumps;
    std::vector<RectFlash> rectFlashes;
    float shakeTime = 0.0f, shakeDur = 0.0f, shakeAmp = 0.0f;
    std::vector<std::pair<sf::Vector2f, float>> recentStrikes; // lightning position, seconds since
    float shareShown = 0.5f, shareVel = 0.0f, shareTrail = 0.5f, trailHold = 0.0f;
    float shareReal = 0.5f;
    std::uint32_t rng = 0xC0FFEEu;
    float frand();

    void handleEvents(const UI_resourceNodes& nodes);
    void spawnDust(sf::Vector2f pos, sf::Color color, int count, float speed);
    void addShake(float amp, float dur);

    // --- HX-07 grid ---
    struct GridSeg {
        sf::Vector2f a, b;   // attach points
        float sag;           // px at mid-span
        float mw;            // power carried
        float capacity;      // load at which the line glows red
        int owner;
        bool reverse;        // flow b -> a
        std::uint64_t key;   // stable id for the pulse phase
    };
    struct Pylon { sf::Vector2f base; float height; int owner; };
    struct Transformer { sf::Vector2f pos; int owner; float mw; };
    struct HydroPipe { sf::Vector2f from, to; int owner; float mw; };
    std::vector<GridSeg> gridSegs;
    std::vector<Pylon> pylons;
    std::vector<Transformer> transformers;
    std::vector<HydroPipe> hydroPipes;
    std::map<std::uint64_t, float> pulsePhase;
    void rebuildGrid();
    void advancePulses(float dt);

    // --- DS-11 lighting ---
    std::unique_ptr<sf::RenderTexture> lightMap;
    std::unique_ptr<sf::RenderTexture> mildMap;
    std::unique_ptr<sf::Texture> radialTex;
    bool lightingReady = false;
    bool lightingFailed = false;
    float night = 0.0f;
    float lightningLight = 0.0f;
    sf::Color skyTop[3], skyBottom[3]; // left / middle / right columns
    void computeAmbient();
    void ensureLightingResources();
    void renderLightMap();

    // --- DS-10 river / traffic ---
    struct Car { int road; int dir; float x; float speed; sf::Color body; };
    std::vector<Car> cars;
    void initTraffic();
    void advanceTraffic(float dt);

    // --- HX-11 seasons ---
    float seasonW[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
    std::unique_ptr<sf::Texture> snowTex;
    struct Decal { sf::Vector2f pos; float rot, size; int kind; };
    std::vector<Decal> decals;
    void ensureSeasonResources();
};

#endif // UI_FX_H
