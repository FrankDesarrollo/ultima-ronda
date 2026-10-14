// Cinemática de llegada al empezar un mapa: afuera de noche, la cámara se acerca a una ventana,
// corte a una toma aérea de la sala de inicio y bajada hasta los ojos del jugador.
#include "game.h"

static const float SHOT1 = 4.2f;     // fin de la toma de afuera
static const float SHOT2 = 8.4f;     // fin de la toma aérea
static const float INTRO_LEN = 10.6f;
static float introT = 0, nextStep = 0;
static bool groaned = false;
static Vector3 outFrom, outTo, outLook;   // toma 1
static Vector3 roomC; float roomR = 4;    // toma 2: órbita sobre el centro de la sala
static float orbitA0 = 0;
static Vector3 eyePos, eyeLook;           // toma 3: los ojos del jugador

struct IntroText { const char *place, *line; };
static IntroText TextFor(const std::string &key) {
    if (key == "fabrica") return { "FÁBRICA ABANDONADA · AFUERAS DE LA CIUDAD", "Aquí iban a esperarnos. No queda nadie." };
    return { "BÚNKER 7 · 23:47", "La radio dejó de sonar hace tres días." };
}

static float Smooth(float x) { x = clampf(x, 0, 1); return x * x * (3 - 2 * x); }

static void OrbitPose(float k, Vector3 &pos, Vector3 &look) {
    float a = orbitA0 + k * 1.1f;
    pos = Vector3{ roomC.x + sinf(a) * roomR, lerpf(3.4f, 2.7f, k), roomC.z + cosf(a) * roomR };
    look = Vector3{ roomC.x, 0.9f, roomC.z };
}

void StartIntro() {
    introT = 0; nextStep = 0.4f; groaned = false;
    const Room &r = rooms[startRoom];
    eyePos = Vector3{ player.pos.x, player.eye, player.pos.z };
    eyeLook = Vector3Add(eyePos, Vector3{ -sinf(player.yaw), 0, -cosf(player.yaw) });
    // la ventana más cercana al punto de inicio (en la Fábrica la sala de inicio no tiene)
    int wi = 0; float bd = 1e9f;
    for (size_t i = 0; i < windows.size(); i++) {
        float d = hyp(windows[i].x - player.pos.x, windows[i].z - player.pos.z);
        if (d < bd) { bd = d; wi = (int)i; }
    }
    if (!windows.empty()) {
        const Window &w = windows[wi];
        outLook = Vector3{ w.x, 1.65f, w.z };
        outFrom = Vector3{ w.x - w.nx * 8.0f, 1.75f, w.z - w.nz * 8.0f };
        outTo = Vector3{ w.x - w.nx * 2.4f, 1.65f, w.z - w.nz * 2.4f };
    } else {
        outLook = eyeLook; outFrom = outTo = Vector3{ eyePos.x, 3, eyePos.z };
    }
    roomC = Vector3{ (r.x1 + r.x2) / 2, 0, (r.z1 + r.z2) / 2 };
    roomR = std::min(r.x2 - r.x1, r.z2 - r.z1) * 0.36f;
    // la órbita empieza detrás del jugador para terminar mirando hacia donde él mira
    orbitA0 = player.yaw - 1.1f;
    game.state = GS_INTRO;
    Play("door", 0.5f);
}

static void EndIntro() {
    game.state = GS_PLAYING;
    Play("door", 0.9f);
    CenterMsg("SOBREVIVE", Color{ 255, 64, 64, 255 }, 2.5f);
}

void UpdateIntro(float dt) {
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) introT = INTRO_LEN;
    introT += dt;
    Vector3 pos, look;
    if (introT < SHOT1) {
        // pasos lentos en la oscuridad, con la cámara al hombro
        float k = Smooth(introT / SHOT1);
        pos = Vector3Lerp(outFrom, outTo, k);
        pos.y += sinf(introT * 7.0f) * 0.035f;
        pos.x += sinf(introT * 1.3f) * 0.05f;
        look = Vector3Add(outLook, Vector3{ sinf(introT * 0.9f) * 0.15f, cosf(introT * 1.1f) * 0.08f, 0 });
        if (introT > nextStep && introT < SHOT1 - 0.6f) { Play("step", 0.35f); nextStep += 0.55f; }
        if (!groaned && introT > SHOT1 - 1.4f) { groaned = true; Play("groan", 0.8f); }
    } else if (introT < SHOT2) {
        OrbitPose((introT - SHOT1) / (SHOT2 - SHOT1), pos, look);
    } else {
        // baja hasta los ojos del jugador
        Vector3 p0, l0;
        OrbitPose(1, p0, l0);
        float k = Smooth((introT - SHOT2) / (INTRO_LEN - SHOT2));
        pos = Vector3Lerp(p0, eyePos, k);
        look = Vector3Lerp(l0, eyeLook, k);
    }
    cam.position = pos;
    cam.target = look;
    cam.up = Vector3{ 0, 1, 0 };
    camFov = introT < SHOT1 ? 62 : lerpf(68, opts.fov, Smooth((introT - SHOT2) / (INTRO_LEN - SHOT2)));
    gunVisible = false;
    if (introT >= INTRO_LEN) EndIntro();
}

