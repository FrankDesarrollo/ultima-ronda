// Armas: datos, modelos 3D, disparo, granadas, proyectiles, explosiones y efectos.
#include "game.h"

enum { S_PISTOL, S_SHOTGUN, S_SMG, S_RIFLE, S_SNIPER, S_RAY, S_ZAP, S_LAUNCHER, S_ROCKET, S_FLARE, S_THUNDER };
static const char *SOUND_NAMES[] = { "pistol", "shotgun", "smg", "rifle", "sniper", "ray", "zap", "launcher", "rocket", "flareshot", "thunder" };

//                 nombre               PaP                    dmg   head rate   auto   mag reserve reload spread pell recoil cost  len   sonido     proj rad splash chain tracer
WeaponDef WEAPONS[W_COUNT] = {
    { "PISTOLA",          "TRUENO GEMELO",       40,   3,   0.17f, false, 8,   80,  1.5f, 0.006f, 1, 0.012f, 0,    0.22f, S_PISTOL,   0, 0,    0,    0,  0 },
    { "ESCOPETA",         "ESCUPIDORA INFERNAL", 30,   2,   0.8f,  false, 6,   36,  2.6f, 0.07f,  8, 0.04f,  500,  0.55f, S_SHOTGUN,  0, 0,    0,    0,  0 },
    { "SUBFUSIL",         "ENJAMBRE",            42,   3,   0.075f,true,  30,  180, 2.0f, 0.025f, 1, 0.006f, 1000, 0.4f,  S_SMG,      0, 0,    0,    0,  0 },
    { "RIFLE DE ASALTO",  "TORMENTA DE PLOMO",   85,   3,   0.1f,  true,  30,  150, 2.3f, 0.012f, 1, 0.008f, 1400, 0.55f, S_RIFLE,    0, 0,    0,    0,  0 },
    { "AK-47",            "LA DEVASTADORA",      110,  2.5f,0.09f, true,  45,  315, 2.5f, 0.03f,  1, 0.007f, 2000, 0.6f,  S_RIFLE,    0, 0,    0,    0,  0 },
    { "FRANCOTIRADOR",    "OJO DEL DIABLO",      900,  5,   1.1f,  false, 5,   40,  3.0f, 0.0f,   1, 0.06f,  1500, 0.7f,  S_SNIPER,   0, 0,    0,    0,  0xffffaa },
    { "LUGER",            "MANO DE DIOS",        220,  4,   0.35f, false, 8,   64,  2.8f, 0.004f, 1, 0.04f,  0,    0.3f,  S_SNIPER,   0, 0,    0,    0,  0 },
    { "PISTOLA DE RAYOS", "RAYO ESTELAR",        700,  2,   0.33f, false, 20,  160, 2.4f, 0.004f, 1, 0.015f, 0,    0.3f,  S_RAY,      0, 0,    2.2f, 0,  0x55ff55 },
    { "WUNDERWAFFE DG-2", "FURIA DE ZEUS",       1e9f, 1,   0.9f,  false, 3,   12,  3.0f, 0.0f,   1, 0.03f,  0,    0.45f, S_ZAP,      0, 0,    0,    6,  0 },
    { "LANZAGRANADAS",    "EL VOLCÁN",           900,  1,   0.55f, false, 6,   36,  3.0f, 0.0f,   1, 0.05f,  2500, 0.45f, S_LAUNCHER, 1, 4.5f, 0,    0,  0 },
    { "LANZAMISILES",     "APOCALIPSIS",         2500, 1,   0.8f,  false, 1,   14,  2.4f, 0.0f,   1, 0.09f,  3000, 0.9f,  S_ROCKET,   2, 6.0f, 0,    0,  0 },
    { "SUOMI KP/-31",     "EL INVIERNO ETERNO",  45,   3,   0.065f,true,  71,  284, 3.2f, 0.03f,  1, 0.006f, 1500, 0.55f, S_SMG,      0, 0,    0,    0,  0 },
    { "PISTOLA DE BENGALAS","SOL DEL INFIERNO",  300,  1,   0.6f,  false, 1,   24,  1.6f, 0.0f,   1, 0.03f,  1000, 0.25f, S_FLARE,    3, 2.6f, 0,    0,  0 },
    { "MAGNUM .357",      "EL JUICIO FINAL",     300,  4,   0.42f, false, 6,   48,  3.0f, 0.004f, 1, 0.05f,  1200, 0.35f, S_SNIPER,   0, 0,    0,    0,  0 },
    { "ESCOPETA RECORTADA","DOBLE CONDENA",      40,   2,   0.25f, false, 2,   40,  2.0f, 0.11f,  10,0.06f,  1200, 0.4f,  S_SHOTGUN,  0, 0,    0,    0,  0 },
    { "CAÑÓN DE TRUENO",  "TORMENTA DIVINA",     1e9f, 1,   1.0f,  false, 2,   12,  3.4f, 0.0f,   1, 0.1f,   0,    0.6f,  S_THUNDER,  4, 15.0f,0,    0,  0 },
};
std::vector<int> BOX_POOL = { W_SHOTGUN, W_SHOTGUN, W_SMG, W_SMG, W_RIFLE, W_RIFLE, W_LMG, W_LMG, W_SNIPER, W_SNIPER,
                              W_REVOLVER, W_REVOLVER, W_RAYGUN, W_RAYGUN, W_WUNDER, W_LAUNCHER, W_LAUNCHER, W_RPG, W_RPG,
                              W_SUOMI, W_SUOMI, W_FLARE, W_FLARE, W_MAGNUM, W_MAGNUM, W_SAWED, W_SAWED, W_THUNDER };

// Estadísticas reales de un arma (con Pack-a-Punch si está mejorada)
WStats Stats(int id, bool papd) {
    const WeaponDef &d = WEAPONS[id];
    WStats s{ d.name, d.dmg, d.head, d.rate, d.autoFire, d.mag, d.reserve, d.reload, d.spread, d.pellets, d.recoil, d.sound, d.projectile, d.radius, d.splash, d.chain, d.tracer };
    if (papd) {
        s.name = d.papName; s.dmg *= 2.5f; s.head += 1;
        s.mag = (int)roundf(d.mag * 1.5f); s.reserve = (int)roundf(d.reserve * 1.6f); s.reload *= 0.85f;
        if (s.tracer) s.tracer = 0xff4466;
        if (s.chain) s.chain = 12;
        if (s.radius) s.radius *= 1.3f;
        if (s.mag < 1) s.mag = 1;
    }
    return s;
}

// ---------------------------------------------------------------
// Modelos de las armas (piezas sencillas con materiales metálicos reales)
// ---------------------------------------------------------------
GunModel gunModels[W_COUNT];
static Mesh coneMesh;
static std::vector<Mesh *> grenadeMeshes; static PMat *grenadeMat = nullptr;

// Armas con modelo 3D real (paquete CC0 "Guns & Explosives", convertido con test/gunprep.cpp)
static const char *REAL_GUN[W_COUNT] = { "pistol", "shotgun", "smg", "rifle", "ak47", "sniper", "luger", nullptr, nullptr, nullptr, nullptr, "suomi", "flare" };
// Disparo grabado de cada arma (assets/sfx/<nombre>.wav)
static const char *GUN_SND[W_COUNT] = { "pistol", "shotgun", "smg", "rifle", "ak47", "sniper", "luger", nullptr, nullptr, nullptr, nullptr, "suomi", "flare", "magnum", "sawed" };
static bool OneHanded(int id) { return id == W_PISTOL || id == W_REVOLVER || id == W_RAYGUN || id == W_FLARE || id == W_MAGNUM; }

// Lee un .mesh: número de mallas, punto de la mano izquierda y, por malla, vértices/normales/uv/índices
static bool LoadRealMeshes(const char *name, std::vector<Mesh *> &out, Vector3 &fore) {
    FILE *f = fopen(TextFormat("assets/guns/%s.mesh", name), "rb");
    if (!f) return false;
    int n = 0;
    bool ok = fread(&n, 4, 1, f) == 1 && fread(&fore, 4, 3, f) == 3 && n > 0 && n < 64;
    void ComputeTangents(Mesh &m, bool dxNormals);
    for (int i = 0; ok && i < n; i++) {
        int vc = 0, ic = 0;
        if (fread(&vc, 4, 1, f) != 1 || fread(&ic, 4, 1, f) != 1 || vc <= 0 || vc > 65535) { ok = false; break; }
        Mesh *m = new Mesh{};
        m->vertexCount = vc; m->triangleCount = ic ? ic / 3 : vc / 3;
        m->vertices = (float *)MemAlloc(vc * 12); m->normals = (float *)MemAlloc(vc * 12); m->texcoords = (float *)MemAlloc(vc * 8);
        ok = fread(m->vertices, 4, vc * 3, f) == (size_t)vc * 3 && fread(m->normals, 4, vc * 3, f) == (size_t)vc * 3 && fread(m->texcoords, 4, vc * 2, f) == (size_t)vc * 2;
        if (ok && ic) { m->indices = (unsigned short *)MemAlloc(ic * 2); ok = fread(m->indices, 2, ic, f) == (size_t)ic; }
        if (!ok) break;
        ComputeTangents(*m, true);   // normales en formato DirectX
        UploadMesh(m, false);
        out.push_back(m);
    }
    fclose(f);
    return ok;
}

