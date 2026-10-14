// Mundo: mapas, paredes, ventanas, puertas, armas de pared, bebidas, caja misteriosa y Pack-a-Punch.
#include "game.h"
#include <deque>

struct RoomDef { const char *id; float x1, z1, x2, z2; unsigned light; bool flicker; std::vector<Vector2> lights; bool open = false; const char *floor = nullptr; };
struct LinkDef { const char *a, *b; float x, z; int cost; };
struct PlaceDef { const char *room; float x, z; };
struct BuyDef { const char *room; int weapon; float x, z; };
struct PerkPlace { const char *room; int perk; float x, z; };
struct PropDef { float x, z, w, d, h; const char *type; };
struct PipeDef { float x1, z1, x2, z2; };
struct MapDef {
    const char *key, *name, *theme; float sx, sz, syaw; const char *startRoom;
    std::vector<RoomDef> rooms; std::vector<LinkDef> links; std::vector<PlaceDef> windows;
    std::vector<BuyDef> wallBuys; std::vector<PerkPlace> perks; std::vector<PlaceDef> boxSpots; PlaceDef pap;
    std::vector<PropDef> props; std::vector<PipeDef> pipes; std::vector<PlaceDef> ground;
    PlaceDef power{ nullptr, 0, 0 };   // interruptor de la corriente (sin él, la corriente siempre está activada)
    bool outdoor = false;              // al aire libre: sin techo, con cielo
};

static const MapDef MAPS[4] = {
    { "bunker", "BÚNKER", "bunker", 0, 4, 0, "A",
      { { "A", -10, -10, 10, 10, 0xffd9a0, false, {} }, { "B", -10, -26, 10, -10, 0xffc890, true, {} }, { "C", 10, -8, 28, 8, 0xaaccff, false, {} },
        { "D", -10, 10, 10, 26, 0xff9a9a, false, {} }, { "E", 10, -26, 28, -8, 0xc8a0ff, false, {} } },
      { { "A", "B", 0, -10, 750 }, { "A", "C", 10, 0, 1000 }, { "A", "D", 0, 10, 1000 }, { "B", "E", 10, -18, 1250 }, { "C", "E", 19, -8, 0 } },
      { { "A", -10, -5 }, { "A", -10, 5 }, { "B", -10, -18 }, { "B", 0, -26 }, { "C", 28, 0 }, { "C", 19, 8 },
        { "D", -10, 18 }, { "D", 0, 26 }, { "D", 10, 18 }, { "E", 19, -26 }, { "E", 28, -17 } },
      { { "A", W_SHOTGUN, -6, -10 }, { "A", W_SMG, 6, 10 }, { "B", W_RIFLE, -10, -22 }, { "E", W_SNIPER, 28, -11 }, { "C", W_LAUNCHER, 14, -8 }, { "D", W_RPG, -6, 26 },
        { "B", W_SAWED, 10, -14 }, { "D", W_SUOMI, 10, 22 } },
      { { "A", P_REVIVE, 10, -6 }, { "B", P_JUGG, 6, -26 }, { "C", P_SPEED, 28, -5 }, { "D", P_DOUBLE, 6, 26 }, { "A", P_STAMIN, -10, -1 } },
      { { "A", -5, 10 }, { "B", -5, -26 }, { "C", 23, 8 }, { "D", 10, 23 } },
      { "E", 24, -26 },
      { { -5, 4, 1.2f, 1.2f, 1.2f, "crate" }, { -6.2f, 4, 1.2f, 1.2f, 1.2f, "crate" }, { -5.6f, 4, 1.1f, 1.1f, 2.3f, "crate" }, { 5, -5, 1.2f, 1.2f, 1.2f, "crate" },
        { 6.2f, -5.6f, 1.2f, 1.2f, 1.2f, "crate" }, { -5, -19, 1.2f, 1.2f, 1.2f, "crate" }, { 4, -21, 1.2f, 1.2f, 1.2f, "crate" }, { 5.3f, -21, 0.8f, 0.8f, 1.1f, "barrel" },
        { 16, 3, 1.2f, 1.2f, 1.2f, "crate" }, { 22, -2, 1.2f, 1.2f, 1.2f, "crate" }, { -4, 19, 1.2f, 1.2f, 1.2f, "crate" }, { 5, 16, 3, 1.2f, 0.9f, "metal" },
        { 15, -17, 1.2f, 1.2f, 1.2f, "crate" }, { 22, -14, 0.8f, 0.8f, 1.1f, "barrel" }, { 23, -14.9f, 0.8f, 0.8f, 1.1f, "barrel" } },
      {}, {}, { "C", 28, 5 } },
    { "fabrica", "FÁBRICA", "fabrica", 0, 6, 0, "A",
      { { "A", -8, -8, 8, 12, 0xffe0b0, false, {} }, { "B", -24, -16, 24, -8, 0xb0d0ff, false, { { -12, -12 }, { 12, -12 } } },
        { "C", -24, -8, -8, 12, 0xffb070, true, {} }, { "D", 8, -8, 24, 12, 0x9affc0, false, {} },
        { "E", -24, 12, 24, 30, 0xff8080, false, { { -12, 21 }, { 12, 21 } } } },
      { { "A", "B", 0, -8, 750 }, { "B", "C", -16, -8, 1000 }, { "B", "D", 16, -8, 1000 }, { "C", "E", -16, 12, 1250 }, { "D", "E", 16, 12, 1250 } },
      { { "B", -18, -16 }, { "B", 0, -16 }, { "B", 18, -16 }, { "C", -24, -2 }, { "C", -24, 8 }, { "D", 24, -2 }, { "D", 24, 8 },
        { "E", -14, 30 }, { "E", 0, 30 }, { "E", 14, 30 }, { "E", -24, 21 }, { "E", 24, 21 } },
      { { "A", W_SHOTGUN, 8, 4 }, { "A", W_SMG, -5, -8 }, { "B", W_RIFLE, 6, -16 }, { "D", W_SNIPER, 11, -8 }, { "E", W_LMG, -24, 15 }, { "C", W_LAUNCHER, -24, -6 }, { "E", W_RPG, 24, 16 },
        { "C", W_SUOMI, -11, -8 }, { "D", W_SAWED, 20, 12 } },
      { { "A", P_REVIVE, -8, 0 }, { "D", P_JUGG, 24, 3 }, { "C", P_SPEED, -21, 12 }, { "B", P_DOUBLE, -9, -16 }, { "A", P_STAMIN, 8, 9 } },
      { { "A", -4, 12 }, { "B", 9, -16 }, { "C", -24, 3 }, { "E", -6, 30 } },
      { "E", 6, 30 },
      { { -4, -3, 1.2f, 1.2f, 1.2f, "crate" }, { 4, 7, 1.2f, 1.2f, 1.2f, "crate" }, { -16, -1, 3, 1.5f, 1.8f, "metal" }, { -19, 5, 1.2f, 1.2f, 1.2f, "crate" },
        { 14, 0, 2.5f, 1.5f, 1.6f, "metal" }, { 19, 6, 0.8f, 0.8f, 1.1f, "barrel" }, { 19.9f, 6.6f, 0.8f, 0.8f, 1.1f, "barrel" }, { -12, -12, 1.2f, 1.2f, 1.2f, "crate" },
        { 12, -12, 1.2f, 1.2f, 1.2f, "crate" }, { -10, 20, 4, 2, 2.4f, "metal" }, { 10, 22, 4, 2, 2.4f, "metal" }, { 0, 19, 1.2f, 1.2f, 1.2f, "crate" },
        { -19, 25, 0.8f, 0.8f, 1.1f, "barrel" }, { 19, 25, 1.2f, 1.2f, 1.2f, "crate" } },
      { { -24, -15.4f, 24, -15.4f }, { -24, 29.4f, 24, 29.4f }, { -23.4f, -8, -23.4f, 12 } },
      { { "A", -5, -5 }, { "A", 5, 9 }, { "A", -5, 9 }, { "A", 5, -5 } }, { "B", -24, -12 } },
    // Pueblo: mapa grande al aire libre. Plaza, calle, cementerio y estación con cielo abierto;
    // iglesia, casa y taller con techo.
    { "pueblo", "PUEBLO", "pueblo", 0, 5, 0, "A",
      { { "A", -15, -15, 15, 15, 0xffd9a0, false, { { -9, -9 }, { 9, -9 }, { -9, 9 }, { 9, 9 } }, true, "tiles" },          // plaza
        { "B", -15, -55, 15, -15, 0xffc890, false, { { -10, -25 }, { 10, -38 }, { -10, -50 } }, true, "concfloor" },      // calle principal
        { "C", 15, -15, 35, 15, 0xffb070, true, { { 25, -7 }, { 25, 7 } }, false, "planks" },                            // iglesia
        { "D", 35, -25, 65, 25, 0x9ab0ff, false, { { 44, -16 }, { 57, 2 }, { 44, 17 } }, true, "ground" },               // cementerio
        { "E", -35, -10, -15, 10, 0xffe0b0, false, {}, false, "planks" },                                                // casa
        { "F", -15, 15, 35, 45, 0xb0d0ff, false, { { -5, 24 }, { 15, 34 }, { 28, 22 }, { 2, 40 } }, true, "concfloor" }, // estación
        { "G", -45, -45, -15, -10, 0x9affc0, false, { { -35, -35 }, { -25, -20 } }, false, "concfloor" } },             // taller
      { { "A", "B", 0, -15, 750 }, { "A", "C", 15, 0, 1000 }, { "A", "E", -15, 0, 1000 }, { "A", "F", 0, 15, 1000 },
        { "C", "D", 35, 0, 1250 }, { "F", "D", 35, 20, 1250 }, { "B", "G", -15, -30, 1000 }, { "G", "E", -25, -10, 750 } },
      { { "A", -15, 12.5f }, { "B", -15, -50 }, { "B", 15, -25 }, { "B", 15, -45 }, { "B", 5, -55 }, { "C", 21, -15 }, { "C", 29, -15 },
        { "D", 45, -25 }, { "D", 58, -25 }, { "D", 65, -10 }, { "D", 65, 12 }, { "D", 50, 25 }, { "E", -35, -4 }, { "E", -35, 6 }, { "E", -25, 10 },
        { "F", -15, 25 }, { "F", -15, 38 }, { "F", 0, 45 }, { "F", 20, 45 }, { "F", 35, 35 }, { "G", -45, -35 }, { "G", -45, -20 }, { "G", -30, -45 }, { "G", -40, -10 } },
      { { "A", W_SHOTGUN, -15, -12.5f }, { "A", W_SMG, 15, 10 }, { "B", W_RIFLE, 15, -35 }, { "B", W_SAWED, -6, -55 }, { "C", W_LAUNCHER, 25, 15 },
        { "C", W_MAGNUM, 35, -10 }, { "D", W_SNIPER, 65, 0 }, { "D", W_RPG, 51, -25 }, { "E", W_LMG, -35, -8 }, { "F", W_SUOMI, 35, 40 }, { "G", W_FLARE, -45, -28 } },
      { { "A", P_REVIVE, 8, -15 }, { "C", P_JUGG, 35, 10 }, { "F", P_SPEED, -15, 32 }, { "G", P_DOUBLE, -32, -10 }, { "B", P_STAMIN, -15, -22 } },
      { { "A", -7, 15 }, { "B", -15, -38 }, { "D", 65, -18 }, { "F", 10, 45 } },
      { "D", 35, -20 },
      { // plaza
        { -9, -3, 2.0f, 4.2f, 1.5f, "car" }, { 6, 4, 3.2f, 0.8f, 1.0f, "sandbag" }, { 10, -5, 0.8f, 0.8f, 1.1f, "barrel" }, { 10.8f, -5.7f, 0.8f, 0.8f, 1.1f, "barrel" },
        { -4, 9, 1.2f, 1.2f, 1.2f, "crate" }, { -2.8f, 9, 1.2f, 1.2f, 1.2f, "crate" }, { -11, 6, 0.6f, 0.6f, 6, "tree" }, { 12, 12, 0.6f, 0.6f, 6, "tree" },
        { 0, 0, 3.0f, 3.0f, 0.8f, "fountain" },
        // calle principal
        { -9, -28, 2.0f, 4.2f, 1.5f, "car" }, { 8, -47, 2.0f, 4.2f, 1.5f, "car" }, { 0, -21, 4.5f, 0.8f, 1.0f, "sandbag" }, { -7, -46, 2.5f, 6.0f, 2.6f, "metal" },
        { 10, -20, 1.2f, 1.2f, 1.2f, "crate" }, { -11, -40, 1.2f, 1.2f, 1.2f, "crate" }, { 4, -33, 0.8f, 0.8f, 1.1f, "barrel" }, { 11, -52, 0.6f, 0.6f, 6, "tree" },
        // iglesia: bancos y altar
        { 20.5f, -7, 4, 0.6f, 0.9f, "pew" }, { 29.5f, -7, 4, 0.6f, 0.9f, "pew" }, { 20.5f, -2, 4, 0.6f, 0.9f, "pew" }, { 29.5f, -2, 4, 0.6f, 0.9f, "pew" },
        { 20.5f, 3, 4, 0.6f, 0.9f, "pew" }, { 29.5f, 3, 4, 0.6f, 0.9f, "pew" }, { 25, -12.5f, 3, 1.2f, 1.1f, "metal" },
        // cementerio: lápidas y árboles
        { 40, -19, 0.8f, 0.25f, 1.0f, "tomb" }, { 46, -19, 0.8f, 0.25f, 1.0f, "tomb" }, { 52, -19, 0.8f, 0.25f, 1.0f, "tomb" }, { 58, -19, 0.8f, 0.25f, 1.0f, "tomb" },
        { 40, -11, 0.8f, 0.25f, 1.0f, "tomb" }, { 50, -11, 0.8f, 0.25f, 1.0f, "tomb" }, { 60, -11, 0.8f, 0.25f, 1.0f, "tomb" },
        { 45, -4, 0.8f, 0.25f, 1.0f, "tomb" }, { 53, -4, 0.8f, 0.25f, 1.0f, "tomb" }, { 41, 5, 0.8f, 0.25f, 1.0f, "tomb" }, { 49, 5, 0.8f, 0.25f, 1.0f, "tomb" },
        { 61, 5, 0.8f, 0.25f, 1.0f, "tomb" }, { 52, 12, 0.8f, 0.25f, 1.0f, "tomb" }, { 58, 18, 0.8f, 0.25f, 1.0f, "tomb" }, { 40, 12, 0.8f, 0.25f, 1.0f, "tomb" },
        { 56, -4, 2.2f, 1.2f, 1.4f, "metal" }, { 38, 22, 0.6f, 0.6f, 7, "deadtree" }, { 62, -22, 0.6f, 0.6f, 7.5f, "deadtree" }, { 62, 21, 0.6f, 0.6f, 6.5f, "deadtree" }, { 36.5f, -6, 0.6f, 0.6f, 6, "deadtree" },
        // casa
        { -25, -4, 2.2f, 1.1f, 0.9f, "pew" }, { -31, 6, 1.2f, 1.2f, 1.2f, "crate" }, { -19, -7, 0.8f, 0.8f, 1.1f, "barrel" },
        // estación: contenedores, auto y sacos
        { 5, 26, 6, 2.5f, 2.6f, "metal" }, { 21, 38, 6, 2.5f, 2.6f, "metal" }, { -8, 31, 2.0f, 4.2f, 1.5f, "car" }, { 27, 28, 3.5f, 0.8f, 1.0f, "sandbag" },
        { 12, 20, 1.2f, 1.2f, 1.2f, "crate" }, { 13.2f, 20, 1.2f, 1.2f, 1.2f, "crate" }, { 30, 42, 0.8f, 0.8f, 1.1f, "barrel" }, { -11, 42, 0.6f, 0.6f, 6, "tree" },
        // taller
        { -35, -30, 3, 1.5f, 1.6f, "metal" }, { -22, -38, 1.2f, 1.2f, 1.2f, "crate" }, { -40, -40, 0.8f, 0.8f, 1.1f, "barrel" }, { -20, -14, 2.0f, 4.2f, 1.5f, "car" } },
      {},
      { { "A", -11, 1 }, { "A", 11, 0 }, { "A", 3, -10 }, { "B", 0, -38 }, { "B", -6, -52 }, { "D", 43, -15 }, { "D", 55, -8 }, { "D", 47, 9 },
        { "D", 60, 14 }, { "D", 39, 1 }, { "F", 6, 33 }, { "F", 26, 42 }, { "F", -6, 40 } },
      { "G", -45, -15 }, true },
    // Campo de tiro: una galería larga con todas las armas gratis y dianas-zombi (sin daño al jugador)
    { "practica", "CAMPO DE TIRO", "practica", 0, 3, 0, "A",
      { { "A", -9, -46, 9, 7, 0xffe6c0, false, { { 0, 2 }, { 0, -10 }, { 0, -22 }, { 0, -34 }, { 0, -44 } } } },
      {}, {},
      { { "A", W_PISTOL, -9, 5.4f }, { "A", W_SHOTGUN, -9, 3.5f }, { "A", W_SAWED, -9, 1.6f }, { "A", W_SMG, -9, -0.3f }, { "A", W_SUOMI, -9, -2.2f }, { "A", W_RIFLE, -9, -4.1f },
        { "A", W_LMG, -9, -6.0f }, { "A", W_SNIPER, -9, -7.9f },
        { "A", W_REVOLVER, 9, 5.4f }, { "A", W_MAGNUM, 9, 3.5f }, { "A", W_FLARE, 9, 1.6f }, { "A", W_RAYGUN, 9, -0.3f }, { "A", W_WUNDER, 9, -2.2f }, { "A", W_THUNDER, 9, -4.1f },
        { "A", W_LAUNCHER, 9, -6.0f }, { "A", W_RPG, 9, -7.9f } },
      { { "A", P_SPEED, -7.5f, 7 }, { "A", P_DOUBLE, 0, 7 }, { "A", P_STAMIN, 7.5f, 7 } },
      { { "A", -4, 7 } },
      { "A", 4, 7 },
      { { -5.5f, -9.5f, 5, 0.6f, 1.0f, "metal" }, { 5.5f, -9.5f, 5, 0.6f, 1.0f, "metal" }, { -7, -19, 1.2f, 1.2f, 1.2f, "crate" },
        { 7, -22, 1.2f, 1.2f, 1.2f, "crate" }, { 7.5f, -33, 0.8f, 0.8f, 1.1f, "barrel" }, { -7.5f, -37, 1.2f, 1.2f, 1.2f, "crate" } },
      {}, {} },
};