static void IText(const Font &f, const char *t, float x, float y, float size, Color c, float align, float spacing = 1) {
    Vector2 m = MeasureTextEx(f, t, size, spacing);
    x -= m.x * align;
    DrawTextEx(f, t, Vector2{ x + 3, y + 3 }, size, spacing, Color{ 0, 0, 0, (unsigned char)(c.a * 0.8f) });
    DrawTextEx(f, t, Vector2{ x, y }, size, spacing, c);
}

// Texto que aparece letra a letra
static std::string Typed(const char *t, float elapsed, float cps) {
    std::string s = t;
    int n = (int)(elapsed * cps);
    if (n <= 0) return "";
    // no cortar a mitad de un carácter UTF-8 (tildes)
    int bytes = 0, chars = 0;
    while (bytes < (int)s.size() && chars < n) { bytes++; while (bytes < (int)s.size() && (s[bytes] & 0xC0) == 0x80) bytes++; chars++; }
    return s.substr(0, bytes);
}

void DrawIntro() {
    int W = GetScreenWidth(), Hh = GetScreenHeight();
    float s = Hh / 900.0f;
    // franjas de cine: entran al principio y se retiran al final
    float bars = Smooth(introT / 0.8f) * (1 - Smooth((introT - (INTRO_LEN - 1.0f)) / 1.0f));
    float bh = Hh * 0.12f * bars;
    DrawRectangle(0, 0, W, (int)bh, BLACK);
    DrawRectangle(0, Hh - (int)bh, W, (int)bh + 1, BLACK);
    // fundidos: desde negro, y a negro entre la toma de afuera y la de adentro
    float fade = 1 - Smooth(introT / 1.2f);
    if (introT > SHOT1 - 0.5f && introT < SHOT1 + 0.6f)
        fade = std::max(fade, introT < SHOT1 ? Smooth((introT - (SHOT1 - 0.5f)) / 0.5f) : 1 - Smooth((introT - SHOT1) / 0.6f));
    if (fade > 0) DrawRectangle(0, 0, W, Hh, Color{ 0, 0, 0, (unsigned char)(fade * 255) });

    IntroText it = TextFor(game.mapKey);
    if (introT < SHOT1) {
        float a = 1 - Smooth((introT - (SHOT1 - 0.6f)) / 0.5f);
        Color c1{ 255, 216, 74, (unsigned char)(a * 255) }, c2{ 230, 230, 230, (unsigned char)(a * 230) };
        IText(fontUI, Typed(it.place, introT - 0.6f, 28).c_str(), 70 * s, Hh - bh - 110 * s, 30 * s, c1, 0, 3);
        IText(fontUI, Typed(it.line, introT - 1.9f, 30).c_str(), 70 * s, Hh - bh - 70 * s, 26 * s, c2, 0);
    } else if (introT < INTRO_LEN - 0.6f) {
        // el nombre del mapa con el título del juego
        float k = introT - SHOT1;
        float a = Smooth((k - 0.5f) / 0.8f) * (1 - Smooth((introT - (INTRO_LEN - 1.8f)) / 0.8f));
        float ts = (120 + k * 4) * s;
        IText(fontTitle, MapName(), W / 2.0f, Hh * 0.36f - ts / 2, ts, Color{ 204, 0, 0, (unsigned char)(a * 255) }, 0.5f, 6);
        IText(fontUI, "RONDA 1", W / 2.0f, Hh * 0.36f + ts * 0.62f, 34 * s, Color{ 230, 230, 230, (unsigned char)(a * 220) }, 0.5f, 8);
    }
    if (introT > 0.8f && introT < INTRO_LEN - 0.8f)
        IText(fontUI, "ESPACIO para saltar", W - 30 * s, Hh - bh + (bh - 22 * s) / 2, 18 * s, Color{ 180, 180, 180, 160 }, 1);
}
