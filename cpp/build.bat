@echo off
rem Compila Ultima Ronda con el compilador portatil incluido en tools\
cd /d "%~dp0"
set PATH=%~dp0tools\w64devkit\bin;%PATH%
set R=tools/raylib-5.5/src
set E=third_party/enet
if not exist UltimaRonda mkdir UltimaRonda
if not exist build mkdir build
rem ENet (red UDP del co-op): se compila una sola vez
if not exist build\libenet.a (
  for %%f in (third_party\enet\*.c) do gcc -O2 -c %%f -I%E%/include -o build\%%~nf.o
  ar rcs build\libenet.a build\callbacks.o build\compress.o build\host.o build\list.o build\packet.o build\peer.o build\protocol.o build\unix.o build\win32.o
)
g++ -std=c++17 -O2 src/*.cpp -I%R% -I%E%/include -L%R% -Lbuild -lraylib -lenet -lopengl32 -lgdi32 -lwinmm -lws2_32 -lwininet -static -mwindows -o UltimaRonda/UltimaRonda.exe
if errorlevel 1 (
  echo.
  echo ERROR al compilar.
  pause
  exit /b 1
)
echo Listo: UltimaRonda\UltimaRonda.exe
pause
