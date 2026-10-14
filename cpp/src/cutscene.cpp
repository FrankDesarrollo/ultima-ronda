// Cinemáticas de la historia, armadas con datos: tomas de cámara, actores (Frank, Milton y zombis títere)
// que se mueven entre puntos con su animación, frases con voz y subtítulos, y efectos (explosiones, fuego, disparos).
// Las tomas pueden ser travellings, grúas, órbitas, cámara pegada a un actor o primeros planos del que habla;
// se cortan en seco como en el cine, con cámara en mano, cámara lenta y grano de película.
// Las frases se encadenan solas según lo que dura cada audio; la última toma se estira hasta que terminan de hablar.
#include "game.h"

enum ActorKind { A_FRANK, A_MILTON, A_ZOMBIE, A_BOSS, A_GUARD };
enum EventKind { EV_BOOM, EV_FLASH, EV_SHAKE, EV_SOUND, EV_SHOT, EV_GREEN, EV_FIRE, EV_RED, EV_BIGBOOM };
// rate: velocidad de la animación · yaw NAN: mira hacia donde camina
struct Key { float t; Vector3 p; float yaw; int clip; float rate = 1; };
// pitch: cabeza levantada (mirar hacia arriba)
struct Actor { int kind; std::vector<Key> keys; float pitch = 0; };
enum CamMode { CAM_LERP, CAM_ORBIT, CAM_FOLLOW };
// CAM_LERP: de camFrom a camTo mirando de lookFrom a lookTo.
// CAM_ORBIT: gira alrededor de lookFrom; camFrom/camTo = (ángulo, altura, radio).
// CAM_FOLLOW: pegada al actor "actor"; camFrom/camTo = (costado, altura, atrás); mira lookFrom.z metros delante, a la altura lookFrom.y.
// talkCam: primer plano del que habla. fovTo: zoom durante la toma. hand: cámara en mano. slow: cámara lenta (1 = normal).
struct Shot {
    float dur; Vector3 camFrom, camTo, lookFrom, lookTo; float fov; bool talkCam = false;
    int mode = CAM_LERP, actor = -1; float fovTo = 0, hand = 0.35f, slow = 1; bool fade = false;
};
struct Event { float t; int kind; Vector3 p; const char *s; };
struct Line { float t; const char *id; };
struct Cutscene { const char *id; const char *caption; std::vector<Shot> shots; std::vector<Actor> actors; std::vector<Line> lines; std::vector<Event> events; };

static const float NA = NAN;
static Vector3 V(float x, float y, float z) { return Vector3{ x, y, z }; }
static Shot Orbit(float dur, Vector3 c, float a0, float a1, float r0, float r1, float h0, float h1, float lookY, float fov) {
    Shot s{ dur, V(a0, h0, r0), V(a1, h1, r1), V(c.x, lookY, c.z), V(c.x, lookY, c.z), fov };
    s.mode = CAM_ORBIT;
    return s;
}
static Shot Follow(float dur, int actor, Vector3 off0, Vector3 off1, float ahead, float lookY, float fov) {
    Shot s{ dur, off0, off1, V(0, lookY, ahead), V(0, lookY, ahead), fov };
    s.mode = CAM_FOLLOW; s.actor = actor;
    return s;
}
static Shot Fx(Shot s, float fovTo, float hand, float slow = 1) { s.fovTo = fovTo; s.hand = hand; s.slow = slow; return s; }