struct Theme { const char *wall, *floor, *ceil; float wallUV, floorUV, ceilUV; Vector3 fog, ambient; float fogDensity; };
static Theme ThemeFor(const char *t) {
    if (!strcmp(t, "practica")) return { "bricks", "concfloor", "ceil", 2.2f, 3.0f, 3.0f, { 0.005f, 0.004f, 0.005f }, { 0.03f, 0.028f, 0.026f }, 0.012f };
    if (!strcmp(t, "pueblo")) return { "bricks", "concfloor", "ceil", 2.2f, 3.0f, 3.0f, { 0.010f, 0.012f, 0.020f }, { 0.030f, 0.032f, 0.045f }, 0.022f };   // noche con luna
    if (!strcmp(t, "fabrica")) return { "concwall", "concfloor", "metalceil", 3.0f, 3.0f, 2.5f, { 0.006f, 0.007f, 0.009f }, { 0.012f, 0.013f, 0.016f }, 0.035f };
    return { "bricks", "tiles", "ceil", 2.2f, 2.0f, 3.0f, { 0.005f, 0.004f, 0.005f }, { 0.012f, 0.011f, 0.012f }, 0.04f };
}

// ---------- Estado del mundo ----------
std::vector<Room> rooms;
std::vector<Link> links;
std::vector<Window> windows;
std::vector<Door> doors;
std::vector<Box2> colliders;
std::vector<BoundingBox> solids;
std::vector<Interact> interactables;
std::vector<Light> lights;
std::vector<Prop> props;
std::vector<Label> labels;
std::vector<Box2> mmWalls;
std::vector<MMIcon> mmIcons;
std::vector<std::pair<Mesh, PMat *>> staticMeshes;
const MapDef *curMap = nullptr;
int startRoom = 0;
Mesh unitCube, unitCyl, unitSphere, unitPlane;
BoxState box;
PapState pap;
static std::vector<char> unlockedRooms;
static std::deque<PMat> dynMats;
static std::vector<Texture2D> mapTextures;   // texturas de este mapa (se liberan al cambiar)
static int boxLabel = -1, papLabel = -1;
// ---------- Corriente ----------
bool powerOn = true;
struct PowerVis { int light; PMat *sign; Vector3 emis; int perk; };   // máquinas que se encienden con la corriente
static std::vector<PowerVis> powerVis;
static int papLight = -1, roomLights = 0;
static float powerT = -1, leverK = 0;   // powerT: tiempo desde que se activó (para el parpadeo)
static Matrix switchBase;
static bool NeedsPower(int perk) { return perk != P_REVIVE; }   // como en el juego, Quick Revive funciona sin corriente
extern float themeFogDensity; extern Vector3 themeFog, themeAmbient;

const char *MapName() { return curMap ? curMap->name : ""; }
bool Practice() { return curMap && !strcmp(curMap->key, "practica"); }
int Cost(int c) { return Practice() ? 0 : c; }
static std::string Price(int c) { return Practice() ? std::string("[GRATIS]") : "[" + std::to_string(c) + "]"; }
bool MapOutdoor() { return curMap && curMap->outdoor; }
Vector2 MapStart(float &yaw) { yaw = curMap->syaw; return Vector2{ curMap->sx, curMap->sz }; }

int RoomIndex(const std::string &id) {
    for (size_t i = 0; i < rooms.size(); i++) if (rooms[i].id == id) return (int)i;
    return 0;
}
int RoomAt(float x, float z) {
    for (size_t i = 0; i < rooms.size(); i++) { const Room &r = rooms[i]; if (x >= r.x1 && x <= r.x2 && z >= r.z1 && z <= r.z2) return (int)i; }
    return -1;
}
bool Unlocked(int r) { return r >= 0 && r < (int)unlockedRooms.size() && unlockedRooms[r]; }

void RecomputeUnlocked() {
    unlockedRooms.assign(rooms.size(), 0);
    std::vector<int> q{ startRoom };
    unlockedRooms[startRoom] = 1;
    while (!q.empty()) {
        int c = q.front(); q.erase(q.begin());
        for (auto &l : links) {
            if (!l.open) continue;
            int a = RoomIndex(l.a), b = RoomIndex(l.b);
            int n = a == c ? b : (b == c ? a : -1);
            if (n >= 0 && !unlockedRooms[n]) { unlockedRooms[n] = 1; q.push_back(n); }
        }
    }
}

// Siguiente sala (y puerta) del camino más corto entre dos salas
bool NextRoom(int from, int to, int &outRoom, int &outLink) {
    if (from < 0 || to < 0 || from == to) return false;
    int n = (int)rooms.size();
    std::vector<int> prevRoom(n, -2), prevLink(n, -1);
    prevRoom[from] = -1;
    std::vector<int> q{ from };
    while (!q.empty()) {
        int c = q.front(); q.erase(q.begin());
        for (size_t li = 0; li < links.size(); li++) {
            const Link &l = links[li];
            if (!l.open) continue;
            int a = RoomIndex(l.a), b = RoomIndex(l.b);
            int nb = a == c ? b : (b == c ? a : -1);
            if (nb < 0 || prevRoom[nb] != -2) continue;
            prevRoom[nb] = c; prevLink[nb] = (int)li;
            if (nb == to) {
                int cur = nb;
                while (prevRoom[cur] != from) cur = prevRoom[cur];
                outRoom = cur; outLink = prevLink[cur];
                return true;
            }
            q.push_back(nb);
        }
    }
    return false;
}

