#include "../includes/UI_tutorial.h"
#include "../includes/UI_text.h"
#include "../includes/UI_shot.h"
#include "../includes/UI_theme.h"
#include "../includes/UI_settings.h"
#include "../includes/UI_arcadeMode.h"
#include <cmath>
#include <algorithm>
#include <iostream>

UI_tutorial::UI_tutorial()
    : p1Step(TutorialStep::WELCOME),
      p2Step(TutorialStep::WELCOME),
      p1Active(true),
      p2Active(true),
      isCoop(false),
      animTimer(0.0f),
      p1StepDelayTimer(0.0f),
      p2StepDelayTimer(0.0f),
      initialP1BuildingCount(0),
      initialP2BuildingCount(0) {
    // Single player centered card
    cardBounds = sf::FloatRect({ 480.0f, 765.0f }, { 640.0f, 120.0f });
    float skipW = cardBounds.size.x * 0.24f;
    float skipH = cardBounds.size.y * 0.20f;
    skipBtnBounds = sf::FloatRect({ cardBounds.position.x + cardBounds.size.x - skipW - cardBounds.size.x * 0.02f,
                                   cardBounds.position.y + cardBounds.size.y * 0.07f },
                                 { skipW, skipH });
    float nextW = cardBounds.size.x * 0.32f;
    float nextH = cardBounds.size.y * 0.22f;
    nextBtnBounds = sf::FloatRect({ cardBounds.position.x + cardBounds.size.x - nextW - cardBounds.size.x * 0.02f,
                                   cardBounds.position.y + cardBounds.size.y - nextH - cardBounds.size.y * 0.07f },
                                 { nextW, nextH });

    // Co-op decoupled dual cards
    p1CardBounds = sf::FloatRect({ 258.0f, 765.0f }, { 520.0f, 122.0f });
    float p1SkipW = p1CardBounds.size.x * 0.24f;
    float p1SkipH = p1CardBounds.size.y * 0.20f;
    p1SkipBtnBounds = sf::FloatRect({ p1CardBounds.position.x + p1CardBounds.size.x - p1SkipW - p1CardBounds.size.x * 0.02f,
                                     p1CardBounds.position.y + p1CardBounds.size.y * 0.07f },
                                   { p1SkipW, p1SkipH });
    float p1NextW = p1CardBounds.size.x * 0.34f;
    float p1NextH = p1CardBounds.size.y * 0.22f;
    p1NextBtnBounds = sf::FloatRect({ p1CardBounds.position.x + p1CardBounds.size.x - p1NextW - p1CardBounds.size.x * 0.02f,
                                     p1CardBounds.position.y + p1CardBounds.size.y - p1NextH - p1CardBounds.size.y * 0.07f },
                                   { p1NextW, p1NextH });

    p2CardBounds = sf::FloatRect({ 822.0f, 765.0f }, { 520.0f, 122.0f });
    float p2SkipW = p2CardBounds.size.x * 0.24f;
    float p2SkipH = p2CardBounds.size.y * 0.20f;
    p2SkipBtnBounds = sf::FloatRect({ p2CardBounds.position.x + p2CardBounds.size.x - p2SkipW - p2CardBounds.size.x * 0.02f,
                                     p2CardBounds.position.y + p2CardBounds.size.y * 0.07f },
                                   { p2SkipW, p2SkipH });
    float p2NextW = p2CardBounds.size.x * 0.34f;
    float p2NextH = p2CardBounds.size.y * 0.22f;
    p2NextBtnBounds = sf::FloatRect({ p2CardBounds.position.x + p2CardBounds.size.x - p2NextW - p2CardBounds.size.x * 0.02f,
                                     p2CardBounds.position.y + p2CardBounds.size.y - p2NextH - p2CardBounds.size.y * 0.07f },
                                   { p2NextW, p2NextH });
}

void UI_tutorial::reset() {
    p1Step = TutorialStep::WELCOME;
    p2Step = TutorialStep::WELCOME;
    p1Active = true;
    p2Active = isCoop;
    animTimer = 0.0f;
    p1StepDelayTimer = 0.0f;
    p2StepDelayTimer = 0.0f;
    initialP1BuildingCount = 0;
    initialP2BuildingCount = 0;
    p1ExitBoostGranted = false;
    p2ExitBoostGranted = false;
    p1MineFunded = false;
    p2MineFunded = false;
}

void UI_tutorial::start() {
    p1Step = TutorialStep::WELCOME;
    p2Step = TutorialStep::WELCOME;
    p1Active = true;
    p2Active = isCoop;
    p1StepDelayTimer = 0.0f;
    p2StepDelayTimer = 0.0f;
    initialP1BuildingCount = 0;
    initialP2BuildingCount = 0;
    p1ExitBoostGranted = false;
    p2ExitBoostGranted = false;
    p1MineFunded = false;
    p2MineFunded = false;
}

void UI_tutorial::grantExitBoost(int player, GameEngine& engine) {
    if (player == 1 && !p1ExitBoostGranted) {
        engine.getPlayerEconomyMut(1).gold += 30;
        p1ExitBoostGranted = true;
    } else if (player == 2 && !p2ExitBoostGranted) {
        engine.getPlayerEconomyMut(2).gold += 30;
        p2ExitBoostGranted = true;
    }
}

void UI_tutorial::skip(GameEngine* engine) {
    if (engine) {
        grantExitBoost(1, *engine);
        if (isCoop) grantExitBoost(2, *engine);
    }
    p1Step = TutorialStep::INACTIVE;
    p2Step = TutorialStep::INACTIVE;
    p1Active = false;
    p2Active = false;
}

void UI_tutorial::skipP1(GameEngine* engine) {
    if (engine) grantExitBoost(1, *engine);
    p1Step = TutorialStep::INACTIVE;
    p1Active = false;
}

void UI_tutorial::skipP2(GameEngine* engine) {
    if (engine) grantExitBoost(2, *engine);
    p2Step = TutorialStep::INACTIVE;
    p2Active = false;
}

bool UI_tutorial::isTutorialBlockingTime() const {
    if (!isActive()) return false;
    bool p1Done = (!p1Active || p1Step == TutorialStep::INACTIVE || p1Step == TutorialStep::COMPLETED);
    if (!isCoop) {
        return !p1Done;
    }
    bool p2Done = (!p2Active || p2Step == TutorialStep::INACTIVE || p2Step == TutorialStep::COMPLETED);
    return !(p1Done && p2Done);
}

