// =============================================================================
// Team b-session: UI_map session features, kept out of the shared UI_map files.
//   F-02  continue an unfinished match (hasMatchInProgress, openPauseMenu)
//   F-05  gamepads: dialog/tutorial presses, connect notice, auto-pause on disconnect
//   F-06  tutorial policy, settings overlay in the pause menu
//   F-10  generated key hints
//   F-18  disk saves: 3 slots, quicksave F5 / quickload F9, autosave at day end
//   UX-12 UI-scale zoom views for the pause, victory and help cards
// =============================================================================
#include "../includes/UI_map.h"
#include "../includes/UI_input.h"
#include "../includes/UI_layout.h"
#include "../includes/UI_paths.h"
#include "../includes/UI_settings.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
const sf::Color TOAST_OK(90, 235, 160);
const sf::Color TOAST_ERR(255, 110, 110);
const sf::Color PAD_COLOR(170, 140, 255);
constexpr float TOAST_SECONDS = 4.0f;

sf::FloatRect grow(sf::FloatRect r, float by) {
    r.position -= sf::Vector2f(by, by);
    r.size += sf::Vector2f(2.0f * by, 2.0f * by);
    return r;
}
} // namespace

// -----------------------------------------------------------------------------
// F-02: continue
// -----------------------------------------------------------------------------
bool UI_map::hasMatchInProgress() const {
    return matchStarted && engine.getCityState().winner == 0;
}

std::string UI_map::matchSummary() const {
    SaveInfo i;
    i.exists = i.valid = true;
    i.day = engine.getCurrentDay();
    i.hour = engine.getHour24();
    i.difficulty = static_cast<int>(bot.getDifficulty());
    i.p1Share = engine.getCityState().p1CityShare;
    return saves::shortLabel(i);
}

void UI_map::openPauseMenu() {
    isPaused = true;
    pauseSelectedIdx = PAUSE_RESUME;
    lastPauseMousePos = { -999.0f, -999.0f };
    showHelpOverlay = false;
    helpOpenedFromPause = false;
    requestMenu = false;
    deltaClock.restart(); // the time spent in the main menu must not reach the simulation
    primeInputEdges(0);
}

bool UI_map::wantsMenuInput() const {
    return isPaused || showHelpOverlay || engine.getCityState().winner != 0 || settingsOverlay.isOpen() || savePanelOpen;
}

// -----------------------------------------------------------------------------
// F-06: tutorial policy and per-frame bookkeeping
// -----------------------------------------------------------------------------
bool UI_map::shouldShowTutorial(BotDifficulty diff) const {
    const GameSettings& s = gameSettings();
    switch (s.tutorialPolicy) {
        case TutorialPolicy::Never: return false;
        case TutorialPolicy::Always: return true;
        case TutorialPolicy::Auto:
        default:
            if (diff == BotDifficulty::HARD) return false; // advanced players
            return (diff == BotDifficulty::NONE) ? !s.tutorialDoneCoop : !s.tutorialDoneSolo;
    }
}

void UI_map::updateSession(float dt) {
    (void)dt;
    inputRouter().setMatchContext(bot.isActive(), controlScheme);
    inputRouter().refreshAssignment();

    // AUTO tutorial policy: once finished or skipped, that mode no longer shows it
    if (tutorialPolicyWatch && !tutorial.isActive()) {
        tutorialPolicyWatch = false;
        GameSettings& s = gameSettings();
        bool& done = tutorialWatchCoop ? s.tutorialDoneCoop : s.tutorialDoneSolo;
        if (!done) {
            done = true;
            saveGameSettings();
        }
    }

    // Autosave right after every day-end settlement
    if (matchStarted && engine.getCurrentDay() != autosaveDay) {
        autosaveDay = engine.getCurrentDay();
        if (engine.getCityState().winner == 0 && writeAutosave()) {
            setSaveStatus("Автозапис: ден " + std::to_string(autosaveDay), false);
        }
    }

    // Overlays cannot be drawn without the font: never leave one blocking the input invisibly
    if (!resourcesLoaded) {
        if (settingsOverlay.isOpen()) settingsOverlay.close();
        savePanelOpen = false;
    }
}

