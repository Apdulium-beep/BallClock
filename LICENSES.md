# Лицензии и сторонние компоненты

Публикация сохраняет имеющиеся уведомления и не объявляет все входящие
библиотеки и исполняемые файлы единым произведением под одной лицензией.

- Основа BallClock и данные шрифтов: AlexGyver, MIT.
  [Текст лицензии](licenses/BallClock-MIT.txt),
  [уведомление прошивки](firmware/NOTICE-BallClock-MIT.txt),
  [исходный проект](https://github.com/AlexGyver/BallClock).
- FastLED 3.10.9: MIT, [текст](licenses/FastLED-MIT.txt),
  [исходники](https://github.com/FastLED/FastLED/tree/3.10.9).
- ESP8266 Arduino Core 3.1.2: LGPL и лицензии включённых компонентов;
  [точные исходники ядра](https://github.com/esp8266/Arduino/tree/3.1.2).
  PlatformIO-платформа закреплена в `firmware/platformio.ini`.
- Adafruit NeoPixel 1.15.5 — закреплённая зависимость PlatformIO;
  [исходники и лицензия](https://github.com/adafruit/Adafruit_NeoPixel/tree/1.15.5).
- Qt 6.11.2: динамически подключённые Core, Gui, Widgets, Network, Svg и плагины.
  [Уведомление и ссылки на исходники](licenses/Qt-NOTICE.txt),
  [лицензионные тексты](licenses/Qt/). Совместимые Qt DLL можно заменить;
  приложение не препятствует разрешённой лицензией замене/отладке библиотек.
- Microsoft Visual C++ Runtime и DirectX-компоненты в Windows-комплекте
  распространяются на условиях их правообладателя; они не являются MIT/Qt.

Исходники приложения и команды сборки доступны в этом репозитории. Каталоги
зависимостей `.pio` и локальные SDK не включены: версии и исходные upstream
доступны по ссылкам и через пакетные менеджеры.
