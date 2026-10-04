#include "../includes/UI_map.h"
#include "../includes/UI_controlsConfig.h"
#include "../includes/UI_text.h"
#include "../includes/UI_shot.h"
#include "../includes/UI_theme.h"
#include <algorithm>
#include <cstdio>
#include <string>

// =============================================================================
// UI_map Controls & Input Handling (Cursors, Actions, Hotkeys, Events)
// =============================================================================

// Real resource list of a building recipe (used by keyboard and mouse selection popups)
static std::string formatCost(const BuildingCost& c) {
    return "Нужно: " + recipeText(c);
}

// -----------------------------------------------------------------------------
// Input ownership & resync helpers
// -----------------------------------------------------------------------------

// Which player the mouse acts for. Single Player: the human (P1). Co-op: only the player whose
// scheme includes the mouse; in the shared-mouse scheme, the player whose half the pointer is in.
int UI_map::mouseOwnerAt(sf::Vector2f pos) const {
#if defined(__ANDROID__)
    if (bot.isActive() || controlScheme != ControlScheme::BOTH_MOUSE) return 1;
#endif
    if (bot.isActive()) return 1;
    switch (controlScheme) {
        case ControlScheme::P1_MOUSE_P2_KEYBOARD: return 1;
        case ControlScheme::P1_KEYBOARD_P2_MOUSE: return 2;
        case ControlScheme::BOTH_MOUSE:           return (pos.x < 800.0f) ? 1 : 2;
        case ControlScheme::DEVHUB_ARCADE:
        case ControlScheme::BOTH_KEYBOARD:
        default:                                  return 0; // Arcade & Keyboard-only co-op: the mouse drives no player
    }
}

// A modal is dismissed only by its own player's confirm/cancel keys
bool UI_map::isModalDismissKey(int player, sf::Keyboard::Key code) const {
    const auto& b = UI_controlsConfig::get().getPlayer(player);
    if (code == b.action || code == b.cancel) return true;
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
        for (int k = 1; k <= 6; ++k) p2PrevNum[k] = true;
    }
}

