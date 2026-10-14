// Información del modelo del soldado: mallas, materiales (color), rango de UV y animaciones
#include "raylib.h"
#include <cstdio>
#include <cfloat>
int main(int argc, char **argv) {
    SetTraceLogLevel(LOG_WARNING); SetConfigFlags(FLAG_WINDOW_HIDDEN); InitWindow(64, 64, "t");
    const char *p = argc > 1 ? argv[1] : "assets/models/soldier.glb";
    Model m = LoadModel(p);
    for (int i = 0; i < m.materialCount; i++) { Color c = m.materials[i].maps[MATERIAL_MAP_ALBEDO].color; printf("mat %d color %d %d %d tex %d\n", i, c.r, c.g, c.b, m.materials[i].maps[0].texture.id); }
    for (int i = 0; i < m.meshCount; i++) {
        Mesh &me = m.meshes[i];
        float u0 = FLT_MAX, u1 = -FLT_MAX, v0 = FLT_MAX, v1 = -FLT_MAX;
        for (int k = 0; me.texcoords && k < me.vertexCount; k++) { float u = me.texcoords[k * 2], v = me.texcoords[k * 2 + 1]; if (u < u0) u0 = u; if (u > u1) u1 = u; if (v < v0) v0 = v; if (v > v1) v1 = v; }
        BoundingBox b = GetMeshBoundingBox(me);
        printf("mesh %2d mat %d v %4d uv %s [%.2f..%.2f]x[%.2f..%.2f] y %.2f..%.2f\n", i, m.meshMaterial[i], me.vertexCount, me.texcoords ? "si" : "no", u0, u1, v0, v1, b.min.y, b.max.y);
    }
    int n = 0; ModelAnimation *a = LoadModelAnimations(p, &n);
    for (int i = 0; i < n; i++) printf("anim %d %s (%d)\n", i, a[i].name, a[i].frameCount);
    for (int i = 0; i < m.boneCount; i++) printf("%s ", m.bones[i].name);
    printf("\n");
    CloseWindow();
}
