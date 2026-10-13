#include "raylib.h"
#include "raymath.h"
#include <cstdio>
const char *F[] = { "AK47", "M4A1", "Grease_Gun", "Suomi_KP", "Shotgun", "Sniper", "Makarov", "Luger", "FlareGun", "FragGrenadeModel" };
int main() {
    SetTraceLogLevel(LOG_WARNING); SetConfigFlags(FLAG_WINDOW_HIDDEN); InitWindow(1500, 1000, "t");
    RenderTexture2D rt = LoadRenderTexture(1500, 1000);
    BeginTextureMode(rt); ClearBackground(Color{ 90, 90, 100, 255 }); EndTextureMode();
    for (int i = 0; i < 10; i++) {
        Model m = LoadModel(TextFormat("../dl/guns/%s.glb", F[i]));
        BoundingBox b = GetModelBoundingBox(m);
        printf("%-16s meshes %d mats %d bbox (%.3f %.3f %.3f)-(%.3f %.3f %.3f) tex %dx%d\n", F[i], m.meshCount, m.materialCount,
               b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z, m.materials[1].maps[0].texture.width, m.materials[1].maps[0].texture.height);
        Vector3 c = Vector3Scale(Vector3Add(b.min, b.max), 0.5f);
        float sz = fmaxf(b.max.x - b.min.x, fmaxf(b.max.y - b.min.y, b.max.z - b.min.z));
        int col = i % 5, row = i / 5;
        // vista lateral (desde +X) en celdas de 300x500
        RenderTexture2D cell = LoadRenderTexture(300, 250);
        for (int v = 0; v < 2; v++) {
            Camera cam{};
            cam.target = c; cam.up = { 0, 1, 0 }; cam.fovy = sz * 1.1f; cam.projection = CAMERA_ORTHOGRAPHIC;
            cam.position = v == 0 ? Vector3{ c.x + 5, c.y, c.z } : Vector3{ c.x, c.y + 5, c.z + 0.001f };
            BeginTextureMode(cell); ClearBackground(Color{ 90, 90, 100, 255 }); BeginMode3D(cam); DrawModel(m, { 0, 0, 0 }, 1, WHITE);
            DrawLine3D(c, Vector3Add(c, { 0, 0, sz * 0.3f }), BLUE); DrawLine3D(c, Vector3Add(c, { 0, sz * 0.3f, 0 }), GREEN); EndMode3D();
            DrawText(TextFormat("%s %s", F[i], v ? "top(+z down)" : "side(+z right)"), 4, 4, 14, WHITE); EndTextureMode();
            BeginTextureMode(rt);
            DrawTextureRec(cell.texture, { 0, 0, 300, -250 }, { (float)col * 300, (float)row * 500 + v * 250 }, WHITE);
            EndTextureMode();
        }
        UnloadRenderTexture(cell);
        UnloadModel(m);
    }
    Image im = LoadImageFromTexture(rt.texture); ImageFlipVertical(&im); ExportImage(im, "guns.png"); CloseWindow();
}