static int countP1SolarPanels(const GameEngine& engine) {
    int count = 0;
    for (const auto& b : engine.getBuildings()) {
        if (b.playerOwner == 1 && b.type == BuildingType::SOLAR_PANEL) count++;
    }
    return count;
}

static int countP2SolarPanels(const GameEngine& engine) {
    int count = 0;
    for (const auto& b : engine.getBuildings()) {
        if (b.playerOwner == 2 && b.type == BuildingType::SOLAR_PANEL) count++;
    }
    return count;
}

void UI_tutorial::update(float dt, GameEngine& engine) {
    if (!isActive()) return;
    animTimer += dt;

    // 1. Decoupled Player 1 Progression
    if (p1Active && p1Step != TutorialStep::INACTIVE && p1Step != TutorialStep::COMPLETED) {
        const auto& econ1 = engine.getPlayerEconomy(1);
        if (p1StepDelayTimer > 0.0f) {
            p1StepDelayTimer -= dt;
            if (p1StepDelayTimer <= 0.0f) {
                if (p1Step == TutorialStep::GATHER_WOOD) p1Step = TutorialStep::GATHER_IRON;
                else if (p1Step == TutorialStep::GATHER_IRON) p1Step = TutorialStep::GATHER_COPPER;
                else if (p1Step == TutorialStep::GATHER_COPPER) p1Step = TutorialStep::GATHER_SILICON;
                else if (p1Step == TutorialStep::GATHER_SILICON) p1Step = TutorialStep::SELECT_SOLAR;
            }
        } else {
            switch (p1Step) {
                case TutorialStep::GATHER_WOOD:
                    if (econ1.wood >= Balance::SOLAR_PANEL.woodCost) p1StepDelayTimer = 0.35f;
                    break;
                case TutorialStep::GATHER_IRON:
                    if (econ1.iron >= Balance::SOLAR_PANEL.ironCost) p1StepDelayTimer = 0.35f;
                    break;
                case TutorialStep::GATHER_COPPER:
                    if (econ1.copper >= Balance::SOLAR_PANEL.copperCost) p1StepDelayTimer = 0.35f;
                    break;
                case TutorialStep::GATHER_SILICON:
                    if (econ1.silicon >= Balance::SOLAR_PANEL.siliconCost) p1StepDelayTimer = 0.35f;
                    break;
                case TutorialStep::SELECT_SOLAR:
                    if (engine.getSelectedBuilding(1) == BuildingType::SOLAR_PANEL) {
                        initialP1BuildingCount = countP1SolarPanels(engine);
                        p1Step = TutorialStep::PLACE_SOLAR;
                    }
                    break;
                case TutorialStep::PLACE_SOLAR: {
                    int cur = countP1SolarPanels(engine);
                    if (cur > initialP1BuildingCount) {
                        p1Step = TutorialStep::UPGRADE_MINE;
                    } else if (cur < initialP1BuildingCount) {
                        initialP1BuildingCount = cur;
                    }
                    break;
                }
                case TutorialStep::UPGRADE_MINE: {
                    if (!p1MineFunded) {
                        if (engine.getPlayerEconomy(1).gold < 15) {
                            engine.getPlayerEconomyMut(1).gold += 15;
                        }
                        p1MineFunded = true;
                    }
                    bool hasUpgraded = false;
                    for (int r = 1; r <= 7; ++r) {
                        if (engine.getMineLevel(1, static_cast<ResourceType>(r)) > 1) {
                            hasUpgraded = true;
                            break;
                        }
                    }
                    if (hasUpgraded) {
                        grantExitBoost(1, engine);
                        p1Step = TutorialStep::COMPLETED;
                    }
                    break;
                }
                default: break;
            }
        }
    }

    // 2. Decoupled Player 2 Progression (Only in Co-op mode)
    if (isCoop && p2Active && p2Step != TutorialStep::INACTIVE && p2Step != TutorialStep::COMPLETED) {
        const auto& econ2 = engine.getPlayerEconomy(2);
        if (p2StepDelayTimer > 0.0f) {
            p2StepDelayTimer -= dt;
            if (p2StepDelayTimer <= 0.0f) {
                if (p2Step == TutorialStep::GATHER_WOOD) p2Step = TutorialStep::GATHER_IRON;
                else if (p2Step == TutorialStep::GATHER_IRON) p2Step = TutorialStep::GATHER_COPPER;
                else if (p2Step == TutorialStep::GATHER_COPPER) p2Step = TutorialStep::GATHER_SILICON;
                else if (p2Step == TutorialStep::GATHER_SILICON) p2Step = TutorialStep::SELECT_SOLAR;
            }
        } else {
            switch (p2Step) {
                case TutorialStep::GATHER_WOOD:
                    if (econ2.wood >= Balance::SOLAR_PANEL.woodCost) p2StepDelayTimer = 0.35f;
                    break;
                case TutorialStep::GATHER_IRON:
                    if (econ2.iron >= Balance::SOLAR_PANEL.ironCost) p2StepDelayTimer = 0.35f;
                    break;
                case TutorialStep::GATHER_COPPER:
                    if (econ2.copper >= Balance::SOLAR_PANEL.copperCost) p2StepDelayTimer = 0.35f;
                    break;
                case TutorialStep::GATHER_SILICON:
                    if (econ2.silicon >= Balance::SOLAR_PANEL.siliconCost) p2StepDelayTimer = 0.35f;
                    break;
                case TutorialStep::SELECT_SOLAR:
                    if (engine.getSelectedBuilding(2) == BuildingType::SOLAR_PANEL) {
                        initialP2BuildingCount = countP2SolarPanels(engine);
                        p2Step = TutorialStep::PLACE_SOLAR;
                    }
                    break;
                case TutorialStep::PLACE_SOLAR: {
                    int cur = countP2SolarPanels(engine);
                    if (cur > initialP2BuildingCount) {
                        p2Step = TutorialStep::UPGRADE_MINE;
                    } else if (cur < initialP2BuildingCount) {
                        initialP2BuildingCount = cur;
                    }
                    break;
                }
                case TutorialStep::UPGRADE_MINE: {
                    if (!p2MineFunded) {
                        if (engine.getPlayerEconomy(2).gold < 15) {
                            engine.getPlayerEconomyMut(2).gold += 15;
                        }
                        p2MineFunded = true;
                    }
                    bool hasUpgraded = false;
                    for (int r = 1; r <= 7; ++r) {
                        if (engine.getMineLevel(2, static_cast<ResourceType>(r)) > 1) {
                            hasUpgraded = true;
                            break;
                        }
                    }
                    if (hasUpgraded) {
                        grantExitBoost(2, engine);
                        p2Step = TutorialStep::COMPLETED;
                    }
                    break;
                }
                default: break;
            }
        }
    }
}

