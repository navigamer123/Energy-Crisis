// =============================================================================
// Team b-session (F-06, F-10, UX-12, HX-15): НАСТРОЙКИ screen (see UI_settingsMenu.h)
// =============================================================================
#include "../includes/UI_settingsMenu.h"
#include "../includes/UI_input.h"
#include "../includes/UI_layout.h"
#include "../includes/UI_settings.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr float PANEL_X = 310.0f;
constexpr float PANEL_Y = 110.0f;
constexpr float PANEL_W = 980.0f;
constexpr float PANEL_H = 640.0f;
constexpr int TAB_COUNT = 4;
const char* TAB_NAMES[TAB_COUNT] = { "ЕКРАН", "ЗВУК", "ИГРА", "УПРАВЛЕНИЕ" };

std::string onOff(bool v) { return v ? "ВКЛЮЧЕНО" : "ИЗКЛЮЧЕНО"; }

template <size_t N>
int stepIndex(int value, const int (&steps)[N]) {
    for (size_t i = 0; i < N; ++i)
        if (steps[i] == value) return static_cast<int>(i);
    return -1;
}

std::string percent(int v) { return std::to_string(v) + " %"; }

void changeVolume(int& v, int dir) {
    v = std::max(0, std::min(100, v + dir * 10));
    gameSettings().touch();
}

bool resolutionFits(int w, int h) {
    sf::VideoMode desk = sf::VideoMode::getDesktopMode();
    return (static_cast<unsigned>(w) <= desk.size.x && static_cast<unsigned>(h) <= desk.size.y) || (w == 1280 && h == 720);
}
} // namespace

void UI_settingsMenu::open(bool match) {
    active = true;
    inMatch = match;
    tab = 0;
    focus = 0;
    flash.clear();
    lastMouse = { -999.0f, -999.0f };
}

void UI_settingsMenu::close() {
    if (!active) return;
    active = false;
    saveGameSettings();
}

void UI_settingsMenu::setFlash(const std::string& text) {
    flash = text;
    flashClock.restart();
}

sf::FloatRect UI_settingsMenu::panelRect() const {
    return sf::FloatRect({ PANEL_X, PANEL_Y }, { PANEL_W, PANEL_H });
}

sf::View UI_settingsMenu::viewFor(const sf::RenderWindow& window) const {
    sf::FloatRect r = panelRect();
    r.position -= sf::Vector2f(8.0f, 8.0f);
    r.size += sf::Vector2f(16.0f, 16.0f);
    return ui::overlayView(window.getView(), r, ui::overlayScale());
}

sf::FloatRect UI_settingsMenu::tabRect(int i) const {
    const float w = 222.0f, gap = 10.0f;
    float x0 = PANEL_X + (PANEL_W - (w * TAB_COUNT + gap * (TAB_COUNT - 1))) / 2.0f;
    return sf::FloatRect({ x0 + (w + gap) * static_cast<float>(i), PANEL_Y + 70.0f }, { w, 42.0f });
}

sf::FloatRect UI_settingsMenu::rowRect(int i) const {
    return sf::FloatRect({ PANEL_X + 30.0f, PANEL_Y + 132.0f + 54.0f * static_cast<float>(i) }, { PANEL_W - 60.0f, 48.0f });
}

sf::FloatRect UI_settingsMenu::valueRect(int i) const {
    sf::FloatRect r = rowRect(i);
    return sf::FloatRect({ PANEL_X + 560.0f, r.position.y + 6.0f }, { 370.0f, 36.0f });
}

sf::FloatRect UI_settingsMenu::backRect() const {
    return sf::FloatRect({ PANEL_X + (PANEL_W - 260.0f) / 2.0f, PANEL_Y + PANEL_H - 96.0f }, { 260.0f, 46.0f });
}

