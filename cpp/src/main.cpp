// =============================================================
//  ÚLTIMA RONDA — juego de zombis en primera persona (C++ / raylib)
//  Principal: ventana, jugador, cámara, controles, rondas y power-ups.
// =============================================================
#include "game.h"

Player player;
Game game;
ViewModel vm;
float T = 0;
bool mouseDown = false, mouseRight = false; float mousePressedAt = -9;
Vector3 muzzleLightPos{}; float muzzleLightI = 0;
Matrix gunWorld = MatrixIdentity(); bool gunVisible = true; bool scoped = false;
extern float flashK, exposureK;
extern RenderTexture2D *captureTarget;
void InitBaseMeshes();
void PointsPop(int n);
void PointsDeny();
void RoundFlash();

PerkDef PERKS[P_COUNT] = {
    { "QUICK REVIVE", 'Q', 0x2a8cff, 500,  "Revives una vez" },
    { "JUGGERNOG",    'J', 0xb01818, 2500, "Más vida" },
    { "SPEED COLA",   'S', 0x18a030, 3000, "Recarga rápida" },
    { "DOUBLE TAP",   'D', 0xd08a10, 2000, "Disparo más rápido" },
    { "POLA VELOZ",   'P', 0xe0b020, 2000, "Corres mucho más rápido" },
};

// Puntos de aparición en el suelo (zombis que salen de la tierra)
std::vector<std::pair<int, Vector2>> GroundSpawns();

// ---------- Armas del jugador ----------
void Equip() {
    vm.swap = 1;
    player.reloading = false;
    player.nextShot = std::max(player.nextShot, T + (HasPerk(player, P_SPEED) ? 0.18f : 0.35f));
}
void GiveWeapon(int id) {
    const WeaponDef &d = WEAPONS[id];
    OwnedWeapon w{ id, d.mag, d.reserve, false };
    int idx = -1;
    for (size_t i = 0; i < player.weapons.size(); i++) if (player.weapons[i].id == id) idx = (int)i;
    if (idx >= 0) { player.weapons[idx] = w; player.cur = idx; }
    else if (player.weapons.size() < 3) { player.weapons.push_back(w); player.cur = (int)player.weapons.size() - 1; }   // 3 espacios de armas
    else player.weapons[player.cur] = w;
    Equip();
}
static void SwitchWeapon(int dir) {
    int n = (int)player.weapons.size();
    if (n < 2 || player.papBusy) return;
    player.cur = (player.cur + dir + n) % n;
    Equip();
}
void StartReload() {
    OwnedWeapon &w = CurWeapon();
    WStats s = Stats(w.id, w.pap);
    if (player.reloading || player.papBusy || w.mag >= s.mag || w.reserve <= 0) return;
    player.reloading = true;
    player.reloadTime = s.reload * (HasPerk(player, P_SPEED) ? 0.5f : 1);
    player.reloadEnd = T + player.reloadTime;
    // recarga grabada según el tipo de arma (o la sintetizada si no hay)
    const char *rs = (w.id == W_PISTOL || w.id == W_REVOLVER || w.id == W_RAYGUN || w.id == W_MAGNUM || w.id == W_FLARE) ? "reload_pistol" : (w.id == W_SMG || w.id == W_SUOMI || w.id == W_THOMPSON) ? "reload_smg" : "reload_rifle";
    Play(HasSound(rs) ? rs : "reload");
    Say(V_RELOAD, 0.2f);
}
bool Spend(int cost) {
    if (Practice()) { Play("buy"); return true; }   // campo de tiro: todo gratis
    if (player.points >= cost) { player.points -= cost; Play("buy"); return true; }
    Play("deny");
    PointsDeny();
    return false;
}
void AddPoints(int n) {
    if (T < game.dbl) n *= 2;
    player.points += n;
    PointsPop(n);
}
void WhiteFlash(float op) { flashK = op; }

static void GameOver() {
    newRecord = !Practice() && !StoryActive() && SubmitRecord(game.mapKey, game.round, game.kills, game.heads);
    game.state = GS_OVER;
    EnableCursor();
    scoped = false;
}
void DamagePlayer(int n) {
    if (n <= 0 || Practice() || player.downed) return;   // en el campo de tiro no te pueden hacer daño
    if (game.state != GS_PLAYING && !(NetActive() && game.state == GS_PAUSED)) return;   // en co-op la pausa no te hace invencible
    player.health -= n;
    player.lastHit = T;
    vm.shake = std::max(vm.shake, 0.3f);
    Play("hurt");
    if (player.health > 0) Say(player.health < player.maxHealth * 0.35f ? V_LOWHP : V_HURT, player.health < player.maxHealth * 0.35f ? 0.6f : 0.45f);
    if (player.health > 0) return;
    if (HasPerk(player, P_REVIVE)) {
        player.perks &= ~(1u << P_REVIVE);
        player.health = player.maxHealth;
        if (!NetIsClient()) for (auto &z : zombies) if (z.state == Z_CHASE && Vector3Distance(z.pos, player.pos) < 6) KillZombie(z, false, true);
        CenterMsg("¡REVIVISTE!", Color{ 124, 196, 255, 255 });
        WhiteFlash(0.7f);
    } else if (NetActive() && remote.present && !remote.downed) {
        // co-op: caes y tu compañero tiene 30 segundos para levantarte
        player.downed = true; player.downT = 30; player.health = 1;
        player.vel = Vector3{ 0, 0, 0 }; player.reloading = false; player.slideT = 0;
        CenterMsg("¡CAÍSTE!", Color{ 255, 64, 64, 255 }, 3);
        SpecialMsg(TextFormat("%s tiene que revivirte: que se acerque y mantenga E", RemoteName()));
    } else { GameOver(); NetOnGameOver(); }
}
void ForceGameOver() { if (game.state == GS_PLAYING || game.state == GS_PAUSED || game.state == GS_INTRO) GameOver(); }

// ---------- Power-ups ----------
struct PowerDef { const char *name, *shortName; unsigned color; };
static PowerDef POWERS[4] = {
    { "MUNICIÓN MÁXIMA", "MAX", 0xffd23f }, { "MUERTE INSTANTÁNEA", "INSTA", 0xff3b3b },
    { "DOBLE PUNTOS", "x2", 0x3bff6b }, { "BOMBA NUCLEAR", "BOMBA", 0xffa23b },
};
struct Powerup { int type; Vector3 p; float t; int id; };
static std::vector<Powerup> powerups;
static Texture2D powerLabels[4];
static PMat powerMats[4];

void DropPowerup(float x, float z, int type) {
    static int idSeq = 1;
    Powerup p{ type < 0 ? irand(4) : type, Vector3{ x, 1, z }, 0, idSeq++ };
    powerups.push_back(p);
    if (NetIsHost()) NetOnDrop(p.id, p.type, x, z);   // co-op: el compañero también lo ve
}
// co-op (invitado): el anfitrión soltó o recogió un power-up
void NetAddPowerup(int id, int type, float x, float z) { powerups.push_back({ type, Vector3{ x, 1, z }, 0, id }); }
void NetRemovePowerup(int id) {
    for (size_t i = 0; i < powerups.size(); i++) if (powerups[i].id == id) { powerups.erase(powerups.begin() + i); return; }
}
void ApplyPowerup(int type) {
    CenterMsg(POWERS[type].name, Hex(POWERS[type].color));
    Play("powerup");
    if (type == 0) { for (auto &w : player.weapons) w.reserve = Stats(w.id, w.pap).reserve; player.grenades = 4; }
    if (type == 1) game.insta = T + 20;
    if (type == 2) game.dbl = T + 20;
    if (type == 3) {
        for (auto &z : zombies) if (z.state != Z_DEAD) KillZombie(z, false, true);
        AddPoints(400);
        WhiteFlash(0.9f);
    }
}
void UpdatePowerups(float dt) {
    for (int i = (int)powerups.size() - 1; i >= 0; i--) {
        Powerup &p = powerups[i];
        p.t += dt;
        p.p.y = 1 + sinf(p.t * 3) * 0.15f;
        if (NetIsClient()) continue;   // co-op: recoger lo decide el anfitrión
        float d = hyp(p.p.x - player.pos.x, p.p.z - player.pos.z);
        if (remote.present && !remote.downed) d = std::min(d, hyp(p.p.x - remote.pos.x, p.p.z - remote.pos.z));   // también lo recoge el compañero
        if (d < 1.3f || p.t > 26) {
            if (d < 1.3f) { ApplyPowerup(p.type); if (NetIsHost()) NetOnPickup(p.id, p.type); }
            else if (NetIsHost()) NetOnPickup(p.id, -1);
            powerups.erase(powerups.begin() + i);
        }
    }
}
void DrawUnlitBegin(float intensity);
void DrawUnlitEnd();
void DrawPowerups() {
    for (auto &p : powerups) {
        if (p.t > 20 && ((int)(p.t * 6)) % 2) continue;
        DrawPM(unitCube, powerMats[p.type], MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixScale(0.4f, 0.4f, 0.4f), MatrixRotateX(p.t)), MatrixRotateY(p.t * 2)), MatrixTranslate(p.p.x, p.p.y, p.p.z)));
    }
    if (depthPass) return;
    DrawUnlitBegin(1.5f);
    for (auto &p : powerups) {
        if (p.t > 20 && ((int)(p.t * 6)) % 2) continue;
        DrawWorldLabel(powerLabels[p.type], Vector3{ p.p.x, p.p.y + 0.6f, p.p.z }, 1.0f);
    }
    DrawUnlitEnd();
}

// ---------- Rondas ----------
static const int MAX_ALIVE = 24;
static void StartRound() {
    game.round++;
    game.between = false;
    game.drops = 0;
    game.repairPts = 0;
    game.zHP = ZombieHP(game.round);
    if (game.round > 1) player.grenades = std::min(4, player.grenades + 2);
    RoundFlash();
    // ronda de perros: cada 4-5 rondas, a partir de la 5 o la 6
    game.dogRound = game.round == game.nextDogRound;
    if (game.dogRound) {
        game.dogRounds++;
        game.nextDogRound = game.round + 4 + irand(2);
        game.toSpawn = std::min(24, 4 + game.dogRounds * 4);
        game.spawnT = 3.5f;
        Play("dogstart");
        Say(V_DOGS);
        AllyOnRound(true);
        CenterMsg("¡RONDA DE PERROS!", Color{ 255, 120, 40, 255 }, 3.5f);
        return;
    }
    game.toSpawn = std::min(70, (int)roundf(5 + game.round * 3 + (game.round > 5 ? game.round * 1.5f : 0)));
    game.spawnT = 1;
    Play("round");
    if (game.round > 1) { Say(V_ROUND, 0.7f); AllyOnRound(false); }
    if (game.round == 5) SpecialMsg(CONFIG::mensaje5);
    if (game.round == 10) SpecialMsg(CONFIG::mensaje10);
    if (game.round == 15) SpecialMsg(CONFIG::mensaje15);
}
extern float themeFogDensity; extern Vector3 themeFog;
void DogFog(float k);
static void UpdateRounds(float dt) {
    // niebla espesa y rojiza durante la ronda de perros
    game.fogK = lerpf(game.fogK, game.dogRound ? 1.0f : 0.0f, dampk(0.8f, dt));
    DogFog(game.fogK);
    if (game.between) {
        game.betweenT -= dt;
        if (game.betweenT <= 0) StartRound();
        return;
    }
    int alive = 0;
    for (auto &z : zombies) if (z.state != Z_DEAD) alive++;
    if (game.toSpawn > 0) {
        game.spawnT -= dt;
        if (game.spawnT <= 0 && alive < (game.dogRound ? 6 + game.dogRounds : MAX_ALIVE)) {
            if (game.dogRound) {
                if (SpawnDog()) game.toSpawn--;
                game.spawnT = frand(0.8f, 2.2f);
            } else {
                SpawnZombie();
                game.toSpawn--;
                game.spawnT = std::max(0.5f, 2.2f - game.round * 0.15f);
            }
        }
    } else if (alive == 0) {
        game.between = true;
        game.betweenT = 9;
        CenterMsg("RONDA SUPERADA", Color{ 255, 216, 74, 255 });
        Play("roundEnd");
        if (game.dogRound) {   // el último perro deja munición máxima
            DropPowerup(game.lastDogPos.x, game.lastDogPos.z, 0);
            game.dogRound = false;
        }
    }
}

// ---------- Interacción ----------
static Interact *currentInteract = nullptr;
static void UpdateInteract() {
    Interact *best = nullptr;
    float bd = 1e9f;
    for (auto &it : interactables) {
        if (!it.active()) continue;
        Vector2 p = it.pos();
        float d = hyp(p.x - player.pos.x, p.y - player.pos.z);
        if (d < it.range && d < bd) { best = &it; bd = d; }
    }
    currentInteract = best;
    promptText = best ? best->text() : "";
    if (best && best->hold && IsKeyDown(KEY_E)) best->action();
}

