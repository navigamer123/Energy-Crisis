#include "../includes/UI_map.h"
#include <cmath>
#include <iostream>
#include <algorithm>

UI_map::UI_map()
    : resourcesLoaded(false),
      controlScheme(ControlScheme::BOTH_KEYBOARD),
      requestMenu(false),
      p1Clock(1),
      p2Clock(2),
      p1Buildings(1, { 20.0f, 120.0f }, { 220.0f, 360.0f }, sf::Color(0, 229, 255)),
      p2Buildings(2, { 1600.0f - 240.0f, 120.0f }, { 220.0f, 360.0f }, sf::Color(255, 120, 200)),
      p1Pos(450.0f, 450.0f),
      p2Pos(1150.0f, 450.0f),
      p1Pulse(0.0f),
      p2Pulse(0.0f) {
    if (grassTexture.loadFromFile("assets/grass.png")) {
        grassTexture.setRepeated(true);
    } else {
        std::cerr << "[UI_map] Warning: Failed to load assets/grass.png\n";
    }

    if (font.openFromFile("assets/font.ttf")) {
        resourcesLoaded = true;
    } else {
        std::cerr << "[UI_map] Warning: Failed to load assets/font.ttf\n";
    }

    // Initialize backend game engine at 1600x900
    engine.init(1600.0f, 900.0f);
    std::cout << "[UI_map] 1600x900 map orchestrator with backend GameEngine ready.\n";
}

UI_map::~UI_map() {
    std::cout << "[UI_map] SFML Map component destroyed.\n";
}

void UI_map::setControlScheme(ControlScheme scheme) {
    controlScheme = scheme;
    p1Pos = { 450.0f, 450.0f };
    p2Pos = { 1150.0f, 450.0f };
    p1Pulse = 0.0f;
    p2Pulse = 0.0f;
    requestMenu = false;
    std::cout << "[UI_map] Active Control Scheme set to: " << static_cast<int>(scheme) << "\n";
}

void UI_map::spawnNotice(const std::string& text, sf::Vector2f pos, sf::Color color) {
    FloatingNotice n;
    n.text = text;
    n.pos = pos;
    n.timer = 1.8f;
    n.maxTimer = 1.8f;
    n.color = color;
    notices.push_back(n);
}

void UI_map::drawGrassBackground(sf::RenderWindow& window) {
    float screenWidth = static_cast<float>(window.getSize().x);
    float screenHeight = static_cast<float>(window.getSize().y);

    if (grassTexture.getSize().x > 0) {
        sf::Sprite sprite(grassTexture);
        sprite.setTextureRect(sf::IntRect({ 0, 0 }, { (int)screenWidth, (int)screenHeight }));
        sprite.setPosition({ 0.0f, 0.0f });
        window.draw(sprite);
    } else {
        sf::RectangleShape ground({ screenWidth, screenHeight });
        ground.setPosition({ 0.0f, 0.0f });
        ground.setFillColor(sf::Color(60, 115, 40));
        window.draw(ground);
    }

    // Dynamic atmospheric tint based on current hour
    float hour = engine.getHour24();
    sf::RectangleShape skyOverlay({ screenWidth, screenHeight });
    skyOverlay.setPosition({ 0.0f, 0.0f });

    if (hour >= 18.0f && hour <= 21.0f) {
        // Sunset / Dusk warm tint
        float sunsetAlpha = (hour - 18.0f) / 3.0f;
        skyOverlay.setFillColor(sf::Color(180, 80, 20, static_cast<std::uint8_t>(60 * (1.0f - sunsetAlpha))));
        window.draw(skyOverlay);
    } else if (hour > 21.0f || hour < 5.0f) {
        // Nighttime dark blue tint
        skyOverlay.setFillColor(sf::Color(10, 15, 30, 100));
        window.draw(skyOverlay);
    }
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
        sf::Text p2Tag(font, "P2", 13);
        p2Tag.setFillColor(sf::Color(255, 140, 220));
        p2Tag.setPosition({ p2Pos.x - 8.0f, p2Pos.y - 28.0f });
        window.draw(p2Tag);
    }
}