// ---------- Guion de las cinemáticas ----------
static const std::vector<Cutscene> &All() {
    static std::vector<Cutscene> list = {
        // Cap. 2: salen corriendo de la fábrica, la explosión los tira al suelo (en cámara lenta) y se dan vuelta a mirar el incendio
        { "fabrica_boom", "FÁBRICA ABANDONADA · 03:58",
          { Fx({ 3.4f, V(9, 0.8f, -35), V(7.0f, 1.7f, -33.5f), V(0, 2.4f, -17), V(-0.6f, 1.6f, -21), 58 }, 52, 0.5f),        // grúa baja: salen corriendo
            Fx(Follow(1.6f, 0, V(2.8f, 1.2f, -1.4f), V(2.5f, 1.1f, -0.5f), 1.2f, 1.1f, 50), 0, 0.9f),                          // travelling al costado de Frank
            Fx({ 1.0f, V(-3.4f, 0.35f, -33), V(-3.1f, 0.45f, -32.6f), V(-0.4f, 1.1f, -27.5f), V(-0.3f, 0.9f, -28.6f), 46 }, 40, 0.25f, 0.28f),   // cámara lenta: el salto
            Fx({ 4.2f, V(-0.2f, 1.1f, -34), V(0.2f, 1.5f, -35), V(0, 2.6f, -14), V(0, 3.4f, -10), 56 }, 0, 0.6f),            // de espaldas, recortados contra el fuego
            { 6.0f, V(3.5f, 1.6f, -31), V(3.0f, 1.6f, -32), V(0, 1.3f, -28.8f), V(0, 1.4f, -28.8f), 50, true } },
          { { A_FRANK, { { 0, V(-2, 0, -14.5f), NA, SC_RUN, 0.8f }, { 4.6f, V(-1.6f, 0, -27), NA, SC_RUN, 0.8f }, { 4.7f, V(-1.6f, 0, -27.2f), NA, SC_JUMP },
                         { 5.5f, V(-1.5f, 0, -28.8f), PI_F, SC_DUCK }, { 7.0f, V(-1.5f, 0, -28.8f), PI_F, SC_DUCK }, { 7.4f, V(-1.5f, 0, -28.8f), 0.15f, C_IDLE } } },
            { A_MILTON, { { 0, V(1.6f, 0, -14), NA, SC_RUN, 0.8f }, { 4.7f, V(1.2f, 0, -27.3f), NA, SC_RUN, 0.8f }, { 4.8f, V(1.2f, 0, -27.5f), NA, SC_JUMP },
                          { 5.6f, V(1.1f, 0, -29.2f), PI_F, SC_DUCK }, { 7.2f, V(1.1f, 0, -29.2f), PI_F, SC_DUCK }, { 7.6f, V(1.1f, 0, -29.2f), -0.15f, C_IDLE } } } },
          { { 8.2f, "c2o_m1" }, { 0, "c2o_f1" }, { 0, "c2o_m2" }, { 0, "c2o_f2" } },
          { { 2.2f, EV_BOOM, V(0, 3, -4), nullptr }, { 3.0f, EV_BOOM, V(-14, 2, -12), nullptr }, { 3.8f, EV_BOOM, V(14, 2, -12), nullptr },
            { 4.75f, EV_BIGBOOM, V(0, 3, -19), nullptr }, { 4.75f, EV_FLASH, {}, nullptr },
            { 6.4f, EV_BOOM, V(-16, 3, 4), nullptr }, { 7.1f, EV_BOOM, V(16, 3, 4), nullptr }, { 8.0f, EV_BIGBOOM, V(0, 5, 10), nullptr },
            { 5.0f, EV_FIRE, V(0, 2, -14), nullptr }, { 6.5f, EV_FIRE, V(-11, 2, -8), nullptr }, { 7.2f, EV_FIRE, V(11, 2, -8), nullptr } } },
        // Cap. 3: los archivos se suben, suena la alarma y Reyes corta la comunicación
        { "hospital_alarma", "QUIRÓFANO · PABELLÓN 6",
          { Fx({ 5.0f, V(15.4f, 1.6f, 28.0f), V(16.3f, 1.7f, 27.0f), V(17.6f, 1.2f, 25.6f), V(17.7f, 1.25f, 25.4f), 50 }, 40, 0.25f),   // detrás de Frank, acercándose a la pantalla
            Fx(Orbit(5.0f, V(13, 0, 21), 2.3f, 1.2f, 3.0f, 2.6f, 1.3f, 1.7f, 1.3f, 50), 0, 0.4f),                                        // vuelta alrededor de Milton, que vigila
            Fx({ 3.5f, V(13.0f, 0.22f, 16.2f), V(13.3f, 0.28f, 15.6f), V(13.6f, 0.5f, 10), V(13.8f, 0.9f, 11), 56 }, 0, 0.2f),          // a ras del piso: llegan los pies
            { 12.0f, V(14, 2.2f, 28), V(14, 2.0f, 27), V(14, 1.3f, 16), V(14, 1.4f, 15), 60, true } },
          { { A_FRANK, { { 0, V(17.3f, 0, 25.2f), 1.03f, C_IDLE }, { 14, V(17.3f, 0, 25.2f), 1.03f, C_IDLE }, { 15.4f, V(16, 0, 22.4f), NA, SC_WALK }, { 15.6f, V(16, 0, 22.4f), 3.0f, C_ATTACK } } },
            { A_MILTON, { { 0, V(13, 0, 21), 3.0f, C_ATTACK } } },
            { A_ZOMBIE, { { 0, V(14, 0, 6), 0, C_WALK }, { 13.5f, V(14, 0, 12.5f), 0, C_WALK }, { 22, V(14, 0, 16), 0, C_WALK } } },
            { A_ZOMBIE, { { 0, V(12.5f, 0, 5), 0, C_WALK }, { 14, V(12.6f, 0, 11.5f), 0, C_WALK }, { 23, V(13, 0, 15), 0, C_WALK } } } },
          { { 1.0f, "c3o_r1" }, { 0, "c3o_r2" }, { 0, "c3o_r3" }, { 0, "c3o_m1" }, { 0, "c3o_v1" }, { 0, "c3o_f1" } },
          { { 0.5f, EV_SOUND, {}, "power" }, { 0.6f, EV_RED, {}, nullptr }, { 6.5f, EV_RED, {}, nullptr }, { 12.0f, EV_RED, {}, nullptr }, { 10.5f, EV_SOUND, {}, "groan" } } },
        // Cap. 4: llegan caminando a la iglesia; Vidal habla desde el campanario
        { "iglesia_vidal", "IGLESIA DE SAN ROQUE",
          { Fx({ 6.0f, V(34, 9, 15), V(31, 2.2f, 8.5f), V(20, 1.0f, 2), V(23, 1.6f, -2), 55 }, 0, 0.3f),                 // grúa que baja desde arriba
            Fx(Follow(4.5f, 0, V(0.8f, 1.7f, 2.5f), V(0.6f, 1.6f, 2.0f), 4.0f, 1.5f, 52), 0, 0.7f),                      // detrás de Frank, caminando
            Fx({ 6.0f, V(23.2f, 0.6f, -1.4f), V(23.4f, 0.5f, -1.8f), V(24.5f, 1.8f, -5.5f), V(25.5f, 3.4f, -12.5f), 50 }, 44, 0.3f),   // contrapicado hacia el campanario
            { 12.0f, V(20, 1.7f, -9), V(21, 1.7f, -8), V(26, 2.0f, -14), V(26, 3.4f, -14), 55, true } },
          { { A_FRANK, { { 0, V(16.5f, 0, 0.5f), NA, SC_WALK }, { 5.5f, V(24, 0, 0.5f), NA, SC_WALK }, { 8.8f, V(24, 0, -4.5f), NA, SC_WALK }, { 9.0f, V(24, 0, -4.5f), PI_F, C_IDLE } }, -0.35f },
            { A_MILTON, { { 0, V(15.3f, 0, 1.3f), NA, SC_WALK }, { 5.8f, V(25.4f, 0, 1.1f), NA, SC_WALK }, { 9.1f, V(25.8f, 0, -4.2f), NA, SC_WALK }, { 9.3f, V(25.8f, 0, -4.2f), PI_F, C_IDLE } }, -0.35f } },
          { { 3.5f, "c4a_m1" }, { 10.6f, "c4a_v1" }, { 0, "c4a_f1" }, { 0, "c4a_v2" }, { 0, "c4a_f2" }, { 0, "c4a_v3" }, { 0, "c4a_m2" } },
          {} },
        // Cap. 4: al amanecer, un zombi se tira sobre Frank; Milton lo empuja (en cámara lenta) y lo muerden
        { "mordida", "PLAZA DE SAN ROQUE · 06:02",
          { Fx({ 3.0f, V(-6, 1.3f, 12), V(-5, 1.3f, 11), V(3, 1.2f, 6), V(4, 1.2f, 6), 50 }, 0, 0.3f),                               // plano general: amanece
            Fx({ 1.0f, V(8.6f, 0.6f, 10.4f), V(8.3f, 0.6f, 10.0f), V(9.6f, 1.2f, 12.4f), V(7.2f, 1.1f, 10.4f), 45 }, 0, 0.8f),            // el zombi arranca a correr
            Fx({ 1.0f, V(1.2f, 1.1f, 10.4f), V(1.5f, 1.0f, 10.0f), V(4.2f, 1.0f, 7.6f), V(4.0f, 1.0f, 7.8f), 42 }, 36, 0.2f, 0.3f),     // cámara lenta: Milton se interpone
            Fx({ 2.3f, V(2.6f, 1.6f, 3.8f), V(2.8f, 1.6f, 4.0f), V(5, 1.2f, 8.8f), V(5, 1.0f, 8.8f), 50 }, 0, 0.9f),                  // por encima del hombro de Frank: dispara
            Fx({ 12.0f, V(2.4f, 2.2f, 9.0f), V(2.7f, 1.8f, 8.7f), V(3.4f, 0.4f, 7.8f), V(3.4f, 0.5f, 7.6f), 46 }, 40, 0.3f) },       // Milton en el suelo
          { { A_FRANK, { { 0, V(4, 0, 6), -2.2f, C_IDLE }, { 4.45f, V(4, 0, 6), -2.2f, C_IDLE }, { 4.6f, V(3.7f, 0, 5.6f), -2.2f, C_HIT },
                         { 5.4f, V(3.7f, 0, 5.6f), 0.35f, C_ATTACK }, { 7.4f, V(3.7f, 0, 5.6f), 0.35f, C_ATTACK }, { 8.4f, V(3.6f, 0, 7.0f), NA, SC_WALK }, { 8.6f, V(3.6f, 0, 7.0f), -0.25f, SC_DUCK } } },
            { A_MILTON, { { 0, V(-1, 0, 9), 2.0f, C_IDLE }, { 3.3f, V(-1, 0, 9), NA, SC_RUN }, { 4.3f, V(3.6f, 0, 7.6f), NA, SC_RUN }, { 4.4f, V(3.6f, 0, 7.6f), 0.9f, SC_PUNCH },
                          { 4.9f, V(3.6f, 0, 7.7f), 0.9f, C_HIT }, { 5.5f, V(3.4f, 0, 7.8f), 0.9f, C_DIE } } },
            { A_ZOMBIE, { { 0, V(10, 0, 13), -2.4f, C_IDLE }, { 3.0f, V(10, 0, 13), NA, C_RUN }, { 4.3f, V(5, 0, 8.8f), NA, C_ATTACK }, { 6.4f, V(5, 0, 8.8f), -2.4f, C_DIE } } } },
          { { 0.6f, "c4b_m0" }, { 3.75f, "c4b_m1" }, { 7.8f, "c4b_m2" }, { 0, "c4b_f1" }, { 0, "c4b_v1" }, { 0, "c4b_m3" }, { 0, "c4b_f2" } },
          { { 5.8f, EV_SHOT, V(4.0f, 1.4f, 6.4f), nullptr }, { 6.05f, EV_SHOT, V(4.0f, 1.4f, 6.4f), nullptr }, { 6.3f, EV_SHOT, V(4.0f, 1.4f, 6.4f), nullptr },
            { 4.5f, EV_SOUND, {}, "hurt" }, { 4.4f, EV_SOUND, {}, "groan" } } },
        // Cap. 5: Reyes habla por los altavoces mientras la cámara recorre la base desde el aire
        { "altavoz", "FUERTE REYES · ALTAVOCES",
          { Fx({ 9.0f, V(-18, 10, -13), V(0, 11, -14), V(-5, 0, 2), V(5, 0, 2), 60 }, 0, 0.25f),
            Fx({ 9.0f, V(18, 9, -10), V(18, 7, 8), V(5, 0, 2), V(-5, 1, 6), 60 }, 0, 0.25f),
            Fx(Orbit(6.0f, V(0.1f, 0, -4.85f), -0.7f, 0.5f, 3.2f, 2.8f, 0.5f, 0.8f, 1.7f, 52), 0, 0.3f),                // contrapicado: miran los altavoces
            { 12.0f, V(3, 1.6f, -9), V(2.5f, 1.8f, -8), V(0, 2.5f, -3), V(0, 4, -3), 52, true } },
          { { A_FRANK, { { 0, V(-0.8f, 0, -4.5f), 0, C_IDLE } }, -0.45f },
            { A_MILTON, { { 0, V(1.0f, 0, -5.2f), 0.4f, C_IDLE } }, -0.45f },
            { A_ZOMBIE, { { 0, V(-12, 0, 6), NA, C_WALK }, { 30, V(-6, 0, 10), NA, C_WALK } } },
            { A_ZOMBIE, { { 0, V(10, 0, 9), NA, C_WALK }, { 30, V(4, 0, 4), NA, C_WALK } } },
            { A_ZOMBIE, { { 0, V(14, 0, -6), NA, C_WALK }, { 30, V(8, 0, 2), NA, C_WALK } } },
            { A_ZOMBIE, { { 0, V(-15, 0, -8), NA, C_WALK }, { 30, V(-9, 0, -2), NA, C_WALK } } } },
          { { 1.0f, "c5a_r1" }, { 0, "c5a_r2" }, { 0, "c5a_r3" }, { 0, "c5a_r4" }, { 0, "c5a_m1" }, { 0, "c5a_f1" } },
          {} },
        // Cap. 6: Reyes despierta a Lázaro (efecto vértigo sobre el monstruo)
        { "lazaro", "NÚCLEO · NIVEL -3",
          { Fx({ 6.0f, V(0, 1.8f, 32), V(0, 2.0f, 34), V(0, 1.8f, 44), V(0, 1.9f, 44), 55 }, 50, 0.3f),
            Fx({ 2.6f, V(-4, 1.2f, 38), V(-3.5f, 1.0f, 38.5f), V(0, 2.4f, 42.5f), V(0, 2.6f, 42.5f), 50 }, 0, 0.5f),
            Fx({ 2.6f, V(0, 1.7f, 37.0f), V(0, 1.6f, 34.8f), V(0, 2.3f, 42.5f), V(0, 2.4f, 42.5f), 26 }, 56, 0.2f),      // vértigo: se aleja mientras abre el zoom
            Fx(Orbit(2.4f, V(0.1f, 0, 35.3f), 2.5f, 3.5f, 2.6f, 2.4f, 1.5f, 1.4f, 1.3f, 55), 0, 0.6f),                   // detrás de ellos, apuntando
            { 10.0f, V(-1, 1.7f, 31.5f), V(-0.5f, 1.7f, 32.5f), V(0, 1.6f, 41), V(0, 1.8f, 41), 58, true } },
          { { A_FRANK, { { 0, V(-1.8f, 0, 35), 0, C_ATTACK } } },
            { A_MILTON, { { 0, V(2, 0, 35.5f), -0.15f, C_ATTACK } } },
            { A_BOSS, { { 0, V(0, 0, 42.5f), PI_F, C_IDLE }, { 6.5f, V(0, 0, 42.5f), PI_F, C_SCREAM }, { 10.5f, V(0, 0, 42.5f), PI_F, C_IDLE } } } },
          { { 0.6f, "c6a_r1" }, { 0, "c6a_r2" }, { 11.4f, "c6a_m1" }, { 0, "c6a_f1" } },
          { { 6.2f, EV_GREEN, V(0, 2, 45), nullptr }, { 6.2f, EV_FLASH, {}, nullptr }, { 6.3f, EV_SHAKE, {}, nullptr }, { 6.5f, EV_SOUND, {}, "groan" }, { 6.6f, EV_SOUND, {}, "groan" } } },
        // Cap. 6: la cura para Milton y la salida
        { "final", "NÚCLEO · 05:41",
          { Fx({ 8.0f, V(4, 1.7f, 36), V(3.5f, 1.6f, 37), V(0, 1.3f, 40), V(0, 1.3f, 40), 50 }, 44, 0.3f),
            { 8.0f, V(-3, 1.4f, 38), V(-2.6f, 1.5f, 38.6f), V(0.8f, 1.5f, 40), V(0.8f, 1.5f, 40), 45, true },
            Fx({ 12.0f, V(0, 2.6f, 26), V(0, 3.6f, 22), V(0, 1.2f, 36), V(0, 1.2f, 31), 60 }, 0, 0.3f) },
          { { A_FRANK, { { 0, V(-0.9f, 0, 40), PI_F / 2, C_IDLE }, { 17, V(-0.9f, 0, 40), PI_F / 2, C_IDLE }, { 24, V(-0.6f, 0, 29), NA, SC_WALK } } },
            { A_MILTON, { { 0, V(0.9f, 0, 40), -PI_F / 2, C_IDLE }, { 9, V(0.9f, 0, 40), -PI_F / 2, C_SCREAM }, { 12, V(0.9f, 0, 40), -PI_F / 2, C_IDLE }, { 17.4f, V(0.9f, 0, 40), -PI_F / 2, C_IDLE }, { 24.4f, V(0.8f, 0, 29.4f), NA, SC_WALK } } } },
          { { 1.0f, "c6b_v1" }, { 0, "c6b_m1" }, { 0, "c6b_v2" }, { 0, "c6b_m2" }, { 0, "c6b_f1" }, { 0, "c6b_m3" } },
          {} },
    };
    return list;
}