// -----------------------------------------------------------------------------
// Event routing for the session overlays and the save shortcuts
// -----------------------------------------------------------------------------
bool UI_map::handleSessionEvent(const sf::Event& event, const sf::RenderWindow& window) {
    if (settingsOverlay.isOpen()) {
        settingsOverlay.handleEvent(event, window);
        if (!settingsOverlay.isOpen()) primeInputEdges(0);
        return true;
    }
    if (savePanelOpen) {
        handleSavePanelEvent(event, window);
        return true;
    }
    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        if (key->code == sf::Keyboard::Key::F5 && !showHelpOverlay) {
            quickSave();
            return true;
        }
        if (key->code == sf::Keyboard::Key::F9 && !showHelpOverlay) {
            quickLoad();
            return true;
        }
    }
    return false;
}

void UI_map::activatePauseOption(int option) {
    switch (option) {
        case PAUSE_RESUME:
            isPaused = false;
            primeInputEdges(0);
            break;
        case PAUSE_SAVELOAD:
            openSavePanel();
            break;
        case PAUSE_SETTINGS:
            settingsOverlay.open(true);
            break;
        case PAUSE_RESTART:
            isPaused = false;
            restartMatch();
            break;
        case PAUSE_HELP:
            helpOpenedFromPause = true;
            showHelpOverlay = true;
            break;
        case PAUSE_MENU:
            isPaused = false;
            requestMenu = true;
            break;
        default:
            break;
    }
}

// -----------------------------------------------------------------------------
// UX-12: zoom views for the shared overlays
// -----------------------------------------------------------------------------
sf::View UI_map::pauseView(const sf::RenderWindow& window) const {
    sf::FloatRect box = ui::pause::box(PAUSE_COUNT);
    box.position.y -= 48.0f; // room for the status banner above the card
    box.size.y += 48.0f;
    return ui::overlayViewNoShrink(window.getView(), grow(box, 12.0f));
}

sf::View UI_map::victoryView(const sf::RenderWindow& window) const {
    return ui::overlayViewNoShrink(window.getView(), grow(sf::FloatRect({ 430.0f, 210.0f }, { 740.0f, 480.0f }), 12.0f));
}

sf::View UI_map::helpView(const sf::RenderWindow& window) const {
    return ui::overlayViewNoShrink(window.getView(), grow(sf::FloatRect({ 220.0f, 80.0f }, { 1160.0f, 740.0f }), 8.0f));
}

// -----------------------------------------------------------------------------
// F-10: generated key hints
// -----------------------------------------------------------------------------
std::string UI_map::keyHint(int player, InputAction action) const {
    return inputRouter().hint(player, action);
}

std::string UI_map::hudHintText() const {
    const InputRouter& r = inputRouter();
    // Primary keys only (Single Player: the P1 and P2 primaries), so the bar stays short and large
    const bool solo = bot.isActive();
    auto keys = [&r, solo](int p, InputAction a) { return r.hint(p, a, false, (solo && p == 1) ? 2 : 1); };
    auto side = [&](int p) {
        bool mouse = (controlScheme == ControlScheme::BOTH_MOUSE) ||
                     (p == 1 && (controlScheme == ControlScheme::P1_MOUSE_P2_KEYBOARD || bot.isActive())) ||
                     (p == 2 && controlScheme == ControlScheme::P1_KEYBOARD_P2_MOUSE);
        std::string act = keys(p, InputAction::Action);
        if (mouse) act = act.substr(0, act.size() - 1) + "/Клик]";
        std::string s = keys(p, InputAction::NextBuilding) + "/" + keys(p, InputAction::PrevBuilding) + " Сграда | " +
                        keys(p, InputAction::Cancel) + " Разруши/Отказ | " + act + " Действие";
        if (r.padFor(p) >= 0) s += " | геймпад";
        return s;
    };
    if (bot.isActive()) {
        return "ИГРАЧ: " + side(1) + " | " + keys(1, InputAction::Upgrade) + " Ъпгрейд  ///  ИЗТОК: БОТ";
    }
    return "P1: " + side(1) + "  ///  P2: " + side(2);
}

