// Huevos de pascua:
//  - Tres ositos escondidos en cada mapa: encontrarlos da la Pistola de Rayos mejorada y el uniforme rosa de Milton.
//  - Una radio secreta en cada mapa con una grabación de la historia; con las seis se escucha la verdad sobre Lázaro.
//  - Escribir CABEZON durante la partida (o en la pausa): zombis cabezones.
// Lo encontrado se guarda en secretos.txt junto al juego.
#include "game.h"

struct EggSpot { const char *map, *room; float x, z; };
// ositos: rincones apartados de cada mapa
static const EggSpot TEDDIES[] = {
    { "bunker", "B", 8.6f, -24.6f }, { "bunker", "D", -8.6f, 24.6f }, { "bunker", "E", 26.6f, -9.4f },
    { "fabrica", "C", -22.6f, 10.6f }, { "fabrica", "D", 22.6f, -6.6f }, { "fabrica", "E", -22.6f, 28.4f },
    { "pueblo", "E", -33.6f, 8.6f }, { "pueblo", "D", 63.4f, 23.4f }, { "pueblo", "G", -43.6f, -43.6f },
    { "hospital", "C", -22.6f, 30.6f }, { "hospital", "E", -22.6f, -6.6f }, { "hospital", "F", 22.6f, 6.6f },
    { "base", "B", -38.6f, 3.6f }, { "base", "D", 43.6f, -23.6f }, { "base", "F", 26.6f, 16.4f },
    { "laboratorio", "C", -28.6f, 24.6f }, { "laboratorio", "E", 18.6f, 53.6f }, { "laboratorio", "F", -28.6f, -6.6f },
};
// radios: una por mapa
static const EggSpot RADIOS[] = {
    { "bunker", "C", 26.6f, -6.6f }, { "fabrica", "B", 22.6f, -14.6f }, { "pueblo", "C", 33.4f, 13.4f },
    { "hospital", "D", 22.6f, 30.6f }, { "base", "C", -38.6f, 18.6f }, { "laboratorio", "D", 26.6f, 22.6f },
};
static const char *RADIO_LINE[] = { "rad_bunker", "rad_fabrica", "rad_pueblo", "rad_hospital", "rad_base", "rad_laboratorio" };

// ---------- Lo guardado ----------
static bool loaded = false, rosa = false;
static unsigned radiosFound = 0;   // un bit por radio
static void Load() {
    if (loaded) return;
    loaded = true;
    if (FILE *f = fopen("secretos.txt", "r")) {
        char key[64]; int v;
        while (fscanf(f, "%63s %d", key, &v) == 2) {
            if (!strcmp(key, "rosa")) rosa = v != 0;
            if (!strcmp(key, "radios")) radiosFound = (unsigned)v;
        }
        fclose(f);
    }
}
static void Save() {
    if (strcmp(recordsFile, "records.txt")) return;   // las pruebas no tocan los secretos del jugador
    if (FILE *f = fopen("secretos.txt", "w")) { fprintf(f, "rosa %d\nradios %u\n", rosa ? 1 : 0, radiosFound); fclose(f); }
}
bool EggRosa() { Load(); return rosa && opts.allyRosa; }
bool EggRosaUnlocked() { Load(); return rosa; }
int EggRadiosFound() { Load(); int n = 0; for (int i = 0; i < 6; i++) if (radiosFound & (1u << i)) n++; return n; }

// ---------- En el mapa ----------
struct Egg { int kind; int idx; Vector3 pos; bool found = false; };   // kind 0 osito, 1 radio
static std::vector<Egg> eggs;
static int teddiesFound = 0;
bool bigHeads = false;
static std::string typed;

