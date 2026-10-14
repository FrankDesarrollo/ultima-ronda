#!/bin/sh
# Compila el juego (usa el compilador portátil de tools/)
cd "$(dirname "$0")"
export PATH="$PWD/tools/w64devkit/bin:$PATH"
R=tools/raylib-5.5/src
E=third_party/enet
mkdir -p UltimaRonda build
# ENet (red UDP del co-op): se compila una vez
[ -f build/libenet.a ] || { for f in $E/*.c; do gcc -O2 -c "$f" -I$E/include -o build/$(basename "$f" .c).o || exit 1; done; ar rcs build/libenet.a build/*.o; }
g++ -std=c++17 -O2 -Wall -Wno-missing-field-initializers -Wno-unused-function src/*.cpp -I"$R" -I$E/include -L"$R" -Lbuild -lraylib -lenet -lopengl32 -lgdi32 -lwinmm -lws2_32 -lwininet -static -mwindows -o UltimaRonda/UltimaRonda.exe "$@"