// -----------------------------------------------------------------------------
// F-18: files
// -----------------------------------------------------------------------------
bool UI_map::saveToFile(const std::string& path, std::string& error) const {
    if (!matchStarted) {
        error = "няма започнат мач";
        return false;
    }
    if (path.empty()) {
        error = "няма папка за записи";
        return false;
    }
    SaveInfo meta;
    meta.savedAt = static_cast<long long>(std::time(nullptr));
    meta.day = engine.getCurrentDay();
    meta.hour = engine.getHour24();
    meta.difficulty = static_cast<int>(bot.getDifficulty());
    meta.scheme = static_cast<int>(controlScheme);
    meta.p1Share = engine.getCityState().p1CityShare;

    std::ostringstream o;
    o << saves::headerText(meta);
    o << std::setprecision(9);
    o << "ui.p1Pos=" << p1Pos.x << "," << p1Pos.y << "\n";
    o << "ui.p2Pos=" << p2Pos.x << "," << p2Pos.y << "\n";
    o << "ui.lightningP1=" << lightningStrikeCooldown << "\n";
    o << "ui.lightningP2=" << lightningStrikeCooldownP2 << "\n";
    o << "[engine]\n";
    if (!engine.saveSnapshot(o)) {
        error = "грешка в състоянието на играта";
        return false;
    }
    if (!ecfs::writeTextAtomic(path, o.str())) {
        error = "файлът не може да се запише";
        return false;
    }
    std::cout << "[UI_map] Saved day " << meta.day << " to " << path << "\n";
    return true;
}

bool UI_map::loadFromFile(const std::string& path, std::string& error) {
    std::string text;
    if (path.empty() || !ecfs::readText(path, text)) {
        error = "файлът липсва";
        return false;
    }
    std::istringstream in(text);
    std::string line;
    std::getline(in, line);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != "ECSAVE 1") {
        error = "непознат формат на файла";
        return false;
    }
    int difficulty = 0, scheme = 0;
    sf::Vector2f pos1 = p1Pos, pos2 = p2Pos;
    float light1 = 5.0f, light2 = 5.0f;
    auto readVec = [](const std::string& v, sf::Vector2f& out) {
        size_t c = v.find(',');
        if (c == std::string::npos) return;
        out = { static_cast<float>(std::atof(v.substr(0, c).c_str())), static_cast<float>(std::atof(v.substr(c + 1).c_str())) };
    };
    bool sawEngine = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == "[engine]") { sawEngine = true; break; }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "meta.difficulty") difficulty = std::atoi(v.c_str());
        else if (k == "meta.scheme") scheme = std::atoi(v.c_str());
        else if (k == "ui.p1Pos") readVec(v, pos1);
        else if (k == "ui.p2Pos") readVec(v, pos2);
        else if (k == "ui.lightningP1") light1 = static_cast<float>(std::atof(v.c_str()));
        else if (k == "ui.lightningP2") light2 = static_cast<float>(std::atof(v.c_str()));
    }
    if (!sawEngine || difficulty < 0 || difficulty > 3 || scheme < 0 || scheme > 3) {
        error = "повреден заглавен блок";
        return false;
    }
    GameEngine loaded;
    std::string engineError;
    if (!loaded.loadSnapshot(in, &engineError)) {
        error = "повреден запис (" + engineError + ")";
        return false;
    }

    // Everything is valid: rebuild the match exactly like a fresh start, then drop the state in
    const bool wasPaused = isPaused;
    bot.init(static_cast<BotDifficulty>(difficulty));
    setControlScheme(static_cast<ControlScheme>(scheme));
    restartMatch(); // per-match UI reset: overlays, cooldowns, dialogs, bot, tutorial, input edges
    engine = loaded;
    tutorial.skip();
    tutorialPolicyWatch = false;
    p1Pos = { std::max(30.0f, std::min(pos1.x, 780.0f)), std::max(40.0f, std::min(pos1.y, 860.0f)) };
    p2Pos = { std::max(820.0f, std::min(pos2.x, 1570.0f)), std::max(40.0f, std::min(pos2.y, 860.0f)) };
    engine.getClosestGridIndex(1, p1Pos, p1GridCol, p1GridRow);
    engine.getClosestGridIndex(2, p2Pos, p2GridCol, p2GridRow);
    lightningStrikeCooldown = std::max(0.5f, std::min(light1, 60.0f));
    lightningStrikeCooldownP2 = std::max(0.5f, std::min(light2, 60.0f));
    notices.clear();
    matchStarted = true;
    autosaveDay = engine.getCurrentDay();
    isPaused = wasPaused;
    inputRouter().setMatchContext(bot.isActive(), controlScheme);
    deltaClock.restart();
    std::cout << "[UI_map] Loaded day " << engine.getCurrentDay() << " from " << path << "\n";
    return true;
}