std::vector<UI_settingsMenu::Row> UI_settingsMenu::buildRows() {
    GameSettings& s = gameSettings();
    std::vector<Row> rows;
    if (tab == 0) {
        rows.push_back({ "Размер на прозореца",
                         [&s]() { return std::to_string(s.windowWidth) + " × " + std::to_string(s.windowHeight); },
                         [&s](int dir) {
                             int n = SettingsSteps::RESOLUTION_COUNT, cur = -1;
                             for (int i = 0; i < n; ++i)
                                 if (SettingsSteps::RESOLUTIONS[i][0] == s.windowWidth && SettingsSteps::RESOLUTIONS[i][1] == s.windowHeight) cur = i;
                             if (cur < 0) cur = 2;
                             for (int k = 1; k <= n; ++k) {
                                 int i = ((cur + dir * k) % n + n) % n;
                                 if (resolutionFits(SettingsSteps::RESOLUTIONS[i][0], SettingsSteps::RESOLUTIONS[i][1])) {
                                     s.windowWidth = SettingsSteps::RESOLUTIONS[i][0];
                                     s.windowHeight = SettingsSteps::RESOLUTIONS[i][1];
                                     break;
                                 }
                             }
                             s.touch();
                         },
                         nullptr,
                         "Размер на прозореца извън режим на цял екран. Играта винаги пази пропорции 16:9." });
        rows.push_back({ "Цял екран", [&s]() { return onOff(s.fullscreen); },
                         [&s](int) { s.fullscreen = !s.fullscreen; s.touch(); }, nullptr,
                         "Цял екран с резолюцията на монитора. Бърз клавиш: F11 или Alt+Enter." });
        rows.push_back({ "Лимит на кадрите",
                         [&s]() { return s.fpsLimit == 0 ? std::string("БЕЗ ЛИМИТ") : std::to_string(s.fpsLimit) + " FPS"; },
                         [&s](int dir) {
                             int i = stepIndex(s.fpsLimit, SettingsSteps::FPS_VALUES);
                             if (i < 0) i = 1;
                             i = ((i + dir) % SettingsSteps::FPS_COUNT + SettingsSteps::FPS_COUNT) % SettingsSteps::FPS_COUNT;
                             s.fpsLimit = SettingsSteps::FPS_VALUES[i];
                             s.touch();
                         },
                         nullptr, "Най-много кадри в секунда. По-нисък лимит пести батерия и шум от вентилатора." });
        rows.push_back({ "Вертикална синхронизация", [&s]() { return onOff(s.vsync); },
                         [&s](int) { s.vsync = !s.vsync; s.touch(); }, nullptr,
                         "Синхронизира кадрите с монитора (без разкъсване на картината). Заменя лимита на кадрите." });
        rows.push_back({ "Мащаб на интерфейса", [&s]() { return percent(s.uiScalePercent); },
                         [&s](int dir) {
                             int i = stepIndex(s.uiScalePercent, SettingsSteps::UI_SCALES);
                             if (i < 0) i = 1;
                             i = std::max(0, std::min(SettingsSteps::UI_SCALE_COUNT - 1, i + dir));
                             s.uiScalePercent = SettingsSteps::UI_SCALES[i];
                             s.touch();
                         },
                         nullptr, "Размер на менютата, паузата, помощта и диалозите. 125-150 % за малки екрани и телевизори." });
        rows.push_back({ "Режим „Проектор“", [&s]() { return onOff(s.projectorMode); },
                         [&s](int) { s.projectorMode = !s.projectorMode; s.touch(); }, nullptr,
                         "За презентация: по-едър интерфейс, по-дебели курсори и контрастна, по-ярка картина." });
    } else if (tab == 1) {
        rows.push_back({ "Обща сила на звука", [&s]() { return percent(s.masterVolume); },
                         [&s](int dir) { changeVolume(s.masterVolume, dir); }, nullptr,
                         "Общо ниво за музика и ефекти." });
        rows.push_back({ "Музика", [&s]() { return percent(s.musicVolume); },
                         [&s](int dir) { changeVolume(s.musicVolume, dir); }, nullptr,
                         "Фонова музика (част от общото ниво)." });
        rows.push_back({ "Звукови ефекти", [&s]() { return percent(s.sfxVolume); },
                         [&s](int dir) { changeVolume(s.sfxVolume, dir); }, nullptr,
                         "Звуци от строеж, добив, мълнии и интерфейс (част от общото ниво)." });
    } else if (tab == 2) {
        rows.push_back({ "Обучение",
                         [&s]() {
                             return s.tutorialPolicy == TutorialPolicy::Always ? std::string("ВИНАГИ")
                                  : s.tutorialPolicy == TutorialPolicy::Never  ? std::string("НИКОГА")
                                                                               : std::string("АВТОМАТИЧНО");
                         },
                         [&s](int dir) {
                             int v = (static_cast<int>(s.tutorialPolicy) + dir + 3) % 3;
                             s.tutorialPolicy = static_cast<TutorialPolicy>(v);
                             s.touch();
                         },
                         nullptr, "АВТОМАТИЧНО: само докато го завършите или пропуснете веднъж (без ТРУДЕН бот)." });
        rows.push_back({ "Бот по подразбиране",
                         [&s]() {
                             return s.defaultBotDifficulty == 1 ? std::string("ЛЕСЕН")
                                  : s.defaultBotDifficulty == 3 ? std::string("ТРУДЕН") : std::string("СРЕДЕН");
                         },
                         [&s](int dir) {
                             s.defaultBotDifficulty = (s.defaultBotDifficulty - 1 + dir + 3) % 3 + 1;
                             s.touch();
                         },
                         nullptr, "Предварително избраната трудност в САМОСТОЯТЕЛНА ИГРА." });
        rows.push_back({ "Време на съобщенията",
                         [&s]() {
                             char b[16];
                             std::snprintf(b, sizeof(b), "%.0f сек", s.popupSeconds);
                             return std::string(b);
                         },
                         [&s](int dir) {
                             s.popupSeconds = std::max(2.0f, std::min(8.0f, std::round(s.popupSeconds) + static_cast<float>(dir)));
                             s.touch();
                         },
                         nullptr, "Колко дълго стоят съобщенията отстрани на всеки играч." });
        rows.push_back({ "Автоматичен запис", [&s]() { return onOff(s.autosave); },
                         [&s](int) { s.autosave = !s.autosave; s.touch(); }, nullptr,
                         "Запис в края на всеки ден и при изход от мача (последните 3 се пазят)." });
        rows.push_back({ "Покажи обучението отново", []() { return std::string("НУЛИРАЙ"); }, nullptr,
                         [this, &s]() {
                             s.tutorialDoneSolo = false;
                             s.tutorialDoneCoop = false;
                             s.touch();
                             setFlash("Обучението ще се покаже в следващата игра.");
                         },
                         "Нулира запомненото завършване на обучението (за режим АВТОМАТИЧНО)." });
    } else {
        rows.push_back({ "Геймпади", [&s]() { return onOff(s.gamepadEnabled); },
                         [&s](int) { s.gamepadEnabled = !s.gamepadEnabled; s.touch(); }, nullptr,
                         "Стик/кръст: движение | A: действие | B: отказ | X: ъпгрейд | LB/RB: сграда | Y: соларен | Start: пауза" });
        rows.push_back({ "Един геймпад при двама",
                         [&s]() { return std::string(s.singlePadOwner == 1 ? "ИГРАЧ 1" : "ИГРАЧ 2"); },
                         [&s](int) { s.singlePadOwner = (s.singlePadOwner == 1) ? 2 : 1; s.touch(); }, nullptr,
                         "При двама играчи и един геймпад: кой играч го управлява (другият играе с клавиатурата)." });
        rows.push_back({ "Свързани геймпади",
                         []() {
                             std::vector<unsigned int> pads = InputRouter::connectedPads();
                             if (pads.empty()) return std::string("НЯМА");
                             std::string n = InputRouter::padName(pads[0]);
                             if (pads.size() > 1) n += " +" + std::to_string(pads.size() - 1);
                             return n;
                         },
                         nullptr, nullptr, "Свържете геймпад по всяко време; той се разпределя автоматично." });
        rows.push_back({ "Клавиши на играчите", []() { return std::string("ПРЕНАСТРОЙ..."); }, nullptr,
                         [this]() { keybinds.open(); },
                         "Отваря таблицата с клавиши: смяна, изчистване и проверка за повторения." });
        rows.push_back({ "Клавиши по подразбиране", []() { return std::string("ВЪЗСТАНОВИ"); }, nullptr,
                         [this, &s]() {
                             s.keys = InputMap::defaults();
                             s.touch();
                             saveGameSettings();
                             setFlash("Клавишите са върнати по подразбиране.");
                         },
                         "Връща WASD/SPACE за Играч 1 и стрелки/ENTER за Играч 2." });
    }
    return rows;
}

