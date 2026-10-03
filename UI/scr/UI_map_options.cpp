// =============================================================================
// UI_map glue for match options & progression [team b-options]
//   F-03  rule numbers on the HUD (clocks, influence bar)
//   F-24 / F-35  rules strip below the city (preset, active mutators, charters)
//   F-35 / F-33 / F-24  real per-player costs on the building menus
//   F-33  research lab buildings, research panels, bot research
// =============================================================================
#include "../includes/UI_map.h"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

// --- Colours (integrator: map these to UI_theme.h tokens) ---------------------------------
const sf::Color COL_STRIP_BG(14, 20, 32, 252);
const sf::Color COL_STRIP_EDGE(70, 95, 130);
const sf::Color COL_STRIP_TITLE(255, 214, 96);
const sf::Color COL_CHIP_BG(48, 34, 70);
const sf::Color COL_CHIP_EDGE(190, 130, 255);
const sf::Color COL_CHIP_TEXT(232, 215, 255);
const sf::Color COL_P1(0, 229, 255);
const sf::Color COL_P2(255, 130, 205);
const sf::Color COL_SCIENCE(150, 120, 255);

// --- Layout: rules strip below "ЦЕНТРАЛНА ГРАНИЦА" ----------------------------------------
constexpr float STRIP_X = 612.0f;
constexpr float STRIP_W = 376.0f;
constexpr float STRIP_Y = 434.0f;
constexpr float STRIP_LINE_H = 20.0f;

constexpr float BOT_RESEARCH_INTERVAL_SEC = 2.0f;

} // namespace

void UI_map::setMatchRules(const MatchRules& rules) {
    engine.setMatchRules(rules); // applied by the next restartMatch() -> engine.init()
    research.closeAll();
    std::cout << "[UI_map] Match rules: preset " << MatchInfo::presetName(rules.preset)
              << ", mutators 0x" << std::hex << rules.mutators << std::dec
              << (rules.sandbox ? ", sandbox" : "") << "\n";
}

