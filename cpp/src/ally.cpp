// Compañero bot: el cabo Milton. Te sigue por el mapa, dispara a los zombis que ve,
// los zombis también lo atacan, y si cae lo levantas manteniendo E (o se levanta solo).
// Habla con frases propias (assets/sfx/milton/<tipo><n>.wav) sin pisarse con la radio ni con Frank.
#include "game.h"

Ally ally;
SoldierView allyView;

static const float ALLY_RADIUS = 0.38f;
static const float ALLY_REVIVE_TIME = 3.0f;
static const float ALLY_SELF_REVIVE = 40.0f;
static int iconIdx = -1;
static Vector3 lastPos{};
static float stuckT = 0, moveT = 0;
static float hitAnimT = 0;
static bool enabled = false;
static float allyDt = 0.016f;   // dt del último fotograma (para mantener E)

// ---------- Voz ----------
enum AllyVoice { AV_KILL, AV_RELOAD, AV_DOWN, AV_REVIVED, AV_SELF, AV_HURT, AV_DOGS, AV_ROUND, AV_WARN, AV_WAIT, AV_COUNT };
static const char *AV_FILE[AV_COUNT] = { "baja", "recarga", "caido", "levantado", "solo", "herido", "perros", "ronda", "aviso", "espera" };
static const std::vector<const char *> AV_TEXT[AV_COUNT] = {
    { "¡Uno menos!", "¡Te tengo, feo!", "¡Al piso!", "¡Ese no se levanta más!", "¡Cuenta esa para mí, Frank!" },
    { "¡Recargando, cúbreme!", "¡Cambio de cargador!", "¡Cúbreme un segundo!" },
    { "¡Frank! ¡Me tiraron!", "¡Estoy en el piso, ayúdame!", "¡Levántame, hermano!" },
    { "¡Gracias, te debo una!", "¡Volví! ¿Me extrañaste?", "Eso dolió... sigamos." },
    { "Tranquilo, me levanto solo." },
    { "¡Me están mordiendo!", "¡Sácamelos de encima!" },
    { "¡Perros! ¡Odio los perros!", "¡Ladridos! ¡Espalda con espalda!" },
    { "Ahí vienen otra vez.", "¿No se cansan nunca estos?", "Otra ronda. Yo invito." },
    { "¡Frank, detrás tuyo!", "¡Cuidado, a tu izquierda!" },
    { "¡Espérame, Frank!", "¡No me dejes atrás!" },
};
static std::vector<Sound> avSounds[AV_COUNT];
static int avLast[AV_COUNT];
static Sound *avPlaying = nullptr;
static const char *avSub = nullptr;
static std::string avFile;   // archivo de la frase que suena (para mover la cabeza)
static float avStart = 0;
static float avSubUntil = 0, avNext = 0;
static bool voicesLoaded = false;

static void LoadAllyVoices() {
    if (voicesLoaded || !IsAudioDeviceReady()) return;
    voicesLoaded = true;
    for (int k = 0; k < AV_COUNT; k++) {
        avLast[k] = -1;
        for (int i = 0; i < 10; i++) {
            const char *p = TextFormat("assets/sfx/milton/%s%d.wav", AV_FILE[k], i);
            if (!FileExists(p)) break;
            avSounds[k].push_back(LoadSound(p));
        }
    }
}
bool AllySpeaking() { return (avPlaying && IsSoundPlaying(*avPlaying)) || DialogTalking(); }
static void AllySay(int kind, float chance = 1, bool important = false) {
    if (!enabled || game.state != GS_PLAYING || StoryTalking() || FrankSpeaking() || AllySpeaking()) return;
    if (T < avNext && !important) return;
    if (frand(0, 1) > chance) return;
    int n = (int)AV_TEXT[kind].size(), i = irand(n);
    if (n > 1 && i == avLast[kind]) i = (i + 1) % n;
    avLast[kind] = i;
    float len = 1.0f + strlen(AV_TEXT[kind][i]) * 0.05f;
    avPlaying = nullptr;
    if (i < (int)avSounds[kind].size()) {
        avPlaying = &avSounds[kind][i];
        SetSoundVolume(*avPlaying, clampf(opts.sfx, 0, 1));
        PlaySound(*avPlaying);
        len = (float)avPlaying->frameCount / avPlaying->stream.sampleRate;
    }
    avSub = AV_TEXT[kind][i];
    avFile = TextFormat("assets/sfx/milton/%s%d.wav", AV_FILE[kind], i);
    avStart = T;
    avSubUntil = T + len + 0.5f;
    avNext = T + 5 + frand(0, 4);
}
const char *AllySubtitle(float &alpha) {
    if (!avSub || T > avSubUntil) return nullptr;
    alpha = clampf((avSubUntil - T) / 0.3f, 0, 1);
    return avSub;
}
void AllyOnRound(bool dogs) { if (ally.active && !ally.downed) AllySay(dogs ? AV_DOGS : AV_ROUND, dogs ? 1.0f : 0.5f, dogs); }