void Collide(Vector3 &pos, float r, bool climb) {
    for (auto &c : colliders) {
        if (climb && pos.y >= c.top - 0.02f) continue;   // estamos encima
        float cx = clampf(pos.x, c.minX, c.maxX), cz = clampf(pos.z, c.minZ, c.maxZ);
        float dx = pos.x - cx, dz = pos.z - cz, d2 = dx * dx + dz * dz;
        if (d2 >= r * r) continue;
        if (d2 > 1e-8f) {
            float d = sqrtf(d2);
            pos.x += dx / d * (r - d); pos.z += dz / d * (r - d);
        } else {
            float l = pos.x - c.minX, rr = c.maxX - pos.x, t = pos.z - c.minZ, b = c.maxZ - pos.z;
            float m = std::min(std::min(l, rr), std::min(t, b));
            if (m == l) pos.x = c.minX - r; else if (m == rr) pos.x = c.maxX + r;
            else if (m == t) pos.z = c.minZ - r; else pos.z = c.maxZ + r;
        }
    }
}
float GroundAt(float x, float z, float r, float y) {
    float g = 0;
    for (auto &c : colliders)
        if (c.top < 1e8f && c.top <= y + 0.05f && c.top > g && x > c.minX - r && x < c.maxX + r && z > c.minZ - r && z < c.maxZ + r) g = c.top;
    return g;
}
bool TryMantle(Vector3 &pos, Vector3 vel, float r) {
    for (auto &c : colliders) {
        if (c.top > 1e8f || c.top <= pos.y + 0.02f || c.top > pos.y + 0.75f) continue;
        float cx = clampf(pos.x, c.minX, c.maxX), cz = clampf(pos.z, c.minZ, c.maxZ);
        float dx = cx - pos.x, dz = cz - pos.z, d = hyp(dx, dz);
        if (d > r + 0.12f) continue;
        if (d > 0.001f && (vel.x * dx + vel.z * dz) / d < 0.5f) continue;   // hay que ir hacia la caja
        pos.y = c.top;
        if (d > 0.001f) { pos.x += dx / d * std::min(d + 0.15f, r); pos.z += dz / d * std::min(d + 0.15f, r); }
        return true;
    }
    return false;
}
bool InsideCollider(float x, float z, float r) {
    for (auto &c : colliders) if (x > c.minX - r && x < c.maxX + r && z > c.minZ - r && z < c.maxZ + r) return true;
    return false;
}

static bool nearf(float a, float b) { return fabsf(a - b) < 1e-4f; }
struct WallPlace { int nx, nz; float ry; };
static WallPlace WallPlaceAt(int room, float x, float z) {
    const Room &r = rooms[room];
    int nx = 0, nz = 0;
    if (nearf(r.x1, x)) nx = 1; else if (nearf(r.x2, x)) nx = -1; else if (nearf(r.z1, z)) nz = 1; else nz = -1;
    return { nx, nz, atan2f((float)nx, (float)nz) };
}

// ---------- Construcción ----------
struct Builders {
    std::vector<std::pair<std::string, MeshBuilder>> list;
    MeshBuilder &Get(const std::string &mat) {
        for (auto &p : list) if (p.first == mat && p.second.Verts() < 60000) return p.second;
        list.push_back({ mat, MeshBuilder() });
        return list.back().second;
    }
};
static Builders builders;
static Theme theme;
static float wallH = H;   // altura de las paredes (más altas en los mapas al aire libre, como fachadas)

static void AddSolidBox(Vector3 mn, Vector3 mx, bool collide) {
    solids.push_back(BoundingBox{ mn, mx });
    if (collide) colliders.push_back(Box2{ mn.x, mx.x, mn.z, mx.z });
}

static void WallSeg(float x1, float z1, float x2, float z2) {
    const float Tk = 0.4f;
    float minX = std::min(x1, x2) - (x1 == x2 ? Tk / 2 : 0), maxX = std::max(x1, x2) + (x1 == x2 ? Tk / 2 : 0);
    float minZ = std::min(z1, z2) - (z1 == z2 ? Tk / 2 : 0), maxZ = std::max(z1, z2) + (z1 == z2 ? Tk / 2 : 0);
    builders.Get(theme.wall).Box(Vector3{ minX, 0, minZ }, Vector3{ maxX, wallH, maxZ }, theme.wallUV, 0, 1 | 2 | 16 | 32);
    AddSolidBox(Vector3{ minX, 0, minZ }, Vector3{ maxX, wallH, maxZ }, true);
    mmWalls.push_back(Box2{ minX, maxX, minZ, maxZ });
}

static void BuildWalls() {
    struct Iv { float s, e; };
    std::vector<std::pair<std::string, std::vector<Iv>>> lines, gaps;
    auto add = [](std::vector<std::pair<std::string, std::vector<Iv>>> &m, const std::string &key, float s, float e) {
        for (auto &p : m) if (p.first == key) { p.second.push_back({ s, e }); return; }
        m.push_back({ key, { { s, e } } });
    };
    char key[32];
    auto K = [&](char c, float v) { snprintf(key, 32, "%c%.2f", c, v); return std::string(key); };
    for (auto &r : rooms) {
        add(lines, K('h', r.z1), r.x1, r.x2); add(lines, K('h', r.z2), r.x1, r.x2);
        add(lines, K('v', r.x1), r.z1, r.z2); add(lines, K('v', r.x2), r.z1, r.z2);
    }
    for (auto &l : links) {
        if (l.axis == 'z') add(gaps, K('v', l.x), l.z - 1.5f, l.z + 1.5f); else add(gaps, K('h', l.z), l.x - 1.5f, l.x + 1.5f);
    }
    for (auto &w : windows) {
        if (w.nx) add(gaps, K('v', w.x), w.z - 0.8f, w.z + 0.8f); else add(gaps, K('h', w.z), w.x - 0.8f, w.x + 0.8f);
    }
    for (auto &ln : lines) {
        auto ivs = ln.second;
        std::sort(ivs.begin(), ivs.end(), [](const Iv &a, const Iv &b) { return a.s < b.s; });
        std::vector<Iv> merged;
        for (auto &iv : ivs) {
            if (!merged.empty() && iv.s <= merged.back().e + 1e-4f) merged.back().e = std::max(merged.back().e, iv.e);
            else merged.push_back(iv);
        }
        if (ln.first[0] == 'h') for (auto &m : merged) { m.s -= 0.2f; m.e += 0.2f; }
        std::vector<Iv> parts = merged;
        for (auto &g : gaps) {
            if (g.first != ln.first) continue;
            for (auto &gp : g.second) {
                std::vector<Iv> next;
                for (auto &p : parts) {
                    if (gp.e <= p.s || gp.s >= p.e) { next.push_back(p); continue; }
                    if (gp.s > p.s) next.push_back({ p.s, gp.s });
                    if (gp.e < p.e) next.push_back({ gp.e, p.e });
                }
                parts = next;
            }
        }
        float c = (float)atof(ln.first.c_str() + 1);
        for (auto &p : parts) {
            if (p.e - p.s < 0.05f) continue;
            if (ln.first[0] == 'h') WallSeg(p.s, c, p.e, c); else WallSeg(c, p.s, c, p.e);
        }
    }
}

static Matrix TRSm(Vector3 pos, float ry, Vector3 scale, float rx = 0, float rz = 0) {
    Matrix m = MatrixScale(scale.x, scale.y, scale.z);
    if (rx) m = MatrixMultiply(m, MatrixRotateX(rx));
    if (rz) m = MatrixMultiply(m, MatrixRotateZ(rz));
    m = MatrixMultiply(m, MatrixRotateY(ry));
    return MatrixMultiply(m, MatrixTranslate(pos.x, pos.y, pos.z));
}

static PMat *TintedFixture(unsigned color, float k = 5.0f) {
    dynMats.push_back(M("fixture"));
    dynMats.back().emis = Vector3Scale(ColorV(color), k);
    return &dynMats.back();
}
// Copia de un material con otro color (con sus propios mapas, para no teñir el original). Se crea una vez.
static PMat *Recolored(const char *base, Color c) {
    static std::vector<std::pair<std::string, PMat *>> made;
    std::string key = TextFormat("%s_%02x%02x%02x", base, c.r, c.g, c.b);
    for (auto &m : made) if (m.first == key) return m.second;
    PMat *pm = new PMat(M(base));
    MaterialMap *maps = (MaterialMap *)MemAlloc(sizeof(MaterialMap) * 12 /* MAX_MATERIAL_MAPS de raylib */);
    memcpy(maps, pm->m.maps, sizeof(MaterialMap) * 12 /* MAX_MATERIAL_MAPS de raylib */);
    pm->m.maps = maps;
    pm->m.maps[MATERIAL_MAP_DIFFUSE].color = c;
    made.push_back({ key, pm });
    return pm;
}

static Texture2D ChalkTexture(const WeaponDef &d) {
    Image img = GenImageColor(512, 256, Color{ 18, 18, 18, 255 });
    Color ch{ 225, 225, 225, 220 };
    ImageDrawRectangleLines(&img, Rectangle{ 8, 8, 496, 240 }, 4, ch);
    // silueta del arma en tiza
    float L = d.len / 0.9f;
    int bodyW = (int)(240 * L), x0 = 256 - bodyW / 2;
    ImageDrawRectangleLines(&img, Rectangle{ (float)x0, 50, (float)bodyW, 38 }, 5, ch);
    ImageDrawRectangleLines(&img, Rectangle{ (float)(x0 + bodyW), 60, 70, 14 }, 4, ch);
    ImageDrawRectangleLines(&img, Rectangle{ (float)(x0 + 40), 88, 28, 50 }, 5, ch);
    if (d.len > 0.35f) ImageDrawRectangleLines(&img, Rectangle{ (float)(x0 - 60), 55, 60, 30 }, 5, ch);
    Vector2 s = MeasureTextEx(fontUI, d.name, 40, 1);
    ImageDrawTextEx(&img, fontUI, d.name, Vector2{ 256 - s.x / 2, 150 }, 40, 1, Color{ 240, 240, 240, 255 });
    char cost[16]; if (Practice()) snprintf(cost, 16, "GRATIS"); else snprintf(cost, 16, "%d", d.cost);
    s = MeasureTextEx(fontUI, cost, 34, 1);
    ImageDrawTextEx(&img, fontUI, cost, Vector2{ 256 - s.x / 2, 198 }, 34, 1, Color{ 255, 216, 74, 255 });
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    GenTextureMipmaps(&t);
    SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
    mapTextures.push_back(t);
    return t;
}

static Texture2D Lbl(const char *text, Color c, int size = 54) {
    Texture2D t = MakeLabel(text, c, size);
    mapTextures.push_back(t);
    return t;
}