// Manos con guante y antebrazos con manga (formas redondeadas).
// La mano derecha rodea la empuñadura (cerca de 0,-0.09,-0.02); la izquierda sujeta el guardamanos en "fore".
static void AddHands(GunModel &g, bool two, Vector3 fore) {
    g.handStart = g.parts.size();
    PMat *glove = &M("glove"), *sleeve = &M("sleeve");
    auto blob = [&](PMat *m, Vector3 s, Vector3 p, float rx = 0, float rz = 0) {
        Matrix t = MatrixScale(s.x, s.y, s.z);
        if (rx) t = MatrixMultiply(t, MatrixRotateX(rx));
        if (rz) t = MatrixMultiply(t, MatrixRotateZ(rz));
        g.parts.push_back({ &unitSphere, m, MatrixMultiply(t, MatrixTranslate(p.x, p.y, p.z)) });
    };
    auto limb = [&](PMat *m, Vector3 a, Vector3 b, float r) {   // cilindro de a hasta b
        Vector3 d = Vector3Subtract(b, a); float len = Vector3Length(d);
        Matrix rot = QuaternionToMatrix(QuaternionFromVector3ToVector3(Vector3{ 0, 1, 0 }, Vector3Scale(d, 1 / len)));
        Vector3 mid = Vector3Lerp(a, b, 0.5f);
        g.parts.push_back({ &unitCyl, m, MatrixMultiply(MatrixMultiply(MatrixScale(r * 2, len, r * 2), rot), MatrixTranslate(mid.x, mid.y, mid.z)) });
        blob(m, Vector3{ r * 2, r * 2, r * 2 }, a);   // codo/muñeca redondeados
    };
    // mano derecha
    blob(glove, Vector3{ 0.06f, 0.1f, 0.075f }, Vector3{ 0.012f, -0.1f, -0.015f }, 0.28f);        // palma
    blob(glove, Vector3{ 0.065f, 0.085f, 0.04f }, Vector3{ 0.002f, -0.105f, -0.055f }, 0.28f);    // dedos alrededor
    blob(glove, Vector3{ 0.03f, 0.03f, 0.065f }, Vector3{ -0.028f, -0.06f, -0.04f });             // pulgar
    limb(sleeve, Vector3{ 0.02f, -0.14f, 0.03f }, Vector3{ 0.1f, -0.36f, 0.42f }, 0.042f);        // antebrazo
    if (two) {
        blob(glove, Vector3{ 0.07f, 0.05f, 0.1f }, Vector3{ fore.x - 0.008f, fore.y - 0.042f, fore.z });                 // palma
        blob(glove, Vector3{ 0.026f, 0.06f, 0.095f }, Vector3{ fore.x + 0.03f, fore.y - 0.018f, fore.z - 0.005f }, 0, 0.15f); // dedos
        blob(glove, Vector3{ 0.024f, 0.04f, 0.07f }, Vector3{ fore.x - 0.036f, fore.y - 0.012f, fore.z + 0.02f });       // pulgar
        limb(sleeve, Vector3{ fore.x - 0.02f, fore.y - 0.07f, fore.z + 0.05f }, Vector3{ -0.2f, -0.42f, fore.z + 0.5f }, 0.04f);
    } else {
        // la izquierda sostiene por debajo a la derecha (agarre de pistola a dos manos)
        blob(glove, Vector3{ 0.065f, 0.07f, 0.09f }, Vector3{ -0.03f, -0.13f, -0.03f }, 0.3f);
        limb(sleeve, Vector3{ -0.045f, -0.16f, 0.0f }, Vector3{ -0.2f, -0.4f, 0.38f }, 0.04f);
    }
}

static bool BuildRealGun(int id) {
    if (!REAL_GUN[id]) return false;
    std::vector<Mesh *> meshes; Vector3 fore{};
    if (!LoadRealMeshes(REAL_GUN[id], meshes, fore)) { TraceLog(LOG_WARNING, "No se pudo cargar el arma %s", REAL_GUN[id]); return false; }
    GunModel &g = gunModels[id];
    PMat *mat = &GunMat(REAL_GUN[id], false);
    g.papMat = &GunMat(REAL_GUN[id], true);
    // boca del cañón: el punto más adelantado; mira: lo más alto
    float minZ = 0, maxY = -1, ySum = 0; int yN = 0;
    for (Mesh *m : meshes) for (int v = 0; v < m->vertexCount; v++) { minZ = std::min(minZ, m->vertices[v * 3 + 2]); maxY = std::max(maxY, m->vertices[v * 3 + 1]); }
    for (Mesh *m : meshes) for (int v = 0; v < m->vertexCount; v++) if (m->vertices[v * 3 + 2] < minZ + 0.015f) { ySum += m->vertices[v * 3 + 1]; yN++; }
    for (Mesh *m : meshes) g.parts.push_back({ m, mat, MatrixIdentity() });
    g.muzzleZ = minZ;
    g.muzzleY = yN ? ySum / yN : 0;
    g.sightY = maxY;
    g.viewScale = 0.95f;   // tamaño real: se dibujan algo más grandes que las de piezas
    AddHands(g, !OneHanded(id), fore);
    return true;
}