void UI_tutorial::drawSpotlight(sf::RenderWindow& window, sf::FloatRect targetRect, float animTime) {
    float sw = 1600.0f;
    float sh = 900.0f;
    sf::Color dimColor = theme::withAlpha(theme::Dim, 195);

    if (targetRect.position.y > 0.0f) {
        sf::RectangleShape top({ sw, targetRect.position.y });
        top.setPosition({ 0.0f, 0.0f });
        top.setFillColor(dimColor);
        window.draw(top);
    }
    float bY = targetRect.position.y + targetRect.size.y;
    if (bY < sh) {
        sf::RectangleShape bot({ sw, sh - bY });
        bot.setPosition({ 0.0f, bY });
        bot.setFillColor(dimColor);
        window.draw(bot);
    }
    if (targetRect.position.x > 0.0f) {
        sf::RectangleShape left({ targetRect.position.x, targetRect.size.y });
        left.setPosition({ 0.0f, targetRect.position.y });
        left.setFillColor(dimColor);
        window.draw(left);
    }
    float rX = targetRect.position.x + targetRect.size.x;
    if (rX < sw) {
        sf::RectangleShape right({ sw - rX, targetRect.size.y });
        right.setPosition({ rX, targetRect.position.y });
        right.setFillColor(dimColor);
        window.draw(right);
    }

    sf::RectangleShape border(targetRect.size);
    border.setPosition(targetRect.position);
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineThickness(2.5f);
    std::uint8_t borderAlpha = static_cast<std::uint8_t>(std::clamp(210.0f + std::sin(animTime * 6.0f) * 45.0f, 0.0f, 255.0f));
    border.setOutlineColor(theme::withAlpha(theme::Info, borderAlpha));
    window.draw(border);

    float cornerLen = 14.0f;
    float ct = 2.5f;
    sf::Color cColor = theme::Focus;
    sf::RectangleShape tlH({ cornerLen, ct }); tlH.setPosition(targetRect.position + sf::Vector2f(-2.0f, -2.0f)); tlH.setFillColor(cColor); window.draw(tlH);
    sf::RectangleShape tlV({ ct, cornerLen }); tlV.setPosition(targetRect.position + sf::Vector2f(-2.0f, -2.0f)); tlV.setFillColor(cColor); window.draw(tlV);
    sf::RectangleShape trH({ cornerLen, ct }); trH.setPosition({ targetRect.position.x + targetRect.size.x - cornerLen + 2.0f, targetRect.position.y - 2.0f }); trH.setFillColor(cColor); window.draw(trH);
    sf::RectangleShape trV({ ct, cornerLen }); trV.setPosition({ targetRect.position.x + targetRect.size.x - 1.0f, targetRect.position.y - 2.0f }); trV.setFillColor(cColor); window.draw(trV);
    sf::RectangleShape blH({ cornerLen, ct }); blH.setPosition({ targetRect.position.x - 2.0f, targetRect.position.y + targetRect.size.y - 1.0f }); blH.setFillColor(cColor); window.draw(blH);
    sf::RectangleShape blV({ ct, cornerLen }); blV.setPosition({ targetRect.position.x - 2.0f, targetRect.position.y + targetRect.size.y - cornerLen + 2.0f }); blV.setFillColor(cColor); window.draw(blV);
    sf::RectangleShape brH({ cornerLen, ct }); brH.setPosition({ targetRect.position.x + targetRect.size.x - cornerLen + 2.0f, targetRect.position.y + targetRect.size.y - 1.0f }); brH.setFillColor(cColor); window.draw(brH);
    sf::RectangleShape brV({ ct, cornerLen }); brV.setPosition({ targetRect.position.x + targetRect.size.x - 1.0f, targetRect.position.y + targetRect.size.y - cornerLen + 2.0f }); brV.setFillColor(cColor); window.draw(brV);
}

void UI_tutorial::drawArrow(sf::RenderWindow& window, sf::Vector2f targetPos, const std::string& label,
                            const sf::Font& font, float animTime, ArrowDir dir, sf::Color arrowColor) {
    float bounce = std::sin(animTime * 6.0f) * 5.0f;

    float pulseRadius = 24.0f + std::sin(animTime * 4.0f) * 4.0f;
    sf::CircleShape pulseCircle(pulseRadius);
    pulseCircle.setOrigin({ pulseRadius, pulseRadius });
    pulseCircle.setPosition(targetPos);
    pulseCircle.setFillColor(theme::withAlpha(arrowColor, 35));
    pulseCircle.setOutlineThickness(2.0f);
    pulseCircle.setOutlineColor(theme::withAlpha(arrowColor, 200));
    window.draw(pulseCircle);

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
    } else { // RIGHT
        tip = sf::Vector2f(targetPos.x - 10.0f + bounce, targetPos.y);
        arrow.setPoint(0, tip);
        arrow.setPoint(1, sf::Vector2f(tip.x - 22.0f, tip.y - 13.0f));
        arrow.setPoint(2, sf::Vector2f(tip.x - 22.0f, tip.y + 13.0f));
    }
    arrow.setFillColor(theme::withAlpha(arrowColor, 240));
    arrow.setOutlineThickness(2.0f);
    arrow.setOutlineColor(theme::Window);
    window.draw(arrow);

    if (!label.empty()) {
        sf::Text& text = ui::pooledText(font, toUtf8(label), fontsize::Label);
        text.setStyle(sf::Text::Bold);
        text.setFillColor(theme::TextPrimary);
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
        } else {
            tagX = tip.x - 22.0f - 6.0f - (tb.size.x + 16.0f) / 2.0f;
            tagY = targetPos.y;
        }

        sf::RectangleShape tagBg({ tb.size.x + 16.0f, 22.0f });
        tagBg.setOrigin({ (tb.size.x + 16.0f) / 2.0f, 11.0f });
        tagBg.setPosition({ tagX, tagY });
        tagBg.setFillColor(theme::withAlpha(theme::Panel, 240));
        tagBg.setOutlineThickness(1.5f);
        tagBg.setOutlineColor(theme::withAlpha(arrowColor, 220));
        window.draw(tagBg);
        ui::lint::occlude(tagBg.getGlobalBounds());

        text.setOrigin({ tb.size.x / 2.0f, tb.size.y / 2.0f });
        text.setPosition({ tagX, tagY - 2.0f });
        ui::drawText(window, text, tagBg.getGlobalBounds());
    }
}

