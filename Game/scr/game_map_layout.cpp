// =============================================================================
// Team b-power: map layout presets, seeded map generator and terrain rolls
// (F-39 + F-15). One MapLayout held by the engine replaces the three copies of
// the plot geometry (init, getGridSlot, getClosestGridIndex).
// =============================================================================
#include "../includes/game_main.h"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

// Area the plots of one side may use (left of the city for West; East is mirrored)
constexpr float AREA_TOP = 105.0f;
constexpr float AREA_MAX_HEIGHT = 445.0f; // keeps the lowest plot above the resource stations (y 582)

int terrainInt(TerrainType t) { return static_cast<int>(t); }

// Small deterministic helpers on top of mt19937 (same results with every standard library)
int rollInt(std::mt19937& rng, int lo, int hi) { // inclusive
    if (hi <= lo) return lo;
    return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1));
}

void setCell(MapLayout& L, int westCol, int row, int value) {
    if (L.inRange(westCol, row)) L.cells[static_cast<size_t>(row * L.plotCols + westCol)] = value;
}

void resetCells(MapLayout& L) {
    L.cells.assign(static_cast<size_t>(L.plotCols * L.plotRows), terrainInt(TerrainType::PLAIN));
}

void riverColumn(MapLayout& L, int fromRow, int toRow) {
    for (int r = fromRow; r <= toRow; ++r) setCell(L, L.plotCols - 1, r, terrainInt(TerrainType::RIVER));
}

// Rolls vents / hills / meadows onto the free plain cells (never the start plot), mirrored for East
void rollTerrain(MapLayout& L, std::mt19937& rng, int vents, int hills, int meadows) {
    std::vector<int> free;
    for (int r = 0; r < L.plotRows; ++r) {
        for (int c = 0; c < L.plotCols; ++c) {
            if (c == L.startCol && r == L.startRow) continue;
            if (L.cellValue(c, r) == terrainInt(TerrainType::PLAIN)) free.push_back(r * L.plotCols + c);
        }
    }
    for (size_t i = free.size(); i > 1; --i) { // Fisher-Yates
        size_t j = static_cast<size_t>(rng() % static_cast<unsigned>(i));
        std::swap(free[i - 1], free[j]);
    }
    size_t k = 0;
    auto assign = [&](int count, TerrainType t) {
        for (int n = 0; n < count && k < free.size(); ++n, ++k) L.cells[static_cast<size_t>(free[k])] = terrainInt(t);
    };
    assign(vents, TerrainType::VENT);
    assign(hills, TerrainType::HILL);
    assign(meadows, TerrainType::MEADOW);
}

void classicGeometry(MapLayout& L) {
    L.plotCols = 3;
    L.plotRows = 4;
    L.plotW = 105.0f;
    L.plotH = 95.0f;
    L.gapX = 12.0f;
    L.gapY = 10.0f;
    L.westStartX = 258.0f;
    L.startY = AREA_TOP;
    L.startCol = 0;
    L.startRow = 0;
    L.landCostGrowth = Balance::LAND_TIER_COST_GROWTH;
    resetCells(L);
}

} // namespace

// -----------------------------------------------------------------------------
// Names
// -----------------------------------------------------------------------------
const char* getTerrainNameBg(TerrainType t) {
    switch (t) {
        case TerrainType::PLAIN:  return "РАВНИНА";
        case TerrainType::RIVER:  return "РЕКА";
        case TerrainType::VENT:   return "ГЕЙЗЕР";
        case TerrainType::HILL:   return "ХЪЛМ";
        case TerrainType::MEADOW: return "ЛИВАДА";
    }
    return "";
}

const char* getTerrainEffectBg(TerrainType t) {
    switch (t) {
        case TerrainType::PLAIN:  return "";
        case TerrainType::RIVER:  return "ВЕЦ / ПАВЕЦ";
        case TerrainType::VENT:   return "Геотермална";
        case TerrainType::HILL:   return "Вятър +30%";
        case TerrainType::MEADOW: return "Слънце +15%";
    }
    return "";
}

const char* getMapPresetNameBg(MapPreset p) {
    switch (p) {
        case MapPreset::CLASSIC:     return "КЛАСИЧЕСКА";
        case MapPreset::RIVER_DELTA: return "РЕЧНА ДЕЛТА";
        case MapPreset::HIGHLANDS:   return "ПЛАНИНИ";
        case MapPreset::LONG_VALLEY: return "ДЪЛГА ДОЛИНА";
        case MapPreset::CROWDED:     return "ТЯСНО ПОЛЕ";
        case MapPreset::GENERATED:   return "ГЕНЕРИРАНА";
        case MapPreset::COUNT:       break;
    }
    return "КЛАСИЧЕСКА";
}