// ---------- Estado ----------
static const Cutscene *cur = nullptr;
static float csT = 0, csLen = 0, shake = 0, redT = 0, timeScale = 1, followYaw = 0;
static int lastShot = -1;
static std::vector<float> lineStart, lineLen;
static std::vector<bool> linePlayed, evDone;
static std::vector<int> puppetIds;
static std::vector<Sound *> lineSnd;
static int prevState = GS_PLAYING;
static float shotEnd[16];

// ---------- Charla al empezar cada capítulo ----------
// Frank y Milton frente a frente donde arranca el mapa: plano general, plano de los dos de costado
// y después primeros planos de quien habla (el otro lo mira).
static Cutscene charla;
static bool MakeCharla(const char *id) {
    if (strncmp(id, "charla_", 7)) return false;
    static const std::vector<std::vector<const char *>> LINES = {
        { "o1_m1", "o1_f1", "o1_m2", "o1_f2" },
        { "o2_f1", "o2_m1", "o2_f2", "o2_m2" },
        { "o3_m1", "o3_f1", "o3_m2", "o3_f2", "o3_m3" },
        { "o4_m1", "o4_f1", "o4_m2", "o4_f2" },
        { "o5_f1", "o5_m1", "o5_f2", "o5_m2" },
        { "o6_m1", "o6_f1", "o6_m2", "o6_f2" } };
    int ch = atoi(id + 7);
    if (ch < 0 || ch >= (int)LINES.size()) return false;
    float yaw;
    Vector2 s = MapStart(yaw);
    Vector3 S{ s.x, 0, s.y }, f{ -sinf(yaw), 0, -cosf(yaw) }, r{ cosf(yaw), 0, -sinf(yaw) };
    auto at = [&](float fw, float rt, float y) { return Vector3{ S.x + f.x * fw + r.x * rt, y, S.z + f.z * fw + r.z * rt }; };
    Vector3 pf = at(0, -1.3f, 0), pm = at(0.1f, 1.3f, 0);
    float yf = atan2f(r.x, r.z), ym = atan2f(-r.x, -r.z);   // uno frente al otro
    charla = Cutscene{ "charla", nullptr,
        { Fx({ 3.2f, at(-4.8f, 1.4f, 2.4f), at(-3.6f, 0.9f, 1.9f), at(0, 0, 1.2f), at(0, 0, 1.3f), 55 }, 50, 0.3f),
          Fx({ 2.6f, at(2.6f, -0.3f, 1.3f), at(2.3f, 0.1f, 1.35f), at(0, 0, 1.25f), at(0, 0, 1.3f), 48 }, 0, 0.4f),
          { 6.0f, at(3.0f, 0.4f, 1.5f), at(2.7f, 0.2f, 1.5f), at(0, 0, 1.25f), at(0, 0, 1.25f), 50, true } },
        { { A_FRANK, { { 0, pf, yf, C_IDLE } } }, { A_MILTON, { { 0, pm, ym, C_IDLE } } } },
        {}, {} };
    float t = 1.4f;
    for (auto lid : LINES[ch]) { charla.lines.push_back({ t, lid }); t = 0; }
    return true;
}

