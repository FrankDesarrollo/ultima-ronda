// Co-op en línea para dos: Frank crea la partida (anfitrión) y Milton se une con la IP pública.
// El anfitrión manda: zombis, rondas, ventanas, puertas, corriente y power-ups. Cada uno mueve
// a su soldado en su PC (sin retraso) y manda su estado 30 veces por segundo; los disparos del
// invitado se mandan al anfitrión, que decide las bajas y avisa a quién le tocan los puntos.
#include "game.h"
#include "netlow.h"

static const int PORT = 27015;
static const unsigned char VERSION = 1;
enum : unsigned char { M_HELLO = 1, M_START, M_STATE, M_SNAP, M_DOOR, M_POWER, M_REPAIR, M_HIT, M_KILL, M_DAMAGE, M_DROP, M_PICK, M_REVIVE, M_GAMEOVER, M_BYE };

#pragma pack(push, 1)
struct MState { unsigned char t; float x, y, z, yaw, pitch, speed, health; unsigned char weapon, flags, room; int points, shots; };   // flags: 1 agachado, 2 caído
struct MHit { unsigned char t; int id; float dmg; unsigned char head; float px, py, pz, dx, dy, dz; };
struct MKill { unsigned char t; int id; unsigned char flags; float dx, dy, dz, force; };                                   // flags: 1 cabeza, 2 del invitado, 4 bomba
struct MDrop { unsigned char t; int id; unsigned char type; float x, z; };
struct MPick { unsigned char t; int id; signed char type; };
#pragma pack(pop)

static int role = NET_OFF;
static bool started = false;          // ya empezó la partida (los dos en el mapa)
static std::string startMap;          // invitado: mapa que mandó el anfitrión, para empezar en el bucle principal
static bool startPending = false;
RemotePlayer remote;
static Vector3 remoteTarget{};
static int remoteShots = 0;
static float sendT = 0, snapT = 0;
static Texture2D nameTex{}, downTex{};
// sala de espera
static int lobby = 0;                 // LB_*
static std::string joinIP, status;
static unsigned char lastRound = 0; static bool lastBetween = true;

int NetRole() { return role; }
bool NetActive() { return role != NET_OFF && started && NL_Connected(); }
const char *LocalName() { return role == NET_CLIENT ? "MILTON" : "FRANK"; }
const char *RemoteName() { return role == NET_CLIENT ? "FRANK" : "MILTON"; }

static void Send(const void *p, size_t n, bool reliable = true) { NL_Send(p, n, reliable); }
static void SendByte(unsigned char t) { Send(&t, 1); }

// ---------- Sala de espera (la dibuja hud.cpp) ----------
int NetLobbyScreen() { return lobby; }
std::string &NetJoinIP() { return joinIP; }
const std::string &NetStatus() { return status; }
std::string NetPublicIP() { return NL_PublicIP(); }
std::string NetLocalIP() { return NL_LocalIP(); }
int NetPing() { return NL_Ping(); }

static void LoadLastIP() {
    if (!joinIP.empty()) return;
    if (char *t = LoadFileText("ultima_ip.txt")) { joinIP = t; UnloadFileText(t); }
    while (!joinIP.empty() && (joinIP.back() == '\n' || joinIP.back() == '\r' || joinIP.back() == ' ')) joinIP.pop_back();
}

void NetShutdown() {
    if (NL_Connected()) { SendByte(M_BYE); NL_Flush(); }
    NL_Close();
    role = NET_OFF; started = false; startPending = false;
    remote = RemotePlayer(); remoteView.visible = false;
    lobby = LB_NONE; status.clear();
}