static void BuildGun(int id) {
    GunModel &g = gunModels[id];
    g.parts.clear();
    if (BuildRealGun(id)) return;
    float L = WEAPONS[id].len;
    auto box = [&](float w, float h, float d, const char *mat, float x, float y, float z, float rx = 0, float ry = 0) {
        Matrix m = MatrixScale(w, h, d);
        if (rx) m = MatrixMultiply(m, MatrixRotateX(rx));
        if (ry) m = MatrixMultiply(m, MatrixRotateY(ry));
        g.parts.push_back({ &unitCube, &M(mat), MatrixMultiply(m, MatrixTranslate(x, y, z)) });
    };
    auto cyl = [&](float r, float len, const char *mat, float x, float y, float z) {
        g.parts.push_back({ &unitCyl, &M(mat), MatrixMultiply(MatrixMultiply(MatrixScale(r * 2, len, r * 2), MatrixRotateX(PI_F / 2)), MatrixTranslate(x, y, z)) });
    };
    auto grip = [&](const char *mat, float z = -0.03f) { box(0.042f, 0.12f, 0.055f, mat, 0, -0.085f, z, 0.28f); box(0.012f, 0.035f, 0.06f, "polymer", 0, -0.04f, z - 0.05f); };
    switch (id) {
    case W_PISTOL:
        box(0.045f, 0.045f, 0.22f, "gunmetal", 0, 0.022f, -0.11f);            // corredera
        box(0.04f, 0.03f, 0.19f, "polymer", 0, -0.012f, -0.1f);                 // armazón
        for (int i = 0; i < 6; i++) box(0.047f, 0.03f, 0.004f, "darkmetal", 0, 0.025f, -0.01f - i * 0.008f);  // estrías
        cyl(0.009f, 0.03f, "darkmetal", 0, 0.022f, -0.225f);
        box(0.008f, 0.012f, 0.01f, "darkmetal", 0, 0.05f, -0.21f);              // mira delantera
        box(0.03f, 0.012f, 0.012f, "darkmetal", 0, 0.05f, -0.01f);              // mira trasera
        grip("polymer", -0.03f);
        g.muzzleZ = -0.24f;
        break;
    case W_MAGNUM:
        box(0.04f, 0.06f, 0.12f, "metal", 0, 0.01f, -0.05f);
        cyl(0.042f, 0.065f, "metal", 0, 0.005f, -0.085f);
        cyl(0.013f, 0.2f, "metal", 0, 0.02f, -0.22f);
        box(0.014f, 0.022f, 0.2f, "metal", 0, 0.035f, -0.22f);
        box(0.008f, 0.015f, 0.012f, "darkmetal", 0, 0.052f, -0.31f);
        box(0.042f, 0.12f, 0.055f, "wood", 0, -0.075f, 0.0f, 0.35f);
        g.muzzleZ = -0.33f;
        break;
    case W_SHOTGUN:
        box(0.06f, 0.08f, 0.24f, "gunmetal", 0, 0, -0.1f);
        cyl(0.019f, 0.52f, "darkmetal", 0, 0.025f, -0.46f);
        cyl(0.016f, 0.4f, "gunmetal", 0, -0.018f, -0.4f);
        box(0.062f, 0.055f, 0.2f, "wood", 0, -0.02f, -0.45f);                   // corredera de madera
        for (int i = 0; i < 5; i++) box(0.064f, 0.058f, 0.008f, "darkmetal", 0, -0.02f, -0.45f + (i - 2) * 0.035f);  // estrías
        box(0.055f, 0.09f, 0.3f, "wood", 0, -0.035f, 0.17f, -0.12f);            // culata
        box(0.058f, 0.1f, 0.02f, "polymer", 0, -0.06f, 0.32f, -0.12f);          // cantonera
        grip("wood", -0.02f);
        box(0.008f, 0.012f, 0.01f, "brass", 0, 0.048f, -0.7f);
        g.muzzleZ = -0.73f;
        break;
    case W_SMG:
        box(0.06f, 0.08f, 0.3f, "gunmetal", 0, 0, -0.14f);
        box(0.062f, 0.02f, 0.26f, "darkmetal", 0, 0.05f, -0.14f);               // riel
        cyl(0.015f, 0.14f, "darkmetal", 0, 0.01f, -0.36f);
        cyl(0.022f, 0.06f, "darkmetal", 0, 0.01f, -0.44f);                     // supresor corto
        box(0.03f, 0.18f, 0.05f, "polymer", 0, -0.12f, -0.2f, 0.12f);           // cargador
        box(0.012f, 0.012f, 0.22f, "darkmetal", 0.02f, -0.01f, 0.1f);           // culata plegable
        box(0.012f, 0.012f, 0.22f, "darkmetal", -0.02f, -0.01f, 0.1f);
        box(0.05f, 0.07f, 0.015f, "darkmetal", 0, -0.02f, 0.21f);
        grip("polymer", -0.02f);
        box(0.02f, 0.025f, 0.03f, "darkmetal", 0, 0.072f, -0.05f);
        g.muzzleZ = -0.48f;
        break;
    case W_RIFLE:
        box(0.06f, 0.085f, 0.32f, "gunmetal", 0, 0, -0.15f);
        box(0.068f, 0.072f, 0.24f, "polymer", 0, 0.002f, -0.42f);               // guardamanos
        for (int i = 0; i < 6; i++) box(0.07f, 0.01f, 0.018f, "darkmetal", 0, 0.03f, -0.33f - i * 0.035f);
        cyl(0.011f, 0.2f, "darkmetal", 0, 0.01f, -0.63f);
        cyl(0.018f, 0.05f, "darkmetal", 0, 0.01f, -0.74f);                     // bocacha
        box(0.012f, 0.05f, 0.012f, "darkmetal", 0, 0.05f, -0.52f);              // mira delantera
        cyl(0.022f, 0.15f, "darkmetal", 0, 0.088f, -0.17f);                    // mira óptica
        cyl(0.026f, 0.025f, "darkmetal", 0, 0.088f, -0.25f);
        box(0.02f, 0.03f, 0.04f, "darkmetal", 0, 0.06f, -0.14f);
        box(0.036f, 0.18f, 0.07f, "polymer", 0, -0.12f, -0.2f, 0.28f);          // cargador curvo
        box(0.05f, 0.08f, 0.26f, "polymer", 0, -0.02f, 0.13f);                  // culata
        box(0.055f, 0.1f, 0.03f, "polymer", 0, -0.03f, 0.26f);
        grip("polymer", -0.02f);
        g.muzzleZ = -0.78f;
        break;
    case W_LMG:
        box(0.08f, 0.1f, 0.36f, "gunmetal", 0, 0, -0.16f);
        cyl(0.03f, 0.3f, "darkmetal", 0, 0.01f, -0.48f);                       // camisa del cañón
        for (int i = 0; i < 5; i++) cyl(0.032f, 0.012f, "gunmetal", 0, 0.01f, -0.38f - i * 0.05f);
        cyl(0.012f, 0.14f, "darkmetal", 0, 0.01f, -0.68f);
        box(0.11f, 0.12f, 0.13f, "olive", 0.02f, -0.12f, -0.16f);               // caja de munición
        box(0.02f, 0.06f, 0.12f, "darkmetal", 0, 0.08f, -0.15f);                // asa
        box(0.04f, 0.015f, 0.12f, "darkmetal", 0, 0.115f, -0.15f);
        box(0.012f, 0.18f, 0.012f, "darkmetal", 0.03f, -0.09f, -0.6f, 0.4f);    // bípode
        box(0.012f, 0.18f, 0.012f, "darkmetal", -0.03f, -0.09f, -0.6f, 0.4f);
        box(0.06f, 0.1f, 0.25f, "polymer", 0, -0.02f, 0.14f);
        grip("polymer", -0.0f);
        g.muzzleZ = -0.76f;
        break;
    case W_SNIPER:
        box(0.055f, 0.07f, 0.62f, "olive", 0, -0.01f, -0.15f);                  // cuerpo/culata
        box(0.05f, 0.11f, 0.2f, "olive", 0, -0.04f, 0.18f, -0.08f);
        box(0.05f, 0.06f, 0.24f, "gunmetal", 0, 0.03f, -0.12f);                 // cajón de mecanismos
        cyl(0.013f, 0.42f, "darkmetal", 0, 0.03f, -0.62f);
        cyl(0.02f, 0.06f, "darkmetal", 0, 0.03f, -0.85f);
        cyl(0.03f, 0.34f, "darkmetal", 0, 0.1f, -0.15f);                       // mira telescópica
        cyl(0.036f, 0.07f, "darkmetal", 0, 0.1f, -0.33f);
        cyl(0.034f, 0.05f, "darkmetal", 0, 0.1f, 0.03f);
        box(0.015f, 0.04f, 0.015f, "darkmetal", 0, 0.065f, -0.22f);
        box(0.015f, 0.04f, 0.015f, "darkmetal", 0, 0.065f, -0.06f);
        box(0.05f, 0.012f, 0.012f, "brass", 0.035f, 0.035f, -0.03f);            // cerrojo
        box(0.03f, 0.06f, 0.07f, "darkmetal", 0, -0.07f, -0.14f);
        grip("olive", 0.0f);
        g.muzzleZ = -0.9f;
        break;
    case W_RAYGUN:
        box(0.05f, 0.07f, 0.18f, "gunmetal", 0, 0.01f, -0.08f);
        for (int i = 0; i < 3; i++) cyl(0.055f, 0.022f, "raygunred", 0, 0.012f, -0.1f - i * 0.065f);
        cyl(0.03f, 0.22f, "gunmetal", 0, 0.012f, -0.17f);
        cyl(0.018f, 0.06f, "wunderblue", 0, 0.012f, -0.3f);
        box(0.042f, 0.11f, 0.05f, "raygunred", 0, -0.07f, 0.0f, 0.3f);
        g.muzzleZ = -0.33f;
        break;
    case W_WUNDER:
        box(0.07f, 0.08f, 0.36f, "gunmetal", 0, 0, -0.16f);
        for (int i = 0; i < 4; i++) cyl(0.05f, 0.025f, "brass", 0, 0.01f, -0.12f - i * 0.07f);
        cyl(0.035f, 0.12f, "wunderblue", 0, 0.07f, -0.2f);                     // bulbo de energía
        cyl(0.012f, 0.16f, "brass", 0, 0.01f, -0.42f);
        box(0.055f, 0.09f, 0.22f, "wood", 0, -0.03f, 0.12f);
        grip("wood", -0.02f);
        g.muzzleZ = -0.5f;
        break;
    case W_LAUNCHER:
        box(0.07f, 0.09f, 0.3f, "olive", 0, 0, -0.12f);
        cyl(0.045f, 0.34f, "darkmetal", 0, 0.035f, -0.36f);                    // cañón gordo
        cyl(0.05f, 0.03f, "gunmetal", 0, 0.035f, -0.53f);
        cyl(0.085f, 0.14f, "gunmetal", 0, -0.025f, -0.17f);                    // tambor
        for (int i = 0; i < 6; i++) {
            float a = i * PI_F / 3;
            cyl(0.02f, 0.145f, "darkmetal", cosf(a) * 0.055f, -0.025f + sinf(a) * 0.055f, -0.17f);
        }
        box(0.05f, 0.1f, 0.08f, "polymer", 0, -0.09f, -0.36f, 0.2f);            // empuñadura delantera
        box(0.06f, 0.08f, 0.22f, "polymer", 0, -0.02f, 0.12f);
        grip("polymer", -0.02f);
        box(0.02f, 0.05f, 0.02f, "darkmetal", 0, 0.1f, -0.1f);
        g.muzzleZ = -0.55f;
        break;
    case W_SAWED:   // escopeta de dos cañones recortada
        for (float x : { -0.017f, 0.017f }) cyl(0.017f, 0.36f, "darkmetal", x, 0.02f, -0.27f);
        box(0.07f, 0.05f, 0.1f, "gunmetal", 0, 0.01f, -0.06f);                 // báscula
        box(0.03f, 0.01f, 0.34f, "gunmetal", 0, 0.042f, -0.27f);                // banda entre cañones
        box(0.06f, 0.04f, 0.2f, "wood", 0, -0.02f, -0.22f);                     // guardamanos
        box(0.05f, 0.11f, 0.12f, "wood", 0, -0.05f, 0.07f, -0.5f);              // culata cortada (pistolete)
        box(0.012f, 0.03f, 0.03f, "brass", 0, 0.05f, -0.04f);                   // palanca de apertura
        grip("wood", -0.0f);
        g.muzzleZ = -0.45f;
        break;
    case W_THUNDER: {   // Cañón de Trueno: cuerpo grueso, turbinas y boca acampanada
        box(0.11f, 0.12f, 0.34f, "gunmetal", 0, 0.0f, -0.12f);
        for (int i = 0; i < 4; i++) cyl(0.075f + i * 0.012f, 0.03f, i % 2 ? "brass" : "darkmetal", 0, 0.01f, -0.31f - i * 0.04f);
        cyl(0.13f, 0.04f, "brass", 0, 0.01f, -0.47f);                          // boca acampanada
        cyl(0.115f, 0.045f, "darkmetal", 0, 0.01f, -0.47f);
        for (int s = -1; s <= 1; s += 2) {
            cyl(0.03f, 0.26f, "wunderblue", s * 0.07f, 0.05f, -0.15f);          // depósitos de energía
            cyl(0.034f, 0.03f, "brass", s * 0.07f, 0.05f, -0.29f);
            cyl(0.034f, 0.03f, "brass", s * 0.07f, 0.05f, -0.01f);
        }
        box(0.04f, 0.05f, 0.18f, "brass", 0, 0.085f, -0.13f);                   // asa superior
        box(0.06f, 0.09f, 0.18f, "wood", 0, -0.03f, 0.13f);                     // culata
        box(0.05f, 0.1f, 0.06f, "wood", 0, -0.1f, -0.3f, 0.2f);                 // empuñadura delantera
        grip("wood", -0.0f);
        g.muzzleZ = -0.5f;
        break;
    }
    case W_RPG: {
        cyl(0.055f, 0.85f, "olive", 0, 0.03f, -0.4f);                          // tubo al hombro
        cyl(0.064f, 0.06f, "darkmetal", 0, 0.03f, -0.1f);
        cyl(0.068f, 0.08f, "darkmetal", 0, 0.03f, -0.8f);
        g.parts.push_back({ &coneMesh, &M("olive"), MatrixMultiply(MatrixMultiply(MatrixScale(0.075f, 0.24f, 0.075f), MatrixRotateX(-PI_F / 2)), MatrixTranslate(0, 0.03f, -0.82f)) });
        box(0.05f, 0.13f, 0.06f, "polymer", 0, -0.08f, -0.05f, 0.25f);
        box(0.05f, 0.12f, 0.06f, "polymer", 0, -0.08f, -0.35f, 0.25f);
        box(0.03f, 0.06f, 0.1f, "darkmetal", -0.07f, 0.08f, -0.3f);             // mira
        g.muzzleZ = -1.0f;
        break;
    }
    }
    // manos y mangas
    bool two = !OneHanded(id);
    float fz = id == W_RPG ? -0.35f : id == W_THUNDER ? -0.3f : -L * 0.72f;
    AddHands(g, two, Vector3{ 0, -0.015f, fz });
}