void UI_map::drawFloatingNotices(sf::RenderWindow& window) {
    if (!resourcesLoaded) return;

    for (const auto& n : notices) {
        float alphaFrac = n.timer / n.maxTimer;
        std::uint8_t alpha = static_cast<std::uint8_t>(alphaFrac * 255);
        sf::Text t(font, toUtf8(n.text), 14);
        sf::Color c = n.color;
        c.a = alpha;
        t.setFillColor(c);
        sf::FloatRect b = t.getLocalBounds();
        t.setPosition(sf::Vector2f(n.pos.x - b.size.x / 2.0f, n.pos.y));
        window.draw(t);
    }
}

void UI_map::triggerPlayerPopup(int player, const std::string& badge, const std::string& title,
                                const std::string& detail, const std::string& action, sf::Color accent) {
    PlayerPopup& pop = (player == 1) ? p1Popup : p2Popup;
    pop.badge = badge;
    pop.title = title;
    pop.detail = detail;
    pop.action = action;
    pop.accentColor = accent;
    pop.timer = 4.0f;
    pop.maxTimer = 4.0f;
    pop.active = true;
}

void UI_map::drawPlayerPopups(sf::RenderWindow& window) {
    if (!resourcesLoaded) return;

    auto drawOnePopup = [&](const PlayerPopup& pop, float x, float y) {
        if (!pop.active) return;

        float alphaRatio = std::min(1.0f, pop.timer / 0.8f);
        std::uint8_t alpha = static_cast<std::uint8_t>(alphaRatio * 245);

        // Glassmorphic container box
        sf::RectangleShape box({ 225.0f, 132.0f });
        box.setPosition({ x, y });
        box.setFillColor(sf::Color(16, 22, 34, alpha));
        box.setOutlineThickness(1.5f);
        sf::Color outColor = pop.accentColor;
        outColor.a = alpha;
        box.setOutlineColor(outColor);
        window.draw(box);

        // Badge pill
        sf::RectangleShape badgeBox({ 65.0f, 18.0f });
        badgeBox.setPosition({ x + 8.0f, y + 8.0f });
        sf::Color bColor = pop.accentColor;
        bColor.a = static_cast<std::uint8_t>(alpha * 0.65f);
        badgeBox.setFillColor(bColor);
        window.draw(badgeBox);

        sf::Text tBadge(font, toUtf8(pop.badge), 10);
        tBadge.setFillColor(sf::Color(255, 255, 255, alpha));
        sf::FloatRect bb = tBadge.getLocalBounds();
        tBadge.setPosition({ x + 8.0f + (65.0f - bb.size.x) / 2.0f, y + 9.0f });
        window.draw(tBadge);

        // Title
        sf::Text tTitle(font, toUtf8(pop.title), 12);
        tTitle.setFillColor(sf::Color(255, 255, 255, alpha));
        tTitle.setPosition({ x + 78.0f, y + 9.0f });
        window.draw(tTitle);

        // Separator
        sf::RectangleShape sep({ 209.0f, 1.0f });
        sep.setPosition({ x + 8.0f, y + 31.0f });
        sep.setFillColor(sf::Color(60, 85, 120, alpha));
        window.draw(sep);

        // Detailed Explanation
        sf::Text tDetail(font, toUtf8(pop.detail), 11);
        tDetail.setFillColor(sf::Color(195, 225, 255, alpha));
        tDetail.setPosition({ x + 8.0f, y + 36.0f });
        window.draw(tDetail);

        // Action instructions
        if (!pop.action.empty()) {
            sf::Text tAct(font, toUtf8(pop.action), 10);
            tAct.setFillColor(sf::Color(255, 215, 80, alpha));
            tAct.setPosition({ x + 8.0f, y + 92.0f });
            window.draw(tAct);
        }

        // Timer progress bar at the bottom
        float pWidth = 209.0f * (pop.timer / pop.maxTimer);
        sf::RectangleShape prog({ std::max(0.0f, pWidth), 2.5f });
        prog.setPosition({ x + 8.0f, y + 122.0f });
        prog.setFillColor(outColor);
        window.draw(prog);
    };

    drawOnePopup(p1Popup, 20.0f, 482.0f);
    drawOnePopup(p2Popup, 1600.0f - 245.0f, 482.0f);
}