void UI_map::resetMatchInputState() {
    tutorialBotHoldLeft = TUTORIAL_BOT_HOLD_SEC;
    helpOpenedFromPause = false;
    p1WasInResourceArea = false;
    p2WasInResourceArea = false;
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

// Name tag above a player cursor, on a dark pill so it stays readable over the map labels
static void drawCursorTag(sf::RenderWindow& window, sf::Text& tag, sf::Vector2f cursorPos, sf::Color accent) {
    sf::FloatRect tb = tag.getLocalBounds();
    sf::Vector2f pillSize(tb.size.x + 10.0f, tb.size.y + 8.0f);
    sf::Vector2f pillPos(cursorPos.x - pillSize.x / 2.0f, cursorPos.y - 26.0f - pillSize.y);
    pillPos.x = std::max(2.0f, std::min(pillPos.x, VIRTUAL_WIDTH - pillSize.x - 2.0f));
    pillPos.y = std::max(2.0f, pillPos.y);
    sf::RectangleShape pill(pillSize);
    pill.setPosition(pillPos);
    pill.setFillColor(theme::withAlpha(theme::Window, 215));
    pill.setOutlineThickness(1.0f);
    pill.setOutlineColor(theme::withAlpha(accent, 170));
    window.draw(pill);
    const sf::FloatRect pillRect(pillPos, pillSize);
    ui::lint::occlude(pillRect);
    tag.setPosition({ pillPos.x + 5.0f - tb.position.x, pillPos.y + 4.0f - tb.position.y });
    ui::drawText(window, tag, pillRect);
}

void UI_map::drawPlayerCursors(sf::RenderWindow& window) {
    float animTime = ui::shot::clockSeconds(animClock.getElapsedTime().asSeconds());

    // -------------------------------------------------------------------------
    // PLAYER 1 CURSOR (WEST SECTOR - CYAN)
    // -------------------------------------------------------------------------
    if (p1Pulse > 0.0f) {
        float radius = 24.0f + (1.0f - p1Pulse) * 60.0f;
        sf::CircleShape pulseCircle(radius);
        pulseCircle.setOrigin({ radius, radius });
        pulseCircle.setPosition(p1Pos);
        std::uint8_t alpha = static_cast<std::uint8_t>(p1Pulse * 220);
        pulseCircle.setFillColor(theme::withAlpha(theme::P1, alpha / 4));
        pulseCircle.setOutlineThickness(2.0f);
        pulseCircle.setOutlineColor(theme::withAlpha(theme::P1Light, alpha));
        window.draw(pulseCircle);
    }

    sf::CircleShape p1Core(8.0f);
    p1Core.setOrigin({ 8.0f, 8.0f });
    p1Core.setPosition(p1Pos);
    p1Core.setFillColor(theme::withAlpha(theme::P1, 220));
    p1Core.setOutlineThickness(2.0f);
    p1Core.setOutlineColor(sf::Color::White);
    window.draw(p1Core);

    float rot1 = animTime * 90.0f;
    for (int i = 0; i < 4; i++) {
        sf::RectangleShape bracket({ 14.0f, 2.5f });
        bracket.setOrigin({ 20.0f, 1.25f });
        bracket.setPosition(p1Pos);
        bracket.setRotation(sf::degrees(rot1 + i * 90.0f));
        bracket.setFillColor(theme::P1Light);
        window.draw(bracket);
    }

    if (resourcesLoaded) {
        sf::Text& p1Tag = ui::pooledText(font, "P1", fontsize::Label);
        p1Tag.setStyle(sf::Text::Bold);
        p1Tag.setFillColor(theme::P1Light);
        drawCursorTag(window, p1Tag, p1Pos, theme::P1);
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
        pulseCircle.setFillColor(theme::withAlpha(theme::P2, alpha / 4));
        pulseCircle.setOutlineThickness(2.0f);
        pulseCircle.setOutlineColor(theme::withAlpha(theme::P2Light, alpha));
        window.draw(pulseCircle);
    }

    sf::CircleShape p2Core(8.0f);
    p2Core.setOrigin({ 8.0f, 8.0f });
    p2Core.setPosition(p2Pos);
    p2Core.setFillColor(theme::withAlpha(theme::P2, 220));
    p2Core.setOutlineThickness(2.0f);
    p2Core.setOutlineColor(sf::Color::White);
    window.draw(p2Core);

    float rot2 = -animTime * 90.0f;
    for (int i = 0; i < 4; i++) {
        sf::RectangleShape bracket({ 14.0f, 2.5f });
        bracket.setOrigin({ 20.0f, 1.25f });
        bracket.setPosition(p2Pos);
        bracket.setRotation(sf::degrees(rot2 + i * 90.0f));
        bracket.setFillColor(theme::P2Light);
        window.draw(bracket);
    }

    if (resourcesLoaded) {
        std::string p2Label = "P2";
        if (bot.isActive()) {
            if (bot.getDifficulty() == BotDifficulty::EASY) p2Label = "P2 [БОТ: ЛЕСЕН]";
            else if (bot.getDifficulty() == BotDifficulty::MEDIUM) p2Label = "P2 [БОТ: СРЕДЕН]";
            else if (bot.getDifficulty() == BotDifficulty::HARD) p2Label = "P2 [БОТ: ТРУДЕН]";
        }
        sf::Text& p2Tag = ui::pooledText(font, toUtf8(p2Label), fontsize::Label);
        p2Tag.setStyle(sf::Text::Bold);
        p2Tag.setFillColor(theme::P2Light);
        drawCursorTag(window, p2Tag, p2Pos, theme::P2);
    }
}

void UI_map::syncBuildingSelectionPos(int player) {
    if (player == 1) {
        if (isPosOnPurchasedLand(1, p1Pos)) {
            engine.getClosestGridIndex(1, p1Pos, p1GridCol, p1GridRow);
            p1Pos = engine.getGridSlot(1, p1GridCol, p1GridRow);
        }
        p1WasInResourceArea = (p1Pos.y >= 540.0f ||
                               nodes.getP1ResourceAt(p1Pos) != ResourceType::NONE ||
                               nodes.getP1UpgradeAt(p1Pos) != ResourceType::NONE ||
                               nodes.getP1ForestBounds().contains(p1Pos) ||
                               nodes.getP1MineBounds().contains(p1Pos));
    } else {
        if (isPosOnPurchasedLand(2, p2Pos)) {
            engine.getClosestGridIndex(2, p2Pos, p2GridCol, p2GridRow);
            p2Pos = engine.getGridSlot(2, p2GridCol, p2GridRow);
        }
        p2WasInResourceArea = (p2Pos.y >= 540.0f ||
                               nodes.getP2ResourceAt(p2Pos) != ResourceType::NONE ||
                               nodes.getP2UpgradeAt(p2Pos) != ResourceType::NONE ||
                               nodes.getP2ForestBounds().contains(p2Pos) ||
                               nodes.getP2MineBounds().contains(p2Pos));
    }
}

void UI_map::executeP1Action() {
    p1Pulse = 1.0f;

    // 1. Building Menu Selection (User Request: Player 1 moves cursor to solar panel with WASD, clicks Space to select it to build!)
    BuildingType pickedCard = p1Buildings.handleClick(p1Pos);
    if (pickedCard != BuildingType::NONE) {
        engine.getPlayerEconomyMut(1).selectedBuilding = static_cast<int>(pickedCard);
        syncBuildingSelectionPos(1);
        BuildingCost c = engine.getBuildingCost(pickedCard);
        triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg,
                           (pickedCard == BuildingType::DEMOLISH ? std::string("Посочете ваша сграда за разрушаване.")
                                                                : formatCost(c) + ".\nДобив: +" + std::to_string(c.basePowerMW) + " MW."),
                           "[SPACE НА ЗЕМЯ]: Постави | [E]: Друга сграда | [X]: Отказ", theme::P1);
        spawnNotice(std::string("ИЗБРАНА СГРАДА: ") + c.nameBg, p1Pos, theme::P1);
        return;
    }

    // 2. Clone existing placed building on map
    if (engine.getSelectedBuilding(1) == BuildingType::NONE) {
        for (const auto& b : engine.getBuildings()) {
            if (b.playerOwner == 1) {
                float dx = b.position.x - p1Pos.x;
                float dy = b.position.y - p1Pos.y;
                if (std::sqrt(dx * dx + dy * dy) <= 28.0f) {
                    engine.getPlayerEconomyMut(1).selectedBuilding = static_cast<int>(b.type);
                    syncBuildingSelectionPos(1);
                    BuildingCost c = engine.getBuildingCost(b.type);
                    triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg,
                                       formatCost(c) + ".\nДобив: +" + std::to_string(c.basePowerMW) + " MW.",
                                       "[SPACE НА ЗЕМЯ]: Постави | [E]: Друга сграда | [X]: Отказ", theme::P1);
                    spawnNotice(std::string("ИЗБРАНА СГРАДА: ") + c.nameBg, p1Pos, theme::P1);
                    return;
                }
            }
        }
    }

    BuildingType sel = engine.getSelectedBuilding(1);

    if (sel != BuildingType::NONE) {
        // If player is at a resource node while building is selected, switch directly to gathering!
        if (nodes.getP1ResourceAt(p1Pos) != ResourceType::NONE || nodes.getP1UpgradeAt(p1Pos) != ResourceType::NONE ||
            nodes.getP1ForestBounds().contains(p1Pos) || nodes.getP1MineBounds().contains(p1Pos) || p1Pos.y >= 540.0f) {
            engine.clearBuildingSelection(1);
            spawnNotice("РЕЖИМ ДОБИВ", p1Pos, theme::P1);
            sel = BuildingType::NONE;
        } else if (sel != BuildingType::DEMOLISH) {
            if (!isPosOnPurchasedLand(1, p1Pos)) {
                // Disallow placement confirmation clicks over unpurchased land!
                triggerPlayerPopup(1, "НЕЗАКУПЕНА ТЕРИТОРИЯ", "Земята не е закупена!", "Не може да строите върху незакупена земя.", "", theme::Warn);
                spawnNotice("НЕЗАКУПЕНА ТЕРИТОРИЯ!", p1Pos, theme::Warn);
                return;
            }
        }
    }

    if (sel != BuildingType::NONE) {
        sf::Vector2f targetPos = (sel == BuildingType::DEMOLISH) ? p1Pos : engine.snapToBuildingGrid(1, p1Pos);
        std::string msg;
        if (engine.placeBuilding(1, sel, targetPos, msg)) {
            triggerPlayerPopup(1, "УСПЕХ", "Действието е успешно!", msg, "[E]: Постави отново същата", theme::Good);
            spawnNotice("ПОСТРОЕНА СГРАДА!", targetPos, theme::Good);
            if (sel != BuildingType::DEMOLISH) engine.clearBuildingSelection(1);
        } else {
            reportBuildFailure(1, sel, msg);
        }
    } else {
        ResourceType resType = nodes.getP1ResourceAt(p1Pos);
        if (resType != ResourceType::NONE) {
            if (p1ResourceCooldown > 0.0f) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "ИЗЧАКАЙТЕ: %.1fs", p1ResourceCooldown);
                spawnNotice(buf, p1Pos + sf::Vector2f(0.0f, -25.0f), theme::Warn);
                return;
            }
            GameEngine::MineResult res;
            std::string msg;
            if (engine.mineResource(1, resType, res, msg)) {
                p1ResourceCooldown = Balance::MINE_COOLDOWN_SEC;
                const auto* st = nodes.getStation(1, resType);
                sf::Color c = st ? st->themeColor : theme::P1;
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
                            triggerPlayerPopup(1, "ЗЕМЯ", "Закупен парцел!", "Парцелът е ваш. Натиснете E за избор на сграда.", "[E]: Избери сграда", theme::Gold);
                            spawnNotice("ЗАКУПЕН ПАРЦЕЛ!", p1Pos, theme::Gold);
                        } else {
                            triggerPlayerModal(1, "НЕДОСТИГ НА ПАРИ", "Не можете да купите парцела!", msg, "Продавайте ток на града за да печелите пари ($)!", theme::Warn);
                        }
                    } else {
                        triggerPlayerPopup(1, "ИНФО", "Ваш парцел", "Земята е свободна за строителство.", "[E]: Изберете сграда за строеж", theme::P1);
                    }
                    break;
                }
            }
        }
    }
}

