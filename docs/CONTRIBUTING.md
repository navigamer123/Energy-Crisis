> Чернова — ще бъде финализирана след приключване на разработката.

# Как да работите по Energy Crisis

Документът е за всеки, който пише код за играта: съотборник, нов участник или агент. Описва клоновете и работните копия, правилото за малки commit-и, стила на commit съобщенията, компилацията, тестовете, стила на кода и правилата за българския текст в интерфейса. Описано е състоянието на клон `Claude-code` към 03.10.2026 (commit `59fc68b`). Всичко, което още не е слято в `Claude-code`, е отбелязано с **(в разработка)**. Номерата на редове са към тази версия.

## Съдържание

1. [Клонове и работни копия](#1-клонове-и-работни-копия)
2. [Малки commit-и](#2-малки-commit-и)
3. [Commit съобщения](#3-commit-съобщения)
4. [Компилация](#4-компилация)
5. [Тестове](#5-тестове)
6. [Стил на кода](#6-стил-на-кода)
7. [Български текст в интерфейса](#7-български-текст-в-интерфейса)
8. [Чеклист преди commit](#8-чеклист-преди-commit)
9. [Свързани документи](#9-свързани-документи)

---

## 1. Клонове и работни копия

### Клоновете

| Клон | За какво е |
| --- | --- |
| `main` | Клонът в GitHub (`navigamer123/Energy-Crisis`), последно `ef39ddb` (сливане на PR #1 от `UI_Main_addon`). |
| `Claude-code` | Интеграционният клон: поправките (`7da0163`, `fdd89c5`, `0ee54a6`, `40fd272`), тестовете и документацията. Всички нови клонове започват оттук. |
| `wave-<вълна>-<тема>` | Един клон за една задача, например `wave-a-build` (CMake, CI, Makefile), `wave-a-engine` (MatchConfig, опашка от събития, стъпка 60 Hz), `wave-b-session` (настройки, запис), `wave-q-devdocs` (този документ). |

Към 03.10.2026 има 17 клона `wave-*`, които се разработват едновременно. Командата `git worktree list` показва кой клон в коя папка е.

### Работни копия (git worktree)

Всеки клон `wave-*` живее в собствена папка до главното копие. Така няколко души (или агенти) работят паралелно, без да превключват клонове един на друг.

```bash
# Ново работно копие за нова задача (от главното копие на проекта)
git worktree add ../ec-wt/<тема> -b wave-<вълна>-<тема> Claude-code

# Какво има и къде
git worktree list
git -C ../ec-wt/<тема> status

# След като клонът е слят
git worktree remove ../ec-wt/<тема>
```

Правила:

- **Работете само в своето работно копие.** Не редактирайте файлове в главното копие или в чуждо работно копие и не превключвайте клона в тях. Git сам отказва `checkout` на клон, който вече е отворен в друго работно копие.
- **Не правете `push` и не сливайте сами.** Клоновете `wave-*` се сливат в `Claude-code` от човека, който интегрира вълната.
- **Пипайте само файловете на задачата.** Не преформатирайте чужд код, не местете функции „за ред“ и не сменяйте края на редовете. Всяка излишна промяна е бъдещ конфликт.
- **Внимавайте с горещите файлове.** Към 03.10.2026 `Game/includes/game_main.h` се променя в 10 паралелни клона, а `Game/scr/game_main.cpp` в 8 (проверка: `git diff --name-only HEAD...<клон>` за всеки клон `wave-*`). Там правете малки, локални добавки. Нова система по-добре влиза в нов файл (`Game/scr/game_<тема>.cpp`, `UI/scr/UI_<тема>.cpp`), а в горещия файл остават само няколко реда, които я извикват.
- **Новите файлове се намират сами.** `Makefile` компилира `$(wildcard UI/scr/*.cpp)` и `$(wildcard Game/scr/*.cpp)`, а `make test` намира всеки `scratch/test_*.cpp`. Не е нужно да ги вписвате никъде.

---

## 2. Малки commit-и

Правилото: **един commit = една логическа промяна, която се компилира и минава тестовете.** Commit-вайте след всяка завършена стъпка, а не накрая. Никога не commit-вайте счупена компилация или паднал тест.

Защо: малък commit се преглежда за минути, лесно се връща с `git revert` и рядко влиза в конфликт с другите клонове.

Добри примери от историята:

| Commit | Какво съдържа |
| --- | --- |
| `8c77bcc build(cmake): add CMake build for SFML 3 with headless engine library and CTest` | само `CMakeLists.txt` и `.gitignore` |
| `7fcb9fc build(ci): add GitHub Actions workflow for Linux, Windows and headless engine tests` | само `.github/workflows/ci.yml` |
| `15a1ed4 build(make): object-based Makefile with header dependencies for Linux and MinGW` | само `Makefile` и `.gitignore` |
| `6282f80`, `17c6095`, `bae2256`, `f17dea5`, `9941556`, `01a30d9` | шест отделни стъпки в `wave-a-engine`: refactor, опашка от събития, MatchConfig, RNG, стъпка 60 Hz, производителност |

Пример за твърде голям commit: `fdd89c5 fix(ui): input ownership, focus handling, clean restart, menu flow, lightning rules, victory draw` събира шест независими теми. Днес той би бил шест commit-а, по един за всяка тема.

Как изглежда една стъпка:

```bash
# 1. Компилация и тестове минават (раздели 4 и 5)
# 2. Добавяте САМО файловете на стъпката (не git add -A / git add .)
git add Game/includes/game_main.h Game/includes/game_balance.h Game/scr/game_main.cpp
git diff --cached --stat
# 3. Commit със заглавие, кратко обяснение и (ако сте ползвали Claude) реда за съавтор
git commit -m "feat(engine): add biogas plant type, balance numbers, cost and per-player limit" \
           -m "55 MW day and night, at most 3 per player; the example from docs/HOWTO_ADD_BUILDING.md." \
           -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

## 3. Commit съобщения

Формат на заглавието: **`тип(област): описание`**, на английски, с малка буква, без точка накрая, в повелително наклонение („add“, „fix“, а не „added“).

| Тип | Кога | Пример от историята |
| --- | --- | --- |
| `feat` | нова възможност | `feat(engine): add event queue drained by pollEvents` |
| `fix` | поправка | `fix(engine): one settlement per day, whole-day energy verdict, working batteries` |
| `refactor` | преструктуриране без промяна в поведението | `refactor(engine): remove dead PlayerData mirror and keep plot geometry in Balance` |
| `perf` | производителност | `perf(engine): grid update without per-step allocations or string copies` |
| `test` | само тестове | `test: rewrite stale tests, add regression suite and make test target` |
| `docs` | документация | `docs(readme): rewrite overview, quick start and links to the docs` |
| `build` | компилация, CMake, CI | `build(ci): add GitHub Actions workflow for Linux, Windows and headless engine tests` |

Областта е частта от кода: `engine`, `ui`, `map`, `ai`, `settings`, `save`, `readme`, `cmake`, `ci`, `make` и т.н. Без област, ако промяната е навсякъде (`test: ...`, `docs: ...`).

Тяло: 1–3 реда, какво и защо (не как). Пример от `7da0163`:

```text
Fixes the double day-end, scores the day on average delivered MW, makes batteries discharge
for any shortfall and pay for their charge, removes the ore wildcard, mirrors land prices, ...
```

Commit-ите, направени с помощта на Claude, завършват с реда `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>` (така са всички commit-и от `7da0163` нататък).

---

## 4. Компилация

### 4.1. Инструменти

| Компонент | Версия | Бележка |
| --- | --- | --- |
| Компилатор | GCC с пълен C++17; под Windows **WinLibs GCC 14.2.0** (MinGW-w64, x86_64, UCRT, posix, SEH) | с него се компилира играта на машината на екипа |
| Графика | **SFML 3.1.0**, компилиран за същия компилатор | SFML 2.x не става: кодът ползва API на SFML 3 |
| make | `make` (Linux) или `mingw32-make` (Windows, идва с WinLibs) | |

Старият MinGW.org g++ 6.3 няма пълен C++17 и не може да компилира SFML 3. С него се компилира само енджинът за тестовете, благодарение на `scratch/sfml_stub/compat_cxx17.h`, който добавя `std::clamp`.

Енджинът (`Game/`) включва само `<SFML/Graphics.hpp>` (`Game/includes/game_main.h:6`) и ползва само `sf::Vector2f` и `sf::FloatRect`. Затова тестовете се компилират без SFML със заместителя `scratch/sfml_stub/SFML/Graphics.hpp`.

### 4.2. Makefile (днес)

| Команда | Какво прави |
| --- | --- |
| `make` | компилира `main.cpp`, `UI/scr/*.cpp`, `Game/scr/*.cpp` до `test_game` (`test_game.exe`) със `-std=c++17 -Wall -Wextra` и `-lsfml-graphics -lsfml-window -lsfml-system` |
| `make run` | компилира и пуска играта |
| `make test` | компилира и пуска всеки `scratch/test_*.cpp` само с енджина (раздел 5) |
| `make clean` | изтрива `test_game` и `scratch/bin/` |

Под Windows (Git Bash), когато SFML не е в стандартните пътища:

```bash
export PATH="/c/tools/mingw64/bin:$PATH"     # пътят до WinLibs GCC 14
SFML=C:/tools/SFML-3.1.0                      # пътят до SFML 3.1.0, във вида C:/..., не /c/...

mingw32-make CXXFLAGS="-std=c++17 -Wall -Wextra -IUI/includes -IGame/includes -I$SFML/include" \
             LIBS="-L$SFML/lib -lsfml-graphics -lsfml-window -lsfml-system"

cp "$SFML"/bin/sfml-graphics-3.dll "$SFML"/bin/sfml-window-3.dll "$SFML"/bin/sfml-system-3.dll .
cp /c/tools/mingw64/bin/{libstdc++-6.dll,libgcc_s_seh-1.dll,libwinpthread-1.dll} .
./test_game.exe
```

Пътят до SFML стига до `g++` вътре в `-I...` и `-L...`, а `g++` е програма за Windows и не разбира пътища от Git Bash. С `SFML=/c/tools/SFML-3.1.0` компилацията спира с `fatal error: SFML/Graphics.hpp: No such file or directory`. Затова пишете `C:/...` (или `SFML=$(cygpath -m /c/tools/SFML-3.1.0)`). Командите по-горе са проверени с WinLibs GCC 14.2.0 и SFML 3.1.0: компилацията минава за около 2 мин и 40 s, без предупреждения.

Ограничение: целта `test_game` няма зависимост от заглавните файлове и компилира всичко наново при всяко извикване.

Играта зарежда `assets/font.ttf` и `assets/grass.png` с относителни пътища. `main.cpp` (`selectAssetDirectory`, редове 14–56) първо търси `assets/` до изпълнимия файл и прави тази папка текуща, а ако там няма, остава в текущата папка.

### 4.3. Скриптове на машината на екипа

На машината на екипа инструментите са в `C:/Users/admin/Projects/ec-tools` (извън репото):

```bash
# Цялата игра, инкрементално, -j8: WinLibs GCC 14.2 + SFML 3.1.0, -O0 -g -Wall -Wextra -MMD -MP.
# Резултат: <OBJ>/energy_crisis.exe + DLL файловете на SFML и MinGW + копие на assets/
bash C:/Users/admin/Projects/ec-tools/build_game.sh <папка-с-кода> <папка-за-компилация>

# Пробно пускане за 8 секунди. Код 124 = играта още работеше и timeout я спря (това е успех).
cd <папка-за-компилация> && timeout 8 ./energy_crisis.exe; echo "exit=$?"
```

При успешно пускане в конзолата се появяват редовете `[Main] Assets folder: ...`, `[GameEngine] Backend initialized with 24 land plots & 0 starter buildings.` и `[Main] Calling main UI function render()...`.

Изискване: компилацията завършва **без грешки и без нови предупреждения** (`-Wall -Wextra`).

### 4.4. CMake (в разработка, клон `wave-a-build`)

`CMakeLists.txt` е в клон `wave-a-build` (commit `8c77bcc`) и още не е слят в `Claude-code`. Командите от заглавието му:

```bash
cmake -S . -B build                         # с инсталиран SFML 3
cmake -S . -B build -DEC_FETCH_SFML=ON      # сваля и компилира SFML 3.1.0, ако няма инсталиран
cmake -S . -B build -DEC_BUILD_GAME=OFF     # само енджинът и тестовете, без SFML
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Цели: `ec_engine` (статична библиотека от `Game/scr/*.cpp`), `energy_crisis` (играта в `build/bin` с `assets/` до нея), по една цел `test_*` за всеки `scratch/test_*.cpp`, `run`. В същия клон `Makefile` е преработен (`15a1ed4`): обектни файлове в `build/make`, `-MMD -MP` и променлива `SFML_DIR=`.

---

## 5. Тестове

Тестовете са програми в `scratch/test_*.cpp`, всяка със собствен `main()`. Компилират се с енджина (`Game/scr/*.cpp`) и заместителя на SFML, без интерфейса. Подробно: [TESTING.md](TESTING.md).

```bash
make test               # компилира и пуска всички; спира при първия паднал
make -k test            # пуска всички, дори някой да падне
make test EC_SEED=42    # същото време и същите случайни събития при всяко пускане
```

Ръчно, за един тест:

```bash
g++ -std=c++17 -Wall -Wextra -Iscratch/sfml_stub -IGame/includes \
    -include scratch/sfml_stub/compat_cxx17.h \
    scratch/test_regressions.cpp Game/scr/*.cpp -o test_regressions
timeout 300 ./test_regressions; echo "exit=$?"
```

На машината на екипа:

```bash
REPO=$(cygpath -u <папка-с-кода>) bash C:/Users/admin/Projects/ec-tools/build_headless.sh \
    <папка-с-кода>/scratch/test_regressions.cpp test_regressions.exe
timeout 300 ./test_regressions.exe; echo "exit=$?"
```

Изход 0 = успех. `test_regressions` връща 1 при неуспешна проверка; тестовете с `assert()` спират с код 3 под Windows. Пускайте винаги с `timeout`, за да не виси зациклил тест.

Днес има пет теста и всички минават: `test_100x_speed_simulation`, `test_economy_balance`, `test_grid_battery_lamp`, `test_regressions`, `test_resource_cooldown`.

Кога какво се пуска:

- **Преди всеки commit:** компилация на играта + всички тестове.
- **При промяна в `UI/`:** и 8-секундно пробно пускане (раздел 4.3), защото тестовете не покриват интерфейса.
- **При нова логика в `Game/`:** нов тест или нова група в `test_regressions.cpp` (раздел 5 на [TESTING.md](TESTING.md)).

---

## 6. Стил на кода

Пълните конвенции са в раздел 16 на [ARCHITECTURE.md](ARCHITECTURE.md). Накратко, това е стилът, който се вижда в кода:

**Език и SFML 3.** C++17. Събития: `while (const auto event = window.pollEvent())` и `event->getIf<sf::Event::KeyPressed>()` (`UI/scr/UI_main.cpp:66`, `:81`). Правоъгълници: `sf::FloatRect({ x, y }, { w, h })` с полета `.position` и `.size`. Клавиши: `sf::Keyboard::Key::Space`. Ъгли: `sf::degrees(...)`.

**Папки и файлове.** `Game/includes` + `Game/scr` за енджина и `UI/includes` + `UI/scr` за интерфейса (`scr`, не `src`). Префикс `game_` в енджина и `UI_` в интерфейса. Голям клас се разделя по теми: `UI_map` е в `UI_map.cpp` (ред на рисуване), `UI_map_controls.cpp` (вход), `UI_map_overlays.cpp` (HUD, пауза, помощ, победа), `UI_map_popups.cpp`, `UI_map_particles.cpp`.

**Имена.** Класове в `PascalCase` в енджина (`GameEngine`, `PlayerEconomy`) и `UI_` + `camelCase` в интерфейса (`UI_map`, `UI_buildings`; изключение `UIBot`). Методи и полета в `camelCase`. Константи в `UPPER_SNAKE_CASE` в `namespace Balance`. Изброявания: `enum class` със стойности `UPPER_SNAKE_CASE`.

**Форматиране.** Отстъп 4 интервала, без табулации; отварящата скоба е на същия ред. Изключение е `UI/scr/UI_buildings.cpp` с 2 интервала. Пазете стила на файла, който редактирате, и не преформатирайте съседен код.

**Кодиране и край на реда.** Изходните файлове са UTF-8 без BOM. В Git редовете завършват с LF (`git ls-files --eol` показва `i/lf`); под Windows с `core.autocrlf=true` работното копие има CRLF и Git ги връща в LF при commit. Не commit-вайте файл с CRLF в индекса.

**Включвания.** От UI: `#include "../../Game/includes/game_main.h"`. В енджина: `#include "../includes/game_main.h"`. В тестовете: `#include "../Game/includes/game_main.h"` или `#include "game_main.h"`.

**Граница енджин / интерфейс.** Енджинът не рисува и не чете клавиатура, а интерфейсът не променя правилата. В `Game/` не добавяйте SFML типове освен `sf::Vector2f` и `sf::FloatRect`, иначе тестовете без SFML спират да се компилират. Действията на енджина връщат `bool` и съобщение на български, а интерфейсът само го показва:

```cpp
bool placeBuilding(int player, BuildingType type, sf::Vector2f pos, std::string& outMsg);   // game_main.h:193
```

**Числа за баланса.** Само в `Game/includes/game_balance.h` и `game_time.h` (`namespace Balance`). Интерфейсът ги чете оттам, както прозорецът за помощ: `Balance::VICTORY_SHARE`, `Balance::FINAL_DAY`, `Balance::MINE_SPEEDUP_MULT` (`UI/scr/UI_map_overlays.cpp:149-151`). Не пишете число от баланса втори път в UI или в тест.

**Играчи.** `int player` е 1 (Западен сектор) или 2 (Източен сектор). Обичайният израз е `auto& econ = (player == 1) ? p1 : p2;`.

**Случайност.** Ползвайте `randomInt()` (`game_random.h`) или `std::rand()`. И двете се засяват веднъж на мач от `GameEngine::init()` със семето от `EC_SEED` или от часовника (`game_main.cpp:52-58`), затова мачът може да се повтори. Не добавяйте нов генератор без семе.

**Дневник.** `std::cout << "[Компонент] ..."` за информация и `std::cerr` за грешки, на английски (например `[UI_map] Warning: Failed to load assets/grass.png`).

**Коментари.** На английски. Обясняват защо, а не какво (`// A held key must not re-trigger menu/pause/hotkey events`, `UI_main.cpp:10`).

**Предупреждения.** Кодът се компилира без предупреждения с `-Wall -Wextra`. Неизползван параметър: `(void)screenWidth;`.

---

## 7. Български текст в интерфейса

Всичко, което играчът вижда, е на български. Изключение са бутоните на главното меню, които са двуезични във вида „БЪЛГАРСКИ / ENGLISH“ (`ИГРА / PLAY`, `НАСТРОЙКИ / SETTINGS`, `UI_mainMenu.cpp:150-152`). Дневникът в конзолата, коментарите и commit съобщенията са на английски. Документацията е на български.

### Техника

- **Всеки низ с кирилица минава през `toUtf8()`** (`UI/includes/UI_types.h:14-21`):

  ```cpp
  sf::Text tHeader(font, toUtf8(pTag), 12);            // UI_buildings.cpp:101
  ```

  `sf::Text` приема `sf::String`, а `std::string` без `toUtf8()` се тълкува според локала и кирилицата излиза като безсмислени знаци. Само низове от ASCII знаци (числа, `"P1"`, `"ENERGY CRISIS"`) могат да се подадат директно.
- **Шрифтът е `assets/font.ttf`.** Ползвайте само знаци, които той съдържа, и проверявайте текста на екрана. Емоджита и редки символи не се използват в текста на играта.
- **SFML не пренася редове сам.** Нов ред се слага с `\n`, а дължината се съобразява с контейнера: изскачащата карта е 225×132 px с текст 10–12 pt (`UI_map_popups.cpp:59`), картата на сграда е около 218 px широка.
- **Центриране:** чрез `getLocalBounds()`, например `t.setPosition({ pos.x - tb.size.x / 2.0f, pos.y - 38.0f });` (`UI_resourceNodes.cpp:449`).
- **Без шрифт не рисувайте текст.** Всеки компонент проверява `fontLoaded` / `resourcesLoaded` преди `sf::Text`.

### Стил

- **Главни букви** за заглавия, бутони, значки и съобщения за грешка: `ГРЕШКА ПРИ СТРОЕЖ`, `НЕДОСТИГ НА ЗЛАТО! НУЖНО: 150 G`. Обяснителният текст под тях е с нормални букви: `Земята е свободна за строителство.`
- **Клавишите са в квадратни скоби с английското име на клавиша:** `[SPACE]`, `[ENTER]`, `[PgDn]`, `[ESC]`, `[F11]`. Подсказка за действие: `[КЛАВИШ]: Действие | [КЛАВИШ]: Действие`, например `[SPACE]: Постави в грида | [E]: Следваща | [X]: Отказ`.
- **Подсказките трябва да отговарят на режима.** В „Самостоятелна игра“ Играч 1 има и Enter, PgDn и Del, затова текстът се сглобява от `p1ActKeys`, `p1NextKeys`, `p1CancelKeys` (`UI_map_controls.cpp:558-561`). Commit `40fd272` поправи точно такива грешни подсказки в обучението.
- **Мерни единици:** `MW`, `MWh`, `G` (злато), `$` (пари), `%`, `px`, `s`. Между числото и `MW` има интервал: `+60 MW`.
- **Съкращения на ресурсите** при малко място: `Дър`/`Дърво`, `Жел`, `Мед`, `Сил`, `Въгл`, `Среб` (`UI_buildings.cpp:198-208`, `game_main.cpp:880-885`).
- **Имената на сградите не се пишат наново.** Вземат се от `Balance::<СГРАДА>.nameBg` или от `getBuildingCost(type).nameBg` (`Слънчев панел`, `Вятърна мелница`, `ВЕЦ / Хидро`, `Батерия`, `Осветителна лампа`).
- **Еднакви термини:** Играч 1 / Играч 2, Западен / Източен сектор, парцел, мина, ниво, град, територия, гратисен период. Проценти се закръглят с `std::lround` (`40fd272`).

---

## 8. Чеклист преди commit

- [ ] `git status` показва само файловете на задачата; добавени са поименно.
- [ ] Играта се компилира без грешки и без нови предупреждения.
- [ ] Всички `scratch/test_*.cpp` минават (пуснати с `timeout`).
- [ ] При промяна в `UI/`: 8-секундно пробно пускане без грешка в конзолата.
- [ ] Числата са в `Balance`, текстът е на български и минава през `toUtf8()`, подсказките за клавиши отговарят на режима.
- [ ] Commit съобщение във формата `тип(област): описание` + 1–3 реда обяснение.

---

## 9. Свързани документи

- [ARCHITECTURE.md](ARCHITECTURE.md): устройство на кода и конвенции
- [TESTING.md](TESTING.md): тестовете и как се добавя нов
- [HOWTO_ADD_BUILDING.md](HOWTO_ADD_BUILDING.md): нова сграда от край до край
- [HOWTO_ADD_UI_SCREEN.md](HOWTO_ADD_UI_SCREEN.md): нов екран или прозорец в интерфейса
- [README.md](README.md): списък на всички документи
