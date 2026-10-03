// =============================================================================
// [b-showcase] Tutorial chapters 2-3 (F-07): step logic, clock policy and grants
// =============================================================================
#include "../includes/UI_tutorialChapters.h"
#include <algorithm>
#include <cmath>

void TutorialChapters::reset() {
    step = ChapterStep::NONE;
    confirmRequested = false;
    stepDelay = 0.0f;
    nextStep = ChapterStep::NONE;
    baseline = 0;
    watchDay = 0;
    watchedSettlement.clear();
    pendingGrants.clear();
    chapter2Granted = plotGranted = hydroGranted = false;
}

void TutorialChapters::startChapter(int chapter) {
    confirmRequested = false;
    stepDelay = 0.0f;
    if (chapter == 2) {
        step = ChapterStep::C2_INTRO;
        chapter2Granted = false;
        watchedSettlement.clear();
    } else if (chapter == 3) {
        step = ChapterStep::C3_INTRO;
        plotGranted = hydroGranted = false;
    } else {
        step = ChapterStep::NONE;
    }
}

int TutorialChapters::getChapter() const {
    if (step == ChapterStep::NONE) return 0;
    return (step <= ChapterStep::C2_DONE) ? 2 : 3;
}

bool TutorialChapters::isDialog() const {
    return step == ChapterStep::C2_INTRO || step == ChapterStep::C2_DONE || step == ChapterStep::C3_INTRO ||
           step == ChapterStep::C3_DONE;
}

int TutorialChapters::stepNumber() const {
    switch (step) {
        case ChapterStep::C2_SELECT_BATTERY: return 1;
        case ChapterStep::C2_PLACE_BATTERY: return 2;
        case ChapterStep::C2_DUSK: return 3;
        case ChapterStep::C2_SELECT_LAMP:
        case ChapterStep::C2_PLACE_LAMP: return 4;
        case ChapterStep::C2_BUILD_IN_LIGHT: return 5;
        case ChapterStep::C2_WATCH_NIGHT: return 6;
        case ChapterStep::C3_EARN_GOLD: return 1;
        case ChapterStep::C3_UPGRADE_MINE: return 2;
        case ChapterStep::C3_BUY_PLOT: return 3;
        case ChapterStep::C3_SELECT_HYDRO:
        case ChapterStep::C3_PLACE_HYDRO: return 4;
        default: return 0;
    }
}

int TutorialChapters::stepCount() const {
    return getChapter() == 2 ? 6 : (getChapter() == 3 ? 4 : 0);
}

float TutorialChapters::clockScale(const GameEngine& engine) const {
    const int gold = engine.getPlayerEconomy(1).gold;
    switch (step) {
        case ChapterStep::C2_SELECT_BATTERY:
        case ChapterStep::C2_PLACE_BATTERY:
        case ChapterStep::C3_SELECT_HYDRO:
        case ChapterStep::C3_PLACE_HYDRO:
            // Building needs light: wait for the morning in a time-lapse if it is night
            return engine.isDaylight() ? 0.0f : TIMELAPSE_SCALE;
        case ChapterStep::C2_DUSK:
            return (stepDelay > 0.0f) ? 0.0f : TIMELAPSE_SCALE;
        case ChapterStep::C2_WATCH_NIGHT:
            return TIMELAPSE_SCALE;
        case ChapterStep::C3_EARN_GOLD:
            return (stepDelay > 0.0f) ? 0.0f : 1.0f; // Real time: dividends flow while the player mines gold
        case ChapterStep::C3_UPGRADE_MINE:
            return (gold < Balance::getMineUpgradeCost(1, false)) ? 1.0f : 0.0f;
        case ChapterStep::C3_BUY_PLOT: {
            const LandPlot* plot = findPlot(engine, riverPlotId(1));
            int cost = plot ? plot->costGold : 0;
            return (stepDelay <= 0.0f && gold < cost) ? 1.0f : 0.0f;
        }
        default:
            return 0.0f; // Dialogs and placement steps hold the clock
    }
}

