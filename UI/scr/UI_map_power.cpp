// =============================================================================
// Team b-power: UI_map glue for terrain (F-15), the nuclear reactor (F-32),
// hazards (F-34) and mega-projects (HX-10): engine PowerFx -> popups, notices
// and effects; repairs; 7/8/9 hotkeys; the bot's repair crew and advanced builds.
// =============================================================================
#include "../includes/UI_map.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

// Colours (the integrator maps these to UI_theme tokens)
const sf::Color MP_WARN(255, 190, 60);
const sf::Color MP_BAD(255, 90, 90);
const sf::Color MP_GOOD(0, 255, 180);
const sf::Color MP_MEGA(255, 215, 0);
const sf::Color MP_REACTOR(120, 230, 255);

const char* hazardBadge(HazardKind k) {
    switch (k) {
        case HazardKind::HAIL:     return "ГРАДУШКА";
        case HazardKind::FLOOD:    return "ПОТОП";
        case HazardKind::WILDFIRE: return "ПОЖАР";
        case HazardKind::QUAKE:    return "ТРУС";
        case HazardKind::NONE:     break;
    }
    return "ВНИМАНИЕ";
}

const char* hazardTitle(HazardKind k) {
    switch (k) {
        case HazardKind::HAIL:     return "Градушка!";
        case HazardKind::FLOOD:    return "Наводнение!";
        case HazardKind::WILDFIRE: return "Горски пожар!";
        case HazardKind::QUAKE:    return "Земетресение!";
        case HazardKind::NONE:     break;
    }
    return "";
}

std::string megaEffectShort(BuildingType t) {
    if (t == BuildingType::MEGA_FUSION) return "+" + std::to_string(PowerBalance::MEGA_FUSION.basePowerMW) + " MW денем и нощем";
    if (t == BuildingType::MEGA_SPACE_SOLAR) return "+" + std::to_string(PowerBalance::MEGA_SPACE_SOLAR.basePowerMW) + " MW от орбита";
    return std::to_string(PowerBalance::MEGA_PUMPED_HYDRO.batteryCapacityMWh) + " MWh съхранение";
}

} // namespace

void UI_map::resetPowerSystems() {
    powerLayer.reset();
    botPowerTimer = 4.0f;
    botRepairTimer = 3.0f;
    for (bool& k : p1PrevAdvKey) k = true; // keys held while the match starts are not fresh presses
    engine.drainPowerFx();
}

std::string UI_map::actionKeyLabel(int player) const {
    bool mouse = (player == 1) ? (controlScheme == ControlScheme::P1_MOUSE_P2_KEYBOARD || controlScheme == ControlScheme::BOTH_MOUSE)
                               : (controlScheme == ControlScheme::P1_KEYBOARD_P2_MOUSE || controlScheme == ControlScheme::BOTH_MOUSE);
    if (player == 1 && bot.isActive()) return "[SPACE/КЛИК]";
    if (mouse) return "[КЛИК]";
    return (player == 1) ? "[SPACE]" : "[ENTER]";
}

void UI_map::updatePowerSystems(float dt) {
    powerLayer.update(dt);
    for (const PowerFx& fx : engine.drainPowerFx()) {
        powerLayer.addFx(fx, engine);
        showPowerFx(fx);
    }
    if (bot.isActive()) updateBotPower(dt * engine.getTimeScale());
}

