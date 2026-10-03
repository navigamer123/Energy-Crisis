#include "../includes/UI_tutorial.h"
#include "../includes/UI_input.h" // Team b-session (F-10): key hints from the real bindings
#include "../includes/UI_settings.h"
#include <cmath>
#include <algorithm>
#include <iostream>

UI_tutorial::UI_tutorial()
    : step(TutorialStep::WELCOME),
      active(true),
      isCoop(false),
      animTimer(0.0f),
      stepDelayTimer(0.0f),
      initialP1BuildingCount(0) {
    cardBounds = sf::FloatRect({ 240.0f, 770.0f }, { 540.0f, 118.0f });
    float skipW = cardBounds.size.x * 0.26f;
    float skipH = cardBounds.size.y * 0.20f;
    skipBtnBounds = sf::FloatRect({ cardBounds.position.x + cardBounds.size.x - skipW - cardBounds.size.x * 0.02f,
                                   cardBounds.position.y + cardBounds.size.y * 0.07f },
                                 { skipW, skipH });
    float nextW = cardBounds.size.x * 0.34f;
    float nextH = cardBounds.size.y * 0.22f;
    nextBtnBounds = sf::FloatRect({ cardBounds.position.x + cardBounds.size.x - nextW - cardBounds.size.x * 0.02f,
                                   cardBounds.position.y + cardBounds.size.y - nextH - cardBounds.size.y * 0.07f },
                                 { nextW, nextH });
}

void UI_tutorial::reset() {
    step = TutorialStep::WELCOME;
    active = true;
    animTimer = 0.0f;
    stepDelayTimer = 0.0f;
    initialP1BuildingCount = 0;
}

void UI_tutorial::start() {
    step = TutorialStep::WELCOME;
    active = true;
    stepDelayTimer = 0.0f;
    initialP1BuildingCount = 0;
}

void UI_tutorial::skip() {
    step = TutorialStep::INACTIVE;
    active = false;
}

// Number of solar panels Player 1 currently owns (tutorial PLACE_SOLAR progress)
static int countP1SolarPanels(const GameEngine& engine) {
    int count = 0;
    for (const auto& b : engine.getBuildings()) {
        if (b.playerOwner == 1 && b.type == BuildingType::SOLAR_PANEL) count++;
    }
    return count;
}

void UI_tutorial::update(float dt, const GameEngine& engine) {
    if (!active || step == TutorialStep::INACTIVE) return;
    animTimer += dt;

    const auto& econ = engine.getPlayerEconomy(1);

    if (stepDelayTimer > 0.0f) {
        stepDelayTimer -= dt;
        if (stepDelayTimer <= 0.0f) {
            if (step == TutorialStep::GATHER_WOOD) step = TutorialStep::GATHER_IRON;
            else if (step == TutorialStep::GATHER_IRON) step = TutorialStep::GATHER_COPPER;
            else if (step == TutorialStep::GATHER_COPPER) step = TutorialStep::GATHER_SILICON;
            else if (step == TutorialStep::GATHER_SILICON) step = TutorialStep::SELECT_SOLAR;
        }
        return;
    }

    // Step automatic progression
    switch (step) {
        case TutorialStep::GATHER_WOOD:
            if (econ.wood >= Balance::SOLAR_PANEL.woodCost) {
                stepDelayTimer = 0.35f;
            }
            break;
        case TutorialStep::GATHER_IRON:
            if (econ.iron >= Balance::SOLAR_PANEL.ironCost) {
                stepDelayTimer = 0.35f;
            }
            break;
        case TutorialStep::GATHER_COPPER:
            if (econ.copper >= Balance::SOLAR_PANEL.copperCost) {
                stepDelayTimer = 0.35f;
            }
            break;
        case TutorialStep::GATHER_SILICON:
            if (econ.silicon >= Balance::SOLAR_PANEL.siliconCost) {
                stepDelayTimer = 0.35f;
            }
            break;
        case TutorialStep::SELECT_SOLAR:
            if (engine.getSelectedBuilding(1) == BuildingType::SOLAR_PANEL) {
                // Baseline: P1's solar panels that already exist, so only a NEW solar completes the step
                initialP1BuildingCount = countP1SolarPanels(engine);
                step = TutorialStep::PLACE_SOLAR;
            }
            break;
        case TutorialStep::PLACE_SOLAR: {
            int currentP1Solars = countP1SolarPanels(engine);
            if (currentP1Solars > initialP1BuildingCount) {
                step = TutorialStep::COMPLETED;
            } else if (currentP1Solars < initialP1BuildingCount) {
                initialP1BuildingCount = currentP1Solars; // a solar was demolished / destroyed meanwhile
            }
            break;
        }
        default:
            break;
    }
}