void TutorialChapters::finishStep(ChapterStep next) {
    nextStep = next;
    stepDelay = STEP_DELAY_SEC;
}

void TutorialChapters::enter(ChapterStep s, const GameEngine& engine) {
    step = s;
    stepDelay = 0.0f;
    nextStep = ChapterStep::NONE;
    switch (s) {
        case ChapterStep::C2_SELECT_BATTERY:
            if (!chapter2Granted) {
                chapter2Granted = true;
                pendingGrants.push_back(chapter2Grant());
            }
            break;
        case ChapterStep::C2_PLACE_BATTERY:
            baseline = countBuildings(engine, 1, BuildingType::BATTERY);
            break;
        case ChapterStep::C2_PLACE_LAMP:
            baseline = countBuildings(engine, 1, BuildingType::LAMP);
            break;
        case ChapterStep::C2_BUILD_IN_LIGHT:
            baseline = countNonLampBuildings(engine, 1);
            break;
        case ChapterStep::C2_WATCH_NIGHT:
            watchDay = engine.getCurrentDay();
            break;
        case ChapterStep::C3_UPGRADE_MINE:
            baseline = mineLevelSum(engine, 1);
            break;
        case ChapterStep::C3_BUY_PLOT:
            if (!plotGranted && !ownsPlot(engine, riverPlotId(1))) {
                plotGranted = true;
                pendingGrants.push_back(plotGrant());
            }
            break;
        case ChapterStep::C3_SELECT_HYDRO:
            if (!hydroGranted) {
                hydroGranted = true;
                pendingGrants.push_back(hydroGrant());
            }
            break;
        case ChapterStep::C3_PLACE_HYDRO:
            baseline = countBuildings(engine, 1, BuildingType::HYDRO_PLANT);
            break;
        default:
            break;
    }
}

void TutorialChapters::update(float dt, const GameEngine& engine) {
    if (step == ChapterStep::NONE) return;

    if (stepDelay > 0.0f) {
        stepDelay -= dt;
        if (stepDelay <= 0.0f) enter(nextStep, engine);
        return;
    }

    if (confirmRequested) {
        confirmRequested = false;
        switch (step) {
            case ChapterStep::C2_INTRO: enter(ChapterStep::C2_SELECT_BATTERY, engine); return;
            case ChapterStep::C2_DONE: startChapter(3); return;
            case ChapterStep::C3_INTRO: enter(ChapterStep::C3_EARN_GOLD, engine); return;
            case ChapterStep::C3_DONE: step = ChapterStep::NONE; return;
            default: break; // Not a dialog: Space belongs to the game
        }
    }

    const BuildingType sel = engine.getSelectedBuilding(1);
    // A placement step: done when a new building of that type stands, back to the selection step
    // when the player cancelled or picked something else
    auto placement = [&](BuildingType type, ChapterStep selectStep, ChapterStep next) {
        int n = countBuildings(engine, 1, type);
        if (n > baseline) {
            finishStep(next);
        } else {
            baseline = std::min(baseline, n); // One was demolished or struck meanwhile
            if (sel != type) enter(selectStep, engine);
        }
    };

    switch (step) {
        case ChapterStep::C2_SELECT_BATTERY:
            if (sel == BuildingType::BATTERY) enter(ChapterStep::C2_PLACE_BATTERY, engine);
            break;
        case ChapterStep::C2_PLACE_BATTERY:
            placement(BuildingType::BATTERY, ChapterStep::C2_SELECT_BATTERY, ChapterStep::C2_DUSK);
            break;
        case ChapterStep::C2_DUSK:
            if (!engine.isDaylight()) finishStep(ChapterStep::C2_SELECT_LAMP);
            break;
        case ChapterStep::C2_SELECT_LAMP:
            if (sel == BuildingType::LAMP) enter(ChapterStep::C2_PLACE_LAMP, engine);
            break;
        case ChapterStep::C2_PLACE_LAMP:
            placement(BuildingType::LAMP, ChapterStep::C2_SELECT_LAMP, ChapterStep::C2_BUILD_IN_LIGHT);
            break;
        case ChapterStep::C2_BUILD_IN_LIGHT: {
            int n = countNonLampBuildings(engine, 1);
            if (n > baseline) finishStep(ChapterStep::C2_WATCH_NIGHT);
            else baseline = std::min(baseline, n);
            break;
        }
        case ChapterStep::C2_WATCH_NIGHT:
            if (engine.getCurrentDay() > watchDay) {
                watchedSettlement = engine.getCityState().lastCutMessage;
                enter(ChapterStep::C2_DONE, engine);
            }
            break;
        case ChapterStep::C3_EARN_GOLD:
            if (engine.getPlayerEconomy(1).gold >= GOLD_TARGET) finishStep(ChapterStep::C3_UPGRADE_MINE);
            break;
        case ChapterStep::C3_UPGRADE_MINE: {
            int levels = mineLevelSum(engine, 1);
            if (levels > baseline) finishStep(ChapterStep::C3_BUY_PLOT);
            else baseline = std::min(baseline, levels);
            break;
        }
        case ChapterStep::C3_BUY_PLOT:
            if (ownsPlot(engine, riverPlotId(1))) finishStep(ChapterStep::C3_SELECT_HYDRO);
            break;
        case ChapterStep::C3_SELECT_HYDRO:
            if (sel == BuildingType::HYDRO_PLANT) enter(ChapterStep::C3_PLACE_HYDRO, engine);
            break;
        case ChapterStep::C3_PLACE_HYDRO:
            placement(BuildingType::HYDRO_PLANT, ChapterStep::C3_SELECT_HYDRO, ChapterStep::C3_DONE);
            break;
        default:
            break;
    }
}