void UI_map::executeP2Action() {
    p2Pulse = 1.0f;

    // 1. Building Menu Selection (User Request: Player 2 moves cursor to building box, clicks Enter to select it to build!)
    BuildingType pickedCard2 = p2Buildings.handleClick(p2Pos);
    if (pickedCard2 != BuildingType::NONE) {
        engine.getPlayerEconomyMut(2).selectedBuilding = static_cast<int>(pickedCard2);
        syncBuildingSelectionPos(2);
        BuildingCost c = engine.getBuildingCost(pickedCard2);
        triggerPlayerPopup(2, "СТРОЕЖ", c.nameBg,
                           (pickedCard2 == BuildingType::DEMOLISH ? std::string("Посочете ваша сграда за разрушаване.")
                                                                 : formatCost(c) + ".\nДобив: +" + std::to_string(c.basePowerMW) + " MW."),
                           "[ENTER НА ЗЕМЯ]: Постави | [PgDn]: Друга сграда | [Del]: Отказ", theme::P2);
        spawnNotice(std::string("ИЗБРАНА СГРАДА: ") + c.nameBg, p2Pos, theme::P2);
        return;
    }

    // 2. Clone existing placed building on map
    if (engine.getSelectedBuilding(2) == BuildingType::NONE) {
        for (const auto& b : engine.getBuildings()) {
            if (b.playerOwner == 2) {
                float dx = b.position.x - p2Pos.x;
                float dy = b.position.y - p2Pos.y;
                if (std::sqrt(dx * dx + dy * dy) <= 28.0f) {
                    engine.getPlayerEconomyMut(2).selectedBuilding = static_cast<int>(b.type);
                    syncBuildingSelectionPos(2);
                    BuildingCost c = engine.getBuildingCost(b.type);
                    triggerPlayerPopup(2, "СТРОЕЖ", c.nameBg,
                                       formatCost(c) + ".\nДобив: +" + std::to_string(c.basePowerMW) + " MW.",
                                       "[ENTER НА ЗЕМЯ]: Постави | [PgDn]: Друга сграда | [Del]: Отказ", theme::P2);
                    spawnNotice(std::string("ИЗБРАНА СГРАДА: ") + c.nameBg, p2Pos, theme::P2);
                    return;
                }
            }
        }
    }

    BuildingType sel = engine.getSelectedBuilding(2);

    if (sel != BuildingType::NONE) {
        // If player 2 is at a resource node while building is selected, switch directly to gathering!
        if (nodes.getP2ResourceAt(p2Pos) != ResourceType::NONE || nodes.getP2UpgradeAt(p2Pos) != ResourceType::NONE ||
            nodes.getP2ForestBounds().contains(p2Pos) || nodes.getP2MineBounds().contains(p2Pos) || p2Pos.y >= 540.0f) {
            engine.clearBuildingSelection(2);
            spawnNotice("РЕЖИМ ДОБИВ", p2Pos, theme::P2);
            sel = BuildingType::NONE;
        } else if (sel != BuildingType::DEMOLISH) {
            if (!isPosOnPurchasedLand(2, p2Pos)) {
                // Disallow placement confirmation clicks over unpurchased land!
                triggerPlayerPopup(2, "НЕЗАКУПЕНА ТЕРИТОРИЯ", "Земята не е закупена!", "Не може да строите върху незакупена земя.", "", theme::Warn);
                spawnNotice("НЕЗАКУПЕНА ТЕРИТОРИЯ!", p2Pos, theme::Warn);
                return;
            }
        }
    }

    if (sel != BuildingType::NONE) {
        sf::Vector2f targetPos = (sel == BuildingType::DEMOLISH) ? p2Pos : engine.snapToBuildingGrid(2, p2Pos);
        std::string msg;
        if (engine.placeBuilding(2, sel, targetPos, msg)) {
            triggerPlayerPopup(2, "УСПЕХ", "Действието е успешно!", msg, "[PgDn]: Постави отново същата", theme::P2);
            spawnNotice("ПОСТРОЕНА СГРАДА!", targetPos, theme::P2);
            if (sel != BuildingType::DEMOLISH) engine.clearBuildingSelection(2);
        } else {
            reportBuildFailure(2, sel, msg);
        }
    } else {
        ResourceType resType = nodes.getP2ResourceAt(p2Pos);
        if (resType != ResourceType::NONE) {
            if (p2ResourceCooldown > 0.0f) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "ИЗЧАКАЙТЕ: %.1fs", p2ResourceCooldown);
                spawnNotice(buf, p2Pos + sf::Vector2f(0.0f, -25.0f), theme::Warn);
                return;
            }
            GameEngine::MineResult res;
            std::string msg;
            if (engine.mineResource(2, resType, res, msg)) {
                p2ResourceCooldown = Balance::MINE_COOLDOWN_SEC;
                const auto* st = nodes.getStation(2, resType);
                sf::Color c = st ? st->themeColor : theme::P2Light;
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
                            triggerPlayerPopup(2, "ЗЕМЯ", "Закупен парцел!", "Парцелът е ваш. Натиснете PgDn за избор.", "[PgDn]: Избери сграда", theme::Gold);
                            spawnNotice("ЗАКУПЕН ПАРЦЕЛ!", p2Pos, theme::Gold);
                        } else {
                            triggerPlayerPopup(2, "ГРЕШКА", "Няма пари!", msg, "Продавайте ток на града за пари ($)!", theme::Bad);
                        }
                    } else {
                        triggerPlayerPopup(2, "ИНФО", "Ваш парцел", "Земята е свободна за строителство.", "[PgDn]: Изберете сграда за строеж", theme::P2Light);
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
        spawnNotice("ЗАСТАНЕТЕ ВЪРХУ МИНА ЗА ДА Я НАДГРАДИТЕ!", p1Pos + sf::Vector2f(0.0f, -25.0f), theme::Warn);
        return;
    }
    std::string msg;
    if (engine.upgradeMine(1, resType, msg)) {
        spawnMiningParticles(p1Pos, theme::Gold, 28);
        triggerPlayerPopup(1, "НАДГРАЖДАНЕ", msg, "Добивът от тази мина е увеличен с +75%!", "[SPACE]: Добив | [F]: Нов ъпгрейд", theme::Gold);
        spawnNotice(msg, p1Pos + sf::Vector2f(0.0f, -25.0f), theme::Gold);
    } else {
        triggerPlayerPopup(1, "ГРЕШКА", msg, "Печелете злато от доставка на ток към града!", "", theme::Bad);
        spawnNotice(msg, p1Pos + sf::Vector2f(0.0f, -25.0f), theme::Bad);
    }
}

void UI_map::executeP2Upgrade() {
    p2Pulse = 1.0f;
    ResourceType resType = nodes.getP2StationAt(p2Pos);
    if (resType == ResourceType::NONE || resType == ResourceType::MONEY) {
        spawnNotice("ЗАСТАНЕТЕ ВЪРХУ МИНА ЗА ДА Я НАДГРАДИТЕ!", p2Pos + sf::Vector2f(0.0f, -25.0f), theme::Warn);
        return;
    }
    std::string msg;
    if (engine.upgradeMine(2, resType, msg)) {
        spawnMiningParticles(p2Pos, theme::Gold, 28);
        triggerPlayerPopup(2, "НАДГРАЖДАНЕ", msg, "Добивът от тази мина е увеличен с +75%!", "[ENTER]: Добив | [RShift]: Нов ъпгрейд", theme::Gold);
        spawnNotice(msg, p2Pos + sf::Vector2f(0.0f, -25.0f), theme::Gold);
    } else {
        triggerPlayerPopup(2, "ГРЕШКА", msg, "Печелете злато от доставка на ток към града!", "", theme::Bad);
        spawnNotice(msg, p2Pos + sf::Vector2f(0.0f, -25.0f), theme::Bad);
    }
}

