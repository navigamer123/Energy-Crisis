# Компилиране на Energy Crisis

Това ръководство описва как да компилирате играта и тестовете под Windows и Linux, с **CMake** (препоръчително) или с **Makefile**.

- [Изисквания](#изисквания)
- [Windows (WinLibs GCC 14.2 + SFML 3.1.0)](#windows-winlibs-gcc-142--sfml-310)
- [Linux](#linux)
- [CMake: опции и цели](#cmake-опции-и-цели)
- [Makefile](#makefile)
- [Тестове](#тестове)
- [Екранни снимки и проверка на текста (`--shot`)](#екранни-снимки-и-проверка-на-текста---shot)
- [Непрекъсната интеграция (GitHub Actions)](#непрекъсната-интеграция-github-actions)
- [Чести проблеми](#чести-проблеми)

---

## Изисквания

| Какво | Версия | Бележка |
| :--- | :--- | :--- |
| Компилатор с C++17 | съвременен GCC или Clang (препоръчително GCC 11+) | Тествано с **GCC 14.2** (WinLibs, Windows); CI компилира и с GCC на Ubuntu 24.04 |
| SFML | **3.0 или по-нова** | Тествано със **SFML 3.1.0**. SFML 2.x **не работи** (кодът ползва API-то на SFML 3) |
| CMake | 3.22 или по-нова (препоръчително 3.28+) | Или GNU make за `Makefile` |
| Git | всяка версия | Нужен само когато CMake сваля SFML (`EC_FETCH_SFML=ON`) |

Логиката на играта (`Game/`) не зависи от SFML за рисуване, затова **тестовете не изискват SFML** и се компилират с всеки C++17 компилатор (дори със стария MinGW g++ 6.3).

---

## Windows (WinLibs GCC 14.2 + SFML 3.1.0)

Готовите библиотеки на SFML работят **само с точно същия компилатор**, с който са компилирани. Ползвайте тези две версии заедно:

| Пакет | Точна версия | Файл |
| :--- | :--- | :--- |
| WinLibs (GCC + MinGW-w64) | GCC **14.2.0** (POSIX threads) + MinGW-w64 **12.0.0 UCRT**, release **r2** | `winlibs-x86_64-posix-seh-gcc-14.2.0-mingw-w64ucrt-12.0.0-r2.7z` |
| SFML | **3.1.0**, вариантът за GCC 14.2.0 MinGW, 64-bit | `SFML-3.1.0-windows-gcc-14.2.0-mingw-64-bit.zip` |

- WinLibs: <https://github.com/brechtsanders/winlibs_mingw/releases/tag/14.2.0posix-19.1.1-12.0.0-ucrt-r2> (същият компилатор ползва и GitHub Actions).
- SFML: <https://www.sfml-dev.org/download/sfml/3.1.0/>

WinLibs съдържа и `cmake` (3.30.4), `ninja`, `mingw32-make` (4.4.1) и `gdb`, така че не е нужно да инсталирате нищо друго.

### Подготовка

1. Разархивирайте WinLibs, например в `C:\mingw64` (архивът съдържа папка `mingw64`).
2. Разархивирайте SFML, например в `C:\SFML-3.1.0`.
3. Добавете `C:\mingw64\bin` в променливата `PATH` (потърсете „Променливи на средата“ в менюто „Старт“) и отворете нов терминал.
4. Проверка: `g++ --version` трябва да покаже `14.2.0`.

### Компилиране с CMake (PowerShell или cmd)

```bat
cmake -S . -B build -G "MinGW Makefiles" -DSFML_DIR=C:/SFML-3.1.0/lib/cmake/SFML
cmake --build build --parallel
build\bin\energy_crisis.exe
```

В `build\bin` се появяват `energy_crisis.exe`, папката `assets\`, DLL файловете на SFML и на компилатора (`libstdc++-6.dll` и др.), така че играта тръгва и с двоен клик.

Без инсталиран SFML: заменете `-DSFML_DIR=...` с `-DEC_FETCH_SFML=ON` – CMake сваля SFML 3.1.0 от GitHub и го компилира заедно с играта (първия път отнема няколко минути).

### Компилиране с Makefile

```bash
mingw32-make SFML_DIR=C:/SFML-3.1.0        # -> test_game.exe + DLL файловете на SFML в главната папка
mingw32-make run SFML_DIR=C:/SFML-3.1.0    # компилира и стартира
```

Работи от Git Bash, MSYS2, cmd и PowerShell. `C:\mingw64\bin` трябва да е в `PATH`, за да се намерят `libstdc++-6.dll` и останалите DLL файлове на компилатора.

### Visual Studio / MSVC (непроверено)

`CMakeLists.txt` подава `/utf-8` (текстовете са на български), но тази комбинация не се проверява в CI:

```bat
cmake -S . -B build -DEC_FETCH_SFML=ON
cmake --build build --config Release
build\bin\Release\energy_crisis.exe
```

---

## Linux

Пакетите на Ubuntu 24.04 и Debian 12 съдържат само **SFML 2.x**, с който играта не се компилира. Най-лесно е CMake да свали и компилира SFML 3.1.0:

```bash
# Ubuntu / Debian: компилатор, CMake и библиотеките, от които SFML има нужда
sudo apt update
sudo apt install build-essential cmake git \
    libxrandr-dev libxcursor-dev libxi-dev libudev-dev \
    libfreetype-dev libharfbuzz-dev libflac-dev libvorbis-dev \
    libgl1-mesa-dev libegl1-mesa-dev

cmake -S . -B build -DEC_FETCH_SFML=ON
cmake --build build --parallel
./build/bin/energy_crisis
```

Ако дистрибуцията ви вече има SFML 3 (проверете версията на пакета, например в Arch Linux), инсталирайте го и пропуснете `-DEC_FETCH_SFML=ON`. CMake намира SFML 3 автоматично; ако е в нестандартна папка, подайте `-DCMAKE_PREFIX_PATH=/път/до/SFML`.

Със SFML 3, инсталиран в системата, работи и Makefile:

```bash
make            # -> ./test_game
make run
make SFML_DIR=$HOME/SFML-3.1.0    # SFML в собствена папка (пътят се записва в програмата)
```

macOS (непроверено): `brew install cmake sfml` и същите CMake команди.

---

## CMake: опции и цели

```bash
cmake -S . -B build [опции]
cmake --build build --parallel
```

| Опция | По подразбиране | Какво прави |
| :--- | :--- | :--- |
| `EC_BUILD_GAME` | `ON` | `OFF` компилира само логиката (`ec_engine`) и тестовете – без SFML |
| `EC_FETCH_SFML` | `OFF` | Ако не е намерен инсталиран SFML 3, сваля и компилира SFML (статично) с FetchContent |
| `EC_SFML_VERSION` | `3.1.0` | Коя версия (git tag) на SFML да се свали |
| `EC_BUILD_TESTS` | `ON` | Компилира `scratch/test_*.cpp` и ги регистрира в CTest |
| `EC_TEST_TIMEOUT` | `120` | Максимално време (секунди) за всеки тест |
| `CMAKE_BUILD_TYPE` | `Release` | `Debug` за дебъгване с gdb |
| `SFML_DIR` | – | Папката `lib/cmake/SFML` на инсталиран SFML 3 |

| Цел | Какво е |
| :--- | :--- |
| `energy_crisis` | Играта (`main.cpp` + `UI/scr/*.cpp`) в `build/bin/`, с `assets/` до нея |
| `ec_engine` | Статична библиотека с правилата на играта (`Game/scr/*.cpp`) |
| `test_*` | По една тестова програма за всеки `scratch/test_*.cpp`, в `build/tests/` |
| `run` | Компилира и стартира играта: `cmake --build build --target run` |
| `install` | Папка за споделяне (exe + assets + DLL): `cmake --install build --prefix dist` |

Нови `.cpp` файлове в `Game/scr/`, `UI/scr/` и нови `scratch/test_*.cpp` се добавят автоматично при следващото `cmake --build`. Папката `assets/` се копира наново при всяка промяна в нея.

---

## Makefile

| Команда | Какво прави |
| :--- | :--- |
| `make` | Компилира играта → `test_game` (`test_game.exe` под Windows) |
| `make run` | Компилира и стартира играта |
| `make test` | Компилира и пуска всички тестове (без SFML) |
| `make -k test` | Пуска всички тестове, дори някой да се провали |
| `make run-test_regressions` | Пуска само един тест |
| `make clean` | Изтрива всичко, създадено от Makefile |

Променливи: `SFML_DIR=<папка на SFML 3>`, `CXX=<компилатор>`, `EC_SEED=<число>`, `BUILD_DIR=<папка>` (по подразбиране `build/make`). Обектните файлове пазят зависимостите от заглавните файлове, така че промяна в `game_balance.h` прекомпилира само това, което го ползва. Под Windows използвайте `mingw32-make` вместо `make`.

---

## Тестове

Тестовете са в `scratch/test_*.cpp`. Те ползват само логиката на играта (`Game/`, библиотеката `ec_engine`) и не отварят прозорец. С Makefile и с `EC_BUILD_GAME=OFF` вместо SFML се ползва минимален заместител (`scratch/sfml_stub`), затова SFML не е нужен; при пълна CMake компилация се ползват истинските заглавни файлове на SFML.

С CMake:

```bash
cmake --build build --parallel
ctest --test-dir build --output-on-failure          # всички тестове
ctest --test-dir build -R regressions -V            # само един, с целия изход
```

Без SFML (например на училищен компютър):

```bash
cmake -S . -B build-engine -DEC_BUILD_GAME=OFF
cmake --build build-engine --parallel
ctest --test-dir build-engine --output-on-failure
```

С Makefile: `make test` (под Windows `mingw32-make test`). CMake и Makefile компилират едни и същи тестови програми.

**Повторяемо време:** при всеки мач играта и тестовете отпечатват семето на генератора (`RNG seed: N`). Задайте `EC_SEED=N`, за да получите същото време отново:

```bash
EC_SEED=42 ctest --test-dir build --output-on-failure    # bash
make test EC_SEED=42
```

```powershell
$env:EC_SEED = 42; ctest --test-dir build --output-on-failure    # PowerShell
```

---

## Екранни снимки и проверка на текста (`--shot`)

Играта може да се стартира в режим за автоматични екранни снимки: рисува дадена сцена, записва прозореца като PNG и излиза.

```bash
energy_crisis --shot <файл.png> [--scene menu|game|night|winter|storm|victory] [--frames N] [--seed S] [--lint]
```

| Параметър | Значение |
| :--- | :--- |
| `--shot <файл.png>` | Къде да се запише снимката |
| `--scene` | `menu` (главно меню), `game` (мач срещу бота), `night`, `winter`, `storm`, `victory` |
| `--frames N` | Колко кадъра да се нарисуват преди снимката (по подразбиране 90) |
| `--seed S` | Семе на генератора – една и съща сцена при всяко пускане |
| `--lint` | Проверява всички надписи: застъпване, излизане извън рамката или извън екрана 1600×900, празни низове и липсващи букви |

С `--lint` всеки проблем се отпечатва на отделен ред, а кодът на излизане е броят на проблемите (най-много 255), така че `0` означава „без проблеми“. Пример:

```bat
build\bin\energy_crisis.exe --shot menu.png --scene menu --lint
build\bin\energy_crisis.exe --shot storm.png --scene storm --seed 42 --frames 120
```

---

## Непрекъсната интеграция (GitHub Actions)

`.github/workflows/ci.yml` се изпълнява при всеки push и pull request (промени само в `.md` файлове и в `docs/` не го пускат):

| Задача | Какво прави |
| :--- | :--- |
| Engine tests | Ubuntu, без SFML: CMake с `EC_BUILD_GAME=OFF` + `ctest`, после `make -k test` |
| Linux | Ubuntu 24.04, GCC, SFML 3.1.0 от изходен код: компилира играта, пуска `ctest`, качва архив `energy-crisis-linux` |
| Windows | WinLibs GCC 14.2.0 + SFML 3.1.0 от изходен код: компилира играта, пуска `ctest`, качва архив `energy-crisis-windows` (exe + assets + DLL) |

Компилираният SFML и WinLibs се пазят в кеша на Actions, така че само първото изпълнение компилира SFML. Тестовете се пускат с фиксирано `EC_SEED`. Готовите архиви се свалят от страницата на съответното изпълнение (Actions → изпълнение → Artifacts).

---

## Чести проблеми

| Симптом | Причина и решение |
| :--- | :--- |
| `SFML 3 was not found` при `cmake` | Подайте `-DSFML_DIR=<SFML>/lib/cmake/SFML` или `-DEC_FETCH_SFML=ON` |
| `SFML/Graphics.hpp: No such file or directory` при `make` | SFML не е в системата: `make SFML_DIR=<папка на SFML 3>` |
| Грешки за `pollEvent`, `std::optional`, `getIf` или `FloatRect::position` | Намерен е SFML 2.x – нужен е SFML 3 |
| `undefined reference to sf::...` при свързване | SFML е компилиран с друг компилатор. Под Windows ползвайте точно WinLibs GCC 14.2.0 UCRT + SFML 3.1.0 за GCC 14.2.0, или `-DEC_FETCH_SFML=ON` |
| „Липсва `sfml-graphics-3.dll`“ или „`libstdc++-6.dll`“ при стартиране | Стартирайте от `build\bin` (там са копирани всички DLL), или добавете `C:\mingw64\bin` и `C:\SFML-3.1.0\bin` в `PATH` |
| Липсват надписите / `assets/font.ttf was not found` | Папката `assets/` трябва да е до exe файла или в текущата папка. CMake я копира в `build/bin` автоматично |
| `make` под Windows казва „command not found“ | Ползвайте `mingw32-make` (идва с WinLibs) |