void TutorialChapters::applyPending(GameEngine& engine) {
    for (const ResourceGrant& g : pendingGrants) {
        applyGrant(engine, 1, g);
        applyGrant(engine, 2, g);
    }
    pendingGrants.clear();
}

// -----------------------------------------------------------------------------
// Engine queries
// -----------------------------------------------------------------------------
int TutorialChapters::countBuildings(const GameEngine& engine, int player, BuildingType type) {
    int n = 0;
    for (const auto& b : engine.getBuildings()) {
        if (b.playerOwner == player && b.type == type) ++n;
    }
    return n;
}

int TutorialChapters::countNonLampBuildings(const GameEngine& engine, int player) {
    int n = 0;
    for (const auto& b : engine.getBuildings()) {
        if (b.playerOwner == player && b.type != BuildingType::LAMP) ++n;
    }
    return n;
}

int TutorialChapters::mineLevelSum(const GameEngine& engine, int player) {
    int sum = 0;
    for (ResourceType r : { ResourceType::WOOD, ResourceType::IRON, ResourceType::COPPER, ResourceType::COAL,
                            ResourceType::SILICON, ResourceType::SILVER, ResourceType::GOLD }) {
        sum += engine.getMineLevel(player, r);
    }
    return sum;
}

float TutorialChapters::batteryCharge(const GameEngine& engine, int player) {
    float stored = 0.0f, capacity = 0.0f;
    for (const auto& b : engine.getBuildings()) {
        if (b.playerOwner == player && b.type == BuildingType::BATTERY) {
            stored += b.energyStored;
            capacity += b.maxCapacity;
        }
    }
    return capacity > 0.0f ? std::clamp(stored / capacity, 0.0f, 1.0f) : 0.0f;
}

int TutorialChapters::riverPlotId(int player) {
    // Top row, the plot column next to the city river (P1: screen column 2 -> id 3, P2: column 0 -> id 13)
    return (player == 1) ? 1 + Balance::P1_RIVER_BANK_PLOT_COL : 13 + Balance::P2_RIVER_BANK_PLOT_COL;
}

