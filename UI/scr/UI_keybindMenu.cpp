// =============================================================================
// Team b-session (F-10): key rebinding screen (see UI_keybindMenu.h)
// =============================================================================
#include "../includes/UI_keybindMenu.h"
#include "../includes/UI_input.h"
#include "../includes/UI_layout.h"
#include "../includes/UI_settings.h"
#include <cmath>

namespace {
constexpr float PANEL_X = 250.0f;
constexpr float PANEL_Y = 88.0f;
constexpr float PANEL_W = 1100.0f;
constexpr float PANEL_H = 724.0f;
constexpr float ROWS_Y = PANEL_Y + 124.0f;
constexpr float ROW_H = 31.0f;
constexpr float CELL_W = 140.0f;
constexpr float CELL_H = 26.0f;
const float CELL_X[4] = { PANEL_X + 460.0f, PANEL_X + 610.0f, PANEL_X + 780.0f, PANEL_X + 930.0f };

int cellPlayer(int c) { return (c < 2) ? 1 : 2; }
int cellSlot(int c) { return c % 2; }
} // namespace

void UI_keybindMenu::open() {
    active = true;
    capturing = false;
    draft = gameSettings().keys;
    row = 0;
    col = 0;
    lastMouse = { -999.0f, -999.0f };
    setStatus("Enter / клик: смяна на клавиш  |  Backspace / десен клик: изчистване  |  Esc: отказ", false);
}

sf::FloatRect UI_keybindMenu::panelRect() const {
    return sf::FloatRect({ PANEL_X, PANEL_Y }, { PANEL_W, PANEL_H });
}

sf::View UI_keybindMenu::viewFor(const sf::RenderWindow& window) const {
    sf::FloatRect r = panelRect();
    r.position -= sf::Vector2f(8.0f, 8.0f);
    r.size += sf::Vector2f(16.0f, 16.0f);
    return ui::overlayView(window.getView(), r, ui::overlayScale());
}

sf::FloatRect UI_keybindMenu::cellRect(int r, int c) const {
    return sf::FloatRect({ CELL_X[c], ROWS_Y + ROW_H * static_cast<float>(r) + (ROW_H - CELL_H) / 2.0f }, { CELL_W, CELL_H });
}

sf::FloatRect UI_keybindMenu::buttonRect(int b) const {
    const float w = 250.0f, h = 44.0f, gap = 30.0f;
    float total = w * BTN_COUNT + gap * (BTN_COUNT - 1);
    float x0 = PANEL_X + (PANEL_W - total) / 2.0f;
    return sf::FloatRect({ x0 + (w + gap) * static_cast<float>(b), PANEL_Y + PANEL_H - 100.0f }, { w, h });
}

void UI_keybindMenu::setStatus(const std::string& text, bool error) {
    status = text;
    statusIsError = error;
}

void UI_keybindMenu::finish(bool save) {
    if (save) {
        if (draft.hasConflicts()) {
            setStatus("Има повтарящи се или системни клавиши (в червено). Поправете ги, за да запазите.", true);
            return;
        }
        gameSettings().keys = draft;
        gameSettings().touch();
        saveGameSettings();
    }
    active = false;
    capturing = false;
}

void UI_keybindMenu::activateButton(int b) {
    if (b == BTN_DEFAULTS) {
        draft = InputMap::defaults();
        setStatus("Възстановени са клавишите по подразбиране (натиснете ЗАПАЗИ).", false);
    } else if (b == BTN_SAVE) {
        finish(true);
    } else {
        finish(false);
    }
}

void UI_keybindMenu::assignCapturedKey(int code) {
    capturing = false;
    InputAction act = static_cast<InputAction>(row);
    if (code == KeyCode::Unknown || keyCodeId(code)[0] == '\0') {
        setStatus("Този клавиш не може да се използва.", true);
        return;
    }
    if (isReservedKey(code)) {
        setStatus("[" + keyLabelLocalized(code) + "] е системен клавиш (пауза, помощ, запис, цял екран).", true);
        return;
    }
    draft.set(cellPlayer(col), act, cellSlot(col), code);
    if (draft.isConflicted(cellPlayer(col), act, cellSlot(col))) {
        setStatus("[" + keyLabelLocalized(code) + "] вече се използва - сменете другото място (в червено).", true);
    } else {
        setStatus(std::string(InputMap::actionLabelBg(act)) + ": [" + keyLabelLocalized(code) + "] за Играч " +
                      std::to_string(cellPlayer(col)) + ".", false);
    }
}

