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
struct Box2 { float minX, maxX, minZ, maxZ; };
struct Room { std::string id; float x1, z1, x2, z2; unsigned light; bool flicker; std::vector<Vector2> lights; };
struct Link { std::string a, b; float x, z; int cost; bool open; char axis; Vector2 sideA, sideB; };
struct Window {
    std::string room; float x, z; int nx, nz; int boards = 5;
    Vector3 boardBase[5]; float boardRot[5]; bool boardVis[5]; Vector3 boardPos[5]; Vector3 boardVel[5]; float boardAnim[5]; float boardSpin[5];
    int climber = -1; float repairT = 0; Vector2 outside, inside;
};
struct Door { int link; Matrix tf; Vector3 pos, size; float sink = 0; bool sinking = false; Box2 col; Texture2D label; };
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
void Collide(Vector3 &pos, float r);
bool InsideCollider(float x, float z, float r);
void TearBoard(Window &w);
void UpdateWorld(float dt);
const char *MapName();
bool Practice();          // ¿estamos en el campo de tiro?
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
                W_SUOMI, W_FLARE, W_MAGNUM, W_SAWED, W_THUNDER, W_COUNT };
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
};
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
void Burst(Vector3 p, Color c, int n, float speed, float size = 0.05f, float glow = 1);
void AddDecal(int type, Vector3 p, Vector3 normal);
void ClearEffects();
extern std::vector<int> BOX_POOL;

// ---------- Zombis ----------
enum ZState { Z_APPROACH, Z_TEAR, Z_CLIMB, Z_CHASE, Z_RISE, Z_DEAD };
enum ZKind { ZK_NORMAL, ZK_BRUTE, ZK_BOMBER };
enum Clip { C_WALK, C_RUN, C_ATTACK, C_DIE, C_IDLE, C_SCREAM, C_HIT, C_COUNT };
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
enum { GS_MENU, GS_PLAYING, GS_PAUSED, GS_OVER };
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
};
extern Options opts;
void LoadOptions();
void SaveOptions();
struct Record { int round = 0, kills = 0, heads = 0; };
Record GetRecord(const std::string &map);
bool SubmitRecord(const std::string &map, int round, int kills, int heads);   // true si es un récord nuevo
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