const LandPlot* TutorialChapters::findPlot(const GameEngine& engine, int plotId) {
    for (const auto& p : engine.getLandPlots()) {
        if (p.id == plotId) return &p;
    }
    return nullptr;
}

bool TutorialChapters::ownsPlot(const GameEngine& engine, int plotId) {
    const LandPlot* p = findPlot(engine, plotId);
    return p != nullptr && p->isPurchased;
}

bool TutorialChapters::findFreeSlot(const GameEngine& engine, int player, int preferredPlotId, bool mustBeLit,
                                    bool riverBankOnly, sf::Vector2f& outPos) {
    std::vector<int> order;
    order.push_back(preferredPlotId);
    for (const auto& p : engine.getLandPlots()) {
        if (p.playerOwner == player && p.id != preferredPlotId) order.push_back(p.id);
    }
    for (int plotId : order) {
        const LandPlot* plot = findPlot(engine, plotId);
        if (plot == nullptr || plot->playerOwner != player || !plot->isPurchased) continue;
        int idx = (plotId - 1) % 12;
        for (int sub = 0; sub < 9; ++sub) {
            int col = (idx % 3) * 3 + sub % 3;
            int row = (idx / 3) * 3 + sub / 3;
            sf::Vector2f pos = engine.getGridSlot(player, col, row);
            if (!plot->bounds.contains(pos)) continue;
            bool occupied = false;
            for (const auto& b : engine.getBuildings()) {
                float dx = b.position.x - pos.x, dy = b.position.y - pos.y;
                if (std::sqrt(dx * dx + dy * dy) < 16.0f) { occupied = true; break; }
            }
            if (occupied) continue;
            if (riverBankOnly && !engine.isRiverBankSlot(player, pos)) continue;
            if (mustBeLit) {
                bool lit = false;
                for (const auto& b : engine.getBuildings()) {
                    if (b.playerOwner != player || b.type != BuildingType::LAMP || b.lightRadius <= 0.0f) continue;
                    float dx = b.position.x - pos.x, dy = b.position.y - pos.y;
                    if (std::sqrt(dx * dx + dy * dy) <= b.lightRadius) { lit = true; break; }
                }
                if (!lit) continue;
            }
            outPos = pos;
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
// Grants
// -----------------------------------------------------------------------------
ResourceGrant TutorialChapters::chapter2Grant() {
    const auto& b = Balance::BATTERY;
    const auto& l = Balance::STREET_LAMP;
    const auto& s = Balance::SOLAR_PANEL;
    ResourceGrant g;
    g.wood = b.woodCost + l.woodCost + s.woodCost;
    g.iron = b.ironCost + l.ironCost + s.ironCost;
    g.copper = b.copperCost + l.copperCost + s.copperCost;
    g.coal = b.coalCost + l.coalCost + s.coalCost;
    g.silicon = b.siliconCost + l.siliconCost + s.siliconCost;
    g.silver = b.silverCost + l.silverCost + s.silverCost;
    return g;
}

ResourceGrant TutorialChapters::plotGrant() {
    ResourceGrant g;
    // Mirrored plots cost the same for both players (Balance::getLandPlotCost)
    g.gold = Balance::getLandPlotCost(0, 2);
    return g;
}

ResourceGrant TutorialChapters::hydroGrant() {
    const auto& h = Balance::HYDRO_PLANT;
    ResourceGrant g;
    g.wood = h.woodCost;
    g.iron = h.ironCost;
    g.copper = h.copperCost;
    g.coal = h.coalCost;
    g.silicon = h.siliconCost;
    g.silver = h.silverCost;
    return g;
}

void TutorialChapters::applyGrant(GameEngine& engine, int player, const ResourceGrant& g) {
    PlayerEconomy& e = engine.getPlayerEconomyMut(player);
    e.wood += g.wood;
    e.iron += g.iron;
    e.copper += g.copper;
    e.coal += g.coal;
    e.silicon += g.silicon;
    e.silver += g.silver;
    e.gold += g.gold;
}