static void BuildKnife();
void BuildGunModels() {
    void ComputeTangents(Mesh &m, bool dxNormals);
    coneMesh = GenMeshCone(1.0f, 1.0f, 14);
    ComputeTangents(coneMesh, false);
    for (int i = 0; i < W_COUNT; i++) BuildGun(i);
    BuildKnife();
    Vector3 dummy;
    if (LoadRealMeshes("grenade", grenadeMeshes, dummy)) grenadeMat = &GunMat("grenade", false);
}

// Las piezas de las manos (glove/sleeve) van al final, desde handStart
void DrawGun(const GunModel &g, Matrix tf, bool papd, bool hands) {
    PMat *gm = &M("gunmetal"), *ol = &M("olive"), *camo = &M("camo");
    size_t n = hands ? g.parts.size() : g.handStart;
    for (size_t i = 0; i < n; i++) {
        const GunPart &p = g.parts[i];
        PMat *m = p.mat;
        if (papd && i < g.handStart) {
            if (g.papMat) m = g.papMat;
            else if (m == gm || m == ol) m = camo;
        }
        DrawPM(*p.mesh, *m, MatrixMultiply(p.local, tf));
    }
}

// ---------------------------------------------------------------
// Efectos: partículas, trazadoras, explosiones, humo y marcas
// ---------------------------------------------------------------
struct Particle { Vector3 p, v; float life, max, size, glow; Color c; bool additive; float grav; };
struct Tracer { std::vector<Vector3> pts; Color c; float t, life; };
struct Boom { Vector3 p; float t, size; };
struct Smoke { Vector3 p; float t, life, size; };
struct Decal { int type; Vector3 p, n; float size, rot; };
static std::vector<Particle> particles;
static std::vector<Tracer> tracers;
static std::vector<Boom> booms;
static std::vector<Smoke> smokes;
static std::vector<Decal> decals;
Vector3 boomLightPos{}; float boomLightI = 0;


void Burst(Vector3 p, Color c, int n, float speed, float size, float glow) {
    for (int i = 0; i < n; i++) {
        Particle q{ p, Vector3{ frand(-1, 1) * speed, frand(0, 1.5f) * speed, frand(-1, 1) * speed }, 0, frand(0.3f, 0.6f), size * frand(0.7f, 1.4f), glow, c, glow > 1.5f, 9.8f };
        q.max = q.life = frand(0.3f, 0.65f);
        particles.push_back(q);
    }
}
static void AddTracer(std::vector<Vector3> pts, Color c, float life) { tracers.push_back({ pts, c, life, life }); }

void AddDecal(int type, Vector3 p, Vector3 n) {
    float size = type == 0 ? 0.12f : type == 1 ? frand(1.2f, 1.8f) : 3.0f;
    decals.push_back({ type, Vector3Add(p, Vector3Scale(n, 0.006f + decals.size() * 0.00002f)), n, size, frand(0, 6.28f) });
    if (decals.size() > 90) decals.erase(decals.begin());
}

static std::vector<Vector3> Jagged(Vector3 a, Vector3 b) {
    std::vector<Vector3> pts{ a };
    for (int i = 1; i < 8; i++) {
        Vector3 p = Vector3Lerp(a, b, i / 8.0f);
        p.x += frand(-0.25f, 0.25f); p.y += frand(-0.25f, 0.25f); p.z += frand(-0.25f, 0.25f);
        pts.push_back(p);
    }
    pts.push_back(b);
    return pts;
}

// ---------------------------------------------------------------
// Disparo
// ---------------------------------------------------------------
static bool RayWorld(Ray r, float maxD, float &dist, Vector3 &point, Vector3 &normal) {
    dist = maxD;
    bool hit = false;
    for (auto &b : solids) {
        RayCollision c = GetRayCollisionBox(r, b);
        if (c.hit && c.distance < dist) { dist = c.distance; point = c.point; normal = c.normal; hit = true; }
    }
    // suelo y techo
    if (r.direction.y < -1e-4f) {
        float t = -r.position.y / r.direction.y;
        if (t > 0 && t < dist) { dist = t; point = Vector3Add(r.position, Vector3Scale(r.direction, t)); normal = Vector3{ 0, 1, 0 }; hit = true; }
    } else if (r.direction.y > 1e-4f) {
        float t = (H - r.position.y) / r.direction.y;
        Vector3 p = Vector3Add(r.position, Vector3Scale(r.direction, t));
        if (t > 0 && t < dist && RoomAt(p.x, p.z) >= 0) { dist = t; point = p; normal = Vector3{ 0, -1, 0 }; hit = true; }
    }
    return hit;
}