void UI_map::drawHUD(sf::RenderWindow& window) {
    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    sf::FloatRect menuBtn({ 1600.0f - 130.0f, 900.0f - 34.0f }, { 120.0f, 28.0f });
    bool hover = menuBtn.contains(mousePos);

    sf::RectangleShape mBox(menuBtn.size);
    mBox.setPosition(menuBtn.position);
    mBox.setFillColor(hover ? sf::Color(55, 75, 105) : sf::Color(32, 42, 58));
    mBox.setOutlineThickness(hover ? 1.5f : 1.0f);
    mBox.setOutlineColor(hover ? sf::Color(255, 204, 0) : sf::Color(80, 110, 150));
    window.draw(mBox);

    if (resourcesLoaded) {
        sf::Text mt(font, toUtf8("ESC / МЕНЮ"), 13);
        mt.setFillColor(hover ? sf::Color(255, 240, 150) : sf::Color::White);
        sf::FloatRect mb = mt.getLocalBounds();
        mt.setPosition({ menuBtn.position.x + (menuBtn.size.x - mb.size.x) / 2.0f, menuBtn.position.y + 4.0f });
        window.draw(mt);

        // Persistent Controls Reminder Bar
        sf::RectangleShape helpBar({ 1100.0f, 26.0f });
        helpBar.setPosition({ 250.0f, 900.0f - 30.0f });
        helpBar.setFillColor(sf::Color(15, 20, 30, 220));
        helpBar.setOutlineThickness(1.0f);
        helpBar.setOutlineColor(sf::Color(60, 85, 120));
        window.draw(helpBar);

        std::string helpText = "P1: [E] Избери какво да правиш  |  [Q] Кенселирай / Отказ  |  [SPACE/КЛИК] Действие/Строеж  ///  P2: [PgDn] Избери  |  [PgUp] Кенселирай  |  [ENTER] Действие";
        sf::Text ht(font, toUtf8(helpText), 11);
        ht.setFillColor(sf::Color(210, 230, 255));
        sf::FloatRect htb = ht.getLocalBounds();
        ht.setPosition({ 250.0f + (1100.0f - htb.size.x) / 2.0f, 900.0f - 26.0f });
        window.draw(ht);
    }
}