void UI_map::showPowerFx(const PowerFx& fx) {
    auto toPlayers = [&](const std::string& badge, const std::string& title, const std::string& detail,
                         const std::string& action, sf::Color accent) {
        for (int p = 1; p <= 2; ++p) {
            if (fx.player == 0 || fx.player == p) triggerPlayerPopup(p, badge, title, detail, action, accent);
        }
    };
    const std::string ownerName = (fx.owner == 2) ? "Играч 2" : "Играч 1";
    const std::string buildingName = engine.getBuildingCost(static_cast<BuildingType>(fx.buildingType)).nameBg;

    switch (fx.kind) {
        case PowerFxKind::HAZARD_WARNING: {
            if (fx.hazard == HazardKind::NONE) { // lightning absorbed by a finished mega-project
                toPlayers("МЪЛНИЯ", "Удар без щети", "Гръмоотводите на мегапроекта\nпоеха мълнията.", "", MP_MEGA);
                spawnNotice("ГРЪМООТВОДИТЕ ПОЕХА УДАРА!", fx.pos + sf::Vector2f(0.0f, -40.0f), MP_MEGA);
                break;
            }
            std::string detail;
            if (fx.hazard == HazardKind::HAIL) detail = "Градушката може да счупи\nслънчеви панели и мелници.";
            else if (fx.hazard == HazardKind::FLOOD) detail = "Дъждът не спира: реката\nможе да залее речния бряг.";
            else detail = "Суша: опасност от пожар\nв парцел със сгради.";
            detail += "\nПригответе дърво и желязо.";
            toPlayers("ВНИМАНИЕ", std::string(hazardTitle(fx.hazard)), detail, "Ремонт: 5 дърво + 5 желязо", MP_WARN);
            break;
        }
        case PowerFxKind::HAZARD_HIT: {
            int hits = static_cast<int>(fx.hits.size());
            std::string detail = (hits > 0) ? "Повредени сгради: " + std::to_string(hits) + ".\nНе дават ток до ремонт."
                                            : "Без щети този път.";
            if (fx.hazard == HazardKind::QUAKE && engine.getReactor(fx.player) != nullptr) detail += "\nАЕЦ спира аварийно (SCRAM).";
            bool botSide = (fx.player == 2 && bot.isActive());
            std::string action = (hits > 0 && !botSide) ? actionKeyLabel(fx.player) + " върху сградата: ремонт" : "";
            toPlayers(hazardBadge(fx.hazard), hazardTitle(fx.hazard), detail, action, (hits > 0) ? MP_BAD : MP_WARN);
            std::string notice = std::string(getHazardNameBg(fx.hazard)) + "!";
            spawnNotice(notice, fx.pos + sf::Vector2f(0.0f, -30.0f), hits > 0 ? MP_BAD : MP_WARN);
            break;
        }
        case PowerFxKind::REACTOR_SCRAM:
            toPlayers("АЕЦ", "SCRAM!", "Аварийно спиране на реактора.\nРестарт след половин ден,\nпосле 1 ден до пълна мощност.", "", MP_BAD);
            spawnNotice("SCRAM! АЕЦ СПРЯ АВАРИЙНО", fx.pos + sf::Vector2f(0.0f, -50.0f), MP_BAD);
            break;
        case PowerFxKind::REACTOR_NO_FUEL:
            toPlayers("АЕЦ", "Няма гориво!", "Реакторът иска " + std::to_string(PowerBalance::NUCLEAR_FUEL_SILVER_PER_DAY) +
                      " сребро на ден.\nТръгва отново, щом\nсъберете среброто.", "Добив: СРЕБРО", MP_BAD);
            spawnNotice("АЕЦ: НЯМА ГОРИВО (СРЕБРО)", fx.pos + sf::Vector2f(0.0f, -50.0f), MP_BAD);
            break;
        case PowerFxKind::REACTOR_RESTART:
            toPlayers("АЕЦ", "Рестарт", "Реакторът отново набира\nмощност (1 ден до 100%).", "", MP_REACTOR);
            break;
        case PowerFxKind::REACTOR_FULL:
            toPlayers("АЕЦ", "Пълна мощност!", "+" + std::to_string(PowerBalance::NUCLEAR.basePowerMW) +
                      " MW денем и нощем,\nпри всяко време.", "", MP_GOOD);
            spawnNotice("АЕЦ: +" + std::to_string(PowerBalance::NUCLEAR.basePowerMW) + " MW", fx.pos + sf::Vector2f(0.0f, -50.0f), MP_GOOD);
            break;
        case PowerFxKind::MEGA_STARTED: {
            const PlacedBuilding* m = engine.getMegaProject(fx.owner);
            bool half = (m != nullptr && engine.getConstructionProgress(*m) > 0.25f);
            if (half) {
                toPlayers("МЕГА", "Мегапроект 50%", ownerName + ":\n" + buildingName + "\nе наполовина готов.", "", MP_MEGA);
            } else {
                int days = (m != nullptr) ? static_cast<int>(std::lround(m->constructionTotal / Balance::SECONDS_PER_DAY)) : 0;
                toPlayers("МЕГА", "Нов мегапроект!", ownerName + " започна:\n" + buildingName + "\nГотов след " + std::to_string(days) + " дни.", "", MP_MEGA);
                spawnNotice("МЕГАПРОЕКТ: " + buildingName, fx.pos + sf::Vector2f(0.0f, -60.0f), MP_MEGA);
            }
            break;
        }
        case PowerFxKind::MEGA_SETBACK:
            toPlayers("МЪЛНИЯ", "Удар в строежа!", buildingName + ":\n-" +
                      std::to_string(static_cast<int>(PowerBalance::MEGA_LIGHTNING_SETBACK * 100.0f)) + "% напредък.", "", MP_BAD);
            spawnNotice("МЪЛНИЯ В СТРОЕЖА!", fx.pos + sf::Vector2f(0.0f, -60.0f), MP_BAD);
            break;
        case PowerFxKind::MEGA_COMPLETE:
            toPlayers("МЕГА", "Мегапроект готов!", ownerName + ":\n" + buildingName + "\n" +
                      megaEffectShort(static_cast<BuildingType>(fx.buildingType)) + ".", "", MP_MEGA);
            spawnNotice("МЕГАПРОЕКТЪТ Е ГОТОВ!", fx.pos + sf::Vector2f(0.0f, -60.0f), MP_MEGA);
            break;
        case PowerFxKind::MEGA_UNLOCKED:
            for (int p = 1; p <= 2; ++p) {
                std::string action = (p == 1) ? "[9] или страница 2" : (bot.isActive() ? "" : "[PgDn] до страница 2");
                triggerPlayerPopup(p, "МЕГА", "Мегапроекти!", "От днес: термоядрен реактор,\nкосмическа СЕЦ или ПАВЕЦ.\nПо един на играч.", action, MP_MEGA);
            }
            spawnNotice("МЕГАПРОЕКТИТЕ СА ОТКЛЮЧЕНИ!", { 800.0f, 560.0f }, MP_MEGA);
            break;
    }
}