// Clics de la sala de espera (ids de botón en hud.cpp)
void NetLobbyClick(int click) {
    if (click == 8) { lobby = LB_CHOICE; status.clear(); LoadLastIP(); return; }
    if (lobby == LB_NONE) return;
    if (click == 26) {   // volver / cancelar
        if (lobby == LB_HOSTING || lobby == LB_CONNECTING) NetShutdown();
        lobby = lobby == LB_CHOICE ? LB_NONE : LB_CHOICE;
        status.clear();
        return;
    }
    if (lobby == LB_CHOICE && click == 20) { lobby = LB_HOST_PICK; return; }
    if (lobby == LB_CHOICE && click == 21) { lobby = LB_JOIN; return; }
    if (lobby == LB_HOST_PICK && click >= 22 && click <= 24) {
        const char *maps[3] = { "bunker", "fabrica", "pueblo" };
        startMap = maps[click - 22];
        if (!NL_Host(PORT)) { status = TextFormat("No se pudo abrir el puerto %d (¿otro programa lo usa?)", PORT); return; }
        role = NET_HOST; started = false;
        lobby = LB_HOSTING;
        status = "Esperando a MILTON...";
        NL_FetchPublicIP();
        return;
    }
    if (lobby == LB_JOIN && click == 25) {
        if (joinIP.empty()) { status = "Escribe la IP de Frank"; return; }
        SaveFileText("ultima_ip.txt", (char *)joinIP.c_str());
        std::string ip = joinIP; int port = PORT;
        size_t c = ip.find(':');
        if (c != std::string::npos) { port = atoi(ip.c_str() + c + 1); ip = ip.substr(0, c); }
        if (!NL_Connect(ip.c_str(), port)) { status = "IP no válida"; return; }
        role = NET_CLIENT; started = false;
        lobby = LB_CONNECTING;
        status = TextFormat("Conectando con %s...", joinIP.c_str());
    }
}

// El bucle principal pregunta si hay que empezar (ResetGame vive en main.cpp)
bool NetTakeStart(std::string &map) {
    if (!startPending) return false;
    startPending = false;
    map = startMap;
    return true;
}

// Anfitrión: "jugar de nuevo" en co-op reinicia a los dos
void NetRestart(const std::string &map) {
    if (role != NET_HOST || !NL_Connected()) return;
    startMap = map;
    unsigned char buf[24] = { M_START };
    strncpy((char *)buf + 1, map.c_str(), 22);
    Send(buf, sizeof(buf));
    startPending = true;
}

static void OnStart() {
    started = true;
    lobby = LB_NONE;
    remote = RemotePlayer();
    remote.present = true;
    lastRound = 0; lastBetween = true;
    if (!nameTex.id) {
        nameTex = MakeLabel(RemoteName(), Color{ 150, 230, 120, 255 }, 54);
        downTex = MakeLabel(TextFormat("%s · CAÍDO", RemoteName()), Color{ 255, 70, 60, 255 }, 54);
    }
}

// ---------- Avisos del juego ----------
void NetOnDoorOpened(int link) { if (NetActive()) { unsigned char b[2] = { M_DOOR, (unsigned char)link }; Send(b, 2); } }
void NetOnPower() { if (NetActive()) SendByte(M_POWER); }
void NetOnRepair(int w) { if (NetActive() && role == NET_CLIENT) { unsigned char b[2] = { M_REPAIR, (unsigned char)w }; Send(b, 2); } }
void NetOnKill(const Zombie &z, bool head, bool byRemote, bool nuke) {
    if (!NetActive()) return;
    MKill m{ M_KILL, z.id, (unsigned char)((head ? 1 : 0) | (byRemote ? 2 : 0) | (nuke ? 4 : 0)), z.hitDir.x, z.hitDir.y, z.hitDir.z, z.hitForce };
    Send(&m, sizeof(m));
}
void NetSendHit(const Zombie &z, float dmg, bool head, Vector3 point) {
    if (!NetActive()) return;
    MHit m{ M_HIT, z.id, dmg, (unsigned char)head, point.x, point.y, point.z, z.hitDir.x, z.hitDir.y, z.hitDir.z };
    Send(&m, sizeof(m));
}
void NetOnDrop(int id, int type, float x, float z) { if (NetActive()) { MDrop m{ M_DROP, id, (unsigned char)type, x, z }; Send(&m, sizeof(m)); } }
void NetOnPickup(int id, int type) { if (NetActive()) { MPick m{ M_PICK, id, (signed char)type }; Send(&m, sizeof(m)); } }
void NetOnGameOver() { if (NetActive()) { SendByte(M_GAMEOVER); NL_Flush(); } }
void NetDamageRemote(int n) { if (NetActive()) { unsigned char b[3] = { M_DAMAGE, (unsigned char)(n & 255), (unsigned char)(n >> 8) }; Send(b, 3); } }
void NetSendRevive() { if (NetActive()) { SendByte(M_REVIVE); remote.downed = false; } }
void NetRevived() {
    player.downed = false; player.downT = 0;
    player.health = player.maxHealth * 0.5f;
    player.eye = 1.65f;
    CenterMsg(TextFormat("¡%s TE LEVANTÓ!", RemoteName()), Color{ 124, 196, 255, 255 }, 2.5f);
    Play("perk");
}

