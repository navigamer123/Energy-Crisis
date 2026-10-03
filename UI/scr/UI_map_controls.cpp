#include "../includes/UI_map.h"
#include <algorithm>
#include <cstdio>
#include <string>

// =============================================================================
// UI_map Controls & Input Handling (Cursors, Actions, Hotkeys, Events)
// =============================================================================

void UI_map::drawPlayerCursors(sf::RenderWindow& window) {
    float animTime = animClock.getElapsedTime().asSeconds();

    // -------------------------------------------------------------------------
    // PLAYER 1 CURSOR (WEST SECTOR - CYAN)
    // -------------------------------------------------------------------------
    if (p1Pulse > 0.0f) {
        float radius = 24.0f + (1.0f - p1Pulse) * 60.0f;
        sf::CircleShape pulseCircle(radius);
        pulseCircle.setOrigin({ radius, radius });
        pulseCircle.setPosition(p1Pos);
        std::uint8_t alpha = static_cast<std::uint8_t>(p1Pulse * 220);
        pulseCircle.setFillColor(sf::Color(0, 229, 255, alpha / 4));
        pulseCircle.setOutlineThickness(2.0f);
        pulseCircle.setOutlineColor(sf::Color(0, 255, 200, alpha));
        window.draw(pulseCircle);
    }

    sf::CircleShape p1Core(8.0f);
    p1Core.setOrigin({ 8.0f, 8.0f });
    p1Core.setPosition(p1Pos);
    p1Core.setFillColor(sf::Color(0, 229, 255, 220));
    p1Core.setOutlineThickness(2.0f);
    p1Core.setOutlineColor(sf::Color::White);
    window.draw(p1Core);

    float rot1 = animTime * 90.0f;
    for (int i = 0; i < 4; i++) {
        sf::RectangleShape bracket({ 14.0f, 2.5f });
        bracket.setOrigin({ 20.0f, 1.25f });
        bracket.setPosition(p1Pos);
        bracket.setRotation(sf::degrees(rot1 + i * 90.0f));
        bracket.setFillColor(sf::Color(0, 255, 220));
        window.draw(bracket);
    }

    if (resourcesLoaded) {
        sf::Text p1Tag(font, "P1", 13);
        p1Tag.setFillColor(sf::Color(0, 255, 255));
        p1Tag.setPosition({ p1Pos.x - 8.0f, p1Pos.y - 28.0f });
        window.draw(p1Tag);
    }

    // -------------------------------------------------------------------------
    // PLAYER 2 CURSOR (EAST SECTOR - MAGENTA / GOLD)
    // -------------------------------------------------------------------------
    if (p2Pulse > 0.0f) {
        float radius = 24.0f + (1.0f - p2Pulse) * 60.0f;
        sf::CircleShape pulseCircle(radius);
        pulseCircle.setOrigin({ radius, radius });
        pulseCircle.setPosition(p2Pos);
        std::uint8_t alpha = static_cast<std::uint8_t>(p2Pulse * 220);
        pulseCircle.setFillColor(sf::Color(255, 120, 200, alpha / 4));
        pulseCircle.setOutlineThickness(2.0f);
        pulseCircle.setOutlineColor(sf::Color(255, 215, 0, alpha));
        window.draw(pulseCircle);
    }

    sf::CircleShape p2Core(8.0f);
    p2Core.setOrigin({ 8.0f, 8.0f });
    p2Core.setPosition(p2Pos);
    p2Core.setFillColor(sf::Color(255, 120, 200, 220));
    p2Core.setOutlineThickness(2.0f);
    p2Core.setOutlineColor(sf::Color::White);
    window.draw(p2Core);

    float rot2 = -animTime * 90.0f;
    for (int i = 0; i < 4; i++) {
        sf::RectangleShape bracket({ 14.0f, 2.5f });
        bracket.setOrigin({ 20.0f, 1.25f });
        bracket.setPosition(p2Pos);
        bracket.setRotation(sf::degrees(rot2 + i * 90.0f));
        bracket.setFillColor(sf::Color(255, 204, 0));
        window.draw(bracket);
    }

    if (resourcesLoaded) {
        std::string p2Label = "P2";
        if (bot.isActive()) {
            if (bot.getDifficulty() == BotDifficulty::EASY) p2Label = "P2 [BOT: ЛЕСЕН]";
            else if (bot.getDifficulty() == BotDifficulty::MEDIUM) p2Label = "P2 [BOT: СРЕДЕН]";
            else if (bot.getDifficulty() == BotDifficulty::HARD) p2Label = "P2 [BOT: ТРУДЕН]";
        }
        sf::Text p2Tag(font, toUtf8(p2Label), 13);
        p2Tag.setFillColor(bot.isActive() ? sf::Color(255, 215, 0) : sf::Color(255, 140, 220));
        sf::FloatRect tb = p2Tag.getLocalBounds();
        p2Tag.setPosition({ p2Pos.x - tb.size.x / 2.0f, p2Pos.y - 28.0f });
        window.draw(p2Tag);
    }
}