// ---------- Jugador ----------
static Matrix CamRot() {
    return MatrixMultiply(MatrixMultiply(MatrixRotateZ(vm.roll), MatrixRotateX(player.pitch)), MatrixRotateY(player.yaw));
}

static const float SLIDE_TIME = 0.8f;
static void UpdatePlayer(float dt) {
    if (player.downed) {
        // co-op: en el suelo, sin moverse, hasta que el compañero te levante (o se acabe el tiempo)
        player.downT -= dt;
        player.vel = Vector3{ 0, 0, 0 };
        player.eye = lerpf(player.eye, 0.45f, dampk(5, dt));
        vm.roll = lerpf(vm.roll, 0.35f, dampk(3, dt));
        cam.position = Vector3{ player.pos.x, player.pos.y + player.eye, player.pos.z };
        if (player.downT <= 0 || !remote.present || remote.downed) { GameOver(); NetOnGameOver(); }
        return;
    }
    // co-op: mantener E junto al compañero caído para levantarlo
    if (remote.present && remote.downed && hyp(remote.pos.x - player.pos.x, remote.pos.z - player.pos.z) < 2.0f && IsKeyDown(KEY_E)) {
        player.reviveK += dt;
        if (player.reviveK >= 3) { player.reviveK = 0; NetSendRevive(); AddPoints(100); }
    } else player.reviveK = 0;
    float fx = -sinf(player.yaw), fz = -cosf(player.yaw);
    float rx = cosf(player.yaw), rz = -sinf(player.yaw);
    float mx = 0, mz = 0;
    if (IsKeyDown(KEY_W)) { mx += fx; mz += fz; }
    if (IsKeyDown(KEY_S)) { mx -= fx; mz -= fz; }
    if (IsKeyDown(KEY_D)) { mx += rx; mz += rz; }
    if (IsKeyDown(KEY_A)) { mx -= rx; mz -= rz; }
    float len = hyp(mx, mz);
    bool crouchKey = IsKeyDown(KEY_C) || IsKeyDown(KEY_LEFT_CONTROL);
    bool crouchPressed = IsKeyPressed(KEY_C) || IsKeyPressed(KEY_LEFT_CONTROL);
    // deslizamiento: corriendo + agacharse => sale disparado hacia delante y frena poco a poco
    if (crouchPressed && player.sprinting && player.onGround && player.slideT <= 0 && T >= player.nextSlide) {
        float hs = hyp(player.vel.x, player.vel.z);
        player.slideDir = hs > 0.1f ? Vector3{ player.vel.x / hs, 0, player.vel.z / hs } : Vector3{ fx, 0, fz };
        player.slideT = SLIDE_TIME;
        player.nextSlide = T + SLIDE_TIME + 0.35f;
        Play("slide", 0.7f);
    }
    bool sliding = player.slideT > 0;
    player.crouch = crouchKey || sliding;
    player.moving = len > 0 || sliding;
    player.sprinting = !sliding && len > 0 && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) && IsKeyDown(KEY_W) && !mouseRight && !player.reloading && !crouchKey;
    float speed = player.crouch ? 2.8f : player.sprinting ? 8.6f : 5.6f;
    if (HasPerk(player, P_STAMIN)) speed *= player.sprinting ? 1.35f : 1.25f;
    if (mouseRight) speed *= 0.6f;
    float tx = len ? mx / len * speed : 0, tz = len ? mz / len * speed : 0;
    if (sliding) {
        float p = 1 - player.slideT / SLIDE_TIME;   // 0 -> 1 a lo largo del deslizamiento
        float ss = lerpf(12.5f, 3.0f, p * p) * (HasPerk(player, P_STAMIN) ? 1.15f : 1.0f);
        // con A/D se puede corregir un poco el rumbo
        float steer = (IsKeyDown(KEY_D) ? 1.0f : 0.0f) - (IsKeyDown(KEY_A) ? 1.0f : 0.0f);
        if (steer != 0) {
            float a = -steer * 0.9f * dt, c = cosf(a), sn = sinf(a);
            player.slideDir = Vector3{ player.slideDir.x * c + player.slideDir.z * sn, 0, -player.slideDir.x * sn + player.slideDir.z * c };
        }
        player.vel.x = player.slideDir.x * ss;
        player.vel.z = player.slideDir.z * ss;
        player.slideT -= dt;
    } else {
        float k = player.lungeT > 0 ? 0 : dampk(player.onGround ? (len ? 14.0f : 10.0f) : 2.0f, dt);   // en la estocada se mantiene el impulso
        player.lungeT = std::max(0.0f, player.lungeT - dt);
        player.vel.x += (tx - player.vel.x) * k;
        player.vel.z += (tz - player.vel.z) * k;
    }
    player.pos.x += player.vel.x * dt;
    player.pos.z += player.vel.z * dt;

    if (IsKeyDown(KEY_SPACE) && player.onGround && (!player.crouch || sliding)) {
        if (sliding) { player.slideT = 0; player.vy = 5.6f; }   // saltar cancela el deslizamiento conservando la inercia
        else player.vy = 6.2f;
        player.onGround = false;
    }
    bool wasAir = !player.onGround;
    float prevY = player.pos.y;
    player.vy -= 18 * dt;
    player.pos.y += player.vy * dt;
    // el suelo puede ser una caja: se cae al bajarse de ella
    float floorY = GroundAt(player.pos.x, player.pos.z, 0.25f, prevY);
    player.onGround = false;
    if (player.pos.y <= floorY) {
        if (wasAir && player.vy < -3) { vm.land = std::min(0.18f, -player.vy * 0.022f); Play("step", 0.8f); }
        player.pos.y = floorY; player.vy = 0; player.onGround = true;
    }
    // saltando contra una caja se trepa encima
    if (!player.onGround && player.vy < 3.5f && (IsKeyDown(KEY_SPACE) || len > 0) && TryMantle(player.pos, player.vel, 0.4f)) {
        player.vy = 0; player.onGround = true;
        vm.land = 0.08f; Play("step", 0.6f);
    }
    for (auto &z : zombies) {
        if (z.state != Z_CHASE) continue;
        float dx = player.pos.x - z.pos.x, dz = player.pos.z - z.pos.z, d = hyp(dx, dz);
        if (d < 0.75f && d > 0.001f) { player.pos.x += dx / d * (0.75f - d); player.pos.z += dz / d * (0.75f - d); }
    }
    if (remote.present) {   // co-op: no atravesar al compañero
        float dx = player.pos.x - remote.pos.x, dz = player.pos.z - remote.pos.z, d = hyp(dx, dz);
        if (d < 0.7f && d > 0.001f) { player.pos.x += dx / d * (0.7f - d); player.pos.z += dz / d * (0.7f - d); }
    }
    Collide(player.pos, 0.4f, true);
    int r = RoomAt(player.pos.x, player.pos.z);
    if (r >= 0) player.room = r;

    if (T - player.lastHit > 4 && player.health < player.maxHealth) player.health = std::min(player.maxHealth, player.health + 60 * dt);
    if (player.health / player.maxHealth < 0.4f && T > player.nextHeart) { Play("heart"); player.nextHeart = T + 0.75f; }

    // cámara suave: balanceo, inclinación lateral y aterrizaje
    float hspeed = hyp(player.vel.x, player.vel.z);
    float prevBob = vm.bobT;
    vm.slide = lerpf(vm.slide, player.slideT > 0 ? 1.0f : 0.0f, dampk(player.slideT > 0 ? 14.0f : 7.0f, dt));
    if (player.onGround && player.slideT <= 0) vm.bobT += dt * std::min(hspeed, 9.0f) * 1.8f;
    if (player.onGround && hspeed > 1.5f && floorf(prevBob / PI_F) != floorf(vm.bobT / PI_F)) Play("step", player.crouch ? 0.15f : player.sprinting ? 0.45f : 0.3f);
    vm.bobAmt = lerpf(vm.bobAmt, player.onGround && player.slideT <= 0 ? std::min(1.0f, hspeed / 6) : 0, dampk(10, dt));
    float bobY = sinf(vm.bobT * 2) * 0.04f * vm.bobAmt;
    float bobX = cosf(vm.bobT) * 0.025f * vm.bobAmt;
    vm.land = std::max(0.0f, vm.land - dt * 0.7f);
    vm.shake = std::max(0.0f, vm.shake - dt);
    player.eye = lerpf(player.eye, player.slideT > 0 ? 0.8f : player.crouch ? 1.05f : 1.65f, dampk(player.slideT > 0 ? 18.0f : 12.0f, dt));
    float sh = vm.shake * 0.12f;
    cam.position = Vector3{ player.pos.x + rx * bobX + frand(-sh, sh), player.pos.y + player.eye + bobY - vm.land, player.pos.z + rz * bobX + frand(-sh, sh) };
    float strafe = player.vel.x * rx + player.vel.z * rz;
    vm.roll = lerpf(vm.roll, -strafe * 0.006f + vm.slide * 0.09f, dampk(8, dt));   // al deslizarse la cámara se ladea
    float rc = vm.recoil * dampk(20, dt);
    player.pitch = clampf(player.pitch + rc, -1.5f, 1.5f);
    vm.recoil -= rc;
}

static void UpdateCamera() {
    Matrix R = CamRot();
    cam.target = Vector3Add(cam.position, Vector3Transform(Vector3{ 0, 0, -1 }, R));
    cam.up = Vector3Transform(Vector3{ 0, 1, 0 }, R);
    cam.projection = CAMERA_PERSPECTIVE;
    muzzleLightPos = Vector3Add(cam.position, Vector3Transform(Vector3{ 0.1f, -0.1f, -0.7f }, R));
}

static void UpdateWeapon(float dt) {
    OwnedWeapon &w = CurWeapon();
    WStats s = Stats(w.id, w.pap);
    if (player.reloading && T >= player.reloadEnd) {
        int n = std::min(s.mag - w.mag, w.reserve);
        w.mag += n; w.reserve -= n;
        player.reloading = false;
    }
    UpdateMelee(dt);
    // ametralladora rotativa: los cañones tienen que girar antes de disparar
    bool spinning = true;
    if (w.id == W_GATLING) {
        bool want = mouseDown && !player.downed && !player.papBusy && player.knifeT < 0 && !player.reloading;
        if (want && gatSpinK < 0.05f) Play("spin", 0.7f);
        gatSpinK = clampf(gatSpinK + (want ? 2.5f : -1.2f) * dt, 0, 1);
        spinning = gatSpinK >= 1;
    } else gatSpinK = std::max(0.0f, gatSpinK - dt * 3);
    gatSpinA += gatSpinK * 38 * dt;
    if (spinning && mouseDown && !player.downed && !player.papBusy && player.knifeT < 0 && (s.autoFire || T - mousePressedAt < 0.25f) && !player.reloading && T >= player.nextShot) {
        if (w.mag > 0) Fire();
        else if (w.reserve > 0) StartReload();
        else { Play("empty"); player.nextShot = T + 0.3f; Say(V_NOAMMO, 0.5f); }
        mousePressedAt = -9;
    }
    // animación del arma
    vm.kick = std::max(0.0f, vm.kick - dt * 7);
    vm.swap = std::max(0.0f, vm.swap - dt * 3);
    vm.throwT = std::max(0.0f, vm.throwT - dt);
    vm.ads += ((mouseRight && !player.reloading && !player.papBusy && player.knifeT < 0 ? 1.0f : 0.0f) - vm.ads) * dampk(12, dt);
    vm.sprint += ((player.sprinting ? 1.0f : 0.0f) - vm.sprint) * dampk(10, dt);
    vm.swayX = lerpf(vm.swayX, vm.swayTX, dampk(10, dt));
    vm.swayY = lerpf(vm.swayY, vm.swayTY, dampk(10, dt));
    vm.swayTX *= expf(-8 * dt); vm.swayTY *= expf(-8 * dt);
    vm.flash -= dt;
    muzzleLightI = vm.flash > 0 ? 25.0f : 0;
    float rl = 0;
    if (player.reloading) rl = sinf(clampf(1 - (player.reloadEnd - T) / player.reloadTime, 0, 1) * PI_F);
    float thr = sinf((vm.throwT / 0.4f) * PI_F);
    float sway = 1 - vm.ads * 0.8f;
    float gs = gunModels[w.id].viewScale;
    float bx = sinf(vm.bobT) * 0.012f * vm.bobAmt * sway;
    float by = fabsf(cosf(vm.bobT)) * 0.01f * vm.bobAmt * sway;
    Vector3 hp{ lerpf(0.17f, 0, vm.ads) + bx + vm.sprint * 0.04f + vm.swayX * sway,
                lerpf(-0.17f, -0.0675f - gunModels[w.id].sightY * gunModels[w.id].viewScale, vm.ads) - by - rl * 0.12f - vm.swap * 0.3f - vm.sprint * 0.04f - thr * 0.25f + vm.swayY * sway,
                lerpf(-0.42f, -0.34f, vm.ads) + vm.kick * 0.06f };
    // cuchillo: el arma baja mientras la mano derecha da el tajo
    float kp = player.knifeT >= 0 ? player.knifeT / KNIFE_TIME : -1;
    float kd = kp < 0 ? 0 : kp < 0.15f ? kp / 0.15f : kp > 0.75f ? (1 - kp) / 0.25f : 1;
    hp.y -= kd * 0.3f; hp.x += kd * 0.08f;
    float sl = vm.slide * (1 - vm.ads);   // arma más baja y ladeada al deslizarse
    hp.x -= sl * 0.03f; hp.y -= sl * 0.03f;
    Vector3 hr{ vm.kick * 0.12f - rl * 0.7f - vm.sprint * 0.2f, vm.sprint * 0.6f + vm.swayX * 2, rl * 0.3f - vm.roll * 2 + sl * 0.5f };
    Matrix holder = MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixRotateZ(hr.z), MatrixRotateY(hr.y)), MatrixRotateX(hr.x)), MatrixTranslate(hp.x, hp.y, hp.z));
    Matrix camWorld = MatrixMultiply(CamRot(), MatrixTranslate(cam.position.x, cam.position.y, cam.position.z));
    gunWorld = MatrixMultiply(MatrixMultiply(MatrixScale(gs, gs, gs), holder), camWorld);
    knifeVisible = kp >= 0 && kp < 0.95f;
    if (knifeVisible) {
        float sw = clampf((kp - 0.08f) / 0.3f, 0, 1); sw = sw * sw * (3 - 2 * sw);   // de derecha a izquierda
        float back = clampf((kp - 0.55f) / 0.4f, 0, 1);                               // y luego se retira
        Vector3 p = Vector3Lerp(Vector3{ 0.28f, -0.17f, -0.45f }, Vector3{ -0.08f, -0.13f, -0.58f }, sw);
        p.y -= back * 0.35f; p.x += back * 0.15f;
        Matrix rot = MatrixMultiply(MatrixMultiply(MatrixRotateZ(-1.35f), MatrixRotateY(lerpf(-0.15f, 0.75f, sw))), MatrixRotateX(0.1f));
        knifeWorld = MatrixMultiply(MatrixMultiply(rot, MatrixTranslate(p.x, p.y, p.z)), camWorld);
    }

    scoped = w.id == W_SNIPER && vm.ads > 0.85f;
    camFov = opts.fov + vm.sprint * 6 + vm.slide * 8 - vm.ads * (w.id == W_SNIPER ? 50 : 20);
    gunVisible = !scoped && !player.papBusy;
}

