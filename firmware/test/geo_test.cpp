// Проверка геометрии на компьютере: g++ -I src test/geo_test.cpp src/geometry.cpp
#include <cstdio>
#include <cstdlib>
#include "geometry.h"
int main() {
  if (!geo::init()) { puts("FAIL init"); return 1; }
  int cnt = 0, jumps = 0;
  for (int r = 0; r < geo::ROWS; r++) for (int x = 0; x < geo::HALF_W; x++) if (geo::grid[r][x] >= 0) cnt++;
  printf("шариков в сетке: %d\n", cnt);
  for (int i = 0; i < geo::LED_COUNT; i++) {
    if (geo::grid[geo::ledRow[i]][geo::ledX[i]] != i) { puts("FAIL обратная таблица"); return 1; }
    if (i) {
      int dr = abs(geo::ledRow[i] - geo::ledRow[i - 1]), dx = abs(geo::ledX[i] - geo::ledX[i - 1]);
      if (!(dr == 1 && dx == 1)) { printf("разрыв ленты между %d и %d\n", i - 1, i); jumps++; }
    }
  }
  printf("(ожидаем 18 разрывов, 19 кусков: 1,12,7x16,3)\n");
  printf("разрывов (мест, где нужен провод данных): %d\n", jumps);
  return cnt == 128 ? 0 : 1;
}