void UI_tutorial::drawPlayerCard(sf::RenderWindow& window, const sf::Font& font, int player,
                                TutorialStep currentStep, const sf::FloatRect& bounds,
                                const sf::FloatRect& skipBtn, const sf::FloatRect& nextBtn,
                                const GameEngine& engine, float animTime) {
    if (currentStep == TutorialStep::INACTIVE) return;
    const auto& econ = engine.getPlayerEconomy(player);
    sf::Color playerColor = (player == 1) ? theme::P1 : theme::P2;
    bool isEn = (UI_settings::get().getLanguage() == "en");
    std::string pTag = isEn ? (player == 1 ? "PLAYER 1" : "PLAYER 2") : (player == 1 ? "ИГРАЧ 1" : "ИГРАЧ 2");

    // Card background
    sf::RectangleShape card(bounds.size);
    card.setPosition(bounds.position);
    card.setFillColor(theme::withAlpha(theme::Window, 245));
    card.setOutlineThickness(2.0f);
    std::uint8_t outlineAlpha = static_cast<std::uint8_t>(std::clamp(190.0f + std::sin(animTime * 5.0f) * 40.0f, 0.0f, 255.0f));
    card.setOutlineColor(theme::withAlpha(playerColor, outlineAlpha));
    window.draw(card);
    ui::lint::occlude(bounds);

    std::string badgeText = pTag + (isEn ? ": BASICS" : ": ОСНОВИ");
    std::string titleText = "";
    std::string descText = "";
    std::string progressText = "";
    float progressRatio = 0.0f;
    bool showNextBtn = false;
    bool isArcade = ArcadeMode::isEnabled();
    std::string nextBtnLabel = isArcade ? (isEn ? "NEXT [A]" : "НАПРЕД [A]")
                                        : (isEn ? (player == 1 ? "NEXT [SPACE]" : "NEXT [ENTER]")
                                                : (player == 1 ? "НАПРЕД [SPACE]" : "НАПРЕД [ENTER]"));

    switch (currentStep) {
        case TutorialStep::WELCOME:
            badgeText = pTag + (isEn ? ": WELCOME!" : ": ДОБРЕ ДОШЛИ!");
            titleText = isEn ? "OBJECTIVE: POWER THE CITY!" : "ЦЕЛ: ЗАХРАНЕТЕ ГРАДА С ТОК!";
            if (isArcade) {
                descText = isEn ? "Move: [JOYSTICK]  |  Action: [A]  |  Building: [D]  |  Cancel: [B]\n"
                                  "First gather resources to construct a Solar Panel."
                                : "Движение: [СТИК]  |  Действие: [A]  |  Сграда: [D]  |  Отказ: [B]\n"
                                  "Първо съберете суровини за изграждане на Слънчев панел.";
            } else {
                if (player == 1) {
                    descText = isEn ? "Move: [W/A/S/D]  |  Action: [SPACE]  |  Build: [E]  |  Cancel: [X]\n"
                                      "Or use Mouse to select & place. Gather resources first!"
                                    : "Движение: [W/A/S/D]  |  Действие: [SPACE]  |  Сграда: [E]  |  Отказ: [X]\n"
                                      "Или с мишката. Първо съберете ресурси за Слънчев панел.";
                } else {
                    descText = isEn ? "Move: [ARROWS]  |  Action: [ENTER]  |  Build: [PgDn]  |  Cancel: [DEL]\n"
                                      "First gather resources to construct a Solar Panel."
                                    : "Движение: [СТРЕЛКИ]  |  Действие: [ENTER]  |  Сграда: [PgDn]  |  Отказ: [DEL]\n"
                                      "Първо съберете ресурси за Слънчев панел.";
                }
            }
            showNextBtn = true;
            nextBtnLabel = isArcade ? (isEn ? "START [A]" : "ЗАПОЧНИ [A]")
                                    : (isEn ? (player == 1 ? "START [SPACE]" : "START [ENTER]")
                                            : (player == 1 ? "ЗАПОЧНИ [SPACE]" : "ЗАПОЧНИ [ENTER]"));
            break;

        case TutorialStep::GATHER_WOOD:
            badgeText = pTag + (isEn ? ": STEP 1/7 (MINING)" : ": СТЪПКА 1/7 (СЪБИРАНЕ)");
            titleText = isEn ? "GATHER WOOD (FOREST)" : "ДОБИЙТЕ ДЪРВЕСИНА (ГОРА)";
            if (isArcade) {
                descText = isEn ? (player == 1 ? "Move to the FOREST station (left) and press [A]."
                                               : "Move to the FOREST station (right) and press [A].")
                                : (player == 1 ? "Застанете на станция ГОРА (ляво) и натиснете [A]."
                                               : "Застанете на станция ГОРА (дясно) и натиснете [A].");
            } else {
                std::string act = (player == 1 ? "[SPACE]" : "[ENTER]");
                std::string dir = (player == 1 ? (isEn ? "left" : "ляво") : (isEn ? "right" : "дясно"));
                descText = isEn ? ("Move to the FOREST station (" + dir + ") and press " + act + ".")
                                : ("Застанете на станция ГОРА (" + dir + ") и натиснете " + act + ".");
            }
            progressRatio = std::min(1.0f, static_cast<float>(econ.wood) / 6.0f);
            progressText = (isEn ? "Wood: " : "Дървесина: ") + std::to_string(econ.wood) + " / 6" + (econ.wood >= 6 ? (isEn ? "  [READY!]" : "  [ГОТОВО!]") : "");
            break;

        case TutorialStep::GATHER_IRON:
            badgeText = pTag + (isEn ? ": STEP 2/7 (MINING)" : ": СТЪПКА 2/7 (СЪБИРАНЕ)");
            titleText = isEn ? "GATHER IRON FOR FRAME" : "ДОБИЙТЕ ЖЕЛЯЗО ЗА РАМКАТА";
            if (isArcade) {
                descText = isEn ? "Move onto the IRON station and press [A]."
                                : "Отидете върху станция ЖЕЛЯЗО и натиснете [A].";
            } else {
                std::string act = (player == 1 ? "[SPACE]" : "[ENTER]");
                descText = isEn ? ("Move onto the IRON station and press " + act + ".")
                                : ("Отидете върху станция ЖЕЛЯЗО и натиснете " + act + ".");
            }
            progressRatio = std::min(1.0f, static_cast<float>(econ.iron) / 4.0f);
            progressText = (isEn ? "Iron: " : "Желязо: ") + std::to_string(econ.iron) + " / 4" + (econ.iron >= 4 ? (isEn ? "  [READY!]" : "  [ГОТОВО!]") : "");
            break;

        case TutorialStep::GATHER_COPPER:
            badgeText = pTag + (isEn ? ": STEP 3/7 (MINING)" : ": СТЪПКА 3/7 (СЪБИРАНЕ)");
            titleText = isEn ? "GATHER COPPER FOR WIRES" : "ДОБИЙТЕ МЕД ЗА КАБЕЛИТЕ";
            if (isArcade) {
                descText = isEn ? "Move onto the COPPER station and press [A]."
                                : "Отидете върху станция МЕД и натиснете [A].";
            } else {
                std::string act = (player == 1 ? "[SPACE]" : "[ENTER]");
                descText = isEn ? ("Move onto the COPPER station and press " + act + ".")
                                : ("Отидете върху станция МЕД и натиснете " + act + ".");
            }
            progressRatio = std::min(1.0f, static_cast<float>(econ.copper) / 6.0f);
            progressText = (isEn ? "Copper: " : "Мед: ") + std::to_string(econ.copper) + " / 6" + (econ.copper >= 6 ? (isEn ? "  [READY!]" : "  [ГОТОВО!]") : "");
            break;

        case TutorialStep::GATHER_SILICON:
            badgeText = pTag + (isEn ? ": STEP 4/7 (MINING)" : ": СТЪПКА 4/7 (СЪБИРАНЕ)");
            titleText = isEn ? "GATHER SILICON FOR CELLS" : "ДОБИЙТЕ СИЛИЦИЙ ЗА КЛЕТКИТЕ";
            if (isArcade) {
                descText = isEn ? "Move onto the SILICON station and press [A]."
                                : "Отидете върху станция СИЛИЦИЙ и натиснете [A].";
            } else {
                std::string act = (player == 1 ? "[SPACE]" : "[ENTER]");
                descText = isEn ? ("Move onto the SILICON station and press " + act + ".")
                                : ("Отидете върху станция СИЛИЦИЙ и натиснете " + act + ".");
            }
            progressRatio = std::min(1.0f, static_cast<float>(econ.silicon) / 8.0f);
            progressText = (isEn ? "Silicon: " : "Силиций: ") + std::to_string(econ.silicon) + " / 8" + (econ.silicon >= 8 ? (isEn ? "  [READY!]" : "  [ГОТОВО!]") : "");
            break;

        case TutorialStep::SELECT_SOLAR:
            badgeText = pTag + (isEn ? ": STEP 5/7 (SELECTION)" : ": СТЪПКА 5/7 (ИЗБОР)");
            titleText = isEn ? "SELECT SOLAR PANEL" : "ИЗБЕРЕТЕ СЛЪНЧЕВ ПАНЕЛ";
            if (isArcade) {
                descText = isEn ? "Press button [D] to select the Solar Panel.\n"
                                  "Cancel selection: press button [B]!"
                                : "Натиснете бутон [D] за избор на Слънчев панел.\n"
                                  "Отказ от избор: натиснете бутон [B]!";
            } else {
                if (player == 1) {
                    descText = isEn ? "Press [E] or [1] (or click on panel icon) to select.\n"
                                      "Cancel: press [X] or right click!"
                                    : "Натиснете [E] или [1] (или щракнете върху панела).\n"
                                      "Отказ: натиснете [X] или десен бутон!";
                } else {
                    descText = isEn ? "Press [PgDn] or [KP_1] to select Solar Panel.\n"
                                      "Cancel: press [DEL]!"
                                    : "Натиснете [PgDn] или [KP_1] за избор на панел.\n"
                                      "Отказ: натиснете [DEL]!";
                }
            }
            progressRatio = 1.0f;
            progressText = isEn ? "Resources: READY!" : "Ресурси: ГОТОВИ!";
            break;

        case TutorialStep::PLACE_SOLAR:
            badgeText = pTag + (isEn ? ": STEP 6/7 (CONSTRUCTION)" : ": СТЪПКА 6/7 (СТРОИТЕЛСТВО)");
            titleText = isEn ? "PLACE PANEL ON YOUR GRID" : "ПОСТАВЕТЕ ПАНЕЛА В ГРИДА";
            if (isArcade) {
                descText = isEn ? "Move cursor with joystick onto your land and press [A]!"
                                : "Насочете курсора със стика върху вашата земя и натиснете [A]!";
                progressText = isEn ? "[A]: Place Panel" : "[A]: Постави панел";
            } else {
                if (player == 1) {
                    descText = isEn ? "Click on your land slot or move cursor and press [SPACE]!"
                                    : "Щракнете върху свободен парцел или натиснете [SPACE]!";
                    progressText = isEn ? "[SPACE] / Click: Place" : "[SPACE] / Щрак: Постави";
                } else {
                    descText = isEn ? "Move cursor with [ARROWS] onto your land and press [ENTER]!"
                                    : "Насочете курсора със [СТРЕЛКИ] върху ваша земя и [ENTER]!";
                    progressText = isEn ? "[ENTER]: Place" : "[ENTER]: Постави";
                }
            }
            progressRatio = 0.5f;
            break;

        case TutorialStep::UPGRADE_MINE:
            badgeText = pTag + (isEn ? ": STEP 7/7 (UPGRADE)" : ": СТЪПКА 7/7 (НАДГРАЖДАНЕ)");
            titleText = isEn ? "UPGRADE A RESOURCE MINE" : "НАДГРАДЕТЕ ДОБИВНА МИНА";
            if (isArcade) {
                descText = isEn ? "Move over a mine station and press Joystick Button C to upgrade!"
                                : "Отидете върху добивна станция и натиснете бутон C от джойстика за надграждане!";
                progressText = isEn ? "Joystick Button C: Upgrade" : "Бутон C от джойстика: Надграждане";
            } else {
                if (player == 1) {
                    descText = isEn ? "Hover over a mine station and press the [F] key to upgrade."
                                    : "Отидете върху добивна станция и натиснете клавиш [F] за надграждане.";
                    progressText = isEn ? "[F] Key: Upgrade" : "Клавиш [F]: Надграждане";
                } else {
                    descText = isEn ? "Hover over a mine station and press the [Shift] key to upgrade."
                                    : "Отидете върху добивна станция и натиснете клавиш [Shift] за надграждане.";
                    progressText = isEn ? "[Shift] Key: Upgrade" : "Клавиш [Shift]: Надграждане";
                }
            }
            progressRatio = 0.5f;
            break;

        case TutorialStep::COMPLETED:
            badgeText = pTag + (isEn ? ": SUCCESS!" : ": УСПЕХ!");
            titleText = isEn ? "TUTORIAL COMPLETED!" : "ОБУЧЕНИЕТО Е ЗАВЪРШЕНО!";
            if (isArcade) {
                descText = isEn ? "The panel produces +60 MW power and generates income ($)!\n"
                                  "Upgrade mines with Joystick Button C and buy land plots with [A]."
                                : "Панелът произвежда +60 MW ток и ви носи печалба ($)!\n"
                                  "Надграждайте мините с бутон C от джойстика и купувайте нови парцели с [A].";
                nextBtnLabel = isEn ? "CLOSE [A]" : "ЗАТВОРИ [A]";
            } else {
                descText = isEn ? (std::string("The panel produces +60 MW power and generates income ($)!\n") +
                                  "Upgrade mines (" + (player == 1 ? "[F]" : "[Shift]") + ") and expand with new land plots!")
                                : (std::string("Панелът произвежда +60 MW ток и ви носи печалба ($)!\n") +
                                  "Надграждайте мините (" + (player == 1 ? "[F]" : "[Shift]") + ") и купувайте нови парцели!");
                nextBtnLabel = isEn ? (player == 1 ? "CLOSE [SPACE]" : "CLOSE [ENTER]")
                                    : (player == 1 ? "ЗАТВОРИ [SPACE]" : "ЗАТВОРИ [ENTER]");
            }
            showNextBtn = true;
            break;

        default:
            break;
    }

    // Draw Badge
    sf::Vector2f badgePos(bounds.position.x + 14.0f, bounds.position.y + 10.0f);
    sf::Text& tBadge = ui::pooledText(font, toUtf8(badgeText), fontsize::Caption);
    tBadge.setStyle(sf::Text::Bold);
    tBadge.setFillColor(playerColor);
    sf::FloatRect bb = tBadge.getLocalBounds();
    sf::RectangleShape badgeBg({ bb.size.x + 14.0f, 18.0f });
    badgeBg.setPosition(badgePos);
    badgeBg.setFillColor(theme::withAlpha(playerColor, 40));
    badgeBg.setOutlineThickness(1.0f);
    badgeBg.setOutlineColor(theme::withAlpha(playerColor, 160));
    window.draw(badgeBg);
    tBadge.setPosition({ badgePos.x + 7.0f - bb.position.x, badgePos.y + 2.0f - bb.position.y });
    ui::drawText(window, tBadge, badgeBg.getGlobalBounds());

    // Draw Title
    sf::Text& tTitle = ui::pooledText(font, toUtf8(titleText), fontsize::Label);
    tTitle.setStyle(sf::Text::Bold);
    tTitle.setFillColor(theme::TextPrimary);
    tTitle.setPosition({ bounds.position.x + 14.0f, bounds.position.y + 32.0f });
    ui::drawText(window, tTitle, bounds);

    // Draw Description (Smart wrapped using ui::wrapText)
    float descW = bounds.size.x * 0.65f;
    std::string wrappedDesc = ui::wrapText(font, descText, fontsize::Caption, descW);
    sf::Text& tDesc = ui::pooledText(font, toUtf8(wrappedDesc), fontsize::Caption);
    tDesc.setFillColor(theme::TextSecondary);
    tDesc.setPosition({ bounds.position.x + 14.0f, bounds.position.y + 52.0f });
    ui::drawText(window, tDesc, bounds);

    // Draw Progress Bar (if not completed or welcome)
    if (currentStep != TutorialStep::WELCOME && currentStep != TutorialStep::COMPLETED) {
        float barX = bounds.position.x + 14.0f;
        float barY = bounds.position.y + bounds.size.y - 20.0f;
        float barW = bounds.size.x * 0.62f;
        float barH = 7.0f;

        sf::RectangleShape pBg({ barW, barH });
        pBg.setPosition({ barX, barY });
        pBg.setFillColor(theme::Well);
        window.draw(pBg);

        if (progressRatio > 0.0f) {
            sf::RectangleShape pFill({ barW * progressRatio, barH });
            pFill.setPosition({ barX, barY });
            pFill.setFillColor(playerColor);
            window.draw(pFill);
        }

        sf::Text& tProg = ui::pooledText(font, toUtf8(progressText), fontsize::Caption);
        tProg.setStyle(sf::Text::Bold);
        tProg.setFillColor(theme::TextPrimary);
        tProg.setPosition({ barX, barY - 14.0f });
        ui::drawText(window, tProg, bounds);
    }

    // Draw Skip Button (Top Right)
    sf::RectangleShape skipBtnRect(skipBtn.size);
    skipBtnRect.setPosition(skipBtn.position);
    skipBtnRect.setFillColor(theme::withAlpha(theme::Panel, 200));
    skipBtnRect.setOutlineThickness(1.0f);
    skipBtnRect.setOutlineColor(theme::Line);
    window.draw(skipBtnRect);
    sf::Text& tSkip = ui::pooledText(font, toUtf8(isArcade ? (isEn ? "SKIP [B]" : "ПРОПУСНИ [B]") : (isEn ? "SKIP [ESC]" : "ПРОПУСНИ [ESC]")), fontsize::Caption);
    tSkip.setFillColor(theme::TextSecondary);
    sf::FloatRect sb = tSkip.getLocalBounds();
    tSkip.setPosition({ skipBtn.position.x + (skipBtn.size.x - sb.size.x) / 2.0f - sb.position.x,
                        skipBtn.position.y + (skipBtn.size.y - sb.size.y) / 2.0f - sb.position.y });
    ui::drawText(window, tSkip, skipBtn);

    // Draw Next Button (Bottom Right)
    if (showNextBtn) {
        sf::RectangleShape nextBtnRect(nextBtn.size);
        nextBtnRect.setPosition(nextBtn.position);
        nextBtnRect.setFillColor(theme::withAlpha(playerColor, 60));
        nextBtnRect.setOutlineThickness(1.5f);
        nextBtnRect.setOutlineColor(playerColor);
        window.draw(nextBtnRect);
        sf::Text& tNext = ui::pooledText(font, toUtf8(nextBtnLabel), fontsize::Caption);
        tNext.setStyle(sf::Text::Bold);
        tNext.setFillColor(theme::TextPrimary);
        sf::FloatRect nb = tNext.getLocalBounds();
        tNext.setPosition({ nextBtn.position.x + (nextBtn.size.x - nb.size.x) / 2.0f - nb.position.x,
                            nextBtn.position.y + (nextBtn.size.y - nb.size.y) / 2.0f - nb.position.y });
        ui::drawText(window, tNext, nextBtn);
    }
}