const char* getMapPresetDescBg(MapPreset p) {
    switch (p) {
        case MapPreset::CLASSIC:     return "3x4 парцела, реката тече до града";
        case MapPreset::RIVER_DELTA: return "Реката се разлива в долния ред: много ВЕЦ";
        case MapPreset::HIGHLANDS:   return "Хълмове и гейзери, къс речен бряг и връх";
        case MapPreset::LONG_VALLEY: return "2x5 дълги парцела покрай реката";
        case MapPreset::CROWDED:     return "3x5 малки и евтини парцела";
        case MapPreset::GENERATED:   return "Огледална карта, генерирана от сийда на мача";
        case MapPreset::COUNT:       break;
    }
    return "";
}

// -----------------------------------------------------------------------------
// MapLayout geometry
// -----------------------------------------------------------------------------
sf::FloatRect MapLayout::plotRect(int player, int screenCol, int row) const {
    float x0 = (player == 1) ? westStartX : eastStartX();
    float x = x0 + static_cast<float>(screenCol) * (plotW + gapX);
    float y = startY + static_cast<float>(row) * (plotH + gapY);
    return sf::FloatRect({ x, y }, { plotW, plotH });
}

sf::Vector2f MapLayout::slotCenter(int player, int col, int row) const {
    col = std::max(0, std::min(col, gridCols() - 1));
    row = std::max(0, std::min(row, gridRows() - 1));
    sf::FloatRect r = plotRect(player, col / 3, row / 3);
    float subW = plotW / 3.0f;
    float subH = plotH / 3.0f;
    return sf::Vector2f(r.position.x + (static_cast<float>(col % 3) + 0.5f) * subW,
                        r.position.y + (static_cast<float>(row % 3) + 0.5f) * subH);
}

int MapLayout::plotCount() const {
    int n = 0;
    for (int v : cells) n += (v >= 0) ? 1 : 0;
    return n;
}

int MapLayout::countTerrain(TerrainType t) const {
    int n = 0;
    for (int v : cells) n += (v == static_cast<int>(t)) ? 1 : 0;
    return n;
}

// -----------------------------------------------------------------------------
// Presets and the seeded generator
// -----------------------------------------------------------------------------
MapLayout buildMapLayout(MapPreset preset, unsigned seed) {
    MapLayout L;
    L.preset = preset;
    L.seed = seed;
    std::mt19937 rng(seed * 2654435761u + static_cast<unsigned>(preset) * 40503u + 7u);
    classicGeometry(L);

    switch (preset) {
        case MapPreset::RIVER_DELTA:
            riverColumn(L, 0, L.plotRows - 1);
            setCell(L, 0, L.plotRows - 1, terrainInt(TerrainType::RIVER));
            setCell(L, 1, L.plotRows - 1, terrainInt(TerrainType::RIVER));
            rollTerrain(L, rng, PowerBalance::VENTS_PER_SIDE, 0, 3);
            break;

        case MapPreset::HIGHLANDS:
            riverColumn(L, 2, L.plotRows - 1);
            setCell(L, 1, 2, -1); // impassable peak
            rollTerrain(L, rng, 3, 4, 0);
            break;

        case MapPreset::LONG_VALLEY:
            L.plotCols = 2;
            L.plotRows = 5;
            L.plotW = 160.0f;
            L.plotH = 80.0f;
            L.gapX = 19.0f;
            L.gapY = 9.0f;
            L.landCostGrowth = 55;
            resetCells(L);
            riverColumn(L, 0, L.plotRows - 1);
            rollTerrain(L, rng, PowerBalance::VENTS_PER_SIDE, 1, 3);
            break;

        case MapPreset::CROWDED:
            L.plotRows = 5;
            L.plotH = 82.0f;
            L.gapY = 8.0f;
            L.landCostGrowth = 32;
            resetCells(L);
            riverColumn(L, 0, L.plotRows - 1);
            rollTerrain(L, rng, PowerBalance::VENTS_PER_SIDE, 2, 3);
            break;

        case MapPreset::GENERATED: {
            bool wide = (rollInt(rng, 1, 10) <= 3); // 30 %: two long columns
            L.plotCols = wide ? 2 : 3;
            L.plotRows = rollInt(rng, 4, 5);
            L.plotW = wide ? 160.0f : 105.0f;
            L.gapX = wide ? 19.0f : 12.0f;
            L.gapY = (L.plotRows == 4) ? 10.0f : 9.0f;
            L.plotH = std::min(95.0f, std::floor((AREA_MAX_HEIGHT - (L.plotRows - 1) * L.gapY) / L.plotRows));
            resetCells(L);
            // River bank: the column next to the city, at least 2 plots long, sometimes with a delta branch
            int riverFrom = rollInt(rng, 0, L.plotRows - 2);
            riverColumn(L, riverFrom, L.plotRows - 1);
            if (L.plotCols >= 3 && rollInt(rng, 0, 1) == 1) {
                setCell(L, L.plotCols - 2, rollInt(rng, riverFrom, L.plotRows - 1), terrainInt(TerrainType::RIVER));
            }
            // 0-2 holes (never the start plot or the river)
            int holes = rollInt(rng, 0, 2);
            for (int h = 0; h < holes; ++h) {
                int c = rollInt(rng, 0, L.plotCols - 1);
                int r = rollInt(rng, 0, L.plotRows - 1);
                bool isStartArea = (r == L.startRow && c <= L.startCol + 1);
                if (!isStartArea && L.cellValue(c, r) == terrainInt(TerrainType::PLAIN)) setCell(L, c, r, -1);
            }
            rollTerrain(L, rng, PowerBalance::VENTS_PER_SIDE, rollInt(rng, 1, 3), rollInt(rng, 1, 3));
            int plots = std::max(1, L.plotCount());
            L.landCostGrowth = std::max(30, std::min(60, (Balance::LAND_TIER_COST_GROWTH * 12) / plots));
            break;
        }

        case MapPreset::CLASSIC:
        case MapPreset::COUNT:
        default:
            L.preset = MapPreset::CLASSIC;
            riverColumn(L, 0, L.plotRows - 1);
            rollTerrain(L, rng, PowerBalance::VENTS_PER_SIDE, 2, 2);
            break;
    }
    return L;
}

