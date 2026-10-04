> Чернова — ще бъде финализирана след приключване на разработката.

# Как работят екраните и как се добавя нов

Ръководството обяснява как интерфейсът на SFML 3 превключва сцени, как рисува слоевете върху мача, как разпределя входа от клавиатурата и мишката и как рисува текст. Накрая са стъпките за нов екран, с пример: прозорец **„Обзор на мача“**, който се отваря с клавиша `I` по време на игра.

Описано е състоянието на клон `Claude-code` към 03.10.2026 (commit `59fc68b`). Номерата на редове са към тази версия. Фрагментите „преди“ са копирани от кода. Примерът от раздел 7 е проверен: приложен е върху копие на кода, играта се компилира без грешки и предупреждения, а програма, която подава на истинския `UI_map` събития „натиснат `I`“ и „натиснат `Esc`“, потвърждава, че мачът стои на пауза, докато прозорецът е отворен, и продължава след затварянето, без да остава менюто на паузата.

## Съдържание

1. [Три нива на екрани](#1-три-нива-на-екрани)
2. [Прозорецът, изгледът и координатите](#2-прозорецът-изгледът-и-координатите)
3. [Сцени: `UI_main`](#3-сцени-ui_main)
4. [Главното меню: `UI_mainMenu`](#4-главното-меню-ui_mainmenu)
5. [Мачът: слоеве и вход в `UI_map`](#5-мачът-слоеве-и-вход-в-ui_map)
6. [Рисуване на текст](#6-рисуване-на-текст)
7. [Стъпки: нов прозорец в мача](#7-стъпки-нов-прозорец-в-мача)
8. [Стъпки: нов подекран в главното меню](#8-стъпки-нов-подекран-в-главното-меню)
9. [Стъпки: нова сцена](#9-стъпки-нова-сцена)
10. [Чеклист](#10-чеклист)
11. [Предстоящи промени в други клонове](#11-предстоящи-промени-в-други-клонове)

---

## 1. Три нива на екрани

| Ниво | Къде | Какво има днес | Как се превключва |
| --- | --- | --- | --- |
| Сцена | `UI_main` (`enum class UIState`, `UI/includes/UI_main.h:8-12`) | `MAIN_MENU`, `PLAYING`, `QUIT` | сцената вдига флаг-заявка, `UI_main` го чете и сменя `currentState` |
| Подекран на менюто | `UI_mainMenu` (`enum class MenuState`, `UI/includes/UI_mainMenu.h:9-15`) | `MAIN`, `MODE_SELECT`, `PLAY_CONTROLS` (компонент `UI_playControls`), `BOT_DIFFICULTY`, `SETTINGS` | `state = MenuState::...` в `handleEvent` |
| Слой в мача | `UI_map` | пауза, помощ, победа, модални прозорци на играчите, обучение (`UI_tutorial`), изскачащи карти, плаващи надписи | флагове: `isPaused`, `showHelpOverlay`, `engine.getCityState().winner`, `p1Modal.active` / `p2Modal.active`, `tutorial.isActive()` |

Правило: нов екран почти винаги е **нов компонент** (собствен клас `UI_<име>` в `UI/includes` + `UI/scr`) с методи `handleEvent` / `draw` и флагове-заявки, а родителят само го вика. Така са направени `UI_playControls` (в менюто) и `UI_tutorial` (в мача). `Makefile` намира новия `.cpp` сам (`$(wildcard UI/scr/*.cpp)`).

---

## 2. Прозорецът, изгледът и координатите

- **Виртуално платно 1600×900.** Всичко се рисува в координати от `VIRTUAL_WIDTH` × `VIRTUAL_HEIGHT` (`UI/includes/UI_types.h:8-9`), независимо от размера на прозореца.
- **Черни ленти вместо разтягане.** `UI_main::updateViewport` (`UI_main.cpp:22-47`) слага `sf::View` 1600×900 и изчислява viewport с ленти отстрани или отгоре и отдолу. Вика се при старт, при `Resized` и след превключване на цял екран.
- **Мишката винаги минава през `mapPixelToCoords`.** При събитие: `window.mapPixelToCoords(mb->position)`. За посочване (hover) в `draw`: `window.mapPixelToCoords(sf::Mouse::getPosition(window))`. Така координатите са във виртуалното платно и при цял екран, и при променен прозорец.
- **60 кадъра в секунда, без повтаряне на клавиши.** `setFramerateLimit(60)` и `setKeyRepeatEnabled(false)` (`UI_main.cpp:9-10`); `toggleFullscreen` ги слага отново, защото `window.create()` ги нулира (`UI_main.cpp:57-58`). Задържан клавиш дава само едно `KeyPressed`.
- **Ресурси:** шрифтът и текстурите се зареждат с относителни пътища `assets/...`; `main.cpp` прави текуща папката, в която има `assets/` (раздел 4.2 на [CONTRIBUTING.md](CONTRIBUTING.md)).

---

## 3. Сцени: `UI_main`

`UI_main::render` (`UI/scr/UI_main.cpp:64-135`) е главният цикъл на играта:

```cpp
    while (window.isOpen() && currentState != UIState::QUIT) {
        while (const auto event = window.pollEvent()) {
            // Closed, Resized, FocusLost (-> map.onFocusLost()), F11 / Alt+Enter (-> toggleFullscreen)
            ...
            if (currentState == UIState::MAIN_MENU) {
                mainMenu.handleEvent(*event, window);
            } else if (currentState == UIState::PLAYING) {
                map.handleEvent(*event, window);
            }
        }

        if (currentState == UIState::MAIN_MENU) {
            if (mainMenu.isPlayRequested()) {
                mainMenu.resetPlayRequest();
                map.restartMatch();
                map.setControlScheme(mainMenu.getSelectedControlScheme());
                map.setBotDifficulty(mainMenu.getSelectedBotDifficulty());
                map.resetMatchInputState(); // The Enter/Space/click that started the match must not act in it
                currentState = UIState::PLAYING;
            } else if (mainMenu.isQuitRequested()) { ... }
        } else if (currentState == UIState::PLAYING) {
            if (map.isMenuRequested()) { ... currentState = UIState::MAIN_MENU; }
            if (map.isFullscreenRequested()) { ... }
        }

        window.setView(gameView);
        window.clear(sf::Color(10, 14, 22));
        if (currentState == UIState::MAIN_MENU) mainMenu.render(window);
        else if (currentState == UIState::PLAYING) map.render(window);
        window.display();
    }
```

(Съкратено: `...` заменя редове, които не са важни тук.)

Какво следва от това:

1. **Всяко събитие отива само в активната сцена.** Изключение: `F11` и `Alt+Enter` се обработват преди сцената (`UI_main.cpp:81-87`) и с `continue` не стигат до нея. Затова клонът за `F11` в `UI_map::handleEvent` (ред 900) практически не се достига; бутонът „ЦЯЛ ЕКРАН“ в мача минава през `requestFullscreenToggle`.
2. **Сцената не сменя сцената.** Тя вдига флаг (`requestPlay`, `requestMenu`, `requestQuit`, `requestFullscreenToggle`), а `UI_main` го прочита, нулира го и прави прехода. Така всеки преход е на едно място.
3. **Загубата на фокус** (Alt+Tab) поставя мача на пауза (`map.onFocusLost()`, `UI_main.cpp:77-79`).
4. **Ред на кадъра:** събития → заявки → изчистване → рисуване на активната сцена → `display()`. `UI_map::render` в същото време движи и симулацията (раздел 5.2).

---

## 4. Главното меню: `UI_mainMenu`

- **Подекраните** са `MenuState`. `render` (`UI_mainMenu.cpp:610-623`) рисува само текущия, всеки със своя функция `draw...Menu`.
- **Филтър за задържан Enter/Space** (`handleEvent`, редове 394-413): клавишът действа веднъж и трябва да се пусне, преди да действа отново. Без това един задържан Enter би минал през всички подменюта и би пуснал мач.
- **Делегиране към компонент:** когато `state == MenuState::PLAY_CONTROLS`, всяко събитие отива в `playControls.handleEvent`, а менюто само проверява заявките му (редове 415-426):

  ```cpp
      if (state == MenuState::PLAY_CONTROLS) {
          playControls.handleEvent(event, window);
          if (playControls.isStartRequested()) {
              playControls.resetRequests();
              selectedBotDifficulty = BotDifficulty::NONE;
              onPlay();
          } else if (playControls.isBackRequested()) {
              playControls.resetRequests();
              state = MenuState::MODE_SELECT;
          }
          return;
      }
  ```

  Това е образецът за нов подекран (раздел 8).
- **Клавиатура:** избраният бутон е индекс (`selectedMainIndex`, `selectedModeIndex`, ...), който се върти по модул броя бутони: `selectedMainIndex = (selectedMainIndex + 2) % 3;` (ред 438). Enter/Space изпълнява избрания, Esc връща назад.
- **Мишка и посочване:** изборът следва мишката само ако тя наистина се е преместила с повече от 2 px (`lastMenuMousePos`, редове 139-145). Иначе мишка, оставена върху бутон, би „крала“ избора от клавиатурата.
- **Правоъгълниците на бутоните** се изчисляват два пъти: в `draw...Menu` и отново в мишия клон на `handleEvent` (редове 531-607). При промяна на подредбата сменете и двете места. Менюто на паузата избягва това: `drawPauseMenu` записва правоъгълниците в членове (`pauseResumeBtn` и др.), а `handleEvent` само ги проверява (раздел 5.4).
- **Бутон с текст в центъра:** `drawButton` (редове 35-57) центрира точно, като вади и отместването на `getLocalBounds()` (вижте раздел 6).

---

## 5. Мачът: слоеве и вход в `UI_map`

### 5.1. Ред на рисуване

`UI_map::render` (`UI/scr/UI_map.cpp:250-379`) рисува всеки кадър в този ред; по-късното е отгоре:

| № | Слой |
| --- | --- |
| 3–7 | терен и небе, река, парцели, енергийни линии, сгради, контур при поставяне |
| 8–10 | град, лента на влиянието, добивни станции |
| 11–13 | часовници, строителни менюта, ресурси в долните ъгли |
| 14–16 | изскачащи карти на играчите, HUD (бутони долу вдясно), частици за времето |
| 17 | модални прозорци на играчите |
| 17.5 | обучение (`tutorial.draw`) |
| 18–19 | курсорите на играчите, плаващи надписи |
| 20 | екран за победа **или** меню на паузата |
| 21 | помощ (`drawHelpOverlay`), винаги най-отгоре |

Нов прозорец, който покрива всичко, се рисува между 20 и 21. Нещо, което трябва да е под курсорите (например стрелки на обучението), се рисува преди 18.

### 5.2. Кога тече симулацията

```cpp
    // 1. Advance continuous backend simulation (only when NOT paused and game not won)
    if (!isPaused && engine.getCityState().winner == 0) {
        engine.update(dt);
        updateControls(window, dt);
        updateWeatherParticles(dt);
        tutorial.update(dt, engine);
    }
```

(`UI_map.cpp:254-260`.) `isPaused == true` спира едновременно времето в енджина, управлението на играчите, бота и частиците. Затова прозорец, който трябва да спре мача, просто вдига `isPaused`, както прави помощта.

### 5.3. Входът: два пътя

**а) Събития** (`UI_map::handleEvent`, `UI_map_controls.cpp:731-1133`) за всичко, което е „едно натискане“: менюта, прозорци, кликове. Първият блок, който приеме събитието, прави `return`, затова редът на блоковете е приоритетът:

| Приоритет | Блок | Редове |
| --- | --- | --- |
| 1 | екран за победа (R, Esc/M, бутони); поглъща всичко | 735-758 |
| 2 | помощ: затваря се с H/F1/Esc/Enter/Space или клик; поглъща всичко | 764-783 |
| 3 | меню на паузата: стрелки/W/S, Enter/Space, R, H/F1, M, бутони; поглъща всичко | 789-860 |
| 4 | модални прозорци: всеки се затваря само с клавишите на своя играч | 866-875 |
| 5 | обучение: `tutorial.handleClick` / `handleKey` връщат `true`, ако са поели събитието | 880-897 |
| 6 | клавиши на системата: H/F1 (помощ), Esc и M (пауза) | 906-928 |
| 7 | мишка: десен клик (отказ), бутоните долу вдясно, после действия само за играча, на когото е мишката (`mouseOwnerAt`) | 930-1132 |

**б) Състояние на клавиатурата** (`UI_map::updateControls`, `UI_map_controls.cpp:367-729`) за играта: движение, действие, смяна на сграда. Всеки кадър се чете `sf::Keyboard::isKeyPressed(...)`, а „ново натискане“ се познава по флаг за предишния кадър:

```cpp
    bool p1PressingAction = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space) ||
                            (allowArrowsForP1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter));
    bool p1JustPressed = p1PressingAction && !p1PrevAction;
    p1PrevAction = p1PressingAction;

    if (p1JustPressed && p1ActionCooldown <= 0.0f && !p1Modal.active && !showHelpOverlay) {
        executeP1Action();
        p1ActionCooldown = 0.20f;
    }
```

(`UI_map_controls.cpp:540-548`.) `updateControls` не прави нищо на пауза и когато прозорецът няма фокус (редове 368-377). Затова прозорец, който вдига `isPaused`, не трябва да добавя нови проверки в него.

**в) Пускане на клавиша след затваряне.** Клавишът, който затваря прозорец (Enter, Space, Esc), често е и клавиш за игра. Ако е още натиснат в следващия кадър, `updateControls` би го видял като ново натискане. Затова при всяко затваряне се вика `primeInputEdges(...)` (`UI_map_controls.cpp:51-67`): всички флагове „предишен кадър“ стават `true` и клавишът трябва да се пусне, преди да подейства. Примери: затваряне на помощта (ред 768), „ПРОДЪЛЖИ“ в паузата (редове 793, 807, 840), затваряне на модален прозорец (`closePlayerModal`, `UI_map_popups.cpp:151`), клавиш, поет от обучението (ред 893).

### 5.4. Съществуващите слоеве като образци

| Слой | Състояние | Рисуване | Вход | Особеност |
| --- | --- | --- | --- | --- |
| Пауза | `isPaused`, `pauseSelectedIdx` | `drawPauseMenu` (`UI_map_overlays.cpp:366-467`) | приоритет 3 | Правоъгълниците на бутоните се записват в членове при рисуване (редове 425-428) и се изчисляват и без шрифт, за да може да се кликне. |
| Помощ | `showHelpOverlay`, `helpOpenedFromPause` | `drawHelpOverlay` (редове 81-184) | приоритет 2 | Отворена с H по време на игра, тя вдига и `isPaused`; при затваряне `isPaused = helpOpenedFromPause` връща играта или паузата. |
| Победа | `engine.getCityState().winner != 0` | `drawVictoryScreen` (редове 186-364) | приоритет 1 | Бутоните `victoryRestartBtn` / `victoryMenuBtn` също се записват при рисуване. |
| Модален прозорец | `p1Modal` / `p2Modal` | `drawPlayerModals` (`UI_map_popups.cpp:154-226`) | приоритет 4 и клик на OK | Покрива само половината на своя играч; срещу бот модалът на Играч 2 става изскачаща карта (редове 122-125). |
| Изскачаща карта | `p1Popup` / `p2Popup` с таймер 4 s | `drawPlayerPopups` (редове 49-118) | няма | Не блокира; отброява се в `updateControls`. |
| Обучение | компонент `UI_tutorial` | `tutorial.draw(...)` | приоритет 5 | `handleClick` / `handleKey` връщат `bool` „поех го“. |

### 5.5. Нулиране при нов мач

`UI_map::restartMatch` (`UI_map.cpp:182-239`) нулира всяко състояние на слоевете (`isPaused`, `showHelpOverlay`, модални прозорци, изскачащи карти, частици, мълнии) и вика `resetMatchInputState()`. Всеки нов флаг на слой трябва да се нулира там, иначе прозорец от стария мач ще се появи в новия.

---

## 6. Рисуване на текст

- **Шрифтът** се зарежда веднъж в конструктора на всяка сцена: `font.openFromFile("assets/font.ttf")` в `UI_map` (`UI_map.cpp:27-32`) и в `UI_mainMenu` (`UI_mainMenu.cpp:19-23`). Компонентите не зареждат шрифт, а го получават като параметър заедно с флаг: `draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded, ...)`.
- **Без шрифт не се създава `sf::Text`.** Всеки метод проверява `fontLoaded` / `resourcesLoaded`. Правоъгълниците на бутоните се рисуват и без шрифт (`UI_map_overlays.cpp:418`), а `UI_map` изключва обучението и модалните прозорци, ако шрифтът липсва (`UI_map.cpp:262-268`), за да не блокират невидимо входа.
- **В SFML 3 шрифтът е в конструктора на текста** и кирилицата минава през `toUtf8()`:

  ```cpp
  sf::Text tHeader(font, toUtf8(pTag), 12);            // UI_buildings.cpp:101
  tHeader.setFillColor(accentColor);
  tHeader.setPosition({panelPos.x + 8.0f, panelPos.y + 6.0f});
  window.draw(tHeader);
  ```

- **Точно центриране.** `getLocalBounds()` връща правоъгълник с отместване (`position`), което не е нула. Точното центриране го вади, както в `drawButton` (`UI_mainMenu.cpp:50-54`):

  ```cpp
          sf::FloatRect textBounds = label.getLocalBounds();
          label.setPosition({
              bounds.position.x + (bounds.size.x - textBounds.size.x) / 2.0f - textBounds.position.x,
              bounds.position.y + (bounds.size.y - textBounds.size.y) / 2.0f - textBounds.position.y
          });
  ```

  На много места в `UI_map` хоризонталното центриране е без `- position.x` (например `UI_map_overlays.cpp:48`). Разликата е 1–2 px; в нов код ползвайте точната форма.
- **Няколко реда:** SFML не пренася текст сам. Редовете се разделят с `\n`, разстоянието се задава със `setLineSpacing(1.25f)`, а височината на блока се взема от `getLocalBounds().size.y` (функцията `drawSection` в помощта, `UI_map_overlays.cpp:133-147`).
- **Удебелен шрифт:** `setStyle(sf::Text::Bold)` (`UI_map_overlays.cpp:108`).
- **Избледняване:** цветът се взема с алфа канал от таймера (изскачащите карти, `UI_map_popups.cpp:55-56`).
- **Цветовете, които се повтарят:**

  | За какво | Цвят |
  | --- | --- |
  | Играч 1 (запад) | `sf::Color(0, 229, 255)` (циан) |
  | Играч 2 (изток) | `sf::Color(255, 120, 200)` (розово) |
  | избран бутон, заглавие на пауза | `sf::Color(255, 215, 0)` (златно) |
  | фон на карта / панел | `sf::Color(16, 22, 34, 252)` |
  | затъмняване на цял екран | `sf::Color(8, 12, 20, 215)` (пауза), `sf::Color(5, 10, 18, 205)` (помощ) |
  | основен текст | `sf::Color(220, 235, 255)` |

Правилата за българския текст (главни букви, клавиши в `[ ]`, мерни единици) са в раздел 7 на [CONTRIBUTING.md](CONTRIBUTING.md).

---

## 7. Стъпки: нов прозорец в мача

Пример: **„Обзор на мача“**. С `I` по време на игра се отваря прозорец с мощността, парите, златото и дела от града на двамата играчи. Докато е отворен, мачът е на пауза. Затваря се с `I`, `Esc`, `Enter`, `Space` или с бутона.

### 7.1. Изберете свободен клавиш

Заети днес по време на мач:

| Кой | Клавиши |
| --- | --- |
| Играч 1 | W, A, S, D, Space, E, Q, X, F, 1–6; срещу бот и стрелките, Enter, Page Up/Down, Delete, Backspace, Right Shift, End |
| Играч 2 | стрелките, Enter, Page Up/Down, Delete, Right Shift, End |
| Система | Esc, H, F1, M, F11, Alt+Enter; R на паузата и на екрана за победа |

`I` е свободен. Проверете и раздел 11: някои клавиши вече са взети в клонове, които още не са слети.

### 7.2. Компонент: заглавен файл

`UI/includes/UI_matchOverview.h`:

```cpp
#ifndef UI_MATCHOVERVIEW_H
#define UI_MATCHOVERVIEW_H

#include <SFML/Graphics.hpp>
#include "../../Game/includes/game_main.h"

// Full-screen "match overview" overlay: power, money, gold and city share of both players.
class UI_matchOverview {
private:
    bool open = false;
    sf::FloatRect closeBtn; // computed in draw(), used by handleEvent()

public:
    bool isOpen() const { return open; }
    void show() { open = true; }
    void hide() { open = false; }

    // Returns true when the event was consumed (every event while the overlay is open)
    bool handleEvent(const sf::Event& event, const sf::RenderWindow& window);
    void draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded, const GameEngine& engine);
};

#endif // UI_MATCHOVERVIEW_H
```

Компонентът получава енджина като `const GameEngine&`: интерфейсът само чете състоянието и не променя правилата.

### 7.3. Компонент: вход и рисуване

`UI/scr/UI_matchOverview.cpp`:

```cpp
#include "../includes/UI_matchOverview.h"
#include "../includes/UI_types.h"
#include <cmath>
#include <string>

bool UI_matchOverview::handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    if (!open) return false;

    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        if (key->code == sf::Keyboard::Key::I || key->code == sf::Keyboard::Key::Escape ||
            key->code == sf::Keyboard::Key::Enter || key->code == sf::Keyboard::Key::Space) {
            open = false;
        }
    }
    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button == sf::Mouse::Button::Left && closeBtn.contains(window.mapPixelToCoords(mb->position))) {
            open = false;
        }
    }
    return true; // an open overlay swallows every event
}

void UI_matchOverview::draw(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                            const GameEngine& engine) {
    if (!open) return;

    // Dim the whole match, then one card in the middle of the 1600x900 canvas
    sf::RectangleShape backdrop({ VIRTUAL_WIDTH, VIRTUAL_HEIGHT });
    backdrop.setFillColor(sf::Color(8, 12, 20, 215));
    window.draw(backdrop);

    const sf::FloatRect card({ 400.0f, 200.0f }, { 800.0f, 420.0f });
    sf::RectangleShape box(card.size);
    box.setPosition(card.position);
    box.setFillColor(sf::Color(16, 22, 34, 252));
    box.setOutlineThickness(2.5f);
    box.setOutlineColor(sf::Color(255, 215, 0));
    window.draw(box);

    // The button rectangle is set even without a font, so a click can still close the overlay
    closeBtn = sf::FloatRect({ card.position.x + (card.size.x - 260.0f) / 2.0f, card.position.y + card.size.y - 62.0f },
                             { 260.0f, 42.0f });
    sf::RectangleShape btn(closeBtn.size);
    btn.setPosition(closeBtn.position);
    btn.setFillColor(sf::Color(35, 110, 65));
    btn.setOutlineThickness(1.5f);
    btn.setOutlineColor(sf::Color(70, 210, 110));
    window.draw(btn);

    if (!fontLoaded) return;

    sf::Text title(font, toUtf8("ОБЗОР НА МАЧА - ДЕН " + std::to_string(engine.getCurrentDay())), 22);
    title.setStyle(sf::Text::Bold);
    title.setFillColor(sf::Color(255, 215, 0));
    sf::FloatRect tb = title.getLocalBounds();
    title.setPosition({ card.position.x + (card.size.x - tb.size.x) / 2.0f - tb.position.x, card.position.y + 22.0f });
    window.draw(title);

    const auto& city = engine.getCityState();
    const int p1Pct = static_cast<int>(std::lround(city.p1CityShare * 100.0f));
    const sf::Color accent[2] = { sf::Color(0, 229, 255), sf::Color(255, 120, 200) };

    for (int player = 1; player <= 2; ++player) {
        const PlayerEconomy& econ = engine.getPlayerEconomy(player);
        const int sharePct = (player == 1) ? p1Pct : 100 - p1Pct;
        const float colX = card.position.x + 70.0f + (player - 1) * 380.0f;

        sf::Text head(font, toUtf8(player == 1 ? "ИГРАЧ 1 (ЗАПАД)" : "ИГРАЧ 2 (ИЗТОК)"), 17);
        head.setStyle(sf::Text::Bold);
        head.setFillColor(accent[player - 1]);
        head.setPosition({ colX, card.position.y + 80.0f });
        window.draw(head);

        std::string lines = "Мощност сега: " + std::to_string(econ.energyMW) + " MW\n" +
                            "Средно днес: " + std::to_string(static_cast<int>(engine.getTodayAverageMW(player))) + " MW\n" +
                            "Пари: " + std::to_string(econ.money) + " $\n" +
                            "Злато: " + std::to_string(econ.gold) + " G\n" +
                            "Дял от града: " + std::to_string(sharePct) + "%";
        sf::Text body(font, toUtf8(lines), 15);
        body.setFillColor(sf::Color(220, 235, 255));
        body.setLineSpacing(1.3f);
        body.setPosition({ colX, card.position.y + 116.0f });
        window.draw(body);
    }

    sf::Text demand(font, toUtf8("Градът иска: " + std::to_string(city.cityEnergyDemand) + " MW"), 14);
    demand.setFillColor(sf::Color(150, 185, 220));
    sf::FloatRect db = demand.getLocalBounds();
    demand.setPosition({ card.position.x + (card.size.x - db.size.x) / 2.0f - db.position.x, card.position.y + 300.0f });
    window.draw(demand);

    sf::Text tBtn(font, toUtf8("ЗАТВОРИ [I / ESC]"), 15);
    tBtn.setFillColor(sf::Color::White);
    sf::FloatRect bb = tBtn.getLocalBounds();
    tBtn.setPosition({ closeBtn.position.x + (closeBtn.size.x - bb.size.x) / 2.0f - bb.position.x,
                       closeBtn.position.y + (closeBtn.size.y - bb.size.y) / 2.0f - bb.position.y });
    window.draw(tBtn);
}
```

Дяловете се показват като `p1Pct` и `100 - p1Pct`, както в енджина (`game_main.cpp:396-397`), за да е сборът винаги 100%.

### 7.4. Член в `UI_map`

`UI/includes/UI_map.h`: включване след `#include "UI_tutorial.h"` и член след `UI_tutorial tutorial;` (ред 40):

```cpp
#include "UI_matchOverview.h"
```

```cpp
    UI_matchOverview overview; // [I] match overview overlay (pauses the match while open)
```

### 7.5. Вход: блок с правилния приоритет

В `UI_map::handleEvent` (`UI_map_controls.cpp`) прозорецът трябва да получава събитията **преди** менюто на паузата, защото вдига `isPaused`. Иначе блокът на паузата (ред 789) би поел `Esc` и би затворил паузата вместо прозореца. Блокът влиза веднага след блока на помощта (след ред 783):

```cpp
    // -------------------------------------------------------------------------
    // 0.5 Match overview: takes every event while open; closing it resumes the match
    // -------------------------------------------------------------------------
    if (overview.isOpen()) {
        overview.handleEvent(event, window);
        if (!overview.isOpen()) {
            isPaused = false;
            primeInputEdges(0); // The I/Esc/Enter/Space that closed it must not act in-game
        }
        return;
    }
```

Клавишът за отваряне влиза в блока с клавишите на системата, преди `Escape` (ред 916):

```cpp
        if (key->code == sf::Keyboard::Key::I) {
            // Pause the match (render() stops engine.update and updateControls) and show the overview
            isPaused = true;
            pauseSelectedIdx = 0;
            overview.show();
            return;
        }
```

Така прозорецът не се отваря върху екрана за победа (приоритет 1), върху помощта (2) или от менюто на паузата (3). Ако искате бутон в паузата, добавете го там по образеца на „ПОМОЩ И ПРАВИЛА“.

### 7.6. Рисуване: между паузата и помощта

В `UI_map::render` (`UI_map.cpp:370-378`) менюто на паузата се рисува, когато `isPaused` е вдигнат. Прозорецът също вдига `isPaused`, затова паузата не трябва да се рисува под него:

```cpp
    // 20. Pause Menu (drawn before help so help is layered on top)
    if (engine.getCityState().winner != 0) {
        drawVictoryScreen(window);
    } else if (isPaused && !overview.isOpen()) {
        drawPauseMenu(window);
    }

    // 20.5 Match overview (it pauses the match, but replaces the pause menu instead of covering it)
    overview.draw(window, font, resourcesLoaded, engine);

    // 21. Help & Rules Manual Overlay — ALWAYS on top of everything (including pause menu)
    drawHelpOverlay(window);
```

### 7.7. Нулиране при нов мач

В `UI_map::restartMatch` (`UI_map.cpp:186-191`), до `showHelpOverlay = false;`:

```cpp
    overview.hide();
```

### 7.8. Документация и подсказки

- Добавете клавиша в раздел „4. УПРАВЛЕНИЕ И БЪРЗИ КЛАВИШИ“ на помощта (`UI_map_overlays.cpp:178-182`) и в лентата с подсказки (`helpText`, ред 72), ако има място.
- Обновете `docs/CONTROLS.md` (раздел 7 „По време на мача“).

### 7.9. Проверка

- Компилация на цялата игра без предупреждения и 8-секундно пробно пускане (раздел 4.3 на [CONTRIBUTING.md](CONTRIBUTING.md)).
- Ръчно: `I` отваря, `I` / `Esc` / клик на бутона затварят; часовникът стои, докато прозорецът е отворен; `Space` или `Enter`, с които сте затворили, не строят и не добиват; `H` над прозореца не прави нищо; `F11` превключва цял екран и прозорецът остава на място; бутонът се натиска и след промяна на размера на прозореца; Alt+Tab и обратно; нов мач от паузата не показва прозореца.

---

## 8. Стъпки: нов подекран в главното меню

Пример: подекран „КАК СЕ ИГРАЕ“, отварян от главното меню.

1. **Компонент** `UI_howToPlay` (`UI/includes/UI_howToPlay.h`, `UI/scr/UI_howToPlay.cpp`) по образеца на `UI_playControls` (`UI/includes/UI_playControls.h`): `handleEvent(const sf::Event&, const sf::RenderWindow&)`, `draw(sf::RenderWindow&, const sf::Font&, bool fontLoaded)` и флаг-заявка `isBackRequested()` / `resetRequests()`. Esc вдига `requestBack`.
2. **Състояние:** нова стойност в `enum class MenuState` (`UI_mainMenu.h:9-15`) и член `UI_howToPlay howToPlay;` до `UI_playControls playControls;` (ред 43).
3. **Вход:** в началото на `UI_mainMenu::handleEvent`, веднага след блока за `PLAY_CONTROLS` (редове 415-426), същият вид делегиране: при `state == MenuState::HOW_TO_PLAY` събитието отива в компонента, а при `isBackRequested()` менюто връща `state = MenuState::MAIN`.
4. **Рисуване:** нов клон в `UI_mainMenu::render` (редове 610-623): `drawHeader(window); howToPlay.draw(window, font, fontLoaded);`.
5. **Бутон в главното меню.** Тук числата са записани на няколко места и всички трябва да се сменят заедно:
   - броят бутони в модулната аритметика: `(selectedMainIndex + 2) % 3` и `(selectedMainIndex + 1) % 3` (редове 438, 440) стават `% 4`;
   - изпълнението по индекс (редове 441-450): кой индекс какво отваря;
   - правоъгълниците в `drawMainMenu` (редове 135-137) и същите правоъгълници в мишия клон (редове 541-548);
   - посочването с мишката (редове 142-144);
   - подсказката под бутоните (ред 158 е на `y = 470`) и коментарът `// 0: Play, 1: Settings, 2: Quit` в `UI_mainMenu.h:23`.
6. **Филтърът за задържан Enter/Space** (редове 394-413) работи и за новия подекран, защото е преди делегирането.

---

## 9. Стъпки: нова сцена

Нова сцена е нужна рядко: за екран, който не е част от менюто и не е част от мача (например надписи накрая).

1. Стойност в `enum class UIState` (`UI_main.h:8-12`) и член-компонент в `UI_main` до `UI_mainMenu mainMenu;` и `UI_map map;`.
2. Компонентът зарежда свой шрифт в конструктора (както `UI_mainMenu`) или го получава отвън.
3. В `UI_main::render` три места: маршрут на събитията (`UI_main.cpp:91-95`), проверка на заявките на компонента (редове 98-122) и рисуване (редове 127-131).
4. Компонентът не сменя сцената сам: вдига флаг (`isDoneRequested()`), а `UI_main` прави прехода и нулира флага. Ако преходът е към мача, повторете стъпките на `isPlayRequested()`: `restartMatch()`, схема, трудност, `resetMatchInputState()`.

---

## 10. Чеклист

- [ ] Новият екран е отделен компонент; в `UI_map` / `UI_mainMenu` / `UI_main` има само няколко реда, които го викат.
- [ ] Блокът за вход е на правилното място в приоритета и поглъща събитията, докато екранът е отворен.
- [ ] При затваряне се вика `primeInputEdges(...)` (в мача) или филтърът за Enter/Space покрива екрана (в менюто).
- [ ] Ако екранът трябва да спре мача, вдига `isPaused`, а менюто на паузата не се рисува под него.
- [ ] Правоъгълниците за клик се изчисляват и без шрифт и съвпадат с нарисуваното.
- [ ] Мишката минава през `mapPixelToCoords`.
- [ ] Флагът се нулира в `restartMatch()`.
- [ ] Текстът е на български, през `toUtf8()`, а клавишите в подсказките са верни за режима.
- [ ] Компилация без предупреждения, всички тестове, 8-секундно пускане и ръчната проверка от 7.9.

---

## 11. Предстоящи промени в други клонове

Следните клонове още не са слети в `Claude-code`, но засягат екраните (в разработка). Проверка: `git diff --stat HEAD...<клон>`.

- **`wave-b-info`**: табло, докато е задържан `Tab` (`UI_dashboard`), панел за разработчици с `F3` (`UI_devOverlay`), дневник на събитията от менюто на паузата, отчет след мача вместо екрана за победа (`UI_postmatch`). Всички са отделни компоненти, викани от `UI_map` и от нов файл `UI/scr/UI_map_info.cpp`, т.е. по същия образец като раздел 7.
- **`wave-b-session`**: `F5` и `F9` по време на мач (запис и зареждане) и таблица с клавиши `InputMap` (`UI/includes/UI_inputmap.h`), за да се сменят клавишите. След сливането новите клавиши трябва да минават през нея, а не да се четат директно със `sf::Keyboard::Key`.
- **`wave-c-demo`**: `F9` в главното меню пуска демо режима за журито.
- **`wave-b-power`**: избор на карта (`UI_mapSelect`) вътре в подекрана за режим на игра.
- **`wave-a-ui`** (`e7c9c9a`): `UI/includes/UI_text.h` с `ui::makeText` и `ui::drawText`, през които минава всеки текст, и режим `--lint`, който открива застъпен текст, текст извън рамката си или извън платното 1600×900. След сливането рисувайте текста на новия екран с `ui::drawText`.