bool CutsceneActive() { return cur != nullptr; }
const char *CutsceneId() { return cur ? cur->id : ""; }
float CutsceneTimeScale() { return cur ? timeScale : 1.0f; }

void StartCutscene(const char *id) {
    cur = nullptr;
    for (auto &c : All()) if (!strcmp(c.id, id)) cur = &c;
    if (!cur && MakeCharla(id)) cur = &charla;
    if (!cur) { StoryOnCutsceneEnd(id); return; }
    csT = 0; shake = 0; redT = 0; timeScale = 1; lastShot = -1;
    // horario de las frases: cada una espera a que termine la anterior
    size_t n = cur->lines.size();
    lineStart.assign(n, 0); lineLen.assign(n, 0); linePlayed.assign(n, false); lineSnd.assign(n, nullptr);
    float t = 0;
    for (size_t i = 0; i < n; i++) {
        lineSnd[i] = StoryLineSound(cur->lines[i].id);
        int who; const char *txt = StoryLineText(cur->lines[i].id, who);
        lineLen[i] = lineSnd[i] && lineSnd[i]->stream.sampleRate ? (float)lineSnd[i]->frameCount / lineSnd[i]->stream.sampleRate : 1.2f + strlen(txt) * 0.055f;
        t = std::max(t + (i ? 0.3f : 0), cur->lines[i].t);
        lineStart[i] = t;
        t += lineLen[i];
    }
    float shots = 0;
    for (auto &s : cur->shots) shots += s.dur;
    csLen = std::max(shots, t + 1.2f);
    // la última toma dura lo que haga falta
    float acc = 0;
    for (size_t i = 0; i < cur->shots.size() && i < 16; i++) { acc += cur->shots[i].dur; shotEnd[i] = acc; }
    if (!cur->shots.empty()) shotEnd[std::min<size_t>(cur->shots.size(), 16) - 1] = csLen;
    evDone.assign(cur->events.size(), false);
    // actores
    cutsceneActors.clear();
    puppetIds.clear();
    for (auto &a : cur->actors) {
        if (a.kind == A_ZOMBIE || a.kind == A_BOSS) puppetIds.push_back(SpawnPuppet(a.keys[0].p, 0, a.kind == A_BOSS ? 1.8f : 1.0f));
        else puppetIds.push_back(-1);
    }
    allyView.visible = false;
    soldierView.visible = false;
    StopRadioForCutscene();
    prevState = game.state;
    game.state = GS_CUTSCENE;
    gunVisible = false;
    scoped = false;
}