bool UI_map::writeAutosave() {
    if (!gameSettings().autosave || !hasMatchInProgress()) return false;
    if (engine.getCurrentDay() == lastAutosaveDay && std::fabs(engine.getHour24() - lastAutosaveHour) < 0.01f) return true; // already saved
    int slot = saves::nextAutosaveSlot();
    std::string err;
    bool ok = saveToFile(saves::pathFor(slot), err);
    if (ok) {
        lastAutosaveDay = engine.getCurrentDay();
        lastAutosaveHour = engine.getHour24();
    } else {
        std::cerr << "[UI_map] Autosave failed: " << err << "\n";
    }
    return ok;
}

// -----------------------------------------------------------------------------
// F-18: quicksave / quickload and the save panel
// -----------------------------------------------------------------------------
void UI_map::setSaveStatus(const std::string& text, bool error) {
    saveStatus = text;
    saveStatusError = error;
    saveStatusClock.restart();
}

void UI_map::quickSave() {
    if (!hasMatchInProgress()) {
        setSaveStatus("Няма активен мач за запис.", true);
        return;
    }
    std::string err;
    if (saveToFile(saves::pathFor(saves::QUICK_SLOT), err)) setSaveStatus("Бърз запис: ден " + std::to_string(engine.getCurrentDay()) + "  (F9 зарежда)", false);
    else setSaveStatus("Грешка при запис: " + err, true);
}

void UI_map::quickLoad() {
    SaveInfo info = saves::readInfo(saves::pathFor(saves::QUICK_SLOT));
    if (!info.valid) {
        setSaveStatus(info.exists ? "Бързият запис е повреден." : "Няма бърз запис (F5 записва).", true);
        return;
    }
    std::string err;
    if (loadFromFile(info.path, err)) {
        setSaveStatus("Зареден бърз запис: " + saves::describe(info, false), false);
        primeInputEdges(0);
    } else {
        setSaveStatus("Грешка при зареждане: " + err, true);
    }
}

void UI_map::refreshSaveSlots() {
    for (int s = 0; s < saves::SLOT_COUNT; ++s) saveSlots[s] = saves::readInfo(saves::pathFor(s));
}

void UI_map::openSavePanel() {
    refreshSaveSlots();
    savePanelOpen = true;
    saveRow = 0;
    saveCol = 0;
    saveOverwriteArmed = -1;
    lastSaveMouse = { -999.0f, -999.0f };
    setSaveStatus(saves::dir().empty() ? "Няма папка за записи: записите не са достъпни." : "Изберете слот.", saves::dir().empty());
}

bool UI_map::saveToSlot(int slot) {
    if (!saves::canSaveTo(slot)) return false;
    if (slot != saves::QUICK_SLOT && saveSlots[slot].exists && saveOverwriteArmed != slot) {
        saveOverwriteArmed = slot;
        setSaveStatus(saves::slotName(slot) + " е зает: натиснете ЗАПИШИ отново, за да го презапишете.", false);
        return false;
    }
    saveOverwriteArmed = -1;
    std::string err;
    bool ok = saveToFile(saves::pathFor(slot), err);
    refreshSaveSlots();
    if (ok) setSaveStatus("Записано в " + saves::slotName(slot) + ".", false);
    else setSaveStatus("Грешка при запис: " + err, true);
    return ok;
}

