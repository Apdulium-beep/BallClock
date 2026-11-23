@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
set "PATH=C:\Qt\6.11.2\msvc2022_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;%PATH%"
cl /nologo /O2 /MT /EHsc /std:c++17 /utf-8 source\launcher\launcher.cpp /Fe:launcher.exe /Fo:build\launcher.obj /link /SUBSYSTEM:WINDOWS
if errorlevel 1 exit /b 1
cmake -S source\qt_app -B build\qt -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/msvc2022_64
if errorlevel 1 exit /b 1
cmake --build build\qt --parallel
if errorlevel 1 exit /b 1
copy /y build\qt\BallClockController.exe qt_app\BallClockController.exe >nul
if errorlevel 1 exit /b 1
copy /y build\qt\clockengine.dll qt_app\clockengine.dll >nul
if errorlevel 1 exit /b 1
windeployqt --release --compiler-runtime --no-translations --no-opengl-sw --no-system-d3d-compiler qt_app\BallClockController.exe
if errorlevel 1 exit /b 1
echo Build completed.
copy /y "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Redist\MSVC\14.51.36231\x64\Microsoft.VC145.CRT\*.dll" qt_app\ >nul
if errorlevel 1 exit /b 1
