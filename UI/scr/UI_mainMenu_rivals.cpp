// =============================================================================
// [AI team] Main menu additions: the red НЕВЪЗМОЖНО difficulty and the rival picker
// ("ИЗБЕРЕТЕ СЪПЕРНИК") that follows Easy / Medium / Hard. Members are declared in
// UI/includes/UI_mainMenu.h; the rival data comes from UI/scr/UI_botProfiles.cpp.
// =============================================================================
#include "../includes/UI_mainMenu.h"
#include "../includes/UI_botProfiles.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace {

// ---- Colours (the integrator maps these to UI_theme.h tokens) ---------------
const sf::Color kRowFill(24, 31, 45);
const sf::Color kRowFillSelected(38, 50, 72);
const sf::Color kRowOutline(60, 78, 105);
const sf::Color kSelectOutline(255, 215, 0);
const sf::Color kTagline(190, 205, 228);
const sf::Color kHint(120, 145, 175);
const sf::Color kDesc(170, 205, 240);
const sf::Color kRandomAccent(200, 205, 220);
const sf::Color kImpossibleFill(58, 12, 16);
const sf::Color kImpossibleFillSelected(175, 28, 28);
const sf::Color kImpossibleText(255, 80, 80);

constexpr int kRivalRows = BOT_PERSONALITY_COUNT + 2; // rivals, random, back
constexpr int kRandomRow = BOT_PERSONALITY_COUNT;
constexpr int kBackRow = BOT_PERSONALITY_COUNT + 1;

float textW(const sf::Text& t) { return t.getLocalBounds().size.x; }

void placeAt(sf::Text& t, float x, float y) {
    sf::FloatRect b = t.getLocalBounds();
    t.setPosition({ std::round(x - b.position.x), std::round(y - b.position.y) });
}

void centerIn(sf::Text& t, sf::FloatRect r) {
    sf::FloatRect b = t.getLocalBounds();
    t.setPosition({ std::round(r.position.x + (r.size.x - b.size.x) / 2.0f - b.position.x),
                    std::round(r.position.y + (r.size.y - b.size.y) / 2.0f - b.position.y) });
}

} // namespace

sf::FloatRect UI_mainMenu::difficultyButtonRect(int index) const {
    const float btnWidth = 460.0f;
    const float btnHeight = 54.0f;
    const float btnX = (VIRTUAL_WIDTH - btnWidth) / 2.0f;
    const float startY = 196.0f;
    const float spacing = 62.0f;
    if (index >= DIFFICULTY_ROWS - 1) {
        return sf::FloatRect({ btnX + (btnWidth - 240.0f) / 2.0f, startY + 4.0f * spacing + 8.0f }, { 240.0f, 46.0f });
    }
    return sf::FloatRect({ btnX, startY + index * spacing }, { btnWidth, btnHeight });
}

sf::FloatRect UI_mainMenu::rivalRowRect(int index) const {
    const float rowW = 720.0f;
    const float rowX = (VIRTUAL_WIDTH - rowW) / 2.0f;
    const float startY = 214.0f;
    const float spacing = 66.0f;
    if (index >= kBackRow) {
        return sf::FloatRect({ (VIRTUAL_WIDTH - 240.0f) / 2.0f, startY + kBackRow * spacing + 6.0f }, { 240.0f, 44.0f });
    }
    return sf::FloatRect({ rowX, startY + index * spacing }, { rowW, 58.0f });
}

void UI_mainMenu::drawImpossibleButton(sf::RenderWindow& window, sf::FloatRect bounds, bool isSelected) {
    sf::RectangleShape shape(bounds.size);
    shape.setPosition(bounds.position);
    shape.setFillColor(isSelected ? kImpossibleFillSelected : kImpossibleFill);
    shape.setOutlineThickness(isSelected ? 3.0f : 2.0f);
    shape.setOutlineColor(isSelected ? sf::Color(255, 200, 200) : kImpossibleText);
    window.draw(shape);
    if (!fontLoaded) return;
    std::string text = "НЕВЪЗМОЖНО / IMPOSSIBLE";
    sf::Text label(font, toUtf8(isSelected ? "> " + text + " <" : text), 18);
    label.setStyle(sf::Text::Bold);
    label.setFillColor(isSelected ? sf::Color::White : kImpossibleText);
    centerIn(label, bounds);
    window.draw(label);
}