static void BuildWindow(Window &w) {
    bool vertical = w.nx != 0;
    float sx = vertical ? 0.4f : 1.6f, sz = vertical ? 1.6f : 0.4f;
    MeshBuilder &b = builders.Get(theme.wall);
    b.Box(Vector3{ w.x - sx / 2, 0, w.z - sz / 2 }, Vector3{ w.x + sx / 2, 0.9f, w.z + sz / 2 }, theme.wallUV, 0, 1 | 2 | 4 | 16 | 32);
    AddSolidBox(Vector3{ w.x - sx / 2, 0, w.z - sz / 2 }, Vector3{ w.x + sx / 2, 0.9f, w.z + sz / 2 }, true);
    b.Box(Vector3{ w.x - sx / 2, 2.4f, w.z - sz / 2 }, Vector3{ w.x + sx / 2, wallH, w.z + sz / 2 }, theme.wallUV, 0, 1 | 2 | 8 | 16 | 32);
    solids.push_back(BoundingBox{ Vector3{ w.x - sx / 2, 2.4f, w.z - sz / 2 }, Vector3{ w.x + sx / 2, wallH, w.z + sz / 2 } });
    // marco de madera
    MeshBuilder &fr = builders.Get("planks");
    float fx = vertical ? 0.46f : 0.12f, fz = vertical ? 0.12f : 0.46f;
    for (int s = -1; s <= 1; s += 2) {
        float ox = vertical ? 0 : s * 0.86f, oz = vertical ? s * 0.86f : 0;
        fr.Box(Vector3{ w.x + ox - fx / 2, 0.9f, w.z + oz - fz / 2 }, Vector3{ w.x + ox + fx / 2, 2.4f, w.z + oz + fz / 2 }, 1.0f);
    }
    for (int i = 0; i < 5; i++) {
        w.boardBase[i] = Vector3{ w.x + w.nx * 0.26f, 1.05f + i * 0.28f, w.z + w.nz * 0.26f };
        w.boardRot[i] = frand(-0.3f, 0.3f);
        w.boardVis[i] = true; w.boardPos[i] = w.boardBase[i]; w.boardAnim[i] = -1; w.boardSpin[i] = 0;
    }
    int wi = (int)(&w - &windows[0]);
    Interact it;
    it.pos = [wi]() { return windows[wi].inside; };
    it.range = 1.7f; it.hold = true;
    it.active = [wi]() { return windows[wi].boards < 5; };
    it.text = []() { return std::string("Mantén E para reparar la ventana"); };
    it.action = [wi]() {
        Window &w = windows[wi];
        if (T < w.repairT || w.boards >= 5) return;
        w.repairT = T + 0.55f;
        int i = w.boards;
        w.boardAnim[i] = -1; w.boardPos[i] = w.boardBase[i]; w.boardVis[i] = true; w.boardSpin[i] = 0;
        w.boards++;
        Play("repair");
        if (game.repairPts < 500) { game.repairPts += 10; AddPoints(10); }
    };
    interactables.push_back(it);
}

void TearBoard(Window &w) {
    if (w.boards <= 0) return;
    w.boards--;
    int i = w.boards;
    w.boardAnim[i] = 0;
    w.boardVel[i] = Vector3{ -w.nx * 2.5f + frand(-0.5f, 0.5f), 1.5f, -w.nz * 2.5f + frand(-0.5f, 0.5f) };
    float d = hyp(w.x - player.pos.x, w.z - player.pos.z);
    if (d < 20) Play("board", 1 - d / 20);
}

static void BuildLink(int li) {
    Link &l = links[li];
    bool vertical = l.axis == 'z';
    builders.Get(theme.wall).Box(Vector3{ l.x - (vertical ? 0.2f : 1.5f), 3, l.z - (vertical ? 1.5f : 0.2f) },
                                 Vector3{ l.x + (vertical ? 0.2f : 1.5f), wallH, l.z + (vertical ? 1.5f : 0.2f) }, theme.wallUV, 0, 1 | 2 | 8 | 16 | 32);
    solids.push_back(BoundingBox{ Vector3{ l.x - (vertical ? 0.2f : 1.5f), 3, l.z - (vertical ? 1.5f : 0.2f) }, Vector3{ l.x + (vertical ? 0.2f : 1.5f), wallH, l.z + (vertical ? 1.5f : 0.2f) } });
    if (l.open) return;
    float w = vertical ? 0.3f : 3, d = vertical ? 3 : 0.3f;
    Door door;
    door.link = li;
    door.pos = Vector3{ l.x, 1.5f, l.z };
    door.size = Vector3{ w, 3, d };
    door.col = Box2{ l.x - w / 2, l.x + w / 2, l.z - d / 2, l.z + d / 2 };
    colliders.push_back(door.col);
    solids.push_back(BoundingBox{ Vector3{ l.x - w / 2, 0, l.z - d / 2 }, Vector3{ l.x + w / 2, 3, l.z + d / 2 } });
    char txt[32]; snprintf(txt, 32, "PUERTA · %d", l.cost);
    Texture2D lt = Lbl(txt, Color{ 255, 216, 74, 255 });
    for (int s = -1; s <= 1; s += 2) labels.push_back({ lt, Vector3{ l.x + (vertical ? s * 0.45f : 0), 2.45f, l.z + (vertical ? 0 : s * 0.45f) }, 1.9f, true });
    doors.push_back(door);
    int di = (int)doors.size() - 1;
    int lab0 = (int)labels.size() - 2;
    Interact it;
    it.pos = [li]() { return Vector2{ links[li].x, links[li].z }; };
    it.range = 2.6f;
    it.active = [li]() { return !links[li].open; };
    it.text = [li]() { char b[64]; snprintf(b, 64, "Presiona E para abrir la puerta [%d]", links[li].cost); return std::string(b); };
    it.action = [li, di, lab0]() {
        if (!Spend(links[li].cost)) return;
        links[li].open = true;
        Door &dr = doors[di];
        // se anulan (no se borran) para no mover los índices de otras colisiones
        for (auto &c : colliders)
            if (c.minX == dr.col.minX && c.minZ == dr.col.minZ && c.maxX == dr.col.maxX && c.maxZ == dr.col.maxZ) { c = Box2{ 1e6f, 1e6f, 1e6f, 1e6f }; break; }
        for (auto &s : solids)
            if (s.min.x == dr.col.minX && s.min.z == dr.col.minZ && s.max.y == 3) { s = BoundingBox{ { 1e6f, 1e6f, 1e6f }, { 1e6f, 1e6f, 1e6f } }; break; }
        labels[lab0].visible = labels[lab0 + 1].visible = false;
        dr.sinking = true;
        RecomputeUnlocked();
        Play("door");
    };
    interactables.push_back(it);
}

// ---------- Cajas y barriles con física (deslizan, saltan con las explosiones y ruedan) ----------
struct DynProp { int prop, col, solid; Vector3 pos, vel; float w, d, h, yaw, spin; bool barrel, awake; };
static std::vector<DynProp> dynProps;

// ---------- Árboles de verdad ("Trees" y "Dead Trees" CC0 de Quaternius) ----------
// Cada archivo trae varias variantes en fila; se separan, se centran y se escalan a la altura pedida.
struct TreeVariant { std::vector<std::pair<Mesh *, PMat *>> parts; Vector3 base; float height; };
static std::vector<TreeVariant> treeKinds[2];   // 0: con hojas, 1: secos
static PMat *ModelMat(const Material &src) {
    PMat *pm = new PMat(M("wood"));
    MaterialMap *maps = (MaterialMap *)MemAlloc(sizeof(MaterialMap) * 12 /* MAX_MATERIAL_MAPS de raylib */);
    memcpy(maps, pm->m.maps, sizeof(MaterialMap) * 12);
    pm->m.maps = maps;
    pm->m.maps[MATERIAL_MAP_ALBEDO].texture = src.maps[MATERIAL_MAP_ALBEDO].texture.id ? src.maps[MATERIAL_MAP_ALBEDO].texture : texWhite;
    pm->m.maps[MATERIAL_MAP_ALBEDO].color = WHITE;
    pm->m.maps[MATERIAL_MAP_NORMAL].texture = src.maps[MATERIAL_MAP_NORMAL].texture.id ? src.maps[MATERIAL_MAP_NORMAL].texture : texFlat;
    pm->m.maps[MATERIAL_MAP_ROUGHNESS].texture = texWhite;
    pm->metal = 0; pm->rough = 0.95f; pm->nstr = 1;
    return pm;
}
static void LoadTrees() {
    static bool done = false;
    if (done) return;
    done = true;
    const char *files[2] = { "assets/models/trees.glb", "assets/models/deadtrees.glb" };
    for (int k = 0; k < 2; k++) {
        if (!FileExists(files[k])) continue;
        Model *m = new Model(LoadModel(files[k]));
        void ComputeTangents(Mesh &m, bool dxNormals);
        std::vector<PMat *> mats;
        for (int i = 0; i < m->materialCount; i++) mats.push_back(ModelMat(m->materials[i]));
        // las variantes están separadas en X: se agrupan las mallas que se solapan
        for (int i = 0; i < m->meshCount; i++) {
            ComputeTangents(m->meshes[i], false);
            BoundingBox b = GetMeshBoundingBox(m->meshes[i]);
            float cx = (b.min.x + b.max.x) / 2;
            TreeVariant *tv = nullptr;
            for (auto &v : treeKinds[k]) if (fabsf(v.base.x - cx) < 1.6f) tv = &v;
            if (!tv) { treeKinds[k].push_back(TreeVariant{ {}, Vector3{ cx, b.min.y, (b.min.z + b.max.z) / 2 }, 0 }); tv = &treeKinds[k].back(); }
            tv->parts.push_back({ &m->meshes[i], mats[m->meshMaterial[i]] });
            tv->height = std::max(tv->height, b.max.y - tv->base.y);
        }
        // fuera los muy bajitos (tocones)
        treeKinds[k].erase(std::remove_if(treeKinds[k].begin(), treeKinds[k].end(), [](const TreeVariant &v) { return v.height < 4; }), treeKinds[k].end());
    }
}
static bool BuildModelTree(const PropDef &p, int kind) {
    LoadTrees();
    auto &kinds = treeKinds[kind];
    if (kinds.empty()) return false;
    int vi = (int)(fabsf(p.x * 7.3f + p.z * 3.1f)) % (int)kinds.size();
    const TreeVariant &v = kinds[vi];
    float s = p.h / v.height, ry = fmodf(p.x * 1.7f + p.z * 0.9f, 2 * PI_F);
    Matrix tf = MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixTranslate(-v.base.x, -v.base.y, -v.base.z), MatrixScale(s, s, s)), MatrixRotateY(ry)), MatrixTranslate(p.x, 0, p.z));
    for (auto &part : v.parts) props.push_back({ part.first, part.second, tf });
    AddSolidBox(Vector3{ p.x - p.w / 2, 0, p.z - p.d / 2 }, Vector3{ p.x + p.w / 2, p.h * 0.6f, p.z + p.d / 2 }, true);
    return true;
}

