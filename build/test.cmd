@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
set "PATH=C:\Qt\6.11.2\msvc2022_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;%PATH%"
cmake -S source\qt_app -B build\qt -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/msvc2022_64
if errorlevel 1 exit /b 1
cmake --build build\qt --target protocol_test geometry_test display_test settings_ui_test engine_test wifi_transport_test firmware_api_test wifi_ui_test network_mode_test
if errorlevel 1 exit /b 1
ctest --test-dir build\qt --output-on-failure