void UI_map::updateControls(const sf::RenderWindow& window, float dt) {
    if (isPaused || engine.getCityState().winner != 0) {
        return;
    }

    // Screenshot mode never reads input, so captures do not depend on focus or keys (the scene's
    // time scale is kept).
    if (ui::shot::isActive()) return;

    // Keyboard & mouse state is global: never read it while another window has the focus
    if (!window.hasFocus()) {
        engine.setTimeScale(1.0f);
        primeInputEdges(0);
        return;
    }

    float speed = 360.0f;
    sf::Vector2f mPos = ui::pointerPos(window);
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

    // 3. Player 1 Movement (Precision Grid during placement on purchased land, smooth analog otherwise)
    const auto& p1Bindings = UI_controlsConfig::get().p1;
    const auto& p2Bindings = UI_controlsConfig::get().p2;

    bool p1BuildingMode = (engine.getSelectedBuilding(1) != BuildingType::NONE);
    bool allowArrowsForP1 = bot.isActive(); // In Single Player, player can use WASD OR Arrow keys!

    bool p1InResourceArea = (p1Pos.y >= 540.0f ||
                             nodes.getP1ResourceAt(p1Pos) != ResourceType::NONE ||
                             nodes.getP1UpgradeAt(p1Pos) != ResourceType::NONE ||
                             nodes.getP1ForestBounds().contains(p1Pos) ||
                             nodes.getP1MineBounds().contains(p1Pos));

    // Only when player enters the resource collection area, turn off build mode!
    if (p1BuildingMode && p1InResourceArea && !p1WasInResourceArea) {
        engine.clearBuildingSelection(1);
        p1BuildingMode = false;
        spawnNotice("РЕЖИМ ДОБИВ", p1Pos, theme::P1);
    }
    p1WasInResourceArea = p1InResourceArea;

    bool p1OnPurchased = isPosOnPurchasedLand(1, p1Pos);

    bool joyUp1 = UI_controlsConfig::get().isJoystickDirectionPressed(1, ControlAction::MOVE_UP);
    bool joyDown1 = UI_controlsConfig::get().isJoystickDirectionPressed(1, ControlAction::MOVE_DOWN);
    bool joyLeft1 = UI_controlsConfig::get().isJoystickDirectionPressed(1, ControlAction::MOVE_LEFT);
    bool joyRight1 = UI_controlsConfig::get().isJoystickDirectionPressed(1, ControlAction::MOVE_RIGHT);

    if (p1BuildingMode && p1OnPurchased) {
        if (controlScheme == ControlScheme::BOTH_KEYBOARD || controlScheme == ControlScheme::P1_KEYBOARD_P2_MOUSE || controlScheme == ControlScheme::DEVHUB_ARCADE || allowArrowsForP1) {
            if (p1GridStepCooldown <= 0.0f) {
                bool moved = false;
                int nextRow = p1GridRow;
                int nextCol = p1GridCol;
                if (sf::Keyboard::isKeyPressed(p1Bindings.up) || (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up)) || joyUp1) {
                    if (p1GridRow > 0) {
                        nextRow = p1GridRow - 1;
                        moved = true;
                    } else {
                        p1Pos.y -= 35.0f;
                        p1GridStepCooldown = 0.14f;
                    }
                } else if (sf::Keyboard::isKeyPressed(p1Bindings.down) || (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down)) || joyDown1) {
                    if (p1GridRow < 11) {
                        nextRow = p1GridRow + 1;
                        moved = true;
                    } else {
                        p1Pos.y += 35.0f;
                        p1GridStepCooldown = 0.14f;
                    }
                }
                if (sf::Keyboard::isKeyPressed(p1Bindings.left) || (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)) || joyLeft1) {
                    if (p1GridCol > 0) {
                        nextCol = p1GridCol - 1;
                        moved = true;
                    } else {
                        p1Pos.x -= 35.0f;
                        p1GridStepCooldown = 0.14f;
                    }
                } else if (sf::Keyboard::isKeyPressed(p1Bindings.right) || (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) || joyRight1) {
                    if (p1GridCol < 8) {
                        nextCol = p1GridCol + 1;
                        moved = true;
                    } else {
                        p1Pos.x += 35.0f;
                        p1GridStepCooldown = 0.14f;
                    }
                }
                if (moved) {
                    p1GridCol = nextCol;
                    p1GridRow = nextRow;
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
        if (controlScheme == ControlScheme::BOTH_KEYBOARD || controlScheme == ControlScheme::P1_KEYBOARD_P2_MOUSE || controlScheme == ControlScheme::DEVHUB_ARCADE || allowArrowsForP1) {
            if (sf::Keyboard::isKeyPressed(p1Bindings.up) || (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))) p1Pos.y -= speed * dt;
            if (sf::Keyboard::isKeyPressed(p1Bindings.down) || (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))) p1Pos.y += speed * dt;
            if (sf::Keyboard::isKeyPressed(p1Bindings.left) || (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))) p1Pos.x -= speed * dt;
            if (sf::Keyboard::isKeyPressed(p1Bindings.right) || (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))) p1Pos.x += speed * dt;

            sf::Vector2f joyVec1 = UI_controlsConfig::get().getJoystickMoveVector(1);
            if (std::abs(joyVec1.x) > 0.05f || std::abs(joyVec1.y) > 0.05f) {
                p1Pos.x += joyVec1.x * speed * dt;
                p1Pos.y += joyVec1.y * speed * dt;
            }
        } else if (controlScheme == ControlScheme::P1_MOUSE_P2_KEYBOARD) {
            if (mouseOnCanvas) p1Pos = mPos;
        } else if (controlScheme == ControlScheme::BOTH_MOUSE) {
            if (mouseOnCanvas && mPos.x < 800.0f) p1Pos = mPos;
        }
        p1Pos.x = std::max(20.0f, std::min(p1Pos.x, 780.0f));
        p1Pos.y = std::max(40.0f, std::min(p1Pos.y, 860.0f));

        if (p1BuildingMode && isPosOnPurchasedLand(1, p1Pos)) {
            engine.getClosestGridIndex(1, p1Pos, p1GridCol, p1GridRow);
            p1Pos = engine.getGridSlot(1, p1GridCol, p1GridRow);
            p1GridStepCooldown = 0.14f;
        } else {
            engine.getClosestGridIndex(1, p1Pos, p1GridCol, p1GridRow);
        }
    }

    // 4. Player 2 Movement (Bot AI or Human Input)
    engine.setP2IsBot(bot.isActive());
    if (bot.isActive()) {
        p2Modal.active = false; // Never block Player 2 bot with a modal dialog

        // An active tutorial gives the human a head start, but only for a limited window:
        // skipping or completing it, or running out of time, releases the bot.
        bool tutorialHoldsBot = tutorial.isTutorialBlockingTime();
        if (tutorialHoldsBot) {
            tutorialBotHoldLeft = TUTORIAL_BOT_HOLD_SEC;
        } else if (tutorial.isActive() && tutorial.getStep() != TutorialStep::COMPLETED && tutorialBotHoldLeft > 0.0f) {
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
        bool p2InResourceArea = (p2Pos.y >= 540.0f ||
                                 nodes.getP2ResourceAt(p2Pos) != ResourceType::NONE ||
                                 nodes.getP2UpgradeAt(p2Pos) != ResourceType::NONE ||
                                 nodes.getP2ForestBounds().contains(p2Pos) ||
                                 nodes.getP2MineBounds().contains(p2Pos));

        // Only when player enters the resource collection area, turn off build mode!
        if (p2BuildingMode && p2InResourceArea && !p2WasInResourceArea) {
            engine.clearBuildingSelection(2);
            p2BuildingMode = false;
            spawnNotice("РЕЖИМ ДОБИВ", p2Pos, theme::P2);
        }
        p2WasInResourceArea = p2InResourceArea;

        bool p2OnPurchased = isPosOnPurchasedLand(2, p2Pos);

        bool joyUp2 = UI_controlsConfig::get().isJoystickDirectionPressed(2, ControlAction::MOVE_UP);
        bool joyDown2 = UI_controlsConfig::get().isJoystickDirectionPressed(2, ControlAction::MOVE_DOWN);
        bool joyLeft2 = UI_controlsConfig::get().isJoystickDirectionPressed(2, ControlAction::MOVE_LEFT);
        bool joyRight2 = UI_controlsConfig::get().isJoystickDirectionPressed(2, ControlAction::MOVE_RIGHT);

        if (p2BuildingMode && p2OnPurchased) {
            if (controlScheme == ControlScheme::BOTH_KEYBOARD || controlScheme == ControlScheme::P1_MOUSE_P2_KEYBOARD || controlScheme == ControlScheme::DEVHUB_ARCADE) {
                if (p2GridStepCooldown <= 0.0f) {
                    bool moved = false;
                    int nextRow = p2GridRow;
                    int nextCol = p2GridCol;
                    if (sf::Keyboard::isKeyPressed(p2Bindings.up) || joyUp2) {
                        if (p2GridRow > 0) {
                            nextRow = p2GridRow - 1;
                            moved = true;
                        } else {
                            p2Pos.y -= 35.0f;
                            p2GridStepCooldown = 0.14f;
                        }
                    } else if (sf::Keyboard::isKeyPressed(p2Bindings.down) || joyDown2) {
                        if (p2GridRow < 11) {
                            nextRow = p2GridRow + 1;
                            moved = true;
                        } else {
                            p2Pos.y += 35.0f;
                            p2GridStepCooldown = 0.14f;
                        }
                    }
                    if (sf::Keyboard::isKeyPressed(p2Bindings.left) || joyLeft2) {
                        if (p2GridCol > 0) {
                            nextCol = p2GridCol - 1;
                            moved = true;
                        } else {
                            p2Pos.x -= 35.0f;
                            p2GridStepCooldown = 0.14f;
                        }
                    } else if (sf::Keyboard::isKeyPressed(p2Bindings.right) || joyRight2) {
                        if (p2GridCol < 8) {
                            nextCol = p2GridCol + 1;
                            moved = true;
                        } else {
                            p2Pos.x += 35.0f;
                            p2GridStepCooldown = 0.14f;
                        }
                    }
                    if (moved) {
                        p2GridCol = nextCol;
                        p2GridRow = nextRow;
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
            if (controlScheme == ControlScheme::BOTH_KEYBOARD || controlScheme == ControlScheme::P1_MOUSE_P2_KEYBOARD || controlScheme == ControlScheme::DEVHUB_ARCADE) {
                if (sf::Keyboard::isKeyPressed(p2Bindings.up)) p2Pos.y -= speed * dt;
                if (sf::Keyboard::isKeyPressed(p2Bindings.down)) p2Pos.y += speed * dt;
                if (sf::Keyboard::isKeyPressed(p2Bindings.left)) p2Pos.x -= speed * dt;
                if (sf::Keyboard::isKeyPressed(p2Bindings.right)) p2Pos.x += speed * dt;

                sf::Vector2f joyVec2 = UI_controlsConfig::get().getJoystickMoveVector(2);
                if (std::abs(joyVec2.x) > 0.05f || std::abs(joyVec2.y) > 0.05f) {
                    p2Pos.x += joyVec2.x * speed * dt;
                    p2Pos.y += joyVec2.y * speed * dt;
                }
            } else if (controlScheme == ControlScheme::P1_KEYBOARD_P2_MOUSE) {
                if (mouseOnCanvas) p2Pos = mPos;
            } else if (controlScheme == ControlScheme::BOTH_MOUSE) {
                if (mouseOnCanvas && mPos.x >= 800.0f) p2Pos = mPos;
            }
            p2Pos.x = std::max(820.0f, std::min(p2Pos.x, 1580.0f));
            p2Pos.y = std::max(40.0f, std::min(p2Pos.y, 860.0f));

            if (p2BuildingMode && isPosOnPurchasedLand(2, p2Pos)) {
                engine.getClosestGridIndex(2, p2Pos, p2GridCol, p2GridRow);
                p2Pos = engine.getGridSlot(2, p2GridCol, p2GridRow);
                p2GridStepCooldown = 0.14f;
            } else {
                engine.getClosestGridIndex(2, p2Pos, p2GridCol, p2GridRow);
            }
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
    bool p1JoyAction = UI_controlsConfig::get().isJoystickActionPressed(1, ControlAction::ACTION);
    bool p1PressingAction = sf::Keyboard::isKeyPressed(p1Bindings.action) ||
                            (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter)) ||
                            p1JoyAction;
    bool p1JustPressed = p1PressingAction && !p1PrevAction;
    p1PrevAction = p1PressingAction;

    if (p1JustPressed && p1ActionCooldown <= 0.0f && !p1Modal.active && !showHelpOverlay) {
        executeP1Action();
        p1ActionCooldown = 0.20f;
    }

    // P1 Upgrade Mine with Gold
    bool p1JoyUpgrade = UI_controlsConfig::get().isJoystickActionPressed(1, ControlAction::UPGRADE);
    bool p1PressingUpgrade = sf::Keyboard::isKeyPressed(p1Bindings.upgrade) ||
                             (allowArrowsForP1 && (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::End))) ||
                             p1JoyUpgrade;
    if (p1PressingUpgrade && !p1PrevUpgrade && !p1Modal.active && !showHelpOverlay) {
        executeP1Upgrade();
    }
    p1PrevUpgrade = p1PressingUpgrade;

    // Key names shown in P1's popups: Enter/PgDn/Del belong to P1 only in Single Player
    const bool isArcade1 = (controlScheme == ControlScheme::DEVHUB_ARCADE) || UI_controlsConfig::get().isJoystickConnected(1);
    const std::string p1ActKeys = isArcade1 ? "[1/A]" : ("[" + keyToString(p1Bindings.action) + (allowArrowsForP1 ? "/ENTER]" : "]"));
    const std::string p1NextKeys = isArcade1 ? "[4/Y]" : ("[" + keyToString(p1Bindings.nextBuilding) + (allowArrowsForP1 ? "/PgDn]" : "]"));
    const std::string p1CancelKeys = isArcade1 ? "[2/B]" : ("[" + keyToString(p1Bindings.cancel) + (allowArrowsForP1 ? "/Del]" : "]"));

    // P1: Cycle Forward
    bool p1JoyNext = UI_controlsConfig::get().isJoystickActionPressed(1, ControlAction::NEXT_BUILDING);
    bool curE = sf::Keyboard::isKeyPressed(p1Bindings.nextBuilding) ||
                (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::PageDown)) ||
                p1JoyNext;
    if (curE && !p1PrevE && p1SelectCooldown <= 0.0f && !p1Modal.active && !showHelpOverlay) {
        engine.cycleBuildingSelection(1);
        p1SelectCooldown = 0.16f;
        BuildingType newSel = engine.getSelectedBuilding(1);
        BuildingCost c = engine.getBuildingCost(newSel);
        syncBuildingSelectionPos(1);
        if (newSel == BuildingType::DEMOLISH) {
            triggerPlayerPopup(1, "ПРЕМАХВАНЕ", c.nameBg, "Кликнете върху ваша сграда за разрушаване.\nВръща 50% от ресурсите.", p1ActKeys + ": Премахни | " + p1NextKeys + ": Следваща | " + p1CancelKeys + ": Отказ", theme::Bad);
        } else if (newSel == BuildingType::LAMP) {
            triggerPlayerPopup(1, "ОСВЕТЛЕНИЕ", c.nameBg, formatCost(c) + ".\nОсветява нощем за строителство.", p1ActKeys + ": Постави | " + p1NextKeys + ": Следваща | " + p1CancelKeys + ": Отказ", theme::Warn);
        } else {
            triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg,
                               formatCost(c) + ".\nДобив: +" + std::to_string(c.basePowerMW) + " MW ток.",
                               p1ActKeys + ": Постави в грида | " + p1NextKeys + ": Следваща | " + p1CancelKeys + ": Отказ", theme::P1);
        }
    }
    p1PrevE = curE;

    // P1: Cycle Backward / Cancel
    bool p1JoyPrev = UI_controlsConfig::get().isJoystickActionPressed(1, ControlAction::PREV_BUILDING);
    bool curQ = sf::Keyboard::isKeyPressed(p1Bindings.prevBuilding) ||
                (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::PageUp)) ||
                p1JoyPrev;
    if (curQ && !p1PrevQ && p1SelectCooldown <= 0.0f && !p1Modal.active && !showHelpOverlay) {
        if (engine.getSelectedBuilding(1) != BuildingType::NONE) {
            engine.cycleBuildingSelectionPrev(1);
            p1SelectCooldown = 0.16f;
            BuildingType newSel = engine.getSelectedBuilding(1);
            BuildingCost c = engine.getBuildingCost(newSel);
            syncBuildingSelectionPos(1);
            if (newSel == BuildingType::DEMOLISH) {
                triggerPlayerPopup(1, "ПРЕМАХВАНЕ", c.nameBg, "Кликнете върху ваша сграда за разрушаване.\nВръща 50% от ресурсите.", p1ActKeys + ": Премахни | " + p1NextKeys + ": Следваща | " + p1CancelKeys + ": Отказ", theme::Bad);
            } else if (newSel == BuildingType::LAMP) {
                triggerPlayerPopup(1, "ОСВЕТЛЕНИЕ", c.nameBg, formatCost(c) + ".\nОсветява нощем за строителство.", p1ActKeys + ": Постави | " + p1NextKeys + ": Следваща | " + p1CancelKeys + ": Отказ", theme::Warn);
            } else {
                triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg,
                                   formatCost(c) + ".\nДобив: +" + std::to_string(c.basePowerMW) + " MW ток.",
                                   p1ActKeys + ": Постави в грида | " + p1NextKeys + ": Следваща | " + p1CancelKeys + ": Отказ", theme::P1);
            }
        }
    }
    p1PrevQ = curQ;

    bool p1JoyCancel = UI_controlsConfig::get().isJoystickActionPressed(1, ControlAction::CANCEL);
    bool curX = sf::Keyboard::isKeyPressed(p1Bindings.cancel) ||
                (allowArrowsForP1 && (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Delete) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Backspace))) ||
                p1JoyCancel;
    if (curX && !p1PrevX && !p1Modal.active && !showHelpOverlay) {
        if (engine.getSelectedBuilding(1) != BuildingType::NONE) {
            engine.clearBuildingSelection(1);
            triggerPlayerPopup(1, "ОТКАЗ", "Изборът е прекратен", "Свободен режим.", p1NextKeys + ": Избери сграда", theme::TextSecondary);
        } else {
            engine.getPlayerEconomyMut(1).selectedBuilding = static_cast<int>(BuildingType::DEMOLISH);
            syncBuildingSelectionPos(1);
            triggerPlayerPopup(1, "ПРЕМАХВАНЕ", "Режим Разрушаване", "Посочете сградата, която искате да махнете.", p1ActKeys + ": Премахни | " + p1CancelKeys + ": Отказ", theme::Bad);
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
            syncBuildingSelectionPos(1);
            triggerPlayerPopup(1, (k == 6 ? "ПРЕМАХВАНЕ" : "СТРОЕЖ"), c.nameBg,
                               (k == 6 ? "Посочете сграда за разрушаване." : formatCost(c)),
                               "[" + keyToString(p1Bindings.action) + "]: Постави в грида | [" + keyToString(p1Bindings.cancel) + "]: Отказ", (k == 6 ? theme::Bad : theme::P1));
        }
        p1PrevNum[k] = curNum;
    }

    // 7. Player 2 Action Input (Human Player 2 only)
    if (!bot.isActive()) {
        const bool isArcade2 = (controlScheme == ControlScheme::DEVHUB_ARCADE) || UI_controlsConfig::get().isJoystickConnected(2);
        const std::string p2ActKeys = isArcade2 ? "[1/A]" : ("[" + keyToString(p2Bindings.action) + "]");
        const std::string p2NextKeys = isArcade2 ? "[4/Y]" : ("[" + keyToString(p2Bindings.nextBuilding) + "]");
        const std::string p2PrevKeys = isArcade2 ? "[3/X]" : ("[" + keyToString(p2Bindings.prevBuilding) + "]");
        const std::string p2CancelKeys = isArcade2 ? "[2/B]" : ("[" + keyToString(p2Bindings.cancel) + "]");

        bool p2JoyAction = UI_controlsConfig::get().isJoystickActionPressed(2, ControlAction::ACTION);
        bool p2PressingAction = sf::Keyboard::isKeyPressed(p2Bindings.action) || p2JoyAction;
        bool p2JustPressed = p2PressingAction && !p2PrevAction;
        p2PrevAction = p2PressingAction;

        if (p2JustPressed && p2ActionCooldown <= 0.0f && !p2Modal.active && !showHelpOverlay) {
            executeP2Action();
            p2ActionCooldown = 0.20f;
        }

        // P2 Upgrade Mine with Gold
        bool p2JoyUpgrade = UI_controlsConfig::get().isJoystickActionPressed(2, ControlAction::UPGRADE);
        bool p2PressingUpgrade = sf::Keyboard::isKeyPressed(p2Bindings.upgrade) || p2JoyUpgrade;
        if (p2PressingUpgrade && !p2PrevUpgrade && !p2Modal.active && !showHelpOverlay) {
            executeP2Upgrade();
        }
        p2PrevUpgrade = p2PressingUpgrade;

        // P2: Cycle Forward
        bool p2JoyNext = UI_controlsConfig::get().isJoystickActionPressed(2, ControlAction::NEXT_BUILDING);
        bool curPgDn = sf::Keyboard::isKeyPressed(p2Bindings.nextBuilding) || p2JoyNext;
        if (curPgDn && !p2PrevPgDn && p2SelectCooldown <= 0.0f && !p2Modal.active && !showHelpOverlay) {
            engine.cycleBuildingSelection(2);
            p2SelectCooldown = 0.16f;
            BuildingType newSel = engine.getSelectedBuilding(2);
            BuildingCost c = engine.getBuildingCost(newSel);
            syncBuildingSelectionPos(2);
            if (newSel == BuildingType::DEMOLISH) {
                triggerPlayerPopup(2, "ПРЕМАХВАНЕ", c.nameBg, "Кликнете върху ваша сграда за разрушаване.\nВръща 50% от ресурсите.", p2ActKeys + ": Премахни | " + p2NextKeys + ": Следваща | " + p2PrevKeys + ": Предишна", theme::Bad);
            } else if (newSel == BuildingType::LAMP) {
                triggerPlayerPopup(2, "ОСВЕТЛЕНИЕ", c.nameBg, formatCost(c) + ".\nОсветява нощем за строителство.", p2ActKeys + ": Постави | " + p2NextKeys + ": Следваща | " + p2PrevKeys + ": Предишна", theme::Warn);
            } else {
                triggerPlayerPopup(2, "СТРОЕЖ", c.nameBg,
                                   formatCost(c) + ".\nДобив: +" + std::to_string(c.basePowerMW) + " MW ток.",
                                   p2ActKeys + ": Постави в грида | " + p2NextKeys + ": Следваща | " + p2PrevKeys + ": Предишна", theme::P2);
            }
        }
        p2PrevPgDn = curPgDn;

        // P2: Cycle Backward
        bool p2JoyPrev = UI_controlsConfig::get().isJoystickActionPressed(2, ControlAction::PREV_BUILDING);
        bool curPgUp = sf::Keyboard::isKeyPressed(p2Bindings.prevBuilding) || p2JoyPrev;
        if (curPgUp && !p2PrevPgUp && p2SelectCooldown <= 0.0f && !p2Modal.active && !showHelpOverlay &&
            engine.getSelectedBuilding(2) != BuildingType::NONE) {
            engine.cycleBuildingSelectionPrev(2);
            p2SelectCooldown = 0.16f;
            BuildingType newSel = engine.getSelectedBuilding(2);
            BuildingCost c = engine.getBuildingCost(newSel);
            syncBuildingSelectionPos(2);
            if (newSel == BuildingType::DEMOLISH) {
                triggerPlayerPopup(2, "ПРЕМАХВАНЕ", c.nameBg, "Кликнете върху ваша сграда за разрушаване.\nВръща 50% от ресурсите.", p2ActKeys + ": Премахни | " + p2NextKeys + ": Следваща | " + p2PrevKeys + ": Предишна", theme::Bad);
            } else if (newSel == BuildingType::LAMP) {
                triggerPlayerPopup(2, "ОСВЕТЛЕНИЕ", c.nameBg, formatCost(c) + ".\nОсветява нощем за строителство.", p2ActKeys + ": Постави | " + p2NextKeys + ": Следваща | " + p2PrevKeys + ": Предишна", theme::Warn);
            } else {
                triggerPlayerPopup(2, "СТРОЕЖ", c.nameBg,
                                   formatCost(c) + ".\nДобив: +" + std::to_string(c.basePowerMW) + " MW ток.",
                                   p2ActKeys + ": Постави в грида | " + p2NextKeys + ": Следваща | " + p2PrevKeys + ": Предишна", theme::P2);
            }
        }
        p2PrevPgUp = curPgUp;

        // P2: Cancel / Demolish mode
        bool p2JoyCancel = UI_controlsConfig::get().isJoystickActionPressed(2, ControlAction::CANCEL);
        bool curDel = sf::Keyboard::isKeyPressed(p2Bindings.cancel) || p2JoyCancel;
        if (curDel && !p2PrevDel && !p2Modal.active && !showHelpOverlay) {
            if (engine.getSelectedBuilding(2) != BuildingType::NONE) {
                engine.clearBuildingSelection(2);
                triggerPlayerPopup(2, "ОТКАЗ", "Изборът е прекратен", "Свободен режим.", p2NextKeys + ": Избери сграда", theme::TextSecondary);
            } else {
                engine.getPlayerEconomyMut(2).selectedBuilding = static_cast<int>(BuildingType::DEMOLISH);
                syncBuildingSelectionPos(2);
                triggerPlayerPopup(2, "ПРЕМАХВАНЕ", "Режим Разрушаване", "Посочете сградата, която искате да махнете.", p2ActKeys + ": Премахни | " + p2CancelKeys + ": Отказ", theme::Bad);
            }
        }
        p2PrevDel = curDel;

        // Direct hotkeys Numpad 1..6 for P2 (same order as the cards and P1's keys 1..6)
        for (int k = 1; k <= 6; ++k) {
            sf::Keyboard::Key numKey = static_cast<sf::Keyboard::Key>(static_cast<int>(sf::Keyboard::Key::Numpad1) + (k - 1));
            bool curNum = sf::Keyboard::isKeyPressed(numKey);
            if (curNum && !p2PrevNum[k] && !p2Modal.active && !showHelpOverlay) {
                engine.getPlayerEconomyMut(2).selectedBuilding = k;
                BuildingCost c = engine.getBuildingCost(static_cast<BuildingType>(k));
                syncBuildingSelectionPos(2);
                triggerPlayerPopup(2, (k == 6 ? "ПРЕМАХВАНЕ" : "СТРОЕЖ"), c.nameBg,
                                   (k == 6 ? "Посочете сграда за разрушаване." : formatCost(c)),
                                   "[ENTER]: Постави в грида | [Del]: Отказ", (k == 6 ? theme::Bad : theme::P2));
            }
            p2PrevNum[k] = curNum;
        }
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
        if (const auto* jb = event.getIf<sf::Event::JoystickButtonPressed>()) {
            if (jb->button == 0 || jb->button == 7) {
                restartMatch();
                return;
            }
            if (jb->button == 1 || jb->button == 6) {
                requestMenu = true;
                return;
            }
        }
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
        if (event.is<sf::Event::MouseButtonPressed>() || event.is<sf::Event::JoystickButtonPressed>()) {
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
        if (const auto* jb = event.getIf<sf::Event::JoystickButtonPressed>()) {
            if (jb->button == 1 || jb->button == 6) { // Back / Coin -> Resume
                isPaused = false;
                primeInputEdges(0);
                return;
            }
            if (jb->button == 0 || jb->button == 7) { // A / Start -> Select item
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
        }
        if (const auto* jm = event.getIf<sf::Event::JoystickMoved>()) {
            if (jm->axis == sf::Joystick::Axis::Y || jm->axis == sf::Joystick::Axis::PovY) {
                if (jm->position < -55.0f) {
                    pauseSelectedIdx = (pauseSelectedIdx + 4) % 5;
                    return;
                } else if (jm->position > 55.0f) {
                    pauseSelectedIdx = (pauseSelectedIdx + 1) % 5;
                    return;
                }
            }
        }
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
    //    confirm/cancel keys or joystick buttons
    // -------------------------------------------------------------------------
    if (const auto* jb = event.getIf<sf::Event::JoystickButtonPressed>()) {
        if (p1Modal.active && (jb->joystickId == 0) && (jb->button == 0 || jb->button == 1 || jb->button == 6 || jb->button == 7)) {
            closePlayerModal(1);
            return;
        }
        if (p2Modal.active && (jb->joystickId == 1 || (jb->joystickId == 0 && jb->button >= 10)) &&
            (jb->button == 0 || jb->button == 1 || jb->button == 6 || jb->button == 7 || jb->button == 10 || jb->button == 11)) {
            closePlayerModal(2);
            return;
        }
    }
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
    // 3. Interactive Tutorial Clicks & Keypresses / Joystick
    // -------------------------------------------------------------------------
    if (tutorial.isActive()) {
        if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
            sf::Vector2f clickPos = window.mapPixelToCoords(mb->position);
            if (tutorial.handleClick(clickPos)) {
                return;
            }
        }
        if (const auto* jb = event.getIf<sf::Event::JoystickButtonPressed>()) {
            if (jb->button == 1 || jb->button == 6) { // Back / Coin skips tutorial
                tutorial.skip();
                return;
            }
            if (jb->button == 0 || jb->button == 7) { // A / Start advances tutorial
                if (tutorial.handleKey(sf::Keyboard::Key::Space)) {
                    primeInputEdges(0);
                    return;
                }
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

    if (const auto* jb = event.getIf<sf::Event::JoystickButtonPressed>()) {
        if (jb->button == 7 || jb->button == 6) { // Start or Coin pauses match
            isPaused = true;
            pauseSelectedIdx = 0;
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
                triggerPlayerPopup(owner, "ОТКАЗ", "Отменен строеж", "Режимът за поставяне е прекратен.", "", theme::TextSecondary);
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

        // 1. Building Menu Clicks (User Request 2: Click directly on building card to select it!)
        // Active in all control schemes so clicking cards always works smoothly
        BuildingType clickedP1 = p1Buildings.handleClick(clickPos);
        if (clickedP1 != BuildingType::NONE) {
            if (engine.getPlayerEconomy(1).selectedBuilding == static_cast<int>(clickedP1)) {
                engine.clearBuildingSelection(1);
                triggerPlayerPopup(1, "ОТКАЗ", "Отменен строеж", "Режимът за поставяне е прекратен.", "", theme::TextSecondary);
                return;
            }
            engine.getPlayerEconomyMut(1).selectedBuilding = static_cast<int>(clickedP1);
            syncBuildingSelectionPos(1);
            BuildingCost c = engine.getBuildingCost(clickedP1);
            triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg,
                               (clickedP1 == BuildingType::DEMOLISH ? std::string("Посочете ваша сграда за разрушаване.")
                                                                    : formatCost(c) + ".\nДобив: +" + std::to_string(c.basePowerMW) + " MW."),
                               "[КЛИК НА ЗЕМЯ]: Постави | [КЛИК КАРТА / ДЕСЕН КЛИК]: Отказ", theme::P1);
            return;
        }

        BuildingType clickedP2 = p2Buildings.handleClick(clickPos);
        if (clickedP2 != BuildingType::NONE) {
            if (engine.getPlayerEconomy(2).selectedBuilding == static_cast<int>(clickedP2)) {
                engine.clearBuildingSelection(2);
                triggerPlayerPopup(2, "ОТКАЗ", "Отменен строеж", "Режимът за поставяне е прекратен.", "", theme::TextSecondary);
                return;
            }
            engine.getPlayerEconomyMut(2).selectedBuilding = static_cast<int>(clickedP2);
            syncBuildingSelectionPos(2);
            BuildingCost c = engine.getBuildingCost(clickedP2);
            triggerPlayerPopup(2, "СТРОЕЖ", c.nameBg,
                               (clickedP2 == BuildingType::DEMOLISH ? std::string("Посочете ваша сграда за разрушаване.")
                                                                    : formatCost(c) + ".\nДобив: +" + std::to_string(c.basePowerMW) + " MW."),
                               "[КЛИК НА ЗЕМЯ]: Постави | [КЛИК КАРТА / ДЕСЕН КЛИК]: Отказ", theme::P2);
            return;
        }

        // Also: Clicking on an existing placed building selects it to build another one (User Request 2)
        for (const auto& b : engine.getBuildings()) {
            float dx = b.position.x - clickPos.x;
            float dy = b.position.y - clickPos.y;
            if (std::sqrt(dx * dx + dy * dy) <= 28.0f) {
                int targetPlayer = (owner != 0 ? owner : b.playerOwner);
                engine.getPlayerEconomyMut(targetPlayer).selectedBuilding = static_cast<int>(b.type);
                syncBuildingSelectionPos(targetPlayer);
                BuildingCost c = engine.getBuildingCost(b.type);
                triggerPlayerPopup(targetPlayer, "СТРОЕЖ", c.nameBg,
                                   formatCost(c) + ".\nДобив: +" + std::to_string(c.basePowerMW) + " MW.",
                                   "[КЛИК НА ЗЕМЯ]: Постави | [ДЕСЕН КЛИК]: Отказ", (targetPlayer == 1 ? theme::P1 : theme::P2));
                return;
            }
        }

        // 2. Buy Land HUD button clicks (with Money $)
        if (resourceHUD.getP1BuyLandButton().contains(clickPos)) {
            std::string msg;
            if (engine.buyNextLandTier(1, msg)) {
                triggerPlayerPopup(1, "ЗЕМЯ", "Разширена земя!", msg, "[E]: Избери сграда за строеж", theme::Good);
            } else {
                triggerPlayerPopup(1, "ГРЕШКА", "Няма пари!", msg, "Продавайте ток на града за пари ($)!", theme::Bad);
            }
            return;
        }
        if (resourceHUD.getP2BuyLandButton().contains(clickPos)) {
            std::string msg;
            if (engine.buyNextLandTier(2, msg)) {
                triggerPlayerPopup(2, "ЗЕМЯ", "Разширена земя!", msg, "[PgDn]: Избери сграда", theme::Good);
            } else {
                triggerPlayerPopup(2, "ГРЕШКА", "Няма пари!", msg, "Продавайте ток на града за пари ($)!", theme::Bad);
            }
            return;
        }

        // Check modal before continuing
        if (ownerModal && ownerModal->active) {
            if (ownerModal->okBtn.contains(clickPos)) {
                closePlayerModal(owner);
            }
            return;
        }

        // 3. Click on Land Plots directly (Buy Plot with Money or Place Building)
        for (const auto& plot : engine.getLandPlots()) {
            if (plot.bounds.contains(clickPos)) {
                int pOwner = plot.playerOwner;
                BuildingType sel = engine.getSelectedBuilding(pOwner);
                if (!plot.isPurchased) {
                    if (sel != BuildingType::NONE) {
                        spawnNotice("НЕЗАКУПЕНА ТЕРИТОРИЯ!", clickPos, theme::Warn);
                        triggerPlayerPopup(pOwner, "НЕЗАКУПЕНА ТЕРИТОРИЯ", "Земята не е закупена!", "Трябва първо да закупите парцела.", "", theme::Warn);
                        return;
                    }
                    std::string msg;
                    if (engine.buyLandPlot(pOwner, plot.id, msg)) {
                        triggerPlayerPopup(pOwner, "ЗЕМЯ", "Купихте парцел!", msg + "\nВече можете да строите тук.", "[КЛИК]: Постави сграда", theme::Good);
                    } else {
                        triggerPlayerModal(pOwner, "НЕДОСТИГ НА ПАРИ", "Не можете да купите парцела!", msg, "Продавайте ток на града за да печелите пари ($)!", theme::Warn);
                    }
                    return;
                } else {
                    if (sel != BuildingType::NONE) {
                        sf::Vector2f targetPos = (sel == BuildingType::DEMOLISH) ? clickPos : engine.snapToBuildingGrid(pOwner, clickPos);
                        std::string msg;
                        if (engine.placeBuilding(pOwner, sel, targetPos, msg)) {
                            triggerPlayerPopup(pOwner, "УСПЕХ", "Действието е успешно!", msg, "", theme::Good);
                            if (sel != BuildingType::DEMOLISH) engine.clearBuildingSelection(pOwner);
                        } else {
                            reportBuildFailure(pOwner, sel, msg);
                        }
                        return;
                    }
                }
                break;
            }
        }

        // 4. Click on Upgrade Button of Resource Stations
        if (owner == 1) {
            ResourceType p1Up = nodes.getP1UpgradeAt(clickPos);
            if (p1Up != ResourceType::NONE) {
                std::string msg;
                if (engine.upgradeMine(1, p1Up, msg)) {
                    spawnMiningParticles(clickPos, theme::Gold, 25);
                    triggerPlayerPopup(1, "НАДГРАЖДАНЕ", msg, "Добивът от тази мина е увеличен с +75%!", "[SPACE]: Добив | [F]: Нов ъпгрейд", theme::Gold);
                    spawnNotice(msg, clickPos + sf::Vector2f(0.0f, -25.0f), theme::Gold);
                } else {
                    triggerPlayerPopup(1, "ГРЕШКА", msg, "Печелете злато от доставка на ток към града!", "", theme::Bad);
                    spawnNotice(msg, clickPos + sf::Vector2f(0.0f, -25.0f), theme::Bad);
                }
                return;
            }
        } else {
            ResourceType p2Up = nodes.getP2UpgradeAt(clickPos);
            if (p2Up != ResourceType::NONE) {
                std::string msg;
                if (engine.upgradeMine(2, p2Up, msg)) {
                    spawnMiningParticles(clickPos, theme::Gold, 25);
                    triggerPlayerPopup(2, "НАДГРАЖДАНЕ", msg, "Добивът от тази мина е увеличен с +75%!", "[ENTER]: Добив | [RShift]: Нов ъпгрейд", theme::Gold);
                    spawnNotice(msg, clickPos + sf::Vector2f(0.0f, -25.0f), theme::Gold);
                } else {
                    triggerPlayerPopup(2, "ГРЕШКА", msg, "Печелете злато от доставка на ток към града!", "", theme::Bad);
                    spawnNotice(msg, clickPos + sf::Vector2f(0.0f, -25.0f), theme::Bad);
                }
                return;
            }
        }

        // 5. Click on Resource Stations
        if (owner == 1) {
            ResourceType p1Res = nodes.getP1ResourceAt(clickPos);
            if (p1Res != ResourceType::NONE) {
                if (engine.getSelectedBuilding(1) != BuildingType::NONE) {
                    engine.clearBuildingSelection(1);
                }
                if (p1ResourceCooldown > 0.0f) {
                    char buf[32];
                    std::snprintf(buf, sizeof(buf), "ИЗЧАКАЙТЕ: %.1fs", p1ResourceCooldown);
                    spawnNotice(buf, clickPos + sf::Vector2f(0.0f, -25.0f), theme::Warn);
                    return;
                }
                GameEngine::MineResult res;
                std::string msg;
                if (engine.mineResource(1, p1Res, res, msg)) {
                    p1ResourceCooldown = Balance::MINE_COOLDOWN_SEC;
                    p1Pulse = 1.0f;
                    const auto* st = nodes.getStation(1, p1Res);
                    sf::Color c = st ? st->themeColor : theme::P1;
                    spawnMiningParticles(clickPos, c, 18);
                    triggerPlayerPopup(1, "ДОБИВ", msg, "Ресурсът е добавен в склада.", "[E]: Избери сграда", c);
                    spawnNotice(msg, clickPos + sf::Vector2f(0.0f, -25.0f), c);
                }
                return;
            }
        } else {
            ResourceType p2Res = nodes.getP2ResourceAt(clickPos);
            if (p2Res != ResourceType::NONE) {
                if (engine.getSelectedBuilding(2) != BuildingType::NONE) {
                    engine.clearBuildingSelection(2);
                }
                if (p2ResourceCooldown > 0.0f) {
                    char buf[32];
                    std::snprintf(buf, sizeof(buf), "ИЗЧАКАЙТЕ: %.1fs", p2ResourceCooldown);
                    spawnNotice(buf, clickPos + sf::Vector2f(0.0f, -25.0f), theme::Warn);
                    return;
                }
                GameEngine::MineResult res;
                std::string msg;
                if (engine.mineResource(2, p2Res, res, msg)) {
                    p2ResourceCooldown = Balance::MINE_COOLDOWN_SEC;
                    p2Pulse = 1.0f;
                    const auto* st = nodes.getStation(2, p2Res);
                    sf::Color c = st ? st->themeColor : theme::P2Light;
                    spawnMiningParticles(clickPos, c, 18);
                    triggerPlayerPopup(2, "ДОБИВ", msg, "Ресурсът е добавен в склада.", "[PgDn]: Избери сграда", c);
                    spawnNotice(msg, clickPos + sf::Vector2f(0.0f, -25.0f), c);
                }
                return;
            }
        }
    }
}