// Anfitrión: ¿este zombi va a por el compañero? (el más cercano de los que siguen en pie)
bool NetZombieTarget(const Zombie &z, Vector3 &pos, int &room) {
    if (!NetActive() || !remote.present || remote.downed) return false;
    float dl = player.downed ? 1e9f : hyp(player.pos.x - z.pos.x, player.pos.z - z.pos.z);
    float dr = hyp(remote.pos.x - z.pos.x, remote.pos.z - z.pos.z);
    if (dr >= dl) return false;
    pos = remote.pos; room = remote.room;
    return true;
}

// ---------- Envío ----------
static void SendState() {
    MState m{};
    m.t = M_STATE;
    m.x = player.pos.x; m.y = player.pos.y; m.z = player.pos.z; m.yaw = player.yaw; m.pitch = player.pitch;
    m.speed = hyp(player.vel.x, player.vel.z); m.health = player.health;
    m.weapon = (unsigned char)CurWeapon().id;
    m.flags = (player.crouch ? 1 : 0) | (player.downed ? 2 : 0);
    m.room = (unsigned char)std::max(0, player.room);
    m.points = player.points; m.shots = player.shots;
    Send(&m, sizeof(m), false);
}
static void SendSnapshot() {
    static NetZ zs[40];
    int nz = NetCollectZombies(zs, 40);
    std::vector<unsigned char> b;
    b.push_back(M_SNAP);
    b.push_back((unsigned char)std::min(game.round, 255));
    b.push_back((unsigned char)((game.between ? 1 : 0) | (game.dogRound ? 2 : 0) | (powerOn ? 4 : 0)));
    unsigned int doorMask = 0;
    for (size_t i = 0; i < links.size() && i < 32; i++) if (links[i].open) doorMask |= 1u << i;
    b.insert(b.end(), (unsigned char *)&doorMask, (unsigned char *)&doorMask + 4);
    b.push_back((unsigned char)windows.size());
    for (auto &w : windows) b.push_back((unsigned char)w.boards);
    b.push_back((unsigned char)nz);
    b.insert(b.end(), (unsigned char *)zs, (unsigned char *)zs + nz * sizeof(NetZ));
    Send(b.data(), b.size(), false);
}

// ---------- Recepción ----------
static void ApplySnapshot(const unsigned char *d, size_t n) {
    if (n < 9 || !started) return;
    size_t o = 1;
    int round = d[o++]; unsigned char fl = d[o++];
    unsigned int doorMask; memcpy(&doorMask, d + o, 4); o += 4;
    int nw = d[o++];
    if (o + nw + 1 > n) return;
    for (int i = 0; i < nw && i < (int)windows.size(); i++) SetWindowBoards(i, d[o + i]);
    o += nw;
    int nz = d[o++];
    if (o + nz * sizeof(NetZ) > n) return;
    NetApplyZombies((const NetZ *)(d + o), nz);
    for (size_t i = 0; i < links.size() && i < 32; i++) if ((doorMask >> i) & 1) OpenLinkNow((int)i);
    if ((fl & 4) && !powerOn) PowerOnNow();
    // rondas: el invitado no genera zombis, sigue al anfitrión
    bool between = fl & 1, dogs = (fl & 2) != 0;
    if (round > lastRound) {   // empezó una ronda nueva
        game.round = round; game.zHP = ZombieHP(round); game.dogRound = dogs;
        if (round > 1) player.grenades = std::min(4, player.grenades + 2);
        RoundFlash();
        if (dogs) { Play("dogstart"); CenterMsg("¡RONDA DE PERROS!", Color{ 255, 120, 40, 255 }, 3.5f); Say(V_DOGS); }
        else { Play("round"); if (round > 1) Say(V_ROUND, 0.7f); }
    }
    if (between && !lastBetween && round > 0) { CenterMsg("RONDA SUPERADA", Color{ 255, 216, 74, 255 }); Play("roundEnd"); }
    game.round = round; game.between = between; game.dogRound = dogs;
    lastRound = (unsigned char)round; lastBetween = between;
}