void UI_settingsMenu::handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    if (!active) return;
    if (keybinds.isOpen()) {
        keybinds.handleEvent(event, window);
        return;
    }
    std::vector<Row> rows = buildRows();
    const int n = static_cast<int>(rows.size());
    sf::View view = viewFor(window);

    auto switchTab = [this](int dir) {
        tab = (tab + dir + TAB_COUNT) % TAB_COUNT;
        int count = static_cast<int>(buildRows().size());
        if (focus >= count) focus = count; // keep it on НАЗАД if it was there
    };

    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        using K = sf::Keyboard::Key;
        if (key->code == K::Escape) { close(); return; }
        if (key->code == K::PageUp || key->code == K::Tab) { switchTab(key->shift || key->code == K::PageUp ? -1 : 1); return; }
        if (key->code == K::PageDown) { switchTab(1); return; }
        if (key->code == K::Up || key->code == K::W) {
            focus = (focus <= -1) ? n : focus - 1;
        } else if (key->code == K::Down || key->code == K::S) {
            focus = (focus >= n) ? -1 : focus + 1;
        } else if (key->code == K::Left || key->code == K::A || key->code == K::Right || key->code == K::D) {
            int dir = (key->code == K::Left || key->code == K::A) ? -1 : 1;
            if (focus == -1) switchTab(dir);
            else if (focus >= 0 && focus < n && rows[focus].change) rows[focus].change(dir);
        } else if (key->code == K::Enter || key->code == K::Space) {
            if (focus == -1) switchTab(1);
            else if (focus == n) close();
            else if (focus >= 0 && focus < n) {
                if (rows[focus].activate) rows[focus].activate();
                else if (rows[focus].change) rows[focus].change(1);
            }
        }
        return;
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button != sf::Mouse::Button::Left) return;
        sf::Vector2f p = window.mapPixelToCoords(mb->position, view);
        for (int i = 0; i < TAB_COUNT; ++i)
            if (tabRect(i).contains(p)) { tab = i; focus = -1; return; }
        for (int i = 0; i < n; ++i) {
            if (!rowRect(i).contains(p)) continue;
            focus = i;
            sf::FloatRect v = valueRect(i);
            if (rows[i].activate) {
                if (v.contains(p)) rows[i].activate();
            } else if (rows[i].change && v.contains(p)) {
                // Left third = previous value, everything else = next value
                rows[i].change(p.x < v.position.x + v.size.x / 3.0f ? -1 : 1);
            }
            return;
        }
        if (backRect().contains(p)) close();
    }
}