// Objetos compuestos del mapa al aire libre: autos, sacos, lápidas, bancos, árboles y la fuente
static bool BuildFancyProp(const PropDef &p) {
    if (!strcmp(p.type, "tree") && BuildModelTree(p, 0)) return true;
    if (!strcmp(p.type, "deadtree") && (BuildModelTree(p, 1) || BuildModelTree(p, 0))) return true;
    auto add = [](Mesh *mesh, PMat *mat, Vector3 pos, Vector3 size, float ry = 0) { props.push_back({ mesh, mat, TRSm(pos, ry, size) }); };
    auto solid = [&](float top, bool climb) {
        AddSolidBox(Vector3{ p.x - p.w / 2, 0, p.z - p.d / 2 }, Vector3{ p.x + p.w / 2, top, p.z + p.d / 2 }, true);
        if (climb) colliders.back().top = top;
    };
    if (!strcmp(p.type, "car")) {   // auto abandonado: carrocería, cabina, ruedas y faros rotos
        add(&unitCube, &M("rust"), Vector3{ p.x, 0.65f, p.z }, Vector3{ p.w, 0.8f, p.d });
        add(&unitCube, &M("darkmetal"), Vector3{ p.x, 1.3f, p.z + p.d * 0.05f }, Vector3{ p.w * 0.9f, 0.55f, p.d * 0.48f });
        for (int i = 0; i < 4; i++)   // ruedas: cilindros tumbados
            props.push_back({ &unitCyl, &M("darkmetal"), TRSm(Vector3{ p.x + (i & 1 ? 1 : -1) * p.w * 0.47f, 0.35f, p.z + (i & 2 ? 1 : -1) * p.d * 0.32f }, 0, Vector3{ 0.7f, 0.25f, 0.7f }, 0, PI_F / 2) });
        solid(1.55f, true);
        return true;
    }
    if (!strcmp(p.type, "sandbag")) {   // muro de sacos de arena
        int n = std::max(2, (int)(std::max(p.w, p.d) / 0.6f));
        bool alongX = p.w >= p.d;
        for (int row = 0; row < 3; row++)
            for (int i = 0; i < n - (row & 1); i++) {
                float t = -0.5f + (i + 0.5f + (row & 1) * 0.5f) / n;
                Vector3 pos{ p.x + (alongX ? t * p.w : 0), 0.17f + row * 0.32f, p.z + (alongX ? 0 : t * p.d) };
                add(&unitSphere, &M("ground"), pos, alongX ? Vector3{ 0.62f, 0.34f, p.d * 0.9f } : Vector3{ p.w * 0.9f, 0.34f, 0.62f });
            }
        solid(p.h, true);
        return true;
    }
    if (!strcmp(p.type, "tomb")) {   // lápida con su base
        add(&unitCube, &M("concwall"), Vector3{ p.x, p.h / 2 + 0.1f, p.z }, Vector3{ p.w, p.h, p.d }, frand(-0.08f, 0.08f));
        add(&unitCube, &M("concwall"), Vector3{ p.x, 0.05f, p.z + 0.5f }, Vector3{ p.w + 0.3f, 0.12f, 1.6f });
        solid(p.h + 0.1f, true);
        return true;
    }
    if (!strcmp(p.type, "pew")) {   // banco o mesa de madera
        add(&unitCube, &M("planks"), Vector3{ p.x, p.h - 0.06f, p.z }, Vector3{ p.w, 0.12f, p.d });
        add(&unitCube, &M("planks"), Vector3{ p.x - p.w * 0.4f, p.h / 2 - 0.06f, p.z }, Vector3{ 0.1f, p.h - 0.12f, p.d * 0.9f });
        add(&unitCube, &M("planks"), Vector3{ p.x + p.w * 0.4f, p.h / 2 - 0.06f, p.z }, Vector3{ 0.1f, p.h - 0.12f, p.d * 0.9f });
        solid(p.h, true);
        return true;
    }
    if (!strcmp(p.type, "tree")) {   // árbol seco: tronco y copa oscura
        add(&unitCyl, &M("wood"), Vector3{ p.x, p.h * 0.35f, p.z }, Vector3{ p.w, p.h * 0.7f, p.d });
        add(&unitSphere, Recolored("ground", Color{ 70, 95, 55, 255 }), Vector3{ p.x, p.h * 0.72f, p.z }, Vector3{ 3.6f, 2.6f, 3.6f });
        add(&unitSphere, Recolored("ground", Color{ 70, 95, 55, 255 }), Vector3{ p.x + 0.8f, p.h * 0.88f, p.z - 0.4f }, Vector3{ 2.6f, 2.0f, 2.6f });
        AddSolidBox(Vector3{ p.x - p.w / 2, 0, p.z - p.d / 2 }, Vector3{ p.x + p.w / 2, p.h * 0.7f, p.z + p.d / 2 }, true);
        return true;
    }
    if (!strcmp(p.type, "fountain")) {   // fuente de piedra de la plaza
        add(&unitCyl, &M("concwall"), Vector3{ p.x, p.h / 2, p.z }, Vector3{ p.w, p.h, p.d });
        add(&unitCyl, &M("darkmetal"), Vector3{ p.x, p.h - 0.02f, p.z }, Vector3{ p.w * 0.85f, 0.06f, p.d * 0.85f });
        add(&unitCyl, &M("concwall"), Vector3{ p.x, p.h + 0.6f, p.z }, Vector3{ 0.45f, 1.2f, 0.45f });
        add(&unitCyl, &M("concwall"), Vector3{ p.x, p.h + 1.25f, p.z }, Vector3{ 1.3f, 0.15f, 1.3f });
        solid(p.h, true);
        return true;
    }
    return false;
}

static void BuildProp(const PropDef &p) {
    if (BuildFancyProp(p)) return;
    Prop pr;
    bool barrel = !strcmp(p.type, "barrel");
    if (barrel) { pr.mesh = &unitCyl; pr.mat = &M("barrel"); }
    else { pr.mesh = &unitCube; pr.mat = &M(!strcmp(p.type, "metal") ? "rust" : "crate"); }
    pr.tf = TRSm(Vector3{ p.x, p.h / 2, p.z }, 0, Vector3{ p.w, p.h, p.d });
    props.push_back(pr);
    AddSolidBox(Vector3{ p.x - p.w / 2, 0, p.z - p.d / 2 }, Vector3{ p.x + p.w / 2, p.h, p.z + p.d / 2 }, true);
    // se puede subir encima (salvo lo muy alto bajo techo, que dejaría la cabeza dentro del techo)
    if (p.h <= 2.0f || curMap->outdoor) colliders.back().top = p.h;
    // solo las cajas y barriles pequeños se mueven (los contenedores grandes no)
    if ((barrel || !strcmp(p.type, "crate")) && p.h <= 1.3f)
        dynProps.push_back({ (int)props.size() - 1, (int)colliders.size() - 1, (int)solids.size() - 1, Vector3{ p.x, 0, p.z }, Vector3{ 0, 0, 0 },
                             p.w, p.d, p.h, 0, 0, barrel, false });
}

static void UpdateDynProps(float dt) {
    for (auto &dp : dynProps) {
        float hw = dp.w / 2, hd = dp.d / 2;
        // el jugador empuja al caminar contra ella
        if (game.state == GS_PLAYING) {
            float cx = clampf(player.pos.x, dp.pos.x - hw, dp.pos.x + hw), cz = clampf(player.pos.z, dp.pos.z - hd, dp.pos.z + hd);
            float dx = cx - player.pos.x, dz = cz - player.pos.z, dist = hyp(dx, dz);
            if (dist < 0.46f && dist > 0.001f && player.pos.y < dp.pos.y + dp.h) {
                Vector3 dir{ dx / dist, 0, dz / dist };
                float pv = player.vel.x * dir.x + player.vel.z * dir.z;
                float cur = dp.vel.x * dir.x + dp.vel.z * dir.z;
                if (pv > 0.4f && cur < pv * 0.8f) {
                    float add = pv * 0.8f - cur;
                    dp.vel.x += dir.x * add; dp.vel.z += dir.z * add;
                    if (dp.barrel) dp.spin += add * frand(-1.5f, 1.5f);
                    dp.awake = true;
                }
            }
        }
        if (!dp.awake) continue;
        // movimiento con gravedad, rebote y rozamiento
        dp.vel.y -= 18 * dt;
        dp.pos = Vector3Add(dp.pos, Vector3Scale(dp.vel, dt));
        if (dp.pos.y <= 0) {
            dp.pos.y = 0;
            if (dp.vel.y < -2.5f) { dp.vel.y *= -0.25f; Play("board", 0.35f); }
            else dp.vel.y = 0;
            float f = expf(-(dp.barrel ? 2.5f : 5.0f) * dt);
            dp.vel.x *= f; dp.vel.z *= f;
        }
        dp.yaw += dp.spin * dt;
        dp.spin *= expf(-2 * dt);
        // choca con paredes y otros objetos (sin chocar consigo misma)
        Box2 own = colliders[dp.col];
        colliders[dp.col] = Box2{ 1e6f, 1e6f, 1e6f, 1e6f };
        Vector3 c{ dp.pos.x, 0, dp.pos.z };
        Collide(c, std::min(hw, hd) * 0.95f);
        if (c.x != dp.pos.x) dp.vel.x *= -0.2f;
        if (c.z != dp.pos.z) dp.vel.z *= -0.2f;
        dp.pos.x = c.x; dp.pos.z = c.z;
        (void)own;
        colliders[dp.col] = Box2{ dp.pos.x - hw, dp.pos.x + hw, dp.pos.z - hd, dp.pos.z + hd, dp.pos.y + dp.h };
        solids[dp.solid] = BoundingBox{ Vector3{ dp.pos.x - hw, dp.pos.y, dp.pos.z - hd }, Vector3{ dp.pos.x + hw, dp.pos.y + dp.h, dp.pos.z + hd } };
        props[dp.prop].tf = TRSm(Vector3{ dp.pos.x, dp.pos.y + dp.h / 2, dp.pos.z }, dp.yaw, Vector3{ dp.w, dp.h, dp.d });
        // se duerme cuando se queda quieta en el suelo
        if (dp.pos.y <= 0 && fabsf(dp.vel.x) + fabsf(dp.vel.z) < 0.05f && fabsf(dp.spin) < 0.05f) { dp.vel = Vector3{ 0, 0, 0 }; dp.awake = false; }
    }
}

// Explosión (o la onda del Cañón de Trueno si dir no es cero): empuja los objetos cercanos
void PushProps(Vector3 p, float radius, float strength, Vector3 dir) {
    for (auto &dp : dynProps) {
        float dx = dp.pos.x - p.x, dz = dp.pos.z - p.z, d = hyp(dx, dz);
        if (d > radius) continue;
        Vector3 out = d > 0.05f ? Vector3{ dx / d, 0, dz / d } : Vector3{ 0, 0, 1 };
        if (dir.x != 0 || dir.z != 0) {
            if (d > 1.5f && (out.x * dir.x + out.z * dir.z) < 0.75f) continue;   // fuera del cono
            out = dir;
        }
        float k = strength * (1 - d / radius) * (dp.barrel ? 1.4f : 1.0f);
        dp.vel.x += out.x * 6 * k; dp.vel.z += out.z * 6 * k;
        dp.vel.y += 4.5f * k;
        dp.spin += frand(-4, 4) * k;
        dp.awake = true;
    }
}
// Disparo que da en un objeto: un empujoncito
void PokeProp(Vector3 hit, Vector3 dir) {
    for (auto &dp : dynProps) {
        if (fabsf(hit.x - dp.pos.x) > dp.w / 2 + 0.05f || fabsf(hit.z - dp.pos.z) > dp.d / 2 + 0.05f || hit.y > dp.pos.y + dp.h + 0.05f) continue;
        dp.vel.x += dir.x * 0.5f; dp.vel.z += dir.z * 0.5f;
        if (dp.barrel) dp.spin += frand(-1, 1);
        dp.awake = true;
        return;
    }
}

static void MakeWallBuy(const BuyDef &b) {
    int room = RoomIndex(b.room);
    const WeaponDef &d = WEAPONS[b.weapon];
    WallPlace p = WallPlaceAt(room, b.x, b.z);
    dynMats.push_back(PMat());
    PMat &pm = dynMats.back();
    pm = M("polymer");
    pm.m.maps[MATERIAL_MAP_ALBEDO].texture = ChalkTexture(d);
    pm.m.maps[MATERIAL_MAP_ALBEDO].color = WHITE;
    pm.m.maps[MATERIAL_MAP_NORMAL].texture = texFlat;
    pm.rough = 1; pm.nstr = 0;
    Prop pr{ &unitCube, &pm, TRSm(Vector3{ b.x + p.nx * 0.205f, 1.7f, b.z + p.nz * 0.205f }, p.ry, Vector3{ 1.8f, 0.9f, 0.01f }) };
    props.push_back(pr);
    mmIcons.push_back({ b.x, b.z, Color{ 255, 216, 74, 255 }, 0.5f });
    int id = b.weapon;
    Vector2 pos{ b.x + p.nx * 1.0f, b.z + p.nz * 1.0f };
    Interact it;
    it.pos = [pos]() { return pos; };
    it.range = 1.8f;
    it.active = []() { return !player.papBusy; };
    it.text = [id]() {
        const WeaponDef &d = WEAPONS[id];
        char buf[96];
        for (auto &w : player.weapons) if (w.id == id) { return "Presiona E para comprar munición " + Price(w.pap ? 2500 : d.cost / 2); }
        snprintf(buf, 96, Practice() ? "Presiona E para tomar %s [GRATIS]" : "Presiona E para comprar %s [%d]", d.name, d.cost);
        return std::string(buf);
    };
    it.action = [id]() {
        const WeaponDef &d = WEAPONS[id];
        for (auto &w : player.weapons) if (w.id == id) {
            WStats s = Stats(w.id, w.pap);
            if (w.reserve >= s.reserve) return;
            if (Spend(w.pap ? 2500 : d.cost / 2)) w.reserve = s.reserve;
            return;
        }
        if (Spend(d.cost)) GiveWeapon(id);
    };
    interactables.push_back(it);
}