// -----------------------------------------------------------------------------
// Engine: land plots from the layout
// -----------------------------------------------------------------------------
void GameEngine::setupPowerWorld(unsigned matchSeed) {
    unsigned mapSeed = (mapSeedOverride != 0) ? mapSeedOverride : matchSeed;
    layout = buildMapLayout(mapPreset, mapSeed);
    world = PowerWorldState();
    world.rng.seed(matchSeed ^ 0x5BD1E995u);

    // Prices grow with the plot's order from the player's own start corner (West orientation),
    // so mirrored plots cost the same. Classic: rank = row * 3 + col, the original price table.
    std::vector<int> rank(layout.cells.size(), 0);
    int next = 1;
    for (int r = 0; r < layout.plotRows; ++r) {
        for (int c = 0; c < layout.plotCols; ++c) {
            if (!layout.hasPlot(c, r)) continue;
            bool isStart = (c == layout.startCol && r == layout.startRow);
            rank[static_cast<size_t>(r * layout.plotCols + c)] = isStart ? 0 : next++;
        }
    }

    landPlots.clear();
    int idCounter = 1;
    for (int player = 1; player <= 2; ++player) {
        for (int r = 0; r < layout.plotRows; ++r) {
            for (int sc = 0; sc < layout.plotCols; ++sc) {
                int wc = layout.westCol(player, sc);
                if (!layout.hasPlot(wc, r)) continue;
                LandPlot plot;
                plot.id = idCounter++;
                plot.playerOwner = player;
                plot.bounds = layout.plotRect(player, sc, r);
                plot.isPurchased = (wc == layout.startCol && r == layout.startRow);
                plot.costGold = Balance::LAND_BASE_COST_GOLD + rank[static_cast<size_t>(r * layout.plotCols + wc)] * layout.landCostGrowth;
                plot.terrain = layout.cellValue(wc, r);
                plot.screenCol = sc;
                plot.row = r;
                landPlots.push_back(plot);
            }
        }
    }
    std::cout << "[GameEngine] Map: " << getMapPresetNameBg(layout.preset) << " (seed " << mapSeed << "), "
              << layout.plotCount() << " plots per player, " << layout.countTerrain(TerrainType::VENT) << " vents, "
              << layout.countTerrain(TerrainType::RIVER) << " river plots.\n";
}

sf::Vector2f GameEngine::getStartPlotSlot(int player, int subCol, int subRow) const {
    int screenCol = (player == 1) ? layout.startCol : (layout.plotCols - 1 - layout.startCol);
    return layout.slotCenter(player, screenCol * 3 + std::max(0, std::min(subCol, 2)),
                             layout.startRow * 3 + std::max(0, std::min(subRow, 2)));
}

const LandPlot* GameEngine::findPlotAt(int player, sf::Vector2f pos) const {
    for (const auto& plot : landPlots) {
        if (plot.playerOwner == player && plot.bounds.contains(pos)) return &plot;
    }
    return nullptr;
}

int GameEngine::countOwnedPlots(int player) const {
    int n = 0;
    for (const auto& plot : landPlots) {
        if (plot.playerOwner == player && plot.isPurchased) ++n;
    }
    return n;
}