static void ChainLightning(int first, int count) {
    std::vector<int> hit;
    int cur = first;
    for (int i = 0; i <= count && cur >= 0; i++) {
        hit.push_back(cur);
        Vector3 from = zombies[cur].pos; from.y = 1.3f;
        Vector3 curPos = zombies[cur].pos;
        KillZombie(zombies[cur], false);
        Burst(from, Color{ 150, 220, 255, 255 }, 8, 3, 0.05f, 4);
        int next = -1; float nd = 7;
        for (size_t o = 0; o < zombies.size(); o++) {
            if (zombies[o].state == Z_DEAD || std::find(hit.begin(), hit.end(), (int)o) != hit.end()) continue;
            float d = Vector3Distance(zombies[o].pos, curPos);
            if (d < nd) { nd = d; next = (int)o; }
        }
        if (next >= 0) { Vector3 to = zombies[next].pos; to.y = 1.3f; AddTracer(Jagged(from, to), Color{ 150, 220, 255, 255 }, 0.3f); }
        cur = next;
    }
    ShowHit(true);
}

struct Projectile { Vector3 p, v; bool rocket; float t, smokeT, dmg, radius; bool flare = false; };
struct FirePatch { Vector3 p; float t, radius, dps; };   // fuego de una bengala en el suelo
static std::vector<FirePatch> firePatches;
static std::vector<Projectile> projectiles;
struct Grenade { Vector3 p, v; float t, rot; };
static std::vector<Grenade> grenades;
void ClearEffects() { particles.clear(); tracers.clear(); booms.clear(); smokes.clear(); decals.clear(); firePatches.clear(); projectiles.clear(); grenades.clear(); }

static void LaunchProjectile(const WStats &s, float dmgMul) {
    bool rocket = s.projectile == 2;
    Vector3 dir = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    float spread = vm.ads > 0.5f ? 0 : 0.01f;
    dir.x += frand(-spread, spread); dir.y += frand(-spread, spread);
    dir = Vector3Normalize(dir);
    Vector3 p = Vector3Add(cam.position, Vector3Scale(dir, 0.7f));
    p.y -= 0.12f;
    bool flare = s.projectile == 3;
    projectiles.push_back({ p, Vector3Scale(dir, rocket ? 34.0f : flare ? 32.0f : 26.0f), rocket, 0, 0, s.dmg * dmgMul * (1 + game.round * 0.1f), s.radius, flare });
}

// Prende fuego a los zombis cercanos (daño por segundo durante unos segundos)
static void Ignite(Vector3 p, float radius, float dps) {
    for (auto &z : zombies) {
        if (z.state == Z_DEAD || (z.state == Z_RISE && z.y < -1.2f)) continue;
        if (hyp(z.pos.x - p.x, z.pos.z - p.z) < radius) { z.burnT = 4.0f; z.burnDps = std::max(z.burnDps, dps); }
    }
}

// Cañón de Trueno: una ráfaga de aire en cono que mata y lanza por los aires a todo lo que pilla
static void ThunderBlast(float range) {
    Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    Vector3 flat = Vector3Normalize(Vector3{ fwd.x, 0, fwd.z });
    int killed = 0;
    for (auto &z : zombies) {
        if (z.state == Z_DEAD || (z.state == Z_RISE && z.y < -1.2f)) continue;
        Vector3 c{ z.pos.x, z.y + 1.0f, z.pos.z };
        Vector3 d = Vector3Subtract(c, cam.position);
        float dist = Vector3Length(d);
        if (dist > range || dist < 0.01f) continue;
        if (dist > 2.5f && Vector3DotProduct(Vector3Scale(d, 1 / dist), fwd) < 0.78f) continue;
        // no atraviesa paredes
        float wd; Vector3 wp, wn;
        if (RayWorld(Ray{ cam.position, Vector3Scale(d, 1 / dist) }, dist, wd, wp, wn) && wd < dist - 0.6f) continue;
        Vector3 push = Vector3Normalize(Vector3{ d.x, 0, d.z });
        push = Vector3Normalize(Vector3Add(Vector3Scale(push, 0.6f), Vector3Scale(flat, 0.4f)));
        float k = 1 - dist / range;
        KillZombie(z, false);
        z.fling = Vector3{ push.x * (7 + 13 * k), 4.5f + 5 * k, push.z * (7 + 13 * k) };
        Burst(c, Color{ 120, 0, 0, 255 }, 8, 2.5f, 0.06f);
        killed++;
    }
    // la onda: polvo y destellos que salen en abanico
    for (int i = 0; i < 70; i++) {
        float a = frand(-0.6f, 0.6f), sp = frand(10, 26);
        Vector3 dir{ flat.x * cosf(a) - flat.z * sinf(a), frand(-0.15f, 0.35f) + fwd.y, flat.x * sinf(a) + flat.z * cosf(a) };
        Vector3 start = Vector3Add(cam.position, Vector3Add(Vector3Scale(fwd, 0.8f), Vector3{ 0, -0.2f, 0 }));
        bool spark = i % 3 == 0;
        Particle q{ start, Vector3Scale(dir, sp), 0, 0, spark ? 0.035f : frand(0.06f, 0.12f), spark ? 4.0f : 1.0f,
                    spark ? Color{ 150, 200, 255, 255 } : Color{ 95, 100, 110, 255 }, spark, 0.5f };
        q.max = q.life = frand(0.35f, 0.6f);
        particles.push_back(q);
    }
    for (int i = 0; i < 8; i++) {
        Vector3 sp = Vector3Add(cam.position, Vector3Scale(fwd, 1.5f + i * 1.3f));
        smokes.push_back({ Vector3Add(sp, Vector3{ frand(-0.5f, 0.5f), -0.3f, frand(-0.5f, 0.5f) }), -i * 0.03f, 0.8f, 1.2f + i * 0.25f });
    }
    vm.shake = std::max(vm.shake, 0.45f);
    PushProps(cam.position, range, 1.6f, flat);
    WhiteFlash(0.25f);
    player.vel.x -= flat.x * 5; player.vel.z -= flat.z * 5;   // el retroceso te empuja un poco
    if (killed) ShowHit(true);
}

void Fire() {
    OwnedWeapon &w = CurWeapon();
    WStats s = Stats(w.id, w.pap);
    bool dt2 = HasPerk(player, P_DOUBLE);
    w.mag--;
    player.nextShot = T + s.rate * (dt2 ? 0.7f : 1);
    if (GUN_SND[w.id] && HasSound(TextFormat("g_%s", GUN_SND[w.id]))) {
        Play(TextFormat("g_%s", GUN_SND[w.id]), 0.9f);    // disparo grabado
        Play(SOUND_NAMES[s.sound], 0.3f);                  // más el golpe grave sintetizado, para dar cuerpo
    } else Play(SOUND_NAMES[s.sound]);
    vm.kick = std::min(1.0f, vm.kick + ((s.pellets > 1 || w.id == W_SNIPER) ? 1.0f : 0.45f));
    vm.flash = 0.05f;
    if (s.projectile) vm.shake = std::max(vm.shake, s.projectile == 2 ? 0.25f : 0.12f);
    bool green = w.id == W_RAYGUN || w.id == W_WUNDER;
    vm.flashColor = w.id == W_WUNDER ? 0x88ccff : green ? 0x55ff55 : 0xffaa55;
    vm.recoil += s.recoil * (vm.ads > 0.5f ? 0.5f : 1);
    player.yaw += frand(-0.3f, 0.3f) * s.recoil;
    if (w.id == W_FLARE) vm.flashColor = 0xff3322;
    if (s.projectile == 4) {   // Cañón de Trueno: no lanza nada, es una onda de aire
        vm.flashColor = 0xbbddff;
        ThunderBlast(s.radius);
        if (w.mag <= 0 && w.reserve > 0) StartReload();
        return;
    }

    if (s.projectile) {
        LaunchProjectile(s, dt2 ? 1.25f : 1);
        if (w.mag <= 0 && w.reserve > 0) StartReload();
        return;
    }
    float spread = s.spread * (vm.ads > 0.5f ? 0.35f : 1) * (player.moving ? 1.4f : 1) * (player.crouch ? 0.7f : 1);
    Vector3 muzzle = Vector3Transform(Vector3{ 0, gunModels[w.id].muzzleY, gunModels[w.id].muzzleZ }, gunWorld);
    float dmgMul = dt2 ? 1.25f : 1;
    Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    for (int i = 0; i < s.pellets; i++) {
        Vector3 dir = fwd;
        dir.x += frand(-spread, spread); dir.y += frand(-spread, spread); dir.z += frand(-spread, spread);
        dir = Vector3Normalize(dir);
        Ray r{ cam.position, dir };
        float wd; Vector3 wp, wn;
        bool wh = RayWorld(r, 150, wd, wp, wn);
        int zi; float zd; bool head; Vector3 zp;
        bool zh = RayZombie(r, wh ? wd : 150, zi, zd, head, zp);
        Vector3 hitPoint;
        if (zh) {
            hitPoint = zp;
            if (s.chain) ChainLightning(zi, s.chain);
            else DamageZombie(zombies[zi], s.dmg * dmgMul, s.head, head, zp, green);
        } else if (wh) {
            hitPoint = wp;
            Burst(Vector3Add(wp, Vector3Scale(wn, 0.02f)), green ? Color{ 100, 255, 100, 255 } : Color{ 255, 210, 122, 255 }, 3, 2, 0.03f, 6);
            Burst(Vector3Add(wp, Vector3Scale(wn, 0.02f)), Color{ 90, 85, 80, 255 }, 3, 1.2f, 0.06f, 1);
            if (!green) AddDecal(0, wp, wn);
            PokeProp(wp, dir);
        } else hitPoint = Vector3Add(cam.position, Vector3Scale(dir, 60));
        if (s.splash && (zh || wh)) {
            Burst(hitPoint, Color{ 100, 255, 100, 255 }, 10, 3, 0.05f, 4);
            for (size_t o = 0; o < zombies.size(); o++) {
                Zombie &z = zombies[o];
                if (z.state == Z_DEAD || (zh && (int)o == zi)) continue;
                if (hyp(z.pos.x - hitPoint.x, z.pos.z - hitPoint.z) < s.splash) DamageZombie(z, s.dmg * 0.5f * dmgMul, 1, false, hitPoint, true);
            }
        }
        if (s.tracer) AddTracer({ muzzle, hitPoint }, Hex(s.tracer), 0.15f);
        else if (s.chain) AddTracer(Jagged(muzzle, hitPoint), Color{ 150, 220, 255, 255 }, 0.25f);
        else if (frand(0, 1) < 0.35f) AddTracer({ muzzle, hitPoint }, Color{ 255, 200, 120, 255 }, 0.05f);
    }
    if (w.mag <= 0 && w.reserve > 0) StartReload();
}