void UI_map::updateControls(const sf::RenderWindow& window, float dt) {
    float speed = 360.0f;
    sf::Vector2f mPos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    // Player 1 controls
    if (controlScheme == ControlScheme::BOTH_KEYBOARD || controlScheme == ControlScheme::P1_KEYBOARD_P2_MOUSE) {
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) p1Pos.y -= speed * dt;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) p1Pos.y += speed * dt;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) p1Pos.x -= speed * dt;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) p1Pos.x += speed * dt;
    } else if (controlScheme == ControlScheme::P1_MOUSE_P2_KEYBOARD) {
        p1Pos = mPos;
    } else if (controlScheme == ControlScheme::BOTH_MOUSE) {
        if (mPos.x <= 800.0f) {
            p1Pos = mPos;
        }
    }
    p1Pos.x = std::max(30.0f, std::min(p1Pos.x, 780.0f));
    p1Pos.y = std::max(40.0f, std::min(p1Pos.y, 860.0f));

    // Player 2 controls
    if (controlScheme == ControlScheme::BOTH_KEYBOARD || controlScheme == ControlScheme::P1_MOUSE_P2_KEYBOARD) {
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up)) p2Pos.y -= speed * dt;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down)) p2Pos.y += speed * dt;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)) p2Pos.x -= speed * dt;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) p2Pos.x += speed * dt;
    } else if (controlScheme == ControlScheme::P1_KEYBOARD_P2_MOUSE) {
        p2Pos = mPos;
    } else if (controlScheme == ControlScheme::BOTH_MOUSE) {
        if (mPos.x >= 800.0f) {
            p2Pos = mPos;
        }
    }
    p2Pos.x = std::max(820.0f, std::min(p2Pos.x, 1570.0f));
    p2Pos.y = std::max(40.0f, std::min(p2Pos.y, 860.0f));

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
    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        if (key->code == sf::Keyboard::Key::Escape || key->code == sf::Keyboard::Key::M) {
            requestMenu = true;
        }

        // =====================================================================
        // PLAYER 1 ACTIONS (WEST)
        // =====================================================================

        // [E]: Choose what to do (Cycle buildings: Solar -> Wind -> Hydro -> Battery)
        if (key->code == sf::Keyboard::Key::E) {
            engine.cycleBuildingSelection(1);
            BuildingType newSel = engine.getSelectedBuilding(1);
            BuildingCost c = engine.getBuildingCost(newSel);
            triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg,
                               "Нужно: " + std::to_string(c.woodCost) + " Дърво, " + std::to_string(c.oreCost) + " Руда.\nДобив: +" + std::to_string(c.basePowerMW) + " MW ток.",
                               "[SPACE/КЛИК]: Постави | [E]: Смени | [Q]: Отказ", sf::Color(0, 229, 255));
        }

        // [Q]: CANCEL / КЕНСЕЛИРАЙ
        if (key->code == sf::Keyboard::Key::Q) {
            if (engine.getSelectedBuilding(1) != BuildingType::NONE) {
                engine.clearBuildingSelection(1);
                triggerPlayerPopup(1, "ОТКАЗ", "Отменен строеж", "Режимът за поставяне е прекратен.", "[E]: Избери нова сграда", sf::Color(180, 180, 180));
            } else {
                triggerPlayerPopup(1, "ИНФО", "Свободен режим", "Няма активен избор на сграда.", "[E]: Избери сграда", sf::Color(180, 180, 180));
            }
        }

        // Direct Hotkeys 1, 2, 3, 4 for P1
        if (key->code == sf::Keyboard::Key::Num1) {
            engine.getPlayerEconomyMut(1).selectedBuilding = 1;
            BuildingCost c = engine.getBuildingCost(BuildingType::SOLAR_PANEL);
            triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg, "Нужно: 35 Дърво, 30 Руда.\nДобив: +25 MW при слънце.", "[SPACE/КЛИК]: Постави | [Q]: Отказ", sf::Color(0, 229, 255));
        } else if (key->code == sf::Keyboard::Key::Num2) {
            engine.getPlayerEconomyMut(1).selectedBuilding = 2;
            BuildingCost c = engine.getBuildingCost(BuildingType::WIND_TURBINE);
            triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg, "Нужно: 50 Дърво, 45 Руда.\nДобив: +45 MW при вятър.", "[SPACE/КЛИК]: Постави | [Q]: Отказ", sf::Color(0, 229, 255));
        } else if (key->code == sf::Keyboard::Key::Num3) {
            engine.getPlayerEconomyMut(1).selectedBuilding = 3;
            BuildingCost c = engine.getBuildingCost(BuildingType::HYDRO_PLANT);
            triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg, "Нужно: 85 Дърво, 90 Руда.\nДобив: +90 MW при дъжд.", "[SPACE/КЛИК]: Постави | [Q]: Отказ", sf::Color(0, 229, 255));
        } else if (key->code == sf::Keyboard::Key::Num4) {
            engine.getPlayerEconomyMut(1).selectedBuilding = 4;
            BuildingCost c = engine.getBuildingCost(BuildingType::BATTERY);
            triggerPlayerPopup(1, "СТРОЕЖ", c.nameBg, "Нужно: 30 Дърво, 60 Руда.\nРезерв: +40 MW през нощта.", "[SPACE/КЛИК]: Постави | [Q]: Отказ", sf::Color(0, 229, 255));
        }

        // [Space] or [F]: Confirm / Place Building / Buy Land / Mine Resource
        if (key->code == sf::Keyboard::Key::Space || key->code == sf::Keyboard::Key::F) {
            BuildingType sel = engine.getSelectedBuilding(1);
            if (sel != BuildingType::NONE) {
                // If over an unpurchased plot, allow buying it first
                for (const auto& plot : engine.getLandPlots()) {
                    if (plot.playerOwner == 1 && plot.bounds.contains(p1Pos)) {
                        if (!plot.isPurchased) {
                            std::string buyMsg;
                            if (engine.buyLandPlot(1, plot.id, buyMsg)) {
                                triggerPlayerPopup(1, "ЗЕМЯ", "Купихте парцел!", "Парцелът е ваш. Натиснете пак SPACE за строеж.", "[SPACE]: Постави сградата", sf::Color(255, 215, 0));
                            } else {
                                triggerPlayerPopup(1, "ГРЕШКА", "Няма злато за земя!", buyMsg + "\nПродавайте ток на града за злато.", "[Q]: Отказ", sf::Color(255, 90, 90));
                            }
                            return;
                        }
                        break;
                    }
                }
                std::string msg;
                if (engine.placeBuilding(1, sel, p1Pos, msg)) {
                    p1Pulse = 1.0f;
                    triggerPlayerPopup(1, "УСПЕХ", "Построена сграда!", msg + "\nГенераторът захранва града!", "[E]: Следващ строеж", sf::Color(0, 255, 180));
                    engine.clearBuildingSelection(1);
                } else {
                    triggerPlayerPopup(1, "ГРЕШКА", "Не може да се построи!", msg, "[Q]: Отказ | Добийте ресурси", sf::Color(255, 90, 90));
                }
            } else {
                // No building selected: check if near mine, forest, or unpurchased plot
                if (nodes.isNearP1Mine(p1Pos)) {
                    std::string msg;
                    engine.mineResource(1, ResourceType::ORE, msg);
                    p1Pulse = 1.0f;
                    triggerPlayerPopup(1, "ДОБИВ", "+12 Руда (Ore)", "Ресурсът е добавен към вашия запас.\nИзползва се за турбини и сгради.", "[E]: Избери сграда", sf::Color(0, 229, 255));
                } else if (nodes.isNearP1Forest(p1Pos)) {
                    std::string msg;
                    engine.mineResource(1, ResourceType::WOOD, msg);
                    p1Pulse = 1.0f;
                    triggerPlayerPopup(1, "ДОБИВ", "+15 Дърво (Wood)", "Ресурсът е добавен към вашия запас.\nИзползва се за конструкции.", "[E]: Избери сграда", sf::Color(100, 255, 140));
                } else {
                    for (const auto& plot : engine.getLandPlots()) {
                        if (plot.playerOwner == 1 && plot.bounds.contains(p1Pos)) {
                            if (!plot.isPurchased) {
                                std::string msg;
                                if (engine.buyLandPlot(1, plot.id, msg)) {
                                    triggerPlayerPopup(1, "ЗЕМЯ", "Закупен парцел!", "Парцелът е ваш. Натиснете E за избор на сграда.", "[E]: Избери сграда", sf::Color(255, 215, 0));
                                } else {
                                    triggerPlayerPopup(1, "ГРЕШКА", "Няма злато!", msg, "[Q]: Отказ", sf::Color(255, 90, 90));
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

        // =====================================================================
        // PLAYER 2 ACTIONS (EAST)
        // =====================================================================

        // [PgDn]: Choose what to do (Cycle buildings)
        if (key->code == sf::Keyboard::Key::PageDown) {
            engine.cycleBuildingSelection(2);
            BuildingType newSel = engine.getSelectedBuilding(2);
            BuildingCost c = engine.getBuildingCost(newSel);
            triggerPlayerPopup(2, "СТРОЕЖ", c.nameBg,
                               "Нужно: " + std::to_string(c.woodCost) + " Дърво, " + std::to_string(c.oreCost) + " Руда.\nДобив: +" + std::to_string(c.basePowerMW) + " MW ток.",
                               "[ENTER/КЛИК]: Постави | [PgDn]: Смени | [PgUp]: Отказ", sf::Color(255, 120, 200));
        }

        // [PgUp]: CANCEL / КЕНСЕЛИРАЙ
        if (key->code == sf::Keyboard::Key::PageUp) {
            if (engine.getSelectedBuilding(2) != BuildingType::NONE) {
                engine.clearBuildingSelection(2);
                triggerPlayerPopup(2, "ОТКАЗ", "Отменен строеж", "Режимът за поставяне е прекратен.", "[PgDn]: Избери нова сграда", sf::Color(180, 180, 180));
            } else {
                triggerPlayerPopup(2, "ИНФО", "Свободен режим", "Няма активен избор на сграда.", "[PgDn]: Избери сграда", sf::Color(180, 180, 180));
            }
        }

        // [Enter] or [Num0] or [RCtrl]: Confirm / Place / Buy / Mine
        if (key->code == sf::Keyboard::Key::Enter || key->code == sf::Keyboard::Key::Numpad0 || key->code == sf::Keyboard::Key::RControl) {
            BuildingType sel = engine.getSelectedBuilding(2);
            if (sel != BuildingType::NONE) {
                for (const auto& plot : engine.getLandPlots()) {
                    if (plot.playerOwner == 2 && plot.bounds.contains(p2Pos)) {
                        if (!plot.isPurchased) {
                            std::string buyMsg;
                            if (engine.buyLandPlot(2, plot.id, buyMsg)) {
                                triggerPlayerPopup(2, "ЗЕМЯ", "Купихте парцел!", "Парцелът е ваш. Натиснете пак ENTER за строеж.", "[ENTER]: Постави сградата", sf::Color(255, 215, 0));
                            } else {
                                triggerPlayerPopup(2, "ГРЕШКА", "Няма злато за земя!", buyMsg, "[PgUp]: Отказ", sf::Color(255, 90, 90));
                            }
                            return;
                        }
                        break;
                    }
                }
                std::string msg;
                if (engine.placeBuilding(2, sel, p2Pos, msg)) {
                    p2Pulse = 1.0f;
                    triggerPlayerPopup(2, "УСПЕХ", "Построена сграда!", msg + "\nГенераторът захранва града!", "[PgDn]: Следващ строеж", sf::Color(255, 120, 200));
                    engine.clearBuildingSelection(2);
                } else {
                    triggerPlayerPopup(2, "ГРЕШКА", "Не може да се построи!", msg, "[PgUp]: Отказ | Добийте ресурси", sf::Color(255, 90, 90));
                }
            } else {
                if (nodes.isNearP2Mine(p2Pos)) {
                    std::string msg;
                    engine.mineResource(2, ResourceType::ORE, msg);
                    p2Pulse = 1.0f;
                    triggerPlayerPopup(2, "ДОБИВ", "+12 Руда (Ore)", "Ресурсът е добавен към вашия запас.\nИзползва се за турбини и сгради.", "[PgDn]: Избери сграда", sf::Color(255, 140, 210));
                } else if (nodes.isNearP2Forest(p2Pos)) {
                    std::string msg;
                    engine.mineResource(2, ResourceType::WOOD, msg);
                    p2Pulse = 1.0f;
                    triggerPlayerPopup(2, "ДОБИВ", "+15 Дърво (Wood)", "Ресурсът е добавен към вашия запас.\nИзползва се за конструкции.", "[PgDn]: Избери сграда", sf::Color(255, 204, 100));
                } else {
                    for (const auto& plot : engine.getLandPlots()) {
                        if (plot.playerOwner == 2 && plot.bounds.contains(p2Pos)) {
                            if (!plot.isPurchased) {
                                std::string msg;
                                if (engine.buyLandPlot(2, plot.id, msg)) {
                                    triggerPlayerPopup(2, "ЗЕМЯ", "Закупен парцел!", "Парцелът е ваш. Натиснете PgDn за избор.", "[PgDn]: Избери сграда", sf::Color(255, 215, 0));
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
            requestMenu = true;
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

        BuildingType clickedP2 = p2Buildings.handleClick(clickPos);
        if (clickedP2 != BuildingType::NONE) {
            engine.getPlayerEconomyMut(2).selectedBuilding = static_cast<int>(clickedP2);
            BuildingCost c = engine.getBuildingCost(clickedP2);
            triggerPlayerPopup(2, "СТРОЕЖ", c.nameBg,
                               "Нужно: " + std::to_string(c.woodCost) + " Дърво, " + std::to_string(c.oreCost) + " Руда.\nДобив: +" + std::to_string(c.basePowerMW) + " MW.",
                               "[КЛИК НА ЗЕМЯ]: Постави | [ДЕСЕН КЛИК]: Отказ", sf::Color(255, 120, 200));
            return;
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
        if (resourceHUD.getP2BuyLandButton().contains(clickPos)) {
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
                if (!plot.isPurchased) {
                    std::string msg;
                    if (engine.buyLandPlot(owner, plot.id, msg)) {
                        triggerPlayerPopup(owner, "ЗЕМЯ", "Купихте парцел!", msg + "\nВече можете да строите тук.", "[КЛИК]: Постави сграда", sf::Color(255, 215, 0));
                    } else {
                        triggerPlayerPopup(owner, "ГРЕШКА", "Няма злато!", msg, "[Q]: Отказ", sf::Color(255, 90, 90));
                    }
                    return;
                } else {
                    BuildingType sel = engine.getSelectedBuilding(owner);
                    if (sel != BuildingType::NONE) {
                        std::string msg;
                        if (engine.placeBuilding(owner, sel, clickPos, msg)) {
                            triggerPlayerPopup(owner, "УСПЕХ", "Построена сграда!", msg + "\nГенераторът захранва града!", "", sf::Color(0, 255, 180));
                            engine.clearBuildingSelection(owner);
                        } else {
                            triggerPlayerPopup(owner, "ГРЕШКА", "Не може да се построи!", msg, "[Q]: Отказ", sf::Color(255, 90, 90));
                        }
                        return;
                    }
                }
            }
        }

        // 4. Click on Resource Mines & Forests
        if (nodes.isNearP1Forest(clickPos)) {
            std::string msg;
            engine.mineResource(1, ResourceType::WOOD, msg);
            p1Pulse = 1.0f;
            triggerPlayerPopup(1, "ДОБИВ", "+15 Дърво (Wood)", "Дървесината е добавена за строеж.", "[E]: Избери сграда", sf::Color(100, 255, 140));
        } else if (nodes.isNearP1Mine(clickPos)) {
            std::string msg;
            engine.mineResource(1, ResourceType::ORE, msg);
            p1Pulse = 1.0f;
            triggerPlayerPopup(1, "ДОБИВ", "+12 Руда (Ore)", "Рудата е добавена за генератори.", "[E]: Избери сграда", sf::Color(0, 229, 255));
        } else if (nodes.isNearP2Mine(clickPos)) {
            std::string msg;
            engine.mineResource(2, ResourceType::ORE, msg);
            p2Pulse = 1.0f;
            triggerPlayerPopup(2, "ДОБИВ", "+12 Руда (Ore)", "Рудата е добавена за генератори.", "[PgDn]: Избери сграда", sf::Color(255, 140, 210));
        } else if (nodes.isNearP2Forest(clickPos)) {
            std::string msg;
            engine.mineResource(2, ResourceType::WOOD, msg);
            p2Pulse = 1.0f;
            triggerPlayerPopup(2, "ДОБИВ", "+15 Дърво (Wood)", "Дървесината е добавена за строеж.", "[PgDn]: Избери сграда", sf::Color(255, 204, 100));
        }
    }
}

void UI_map::render(sf::RenderWindow& window) {
    float dt = deltaClock.restart().asSeconds();
    if (dt > 0.05f) dt = 0.05f;

    // 1. Advance continuous backend simulation
    engine.update(dt);
    updateControls(window, dt);

    // 2. Synchronize clock displays with continuous time and dynamic weather
    p1Clock.setHour(engine.getHour24());
    p1Clock.setDay(engine.getCurrentDay());
    p1Clock.setWeather(engine.getPlayerWeather(1));
    p1Clock.setSeason(engine.getSeason());

    p2Clock.setHour(engine.getHour24());
    p2Clock.setDay(engine.getCurrentDay());
    p2Clock.setWeather(engine.getPlayerWeather(2));
    p2Clock.setSeason(engine.getSeason());

    float animTime = animClock.getElapsedTime().asSeconds();
    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    // 3. Render terrain & atmosphere
    drawGrassBackground(window);

    // 4. Central dividing line & river
    city.drawDividingRiver(window, font, resourcesLoaded, animTime);

    // 5. Purchasable Land Plots Grid
    nodes.drawLandPlots(window, font, resourcesLoaded, engine.getLandPlots(), mousePos);

    // 6. Placed Buildings on the Map
    nodes.drawPlacedBuildings(window, font, resourcesLoaded, engine.getBuildings());

    // 7. Holographic ghost preview if building is selected
    BuildingType p1Sel = engine.getSelectedBuilding(1);
    if (p1Sel != BuildingType::NONE) {
        std::string reason;
        bool valid = engine.canPlaceBuilding(1, p1Sel, p1Pos, reason);
        nodes.drawBuildingGhost(window, font, resourcesLoaded, p1Sel, p1Pos, valid, engine.getBuildingCost(p1Sel));
    }
    BuildingType p2Sel = engine.getSelectedBuilding(2);
    if (p2Sel != BuildingType::NONE) {
        std::string reason;
        bool valid = engine.canPlaceBuilding(2, p2Sel, p2Pos, reason);
        nodes.drawBuildingGhost(window, font, resourcesLoaded, p2Sel, p2Pos, valid, engine.getBuildingCost(p2Sel));
    }

    // 8. Compact Metropolis City Center with territorial slicing & conquest
    city.drawCity(window, font, resourcesLoaded, animTime, engine.getCityState().p1CityShare, engine.getCityState().lastCutMessage);

    // 9. City Demand & Influence Tug-of-War Bar (Above City)
    city.drawInfluenceBar(window, font, resourcesLoaded, engine.getCityState().cityEnergyDemand,
                          engine.getPlayerEconomy(1).energyMW, engine.getPlayerEconomy(2).energyMW,
                          engine.getCityState().p1CityShare);

    // 10. Resource Mines & Timber Forests
    nodes.drawNodes(window, font, resourcesLoaded);

    // 11. Top-Left & Top-Right Clocks (Continuous 24h cycle & weather)
    p1Clock.draw(window, font, resourcesLoaded, { 20.0f, 10.0f }, { 230.0f, 100.0f }, sf::Color(0, 229, 255));
    p2Clock.draw(window, font, resourcesLoaded, { 1600.0f - 250.0f, 10.0f }, { 230.0f, 100.0f }, sf::Color(255, 120, 200));

    // 12. Left & Right Building Menus
    p1Buildings.draw(window, font, resourcesLoaded, mousePos, engine.getPlayerEconomy(1), p1Sel);
    p2Buildings.draw(window, font, resourcesLoaded, mousePos, engine.getPlayerEconomy(2), p2Sel);

    // 13. Bottom Corner Quarter-Circles (Pure icons and numbers, gold at bottom)
    resourceHUD.drawQuarterCircle(window, font, resourcesLoaded, engine.getPlayerEconomy(1), true);
    resourceHUD.drawQuarterCircle(window, font, resourcesLoaded, engine.getPlayerEconomy(2), false);

    // 14. Player Side Popups (rendered on player's sector)
    drawPlayerPopups(window);

    // 15. Menu button & persistent HUD
    drawHUD(window);

    // 16. Player targeting cursors (RENDERED ON TOP OF QUARTER CIRCLES AND ALL HUD!)
    drawPlayerCursors(window);

    // 17. Floating Notices
    drawFloatingNotices(window);
}