// ---------- Partida ----------
static int ignoreMouse = 0;
void ResetGame(const std::string &mapKey) {
    zombies.clear();
    powerups.clear();
    BuildMap(mapKey.c_str());
    game = Game();
    game.mapKey = mapKey;
    game.nextDogRound = 5 + irand(2);
    game.state = GS_PLAYING;
    player = Player();
    player.points = CONFIG::puntosIniciales;
    float yaw;
    Vector2 s = MapStart(yaw);
    player.pos = Vector3{ s.x, 0, s.y };
    player.yaw = yaw;
    player.room = startRoom;
    if (NetRole() == NET_CLIENT) player.pos.x += 1.6f;   // co-op: Milton aparece al lado de Frank
    vm = ViewModel();
    T = 0;
    ResetVoiceLines();
    GiveWeapon(W_PISTOL);
    StorySetupMap();
    AllySpawn();
    EggsSetupMap();
    if (Practice()) {
        void ResetPractice();
        ResetPractice();
        game.drops = 99;
        CenterMsg("CAMPO DE TIRO", Color{ 255, 216, 74, 255 }, 2.5f);
        SpecialMsg("Todo es gratis y nadie te puede hacer daño. ¡A practicar!");
    } else StartIntro();   // "SOBREVIVE" sale al terminar la cinemática
    DisableCursor();
    ignoreMouse = 3;
}

void BackToMenu() {
    NetShutdown();
    StoryQuit();
    game.state = GS_MENU;
    zombies.clear();
    BuildMap("bunker");
    PowerOnInstant();
    EnableCursor();
}

static void Update(float dt) {
    T += dt;
    flashK = std::max(0.0f, flashK - dt * 0.8f);
    UpdatePlayer(dt);
    UpdateCamera();
    UpdateWeapon(dt);
    UpdateZombies(dt);
    UpdateAlly(dt);
    UpdateDialog();
    if (Practice()) UpdatePractice(dt); else if (!NetIsClient()) UpdateRounds(dt);   // co-op: las rondas las lleva el anfitrión
    UpdateStory(dt);
    UpdateWorld(dt);
    UpdateProjectiles(dt);
    UpdateEffects(dt);
    UpdatePowerups(dt);
    UpdateInteract();
}

static void HandleInput() {
    Vector2 md = GetMouseDelta();
    if (ignoreMouse > 0) { ignoreMouse--; md = Vector2{ 0, 0 }; }   // evita el salto al capturar el ratón
    player.yaw -= md.x * CONFIG::sensibilidad * opts.sens * (1 - vm.ads * 0.4f);
    player.pitch = clampf(player.pitch - md.y * CONFIG::sensibilidad * opts.sens * (1 - vm.ads * 0.4f), -1.5f, 1.5f);
    vm.swayTX = clampf(vm.swayTX - md.x * 0.0003f, -0.03f, 0.03f);
    vm.swayTY = clampf(vm.swayTY + md.y * 0.0003f, -0.03f, 0.03f);
    mouseDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    mouseRight = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    if (player.downed) { mouseDown = mouseRight = false; return; }   // caído: solo mirar
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) mousePressedAt = T;
    if (IsKeyPressed(KEY_R)) StartReload();
    if (IsKeyPressed(KEY_E) && currentInteract && !currentInteract->hold) currentInteract->action();
    if (IsKeyPressed(KEY_G)) ThrowGrenade();
    if (IsKeyPressed(KEY_V) || IsMouseButtonPressed(MOUSE_BUTTON_SIDE)) Melee();   // cuchillo
    if (IsKeyPressed(KEY_M)) ToggleMusic();
    for (int k = 0; k < 3; k++)   // teclas 1, 2 y 3: elegir el espacio directamente
        if (IsKeyPressed(KEY_ONE + k) && k < (int)player.weapons.size() && player.cur != k && !player.papBusy) { player.cur = k; Equip(); }
    float wheel = GetMouseWheelMove();
    if (wheel > 0) SwitchWeapon(-1); else if (wheel < 0) SwitchWeapon(1);
}

std::vector<std::pair<int, Vector2>> GroundSpawnsImpl();
std::vector<std::pair<int, Vector2>> GroundSpawns() { return GroundSpawnsImpl(); }

static void LoadPowerupAssets() {
    for (int i = 0; i < 4; i++) {
        powerLabels[i] = MakeLabel(POWERS[i].shortName, Hex(POWERS[i].color), 60);
        powerMats[i] = M("fixture");
        powerMats[i].emis = Vector3Scale(ColorV(POWERS[i].color), 3.0f);
    }
}