void UI_keybindMenu::handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    if (!active) return;
    sf::View view = viewFor(window);

    if (capturing) {
        if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
            if (key->code == sf::Keyboard::Key::Escape) {
                capturing = false;
                setStatus("Смяната е отказана.", false);
                return;
            }
            int code = static_cast<int>(key->scancode);
            if (code == KeyCode::Unknown) code = static_cast<int>(sf::Keyboard::delocalize(key->code));
            assignCapturedKey(code);
        } else if (event.is<sf::Event::MouseButtonPressed>() || event.is<sf::Event::FocusLost>()) {
            capturing = false;
            setStatus("Смяната е отказана.", false);
        }
        return;
    }

    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        using K = sf::Keyboard::Key;
        const int rows = INPUT_ACTION_COUNT + 1;
        if (key->code == K::Escape) { finish(false); return; }
        if (key->code == K::Up || key->code == K::W) {
            row = (row + rows - 1) % rows;
            if (row == INPUT_ACTION_COUNT) col = std::min(col, BTN_COUNT - 1);
        } else if (key->code == K::Down || key->code == K::S) {
            row = (row + 1) % rows;
            if (row == INPUT_ACTION_COUNT) col = std::min(col, BTN_COUNT - 1);
        } else if (key->code == K::Left || key->code == K::A) {
            int n = (row == INPUT_ACTION_COUNT) ? BTN_COUNT : 4;
            col = (col + n - 1) % n;
        } else if (key->code == K::Right || key->code == K::D) {
            int n = (row == INPUT_ACTION_COUNT) ? BTN_COUNT : 4;
            col = (col + 1) % n;
        } else if (key->code == K::Enter || key->code == K::Space) {
            if (row == INPUT_ACTION_COUNT) {
                activateButton(col);
            } else {
                capturing = true;
                setStatus("Натиснете клавиш за \"" + std::string(InputMap::actionLabelBg(static_cast<InputAction>(row))) +
                              "\" (Играч " + std::to_string(cellPlayer(col)) + ")  |  Esc: отказ", false);
            }
        } else if (key->code == K::Backspace || key->code == K::Delete) {
            if (row < INPUT_ACTION_COUNT) {
                draft.clear(cellPlayer(col), static_cast<InputAction>(row), cellSlot(col));
                setStatus("Изчистено.", false);
            }
        }
        return;
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        sf::Vector2f p = window.mapPixelToCoords(mb->position, view);
        for (int r = 0; r < INPUT_ACTION_COUNT; ++r)
            for (int c = 0; c < 4; ++c)
                if (cellRect(r, c).contains(p)) {
                    row = r;
                    col = c;
                    if (mb->button == sf::Mouse::Button::Right) {
                        draft.clear(cellPlayer(c), static_cast<InputAction>(r), cellSlot(c));
                        setStatus("Изчистено.", false);
                    } else if (mb->button == sf::Mouse::Button::Left) {
                        capturing = true;
                        setStatus("Натиснете клавиш за \"" + std::string(InputMap::actionLabelBg(static_cast<InputAction>(r))) +
                                      "\" (Играч " + std::to_string(cellPlayer(c)) + ")  |  Esc: отказ", false);
                    }
                    return;
                }
        if (mb->button == sf::Mouse::Button::Left)
            for (int b = 0; b < BTN_COUNT; ++b)
                if (buttonRect(b).contains(p)) {
                    row = INPUT_ACTION_COUNT;
                    col = b;
                    activateButton(b);
                    return;
                }
    }
}

