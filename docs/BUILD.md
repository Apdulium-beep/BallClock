# Сборка

Готовые программы доступны в релизах; сборка не нужна для обычной установки.

## ESP8266

Нужны Python и PlatformIO Core. Из корня:

```powershell
pio run --project-dir firmware
```

Либо `build-firmware.cmd` в Windows. Платформа и библиотеки закреплены в
`firmware/platformio.ini`. Сборка генерирует `src/webpage.h` из
`source/android/assets/mobile.html` и **не прошивает плату автоматически**.

Загрузка, только если она нужна:

```powershell
pio run --project-dir firmware --target upload --upload-port COM5
```

Заменить COM5 на свой порт. Сначала сохранить настройки/сделать собственную
резервную копию. Не стирать весь flash и не загружать пустую файловую систему,
если нужно сохранить LittleFS с параметрами, градиентами и картинами.

## Windows / Qt

Текущие готовые бинарники — Windows x64. Используются Qt 6.11.2 MSVC x64,
Visual Studio C++ Build Tools, CMake, Ninja и MinGW g++ для отдельной DLL общего
рендера. Сначала собрать прошивку, чтобы PlatformIO установил FastLED.

`build.cmd` содержит пути к Qt в `C:\Qt` и Visual Studio 2026 Community.
Перед запуском отредактировать пути под свою установку; путь к MinGW для
`clockengine.dll` также задан в `source/qt_app/CMakeLists.txt`.

```powershell
cmd /c build.cmd
cmd /c build\test.cmd
```

Не смешивать EXE и `clockengine.dll` от разных сборок: их внутренний ABI общий.
Qt DLL и плагины развёртываются через `windeployqt`. См. `licenses/Qt-NOTICE.txt`.
Аппаратные тесты `build/test-live.cmd` и `source/qt_app/tests/*live*.py` требуют
подключённую плату и могут временно менять/восстанавливать её параметры.

## Android

Нужны JDK 17 (`JAVA_HOME`), Android SDK platform 35 и build-tools 35.0.0.
Задать `ANDROID_SDK_ROOT` либо передать `-SdkRoot`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File source/android/build.ps1 -SdkRoot C:\Android\Sdk
powershell -NoProfile -ExecutionPolicy Bypass -File source/android/test.ps1
node source/android/tests/preview_mobile.cjs
```

Тестовый запуск Chromium требует Node.js и установленный Google Chrome;
пути к инструментам тестов при необходимости адаптировать под свою машину.

APK получается в `android/BallClock.apk`. При первой самостоятельной сборке
создаётся **свой** ключ и пароль в `backups/android-signing/`; Git их игнорирует.
Ключ релизных APK в репозитории **не публикуется**. Самостоятельно подписанный
APK нельзя установить поверх опубликованного APK с другой подписью.
Не удалять своё приложение с данными без необходимости; для разработки можно
использовать отдельный package name. Пароль/ключ хранить отдельно и не коммитить.

## Обновление публикации

В корне хранится только актуальный APK. Старые APK — в отдельных релизах
`android-v1.0` ... `android-v1.8`. Контрольные суммы: `docs/apk-manifest.json`,
корневой `SHA256SUMS.txt` и `SHA256SUMS.txt` в каждом релизе.