void UI_map::executeP1Action() {
    p1Pulse = 1.0f;
    BuildingType sel = engine.getSelectedBuilding(1);

    if (sel != BuildingType::NONE) {
        if (sel != BuildingType::DEMOLISH) {
            bool onPlot = false;
            for (const auto& plot : engine.getLandPlots()) {
                if (plot.playerOwner == 1 && plot.bounds.contains(p1Pos)) {
                    onPlot = true;
                    if (!plot.isPurchased) {
                        std::string buyMsg;
                        if (engine.buyLandPlot(1, plot.id, buyMsg)) {
                            triggerPlayerPopup(1, "ЗЕМЯ", "Купихте парцел!", "Парцелът е ваш. Натиснете пак SPACE за строеж.", "[SPACE]: Постави сградата", sf::Color(255, 215, 0));
                            spawnNotice("ЗАКУПЕН ПАРЦЕЛ!", p1Pos, sf::Color(255, 215, 0));
                        } else {
                            triggerPlayerModal(1, "НЕДОСТИГ НА ЗЛАТО", "Не можете да купите земята!", buyMsg, "Продавайте ток на града за да печелите пари и злато!", sf::Color(255, 180, 50));
                        }
                        return;
                    }
                    break;
                }
            }
            if (!onPlot) {
                // Outside buildable land plots: do not attempt placement and don't pop up any error
                return;
            }
        }
        sf::Vector2f targetPos = (sel == BuildingType::DEMOLISH) ? p1Pos : engine.snapToBuildingGrid(1, p1Pos);
        std::string msg;
        if (engine.placeBuilding(1, sel, targetPos, msg)) {
            triggerPlayerPopup(1, "УСПЕХ", "Действието е успешно!", msg, "[E]: Постави отново същата", sf::Color(0, 255, 180));
            spawnNotice("ПОСТРОЕНА СГРАДА!", targetPos, sf::Color(0, 255, 180));
            if (sel != BuildingType::DEMOLISH) engine.clearBuildingSelection(1);
        } else {
            triggerPlayerModal(1, "ГРЕШКА ПРИ СТРОЕЖ", "Строежът е невъзможен!", msg,
                               (!engine.isDaylight() ? "Поставете и захранете Осветителна лампа за работа нощем!" : "Проверете ресурсите си или изберете друго място!"), sf::Color(255, 75, 75));
        }
    } else {
        ResourceType resType = nodes.getP1ResourceAt(p1Pos);
        if (resType != ResourceType::NONE) {
            if (p1ResourceCooldown > 0.0f) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "ИЗЧАКАЙТЕ: %.1fs", p1ResourceCooldown);
                spawnNotice(buf, p1Pos + sf::Vector2f(0.0f, -25.0f), sf::Color(255, 180, 50));
                return;
            }
            GameEngine::MineResult res;
            std::string msg;
            if (engine.mineResource(1, resType, res, msg)) {
                p1ResourceCooldown = 1.0f;
                const auto* st = nodes.getStation(1, resType);
                sf::Color c = st ? st->themeColor : sf::Color(0, 229, 255);
                spawnMiningParticles(p1Pos, c, 18);
                triggerPlayerPopup(1, "ДОБИВ", msg, "Ресурсът е добавен в склада.", "[SPACE]: Добив (на 1 сек)", c);
                spawnNotice(msg, p1Pos + sf::Vector2f(0.0f, -25.0f), c);
            }
        } else {
            for (const auto& plot : engine.getLandPlots()) {
                if (plot.playerOwner == 1 && plot.bounds.contains(p1Pos)) {
                    if (!plot.isPurchased) {
                        std::string msg;
                        if (engine.buyLandPlot(1, plot.id, msg)) {
                            triggerPlayerPopup(1, "ЗЕМЯ", "Закупен парцел!", "Парцелът е ваш. Натиснете E за избор на сграда.", "[E]: Избери сграда", sf::Color(255, 215, 0));
                            spawnNotice("ЗАКУПЕН ПАРЦЕЛ!", p1Pos, sf::Color(255, 215, 0));
                        } else {
                            triggerPlayerModal(1, "НЕДОСТИГ НА ЗЛАТО", "Не можете да купите парцела!", msg, "Продавайте ток на града за да печелите пари и злато!", sf::Color(255, 180, 50));
                        }
                    } else {
                        triggerPlayerPopup(1, "ИНФО", "Ваш парцел", "Земята е свободна за строителство.", "[E]: Изберете сграда за строеж", sf::Color(0, 229, 255));
                    }
                    break;
                }
            }
        }
    }
}

void UI_map::executeP2Action() {
    p2Pulse = 1.0f;
    BuildingType sel = engine.getSelectedBuilding(2);

    if (sel != BuildingType::NONE) {
        if (sel != BuildingType::DEMOLISH) {
            bool onPlot = false;
            for (const auto& plot : engine.getLandPlots()) {
                if (plot.playerOwner == 2 && plot.bounds.contains(p2Pos)) {
                    onPlot = true;
                    if (!plot.isPurchased) {
                        std::string buyMsg;
                        if (engine.buyLandPlot(2, plot.id, buyMsg)) {
                            triggerPlayerPopup(2, "ЗЕМЯ", "Купихте парцел!", "Парцелът е ваш. Натиснете пак ENTER за строеж.", "[ENTER]: Постави сградата", sf::Color(255, 215, 0));
                            spawnNotice("ЗАКУПЕН ПАРЦЕЛ!", p2Pos, sf::Color(255, 215, 0));
                        } else {
                            triggerPlayerModal(2, "НЕДОСТИГ НА ЗЛАТО", "Не можете да купите земята!", buyMsg, "Продавайте ток на града за да печелите пари и злато!", sf::Color(255, 180, 50));
                        }
                        return;
                    }
                    break;
                }
            }
            if (!onPlot) {
                // Outside buildable land plots: do not attempt placement and don't pop up any error
                return;
            }
        }
        sf::Vector2f targetPos = (sel == BuildingType::DEMOLISH) ? p2Pos : engine.snapToBuildingGrid(2, p2Pos);
        std::string msg;
        if (engine.placeBuilding(2, sel, targetPos, msg)) {
            triggerPlayerPopup(2, "УСПЕХ", "Действието е успешно!", msg, "[PgDn]: Постави отново същата", sf::Color(255, 120, 200));
            spawnNotice("ПОСТРОЕНА СГРАДА!", targetPos, sf::Color(255, 120, 200));
            if (sel != BuildingType::DEMOLISH) engine.clearBuildingSelection(2);
        } else {
            triggerPlayerModal(2, "ГРЕШКА ПРИ СТРОЕЖ", "Строежът е невъзможен!", msg,
                               (!engine.isDaylight() ? "Поставете и захранете Осветителна лампа за работа нощем!" : "Проверете ресурсите си или изберете друго място!"), sf::Color(255, 75, 75));
        }
    } else {
        ResourceType resType = nodes.getP2ResourceAt(p2Pos);
        if (resType != ResourceType::NONE) {
            if (p2ResourceCooldown > 0.0f) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "ИЗЧАКАЙТЕ: %.1fs", p2ResourceCooldown);
                spawnNotice(buf, p2Pos + sf::Vector2f(0.0f, -25.0f), sf::Color(255, 180, 50));
                return;
            }
            GameEngine::MineResult res;
            std::string msg;
            if (engine.mineResource(2, resType, res, msg)) {
                p2ResourceCooldown = 1.0f;
                const auto* st = nodes.getStation(2, resType);
                sf::Color c = st ? st->themeColor : sf::Color(255, 140, 210);
                spawnMiningParticles(p2Pos, c, 18);
                triggerPlayerPopup(2, "ДОБИВ", msg, "Ресурсът е добавен в склада.", "[ENTER]: Добив (на 1 сек)", c);
                spawnNotice(msg, p2Pos + sf::Vector2f(0.0f, -25.0f), c);
            }
        } else {
            for (const auto& plot : engine.getLandPlots()) {
                if (plot.playerOwner == 2 && plot.bounds.contains(p2Pos)) {
                    if (!plot.isPurchased) {
                        std::string msg;
                        if (engine.buyLandPlot(2, plot.id, msg)) {
                            triggerPlayerPopup(2, "ЗЕМЯ", "Закупен парцел!", "Парцелът е ваш. Натиснете PgDn за избор.", "[PgDn]: Избери сграда", sf::Color(255, 215, 0));
                            spawnNotice("ЗАКУПЕН ПАРЦЕЛ!", p2Pos, sf::Color(255, 215, 0));
                        } else {
                            triggerPlayerPopup(2, "ГРЕШКА", "Няма злато!", msg, "[PgUp]: Отказ", sf::Color(255, 90, 90));
                        }
                    } else {
                        triggerPlayerPopup(2, "ИНФО", "Ваш парцел", "Земята е свободна за строителство.", "[PgDn]: Изберете сграда за строеж", sf::Color(255, 140, 220));
                    }
                    break;
                }
            }
        }
    }
}