void UI_tutorial::draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                       const GameEngine& engine, const UI_resourceNodes& nodes, float animTime,
                       sf::Vector2f mousePos, sf::Vector2f p1Pos, sf::Vector2f p2Pos) {
    (void)mousePos; (void)p1Pos; (void)p2Pos;
    if (!isActive() || !fontLoaded) return;

    bool isEn = (UI_settings::get().getLanguage() == "en");
    bool isArcade = ArcadeMode::isEnabled();

    // Dim background if both players or active single player is in WELCOME or COMPLETED
    bool p1Dim = (p1Active && (p1Step == TutorialStep::WELCOME || p1Step == TutorialStep::COMPLETED));
    bool p2Dim = (isCoop && p2Active && (p2Step == TutorialStep::WELCOME || p2Step == TutorialStep::COMPLETED));
    if ((!isCoop && p1Dim) || (isCoop && p1Dim && p2Dim)) {
        sf::RectangleShape dim({ 1600.0f, 900.0f });
        dim.setFillColor(theme::withAlpha(theme::Dim, 140));
        window.draw(dim);
    }

    auto getStationPos = [&](int player, TutorialStep s) -> std::pair<sf::Vector2f, std::string> {
        ResourceType rt = ResourceType::NONE;
        std::string name;
        switch (s) {
            case TutorialStep::GATHER_WOOD:    rt = ResourceType::WOOD; name = isEn ? "FOREST" : "ГОРА"; break;
            case TutorialStep::GATHER_IRON:    rt = ResourceType::IRON; name = isEn ? "IRON" : "ЖЕЛЯЗО"; break;
            case TutorialStep::GATHER_COPPER:  rt = ResourceType::COPPER; name = isEn ? "COPPER" : "МЕД"; break;
            case TutorialStep::GATHER_SILICON: rt = ResourceType::SILICON; name = isEn ? "SILICON" : "СИЛИЦИЙ"; break;
            default: break;
        }
        if (rt != ResourceType::NONE) {
            const auto* st = nodes.getStation(player, rt);
            if (st) {
                sf::Vector2f center(st->bounds.position.x + st->bounds.size.x / 2.0f, st->bounds.position.y + 35.0f);
                std::string prefix = isEn ? "MINE: " : "ДОБИВ: ";
                std::string act = isArcade ? " [A]" : (player == 1 ? " [SPACE]" : " [ENTER]");
                return { center, prefix + name + act };
            }
        }
        return { { 0.0f, 0.0f }, "" };
    };

    // Draw Objective Highlights and Arrows for Player 1
    if (p1Active && p1Step != TutorialStep::INACTIVE && p1Step != TutorialStep::WELCOME && p1Step != TutorialStep::COMPLETED) {
        if (p1Step == TutorialStep::SELECT_SOLAR) {
            sf::Vector2f arrowTarget(248.0f, 170.0f);
            std::string p1Sel = isArcade ? " [D]" : " [E]";
            drawArrow(window, arrowTarget, isEn ? ("P1: SELECT" + p1Sel) : ("P1: ИЗБЕРЕТЕ" + p1Sel), font, animTime, ArrowDir::LEFT, theme::P1);
        } else if (p1Step == TutorialStep::PLACE_SOLAR) {
            int targetCol = 1;
            int targetRow = 0;
            for (const auto& b : engine.getBuildings()) {
                if (b.playerOwner == 1) {
                    sf::Vector2f s = engine.getGridSlot(1, targetCol, targetRow);
                    if (std::abs(b.position.x - s.x) < 10.0f && std::abs(b.position.y - s.y) < 10.0f) {
                        targetRow = 1;
                        break;
                    }
                }
            }
            sf::Vector2f slot = engine.getGridSlot(1, targetCol, targetRow);
            std::string p1Place = isArcade ? " [A]" : " [SPACE]";
            drawArrow(window, slot, isEn ? ("P1: PLACE" + p1Place) : ("P1: ПОСТАВЕТЕ" + p1Place), font, animTime, ArrowDir::DOWN, theme::P1);
        } else if (p1Step == TutorialStep::UPGRADE_MINE) {
            const auto* st = nodes.getStation(1, ResourceType::WOOD);
            if (st) {
                sf::Vector2f center(st->bounds.position.x + st->bounds.size.x / 2.0f, st->bounds.position.y + 35.0f);
                std::string p1Up = isArcade ? " [C]" : " [F]";
                drawArrow(window, center, isEn ? ("P1: UPGRADE" + p1Up) : ("P1: НАДГРАДИ" + p1Up), font, animTime, ArrowDir::DOWN, theme::P1);
            }
        } else {
            auto [pos, label] = getStationPos(1, p1Step);
            if (!label.empty()) {
                drawArrow(window, pos, "P1: " + label, font, animTime, ArrowDir::DOWN, theme::P1);
            }
        }
    }

    // Draw Objective Highlights and Arrows for Player 2 (Co-op)
    if (isCoop && p2Active && p2Step != TutorialStep::INACTIVE && p2Step != TutorialStep::WELCOME && p2Step != TutorialStep::COMPLETED) {
        if (p2Step == TutorialStep::SELECT_SOLAR) {
            sf::Vector2f arrowTarget(1350.0f, 170.0f);
            std::string p2Sel = isArcade ? " [D]" : " [PgDn]";
            drawArrow(window, arrowTarget, isEn ? ("P2: SELECT" + p2Sel) : ("P2: ИЗБЕРЕТЕ" + p2Sel), font, animTime, ArrowDir::RIGHT, theme::P2);
        } else if (p2Step == TutorialStep::PLACE_SOLAR) {
            int targetCol = 7;
            int targetRow = 0;
            for (const auto& b : engine.getBuildings()) {
                if (b.playerOwner == 2) {
                    sf::Vector2f s = engine.getGridSlot(2, targetCol, targetRow);
                    if (std::abs(b.position.x - s.x) < 10.0f && std::abs(b.position.y - s.y) < 10.0f) {
                        targetRow = 1;
                        break;
                    }
                }
            }
            sf::Vector2f slot = engine.getGridSlot(2, targetCol, targetRow);
            std::string p2Place = isArcade ? " [A]" : " [ENTER]";
            drawArrow(window, slot, isEn ? ("P2: PLACE" + p2Place) : ("P2: ПОСТАВЕТЕ" + p2Place), font, animTime, ArrowDir::DOWN, theme::P2);
        } else if (p2Step == TutorialStep::UPGRADE_MINE) {
            const auto* st = nodes.getStation(2, ResourceType::WOOD);
            if (st) {
                sf::Vector2f center(st->bounds.position.x + st->bounds.size.x / 2.0f, st->bounds.position.y + 35.0f);
                std::string p2Up = isArcade ? " [C]" : " [Shift]";
                drawArrow(window, center, isEn ? ("P2: UPGRADE" + p2Up) : ("P2: НАДГРАДИ" + p2Up), font, animTime, ArrowDir::DOWN, theme::P2);
            }
        } else {
            auto [pos, label] = getStationPos(2, p2Step);
            if (!label.empty()) {
                drawArrow(window, pos, "P2: " + label, font, animTime, ArrowDir::DOWN, theme::P2);
            }
        }
    }

    // Draw Tutorial HUD Cards
    if (isCoop) {
        if (p1Active && p1Step != TutorialStep::INACTIVE) {
            drawPlayerCard(window, font, 1, p1Step, p1CardBounds, p1SkipBtnBounds, p1NextBtnBounds, engine, animTime);
        }
        if (p2Active && p2Step != TutorialStep::INACTIVE) {
            drawPlayerCard(window, font, 2, p2Step, p2CardBounds, p2SkipBtnBounds, p2NextBtnBounds, engine, animTime);
        }
    } else {
        if (p1Active && p1Step != TutorialStep::INACTIVE) {
            drawPlayerCard(window, font, 1, p1Step, cardBounds, skipBtnBounds, nextBtnBounds, engine, animTime);
        }
    }
}

