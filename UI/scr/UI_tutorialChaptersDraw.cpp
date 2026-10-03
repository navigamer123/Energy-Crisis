// =============================================================================
// [b-showcase] Tutorial chapters 2-3 cards (F-07) and the clock chip (UX-08)
// UI_tutorial member functions kept out of UI_tutorial.cpp to keep that file's diff small.
// =============================================================================
#include "../includes/UI_tutorial.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace {

// --- Palette (same look as the chapter-1 card; integrator: map to UI_theme tokens) ---
const sf::Color kCardBg(10, 16, 26, 248);
const sf::Color kCardEdge(0, 229, 255, 230);
const sf::Color kAccent(0, 229, 255);
const sf::Color kTitle(255, 215, 0);
const sf::Color kBody(215, 225, 235);
const sf::Color kProgressBg(30, 40, 55);
const sf::Color kProgressEdge(60, 80, 110);
const sf::Color kProgressFill(0, 229, 255);
const sf::Color kProgressDone(0, 255, 160);
const sf::Color kProgressText(255, 220, 120);
const sf::Color kProgressTextDone(100, 255, 180);
const sf::Color kButton(0, 200, 150, 220);
const sf::Color kButtonHover(0, 255, 180, 240);
const sf::Color kButtonText(10, 20, 30);
const sf::Color kSkipBg(25, 30, 42, 200);
const sf::Color kSkipBgHover(65, 30, 40, 230);
const sf::Color kSkipEdge(150, 160, 180);
const sf::Color kSkipEdgeHover(255, 100, 100);
const sf::Color kSkipText(180, 190, 200);
const sf::Color kSkipTextHover(255, 140, 140);
const sf::Color kChipBg(18, 24, 36, 240);
const sf::Color kChipHold(255, 196, 90);
const sf::Color kChipFast(0, 229, 255);
const sf::Color kChipRun(110, 255, 170);
const sf::Color kPillBg(10, 16, 26, 235);
const sf::Color kPillText(0, 255, 230);

constexpr unsigned kBodySize = 12;
constexpr float kLineStep = 15.0f;

float textWidth(const sf::Font& font, const std::string& s, unsigned size) {
    sf::Text t(font, toUtf8(s), size);
    return t.getLocalBounds().size.x;
}

// Greedy word wrap with a maximum width per line (explicit '\n' forces a break). Returns false
// when the text does not fit into the available lines.
bool wrapText(const sf::Font& font, const std::string& text, unsigned size, const std::vector<float>& widths,
              std::vector<std::string>& out) {
    out.clear();
    std::stringstream paragraphs(text);
    std::string para;
    while (std::getline(paragraphs, para, '\n')) {
        std::stringstream words(para);
        std::string word, line;
        while (words >> word) {
            std::string candidate = line.empty() ? word : line + " " + word;
            size_t idx = std::min(out.size(), widths.size() - 1);
            if (!line.empty() && textWidth(font, candidate, size) > widths[idx]) {
                out.push_back(line);
                line = word;
            } else {
                line = candidate;
            }
        }
        out.push_back(line);
    }
    if (out.size() > widths.size()) return false;
    for (size_t i = 0; i < out.size(); ++i) {
        if (textWidth(font, out[i], size) > widths[i]) return false;
    }
    return true;
}

// Card of building `index` (0 = solar .. 5 = demolish) in Player 1's build panel; mirrors the
// proportional layout of UI_buildings::setPlayer (panel at 18,115 size 230x395, 6 cards)
sf::FloatRect p1BuildCard(int index) {
    const float panelX = 18.0f, panelY = 115.0f, panelW = 230.0f, panelH = 395.0f;
    const float marginX = panelW * 0.026f;
    const float headerH = panelH * 0.071f;
    const float spacing = (panelH - headerH - panelH * 0.018f) / 6.0f;
    return sf::FloatRect({ panelX + marginX, panelY + headerH + index * spacing },
                         { panelW - 2.0f * marginX, spacing * 0.916f });
}

sf::FloatRect grow(const sf::FloatRect& r, float by) {
    return sf::FloatRect({ r.position.x - by, r.position.y - by }, { r.size.x + 2.0f * by, r.size.y + 2.0f * by });
}

