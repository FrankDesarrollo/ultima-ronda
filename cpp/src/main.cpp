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
    const char *rs = (w.id == W_PISTOL || w.id == W_REVOLVER || w.id == W_RAYGUN || w.id == W_MAGNUM || w.id == W_FLARE) ? "reload_pistol" : (w.id == W_SMG || w.id == W_SUOMI) ? "reload_smg" : "reload_rifle";
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
    newRecord = !Practice() && SubmitRecord(game.mapKey, game.round, game.kills, game.heads);
    game.state = GS_OVER;
    EnableCursor();
    scoped = false;
}
void DamagePlayer(int n) {
    if (game.state != GS_PLAYING || n <= 0 || Practice()) return;   // en el campo de tiro no te pueden hacer daño
    player.health -= n;
    player.lastHit = T;
    vm.shake = std::max(vm.shake, 0.3f);
    Play("hurt");
    if (player.health > 0) Say(player.health < player.maxHealth * 0.35f ? V_LOWHP : V_HURT, player.health < player.maxHealth * 0.35f ? 0.6f : 0.45f);
    if (player.health > 0) return;
    if (HasPerk(player, P_REVIVE)) {
        player.perks &= ~(1u << P_REVIVE);
        player.health = player.maxHealth;
        for (auto &z : zombies) if (z.state == Z_CHASE && Vector3Distance(z.pos, player.pos) < 6) KillZombie(z, false, true);
        CenterMsg("¡REVIVISTE!", Color{ 124, 196, 255, 255 });
        WhiteFlash(0.7f);
    } else GameOver();
}

// ---------- Power-ups ----------
struct PowerDef { const char *name, *shortName; unsigned color; };
static PowerDef POWERS[4] = {
    { "MUNICIÓN MÁXIMA", "MAX", 0xffd23f }, { "MUERTE INSTANTÁNEA", "INSTA", 0xff3b3b },
    { "DOBLE PUNTOS", "x2", 0x3bff6b }, { "BOMBA NUCLEAR", "BOMBA", 0xffa23b },
};
struct Powerup { int type; Vector3 p; float t; };
static std::vector<Powerup> powerups;
static Texture2D powerLabels[4];
static PMat powerMats[4];

void DropPowerup(float x, float z, int type) { powerups.push_back({ type < 0 ? irand(4) : type, Vector3{ x, 1, z }, 0 }); }
static void ApplyPowerup(int type) {
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
        float d = hyp(p.p.x - player.pos.x, p.p.z - player.pos.z);
        if (d < 1.3f || p.t > 26) {
            if (d < 1.3f) ApplyPowerup(p.type);
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
        CenterMsg("¡RONDA DE PERROS!", Color{ 255, 120, 40, 255 }, 3.5f);
        return;
    }
    game.toSpawn = std::min(70, (int)roundf(5 + game.round * 3 + (game.round > 5 ? game.round * 1.5f : 0)));
    game.spawnT = 1;
    Play("round");
    if (game.round > 1) Say(V_ROUND, 0.7f);
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
    if (mouseDown && !player.papBusy && player.knifeT < 0 && (s.autoFire || T - mousePressedAt < 0.25f) && !player.reloading && T >= player.nextShot) {
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
static void ResetGame(const std::string &mapKey) {
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
    vm = ViewModel();
    T = 0;
    ResetVoiceLines();
    GiveWeapon(W_PISTOL);
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

static void Update(float dt) {
    T += dt;
    flashK = std::max(0.0f, flashK - dt * 0.8f);
    UpdatePlayer(dt);
    UpdateCamera();
    UpdateWeapon(dt);
    UpdateZombies(dt);
    if (Practice()) UpdatePractice(dt); else UpdateRounds(dt);
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
    for (auto &v : views) {
        player.pos = Vector3{ v.x, 0, v.z }; player.yaw = v.yaw; player.pitch = v.pitch; player.vel = Vector3{ 0, 0, 0 };
        for (int i = 0; i < 10; i++) Update(0.05f);
        player.yaw = v.yaw; player.pitch = v.pitch;
        Update(0.05f);
        captureTarget = &rt;
        for (int i = 0; i < 2; i++) RenderFrame(false);
        captureTarget = nullptr;
        Image img = LoadImageFromTexture(rt.texture);
        ImageFlipVertical(&img);
        ExportImage(img, TextFormat("%s_%s.png", outPrefix, v.name));
        UnloadImage(img);
    }
    printf("capturas de %s listas; zombis: %d\n", mapKey, (int)zombies.size());
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
    bool climb = argc >= 2 && !strcmp(argv[1], "--climb");
    bool shots = argc >= 4 && !strcmp(argv[1], "--shots");
    test = test || shots;
    test = test || climb;
    test = test || intro;
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
    if (climb) return RunClimbTest();
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
            if (click == 0 || click == 1) ResetGame(click == 0 ? "bunker" : "fabrica");
            if (click == 7) ResetGame("pueblo");
            if (click == 4) ResetGame("practica");
            if (click == 5) optionsOpen = true;
            if (optionsOpen && (click == 6 || IsKeyPressed(KEY_ESCAPE))) { optionsOpen = false; SaveOptions(); }
            RenderFrame(game.state == GS_MENU);
            continue;
        }
        if (game.state == GS_INTRO) {
            UpdateIntro(dt);
            UpdateWorld(dt);
            UpdateEffects(dt);
            if (game.state == GS_PLAYING) ignoreMouse = 3;
        } else if (game.state == GS_PLAYING) {
            HandleInput();
            if (IsKeyPressed(KEY_ESCAPE)) { game.state = GS_PAUSED; EnableCursor(); }
            else Update(dt);
        } else if (game.state == GS_PAUSED) {
            if (optionsOpen) {
                if (click == 6 || IsKeyPressed(KEY_ESCAPE)) { optionsOpen = false; SaveOptions(); }
            } else if (click == 5) optionsOpen = true;
            else if (click == 3) { game.state = GS_MENU; zombies.clear(); BuildMap("bunker"); PowerOnInstant(); }
            else if ((IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !PauseButtonHover()) || IsKeyPressed(KEY_ESCAPE)) { game.state = GS_PLAYING; DisableCursor(); ignoreMouse = 3; }
        } else if (game.state == GS_OVER) {
            if (click == 2) ResetGame(game.mapKey);
            if (click == 3) { game.state = GS_MENU; zombies.clear(); BuildMap("bunker"); PowerOnInstant(); }
        }
        RenderFrame(game.state == GS_MENU);
    }
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