static void EndCutscene() {
    for (size_t i = 0; i < lineSnd.size(); i++) if (lineSnd[i]) StopSound(*lineSnd[i]);
    cutsceneActors.clear();
    ClearPuppets();
    const char *id = cur->id;
    cur = nullptr;
    timeScale = 1;
    game.state = GS_PLAYING;
    StoryOnCutsceneEnd(id);
}

static float Smooth(float x) { x = clampf(x, 0, 1); return x * x * (3 - 2 * x); }
static float WrapAngle(float a) { while (a > PI_F) a -= 2 * PI_F; while (a < -PI_F) a += 2 * PI_F; return a; }
static float LerpAngle(float a, float b, float k) { return a + WrapAngle(b - a) * k; }

// Cómo está un actor en el momento t: posición, hacia dónde mira, animación (y la anterior, para mezclarlas)
struct ActorPose { Vector3 p; float yaw; int clip; float animT; int prevClip = -1; float prevT = 0, blend = 0; };
static float SegYaw(const std::vector<Key> &k, size_t i) {   // hacia dónde mira en el tramo i
    if (!std::isnan(k[i].yaw)) return k[i].yaw;
    Vector3 d = i + 1 < k.size() ? Vector3Subtract(k[i + 1].p, k[i].p) : (i > 0 ? Vector3Subtract(k[i].p, k[i - 1].p) : Vector3{ 0, 0, 1 });
    if (hyp(d.x, d.z) < 0.01f) return i > 0 ? SegYaw(k, i - 1) : 0;
    return atan2f(d.x, d.z);
}
static float ClipRate(int clip) { return clip == SC_WALK ? 0.55f : clip == C_WALK ? 1.1f : 1.0f; }
static ActorPose SampleActor(const Actor &a, float t) {
    const auto &k = a.keys;
    size_t i = 0;
    while (i + 1 < k.size() && k[i + 1].t <= t) i++;
    ActorPose o{};
    o.clip = k[i].clip;
    if (i + 1 < k.size()) o.p = Vector3Lerp(k[i].p, k[i + 1].p, clampf((t - k[i].t) / std::max(0.01f, k[i + 1].t - k[i].t), 0, 1));
    else o.p = k[i].p;
    o.yaw = SegYaw(k, i);
    if (i > 0 && t - k[i].t < 0.35f) o.yaw = LerpAngle(SegYaw(k, i - 1), o.yaw, Smooth((t - k[i].t) / 0.35f));   // gira de a poco
    // la animación sigue corriendo mientras no cambie de clip (aunque cambie de tramo)
    size_t j = i;
    while (j > 0 && k[j - 1].clip == k[i].clip) j--;
    o.animT = (t - k[j].t) * k[i].rate * ClipRate(o.clip);
    if (j > 0 && t - k[j].t < 0.3f) {   // mezcla con la animación anterior
        size_t jj = j - 1;
        while (jj > 0 && k[jj - 1].clip == k[j - 1].clip) jj--;
        o.prevClip = k[j - 1].clip;
        o.prevT = (t - k[jj].t) * k[j - 1].rate * ClipRate(o.prevClip);
        o.blend = 1 - (t - k[j].t) / 0.3f;
    }
    return o;
}
static float HoldTime(int clip, float t) {   // poses que se sostienen (caído, agachado)
    if (clip == C_DIE) return std::min(t, 1.4f);
    if (clip == SC_DUCK) return std::min(t, 0.6f);
    return t;
}