void UI_settingsMenu::draw(sf::RenderWindow& window, const sf::Font& font) {
    if (!active) return;
    if (keybinds.isOpen()) {
        keybinds.draw(window, font);
        return;
    }
    const ui::Palette& pal = ui::palette();
    std::vector<Row> rows = buildRows();
    const int n = static_cast<int>(rows.size());
    if (focus > n) focus = n;

    sf::View base = window.getView();
    ui::drawBackdrop(window);
    window.setView(viewFor(window));
    sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    bool mouseMoved = std::fabs(mouse.x - lastMouse.x) > 2.0f || std::fabs(mouse.y - lastMouse.y) > 2.0f;
    if (mouseMoved) {
        lastMouse = mouse;
        for (int i = 0; i < n; ++i)
            if (rowRect(i).contains(mouse)) focus = i;
        if (backRect().contains(mouse)) focus = n;
    }

    ui::drawPanel(window, font, panelRect(), inMatch ? "НАСТРОЙКИ  (ИГРАТА Е НА ПАУЗА)" : "НАСТРОЙКИ");

    // Tabs
    for (int i = 0; i < TAB_COUNT; ++i) {
        sf::FloatRect r = tabRect(i);
        bool selected = (tab == i);
        bool focused = selected && focus == -1;
        sf::RectangleShape t(r.size);
        t.setPosition(r.position);
        t.setFillColor(selected ? pal.buttonHover : pal.button);
        t.setOutlineThickness(focused ? pal.focusOutline : pal.outline);
        t.setOutlineColor(focused ? pal.focus : (selected ? pal.accent : sf::Color(pal.border.r, pal.border.g, pal.border.b, 120)));
        window.draw(t);
        if (selected) {
            sf::RectangleShape bar({ r.size.x, 4.0f });
            bar.setPosition({ r.position.x, r.position.y + r.size.y - 4.0f });
            bar.setFillColor(pal.accent);
            window.draw(bar);
        }
        ui::drawText(window, font, TAB_NAMES[i], 17, { r.position.x + r.size.x / 2.0f, r.position.y + r.size.y / 2.0f - 6.5f },
                     focused ? pal.focus : (selected ? pal.text : pal.textDim), 1, selected);
    }

    // Rows
    for (int i = 0; i < n; ++i) {
        const Row& row = rows[i];
        sf::FloatRect r = rowRect(i);
        bool focused = (focus == i);
        if (focused) {
            sf::RectangleShape bg(r.size);
            bg.setPosition(r.position);
            bg.setFillColor(pal.rowFocus);
            bg.setOutlineThickness(pal.outline);
            bg.setOutlineColor(pal.focus);
            window.draw(bg);
        }
        ui::drawText(window, font, row.label, 18, { r.position.x + 18.0f, r.position.y + r.size.y / 2.0f - 6.5f },
                     focused ? pal.focus : pal.text, 0, focused);

        sf::FloatRect v = valueRect(i);
        std::string value = row.value ? row.value() : std::string();
        bool isButton = (bool)row.activate;
        if (isButton) {
            ui::drawButton(window, font, v, value, focused, v.contains(mouse), true, 16);
            continue;
        }
        sf::RectangleShape box(v.size);
        box.setPosition(v.position);
        box.setFillColor(pal.button);
        box.setOutlineThickness(1.0f);
        box.setOutlineColor(sf::Color(pal.border.r, pal.border.g, pal.border.b, 130));
        window.draw(box);

        // Volume rows get a level bar under the value
        if (tab == 1) {
            int level = (i == 0) ? gameSettings().masterVolume : (i == 1 ? gameSettings().musicVolume : gameSettings().sfxVolume);
            sf::RectangleShape fill({ (v.size.x - 80.0f) * level / 100.0f, 4.0f });
            fill.setPosition({ v.position.x + 40.0f, v.position.y + v.size.y - 7.0f });
            fill.setFillColor(pal.accent);
            window.draw(fill);
        }
        if (row.change) {
            sf::Color ac = focused ? pal.focus : pal.textDim;
            ui::drawArrow(window, { v.position.x + 18.0f, v.position.y + v.size.y / 2.0f }, 14.0f, true, ac);
            ui::drawArrow(window, { v.position.x + v.size.x - 18.0f, v.position.y + v.size.y / 2.0f }, 14.0f, false, ac);
        }
        unsigned int vs = ui::fitTextSize(font, value, 17, 11, v.size.x - 70.0f);
        ui::drawText(window, font, value, vs, { v.position.x + v.size.x / 2.0f, v.position.y + v.size.y / 2.0f - vs * 0.36f - (tab == 1 ? 2.0f : 0.0f) },
                     focused ? pal.focus : pal.text, 1, true);
    }

    // Description of the focused row (or the flash message)
    float descY = rowRect(6).position.y + 18.0f;
    std::string desc;
    sf::Color descColor = pal.textDim;
    if (!flash.empty() && flashClock.getElapsedTime().asSeconds() < 2.5f) {
        desc = flash;
        descColor = pal.ok;
    } else if (focus >= 0 && focus < n) {
        desc = rows[focus].help;
    } else if (focus == -1) {
        desc = "Сменете раздела със стрелките наляво/надясно (или PgUp/PgDn, LB/RB на геймпад).";
    } else {
        desc = "Запазва настройките и се връща назад.";
    }
    if (tab == 1 && focus < n && (flash.empty() || flashClock.getElapsedTime().asSeconds() >= 2.5f))
        desc += "  Нивата важат, когато звуковият модул е наличен.";
    unsigned int ds = ui::fitTextSize(font, desc, 15, 11, PANEL_W - 70.0f);
    ui::drawText(window, font, desc, ds, { PANEL_X + PANEL_W / 2.0f, descY }, descColor, 1);

    ui::drawButton(window, font, backRect(), "НАЗАД (ЗАПАЗИ)", focus == n, backRect().contains(mouse), true, 17);
    ui::drawText(window, font, "Навигация: стрелки / геймпад  |  Промяна: ← →  |  Раздел: PgUp/PgDn  |  Назад: Esc", 13,
                 { PANEL_X + PANEL_W / 2.0f, PANEL_Y + PANEL_H - 32.0f }, pal.textDim, 1);
    window.setView(base);
}
