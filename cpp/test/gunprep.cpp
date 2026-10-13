// Convierte los GLB de armas (paquete CC0 "Guns & Explosives" de 3DModelsCC0) al formato del juego:
//  - geometría con la empuñadura en el origen del arma (gatillo en 0,-0.04,-0.08 y cañón hacia -Z)
//  - texturas reducidas a 1024: _col (color), _nrm (normal DirectX), _mr (R = rugosidad, G = metal)
// Uso: gunprep.exe <carpeta glb> <carpeta salida>
#include "raylib.h"
#include "raymath.h"
#include <cstdio>
#include <vector>
#include <string>

struct Cfg {
    const char *file, *out;
    int trigger;                 // malla del gatillo (referencia de la mano)
    std::vector<int> hide;       // mallas descolocadas que se ocultan
    float pitchDeg;              // corrección de inclinación (grados sobre X)
    bool flip;                   // girar 180° (cañón hacia +Z en el original)
    float foreZ, foreY;          // punto de la mano izquierda (coordenadas originales)
    float scale;
};

static const Cfg CFG[] = {
    { "Makarov",          "pistol",   4, { 0 },  0,      false,  0,      0,     1 },
    { "Shotgun",          "shotgun",  2, {},     0,      false, -0.056f, -0.04f, 1 },
    { "Grease_Gun",       "smg",      4, {},     0,      false, -0.09f, -0.06f,  1 },
    { "M4A1",             "rifle",   12, {},     0,      false, -0.22f, -0.02f,  1 },
    { "AK47",             "ak47",     3, {},     0,      false,  0.0f,  -0.03f,  1 },
    { "Sniper",           "sniper",   7, {},     -10.46f, false, -0.1f,  -0.05f,  1 },
    { "Luger",            "luger",    7, { 1 },  0,      true,   0,      0,      1 },
    { "Suomi_KP",         "suomi",    4, {},     0,      false, -0.22f, -0.01f,  1 },
    { "FlareGun",         "flare",    3, {},     0,      false,  0,      0,      1 },
    { "FragGrenadeModel", "grenade", -1, {},     0,      false,  0,      0,      1 },
};

static void SaveTex(Texture2D t, const char *path, int mode) {
    Image img = LoadImageFromTexture(t);
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    if (img.width > 1024) ImageResize(&img, 1024, 1024);
    if (mode == 1) {   // glTF: G = rugosidad, B = metal  ->  R = rugosidad, G = metal
        Color *px = (Color *)img.data;
        for (int i = 0; i < img.width * img.height; i++) { unsigned char r = px[i].g, m = px[i].b; px[i] = Color{ r, m, 0, 255 }; }
    }
    if (mode != 2) ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
    ExportImage(img, path);
    UnloadImage(img);
}

int main(int argc, char **argv) {
    if (argc < 3) { printf("uso: gunprep <glb dir> <out dir>\n"); return 1; }
    SetTraceLogLevel(LOG_WARNING); SetConfigFlags(FLAG_WINDOW_HIDDEN); InitWindow(64, 64, "prep");
    for (const Cfg &c : CFG) {
        Model m = LoadModel(TextFormat("%s/%s.glb", argv[1], c.file));
        if (!m.meshCount) { printf("falta %s\n", c.file); return 1; }
        Matrix fix = MatrixMultiply(MatrixRotateX(c.pitchDeg * DEG2RAD), MatrixRotateY(c.flip ? PI : 0));
        fix = MatrixMultiply(fix, MatrixScale(c.scale, c.scale, c.scale));
        Vector3 anchor{ 0, 0, 0 };
        if (c.trigger >= 0) {
            BoundingBox b = GetMeshBoundingBox(m.meshes[c.trigger]);
            anchor = Vector3Transform(Vector3Scale(Vector3Add(b.min, b.max), 0.5f), fix);
        }
        Vector3 target = c.trigger >= 0 ? Vector3{ 0, -0.04f, -0.08f } : Vector3{ 0, 0, 0 };
        Matrix tf = MatrixMultiply(fix, MatrixTranslate(target.x - anchor.x, target.y - anchor.y, target.z - anchor.z));
        Matrix nf = MatrixTranspose(MatrixInvert(tf));
        Vector3 fore = Vector3Transform(Vector3{ 0, c.foreY, c.foreZ }, tf);
        FILE *f = fopen(TextFormat("%s/%s.mesh", argv[2], c.out), "wb");
        int n = 0;
        for (int i = 0; i < m.meshCount; i++) { bool h = false; for (int k : c.hide) h |= k == i; n += !h; }
        float minZ = 1e9f, maxY = -1e9f;
        fwrite(&n, 4, 1, f);
        fwrite(&fore, 4, 3, f);
        for (int i = 0; i < m.meshCount; i++) {
            bool h = false; for (int k : c.hide) h |= k == i;
            if (h) continue;
            Mesh &me = m.meshes[i];
            int vc = me.vertexCount, ic = me.indices ? me.triangleCount * 3 : 0;
            std::vector<float> v(vc * 3), nr(vc * 3), uv(vc * 2);
            for (int k = 0; k < vc; k++) {
                Vector3 p = Vector3Transform(Vector3{ me.vertices[k * 3], me.vertices[k * 3 + 1], me.vertices[k * 3 + 2] }, tf);
                Vector3 q = Vector3Normalize(Vector3Transform(Vector3{ me.normals[k * 3], me.normals[k * 3 + 1], me.normals[k * 3 + 2] }, nf));
                v[k * 3] = p.x; v[k * 3 + 1] = p.y; v[k * 3 + 2] = p.z;
                nr[k * 3] = q.x; nr[k * 3 + 1] = q.y; nr[k * 3 + 2] = q.z;
                uv[k * 2] = me.texcoords[k * 2]; uv[k * 2 + 1] = me.texcoords[k * 2 + 1];
                if (p.z < minZ) minZ = p.z;
                if (p.y > maxY) maxY = p.y;
            }
            fwrite(&vc, 4, 1, f); fwrite(&ic, 4, 1, f);
            fwrite(v.data(), 4, vc * 3, f); fwrite(nr.data(), 4, vc * 3, f); fwrite(uv.data(), 4, vc * 2, f);
            if (ic) fwrite(me.indices, 2, ic, f);
        }
        fclose(f);
        Material &mt = m.materials[1];
        SaveTex(mt.maps[MATERIAL_MAP_ALBEDO].texture, TextFormat("%s/%s_col.png", argv[2], c.out), 0);
        SaveTex(mt.maps[MATERIAL_MAP_NORMAL].texture, TextFormat("%s/%s_nrm.png", argv[2], c.out), 0);
        SaveTex(mt.maps[MATERIAL_MAP_ROUGHNESS].texture, TextFormat("%s/%s_mr.png", argv[2], c.out), 1);
        printf("%-8s meshes %d  muzzleZ %.3f  topY %.3f  fore (%.3f %.3f %.3f)\n", c.out, n, minZ, maxY, fore.x, fore.y, fore.z);
        UnloadModel(m);
    }
    CloseWindow();
}
