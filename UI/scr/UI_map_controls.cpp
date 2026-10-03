#include "../includes/UI_map.h"
#include <algorithm>
#include <cstdio>
#include <string>

// =============================================================================
// UI_map Controls & Input Handling (Cursors, Actions, Hotkeys, Events)
// =============================================================================

// Real resource list of a building recipe (used by keyboard and mouse selection popups)
static std::string formatCost(const BuildingCost& c) {
    std::string s = "Нужно: " + std::to_string(c.woodCost) + " Дърво";
    if (c.ironCost > 0) s += ", " + std::to_string(c.ironCost) + " Жел";
    if (c.copperCost > 0) s += ", " + std::to_string(c.copperCost) + " Мед";
    if (c.siliconCost > 0) s += ", " + std::to_string(c.siliconCost) + " Сил";
    if (c.coalCost > 0) s += ", " + std::to_string(c.coalCost) + " Въгл";
    if (c.silverCost > 0) s += ", " + std::to_string(c.silverCost) + " Среб";
    return s;
}

// -----------------------------------------------------------------------------
// Input ownership & resync helpers
// -----------------------------------------------------------------------------

// Which player the mouse acts for. Single Player: the human (P1). Co-op: only the player whose
// scheme includes the mouse; in the shared-mouse scheme, the player whose half the pointer is in.
int UI_map::mouseOwnerAt(sf::Vector2f pos) const {
    if (bot.isActive()) return 1;
    switch (controlScheme) {
        case ControlScheme::P1_MOUSE_P2_KEYBOARD: return 1;
        case ControlScheme::P1_KEYBOARD_P2_MOUSE: return 2;
        case ControlScheme::BOTH_MOUSE:           return (pos.x < 800.0f) ? 1 : 2;
        case ControlScheme::BOTH_KEYBOARD:
        default:                                  return 0; // Keyboard-only co-op: the mouse drives no player
    }
}

// A modal is dismissed only by its own player's confirm/cancel keys
bool UI_map::isModalDismissKey(int player, sf::Keyboard::Key code) const {
    if (player == 1) {
        if (code == sf::Keyboard::Key::Space || code == sf::Keyboard::Key::X) return true;
        // Single Player: P1 also owns Enter/Delete/Backspace, and Esc is unambiguous
        return bot.isActive() && (code == sf::Keyboard::Key::Enter || code == sf::Keyboard::Key::Delete ||
                                  code == sf::Keyboard::Key::Backspace || code == sf::Keyboard::Key::Escape);
    }
    return code == sf::Keyboard::Key::Enter || code == sf::Keyboard::Key::Delete;
}

// Mark every polled one-shot key as "already down": a key that a menu, dialog, pause screen or
// fullscreen toggle just consumed must be released before it counts as a new in-game press.
void UI_map::primeInputEdges(int player) {
    if (player != 2) {
        p1PrevAction = true;
        p1PrevUpgrade = true;
        p1PrevE = true;
        p1PrevQ = true;
        p1PrevX = true;
        for (int k = 1; k <= 6; ++k) p1PrevNum[k] = true;
    }
    if (player != 1) {
        p2PrevAction = true;
        p2PrevUpgrade = true;
        p2PrevPgDn = true;
        p2PrevPgUp = true;
        p2PrevDel = true;
    }
}

void UI_map::resetMatchInputState() {
    tutorialBotHoldLeft = TUTORIAL_BOT_HOLD_SEC;
    helpOpenedFromPause = false;
    primeInputEdges(0);
}