void UI_map::syncMatchOptionsUI(float dt) {
    p1Clock.setRules(engine.getFinalDay(), engine.getGraceDays());
    p2Clock.setRules(engine.getFinalDay(), engine.getGraceDays());
    city.setRuleInfo(engine.getGraceDays(), engine.getVictoryShare());
    p1Buildings.syncCosts(engine);
    p2Buildings.syncCosts(engine);

    // In single player only P1 is human: the bot never opens a panel
    if (bot.isActive()) research.close(2);

    // F-33: the bot spends its city money in the lab (it never had another use for money)
    if (bot.isActive() && !engine.isSandbox() && !isPaused && engine.getCityState().winner == 0) {
        botResearchTimer -= dt;
        if (botResearchTimer <= 0.0f) {
            botResearchTimer = BOT_RESEARCH_INTERVAL_SEC;
            std::string msg;
            TechState before = engine.getPlayerEconomy(2).tech;
            if (engine.autoResearch(2, msg)) {
                sf::FloatRect lab = UI_research::labRect(2);
                sf::Vector2f c(lab.position.x + lab.size.x / 2.0f, lab.position.y + 20.0f);
                spawnNotice("БОТЪТ ИЗСЛЕДВА НОВА ТЕХНОЛОГИЯ!", c, COL_SCIENCE);
                spawnMiningParticles(c, COL_SCIENCE, 24);
                // Short popup: the new tech's name and effect (the full message is too wide)
                for (int b = 0; b < TECH_BRANCHES; ++b)
                    for (int t = 0; t < TECH_TIERS; ++t)
                        if (before.choice[b][t] < 0 && engine.getTechChoice(2, b, t) >= 0) {
                            const TechInfo& info = MatchInfo::techInfo(b, t, engine.getTechChoice(2, b, t));
                            triggerPlayerPopup(2, "НАУКА", info.name, info.effect, "", COL_SCIENCE);
                        }
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Input
// -----------------------------------------------------------------------------
void UI_map::beginOptionsInputGuard() {
    // Remember the cursor of every player whose panel is open and mark their keys as held,
    // so updateControls() neither moves them nor fires a fresh action for them
    for (int pl = 1; pl <= 2; ++pl) {
        if (!research.isOpen(pl)) continue;
        int i = pl - 1;
        guardPos[i] = (pl == 1) ? p1Pos : p2Pos;
        guardGrid[i][0] = (pl == 1) ? p1GridCol : p2GridCol;
        guardGrid[i][1] = (pl == 1) ? p1GridRow : p2GridRow;
        primeInputEdges(pl);
    }
}

void UI_map::endOptionsInputGuard() {
    for (int pl = 1; pl <= 2; ++pl) {
        if (!research.isOpen(pl)) continue;
        int i = pl - 1;
        if (pl == 1) { p1Pos = guardPos[i]; p1GridCol = guardGrid[i][0]; p1GridRow = guardGrid[i][1]; }
        else         { p2Pos = guardPos[i]; p2GridCol = guardGrid[i][0]; p2GridRow = guardGrid[i][1]; }
    }
}

void UI_map::tryResearch(int player, int branch, int tier, int option) {
    std::string msg;
    bool ok = engine.researchTech(player, branch, tier, option, msg);
    research.flash(player, msg, ok);
    if (ok) {
        sf::FloatRect lab = UI_research::labRect(player);
        sf::Vector2f c(lab.position.x + lab.size.x / 2.0f, lab.position.y + 20.0f);
        spawnNotice("НОВА ТЕХНОЛОГИЯ!", c, COL_SCIENCE);
        spawnMiningParticles(c, COL_SCIENCE, 30);
    }
}

bool UI_map::handleMatchOptionsEvent(const sf::Event& event, const sf::RenderWindow& window) {
    using K = sf::Keyboard::Key;
    const bool singleHuman = bot.isActive(); // single player / sandbox: P1 also owns arrows, Enter, Del
    const bool p2Human = !bot.isActive();

    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        // Open / close the research panels: P1 [R], P2 [Home]
        if (key->code == K::R) {
            research.toggle(1);
            primeInputEdges(1);
            return true;
        }
        if (p2Human && key->code == K::Home) {
            research.toggle(2);
            primeInputEdges(2);
            return true;
        }
        if (key->code == K::Escape && research.anyOpen()) {
            research.closeAll();
            primeInputEdges(0);
            return true;
        }

        if (research.isOpen(1)) {
            int db = 0, ds = 0;
            if (key->code == K::A || (singleHuman && key->code == K::Left)) db = -1;
            else if (key->code == K::D || (singleHuman && key->code == K::Right)) db = +1;
            else if (key->code == K::W || (singleHuman && key->code == K::Up)) ds = -1;
            else if (key->code == K::S || (singleHuman && key->code == K::Down)) ds = +1;
            if (db != 0 || ds != 0) {
                research.move(1, db, ds);
                return true;
            }
            if (key->code == K::Space || (singleHuman && key->code == K::Enter)) {
                tryResearch(1, research.focusBranch(1), research.focusTier(1), research.focusOption(1));
                return true;
            }
            if (key->code == K::X || (singleHuman && (key->code == K::Delete || key->code == K::Backspace))) {
                research.close(1);
                primeInputEdges(1);
                return true;
            }
        }
        if (p2Human && research.isOpen(2)) {
            int db = 0, ds = 0;
            if (key->code == K::Left) db = -1;
            else if (key->code == K::Right) db = +1;
            else if (key->code == K::Up) ds = -1;
            else if (key->code == K::Down) ds = +1;
            if (db != 0 || ds != 0) {
                research.move(2, db, ds);
                return true;
            }
            if (key->code == K::Enter) {
                tryResearch(2, research.focusBranch(2), research.focusTier(2), research.focusOption(2));
                return true;
            }
            if (key->code == K::Delete) {
                research.close(2);
                primeInputEdges(2);
                return true;
            }
        }

        // The action key with the cursor on your own lab (and no building selected) opens it
        if ((key->code == K::Space || (singleHuman && key->code == K::Enter)) && !research.isOpen(1) && !p1Modal.active &&
            engine.getSelectedBuilding(1) == BuildingType::NONE && UI_research::labRect(1).contains(p1Pos)) {
            research.open(1);
            primeInputEdges(1);
            return true;
        }
        if (p2Human && key->code == K::Enter && !research.isOpen(2) && !p2Modal.active &&
            engine.getSelectedBuilding(2) == BuildingType::NONE && UI_research::labRect(2).contains(p2Pos)) {
            research.open(2);
            primeInputEdges(2);
            return true;
        }
        return false;
    }

    if (const auto* moved = event.getIf<sf::Event::MouseMoved>()) {
        sf::Vector2f p = window.mapPixelToCoords(moved->position);
        int owner = mouseOwnerAt(p);
        int b, t, o;
        if (owner != 0 && research.cardAt(owner, p, b, t, o)) research.setFocus(owner, b, t, o);
        return false;
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button != sf::Mouse::Button::Left) return false;
        sf::Vector2f p = window.mapPixelToCoords(mb->position);
        int owner = mouseOwnerAt(p);
        for (int pl = 1; pl <= 2; ++pl) {
            if (!research.isOpen(pl) || !UI_research::panelRect(pl).contains(p)) continue;
            if (owner == pl) {
                int b, t, o;
                if (research.cardAt(pl, p, b, t, o)) {
                    research.setFocus(pl, b, t, o);
                    tryResearch(pl, b, t, o);
                }
            }
            return true; // a click on an open panel never reaches the map below
        }
        for (int pl = 1; pl <= 2; ++pl) {
            if (owner == pl && UI_research::labRect(pl).contains(p) && !(pl == 2 && bot.isActive())) {
                research.toggle(pl);
                return true;
            }
        }
        if (owner != 0 && research.isOpen(owner)) {
            research.close(owner); // clicking the map closes your own panel
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
// Drawing
// -----------------------------------------------------------------------------
void UI_map::drawMatchOptionsWorld(sf::RenderWindow& window, float animTime) {
    const MatchRules& rules = engine.getMatchRules();

    // F-24 / F-35: what is different in this match (hidden for a plain STANDARD match)
    bool anyCharter = rules.charter[0] != CharterType::NONE || rules.charter[1] != CharterType::NONE;
    bool showStrip = !rules.sandbox && (rules.preset != MatchPreset::STANDARD || rules.mutators != 0u || anyCharter);
    if (showStrip && resourcesLoaded) {
        int lines = 1 + (rules.mutators != 0u ? 1 : 0) + (anyCharter ? 1 : 0);
        float h = 8.0f + STRIP_LINE_H * static_cast<float>(lines) + 4.0f;
        sf::RectangleShape bg({ STRIP_W, h });
        bg.setPosition({ STRIP_X, STRIP_Y });
        bg.setFillColor(COL_STRIP_BG);
        bg.setOutlineThickness(1.5f);
        bg.setOutlineColor(COL_STRIP_EDGE);
        window.draw(bg);

        const float cx = STRIP_X + STRIP_W / 2.0f;
        float y = STRIP_Y + 4.0f + STRIP_LINE_H / 2.0f;
        std::string head = std::string(MatchInfo::presetName(rules.preset)) + " · победа при " +
                           std::to_string(static_cast<int>(std::lround(rules.victoryShare * 100.0f))) + "% · " +
                           (rules.finalDay > 0 ? "до ден " + std::to_string(rules.finalDay) : std::string("без краен ден"));
        optionsTexts.draw(window, 1, font, head, 12, COL_STRIP_TITLE, cx, y, STRIP_W - 16.0f, OptionsTextCache::CENTER);
        y += STRIP_LINE_H;

        if (rules.mutators != 0u) {
            // Mutator chips, shrunk together until they fit the strip
            std::vector<std::string> names;
            for (int i = 0; i < MUTATOR_COUNT; ++i) {
                std::uint32_t f = MatchInfo::mutatorFlag(i);
                if (rules.hasMutator(f)) names.push_back(MatchInfo::mutatorName(f));
            }
            // The chip size is chosen once per mutator set (text building is the costly part)
            static std::uint32_t sizedFor = 0xFFFFFFFFu;
            static unsigned int size = 11;
            const float gap = 6.0f, padX = 6.0f;
            auto totalWidth = [&](unsigned int s) {
                float w = 0.0f;
                for (size_t k = 0; k < names.size(); ++k)
                    w += optionsTexts.get(10 + static_cast<int>(k), font, names[k], s).getLocalBounds().size.x + 2.0f * padX;
                return w + gap * static_cast<float>(names.size() - 1);
            };
            if (sizedFor != rules.mutators) {
                sizedFor = rules.mutators;
                size = 11;
                while (size > 9 && totalWidth(size) > STRIP_W - 12.0f) --size;
            }
            float x = cx - totalWidth(size) / 2.0f;
            for (size_t k = 0; k < names.size(); ++k) {
                const std::string& n = names[k];
                float w = optionsTexts.get(10 + static_cast<int>(k), font, n, size).getLocalBounds().size.x + 2.0f * padX;
                sf::RectangleShape chip({ w, 16.0f });
                chip.setPosition({ x, y - 8.0f });
                chip.setFillColor(COL_CHIP_BG);
                chip.setOutlineThickness(1.0f);
                chip.setOutlineColor(COL_CHIP_EDGE);
                window.draw(chip);
                optionsTexts.draw(window, 10 + static_cast<int>(k), font, n, size, COL_CHIP_TEXT, x + w / 2.0f, y, 100000.0f,
                                  OptionsTextCache::CENTER);
                x += w + gap;
            }
            y += STRIP_LINE_H;
        }

        if (anyCharter) {
            std::string p1 = std::string("P1: ") + MatchInfo::charterShortName(rules.charter[0]);
            std::string p2 = std::string(bot.isActive() ? "БОТ: " : "P2: ") + MatchInfo::charterShortName(rules.charter[1]);
            optionsTexts.draw(window, 20, font, p1, 12, COL_P1, STRIP_X + STRIP_W * 0.27f, y, STRIP_W * 0.46f, OptionsTextCache::CENTER);
            optionsTexts.draw(window, 21, font, p2, 12, COL_P2, STRIP_X + STRIP_W * 0.73f, y, STRIP_W * 0.46f, OptionsTextCache::CENTER);
        }
    }

    // F-33 research lab buildings
    research.drawLab(window, font, resourcesLoaded, engine, 1, animTime, UI_research::labRect(1).contains(p1Pos), "[R]", true);
    research.drawLab(window, font, resourcesLoaded, engine, 2, animTime,
                     !bot.isActive() && UI_research::labRect(2).contains(p2Pos), bot.isActive() ? "(БОТ)" : "[Home]", !bot.isActive());
}

void UI_map::drawMatchOptionsOverlays(sf::RenderWindow& window, float animTime) {
    const bool singleHuman = bot.isActive();
    research.drawPanel(window, font, resourcesLoaded, engine, 1, animTime, singleHuman ? "[SPACE/ENTER]" : "[SPACE]",
                       singleHuman ? "[WASD/стрелки] избор · [R/X] затвори" : "[W/A/S/D] избор · [R/X] затвори");
    if (!bot.isActive()) {
        research.drawPanel(window, font, resourcesLoaded, engine, 2, animTime, "[ENTER]",
                           "[стрелки] избор · [Home/Del] затвори");
    }
}

// -----------------------------------------------------------------------------
// UI_buildings: costs after perks (charter, research, Building Boom)
// -----------------------------------------------------------------------------
void UI_buildings::syncCosts(const GameEngine& engine) {
    for (auto& b : buildings) {
        if (b.type == BuildingType::DEMOLISH || b.type == BuildingType::NONE) continue;
        BuildingCost c = engine.getBuildingCostFor(playerIndex, b.type);
        b.woodCost = c.woodCost;
        b.ironCost = c.ironCost;
        b.copperCost = c.copperCost;
        b.coalCost = c.coalCost;
        b.siliconCost = c.siliconCost;
        b.silverCost = c.silverCost;
        b.oreCost = c.oreCost;
    }
}