void UI_map::executeP1Upgrade() {
    p1Pulse = 1.0f;
    ResourceType resType = nodes.getP1StationAt(p1Pos);
    if (resType == ResourceType::NONE || resType == ResourceType::MONEY) {
        spawnNotice("ЗАСТАНЕТЕ ВЪРХУ МИНА ЗА ДА Я НАДГРАДИТЕ!", p1Pos + sf::Vector2f(0.0f, -25.0f), sf::Color(255, 180, 50));
        return;
    }
    std::string msg;
    if (engine.upgradeMine(1, resType, msg)) {
        spawnMiningParticles(p1Pos, sf::Color(255, 215, 0), 28);
        triggerPlayerPopup(1, "НАДГРАЖДАНЕ", msg, "Добивът от тази мина е увеличен с +75%!", "[SPACE]: Добив | [F]: Нов ъпгрейд", sf::Color(255, 215, 0));
        spawnNotice(msg, p1Pos + sf::Vector2f(0.0f, -25.0f), sf::Color(255, 215, 0));
    } else {
        triggerPlayerPopup(1, "ГРЕШКА", msg, "Печелете злато от доставка на ток към града!", "", sf::Color(255, 90, 90));
        spawnNotice(msg, p1Pos + sf::Vector2f(0.0f, -25.0f), sf::Color(255, 90, 90));
    }
}

void UI_map::executeP2Upgrade() {
    p2Pulse = 1.0f;
    ResourceType resType = nodes.getP2StationAt(p2Pos);
    if (resType == ResourceType::NONE || resType == ResourceType::MONEY) {
        spawnNotice("ЗАСТАНЕТЕ ВЪРХУ МИНА ЗА ДА Я НАДГРАДИТЕ!", p2Pos + sf::Vector2f(0.0f, -25.0f), sf::Color(255, 180, 50));
        return;
    }
    std::string msg;
    if (engine.upgradeMine(2, resType, msg)) {
        spawnMiningParticles(p2Pos, sf::Color(255, 215, 0), 28);
        triggerPlayerPopup(2, "НАДГРАЖДАНЕ", msg, "Добивът от тази мина е увеличен с +75%!", "[ENTER]: Добив | [RShift]: Нов ъпгрейд", sf::Color(255, 215, 0));
        spawnNotice(msg, p2Pos + sf::Vector2f(0.0f, -25.0f), sf::Color(255, 215, 0));
    } else {
        triggerPlayerPopup(2, "ГРЕШКА", msg, "Печелете злато от доставка на ток към града!", "", sf::Color(255, 90, 90));
        spawnNotice(msg, p2Pos + sf::Vector2f(0.0f, -25.0f), sf::Color(255, 90, 90));
    }
}