static void OnMessage(const std::vector<unsigned char> &m) {
    if (m.empty()) return;
    const unsigned char *d = m.data();
    switch (d[0]) {
    case M_HELLO:
        if (role == NET_HOST && !started) {
            if (m.size() < 2 || d[1] != VERSION) { status = "MILTON tiene otra versión del juego"; break; }
            NetRestart(startMap);
            OnStart();
        }
        break;
    case M_START:
        if (role == NET_CLIENT && m.size() >= 2) {
            startMap = std::string((const char *)d + 1, strnlen((const char *)d + 1, m.size() - 1));
            startPending = true;
            OnStart();
        }
        break;
    case M_STATE:
        if (m.size() >= sizeof(MState) && started) {
            MState s; memcpy(&s, d, sizeof(s));
            remoteTarget = Vector3{ s.x, s.y, s.z };
            if (!remote.present || Vector3Distance(remote.pos, remoteTarget) > 6) remote.pos = remoteTarget;   // salto grande: teletransporte
            remote.present = true;
            remote.yaw = s.yaw; remote.pitch = s.pitch; remote.speed = s.speed; remote.health = s.health;
            remote.weapon = s.weapon; remote.crouch = s.flags & 1; remote.points = s.points; remote.room = s.room;
            bool wasDown = remote.downed;
            remote.downed = (s.flags & 2) != 0;
            if (remote.downed && !wasDown) { CenterMsg(TextFormat("¡%s CAYÓ!", RemoteName()), Color{ 255, 90, 60, 255 }, 3); SpecialMsg(TextFormat("Corre a levantarlo: acércate y mantén E")); remote.animT = 0; }
            if (s.shots > remoteShots && remoteShots > 0) PlayGunAt(s.weapon, remote.pos);
            remoteShots = s.shots;
        }
        break;
    case M_SNAP: if (role == NET_CLIENT) ApplySnapshot(d, m.size()); break;
    case M_DOOR: if (m.size() >= 2) OpenLinkNow(d[1]); break;
    case M_POWER: PowerOnNow(); break;
    case M_REPAIR: if (role == NET_HOST && m.size() >= 2) RepairBoardNow(d[1]); break;
    case M_HIT:
        if (role == NET_HOST && m.size() >= sizeof(MHit)) {
            MHit h; memcpy(&h, d, sizeof(h));
            NetRemoteHit(h.id, h.dmg, h.head, Vector3{ h.px, h.py, h.pz }, Vector3{ h.dx, h.dy, h.dz });
        }
        break;
    case M_KILL:
        if (role == NET_CLIENT && m.size() >= sizeof(MKill)) {
            MKill k; memcpy(&k, d, sizeof(k));
            NetRemoteKill(k.id, k.flags & 1, (k.flags & 2) != 0, (k.flags & 4) != 0, Vector3{ k.dx, k.dy, k.dz }, k.force);
        }
        break;
    case M_DAMAGE: if (m.size() >= 3) DamagePlayer(d[1] | (d[2] << 8)); break;
    case M_DROP: if (role == NET_CLIENT && m.size() >= sizeof(MDrop)) { MDrop p; memcpy(&p, d, sizeof(p)); NetAddPowerup(p.id, p.type, p.x, p.z); } break;
    case M_PICK:
        if (role == NET_CLIENT && m.size() >= sizeof(MPick)) {
            MPick p; memcpy(&p, d, sizeof(p));
            NetRemovePowerup(p.id);
            if (p.type >= 0) ApplyPowerup(p.type);
        }
        break;
    case M_REVIVE: if (player.downed) NetRevived(); break;
    case M_GAMEOVER: ForceGameOver(); break;
    case M_BYE: break;
    }
}

