#!/bin/sh
# Регрессионные тесты на компьютере (нужен g++). Запуск из папки test/host:  sh run.sh
# Сравнивает отрисовку (129 600 кадров) и работу настроек с эталонными результатами golden_*.txt.
# Если вы НАМЕРЕННО изменили вид или настройки, проверьте, что отличия ожидаемы, и обновите эталон: sh run.sh update
set -e
SRC=../../src
g++ -std=c++17 -w -I. -I$SRC h_render.cpp $SRC/render.cpp $SRC/geometry.cpp $SRC/settings.cpp -o /tmp/bc_render
g++ -std=c++17 -w -I. -I$SRC h_settings.cpp $SRC/settings.cpp -o /tmp/bc_settings
/tmp/bc_render > /tmp/bc_render.txt
/tmp/bc_settings > /tmp/bc_settings.txt
if [ "$1" = "update" ]; then cp /tmp/bc_render.txt golden_render.txt; cp /tmp/bc_settings.txt golden_settings.txt; echo "эталоны обновлены"; exit 0; fi
diff golden_render.txt /tmp/bc_render.txt   && echo "отрисовка: OK"
diff golden_settings.txt /tmp/bc_settings.txt && echo "настройки: OK"
(cd ../.. && g++ -std=c++17 -I src test/geo_test.cpp src/geometry.cpp -o /tmp/bc_geo && /tmp/bc_geo | tail -2)