void UI_tutorial::drawSpotlight(sf::RenderWindow& window, sf::FloatRect targetRect, float animTime) {
    float sw = 1600.0f;
    float sh = 900.0f;
    sf::Color dimColor(0, 0, 0, 195);

    // 1. Top rect
    if (targetRect.position.y > 0.0f) {
        sf::RectangleShape top({ sw, targetRect.position.y });
        top.setPosition({ 0.0f, 0.0f });
        top.setFillColor(dimColor);
        window.draw(top);
    }

    // 2. Bottom rect
    float bY = targetRect.position.y + targetRect.size.y;
    if (bY < sh) {
        sf::RectangleShape bot({ sw, sh - bY });
        bot.setPosition({ 0.0f, bY });
        bot.setFillColor(dimColor);
        window.draw(bot);
    }

    // 3. Left rect
    if (targetRect.position.x > 0.0f) {
        sf::RectangleShape left({ targetRect.position.x, targetRect.size.y });
        left.setPosition({ 0.0f, targetRect.position.y });
        left.setFillColor(dimColor);
        window.draw(left);
    }

    // 4. Right rect
    float rX = targetRect.position.x + targetRect.size.x;
    if (rX < sw) {
        sf::RectangleShape right({ sw - rX, targetRect.size.y });
        right.setPosition({ rX, targetRect.position.y });
        right.setFillColor(dimColor);
        window.draw(right);
    }

    // Glowing Neon Highlight Border around target
    sf::RectangleShape border(targetRect.size);
    border.setPosition(targetRect.position);
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineThickness(2.5f);
    // Pulse computed in float and clamped before the single cast (a negative float -> uint8_t cast is UB)
    std::uint8_t borderAlpha = static_cast<std::uint8_t>(std::clamp(210.0f + std::sin(animTime * 6.0f) * 45.0f, 0.0f, 255.0f));
    border.setOutlineColor(sf::Color(0, 255, 200, borderAlpha));
    window.draw(border);

    // Corner bracket accents
    float cornerLen = 14.0f;
    float ct = 2.5f;
    sf::Color cColor(255, 215, 0);

    // Top-left
    sf::RectangleShape tlH({ cornerLen, ct }); tlH.setPosition(targetRect.position + sf::Vector2f(-2.0f, -2.0f)); tlH.setFillColor(cColor); window.draw(tlH);
    sf::RectangleShape tlV({ ct, cornerLen }); tlV.setPosition(targetRect.position + sf::Vector2f(-2.0f, -2.0f)); tlV.setFillColor(cColor); window.draw(tlV);

    // Top-right
    sf::RectangleShape trH({ cornerLen, ct }); trH.setPosition({ targetRect.position.x + targetRect.size.x - cornerLen + 2.0f, targetRect.position.y - 2.0f }); trH.setFillColor(cColor); window.draw(trH);
    sf::RectangleShape trV({ ct, cornerLen }); trV.setPosition({ targetRect.position.x + targetRect.size.x - 1.0f, targetRect.position.y - 2.0f }); trV.setFillColor(cColor); window.draw(trV);

    // Bottom-left
    sf::RectangleShape blH({ cornerLen, ct }); blH.setPosition({ targetRect.position.x - 2.0f, targetRect.position.y + targetRect.size.y - 1.0f }); blH.setFillColor(cColor); window.draw(blH);
    sf::RectangleShape blV({ ct, cornerLen }); blV.setPosition({ targetRect.position.x - 2.0f, targetRect.position.y + targetRect.size.y - cornerLen + 2.0f }); blV.setFillColor(cColor); window.draw(blV);

    // Bottom-right
    sf::RectangleShape brH({ cornerLen, ct }); brH.setPosition({ targetRect.position.x + targetRect.size.x - cornerLen + 2.0f, targetRect.position.y + targetRect.size.y - 1.0f }); brH.setFillColor(cColor); window.draw(brH);
    sf::RectangleShape brV({ ct, cornerLen }); brV.setPosition({ targetRect.position.x + targetRect.size.x - 1.0f, targetRect.position.y + targetRect.size.y - cornerLen + 2.0f }); brV.setFillColor(cColor); window.draw(brV);
}