void UI_map::onFocusLost() {
    if (engine.getCityState().winner != 0) return;
    if (!isPaused) {
        isPaused = true;
        pauseSelectedIdx = 0;
    }
    helpOpenedFromPause = true; // If help is open, closing it lands on the pause menu
}

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
                p1ResourceCooldown = Balance::MINE_COOLDOWN_SEC;
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
                p2ResourceCooldown = Balance::MINE_COOLDOWN_SEC;
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
                            triggerPlayerPopup(2, "ГРЕШКА", "Няма злато!", msg, "Продавайте ток на града за злато!", sf::Color(255, 90, 90));
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
        return;
    }

    // Keyboard & mouse state is global: never read it while another window has the focus
    if (!window.hasFocus()) {
        engine.setTimeScale(1.0f);
        primeInputEdges(0);
        return;
    }

    float speed = 360.0f;
    sf::Vector2f mPos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    // The pointer only drives mouse-scheme cursors while it is over the game canvas
    const bool mouseOnCanvas = (mPos.x >= 0.0f && mPos.x < VIRTUAL_WIDTH && mPos.y >= 0.0f && mPos.y < VIRTUAL_HEIGHT);

    // 1. Update floating notices & mining particles
    for (auto it = notices.begin(); it != notices.end();) {
        it->timer -= dt;
        it->pos.y -= 40.0f * dt;
        if (it->timer <= 0.0f) it = notices.erase(it);
        else ++it;
    }
    updateMiningParticles(dt);

    // 2. Mining speed-up: game time runs faster only while EVERY human player's cursor is in a
    //    resource zone (the bot never triggers it, and one player cannot speed up the other's day)
    bool p1InRes = (nodes.getP1ResourceAt(p1Pos) != ResourceType::NONE);
    bool p2InRes = (nodes.getP2ResourceAt(p2Pos) != ResourceType::NONE);
    bool allHumansInRes = bot.isActive() ? p1InRes : (p1InRes && p2InRes);
    engine.setTimeScale(allHumansInRes ? Balance::MINE_SPEEDUP_MULT : 1.0f);

    // Decrement grid step cooldowns
    if (p1GridStepCooldown > 0.0f) p1GridStepCooldown -= dt;
    if (p2GridStepCooldown > 0.0f) p2GridStepCooldown -= dt;

    // 3. Player 1 Movement (Precision Grid during placement, smooth analog otherwise)
    bool p1BuildingMode = (engine.getSelectedBuilding(1) != BuildingType::NONE);
    bool allowArrowsForP1 = bot.isActive(); // In Single Player, player can use WASD OR Arrow keys!

    if (p1BuildingMode) {
        if (controlScheme == ControlScheme::BOTH_KEYBOARD || controlScheme == ControlScheme::P1_KEYBOARD_P2_MOUSE || allowArrowsForP1) {
            if (p1GridStepCooldown <= 0.0f) {
                bool moved = false;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) || (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))) {
                    p1GridRow = std::max(0, p1GridRow - 1);
                    moved = true;
                } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) || (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))) {
                    p1GridRow = std::min(11, p1GridRow + 1);
                    moved = true;
                }
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))) {
                    p1GridCol = std::max(0, p1GridCol - 1);
                    moved = true;
                } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) || (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))) {
                    p1GridCol = std::min(8, p1GridCol + 1);
                    moved = true;
                }
                if (moved) {
                    p1Pos = engine.getGridSlot(1, p1GridCol, p1GridRow);
                    p1GridStepCooldown = 0.14f;
                }
            }
        } else {
            // Mouse-driven P1 (shared-mouse scheme: only while the pointer is in the west half)
            if (mouseOnCanvas && (controlScheme != ControlScheme::BOTH_MOUSE || mPos.x < 800.0f)) {
                engine.getClosestGridIndex(1, mPos, p1GridCol, p1GridRow);
                p1Pos = engine.getGridSlot(1, p1GridCol, p1GridRow);
            }
        }
    } else {
        if (controlScheme == ControlScheme::BOTH_KEYBOARD || controlScheme == ControlScheme::P1_KEYBOARD_P2_MOUSE || allowArrowsForP1) {
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) || (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))) p1Pos.y -= speed * dt;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) || (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))) p1Pos.y += speed * dt;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))) p1Pos.x -= speed * dt;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) || (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))) p1Pos.x += speed * dt;
        } else if (controlScheme == ControlScheme::P1_MOUSE_P2_KEYBOARD) {
            if (mouseOnCanvas) p1Pos = mPos;
        } else if (controlScheme == ControlScheme::BOTH_MOUSE) {
            if (mouseOnCanvas && mPos.x < 800.0f) p1Pos = mPos;
        }
        p1Pos.x = std::max(30.0f, std::min(p1Pos.x, 780.0f));
        p1Pos.y = std::max(40.0f, std::min(p1Pos.y, 860.0f));
        engine.getClosestGridIndex(1, p1Pos, p1GridCol, p1GridRow);
    }

    // 4. Player 2 Movement (Bot AI or Human Input)
    if (bot.isActive()) {
        p2Modal.active = false; // Never block Player 2 bot with a modal dialog

        // An active tutorial gives the human a head start, but only for a limited window:
        // skipping or completing it, or running out of time, releases the bot.
        bool tutorialHoldsBot = tutorial.isActive() && tutorial.getStep() != TutorialStep::COMPLETED &&
                                tutorialBotHoldLeft > 0.0f;
        if (tutorialHoldsBot) {
            tutorialBotHoldLeft -= dt;
        } else {
            bool botTriggerAction = false;
            bool botTriggerUpgrade = false;
            BuildingType botSel = BuildingType::NONE;
            bot.update(dt, engine, nodes, p2Pos, botTriggerAction, botTriggerUpgrade, botSel);

            // Always sync selectedBuilding with bot's desired building state (clearing when NONE)
            engine.getPlayerEconomyMut(2).selectedBuilding = static_cast<int>(botSel);

            if (botTriggerAction && p2ActionCooldown <= 0.0f && !showHelpOverlay) {
                executeP2Action();
                p2ActionCooldown = 0.15f;
            }

            if (botTriggerUpgrade && !showHelpOverlay) {
                executeP2Upgrade();
            }
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
                // Mouse-driven P2 (shared-mouse scheme: only while the pointer is in the east half)
                if (mouseOnCanvas && (controlScheme != ControlScheme::BOTH_MOUSE || mPos.x >= 800.0f)) {
                    engine.getClosestGridIndex(2, mPos, p2GridCol, p2GridRow);
                    p2Pos = engine.getGridSlot(2, p2GridCol, p2GridRow);
                }
            }
        } else {
            if (controlScheme == ControlScheme::BOTH_KEYBOARD || controlScheme == ControlScheme::P1_MOUSE_P2_KEYBOARD) {
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up)) p2Pos.y -= speed * dt;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down)) p2Pos.y += speed * dt;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)) p2Pos.x -= speed * dt;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) p2Pos.x += speed * dt;
            } else if (controlScheme == ControlScheme::P1_KEYBOARD_P2_MOUSE) {
                if (mouseOnCanvas) p2Pos = mPos;
            } else if (controlScheme == ControlScheme::BOTH_MOUSE) {
                if (mouseOnCanvas && mPos.x >= 800.0f) p2Pos = mPos;
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
    bool p1PressingAction = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space) ||
                            (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter));
    bool p1JustPressed = p1PressingAction && !p1PrevAction;
    p1PrevAction = p1PressingAction;

    if (p1JustPressed && p1ActionCooldown <= 0.0f && !p1Modal.active && !showHelpOverlay) {
        executeP1Action();
        p1ActionCooldown = 0.20f;
    }

    // P1 Upgrade Mine with Gold: [F] or [RShift / End in Single Player]
    bool p1PressingUpgrade = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F) ||
                             (allowArrowsForP1 && (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::End)));
    if (p1PressingUpgrade && !p1PrevUpgrade && !p1Modal.active && !showHelpOverlay) {
        executeP1Upgrade();
    }
    p1PrevUpgrade = p1PressingUpgrade;

    // Key names shown in P1's popups: Enter/PgDn/Del belong to P1 only in Single Player
    const std::string p1ActKeys = allowArrowsForP1 ? "[SPACE/ENTER]" : "[SPACE]";
    const std::string p1NextKeys = allowArrowsForP1 ? "[E/PgDn]" : "[E]";
    const std::string p1CancelKeys = allowArrowsForP1 ? "[X/Del]" : "[X]";

    // P1: [E] Cycle Forward (or PgDn in Single Player)
    bool curE = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E) ||
                (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::PageDown));
    if (curE && !p1PrevE && p1SelectCooldown <= 0.0f && !p1Modal.active && !showHelpOverlay) {
        engine.cycleBuildingSelection(1);
        p1SelectCooldown = 0.16f;
        BuildingType newSel = engine.getSelectedBuilding(1);
        BuildingCost c = engine.getBuildingCost(newSel);
        p1Pos = engine.getGridSlot(1, p1GridCol, p1GridRow);
        if (newSel == BuildingType::DEMOLISH) {
            triggerPlayerPopup(1, "ПРЕМАХВАНЕ", c.nameBg, "Кликнете върху ваша сграда за разрушаване.\nВръща 50% от ресурсите.", p1ActKeys + ": Премахни | " + p1NextKeys + ": Следваща | " + p1CancelKeys + ": Отказ", sf::Color(255, 80, 80));
        } else if (newSel == BuildingType::LAMP) {
            triggerPlayerPopup(1, "ОСВЕТЛЕНИЕ", c.nameBg, formatCost(c) + ".\nОсветява нощем за строителство.", p1ActKeys + ": Постави | " + p1NextKeys + ": Следваща | " + p1CancelKeys + ": Отказ", sf::Color(255, 220, 100));
        } else {
            triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg,
                               formatCost(c) + ".\nДобив: +" + std::to_string(c.basePowerMW) + " MW ток.",
                               p1ActKeys + ": Постави в грида | " + p1NextKeys + ": Следваща | " + p1CancelKeys + ": Отказ", sf::Color(0, 229, 255));
        }
    }
    p1PrevE = curE;

    // P1: [Q] Cycle Backward / Cancel (or PgUp in Single Player)
    bool curQ = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q) ||
                (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::PageUp));
    if (curQ && !p1PrevQ && p1SelectCooldown <= 0.0f && !p1Modal.active && !showHelpOverlay) {
        if (engine.getSelectedBuilding(1) != BuildingType::NONE) {
            engine.cycleBuildingSelectionPrev(1);
            p1SelectCooldown = 0.16f;
            BuildingType newSel = engine.getSelectedBuilding(1);
            BuildingCost c = engine.getBuildingCost(newSel);
            p1Pos = engine.getGridSlot(1, p1GridCol, p1GridRow);
            if (newSel == BuildingType::DEMOLISH) {
                triggerPlayerPopup(1, "ПРЕМАХВАНЕ", c.nameBg, "Кликнете върху ваша сграда за разрушаване.\nВръща 50% от ресурсите.", p1ActKeys + ": Премахни | " + p1NextKeys + ": Следваща | " + p1CancelKeys + ": Отказ", sf::Color(255, 80, 80));
            } else if (newSel == BuildingType::LAMP) {
                triggerPlayerPopup(1, "ОСВЕТЛЕНИЕ", c.nameBg, formatCost(c) + ".\nОсветява нощем за строителство.", p1ActKeys + ": Постави | " + p1NextKeys + ": Следваща | " + p1CancelKeys + ": Отказ", sf::Color(255, 220, 100));
            } else {
                triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg,
                                   formatCost(c) + ".\nДобив: +" + std::to_string(c.basePowerMW) + " MW ток.",
                                   p1ActKeys + ": Постави в грида | " + p1NextKeys + ": Следваща | " + p1CancelKeys + ": Отказ", sf::Color(0, 229, 255));
            }
        }
    }
    p1PrevQ = curQ;

    bool curX = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::X) ||
                (allowArrowsForP1 && (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Delete) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Backspace)));
    if (curX && !p1PrevX && !p1Modal.active && !showHelpOverlay) {
        if (engine.getSelectedBuilding(1) != BuildingType::NONE) {
            engine.clearBuildingSelection(1);
            triggerPlayerPopup(1, "ОТКАЗ", "Изборът е прекратен", "Свободен режим.", p1NextKeys + ": Избери сграда", sf::Color(180, 180, 180));
        } else {
            engine.getPlayerEconomyMut(1).selectedBuilding = static_cast<int>(BuildingType::DEMOLISH);
            p1Pos = engine.getGridSlot(1, p1GridCol, p1GridRow);
            triggerPlayerPopup(1, "ПРЕМАХВАНЕ", "Режим Разрушаване", "Посочете сградата, която искате да махнете.", p1ActKeys + ": Премахни | " + p1CancelKeys + ": Отказ", sf::Color(255, 80, 80));
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

        // P2: [PgUp] Cycle Backward (like P1's [Q]: only while a building is selected)
        bool curPgUp = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::PageUp);
        if (curPgUp && !p2PrevPgUp && p2SelectCooldown <= 0.0f && !p2Modal.active && !showHelpOverlay &&
            engine.getSelectedBuilding(2) != BuildingType::NONE) {
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

        // P2: [Del] Cancel / Demolish mode ([End] is P2's upgrade key only)
        bool curDel = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Delete);
        if (curDel && !p2PrevDel && !p2Modal.active && !showHelpOverlay) {
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
    // team info: developer overlay keys work on every in-match screen ([F3], and F6/F7/F8 while it is open)
    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        if (key->code == sf::Keyboard::Key::F3) {
            devOverlay.toggle();
            return;
        }
        if (devOverlay.handleKey(key->code)) return;
    }

    // -------------------------------------------------------------------------
    // -1. If Victory Screen is active, handle Restart [R], Menu [ESC/M], or button clicks
    // -------------------------------------------------------------------------
    if (engine.getCityState().winner != 0) {
        if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
            if (postMatch.handleKey(key->code)) return; // team info: report tabs (1/2/3, arrows)
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
            if (postMatch.handleClick(clickPos)) return; // team info: report tab headers
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
    //    Closing returns to the pause menu only if help was opened from it.
    // -------------------------------------------------------------------------
    if (showHelpOverlay) {
        auto closeHelp = [this]() {
            showHelpOverlay = false;
            isPaused = helpOpenedFromPause;
            primeInputEdges(0); // The Enter/Space that closed help must not act in-game
        };
        if (event.is<sf::Event::MouseButtonPressed>()) {
            closeHelp();
            return;
        }
        if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
            if (key->code == sf::Keyboard::Key::H || key->code == sf::Keyboard::Key::F1 ||
                key->code == sf::Keyboard::Key::Escape || key->code == sf::Keyboard::Key::Enter ||
                key->code == sf::Keyboard::Key::Space) {
                closeHelp();
                return;
            }
        }
        return; // swallow all other input while help is open
    }

    // team info: the event log overlay (opened from the pause menu) takes all input until closed
    if (showEventLog) {
        handleEventLogInput(event);
        return;
    }

    // -------------------------------------------------------------------------
    // 1. Pause Menu Event Handling (before the tutorial and the dialogs, so they
    //    never receive keys or clicks meant for the pause menu)
    // -------------------------------------------------------------------------
    if (isPaused) {
        if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
            if (key->code == sf::Keyboard::Key::Escape) {
                isPaused = false;
                primeInputEdges(0);
                return;
            }
            // team info: 5 entries (index 3 = event log, 4 = main menu)
            if (key->code == sf::Keyboard::Key::Up || key->code == sf::Keyboard::Key::W) {
                pauseSelectedIdx = (pauseSelectedIdx + 4) % 5;
                return;
            }
            if (key->code == sf::Keyboard::Key::Down || key->code == sf::Keyboard::Key::S) {
                pauseSelectedIdx = (pauseSelectedIdx + 1) % 5;
                return;
            }
            if (key->code == sf::Keyboard::Key::Enter || key->code == sf::Keyboard::Key::Space) {
                if (pauseSelectedIdx == 0) {
                    isPaused = false;
                    primeInputEdges(0);
                } else if (pauseSelectedIdx == 1) {
                    isPaused = false;
                    restartMatch();
                } else if (pauseSelectedIdx == 2) {
                    helpOpenedFromPause = true;
                    showHelpOverlay = true;
                } else if (pauseSelectedIdx == 3) {
                    openEventLog();
                } else if (pauseSelectedIdx == 4) {
                    isPaused = false;
                    requestMenu = true;
                }
                return;
            }
            if (key->code == sf::Keyboard::Key::L) { // team info
                openEventLog();
                return;
            }
            if (key->code == sf::Keyboard::Key::R) {
                isPaused = false;
                restartMatch();
                return;
            }
            if (key->code == sf::Keyboard::Key::H || key->code == sf::Keyboard::Key::F1) {
                helpOpenedFromPause = true;
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
                primeInputEdges(0);
                return;
            }
            if (pauseRestartBtn.contains(clickPos)) {
                isPaused = false;
                restartMatch();
                return;
            }
            if (pauseHelpBtn.contains(clickPos)) {
                helpOpenedFromPause = true;
                showHelpOverlay = true;
                return;
            }
            if (pauseLogBtn.contains(clickPos)) { // team info
                openEventLog();
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

    // -------------------------------------------------------------------------
    // 2. Player modal dialogs: each one closes only with its own player's
    //    confirm/cancel keys (mouse: that player's click on its OK button, below)
    // -------------------------------------------------------------------------
    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        if (p1Modal.active && isModalDismissKey(1, key->code)) {
            closePlayerModal(1);
            return;
        }
        if (p2Modal.active && isModalDismissKey(2, key->code)) {
            closePlayerModal(2);
            return;
        }
    }

    // -------------------------------------------------------------------------
    // 3. Interactive Tutorial Clicks & Keypresses
    // -------------------------------------------------------------------------
    if (tutorial.isActive()) {
        if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
            sf::Vector2f clickPos = window.mapPixelToCoords(mb->position);
            if (tutorial.handleClick(clickPos)) {
                return;
            }
        }
        if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
            if (key->code == sf::Keyboard::Key::Escape) {
                tutorial.skip();
                return;
            }
            if (tutorial.handleKey(key->code)) {
                primeInputEdges(0); // The Space/Enter the tutorial consumed must not act in-game
                return;
            }
        }
    }

    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        if (key->code == sf::Keyboard::Key::F11) {
            requestFullscreenToggle = true;
            return;
        }
    }

    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        if (key->code == sf::Keyboard::Key::H || key->code == sf::Keyboard::Key::F1) {
            // Auto-pause then open help on top; closing help resumes the game
            isPaused = true;
            pauseSelectedIdx = 0;
            helpOpenedFromPause = false;
            showHelpOverlay = true;
            return;
        }

        if (key->code == sf::Keyboard::Key::Escape) {
            isPaused = true;
            pauseSelectedIdx = 0;
            return;
        }

        if (key->code == sf::Keyboard::Key::M) {
            // Open on "ПРОДЪЛЖИ": a following Space/Enter must not abandon the match
            isPaused = true;
            pauseSelectedIdx = 0;
            return;
        }
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        sf::Vector2f clickPos = window.mapPixelToCoords(mb->position);
        // The mouse acts only for the player who owns it in the chosen control scheme
        const int owner = mouseOwnerAt(clickPos);
        PlayerModalDialog* ownerModal = (owner == 1) ? &p1Modal : (owner == 2 ? &p2Modal : nullptr);

        // Right-Click: CANCEL / КЕНСЕЛИРАЙ (only the mouse owner's own selection)
        if (mb->button == sf::Mouse::Button::Right) {
            if (owner != 0 && !ownerModal->active && engine.getSelectedBuilding(owner) != BuildingType::NONE) {
                engine.clearBuildingSelection(owner);
                triggerPlayerPopup(owner, "ОТКАЗ", "Отменен строеж", "Режимът за поставяне е прекратен.", "", sf::Color(180, 180, 180));
            }
            return;
        }
        if (mb->button != sf::Mouse::Button::Left) return; // Middle / extra buttons do nothing

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

        // Click on Help button — auto-pause then show help on top; closing help resumes the game
        if (sf::FloatRect({ 1600.0f - 405.0f, 900.0f - 34.0f }, { 120.0f, 28.0f }).contains(clickPos)) {
            isPaused = true;
            pauseSelectedIdx = 0;
            helpOpenedFromPause = false;
            showHelpOverlay = true;
            return;
        }

        if (owner == 0) return; // Keyboard-only scheme: the mouse plays for nobody

        // The owner's open modal blocks their other clicks; its OK button closes it
        if (ownerModal->active) {
            if (ownerModal->okBtn.contains(clickPos)) {
                closePlayerModal(owner);
            }
            return;
        }

        // 1. Building Menu Clicks
        if (owner == 1) {
            BuildingType clickedP1 = p1Buildings.handleClick(clickPos);
            if (clickedP1 != BuildingType::NONE) {
                engine.getPlayerEconomyMut(1).selectedBuilding = static_cast<int>(clickedP1);
                p1Pos = engine.getGridSlot(1, p1GridCol, p1GridRow); // Snap like the keyboard paths so the action matches the ghost
                BuildingCost c = engine.getBuildingCost(clickedP1);
                triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg,
                                   (clickedP1 == BuildingType::DEMOLISH ? std::string("Посочете ваша сграда за разрушаване.")
                                                                        : formatCost(c) + ".\nДобив: +" + std::to_string(c.basePowerMW) + " MW."),
                                   "[КЛИК НА ЗЕМЯ]: Постави | [ДЕСЕН КЛИК]: Отказ", sf::Color(0, 229, 255));
                return;
            }
        } else {
            BuildingType clickedP2 = p2Buildings.handleClick(clickPos);
            if (clickedP2 != BuildingType::NONE) {
                engine.getPlayerEconomyMut(2).selectedBuilding = static_cast<int>(clickedP2);
                p2Pos = engine.getGridSlot(2, p2GridCol, p2GridRow); // Snap like the keyboard paths so the action matches the ghost
                BuildingCost c = engine.getBuildingCost(clickedP2);
                triggerPlayerPopup(2, "СТРОЕЖ", c.nameBg,
                                   (clickedP2 == BuildingType::DEMOLISH ? std::string("Посочете ваша сграда за разрушаване.")
                                                                        : formatCost(c) + ".\nДобив: +" + std::to_string(c.basePowerMW) + " MW."),
                                   "[КЛИК НА ЗЕМЯ]: Постави | [ДЕСЕН КЛИК]: Отказ", sf::Color(255, 120, 200));
                return;
            }
        }

        // 2. Buy Land HUD button clicks
        if (owner == 1 && resourceHUD.getP1BuyLandButton().contains(clickPos)) {
            std::string msg;
            if (engine.buyNextLandTier(1, msg)) {
                triggerPlayerPopup(1, "ЗЕМЯ", "Разширена земя!", msg, "[E]: Избери сграда за строеж", sf::Color(255, 215, 0));
            } else {
                triggerPlayerPopup(1, "ГРЕШКА", "Няма злато!", msg, "Продавайте ток на града за злато!", sf::Color(255, 90, 90));
            }
            return;
        }
        if (owner == 2 && resourceHUD.getP2BuyLandButton().contains(clickPos)) {
            std::string msg;
            if (engine.buyNextLandTier(2, msg)) {
                triggerPlayerPopup(2, "ЗЕМЯ", "Разширена земя!", msg, "[PgDn]: Избери сграда", sf::Color(255, 215, 0));
            } else {
                triggerPlayerPopup(2, "ГРЕШКА", "Няма злато!", msg, "Продавайте ток на града за злато!", sf::Color(255, 90, 90));
            }
            return;
        }

        // 3. Click on the owner's Land Plots directly (Buy Plot or Place Building on it)
        for (const auto& plot : engine.getLandPlots()) {
            if (plot.playerOwner == owner && plot.bounds.contains(clickPos)) {
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
        if (owner == 1) {
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
        } else {
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
        if (owner == 1) {
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
                    p1ResourceCooldown = Balance::MINE_COOLDOWN_SEC;
                    p1Pulse = 1.0f;
                    const auto* st = nodes.getStation(1, p1Res);
                    sf::Color c = st ? st->themeColor : sf::Color(0, 229, 255);
                    spawnMiningParticles(clickPos, c, 18);
                    triggerPlayerPopup(1, "ДОБИВ", msg, "Ресурсът е добавен в склада.", "[E]: Избери сграда", c);
                    spawnNotice(msg, clickPos + sf::Vector2f(0.0f, -25.0f), c);
                }
                return;
            }
        } else {
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
                    p2ResourceCooldown = Balance::MINE_COOLDOWN_SEC;
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