bool UI_map::loadFromSlot(int slot) {
    const SaveInfo& info = saveSlots[slot];
    if (!info.exists) {
        setSaveStatus(saves::slotName(slot) + " е празен.", true);
        return false;
    }
    std::string err;
    if (!info.valid || !loadFromFile(info.path, err)) {
        setSaveStatus("Грешка при зареждане: " + (info.valid ? err : std::string("повреден файл")), true);
        return false;
    }
    savePanelOpen = false;
    isPaused = true; // land on the pause menu: everybody gets ready before ПРОДЪЛЖИ
    pauseSelectedIdx = PAUSE_RESUME;
    setSaveStatus("Зареден " + saves::slotName(slot) + ": " + saves::describe(info, false), false);
    return true;
}

sf::FloatRect UI_map::savePanelRect() const {
    return sf::FloatRect({ 330.0f, 112.0f }, { 940.0f, 676.0f });
}

sf::FloatRect UI_map::saveButtonRect(int row, int col) const {
    sf::FloatRect p = savePanelRect();
    if (row >= saves::SLOT_COUNT) return sf::FloatRect({ p.position.x + (p.size.x - 260.0f) / 2.0f, p.position.y + p.size.y - 104.0f }, { 260.0f, 44.0f });
    float y = p.position.y + 78.0f + 64.0f * static_cast<float>(row) + 8.0f;
    return sf::FloatRect({ p.position.x + (col == 0 ? 610.0f : 770.0f), y }, { 140.0f, 40.0f });
}

void UI_map::handleSavePanelEvent(const sf::Event& event, const sf::RenderWindow& window) {
    sf::FloatRect panel = savePanelRect();
    sf::View view = ui::overlayView(window.getView(), grow(panel, 10.0f), ui::overlayScale());
    auto clampCol = [this]() {
        if (saveRow < saves::SLOT_COUNT && !saves::canSaveTo(saveRow)) saveCol = 1;
    };
    auto activate = [this]() {
        if (saveRow >= saves::SLOT_COUNT) { savePanelOpen = false; return; }
        if (saveCol == 0 && saves::canSaveTo(saveRow)) saveToSlot(saveRow);
        else loadFromSlot(saveRow);
    };
    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        using K = sf::Keyboard::Key;
        if (key->code == K::Escape) { savePanelOpen = false; primeInputEdges(0); return; }
        if (key->code == K::Up || key->code == K::W) { saveRow = (saveRow + saves::SLOT_COUNT) % (saves::SLOT_COUNT + 1); clampCol(); saveOverwriteArmed = -1; }
        else if (key->code == K::Down || key->code == K::S) { saveRow = (saveRow + 1) % (saves::SLOT_COUNT + 1); clampCol(); saveOverwriteArmed = -1; }
        else if (key->code == K::Left || key->code == K::A || key->code == K::Right || key->code == K::D) {
            saveCol = 1 - saveCol;
            clampCol();
            saveOverwriteArmed = -1;
        } else if (key->code == K::Enter || key->code == K::Space) {
            activate();
        }
        return;
    }
    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button != sf::Mouse::Button::Left) return;
        sf::Vector2f p = window.mapPixelToCoords(mb->position, view);
        for (int r = 0; r <= saves::SLOT_COUNT; ++r)
            for (int c = 0; c < 2; ++c) {
                if (r == saves::SLOT_COUNT && c == 1) continue;
                if (r < saves::SLOT_COUNT && c == 0 && !saves::canSaveTo(r)) continue;
                if (saveButtonRect(r, c).contains(p)) {
                    if (saveRow != r || saveCol != c) saveOverwriteArmed = -1;
                    saveRow = r;
                    saveCol = c;
                    activate();
                    return;
                }
            }
    }
}

