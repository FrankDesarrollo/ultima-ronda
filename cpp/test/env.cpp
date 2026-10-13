#include "raylib.h"
#include <cstdio>
#include <cmath>
int main(int c,char**v){ SetTraceLogLevel(LOG_WARNING); for(int f=1;f<c;f++){ Wave w=LoadWave(v[f]); WaveFormat(&w,44100,32,1); float*s=(float*)w.data; printf("%-16s",GetFileName(v[f])); for(unsigned i=0;i+2205<=w.frameCount;i+=2205){ float r=0; for(int k=0;k<2205;k++) r+=s[i+k]*s[i+k]; r=sqrtf(r/2205); printf("%c"," .:-=+*#%@"[(int)fminf(9,r*20)]); } printf("\n"); UnloadWave(w);} }