// ---------- Activación ----------
bool AllyEnabled() {
    if (NetRole() != NET_OFF || Practice()) return false;
    if (StoryActive()) return StoryWantsAlly();
    return opts.ally;
}

static Vector2 FreeSpotNear(Vector3 p, float back, float side, float yaw) {
    float fx = -sinf(yaw), fz = -cosf(yaw), rx = cosf(yaw), rz = -sinf(yaw);
    for (int tries = 0; tries < 24; tries++) {
        float b = back + (tries / 6) * 0.6f, s = side * ((tries % 2) ? -1 : 1) * (1 + (tries % 6) * 0.3f);
        float x = p.x - fx * b + rx * s, z = p.z - fz * b + rz * s;
        int r = RoomAt(x, z);
        if (r < 0 || !Unlocked(r) || InsideCollider(x, z, ALLY_RADIUS + 0.1f)) continue;
        return Vector2{ x, z };
    }
    return Vector2{ p.x + 1.4f, p.z };
}

// Lo llama ResetGame después de construir el mapa y colocar al jugador
static void AddChatInteract();
void AllySpawn() {
    DialogClear();
    enabled = AllyEnabled();
    ally = Ally();
    allyView.visible = false;
    iconIdx = -1;
    if (!enabled) return;
    LoadAllyVoices();
    ally.active = true;
    ally.maxHealth = ally.health = 160 * StoryAllyHealth();   // en los últimos capítulos Milton está herido
    Vector2 s = FreeSpotNear(player.pos, 1.2f, 1.5f, player.yaw);
    ally.pos = Vector3{ s.x, 0, s.y };
    ally.moveYaw = ally.aimYaw = player.yaw + PI_F;
    ally.room = player.room;
    lastPos = ally.pos;
    avNext = T + 8;
    mmIcons.push_back({ ally.pos.x, ally.pos.z, Color{ 90, 230, 120, 255 }, 0.5f });
    iconIdx = (int)mmIcons.size() - 1;
    // levantarlo: mantener E cerca
    Interact it;
    it.pos = []() { return Vector2{ ally.pos.x, ally.pos.z }; };
    it.range = 2.0f;
    it.hold = true;
    it.active = []() { return ally.active && ally.downed; };
    it.text = []() { return ally.reviveK > 0 ? std::string(TextFormat("Levantando a MILTON... %d%%", (int)(ally.reviveK / ALLY_REVIVE_TIME * 100))) : std::string("Mantén E para levantar a MILTON"); };
    it.action = []() { ally.reviveK += allyDt; ally.reviveHeld = true; };
    interactables.push_back(it);
    AddChatInteract();
}

// ---------- Daño ----------
void AllyDamage(int n) {
    if (!ally.active || ally.downed || game.state != GS_PLAYING) return;
    ally.health -= n * 0.6f;   // aguanta un poco más que el jugador
    ally.lastHit = T;
    hitAnimT = 0.35f;
    if (ally.health <= 0) {
        ally.health = 0;
        ally.downed = true;
        ally.downT = ALLY_SELF_REVIVE;
        ally.reviveK = 0;
        ally.animT = 0;
        AllySay(AV_DOWN, 1, true);
        CenterMsg("¡MILTON CAYÓ!", Color{ 255, 90, 60, 255 }, 2.5f);
        StoryOnAllyDown();
    } else if (ally.health < ally.maxHealth * 0.4f) AllySay(AV_HURT, 0.5f);
}
static void Revive(bool self) {
    ally.downed = false;
    ally.health = ally.maxHealth * 0.6f;
    ally.reviveK = 0;
    ally.lastHit = T;
    AllySay(self ? AV_SELF : AV_REVIVED, 1, true);
    if (!self) AddPoints(50);
}

