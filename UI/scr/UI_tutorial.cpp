#include "../includes/UI_tutorial.h"
#include <cmath>
#include <algorithm>
#include <iostream>

UI_tutorial::UI_tutorial()
    : step(TutorialStep::WELCOME),
      active(true),
      animTimer(0.0f),
      stepDelayTimer(0.0f),
      initialP1BuildingCount(1) {
    cardBounds = sf::FloatRect({ 240.0f, 756.0f }, { 540.0f, 126.0f });
    skipBtnBounds = sf::FloatRect({ cardBounds.position.x + cardBounds.size.x - 148.0f, cardBounds.position.y + 8.0f }, { 140.0f, 24.0f });
    nextBtnBounds = sf::FloatRect({ cardBounds.position.x + cardBounds.size.x - 190.0f, cardBounds.position.y + cardBounds.size.y - 36.0f }, { 180.0f, 28.0f });
}

void UI_tutorial::reset() {
    step = TutorialStep::WELCOME;
    active = true;
    animTimer = 0.0f;
    stepDelayTimer = 0.0f;
    initialP1BuildingCount = 1;
}

void UI_tutorial::start() {
    step = TutorialStep::WELCOME;
    active = true;
}

void UI_tutorial::skip() {
    step = TutorialStep::INACTIVE;
    active = false;
}

void UI_tutorial::update(float dt, const GameEngine& engine) {
    if (!active || step == TutorialStep::INACTIVE) return;
    animTimer += dt;

    const auto& econ = engine.getPlayerEconomy(1);

    // Initial building count tracking (starts with 1 starter solar panel)
    if (initialP1BuildingCount <= 1) {
        int count = 0;
        for (const auto& b : engine.getBuildings()) {
            if (b.playerOwner == 1) count++;
        }
        if (count > 0) initialP1BuildingCount = count;
    }

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
                step = TutorialStep::PLACE_SOLAR;
            }
            break;
        case TutorialStep::PLACE_SOLAR: {
            int currentP1Buildings = 0;
            for (const auto& b : engine.getBuildings()) {
                if (b.playerOwner == 1) currentP1Buildings++;
            }
            if (currentP1Buildings > initialP1BuildingCount) {
                step = TutorialStep::COMPLETED;
            }
            break;
        }
        default:
            break;
    }
}

void UI_tutorial::drawArrow(sf::RenderWindow& window, sf::Vector2f targetPos, const std::string& label,
                            const sf::Font& font, float animTime, bool pointUp) {
    float bounce = std::sin(animTime * 6.0f) * 6.0f;

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
    if (!pointUp) {
        // Arrow pointing DOWN at targetPos
        sf::Vector2f tip(targetPos.x, targetPos.y - 12.0f + bounce);
        arrow.setPoint(0, tip);
        arrow.setPoint(1, sf::Vector2f(tip.x - 14.0f, tip.y - 24.0f));
        arrow.setPoint(2, sf::Vector2f(tip.x + 14.0f, tip.y - 24.0f));
    } else {
        // Arrow pointing UP at targetPos
        sf::Vector2f tip(targetPos.x, targetPos.y + 12.0f + bounce);
        arrow.setPoint(0, tip);
        arrow.setPoint(1, sf::Vector2f(tip.x - 14.0f, tip.y + 24.0f));
        arrow.setPoint(2, sf::Vector2f(tip.x + 14.0f, tip.y + 24.0f));
    }
    arrow.setFillColor(sf::Color(255, 215, 0, 240));
    arrow.setOutlineThickness(2.0f);
    arrow.setOutlineColor(sf::Color::White);
    window.draw(arrow);

    // Floating text tag over arrow
    if (!label.empty()) {
        sf::Text text(font, toUtf8(label), 12);
        text.setFillColor(sf::Color::White);
        sf::FloatRect tb = text.getLocalBounds();

        float tagY = pointUp ? (targetPos.y + 40.0f + bounce) : (targetPos.y - 58.0f + bounce);
        sf::RectangleShape tagBg({ tb.size.x + 16.0f, 22.0f });
        tagBg.setOrigin({ (tb.size.x + 16.0f) / 2.0f, 11.0f });
        tagBg.setPosition({ targetPos.x, tagY });
        tagBg.setFillColor(sf::Color(10, 16, 26, 240));
        tagBg.setOutlineThickness(1.5f);
        tagBg.setOutlineColor(sf::Color(255, 215, 0, 220));
        window.draw(tagBg);

        text.setOrigin({ tb.size.x / 2.0f, tb.size.y / 2.0f });
        text.setPosition({ targetPos.x, tagY - 2.0f });
        window.draw(text);
    }
}

