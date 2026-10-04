# Компилиране и пускане на Energy Crisis за Android

Този документ описва архитектурата, компилацията и инсталацията на мобилната версия на **Energy Crisis** за Android телефони и таблети.

---

## 📱 Характеристики на Android версията

* **C++17 + SFML 3 (NativeActivity)**: Играта работи директно нативно през NDK без нужда от тромави Web wrappers.
* **Сензорно управление (Touch Controls)**:
  * **1 пръст (Tap)**: Избор на сграда, покупка на територия, добив от мина, строеж върху парцел, кликове по менюта и бутони.
  * **2 пръста едновременно (Two-finger tap)**: Еквивалент на десен бутон (ОТКАЗ / прекратяване на строеж).
  * **Повторен клик върху избрана карта**: Бърз отказ от текущия строеж.
* **Адаптивен екран (Dynamic Viewport)**:
  * Играта използва виртуален canvas **1600x900** (16:9).
  * Автоматично мащабиране (letterbox / pillarbox) според екрана на телефона (18:9, 19.5:9, 20:9 и др.) без изкривяване на пропорциите.
  * Заключена ориентация: **Landscape** (`sensorLandscape`).
* **Asset стрийминг**: Шрифтовете, графиката и многоезичните файлове (`assets/lang/*.lang`) се зареждат директно от APK архива чрез `sf::FileInputStream` и Android AssetManager.
* **Жизнен цикъл**: Автоматична пауза на звука и играта при преминаване на приложението на заден план или заключване на екрана.

---

## 🛠️ Изисквания за компилация

| Компонент | Версия / Път | Описание |
| :--- | :--- | :--- |
| **Java Development Kit (JDK)** | OpenJDK 17 или 21 | Нужен за Gradle |
| **Android SDK** | API Level 34 (Android 14) | С Build-tools 34.0.0 |
| **Android NDK** | r26b / r26d (26.1.10909125) | C++ toolchain с Clang |
| **CMake** | 3.22+ | Използва се от Gradle NDK |
| **Gradle** | 8.7 (включен е `gradlew`) | Сглобява пакета |

---

## 🚀 Бързо компилиране (Gradle)

Всички файлове на Android проекта са организирани в папка `android/`:

```bash
cd android

# Компилиране на инсталационен APK пакет (Debug):
./gradlew assembleDebug

# Компилиране на оптимизиран APK пакет (Release):
./gradlew assembleRelease
```

След успешна компилация готовият `.apk` файл се намира в:
* `android/app/build/outputs/apk/debug/app-debug.apk`
* `android/app/build/outputs/apk/release/app-release.apk`

---

## 📲 Инсталиране на реален телефон

1. Активирайте **Developer Options** на вашия телефон:
   * `Settings` -> `About phone` -> кликнете 7 пъти върху `Build number`.
2. Включете **USB Debugging** (`Настройки за програмисти` -> `USB отстраняване на грешки`).
3. Свържете телефона с кабел към компютъра и разрешете достъпа.
4. Проверете връзката:
   ```bash
   adb devices
   ```
5. Инсталирайте APK файла:
   ```bash
   adb install -r android/app/build/outputs/apk/debug/app-debug.apk
   ```
6. Стартирайте играта:
   ```bash
   adb shell am start -n bg.energycrisis.game/android.app.NativeActivity
   ```

---

## 🤖 Тестване под Linux с Waydroid

Ако използвате Linux и имате инсталиран **Waydroid**:

```bash
# Проверка за активен контейнер
adb devices

# Директна инсталация в Waydroid
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
```

---

## 🔍 Преглед на логовете (Logcat)

За преглед на системните съобщения и дебъг логове от играта:

```bash
adb logcat -s EnergyCrisis:V SFML:V AndroidRuntime:E
```
