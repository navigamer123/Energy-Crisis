// =============================================================================
// Team b-session (F-02, UX-12): main menu additions kept out of UI_mainMenu.cpp —
// the ПРОДЪЛЖИ button, the "start a new match?" confirmation and the content
// rect UI_main zooms by the UI scale.
// =============================================================================
#include "../includes/UI_mainMenu.h"
#include "../includes/UI_layout.h"
#include "../includes/UI_settings.h"
#include <cmath>

namespace {
const sf::Color CONTINUE_FILL(26, 64, 52);
const sf::Color CONTINUE_HOVER(34, 140, 96);
const sf::Color CONTINUE_OUTLINE(0, 255, 180);
const sf::FloatRect CONFIRM_BOX({ 500.0f, 300.0f }, { 600.0f, 270.0f });

sf::FloatRect confirmButton(int i) {
    const float w = 240.0f, h = 46.0f;
    float x = CONFIRM_BOX.position.x + (i == 0 ? 45.0f : CONFIRM_BOX.size.x - 45.0f - w);
    return sf::FloatRect({ x, CONFIRM_BOX.position.y + CONFIRM_BOX.size.y - 72.0f }, { w, h });
}
} // namespace

void UI_mainMenu::setContinueInfo(bool available, bool inMemory, const std::string& label) {
    bool appeared = available && !continueAvailable;
    bool vanished = !available && continueAvailable;
    continueAvailable = available;
    continueInMemory = available && inMemory;
    continueLabel = label;
    if (state == MenuState::MAIN && appeared) selectedMainIndex = 0; // ПРОДЪЛЖИ is preselected
    if (vanished && selectedMainIndex > 0) --selectedMainIndex;      // keep the same button selected
    if (!continueInMemory) confirmNewOpen = false;
}

UI_mainMenu::MainAction UI_mainMenu::mainButtonAction(int index) const {
    if (continueAvailable) {
        if (index == 0) return MainAction::CONTINUE;
        --index;
    }
    if (index == 0) return MainAction::PLAY;
    if (index == 1) return MainAction::SETTINGS;
    return MainAction::QUIT;
}

void UI_mainMenu::activateMainButton(int index) {
    switch (mainButtonAction(index)) {
        case MainAction::CONTINUE:
            requestContinue = true;
            enterHeld = false;
            spaceHeld = false;
            break;
        case MainAction::PLAY:
            if (continueInMemory) {
                // The running match would be thrown away: ask first (default answer: ОТКАЗ)
                confirmNewOpen = true;
                confirmIndex = 1;
            } else {
                state = MenuState::MODE_SELECT;
                selectedModeIndex = 0;
            }
            break;
        case MainAction::SETTINGS:
            onSettings();
            break;
        case MainAction::QUIT:
            onQuit();
            break;
    }
}

sf::FloatRect UI_mainMenu::contentBounds() const {
    float bottom = 560.0f;
    switch (state) {
        case MenuState::MAIN: {
            sf::FloatRect last = ui::menu::button(mainButtonCount() - 1);
            bottom = last.position.y + last.size.y + 64.0f;
            break;
        }
        case MenuState::MODE_SELECT: bottom = 520.0f; break;
        case MenuState::BOT_DIFFICULTY: bottom = 530.0f; break;
        case MenuState::PLAY_CONTROLS: bottom = 712.0f; break;
        case MenuState::SETTINGS: return sf::FloatRect({ 0.0f, 0.0f }, { VIRTUAL_WIDTH, VIRTUAL_HEIGHT });
    }
    return sf::FloatRect({ 250.0f, 46.0f }, { 1100.0f, bottom - 46.0f });
}