static void MakePerk(const PerkPlace &pp) {
    int room = RoomIndex(pp.room);
    const PerkDef &pk = PERKS[pp.perk];
    WallPlace p = WallPlaceAt(room, pp.x, pp.z);
    float cx = pp.x + p.nx * 0.65f, cz = pp.z + p.nz * 0.65f;
    char mn[16]; snprintf(mn, 16, "perk%d", pp.perk);
    props.push_back({ &unitCube, &M(mn), TRSm(Vector3{ cx, 1.1f, cz }, p.ry, Vector3{ 1.1f, 2.2f, 0.8f }) });
    // cartel luminoso y ranura
    dynMats.push_back(M("fixture"));
    dynMats.back().emis = Vector3Scale(ColorV(pk.color), 3.0f);
    Matrix base = MatrixMultiply(MatrixRotateY(p.ry), MatrixTranslate(cx, 1.1f, cz));
    props.push_back({ &unitCube, &dynMats.back(), MatrixMultiply(MatrixMultiply(MatrixScale(0.9f, 0.35f, 0.05f), MatrixTranslate(0, 0.7f, 0.42f)), base) });
    props.push_back({ &unitCube, &M("darkmetal"), MatrixMultiply(MatrixMultiply(MatrixScale(0.5f, 0.4f, 0.05f), MatrixTranslate(0, -0.5f, 0.42f)), base) });
    char txt[64]; snprintf(txt, 64, Practice() ? "%s · GRATIS" : "%s · %d", pk.name, pk.cost);
    labels.push_back({ Lbl(txt, Hex(pk.color)), Vector3{ cx, 2.65f, cz }, 2.3f, true });
    lights.push_back({ Vector3{ cx + p.nx * 0.9f, 1.6f, cz + p.nz * 0.9f }, Vector3Scale(ColorV(pk.color), 2.5f), 5.0f, false, 1, false });
    powerVis.push_back({ (int)lights.size() - 1, &dynMats.back(), dynMats.back().emis, pp.perk });
    colliders.push_back(Box2{ cx - 0.55f, cx + 0.55f, cz - 0.55f, cz + 0.55f });
    solids.push_back(BoundingBox{ Vector3{ cx - 0.55f, 0, cz - 0.55f }, Vector3{ cx + 0.55f, 2.2f, cz + 0.55f } });
    mmIcons.push_back({ cx, cz, Hex(pk.color), 0.7f });
    int k = pp.perk;
    Vector2 pos{ cx + p.nx * 1.0f, cz + p.nz * 1.0f };
    Interact it;
    it.pos = [pos]() { return pos; };
    it.range = 1.8f;
    it.active = [k]() { return !HasPerk(player, k); };
    it.text = [k]() {
        if (!powerOn && NeedsPower(k)) return std::string("Primero tienes que activar la corriente");
        char b[128]; snprintf(b, 128, "Presiona E para beber %s (%s) ", PERKS[k].name, PERKS[k].desc); return std::string(b) + Price(PERKS[k].cost);
    };
    it.action = [k]() {
        if (HasPerk(player, k) || (!powerOn && NeedsPower(k)) || !Spend(PERKS[k].cost)) return;
        player.perks |= 1u << k;
        if (k == P_JUGG) { player.maxHealth = 250; player.health = 250; }
        Play("perk");
        CenterMsg(PERKS[k].name, Hex(PERKS[k].color));
    };
    interactables.push_back(it);
}

// ---------- Caja misteriosa ----------
static void PlaceBox(int idx) {
    const PlaceDef &s = curMap->boxSpots[idx];
    int room = RoomIndex(s.room);
    WallPlace p = WallPlaceAt(room, s.x, s.z);
    float cx = s.x + p.nx * 0.6f, cz = s.z + p.nz * 0.6f;
    float hx = p.nx ? 0.4f : 0.9f, hz = p.nx ? 0.9f : 0.4f;
    Box2 col{ cx - hx, cx + hx, cz - hz, cz + hz, 0.85f };   // a la caja misteriosa también se puede subir
    if (box.colIdx >= 0 && box.colIdx < (int)colliders.size()) colliders[box.colIdx] = col;
    else { colliders.push_back(col); box.colIdx = (int)colliders.size() - 1; }
    box.front = Vector2{ cx + p.nx * 1.2f, cz + p.nz * 1.2f };
    box.pos = Vector2{ cx, cz };
    box.ry = p.ry;
    box.spot = idx;
    if (boxLabel >= 0) labels[boxLabel].pos = Vector3{ cx, 2.2f, cz };
}

static void BuildBox() {
    box = BoxState();
    box.colIdx = -1;
    labels.push_back({ Lbl(Practice() ? "CAJA MISTERIOSA · GRATIS" : "CAJA MISTERIOSA · 950", Color{ 127, 212, 255, 255 }), Vector3{ 0, 2.2f, 0 }, 2.5f, true });
    boxLabel = (int)labels.size() - 1;
    PlaceBox(0);
    Interact it;
    it.pos = []() { return box.front; };
    it.range = 2.0f;
    it.active = []() { return (box.state == BOX_IDLE || box.state == BOX_OFFER) && !player.papBusy; };
    it.text = []() {
        if (box.state == BOX_OFFER) return std::string("Presiona E para tomar ") + WEAPONS[box.offer].name;
        return "Presiona E para la Caja Misteriosa " + Price(950);
    };
    it.action = []() {
        if (box.state == BOX_IDLE) {
            if (!Spend(950)) return;
            box.uses++;
            box.state = BOX_ROLLING; box.t = 0; box.swapT = 0;
            Play("box");
        } else if (box.state == BOX_OFFER) {
            GiveWeapon(box.offer);
            box.state = BOX_CLOSING; box.t = 0; box.show = -1;
        }
    };
    interactables.push_back(it);
}

void UpdateBox(float dt) {
    BoxState &b = box;
    bool open = b.state == BOX_ROLLING || b.state == BOX_OFFER || b.state == BOX_TEDDY;
    b.lid += ((open ? 1.2f : 0) - b.lid) * dampk(8, dt);
    if (b.state == BOX_ROLLING) {
        b.t += dt; b.swapT -= dt;
        if (b.swapT <= 0) { b.show = BOX_POOL[irand((int)BOX_POOL.size())]; b.swapT = 0.06f + b.t * 0.05f; }
        b.lift = 0.5f + std::min(b.t / 4, 1.0f) * 0.9f;
        if (b.t > 4) {
            if (b.uses >= 4 && frand(0, 1) < 0.2f && curMap->boxSpots.size() > 1) {
                b.state = BOX_TEDDY; b.t = 0; b.show = -1; b.teddy = true;
                Play("laugh");
            } else {
                std::vector<int> options;
                for (int id : BOX_POOL) {
                    bool owned = false;
                    for (auto &w : player.weapons) if (w.id == id) owned = true;
                    if (!owned) options.push_back(id);
                }
                if (options.empty()) options = BOX_POOL;
                b.offer = options[irand((int)options.size())];
                b.show = b.offer;
                b.state = BOX_OFFER; b.t = 0;
            }
        }
    } else if (b.state == BOX_OFFER) {
        b.t += dt;
        b.lift = 1.4f - b.t * 0.08f;
        if (b.t > 10) { b.show = -1; b.state = BOX_CLOSING; b.t = 0; }
    } else if (b.state == BOX_CLOSING) {
        b.t += dt;
        if (b.t > 0.5f) b.state = BOX_IDLE;
    } else if (b.state == BOX_TEDDY) {
        b.t += dt;
        b.teddyRot += dt * 3;
        if (b.t > 2.5f) { b.teddy = false; b.state = BOX_LEAVING; b.t = 0; CenterMsg("¡LA CAJA SE ESTÁ MOVIENDO!", Color{ 127, 212, 255, 255 }); }
    } else if (b.state == BOX_LEAVING) {
        b.t += dt;
        b.spin += dt * b.t * 4;
        if (b.t > 3) {
            int idx;
            do idx = irand((int)curMap->boxSpots.size()); while (idx == b.spot);
            PlaceBox(idx);
            b.spin = 0;
            player.points += 950;
            CenterMsg("Se te devolvieron 950 puntos", Color{ 255, 216, 74, 255 });
            b.state = BOX_IDLE;
        }
    }
}

// ---------- Pack-a-Punch ----------
static void BuildPap() {
    const PlaceDef &s = curMap->pap;
    int room = RoomIndex(s.room);
    WallPlace p = WallPlaceAt(room, s.x, s.z);
    float cx = s.x + p.nx * 0.85f, cz = s.z + p.nz * 0.85f;
    pap = PapState();
    pap.pos = Vector3{ cx, 0, cz };
    pap.ry = p.ry;
    float hx = p.nx ? 0.6f : 0.85f, hz = p.nx ? 0.85f : 0.6f;
    colliders.push_back(Box2{ cx - hx, cx + hx, cz - hz, cz + hz });
    solids.push_back(BoundingBox{ Vector3{ cx - hx, 0, cz - hz }, Vector3{ cx + hx, 1.8f, cz + hz } });
    pap.front = Vector2{ cx + p.nx * 1.4f, cz + p.nz * 1.4f };
    labels.push_back({ Lbl(Practice() ? "PACK-A-PUNCH · GRATIS" : "PACK-A-PUNCH · 5000", Color{ 208, 140, 255, 255 }), Vector3{ cx, 2.7f, cz }, 2.5f, true });
    papLabel = (int)labels.size() - 1;
    lights.push_back({ Vector3{ cx + p.nx * 1.0f, 2.0f, cz + p.nz * 1.0f }, Vector3{ 2.0f, 0.6f, 4.0f }, 6.0f, false, 1, false });
    papLight = (int)lights.size() - 1;
    mmIcons.push_back({ cx, cz, Color{ 176, 74, 255, 255 }, 0.9f });
    Interact it;
    it.pos = []() { return pap.front; };
    it.range = 2.0f;
    it.active = []() { return pap.state == 0; };
    it.text = []() {
        if (!powerOn) return std::string("Primero tienes que activar la corriente");
        if (CurWeapon().pap) return std::string("Tu arma ya está mejorada");
        return "Presiona E para mejorar tu arma con Pack-a-Punch " + Price(5000);
    };
    it.action = []() {
        OwnedWeapon &w = CurWeapon();
        if (!powerOn || w.pap || player.papBusy || !Spend(5000)) return;
        pap.state = 1; pap.t = 0; pap.weaponIdx = player.cur;
        player.papBusy = true; player.reloading = false;
        Play("pap");
    };
    interactables.push_back(it);
}

void UpdatePap(float dt) {
    if (pap.state != 1) return;
    pap.t += dt;
    pap.roll += dt * 12;
    if (pap.t > 3.5f) {
        OwnedWeapon &w = player.weapons[pap.weaponIdx];
        w.pap = true;
        WStats s = Stats(w.id, true);
        w.mag = s.mag; w.reserve = s.reserve;
        player.papBusy = false;
        player.cur = pap.weaponIdx;
        Equip();
        CenterMsg(s.name, Color{ 208, 140, 255, 255 });
        Play("perk");
        pap.state = 0;
    }
}