bool UI_tutorial::handleClick(sf::Vector2f mousePos, GameEngine* engine) {
    if (!isActive()) return false;

    if (isCoop) {
        // Player 1 Card Clicks
        if (p1Active && p1Step != TutorialStep::INACTIVE) {
            if (p1SkipBtnBounds.contains(mousePos)) {
                skipP1(engine);
                return true;
            }
            if ((p1Step == TutorialStep::WELCOME || p1Step == TutorialStep::COMPLETED) && p1NextBtnBounds.contains(mousePos)) {
                if (p1Step == TutorialStep::WELCOME) p1Step = TutorialStep::GATHER_WOOD;
                else skipP1(engine);
                return true;
            }
        }
        // Player 2 Card Clicks
        if (p2Active && p2Step != TutorialStep::INACTIVE) {
            if (p2SkipBtnBounds.contains(mousePos)) {
                skipP2(engine);
                return true;
            }
            if ((p2Step == TutorialStep::WELCOME || p2Step == TutorialStep::COMPLETED) && p2NextBtnBounds.contains(mousePos)) {
                if (p2Step == TutorialStep::WELCOME) p2Step = TutorialStep::GATHER_WOOD;
                else skipP2(engine);
                return true;
            }
        }
    } else {
        if (p1Active && p1Step != TutorialStep::INACTIVE) {
            if (skipBtnBounds.contains(mousePos)) {
                skip(engine);
                return true;
            }
            if ((p1Step == TutorialStep::WELCOME || p1Step == TutorialStep::COMPLETED) && nextBtnBounds.contains(mousePos)) {
                if (p1Step == TutorialStep::WELCOME) p1Step = TutorialStep::GATHER_WOOD;
                else skip(engine);
                return true;
            }
        }
    }

    return false;
}