void NetUpdate(float dt) {
    // escribir la IP en la sala de espera
    if (lobby == LB_JOIN) {
        int c;
        while ((c = GetCharPressed()) > 0)
            if (joinIP.size() < 40 && ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == ':' || c == '-')) joinIP += (char)c;
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !joinIP.empty()) joinIP.pop_back();
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_V))
            if (const char *cb = GetClipboardText()) for (const char *p = cb; *p && joinIP.size() < 40; p++) if (*p != ' ' && *p != '\n' && *p != '\r') joinIP += *p;
        if (IsKeyPressed(KEY_ENTER)) NetLobbyClick(25);
    }
    if (role == NET_OFF) return;

    NLEvent e;
    int ev;
    while ((ev = NL_Poll(e)) != NL_NONE) {
        if (ev == NL_CONNECT) {
            if (role == NET_CLIENT) { unsigned char b[2] = { M_HELLO, VERSION }; Send(b, 2); status = "Conectado. Esperando a que empiece..."; }
            else status = "¡MILTON se conectó!";
        } else if (ev == NL_DISCONNECT) {
            bool wasPlaying = started;
            remote.present = false; remoteView.visible = false;
            started = false;
            if (role == NET_HOST) {
                if (wasPlaying) { CenterMsg("MILTON SE DESCONECTÓ", Color{ 255, 120, 60, 255 }, 3); SpecialMsg("Sigues solo"); }
                status = "Esperando a MILTON...";
                if (!wasPlaying && lobby == LB_HOSTING) {}   // sigue esperando
                else { NL_Close(); role = NET_OFF; }
            } else {
                if (wasPlaying) { ForceGameOver(); CenterMsg("SE PERDIÓ LA CONEXIÓN CON FRANK", Color{ 255, 120, 60, 255 }, 4); }
                else status = "No se pudo conectar. Revisa la IP y que Frank haya abierto el puerto UDP 27015.";
                NL_Close(); role = NET_OFF;
                if (!wasPlaying) lobby = LB_JOIN;
            }
        } else if (ev == NL_DATA) OnMessage(e.data);
    }
    if (role == NET_OFF || !started) return;

    // el compañero: suavizar su movimiento y animarlo
    if (remote.present) {
        remote.pos = Vector3Lerp(remote.pos, remoteTarget, dampk(14, dt));
        remote.animT += dt;
        bool show = game.state == GS_PLAYING || game.state == GS_PAUSED || game.state == GS_OVER;
        int clip = remote.downed ? C_DIE : remote.speed > 0.6f ? C_WALK : C_ATTACK;
        static float runT = 0; runT += dt * clampf(remote.speed / 4.5f, 0.6f, 1.6f);
        remoteView = SoldierView{ show, remote.pos, remote.yaw + PI_F, clip, clip == C_WALK ? runT : remote.animT };
    }
    // el invitado sigue la niebla de las rondas de perros
    if (role == NET_CLIENT) { game.fogK = lerpf(game.fogK, game.dogRound ? 1.0f : 0.0f, dampk(0.8f, dt)); DogFog(game.fogK); }

    sendT -= dt; snapT -= dt;
    if (sendT <= 0) { sendT = 1.0f / 30; SendState(); }
    if (role == NET_HOST && snapT <= 0) { snapT = 1.0f / 20; SendSnapshot(); }
    NL_Flush();
}

// Nombre sobre la cabeza del compañero
void NetDrawLabels() {
    if (!remoteView.visible || !nameTex.id) return;
    void DrawUnlitBegin(float intensity);
    void DrawUnlitEnd();
    DrawUnlitBegin(1.4f);
    DrawWorldLabel(remote.downed ? downTex : nameTex, Vector3{ remote.pos.x, remote.pos.y + (remote.downed ? 0.9f : 2.15f), remote.pos.z }, remote.downed ? 1.6f : 0.9f);
    DrawUnlitEnd();
}
bool NetConnected() { return NL_Connected(); }