// ---------------------------------------------------------------
// Granadas y proyectiles
// ---------------------------------------------------------------
void ThrowGrenade() {
    if (player.grenades <= 0 || T < player.nextGrenade || player.papBusy) return;
    player.grenades--;
    player.nextGrenade = T + 0.8f;
    Vector3 dir = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    Vector3 v = Vector3Scale(dir, 15);
    v.y += 3.5f; v.x += player.vel.x; v.z += player.vel.z;
    grenades.push_back({ Vector3Add(cam.position, Vector3Scale(dir, 0.5f)), v, 0, 0 });
    vm.throwT = 0.4f;
    Play("pin");
}

// dmg: daño en el centro · radius: alcance · selfDmg: daño máximo al jugador si está cerca
void Explode(Vector3 p, float dmg, float radius, float selfDmg, float size) {
    Play("boom");
    PushProps(p, radius * 1.3f, std::min(size, 1.5f));
    boomLightPos = Vector3{ p.x, std::max(p.y, 1.5f), p.z };
    boomLightI = 40 * size;
    booms.push_back({ p, 0, size });
    for (int i = 0; i < (int)(10 * size); i++) smokes.push_back({ Vector3Add(p, Vector3{ frand(-1, 1) * size, frand(0, 1), frand(-1, 1) * size }), -frand(0, 0.3f), frand(1.5f, 2.6f), frand(1.5f, 2.5f) * size });
    Burst(p, Color{ 255, 170, 51, 255 }, (int)(25 * size), 6 * size, 0.06f, 6);
    Burst(p, Color{ 60, 60, 60, 255 }, (int)(15 * size), 3, 0.08f, 1);
    AddDecal(2, Vector3{ p.x, 0.0f, p.z }, Vector3{ 0, 1, 0 });
    for (auto &z : zombies) {
        if (z.state == Z_DEAD) continue;
        float d = hyp(z.pos.x - p.x, z.pos.z - p.z);
        if (d >= radius) continue;
        DamageZombie(z, dmg * (1 - d / (radius * 1.08f)), 1, false, Vector3{ z.pos.x, 1.2f, z.pos.z }, false);
        // la onda empuja el cuerpo hacia fuera y hacia arriba
        Vector3 out = d > 0.05f ? Vector3{ (z.pos.x - p.x) / d, 0, (z.pos.z - p.z) / d } : Vector3{ 0, 0, 0 };
        z.hitDir = Vector3Normalize(Vector3{ out.x, 0.9f, out.z });
        z.hitForce = 4 + 8 * (1 - d / radius) * std::min(size, 1.5f);
        z.hitAll = true;
    }
    float pd = hyp(player.pos.x - p.x, player.pos.z - p.z);
    vm.shake = std::max(vm.shake, std::max(0.0f, 0.5f * size - pd * 0.04f));
    if (pd < 4 && selfDmg > 0) DamagePlayer((int)roundf(selfDmg * (1 - pd / 4)));
}

void UpdateProjectiles(float dt) {
    for (int i = (int)grenades.size() - 1; i >= 0; i--) {
        Grenade &g = grenades[i];
        g.t += dt;
        g.v.y -= 18 * dt;
        float nx = g.p.x + g.v.x * dt;
        if (InsideCollider(nx, g.p.z, 0.08f)) g.v.x *= -0.5f; else g.p.x = nx;
        float nz = g.p.z + g.v.z * dt;
        if (InsideCollider(g.p.x, nz, 0.08f)) g.v.z *= -0.5f; else g.p.z = nz;
        g.p.y += g.v.y * dt;
        if (g.p.y < 0.08f) { g.p.y = 0.08f; g.v.y = fabsf(g.v.y) > 1.5f ? -g.v.y * 0.4f : 0; g.v.x *= 0.7f; g.v.z *= 0.7f; }
        if (g.p.y > H - 0.1f) { g.p.y = H - 0.1f; g.v.y = -fabsf(g.v.y) * 0.4f; }
        g.rot += dt * 10;
        if (g.t > 2.2f) { Explode(g.p, 400.0f + game.round * 80, 6, 30, 1); grenades.erase(grenades.begin() + i); }
    }
    for (int i = (int)projectiles.size() - 1; i >= 0; i--) {
        Projectile &pr = projectiles[i];
        pr.t += dt;
        if (!pr.rocket) pr.v.y -= (pr.flare ? 3 : 9) * dt;
        bool hit = pr.t > 5;
        for (int k = 0; k < 3 && !hit; k++) {
            pr.p = Vector3Add(pr.p, Vector3Scale(pr.v, dt / 3));
            if (pr.p.y < 0.05f || pr.p.y > H - 0.05f) hit = true;
            else {
                for (auto &b : solids) if (pr.p.x > b.min.x && pr.p.x < b.max.x && pr.p.z > b.min.z && pr.p.z < b.max.z && pr.p.y > b.min.y && pr.p.y < b.max.y) { hit = true; break; }
                if (!hit) for (auto &z : zombies) {
                    if (z.state == Z_DEAD || z.state == Z_RISE) continue;
                    if (pr.p.y < 2.0f && hyp(z.pos.x - pr.p.x, z.pos.z - pr.p.z) < 0.5f) { hit = true; break; }
                }
            }
        }
        if (pr.flare) {   // estela de chispas rojas
            pr.smokeT -= dt;
            if (pr.smokeT <= 0) { pr.smokeT = 0.015f; Burst(pr.p, Color{ 255, 80, 40, 255 }, 1, 0.4f, 0.05f, 5); }
        }
        if (pr.rocket) {
            pr.smokeT -= dt;
            if (pr.smokeT <= 0) { pr.smokeT = 0.02f; smokes.push_back({ pr.p, 0, 0.9f, 0.3f }); }
        }
        if (hit) {
            Vector3 p = pr.p;
            p.y = clampf(p.y, 0.2f, H - 0.3f);
            if (pr.flare) {   // bengala: incendia a los zombis de alrededor y deja fuego en el suelo
                Play("flareburst");
                Burst(p, Color{ 255, 120, 40, 255 }, 30, 4, 0.06f, 6);
                Burst(p, Color{ 255, 60, 30, 255 }, 15, 2, 0.1f, 5);
                boomLightPos = Vector3{ p.x, std::max(p.y, 1.0f), p.z }; boomLightI = 25;
                for (auto &z : zombies) if (z.state != Z_DEAD && hyp(z.pos.x - p.x, z.pos.z - p.z) < 0.9f) DamageZombie(z, pr.dmg * 0.5f, 1, false, Vector3{ z.pos.x, 1.2f, z.pos.z }, false);
                Ignite(p, pr.radius, pr.dmg * 0.4f);
                firePatches.push_back({ Vector3{ p.x, 0.05f, p.z }, 0, pr.radius * 0.75f, pr.dmg * 0.4f });
                AddDecal(2, Vector3{ p.x, 0.0f, p.z }, Vector3{ 0, 1, 0 });
                projectiles.erase(projectiles.begin() + i);
                continue;
            }
            Explode(p, pr.dmg, pr.radius, 0, pr.rocket ? 1.5f : 1.1f);
            projectiles.erase(projectiles.begin() + i);
        }
    }
    // fuego de bengalas en el suelo: quema a quien lo pise
    for (int i = (int)firePatches.size() - 1; i >= 0; i--) {
        FirePatch &f = firePatches[i];
        f.t += dt;
        if (f.t > 6) { firePatches.erase(firePatches.begin() + i); continue; }
        Ignite(f.p, f.radius, f.dps);
        if (frand(0, 1) < dt * 40) {
            float a = frand(0, 6.28f), r = frand(0, f.radius);
            Particle q{ Vector3{ f.p.x + cosf(a) * r, 0.1f, f.p.z + sinf(a) * r }, Vector3{ frand(-0.3f, 0.3f), frand(1.0f, 2.2f), frand(-0.3f, 0.3f) }, 0, 0, frand(0.08f, 0.16f), 5,
                        frand(0, 1) < 0.5f ? Color{ 255, 140, 40, 255 } : Color{ 255, 70, 20, 255 }, true, -0.5f };
            q.max = q.life = frand(0.4f, 0.8f);
            particles.push_back(q);
        }
    }
}

