@echo off
rem Compila Última Ronda con el compilador portátil incluido en tools\
cd /d "%~dp0"
set PATH=%~dp0tools\w64devkit\bin;%PATH%
set R=tools/raylib-5.5/src
if not exist UltimaRonda mkdir UltimaRonda
g++ -std=c++17 -O2 src/anim.cpp src/audio.cpp src/hud.cpp src/intro.cpp src/main.cpp src/options.cpp src/ragdoll.cpp src/render.cpp src/weapons.cpp src/world.cpp src/zombie.cpp -I%R% -L%R% -lraylib -lopengl32 -lgdi32 -lwinmm -static -mwindows -o UltimaRonda/UltimaRonda.exe
if errorlevel 1 (
  echo.
  echo ERROR al compilar.
  pause
  exit /b 1
)
echo Listo: UltimaRonda\UltimaRonda.exe
pause
