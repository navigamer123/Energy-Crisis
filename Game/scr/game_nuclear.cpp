// =============================================================================
// Team b-power (F-32): nuclear reactor — slow ramp, silver fuel, SCRAM.
// One reactor per player, unlocked at 6 owned plots, fills a whole plot
// (exclusion zone), 400 MW after a one-day ramp, 12 silver per day or SCRAM,
// no refund. Lightning (and quakes) cause a SCRAM instead of destroying it.
// Lightning on a mega-project costs construction progress instead (HX-10).
// =============================================================================
#include "../includes/game_main.h"
#include <algorithm>
#include <cmath>

void GameEngine::scramReactor(PlacedBuilding& b, const std::string& cause) {
    if (b.type != BuildingType::NUCLEAR) return;
    b.scramTimer = PowerBalance::NUCLEAR_SCRAM_COOLDOWN_SEC;
    b.rampProgress = 0.0f;
    b.currentOutputMW = 0.0f;
    PowerFx fx;
    fx.kind = PowerFxKind::REACTOR_SCRAM;
    fx.player = b.playerOwner;
    fx.buildingType = static_cast<int>(b.type);
    fx.owner = b.playerOwner;
    fx.pos = b.position;
    fx.radius = 60.0f;
    fx.title = "SCRAM! АВАРИЙНО СПИРАНЕ НА АЕЦ";
    fx.detail = cause + ": реакторът спира за половин ден,\nпосле отново набира мощност.";
    pushPowerFx(fx);
}

void GameEngine::updateReactors(float dt) {
    const int fuel = PowerBalance::NUCLEAR_FUEL_SILVER_PER_DAY;
    for (auto& b : buildings) {
        if (b.type != BuildingType::NUCLEAR) continue;
        PlayerEconomy& econ = (b.playerOwner == 1) ? p1 : p2;

        if (b.scramTimer > 0.0f) {
            b.scramTimer -= dt;
            if (b.scramTimer <= 0.0f) {
                b.scramTimer = 0.0f;
                b.rampProgress = 0.0f;
                if (!b.needsFuel) {
                    PowerFx fx;
                    fx.kind = PowerFxKind::REACTOR_RESTART;
                    fx.player = b.playerOwner;
                    fx.buildingType = static_cast<int>(b.type);
                    fx.owner = b.playerOwner;
                    fx.pos = b.position;
                    fx.title = "АЕЦ: РЕСТАРТ";
                    fx.detail = "Реакторът отново набира мощност (1 ден до 100%).";
                    pushPowerFx(fx);
                }
            }
            continue;
        }

        if (b.needsFuel) {
            // Waits for silver: refuels on its own as soon as the player has enough
            if (econ.silver >= fuel) {
                econ.silver -= fuel;
                econ.data.silver = econ.silver;
                b.needsFuel = false;
                b.rampProgress = 0.0f;
                PowerFx fx;
                fx.kind = PowerFxKind::REACTOR_RESTART;
                fx.player = b.playerOwner;
                fx.buildingType = static_cast<int>(b.type);
                fx.owner = b.playerOwner;
                fx.pos = b.position;
                fx.title = "АЕЦ Е ЗАРЕДЕН С ГОРИВО";
                fx.detail = "-" + std::to_string(fuel) + " сребро. Реакторът отново набира мощност.";
                pushPowerFx(fx);
            }
            continue;
        }

        float before = b.rampProgress;
        b.rampProgress = std::min(1.0f, b.rampProgress + dt / PowerBalance::NUCLEAR_RAMP_SECONDS);
        if (before < 1.0f && b.rampProgress >= 1.0f) {
            PowerFx fx;
            fx.kind = PowerFxKind::REACTOR_FULL;
            fx.player = b.playerOwner;
            fx.buildingType = static_cast<int>(b.type);
            fx.owner = b.playerOwner;
            fx.pos = b.position;
            fx.title = "АЕЦ НА ПЪЛНА МОЩНОСТ";
            fx.detail = "+" + std::to_string(PowerBalance::NUCLEAR.basePowerMW) + " MW базова мощност, независимо от времето.";
            pushPowerFx(fx);
        }
    }
}

void GameEngine::payReactorFuel() {
    const int fuel = PowerBalance::NUCLEAR_FUEL_SILVER_PER_DAY;
    for (auto& b : buildings) {
        if (b.type != BuildingType::NUCLEAR || b.needsFuel) continue;
        PlayerEconomy& econ = (b.playerOwner == 1) ? p1 : p2;
        if (econ.silver >= fuel) {
            econ.silver -= fuel;
            econ.data.silver = econ.silver;
            continue;
        }
        b.needsFuel = true;
        b.rampProgress = 0.0f;
        b.currentOutputMW = 0.0f;
        PowerFx fx;
        fx.kind = PowerFxKind::REACTOR_NO_FUEL;
        fx.player = b.playerOwner;
        fx.buildingType = static_cast<int>(b.type);
        fx.owner = b.playerOwner;
        fx.pos = b.position;
        fx.radius = 60.0f;
        fx.title = "SCRAM: АЕЦ НЯМА ГОРИВО!";
        fx.detail = "Нужни са " + std::to_string(fuel) + " сребро на ден. Реакторът\nтръгва отново, щом ги съберете.";
        pushPowerFx(fx);
    }
}

// Lightning: reactors SCRAM, construction sites lose progress, finished mega-projects have rods
bool GameEngine::absorbLightningAt(sf::Vector2f pos) {
    for (auto& b : buildings) {
        if (std::hypot(b.position.x - pos.x, b.position.y - pos.y) > 30.0f) continue;
        if (b.type == BuildingType::NUCLEAR) {
            scramReactor(b, "МЪЛНИЯ");
            return true;
        }
        if (isMegaProject(b.type)) {
            PowerFx fx;
            fx.player = b.playerOwner;
            fx.buildingType = static_cast<int>(b.type);
            fx.owner = b.playerOwner;
            fx.pos = b.position;
            if (b.constructionLeft > 0.0f) {
                float lost = b.constructionTotal * PowerBalance::MEGA_LIGHTNING_SETBACK;
                b.constructionLeft = std::min(b.constructionTotal, b.constructionLeft + lost);
                fx.kind = PowerFxKind::MEGA_SETBACK;
                fx.title = "МЪЛНИЯ УДАРИ СТРОЕЖА!";
                fx.detail = getBuildingCost(b.type).nameBg + ": -" +
                            std::to_string(static_cast<int>(PowerBalance::MEGA_LIGHTNING_SETBACK * 100.0f)) + "% напредък.";
            } else {
                fx.kind = PowerFxKind::HAZARD_WARNING;
                fx.title = "ГРЪМООТВОДИТЕ ПОЕХА УДАРА";
                fx.detail = getBuildingCost(b.type).nameBg + " е невредим.";
            }
            pushPowerFx(fx);
            return true;
        }
        return false; // ordinary buildings: the caller deletes them (team decision)
    }
    return false;
}