void UI_map::updateControls(const sf::RenderWindow& window, float dt) {
    if (isPaused || engine.getCityState().winner != 0) {
        if (engine.getCityState().winner != 0 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::R)) {
            restartMatch();
        }
        return;
    }

    float speed = 360.0f;
    sf::Vector2f mPos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    // 1. Update floating notices & mining particles
    for (auto it = notices.begin(); it != notices.end();) {
        it->timer -= dt;
        it->pos.y -= 40.0f * dt;
        if (it->timer <= 0.0f) it = notices.erase(it);
        else ++it;
    }
    updateMiningParticles(dt);

    // 2. Resource zone detection -> 6x time speedup!
    bool p1InRes = (nodes.getP1ResourceAt(p1Pos) != ResourceType::NONE);
    bool p2InRes = (nodes.getP2ResourceAt(p2Pos) != ResourceType::NONE);
    if (p1InRes || p2InRes) {
        engine.setTimeScale(6.0f);
    } else {
        engine.setTimeScale(1.0f);
    }

    // Decrement grid step cooldowns
    if (p1GridStepCooldown > 0.0f) p1GridStepCooldown -= dt;
    if (p2GridStepCooldown > 0.0f) p2GridStepCooldown -= dt;

    // Helper to format building costs
    auto formatCost = [](const BuildingCost& c) {
        std::string s = "Нужно: " + std::to_string(c.woodCost) + " Дърво";
        if (c.ironCost > 0) s += ", " + std::to_string(c.ironCost) + " Жел";
        if (c.copperCost > 0) s += ", " + std::to_string(c.copperCost) + " Мед";
        if (c.siliconCost > 0) s += ", " + std::to_string(c.siliconCost) + " Сил";
        if (c.coalCost > 0) s += ", " + std::to_string(c.coalCost) + " Въгл";
        if (c.silverCost > 0) s += ", " + std::to_string(c.silverCost) + " Среб";
        return s;
    };

    // 3. Player 1 Movement (Precision Grid during placement, smooth analog otherwise)
    bool p1BuildingMode = (engine.getSelectedBuilding(1) != BuildingType::NONE);
    if (p1BuildingMode) {
        if (controlScheme == ControlScheme::BOTH_KEYBOARD || controlScheme == ControlScheme::P1_KEYBOARD_P2_MOUSE) {
            if (p1GridStepCooldown <= 0.0f) {
                bool moved = false;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
                    p1GridRow = std::max(0, p1GridRow - 1);
                    moved = true;
                } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
                    p1GridRow = std::min(11, p1GridRow + 1);
                    moved = true;
                }
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
                    p1GridCol = std::max(0, p1GridCol - 1);
                    moved = true;
                } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
                    p1GridCol = std::min(8, p1GridCol + 1);
                    moved = true;
                }
                if (moved) {
                    p1Pos = engine.getGridSlot(1, p1GridCol, p1GridRow);
                    p1GridStepCooldown = 0.14f;
                }
            }
        } else {
            engine.getClosestGridIndex(1, mPos, p1GridCol, p1GridRow);
            p1Pos = engine.getGridSlot(1, p1GridCol, p1GridRow);
        }
    } else {
        if (controlScheme == ControlScheme::BOTH_KEYBOARD || controlScheme == ControlScheme::P1_KEYBOARD_P2_MOUSE) {
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) p1Pos.y -= speed * dt;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) p1Pos.y += speed * dt;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) p1Pos.x -= speed * dt;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) p1Pos.x += speed * dt;
        } else if (controlScheme == ControlScheme::P1_MOUSE_P2_KEYBOARD) {
            p1Pos = mPos;
        } else if (controlScheme == ControlScheme::BOTH_MOUSE) {
            if (mPos.x <= 800.0f) p1Pos = mPos;
        }
        p1Pos.x = std::max(30.0f, std::min(p1Pos.x, 780.0f));
        p1Pos.y = std::max(40.0f, std::min(p1Pos.y, 860.0f));
        engine.getClosestGridIndex(1, p1Pos, p1GridCol, p1GridRow);
    }

    // 4. Player 2 Movement (Bot AI or Human Input)
    if (bot.isActive()) {
        bool botTriggerAction = false;
        bool botTriggerUpgrade = false;
        BuildingType botSel = BuildingType::NONE;
        bot.update(dt, engine, nodes, p2Pos, botTriggerAction, botTriggerUpgrade, botSel);

        if (botSel != BuildingType::NONE) {
            engine.getPlayerEconomyMut(2).selectedBuilding = static_cast<int>(botSel);
        }

        if (botTriggerAction && p2ActionCooldown <= 0.0f && !p2Modal.active && !showHelpOverlay) {
            executeP2Action();
            p2ActionCooldown = 0.20f;
        }

        if (botTriggerUpgrade && !p2Modal.active && !showHelpOverlay) {
            executeP2Upgrade();
        }
    } else {
        bool p2BuildingMode = (engine.getSelectedBuilding(2) != BuildingType::NONE);
        if (p2BuildingMode) {
            if (controlScheme == ControlScheme::BOTH_KEYBOARD || controlScheme == ControlScheme::P1_MOUSE_P2_KEYBOARD) {
                if (p2GridStepCooldown <= 0.0f) {
                    bool moved = false;
                    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up)) {
                        p2GridRow = std::max(0, p2GridRow - 1);
                        moved = true;
                    } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down)) {
                        p2GridRow = std::min(11, p2GridRow + 1);
                        moved = true;
                    }
                    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)) {
                        p2GridCol = std::max(0, p2GridCol - 1);
                        moved = true;
                    } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) {
                        p2GridCol = std::min(8, p2GridCol + 1);
                        moved = true;
                    }
                    if (moved) {
                        p2Pos = engine.getGridSlot(2, p2GridCol, p2GridRow);
                        p2GridStepCooldown = 0.14f;
                    }
                }
            } else {
                engine.getClosestGridIndex(2, mPos, p2GridCol, p2GridRow);
                p2Pos = engine.getGridSlot(2, p2GridCol, p2GridRow);
            }
        } else {
            if (controlScheme == ControlScheme::BOTH_KEYBOARD || controlScheme == ControlScheme::P1_MOUSE_P2_KEYBOARD) {
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up)) p2Pos.y -= speed * dt;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down)) p2Pos.y += speed * dt;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)) p2Pos.x -= speed * dt;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) p2Pos.x += speed * dt;
            } else if (controlScheme == ControlScheme::P1_KEYBOARD_P2_MOUSE) {
                p2Pos = mPos;
            } else if (controlScheme == ControlScheme::BOTH_MOUSE) {
                if (mPos.x >= 800.0f) p2Pos = mPos;
            }
            p2Pos.x = std::max(820.0f, std::min(p2Pos.x, 1570.0f));
            p2Pos.y = std::max(40.0f, std::min(p2Pos.y, 860.0f));
            engine.getClosestGridIndex(2, p2Pos, p2GridCol, p2GridRow);
        }
    }

    // 5. Action and selection cooldown decrements
    if (p1ActionCooldown > 0.0f) p1ActionCooldown -= dt;
    if (p2ActionCooldown > 0.0f) p2ActionCooldown -= dt;
    if (p1ResourceCooldown > 0.0f) p1ResourceCooldown -= dt;
    if (p2ResourceCooldown > 0.0f) p2ResourceCooldown -= dt;
    if (p1SelectCooldown > 0.0f) p1SelectCooldown -= dt;
    if (p2SelectCooldown > 0.0f) p2SelectCooldown -= dt;

    // 6. Player 1 Action Input (Single Press only, NO continuous hold-to-mine!)
    bool p1PressingAction = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
    bool p1JustPressed = p1PressingAction && !p1PrevAction;
    p1PrevAction = p1PressingAction;

    if (p1JustPressed && p1ActionCooldown <= 0.0f && !p1Modal.active && !showHelpOverlay) {
        executeP1Action();
        p1ActionCooldown = 0.20f;
    }

    // P1 Upgrade Mine with Gold: [F]
    bool p1PressingUpgrade = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F);
    static bool p1PrevUpgrade = false;
    if (p1PressingUpgrade && !p1PrevUpgrade && !p1Modal.active && !showHelpOverlay) {
        executeP1Upgrade();
    }
    p1PrevUpgrade = p1PressingUpgrade;

    // P1: [E] Cycle Forward
    bool curE = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E);
    if (curE && !p1PrevE && p1SelectCooldown <= 0.0f && !p1Modal.active && !showHelpOverlay) {
        engine.cycleBuildingSelection(1);
        p1SelectCooldown = 0.16f;
        BuildingType newSel = engine.getSelectedBuilding(1);
        BuildingCost c = engine.getBuildingCost(newSel);
        p1Pos = engine.getGridSlot(1, p1GridCol, p1GridRow);
        if (newSel == BuildingType::DEMOLISH) {
            triggerPlayerPopup(1, "ПРЕМАХВАНЕ", c.nameBg, "Кликнете върху ваша сграда за разрушаване.\nВръща 50% от ресурсите.", "[SPACE]: Премахни | [E]: Следваща | [Q]: Предишна", sf::Color(255, 80, 80));
        } else if (newSel == BuildingType::LAMP) {
            triggerPlayerPopup(1, "ОСВЕТЛЕНИЕ", c.nameBg, formatCost(c) + ".\nОсветява нощем за строителство.", "[SPACE]: Постави | [E]: Следваща | [Q]: Предишна", sf::Color(255, 220, 100));
        } else {
            triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg,
                               formatCost(c) + ".\nДобив: +" + std::to_string(c.basePowerMW) + " MW ток.",
                               "[SPACE]: Постави в грида | [E]: Следваща | [Q]: Предишна", sf::Color(0, 229, 255));
        }
    }
    p1PrevE = curE;

    // P1: [Q] Cycle Backward / Cancel
    bool curQ = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q);
    if (curQ && !p1PrevQ && p1SelectCooldown <= 0.0f && !p1Modal.active && !showHelpOverlay) {
        if (engine.getSelectedBuilding(1) != BuildingType::NONE) {
            engine.cycleBuildingSelectionPrev(1);
            p1SelectCooldown = 0.16f;
            BuildingType newSel = engine.getSelectedBuilding(1);
            BuildingCost c = engine.getBuildingCost(newSel);
            p1Pos = engine.getGridSlot(1, p1GridCol, p1GridRow);
            if (newSel == BuildingType::DEMOLISH) {
                triggerPlayerPopup(1, "ПРЕМАХВАНЕ", c.nameBg, "Кликнете върху ваша сграда за разрушаване.\nВръща 50% от ресурсите.", "[SPACE]: Премахни | [E]: Следваща | [Q]: Предишна", sf::Color(255, 80, 80));
            } else if (newSel == BuildingType::LAMP) {
                triggerPlayerPopup(1, "ОСВЕТЛЕНИЕ", c.nameBg, formatCost(c) + ".\nОсветява нощем за строителство.", "[SPACE]: Постави | [E]: Следваща | [Q]: Предишна", sf::Color(255, 220, 100));
            } else {
                triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg,
                                   formatCost(c) + ".\nДобив: +" + std::to_string(c.basePowerMW) + " MW ток.",
                                   "[SPACE]: Постави в грида | [E]: Следваща | [Q]: Предишна", sf::Color(0, 229, 255));
            }
        }
    }
    p1PrevQ = curQ;

    bool curX = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::X);
    if (curX && !p1PrevX) {
        if (engine.getSelectedBuilding(1) != BuildingType::NONE) {
            engine.clearBuildingSelection(1);
            triggerPlayerPopup(1, "ОТКАЗ", "Изборът е прекратен", "Свободен режим.", "[E]: Избери сграда", sf::Color(180, 180, 180));
        } else {
            engine.getPlayerEconomyMut(1).selectedBuilding = static_cast<int>(BuildingType::DEMOLISH);
            p1Pos = engine.getGridSlot(1, p1GridCol, p1GridRow);
            triggerPlayerPopup(1, "ПРЕМАХВАНЕ", "Режим Разрушаване", "Посочете сградата, която искате да махнете.", "[SPACE]: Премахни | [X]: Отказ", sf::Color(255, 80, 80));
        }
    }
    p1PrevX = curX;

    // Direct Hotkeys 1..6 for P1
    for (int k = 1; k <= 6; ++k) {
        sf::Keyboard::Key numKey = static_cast<sf::Keyboard::Key>(static_cast<int>(sf::Keyboard::Key::Num1) + (k - 1));
        bool curNum = sf::Keyboard::isKeyPressed(numKey);
        if (curNum && !p1PrevNum[k] && !p1Modal.active && !showHelpOverlay) {
            engine.getPlayerEconomyMut(1).selectedBuilding = k;
            BuildingCost c = engine.getBuildingCost(static_cast<BuildingType>(k));
            p1Pos = engine.getGridSlot(1, p1GridCol, p1GridRow);
            triggerPlayerPopup(1, (k == 6 ? "ПРЕМАХВАНЕ" : "СТРОЕЖ"), c.nameBg,
                               (k == 6 ? "Посочете сграда за разрушаване." : formatCost(c)),
                               "[SPACE]: Постави в грида | [X]: Отказ", (k == 6 ? sf::Color(255, 80, 80) : sf::Color(0, 229, 255)));
        }
        p1PrevNum[k] = curNum;
    }

    // 7. Player 2 Action Input (Human Player 2 only)
    if (!bot.isActive()) {
        bool p2PressingAction = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter);
        bool p2JustPressed = p2PressingAction && !p2PrevAction;
        p2PrevAction = p2PressingAction;

        if (p2JustPressed && p2ActionCooldown <= 0.0f && !p2Modal.active && !showHelpOverlay) {
            executeP2Action();
            p2ActionCooldown = 0.20f;
        }

        // P2 Upgrade Mine with Gold: [RShift] or [End]
        bool p2PressingUpgrade = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::End);
        static bool p2PrevUpgrade = false;
        if (p2PressingUpgrade && !p2PrevUpgrade && !p2Modal.active && !showHelpOverlay) {
            executeP2Upgrade();
        }
        p2PrevUpgrade = p2PressingUpgrade;

        // P2: [PgDn] Cycle Forward (Solar -> Wind -> Hydro -> Battery -> Lamp -> Demolish)
        bool curPgDn = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::PageDown);
        if (curPgDn && !p2PrevPgDn && p2SelectCooldown <= 0.0f && !p2Modal.active && !showHelpOverlay) {
            engine.cycleBuildingSelection(2);
            p2SelectCooldown = 0.16f;
            BuildingType newSel = engine.getSelectedBuilding(2);
            BuildingCost c = engine.getBuildingCost(newSel);
            p2Pos = engine.getGridSlot(2, p2GridCol, p2GridRow);
            if (newSel == BuildingType::DEMOLISH) {
                triggerPlayerPopup(2, "ПРЕМАХВАНЕ", c.nameBg, "Кликнете върху ваша сграда за разрушаване.\nВръща 50% от ресурсите.", "[ENTER]: Премахни | [PgDn]: Следваща | [PgUp]: Предишна", sf::Color(255, 80, 80));
            } else if (newSel == BuildingType::LAMP) {
                triggerPlayerPopup(2, "ОСВЕТЛЕНИЕ", c.nameBg, formatCost(c) + ".\nОсветява нощем за строителство.", "[ENTER]: Постави | [PgDn]: Следваща | [PgUp]: Предишна", sf::Color(255, 220, 100));
            } else {
                triggerPlayerPopup(2, "СТРОЕЖ", c.nameBg,
                                   formatCost(c) + ".\nДобив: +" + std::to_string(c.basePowerMW) + " MW ток.",
                                   "[ENTER]: Постави в грида | [PgDn]: Следваща | [PgUp]: Предишна", sf::Color(255, 120, 200));
            }
        }
        p2PrevPgDn = curPgDn;

        // P2: [PgUp] Cycle Backward
        bool curPgUp = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::PageUp);
        if (curPgUp && !p2PrevPgUp && p2SelectCooldown <= 0.0f && !p2Modal.active && !showHelpOverlay) {
            engine.cycleBuildingSelectionPrev(2);
            p2SelectCooldown = 0.16f;
            BuildingType newSel = engine.getSelectedBuilding(2);
            BuildingCost c = engine.getBuildingCost(newSel);
            p2Pos = engine.getGridSlot(2, p2GridCol, p2GridRow);
            if (newSel == BuildingType::DEMOLISH) {
                triggerPlayerPopup(2, "ПРЕМАХВАНЕ", c.nameBg, "Кликнете върху ваша сграда за разрушаване.\nВръща 50% от ресурсите.", "[ENTER]: Премахни | [PgDn]: Следваща | [PgUp]: Предишна", sf::Color(255, 80, 80));
            } else if (newSel == BuildingType::LAMP) {
                triggerPlayerPopup(2, "ОСВЕТЛЕНИЕ", c.nameBg, formatCost(c) + ".\nОсветява нощем за строителство.", "[ENTER]: Постави | [PgDn]: Следваща | [PgUp]: Предишна", sf::Color(255, 220, 100));
            } else {
                triggerPlayerPopup(2, "СТРОЕЖ", c.nameBg,
                                   formatCost(c) + ".\nДобив: +" + std::to_string(c.basePowerMW) + " MW ток.",
                                   "[ENTER]: Постави в грида | [PgDn]: Следваща | [PgUp]: Предишна", sf::Color(255, 120, 200));
            }
        }
        p2PrevPgUp = curPgUp;

        bool curDel = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Delete) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::End);
        if (curDel && !p2PrevDel) {
            if (engine.getSelectedBuilding(2) != BuildingType::NONE) {
                engine.clearBuildingSelection(2);
                triggerPlayerPopup(2, "ОТКАЗ", "Изборът е прекратен", "Свободен режим.", "[PgDn]: Избери сграда", sf::Color(180, 180, 180));
            } else {
                engine.getPlayerEconomyMut(2).selectedBuilding = static_cast<int>(BuildingType::DEMOLISH);
                p2Pos = engine.getGridSlot(2, p2GridCol, p2GridRow);
                triggerPlayerPopup(2, "ПРЕМАХВАНЕ", "Режим Разрушаване", "Посочете сградата, която искате да махнете.", "[ENTER]: Премахни | [Del]: Отказ", sf::Color(255, 80, 80));
            }
        }
        p2PrevDel = curDel;
    }

    // Pulse decay
    if (p1Pulse > 0.0f) {
        p1Pulse -= dt * 2.2f;
        if (p1Pulse < 0.0f) p1Pulse = 0.0f;
    }
    if (p2Pulse > 0.0f) {
        p2Pulse -= dt * 2.2f;
        if (p2Pulse < 0.0f) p2Pulse = 0.0f;
    }

    // Player Side Popup Timers
    if (p1Popup.active) {
        p1Popup.timer -= dt;
        if (p1Popup.timer <= 0.0f) p1Popup.active = false;
    }
    if (p2Popup.active) {
        p2Popup.timer -= dt;
        if (p2Popup.timer <= 0.0f) p2Popup.active = false;
    }
}

