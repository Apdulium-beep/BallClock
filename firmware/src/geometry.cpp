#include "geometry.h"
#include <string.h>

namespace geo {
uint8_t ledRow[LED_COUNT];
uint8_t ledX[LED_COUNT];
int8_t  grid[ROWS][HALF_W];

static bool hasBall(uint8_t r, uint8_t x) {
  const uint8_t n = ROW_LEN[r], x0 = W - n;
  return x >= x0 && x <= x0 + 2 * (n - 1) && ((x - x0) & 1) == 0;
}

bool init() {
  memset(grid, -1, sizeof(grid));
  uint8_t idx = 0;
  // Группа g занимает полу-колонки X = 2g-1 и 2g (группа 0 = один шарик X=0).
  // Нечётные группы идут сверху вниз, чётные снизу вверх.
  for (uint8_t g = 0; g < W; g++) {
    for (uint8_t s = 0; s < ROWS; s++) {
      const uint8_t r = (g & 1) ? s : ROWS - 1 - s;
      for (int8_t d = -1; d <= 0; d++) {
        const int8_t x = 2 * g + d;
        if (x >= 0 && x < HALF_W && hasBall(r, x)) {
          if (idx >= LED_COUNT) return false;
          ledRow[idx] = r; ledX[idx] = (uint8_t)x; grid[r][x] = idx; idx++;
        }
      }
    }
  }
  return idx == LED_COUNT;
}
}