bool UI_map::tryRepairAt(int player, sf::Vector2f pos) {
    const PlacedBuilding* target = nullptr;
    float best = 22.0f;
    for (const auto& b : engine.getBuildings()) {
        if (b.playerOwner != player || !b.isBroken) continue;
        float d = std::hypot(b.position.x - pos.x, b.position.y - pos.y);
        if (d < best) { best = d; target = &b; }
    }
    if (target == nullptr) return false;
    sf::Vector2f at = target->position;
    std::string msg;
    if (engine.repairBuilding(player, at, msg)) {
        spawnMiningParticles(at, sf::Color(255, 215, 0), 18);
        triggerPlayerPopup(player, "РЕМОНТ", "Поправено!", msg, "", MP_GOOD);
        spawnNotice("ПОПРАВЕНО!", at + sf::Vector2f(0.0f, -28.0f), MP_GOOD);
    } else {
        triggerPlayerPopup(player, "РЕМОНТ", "Няма материали", msg, "Добив: ГОРА и ЖЕЛЯЗО", MP_BAD);
        spawnNotice("НУЖНИ: 5 ДЪРВО + 5 ЖЕЛЯЗО", at + sf::Vector2f(0.0f, -28.0f), MP_BAD);
    }
    return true;
}

// P1 hotkeys 7 / 8 / 9: reactor, geothermal, mega-projects (9 again cycles the three projects)
void UI_map::handleAdvancedHotkeys() {
    bool p1Keyboard = bot.isActive() || controlScheme == ControlScheme::BOTH_KEYBOARD ||
                      controlScheme == ControlScheme::P1_KEYBOARD_P2_MOUSE;
    const sf::Keyboard::Key keys[3] = { sf::Keyboard::Key::Num7, sf::Keyboard::Key::Num8, sf::Keyboard::Key::Num9 };
    const sf::Keyboard::Key pads[3] = { sf::Keyboard::Key::Numpad7, sf::Keyboard::Key::Numpad8, sf::Keyboard::Key::Numpad9 };
    for (int i = 0; i < 3; ++i) {
        bool cur = sf::Keyboard::isKeyPressed(keys[i]) || (bot.isActive() && sf::Keyboard::isKeyPressed(pads[i]));
        bool fresh = cur && !p1PrevAdvKey[i];
        p1PrevAdvKey[i] = cur;
        if (!fresh || !p1Keyboard || p1Modal.active || showHelpOverlay) continue;

        BuildingType sel = BuildingType::NUCLEAR;
        if (i == 1) sel = BuildingType::GEOTHERMAL;
        if (i == 2) {
            BuildingType now = engine.getSelectedBuilding(1);
            sel = (now == BuildingType::MEGA_FUSION) ? BuildingType::MEGA_SPACE_SOLAR
                : (now == BuildingType::MEGA_SPACE_SOLAR) ? BuildingType::MEGA_PUMPED_HYDRO
                                                           : BuildingType::MEGA_FUSION;
        }
        engine.getPlayerEconomyMut(1).selectedBuilding = static_cast<int>(sel);
        p1Pos = engine.getGridSlot(1, p1GridCol, p1GridRow);
        BuildingCost c = engine.getBuildingCost(sel);
        std::string lock;
        std::string detail;
        if (!engine.isAdvancedUnlocked(1, sel, lock)) {
            detail = lock;
        } else if (sel == BuildingType::GEOTHERMAL) {
            detail = "Само на парцел ГЕЙЗЕР.\n+" + std::to_string(c.basePowerMW) + " MW постоянно.";
        } else if (sel == BuildingType::NUCLEAR) {
            detail = "Цял празен парцел. +400 MW\nслед 1 ден. 12 сребро/ден.";
        } else {
            detail = megaEffectShort(sel) + ".\nЦял празен парцел" + (sel == BuildingType::MEGA_PUMPED_HYDRO ? " на РЕКА." : ".");
        }
        triggerPlayerPopup(1, (i == 2 ? "МЕГА" : "СТРОЕЖ"), c.nameBg, detail, "[SPACE]: Постави | [9]: Друг проект | [X]: Отказ",
                           (i == 2) ? MP_MEGA : sf::Color(0, 229, 255));
    }
}