void UI_tutorial::draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                       const GameEngine& engine, const UI_resourceNodes& nodes, float animTime, sf::Vector2f mousePos) {
    if (!active || step == TutorialStep::INACTIVE || !fontLoaded) return;

    const auto& econ = engine.getPlayerEconomy(1);

    // -------------------------------------------------------------------------
    // 1. Draw Visual Focus Arrows & Highlights on Targets
    // -------------------------------------------------------------------------
    switch (step) {
        case TutorialStep::WELCOME: {
            const auto* st = nodes.getStation(1, ResourceType::WOOD);
            if (st) {
                sf::Vector2f center(st->bounds.position.x + st->bounds.size.x / 2.0f, st->bounds.position.y + 35.0f);
                drawArrow(window, center, "СТАНЦИЯ ГОРА (ДЪРВО)", font, animTime, false);
            }
            break;
        }
        case TutorialStep::GATHER_WOOD: {
            const auto* st = nodes.getStation(1, ResourceType::WOOD);
            if (st) {
                sf::Vector2f center(st->bounds.position.x + st->bounds.size.x / 2.0f, st->bounds.position.y + 35.0f);
                drawArrow(window, center, "ДОБИВ: ДЪРВЕСИНА [SPACE]", font, animTime, false);
            }
            break;
        }
        case TutorialStep::GATHER_IRON: {
            const auto* st = nodes.getStation(1, ResourceType::IRON);
            if (st) {
                sf::Vector2f center(st->bounds.position.x + st->bounds.size.x / 2.0f, st->bounds.position.y + 35.0f);
                drawArrow(window, center, "ДОБИВ: ЖЕЛЯЗО [SPACE]", font, animTime, false);
            }
            break;
        }
        case TutorialStep::GATHER_COPPER: {
            const auto* st = nodes.getStation(1, ResourceType::COPPER);
            if (st) {
                sf::Vector2f center(st->bounds.position.x + st->bounds.size.x / 2.0f, st->bounds.position.y + 35.0f);
                drawArrow(window, center, "ДОБИВ: МЕД [SPACE]", font, animTime, false);
            }
            break;
        }
        case TutorialStep::GATHER_SILICON: {
            const auto* st = nodes.getStation(1, ResourceType::SILICON);
            if (st) {
                sf::Vector2f center(st->bounds.position.x + st->bounds.size.x / 2.0f, st->bounds.position.y + 35.0f);
                drawArrow(window, center, "ДОБИВ: СИЛИЦИЙ [SPACE]", font, animTime, false);
            }
            break;
        }
        case TutorialStep::SELECT_SOLAR: {
            // Point towards Player 1 Building menu on left
            sf::Vector2f menuSlot(95.0f, 440.0f);
            drawArrow(window, menuSlot, "ИЗБЕРЕТЕ: [1] СЛЪНЧЕВ ПАНЕЛ", font, animTime, false);
            break;
        }
        case TutorialStep::PLACE_SOLAR: {
            // Highlight free slot on Player 1's starting plot (r=0, c=0, slot index 1,0)
            sf::Vector2f targetSlot = engine.getGridSlot(1, 1, 0);

            // Pulsing highlight box on plot slot
            sf::RectangleShape slotBox({ 32.0f, 30.0f });
            slotBox.setOrigin({ 16.0f, 15.0f });
            slotBox.setPosition(targetSlot);
            slotBox.setFillColor(sf::Color(0, 255, 180, 50 + static_cast<std::uint8_t>(std::sin(animTime * 5.0f) * 35.0f)));
            slotBox.setOutlineThickness(2.0f);
            slotBox.setOutlineColor(sf::Color(0, 255, 180, 220));
            window.draw(slotBox);

            drawArrow(window, targetSlot, "ПОСТАВЕТЕ ТУК [SPACE]", font, animTime, false);
            break;
        }
        default:
            break;
    }

    // -------------------------------------------------------------------------
    // 2. Tutorial Glassmorphic Banner Card (Bottom Left Area)
    // -------------------------------------------------------------------------
    sf::RectangleShape card(cardBounds.size);
    card.setPosition(cardBounds.position);
    card.setFillColor(sf::Color(10, 16, 26, 245));
    card.setOutlineThickness(2.0f);
    card.setOutlineColor(sf::Color(0, 229, 255, 220));
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
            descText = "Целта е да захраните мегаполиса с чиста енергия преди противника!\n"
                       "Движете курсора си с [W/A/S/D] (или мишката).\n"
                       "За да построите първия си Соларен панел, са ви нужни ресурси!";
            showNextBtn = true;
            nextBtnLabel = "ЗАПОЧНИ [SPACE]";
            break;

        case TutorialStep::GATHER_WOOD:
            badgeText = "СТЪПКА 1 / 6: ДОБИВ";
            titleText = "СЪБЕРЕТЕ ДЪРВЕСИНА ЗА КОНСТРУКЦИЯТА";
            descText = "Застанете върху станция 'ГОРА' и натиснете [SPACE] (или ляв клик).\n"
                       "Всеки удар добива дърво за складовете ви. Нужно: 6 Дърво.";
            progressRatio = std::min(1.0f, static_cast<float>(econ.wood) / 6.0f);
            progressText = "Дървесина: " + std::to_string(econ.wood) + " / 6" + (econ.wood >= 6 ? "  [ГОТОВО!]" : "");
            break;

        case TutorialStep::GATHER_IRON:
            badgeText = "СТЪПКА 2 / 6: ДОБИВ";
            titleText = "СЪБЕРЕТЕ ЖЕЛЯЗО ЗА МЕТАЛНИТЕ РАМКИ";
            descText = "Отлично! Сега отидете върху станция 'ЖЕЛЯЗО' и натиснете [SPACE].\n"
                       "Желязото е основата за тежки конструкции. Нужно: 4 Желязо.";
            progressRatio = std::min(1.0f, static_cast<float>(econ.iron) / 4.0f);
            progressText = "Желязо: " + std::to_string(econ.iron) + " / 4" + (econ.iron >= 4 ? "  [ГОТОВО!]" : "");
            break;

        case TutorialStep::GATHER_COPPER:
            badgeText = "СТЪПКА 3 / 6: ДОБИВ";
            titleText = "СЪБЕРЕТЕ МЕД ЗА ПРОВОДНИЦИТЕ";
            descText = "Чудесно! Отидете върху станция 'МЕД' и натиснете [SPACE].\n"
                       "Медта се използва за кабели и енергопренос. Нужно: 6 Мед.";
            progressRatio = std::min(1.0f, static_cast<float>(econ.copper) / 6.0f);
            progressText = "Мед: " + std::to_string(econ.copper) + " / 6" + (econ.copper >= 6 ? "  [ГОТОВО!]" : "");
            break;

        case TutorialStep::GATHER_SILICON:
            badgeText = "СТЪПКА 4 / 6: ДОБИВ";
            titleText = "СЪБЕРЕТЕ СИЛИЦИЙ ЗА ФОТОВОЛТАИЧНИТЕ КЛЕТКИ";
            descText = "Силицият се намира на втория ред в кариерата. Отидете на 'СИЛИЦИЙ'.\n"
                       "Натиснете [SPACE], за да го добиете. Нужно: 8 Силиций.";
            progressRatio = std::min(1.0f, static_cast<float>(econ.silicon) / 8.0f);
            progressText = "Силиций: " + std::to_string(econ.silicon) + " / 8" + (econ.silicon >= 8 ? "  [ГОТОВО!]" : "");
            break;

        case TutorialStep::SELECT_SOLAR:
            badgeText = "СТЪПКА 5 / 6: СЕЛЕКЦИЯ";
            titleText = "ИЗБЕРЕТЕ СЛЪНЧЕВ ПАНЕЛ ЗА СТРОЕЖ";
            descText = "Всички материали са налице! Натиснете клавиш [1] (или [E]),\n"
                       "за да активирате режима за строеж на Слънчев панел.";
            progressRatio = 1.0f;
            progressText = "Ресурси: ГОТОВИ!  [Натиснете 1]";
            break;

        case TutorialStep::PLACE_SOLAR:
            badgeText = "СТЪПКА 6 / 6: СТРОИТЕЛСТВО";
            titleText = "ПОСТАВЕТЕ ПАНЕЛА ВЪРХУ ВАШИЯ ПАРЦЕЛ";
            descText = "Преместете курсора си върху маркираната клетка на парцела.\n"
                       "Натиснете [SPACE] (или ляв клик), за да завършите строежа!";
            progressRatio = 0.5f;
            progressText = "Позиционирайте курсора и натиснете [SPACE]";
            break;

        case TutorialStep::COMPLETED:
            badgeText = "УСПЕХ!";
            titleText = "БРАВО! ВАШИЯТ СЛЪНЧЕВ ПАНЕЛ РАБОТИ!";
            descText = "Панелът генерира +60 MW чист ток за града през деня!\n"
                       "Доставката на ток ви носи пари, злато и градско влияние.\n"
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
    badge.setPosition({ cardBounds.position.x + 16.0f, cardBounds.position.y + 10.0f });
    window.draw(badge);

    // Title
    sf::Text title(font, toUtf8(titleText), 15);
    title.setFillColor(sf::Color(255, 215, 0));
    title.setPosition({ cardBounds.position.x + 16.0f, cardBounds.position.y + 26.0f });
    window.draw(title);

    // Description
    sf::Text desc(font, toUtf8(descText), 12);
    desc.setFillColor(sf::Color(215, 225, 235));
    desc.setLineSpacing(1.15f);
    desc.setPosition({ cardBounds.position.x + 16.0f, cardBounds.position.y + 48.0f });
    window.draw(desc);

    // Progress Bar (when applicable)
    if (!progressText.empty()) {
        float barX = cardBounds.position.x + 16.0f;
        float barY = cardBounds.position.y + cardBounds.size.y - 24.0f;
        float barW = 230.0f;
        float barH = 10.0f;

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
