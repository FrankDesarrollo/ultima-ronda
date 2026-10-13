#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <cstdio>
const char *F[] = { "Makarov", "Shotgun", "Grease_Gun", "M4A1", "AK47", "Sniper", "Luger", "Suomi_KP" };
int main() {
    SetTraceLogLevel(LOG_WARNING); SetConfigFlags(FLAG_WINDOW_HIDDEN); InitWindow(1600, 1600, "t");
    RenderTexture2D rt = LoadRenderTexture(1600, 1600);
    BeginTextureMode(rt); ClearBackground(Color{ 150, 150, 160, 255 }); EndTextureMode();
    RenderTexture2D cell = LoadRenderTexture(800, 400);
    for (int i = 0; i < 8; i++) {
        Model m = LoadModel(TextFormat("../dl/guns/%s.glb", F[i]));
        Camera cam{}; cam.target = { 0, 0, 0.05f }; cam.position = { 5, 0, 0.05f }; cam.up = { 0, 1, 0 }; cam.fovy = 0.6f; cam.projection = CAMERA_ORTHOGRAPHIC;
        if (i == 0 || i == 6) cam.fovy = 0.3f;
        BeginTextureMode(cell); ClearBackground(Color{ 150, 150, 160, 255 }); BeginMode3D(cam);
        for (int k = 0; k < m.meshCount; k++) DrawMesh(m.meshes[k], m.materials[m.meshMaterial[k]], MatrixIdentity());
        float st = cam.fovy > 0.4f ? 0.1f : 0.05f;
        for (int g = -12; g <= 12; g++) {
            Color c = g == 0 ? RED : Color{ 255, 255, 255, 90 };
            DrawLine3D({ 0.2f, g * st, -1.5f }, { 0.2f, g * st, 1.5f }, c);
            DrawLine3D({ 0.2f, -1.5f, g * st }, { 0.2f, 1.5f, g * st }, c);
        }
        EndMode3D();
        DrawText(TextFormat("%s grid %.2fm (red=0) +z right, +y up", F[i], st), 4, 4, 20, BLACK);
        for (int k = 0; k < m.meshCount; k++) { BoundingBox b = GetMeshBoundingBox(m.meshes[k]); printf("%s mesh%d z[%.3f,%.3f] y[%.3f,%.3f] x[%.3f,%.3f]\n", F[i], k, b.min.z, b.max.z, b.min.y, b.max.y, b.min.x, b.max.x); }
        EndTextureMode();
        BeginTextureMode(rt); DrawTextureRec(cell.texture, { 0, 0, 800, -400 }, { (float)(i % 2) * 800, (float)(i / 2) * 400 }, WHITE); EndTextureMode();
        UnloadModel(m);
    }
    Image im = LoadImageFromTexture(rt.texture); ImageFlipVertical(&im); ExportImage(im, "gungrid.png"); CloseWindow();
}
