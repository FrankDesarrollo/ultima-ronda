#include "raylib.h"
#include "raymath.h"
#include <cstdio>
int main(int argc,char**argv){ SetTraceLogLevel(LOG_WARNING); SetConfigFlags(FLAG_WINDOW_HIDDEN); InitWindow(64,64,"t");
 for(int a=1;a<argc;a++){ Model m=LoadModel(argv[a]); printf("== %s\n",argv[a]);
  for(int i=0;i<m.meshCount;i++){ BoundingBox b=GetMeshBoundingBox(m.meshes[i]); printf(" mesh %2d mat %d v %5d  (%.3f %.3f %.3f)-(%.3f %.3f %.3f)\n",i,m.meshMaterial[i],m.meshes[i].vertexCount,b.min.x,b.min.y,b.min.z,b.max.x,b.max.y,b.max.z);} }
 CloseWindow(); }
