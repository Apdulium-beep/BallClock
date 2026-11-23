#pragma once
// Геометрия сот. Не зависит от Arduino: проверяется на компьютере (test/geo_test.cpp).
#include <stdint.h>

namespace geo {
constexpr uint8_t ROWS = 7;
constexpr uint8_t W = 20;                  // шариков в самом длинном ряду
constexpr uint8_t HALF_W = 2 * W - 1;      // координата X в полушагах: 0..38
constexpr uint8_t LED_COUNT = 128;
constexpr uint8_t ROW_LEN[ROWS] = {17, 18, 19, 20, 19, 18, 17};

extern uint8_t ledRow[LED_COUNT];          // ряд шарика с номером i на ленте
extern uint8_t ledX[LED_COUNT];            // позиция X (полушаги)
extern int8_t  grid[ROWS][HALF_W];         // обратная таблица: номер светодиода или -1

// Строит таблицы. Порядок ленты: вертикальные зигзаги по парам полу-колонок {1,2},{3,4}...
// (шарик X=0 идёт первым и один). Правило проверено по таблице xyToLed проекта BallClock: 128/128.
// Если лента у вас идёт иначе — менять нужно только эту функцию.
bool init();
}
