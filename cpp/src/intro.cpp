// Cinemática de llegada al empezar un mapa: afuera de noche, la cámara se acerca a una ventana,
// corte a una toma aérea de la sala de inicio y bajada hasta los ojos del jugador.
// Un soldado habla por radio (assets/sfx/voz_<mapa><n>.wav) y sus frases salen subtituladas.
#include "game.h"

static float SHOT1 = 4.2f;     // fin de la toma de afuera
static float SHOT2 = 8.4f;     // fin de la toma aérea
static float INTRO_LEN = 10.6f;
static float introT = 0, nextStep = 0;
static bool groaned = false;
static Vector3 outFrom, outTo, outLook;   // toma 1
static Vector3 roomC; float roomR = 4;    // toma 2: órbita sobre el centro de la sala
static float orbitA0 = 0, orbitH0 = 3.4f, orbitH1 = 2.7f;   // al aire libre la toma va mucho más alta
static Vector3 eyePos, eyeLook;           // toma 3: los ojos del jugador

// ---------- Voces por radio ----------
struct VoiceLine { Sound snd{}; float start = 0, len = 0; const char *sub = ""; bool played = false; };
static VoiceLine voices[4];
static int voiceCount = 0;
static std::string voicesMap;

struct IntroText { const char *place; const char *subs[4]; };
static IntroText TextFor(const std::string &key) {
    if (key == "pueblo") return { "PUEBLO DE SAN ROQUE · 02:13",
        { "Base, aquí unidad siete. Entramos al pueblo.", "Las calles están vacías... demasiado vacías.", "Hay tumbas abiertas en el cementerio.", "Que Dios nos ayude. Ya vienen." } };
    if (key == "fabrica") return { "FÁBRICA ABANDONADA · AFUERAS DE LA CIUDAD",
        { "Base, aquí unidad siete. Llegamos a la fábrica.", "No queda nadie... o eso parece.", "Escucho pasos. No estamos solos.", "Prepárate. Ya vienen." } };
    return { "BÚNKER 7 · 23:47",
        { "Base, aquí unidad siete. Llegamos al búnker.", "Algo se mueve ahí dentro.", "Tapen las ventanas. No dejen que entren.", "Aguanta. Cambio y fuera." } };
}

static void LoadVoices(const std::string &key) {
    if (voicesMap == key) return;
    for (int i = 0; i < voiceCount; i++) UnloadSound(voices[i].snd);
    voiceCount = 0; voicesMap = key;
    IntroText it = TextFor(key);
    for (int i = 0; i < 4; i++) {
        const char *path = TextFormat("assets/sfx/voz_%s%d.wav", key.c_str(), i + 1);
        if (!IsAudioDeviceReady() || !FileExists(path)) break;
        VoiceLine &v = voices[voiceCount++];
        v = VoiceLine{};
        v.snd = LoadSound(path);
        v.len = v.snd.stream.sampleRate ? (float)v.snd.frameCount / v.snd.stream.sampleRate : 0;
        v.sub = it.subs[i];
    }
}

// Las tomas se acomodan a lo que dura cada frase: 1 y 2 afuera, 3 en la toma aérea, 4 en la bajada
static void ScheduleVoices() {
    SHOT1 = 4.2f; SHOT2 = 8.4f; INTRO_LEN = 10.6f;
    if (voiceCount < 4) return;
    float t = 0.5f;
    for (int i = 0; i < 4; i++) {
        if (i == 2) t = std::max(t + 0.3f, 6.0f);
        voices[i].start = t;
        voices[i].played = false;
        t += voices[i].len + 0.25f;
    }
    SHOT1 = voices[2].start - 0.5f;
    SHOT2 = voices[3].start;
    INTRO_LEN = std::max(SHOT2 + 2.2f, voices[3].start + voices[3].len + 0.3f);
}

static void StopVoices() { for (int i = 0; i < voiceCount; i++) StopSound(voices[i].snd); }

static float Smooth(float x) { x = clampf(x, 0, 1); return x * x * (3 - 2 * x); }

static void OrbitPose(float k, Vector3 &pos, Vector3 &look) {
    float a = orbitA0 + k * 1.1f;
    pos = Vector3{ roomC.x + sinf(a) * roomR, lerpf(orbitH0, orbitH1, k), roomC.z + cosf(a) * roomR };
    look = Vector3{ roomC.x, 0.9f, roomC.z };
}

void StartIntro() {
    introT = 0; nextStep = 0.4f; groaned = false;
    LoadVoices(game.mapKey);
    ScheduleVoices();
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
    orbitH0 = r.open ? 11.0f : 3.4f; orbitH1 = r.open ? 6.0f : 2.7f;
    game.state = GS_INTRO;
    Play("door", 0.5f);
}

static void EndIntro() {
    game.state = GS_PLAYING;
    StopVoices();
    Play("door", 0.9f);
    CenterMsg("SOBREVIVE", Color{ 255, 64, 64, 255 }, 2.5f);
}

void UpdateIntro(float dt) {
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) introT = INTRO_LEN;
    introT += dt;
    for (int i = 0; i < voiceCount; i++)
        if (!voices[i].played && introT >= voices[i].start && introT < INTRO_LEN) {
            voices[i].played = true;
            SetSoundVolume(voices[i].snd, clampf(opts.sfx * 1.1f, 0, 1));
            PlaySound(voices[i].snd);
        }
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
        Color c1{ 255, 216, 74, (unsigned char)(a * 255) };
        IText(fontUI, Typed(it.place, introT - 0.6f, 28).c_str(), 70 * s, Hh - bh - 70 * s, 30 * s, c1, 0, 3);
    } else if (introT < INTRO_LEN - 0.6f) {
        // el nombre del mapa con el título del juego
        float k = introT - SHOT1;
        float a = Smooth((k - 0.5f) / 0.8f) * (1 - Smooth((introT - (INTRO_LEN - 1.8f)) / 0.8f));
        float ts = (120 + k * 4) * s;
        IText(fontTitle, MapName(), W / 2.0f, Hh * 0.36f - ts / 2, ts, Color{ 204, 0, 0, (unsigned char)(a * 255) }, 0.5f, 6);
        IText(fontUI, "RONDA 1", W / 2.0f, Hh * 0.36f + ts * 0.62f, 34 * s, Color{ 230, 230, 230, (unsigned char)(a * 220) }, 0.5f, 8);
    }
    // subtítulos de la radio, dentro de la franja de abajo
    for (int i = 0; i < voiceCount; i++) {
        const VoiceLine &v = voices[i];
        float k = introT - v.start;
        if (k < 0 || k > v.len + 0.2f) continue;
        float a = Smooth(k / 0.15f) * (1 - Smooth((k - v.len) / 0.2f));
        float y = Hh - bh / 2 - 16 * s;
        IText(fontUI, "RADIO ·", W / 2.0f - MeasureTextEx(fontUI, v.sub, 30 * s, 1).x / 2 - 12 * s, y + 4 * s, 22 * s, Color{ 120, 200, 120, (unsigned char)(a * 220) }, 1, 2);
        IText(fontUI, v.sub, W / 2.0f, y, 30 * s, Color{ 240, 240, 240, (unsigned char)(a * 255) }, 0.5f);
    }
    if (introT > 0.8f && introT < INTRO_LEN - 0.8f)
        IText(fontUI, "ESPACIO para saltar", W - 30 * s, Hh - bh + (bh - 22 * s) / 2, 18 * s, Color{ 180, 180, 180, 160 }, 1);
}