bool firstBattery(const GameEngine& engine, sf::Vector2f& pos) {
    for (const auto& b : engine.getBuildings()) {
        if (b.playerOwner == 1 && b.type == BuildingType::BATTERY) {
            pos = b.position;
            return true;
        }
    }
    return false;
}

struct ChapterCard {
    std::string badge;
    std::string title;
    std::string desc;
    std::string progressText;
    float progress = -1.0f; // < 0: no progress bar
    bool showNext = false;
    std::string nextLabel;
    std::string hint;       // Key hint pill above Player 1's cursor
};

} // namespace

// -----------------------------------------------------------------------------
// UX-08 clock policy and chapter entry
// -----------------------------------------------------------------------------
float UI_tutorial::clockScale(const GameEngine& engine) const {
    if (!isActive()) return 1.0f;
    if (step == TutorialStep::CHAPTER) return chapters.clockScale(engine);
    return 0.0f; // Chapter 1 (and its welcome / result cards): the day waits for the learner
}

void UI_tutorial::startChapter(int chapter) {
    if (chapter != 2 && chapter != 3) return;
    active = true;
    step = TutorialStep::CHAPTER;
    stepDelayTimer = 0.0f;
    chapters.startChapter(chapter);
}

void UI_tutorial::drawClockChip(sf::RenderWindow& window, const sf::Font& font, const GameEngine& engine,
                                float badgeRight) {
    const float scale = clockScale(engine);
    std::string label;
    sf::Color color;
    if (scale <= 0.0f) {
        label = "ЧАСОВНИКЪТ ЧАКА";
        color = kChipHold;
    } else if (scale > 1.5f) {
        label = "ВРЕМЕ x" + std::to_string(static_cast<int>(std::lround(scale)));
        color = kChipFast;
    } else {
        label = "ВРЕМЕТО ТЕЧЕ";
        color = kChipRun;
    }
    sf::Text text(font, toUtf8(label), 10);
    sf::FloatRect lb = text.getLocalBounds();
    const float iconW = 10.0f;
    const float chipW = lb.size.x + iconW + 18.0f;
    const float chipH = 15.0f;
    const float chipX = skipBtnBounds.position.x - 10.0f - chipW;
    const float chipY = cardBounds.position.y + 6.0f; // On the badge line, clear of the title below
    if (chipX < badgeRight + 10.0f) return; // No room next to a long badge: leave it out

    sf::RectangleShape pill({ chipW, chipH });
    pill.setPosition({ chipX, chipY });
    pill.setFillColor(kChipBg);
    pill.setOutlineThickness(1.0f);
    pill.setOutlineColor(sf::Color(color.r, color.g, color.b, 170));
    window.draw(pill);

    // Icon: pause bars (held) or a play / fast-forward arrow
    const float ix = chipX + 6.0f, iy = chipY + 3.0f;
    if (scale <= 0.0f) {
        for (float dx : { 0.0f, 5.0f }) {
            sf::RectangleShape bar({ 3.0f, 9.0f });
            bar.setPosition({ ix + dx, iy });
            bar.setFillColor(color);
            window.draw(bar);
        }
    } else {
        int arrows = (scale > 1.5f) ? 2 : 1;
        for (int a = 0; a < arrows; ++a) {
            sf::ConvexShape tri(3);
            float ax = ix + a * 5.0f;
            tri.setPoint(0, { ax, iy });
            tri.setPoint(1, { ax + 5.0f, iy + 4.5f });
            tri.setPoint(2, { ax, iy + 9.0f });
            tri.setFillColor(color);
            window.draw(tri);
        }
    }
    text.setFillColor(color);
    text.setOrigin({ lb.position.x, lb.position.y + lb.size.y / 2.0f });
    text.setPosition({ std::round(chipX + iconW + 11.0f), std::round(chipY + chipH / 2.0f) });
    window.draw(text);
}