bool UI_tutorial::handleAction(int player, GameEngine* engine) {
    if (!isActive()) return false;
    if (player == 1 && p1Active) {
        if (p1Step == TutorialStep::WELCOME) {
            p1Step = TutorialStep::GATHER_WOOD;
            return true;
        } else if (p1Step == TutorialStep::COMPLETED) {
            skipP1(engine);
            return true;
        }
    } else if (player == 2 && isCoop && p2Active) {
        if (p2Step == TutorialStep::WELCOME) {
            p2Step = TutorialStep::GATHER_WOOD;
            return true;
        } else if (p2Step == TutorialStep::COMPLETED) {
            skipP2(engine);
            return true;
        }
    }
    return false;
}

bool UI_tutorial::handleSkip(int player, GameEngine* engine) {
    if (!isActive()) return false;
    if (player == 1 && p1Active) {
        skipP1(engine);
        return true;
    } else if (player == 2 && isCoop && p2Active) {
        skipP2(engine);
        return true;
    }
    return false;
}

bool UI_tutorial::handleKey(sf::Keyboard::Key key, GameEngine* engine) {
    if (!isActive()) return false;
    if (ArcadeMode::isEnabled()) return false; // Keyboard input disabled in Arcade Mode
    if (key == sf::Keyboard::Key::Space) {
        return handleAction(1, engine);
    }
    if (key == sf::Keyboard::Key::Enter) {
        return isCoop ? handleAction(2, engine) : handleAction(1, engine);
    }
    if (key == sf::Keyboard::Key::Escape) {
        skip(engine);
        return true;
    }
    return false;
}