void UI_tutorial::drawArrow(sf::RenderWindow& window, sf::Vector2f targetPos, const std::string& label,
                            const sf::Font& font, float animTime, ArrowDir dir) {
    float bounce = std::sin(animTime * 6.0f) * 5.0f;

    // Glowing target ring around the objective
    float pulseRadius = 24.0f + std::sin(animTime * 4.0f) * 4.0f;
    sf::CircleShape pulseCircle(pulseRadius);
    pulseCircle.setOrigin({ pulseRadius, pulseRadius });
    pulseCircle.setPosition(targetPos);
    pulseCircle.setFillColor(sf::Color(0, 229, 255, 35));
    pulseCircle.setOutlineThickness(2.0f);
    pulseCircle.setOutlineColor(sf::Color(0, 255, 200, 200));
    window.draw(pulseCircle);

    // Chevron / Arrow Triangle
    sf::ConvexShape arrow(3);
    sf::Vector2f tip;
    if (dir == ArrowDir::DOWN) {
        tip = sf::Vector2f(targetPos.x, targetPos.y - 12.0f + bounce);
        arrow.setPoint(0, tip);
        arrow.setPoint(1, sf::Vector2f(tip.x - 14.0f, tip.y - 22.0f));
        arrow.setPoint(2, sf::Vector2f(tip.x + 14.0f, tip.y - 22.0f));
    } else if (dir == ArrowDir::UP) {
        tip = sf::Vector2f(targetPos.x, targetPos.y + 12.0f + bounce);
        arrow.setPoint(0, tip);
        arrow.setPoint(1, sf::Vector2f(tip.x - 14.0f, tip.y + 22.0f));
        arrow.setPoint(2, sf::Vector2f(tip.x + 14.0f, tip.y + 22.0f));
    } else if (dir == ArrowDir::LEFT) {
        tip = sf::Vector2f(targetPos.x + 10.0f + bounce, targetPos.y);
        arrow.setPoint(0, tip);
        arrow.setPoint(1, sf::Vector2f(tip.x + 22.0f, tip.y - 13.0f));
        arrow.setPoint(2, sf::Vector2f(tip.x + 22.0f, tip.y + 13.0f));
    } else { // ArrowDir::RIGHT
        tip = sf::Vector2f(targetPos.x - 10.0f + bounce, targetPos.y);
        arrow.setPoint(0, tip);
        arrow.setPoint(1, sf::Vector2f(tip.x - 22.0f, tip.y - 13.0f));
        arrow.setPoint(2, sf::Vector2f(tip.x - 22.0f, tip.y + 13.0f));
    }
    arrow.setFillColor(sf::Color(255, 215, 0, 240));
    arrow.setOutlineThickness(2.0f);
    arrow.setOutlineColor(sf::Color::White);
    window.draw(arrow);

    // Floating text tag over/next to arrow
    if (!label.empty()) {
        sf::Text text(font, toUtf8(label), 12);
        text.setFillColor(sf::Color::White);
        sf::FloatRect tb = text.getLocalBounds();

        float tagX = targetPos.x;
        float tagY = targetPos.y;
        if (dir == ArrowDir::DOWN) {
            tagY = targetPos.y - 56.0f + bounce;
        } else if (dir == ArrowDir::UP) {
            tagY = targetPos.y + 40.0f + bounce;
        } else if (dir == ArrowDir::LEFT) {
            tagX = tip.x + 22.0f + 6.0f + (tb.size.x + 16.0f) / 2.0f;
            tagY = targetPos.y;
        } else { // RIGHT
            tagX = tip.x - 22.0f - 6.0f - (tb.size.x + 16.0f) / 2.0f;
            tagY = targetPos.y;
        }

        sf::RectangleShape tagBg({ tb.size.x + 16.0f, 22.0f });
        tagBg.setOrigin({ (tb.size.x + 16.0f) / 2.0f, 11.0f });
        tagBg.setPosition({ tagX, tagY });
        tagBg.setFillColor(sf::Color(10, 16, 26, 240));
        tagBg.setOutlineThickness(1.5f);
        tagBg.setOutlineColor(sf::Color(255, 215, 0, 220));
        window.draw(tagBg);

        text.setOrigin({ tb.size.x / 2.0f, tb.size.y / 2.0f });
        text.setPosition({ tagX, tagY - 2.0f });
        window.draw(text);
    }
}