// El zombi va por Milton si está más cerca que el jugador
bool AllyZombieTarget(const Zombie &z, Vector3 &pos, int &room) {
    if (!ally.active || ally.downed) return false;
    float dl = player.downed ? 1e9f : hyp(player.pos.x - z.pos.x, player.pos.z - z.pos.z);
    float da = hyp(ally.pos.x - z.pos.x, ally.pos.z - z.pos.z);
    if (da >= dl) return false;
    pos = ally.pos; room = ally.room;
    return true;
}

// ---------- Línea de visión ----------
static bool LineClear(Vector3 a, Vector3 b) {
    Vector3 d = Vector3Subtract(b, a);
    float len = Vector3Length(d);
    if (len < 0.01f) return true;
    Ray r{ a, Vector3Scale(d, 1 / len) };
    for (auto &s : solids) {
        RayCollision c = GetRayCollisionBox(r, s);
        if (c.hit && c.distance < len - 0.3f) return false;
    }
    return true;
}

// ---------- Movimiento ----------
static float MoveTo(float tx, float tz, float speed, float dt) {
    float dx = tx - ally.pos.x, dz = tz - ally.pos.z, d = hyp(dx, dz);
    if (d < 0.05f) return 0;
    float step = std::min(d, speed * dt);
    ally.pos.x += dx / d * step; ally.pos.z += dz / d * step;
    ally.moveYaw = atan2f(dx, dz);
    return step;
}

static void Teleport() {
    Vector2 s = FreeSpotNear(player.pos, 2.0f, 1.3f, player.yaw);
    ally.pos = Vector3{ s.x, 0, s.y };
    ally.room = RoomAt(s.x, s.y);
    lastPos = ally.pos; stuckT = 0;
}

static int target = -1;
static float retargetT = 0;