void DrawBoxAndPap() {
    // caja
    Matrix base = MatrixMultiply(MatrixRotateY(box.ry + box.spin), MatrixTranslate(box.pos.x, box.state == BOX_LEAVING ? box.t * box.t * 1.5f : 0, box.pos.y));
    auto part = [&](Mesh &m, PMat &pm, Vector3 s, Vector3 p, Matrix extra = MatrixIdentity()) {
        DrawPM(m, pm, MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixScale(s.x, s.y, s.z), MatrixTranslate(p.x, p.y, p.z)), extra), base));
    };
    part(unitCube, M("crate"), Vector3{ 1.8f, 0.75f, 0.8f }, Vector3{ 0, 0.375f, 0 });
    // tapa con bisagra atrás
    Matrix lid = MatrixMultiply(MatrixMultiply(MatrixTranslate(0, 0, 0.4f), MatrixRotateX(-box.lid)), MatrixTranslate(0, 0.75f, -0.4f));
    DrawPM(unitCube, M("crate"), MatrixMultiply(MatrixMultiply(MatrixScale(1.84f, 0.1f, 0.84f), MatrixTranslate(0, 0.05f, 0)), MatrixMultiply(lid, base)));
    if (box.show >= 0 && !depthPass) {
        Matrix g = MatrixMultiply(MatrixMultiply(MatrixScale(2.0f, 2.0f, 2.0f), MatrixRotateY(PI_F / 2)), MatrixTranslate(0.45f, box.lift, 0));
        DrawGun(gunModels[box.show], MatrixMultiply(g, base), false, false);
    }
    if (box.teddy) {
        Matrix tb = MatrixMultiply(MatrixMultiply(MatrixRotateY(box.teddyRot), MatrixTranslate(0, 1.3f, 0)), base);
        auto ball = [&](float r, float x, float y, float z) { DrawPM(unitSphere, M("fur"), MatrixMultiply(MatrixMultiply(MatrixScale(r * 2, r * 2, r * 2), MatrixTranslate(x, y, z)), tb)); };
        ball(0.22f, 0, 0, 0); ball(0.16f, 0, 0.3f, 0); ball(0.06f, -0.12f, 0.44f, 0); ball(0.06f, 0.12f, 0.44f, 0);
        ball(0.08f, -0.22f, 0.05f, 0.05f); ball(0.08f, 0.22f, 0.05f, 0.05f); ball(0.09f, -0.1f, -0.22f, 0.05f); ball(0.09f, 0.1f, -0.22f, 0.05f);
    }
    // Pack-a-Punch
    Matrix pb = MatrixMultiply(MatrixRotateY(pap.ry), MatrixTranslate(pap.pos.x, 0, pap.pos.z));
    auto pp = [&](Mesh &m, PMat &pm, Vector3 s, Vector3 p, Matrix extra = MatrixIdentity()) {
        DrawPM(m, pm, MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixScale(s.x, s.y, s.z), extra), MatrixTranslate(p.x, p.y, p.z)), pb));
    };
    pp(unitCube, M("rust"), Vector3{ 1.7f, 1.2f, 1.2f }, Vector3{ 0, 0.6f, 0 });
    PMat &glow = M("papglow");
    Vector3 ge = glow.emis;
    if (!powerOn) glow.emis = Vector3Scale(ge, 0.05f);
    else if (pap.state == 1) glow.emis = Vector3Scale(ge, frand(0.8f, 2.2f));
    else glow.emis = Vector3Scale(ge, 0.8f + sinf(T * 2) * 0.25f);
    pp(unitCube, glow, Vector3{ 1.5f, 0.6f, 1.0f }, Vector3{ 0, 1.5f, 0 });
    glow.emis = ge;
    for (float rz : { -0.25f, 0.25f })
        pp(unitCyl, M("metal"), Vector3{ 0.24f, 1.3f, 0.24f }, Vector3{ 0, 1.9f, rz }, MatrixMultiply(MatrixRotateY(pap.roll), MatrixRotateZ(PI_F / 2)));
    pp(unitCube, M("darkmetal"), Vector3{ 1.0f, 0.9f, 0.02f }, Vector3{ 0, 0.6f, 0.61f });
}

// ---------- Interruptor de la corriente ----------
// k: 0 = sin corriente, 1 = encendida. Las máquinas y el Pack-a-Punch se apagan y las salas quedan en penumbra.
static void PowerVisuals(float k) {
    for (auto &pv : powerVis) {
        bool on = !NeedsPower(pv.perk) || k > 0.5f;
        lights[pv.light].k = on ? (NeedsPower(pv.perk) ? k : 1) : 0;
        pv.sign->emis = Vector3Scale(pv.emis, on ? std::max(0.08f, k) : 0.08f);
    }
    if (papLight >= 0) lights[papLight].k = k;
    for (int i = 0; i < roomLights; i++) lights[i].k = lerpf(0.45f, 1.0f, k);
}

static void BuildPowerSwitch() {
    powerOn = curMap->power.room == nullptr;
    powerT = powerOn ? 99 : -1;
    leverK = powerOn ? 1 : 0;
    if (powerOn) return;
    const PlaceDef &s = curMap->power;
    WallPlace p = WallPlaceAt(RoomIndex(s.room), s.x, s.z);
    float cx = s.x + p.nx * 0.28f, cz = s.z + p.nz * 0.28f;
    switchBase = MatrixMultiply(MatrixRotateY(p.ry), MatrixTranslate(cx, 0, cz));
    // caja eléctrica, cables y cartel
    props.push_back({ &unitCube, &M("darkmetal"), MatrixMultiply(MatrixMultiply(MatrixScale(0.7f, 0.95f, 0.16f), MatrixTranslate(0, 1.45f, 0)), switchBase) });
    props.push_back({ &unitCube, &M("rust"), MatrixMultiply(MatrixMultiply(MatrixScale(0.6f, 0.08f, 0.04f), MatrixTranslate(0, 1.82f, 0.09f)), switchBase) });
    for (float x : { -0.22f, 0.22f })
        props.push_back({ &unitCyl, &M("polymer"), MatrixMultiply(MatrixMultiply(MatrixScale(0.05f, H - 1.9f, 0.05f), MatrixTranslate(x, 1.9f + (H - 1.9f) / 2, -0.04f)), switchBase) });
    labels.push_back({ Lbl("CORRIENTE", Color{ 255, 216, 74, 255 }), Vector3{ cx + p.nx * 0.1f, 2.25f, cz + p.nz * 0.1f }, 1.6f, true });
    int lab = (int)labels.size() - 1;
    mmIcons.push_back({ cx, cz, Color{ 255, 230, 60, 255 }, 0.6f });
    Vector2 pos{ cx + p.nx * 0.9f, cz + p.nz * 0.9f };
    Interact it;
    it.pos = [pos]() { return pos; };
    it.range = 1.8f;
    it.active = []() { return !powerOn; };
    it.text = []() { return std::string("Presiona E para activar la corriente"); };
    it.action = [lab]() {
        if (powerOn) return;
        powerOn = true; powerT = 0;
        labels[lab].visible = false;
        Play("power");
        CenterMsg("¡CORRIENTE ACTIVADA!", Color{ 255, 230, 60, 255 }, 3);
        SpecialMsg("Las bebidas y el Pack-a-Punch ya funcionan");
        WhiteFlash(0.35f);
        vm.shake = std::max(vm.shake, 0.25f);
    };
    interactables.push_back(it);
}

static void UpdatePower(float dt) {
    leverK = std::min(1.0f, leverK + (powerOn ? dt * 4 : 0));
    if (powerT < 0) { PowerVisuals(0); return; }
    if (powerT > 50) return;
    powerT += dt;
    // las luces parpadean un par de segundos y se quedan encendidas
    float k = powerT < 2.2f ? (sinf(powerT * 37) * sinf(powerT * 13) > 0.1f ? 1.0f : 0.15f) : 1.0f;
    PowerVisuals(k);
    if (powerT >= 2.2f) powerT = 99;
}

void PowerOnInstant() { powerOn = true; powerT = 99; leverK = 1; PowerVisuals(1); }   // fondo del menú: todo encendido

static void DrawPowerSwitch() {
    if (curMap->power.room == nullptr) return;
    // palanca: abajo = apagada, arriba = encendida
    float a = lerpf(-0.9f, 0.9f, leverK);
    Matrix pivot = MatrixMultiply(MatrixRotateX(a), MatrixTranslate(0.15f, 1.4f, 0.1f));
    DrawPM(unitCube, M("metal"), MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixScale(0.05f, 0.34f, 0.05f), MatrixTranslate(0, 0.17f, 0)), pivot), switchBase));
    DrawPM(unitSphere, M("raygunred"), MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixScale(0.09f, 0.09f, 0.09f), MatrixTranslate(0, 0.35f, 0)), pivot), switchBase));
    DrawPM(unitCyl, M("darkmetal"), MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixScale(0.08f, 0.12f, 0.08f), MatrixRotateZ(PI_F / 2)), MatrixTranslate(0.15f, 1.4f, 0.1f)), switchBase));
    // piloto: rojo sin corriente, verde con corriente
    PMat &bulb = M("fixture");
    Vector3 e = bulb.emis;
    bulb.emis = powerOn ? Vector3{ 0.2f, 3, 0.4f } : Vector3{ 3, 0.15f, 0.1f };
    DrawPM(unitSphere, bulb, MatrixMultiply(MatrixMultiply(MatrixScale(0.06f, 0.06f, 0.06f), MatrixTranslate(-0.18f, 1.7f, 0.09f)), switchBase));
    bulb.emis = e;
}