void UI_tutorial::draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                       const GameEngine& engine, const UI_resourceNodes& nodes, float animTime,
                       sf::Vector2f mousePos, sf::Vector2f p1Pos, sf::Vector2f p2Pos) {
    if (!active || step == TutorialStep::INACTIVE || !fontLoaded) return;

    const auto& econ = engine.getPlayerEconomy(1);

    // -------------------------------------------------------------------------
    // 1. Draw Spotlight Darkness + Focus Hole around current Objective
    // -------------------------------------------------------------------------
    sf::FloatRect spotlightRect;
    sf::Vector2f arrowTarget;
    std::string arrowLabel = "";

    switch (step) {
        case TutorialStep::WELCOME: {
            // Soft overall dimming for introduction
            sf::RectangleShape softDim({ 1600.0f, 900.0f });
            softDim.setFillColor(sf::Color(0, 0, 0, 140));
            window.draw(softDim);
            break;
        }
        case TutorialStep::GATHER_WOOD: {
            const auto* st = nodes.getStation(1, ResourceType::WOOD);
            if (st) {
                spotlightRect = sf::FloatRect({ st->bounds.position.x - 5.0f, st->bounds.position.y - 5.0f },
                                             { st->bounds.size.x + 10.0f, st->bounds.size.y + 10.0f });
                drawSpotlight(window, spotlightRect, animTime);
                arrowTarget = sf::Vector2f(st->bounds.position.x + st->bounds.size.x / 2.0f, st->bounds.position.y + 35.0f);
                arrowLabel = "СТАНЦИЯ ГОРА " + inputRouter().hint(1, InputAction::Action, false, 1);
                drawArrow(window, arrowTarget, arrowLabel, font, animTime, ArrowDir::DOWN);
            }
            break;
        }
        case TutorialStep::GATHER_IRON: {
            const auto* st = nodes.getStation(1, ResourceType::IRON);
            if (st) {
                spotlightRect = sf::FloatRect({ st->bounds.position.x - 5.0f, st->bounds.position.y - 5.0f },
                                             { st->bounds.size.x + 10.0f, st->bounds.size.y + 10.0f });
                drawSpotlight(window, spotlightRect, animTime);
                arrowTarget = sf::Vector2f(st->bounds.position.x + st->bounds.size.x / 2.0f, st->bounds.position.y + 35.0f);
                arrowLabel = "ДОБИВ: ЖЕЛЯЗО " + inputRouter().hint(1, InputAction::Action, false, 1);
                drawArrow(window, arrowTarget, arrowLabel, font, animTime, ArrowDir::DOWN);
            }
            break;
        }
        case TutorialStep::GATHER_COPPER: {
            const auto* st = nodes.getStation(1, ResourceType::COPPER);
            if (st) {
                spotlightRect = sf::FloatRect({ st->bounds.position.x - 5.0f, st->bounds.position.y - 5.0f },
                                             { st->bounds.size.x + 10.0f, st->bounds.size.y + 10.0f });
                drawSpotlight(window, spotlightRect, animTime);
                arrowTarget = sf::Vector2f(st->bounds.position.x + st->bounds.size.x / 2.0f, st->bounds.position.y + 35.0f);
                arrowLabel = "ДОБИВ: МЕД " + inputRouter().hint(1, InputAction::Action, false, 1);
                drawArrow(window, arrowTarget, arrowLabel, font, animTime, ArrowDir::DOWN);
            }
            break;
        }
        case TutorialStep::GATHER_SILICON: {
            const auto* st = nodes.getStation(1, ResourceType::SILICON);
            if (st) {
                spotlightRect = sf::FloatRect({ st->bounds.position.x - 5.0f, st->bounds.position.y - 5.0f },
                                             { st->bounds.size.x + 10.0f, st->bounds.size.y + 10.0f });
                drawSpotlight(window, spotlightRect, animTime);
                arrowTarget = sf::Vector2f(st->bounds.position.x + st->bounds.size.x / 2.0f, st->bounds.position.y + 35.0f);
                arrowLabel = "ДОБИВ: СИЛИЦИЙ " + inputRouter().hint(1, InputAction::Action, false, 1);
                drawArrow(window, arrowTarget, arrowLabel, font, animTime, ArrowDir::DOWN);
            }
            break;
        }
        case TutorialStep::SELECT_SOLAR: {
            // Spotlight on Player 1 Building Bar (Solar Panel card at x=24, y=143, w=218, h=55)
            spotlightRect = sf::FloatRect({ 20.0f, 140.0f }, { 226.0f, 61.0f });
            drawSpotlight(window, spotlightRect, animTime);
            arrowTarget = sf::Vector2f(246.0f, 170.5f);
            arrowLabel = "ИЗБЕРЕТЕ: [1] СЛЪНЧЕВ ПАНЕЛ";
            drawArrow(window, arrowTarget, arrowLabel, font, animTime, ArrowDir::LEFT);
            break;
        }
        case TutorialStep::PLACE_SOLAR: {
            // Highlight free slot on Player 1's starting plot (r=0, c=0, slot index 1,0)
            sf::Vector2f slot = engine.getGridSlot(1, 1, 0);
            spotlightRect = sf::FloatRect({ slot.x - 20.0f, slot.y - 20.0f }, { 40.0f, 40.0f });
            drawSpotlight(window, spotlightRect, animTime);
            arrowLabel = "ПОСТАВЕТЕ ТУК " + inputRouter().hint(1, InputAction::Action, false, 1);
            drawArrow(window, slot, arrowLabel, font, animTime, ArrowDir::DOWN);
            break;
        }
        case TutorialStep::COMPLETED: {
            sf::RectangleShape softDim({ 1600.0f, 900.0f });
            softDim.setFillColor(sf::Color(0, 0, 0, 140));
            window.draw(softDim);
            break;
        }
        default:
            break;
    }

    // -------------------------------------------------------------------------
    // 2. Under-Hero Floating Control Badges
    // -------------------------------------------------------------------------
    if (step != TutorialStep::INACTIVE) {
        // Context-aware Player 1 Cursor hint
        std::string p1Hint;
        if (isCoop) {
            if (step == TutorialStep::SELECT_SOLAR) {
                p1Hint = "P1: " + inputRouter().hint(1, InputAction::Quick1, false) + " Избери Слънчев панел  |  " + inputRouter().hint(1, InputAction::Cancel, false) + " Отказ";
            } else if (step == TutorialStep::PLACE_SOLAR) {
                p1Hint = "P1: " + inputRouter().hint(1, InputAction::Action, false) + " Строеж  |  " + inputRouter().hint(1, InputAction::Cancel, false) + " Отказ";
            } else if (step == TutorialStep::COMPLETED) {
                p1Hint = "P1: [SPACE] Продължи";
            } else {
                p1Hint = "P1: " + inputRouter().moveHint(1) + " Движение  |  " + inputRouter().hint(1, InputAction::Action, false, 1) + " Добив";
            }
        } else {
            if (step == TutorialStep::SELECT_SOLAR) {
                p1Hint = inputRouter().hint(1, InputAction::Quick1, false, 1) + " или " + inputRouter().hint(1, InputAction::NextBuilding, false, 1) + " - Избери Слънчев панел  |  " + inputRouter().hint(1, InputAction::Cancel, false) + " Отказ";
            } else if (step == TutorialStep::PLACE_SOLAR) {
                p1Hint = inputRouter().hint(1, InputAction::Action, false) + " - Строеж  |  " + inputRouter().hint(1, InputAction::Cancel, false) + " Отказ";
            } else if (step == TutorialStep::COMPLETED) {
                p1Hint = "[SPACE] или [ENTER] - Продължи";
            } else {
                p1Hint = inputRouter().moveHint(1) + " или " + inputRouter().moveHint(2) + " - Движение  |  " + inputRouter().hint(1, InputAction::Action, false) + " - Добив";
            }
        }

        sf::Text p1Tag(font, toUtf8(p1Hint), 11);
        p1Tag.setFillColor(sf::Color(0, 255, 230));
        sf::FloatRect p1b = p1Tag.getLocalBounds();

        float pillHalfW = (p1b.size.x + 14.0f) / 2.0f;
        // Clamp pill so its left edge NEVER overlaps the left building panel (x in [18, 248])
        float pillX = std::max(252.0f + pillHalfW, p1Pos.x);
        // Clamp pill vertically so it stays within game bounds and above the tutorial card
        float pillY = std::min(740.0f, std::max(40.0f, p1Pos.y + 28.0f));

        sf::RectangleShape p1Pill({ p1b.size.x + 14.0f, 20.0f });
        p1Pill.setOrigin({ pillHalfW, 10.0f });
        p1Pill.setPosition({ pillX, pillY });
        p1Pill.setFillColor(sf::Color(10, 16, 26, 235));
        p1Pill.setOutlineThickness(1.2f);
        p1Pill.setOutlineColor(sf::Color(0, 229, 255, 200));
        window.draw(p1Pill);

        p1Tag.setOrigin({ p1b.size.x / 2.0f, p1b.size.y / 2.0f });
        p1Tag.setPosition({ pillX, pillY - 2.0f });
        window.draw(p1Tag);

        // Player 2 Cursor hint in Co-op mode
        if (isCoop) {
            const InputRouter& k2 = inputRouter(); // b-session (F-10): P2's real keys
            std::string p2Hint = (step == TutorialStep::SELECT_SOLAR) ? "P2: " + k2.hint(2, InputAction::NextBuilding, false, 1) + " Сграда  |  " + k2.hint(2, InputAction::Cancel, false, 1) + " Отказ"
                               : (step == TutorialStep::PLACE_SOLAR)  ? "P2: " + k2.hint(2, InputAction::Action, false, 1) + " Строеж  |  " + k2.hint(2, InputAction::Cancel, false, 1) + " Отказ"
                               : (step == TutorialStep::COMPLETED)    ? std::string("P2: [ENTER] Продължи")
                                                                      : "P2: " + k2.moveHint(2) + " Движение  |  " + k2.hint(2, InputAction::Action, false, 1) + " Добив";
            sf::Text p2Tag(font, toUtf8(p2Hint), 11);
            p2Tag.setFillColor(sf::Color(255, 140, 220));
            sf::FloatRect p2b = p2Tag.getLocalBounds();

            float p2HalfW = (p2b.size.x + 14.0f) / 2.0f;
            // Clamp pill so its right edge NEVER overlaps the right building panel (x in [1352, 1582])
            float p2PillX = std::min(1348.0f - p2HalfW, p2Pos.x);
            float p2PillY = std::min(740.0f, std::max(40.0f, p2Pos.y + 28.0f));

            sf::RectangleShape p2Pill({ p2b.size.x + 14.0f, 20.0f });
            p2Pill.setOrigin({ p2HalfW, 10.0f });
            p2Pill.setPosition({ p2PillX, p2PillY });
            p2Pill.setFillColor(sf::Color(20, 14, 26, 235));
            p2Pill.setOutlineThickness(1.2f);
            p2Pill.setOutlineColor(sf::Color(255, 120, 200, 200));
            window.draw(p2Pill);

            p2Tag.setOrigin({ p2b.size.x / 2.0f, p2b.size.y / 2.0f });
            p2Tag.setPosition({ p2PillX, p2PillY - 2.0f });
            window.draw(p2Tag);
        }
    }

    // -------------------------------------------------------------------------
    // 3. Tutorial Glassmorphic Banner Card (Bottom Area)
    // -------------------------------------------------------------------------
    sf::RectangleShape card(cardBounds.size);
    card.setPosition(cardBounds.position);
    card.setFillColor(sf::Color(10, 16, 26, 248));
    card.setOutlineThickness(2.0f);
    card.setOutlineColor(sf::Color(0, 229, 255, 230));
    window.draw(card);

    // Cyan glowing top accent bar
    sf::RectangleShape topBar({ cardBounds.size.x, 3.0f });
    topBar.setPosition(cardBounds.position);
    topBar.setFillColor(sf::Color(0, 229, 255));
    window.draw(topBar);

    // Skip button in top right of card
    bool hoverSkip = skipBtnBounds.contains(mousePos);
    sf::RectangleShape skipBtn(skipBtnBounds.size);
    skipBtn.setPosition(skipBtnBounds.position);
    skipBtn.setFillColor(hoverSkip ? sf::Color(65, 30, 40, 230) : sf::Color(25, 30, 42, 200));
    skipBtn.setOutlineThickness(1.0f);
    skipBtn.setOutlineColor(hoverSkip ? sf::Color(255, 100, 100) : sf::Color(150, 160, 180));
    window.draw(skipBtn);

    sf::Text skipText(font, toUtf8("ПРОПУСНИ [ESC]"), 11);
    skipText.setFillColor(hoverSkip ? sf::Color(255, 140, 140) : sf::Color(180, 190, 200));
    sf::FloatRect stb = skipText.getLocalBounds();
    skipText.setPosition({ skipBtnBounds.position.x + (skipBtnBounds.size.x - stb.size.x) / 2.0f,
                           skipBtnBounds.position.y + (skipBtnBounds.size.y - stb.size.y) / 2.0f - 2.0f });
    window.draw(skipText);

    // Step Header & Descriptions
    std::string badgeText = "ТУТОРИАЛ: ОСНОВИ";
    std::string titleText = "";
    std::string descText = "";
    std::string progressText = "";
    float progressRatio = 0.0f;
    bool showNextBtn = false;
    std::string nextBtnLabel = "НАПРЕД [SPACE]";

    switch (step) {
        case TutorialStep::WELCOME:
            badgeText = "ОСНОВИ НА ИГРАТА";
            titleText = "ДОБРЕ ДОШЛИ В ENERGY CRISIS!";
            descText = "Целта е да захраните града с чиста електроенергия!\n"
                       "Започвате от нулата — първо трябва да добиете нужните суровини за Слънчев панел.\n"
                       "Движение: " + inputRouter().moveHint(1) + ". Действие: " + inputRouter().hint(1, InputAction::Action, false, 1) + "."; // b-session: short, the card button sits on this line
            showNextBtn = true;
            nextBtnLabel = "ЗАПОЧНИ [SPACE]";
            break;

        case TutorialStep::GATHER_WOOD:
            badgeText = "СТЪПКА 1 / 6: СЪБИРАНЕ НА РЕСУРСИ";
            titleText = "ДОБИЙТЕ ДЪРВЕСИНА ОТ СТАНЦИЯ 'ГОРА'";
            descText = "Застанете върху осветената станция ГОРА и натиснете " + inputRouter().hint(1, InputAction::Action, false, 1) + " (или ляв клик).\n"
                       "Всеки удар добива дърво за склада ви. Нужно за панел: 6 Дърво.";
            progressRatio = std::min(1.0f, static_cast<float>(econ.wood) / 6.0f);
            progressText = "Дървесина: " + std::to_string(econ.wood) + " / 6" + (econ.wood >= 6 ? "  [ГОТОВО!]" : "");
            break;

        case TutorialStep::GATHER_IRON:
            badgeText = "СТЪПКА 2 / 6: СЪБИРАНЕ НА РЕСУРСИ";
            titleText = "ДОБИЙТЕ ЖЕЛЯЗО ЗА РАМКАТА";
            descText = "Отлично! Преместете се върху станция ЖЕЛЯЗО и натиснете " + inputRouter().hint(1, InputAction::Action, false, 1) + ".\n"
                       "Желязото осигурява стабилна носеща конструкция. Нужно: 4 Желязо.";
            progressRatio = std::min(1.0f, static_cast<float>(econ.iron) / 4.0f);
            progressText = "Желязо: " + std::to_string(econ.iron) + " / 4" + (econ.iron >= 4 ? "  [ГОТОВО!]" : "");
            break;

        case TutorialStep::GATHER_COPPER:
            badgeText = "СТЪПКА 3 / 6: СЪБИРАНЕ НА РЕСУРСИ";
            titleText = "ДОБИЙТЕ МЕД ЗА ЕЛЕКТРОПРОВОДИТЕ";
            descText = "Чудесно! Отидете върху станция МЕД и натиснете " + inputRouter().hint(1, InputAction::Action, false, 1) + " за добив.\n"
                       "Медта провежда изработения ток към централната мрежа. Нужно: 6 Мед.";
            progressRatio = std::min(1.0f, static_cast<float>(econ.copper) / 6.0f);
            progressText = "Мед: " + std::to_string(econ.copper) + " / 6" + (econ.copper >= 6 ? "  [ГОТОВО!]" : "");
            break;

        case TutorialStep::GATHER_SILICON:
            badgeText = "СТЪПКА 4 / 6: СЪБИРАНЕ НА РЕСУРСИ";
            titleText = "ДОБИЙТЕ СИЛИЦИЙ ЗА СОЛАРНИТЕ КЛЕТКИ";
            descText = "Силицият е на втория ред. Отидете върху станция СИЛИЦИЙ и натиснете " + inputRouter().hint(1, InputAction::Action, false, 1) + ".\n"
                       "Той преобразува слънчевата светлина в електричество. Нужно: 8 Силиций.";
            progressRatio = std::min(1.0f, static_cast<float>(econ.silicon) / 8.0f);
            progressText = "Силиций: " + std::to_string(econ.silicon) + " / 8" + (econ.silicon >= 8 ? "  [ГОТОВО!]" : "");
            break;

        case TutorialStep::SELECT_SOLAR:
            badgeText = "СТЪПКА 5 / 6: ИЗБОР И ОТКАЗ";
            titleText = "ИЗБЕРЕТЕ СЛЪНЧЕВ ПАНЕЛ ЗА СТРОЕЖ";
            descText = "Натиснете клавиш " + inputRouter().hint(1, InputAction::Quick1, false, 1) + " (или " + inputRouter().hint(1, InputAction::NextBuilding, false) + "), за да изберете Слънчев панел.\n" +
                       "СЪВЕТ: Ако решите да се откажете от строеж, натиснете " + inputRouter().hint(1, InputAction::Cancel, false) + " (или десен клик)!";
            progressRatio = 1.0f;
            progressText = "Ресурси: ГОТОВИ!  [Натиснете " + keyLabelLocalized(gameSettings().keys.key(1, InputAction::Quick1, 0)) + " за избор]";
            break;

        case TutorialStep::PLACE_SOLAR:
            badgeText = "СТЪПКА 6 / 6: СТРОИТЕЛСТВО";
            titleText = "ПОСТАВЕТЕ ПАНЕЛА ВЪРХУ ВАШАТА ЗЕМЯ";
            descText = "Преместете курсора си върху маркираната свободна клетка от вашия парцел.\n"
                       "Натиснете " + inputRouter().hint(1, InputAction::Action, false, 1) + " (или ляв клик), за да завършите строежа!";
            progressRatio = 0.5f;
            progressText = "Позиционирайте курсора и натиснете " + inputRouter().hint(1, InputAction::Action, false) + "";
            break;

        case TutorialStep::COMPLETED:
            badgeText = "УСПЕХ! ПЪРВИЯТ ВИ ПАНЕЛ РАБОТИ!";
            titleText = "ПОЗДРАВЛЕНИЯ! ВЕЧЕ ПРОИЗВЕЖДАТЕ ТОК!";
            descText = "Панелът ви дава +60 MW чиста мощност през деня!\n"
                       "Снабдяването на града ви носи пари ($) и злато за нови земи и надграждане на мините.\n"
                       "Стройте още панели, вятърни мелници и батерии за победа!";
            showNextBtn = true;
            nextBtnLabel = "КЪМ ИГРАТА [SPACE]";
            break;

        default:
            break;
    }

    // Badge
    sf::Text badge(font, toUtf8(badgeText), 11);
    badge.setFillColor(sf::Color(0, 229, 255));
    badge.setPosition({ cardBounds.position.x + 16.0f, cardBounds.position.y + 9.0f });
    window.draw(badge);

    // Title
    sf::Text title(font, toUtf8(titleText), 15);
    title.setFillColor(sf::Color(255, 215, 0));
    title.setPosition({ cardBounds.position.x + 16.0f, cardBounds.position.y + 24.0f });
    window.draw(title);

    // Description
    sf::Text desc(font, toUtf8(descText), 12);
    desc.setFillColor(sf::Color(215, 225, 235));
    desc.setLineSpacing(1.15f);
    desc.setPosition({ cardBounds.position.x + 16.0f, cardBounds.position.y + 44.0f });
    window.draw(desc);

    // Progress Bar (when applicable)
    if (!progressText.empty()) {
        float barX = cardBounds.position.x + 16.0f;
        float barY = cardBounds.position.y + cardBounds.size.y - 20.0f;
        float barW = 230.0f;
        float barH = 9.0f;

        sf::RectangleShape barBg({ barW, barH });
        barBg.setPosition({ barX, barY });
        barBg.setFillColor(sf::Color(30, 40, 55));
        barBg.setOutlineThickness(1.0f);
        barBg.setOutlineColor(sf::Color(60, 80, 110));
        window.draw(barBg);

        if (progressRatio > 0.0f) {
            sf::RectangleShape barFill({ barW * progressRatio, barH });
            barFill.setPosition({ barX, barY });
            barFill.setFillColor(progressRatio >= 1.0f ? sf::Color(0, 255, 160) : sf::Color(0, 229, 255));
            window.draw(barFill);
        }

        sf::Text progTxt(font, toUtf8(progressText), 11);
        progTxt.setFillColor(progressRatio >= 1.0f ? sf::Color(100, 255, 180) : sf::Color(255, 220, 120));
        progTxt.setPosition({ barX + barW + 12.0f, barY - 2.0f });
        window.draw(progTxt);
    }

    // Action button (Welcome & Completed steps)
    if (showNextBtn) {
        bool hoverNext = nextBtnBounds.contains(mousePos);
        sf::RectangleShape nextBtn(nextBtnBounds.size);
        nextBtn.setPosition(nextBtnBounds.position);
        nextBtn.setFillColor(hoverNext ? sf::Color(0, 255, 180, 240) : sf::Color(0, 200, 150, 220));
        nextBtn.setOutlineThickness(1.5f);
        nextBtn.setOutlineColor(sf::Color::White);
        window.draw(nextBtn);

        sf::Text nxtTxt(font, toUtf8(nextBtnLabel), 12);
        nxtTxt.setFillColor(sf::Color(10, 20, 30));
        sf::FloatRect ntb = nxtTxt.getLocalBounds();
        nxtTxt.setPosition({ nextBtnBounds.position.x + (nextBtnBounds.size.x - ntb.size.x) / 2.0f,
                            nextBtnBounds.position.y + (nextBtnBounds.size.y - ntb.size.y) / 2.0f - 2.0f });
        window.draw(nxtTxt);
    }
}

bool UI_tutorial::handleClick(sf::Vector2f mousePos) {
    if (!active || step == TutorialStep::INACTIVE) return false;

    if (skipBtnBounds.contains(mousePos)) {
        skip();
        return true;
    }

    if (step == TutorialStep::WELCOME) {
        if (nextBtnBounds.contains(mousePos)) {
            step = TutorialStep::GATHER_WOOD;
            return true;
        }
    } else if (step == TutorialStep::COMPLETED) {
        if (nextBtnBounds.contains(mousePos)) {
            skip();
            return true;
        }
    }

    return false;
}

bool UI_tutorial::handleKey(sf::Keyboard::Key key) {
    if (!active || step == TutorialStep::INACTIVE) return false;

    if (key == sf::Keyboard::Key::Escape) {
        skip();
        return true;
    }

    if (step == TutorialStep::WELCOME) {
        if (key == sf::Keyboard::Key::Space || key == sf::Keyboard::Key::Enter) {
            step = TutorialStep::GATHER_WOOD;
            return true;
        }
    } else if (step == TutorialStep::COMPLETED) {
        if (key == sf::Keyboard::Key::Space || key == sf::Keyboard::Key::Enter) {
            skip();
            return true;
        }
    }

    return false;
}