void UpdateAlly(float dt) {
    if (!ally.active) { allyView.visible = false; return; }
    allyDt = dt;
    if (!ally.reviveHeld) ally.reviveK = 0;   // soltó la E (o se alejó)
    ally.reviveHeld = false;
    hitAnimT -= dt;
    int r = RoomAt(ally.pos.x, ally.pos.z);
    if (r >= 0) ally.room = r;
    if (iconIdx >= 0 && iconIdx < (int)mmIcons.size()) { mmIcons[iconIdx].x = ally.pos.x; mmIcons[iconIdx].z = ally.pos.z; mmIcons[iconIdx].c = ally.downed ? Color{ 255, 80, 60, 255 } : Color{ 90, 230, 120, 255 }; }

    // caído: espera que lo levanten
    if (ally.downed) {
        ally.animT += dt;
        ally.downT -= dt;
        if (ally.reviveK >= ALLY_REVIVE_TIME) Revive(false);
        else if (ally.downT <= 0) Revive(true);
        allyView = SoldierView{ true, ally.pos, ally.moveYaw, C_DIE, std::min(ally.animT, 1.4f), AllySkin() };
        return;
    }
    // se cura solo si no lo golpean un rato
    if (T - ally.lastHit > 5) ally.health = std::min(ally.maxHealth, ally.health + 12 * dt);

    float pdx = player.pos.x - ally.pos.x, pdz = player.pos.z - ally.pos.z, pd = hyp(pdx, pdz);
    // demasiado lejos o trabado: aparece detrás del jugador (sin que se vea)
    Vector3 camF = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    Vector3 toAlly = Vector3Normalize(Vector3Subtract(ally.pos, cam.position));
    bool seen = Vector3DotProduct(camF, toAlly) > 0.5f && pd < 30;
    if (pd > 32 || (stuckT > 3.5f && !seen) || stuckT > 8) { Teleport(); AllySay(AV_WAIT, 0.6f); return; }

    // elegir objetivo: el zombi visible más cercano
    retargetT -= dt;
    Vector3 eye{ ally.pos.x, 1.45f, ally.pos.z };
    auto valid = [&](int i) {
        if (i < 0 || i >= (int)zombies.size()) return false;
        const Zombie &z = zombies[i];
        if (z.state == Z_DEAD || z.state == Z_RISE || z.dummy >= 0 || z.puppet) return false;
        float d = hyp(z.pos.x - ally.pos.x, z.pos.z - ally.pos.z);
        return d < 24 && LineClear(eye, Vector3{ z.pos.x, z.y + 1.2f * z.scale, z.pos.z });
    };
    if (retargetT <= 0) {
        retargetT = 0.25f;
        target = -1; float bd = 1e9f;
        for (int i = 0; i < (int)zombies.size(); i++) {
            const Zombie &z = zombies[i];
            if (z.state == Z_DEAD || z.state == Z_RISE || z.dummy >= 0 || z.puppet) continue;
            float d = hyp(z.pos.x - ally.pos.x, z.pos.z - ally.pos.z);
            // prioridad: los que están encima del jugador
            float dp = hyp(z.pos.x - player.pos.x, z.pos.z - player.pos.z);
            float score = d - (dp < 3 ? 6 : 0);
            if (score < bd && valid(i)) { bd = score; target = i; }
        }
        // aviso: zombi detrás del jugador
        for (auto &z : zombies) {
            if (z.state != Z_CHASE || z.dummy >= 0) continue;
            float dx = z.pos.x - player.pos.x, dz = z.pos.z - player.pos.z, d = hyp(dx, dz);
            if (d < 2.6f && (dx * camF.x + dz * camF.z) / std::max(d, 0.01f) < -0.4f) { AllySay(AV_WARN, 0.15f); break; }
        }
    } else if (!valid(target)) target = -1;

    // moverse: seguir al jugador (por las puertas si está en otra sala)
    float speed = pd > 7 ? 6.6f : 4.6f;
    bool shooting = target >= 0 && pd < 10;
    float moved = 0;
    int pr = player.room >= 0 ? player.room : RoomAt(player.pos.x, player.pos.z);
    int nr = -1, nl = -1;
    bool path = ally.room != pr && pr >= 0 && NextRoom(ally.room, pr, nr, nl);
    if (path) {
        const Link &L = links[nl];
        bool curIsA = RoomIndex(L.a) == ally.room;
        Vector2 sc = curIsA ? L.sideA : L.sideB, sn = curIsA ? L.sideB : L.sideA;
        float lateral = L.axis == 'z' ? fabsf(ally.pos.z - L.z) : fabsf(ally.pos.x - L.x);
        float depth = L.axis == 'z' ? fabsf(ally.pos.x - L.x) : fabsf(ally.pos.z - L.z);
        Vector2 t = (lateral > 0.8f && depth > 0.4f) ? sc : sn;
        moved = MoveTo(t.x, t.y, 6.0f, dt);
    } else if (pd > 3.8f || (!shooting && pd > 2.8f)) {
        // un punto detrás y al costado del jugador
        float fx = -sinf(player.yaw), fz = -cosf(player.yaw), rx = cosf(player.yaw), rz = -sinf(player.yaw);
        float side = 1.4f;   // a la derecha del jugador
        float tx = player.pos.x - fx * 2.2f + rx * side, tz = player.pos.z - fz * 2.2f + rz * side;
        if (InsideCollider(tx, tz, ALLY_RADIUS)) { tx = player.pos.x; tz = player.pos.z; }
        moved = MoveTo(tx, tz, speed, dt);
    }
    // no quedarse delante del arma del jugador
    {
        float fx = -sinf(player.yaw), fz = -cosf(player.yaw);
        float along = -pdx * fx + -pdz * fz;   // distancia delante del jugador
        float across = -pdx * cosf(player.yaw) + pdz * sinf(player.yaw);
        if (along > 0.5f && along < 9 && fabsf(across) < 1.0f) {
            float s = across >= 0 ? 1.0f : -1.0f;
            ally.pos.x += cosf(player.yaw) * s * 3.0f * dt;
            ally.pos.z += -sinf(player.yaw) * s * 3.0f * dt;
        }
    }
    // no atravesar al jugador
    if (pd < 1.1f && pd > 0.001f) { ally.pos.x -= pdx / pd * (1.1f - pd); ally.pos.z -= pdz / pd * (1.1f - pd); }
    Collide(ally.pos, ALLY_RADIUS);
    ally.pos.y = 0;

    // ¿se trabó?
    moveT += dt;
    if (moveT > 1.0f) {
        float m = hyp(ally.pos.x - lastPos.x, ally.pos.z - lastPos.z);
        bool wants = path || pd > 4.5f;
        stuckT = (wants && m < 0.4f) ? stuckT + moveT : 0;
        lastPos = ally.pos; moveT = 0;
    }

    // disparar
    ally.fireT -= dt;
    if (ally.reloadT > 0) {
        ally.reloadT -= dt;
        if (ally.reloadT <= 0) ally.mag = 30;
    }
    bool firing = false;
    if (target >= 0) {
        Zombie &z = zombies[target];
        float dx = z.pos.x - ally.pos.x, dz = z.pos.z - ally.pos.z;
        float want = atan2f(dx, dz);
        float diff = atan2f(sinf(want - ally.aimYaw), cosf(want - ally.aimYaw));
        ally.aimYaw += diff * dampk(10, dt);
        if (fabsf(diff) < 0.25f && ally.reloadT <= 0 && ally.fireT <= 0 && (moved < 0.02f || shooting)) {
            if (ally.mag <= 0) { ally.reloadT = 2.4f; AllySay(AV_RELOAD, 0.6f); }
            else {
                ally.mag--;
                ally.fireT = 0.3f;
                firing = true;
                float d = hyp(dx, dz);
                Vector3 muzzle{ ally.pos.x + sinf(ally.aimYaw) * 0.6f, 1.38f, ally.pos.z + cosf(ally.aimYaw) * 0.6f };
                bool head = frand(0, 1) < 0.15f;
                Vector3 aimP{ z.pos.x, z.y + (head ? 1.62f : 1.15f) * z.scale, z.pos.z };
                bool hit = frand(0, 1) < clampf(0.92f - d * 0.015f, 0.55f, 0.92f);
                if (!hit) aimP = Vector3Add(aimP, Vector3{ frand(-0.6f, 0.6f), frand(-0.3f, 0.5f), frand(-0.6f, 0.6f) });
                AllyTracer(muzzle, aimP);
                PlayGunAt(W_RIFLE, ally.pos);
                Burst(muzzle, Color{ 255, 200, 120, 255 }, 3, 1.2f, 0.04f, 6);
                if (hit) {
                    bool killed = AllyShootZombie(z, game.zHP * 0.16f, head, aimP, eye);
                    if (killed) { target = -1; ally.kills++; AllySay(AV_KILL, 0.12f); }
                }
            }
        }
    } else if (moved > 0.01f) ally.aimYaw = ally.moveYaw;
    else if (ally.mag < 12 && ally.reloadT <= 0) { ally.reloadT = 2.4f; ally.mag = 0; }   // recarga en la calma

    // animación
    bool moving = moved > 0.01f;
    ally.animT += dt * (moving ? clampf(moved / dt / 4.6f, 0.6f, 1.4f) : 1);
    int clip = moving ? C_WALK : (target >= 0 || firing) ? C_ATTACK : C_IDLE;
    if (hitAnimT > 0 && !moving) clip = C_HIT;
    float yaw = moving && target < 0 ? ally.moveYaw : ally.aimYaw;
    float talk = AllyTalk();
    if (talk > 0.05f && clip == C_IDLE) clip = C_TALK;   // asiente mientras habla
    allyView = SoldierView{ true, ally.pos, yaw, clip, ally.animT, AllySkin(), talk };
}