static void LoadFonts() {
    std::vector<int> cps;
    for (int c = 32; c < 256; c++) cps.push_back(c);
    fontUI = LoadFontEx("assets/fonts/Oswald.ttf", 64, cps.data(), (int)cps.size());
    if (!fontUI.texture.id || fontUI.glyphCount == 0) fontUI = GetFontDefault();
    fontTitle = LoadFontEx("assets/fonts/Creepster.ttf", 140, cps.data(), (int)cps.size());
    if (!fontTitle.texture.id || fontTitle.glyphCount == 0) fontTitle = fontUI;
    SetTextureFilter(fontUI.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(fontTitle.texture, TEXTURE_FILTER_BILINEAR);
}

static void Loading(const char *msg) {
    BeginDrawing();
    ClearBackground(BLACK);
    float s = GetScreenHeight() / 900.0f;
    const char *t = CONFIG::titulo;
    Font f = fontTitle.texture.id ? fontTitle : GetFontDefault();
    Vector2 m = MeasureTextEx(f, t, 100 * s, 4);
    DrawTextEx(f, t, Vector2{ GetScreenWidth() / 2 - m.x / 2, GetScreenHeight() * 0.38f }, 100 * s, 4, Color{ 180, 0, 0, 255 });
    Font u = fontUI.texture.id ? fontUI : GetFontDefault();
    Vector2 mm = MeasureTextEx(u, msg, 24 * s, 1);
    DrawTextEx(u, msg, Vector2{ GetScreenWidth() / 2 - mm.x / 2, GetScreenHeight() * 0.38f + 150 * s }, 24 * s, 1, GRAY);
    Vector2 bm = MeasureTextEx(u, CONFIG::marca, 28 * s, 1);
    DrawTextEx(u, CONFIG::marca, Vector2{ GetScreenWidth() / 2 - bm.x / 2, GetScreenHeight() * 0.38f + 105 * s }, 28 * s, 1, Color{ 220, 220, 220, 255 });
    EndDrawing();
}

// ---------- Modo de prueba automática (--test mapa salida) ----------
// Prueba del campo de tiro: gratis, sin daño, dianas que reaparecen
static int RunPracticeTest(const char *outPrefix) {
    auto step = [](int n) { for (int i = 0; i < n; i++) Update(0.05f); };
    ResetGame("practica");
    EnableCursor();
    step(80);
    auto alive = []() { int n = 0; for (auto &z : zombies) if (z.dummy >= 0 && z.state != Z_DEAD) n++; return n; };
    printf("practica: dianas vivas %d\n", alive());
    player.health = 100; DamagePlayer(500); printf("vida tras golpe de 500: %.0f\n", player.health);
    int pts = player.points; bool ok = Spend(5000); printf("compra de 5000: %d, puntos %d -> %d\n", ok, pts, player.points);
    int taken = 0;
    for (auto &it : interactables) if (it.text().find("tomar") != std::string::npos) { it.action(); taken++; }
    printf("armas en la pared: %d, arma actual: %s\n", taken, WEAPONS[CurWeapon().id].name);
    // 3 espacios de armas
    player.weapons.clear(); GiveWeapon(W_PISTOL); GiveWeapon(W_SHOTGUN); GiveWeapon(W_SMG);
    printf("espacios: %d (actual %s)\n", (int)player.weapons.size(), WEAPONS[CurWeapon().id].name);
    GiveWeapon(W_RPG);
    printf("4a arma reemplaza la actual: %d espacios, %s/%s/%s\n", (int)player.weapons.size(), WEAPONS[player.weapons[0].id].name, WEAPONS[player.weapons[1].id].name, WEAPONS[player.weapons[2].id].name);
    // cuchillo con estocada contra la diana de (-4,-14)
    for (auto &z : zombies) if (z.dummy == 0 && z.state != Z_DEAD) {
        player.pos = Vector3{ -4, 0, -11.2f }; player.yaw = 0; player.pitch = -0.1f; player.vel = Vector3{ 0, 0, 0 };
        float hp0 = z.hp; Vector3 p0 = player.pos;
        Melee(); step(4);
        printf("cuchillo: vida diana %.0f -> %.0f, estocada %.2f m\n", hp0, z.hp, Vector3Distance(p0, player.pos));
        break;
    }
    step(20);
    GiveWeapon(W_RIFLE); step(10);
    for (int k = 0; k < 400; k++) {
        Zombie *best = nullptr; float bd = 1e9f;
        for (auto &z : zombies) if (z.state == Z_CHASE) { float d = Vector3Distance(z.pos, player.pos); if (d < bd) { bd = d; best = &z; } }
        if (best) {
            float dx = best->pos.x - player.pos.x, dz = best->pos.z - player.pos.z;
            player.yaw = atan2f(-dx, -dz);
            player.pitch = atan2f(best->headPos.y - 1.65f, hyp(dx, dz));
            mouseDown = true; mousePressedAt = T;
        } else mouseDown = false;
        step(2);
    }
    mouseDown = false;
    printf("bajas %d (cabeza %d), reserva %d, vida %.0f\n", game.kills, game.heads, CurWeapon().reserve, player.health);
    step(80);
    printf("dianas vivas tras reaparecer: %d, ronda %d\n", alive(), game.round);
    RenderTexture2D cap = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    captureTarget = &cap;
    auto shot = [&](const char *name) {
        UpdateCamera();
        for (int i = 0; i < 3; i++) RenderFrame(false);
        Image img = LoadImageFromTexture(cap.texture);
        ImageFlipVertical(&img);
        ExportImage(img, TextFormat("%s_%s.png", outPrefix, name));
        UnloadImage(img);
    };
    float yw; Vector2 st = MapStart(yw);
    player.pos = Vector3{ st.x, 0, st.y }; player.yaw = yw; player.pitch = -0.02f; vm.swap = 0;
    step(2); shot("pista");
    player.pos = Vector3{ 0, 0, 1 }; player.yaw = PI_F / 2 * 0.75f; player.pitch = 0.05f; step(2); shot("armas_izq");
    player.yaw = -PI_F / 2 * 0.75f; step(2); shot("armas_der");
    player.pos = Vector3{ 0, 0, -1 }; player.yaw = PI_F; player.pitch = 0.05f; step(2); shot("fondo");
    // perro del infierno de cerca
    for (auto &z : zombies) if (z.dog && z.state == Z_CHASE) {
        player.pos = Vector3{ z.pos.x + 0.5f, 0, z.pos.z + 3.2f }; player.yaw = 0.15f; player.pitch = -0.25f;
        GiveWeapon(W_SUOMI); step(10); vm.swap = 0;
        shot("perro");
        printf("perro: vida %.0f, escala %.2f, cabeza y=%.2f lomo y=%.2f\n", z.hp, z.scale, z.headPos.y, z.hipsPos.y);
        break;
    }
    // Cañón de Trueno contra las dianas
    {
        player.pos = Vector3{ 0, 0, -8 }; player.yaw = 0; player.pitch = -0.05f;
        GiveWeapon(W_THUNDER); step(12); vm.swap = 0;
        int k0 = game.kills;
        mouseDown = true; mousePressedAt = T; Update(0.016f); mouseDown = false;
        for (int i = 0; i < 6; i++) Update(0.033f);
        int flying = 0; for (auto &z : zombies) if (z.state == Z_DEAD && z.y > 0.3f) flying++;
        shot("trueno");
        printf("trueno: bajas %d, volando %d\n", game.kills - k0, flying);
        step(40);
    }
    // pistola de bengalas
    {
        step(80);
        player.pos = Vector3{ 0, 0, -10.5f };
        Zombie *tz = nullptr;
        for (auto &z : zombies) if (z.dummy >= 0 && z.state == Z_CHASE && !z.dog && (!tz || z.pos.z > tz->pos.z)) tz = &z;
        if (tz) {
            float dx = tz->pos.x - player.pos.x, dz = tz->pos.z - player.pos.z;
            player.yaw = atan2f(-dx, -dz); player.pitch = atan2f(0.9f - 1.65f, hyp(dx, dz));
        }
        GiveWeapon(W_FLARE); step(12); vm.swap = 0;
        mouseDown = true; mousePressedAt = T; Update(0.016f); mouseDown = false;
        for (int i = 0; i < 20; i++) Update(0.033f);
        int burning = 0; for (auto &z : zombies) if (z.burnT > 0) burning++;
        shot("bengala");
        printf("bengala: ardiendo %d\n", burning);
    }
    // muñecos de trapo: escopetazo a quemarropa y explosión
    {
        step(80);
        Zombie *tz = nullptr;
        for (auto &z : zombies) if (z.dummy >= 0 && z.state == Z_CHASE && !z.dog && z.dummy < 7) { tz = &z; break; }
        if (tz) {
            player.pos = Vector3{ tz->pos.x + 0.3f, 0, tz->pos.z + 2.6f };
            float dx = tz->pos.x - player.pos.x, dz = tz->pos.z - player.pos.z;
            player.yaw = atan2f(-dx, -dz); player.pitch = -0.12f;
            GiveWeapon(W_SHOTGUN); step(12); vm.swap = 0;
            Vector3 p0 = tz->pos;
            int idx = (int)(tz - &zombies[0]);
            for (int s = 0; s < 3 && zombies[idx].state != Z_DEAD; s++) { mouseDown = true; mousePressedAt = T; Update(0.016f); mouseDown = false; for (int i = 0; i < 18; i++) Update(0.05f); }
            for (int i = 0; i < 8; i++) Update(0.033f);
            shot("ragdoll1");
            for (int i = 0; i < 25; i++) Update(0.033f);
            Zombie &z = zombies[idx];
            printf("ragdoll escopeta: muerto %d, ragdoll %d, cadera y=%.2f, se movió %.2f m\n", (int)(z.state == Z_DEAD), (int)(bool)z.rag, z.hipsPos.y, hyp(z.hipsPos.x - p0.x, z.hipsPos.z - p0.z));
            shot("ragdoll2");
        }
        // explosión entre varias dianas
        step(80);
        int k0 = game.kills;
        Explode(Vector3{ 0, 0.3f, -27 }, 5000, 6, 0, 1.5f);
        for (int i = 0; i < 10; i++) Update(0.033f);
        float maxY = 0; int rags = 0;
        for (auto &z : zombies) if (z.rag) { rags++; maxY = std::max(maxY, z.hipsPos.y); }
        player.pos = Vector3{ 0, 0, -18 }; player.yaw = 0; player.pitch = -0.1f;
        shot("ragdoll3");
        printf("ragdoll explosión: bajas %d, ragdolls %d, cadera más alta %.2f m\n", game.kills - k0, rags, maxY);
        // cajas y barriles
        std::vector<Vector3> before;
        for (int i = 0; i < DynPropCount(); i++) before.push_back(DynPropPos(i));
        Explode(Vector3{ 5.5f, 0.3f, -22 }, 0, 5, 0, 1.2f);
        float maxUp = 0;
        for (int i = 0; i < 12; i++) { Update(0.033f); for (int k = 0; k < DynPropCount(); k++) maxUp = std::max(maxUp, DynPropPos(k).y); }
        step(60);
        for (int i = 0; i < DynPropCount(); i++) printf("objeto %d: (%.1f,%.1f) -> (%.1f,%.1f)\n", i, before[i].x, before[i].z, DynPropPos(i).x, DynPropPos(i).z);
        printf("altura máxima de un objeto: %.2f\n", maxUp);
        // el jugador empuja una caja caminando
        Vector3 c0 = DynPropPos(0);
        player.pos = Vector3{ c0.x, 0, c0.z + 1.4f }; player.vel = Vector3{ 0, 0, -5 }; player.yaw = 0;
        Vector3 cb = DynPropPos(0);
        for (int i = 0; i < 20; i++) { player.vel = Vector3{ 0, 0, -5 }; player.pos.z -= 5 * 0.05f; Update(0.05f); }
        printf("empujar caja: (%.2f,%.2f) -> (%.2f,%.2f)\n", cb.x, cb.z, DynPropPos(0).x, DynPropPos(0).z);
    }
    // armas nuevas en primera persona
    {
        player.pos = Vector3{ 0, 0, 1 }; player.yaw = 0; player.pitch = -0.05f;
        int ids[] = { W_SUOMI, W_FLARE, W_MAGNUM, W_SAWED, W_THUNDER };
        for (int id : ids) { GiveWeapon(id); step(12); vm.swap = 0; vm.kick = 0; shot(TextFormat("nueva%d", id)); }
    }
    for (int i = 0; i < 3; i++) {   // cuchillo a mitad del tajo
        GiveWeapon(W_RIFLE); step(10); vm.swap = 0; player.nextKnife = 0;
        Melee(); for (int k = 0; k < 2 + i * 2; k++) Update(0.033f);
        shot(TextFormat("cuchillo%d", i));
    }
    captureTarget = nullptr;
    game.state = GS_MENU; BuildMap("bunker"); PowerOnInstant();
    {
        RenderTexture2D mc = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
        captureTarget = &mc;
        cam.position = Vector3{ 2, 1.8f, 7 }; cam.target = Vector3{ 0, 1.4f, -6 }; cam.up = Vector3{ 0, 1, 0 }; camFov = 70;
        for (int i = 0; i < 3; i++) RenderFrame(true);
        Image img = LoadImageFromTexture(mc.texture);
        ImageFlipVertical(&img);
        ExportImage(img, TextFormat("%s_menu.png", outPrefix));
        UnloadImage(img);
        captureTarget = nullptr;
    }
    printf("TEST OK\n");
    return 0;
}

static int RunTest(const char *mapKey, const char *outPrefix) {
    auto step = [](int n) { for (int i = 0; i < n; i++) Update(0.05f); };
    {
        // captura del menú principal
        RenderTexture2D mc = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
        captureTarget = &mc;
        cam.position = Vector3{ 2, 1.8f, 7 }; cam.target = Vector3{ 0, 1.4f, -6 }; cam.up = Vector3{ 0, 1, 0 }; camFov = 70;
        for (int i = 0; i < 3; i++) RenderFrame(true);
        Image img = LoadImageFromTexture(mc.texture);
        ImageFlipVertical(&img);
        ExportImage(img, TextFormat("%s_menu.png", outPrefix));
        UnloadImage(img);
        optionsOpen = true;
        for (int i = 0; i < 2; i++) RenderFrame(true);
        img = LoadImageFromTexture(mc.texture);
        ImageFlipVertical(&img);
        ExportImage(img, TextFormat("%s_opciones.png", outPrefix));
        UnloadImage(img);
        optionsOpen = false;
        captureTarget = nullptr;
    }
    ResetGame(mapKey); game.state = GS_PLAYING;   // sin cinemática
    EnableCursor();
    player.points = 99999; game.betweenT = 0.1f;
    printf("walls:%d win:%d cols:%d inter:%d lights:%d\n", (int)mmWalls.size(), (int)windows.size(), (int)colliders.size(), (int)interactables.size(), (int)lights.size());
    for (auto &it : interactables) if (it.text().find("puerta") != std::string::npos) it.action();
    int un = 0; for (size_t i = 0; i < rooms.size(); i++) un += Unlocked((int)i);
    printf("unlocked:%d/%d\n", un, (int)rooms.size());
    // corriente: sin ella no se puede beber (salvo Quick Revive) ni usar el Pack-a-Punch
    {
        unsigned p0 = player.perks;
        for (auto &it : interactables) if (it.text().find("beber JUGGERNOG") != std::string::npos) it.action();
        std::string papTxt;
        for (auto &it : interactables) if (it.text().find("corriente") != std::string::npos && it.text().find("Primero") != std::string::npos) papTxt = it.text();
        printf("sin corriente: power=%d, juggernog comprado=%d, aviso='%s'\n", (int)powerOn, (int)(player.perks != p0), papTxt.c_str());
        for (auto &it : interactables) if (it.text().find("Presiona E para activar la corriente") != std::string::npos) { it.action(); break; }
        step(50);
        printf("con corriente: power=%d\n", (int)powerOn);
    }
    printf("sonidos reales: pistola %d ak %d recarga %d zombi %d\n", HasSound("g_pistol"), HasSound("g_ak47"), HasSound("reload_rifle"), HasSound("groan"));
    player.health = 1e9f; player.maxHealth = 1e9f;
    for (int k = 0; k < 600; k++) {
        Zombie *best = nullptr; float bd = 1e9f;
        for (auto &z : zombies) if (z.state == Z_CHASE) { float d = Vector3Distance(z.pos, player.pos); if (d < bd) { bd = d; best = &z; } }
        if (best) {
            float dx = best->pos.x - player.pos.x, dz = best->pos.z - player.pos.z;
            player.yaw = atan2f(-dx, -dz);
            player.pitch = atan2f(best->headPos.y - 1.65f, hyp(dx, dz));
            mouseDown = true; mousePressedAt = T;
        } else mouseDown = false;
        if (CurWeapon().reserve < 10) CurWeapon().reserve = 200;
        step(2);
    }
    mouseDown = false;
    std::string st;
    for (auto &z : zombies) st += "ATCRHD"[z.state];
    printf("round:%d kills:%d heads:%d zombies:%d states:%s\n", game.round, game.kills, game.heads, (int)zombies.size(), st.c_str());
    std::string bs; for (auto &w : windows) bs += (char)('0' + w.boards);
    printf("boards:%s\n", bs.c_str());
    // reparar
    for (auto &w : windows) if (w.boards < 5) {
        player.pos = Vector3{ w.inside.x, 0, w.inside.y };
        for (int i = 0; i < 30; i++) { Update(0.05f); if (currentInteract && currentInteract->hold) currentInteract->action(); }
        printf("repaired:%d\n", w.boards);
        break;
    }
    // granada y caja
    ThrowGrenade(); step(60); printf("grenades:%d\n", player.grenades);
    player.pos = Vector3{ box.front.x, 0, box.front.y }; step(1);
    if (currentInteract) currentInteract->action();
    step(90);
    printf("box:%d offer:%d\n", box.state, box.offer);
    if (currentInteract) currentInteract->action();
    step(2);
    std::string ws; for (auto &w : player.weapons) ws += std::string(WEAPONS[w.id].name) + ",";
    printf("weapons:%s\n", ws.c_str());
    // Pack-a-Punch
    player.pos = Vector3{ pap.front.x, 0, pap.front.y }; step(1);
    if (currentInteract) { printf("papPrompt:%s\n", currentInteract->text().c_str()); currentInteract->action(); }
    step(80);
    printf("pap:%d %s\n", (int)CurWeapon().pap, Stats(CurWeapon().id, CurWeapon().pap).name);
    // bebidas
    for (auto &it : interactables) if (it.text().find("beber") != std::string::npos) it.action();
    printf("perks:%u\n", player.perks);
    // lanzamisiles
    GiveWeapon(W_RPG); step(10);
    int k0 = game.kills;
    for (int k = 0; k < 200; k++) {
        Zombie *best = nullptr; float bd = 1e9f;
        for (auto &z : zombies) if (z.state == Z_CHASE) { float d = Vector3Distance(z.pos, player.pos); if (d < bd) { bd = d; best = &z; } }
        if (best) { float dx = best->pos.x - player.pos.x, dz = best->pos.z - player.pos.z; player.yaw = atan2f(-dx, -dz); player.pitch = atan2f(1.0f - 1.65f, hyp(dx, dz)); mouseDown = true; mousePressedAt = T; }
        else mouseDown = false;
        if (CurWeapon().reserve < 5) CurWeapon().reserve = 20;
        step(2);
    }
    mouseDown = false;
    printf("rpg kills:%d round:%d\n", game.kills - k0, game.round);
    // ronda de perros: se fuerza la siguiente ronda y se dispara a los perros
    {
        for (auto &z : zombies) z.remove = true;
        step(1);
        game.toSpawn = 0; game.between = true; game.betweenT = 0.1f; game.nextDogRound = game.round + 1;
        GiveWeapon(W_SUOMI); step(10);
        float yw; Vector2 st = MapStart(yw);
        player.pos = Vector3{ st.x, 0, st.y };
        int dogsSeen = 0, dogKills = 0, maxDogs = 0;
        for (int k = 0; k < 2400 && (game.dogRound || k < 5); k++) {
            int n = 0; Zombie *best = nullptr; float bd = 1e9f;
            for (auto &z : zombies) if (z.dog && z.state != Z_DEAD) { n++; if (z.state == Z_CHASE) { float d = Vector3Distance(z.pos, player.pos); if (d < bd) { bd = d; best = &z; } } }
            maxDogs = std::max(maxDogs, n);
            if (best) {
                float dx = best->pos.x - player.pos.x, dz = best->pos.z - player.pos.z;
                player.yaw = atan2f(-dx, -dz);
                Vector3 c = Vector3Lerp(best->hipsPos, best->neckPos, 0.5f);
                player.pitch = atan2f(c.y - 1.65f, hyp(dx, dz));
                mouseDown = true; mousePressedAt = T;
            } else mouseDown = false;
            if (CurWeapon().reserve < 50) CurWeapon().reserve = 284;
            int before = game.kills;
            step(1);
            if (game.kills > before) dogKills += game.kills - before;
            dogsSeen = std::max(dogsSeen, game.dogRounds);
        }
        mouseDown = false;
        for (auto &z : zombies)
            if (z.dog && z.state != Z_DEAD) printf("  perro vivo en (%.1f, %.1f) sala %d estado %d, jugador en sala %d\n", z.pos.x, z.pos.z, z.room, z.state, player.room);
        printf("perros: ronda de perros %d, a la vez max %d, perros eliminados %d, sigue la ronda de perros %d, vida %.0f, ronda %d\n",
               dogsSeen, maxDogs, dogKills, (int)game.dogRound, player.health, game.round);
        player.health = 1e9f;
    }

    // capturas
    RenderTexture2D cap = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    captureTarget = &cap;
    auto shot = [&](const char *name) {
        UpdateCamera();
        for (int i = 0; i < 3; i++) RenderFrame(false);
        Image img = LoadImageFromTexture(cap.texture);
        ImageFlipVertical(&img);
        ExportImage(img, TextFormat("%s_%s.png", outPrefix, name));
        UnloadImage(img);
    };
    // 1) frente a una ventana con zombis entrando
    for (auto &z : zombies) z.remove = true;
    step(1);
    game.zHP = 1e9f; game.toSpawn = 0; game.between = true; game.betweenT = 999;
    player.maxHealth = 100; player.health = 100; player.lastHit = -99;
    Window &w = windows[0];
    for (int i = 0; i < 5; i++) { w.boardVis[i] = i < 2; w.boardAnim[i] = -1; w.boardPos[i] = w.boardBase[i]; w.boardSpin[i] = 0; }
    w.boards = 2;
    player.pos = Vector3{ w.inside.x + w.nx * 4.5f + w.nz * 1.0f, 0, w.inside.y + w.nz * 4.5f + w.nx * 1.0f };
    float dx = w.x - player.pos.x, dz = w.z - player.pos.z;
    player.yaw = atan2f(-dx, -dz); player.pitch = -0.04f;
    zombies.push_back(Zombie());
    {
        Zombie &z = zombies.back();
        z.pos = Vector3{ w.outside.x, 0, w.outside.y }; z.state = Z_TEAR; z.win = 0; z.hp = 1e9f; z.speed = 1.2f; z.runner = false;
        z.nextAttack = 1e9f; z.tearT = 1e9f; z.scale = 1; z.clip = C_ATTACK; z.yaw = atan2f((float)w.nx, (float)w.nz);
    }
    zombies.push_back(Zombie());
    {
        Zombie &z = zombies.back();
        z.pos = Vector3{ w.inside.x + w.nx * 2.0f - w.nz * 1.3f, 0, w.inside.y + w.nz * 2.0f + w.nx * 1.3f }; z.state = Z_CHASE; z.hp = 1e9f; z.speed = 0.01f;
        z.nextAttack = 1e9f; z.scale = 1; z.clip = C_WALK; z.room = RoomIndex(w.room);
    }
    GiveWeapon(W_RIFLE); vm.swap = 0;
    step(12);
    vm.swap = 0;
    for (auto &z : zombies) z.nextAttack = 1e9f;
    shot("ventana");
    // 2) disparo de lanzamisiles por el pasillo
    GiveWeapon(W_RPG); step(10); vm.swap = 0;
    float yaw; Vector2 s = MapStart(yaw);
    player.pos = Vector3{ s.x, 0, s.y }; player.yaw = 0; player.pitch = 0;
    for (int i = 0; i < 4; i++) {
        zombies.push_back(Zombie());
        Zombie &z = zombies.back();
        z.pos = Vector3{ s.x + frand(-2, 2), 0, s.y - 9 - frand(0, 2) }; z.state = Z_CHASE; z.hp = 1e9f; z.speed = 0.01f;
        z.nextAttack = 1e9f; z.scale = 1; z.clip = C_WALK; z.clipT = frand(0, 3); z.room = startRoom; z.yaw = PI_F * 0 ;
    }
    step(4);
    mouseDown = true; mousePressedAt = T; step(1); mouseDown = false; step(2);
    shot("misil");
    // 3) cámara mirando la caja misteriosa
    player.pos = Vector3{ box.front.x, 0, box.front.y };
    dx = box.pos.x - player.pos.x; dz = box.pos.y - player.pos.z;
    player.yaw = atan2f(-dx, -dz); player.pitch = -0.3f;
    box.state = BOX_IDLE; for (auto &it : interactables) (void)it;
    player.points = 5000;
    step(1);
    if (currentInteract) currentInteract->action();
    step(30);
    shot("caja");
    // 4) un zombi de cerca bajo la luz
    {
        for (auto &z : zombies) z.remove = true;
        step(1);
        float yw; Vector2 st = MapStart(yw);
        player.pos = Vector3{ st.x, 0, st.y }; player.yaw = 0; player.pitch = 0.02f;
        zombies.push_back(Zombie());
        Zombie &z = zombies.back();
        z.pos = Vector3{ st.x + 0.3f, 0, st.y - 2.3f }; z.state = Z_CHASE; z.hp = 1e9f; z.speed = 0.01f;
        z.nextAttack = 1e9f; z.scale = 1; z.clip = C_IDLE; z.room = startRoom; z.yaw = 0;
        GiveWeapon(W_SHOTGUN); step(10); vm.swap = 0;
        shot("zombi");
    }
    // 5) desde fuera, mirando una ventana (zombi acercándose)
    {
        Window &w0 = windows[0];
        player.pos = Vector3{ w0.x - w0.nx * 7.0f, 0, w0.z - w0.nz * 7.0f };
        dx = w0.x - player.pos.x; dz = w0.z - player.pos.z;
        player.yaw = atan2f(-dx, -dz); player.pitch = 0.0f;
        step(1);
        shot("fuera");
    }
    // 6) deslizamiento desde el punto de inicio
    {
        for (auto &z : zombies) z.remove = true;
        step(1);
        float yw; Vector2 st = MapStart(yw);
        player.pos = Vector3{ st.x, 0, st.y }; player.yaw = yw; player.pitch = 0; player.vel = Vector3{ 0, 0, 0 };
        GiveWeapon(W_RIFLE); step(10); vm.swap = 0;
        Vector3 p0 = player.pos;
        player.slideDir = Vector3{ -sinf(yw), 0, -cosf(yw) }; player.slideT = SLIDE_TIME;
        for (int i = 0; i < 6; i++) Update(0.05f);
        float mid = Vector3Distance(p0, player.pos);
        printf("slide mid: eye=%.2f roll=%.3f dist=%.2f\n", player.eye, vm.roll, mid);
        shot("desliz");
        for (int i = 0; i < 30; i++) Update(0.05f);
        printf("slide end: slideT=%.2f dist=%.2f eye=%.2f\n", player.slideT, Vector3Distance(p0, player.pos), player.eye);
    }
    // 7) cada arma en primera persona (y algunas apuntando / con Pack-a-Punch)
    {
        float yw; Vector2 st = MapStart(yw);
        player.pos = Vector3{ st.x, 0, st.y }; player.yaw = yw; player.pitch = -0.05f;
        for (int id = 0; id < W_COUNT; id++) {
            GiveWeapon(id); step(10); vm.swap = 0; vm.kick = 0;
            shot(TextFormat("arma%d", id));
        }
        int adsIds[] = { W_PISTOL, W_RIFLE, W_SHOTGUN };
        mouseRight = true;
        for (int id : adsIds) { GiveWeapon(id); step(14); vm.swap = 0; shot(TextFormat("mira%d", id)); }
        mouseRight = false;
        GiveWeapon(W_SMG); CurWeapon().pap = true; step(10); vm.swap = 0; shot("pap");
        ThrowGrenade(); step(5); shot("granada");
    }
    // 8) el interruptor de la corriente
    {
        Vector3 sp, eye;
        if (PowerSwitchView(sp, eye)) {
            player.pos = eye; step(1);
            float dx = sp.x - player.pos.x, dz = sp.z - player.pos.z;
            player.yaw = atan2f(-dx, -dz); player.pitch = atan2f(sp.y - 1.65f, hyp(dx, dz));
            GiveWeapon(W_PISTOL); step(10); vm.swap = 0;
            shot("interruptor");
        }
    }
    // 9) ronda de perros: niebla y perros corriendo hacia el jugador
    {
        for (auto &z : zombies) z.remove = true;
        step(1);
        float yw; Vector2 st = MapStart(yw);
        player.pos = Vector3{ st.x, 0, st.y }; player.yaw = 0; player.pitch = -0.08f;
        game.dogRound = true; game.fogK = 1; game.between = false; game.toSpawn = 0;
        for (int i = 0; i < 3; i++) {
            zombies.push_back(Zombie());
            Zombie &z = zombies.back();
            z.dog = true; z.state = Z_CHASE; z.hp = 1e9f; z.speed = 0.01f; z.scale = 0.35f; z.runner = true;
            z.pos = Vector3{ st.x - 1.6f + i * 1.6f, 0, st.y - 4.5f - i * 1.2f }; z.yaw = 0; z.clip = C_RUN; z.clipT = i * 0.2f; z.room = startRoom;
            z.nextAttack = 1e9f;
        }
        GiveWeapon(W_THUNDER); step(3); vm.swap = 0;
        for (auto &z : zombies) z.speed = 0.01f;
        shot("perros");
        game.dogRound = false; game.fogK = 0;
    // 10) fin de partida con récord nuevo (el archivo de récords de la prueba se borra después)
    {
        game.round = 7; game.kills = 123; game.heads = 45;
        GameOver();
        printf("récord nuevo: %d, guardado: ronda %d\n", (int)newRecord, GetRecord(mapKey).round);
        shot("fin");
        remove(recordsFile);
    }
    }
    captureTarget = nullptr;
    printf("TEST OK\n");
    return 0;
}

// ---------- Capturas desde varios puntos de un mapa (--shots mapa prefijo) ----------
static int RunMapShots(const char *mapKey, const char *outPrefix) {
    RenderTexture2D rt = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    ResetGame(mapKey); game.state = GS_PLAYING;
    PowerOnInstant();
    for (auto &it : interactables) if (it.text().find("puerta") != std::string::npos) it.action();
    player.points = 0;
    struct View { float x, z, yaw, pitch; const char *name; };
    const View views[] = { { 0, 12, 0, 0.05f, "plaza" }, { 0, 12, 0.6f, 0.75f, "cielo" }, { 0, -20, 0, 0, "calle" },
                           { 38, 0, -1.57f, 0, "cementerio" }, { 30, 0, 1.57f, 0, "iglesia" }, { -12, 22, -2.3f, 0.02f, "estacion" }, { -20, -20, 2.4f, 0, "taller" }, { -6, 10, 0.9f, 0.15f, "arbol" }, { 45, 5, 2.75f, 0.12f, "arbolseco" } };
    std::vector<View> list(views, views + sizeof(views) / sizeof(views[0]));
    static std::vector<std::string> names;
    if (strcmp(mapKey, "pueblo")) {   // otro mapa: una vista por sala, desde una esquina hacia el centro
        list.clear(); names.clear();
        for (auto &r : rooms) names.push_back("sala" + r.id);
        for (size_t i = 0; i < rooms.size(); i++) {
            const Room &r = rooms[i];
            float x = r.x1 + 1.6f, z = r.z1 + 1.6f, cx = (r.x1 + r.x2) / 2, cz = (r.z1 + r.z2) / 2;
            list.push_back({ x, z, atan2f(-(cx - x), -(cz - z)), 0.0f, names[i].c_str() });
        }
    }
    for (auto &v : list) {
        player.pos = Vector3{ v.x, 0, v.z }; player.yaw = v.yaw; player.pitch = v.pitch; player.vel = Vector3{ 0, 0, 0 };
        for (int i = 0; i < 10; i++) Update(0.05f);
        player.yaw = v.yaw; player.pitch = v.pitch;
        Update(0.05f);
        captureTarget = &rt;
        for (int i = 0; i < 2; i++) RenderFrame(false);
        captureTarget = nullptr;
        Image img = LoadImageFromTexture(rt.texture);
        ImageFlipVertical(&img);
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
        ExportImage(img, TextFormat("%s_%s.png", outPrefix, v.name));
        UnloadImage(img);
    }
    printf("capturas de %s listas; zombis: %d\n", mapKey, (int)zombies.size());
    return 0;
}

// ---------- Prueba del co-op en esta PC (--net host mapa prefijo / --net join ip prefijo) ----------
// Dos copias del juego: el anfitrión se queda quieto y el invitado camina y dispira a los zombis.
static int RunNetTest(bool host, const char *arg, const char *outPrefix) {
    NetLobbyClick(8);
    if (host) {
        NetLobbyClick(20);
        int m = !strcmp(arg, "fabrica") ? 1 : !strcmp(arg, "pueblo") ? 2 : 0;
        NetLobbyClick(22 + m);
    } else {
        NetLobbyClick(21);
        NetJoinIP() = arg;
        NetLobbyClick(25);
    }
    double t0 = GetTime(), lastPrint = 0;
    bool didDown = false, didRevive = false, shot = false;
    float simT = 0;
    while (GetTime() - t0 < 45) {
        float dt = 1.0f / 60;
        NetUpdate(dt);
        std::string mp;
        if (NetTakeStart(mp)) { ResetGame(mp); game.state = GS_PLAYING; soldierView.visible = false; printf("[%s] empieza en %s\n", LocalName(), mp.c_str()); }
        if (game.state == GS_PLAYING) {
            simT += dt;
            if (!host) {   // Milton camina en círculo y dispara al zombi más cercano
                player.pos.x += cosf(simT * 0.7f) * 3 * dt; player.pos.z += sinf(simT * 0.7f) * 3 * dt;
                Zombie *best = nullptr; float bd = 1e9f;
                for (auto &z : zombies) if (z.state == Z_CHASE) { float d = Vector3Distance(z.pos, player.pos); if (d < bd) { bd = d; best = &z; } }
                if (best) { float dx = best->pos.x - player.pos.x, dz = best->pos.z - player.pos.z; player.yaw = atan2f(-dx, -dz); player.pitch = atan2f(best->headPos.y - 1.65f, hyp(dx, dz)); mouseDown = true; mousePressedAt = T; }
                else mouseDown = false;
                if (CurWeapon().reserve < 10) CurWeapon().reserve = 200;
                if (simT > (getenv("UR_DIE") ? 8 : 22) && !didDown) { didDown = true; player.health = 5; DamagePlayer(40); printf("[MILTON] cae: downed=%d\n", (int)player.downed); }
            } else {
                player.health = 1e9f; player.maxHealth = 1e9f;   // Frank no muere en la prueba
                if (remote.downed && !didRevive && simT > 25 && !getenv("UR_DIE")) { didRevive = true; NetSendRevive(); printf("[FRANK] levanta a Milton\n"); }
                // mira hacia el compañero para la captura
                float dx = remote.pos.x - player.pos.x, dz = remote.pos.z - player.pos.z;
                if (remote.present) { player.yaw = atan2f(-dx, -dz); player.pitch = -0.1f; }
            }
            Update(dt);
        }
        if (game.state != GS_MENU) RenderFrame(false);
        if (GetTime() - lastPrint > 3 && game.state != GS_MENU) {
            lastPrint = GetTime();
            int alive = 0; for (auto &z : zombies) if (z.state != Z_DEAD) alive++;
            printf("[%s] t=%.0f compañero=%d (%.1f,%.1f) caído=%d ping=%d | ronda %d zombis vivos %d | mis puntos %d bajas %d | sus puntos %d | yo caído=%d\n",
                   LocalName(), simT, (int)remote.present, remote.pos.x, remote.pos.z, (int)remote.downed, NetPing(), game.round, alive, player.points, game.kills, remote.points, (int)player.downed);
            fflush(stdout);
        }
        if (host && !shot && simT > 18 && remote.present) {
            shot = true;
            RenderTexture2D rt = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
            captureTarget = &rt;
            for (int i = 0; i < 2; i++) RenderFrame(false);
            captureTarget = nullptr;
            Image img = LoadImageFromTexture(rt.texture);
            ImageFlipVertical(&img);
            ExportImage(img, TextFormat("%s_coop.png", outPrefix));
            UnloadImage(img);
        }
        if (game.state == GS_OVER) { printf("[%s] fin de la partida\n", LocalName()); break; }
    }
    NetShutdown();
    return 0;
}

// ---------- Subirse a las cajas (--climb bunker x) ----------
static int RunClimbTest() {
    ResetGame("bunker"); game.state = GS_PLAYING;
    auto step = [](int n) { for (int i = 0; i < n; i++) Update(0.05f); };
    // caja de 1.2 m en (-5, 4): llegamos saltando hacia ella
    player.pos = Vector3{ -3.95f, 1.0f, 4 }; player.vel = Vector3{ -5, 0, 0 }; player.onGround = false;
    bool m = TryMantle(player.pos, player.vel, 0.4f);
    player.vel = Vector3{ 0, 0, 0 }; player.vy = 0; player.onGround = true;
    step(20);
    printf("trepar: %d  altura tras 1 s encima: %.2f (esperado 1.20)\n", (int)m, player.pos.y);
    player.pos.x = -2.5f; step(20);
    printf("al bajarse: %.2f (esperado 0.00)\n", player.pos.y);
    player.pos = Vector3{ -3.95f, 0.2f, 4 }; Vector3 p = player.pos;
    bool high = TryMantle(p, Vector3{ -5, 0, 0 }, 0.4f);
    printf("desde el suelo sin saltar no trepa: %d (esperado 0)\n", (int)high);
    // la caja misteriosa
    Vector3 bp{ box.pos.x + (box.front.x - box.pos.x) * 0.6f, 0.5f, box.pos.y + (box.front.y - box.pos.y) * 0.6f };
    Vector3 bv{ (box.pos.x - box.front.x) * 5, 0, (box.pos.y - box.front.y) * 5 };
    bool bm = TryMantle(bp, bv, 0.4f);
    printf("caja misteriosa: %d altura %.2f (esperado 1 y 0.85)\n", (int)bm, bp.y);
    return 0;
}

// ---------- Capturas de la cinemática de llegada (--intro mapa prefijo) ----------
static int RunIntroTest(const char *mapKey, const char *outPrefix) {
    RenderTexture2D rt = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    ResetGame(mapKey);
    const float shots[] = { 1.5f, 4.5f, 8.5f, 10.5f, 12.8f, 14.0f };
    float t = 0; int si = 0;
    while (game.state == GS_INTRO && si < 6) {
        UpdateIntro(0.05f); UpdateWorld(0.05f); t += 0.05f;
        if (t >= shots[si]) {
            captureTarget = &rt;
            for (int i = 0; i < 2; i++) RenderFrame(false);
            captureTarget = nullptr;
            Image img = LoadImageFromTexture(rt.texture);
            ImageFlipVertical(&img);
            ExportImage(img, TextFormat("%s_intro%d.png", outPrefix, si));
            UnloadImage(img);
            si++;
        }
    }
    printf("intro %s: %d capturas, estado final %d\n", mapKey, si, game.state);
    return 0;
}

// ---------- Prueba de los huevos de pascua (--secretos mapa salida) ----------
static int RunEggTest(const char *mapKey, const char *outPrefix) {
    RenderTexture2D cap = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    auto shot = [&](const char *nameIn) {
        std::string name = nameIn;
        captureTarget = &cap;
        for (int i = 0; i < 2; i++) RenderFrame(false);
        captureTarget = nullptr;
        Image img = LoadImageFromTexture(cap.texture);
        ImageFlipVertical(&img);
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
        ExportImage(img, TextFormat("%s_%s.png", outPrefix, name.c_str()));
        UnloadImage(img);
    };
    opts.ally = true;
    ResetGame(mapKey); game.state = GS_PLAYING; soldierView.visible = false;
    player.maxHealth = player.health = 1e7f;
    for (int i = 0; i < (int)links.size(); i++) OpenLinkNow(i);
    int teddies = 0, radios = 0, shots = 0;
    for (auto &it : interactables) {
        if (!it.active()) continue;
        std::string tx = it.text();
        bool ted = tx.find("osito") != std::string::npos, rad = tx.find("la radio") != std::string::npos;
        if (!ted && !rad) continue;
        Vector2 p = it.pos();
        bool inside = InsideCollider(p.x, p.y, 0.2f);
        printf("%s en (%.1f, %.1f) sala %d%s\n", ted ? "osito" : "radio", p.x, p.y, RoomAt(p.x, p.y), inside ? " ¡DENTRO DE UNA CAJA!" : "");
        // mirarlo de cerca
        player.pos = Vector3{ p.x + 1.2f, 0, p.y + 0.6f };
        player.yaw = atan2f(1.2f, 0.6f); player.pitch = -0.5f;
        for (int k = 0; k < 4; k++) Update(0.05f);
        player.yaw = atan2f(1.2f, 0.6f); player.pitch = -0.5f; Update(0.05f);
        if (shots < 2) { std::string nm = ted ? "osito" : "radio"; shot((nm + std::to_string(shots++)).c_str()); }
        it.action();
        if (ted) teddies++; else radios++;
    }
    for (int k = 0; k < 400; k++) Update(0.05f);   // que suenen las frases
    printf("ositos %d, radios %d; arma: %s (pap %d); Milton rosa: %d; grabaciones guardadas: %d\n", teddies, radios, Stats(CurWeapon().id, CurWeapon().pap).name,
           (int)CurWeapon().pap, (int)EggRosa(), EggRadiosFound());
    // truco CABEZON y charla con Milton
    bigHeads = true;
    for (int k = 0; k < 20; k++) SpawnZombie();
    for (int k = 0; k < 200; k++) Update(0.05f);
    for (auto &z : zombies) if (z.state == Z_CHASE) { player.pos = Vector3{ z.pos.x + 2.5f, 0, z.pos.z }; player.yaw = atan2f(2.5f, 0); player.pitch = 0.0f; break; }
    for (int k = 0; k < 2; k++) Update(0.05f);
    shot("cabezon");
    return 0;
}

// ---------- Armas nuevas, carteles de tiza, caja con osito y explosiones (--armas salida) ----------
static int RunArmasTest(const char *outPrefix) {
    RenderTexture2D cap = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    auto shot = [&](const char *nameIn) {
        std::string name = nameIn;   // TextFormat se reutiliza al dibujar
        captureTarget = &cap;
        for (int i = 0; i < 2; i++) RenderFrame(false);
        captureTarget = nullptr;
        Image img = LoadImageFromTexture(cap.texture);
        ImageFlipVertical(&img);
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
        ExportImage(img, TextFormat("%s_%s.png", outPrefix, name.c_str()));
        UnloadImage(img);
    };
    auto step = [&](int n) { for (int i = 0; i < n; i++) Update(0.033f); };
    auto look = [&](float x, float z, float tx, float tz, float pitch) {
        player.pos = Vector3{ x, 0, z }; player.yaw = atan2f(-(tx - x), -(tz - z)); player.pitch = pitch; player.vel = Vector3{ 0, 0, 0 };
    };
    ResetGame("practica"); game.state = GS_PLAYING; soldierView.visible = false;
    player.maxHealth = player.health = 1e7f; player.points = 999999;
    // cada arma nueva en primera persona
    int ids[] = { W_THOMPSON, W_GARAND, W_CROSSBOW, W_FLAMER, W_GATLING };
    look(0, 2, 0, -10, -0.05f);
    for (int id : ids) { GiveWeapon(id); step(12); vm.swap = 0; vm.kick = 0; shot(TextFormat("arma_%s", WEAPONS[id].name)); }
    // ametralladora rotativa: gira y dispara
    GiveWeapon(W_GATLING); step(10);
    mouseDown = true; mousePressedAt = T; step(10); printf("rotativa a los 0,33 s: giro %.2f, balas %d\n", gatSpinK, CurWeapon().mag);
    step(30); printf("rotativa al segundo: giro %.2f, balas %d\n", gatSpinK, CurWeapon().mag); shot("rotativa_disparando"); mouseDown = false;
    // lanzallamas contra zombis
    for (int k = 0; k < 6; k++) SpawnZombie();
    for (auto &z : zombies) { z.pos = Vector3{ frand(-2, 2), 0, frand(-6, -3) }; z.state = Z_CHASE; z.y = 0; }
    GiveWeapon(W_FLAMER); step(8);
    mouseDown = true; mousePressedAt = T; step(25); shot("lanzallamas"); mouseDown = false;
    int burning = 0; for (auto &z : zombies) if (z.burnT > 0 || z.state == Z_DEAD) burning++;
    printf("lanzallamas: %d de %d zombis quemados o muertos\n", burning, (int)zombies.size());
    step(60); zombies.clear(); ClearEffects();
    // ballesta: la flecha se clava, pita y explota
    GiveWeapon(W_CROSSBOW); step(8); look(0, 2, 0, -14, -0.12f);
    step(20); mouseDown = true; mousePressedAt = T; step(2); mouseDown = false;
    printf("ballesta: flechas en el cargador %d\n", CurWeapon().mag);
    step(10); shot("ballesta_clavada");
    step(48); shot("ballesta_boom");
    step(90);
    // carteles de tiza con la forma real y el arma colgada después de comprarla
    look(-6.8f, -12.4f, -9, -12.4f, 0.12f); step(3); shot("pared_tiza");
    for (auto &it : interactables) if (it.active() && it.text().find("THOMPSON") != std::string::npos) it.action();
    look(-6.8f, -12.4f, -9, -12.4f, 0.12f); step(3); shot("pared_comprada");
    look(6.6f, -13.4f, 9, -13.4f, 0.12f); step(3); shot("pared_tiza2");
    // caja misteriosa: el arma que sale y su nombre; después el osito
    Vector2 bf = box.front;
    look(bf.x + 0.3f, bf.y, box.pos.x, box.pos.y, -0.25f); step(3);
    box.state = BOX_ROLLING; box.t = 0; box.swapT = 0; box.uses = 0; step(20); shot("caja_girando");
    step(110); printf("caja: estado %d, arma %s\n", box.state, box.offer >= 0 ? WEAPONS[box.offer].name : "-"); shot("caja_arma");
    box.state = BOX_IDLE; box.show = -1; step(10); shot("caja_luz");
    box.state = BOX_TEDDY; box.t = 0; box.teddy = true; box.show = -1; step(40); shot("caja_osito");
    step(60);
    // explosiones: un misil contra el suelo y la explosión grande de las cinemáticas
    ClearEffects();
    look(0, 4, 0, -6, -0.1f); step(2);
    Explode(Vector3{ 0, 0.3f, -6 }, 0, 1, 0, 1.5f);
    step(2); shot("boom_0"); step(8); shot("boom_1"); step(25); shot("boom_2"); step(60); shot("boom_3");
    ClearEffects();
    look(0, 6, 0, -20, 0.05f); step(2);
    ExplosionFX(Vector3{ 0, 1, -22 }, 3.0f, true);
    step(4); shot("bigboom_0"); step(20); shot("bigboom_1"); step(60); shot("bigboom_2");
    return 0;
}

// ---------- Foto de los uniformes (--soldados salida) ----------
static int RunSoldierShots(const char *outPrefix) {
    RenderTexture2D cap = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    ResetGame("bunker");
    PowerOnInstant();
    game.state = GS_CUTSCENE;   // sin HUD
    zombies.clear();
    auto shot = [&](const char *nameIn) {
        std::string name = nameIn;
        captureTarget = &cap;
        for (int i = 0; i < 2; i++) RenderFrame(false);
        captureTarget = nullptr;
        Image img = LoadImageFromTexture(cap.texture);
        ImageFlipVertical(&img);
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
        ExportImage(img, TextFormat("%s_%s.png", outPrefix, name.c_str()));
        UnloadImage(img);
    };
    allyView.visible = false; soldierView.visible = false;
    for (int pose = 0; pose < 2; pose++) {
        cutsceneActors.clear();
        for (int k = 0; k < SKIN_COUNT; k++)
            cutsceneActors.push_back(SoldierView{ true, Vector3{ -2.4f + k * 1.6f, 0, 0 }, 0, pose ? C_TALK : C_IDLE, 0.4f + k * 0.3f, k, pose ? 0.8f : 0 });
        cam.position = Vector3{ 0, 1.5f, 4.6f }; cam.target = Vector3{ 0, 1.0f, 0 }; cam.up = Vector3{ 0, 1, 0 }; camFov = 55;
        shot(pose ? "hablando" : "uniformes");
    }
    cam.position = Vector3{ -1.4f, 1.6f, 1.4f }; cam.target = Vector3{ -0.8f, 1.45f, 0 }; camFov = 40;
    shot("cerca");
    // cada animación en tres momentos
    int clips[] = { C_ATTACK, SC_DUCK, SC_PUNCH, SC_NO, SC_JUMP, SC_RUN, C_SCREAM, C_HIT, C_DIE };
    for (int c : clips) {
        cutsceneActors.clear();
        for (int k = 0; k < 3; k++) cutsceneActors.push_back(SoldierView{ true, Vector3{ -1.8f + k * 1.8f, 0, 0 }, 0.5f, c, 0.25f + k * 0.45f, k % SKIN_COUNT, 0 });
        cam.position = Vector3{ 0, 1.4f, 4.4f }; cam.target = Vector3{ 0, 0.9f, 0 }; camFov = 55;
        shot(TextFormat("clip%d", c));
    }
    printf("uniformes: %d\n", SKIN_COUNT);
    return 0;
}

// ---------- Prueba del compañero bot (--ally mapa salida) ----------
static int RunAllyTest(const char *mapKey, const char *outPrefix) {
    RenderTexture2D cap = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    auto shot = [&](const char *nameIn) {
        std::string name = nameIn;
        captureTarget = &cap;
        for (int i = 0; i < 2; i++) RenderFrame(false);
        captureTarget = nullptr;
        Image img = LoadImageFromTexture(cap.texture);
        ImageFlipVertical(&img);
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
        ExportImage(img, TextFormat("%s_%s.png", outPrefix, name.c_str()));
        UnloadImage(img);
    };
    opts.ally = true;
    ResetGame(mapKey); game.state = GS_PLAYING; soldierView.visible = false;
    if (!ally.active) { printf("Milton no apareció\n"); return 1; }
    player.maxHealth = player.health = 1e7f;
    for (int i = 0; i < (int)links.size(); i++) OpenLinkNow(i);
    float maxD = 0, sumD = 0; int n = 0, downs = 0, revivesOk = 0, teleports = 0;
    Vector3 prevAlly = ally.pos;
    bool wasDown = false;
    int shots = 0;
    for (int f = 0; f < 20 * 240; f++) {
        // el jugador recorre las salas: cada 12 s se va al centro de la siguiente
        if (f % 240 == 0) {
            const Room &r = rooms[(f / 240) % rooms.size()];
            Vector3 dst{ (r.x1 + r.x2) / 2, 0, (r.z1 + r.z2) / 2 };
            for (int k = 0; k < 6 && InsideCollider(dst.x, dst.z, 0.5f); k++) dst.x += 1.0f;
            player.pos = dst;
        }
        // y camina un poco en círculo para que Milton tenga que moverse
        player.yaw += 0.01f;
        if (f == 2000) AllyDamage(100000);   // derribarlo
        if (ally.downed && f > 2000 && f < 2400) {   // el jugador va a levantarlo
            player.pos = Vector3{ ally.pos.x + 1.0f, 0, ally.pos.z };
            for (auto &it : interactables) if (it.hold && it.active() && it.text().find("MILTON") != std::string::npos) it.action();
        }
        if (f == 3200) AllyDamage(100000);   // y otra vez, para que se levante solo
        Update(0.05f);
        if (Vector3Distance(prevAlly, ally.pos) > 4) teleports++;
        prevAlly = ally.pos;
        if (ally.downed && !wasDown) downs++;
        if (!ally.downed && wasDown) revivesOk++;
        wasDown = ally.downed;
        float d = hyp(ally.pos.x - player.pos.x, ally.pos.z - player.pos.z);
        if (f % 240 > 100) { maxD = std::max(maxD, d); sumD += d; n++; }   // tras darle tiempo a llegar
        if (shots < 3 && f > 600 && f % 400 == 0 && !ally.downed) {
            player.yaw = atan2f(-(ally.pos.x - player.pos.x), -(ally.pos.z - player.pos.z));
            player.pitch = -0.05f;
            Update(0.05f);
            std::string nm = "milton" + std::to_string(shots++); shot(nm.c_str());
        }
        if (f % 400 == 0) printf("t=%5.1f ronda %d  dist %.1f  vida %.0f  bajas de Milton %d  %s\n", T, game.round, d, ally.health, ally.kills, ally.downed ? "CAÍDO" : "");
    }
    printf("puntos del jugador: %d (empezó con 500 y no disparó)\n", player.points);
    printf("distancia media %.1f, máxima %.1f; teletransportes %d; caídas %d, levantadas %d; bajas de Milton %d, del jugador %d\n",
           n ? sumD / n : 0, maxD, teleports, downs, revivesOk, ally.kills, game.kills);
    return 0;
}

// ---------- Prueba del modo historia (--story capítulo salida): juega el capítulo entero solo ----------
const char *StoryDebug();
Vector2 StoryDefendZone();
static int RunStoryTest(int ch, const char *outPrefix) {
    RenderTexture2D cap = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    auto shot = [&](const char *nameIn, bool menu = false) {
        std::string name = nameIn;
        captureTarget = &cap;
        for (int i = 0; i < 2; i++) RenderFrame(menu);
        captureTarget = nullptr;
        Image img = LoadImageFromTexture(cap.texture);
        ImageFlipVertical(&img);
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
        ExportImage(img, TextFormat("%s_%s.png", outPrefix, name.c_str()));
        UnloadImage(img);
    };
    if (ch == 0) {   // menú principal y pantalla de capítulos
        cam.position = Vector3{ 2, 1.8f, 7 }; cam.target = Vector3{ 0, 1.4f, -6 }; cam.up = Vector3{ 0, 1, 0 }; camFov = 70;
        shot("menu", true);
        storyMenuOpen = true; shot("capitulos", true); storyMenuOpen = false;
    }
    StoryStart(ch);
    int n = 0;
    while (game.state == GS_STORY && n < 2000) { UpdateStoryCards(0.05f); n++; if (n == 40) shot("tarjeta"); if (n == 120) shot("narracion"); }
    printf("tarjetas: %.1f s, estado %d\n", n * 0.05f, game.state);
    n = 0;
    while (game.state == GS_INTRO && n < 1000) { UpdateIntro(0.05f); UpdateWorld(0.05f); n++; if (n == 60) shot("intro"); }
    printf("cinemática: %.1f s, estado %d\n", n * 0.05f, game.state);
    player.maxHealth = player.health = 1e7f;
    for (int i = 0; i < (int)links.size(); i++) OpenLinkNow(i);   // el test no compra puertas
    player.points = 99999;
    std::string last;
    int shots = 0;
    int cutShots = 0;
    for (int f = 0; f < 20 * 60 * 20 && (game.state == GS_PLAYING || game.state == GS_CUTSCENE); f++) {
        // cinemática: avanzar y sacar un par de capturas
        if (game.state == GS_CUTSCENE) {
            int k = 0;
            while (game.state == GS_CUTSCENE && k < 2400) {
                UpdateCutscene(0.05f); float cdt = 0.05f * CutsceneTimeScale(); UpdateZombies(cdt); UpdateWorld(cdt); UpdateEffects(cdt); k++;
                if (k == 50 || k == 140 || k == 260) { std::string nm = "cine" + std::to_string(cutShots++); shot(nm.c_str()); }
            }
            printf("t=%5.1f  cinemática de %.1f s\n", T, k * 0.05f);
            continue;
        }
        // los zombis caen solos y el jugador va directo a cada objetivo (el jefe recibe daño de a poco)
        if (f % 20 == 0) for (auto &z : zombies) if (z.state == Z_CHASE && !z.boss) KillZombie(z, false, true);
        if (f % 10 == 0) for (auto &z : zombies) if (z.boss && z.state != Z_DEAD) { z.hp -= 600; if (z.hp <= 0) KillZombie(z, true); }
        Update(0.05f);
        std::string d = StoryDebug();
        if (d != last) {
            printf("t=%5.1f  %s\n", T, d.c_str()); last = d;
            if (shots < 6 && d.find("hecho") == std::string::npos && d.find("paso") != std::string::npos) {
                for (int i = 0; i < 30 && game.state == GS_PLAYING; i++) Update(0.05f);
                UpdateCamera();
                { std::string nm = "paso" + std::to_string(shots++); shot(nm.c_str()); }
            }
        }
        if (!powerOn && f % 100 == 50 && d.find("POWER") != std::string::npos) {
            Vector2 p; if (PowerSwitchSpot(p)) { player.pos = Vector3{ p.x, 0, p.y }; }
            PowerOnNow();
        }
        if (d.find("REACH") != std::string::npos && f % 100 == 50) {
            size_t sp = d.find("sala ");
            std::string room = sp != std::string::npos ? d.substr(sp + 5, 1) : "A";
            const Room &r = rooms[RoomIndex(room)];
            Vector3 c{ (r.x1 + r.x2) / 2, 0, (r.z1 + r.z2) / 2 };
            for (int k = 0; k < 12 && InsideCollider(c.x, c.z, 0.5f); k++) c.z -= 1.0f;
            player.pos = c;
        }
        if (d.find("DEFEND") != std::string::npos) { Vector2 zc = StoryDefendZone(); if (hyp(player.pos.x - zc.x, player.pos.z - zc.y) > 3) player.pos = Vector3{ zc.x + 2.5f, 0, zc.y - 2.5f }; }
        if ((d.find("COLLECT") != std::string::npos || d.find("PLANT") != std::string::npos) && f % 60 == 30) {
            for (auto &it : interactables) {
                if (!it.active()) continue;
                std::string tx = it.text();
                const char *keys[] = { "el diario", "la carga", "la muestra", "la ficha", "la tarjeta", "el código", "el componente" };   // ojo: "Recarga rápida" no
                bool mine = false;
                for (auto k : keys) if (tx.find(k) != std::string::npos) mine = true;
                if (!mine) continue;
                Vector2 p = it.pos();
                if (shots < 7) { for (int k = 0; k < 4; k++) { player.pos = Vector3{ p.x + 1.6f, 0, p.y }; player.yaw = atan2f(1.0f, 0); player.pitch = -0.25f; Update(0.05f); } shot("objeto"); shots = 7; }
                player.pos = Vector3{ p.x + 1.2f, 0, p.y };
                for (int k = 0; k < 400 && it.active(); k++) it.action();
                break;
            }
        }
    }
    printf("final: %s, estado %d\n", StoryDebug(), game.state);
    if (game.state == GS_STORY) {
        for (int i = 0; i < 30; i++) UpdateStoryCards(0.05f);
        shot("final");
    }
    return 0;
}

// ---------- Medición de rendimiento (--bench mapa segundos) ----------
static int RunBench(const char *mapKey, float secs) {
    ResetGame(mapKey); game.state = GS_PLAYING;
    EnableCursor();
    player.points = 99999; game.betweenT = 0.5f;
    for (auto &it : interactables) if (it.text().find("puerta") != std::string::npos) it.action();
    player.health = 1e9f; player.maxHealth = 1e9f;
    game.round = 7;
    int frames = 0; double t0 = GetTime(), last = t0; int lf = 0;
    while (!WindowShouldClose() && GetTime() - t0 < secs) {
        float dt = std::min(GetFrameTime(), 0.05f);
        Zombie *best = nullptr; float bd = 1e9f;
        for (auto &z : zombies) if (z.state == Z_CHASE) { float d = Vector3Distance(z.pos, player.pos); if (d < bd) { bd = d; best = &z; } }
        if (best) { float dx = best->pos.x - player.pos.x, dz = best->pos.z - player.pos.z; player.yaw = atan2f(-dx, -dz); player.pitch = atan2f(best->headPos.y - 1.65f, hyp(dx, dz)); mouseDown = true; mousePressedAt = T; }
        else { mouseDown = false; player.yaw += dt * 0.5f; }
        if (CurWeapon().reserve < 10) CurWeapon().reserve = 200;
        Update(dt);
        RenderFrame(false);
        frames++;
        if (GetTime() - last > 2) { printf("fps %.1f  zombis %d  ronda %d\n", (frames - lf) / (GetTime() - last), (int)zombies.size(), game.round); last = GetTime(); lf = frames; fflush(stdout); }
    }
    printf("media %.1f fps\n", frames / (GetTime() - t0));
    return 0;
}

int main(int argc, char **argv) {
    bool test = argc >= 4 && !strcmp(argv[1], "--test");
    bool bench = argc >= 4 && !strcmp(argv[1], "--bench");
    bool intro = argc >= 4 && !strcmp(argv[1], "--intro");
    bool netTest = argc >= 5 && !strcmp(argv[1], "--net");
    test = test || netTest;
    bool climb = argc >= 2 && !strcmp(argv[1], "--climb");
    bool shots = argc >= 4 && !strcmp(argv[1], "--shots");
    test = test || shots;
    test = test || climb;
    test = test || intro;
    bool story = argc >= 4 && !strcmp(argv[1], "--story");
    bool allyT = argc >= 4 && !strcmp(argv[1], "--ally");
    bool soldT = argc >= 3 && !strcmp(argv[1], "--soldados");
    bool eggT = argc >= 4 && !strcmp(argv[1], "--secretos");
    bool armasT = argc >= 3 && !strcmp(argv[1], "--armas");
    test = test || armasT;
    test = test || eggT;
    test = test || soldT;
    test = test || allyT;
    test = test || story;
    ChangeDirectory(GetApplicationDirectory());
    unsigned flags = FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT;
    if (test) flags = FLAG_WINDOW_HIDDEN;
    if (bench) flags = 0;
    SetConfigFlags(flags);
    SetTraceLogLevel(test ? LOG_WARNING : LOG_ERROR);
    InitWindow(test ? 1280 : 1600, test ? 720 : 900, "Última Ronda · madebyfrank");
    SetExitKey(KEY_NULL);
    LoadFonts();
    Loading("Cargando...");
    InitRender();
    InitBaseMeshes();
    LoadMaterials();
    Loading("Cargando zombis...");
    BuildGunModels();
    InitZombies();
    InitAudioSynth();
    LoadVoiceLines();
    LoadOptions();
    LoadPowerupAssets();
    BuildMap("bunker"); PowerOnInstant();
    cam.fovy = 75;
    cam.up = Vector3{ 0, 1, 0 };
    cam.projection = CAMERA_PERSPECTIVE;
    player.weapons.push_back({ W_PISTOL, 8, 80, false });

    if (test) recordsFile = "records_prueba.txt";
    if (intro) return RunIntroTest(argv[2], argv[3]);
    if (story) return RunStoryTest(atoi(argv[2]), argv[3]);
    if (allyT) return RunAllyTest(argv[2], argv[3]);
    if (soldT) return RunSoldierShots(argv[2]);
    if (eggT) return RunEggTest(argv[2], argv[3]);
    if (armasT) return RunArmasTest(argv[2]);
    if (climb) return RunClimbTest();
    if (netTest) return RunNetTest(!strcmp(argv[2], "host"), argv[3], argv[4]);
    if (shots) return RunMapShots(argv[2], argv[3]);
    if (test) return !strcmp(argv[2], "practica") ? RunPracticeTest(argv[3]) : RunTest(argv[2], argv[3]);
    if (bench) return RunBench(argv[2], (float)atof(argv[3]));

    float menuT = 0;
    while (!WindowShouldClose()) {
        float dt = std::min(GetFrameTime(), 0.05f);
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();
        if (IsKeyPressed(KEY_F3)) {
            extern bool lowQuality;
            lowQuality = !lowQuality;
            SaveOptions();
            CenterMsg(lowQuality ? "GRÁFICOS: RÁPIDO" : "GRÁFICOS: ALTO", WHITE, 1.5f);
        }
        UpdateHUDTimers(dt);
        UpdateMusic();
        int click = MenuClicked();
        NetUpdate(dt);   // co-op: recibir, enviar y la sala de espera
        StoryAudioTick();
        { std::string m; if (NetTakeStart(m)) { optionsOpen = false; ResetGame(m); } }
        if (game.state == GS_MENU) {
            menuT += dt;
            // cámara girando despacio por el mapa
            float a = menuT * 0.08f;
            cam.position = Vector3{ sinf(a) * 4, 1.8f, 4 + cosf(a) * 4 };
            cam.target = Vector3{ 0, 1.4f, -6 };
            cam.up = Vector3{ 0, 1, 0 };
            camFov = 70;
            UpdateWorld(dt);
            UpdateEffects(dt);
            NetLobbyClick(click);
            bool solo = NetLobbyScreen() == LB_NONE && NetRole() == NET_OFF;
            if (solo && (click == 0 || click == 1)) ResetGame(click == 0 ? "bunker" : "fabrica");
            if (solo && click == 7) ResetGame("pueblo");
            if (solo && click == 9) ResetGame("hospital");
            if (solo && click == 10) ResetGame("base");
            if (solo && click == 11) ResetGame("laboratorio");
            if (solo && click == 4) ResetGame("practica");
            if (solo && click == 30) storyMenuOpen = true;
            if (storyMenuOpen && click >= 31 && click <= 36 && click - 31 <= StoryProgress()) { storyMenuOpen = false; StoryStart(click - 31); }
            if (storyMenuOpen && (click == 37 || (IsKeyPressed(KEY_ESCAPE) && !optionsOpen))) storyMenuOpen = false;
            if (click == 5) optionsOpen = true;
            if (optionsOpen && (click == 6 || IsKeyPressed(KEY_ESCAPE))) { optionsOpen = false; SaveOptions(); }
            RenderFrame(game.state == GS_MENU);
            continue;
        }
        if (game.state == GS_CUTSCENE) {
            UpdateCutscene(dt);
            float cdt = dt * CutsceneTimeScale();   // cámara lenta
            UpdateZombies(cdt);
            UpdateWorld(cdt);
            UpdateEffects(cdt);
            if (game.state != GS_CUTSCENE) ignoreMouse = 3;
        } else if (game.state == GS_STORY) {
            UpdateStoryCards(dt);
            if (game.state != GS_STORY) ignoreMouse = 3;
        } else if (game.state == GS_INTRO) {
            UpdateIntro(dt);
            UpdateWorld(dt);
            UpdateEffects(dt);
            if (game.state == GS_PLAYING) ignoreMouse = 3;
        } else if (game.state == GS_PLAYING) {
            EggsUpdate();
            HandleInput();
            if (IsKeyPressed(KEY_ESCAPE)) { game.state = GS_PAUSED; EnableCursor(); }
            else Update(dt);
        } else if (game.state == GS_PAUSED) {
            if (NetActive()) Update(dt);   // co-op: el mundo no se detiene
            EggsUpdate();
            if (optionsOpen) {
                if (click == 6 || IsKeyPressed(KEY_ESCAPE)) { optionsOpen = false; SaveOptions(); }
            } else if (click == 5) optionsOpen = true;
            else if (click == 3) BackToMenu();
            else if ((IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !PauseButtonHover()) || IsKeyPressed(KEY_ESCAPE)) { game.state = GS_PLAYING; DisableCursor(); ignoreMouse = 3; }
        } else if (game.state == GS_OVER) {
            if (click == 2 && NetRole() == NET_HOST && NetConnected()) NetRestart(game.mapKey);   // co-op: reinicia a los dos
            else if (click == 2 && StoryActive()) StoryStart(StoryChapter(), true);   // historia: reintentar el capítulo
            else if (click == 2 && NetRole() != NET_CLIENT) ResetGame(game.mapKey);
            if (click == 3) BackToMenu();
        }
        RenderFrame(game.state == GS_MENU);
    }
    NetShutdown();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