void UI_map::handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    // -------------------------------------------------------------------------
    // -1. If Victory Screen is active, handle Restart [R], Menu [ESC/M], or button clicks
    // -------------------------------------------------------------------------
    if (engine.getCityState().winner != 0) {
        if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
            if (key->code == sf::Keyboard::Key::R) {
                restartMatch();
                return;
            }
            if (key->code == sf::Keyboard::Key::Escape || key->code == sf::Keyboard::Key::M) {
                requestMenu = true;
                return;
            }
        }
        if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
            sf::Vector2f clickPos = window.mapPixelToCoords(mb->position);
            if (victoryRestartBtn.contains(clickPos)) {
                restartMatch();
                return;
            }
            if (victoryMenuBtn.contains(clickPos)) {
                requestMenu = true;
                return;
            }
        }
        return;
    }

    // -------------------------------------------------------------------------
    // 0. If Help overlay is active, any dismiss key or click closes it.
    //    Stays paused after (if it was opened from the pause menu).
    // -------------------------------------------------------------------------
    if (showHelpOverlay) {
        if (event.is<sf::Event::MouseButtonPressed>()) {
            showHelpOverlay = false;
            return; // stays paused if isPaused is true
        }
        if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
            if (key->code == sf::Keyboard::Key::H || key->code == sf::Keyboard::Key::F1 ||
                key->code == sf::Keyboard::Key::Escape || key->code == sf::Keyboard::Key::Enter ||
                key->code == sf::Keyboard::Key::Space) {
                showHelpOverlay = false;
                return; // stays paused if isPaused is true
            }
        }
        return; // swallow all other input while help is open
    }

    // -------------------------------------------------------------------------
    // 1. Check if interactive player modal dialog is active (any click or key dismisses easily)
    // -------------------------------------------------------------------------
    if (p1Modal.active || p2Modal.active) {
        if (event.is<sf::Event::MouseButtonPressed>() || event.is<sf::Event::KeyPressed>()) {
            if (p1Modal.active) closePlayerModal(1);
            if (p2Modal.active) closePlayerModal(2);
            return;
        }
    }

    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        if (key->code == sf::Keyboard::Key::F11) {
            requestFullscreenToggle = true;
            return;
        }
    }

    // -------------------------------------------------------------------------
    // 2. Pause Menu Event Handling
    // -------------------------------------------------------------------------
    if (isPaused) {
        if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
            if (key->code == sf::Keyboard::Key::Escape) {
                isPaused = false;
                return;
            }
            if (key->code == sf::Keyboard::Key::Up || key->code == sf::Keyboard::Key::W) {
                pauseSelectedIdx = (pauseSelectedIdx + 3) % 4;
                return;
            }
            if (key->code == sf::Keyboard::Key::Down || key->code == sf::Keyboard::Key::S) {
                pauseSelectedIdx = (pauseSelectedIdx + 1) % 4;
                return;
            }
            if (key->code == sf::Keyboard::Key::Enter || key->code == sf::Keyboard::Key::Space) {
                if (pauseSelectedIdx == 0) {
                    isPaused = false;
                } else if (pauseSelectedIdx == 1) {
                    isPaused = false;
                    restartMatch();
                } else if (pauseSelectedIdx == 2) {
                    showHelpOverlay = true;
                } else if (pauseSelectedIdx == 3) {
                    isPaused = false;
                    requestMenu = true;
                }
                return;
            }
            if (key->code == sf::Keyboard::Key::R) {
                isPaused = false;
                restartMatch();
                return;
            }
            if (key->code == sf::Keyboard::Key::H || key->code == sf::Keyboard::Key::F1) {
                showHelpOverlay = true;
                return;
            }
            if (key->code == sf::Keyboard::Key::M) {
                isPaused = false;
                requestMenu = true;
                return;
            }
        }
        if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
            sf::Vector2f clickPos = window.mapPixelToCoords(mb->position);
            if (pauseResumeBtn.contains(clickPos)) {
                isPaused = false;
                return;
            }
            if (pauseRestartBtn.contains(clickPos)) {
                isPaused = false;
                restartMatch();
                return;
            }
            if (pauseHelpBtn.contains(clickPos)) {
                showHelpOverlay = true;
                return;
            }
            if (pauseMenuBtn.contains(clickPos)) {
                isPaused = false;
                requestMenu = true;
                return;
            }
        }
        return;
    }

    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        if (key->code == sf::Keyboard::Key::H || key->code == sf::Keyboard::Key::F1) {
            // Auto-pause then open help on top
            isPaused = true;
            pauseSelectedIdx = 0;
            showHelpOverlay = true;
            return;
        }

        if (key->code == sf::Keyboard::Key::Escape) {
            isPaused = true;
            pauseSelectedIdx = 0;
            return;
        }

        if (key->code == sf::Keyboard::Key::M) {
            isPaused = true;
            pauseSelectedIdx = 3;
            return;
        }
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        sf::Vector2f clickPos = window.mapPixelToCoords(mb->position);

        // Right-Click: CANCEL / КЕНСЕЛИРАЙ
        if (mb->button == sf::Mouse::Button::Right) {
            bool was1 = (engine.getSelectedBuilding(1) != BuildingType::NONE);
            bool was2 = (engine.getSelectedBuilding(2) != BuildingType::NONE);
            engine.clearBuildingSelection(1);
            engine.clearBuildingSelection(2);
            if (was1) triggerPlayerPopup(1, "ОТКАЗ", "Отменен строеж", "Режимът за поставяне е прекратен.", "", sf::Color(180, 180, 180));
            if (was2) triggerPlayerPopup(2, "ОТКАЗ", "Отменен строеж", "Режимът за поставяне е прекратен.", "", sf::Color(180, 180, 180));
            return;
        }

        // Left-Click:
        // Click on ESC / MENU
        if (sf::FloatRect({ 1600.0f - 130.0f, 900.0f - 34.0f }, { 120.0f, 28.0f }).contains(clickPos)) {
            isPaused = true;
            pauseSelectedIdx = 0;
            return;
        }

        // Click on Fullscreen button
        if (sf::FloatRect({ 1600.0f - 275.0f, 900.0f - 34.0f }, { 135.0f, 28.0f }).contains(clickPos)) {
            requestFullscreenToggle = true;
            return;
        }

        // Click on Help button — auto-pause then show help on top
        if (sf::FloatRect({ 1600.0f - 405.0f, 900.0f - 34.0f }, { 120.0f, 28.0f }).contains(clickPos)) {
            isPaused = true;
            pauseSelectedIdx = 0;
            showHelpOverlay = true;
            return;
        }

        // 1. Building Menu Clicks
        BuildingType clickedP1 = p1Buildings.handleClick(clickPos);
        if (clickedP1 != BuildingType::NONE) {
            engine.getPlayerEconomyMut(1).selectedBuilding = static_cast<int>(clickedP1);
            BuildingCost c = engine.getBuildingCost(clickedP1);
            triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg,
                               "Нужно: " + std::to_string(c.woodCost) + " Дърво, " + std::to_string(c.oreCost) + " Руда.\nДобив: +" + std::to_string(c.basePowerMW) + " MW.",
                               "[КЛИК НА ЗЕМЯ]: Постави | [ДЕСЕН КЛИК]: Отказ", sf::Color(0, 229, 255));
            return;
        }

        if (!bot.isActive()) {
            BuildingType clickedP2 = p2Buildings.handleClick(clickPos);
            if (clickedP2 != BuildingType::NONE) {
                engine.getPlayerEconomyMut(2).selectedBuilding = static_cast<int>(clickedP2);
                BuildingCost c = engine.getBuildingCost(clickedP2);
                triggerPlayerPopup(2, "СТРОЕЖ", c.nameBg,
                                   "Нужно: " + std::to_string(c.woodCost) + " Дърво, " + std::to_string(c.oreCost) + " Руда.\nДобив: +" + std::to_string(c.basePowerMW) + " MW.",
                                   "[КЛИК НА ЗЕМЯ]: Постави | [ДЕСЕН КЛИК]: Отказ", sf::Color(255, 120, 200));
                return;
            }
        }

        // 2. Buy Land HUD button clicks
        if (resourceHUD.getP1BuyLandButton().contains(clickPos)) {
            std::string msg;
            if (engine.buyNextLandTier(1, msg)) {
                triggerPlayerPopup(1, "ЗЕМЯ", "Разширена земя!", msg, "[E]: Избери сграда за строеж", sf::Color(255, 215, 0));
            } else {
                triggerPlayerPopup(1, "ГРЕШКА", "Няма злато!", msg, "[Q]: Отказ", sf::Color(255, 90, 90));
            }
            return;
        }
        if (!bot.isActive() && resourceHUD.getP2BuyLandButton().contains(clickPos)) {
            std::string msg;
            if (engine.buyNextLandTier(2, msg)) {
                triggerPlayerPopup(2, "ЗЕМЯ", "Разширена земя!", msg, "[PgDn]: Избери сграда", sf::Color(255, 215, 0));
            } else {
                triggerPlayerPopup(2, "ГРЕШКА", "Няма злато!", msg, "[PgUp]: Отказ", sf::Color(255, 90, 90));
            }
            return;
        }

        // 3. Click on Land Plots directly (Buy Plot or Place Building on it)
        for (const auto& plot : engine.getLandPlots()) {
            if (plot.bounds.contains(clickPos)) {
                int owner = plot.playerOwner;
                if (owner == 2 && bot.isActive()) return; // Bot handles its own land plots
                if (!plot.isPurchased) {
                    std::string msg;
                    if (engine.buyLandPlot(owner, plot.id, msg)) {
                        triggerPlayerPopup(owner, "ЗЕМЯ", "Купихте парцел!", msg + "\nВече можете да строите тук.", "[КЛИК]: Постави сграда", sf::Color(255, 215, 0));
                    } else {
                        triggerPlayerModal(owner, "НЕДОСТИГ НА ЗЛАТО", "Не можете да купите парцела!", msg, "Продавайте ток на града за да печелите злато!", sf::Color(255, 180, 50));
                    }
                    return;
                } else {
                    BuildingType sel = engine.getSelectedBuilding(owner);
                    if (sel != BuildingType::NONE) {
                        sf::Vector2f targetPos = (sel == BuildingType::DEMOLISH) ? clickPos : engine.snapToBuildingGrid(owner, clickPos);
                        std::string msg;
                        if (engine.placeBuilding(owner, sel, targetPos, msg)) {
                            triggerPlayerPopup(owner, "УСПЕХ", "Действието е успешно!", msg, "", sf::Color(0, 255, 180));
                            if (sel != BuildingType::DEMOLISH) engine.clearBuildingSelection(owner);
                        } else {
                            triggerPlayerModal(owner, "ГРЕШКА ПРИ СТРОЕЖ", "Строежът е невъзможен!", msg,
                                               (!engine.isDaylight() ? "Поставете и захранете Осветителна лампа за работа нощем!" : "Проверете вашите ресурси и парцели!"), sf::Color(255, 75, 75));
                        }
                        return;
                    }
                }
            }
        }

        // 4. Click on Upgrade Button of Resource Stations
        ResourceType p1Up = nodes.getP1UpgradeAt(clickPos);
        if (p1Up != ResourceType::NONE) {
            std::string msg;
            if (engine.upgradeMine(1, p1Up, msg)) {
                spawnMiningParticles(clickPos, sf::Color(255, 215, 0), 25);
                triggerPlayerPopup(1, "НАДГРАЖДАНЕ", msg, "Добивът от тази мина е увеличен с +75%!", "[SPACE]: Добив | [F]: Нов ъпгрейд", sf::Color(255, 215, 0));
                spawnNotice(msg, clickPos + sf::Vector2f(0.0f, -25.0f), sf::Color(255, 215, 0));
            } else {
                triggerPlayerPopup(1, "ГРЕШКА", msg, "Печелете злато от доставка на ток към града!", "", sf::Color(255, 90, 90));
                spawnNotice(msg, clickPos + sf::Vector2f(0.0f, -25.0f), sf::Color(255, 90, 90));
            }
            return;
        }

        if (!bot.isActive()) {
            ResourceType p2Up = nodes.getP2UpgradeAt(clickPos);
            if (p2Up != ResourceType::NONE) {
                std::string msg;
                if (engine.upgradeMine(2, p2Up, msg)) {
                    spawnMiningParticles(clickPos, sf::Color(255, 215, 0), 25);
                    triggerPlayerPopup(2, "НАДГРАЖДАНЕ", msg, "Добивът от тази мина е увеличен с +75%!", "[ENTER]: Добив | [RShift]: Нов ъпгрейд", sf::Color(255, 215, 0));
                    spawnNotice(msg, clickPos + sf::Vector2f(0.0f, -25.0f), sf::Color(255, 215, 0));
                } else {
                    triggerPlayerPopup(2, "ГРЕШКА", msg, "Печелете злато от доставка на ток към града!", "", sf::Color(255, 90, 90));
                    spawnNotice(msg, clickPos + sf::Vector2f(0.0f, -25.0f), sf::Color(255, 90, 90));
                }
                return;
            }
        }

        // 5. Click on Resource Stations
        ResourceType p1Res = nodes.getP1ResourceAt(clickPos);
        if (p1Res != ResourceType::NONE) {
            if (p1ResourceCooldown > 0.0f) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "ИЗЧАКАЙТЕ: %.1fs", p1ResourceCooldown);
                spawnNotice(buf, clickPos + sf::Vector2f(0.0f, -25.0f), sf::Color(255, 180, 50));
                return;
            }
            GameEngine::MineResult res;
            std::string msg;
            if (engine.mineResource(1, p1Res, res, msg)) {
                p1ResourceCooldown = 1.0f;
                p1Pulse = 1.0f;
                const auto* st = nodes.getStation(1, p1Res);
                sf::Color c = st ? st->themeColor : sf::Color(0, 229, 255);
                spawnMiningParticles(clickPos, c, 18);
                triggerPlayerPopup(1, "ДОБИВ", msg, "Ресурсът е добавен в склада.", "[E]: Избери сграда", c);
                spawnNotice(msg, clickPos + sf::Vector2f(0.0f, -25.0f), c);
            }
            return;
        }

        if (!bot.isActive()) {
            ResourceType p2Res = nodes.getP2ResourceAt(clickPos);
            if (p2Res != ResourceType::NONE) {
                if (p2ResourceCooldown > 0.0f) {
                    char buf[32];
                    std::snprintf(buf, sizeof(buf), "ИЗЧАКАЙТЕ: %.1fs", p2ResourceCooldown);
                    spawnNotice(buf, clickPos + sf::Vector2f(0.0f, -25.0f), sf::Color(255, 180, 50));
                    return;
                }
                GameEngine::MineResult res;
                std::string msg;
                if (engine.mineResource(2, p2Res, res, msg)) {
                    p2ResourceCooldown = 1.0f;
                    p2Pulse = 1.0f;
                    const auto* st = nodes.getStation(2, p2Res);
                    sf::Color c = st ? st->themeColor : sf::Color(255, 140, 210);
                    spawnMiningParticles(clickPos, c, 18);
                    triggerPlayerPopup(2, "ДОБИВ", msg, "Ресурсът е добавен в склада.", "[PgDn]: Избери сграда", c);
                    spawnNotice(msg, clickPos + sf::Vector2f(0.0f, -25.0f), c);
                }
                return;
            }
        }
    }
}