// ---------- Interfaz ----------
void DrawAllyHUD() {
    if (!ally.active) return;
    int W = GetScreenWidth(), Hh = GetScreenHeight();
    float s = Hh / 900.0f;
    (void)W;
    // barra de vida de Milton encima de la de Frank
    float hx = 40 * s, hy = Hh - 100 * s;
    Color nc = ally.downed ? Color{ 255, 90, 60, 255 } : Color{ 150, 230, 120, 230 };
    DrawTextEx(fontUI, ally.downed ? TextFormat("CABO MILTON · CAÍDO %d s", (int)ceilf(std::max(0.0f, ally.downT))) : "CABO MILTON", Vector2{ hx, hy - 20 * s }, 16 * s, 1, nc);
    DrawRectangle((int)hx, (int)hy, (int)(160 * s), (int)(7 * s), Color{ 0, 0, 0, 140 });
    DrawRectangle((int)hx, (int)hy, (int)(160 * s * clampf(ally.health / ally.maxHealth, 0, 1)), (int)(7 * s), Color{ 80, 200, 100, 255 });
    DrawRectangleLines((int)hx, (int)hy, (int)(160 * s), (int)(7 * s), Color{ 255, 255, 255, 70 });
    // marcador sobre su cabeza
    Vector3 head{ ally.pos.x, ally.downed ? 0.9f : 2.15f, ally.pos.z };
    Vector3 f = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    if (Vector3DotProduct(f, Vector3Subtract(head, cam.position)) > 0.3f) {
        Vector2 sp = GetWorldToScreen(head, cam);
        float d = Vector3Distance(head, cam.position);
        if (d > 3 || ally.downed) {
            const char *t = ally.downed ? "MILTON · mantén E" : "MILTON";
            Vector2 m = MeasureTextEx(fontUI, t, 16 * s, 1);
            DrawTextEx(fontUI, t, Vector2{ sp.x - m.x / 2 + 1, sp.y - m.y + 1 }, 16 * s, 1, Color{ 0, 0, 0, 180 });
            DrawTextEx(fontUI, t, Vector2{ sp.x - m.x / 2, sp.y - m.y }, 16 * s, 1, nc);
        }
    }
    // subtítulo de lo que dice
    float a = 0;
    if (const char *sub = AllySubtitle(a)) {
        unsigned char al = (unsigned char)(a * 255);
        float sy = Hh * 0.8f, fs = 26 * s;
        float tw = MeasureTextEx(fontUI, sub, fs, 1).x;
        float lx = W / 2.0f - tw / 2 - 10 * s;
        const char *who = "MILTON:";
        Vector2 wm = MeasureTextEx(fontUI, who, 20 * s, 1);
        DrawTextEx(fontUI, who, Vector2{ lx - wm.x + 2, sy + 5 * s }, 20 * s, 1, Color{ 0, 0, 0, al });
        DrawTextEx(fontUI, who, Vector2{ lx - wm.x, sy + 3 * s }, 20 * s, 1, Color{ 120, 230, 230, al });
        DrawTextEx(fontUI, sub, Vector2{ W / 2.0f - tw / 2 + 2, sy + 2 }, fs, 1, Color{ 0, 0, 0, al });
        DrawTextEx(fontUI, sub, Vector2{ W / 2.0f - tw / 2, sy }, fs, 1, Color{ 245, 245, 245, al });
    }
}