// -----------------------------------------------------------------------------
// F-07 chapter cards
// -----------------------------------------------------------------------------
void UI_tutorial::drawChapter(sf::RenderWindow& window, const sf::Font& font, const GameEngine& engine,
                              const UI_resourceNodes& nodes, float animTime, sf::Vector2f mousePos, sf::Vector2f p1Pos) {
    const ChapterStep cs = chapters.getStep();
    const auto& econ = engine.getPlayerEconomy(1);
    const float scale = chapters.clockScale(engine);
    const bool waitingForMorning = scale > 1.5f &&
        (cs == ChapterStep::C2_SELECT_BATTERY || cs == ChapterStep::C2_PLACE_BATTERY ||
         cs == ChapterStep::C3_SELECT_HYDRO || cs == ChapterStep::C3_PLACE_HYDRO);
    const std::string p = isCoop ? "P1: " : "";
    const std::string moveKeys = isCoop ? "[W/A/S/D]" : "[W/A/S/D]/[СТРЕЛКИ]";
    const int batteryPct = static_cast<int>(std::lround(TutorialChapters::batteryCharge(engine, 1) * 100.0f));
    const std::string clockText = Balance::formatHourMinute(engine.getHour24());

    // ---------------------------------------------------------------------
    // 1. Spotlight / arrow on the objective
    // ---------------------------------------------------------------------
    sf::FloatRect focusRect; // Last spotlight hole (the key tab keeps clear of it)
    bool hasFocus = false;
    auto spot = [&](const sf::FloatRect& r) {
        focusRect = r;
        hasFocus = true;
        drawSpotlight(window, r, animTime);
    };
    auto spotlightCard = [&](int index, const std::string& label) {
        sf::FloatRect card = p1BuildCard(index);
        spot(grow(card, 3.0f));
        drawArrow(window, { card.position.x + card.size.x + 4.0f, card.position.y + card.size.y / 2.0f }, label, font,
                  animTime, ArrowDir::LEFT);
    };
    // Near the slot the game draws its own ghost preview and cost label around the cursor, so the
    // arrow keeps only its pulse ring there (no label on top of the preview)
    auto onTarget = [&](const sf::Vector2f& target) {
        float dx = p1Pos.x - target.x, dy = p1Pos.y - target.y;
        float r = (engine.getSelectedBuilding(1) != BuildingType::NONE) ? 95.0f : 30.0f;
        return dx * dx + dy * dy < r * r;
    };
    auto spotlightSlot = [&](const sf::Vector2f& slot, const std::string& label) {
        spot(sf::FloatRect({ slot.x - 20.0f, slot.y - 20.0f }, { 40.0f, 40.0f }));
        drawArrow(window, slot, onTarget(slot) ? std::string() : label, font, animTime,
                  slot.y < 200.0f ? ArrowDir::UP : ArrowDir::DOWN);
    };
    auto pointAtBattery = [&]() {
        sf::Vector2f bat;
        if (firstBattery(engine, bat)) {
            drawArrow(window, bat, "ЗАРЯД: " + std::to_string(batteryPct) + "%", font, animTime,
                      bat.y < 200.0f ? ArrowDir::UP : ArrowDir::DOWN);
        }
    };

    if (!waitingForMorning) {
        sf::Vector2f slot;
        switch (cs) {
            case ChapterStep::C2_INTRO:
            case ChapterStep::C2_DONE:
            case ChapterStep::C3_INTRO:
            case ChapterStep::C3_DONE: {
                sf::RectangleShape softDim({ 1600.0f, 900.0f });
                softDim.setFillColor(sf::Color(0, 0, 0, 140));
                window.draw(softDim);
                break;
            }
            case ChapterStep::C2_SELECT_BATTERY: spotlightCard(3, "[4] БАТЕРИЯ"); break;
            case ChapterStep::C2_PLACE_BATTERY:
                if (TutorialChapters::findFreeSlot(engine, 1, 1, false, false, slot)) spotlightSlot(slot, "ТУК [SPACE]");
                break;
            case ChapterStep::C2_DUSK:
            case ChapterStep::C2_WATCH_NIGHT: pointAtBattery(); break;
            case ChapterStep::C2_SELECT_LAMP: spotlightCard(4, "[5] ЛАМПА"); break;
            case ChapterStep::C2_PLACE_LAMP:
                if (TutorialChapters::findFreeSlot(engine, 1, 1, false, false, slot)) spotlightSlot(slot, "ТУК [SPACE]");
                break;
            case ChapterStep::C2_BUILD_IN_LIGHT:
                if (TutorialChapters::findFreeSlot(engine, 1, 1, true, false, slot)) {
                    // Reveal the lamp's whole light circle: everything inside it may be built at night
                    sf::FloatRect lit({ slot.x - 20.0f, slot.y - 20.0f }, { 40.0f, 40.0f });
                    for (const auto& b : engine.getBuildings()) {
                        if (b.playerOwner == 1 && b.type == BuildingType::LAMP && b.lightRadius > 0.0f) {
                            // Clipped to the land area so the hole does not cut through the HUD panels
                            float left = std::max(b.position.x - b.lightRadius, 254.0f);
                            float top = std::max(b.position.y - b.lightRadius, 100.0f);
                            float right = std::min(b.position.x + b.lightRadius, 602.0f);
                            float bottom = std::min(b.position.y + b.lightRadius, 520.0f);
                            lit = sf::FloatRect({ left, top }, { right - left, bottom - top });
                            break;
                        }
                    }
                    spot(lit);
                    std::string label = engine.getSelectedBuilding(1) == BuildingType::NONE ? "[1] ПАНЕЛ" : "ТУК [SPACE]";
                    drawArrow(window, slot, onTarget(slot) ? std::string() : label, font, animTime,
                              slot.y < 200.0f ? ArrowDir::UP : ArrowDir::DOWN);
                }
                break;
            case ChapterStep::C3_EARN_GOLD:
                if (const auto* st = nodes.getStation(1, ResourceType::GOLD)) {
                    spot(grow(st->bounds, 5.0f));
                    drawArrow(window, { st->bounds.position.x + st->bounds.size.x + 6.0f, st->bounds.position.y + st->bounds.size.y / 2.0f },
                              "ЗЛАТО [SPACE]", font, animTime, ArrowDir::LEFT);
                }
                break;
            case ChapterStep::C3_UPGRADE_MINE:
                if (const auto* st = nodes.getStation(1, ResourceType::WOOD)) {
                    spot(grow(st->bounds, 5.0f));
                    // Inside the station the game shows its own mining prompt there: no arrow on top of it
                    if (!st->bounds.contains(p1Pos)) {
                        drawArrow(window, { st->bounds.position.x + st->bounds.size.x / 2.0f, st->bounds.position.y + 35.0f },
                                  "НАДГРАДИ [F]", font, animTime, ArrowDir::DOWN);
                    }
                }
                break;
            case ChapterStep::C3_BUY_PLOT:
                if (const LandPlot* plot = TutorialChapters::findPlot(engine, TutorialChapters::riverPlotId(1))) {
                    spot(grow(plot->bounds, 3.0f));
                    sf::Vector2f c(plot->bounds.position.x + plot->bounds.size.x / 2.0f, plot->bounds.position.y + plot->bounds.size.y / 2.0f);
                    drawArrow(window, c, "КУПИ [SPACE]", font, animTime, ArrowDir::UP);
                }
                break;
            case ChapterStep::C3_SELECT_HYDRO: spotlightCard(2, "[3] ВЕЦ"); break;
            case ChapterStep::C3_PLACE_HYDRO:
                if (TutorialChapters::findFreeSlot(engine, 1, TutorialChapters::riverPlotId(1), false, true, slot)) {
                    spotlightSlot(slot, "ТУК [SPACE]");
                }
                break;
            default:
                break;
        }
    }

    // ---------------------------------------------------------------------
    // 2. Card texts
    // ---------------------------------------------------------------------
    ChapterCard c;
    const int n = chapters.stepNumber(), total = chapters.stepCount();
    const std::string stepBadge = "ГЛАВА " + std::to_string(chapters.getChapter()) + " · СТЪПКА " +
                                  std::to_string(n) + " / " + std::to_string(total);
    switch (cs) {
        case ChapterStep::C2_INTRO:
            c.badge = "ГЛАВА 2: НОЩ И БАТЕРИИ";
            c.title = "ТОК ЗА ГРАДА И ПРЕЗ НОЩТА";
            c.desc = "Нощем слънчевите панели спират, а строежът е възможен само на светло.\n"
                     "Ще построите батерия, лампа и панел.\n"
                     "Общината дава материалите и на двамата играчи.";
            c.showNext = true;
            c.nextLabel = "НАПРЕД [SPACE]";
            c.hint = p + "[SPACE] Напред  |  [ESC] Към играта";
            break;
        case ChapterStep::C2_SELECT_BATTERY:
            c.badge = stepBadge;
            c.title = "ИЗБЕРЕТЕ БАТЕРИЯ";
            c.desc = "Батерията пази излишния ток от деня и го отдава нощем.\nНатиснете [4], за да я изберете.";
            c.hint = p + "[4] Батерия";
            break;
        case ChapterStep::C2_PLACE_BATTERY:
            c.badge = stepBadge;
            c.title = "ПОСТАВЕТЕ БАТЕРИЯТА";
            c.desc = "Преместете курсора на маркираната клетка и натиснете [SPACE].\nНовата батерия започва с 0% заряд.";
            c.hint = p + moveKeys + " Клетка  |  [SPACE] Строеж  |  [X] Отказ";
            break;
        case ChapterStep::C2_DUSK:
            c.badge = stepBadge;
            c.title = "ВРЕМЕТО ТЕЧЕ x12 ДО ЗАЛЕЗ";
            c.desc = "Панелът зарежда батерията с излишъка (до 40 MW).\nГледайте как се пълни, докато слънцето залязва.";
            c.progress = batteryPct / 100.0f;
            c.progressText = "Батерия: " + std::to_string(batteryPct) + "%  ·  Час: " + clockText;
            break;
        case ChapterStep::C2_SELECT_LAMP:
            c.badge = stepBadge;
            c.title = "НОЩ Е! ИЗБЕРЕТЕ ЛАМПА";
            c.desc = "На тъмно не се строи. Лампата осветява кръг от 150 px и харчи 10 MW от батерията.\n"
                     "Натиснете [5].";
            c.hint = p + "[5] Осветителна лампа";
            break;
        case ChapterStep::C2_PLACE_LAMP:
            c.badge = stepBadge;
            c.title = "ПОСТАВЕТЕ ЛАМПАТА";
            c.desc = "Лампа може да се поставя и нощем.\nИзберете клетка и натиснете [SPACE].";
            c.hint = p + moveKeys + " Клетка  |  [SPACE] Строеж  |  [X] Отказ";
            break;
        case ChapterStep::C2_BUILD_IN_LIGHT:
            c.badge = stepBadge;
            c.title = "СТРОЙТЕ В ОСВЕТЕНИЯ КРЪГ";
            c.desc = "Натиснете [1] за Слънчев панел и го поставете в светлината на лампата.\n"
                     "Извън кръга строежът нощем е забранен.";
            c.hint = p + "[1] Панел  |  [SPACE] Строеж  |  [X] Отказ";
            break;
        case ChapterStep::C2_WATCH_NIGHT:
            c.badge = stepBadge;
            c.title = "НОЩТА МИНАВА x12";
            c.desc = "Батерията захранва лампата. В 06:00 градът оценява средния ток за целия ден, "
                     "затова батериите пазят победата през нощта.";
            c.progress = batteryPct / 100.0f;
            c.progressText = "Батерия: " + std::to_string(batteryPct) + "%  ·  Час: " + clockText;
            break;
        case ChapterStep::C2_DONE:
            c.badge = "ГЛАВА 2 ЗАВЪРШЕНА!";
            c.title = "ПРЕЖИВЯХТЕ ПЪРВАТА НОЩ";
            c.desc = (chapters.getWatchedSettlement().empty() ? std::string("Денят приключи.")
                                                               : chapters.getWatchedSettlement()) +
                     "\nГлава 3: земя, злато и мини.";
            c.showNext = true;
            c.nextLabel = "ГЛАВА 3 [SPACE]";
            c.hint = p + "[SPACE] Глава 3  |  [ESC] Към играта";
            break;
        case ChapterStep::C3_INTRO:
            c.badge = "ГЛАВА 3: ЗЕМЯ И МИНИ";
            c.title = "ЗЛАТОТО КУПУВА ЗЕМЯ И МИНИ";
            c.desc = "Злато идва от златната жила и като дивидент за доставения ток. "
                     "С него надграждате мини (+75% добив) и купувате парцели.";
            c.showNext = true;
            c.nextLabel = "НАПРЕД [SPACE]";
            c.hint = p + "[SPACE] Напред  |  [ESC] Към играта";
            break;
        case ChapterStep::C3_EARN_GOLD:
            c.badge = stepBadge;
            c.title = "СЪБЕРЕТЕ 30 ЗЛАТО";
            c.desc = "Добивайте от станция ЗЛАТО с [SPACE] или изчакайте дивидентите.\n"
                     "Тук времето тече нормално.";
            c.progress = std::min(1.0f, econ.gold / static_cast<float>(TutorialChapters::GOLD_TARGET));
            c.progressText = "Злато: " + std::to_string(std::min(econ.gold, 9999)) + " / " +
                             std::to_string(TutorialChapters::GOLD_TARGET) + (econ.gold >= TutorialChapters::GOLD_TARGET ? "  [ГОТОВО!]" : "");
            c.hint = p + moveKeys + " Движение  |  [SPACE] Добив";
            break;
        case ChapterStep::C3_UPGRADE_MINE:
            c.badge = stepBadge;
            c.title = "НАДГРАДЕТЕ МИНА";
            c.desc = "Застанете върху станция ГОРА и натиснете [F].\nВсяко ниво дава +75% добив завинаги.";
            c.hint = p + "[F] Надгради мината";
            break;
        case ChapterStep::C3_BUY_PLOT: {
            const LandPlot* plot = TutorialChapters::findPlot(engine, TutorialChapters::riverPlotId(1));
            int cost = plot ? plot->costGold : 0;
            c.badge = stepBadge;
            c.title = "КУПЕТЕ ПАРЦЕЛ ДО РЕКАТА";
            c.desc = "Общината дава " + std::to_string(cost) + " G на двамата. ВЕЦ се строи само до реката.\n"
                     "Застанете върху парцела и натиснете [SPACE].";
            c.progress = cost > 0 ? std::min(1.0f, econ.gold / static_cast<float>(cost)) : 1.0f;
            c.progressText = "Злато: " + std::to_string(std::min(econ.gold, 9999)) + " / " + std::to_string(cost) + " G";
            c.hint = p + "[SPACE] Купи парцела";
            break;
        }
        case ChapterStep::C3_SELECT_HYDRO:
            c.badge = stepBadge;
            c.title = "ИЗБЕРЕТЕ ВЕЦ";
            c.desc = "ВЕЦ дава 110 MW денем и нощем, а в дъжд още повече.\nМатериалите са осигурени. Натиснете [3].";
            c.hint = p + "[3] ВЕЦ";
            break;
        case ChapterStep::C3_PLACE_HYDRO:
            c.badge = stepBadge;
            c.title = "ПОСТАВЕТЕ ВЕЦ ДО РЕКАТА";
            c.desc = "Изберете клетка в новия парцел до реката\nи натиснете [SPACE].";
            c.hint = p + moveKeys + " Клетка  |  [SPACE] Строеж  |  [X] Отказ";
            break;
        case ChapterStep::C3_DONE:
            c.badge = "ОБУЧЕНИЕТО Е ЗАВЪРШЕНО!";
            c.title = "ГОТОВИ СТЕ ЗА ПОБЕДА";
            c.desc = "Захранвайте града по-добре от съперника и спечелете 85% от него.\n"
                     "Нуждата на града расте всеки ден. Успех!";
            c.showNext = true;
            c.nextLabel = "КЪМ ИГРАТА [SPACE]";
            c.hint = p + "[SPACE] Към играта";
            break;
        default:
            break;
    }
    if (waitingForMorning) {
        c.title = "ИЗЧАКАЙТЕ СУТРИНТА";
        c.desc = "Нощем се строи само на светло.\nВремето тече x12 до изгрева.";
        c.progress = -1.0f;
        c.progressText.clear();
        c.hint.clear();
    }

    // ---------------------------------------------------------------------
    // 3. Key tab on top of the card (fixed, so it never collides with the game's own prompts,
    //    ghost previews or popups around the cursor)
    // ---------------------------------------------------------------------
    if (!c.hint.empty()) {
        sf::Text tag(font, toUtf8(c.hint), 11);
        tag.setFillColor(kPillText);
        sf::FloatRect tb = tag.getLocalBounds();
        const float tabW = std::min(tb.size.x + 16.0f, cardBounds.size.x - 24.0f);
        const float tabH = 17.0f;
        const float tabY = cardBounds.position.y - tabH - 2.0f;
        // Left-aligned on the card; right-aligned when that would touch the spotlight hole; left out
        // when both would (the card itself names the keys too)
        auto touchesFocus = [&](float x) {
            if (!hasFocus) return false;
            sf::FloatRect tabRect({ x - 4.0f, tabY - 4.0f }, { tabW + 8.0f, tabH + 8.0f });
            return tabRect.findIntersection(grow(focusRect, 4.0f)).has_value();
        };
        float tabX = cardBounds.position.x + 12.0f;
        if (touchesFocus(tabX)) tabX = cardBounds.position.x + cardBounds.size.x - 12.0f - tabW;
        const bool showTab = !touchesFocus(tabX);
        if (showTab) {
            sf::RectangleShape tab({ tabW, tabH });
            tab.setPosition({ tabX, tabY });
            tab.setFillColor(kPillBg);
            tab.setOutlineThickness(1.0f);
            tab.setOutlineColor(sf::Color(0, 229, 255, 200));
            window.draw(tab);
            if (tb.size.x > tabW - 16.0f) tag.setScale({ (tabW - 16.0f) / tb.size.x, (tabW - 16.0f) / tb.size.x });
            tag.setOrigin({ tb.position.x, tb.position.y + tb.size.y / 2.0f });
            tag.setPosition({ std::round(tabX + 8.0f), std::round(tabY + tabH / 2.0f) });
            window.draw(tag);
        }
    }

    // ---------------------------------------------------------------------
    // 4. Card frame, skip button, texts, progress, next button
    // ---------------------------------------------------------------------
    const float cx = cardBounds.position.x, cy = cardBounds.position.y;
    const float cw = cardBounds.size.x, ch = cardBounds.size.y;
    sf::RectangleShape card(cardBounds.size);
    card.setPosition(cardBounds.position);
    card.setFillColor(kCardBg);
    card.setOutlineThickness(2.0f);
    card.setOutlineColor(kCardEdge);
    window.draw(card);
    sf::RectangleShape topBar({ cw, 3.0f });
    topBar.setPosition(cardBounds.position);
    topBar.setFillColor(kAccent);
    window.draw(topBar);

    const bool hoverSkip = skipBtnBounds.contains(mousePos);
    sf::RectangleShape skipBtn(skipBtnBounds.size);
    skipBtn.setPosition(skipBtnBounds.position);
    skipBtn.setFillColor(hoverSkip ? kSkipBgHover : kSkipBg);
    skipBtn.setOutlineThickness(1.0f);
    skipBtn.setOutlineColor(hoverSkip ? kSkipEdgeHover : kSkipEdge);
    window.draw(skipBtn);
    sf::Text skipText(font, toUtf8(chapters.isDialog() ? "КЪМ ИГРАТА [ESC]" : "ПРОПУСНИ [ESC]"), 11);
    skipText.setFillColor(hoverSkip ? kSkipTextHover : kSkipText);
    sf::FloatRect stb = skipText.getLocalBounds();
    skipText.setOrigin({ stb.position.x + stb.size.x / 2.0f, stb.position.y + stb.size.y / 2.0f });
    skipText.setPosition({ std::round(skipBtnBounds.position.x + skipBtnBounds.size.x / 2.0f),
                           std::round(skipBtnBounds.position.y + skipBtnBounds.size.y / 2.0f) });
    window.draw(skipText);

    sf::Text badge(font, toUtf8(c.badge), 11);
    badge.setFillColor(kAccent);
    badge.setPosition({ cx + 16.0f, cy + 9.0f });
    window.draw(badge);
    drawClockChip(window, font, engine, badge.getGlobalBounds().position.x + badge.getGlobalBounds().size.x);

    // Title: must stay left of the skip button (they share the top band)
    sf::Text title(font, toUtf8(c.title), 15);
    title.setFillColor(kTitle);
    float titleMax = skipBtnBounds.position.x - (cx + 16.0f) - 10.0f;
    float tw = title.getLocalBounds().size.x;
    if (tw > titleMax) title.setScale({ titleMax / tw, titleMax / tw });
    title.setPosition({ cx + 16.0f, cy + 26.0f });
    window.draw(title);

    // Description: three lines; the last one stays clear of the next button
    sf::FloatRect nextRect = nextBtnBounds;
    const float fullW = cw - 32.0f;
    const float besideBtn = nextRect.position.x - (cx + 16.0f) - 12.0f;
    const bool hasProgress = c.progress >= 0.0f;
    std::vector<float> widths = { fullW, fullW, c.showNext ? besideBtn : fullW };
    if (hasProgress) widths.pop_back(); // The progress row takes the third line
    std::vector<std::string> lines;
    unsigned bodySize = kBodySize;
    if (!wrapText(font, c.desc, bodySize, widths, lines)) {
        bodySize = 11;
        if (!wrapText(font, c.desc, bodySize, widths, lines)) {
            lines.resize(std::min(lines.size(), widths.size())); // Never draw outside the card
        }
    }
    for (size_t i = 0; i < lines.size(); ++i) {
        sf::Text line(font, toUtf8(lines[i]), bodySize);
        line.setFillColor(kBody);
        line.setPosition({ cx + 16.0f, cy + 47.0f + i * kLineStep });
        window.draw(line);
    }

    if (hasProgress) {
        const float barX = cx + 16.0f, barY = cy + ch - 20.0f, barW = 230.0f, barH = 9.0f;
        sf::RectangleShape barBg({ barW, barH });
        barBg.setPosition({ barX, barY });
        barBg.setFillColor(kProgressBg);
        barBg.setOutlineThickness(1.0f);
        barBg.setOutlineColor(kProgressEdge);
        window.draw(barBg);
        float ratio = std::clamp(c.progress, 0.0f, 1.0f);
        if (ratio > 0.0f) {
            sf::RectangleShape fill({ barW * ratio, barH });
            fill.setPosition({ barX, barY });
            fill.setFillColor(ratio >= 1.0f ? kProgressDone : kProgressFill);
            window.draw(fill);
        }
        sf::Text prog(font, toUtf8(c.progressText), 11);
        prog.setFillColor(ratio >= 1.0f ? kProgressTextDone : kProgressText);
        float maxW = cx + cw - 16.0f - (barX + barW + 12.0f);
        float pw = prog.getLocalBounds().size.x;
        if (pw > maxW) prog.setScale({ maxW / pw, maxW / pw });
        sf::FloatRect pb = prog.getLocalBounds();
        prog.setOrigin({ pb.position.x, pb.position.y + pb.size.y / 2.0f });
        prog.setPosition({ barX + barW + 12.0f, std::round(barY + barH / 2.0f) });
        window.draw(prog);
    }

    if (c.showNext) {
        bool hoverNext = nextRect.contains(mousePos);
        sf::RectangleShape nextBtn(nextRect.size);
        nextBtn.setPosition(nextRect.position);
        nextBtn.setFillColor(hoverNext ? kButtonHover : kButton);
        nextBtn.setOutlineThickness(1.5f);
        nextBtn.setOutlineColor(sf::Color::White);
        window.draw(nextBtn);
        sf::Text nxt(font, toUtf8(c.nextLabel), 12);
        nxt.setFillColor(kButtonText);
        float nw = nxt.getLocalBounds().size.x;
        if (nw > nextRect.size.x - 12.0f) nxt.setScale({ (nextRect.size.x - 12.0f) / nw, (nextRect.size.x - 12.0f) / nw });
        sf::FloatRect nb = nxt.getLocalBounds();
        nxt.setOrigin({ nb.position.x + nb.size.x / 2.0f, nb.position.y + nb.size.y / 2.0f });
        nxt.setPosition({ std::round(nextRect.position.x + nextRect.size.x / 2.0f),
                          std::round(nextRect.position.y + nextRect.size.y / 2.0f) });
        window.draw(nxt);
    }
}