void UpdateCutscene(float dt) {
    if (!cur) return;
    bool skip = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE);
    if (skip && csT > 0.5f) { EndCutscene(); return; }
    // cámara lenta según la toma
    size_t si = 0;
    while (si + 1 < cur->shots.size() && csT >= shotEnd[si]) si++;
    timeScale += (cur->shots[si].slow - timeScale) * dampk(12, dt);
    float sdt = dt * timeScale;
    csT += sdt;
    extern float flashK;
    flashK = std::max(0.0f, flashK - dt * 0.8f);   // el destello se apaga (Update no corre en la cinemática)
    shake = std::max(0.0f, shake - dt * 1.5f);
    redT = std::max(0.0f, redT - sdt);
    // frases
    for (size_t i = 0; i < lineStart.size(); i++)
        if (!linePlayed[i] && csT >= lineStart[i]) {
            linePlayed[i] = true;
            if (lineSnd[i]) { SetSoundVolume(*lineSnd[i], clampf(opts.sfx * 1.1f, 0, 1)); PlaySound(*lineSnd[i]); }
        }
    // efectos
    for (size_t i = 0; i < cur->events.size(); i++) {
        const Event &e = cur->events[i];
        if (e.kind == EV_FIRE && csT >= e.t) {   // incendio que sigue ardiendo
            FireFX(e.p, 3.0f, 1.0f, sdt);
            boomLightPos = Vector3{ e.p.x, e.p.y + 1, e.p.z - 6 }; boomLightI = std::max(boomLightI, 16.0f + sinf(csT * 17) * 2.5f + sinf(csT * 7.3f) * 1.5f);   // el incendio ilumina a los que miran
            continue;
        }
        if (evDone[i] || csT < e.t) continue;
        evDone[i] = true;
        switch (e.kind) {
            case EV_BOOM: ExplosionFX(e.p, 1.6f, true); shake = std::max(shake, 0.6f); break;
            case EV_BIGBOOM: ExplosionFX(e.p, 2.8f, true); shake = std::max(shake, 1.2f); break;
            case EV_FLASH: WhiteFlash(0.9f); break;
            case EV_SHAKE: shake = 1.2f; break;
            case EV_SOUND: if (e.s) Play(e.s, 0.9f); break;
            case EV_SHOT: Burst(e.p, Color{ 255, 210, 120, 255 }, 6, 2, 0.05f, 8); boomLightPos = e.p; boomLightI = std::max(boomLightI, 10.0f); PlayGunAt(W_PISTOL, e.p); shake = std::max(shake, 0.15f); break;
            case EV_GREEN: Burst(e.p, Color{ 80, 255, 120, 255 }, 60, 7, 0.12f, 7); Burst(e.p, Color{ 200, 255, 220, 255 }, 40, 4, 0.06f, 9); boomLightPos = e.p; boomLightI = 30; Play("boom", 0.7f); Play("bigboom", 0.6f); break;
            case EV_RED: redT = 5.5f; break;
        }
    }
    // quién está hablando ahora (Frank o Milton en persona: se le mueve la cabeza y la cámara lo puede encuadrar)
    int speakerKind = -1; float talkLv = 0, talkK = 0;
    for (size_t i = 0; i < lineStart.size(); i++) {
        float k = csT - lineStart[i];
        if (k < 0 || k > lineLen[i]) continue;
        int who; StoryLineText(cur->lines[i].id, who);
        const char *nm = StorySpeakerName(who);
        speakerKind = !strcmp(nm, "FRANK") ? A_FRANK : !strcmp(nm, "MILTON") ? A_MILTON : -1;
        talkLv = VoiceEnvelope(StoryLineFile(cur->lines[i].id), k);
        talkK = k;
    }
    // actores
    cutsceneActors.clear();
    int speakerIdx = -1, listenerIdx = -1;
    std::vector<ActorPose> poses(cur->actors.size());
    for (size_t i = 0; i < cur->actors.size(); i++) poses[i] = SampleActor(cur->actors[i], csT);
    for (size_t i = 0; i < cur->actors.size(); i++) {
        const Actor &a = cur->actors[i];
        if (a.kind == speakerKind) speakerIdx = (int)i;
        else if ((a.kind == A_FRANK || a.kind == A_MILTON) && listenerIdx < 0) listenerIdx = (int)i;
    }
    for (size_t i = 0; i < cur->actors.size(); i++) {
        const Actor &a = cur->actors[i];
        const ActorPose &o = poses[i];
        if (a.kind == A_FRANK || a.kind == A_MILTON || a.kind == A_GUARD) {
            SoldierView v{ true, o.p, o.yaw, o.clip, HoldTime(o.clip, o.animT), a.kind == A_MILTON ? AllySkin() : a.kind == A_GUARD ? SKIN_GUARDIA : SKIN_FRANK, 0 };
            if (o.prevClip >= 0) { v.clip2 = o.prevClip; v.t2 = HoldTime(o.prevClip, o.prevT); v.w2 = o.blend; }
            bool speaking = (int)i == speakerIdx;
            if (speaking && o.clip == C_IDLE) { v.clip = C_TALK; v.t = csT; }   // asiente mientras habla
            if (speaking) v.talk = std::max(0.25f, talkLv);
            // el que escucha gira la cabeza hacia el que habla
            if (!speaking && speakerIdx >= 0 && (o.clip == C_IDLE || o.clip == C_ATTACK)) {
                Vector3 d = Vector3Subtract(poses[speakerIdx].p, o.p);
                v.lookYaw = clampf(WrapAngle(atan2f(d.x, d.z) - o.yaw), -0.9f, 0.9f) * 0.8f;
            }
            v.lookPitch = a.pitch;
            cutsceneActors.push_back(v);
        } else {
            float t = (o.clip == C_WALK || o.clip == C_IDLE || o.clip == C_RUN) ? csT + i : o.animT;
            PuppetPose(puppetIds[i], o.p, o.yaw, o.clip, t);
        }
    }
    // cámara
    const Shot &s = cur->shots[si];
    if ((int)si != lastShot) { lastShot = (int)si; if (s.mode == CAM_FOLLOW && s.actor >= 0) followYaw = poses[s.actor].yaw; }
    float s0 = si ? shotEnd[si - 1] : 0, s1 = shotEnd[si];
    float u = Smooth((csT - s0) / std::max(0.01f, s1 - s0));
    float lin = clampf((csT - s0) / std::max(0.01f, s1 - s0), 0, 1);
    Vector3 pos = Vector3Lerp(s.camFrom, s.camTo, u), look = Vector3Lerp(s.lookFrom, s.lookTo, u);
    float fov = s.fovTo > 0 ? lerpf(s.fov, s.fovTo, lin) : s.fov;
    if (s.mode == CAM_ORBIT) {
        float a = lerpf(s.camFrom.x, s.camTo.x, u), h = lerpf(s.camFrom.y, s.camTo.y, u), r = lerpf(s.camFrom.z, s.camTo.z, u);
        pos = Vector3{ s.lookFrom.x + sinf(a) * r, h, s.lookFrom.z + cosf(a) * r };
        look = s.lookFrom;
    } else if (s.mode == CAM_FOLLOW && s.actor >= 0 && s.actor < (int)poses.size()) {
        Vector3 P = poses[s.actor].p;
        followYaw = LerpAngle(followYaw, poses[s.actor].yaw, dampk(3, dt));
        Vector3 f{ sinf(followYaw), 0, cosf(followYaw) }, rt{ cosf(followYaw), 0, -sinf(followYaw) };
        Vector3 off = Vector3Lerp(s.camFrom, s.camTo, u);
        pos = Vector3{ P.x + rt.x * off.x - f.x * off.z, off.y, P.z + rt.z * off.x - f.z * off.z };
        look = Vector3{ P.x + f.x * s.lookFrom.z, s.lookFrom.y, P.z + f.z * s.lookFrom.z };
    }
    if (s.talkCam && speakerIdx >= 0) {
        // primer plano del que habla, por encima del hombro del que escucha (o de frente si está solo)
        Vector3 P = poses[speakerIdx].p;
        Vector3 head{ P.x, 1.42f, P.z };
        Vector3 from;
        if (listenerIdx >= 0 && listenerIdx != speakerIdx) {
            Vector3 L = poses[listenerIdx].p;
            Vector3 d = Vector3Normalize(Vector3{ P.x - L.x, 0, P.z - L.z });
            Vector3 side{ -d.z, 0, d.x };
            from = Vector3{ L.x - d.x * 1.35f + side.x * 1.05f, 1.72f, L.z - d.z * 1.35f + side.z * 1.05f };
        } else {
            float y = poses[speakerIdx].yaw;
            from = Vector3{ P.x + sinf(y) * 1.7f + cosf(y) * 0.4f, 1.55f, P.z + cosf(y) * 1.7f - sinf(y) * 0.4f };
        }
        Vector3 toHead = Vector3Normalize(Vector3Subtract(head, from));
        pos = Vector3Add(from, Vector3Scale(toHead, std::min(talkK, 4.0f) * 0.05f));   // se acerca despacito
        look = head;
        fov = 42;
    }
    if (shake > 0) { pos.x += frand(-1, 1) * shake * 0.12f; pos.y += frand(-1, 1) * shake * 0.1f; }
    // cámara en mano: vaivén suave (suma de ondas), más fuerte en las tomas de acción
    float h = s.hand * (s.talkCam ? 0.5f : 1);
    float rt = (float)GetTime();
    pos.x += (sinf(rt * 1.1f) + 0.5f * sinf(rt * 2.7f + 1.3f)) * 0.03f * h;
    pos.y += (sinf(rt * 1.6f + 0.7f) + 0.4f * sinf(rt * 3.3f)) * 0.025f * h;
    look.x += sinf(rt * 0.8f + 2.1f) * 0.04f * h; look.y += sinf(rt * 1.2f + 0.4f) * 0.03f * h;
    cam.position = pos; cam.target = look; cam.up = V(0, 1, 0);
    camFov = fov;
    gunVisible = false;
    if (csT >= csLen) EndCutscene();
}