int AllySkin() { return EggRosa() ? SKIN_ROSA : SKIN_MILTON; }
// Cuánto está hablando Milton ahora (0..1): frases de combate, del guion o de la charla
float AllyTalk() {
    float lv = StoryMiltonTalk();
    if (AllySpeaking()) lv = std::max(lv, VoiceEnvelope(avFile.c_str(), T - avStart));
    lv = std::max(lv, AllyChatTalk());
    return lv;
}

// ---------- Diálogo fuera del guion: charlar con Milton y los huevos de pascua ----------
// Frases de guion.tsv que se encadenan, sin pisarse con la radio de la historia ni con las frases de combate.
#include <deque>
static std::deque<std::string> dq;
static std::string dCur;
static Sound *dSnd = nullptr;
static float dStart = 0, dEnd = 0, dNext = 0;
void DialogSay(std::vector<const char *> ids) { for (auto id : ids) dq.push_back(id); }
bool DialogTalking() { return !dCur.empty() || !dq.empty(); }
void DialogClear() { if (dSnd) StopSound(*dSnd); dSnd = nullptr; dCur.clear(); dq.clear(); dNext = 0; }
void UpdateDialog() {
    if (!dCur.empty() && T >= dEnd) { dCur.clear(); dSnd = nullptr; dNext = T + 0.3f; }
    if (dCur.empty() && !dq.empty() && T >= dNext && !StoryTalking() && !FrankSpeaking() && !(avPlaying && IsSoundPlaying(*avPlaying))) {
        dCur = dq.front(); dq.pop_front();
        dSnd = StoryLineSound(dCur.c_str());
        int who; const char *txt = StoryLineText(dCur.c_str(), who);
        float len = 1.2f + strlen(txt) * 0.055f;
        if (dSnd) { SetSoundVolume(*dSnd, clampf(opts.sfx * 1.1f, 0, 1)); PlaySound(*dSnd); len = (float)dSnd->frameCount / dSnd->stream.sampleRate; }
        dStart = T; dEnd = T + len;
    }
}
float AllyChatTalk() {
    if (dCur.empty()) return 0;
    int who; StoryLineText(dCur.c_str(), who);
    if (strcmp(StorySpeakerName(who), "MILTON")) return 0;
    return VoiceEnvelope(StoryLineFile(dCur.c_str()), T - dStart);
}
void DrawDialogHUD() {
    if (dCur.empty()) return;
    int W = GetScreenWidth(), Hh = GetScreenHeight();
    float s = Hh / 900.0f;
    int who; const char *txt = StoryLineText(dCur.c_str(), who);
    float k = T - dStart, len = dEnd - dStart;
    float a = clampf(k / 0.15f, 0, 1) * clampf((len - k) / 0.2f + 0.5f, 0, 1);
    float fs = 26 * s, y = Hh * 0.74f;
    float tw = MeasureTextEx(fontUI, txt, fs, 1).x;
    if (tw > W * 0.8f) { fs *= W * 0.8f / tw; tw = W * 0.8f; }
    Color wc = StorySpeakerColor(who); wc.a = (unsigned char)(a * 255);
    const char *name = StorySpeakerName(who);
    Vector2 nm = MeasureTextEx(fontUI, name, 18 * s, 1);
    DrawTextEx(fontUI, name, Vector2{ W / 2.0f - nm.x / 2 + 2, y - 24 * s + 2 }, 18 * s, 1, Color{ 0, 0, 0, wc.a });
    DrawTextEx(fontUI, name, Vector2{ W / 2.0f - nm.x / 2, y - 24 * s }, 18 * s, 1, wc);
    DrawTextEx(fontUI, txt, Vector2{ W / 2.0f - tw / 2 + 2, y + 2 }, fs, 1, Color{ 0, 0, 0, (unsigned char)(a * 255) });
    DrawTextEx(fontUI, txt, Vector2{ W / 2.0f - tw / 2, y }, fs, 1, Color{ 245, 245, 245, (unsigned char)(a * 255) });
}
// Charlas con Milton (E cerca de él): algunas dan pistas de los secretos
static const char *JOKES[][2] = { { "j1m", "j1f" }, { "j2m", "j2f" }, { "j3m", "j3f" }, { "j4m", "j4f" }, { "j5m", "j5f" }, { "j6m", "j6f" },
                                  { "j7m", "j7f" }, { "j8m", "j8f" }, { "j9m", "j9f" }, { "j10m", "j10f" }, { "j11m", "j11f" }, { "j12m", "j12f" } };
static int jokeNext = -1;
static void AddChatInteract() {
    Interact it;
    it.pos = []() { return Vector2{ ally.pos.x, ally.pos.z }; };
    it.range = 1.9f;
    it.active = []() { return ally.active && !ally.downed && !DialogTalking() && !StoryTalking() && !AllySpeaking(); };
    it.text = []() { return std::string("Presiona E para hablar con MILTON"); };
    it.action = []() {
        int n = (int)(sizeof(JOKES) / sizeof(JOKES[0]));
        if (jokeNext < 0) jokeNext = irand(n);
        DialogSay({ JOKES[jokeNext][0], JOKES[jokeNext][1] });
        jokeNext = (jokeNext + 1 + irand(2)) % n;
    };
    interactables.push_back(it);
}