// The bot's repair crew (all levels) and advanced builds (medium: geothermal + reactor, hard: + mega-project)
void UI_map::updateBotPower(float gameDt) {
    BotDifficulty d = bot.getDifficulty();
    const sf::Color botColor(255, 120, 200);

    botRepairTimer -= gameDt;
    if (botRepairTimer <= 0.0f) {
        botRepairTimer = (d == BotDifficulty::HARD) ? 2.0f : (d == BotDifficulty::MEDIUM ? 4.0f : 7.0f);
        for (const auto& b : engine.getBuildings()) {
            if (b.playerOwner == 2 && b.isBroken) {
                sf::Vector2f at = b.position;
                std::string msg;
                if (engine.repairBuilding(2, at, msg)) spawnNotice("БОТ: РЕМОНТ", at + sf::Vector2f(0.0f, -28.0f), botColor);
                break;
            }
        }
    }

    if (d == BotDifficulty::EASY || d == BotDifficulty::NONE) return;
    botPowerTimer -= gameDt;
    if (botPowerTimer > 0.0f) return;
    botPowerTimer = (d == BotDifficulty::HARD) ? 5.0f : 9.0f;

    auto tryPlace = [&](BuildingType type, sf::Vector2f pos) {
        std::string reason;
        if (!engine.canPlaceBuilding(2, type, pos, reason)) return false;
        std::string msg;
        if (!engine.placeBuilding(2, type, pos, msg)) return false;
        spawnNotice("БОТ: " + engine.getBuildingCost(type).nameBg, pos + sf::Vector2f(0.0f, -40.0f), botColor);
        triggerPlayerPopup(2, "БОТ", "Нова постройка", msg, "", botColor);
        return true;
    };
    auto centre = [](const LandPlot& p) {
        return sf::Vector2f(p.bounds.position.x + p.bounds.size.x * 0.5f, p.bounds.position.y + p.bounds.size.y * 0.5f);
    };

    // 1. Geothermal wells on owned vents
    for (const auto& p : engine.getLandPlots()) {
        if (p.playerOwner != 2 || !p.isPurchased || p.terrain != static_cast<int>(TerrainType::VENT)) continue;
        for (int s = 0; s < 9; ++s) {
            sf::Vector2f slot = engine.getGridSlot(2, p.screenCol * 3 + s % 3, p.row * 3 + s / 3);
            if (tryPlace(BuildingType::GEOTHERMAL, slot)) return;
        }
    }
    // 2. A reactor on an empty owned plot (keeps 24 silver for two days of fuel)
    std::string lock;
    if (engine.isAdvancedUnlocked(2, BuildingType::NUCLEAR, lock) &&
        engine.getPlayerEconomy(2).silver >= PowerBalance::NUCLEAR.silverCost + 2 * PowerBalance::NUCLEAR_FUEL_SILVER_PER_DAY) {
        for (const auto& p : engine.getLandPlots()) {
            if (p.playerOwner == 2 && p.isPurchased && tryPlace(BuildingType::NUCLEAR, centre(p))) return;
        }
    }
    // 3. Hard: one mega-project (the dam on a river plot, else fusion, else the space array)
    if (d == BotDifficulty::HARD && engine.isAdvancedUnlocked(2, BuildingType::MEGA_FUSION, lock)) {
        const BuildingType order[3] = { BuildingType::MEGA_FUSION, BuildingType::MEGA_SPACE_SOLAR, BuildingType::MEGA_PUMPED_HYDRO };
        for (BuildingType t : order) {
            for (const auto& p : engine.getLandPlots()) {
                if (p.playerOwner == 2 && p.isPurchased && tryPlace(t, centre(p))) return;
            }
        }
    }
}

void UI_map::drawPowerOverlays(sf::RenderWindow& window) {
    // Repair prompts for human cursors resting on their own broken buildings
    if (engine.getSelectedBuilding(1) == BuildingType::NONE) {
        powerLayer.drawRepairPrompt(window, font, resourcesLoaded, engine, 1, p1Pos, actionKeyLabel(1));
    }
    if (!bot.isActive() && engine.getSelectedBuilding(2) == BuildingType::NONE) {
        powerLayer.drawRepairPrompt(window, font, resourcesLoaded, engine, 2, p2Pos, actionKeyLabel(2));
    }
}
