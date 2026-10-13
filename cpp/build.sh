#!/bin/sh
# Compila el juego (usa el compilador portátil de tools/)
cd "$(dirname "$0")"
export PATH="$PWD/tools/w64devkit/bin:$PATH"
R=tools/raylib-5.5/src
mkdir -p UltimaRonda
g++ -std=c++17 -O2 -Wall -Wno-missing-field-initializers -Wno-unused-function src/*.cpp -I"$R" -L"$R" -lraylib -lopengl32 -lgdi32 -lwinmm -static -mwindows -o UltimaRonda/UltimaRonda.exe "$@"