void UI_map::drawSavePanel(sf::RenderWindow& window) {
    const ui::Palette& pal = ui::palette();
    sf::View base = window.getView();
    ui::drawBackdrop(window);
    sf::FloatRect panel = savePanelRect();
    window.setView(ui::overlayView(base, grow(panel, 10.0f), ui::overlayScale()));
    sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    bool moved = std::fabs(mouse.x - lastSaveMouse.x) > 2.0f || std::fabs(mouse.y - lastSaveMouse.y) > 2.0f;

    ui::drawPanel(window, font, panel, "ЗАПИС И ЗАРЕЖДАНЕ");
    for (int r = 0; r < saves::SLOT_COUNT; ++r) {
        float y = panel.position.y + 78.0f + 64.0f * static_cast<float>(r);
        bool rowFocused = (saveRow == r);
        if (rowFocused) {
            sf::RectangleShape bg({ panel.size.x - 40.0f, 56.0f });
            bg.setPosition({ panel.position.x + 20.0f, y });
            bg.setFillColor(pal.rowFocus);
            window.draw(bg);
        }
        if (r == saves::QUICK_SLOT || r == saves::FIRST_AUTO_SLOT) {
            sf::RectangleShape sep({ panel.size.x - 40.0f, 1.0f });
            sep.setPosition({ panel.position.x + 20.0f, y - 4.0f });
            sep.setFillColor(sf::Color(pal.border.r, pal.border.g, pal.border.b, 90));
            window.draw(sep);
        }
        const SaveInfo& info = saveSlots[r];
        std::string name = saves::slotName(r);
        if (r == saves::QUICK_SLOT) name += "  (F5 / F9)";
        ui::drawText(window, font, name, 17, { panel.position.x + 36.0f, y + 10.0f }, rowFocused ? pal.focus : pal.text, 0, true);
        std::string desc = saves::describe(info);
        unsigned int ds = ui::fitTextSize(font, desc, 13, 10, 540.0f);
        sf::Color dc = !info.exists ? pal.textDim : (info.valid ? sf::Color(pal.text.r, pal.text.g, pal.text.b, 220) : pal.danger);
        ui::drawText(window, font, desc, ds, { panel.position.x + 36.0f, y + 35.0f }, dc);

        for (int c = 0; c < 2; ++c) {
            if (c == 0 && !saves::canSaveTo(r)) continue;
            sf::FloatRect b = saveButtonRect(r, c);
            bool hover = b.contains(mouse);
            if (hover && moved) { saveRow = r; saveCol = c; }
            bool enabled = (c == 0) ? hasMatchInProgress() : info.valid;
            std::string label = (c == 0) ? ((saveOverwriteArmed == r) ? "ПРЕЗАПИШИ?" : "ЗАПИШИ") : "ЗАРЕДИ";
            ui::drawButton(window, font, b, label, rowFocused && saveCol == c, hover, enabled, 15);
        }
    }
    sf::FloatRect back = saveButtonRect(saves::SLOT_COUNT, 0);
    if (back.contains(mouse) && moved) saveRow = saves::SLOT_COUNT;
    if (moved) lastSaveMouse = mouse;

    // Status line
    float sy = panel.position.y + 78.0f + 64.0f * saves::SLOT_COUNT + 6.0f;
    unsigned int ss = ui::fitTextSize(font, saveStatus, 15, 10, panel.size.x - 60.0f);
    ui::drawText(window, font, saveStatus, ss, { panel.position.x + panel.size.x / 2.0f, sy }, saveStatusError ? pal.danger : pal.ok, 1);

    ui::drawButton(window, font, back, "НАЗАД", saveRow == saves::SLOT_COUNT, back.contains(mouse), true, 17);
    ui::drawText(window, font, "Стрелки: избор | Enter: ЗАПИШИ / ЗАРЕДИ | Esc: назад | Автозапис: края на всеки ден", 12,
                 { panel.position.x + panel.size.x / 2.0f, panel.position.y + panel.size.y - 40.0f }, pal.textDim, 1);
    window.setView(base);
}