void UI_mainMenu::drawDifficultyNote(sf::RenderWindow& window, float y) {
    if (!fontLoaded) return;
    sf::Text warn(font, toUtf8("ВНИМАНИЕ: ТОЗИ БОТ НЕ МОЖЕ ДА БЪДЕ ПОБЕДЕН!"), 16);
    warn.setStyle(sf::Text::Bold);
    warn.setFillColor(kImpossibleText);
    placeAt(warn, (VIRTUAL_WIDTH - textW(warn)) / 2.0f, y);
    window.draw(warn);
    sf::Text detail(font, toUtf8("Добив ×3, доход ×2, сгради на половин цена, без пауза при добив и +6% от града всеки ден."), 13);
    detail.setFillColor(sf::Color(255, 170, 170));
    placeAt(detail, (VIRTUAL_WIDTH - textW(detail)) / 2.0f, y + 26.0f);
    window.draw(detail);
}

void UI_mainMenu::chooseDifficulty(int index) {
    if (index >= 0 && index <= 2) {
        pendingDifficulty = (index == 0) ? BotDifficulty::EASY : (index == 1 ? BotDifficulty::MEDIUM : BotDifficulty::HARD);
        state = MenuState::BOT_RIVAL;
        if (selectedRivalIndex < 0 || selectedRivalIndex >= kBackRow) selectedRivalIndex = 0;
    } else if (index == 3) {
        selectedBotDifficulty = BotDifficulty::IMPOSSIBLE;
        selectedBotPersonality = BOT_PERSONALITY_OMEGA;
        onPlay();
    } else {
        state = MenuState::MODE_SELECT;
    }
}

void UI_mainMenu::chooseRival(int index) {
    if (index >= 0 && index < BOT_PERSONALITY_COUNT) {
        selectedBotPersonality = index;
    } else if (index == kRandomRow) {
        selectedBotPersonality = std::rand() % BOT_PERSONALITY_COUNT;
    } else {
        state = MenuState::BOT_DIFFICULTY;
        return;
    }
    selectedBotDifficulty = pendingDifficulty;
    onPlay();
}

bool UI_mainMenu::handleRivalEvent(const sf::Event& event, const sf::RenderWindow& window) {
    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        if (key->code == sf::Keyboard::Key::Up || key->code == sf::Keyboard::Key::W) {
            selectedRivalIndex = (selectedRivalIndex + kRivalRows - 1) % kRivalRows;
            return true;
        }
        if (key->code == sf::Keyboard::Key::Down || key->code == sf::Keyboard::Key::S) {
            selectedRivalIndex = (selectedRivalIndex + 1) % kRivalRows;
            return true;
        }
        if (key->code == sf::Keyboard::Key::Enter || key->code == sf::Keyboard::Key::Space) {
            chooseRival(selectedRivalIndex);
            return true;
        }
        if (key->code == sf::Keyboard::Key::Escape) {
            state = MenuState::BOT_DIFFICULTY;
            return true;
        }
    }
    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button != sf::Mouse::Button::Left) return false;
        sf::Vector2f clickPos = window.mapPixelToCoords(mb->position);
        for (int i = 0; i < kRivalRows; ++i) {
            if (rivalRowRect(i).contains(clickPos)) {
                selectedRivalIndex = i;
                chooseRival(i);
                return true;
            }
        }
    }
    return false;
}