// ---------- Interfaz ----------
static void CText(const Font &f, const char *t, float x, float y, float size, Color c, float align, float spacing = 1) {
    Vector2 m = MeasureTextEx(f, t, size, spacing);
    x -= m.x * align;
    DrawTextEx(f, t, Vector2{ x + 3, y + 3 }, size, spacing, Color{ 0, 0, 0, (unsigned char)(c.a * 0.8f) });
    DrawTextEx(f, t, Vector2{ x, y }, size, spacing, c);
}

// Aspecto de película: viñeta oscura en los bordes y grano que cambia en cada cuadro
void DrawFilmLook(float k) {
    static Texture2D vig{}, grain{};
    if (!vig.id) {
        Image v = GenImageColor(256, 256, BLANK);
        Color *px = (Color *)v.data;
        for (int y = 0; y < 256; y++) for (int x = 0; x < 256; x++) {
            float u = (x + 0.5f) / 128 - 1, w = (y + 0.5f) / 128 - 1, r = sqrtf(u * u * 0.8f + w * w);
            px[y * 256 + x] = Color{ 0, 0, 0, (unsigned char)(clampf((r - 0.6f) / 0.8f, 0, 1) * 130) };
        }
        vig = LoadTextureFromImage(v); UnloadImage(v);
        SetTextureFilter(vig, TEXTURE_FILTER_BILINEAR);
        Image g = GenImageWhiteNoise(256, 256, 0.5f);
        grain = LoadTextureFromImage(g); UnloadImage(g);
        SetTextureWrap(grain, TEXTURE_WRAP_REPEAT);
    }
    if (k <= 0) return;
    int W = GetScreenWidth(), Hh = GetScreenHeight();
    DrawTexturePro(vig, Rectangle{ 0, 0, 256, 256 }, Rectangle{ 0, 0, (float)W, (float)Hh }, Vector2{ 0, 0 }, 0, Color{ 255, 255, 255, (unsigned char)(255 * k) });
    float ox = (float)GetRandomValue(0, 255), oy = (float)GetRandomValue(0, 255);
    DrawTexturePro(grain, Rectangle{ ox, oy, W / 2.0f, Hh / 2.0f }, Rectangle{ 0, 0, (float)W, (float)Hh }, Vector2{ 0, 0 }, 0, Color{ 255, 255, 255, (unsigned char)(14 * k) });
}

