#include "raylib.h"
#include "raymath.h"
#include <cstdio>
int main(int argc,char**argv){ SetTraceLogLevel(LOG_WARNING); SetConfigFlags(FLAG_WINDOW_HIDDEN); InitWindow(1400,500,"t");
 const char* mp=argv[1];
 Model m=LoadModel(mp);
 BoundingBox bb=GetModelBoundingBox(m);
 printf("meshes %d mats %d bones %d bb (%.2f %.2f %.2f)-(%.2f %.2f %.2f)\n",m.meshCount,m.materialCount,m.boneCount,bb.min.x,bb.min.y,bb.min.z,bb.max.x,bb.max.y,bb.max.z);
 for(int i=0;i<m.meshCount;i++) printf("mesh %d mat %d verts %d bones %d\n",i,m.meshMaterial[i],m.meshes[i].vertexCount,m.meshes[i].boneCount);
 for(int i=0;i<m.materialCount;i++){Color c=m.materials[i].maps[0].color; printf("mat %d col %d %d %d tex %d\n",i,c.r,c.g,c.b,m.materials[i].maps[0].texture.id);}
 for(int i=0;i<m.boneCount;i++) printf("%s(%d) ",m.bones[i].name,m.bones[i].parent); printf("\n");
 int n=0; ModelAnimation*A=LoadModelAnimations(mp,&n);
 for(int i=0;i<n;i++) printf("anim %d %s frames %d\n",i,A[i].name,A[i].frameCount);
 RenderTexture2D rt=LoadRenderTexture(1400,500);
 float s=argc>2?atof(argv[2]):1;
 Camera cam={{0,1.0f,7.5f},{0,0.6f,0},{0,1,0},45,CAMERA_PERSPECTIVE};
 BeginTextureMode(rt); ClearBackground(DARKGRAY); BeginMode3D(cam); DrawGrid(10,1);
 int pick[6]={-1,0,1,3,4,5}; 
 for(int k=0;k<6;k++){ if(pick[k]>=0&&pick[k]<n) UpdateModelAnimation(m,A[pick[k]],A[pick[k]].frameCount/3);
   m.transform=MatrixRotateY(PI/2); DrawModel(m,{-5.5f+k*2.2f,0,0},s,WHITE); }
 EndMode3D(); EndTextureMode();
 Image im=LoadImageFromTexture(rt.texture); ImageFlipVertical(&im); ExportImage(im,"wolf.png"); CloseWindow(); }
