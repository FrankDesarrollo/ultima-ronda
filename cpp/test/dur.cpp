#include "raylib.h"
#include <cstdio>
int main(int c,char**v){ SetTraceLogLevel(LOG_WARNING); for(int f=1;f<c;f++){ Wave w=LoadWave(v[f]); printf("%s %.2fs %dHz ch%d\n",GetFileName(v[f]),w.frameCount/(float)w.sampleRate,w.sampleRate,w.channels); UnloadWave(w);} }