void DrawProjectiles() {
    for (auto &g : grenades) {
        if (grenadeMat) {   // granada real (centrada en su mitad, un poco más grande para que se vea)
            Matrix tf = MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixTranslate(0, -0.05f, 0), MatrixScale(1.6f, 1.6f, 1.6f)), MatrixRotateX(g.rot)), MatrixTranslate(g.p.x, g.p.y, g.p.z));
            for (Mesh *m : grenadeMeshes) DrawPM(*m, *grenadeMat, tf);
        } else DrawPM(unitSphere, M("olive"), MatrixMultiply(MatrixMultiply(MatrixScale(0.16f, 0.2f, 0.16f), MatrixRotateX(g.rot)), MatrixTranslate(g.p.x, g.p.y, g.p.z)));
    }
    for (auto &pr : projectiles) {
        Vector3 d = Vector3Normalize(pr.v);
        float yaw = atan2f(-d.x, -d.z), pitch = asinf(clampf(d.y, -1, 1));
        Matrix rot = MatrixMultiply(MatrixRotateX(pitch), MatrixRotateY(yaw));
        if (pr.rocket) DrawPM(coneMesh, M("olive"), MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixScale(0.07f, 0.28f, 0.07f), MatrixRotateX(-PI_F / 2)), rot), MatrixTranslate(pr.p.x, pr.p.y, pr.p.z)));
        else DrawPM(unitCyl, M("brass"), MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixScale(0.12f, 0.14f, 0.12f), MatrixRotateX(PI_F / 2)), rot), MatrixTranslate(pr.p.x, pr.p.y, pr.p.z)));
    }
}

void UpdateEffects(float dt) {
    for (int i = (int)particles.size() - 1; i >= 0; i--) {
        Particle &p = particles[i];
        p.life -= dt;
        p.v.y -= p.grav * dt;
        p.p = Vector3Add(p.p, Vector3Scale(p.v, dt));
        if (p.p.y < 0.02f) { p.p.y = 0.02f; p.v = Vector3{ 0, 0, 0 }; }
        if (p.life <= 0) particles.erase(particles.begin() + i);
    }
    for (int i = (int)tracers.size() - 1; i >= 0; i--) { tracers[i].t -= dt; if (tracers[i].t <= 0) tracers.erase(tracers.begin() + i); }
    for (int i = (int)booms.size() - 1; i >= 0; i--) { booms[i].t += dt; if (booms[i].t > 0.45f) booms.erase(booms.begin() + i); }
    for (int i = (int)smokes.size() - 1; i >= 0; i--) {
        Smoke &s = smokes[i];
        s.t += dt; s.p.y += dt * 0.5f;
        if (s.t > s.life) smokes.erase(smokes.begin() + i);
    }
    boomLightI = std::max(0.0f, boomLightI - dt * 90);
}

void DrawUnlitBegin(float intensity);
void DrawUnlitEnd();

static void DrawDecalQuad(const Decal &d, Texture2D tex, Color c) {
    Vector3 n = d.n;
    Vector3 up = fabsf(n.y) > 0.9f ? Vector3{ 1, 0, 0 } : Vector3{ 0, 1, 0 };
    Vector3 t = Vector3Normalize(Vector3CrossProduct(up, n));
    Vector3 b = Vector3CrossProduct(n, t);
    float cr = cosf(d.rot), sr = sinf(d.rot), h = d.size / 2;
    Vector3 T2 = Vector3Add(Vector3Scale(t, cr * h), Vector3Scale(b, sr * h));
    Vector3 B2 = Vector3Add(Vector3Scale(t, -sr * h), Vector3Scale(b, cr * h));
    Vector3 P[4] = { Vector3Subtract(Vector3Subtract(d.p, T2), B2), Vector3Subtract(Vector3Add(d.p, T2), B2), Vector3Add(Vector3Add(d.p, T2), B2), Vector3Add(Vector3Subtract(d.p, T2), B2) };
    float uv[4][2] = { { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 } };
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(c.r, c.g, c.b, c.a);
    rlNormal3f(n.x, n.y, n.z);
    // ambos lados para no depender del orden
    for (int i = 0; i < 4; i++) { rlTexCoord2f(uv[i][0], uv[i][1]); rlVertex3f(P[i].x, P[i].y, P[i].z); }
    for (int i = 3; i >= 0; i--) { rlTexCoord2f(uv[i][0], uv[i][1]); rlVertex3f(P[i].x, P[i].y, P[i].z); }
    rlEnd();
    rlSetTexture(0);
}