void DrawCutscene() {
    if (!cur) return;
    int W = GetScreenWidth(), Hh = GetScreenHeight();
    float s = Hh / 900.0f;
    float bars = Smooth(csT / 0.6f) * (1 - Smooth((csT - (csLen - 0.8f)) / 0.8f));
    float bh = Hh * 0.12f * bars;
    if (redT > 0 && ((int)(csT * 3) % 2 == 0)) DrawRectangle(0, 0, W, Hh, Color{ 160, 0, 0, (unsigned char)(50 * std::min(1.0f, redT)) });
    DrawFilmLook(bars);
    DrawRectangle(0, 0, W, (int)bh, BLACK);
    DrawRectangle(0, Hh - (int)bh, W, (int)bh + 1, BLACK);
    // fundido desde y hacia negro; entre tomas, corte seco (salvo las que piden fundido)
    float fade = std::max(1 - Smooth(csT / 0.8f), Smooth((csT - (csLen - 0.7f)) / 0.7f));
    for (size_t i = 0; i + 1 < cur->shots.size() && i < 15; i++) {
        if (!cur->shots[i + 1].fade) continue;
        float d = fabsf(csT - shotEnd[i]);
        if (d < 0.25f) fade = std::max(fade, 1 - d / 0.25f);
    }
    if (fade > 0) DrawRectangle(0, 0, W, Hh, Color{ 0, 0, 0, (unsigned char)(fade * 255) });
    // lugar y hora al principio
    if (cur->caption && csT < 4.5f) {
        float a = Smooth((csT - 0.5f) / 0.5f) * (1 - Smooth((csT - 3.8f) / 0.6f));
        CText(fontUI, cur->caption, 70 * s, Hh - bh - 70 * s, 28 * s, Color{ 255, 216, 74, (unsigned char)(a * 255) }, 0, 3);
    }
    // subtítulos
    for (size_t i = 0; i < lineStart.size(); i++) {
        float k = csT - lineStart[i];
        if (k < 0 || k > lineLen[i] + 0.2f) continue;
        int who; const char *txt = StoryLineText(cur->lines[i].id, who);
        float a = Smooth(k / 0.15f) * (1 - Smooth((k - lineLen[i]) / 0.2f));
        float fs = 28 * s;
        float y = Hh - bh / 2 - 14 * s;
        float tw = MeasureTextEx(fontUI, txt, fs, 1).x;
        if (tw > W * 0.75f) { fs *= W * 0.75f / tw; tw = W * 0.75f; }
        Color wc = StorySpeakerColor(who); wc.a = (unsigned char)(a * 230);
        CText(fontUI, TextFormat("%s ·", StorySpeakerName(who)), W / 2.0f - tw / 2 - 12 * s, y + 4 * s, 20 * s, wc, 1, 2);
        CText(fontUI, txt, W / 2.0f, y, fs, Color{ 240, 240, 240, (unsigned char)(a * 255) }, 0.5f);
    }
    if (csT > 0.6f && csT < csLen - 0.8f)
        CText(fontUI, "ESPACIO para saltar", W - 30 * s, Hh - bh + (bh - 22 * s) / 2, 18 * s, Color{ 180, 180, 180, 160 }, 1);
}