// ---------- Construcción del mapa ----------
void BuildMap(const char *key) {
    for (auto &sm : staticMeshes) UnloadMesh(sm.first);
    staticMeshes.clear();
    for (auto &t : mapTextures) UnloadTexture(t);
    mapTextures.clear();
    rooms.clear(); links.clear(); windows.clear(); doors.clear(); colliders.clear(); solids.clear();
    interactables.clear(); lights.clear(); props.clear(); labels.clear(); mmWalls.clear(); mmIcons.clear();
    dynMats.clear();
    dynProps.clear();
    RagdollResetCount();
    builders.list.clear();
    boxLabel = papLabel = -1;
    powerVis.clear(); papLight = -1;
    ClearEffects();

    curMap = &MAPS[0];
    for (auto &m : MAPS) if (!strcmp(m.key, key)) curMap = &m;
    theme = ThemeFor(curMap->theme);
    themeFog = theme.fog; themeAmbient = theme.ambient; themeFogDensity = theme.fogDensity;

    for (auto &r : curMap->rooms) rooms.push_back(Room{ r.id, r.x1, r.z1, r.x2, r.z2, r.light, r.flicker, r.lights, r.open });
    startRoom = RoomIndex(curMap->startRoom);
    for (auto &ld : curMap->links) {
        Link l{ ld.a, ld.b, ld.x, ld.z, ld.cost, ld.cost == 0, 'x', {}, {} };
        const Room &ra = rooms[RoomIndex(ld.a)];
        l.axis = ((nearf(ra.x1, ld.x) || nearf(ra.x2, ld.x)) && ld.z > ra.z1 && ld.z < ra.z2) ? 'z' : 'x';
        for (int k = 0; k < 2; k++) {
            const Room &r = rooms[RoomIndex(k ? ld.b : ld.a)];
            Vector2 s;
            if (l.axis == 'z') s = Vector2{ ld.x + ((r.x1 + r.x2) / 2 > ld.x ? 1.3f : -1.3f), ld.z };
            else s = Vector2{ ld.x, ld.z + ((r.z1 + r.z2) / 2 > ld.z ? 1.3f : -1.3f) };
            if (k) l.sideB = s; else l.sideA = s;
        }
        links.push_back(l);
    }

    // suelo exterior
    builders.Get("ground").Box(Vector3{ -110, -0.12f, -110 }, Vector3{ 110, -0.02f, 110 }, 4.0f, 0, 4);
    // salas: suelo, techo y luces
    wallH = curMap->outdoor ? 6.5f : H;
    for (size_t ri = 0; ri < rooms.size(); ri++) {
        const Room &r = rooms[ri];
        const char *floorMat = curMap->rooms[ri].floor ? curMap->rooms[ri].floor : theme.floor;
        builders.Get(floorMat).Box(Vector3{ r.x1, -0.1f, r.z1 }, Vector3{ r.x2, 0, r.z2 }, theme.floorUV, 0, 4);
        std::vector<Vector2> ls = r.lights;
        if (ls.empty()) ls.push_back(Vector2{ (r.x1 + r.x2) / 2, (r.z1 + r.z2) / 2 });
        if (r.open) {
            // al aire libre: sin techo y con farolas
            for (auto &lp : ls) {
                lights.push_back({ Vector3{ lp.x, 4.9f, lp.y }, Vector3Scale(ColorV(r.light), 24.0f), 26.0f, r.flicker, 1, true });
                props.push_back({ &unitCyl, &M("darkmetal"), TRSm(Vector3{ lp.x, 2.6f, lp.y }, 0, Vector3{ 0.16f, 5.2f, 0.16f }) });
                props.push_back({ &unitCube, &M("darkmetal"), TRSm(Vector3{ lp.x, 5.25f, lp.y }, 0, Vector3{ 0.9f, 0.18f, 0.5f }) });
                props.push_back({ &unitCube, TintedFixture(r.light), TRSm(Vector3{ lp.x, 5.13f, lp.y }, 0, Vector3{ 0.75f, 0.06f, 0.38f }) });
                colliders.push_back(Box2{ lp.x - 0.12f, lp.x + 0.12f, lp.y - 0.12f, lp.y + 0.12f });
                solids.push_back(BoundingBox{ Vector3{ lp.x - 0.1f, 0, lp.y - 0.1f }, Vector3{ lp.x + 0.1f, 5.2f, lp.y + 0.1f } });
            }
            continue;
        }
        builders.Get(theme.ceil).Box(Vector3{ r.x1, H, r.z1 }, Vector3{ r.x2, H + 0.1f, r.z2 }, theme.ceilUV, 0, 8);
        // en un mapa con paredes altas, el tejado tapa el hueco hasta lo alto de las fachadas
        if (wallH > H) builders.Get("rust").Box(Vector3{ r.x1, wallH - 0.1f, r.z1 }, Vector3{ r.x2, wallH, r.z2 }, 3.0f, 0, 4);
        for (auto &lp : ls) {
            lights.push_back({ Vector3{ lp.x, H - 0.35f, lp.y }, Vector3Scale(ColorV(r.light), 19.0f), 24.0f, r.flicker, 1, true });
            props.push_back({ &unitCube, TintedFixture(r.light), TRSm(Vector3{ lp.x, H - 0.05f, lp.y }, 0, Vector3{ 1.2f, 0.06f, 0.35f }) });
            props.push_back({ &unitCube, &M("darkmetal"), TRSm(Vector3{ lp.x, H - 0.02f, lp.y }, 0, Vector3{ 1.3f, 0.04f, 0.45f }) });
        }
    }
    roomLights = (int)lights.size();   // las primeras luces son las de las salas
    // ventanas
    for (auto &wd : curMap->windows) {
        int ri = RoomIndex(wd.room);
        WallPlace p = WallPlaceAt(ri, wd.x, wd.z);
        Window w;
        w.room = wd.room; w.x = wd.x; w.z = wd.z; w.nx = p.nx; w.nz = p.nz;
        w.outside = Vector2{ wd.x - p.nx * 1.1f, wd.z - p.nz * 1.1f };
        w.inside = Vector2{ wd.x + p.nx * 1.0f, wd.z + p.nz * 1.0f };
        windows.push_back(w);
    }
    BuildWalls();
    for (auto &w : windows) BuildWindow(w);
    // fachadas de las calles: ventanas altas (algunas con luz) y cornisa, para que parezcan edificios
    if (curMap->outdoor) {
        unsigned seed = 7;
        auto rnd = [&]() { seed = seed * 1103515245u + 12345u; return (seed >> 9) / 8388608.0f; };
        for (auto &r : rooms) {
            if (!r.open) continue;
            for (int e = 0; e < 4; e++) {
                bool alongX = e < 2;
                float fixed = e == 0 ? r.z1 : e == 1 ? r.z2 : e == 2 ? r.x1 : r.x2;
                float a = alongX ? r.x1 : r.z1, b = alongX ? r.x2 : r.z2;
                float in = (e == 0 || e == 2) ? 0.22f : -0.22f;   // hacia dentro de la calle
                float c = fixed + in;
                Vector3 corn = alongX ? Vector3{ (a + b) / 2, wallH - 0.25f, c } : Vector3{ c, wallH - 0.25f, (a + b) / 2 };
                props.push_back({ &unitCube, &M("concwall"), TRSm(corn, 0, alongX ? Vector3{ b - a, 0.3f, 0.2f } : Vector3{ 0.2f, 0.3f, b - a }) });
                for (float t = a + 2.5f; t < b - 2.0f; t += 4.0f + rnd() * 1.5f) {
                    Vector3 pos = alongX ? Vector3{ t, 4.4f, c } : Vector3{ c, 4.4f, t };
                    Vector3 frame = alongX ? Vector3{ 1.3f, 1.7f, 0.08f } : Vector3{ 0.08f, 1.7f, 1.3f };
                    Vector3 pane = alongX ? Vector3{ 1.05f, 1.45f, 0.1f } : Vector3{ 0.1f, 1.45f, 1.05f };
                    props.push_back({ &unitCube, &M("wood"), TRSm(pos, 0, frame) });
                    bool lit = rnd() < 0.22f;
                    props.push_back({ &unitCube, lit ? TintedFixture(rnd() < 0.5f ? 0xffb060 : 0xffd9a0, 1.2f) : &M("darkmetal"), TRSm(pos, 0, pane) });
                }
            }
        }
    }
    // farolas exteriores frente a algunas ventanas
    for (size_t i = 0; i < windows.size(); i += 3) {
        Window &w = windows[i];
        lights.push_back({ Vector3{ w.x - w.nx * 7.0f, 3.0f, w.z - w.nz * 7.0f }, Vector3{ 1.4f, 1.8f, 3.0f }, 14.0f, false, 1, false });
    }
    for (size_t i = 0; i < links.size(); i++) BuildLink((int)i);
    for (auto &p : curMap->props) BuildProp(p);
    for (auto &pp : curMap->pipes) {
        float len = hyp(pp.x2 - pp.x1, pp.z2 - pp.z1);
        Matrix m = MatrixMultiply(MatrixMultiply(MatrixScale(0.3f, len, 0.3f), MatrixRotateZ(PI_F / 2)), MatrixRotateY(-atan2f(pp.z2 - pp.z1, pp.x2 - pp.x1)));
        props.push_back({ &unitCyl, &M("rust"), MatrixMultiply(m, MatrixTranslate((pp.x1 + pp.x2) / 2, H - 0.35f, (pp.z1 + pp.z2) / 2)) });
    }
    for (auto &b : curMap->wallBuys) MakeWallBuy(b);
    for (auto &p : curMap->perks) MakePerk(p);
    BuildBox();
    BuildPap();
    BuildPowerSwitch();
    PowerVisuals(powerOn ? 1.0f : 0.0f);
    RecomputeUnlocked();
    for (auto &b : builders.list) staticMeshes.push_back({ b.second.Build(), &M(b.first.c_str()) });
}

void UpdateWorld(float dt) {
    for (auto &w : windows) for (int i = 0; i < 5; i++) {
        if (w.boardAnim[i] < 0) continue;
        w.boardAnim[i] += dt;
        w.boardVel[i].y -= 9.8f * dt;
        w.boardPos[i] = Vector3Add(w.boardPos[i], Vector3Scale(w.boardVel[i], dt));
        w.boardSpin[i] += dt * 4;
        if (w.boardAnim[i] > 0.8f) { w.boardVis[i] = false; w.boardAnim[i] = -1; }
    }
    for (auto &d : doors) if (d.sinking && d.sink < 3.2f) d.sink += dt * 1.6f;
    UpdateBox(dt);
    UpdatePap(dt);
    UpdatePower(dt);
    UpdateDynProps(dt);
}

void DrawWorldDynamic() {
    DrawPowerSwitch();
    for (auto &d : doors) {
        if (d.sink >= 3.2f) continue;
        DrawPM(unitCube, M("door"), MatrixMultiply(MatrixScale(d.size.x, d.size.y, d.size.z), MatrixTranslate(d.pos.x, d.pos.y - d.sink, d.pos.z)));
    }
    for (auto &w : windows) for (int i = 0; i < 5; i++) {
        if (!w.boardVis[i]) continue;
        bool vertical = w.nx != 0;
        Vector3 s = vertical ? Vector3{ 0.06f, 0.2f, 1.9f } : Vector3{ 1.9f, 0.2f, 0.06f };
        Matrix m = MatrixScale(s.x, s.y, s.z);
        m = MatrixMultiply(m, vertical ? MatrixRotateX(w.boardRot[i]) : MatrixRotateZ(w.boardRot[i]));
        if (w.boardSpin[i]) m = MatrixMultiply(m, MatrixRotateY(w.boardSpin[i]));
        Vector3 p = w.boardPos[i];
        DrawPM(unitCube, M("planks"), MatrixMultiply(m, MatrixTranslate(p.x, p.y, p.z)));
    }
}

void DrawUnlitBegin(float intensity);
void DrawUnlitEnd();
void DrawLabels() {
    if (boxLabel >= 0) labels[boxLabel].visible = box.state != BOX_LEAVING;
    DrawUnlitBegin(1.0f);
    for (auto &l : labels) if (l.visible) DrawWorldLabel(l.tex, l.pos, l.width);
    DrawUnlitEnd();
}

std::vector<std::pair<int, Vector2>> GroundSpawnsImpl() {
    std::vector<std::pair<int, Vector2>> v;
    if (curMap) for (auto &g : curMap->ground) v.push_back({ RoomIndex(g.room), Vector2{ g.x, g.z } });
    return v;
}

// ---------- Mallas base ----------
void InitBaseMeshes() {
    MeshBuilder c; c.Box(Vector3{ -0.5f, -0.5f, -0.5f }, Vector3{ 0.5f, 0.5f, 0.5f }, 1, 1); unitCube = c.Build();
    MeshBuilder y; y.Cylinder(0.5f, 1, 18, 1); unitCyl = y.Build();
    void ComputeTangents(Mesh &m, bool dxNormals);
    unitSphere = GenMeshSphere(0.5f, 14, 14); ComputeTangents(unitSphere, false);
    MeshBuilder p; p.Box(Vector3{ -0.5f, -0.001f, -0.5f }, Vector3{ 0.5f, 0, 0.5f }, 1, 1, 4); unitPlane = p.Build();
}

// Niebla de la ronda de perros: más espesa y de color brasa (k de 0 a 1)
void DogFog(float k) {
    themeFogDensity = theme.fogDensity * (1 + 1.6f * k);
    themeFog = Vector3Lerp(theme.fog, Vector3{ 0.035f, 0.012f, 0.005f }, k);
}

// Para las pruebas: dónde está el interruptor y desde dónde se ve
bool PowerSwitchView(Vector3 &pos, Vector3 &eye) {
    if (!curMap || !curMap->power.room) return false;
    const PlaceDef &s = curMap->power;
    WallPlace p = WallPlaceAt(RoomIndex(s.room), s.x, s.z);
    pos = Vector3{ s.x, 1.5f, s.z };
    eye = Vector3{ s.x + p.nx * 2.4f + p.nz * 0.8f, 0, s.z + p.nz * 2.4f + p.nx * 0.8f };
    return true;
}
// Para las pruebas
int DynPropCount() { return (int)dynProps.size(); }
Vector3 DynPropPos(int i) { return dynProps[i].pos; }