void DrawEffects() {
    // marcas en paredes y suelo
    DrawUnlitBegin(0.35f);
    for (auto &d : decals) if (d.type != 1) DrawDecalQuad(d, d.type == 0 ? texHole : texScorch, WHITE);
    DrawUnlitEnd();
    DrawUnlitBegin(0.1f);   // la sangre, oscura (las marcas no reciben luz)
    for (auto &d : decals) if (d.type == 1) DrawDecalQuad(d, texBlood, WHITE);
    DrawUnlitEnd();
    // humo
    DrawUnlitBegin(0.25f);
    for (auto &s : smokes) {
        if (s.t < 0) continue;
        float k = s.t / s.life;
        DrawBillboard(cam, texSmoke, s.p, s.size * (0.3f + k * 1.4f), Color{ 120, 115, 110, (unsigned char)(200 * (1 - k)) });
    }
    DrawUnlitEnd();
    // sangre y polvo
    DrawUnlitBegin(0.5f);
    for (auto &p : particles) if (!p.additive) DrawBillboard(cam, texFlash, p.p, p.size * 2.2f, Color{ p.c.r, p.c.g, p.c.b, (unsigned char)(255 * clampf(p.life / p.max * 1.5f, 0, 1)) });
    DrawUnlitEnd();
    // chispas, fuego y trazadoras (brillan)
    rlSetBlendMode(BLEND_ADDITIVE);
    DrawUnlitBegin(6.0f);
    for (auto &p : particles) if (p.additive) DrawBillboard(cam, texFlash, p.p, p.size * 2.0f, Color{ p.c.r, p.c.g, p.c.b, (unsigned char)(255 * clampf(p.life / p.max, 0, 1)) });
    for (auto &b : booms) {
        float k = b.t / 0.45f;
        DrawBillboard(cam, texFlash, b.p, (1.5f + k * 7.0f) * b.size, Color{ 255, 150, 60, (unsigned char)(255 * (1 - k)) });
        DrawBillboard(cam, texFlash, b.p, (0.8f + k * 3.0f) * b.size, Color{ 255, 240, 200, (unsigned char)(255 * (1 - k)) });
    }
    for (auto &pr : projectiles) if (pr.rocket) {
        Vector3 back = Vector3Subtract(pr.p, Vector3Scale(Vector3Normalize(pr.v), 0.2f));
        DrawBillboard(cam, texFlash, back, frand(0.4f, 0.6f), Color{ 255, 180, 90, 255 });
    }
    for (auto &pr : projectiles) if (pr.flare) {
        DrawBillboard(cam, texFlash, pr.p, frand(0.5f, 0.7f), Color{ 255, 60, 40, 255 });
        DrawBillboard(cam, texFlash, pr.p, 0.2f, Color{ 255, 230, 200, 255 });
    }
    for (auto &f : firePatches) {   // llamas bajas que titilan sobre el suelo
        float a = clampf(std::min(f.t * 4, (6 - f.t) / 1.5f), 0, 1);
        for (int i = 0; i < 6; i++) {
            float ang = i * 1.047f + f.t * 0.7f, r = f.radius * (i % 2 ? 0.55f : 0.25f);
            Vector3 c{ f.p.x + cosf(ang) * r, 0.18f + 0.06f * sinf(T * 9 + i), f.p.z + sinf(ang) * r };
            DrawBillboard(cam, texFlash, c, frand(0.35f, 0.55f), Color{ 255, (unsigned char)(70 + i * 12), 25, (unsigned char)(170 * a) });
        }
    }
    // zombis ardiendo
    for (auto &z : zombies) if (z.burnT > 0 && z.state != Z_DEAD)
        DrawBillboard(cam, texFlash, Vector3Lerp(z.hipsPos, z.neckPos, 0.4f), frand(0.7f, 1.0f), Color{ 255, 110, 30, (unsigned char)(170 * std::min(1.0f, z.burnT)) });
    DrawUnlitEnd();
    DrawUnlitBegin(4.0f);
    for (auto &t : tracers) {
        float a = clampf(t.t / t.life, 0, 1);
        Color c{ t.c.r, t.c.g, t.c.b, (unsigned char)(255 * a) };
        for (size_t i = 1; i < t.pts.size(); i++) DrawLine3D(t.pts[i - 1], t.pts[i], c);
    }
    DrawUnlitEnd();
    rlSetBlendMode(BLEND_ALPHA);
}

// ---------------------------------------------------------------
// Cuchillo (V): tajo rápido con estocada hacia el zombi más cercano
// ---------------------------------------------------------------
Matrix knifeWorld = MatrixIdentity(); bool knifeVisible = false;
static GunModel knifeModel;

static void BuildKnife() {
    GunModel &g = knifeModel;
    g.parts.clear();
    auto box = [&](Mesh *mesh, const char *mn, Vector3 s, Vector3 p) {
        g.parts.push_back({ mesh, &M(mn), MatrixMultiply(MatrixScale(s.x, s.y, s.z), MatrixTranslate(p.x, p.y, p.z)) });
    };
    box(&unitCube, "metal", Vector3{ 0.006f, 0.032f, 0.19f }, Vector3{ 0, 0.004f, -0.155f });       // hoja
    box(&unitCube, "metal", Vector3{ 0.005f, 0.02f, 0.03f }, Vector3{ 0, 0.0f, -0.255f });          // punta
    box(&unitCube, "darkmetal", Vector3{ 0.016f, 0.06f, 0.012f }, Vector3{ 0, 0.004f, -0.058f });   // guarda
    box(&unitCube, "polymer", Vector3{ 0.026f, 0.032f, 0.115f }, Vector3{ 0, 0, 0 });              // mango
    box(&unitSphere, "darkmetal", Vector3{ 0.03f, 0.03f, 0.03f }, Vector3{ 0, 0, 0.06f });          // pomo
    g.handStart = g.parts.size();
    box(&unitSphere, "glove", Vector3{ 0.07f, 0.075f, 0.1f }, Vector3{ 0.004f, -0.006f, 0.0f });   // puño
    Vector3 a{ 0.01f, -0.02f, 0.06f }, b{ 0.06f, -0.12f, 0.34f };
    Vector3 d = Vector3Subtract(b, a); float len = Vector3Length(d);
    Matrix rot = QuaternionToMatrix(QuaternionFromVector3ToVector3(Vector3{ 0, 1, 0 }, Vector3Scale(d, 1 / len)));
    Vector3 mid = Vector3Lerp(a, b, 0.5f);
    g.parts.push_back({ &unitCyl, &M("sleeve"), MatrixMultiply(MatrixMultiply(MatrixScale(0.07f, len, 0.07f), rot), MatrixTranslate(mid.x, mid.y, mid.z)) });
}

void DrawKnife() { if (knifeVisible) DrawGun(knifeModel, knifeWorld, false); }

static bool Knifeable(const Zombie &z) { return z.state != Z_DEAD && !(z.state == Z_RISE && z.y < -1.0f); }

void Melee() {
    if (player.knifeT >= 0 || T < player.nextKnife || player.papBusy) return;
    player.knifeT = 0; player.knifeHit = false;
    player.nextKnife = T + KNIFE_TIME + 0.1f;
    player.reloading = false;   // como en el juego: acuchillar cancela la recarga
    Play("knife");
    // estocada: si hay un zombi delante a menos de 3 m, te lanzas hacia él
    float fx = -sinf(player.yaw), fz = -cosf(player.yaw);
    float bd = 3.2f, bx = 0, bz = 0;
    for (auto &z : zombies) {
        if (z.state != Z_CHASE) continue;
        float dx = z.pos.x - player.pos.x, dz = z.pos.z - player.pos.z, d = hyp(dx, dz);
        if (d < bd && d > 0.01f && (dx * fx + dz * fz) / d > 0.7f) { bd = d; bx = dx / d; bz = dz / d; }
    }
    if (bd < 3.2f && bd > 1.3f) {
        float v = (bd - 1.0f) / 0.15f;
        player.vel.x = bx * v; player.vel.z = bz * v;
        player.lungeT = 0.15f;
    }
}

static void KnifeHit() {
    Matrix R = MatrixMultiply(MatrixRotateX(player.pitch), MatrixRotateY(player.yaw));
    Ray r{ cam.position, Vector3Transform(Vector3{ 0, 0, -1 }, R) };
    int zi; float dist; bool head = false; Vector3 point;
    Zombie *hit = nullptr;
    if (RayZombie(r, 2.3f, zi, dist, head, point)) hit = &zombies[zi];
    else {
        // si no apuntas justo, vale el zombi más cercano dentro de un cono por delante
        float fx = -sinf(player.yaw), fz = -cosf(player.yaw), bd = 2.0f;
        for (auto &z : zombies) {
            if (!Knifeable(z)) continue;
            float dx = z.pos.x - player.pos.x, dz = z.pos.z - player.pos.z, d = hyp(dx, dz);
            if (d < bd && d > 0.01f && (dx * fx + dz * fz) / d > 0.55f) { bd = d; hit = &z; }
        }
        if (hit) point = Vector3Lerp(hit->hipsPos, hit->neckPos, 0.6f);
    }
    if (!hit) return;
    DamageZombie(*hit, 150, 1, false, point, false);
    if (hit->state == Z_DEAD) AddPoints(70);   // como en el juego: 130 puntos por baja a cuchillo
    Burst(point, Color{ 110, 0, 0, 255 }, 14, 2.0f, 0.06f);
    Play("stab");
    vm.shake = std::max(vm.shake, 0.12f);
}

void UpdateMelee(float dt) {
    if (player.knifeT < 0) return;
    player.knifeT += dt;
    if (!player.knifeHit && player.knifeT >= 0.15f) { player.knifeHit = true; KnifeHit(); }
    if (player.knifeT >= KNIFE_TIME) player.knifeT = -1;
}

// Rayo que cae del techo (llegada de un perro del infierno)
void LightningStrike(Vector3 p) {
    Vector3 top{ p.x + frand(-0.6f, 0.6f), H - 0.1f, p.z + frand(-0.6f, 0.6f) };
    for (int i = 0; i < 2; i++) AddTracer(Jagged(top, Vector3{ p.x, 0.05f, p.z }), Color{ 200, 225, 255, 255 }, 0.35f);
    Burst(Vector3{ p.x, 0.1f, p.z }, Color{ 180, 210, 255, 255 }, 18, 3, 0.05f, 6);
    Burst(Vector3{ p.x, 0.1f, p.z }, Color{ 70, 60, 60, 255 }, 8, 1.2f, 0.1f, 1);
    boomLightPos = Vector3{ p.x, 2.5f, p.z }; boomLightI = std::max(boomLightI, 30.0f);
    PlayAt("strike", p, 35);
}
