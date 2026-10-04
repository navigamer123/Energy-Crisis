> Чернова — ще бъде финализирана след приключване на разработката.

# Как се добавя нова сграда

Ръководство стъпка по стъпка: как се добавя нов вид сграда от край до край, от изброяването в енджина до бота и тестовете. Като пример добавяме **Биогазова централа** (`BIOGAS_PLANT`): 55 MW денонощно, независимо от времето, най-много 3 на играч. Числата са само пример, не са одобрен баланс.

Описано е състоянието на клон `Claude-code` към 03.10.2026 (commit `59fc68b`). Номерата на редове са към тази версия. Всички фрагменти „преди“ са копирани от кода. Всички фрагменти „след“ са проверени: примерът е приложен върху копие на кода, играта (WinLibs GCC 14.2 + SFML 3.1.0) се компилира без грешки и предупреждения и се пуска, всички тестове минават, заедно с теста от стъпка 11 (с няколко стойности на `EC_SEED`), а менюто и рисунката на сградата са проверени с картина, нарисувана от самия код на интерфейса.

## Съдържание

0. [Карта на промените](#0-карта-на-промените)
1. [Изброяването `BuildingType`](#1-изброяването-buildingtype)
2. [Числата за баланса](#2-числата-за-баланса)
3. [Цена: `getBuildingCost`](#3-цена-getbuildingcost)
4. [Правила за поставяне: `canPlaceBuilding`](#4-правила-за-поставяне-canplacebuilding)
5. [Енергиен модел: `updateBuildingsEnergy`](#5-енергиен-модел-updatebuildingsenergy)
6. [Редът на избора (E/Q) и клавишите](#6-редът-на-избора-eq-и-клавишите)
7. [Карта в строителното меню](#7-карта-в-строителното-меню)
8. [Изглед на картата (икона)](#8-изглед-на-картата-икона)
9. [Ботът](#9-ботът)
10. [Текстове и документация](#10-текстове-и-документация)
11. [Тестове](#11-тестове)
12. [Проверка и commit-и](#12-проверка-и-commit-и)
13. [Предстоящи промени в други клонове](#13-предстоящи-промени-в-други-клонове)

---

## 0. Карта на промените

| Стъпка | Файл | Какво |
| --- | --- | --- |
| 1 | `Game/includes/game_main.h` | нова стойност **в края** на `enum class BuildingType` |
| 2 | `Game/includes/game_balance.h` | `constexpr BuildingDef` с цената и мощността (+ числата на правилото) |
| 3 | `Game/scr/game_main.cpp` | `case` в `getBuildingCost` |
| 4 | `Game/scr/game_main.cpp` | правило в `canPlaceBuilding` (ако има) |
| 5 | `Game/scr/game_main.cpp` | производство в `updateBuildingsEnergy` |
| 6 | `Game/scr/game_main.cpp` | място в реда на избора с E/Q (клавиш с цифра: по желание) |
| 7 | `UI/scr/UI_buildings.cpp` | карта в строителното меню |
| 8 | `UI/scr/UI_resourceNodes.cpp` | рисунка на построената сграда |
| 9 | `UI/scr/UI_bot.cpp` | оценка за бота и същото правило |
| 10 | `UI/scr/UI_map_overlays.cpp`, `docs/*.md` | помощ и документация |
| 11 | `scratch/test_<име>.cpp` | нов тест |

Стъпки 1–6 са в енджина и се проверяват с тестовете без графика. Стъпки 7–9 са в интерфейса и изискват компилация на цялата игра и пробно пускане.

Какво **не** изисква промяна, защото работи за всеки тип:

- плащането на рецептата в `placeBuilding` и връщането на 50% в `removeBuilding` (четат `getBuildingCost`);
- проверката „имате ли всеки ресурс“ в `canPlaceBuilding` и в картата на менюто;
- нощното правило (строеж само в светлия кръг на захранена лампа) и колизията с други сгради;
- закупуването на парцел, ремонтът, мълниите (`breakBuildingAt` маха всяка сграда);
- енергийните линии към града (`UI_map::drawEnergyConduits` пропуска само `LAMP`, `UI_map.cpp:141`);
- изскачащите карти при избор с E/Q (за всичко освен `LAMP` и `DEMOLISH` показват „СТРОЕЖ“ и `+<MW> MW ток`);
- контурът при поставяне (`drawBuildingGhost` рисува еднаква рамка 48×42 за всички типове);
- изчисляването на липсващите ресурси в бота (чете `getBuildingCost`).

---

## 1. Изброяването `BuildingType`

`Game/includes/game_main.h:49-57`, преди:

```cpp
enum class BuildingType {
    NONE = 0,
    SOLAR_PANEL,
    WIND_TURBINE,
    HYDRO_PLANT,
    BATTERY,
    LAMP,
    DEMOLISH
};
```

**Новата стойност се добавя в края и съществуващите числа не се променят.** Числата на `BuildingType` се използват като такива на много места:

- `PlayerEconomy::selectedBuilding` и `lastPlacedBuilding` пазят избора като `int` (`0 = None, 1 = Solar, ... 6 = Demolish`, `game_main.h:106`), а `getSelectedBuilding` го превръща обратно със `static_cast<BuildingType>` (`game_main.cpp:672`);
- клавишите 1…6 на Играч 1 записват цифрата директно като тип (`selectedBuilding = k`, `UI_map_controls.cpp:626`), а 6 е „Премахване“;
- `scratch/test_economy_balance.cpp:172` проверява, че обратното превъртане стига до 6;
- в клон `wave-b-session` (в разработка) записът на играта пази `static_cast<int>(b.type)`, а в `wave-b-power` (в разработка) новите сгради са добавени след `DEMOLISH` със стойности 7–11, „за да остане DEMOLISH 6“.

Затова не вмъквайте нов тип преди `DEMOLISH`: това преномерира „Премахване“, чупи клавиша 6, теста и бъдещите записи. Мястото в менюто и в цикъла на избора не зависи от числото и се задава отделно (стъпки 6 и 7).

След:

```cpp
enum class BuildingType {
    NONE = 0,
    SOLAR_PANEL,
    WIND_TURBINE,
    HYDRO_PLANT,
    BATTERY,
    LAMP,
    DEMOLISH,
    BIOGAS_PLANT   // 7: appended, existing values never change
};
```

Допишете и коментара на `selectedBuilding` (`game_main.h:106`): `..., 6 = Demolish, 7 = Biogas`.

---

## 2. Числата за баланса

Всички числа на сградите са в `namespace Balance` в `Game/includes/game_balance.h`. Структурата (`game_balance.h:18-31`):

```cpp
struct BuildingDef {
    int woodCost;
    int ironCost;
    int copperCost;
    int coalCost;
    int siliconCost;
    int silverCost;
    int basePowerMW;       // Positive for generator, 0 for battery/lamp
    int batteryCapacityMWh;// Only for Battery
    float lightRadius;     // Only for Lamp
    int lampConsumptionMW; // Power consumption for lamp (10 MW)
    const char* nameBg;
    const char* nameEn;
};
```

Пример от кода, ВЕЦ (`game_balance.h:47-52`):

```cpp
// Hydro Plant: High-output river hydro generator, boosted during rain
constexpr BuildingDef HYDRO_PLANT = {
    15, 20, 12, 0, 6, 0,   // Wood, Iron, Copper, Coal, Silicon, Silver
    110, 0, 0.0f, 0,       // 110 MW base (matches user balance adjustment)
    "ВЕЦ / Хидро", "Hydro Plant"
};
```

Новата сграда, веднага след `STREET_LAMP` (преди `DEMOLISH_REFUND_FRACTION`), заедно с числото за правилото от стъпка 4:

```cpp
// Biogas Plant: burns farm and forest waste; steady output day and night, ignores weather
constexpr BuildingDef BIOGAS_PLANT = {
    14, 10, 6, 0, 0, 2,    // Wood, Iron, Copper, Coal, Silicon, Silver
    55, 0, 0.0f, 0,        // 55 MW base, constant
    "Биогазова централа", "Biogas Plant"
};
constexpr int BIOGAS_MAX_PER_PLAYER = 3; // each sector produces only so much farm waste
```

- Редът на полетата е **дърво, желязо, мед, въглища, силиций, сребро**, а не редът от менюто. Сверете с коментара.
- Еднаквото име `Balance::BIOGAS_PLANT` и `BuildingType::BIOGAS_PLANT` не е проблем: така е и за `SOLAR_PANEL`, `BATTERY` и другите.
- `nameBg` се показва навсякъде в играта (меню, изскачащи карти, съобщения на енджина), затова се пише веднъж тук.
- Числото на правилото (тук лимитът) също е в `Balance`, а не в `game_main.cpp`, в UI или в бота. Енджинът, ботът и тестът го четат от едно място.
- Сградата използва само съществуващите шест ресурса. Нов ресурс е отделна, много по-голяма задача (`ResourceType`, `PlayerEconomy`, станция на картата, HUD, бот).

---

## 3. Цена: `getBuildingCost`

`GameEngine::getBuildingCost` (`game_main.cpp:675-707`) превръща `Balance::...` в `BuildingCost`, който ползват енджинът, менюто, контурът при поставяне и ботът. Пример от кода:

```cpp
        case BuildingType::HYDRO_PLANT: {
            const auto& b = Balance::HYDRO_PLANT;
            int ore = b.ironCost + b.copperCost + b.siliconCost;
            return { BuildingType::HYDRO_PLANT, b.nameBg, b.nameEn, b.woodCost, b.ironCost, b.copperCost, b.coalCost, b.siliconCost, b.silverCost, ore, b.basePowerMW };
        }
```

Нов `case` преди `default:`:

```cpp
        case BuildingType::BIOGAS_PLANT: {
            const auto& b = Balance::BIOGAS_PLANT;
            int ore = b.ironCost + b.copperCost + b.silverCost;
            return { BuildingType::BIOGAS_PLANT, b.nameBg, b.nameEn, b.woodCost, b.ironCost, b.copperCost, b.coalCost, b.siliconCost, b.silverCost, ore, b.basePowerMW };
        }
```

`ore` е наследено поле (`int oreCost = 0; // Legacy backwards compatibility`, `game_main.h:69`). Вече не се ползва за проверка на ресурсите, но се попълва като сбор от минералите, за да е еднакво с другите типове.

Без този `case` типът попада в `default:` и връща нулева цена с `BuildingType::NONE`: сградата би била безплатна и без мощност.

---

## 4. Правила за поставяне: `canPlaceBuilding`

`GameEngine::canPlaceBuilding` (`game_main.cpp:850-939`) проверява в този ред:

1. `NONE` → „НЯМА ИЗБРАНА СГРАДА!“; `DEMOLISH` → има ли ваша сграда в клетката.
2. Прилепване към клетка: `pos = snapToBuildingGrid(player, pos);`.
3. Всеки ресурс от рецептата.
4. Нощем: само в светлия кръг на ваша захранена лампа (лампата е изключение).
5. Закупен парцел на същия играч.
6. ВЕЦ само на брега (`game_main.cpp:921-925`).
7. Свободна клетка (няма сграда по-близо от 16 px).

Новото правило влиза след правилото за ВЕЦ и преди проверката за свободна клетка. Пример от кода:

```cpp
    // Hydro plants need the river: only the plot column next to the city river counts as river bank
    if (type == BuildingType::HYDRO_PLANT && !isRiverBankSlot(player, pos)) {
        reason = "ВЕЦ СЕ СТРОИ САМО НА БРЕГА НА РЕКАТА!\nИзползвайте парцелите в колоната до града (до реката).";
        return false;
    }
```

Новото правило (лимит на играч):

```cpp
    // Biogas plants are limited per player (each sector produces only so much farm waste)
    if (type == BuildingType::BIOGAS_PLANT) {
        int owned = 0;
        for (const auto& b : buildings) {
            if (b.playerOwner == player && b.type == BuildingType::BIOGAS_PLANT) owned++;
        }
        if (owned >= Balance::BIOGAS_MAX_PER_PLAYER) {
            reason = "ЛИМИТ: НАЙ-МНОГО " + std::to_string(Balance::BIOGAS_MAX_PER_PLAYER) +
                     " БИОГАЗОВИ ЦЕНТРАЛИ!\nПремахнете стара централа, за да построите нова.";
            return false;
        }
    }
```

- Текстът на `reason` следва стила на останалите: първо ред с главни букви, после `\n` и съвет с нормални букви. Интерфейсът го показва без промяна в модалния прозорец „ГРЕШКА ПРИ СТРОЕЖ“.
- `placeBuilding` винаги вика `canPlaceBuilding`, затова правилото важи и за мишката, и за клавиатурата, и за бота.
- Правило, свързано с мястото, се пише като проверка на клетката. Ако е нужна нова геометрия, добавете публичен метод в `GameEngine`, както е `isRiverBankSlot` (`game_main.h:200-201`), за да ползва ботът същата проверка (стъпка 9), а не свое копие.

По желание: собствено съобщение при успех. `placeBuilding` има отделни текстове за `LAMP` и `BATTERY`, а за всичко друго пише `"ПОСТРОЕН " + cost.nameBg + "! (+" + MW + " MW)"` (`game_main.cpp:987-993`).

---

## 5. Енергиен модел: `updateBuildingsEnergy`

Всеки кадър енджинът минава през сградите на всеки играч в `processPlayerGrid` (`game_main.cpp:248-346`):

| Стъпка | Какво става |
| --- | --- |
| A | Сбор от производството `rawGen` на слънчеви, вятърни и водни централи; лампите и батериите се отделят в свои списъци. |
| B | Товар = 10 MW за всяка лампа + нуждата на града. |
| C | Ако производството не стига, батериите отдават (до 40 MW всяка и до заряда си). |
| D | Ако има излишък, батериите се зареждат (до 40 MW всяка и до капацитета си). |
| E | Лампите се захранват първи, по 10 MW. Незахранената лампа угасва (`lightRadius = 0`). |
| F | Останалото отива към града: `econ.energyMW`, а за дневния отчет `dailyDelivered += net * dt`. |

Нов генератор се добавя само в стъпка A. Пример от кода (`game_main.cpp:266-270`):

```cpp
            } else if (b.type == BuildingType::HYDRO_PLANT) {
                float out = cost.basePowerMW * WeatherSystem::getHydroMultiplier(w);
                b.currentOutputMW = out;
                rawGen += out;
            } else if (b.type == BuildingType::LAMP) {
```

Новият клон, преди `LAMP`:

```cpp
            } else if (b.type == BuildingType::BIOGAS_PLANT) {
                float out = static_cast<float>(cost.basePowerMW); // weather and daylight do not matter
                b.currentOutputMW = out;
                rawGen += out;
            } else if (b.type == BuildingType::LAMP) {
```

- `b.currentOutputMW` е за показване и за тестовете. В сметката влиза само `rawGen`.
- **Генератор, зависим от времето:** добавете статичен метод в `WeatherSystem` (`Game/includes/game_weather.h:40-47`, `Game/scr/game_weather.cpp:101-155`) по образеца на `getHydroMultiplier(WeatherType w)`, с по един коефициент за всеки от шестте `WeatherType`. Часът е `hour24`, сезонът `currentSeason`.
- **Консуматор** (като лампата): трябва промяна в стъпки B и E, иначе сградата няма да тегли ток. Изключете го и от енергийните линии в `UI_map::drawEnergyConduits` (`UI_map.cpp:141`).
- **Склад** (като батерията): включете го в списъка `playerBatteries`. `placeBuilding` слага `maxCapacity` от `Balance::BATTERY.batteryCapacityMWh` на всяка сграда (`game_main.cpp:983`), затова друг капацитет иска промяна и там.
- Енджинът разделя големите стъпки на подстъпки от 0,25 s (`MAX_SIM_STEP_SEC`, `game_main.cpp:12`) и никога не прескача 06:00. Формулата трябва да дава верен резултат за всяко `dt`; не пазете „последен кадър“ в статични променливи.

---

## 6. Редът на избора (E/Q) и клавишите

„Следваща / предишна сграда“ (E/Q за Играч 1, PgDn/PgUp за Играч 2) минава през `cycleBuildingSelection` и `cycleBuildingSelectionPrev` (`game_main.cpp:641-663`). Днес те просто броят от 1 до 6:

```cpp
void GameEngine::cycleBuildingSelection(int player) {
    auto& econ = (player == 1) ? p1 : p2;
    if (econ.selectedBuilding == 0) {
        int last = econ.lastPlacedBuilding;
        econ.selectedBuilding = (last >= 1 && last <= 6) ? last : 1;
    } else if (econ.selectedBuilding >= 6) {
        econ.selectedBuilding = 1;
    } else {
        econ.selectedBuilding++;
    }
}
```

Стойност 7 никога не се достига с E/Q, а последно построената сграда 7 не се помни (`last <= 6`). Поправката е изричен ред на избора, независим от числата. Сложете го точно преди двете функции:

```cpp
namespace {
// Order of "next / previous building" (E/Q, PgDn/PgUp): every building, then Demolish.
// BuildingType values never change (hotkeys 1..6 and tests use them), so a new type is appended
// to the enum and gets its place in the cycle here.
constexpr BuildingType kSelectionOrder[] = {
    BuildingType::SOLAR_PANEL, BuildingType::WIND_TURBINE, BuildingType::HYDRO_PLANT,
    BuildingType::BATTERY,     BuildingType::LAMP,         BuildingType::BIOGAS_PLANT,
    BuildingType::DEMOLISH
};
constexpr int kSelectionCount = static_cast<int>(sizeof(kSelectionOrder) / sizeof(kSelectionOrder[0]));

// Position of a building in kSelectionOrder, or -1 when it is not selectable
int selectionIndex(int selected) {
    for (int i = 0; i < kSelectionCount; ++i) {
        if (static_cast<int>(kSelectionOrder[i]) == selected) return i;
    }
    return -1;
}
} // namespace

void GameEngine::cycleBuildingSelection(int player) {
    auto& econ = (player == 1) ? p1 : p2;
    if (econ.selectedBuilding == 0) {
        int last = econ.lastPlacedBuilding;
        econ.selectedBuilding = (selectionIndex(last) >= 0) ? last : static_cast<int>(kSelectionOrder[0]);
    } else {
        int i = selectionIndex(econ.selectedBuilding); // -1 (unknown) restarts at the first entry
        econ.selectedBuilding = static_cast<int>(kSelectionOrder[(i + 1) % kSelectionCount]);
    }
}

void GameEngine::cycleBuildingSelectionPrev(int player) {
    auto& econ = (player == 1) ? p1 : p2;
    if (econ.selectedBuilding == 0) {
        int last = econ.lastPlacedBuilding;
        econ.selectedBuilding = (selectionIndex(last) >= 0) ? last : static_cast<int>(BuildingType::DEMOLISH);
    } else {
        int i = std::max(0, selectionIndex(econ.selectedBuilding));
        econ.selectedBuilding = static_cast<int>(kSelectionOrder[(i + kSelectionCount - 1) % kSelectionCount]);
    }
}
```

За шестте стари стойности поведението е същото: без избор E избира последно построената (в началото Слънчев панел), Q без избор избира „Премахване“, а след „Премахване“ идва Слънчев панел. Затова `test_economy_balance` и `test_100x_speed_simulation` минават без промяна. Следващата нова сграда изисква само още един ред в `kSelectionOrder`.

Ботът не минава през тези функции: той сам записва избраната сграда в `selectedBuilding` (`UI_map_controls.cpp:471`).

**Клавиш с цифра (по желание).** Клавишите 1…6 (`UI_map_controls.cpp:621-634`) записват цифрата като тип, затова стойност 7 би получила клавиш 7 с три промени: цикълът по клавишите до 7, цикълът в `primeInputEdges` (`UI_map_controls.cpp:58`) до 7 и масивът `bool p1PrevNum[7]` (`UI/includes/UI_map.h:135`, индекс = тип) с размер 8. Не го правете, ако клонът `wave-b-power` вече е слят: там 7 е АЕЦ (раздел 13).

---

## 7. Карта в строителното меню

Менюто за всеки играч е `UI_buildings` (`UI/scr/UI_buildings.cpp`). Списъкът с карти се пълни в `setPlayer` (редове 24-61). Всяка карта пази своя `BuildingType`, затова редът на картите не зависи от числата и трябва да съвпада с `kSelectionOrder`. Пример от кода:

```cpp
  const auto& hp = Balance::HYDRO_PLANT;
  buildings.push_back({BuildingType::HYDRO_PLANT, hp.nameEn, hp.nameBg,
                       hp.woodCost, hp.ironCost, hp.copperCost, hp.coalCost,
                       hp.siliconCost, hp.silverCost,
                       hp.ironCost + hp.copperCost + hp.siliconCost,
                       hp.basePowerMW, 0, {}});
```

Новата карта, преди картата на `DEMOLISH` (ред 60):

```cpp
  const auto& bg = Balance::BIOGAS_PLANT;
  buildings.push_back({BuildingType::BIOGAS_PLANT, bg.nameEn, bg.nameBg,
                       bg.woodCost, bg.ironCost, bg.copperCost, bg.coalCost,
                       bg.siliconCost, bg.silverCost,
                       bg.ironCost + bg.copperCost + bg.silverCost,
                       bg.basePowerMW, 0, {}});
```

Какво прави менюто само:

- **Кликът** се връща от `handleClick` като `BuildingType` на картата, а `UI_map::handleEvent` го записва в `selectedBuilding` (`UI_map_controls.cpp:981-991`).
- **„Имате ли ресурсите“** се проверява за всеки ресурс поотделно; картата става червена с „НЕДОСТИГ НА РЕСУРСИ“.
- **Значката вдясно** показва `+<MW> MW` за всеки генератор. Само `DEMOLISH`, `LAMP` и `BATTERY` имат свой текст (`UI_buildings.cpp:175-186`). Нов склад или консуматор иска нов клон там.
- **Редът с цената** включва само ненулевите ресурси (`UI_buildings.cpp:198-208`).

### Височината на картите

Височината на всяка карта е част от панела: `spacing = availH / брой_карти` и `itemH = spacing * 0.916f` (`UI_buildings.cpp:70`, `:120`). Панелът е 230×395 px (`UI_map.cpp:12-13`). Съдържанието на картата заема 51 px: име при `y + 3`, цена при `y + 19` и бутон с височина 16 px при `y + 35` (`UI_buildings.cpp:215`, `:220`).

| Брой карти | Разстояние | Височина на картата | Бутонът свършва при |
| --- | --- | --- | --- |
| 6 (днес) | 60,0 px | 54,9 px | 51 px: вътре в картата |
| 7 | 51,4 px | 47,1 px | 51 px: излиза ~4 px под фона на картата |

Със седем карти бутонът излиза под фона на своята карта (без да застъпва следващата, която започва при 51,4 px). Това е проверено с картина на двете менюта, нарисувани от `UI_buildings::draw`. Възможни поправки:

- **По-сбита карта (препоръчително):** само в `UI_buildings::draw` преместете цената на `y + 17` (ред 215), бутона на `y + 31` (ред 220) и текста на бутона на `y + 32` (ред 253 и следващия). Тогава съдържанието свършва при 47 px и бутоните остават в картите (проверено по същия начин).
- **По-висок панел:** 7 карти по 51 px искат панел от около 430 px. Панелът започва при y = 115 и би свършил при 545, но изскачащата карта на играча се рисува при y = 520 (`UI_map_popups.cpp:116`) върху долната част на менюто. Ако изберете това, преместете и изскачащите карти.

Името (11 pt, вляво) и значката (вдясно) са на един ред с около 206 px място. „Биогазова централа“ и „+55 MW“ се събират, но още днес „Премахване / Разруши“ застъпва „[-50% Връща]“. Пазете името кратко или съкратете значката.

Проверете с пробно пускане, че текстът на всяка карта стои в рамката ѝ.

---

## 8. Изглед на картата (икона)

В репото няма файлове с икони: в `assets/` са само `font.ttf` и `grass.png`. Всяка сграда се рисува с фигури на SFML в `UI_resourceNodes::drawPlacedBuildings` (`UI/scr/UI_resourceNodes.cpp:279-416`): една верига `if / else if` по `b.type`. Центърът е `b.position` (центърът на клетката), рисунката е около 26×22 px (клетката е 35×31,7 px), а контурът е в цвета на играча (`ownerColor`: циан за Играч 1, розово за Играч 2). Пример от кода, ВЕЦ:

```cpp
        } else if (b.type == BuildingType::HYDRO_PLANT) {
            sf::RectangleShape station({ 26.0f, 22.0f });
            station.setOrigin({ 13.0f, 11.0f });
            station.setPosition(b.position);
            station.setFillColor(sf::Color(30, 48, 70));
            station.setOutlineThickness(1.2f);
            station.setOutlineColor(ownerColor);
            window.draw(station);

            sf::CircleShape wheel(6.0f);
            wheel.setOrigin({ 6.0f, 6.0f });
            wheel.setPosition(b.position);
            wheel.setFillColor(sf::Color(45, 120, 180, 180));
            wheel.setOutlineThickness(1.0f);
            wheel.setOutlineColor(sf::Color(100, 220, 255));
            window.draw(wheel);
        } else if (b.type == BuildingType::BATTERY) {
```

Новият клон, преди `} else if (b.type == BuildingType::LAMP) {` (ред 389): купол на ферментатора, резервоар с контур в цвета на играча и факла с трептящ пламък, който се движи с `b.animTimer`:

```cpp
        } else if (b.type == BuildingType::BIOGAS_PLANT) {
            sf::CircleShape dome(9.0f); // digester dome, lower half hidden by the tank
            dome.setOrigin({ 9.0f, 9.0f });
            dome.setPosition({ b.position.x - 3.0f, b.position.y + 1.0f });
            dome.setFillColor(sf::Color(90, 150, 70));
            dome.setOutlineThickness(1.0f);
            dome.setOutlineColor(sf::Color(150, 210, 120));
            window.draw(dome);

            sf::RectangleShape tank({ 26.0f, 10.0f });
            tank.setOrigin({ 13.0f, 0.0f });
            tank.setPosition({ b.position.x, b.position.y + 1.0f });
            tank.setFillColor(sf::Color(55, 60, 45));
            tank.setOutlineThickness(1.2f);
            tank.setOutlineColor(ownerColor);
            window.draw(tank);

            sf::RectangleShape stack({ 2.0f, 12.0f }); // gas flare stack
            stack.setOrigin({ 1.0f, 12.0f });
            stack.setPosition({ b.position.x + 9.0f, b.position.y + 1.0f });
            stack.setFillColor(sf::Color(170, 185, 205));
            window.draw(stack);

            float flameR = 2.5f * (0.75f + 0.25f * std::sin(b.animTimer * 12.0f)); // flickers
            sf::CircleShape flame(flameR);
            flame.setOrigin({ flameR, flameR });
            flame.setPosition({ b.position.x + 9.0f, b.position.y - 13.0f });
            flame.setFillColor(sf::Color(255, 170, 40));
            window.draw(flame);
        } else if (b.type == BuildingType::LAMP) {
```

- `b.animTimer` расте с игровото време (`updateBuildingsEnergy`, `game_main.cpp:239-241`), затова анимацията спира на пауза.
- Тип без клон в тази верига просто не се рисува: сградата съществува и произвежда, но е невидима.
- Контурът при поставяне (`drawBuildingGhost`, ред 418) е общ. Свой текст под контура има само за `LAMP` и `BATTERY` (ред 459-460).
- **Икона от файл** (ако все пак искате): заредете `sf::Texture` в конструктора на компонента от `assets/...`, както `UI_map` зарежда `assets/grass.png` (`UI_map.cpp:21-25`), и рисувайте със `sf::Sprite sprite(texture);` (`UI_map.cpp:88`). Скриптовете за компилация копират цялата папка `assets/` до играта. Без файла играта трябва да продължи с рисунката от фигури, както `drawGrassBackground` минава на зелен правоъгълник (`UI_map.cpp:92-97`).

---

## 9. Ботът

Ботът (`UIBot`, `UI/scr/UI_bot.cpp`) играе за Играч 2. В `planNextAction` (раздел 4, редове 190-249) всеки вид сграда получава оценка, списъкът се сортира и се взема първият тип, за който има подходяща свободна клетка. Без промяна ботът никога не строи новата сграда. Пример от кода:

```cpp
    // Hydro Plant: 110 MW base, 24/7 continuous output, rain boost
    float hydroScore = (difficulty == BotDifficulty::HARD ? 135.0f : (difficulty == BotDifficulty::MEDIUM ? 105.0f : 70.0f));
    if (weather == WeatherType::RAINY) hydroScore += 45.0f;
    candidateList.push_back({ BuildingType::HYDRO_PLANT, hydroScore });
```

**Ботът трябва да спазва същото правило като енджина.** Иначе избира сграда, която `canPlaceBuilding` отказва, планира отново и пак стига до същия избор (оценките и свободните клетки се обхождат винаги в един и същ ред), така че може да се върти на място. Оценките -500 и по-ниски се пропускат (`if (cand.score <= -500.0f) continue;`): така ботът вече забранява соларен панел нощем (`-999.0f`) и втора лампа.

Ботът вече брои своите лампи и батерии (редове 193-200). Добавете брояч за новата сграда в същия цикъл:

```cpp
    int lampCount = 0;
    int batteryCount = 0;
    int biogasCount = 0;
    for (const auto& b : engine.getBuildings()) {
        if (b.playerOwner == 2) {
            if (b.type == BuildingType::LAMP) lampCount++;
            if (b.type == BuildingType::BATTERY) batteryCount++;
            if (b.type == BuildingType::BIOGAS_PLANT) biogasCount++;
        }
    }
```

и нов кандидат след вятърната турбина (ред 218), с лимита от `Balance`:

```cpp
    // Biogas Plant: 55 MW base, 24/7, ignores weather; limited per player (same rule as canPlaceBuilding)
    float biogasScore = (difficulty == BotDifficulty::HARD ? 115.0f : (difficulty == BotDifficulty::MEDIUM ? 90.0f : 60.0f));
    if (biogasCount >= Balance::BIOGAS_MAX_PER_PLAYER) biogasScore = -999.0f;
    candidateList.push_back({ BuildingType::BIOGAS_PLANT, biogasScore });
```

Правило, свързано с мястото, се спазва при избора на клетка. Днес там е проверката за брега на ВЕЦ (ред 273); нова проверка се добавя към същото условие:

```cpp
                if (cand.type != BuildingType::HYDRO_PLANT || engine.isRiverBankSlot(2, slot)) {
```

Без промяна остават:

- **Нощем** се ползват само осветени свободни клетки (`illuminatedFreeSlots`).
- **Липсващите ресурси** се изчисляват от `engine.getBuildingCost(plannedBuilding)` (редове 326-333), а ботът отива да добива първия липсващ ресурс в реда дърво, желязо, мед, въглища, силиций, сребро.
- **Ботът натиска същото действие като човека** (`executeP2Action` → `placeBuilding`), затова всички правила на енджина важат и за него. При отказ той не получава модален прозорец, а само изскачаща карта (`triggerPlayerModal`, `UI_map_popups.cpp:122-125`).

Подробно за бота: [AI.md](AI.md).

---

## 10. Текстове и документация

- **Помощ в играта** (`UI_map::drawHelpOverlay`, `UI/scr/UI_map_overlays.cpp:81-184`): ако сградата има особено правило, добавете ред в раздел „2. СЕЗОНЕН ДЕН/НОЩ ЦИКЪЛ И СЛЪНЧЕВ ГРАФИК“, където са описани лампите и батериите, на български и с числата от `Balance`. Днес помощта не споменава правилото за ВЕЦ.
- **Обучението** (`UI/scr/UI_tutorial.cpp`) учи само на слънчев панел; не се променя.
- **`docs/GAMEPLAY.md`**, раздел 5: ред в таблицата със сградите, ред в таблицата с върнатите 50% (закръглени надолу, както в `removeBuilding`) и правилото в „Правила за строеж“.
- **`docs/CONTROLS.md`**, раздел 4: таблицата „Ред на сградите“ (редът от `kSelectionOrder`) и, ако има, новият клавиш в раздел 2.
- **`docs/AI.md`**: оценката на новия кандидат.

Правилата за текста са в раздел 7 на [CONTRIBUTING.md](CONTRIBUTING.md).

---

## 11. Тестове

Новата сграда получава собствен тест `scratch/test_biogas.cpp`. `make test` го намира автоматично. Тестът проверява цената, лимита, мощността денем и нощем, реда на избора и връщането на 50%:

```cpp
// Biogas plant: cost from Balance, per-player limit, constant output, selection order, refund
#include <iostream>
#include <string>
#include "game_main.h"

static int g_failures = 0;
static void check(bool ok, const std::string& what) {
    std::cout << (ok ? "  [OK]   " : "  [FAIL] ") << what << "\n";
    if (!ok) ++g_failures;
}

static void payFor(PlayerEconomy& p, const BuildingCost& c) {
    p.wood = c.woodCost; p.iron = c.ironCost; p.copper = c.copperCost;
    p.coal = c.coalCost; p.silicon = c.siliconCost; p.silver = c.silverCost;
}

int main() {
    GameEngine engine;
    engine.init(1600.0f, 900.0f);                       // 08:00 on day 1: daylight, grace period

    const BuildingCost cost = engine.getBuildingCost(BuildingType::BIOGAS_PLANT);
    check(cost.type == BuildingType::BIOGAS_PLANT, "getBuildingCost knows the type");
    check(cost.basePowerMW == Balance::BIOGAS_PLANT.basePowerMW, "power comes from Balance");

    // 1. Up to the limit on the start plot (grid row 0), full recipe paid every time
    auto& p1 = engine.getPlayerEconomyMut(1);
    std::string msg;
    for (int i = 0; i < Balance::BIOGAS_MAX_PER_PLAYER; ++i) {
        payFor(p1, cost);
        bool placed = engine.placeBuilding(1, BuildingType::BIOGAS_PLANT, engine.getGridSlot(1, i, 0), msg);
        check(placed, "plant " + std::to_string(i + 1) + " placed: " + msg);
    }
    check(p1.wood == 0 && p1.iron == 0 && p1.copper == 0 && p1.silver == 0, "full recipe paid");

    // 2. One more is refused although resources and a free slot are there
    payFor(p1, cost);
    std::string reason;
    bool allowed = engine.canPlaceBuilding(1, BuildingType::BIOGAS_PLANT, engine.getGridSlot(1, 0, 1), reason);
    check(!allowed, "limit reached: " + reason);

    // 3. Constant output: the same MW at noon and at midnight, all of it reaches the city
    while (engine.getHour24() < 12.0f) engine.update(0.25f);
    const float noonMW = engine.getBuildings()[0].currentOutputMW;
    while (engine.getHour24() >= 12.0f) engine.update(0.25f); // runs past midnight
    const float nightMW = engine.getBuildings()[0].currentOutputMW;
    check(!engine.isDaylight(), "it is night now");
    check(noonMW == cost.basePowerMW && nightMW == cost.basePowerMW, "same MW at noon and at night");
    check(engine.getPlayerEconomy(1).energyMW == Balance::BIOGAS_MAX_PER_PLAYER * cost.basePowerMW,
          "the whole output goes to the city");

    // 4. Selection order: last placed -> Demolish -> Solar, and Q from Demolish back to biogas
    engine.clearBuildingSelection(1);
    engine.cycleBuildingSelection(1);
    check(engine.getSelectedBuilding(1) == BuildingType::BIOGAS_PLANT, "E reselects the last placed building");
    engine.cycleBuildingSelection(1);
    check(engine.getSelectedBuilding(1) == BuildingType::DEMOLISH, "Demolish is still the last tool");
    engine.cycleBuildingSelection(1);
    check(engine.getSelectedBuilding(1) == BuildingType::SOLAR_PANEL, "the cycle wraps to Solar");
    engine.cycleBuildingSelectionPrev(1);
    engine.cycleBuildingSelectionPrev(1);
    check(engine.getSelectedBuilding(1) == BuildingType::BIOGAS_PLANT, "Q steps back from Demolish to biogas");

    // 5. Demolish refunds 50% (rounded down) of the recipe
    PlayerEconomy before = engine.getPlayerEconomy(1);
    bool removed = engine.removeBuilding(1, engine.getGridSlot(1, 0, 0), msg);
    check(removed, "demolished: " + msg);
    const PlayerEconomy& after = engine.getPlayerEconomy(1);
    check(after.wood - before.wood == static_cast<int>(cost.woodCost * Balance::DEMOLISH_REFUND_FRACTION),
          "50% wood refund");

    std::cout << (g_failures == 0 ? "BIOGAS TEST PASSED\n" : "BIOGAS TEST FAILED\n");
    return (g_failures == 0) ? 0 : 1;
}
```

Бележки:

- Сградите в теста се поставят **през деня** (мачът започва в 08:00). Нощем строежът изисква осветена клетка.
- Резултатът от действието се пази в променлива преди `check(...)`. Редът, в който C++ изчислява аргументите на функция, не е определен: в `check(engine.placeBuilding(..., msg), "..." + msg)` GCC първо сглобява текста и показва съобщението от предишното действие.
- Числата се четат от `Balance` и от `getBuildingCost`, никога не са вписани на ръка. Така тестът не остарява при промяна на баланса.
- Мълниите са в интерфейса (`UI_map_particles.cpp`), затова в тест без графика сградите не се чупят.
- Пуснете теста и с няколко стойности на `EC_SEED`: резултатът не трябва да зависи от времето (метеорологията).

---

## 12. Проверка и commit-и

```bash
# Енджинът и тестовете (без SFML)
make test
make test EC_SEED=1 && make test EC_SEED=42

# Цялата игра
make            # или на машината на екипа: build_game.sh (CONTRIBUTING.md, раздел 4.3)
```

Ръчна проверка в играта:

1. E/Q минават през сградите в реда от `kSelectionOrder`, а „Премахване“ е последно; клавишите 1…6 работят както преди.
2. Картата в менюто показва името, `+55 MW` и цената; текстът е в рамката.
3. Строят се три централи; четвъртата се отказва с ясно съобщение.
4. Построената сграда се вижда на картата, а енергийната линия води до града.
5. Срещу бот на ТРУДНО ботът строи новата сграда, спира на лимита и не се върти на място.

Разделете работата на малки commit-и, всеки компилиращ се и с минаващи тестове, например:

```text
feat(engine): selection cycle follows an explicit order table
feat(engine): add biogas plant type, balance numbers, cost and per-player limit
feat(engine): biogas plant delivers constant output
test: cover the biogas plant
feat(ui): biogas card in the build menu and its map drawing
feat(ai): bot builds biogas plants up to the limit
docs: biogas plant in GAMEPLAY, CONTROLS and AI
```

Таблицата `kSelectionOrder` може да влезе първа: с шестте стари типа тя не променя поведението и тестовете минават.

---

## 13. Предстоящи промени в други клонове

Следните клонове още не са слети в `Claude-code`, но засягат стъпките по-горе (в разработка). Проверка какво е променено в даден клон: `git diff --stat HEAD...<клон>`.

- **`wave-b-power`** (`075745e`, `d083369`): добавя пет сгради **след** `DEMOLISH` (`NUCLEAR` = 7, `GEOTHERMAL` = 8, `MEGA_FUSION`, `MEGA_SPACE_SOLAR`, `MEGA_PUMPED_HYDRO` = 11) и таблица за реда на избора `kSelectionCycle[] = { 1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 6 }` в `game_main.cpp`. Цената, мощността и правилата на тези сгради са в `getAdvancedBuildingCost`, `advancedOutputMW`, `checkAdvancedPlacement` (`Game/scr/game_power_buildings.cpp`). След сливането новата сграда получава следващата свободна стойност (12), влиза в `kSelectionCycle` вместо в `kSelectionOrder` от стъпка 6 и не взема клавиш 7.
- **`wave-b-session`** (`9082fd0`): записът на играта пази `static_cast<int>(b.type)`, а при зареждане приема само сгради от `SOLAR_PANEL` до `LAMP` (`Game/scr/game_save.cpp:348`) и избор до `DEMOLISH`. Нова сграда трябва да се добави и в тези проверки, иначе запис с нея не се зарежда.
- **`wave-a-engine`** (`6282f80`): премахва дублиращото копие `PlayerData` (полето `econ.data`). Тогава в `placeBuilding` и `removeBuilding` няма редове `econ.data.wood = econ.wood;`. Същият клон добавя опашка от събития (`17c6095`) и фиксирана стъпка 60 Hz (`9941556`).
- **`wave-a-ui`** (`e7c9c9a`): добавя `UI/includes/UI_text.h` (`ui::makeText`, `ui::drawText`) и проверка на подредбата `--lint`. Тя открива текст, който излиза от рамката си, включително текста в картите от стъпка 7.