void EggsSetupMap() {
    Load();
    eggs.clear();
    teddiesFound = 0;
    if (!curMap || Practice()) return;
    std::string key = MapKeyNow();
    for (int i = 0; i < (int)(sizeof(TEDDIES) / sizeof(TEDDIES[0])); i++)
        if (key == TEDDIES[i].map) eggs.push_back({ 0, i, Vector3{ TEDDIES[i].x, 0, TEDDIES[i].z } });
    for (int i = 0; i < 6; i++)
        if (key == RADIOS[i].map) eggs.push_back({ 1, i, Vector3{ RADIOS[i].x, 0, RADIOS[i].z }, (radiosFound & (1u << i)) != 0 });
    // que no queden dentro de una caja: se corren un poco si hace falta
    for (auto &e : eggs)
        for (int k = 0; k < 12 && InsideCollider(e.pos.x, e.pos.z, 0.3f); k++) {
            const Room &r = rooms[RoomIndex(e.kind ? RADIOS[e.idx].room : TEDDIES[e.idx].room)];
            e.pos.x += ((r.x1 + r.x2) / 2 > e.pos.x ? 0.5f : -0.5f);
            e.pos.z += ((r.z1 + r.z2) / 2 > e.pos.z ? 0.5f : -0.5f);
        }
    for (int i = 0; i < (int)eggs.size(); i++) {
        Interact it;
        it.pos = [i]() { return Vector2{ eggs[i].pos.x, eggs[i].pos.z }; };
        it.range = 1.4f;
        it.active = [i]() { return i < (int)eggs.size() && !eggs[i].found; };
        it.text = [i]() { return std::string(eggs[i].kind ? "Presiona E para escuchar la radio" : "Presiona E para tomar el osito"); };
        it.action = [i]() {
            Egg &e = eggs[i];
            if (e.found) return;
            e.found = true;
            if (e.kind == 0) {
                teddiesFound++;
                Play("laugh", 0.6f);
                CenterMsg(TextFormat("OSITO %d/3", teddiesFound), Color{ 255, 170, 210, 255 }, 2.0f);
                if (teddiesFound == 1) DialogSay({ "egg_osito" });
                if (teddiesFound == 3) {
                    WhiteFlash(0.6f);
                    Play("pap", 0.8f);
                    GiveWeapon(W_RAYGUN);
                    CurWeapon().pap = true;
                    CenterMsg("¡HUEVO DE PASCUA!", Color{ 255, 120, 200, 255 }, 3.5f);
                    bool nuevo = !rosa;
                    rosa = true; opts.allyRosa = true; Save(); SaveOptions();
                    SpecialMsg(nuevo ? "Pistola de Rayos mejorada · Milton ahora usa camuflaje rosa" : "Pistola de Rayos mejorada");
                    DialogSay({ "egg_ositos3" });
                }
            } else {
                Play("power", 0.4f);
                bool was = (radiosFound & (1u << e.idx)) != 0;
                radiosFound |= 1u << e.idx;
                Save();
                CenterMsg(TextFormat("GRABACIÓN SECRETA %d/6", EggRadiosFound()), Color{ 140, 230, 140, 255 }, 2.5f);
                std::vector<const char *> ls{ RADIO_LINE[e.idx] };
                if (!was && EggRadiosFound() == 6) { ls.push_back("rad_final"); SpecialMsg("Encontraste todas las grabaciones"); }
                DialogSay(ls);
            }
        };
        interactables.push_back(it);
    }
}

void EggsUpdate() {
    // trucos: se escriben con el teclado durante la partida o en la pausa
    int c;
    while ((c = GetCharPressed()) > 0) {
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c < 'A' || c > 'Z') continue;
        typed += (char)c;
        if (typed.size() > 16) typed.erase(0, typed.size() - 16);
        if (typed.size() >= 7 && typed.compare(typed.size() - 7, 7, "CABEZON") == 0) {
            typed.clear();
            bigHeads = !bigHeads;
            CenterMsg(bigHeads ? "MODO CABEZÓN" : "CABEZAS NORMALES", Color{ 255, 200, 80, 255 }, 2.5f);
            Play("laugh", 0.7f);
            if (bigHeads) DialogSay({ "egg_cab_f", "egg_cab_m" });
        }
    }
}

static void DrawTeddyAt(Vector3 p, float yaw) {
    Matrix tb = MatrixMultiply(MatrixRotateY(yaw), MatrixTranslate(p.x, p.y + 0.26f, p.z));
    auto ball = [&](float r, float x, float y, float z) { DrawPM(unitSphere, M("fur"), MatrixMultiply(MatrixMultiply(MatrixScale(r * 2, r * 2, r * 2), MatrixTranslate(x, y, z)), tb)); };
    ball(0.16f, 0, 0, 0); ball(0.12f, 0, 0.22f, 0); ball(0.045f, -0.09f, 0.33f, 0); ball(0.045f, 0.09f, 0.33f, 0);
    ball(0.06f, -0.16f, 0.04f, 0.04f); ball(0.06f, 0.16f, 0.04f, 0.04f); ball(0.07f, -0.08f, -0.16f, 0.06f); ball(0.07f, 0.08f, -0.16f, 0.06f);
}
void EggsDraw() {
    float t = (float)GetTime();
    for (auto &e : eggs) {
        if (e.kind == 0) { if (!e.found) DrawTeddyAt(e.pos, e.pos.x * 1.7f); continue; }
        // radio vieja: caja, perilla, antena y una lucecita que parpadea
        Matrix base = MatrixTranslate(e.pos.x, 0, e.pos.z);
        DrawPM(unitCube, M("wood"), MatrixMultiply(MatrixMultiply(MatrixScale(0.5f, 0.32f, 0.26f), MatrixTranslate(0, 0.16f, 0)), base));
        DrawPM(unitCube, M("darkmetal"), MatrixMultiply(MatrixMultiply(MatrixScale(0.3f, 0.18f, 0.02f), MatrixTranslate(-0.05f, 0.17f, 0.135f)), base));
        DrawPM(unitCyl, M("darkmetal"), MatrixMultiply(MatrixMultiply(MatrixScale(0.015f, 0.45f, 0.015f), MatrixTranslate(0.18f, 0.55f, 0)), base));
        if (!e.found && fmodf(t, 1.4f) < 0.5f) {
            static PMat led;
            static bool init = false;
            if (!init) { init = true; led = M("fixture"); led.emis = Vector3{ 0.3f, 4, 0.5f }; }
            DrawPM(unitCube, led, MatrixMultiply(MatrixMultiply(MatrixScale(0.04f, 0.04f, 0.04f), MatrixTranslate(0.17f, 0.2f, 0.135f)), base));
        }
    }
}