void UI_mainMenu::drawRivalMenu(sf::RenderWindow& window) {
    drawHeader(window);

    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    bool mouseMoved = (std::abs(mousePos.x - lastMenuMousePos.x) > 2.0f || std::abs(mousePos.y - lastMenuMousePos.y) > 2.0f);
    if (mouseMoved) {
        lastMenuMousePos = mousePos;
        for (int i = 0; i < kRivalRows; ++i) {
            if (rivalRowRect(i).contains(mousePos)) selectedRivalIndex = i;
        }
    }

    if (fontLoaded) {
        // Title: "ИЗБЕРЕТЕ СЪПЕРНИК · ТРУДНО"
        sf::Text title(font, toUtf8("ИЗБЕРЕТЕ СЪПЕРНИК"), 22);
        title.setFillColor(sf::Color(0, 229, 255));
        title.setStyle(sf::Text::Bold);
        sf::Text level(font, toUtf8(std::string("·  ") + botDifficultyNameBg(pendingDifficulty)), 22);
        level.setFillColor(botDifficultyColor(pendingDifficulty));
        level.setStyle(sf::Text::Bold);
        float total = textW(title) + 12.0f + textW(level);
        float tx = (VIRTUAL_WIDTH - total) / 2.0f;
        placeAt(title, tx, 172.0f);
        placeAt(level, tx + textW(title) + 12.0f, 172.0f);
        window.draw(title);
        window.draw(level);
    }

    for (int i = 0; i < kBackRow; ++i) {
        sf::FloatRect r = rivalRowRect(i);
        bool sel = (selectedRivalIndex == i);
        bool isRandom = (i == kRandomRow);
        const BotProfile& rival = botPersonality(isRandom ? 0 : i);
        sf::Color accent = isRandom ? kRandomAccent : rival.accent;

        sf::RectangleShape row(r.size);
        row.setPosition(r.position);
        row.setFillColor(sel ? kRowFillSelected : kRowFill);
        row.setOutlineThickness(sel ? 2.5f : 1.5f);
        row.setOutlineColor(sel ? kSelectOutline : kRowOutline);
        window.draw(row);

        sf::RectangleShape stripe({ 6.0f, r.size.y });
        stripe.setPosition(r.position);
        stripe.setFillColor(accent);
        window.draw(stripe);

        if (!fontLoaded) continue;
        sf::Text name(font, toUtf8(isRandom ? "Случаен съперник" : rival.nameBg), 20);
        name.setFillColor(accent);
        name.setStyle(sf::Text::Bold);
        placeAt(name, r.position.x + 22.0f, r.position.y + 9.0f);
        window.draw(name);

        sf::Text tagline(font, toUtf8(isRandom ? "Изненадай ме: един от петимата, избран на случаен принцип." : rival.taglineBg), 14);
        tagline.setFillColor(kTagline);
        tagline.setStyle(sf::Text::Italic);
        placeAt(tagline, r.position.x + 22.0f, r.position.y + 37.0f);
        window.draw(tagline);

        // Play-style chip, right-aligned and vertically centred
        sf::Text chip(font, toUtf8(isRandom ? "?" : rival.signatureBg), 13);
        chip.setFillColor(accent);
        float chipW = textW(chip) + 20.0f;
        sf::FloatRect chipRect({ r.position.x + r.size.x - 16.0f - chipW, r.position.y + (r.size.y - 24.0f) / 2.0f },
                               { chipW, 24.0f });
        sf::RectangleShape chipBox(chipRect.size);
        chipBox.setPosition(chipRect.position);
        chipBox.setFillColor(sf::Color(accent.r / 6, accent.g / 6, accent.b / 6, 220));
        chipBox.setOutlineThickness(1.0f);
        chipBox.setOutlineColor(accent);
        window.draw(chipBox);
        centerIn(chip, chipRect);
        window.draw(chip);
    }

    sf::Color defaultBtn(30, 40, 56);
    drawButton(window, rivalRowRect(kBackRow), toUtf8("НАЗАД / BACK"), defaultBtn, sf::Color(70, 45, 60),
               sf::Color(240, 245, 255), selectedRivalIndex == kBackRow);

    if (fontLoaded) {
        sf::FloatRect back = rivalRowRect(kBackRow);
        float y = back.position.y + back.size.y + 20.0f;
        std::string desc;
        if (selectedRivalIndex < BOT_PERSONALITY_COUNT) desc = "Стил: " + botPersonality(selectedRivalIndex).styleBg;
        else if (selectedRivalIndex == kRandomRow) desc = "Съперникът се разкрива в началото на мача.";
        else desc = "Връщане към избор на трудност";
        sf::Text tDesc(font, toUtf8(desc), 14);
        tDesc.setFillColor(kDesc);
        placeAt(tDesc, (VIRTUAL_WIDTH - textW(tDesc)) / 2.0f, y);
        window.draw(tDesc);

        sf::Text hint(font, toUtf8("Навигация: [Стрелки / W,S] | Старт: [Enter / Space] | Назад: [ESC]"), 13);
        hint.setFillColor(kHint);
        placeAt(hint, (VIRTUAL_WIDTH - textW(hint)) / 2.0f, y + 30.0f);
        window.draw(hint);
    }
}