// -----------------------------------------------------------------------------
// Overlays drawn after everything else (settings, save panel, status toast)
// -----------------------------------------------------------------------------
void UI_map::drawSessionOverlays(sf::RenderWindow& window) {
    if (!resourcesLoaded) return;
    if (settingsOverlay.isOpen()) {
        settingsOverlay.draw(window, font);
        return;
    }
    if (savePanelOpen) {
        drawSavePanel(window);
        return;
    }
    // Toast for quicksave / quickload / autosave / gamepad messages (also visible over the pause menu)
    if (saveStatus.empty() || saveStatusClock.getElapsedTime().asSeconds() > TOAST_SECONDS) return;
    const ui::Palette& pal = ui::palette();
    sf::View base = window.getView();
    float y = 812.0f;
    if (isPaused && engine.getCityState().winner == 0 && !showHelpOverlay) {
        window.setView(pauseView(window));
        y = ui::pause::box(PAUSE_COUNT).position.y - 46.0f;
    }
    unsigned int size = ui::fitTextSize(font, saveStatus, 16, 11, 860.0f);
    float w = ui::textWidth(font, saveStatus, size) + 44.0f;
    float alpha = std::min(1.0f, (TOAST_SECONDS - saveStatusClock.getElapsedTime().asSeconds()) / 0.4f);
    sf::RectangleShape bg({ w, 36.0f });
    bg.setPosition({ 800.0f - w / 2.0f, y });
    bg.setFillColor(sf::Color(pal.panel.r, pal.panel.g, pal.panel.b, static_cast<std::uint8_t>(240 * alpha)));
    sf::Color oc = saveStatusError ? TOAST_ERR : TOAST_OK;
    bg.setOutlineThickness(pal.outline + 0.5f);
    bg.setOutlineColor(sf::Color(oc.r, oc.g, oc.b, static_cast<std::uint8_t>(255 * alpha)));
    window.draw(bg);
    sf::Color tc = saveStatusError ? TOAST_ERR : pal.text;
    tc.a = static_cast<std::uint8_t>(255 * alpha);
    ui::drawText(window, font, saveStatus, size, { 800.0f, y + 18.0f - size * 0.36f }, tc, 1);
    window.setView(base);
}

// -----------------------------------------------------------------------------
// F-05: gamepads
// -----------------------------------------------------------------------------
void UI_map::onGamepadPress(unsigned int joystickId, unsigned int button) {
    int player = inputRouter().playerForPad(joystickId);
    if (player == 0) return;
    PlayerModalDialog& m = (player == 1) ? p1Modal : p2Modal;
    if (m.active && (button == GamepadButton::A || button == GamepadButton::B)) {
        closePlayerModal(player);
        return;
    }
    if (tutorial.isActive() && button == GamepadButton::A && tutorial.handleKey(sf::Keyboard::Key::Space)) {
        primeInputEdges(0);
    }
}

void UI_map::onGamepadConnection(unsigned int joystickId, bool connected) {
    inputRouter().setMatchContext(bot.isActive(), controlScheme);
    if (connected) {
        inputRouter().refreshAssignment();
        int p = inputRouter().playerForPad(joystickId);
        if (p == 0) {
            setSaveStatus("Геймпад свързан (не е нужен в този режим).", false);
            return;
        }
        setSaveStatus("Геймпад свързан: управлява Играч " + std::to_string(p) + ".", false);
        triggerPlayerPopup(p, "ГЕЙМПАД", InputRouter::padName(joystickId),
                           "Стик: движение | A: действие | B: отказ\nX: ъпгрейд | LB/RB: сграда | Y: соларен",
                           "Start: пауза | Back: помощ", PAD_COLOR);
        return;
    }
    int lost = 0;
    for (int p = 1; p <= 2; ++p)
        if (inputRouter().lastPadFor(p) == static_cast<int>(joystickId)) lost = p;
    inputRouter().refreshAssignment();
    if (lost == 0 || (lost == 2 && bot.isActive())) return;
    if (hasMatchInProgress() && !isPaused) {
        isPaused = true;
        pauseSelectedIdx = PAUSE_RESUME;
    }
    setSaveStatus("Геймпадът на Играч " + std::to_string(lost) + " е изключен - играта е на пауза.", true);
}