void UI_keybindMenu::draw(sf::RenderWindow& window, const sf::Font& font) {
    if (!active) return;
    const ui::Palette& pal = ui::palette();
    sf::View base = window.getView();
    ui::drawBackdrop(window);
    window.setView(viewFor(window));

    sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    bool mouseMoved = std::fabs(mouse.x - lastMouse.x) > 2.0f || std::fabs(mouse.y - lastMouse.y) > 2.0f;
    if (mouseMoved && !capturing) {
        lastMouse = mouse;
        for (int r = 0; r < INPUT_ACTION_COUNT; ++r)
            for (int c = 0; c < 4; ++c)
                if (cellRect(r, c).contains(mouse)) { row = r; col = c; }
        for (int b = 0; b < BTN_COUNT; ++b)
            if (buttonRect(b).contains(mouse)) { row = INPUT_ACTION_COUNT; col = b; }
    }

    sf::FloatRect panel = panelRect();
    ui::drawPanel(window, font, panel, "КЛАВИШИ / ПРЕНАСТРОЙВАНЕ");

    // Column headers
    float hy = PANEL_Y + 70.0f;
    ui::drawText(window, font, "ДЕЙСТВИЕ", 14, { PANEL_X + 34.0f, hy + 12.0f }, pal.textDim, 0, true);
    ui::drawText(window, font, "ИГРАЧ 1 (ЗАПАД)", 15, { (CELL_X[0] + CELL_X[1] + CELL_W) / 2.0f, hy }, sf::Color(0, 229, 255), 1, true);
    ui::drawText(window, font, "ИГРАЧ 2 (ИЗТОК)", 15, { (CELL_X[2] + CELL_X[3] + CELL_W) / 2.0f, hy }, sf::Color(255, 140, 215), 1, true);
    const char* sub[4] = { "основен", "резервен", "основен", "резервен" };
    for (int c = 0; c < 4; ++c)
        ui::drawText(window, font, sub[c], 12, { CELL_X[c] + CELL_W / 2.0f, hy + 26.0f }, pal.textDim, 1);

    std::vector<InputMap::Cell> bad = draft.conflicts();
    auto isBad = [&bad](int r, int c) {
        for (const auto& cell : bad)
            if (cell.action == r && cell.player == cellPlayer(c) && cell.slot == cellSlot(c)) return true;
        return false;
    };

    for (int r = 0; r < INPUT_ACTION_COUNT; ++r) {
        float y = ROWS_Y + ROW_H * static_cast<float>(r);
        if (r % 2 == 0) {
            sf::RectangleShape stripe({ PANEL_W - 40.0f, ROW_H });
            stripe.setPosition({ PANEL_X + 20.0f, y });
            stripe.setFillColor(sf::Color(pal.panelAlt.r, pal.panelAlt.g, pal.panelAlt.b, 120));
            window.draw(stripe);
        }
        bool rowFocused = (row == r);
        ui::drawText(window, font, InputMap::actionLabelBg(static_cast<InputAction>(r)), 15,
                     { PANEL_X + 34.0f, y + ROW_H / 2.0f - 5.5f }, rowFocused ? pal.focus : pal.text);
        for (int c = 0; c < 4; ++c) {
            sf::FloatRect cr = cellRect(r, c);
            bool focused = rowFocused && col == c;
            bool conflict = isBad(r, c);
            sf::RectangleShape cell(cr.size);
            cell.setPosition(cr.position);
            cell.setFillColor(conflict ? sf::Color(110, 25, 30, 235) : (focused ? pal.buttonHover : pal.button));
            cell.setOutlineThickness(focused ? pal.focusOutline : 1.0f);
            cell.setOutlineColor(focused ? pal.focus : (conflict ? pal.danger : sf::Color(pal.border.r, pal.border.g, pal.border.b, 110)));
            window.draw(cell);

            std::string label;
            sf::Color tc = conflict ? sf::Color(255, 200, 200) : pal.text;
            if (focused && capturing) {
                label = "...";
                tc = pal.focus;
            } else {
                int code = draft.key(cellPlayer(c), static_cast<InputAction>(r), cellSlot(c));
                label = (code == KeyCode::Unknown) ? std::string("-") : keyLabelLocalized(code);
                if (code == KeyCode::Unknown) tc = pal.textDim;
            }
            unsigned int s = ui::fitTextSize(font, label, 15, 10, CELL_W - 10.0f);
            ui::drawText(window, font, label, s, { cr.position.x + CELL_W / 2.0f, cr.position.y + CELL_H / 2.0f - s * 0.36f }, tc, 1);
        }
    }

    // Status line (error = red)
    float sy = ROWS_Y + ROW_H * INPUT_ACTION_COUNT + 12.0f;
    std::string st = status;
    if (!statusIsError && !bad.empty() && !capturing)
        st = "Червените клавиши се повтарят или са системни - ЗАПАЗИ е недостъпно.";
    bool err = statusIsError || (!bad.empty() && !capturing);
    unsigned int ss = ui::fitTextSize(font, st, 14, 10, PANEL_W - 60.0f);
    ui::drawText(window, font, st, ss, { PANEL_X + PANEL_W / 2.0f, sy }, err ? pal.danger : pal.textDim, 1);

    const char* labels[BTN_COUNT] = { "ПО ПОДРАЗБИРАНЕ", "ЗАПАЗИ", "ОТКАЗ" };
    for (int b = 0; b < BTN_COUNT; ++b) {
        sf::FloatRect br = buttonRect(b);
        bool enabled = !(b == BTN_SAVE && !bad.empty());
        ui::drawButton(window, font, br, labels[b], row == INPUT_ACTION_COUNT && col == b, br.contains(mouse), enabled, 16);
    }

    ui::drawText(window, font, "Навигация: стрелки | Enter: избор | Esc: отказ без запис", 12,
                 { PANEL_X + PANEL_W / 2.0f, PANEL_Y + PANEL_H - 36.0f }, pal.textDim, 1);
    window.setView(base);
}