void UI_mainMenu::drawContinueButton(sf::RenderWindow& window, sf::FloatRect bounds, bool selected) {
    sf::RectangleShape shape(bounds.size);
    shape.setPosition(bounds.position);
    shape.setFillColor(selected ? CONTINUE_HOVER : CONTINUE_FILL);
    shape.setOutlineThickness(selected ? 3.0f : 1.5f);
    shape.setOutlineColor(selected ? sf::Color(255, 215, 0) : CONTINUE_OUTLINE);
    window.draw(shape);
    if (!fontLoaded) return;

    float cx = bounds.position.x + bounds.size.x / 2.0f;
    std::string title = selected ? "> ПРОДЪЛЖИ / CONTINUE <" : "ПРОДЪЛЖИ / CONTINUE";
    if (continueLabel.empty()) {
        ui::drawText(window, font, title, 18, { cx, bounds.position.y + bounds.size.y / 2.0f - 6.5f },
                     selected ? sf::Color(255, 240, 150) : sf::Color(240, 245, 255), 1);
        return;
    }
    ui::drawText(window, font, title, 17, { cx, bounds.position.y + 9.0f },
                 selected ? sf::Color(255, 240, 150) : sf::Color(240, 245, 255), 1);
    unsigned int s = ui::fitTextSize(font, continueLabel, 12, 9, bounds.size.x - 20.0f);
    ui::drawText(window, font, continueLabel, s, { cx, bounds.position.y + 33.0f }, sf::Color(190, 240, 215), 1);
}

void UI_mainMenu::drawConfirmNew(sf::RenderWindow& window) {
    const ui::Palette& pal = ui::palette();
    ui::drawBackdrop(window);
    ui::drawPanel(window, font, CONFIRM_BOX, "НОВА ИГРА?");
    if (!fontLoaded) return;

    float cx = CONFIRM_BOX.position.x + CONFIRM_BOX.size.x / 2.0f;
    std::string line1 = "Текущият мач ще бъде прекратен:";
    std::string line2 = gameSettings().autosave ? "Той е в автозаписите: ЗАПИС / ЗАРЕЖДАНЕ от паузата го връща."
                                                : "Автозаписът е изключен: мачът ще бъде загубен.";
    unsigned int s1 = ui::fitTextSize(font, line1, 16, 11, CONFIRM_BOX.size.x - 50.0f);
    unsigned int s2 = ui::fitTextSize(font, line2, 14, 10, CONFIRM_BOX.size.x - 50.0f);
    unsigned int sl = ui::fitTextSize(font, continueLabel, 15, 10, CONFIRM_BOX.size.x - 50.0f);
    ui::drawText(window, font, line1, s1, { cx, CONFIRM_BOX.position.y + 76.0f }, pal.text, 1);
    ui::drawText(window, font, continueLabel, sl, { cx, CONFIRM_BOX.position.y + 104.0f }, pal.accent, 1);
    ui::drawText(window, font, line2, s2, { cx, CONFIRM_BOX.position.y + 138.0f },
                 gameSettings().autosave ? pal.textDim : pal.danger, 1);

    sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    ui::drawButton(window, font, confirmButton(0), "ЗАПОЧНИ НОВА", confirmIndex == 0, confirmButton(0).contains(mouse), true, 16);
    ui::drawButton(window, font, confirmButton(1), "ОТКАЗ", confirmIndex == 1, confirmButton(1).contains(mouse), true, 16);
}

bool UI_mainMenu::handleConfirmNewEvent(const sf::Event& event, const sf::RenderWindow& window) {
    auto accept = [this]() {
        confirmNewOpen = false;
        state = MenuState::MODE_SELECT;
        selectedModeIndex = 0;
    };
    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        using K = sf::Keyboard::Key;
        if (key->code == K::Escape) {
            confirmNewOpen = false;
        } else if (key->code == K::Left || key->code == K::Right || key->code == K::Up || key->code == K::Down ||
                   key->code == K::A || key->code == K::D || key->code == K::Tab) {
            confirmIndex = 1 - confirmIndex;
        } else if (key->code == K::Enter || key->code == K::Space) {
            if (confirmIndex == 0) accept();
            else confirmNewOpen = false;
        }
        return true;
    }
    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button != sf::Mouse::Button::Left) return true;
        sf::Vector2f p = window.mapPixelToCoords(mb->position);
        if (confirmButton(0).contains(p)) accept();
        else if (confirmButton(1).contains(p)) confirmNewOpen = false;
        return true;
    }
    if (const auto* mm = event.getIf<sf::Event::MouseMoved>()) {
        sf::Vector2f p = window.mapPixelToCoords(mm->position);
        if (confirmButton(0).contains(p)) confirmIndex = 0;
        else if (confirmButton(1).contains(p)) confirmIndex = 1;
    }
    return false;
}
