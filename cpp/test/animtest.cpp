#include "raylib.h"
#include "rlgl.h"
#include "../src/anim.h"
#include <cstdio>
int main(){ SetTraceLogLevel(LOG_WARNING); SetConfigFlags(FLAG_WINDOW_HIDDEN); InitWindow(1200,500,"t");
 const char* mp="../assets/models/InfectedCityMan.glb";
 Model m=LoadModel(mp);
 const char* tp="../assets/models/tex/InfectedCityMan_City_";
 m.materials[1].maps[0].texture=LoadTexture(TextFormat("%sMan_Infected_Outfit1_BaseColor.png",tp));
 m.materials[2].maps[0].texture=LoadTexture(TextFormat("%sInfected_Man_Body1_BaseColor.png",tp));
 for(int i=0;i<m.meshCount;i++) printf("mesh %d mat %d verts %d\n",i,m.meshMaterial[i],m.meshes[i].vertexCount);
 const char* an[]={"Zombie_Walk","Zombie_Running","Zombie_Attack","Zombie_Dying"};
 ModelAnimation A[4]; for(int i=0;i<4;i++){ bool ok=BakeAnimation(m,mp,TextFormat("../assets/models/%s.glb",an[i]),30,true,&A[i]); printf("%s ok %d frames %d\n",an[i],ok,A[i].frameCount); }
 RenderTexture2D rt=LoadRenderTexture(1200,500);
 Camera cam={{0,1.2f,6.5f},{0,0.9f,0},{0,1,0},45,CAMERA_PERSPECTIVE};
 BeginTextureMode(rt); ClearBackground(DARKGRAY); BeginMode3D(cam); DrawGrid(10,1);
 float xs[]={-4.5f,-2.7f,-0.9f,0.9f,2.7f,4.5f};
 int which[]={-1,0,0,1,2,3}; int fr[]={0,5,20,10,25,90};
 for(int k=0;k<6;k++){ if(which[k]>=0) UpdateModelAnimation(m,A[which[k]],fr[k]%A[which[k]].frameCount); else { /* bind pose */ }
   DrawModel(m,{xs[k],0,0},1.0f,WHITE); }
 EndMode3D(); EndTextureMode();
 Image im=LoadImageFromTexture(rt.texture); ImageFlipVertical(&im); ExportImage(im,"anim.png"); CloseWindow(); }
