// =============================================================
//  ÚLTIMA RONDA — versión C++ (raylib)
//  Cabecera común: tipos, constantes y estado global del juego.
// =============================================================
#pragma once
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <vector>
#include <string>
#include <functional>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <memory>

// ---------- Configuración (personaliza aquí) ----------
namespace CONFIG {
    inline const char *titulo = "ÚLTIMA RONDA";         // nombre del juego
    inline const char *marca = "madebyfrank";           // firma del autor
    inline const char *mensaje5 = "¡Ronda 5! Esto se pone serio";
    inline const char *mensaje10 = "¡Ronda 10! Eres una leyenda";
    inline const char *mensaje15 = "Ronda 15: ya nadie te para";
    inline int puntosIniciales = 500;
    inline float sensibilidad = 0.0022f;
}

constexpr float H = 4.0f;            // altura de las paredes
constexpr float PI_F = 3.14159265f;

// ---------- Utilidades ----------
inline float frand(float a, float b) { return a + (b - a) * (rand() / (float)RAND_MAX); }
inline int irand(int n) { return n > 0 ? rand() % n : 0; }
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float dampk(float k, float dt) { return 1.0f - expf(-k * dt); }
inline float hyp(float x, float z) { return sqrtf(x * x + z * z); }
inline Color Hex(unsigned int c, unsigned char a = 255) { return Color{ (unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c, a }; }
inline Vector3 ColorV(unsigned int c, float k = 1) { return Vector3{ ((c >> 16) & 255) / 255.f * k, ((c >> 8) & 255) / 255.f * k, (c & 255) / 255.f * k }; }

// ---------- Render ----------
// Material PBR: textura de color, de relieve (normal) y de rugosidad
struct PMat {
    Material m{};
    float metal = 0, rough = 1, nstr = 1;
    Vector3 emis{ 0, 0, 0 };
    float uvScroll = 0;
};
struct MeshBuilder {
    std::vector<float> v, n, t, tg;
    std::vector<unsigned short> idx;
    // uvMode 0: coordenadas del mundo / uvScale; 1: cada cara de 0 a 1
    void Box(Vector3 mn, Vector3 mx, float uvScale, int uvMode = 0, unsigned faces = 63);
    void Cylinder(float r, float h, int seg, float uvScale);
    void Quad(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, Vector3 nrm, Vector2 uv0, Vector2 uv1, Vector2 uv2, Vector2 uv3, Vector3 tan, float w);
    int Verts() const { return (int)v.size() / 3; }
    Mesh Build();
};
struct Light { Vector3 pos; Vector3 color; float range; bool flicker = false; float k = 1; bool caster = false; };

void InitRender();
void ResizeRender();
void LoadMaterials();
PMat &M(const char *name);
void DrawPM(const Mesh &mesh, PMat &pm, Matrix tf);
void RenderFrame(bool menu);
void DrawWorldLabel(Texture2D tex, Vector3 pos, float width);
Texture2D MakeLabel(const char *text, Color color, int fontSize = 54);
extern Font fontUI, fontTitle;
extern Shader shLit, shLitSkin, shDepth, shDepthSkin, shUnlit;
extern bool depthPass;
extern Texture2D texWhite, texFlat, texFlash, texBlood, texHole, texScorch, texSmoke, texCamo;
extern Camera3D cam;
extern float camFov;
void SetSkinned(bool on);

// ---------- Mundo ----------
struct Box2 { float minX, maxX, minZ, maxZ; float top = 1e9f; };   // top: altura a la que se puede subir (1e9 = pared)
struct Room { std::string id; float x1, z1, x2, z2; unsigned light; bool flicker; std::vector<Vector2> lights; bool open = false; };   // open: al aire libre (sin techo)
struct Link { std::string a, b; float x, z; int cost; bool open; char axis; Vector2 sideA, sideB; };
struct Window {
    std::string room; float x, z; int nx, nz; int boards = 5;
    Vector3 boardBase[5]; float boardRot[5]; bool boardVis[5]; Vector3 boardPos[5]; Vector3 boardVel[5]; float boardAnim[5]; float boardSpin[5];
    int climber = -1; float repairT = 0; Vector2 outside, inside;
};
struct Door { int link; Matrix tf; Vector3 pos, size; float sink = 0; bool sinking = false; Box2 col; Texture2D label; int lab0 = -1; };
struct Interact {
    std::function<Vector2()> pos;
    float range;
    std::function<bool()> active;
    std::function<std::string()> text;
    std::function<void()> action;
    bool hold = false;
};
struct Prop { Mesh *mesh; PMat *mat; Matrix tf; bool visible = true; };
struct Label { Texture2D tex; Vector3 pos; float width; bool visible = true; };
struct MapDef;

extern std::vector<Room> rooms;
extern std::vector<Link> links;
extern std::vector<Window> windows;
extern std::vector<Door> doors;
extern std::vector<Box2> colliders;
extern std::vector<BoundingBox> solids;     // cajas 3D que detienen balas
extern std::vector<Interact> interactables;
extern std::vector<Light> lights;
extern std::vector<Prop> props;
extern std::vector<Label> labels;
extern std::vector<Box2> mmWalls;
struct MMIcon { float x, z; Color c; float r; };
extern std::vector<MMIcon> mmIcons;
extern std::vector<std::pair<Mesh, PMat *>> staticMeshes;
extern const MapDef *curMap;
extern int startRoom;
extern Mesh unitCube, unitCyl, unitSphere, unitPlane;
void BuildMap(const char *key);
int RoomAt(float x, float z);
int RoomIndex(const std::string &id);
bool Unlocked(int room);
void RecomputeUnlocked();
bool NextRoom(int from, int to, int &outRoom, int &outLink);
void Collide(Vector3 &pos, float r, bool climb = false);   // climb: lo que está por debajo de pos.y no estorba
float GroundAt(float x, float z, float r, float y);         // altura del suelo (o de la caja) bajo los pies
bool TryMantle(Vector3 &pos, Vector3 vel, float r);         // trepar a una caja al saltar contra ella
bool InsideCollider(float x, float z, float r);
void TearBoard(Window &w);
void UpdateWorld(float dt);
const char *MapName();
bool Practice();          // ¿estamos en el campo de tiro?
bool MapOutdoor();        // ¿el mapa es al aire libre? (cielo con luna)
int Cost(int c);
void UpdatePractice(float dt);
Vector2 MapStart(float &yaw);
extern bool powerOn;      // ¿se activó el interruptor de la corriente?
void PowerOnInstant();
bool PowerSwitchView(Vector3 &pos, Vector3 &eye);

// ---------- Caja misteriosa y Pack-a-Punch ----------
struct BoxState {
    int spot = 0; int state = 0; float t = 0, swapT = 0; int offer = -1, show = -1; int uses = 0;
    float lid = 0, lift = 0, spin = 0; Vector2 front{}, pos{}; float ry = 0; int colIdx = -1; bool teddy = false; float teddyRot = 0;
};
enum { BOX_IDLE, BOX_ROLLING, BOX_OFFER, BOX_CLOSING, BOX_TEDDY, BOX_LEAVING };
extern BoxState box;
struct PapState { int state = 0; float t = 0; int weaponIdx = 0; Vector2 front{}; Vector3 pos{}; float ry = 0; float roll = 0; };
extern PapState pap;
void UpdateBox(float dt);
void UpdatePap(float dt);
void DrawBoxAndPap();

// ---------- Armas ----------
enum WeaponId { W_PISTOL, W_SHOTGUN, W_SMG, W_RIFLE, W_LMG, W_SNIPER, W_REVOLVER, W_RAYGUN, W_WUNDER, W_LAUNCHER, W_RPG,
                W_SUOMI, W_FLARE, W_MAGNUM, W_SAWED, W_THUNDER, W_THOMPSON, W_GARAND, W_CROSSBOW, W_FLAMER, W_GATLING, W_COUNT };
struct WeaponDef {
    const char *name, *papName;
    float dmg, head, rate; bool autoFire; int mag, reserve; float reload, spread; int pellets; float recoil; int cost; float len;
    int sound; int projectile; float radius; float splash; int chain; unsigned tracer;
};
struct WStats { const char *name; float dmg, head, rate; bool autoFire; int mag, reserve; float reload, spread; int pellets; float recoil; int sound, projectile; float radius, splash; int chain; unsigned tracer; };
extern WeaponDef WEAPONS[W_COUNT];
WStats Stats(int id, bool papd);
struct OwnedWeapon { int id; int mag, reserve; bool pap; };
struct GunPart { Mesh *mesh; PMat *mat; Matrix local; };
struct GunModel {
    std::vector<GunPart> parts; float muzzleZ; float muzzleY = 0.015f, sightY = 0.05f, viewScale = 0.75f;
    PMat *papMat = nullptr;   // armas reales: material del Pack-a-Punch
    size_t handStart = 0;     // a partir de esta pieza van las manos
    size_t spinStart = 0, spinEnd = 0; float spinY = 0;   // piezas que giran sobre el eje del cañón (ametralladora rotativa)
};
extern float gatSpinK, gatSpinA;   // ametralladora rotativa: cuánto giran los cañones (0..1) y su ángulo
extern GunModel gunModels[W_COUNT];
void BuildGunModels();
void DrawGun(const GunModel &g, Matrix tf, bool papd, bool hands = true);
PMat &GunMat(const char *name, bool papd);
void Fire();
void ThrowGrenade();
void UpdateProjectiles(float dt);
void UpdateEffects(float dt);
void DrawEffects();
void DrawProjectiles();
void Explode(Vector3 p, float dmg, float radius, float selfDmg, float size);
void ExplosionFX(Vector3 p, float size, bool big);   // solo lo visual y el sonido (cinemáticas)
void FireFX(Vector3 p, float spread, float k, float dt);   // incendio que sigue ardiendo
void Burst(Vector3 p, Color c, int n, float speed, float size = 0.05f, float glow = 1);
void AddDecal(int type, Vector3 p, Vector3 normal);
void ClearEffects();
extern std::vector<int> BOX_POOL;

// ---------- Zombis ----------
enum ZState { Z_APPROACH, Z_TEAR, Z_CLIMB, Z_CHASE, Z_RISE, Z_DEAD };
enum ZKind { ZK_NORMAL, ZK_BRUTE, ZK_BOMBER };
enum Clip { C_WALK, C_RUN, C_ATTACK, C_DIE, C_IDLE, C_SCREAM, C_HIT, C_COUNT };
// animaciones que solo tiene el soldado (Frank, Milton y los guardias): agacharse, golpe, negar, salto, correr sin apuntar y caminar
enum SoldierClip { SC_DUCK = C_COUNT, SC_PUNCH, SC_NO, SC_JUMP, SC_RUN, SC_WALK, SC_COUNT };
struct Zombie {
    Vector3 pos; float yaw = 0, y = 0; int state; int win = -1; int room = 0;
    float hp, speed; bool runner;
    float deadT = 0, riseT = 0, climbT = 0, tearT = 0, nextAttack = 0, attackT = 0, nextGroan = 0, stuckCheckT = 0;
    Vector3 stuckPos; bool remove = false; float scale = 1; Color tint = WHITE;
    int clip = C_WALK, prevClip = C_WALK; float clipT = 0, prevT = 0, blend = 1, rate = 1;
    float hitFlash = 0;
    int dummy = -1; float patrolDir = 1;   // campo de tiro: índice de diana (-1 = zombi normal)
    bool dog = false;                      // perro del infierno (rondas de perros)
    int kind = 0;                          // ZK_NORMAL, ZK_BRUTE (grande y duro) o ZK_BOMBER (explota)
    bool blown = false;                    // el explosivo ya reventó
    std::shared_ptr<struct Ragdoll> rag;   // muñeco de trapo al morir
    Vector3 hitDir{ 0, 0, 0 }, hitPoint{ 0, 0, 0 }; float hitForce = 0; bool hitAll = false, ragTried = false;   // último impacto (para el ragdoll)
    Vector3 fling{ 0, 0, 0 };              // lanzado por el Cañón de Trueno
    float burnT = 0, burnDps = 0;          // ardiendo (pistola de bengalas)
    Vector3 headPos{}, hipsPos{}, neckPos{};
    int id = 0;                            // co-op: el mismo número en las dos PCs
    Vector3 netPos{ 0, 0, 0 }; float netYaw = 0, netY = 0; int netMiss = 0;   // co-op (invitado): última posición que mandó el anfitrión
    bool atRemote = false;                 // co-op (anfitrión): persigue al compañero
    bool atAlly = false;                   // persigue a Milton (el compañero bot)
    bool puppet = false;                   // títere de una cinemática: sin IA
    bool boss = false; float bossT = 0, chargeT = 0, summonT = 0;   // jefe final (Lázaro)
};
extern std::vector<Zombie> zombies;
void InitZombies();
void SpawnZombie();
void UpdateZombies(float dt);
void DrawZombies();
void DamageZombie(Zombie &z, float dmg, float headMul, bool head, Vector3 point, bool green);
void KillZombie(Zombie &z, bool head, bool fromNuke = false);
bool RayZombie(Ray r, float maxDist, int &zi, float &dist, bool &head, Vector3 &point);
float ZombieHP(int round);

// ---------- Jugador y partida ----------
struct Player {
    Vector3 pos{ 0, 0, 3 }, vel{ 0, 0, 0 }; float vy = 0; bool onGround = true; float yaw = 0, pitch = 0, eye = 1.65f; int room = 0;
    float health = 100, maxHealth = 100, lastHit = -99; int points = 500;
    std::vector<OwnedWeapon> weapons; int cur = 0; bool reloading = false; float reloadEnd = 0, reloadTime = 1, nextShot = 0;
    unsigned perks = 0; bool moving = false, sprinting = false, crouch = false;
    float slideT = 0, nextSlide = 0; Vector3 slideDir{ 0, 0, 0 };   // deslizamiento estilo Warzone
    float knifeT = -1, nextKnife = 0, lungeT = 0; bool knifeHit = false;   // cuchillo (knifeT < 0: guardado)
    int grenades = 2; float nextGrenade = 0; bool papBusy = false; float nextHeart = 0;
    bool downed = false; float downT = 0, reviveK = 0; int shots = 0;   // co-op: caído (esperando que lo reviva el compañero)
};
enum PerkId { P_REVIVE, P_JUGG, P_SPEED, P_DOUBLE, P_STAMIN, P_COUNT };
struct PerkDef { const char *name; char letter; unsigned color; int cost; const char *desc; };
extern PerkDef PERKS[P_COUNT];
inline bool HasPerk(const Player &p, int k) { return (p.perks >> k) & 1; }
struct Game {
    int state = 0; int round = 0, toSpawn = 0; float spawnT = 0; bool between = true; float betweenT = 3; float zHP = 150;
    float insta = 0, dbl = 0; int drops = 0, kills = 0, heads = 0, repairPts = 0; std::string mapKey = "bunker";
    bool dogRound = false; int nextDogRound = 5, dogRounds = 0; float fogK = 0; Vector3 lastDogPos{ 0, 0, 0 };   // rondas de perros
};
enum { GS_MENU, GS_PLAYING, GS_PAUSED, GS_OVER, GS_INTRO, GS_STORY, GS_CUTSCENE };
void ForceGameOver();   // co-op: el compañero avisa que se acabó
void ResetGame(const std::string &mapKey);   // empieza una partida en el mapa (con su cinemática)
void BackToMenu();
// Cinemática de llegada al mapa (intro.cpp)
void StartIntro();
void UpdateIntro(float dt);
void DrawIntro();
struct ViewModel { float kick = 0, swap = 0, ads = 0, bobT = 0, bobAmt = 0, flash = 0, shake = 0, sprint = 0, land = 0, roll = 0, recoil = 0, swayX = 0, swayY = 0, swayTX = 0, swayTY = 0, throwT = 0, slide = 0, fov = 75; unsigned flashColor = 0xffaa55; };
extern Player player;
extern Game game;
extern ViewModel vm;
extern float T;
extern bool mouseDown, mouseRight; extern float mousePressedAt;
inline OwnedWeapon &CurWeapon() { return player.weapons[player.cur]; }
void GiveWeapon(int id);
void Equip();
void StartReload();
bool Spend(int cost);
void AddPoints(int n);
void DamagePlayer(int n);
void CenterMsg(const std::string &text, Color c, float dur = 2.5f);
void SpecialMsg(const std::string &text);
void ShowHit(bool kill);
void WhiteFlash(float op);
void DropPowerup(float x, float z, int type = -1);   // type -1: al azar
void LightningStrike(Vector3 p);
bool SpawnDog();
void UpdatePowerups(float dt);
void DrawPowerups();
extern Vector3 muzzleLightPos; extern float muzzleLightI; extern Vector3 boomLightPos; extern float boomLightI;
extern Matrix gunWorld; extern bool gunVisible; extern bool scoped;
void Melee(); void UpdateMelee(float dt); void DrawKnife();
extern Matrix knifeWorld; extern bool knifeVisible;
const float KNIFE_TIME = 0.5f;

// ---------- Sonido ----------
void InitAudioSynth();
void Play(const char *name, float vol = 1, float pan = 0.5f);
void PlayAt(const char *name, Vector3 p, float maxDist = 25);
bool HasSound(const char *name);
void UpdateMusic();
void ToggleMusic();
extern bool musicOn;

// ---------- HUD ----------
void DrawHUD();
void DrawMenu();
void DrawPause();
void DrawGameOver();
void UpdateHUDTimers(float dt);
int MenuClicked();   // devuelve 0/1 si se eligió un mapa, 2 = jugar de nuevo
extern std::string promptText;

// ---------- Opciones y récords (se guardan junto al juego) ----------
struct Options {
    float sens = 1.0f;                      // multiplicador de la sensibilidad del ratón
    float master = 0.8f, music = 0.7f, sfx = 1.0f;
    float fov = 75;                         // campo de visión
    bool lowGfx = false, showFps = false;
    bool ally = false;                      // Milton acompaña también en supervivencia
    bool allyRosa = true;                   // uniforme rosa de Milton (si se desbloqueó)
};
extern Options opts;
void LoadOptions();
void SaveOptions();
struct Record { int round = 0, kills = 0, heads = 0; };
Record GetRecord(const std::string &map);
bool SubmitRecord(const std::string &map, int round, int kills, int heads);   // true si es un récord nuevo
extern const char *recordsFile;
extern bool optionsOpen;
void DrawOptions();
bool PauseButtonHover();
extern bool newRecord;   // la última partida batió el récord del mapa

// ---------- Ragdoll (ragdoll.cpp) ----------
struct Ragdoll;
std::shared_ptr<Ragdoll> RagdollStart(const Model &m, const std::vector<Transform> &pose, Matrix zm, float yaw, float scale, Vector3 vAll, Vector3 hit, Vector3 hitV, bool topple);
bool RagdollStep(Ragdoll &r, float dt);
void RagdollRelease(Ragdoll &r);
const std::vector<Transform> &RagdollPose(const Ragdoll &r);
Vector3 RagdollPoint(const Ragdoll &r, int which);   // 0 cabeza, 1 cadera, 2 pecho
void RagdollResetCount();
void PushProps(Vector3 p, float radius, float strength, Vector3 dir = Vector3{ 0, 0, 0 });   // explosiones y onda de trueno
void PokeProp(Vector3 hit, Vector3 dir);                                                    // un disparo mueve cajas y barriles
int DynPropCount();          // pruebas
Vector3 DynPropPos(int i);

// ---------- Frases del personaje (voice.cpp) ----------
enum VoiceKind { V_HURT, V_KILL, V_HEAD, V_RELOAD, V_ROUND, V_DOGS, V_LOWHP, V_NOAMMO, V_COUNT };
void LoadVoiceLines();
void ResetVoiceLines();
void Say(int kind, float chance = 1);   // chance: probabilidad de que hable
const char *VoiceSubtitle(float &alpha);   // lo que está diciendo Frank ahora (o nullptr)

// ---------- Modo historia "Operación Lázaro" (story.cpp) ----------
bool StoryActive();
int StoryChapter();
int StoryProgress();                       // capítulos terminados (0 a 3)
void StoryStart(int chapter, bool skipCards = false);
void StoryQuit();
void StorySetupMap();                      // objetos y objetivos del capítulo (después de BuildMap)
void UpdateStory(float dt);
void StoryAudioTick();                     // pausa la radio con el juego
bool StoryTalking();                       // la radio está hablando (Frank no se pisa con ella)
void StoryOnIntroEnd();
extern bool storyMenuOpen;                 // pantalla para elegir capítulo (hud.cpp)
bool StoryIntroLines(const char *&place, const char *ids[4]);
const char *StoryChapterTitle();
const char *StoryLineFile(const char *id);
const char *StoryLineText(const char *id, int &who);
const char *StorySpeakerName(int who);
Color StorySpeakerColor(int who);
void UpdateStoryCards(float dt);
void DrawStoryCards();
void DrawStoryWorld();
void DrawStoryLabels();
void DrawStoryHUD();
bool PowerSwitchSpot(Vector2 &p);          // dónde está el interruptor de la corriente (world.cpp)

// ---------- Compañero bot: el cabo Milton (ally.cpp) ----------
struct Ally {
    bool active = false; Vector3 pos{}; float moveYaw = 0, aimYaw = 0; int room = 0;
    float health = 160, maxHealth = 160, lastHit = -99;
    bool downed = false; float downT = 0, reviveK = 0; bool reviveHeld = false;
    int mag = 30; float reloadT = 0, fireT = 0, animT = 0; int kills = 0;
};
extern Ally ally;
bool AllyEnabled();
void AllySpawn();                          // después de BuildMap y de colocar al jugador
void UpdateAlly(float dt);
void AllyDamage(int n);
bool AllyZombieTarget(const Zombie &z, Vector3 &pos, int &room);
void AllyOnRound(bool dogs);
bool AllySpeaking();
void DrawAllyHUD();
bool AllyShootZombie(Zombie &z, float dmg, bool head, Vector3 point, Vector3 from);   // zombie.cpp: true si lo mató
void AllyTracer(Vector3 a, Vector3 b);    // weapons.cpp
bool FrankSpeaking();                      // voice.cpp
bool StoryWantsAlly();                     // story.cpp: ¿este capítulo tiene a Milton?
void StoryOnAllyDown();

// ---------- Cinemáticas con actores (cutscene.cpp) ----------
void StartCutscene(const char *id);
float CutsceneTimeScale();   // cámara lenta de la cinemática (1 = normal)
void DrawFilmLook(float k);   // viñeta y grano de película
void UpdateCutscene(float dt);
void DrawCutscene();
bool CutsceneActive();
int SpawnPuppet(Vector3 p, float yaw, float scale);          // zombie.cpp: zombi títere (sin IA)
void PuppetPose(int id, Vector3 p, float yaw, int clip, float t);
void ClearPuppets();
int SpawnBoss(Vector3 p, float hp);                           // zombie.cpp: Lázaro, el jefe final
const Zombie *FindBoss();
void StoryOnCutsceneEnd(const char *id);
Sound *StoryLineSound(const char *id);
void StopRadioForCutscene();
float StoryAllyHealth();
const char *StoryChapterName(int ch, const char *&sub);
int StoryChapterCount();

// ---------- Frank en la cinemática (zombie.cpp) ----------
// skin: uniforme (SKIN_FRANK, SKIN_MILTON, SKIN_GUARDIA, SKIN_ROSA); talk: 0..1, mueve la cabeza al hablar
enum { SKIN_FRANK, SKIN_MILTON, SKIN_GUARDIA, SKIN_ROSA, SKIN_COUNT };
struct SoldierView { bool visible = false; Vector3 pos{}; float yaw = 0; int clip = C_WALK; float t = 0; int skin = SKIN_FRANK; float talk = 0;
                     int clip2 = -1; float t2 = 0, w2 = 0;   // la animación anterior, que se va desvaneciendo (w2: 1 -> 0)
                     float lookYaw = 0, lookPitch = 0; };   // giro extra de la cabeza (mirar a alguien)
const int C_TALK = C_RUN;   // soldados: el clip "Run" se cambió por "Yes" (asiente al hablar)
int AllySkin();             // Milton (o rosa, si se encontró el huevo de pascua)
extern SoldierView soldierView;   // clip: C_WALK correr con arma, C_ATTACK apuntar, C_IDLE quieto
extern SoldierView allyView;      // Milton, el compañero bot
extern std::vector<SoldierView> cutsceneActors;   // soldados extra de las cinemáticas

// ---------- Co-op en línea (net.cpp): Frank es el anfitrión, Milton se une por IP ----------
enum { NET_OFF, NET_HOST, NET_CLIENT };
int NetRole();
bool NetActive();                 // hay partida en co-op con el compañero conectado
inline bool NetIsHost() { return NetRole() == NET_HOST && NetActive(); }
inline bool NetIsClient() { return NetRole() == NET_CLIENT; }
const char *LocalName();          // "FRANK" o "MILTON"
const char *RemoteName();
void NetUpdate(float dt);         // cada fotograma: recibir y enviar
void NetShutdown();
// avisos del juego a la red
void NetOnDoorOpened(int link);
void NetOnPower();
void NetOnRepair(int window);
void NetOnKill(const Zombie &z, bool head, bool byRemote, bool nuke);   // anfitrión
void NetSendHit(const Zombie &z, float dmg, bool head, Vector3 point);  // invitado
void NetOnDrop(int id, int type, float x, float z);                     // anfitrión
void NetOnPickup(int id, int type);                                     // anfitrión
void NetOnGameOver();
void NetDamageRemote(int n);
void NetDrawLabels();
// el compañero, visto desde aquí
struct RemotePlayer { bool present = false; Vector3 pos{}; float yaw = 0, pitch = 0, speed = 0, animT = 0, health = 100; int weapon = 0, points = 0, room = 0; bool crouch = false, downed = false; };
extern RemotePlayer remote;
bool NetZombieTarget(const Zombie &z, Vector3 &pos, int &room);   // anfitrión: ¿va a por el compañero?
// mundo compartido
void OpenLinkNow(int link);
void PowerOnNow();
void RepairBoardNow(int window);
void SetWindowBoards(int window, int count);
void PlayGunAt(int weapon, Vector3 pos);
// zombis replicados (zombie.cpp)
#pragma pack(push, 1)
struct NetZ { int id; float x, y, z, yaw, clipT, rate, scale; unsigned char state, clip, kind, flags, tint; };   // flags: 1 perro, 2 corredor
#pragma pack(pop)
int NetCollectZombies(NetZ *out, int max);
void NetApplyZombies(const NetZ *in, int n);
void NetRemoteHit(int id, float dmg, bool head, Vector3 point, Vector3 dir);   // anfitrión: disparo del invitado
void NetRemoteKill(int id, bool head, bool mine, bool nuke, Vector3 dir, float force);   // invitado
extern SoldierView remoteView;    // el soldado del compañero
// power-ups compartidos (main.cpp)
void ApplyPowerup(int type);
void NetAddPowerup(int id, int type, float x, float z);
void NetRemovePowerup(int id);
void RoundFlash();
void DogFog(float k);
void NetSendRevive();             // levanté al compañero
void NetRevived();                // el compañero me levantó
// sala de espera del co-op (net.cpp, la dibuja hud.cpp)
enum { LB_NONE, LB_CHOICE, LB_HOST_PICK, LB_HOSTING, LB_JOIN, LB_CONNECTING };
int NetLobbyScreen();
void NetLobbyClick(int click);
std::string &NetJoinIP();
const std::string &NetStatus();
std::string NetPublicIP();
std::string NetLocalIP();
int NetPing();
bool NetTakeStart(std::string &map);
void NetRestart(const std::string &map);
bool NetConnected();             // hay conexión con el compañero (aunque la partida no haya empezado)

// ---------- Diálogo fuera del guion y huevos de pascua (ally.cpp, eggs.cpp) ----------
void DialogSay(std::vector<const char *> ids);   // frases de guion.tsv en cola (charla con Milton, secretos)
bool DialogTalking();
void DialogClear();
void UpdateDialog();
void DrawDialogHUD();
float AllyChatTalk();
float AllyTalk();
float VoiceEnvelope(const char *file, float t);   // story.cpp: volumen de la voz en el segundo t (0..1)
float StoryMiltonTalk();
bool EggRosa();                                    // ¿Milton usa el uniforme rosa?
bool EggRosaUnlocked();
int EggRadiosFound();
void EggsSetupMap();
void EggsUpdate();
void EggsDraw();
extern bool bigHeads;                              // truco CABEZON
const char *MapKeyNow();                           // world.cpp: clave del mapa actual
