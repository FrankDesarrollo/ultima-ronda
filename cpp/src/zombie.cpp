// Zombis: modelo animado, IA (ventanas, persecución entre salas), impactos y aparición.
// También los perros del infierno de las rondas de perros (lobo animado CC0 de Quaternius).
#include "game.h"
#include "anim.h"

std::vector<Zombie> zombies;
// Esqueleto animado: modelo, animaciones y huesos de referencia (cabeza, cadera y cuello)
struct Rig {
    Model model{};
    ModelAnimation clips[C_COUNT]{};
    bool loop[C_COUNT] = { true, true, true, false, true, false, false };
    float fps = 30;
    int head = 0, hips = 0, neck = 0;
    std::vector<Transform> pose, pose2;
    std::vector<Matrix> invBind;   // si está, el skinning usa las matrices del glTF en vez de la pose de reposo de raylib
};
static Rig zr, dr;   // zombi y perro
static Rig &RigOf(const Zombie &z) { return z.dog && dr.model.meshCount ? dr : zr; }
static Texture2D zTex[6];
void SetSkinParams(float metal, float rough, float nstr, Vector3 emis, float hitFlash);
Material &DepthMatSkin();

static const char *ZDIR = "assets/models/";

// Mapa de rugosidad: el canal verde de la textura ORM (oclusión/rugosidad/metal)
static Texture2D LoadRoughFromORM(const char *path) {
    Image img = LoadImage(path);
    if (!img.data) return texWhite;
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Color *px = (Color *)img.data;
    for (int i = 0; i < img.width * img.height; i++) { px[i].r = px[i].g; px[i].b = px[i].g; }
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    GenTextureMipmaps(&t);
    SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
    return t;
}
static Texture2D LoadZTex(const char *name) {
    Texture2D t = LoadTexture(TextFormat("%stex/InfectedCityMan_City_%s", ZDIR, name));
    if (!t.id) return texWhite;
    GenTextureMipmaps(&t);
    SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
    return t;
}

static int FindBone(const Model &m, const char *name) {
    for (int i = 0; i < m.boneCount; i++) {
        const char *n = strrchr(m.bones[i].name, ':');
        n = n ? n + 1 : m.bones[i].name;
        if (!strcmp(n, name)) return i;
    }
    return 0;
}

// Perro del infierno: lobo con pelaje chamuscado y ojos que brillan
static void InitDogs() {
    const char *mp = "assets/models/wolf.glb";
    if (!FileExists(mp)) return;
    dr.model = LoadModel(mp);
    if (!dr.model.meshCount || !dr.model.boneCount) return;
    void ComputeTangents(Mesh &m, bool dxNormals);
    for (int i = 0; i < dr.model.meshCount; i++) ComputeTangents(dr.model.meshes[i], false);
    Color cols[] = { Color{ 255, 255, 255, 255 }, Color{ 34, 24, 21, 255 }, Color{ 78, 44, 30, 255 }, Color{ 255, 40, 10, 255 }, Color{ 10, 8, 8, 255 } };
    for (int i = 0; i < dr.model.materialCount; i++) {
        Material &mt = dr.model.materials[i];
        mt.shader = shLitSkin;
        mt.maps[MATERIAL_MAP_ALBEDO].texture = texWhite;
        mt.maps[MATERIAL_MAP_ALBEDO].color = cols[std::min(i, 4)];
        mt.maps[MATERIAL_MAP_NORMAL].texture = texFlat;
        mt.maps[MATERIAL_MAP_ROUGHNESS].texture = texWhite;
    }
    const char *names[C_COUNT] = { "Walk", "Gallop", "Attack", "Death", "Idle", "Jump_ToIdle", "Idle_HitReact_Left" };
    for (int c = 0; c < C_COUNT; c++)
        if (!BakeAnimation(dr.model, mp, mp, dr.fps, true, &dr.clips[c], names[c])) TraceLog(LOG_WARNING, "Perro: falta la animación %s", names[c]);
    dr.head = FindBone(dr.model, "Head");
    dr.hips = FindBone(dr.model, "Back");
    dr.neck = FindBone(dr.model, "Neck1");
    if (!LoadInverseBind(mp, dr.invBind) || (int)dr.invBind.size() != dr.model.boneCount) dr.invBind.clear();
    dr.pose.resize(dr.model.boneCount);
    dr.pose2.resize(dr.model.boneCount);
}

void InitZombies() {
    std::string mp = std::string(ZDIR) + "InfectedCityMan.glb";
    Model &zModel = zr.model;
    zModel = LoadModel(mp.c_str());
    void ComputeTangents(Mesh &m, bool dxNormals);
    for (int i = 0; i < zModel.meshCount; i++) ComputeTangents(zModel.meshes[i], true);
    zTex[0] = LoadZTex("Man_Infected_Outfit1_BaseColor.png");
    zTex[1] = LoadZTex("Man_Infected_Outfit1_Normal.png");
    zTex[2] = LoadRoughFromORM(TextFormat("%stex/InfectedCityMan_City_Man_Infected_Outfit1_OcclusionRoughnessMetallic.png", ZDIR));
    zTex[3] = LoadZTex("Infected_Man_Body1_BaseColor.png");
    zTex[4] = LoadZTex("Infected_Man_Body1_Normal.png");
    zTex[5] = LoadRoughFromORM(TextFormat("%stex/InfectedCityMan_City_Infected_Man_Body1_OcclusionRoughnessMetallic.png", ZDIR));
    for (int i = 0; i < zModel.materialCount; i++) {
        Material &mt = zModel.materials[i];
        mt.shader = shLitSkin;
        int set = i == 1 ? 0 : i == 2 ? 3 : -1;
        if (set >= 0) {
            mt.maps[MATERIAL_MAP_ALBEDO].texture = zTex[set];
            mt.maps[MATERIAL_MAP_NORMAL].texture = zTex[set + 1];
            mt.maps[MATERIAL_MAP_ROUGHNESS].texture = zTex[set + 2];
        } else {
            mt.maps[MATERIAL_MAP_ALBEDO].texture = texWhite;
            mt.maps[MATERIAL_MAP_ALBEDO].color = Color{ 70, 62, 55, 255 };
            mt.maps[MATERIAL_MAP_NORMAL].texture = texFlat;
            mt.maps[MATERIAL_MAP_ROUGHNESS].texture = texWhite;
        }
    }
    zr.head = FindBone(zModel, "Head");
    zr.hips = FindBone(zModel, "Hips");
    zr.neck = FindBone(zModel, "Neck");
    const char *files[C_COUNT] = { "Zombie_Walk", "Zombie_Running", "Zombie_Attack", "Zombie_Dying", "Zombie_Idle", "Zombie_Scream", "Zombie_Reaction_Hit" };
    for (int c = 0; c < C_COUNT; c++) {
        if (!BakeAnimation(zModel, mp.c_str(), TextFormat("%s%s.glb", ZDIR, files[c]), zr.fps, true, &zr.clips[c]))
            TraceLog(LOG_WARNING, "No se pudo cargar la animación %s", files[c]);
    }
    zr.pose.resize(zModel.boneCount);
    zr.pose2.resize(zModel.boneCount);
    InitDogs();
}

// ---------- Poses ----------
static void SampleClip(const Rig &r, int c, float t, std::vector<Transform> &out) {
    const ModelAnimation &a = r.clips[c];
    if (a.frameCount == 0) { for (int i = 0; i < r.model.boneCount; i++) out[i] = r.model.bindPose[i]; return; }
    float f = t * r.fps;
    int f0, f1; float k;
    if (r.loop[c]) {
        f = fmodf(f, (float)a.frameCount);
        if (f < 0) f += a.frameCount;
        f0 = (int)f; f1 = (f0 + 1) % a.frameCount; k = f - f0;
    } else {
        f = clampf(f, 0, (float)(a.frameCount - 1));
        f0 = (int)f; f1 = std::min(f0 + 1, a.frameCount - 1); k = f - f0;
    }
    for (int i = 0; i < r.model.boneCount; i++) {
        const Transform &A = a.framePoses[f0][i], &B = a.framePoses[f1][i];
        out[i].translation = Vector3Lerp(A.translation, B.translation, k);
        out[i].rotation = QuaternionSlerp(A.rotation, B.rotation, k);
        out[i].scale = Vector3Lerp(A.scale, B.scale, k);
    }
}
static float ClipLen(const Rig &r, int c) { return r.clips[c].frameCount / r.fps; }

static Matrix ZMatrix(const Zombie &z) {
    return MatrixMultiply(MatrixMultiply(MatrixScale(z.scale, z.scale, z.scale), MatrixRotateY(z.yaw)), MatrixTranslate(z.pos.x, z.y, z.pos.z));
}

static void PoseZombie(Zombie &z) {
    Rig &r = RigOf(z);
    if (z.rag) {   // muñeco de trapo: la pose sale de la física
        const std::vector<Transform> &rp = RagdollPose(*z.rag);
        if ((int)rp.size() == r.model.boneCount) std::copy(rp.begin(), rp.end(), r.pose.begin());
    } else {
        SampleClip(r, z.clip, z.clipT, r.pose);
        if (z.blend < 1) {
            SampleClip(r, z.prevClip, z.prevT, r.pose2);
            for (int i = 0; i < r.model.boneCount; i++) {
                r.pose[i].translation = Vector3Lerp(r.pose2[i].translation, r.pose[i].translation, z.blend);
                r.pose[i].rotation = QuaternionSlerp(r.pose2[i].rotation, r.pose[i].rotation, z.blend);
            }
        }
    }
    Matrix zm = ZMatrix(z);
    z.headPos = Vector3Transform(r.pose[r.head].translation, zm);
    z.hipsPos = Vector3Transform(r.pose[r.hips].translation, zm);
    z.neckPos = Vector3Transform(r.pose[r.neck].translation, zm);
}

// Igual que UpdateModelAnimationBones de raylib, pero con la pose ya mezclada
static void UploadPose(Rig &r) {
    for (int m = 0; m < r.model.meshCount; m++) {
        Mesh &mesh = r.model.meshes[m];
        if (!mesh.boneMatrices) continue;
        if (!r.invBind.empty()) {
            for (int b = 0; b < mesh.boneCount && b < r.model.boneCount; b++) {
                const Transform &p = r.pose[b];
                Matrix pm = MatrixMultiply(MatrixMultiply(MatrixScale(p.scale.x, p.scale.y, p.scale.z), QuaternionToMatrix(p.rotation)), MatrixTranslate(p.translation.x, p.translation.y, p.translation.z));
                mesh.boneMatrices[b] = MatrixMultiply(r.invBind[b], pm);
            }
            continue;
        }
        for (int b = 0; b < mesh.boneCount && b < r.model.boneCount; b++) {
            const Transform &in = r.model.bindPose[b], &out = r.pose[b];
            Vector3 invT = Vector3RotateByQuaternion(Vector3Negate(in.translation), QuaternionInvert(in.rotation));
            Quaternion invR = QuaternionInvert(in.rotation);
            Vector3 invS = Vector3Divide(Vector3{ 1, 1, 1 }, in.scale);
            Vector3 bt = Vector3Add(Vector3RotateByQuaternion(Vector3Multiply(out.scale, invT), out.rotation), out.translation);
            Quaternion br = QuaternionMultiply(out.rotation, invR);
            Vector3 bs = Vector3Multiply(out.scale, invS);
            mesh.boneMatrices[b] = MatrixMultiply(MatrixMultiply(QuaternionToMatrix(br), MatrixTranslate(bt.x, bt.y, bt.z)), MatrixScale(bs.x, bs.y, bs.z));
        }
    }
}

static void SetClip(Zombie &z, int c, bool randomStart = false) {
    if (z.clip == c) return;
    z.prevClip = z.clip; z.prevT = z.clipT;
    z.clip = c;
    z.clipT = randomStart ? frand(0, ClipLen(RigOf(z), c)) : 0;
    z.blend = 0;
}

void DrawZombies() {
    for (auto &z : zombies) {
        if (Vector3Distance(z.pos, cam.position) > 45) continue;
        if (z.state == Z_RISE && (z.y < -1.85f || z.dog)) continue;   // perro: aún no ha llegado (rayo)
        Rig &r = RigOf(z);
        PoseZombie(z);
        UploadPose(r);
        Matrix zm = ZMatrix(z);
        Model &mdl = r.model;
        if (depthPass) {
            for (int i = 0; i < mdl.meshCount; i++) DrawMesh(mdl.meshes[i], DepthMatSkin(), zm);
            continue;
        }
        for (int i = 0; i < mdl.meshCount; i++) {
            Material &mt = mdl.materials[mdl.meshMaterial[i]];
            Color keep = mt.maps[MATERIAL_MAP_ALBEDO].color;
            if (z.dog) {
                // ojos encendidos (material 3) y un resplandor de brasas en el pelaje
                bool eyes = mdl.meshMaterial[i] == 3;
                SetSkinParams(0, eyes ? 0.3f : 0.9f, 0, eyes ? Vector3{ 6, 0.6f, 0.1f } : Vector3{ 0.004f + 0.003f * sinf(T * 7 + z.pos.x), 0.0006f, 0 }, z.hitFlash);
            } else {
                SetSkinParams(0, 1.0f, 1.0f, Vector3{ 0, 0, 0 }, z.hitFlash);
                if (mt.maps[MATERIAL_MAP_ALBEDO].texture.id != texWhite.id) mt.maps[MATERIAL_MAP_ALBEDO].color = z.tint;
            }
            DrawMesh(mdl.meshes[i], mt, zm);
            mt.maps[MATERIAL_MAP_ALBEDO].color = keep;
        }
    }
    if (!depthPass) SetSkinParams(0, 1, 1, Vector3{ 0, 0, 0 }, 0);
}

// ---------- Impactos ----------
bool RayZombie(Ray r, float maxDist, int &zi, float &dist, bool &head, Vector3 &point) {
    dist = maxDist; zi = -1;
    for (size_t i = 0; i < zombies.size(); i++) {
        Zombie &z = zombies[i];
        if (z.state == Z_DEAD || (z.state == Z_RISE && (z.y < -1.2f || z.dog))) continue;
        if (Vector3Distance(z.pos, r.position) > maxDist + 2) continue;
        PoseZombie(z);
        auto test = [&](Vector3 p, float rad, bool h) {
            RayCollision c = GetRayCollisionSphere(r, p, rad);
            if (c.hit && c.distance < dist) { dist = c.distance; zi = (int)i; head = h; point = c.point; }
        };
        if (z.dog) {   // perro: cabeza, lomo y patas
            test(z.headPos, 0.2f, true);
            for (int k = 0; k <= 3; k++) test(Vector3Lerp(z.hipsPos, z.neckPos, k / 3.0f), 0.26f, false);
            Vector3 mid = Vector3Lerp(z.hipsPos, z.neckPos, 0.5f);
            test(Vector3{ mid.x, z.y + 0.25f, mid.z }, 0.22f, false);
            continue;
        }
        test(z.headPos, 0.17f * z.scale, true);
        // torso y piernas como cadenas de esferas
        for (int k = 0; k <= 3; k++) test(Vector3Lerp(z.hipsPos, z.neckPos, k / 3.0f), 0.24f * z.scale, false);
        Vector3 feet{ z.hipsPos.x, z.y + 0.15f, z.hipsPos.z };
        for (int k = 1; k <= 3; k++) test(Vector3Lerp(z.hipsPos, feet, k / 3.0f), 0.2f * z.scale, false);
    }
    return zi >= 0;
}

float ZombieHP(int r) {
    if (r < 10) return 150.0f + 100 * (r - 1);
    float h = 950;
    for (int i = 10; i <= r; i++) h *= 1.1f;
    return roundf(h);
}
static float ZombieSpeed() { return std::min(4.4f, 1.2f + game.round * 0.32f) * frand(0.85f, 1.15f); }
static float TearInterval() { return std::max(0.45f, 1.3f - game.round * 0.06f); }

void KillZombie(Zombie &z, bool head, bool fromNuke) {
    if (z.state == Z_DEAD) return;
    if (z.win >= 0 && windows[z.win].climber == (int)(&z - &zombies[0])) windows[z.win].climber = -1;
    bool wasChase = z.state == Z_CHASE;
    z.state = Z_DEAD;
    z.deadT = 0;
    SetClip(z, C_DIE);
    if (z.dog) {   // el perro estalla en llamas
        Vector3 c = Vector3Lerp(z.hipsPos, z.neckPos, 0.5f);
        Burst(c, Color{ 255, 120, 30, 255 }, 26, 3.5f, 0.07f, 6);
        Burst(c, Color{ 255, 60, 20, 255 }, 12, 2, 0.12f, 5);
        Burst(c, Color{ 40, 30, 30, 255 }, 10, 1.5f, 0.12f, 1);
        boomLightPos = Vector3{ c.x, 1.2f, c.z }; boomLightI = std::max(boomLightI, 14.0f);
        PlayAt("dogdie", z.pos, 25);
        game.lastDogPos = z.pos;
    } else {
        if (!fromNuke && frand(0, 1) < 0.5f) PlayAt("groan", z.pos, 20);
        if (head) { Burst(z.headPos, Color{ 140, 0, 0, 255 }, 22, 3, 0.07f); Burst(z.headPos, Color{ 90, 0, 0, 255 }, 10, 1.5f, 0.12f); }
        if (wasChase) AddDecal(1, Vector3{ z.pos.x, 0.0f, z.pos.z }, Vector3{ 0, 1, 0 });
    }
    if (fromNuke) return;
    game.kills++;
    if (head) game.heads++;
    AddPoints(head ? 100 : 60);
    if (wasChase && !z.dog && !Practice() && game.drops < 4 && frand(0, 1) < 0.035f) { DropPowerup(z.pos.x, z.pos.z); game.drops++; }
}

void DamageZombie(Zombie &z, float dmg, float headMul, bool head, Vector3 point, bool green) {
    if (z.state == Z_DEAD) return;
    {   // dirección y fuerza del impacto (para el muñeco de trapo)
        Vector3 d = Vector3Subtract(point, cam.position);
        float l = Vector3Length(d);
        z.hitDir = l > 0.01f ? Vector3Scale(d, 1 / l) : Vector3{ 0, 0, 0 };
        z.hitDir.y = std::max(z.hitDir.y, 0.0f) + 0.15f;
        z.hitPoint = point; z.hitAll = false;
        z.hitForce = clampf(dmg / 80.0f, 1.5f, 6.0f);
    }
    if (head) dmg *= headMul;
    if (T < game.insta) dmg = 1e9f;
    z.hp -= dmg;
    z.hitFlash = 0.12f;
    Burst(point, green ? Color{ 70, 255, 70, 255 } : Color{ 120, 0, 0, 255 }, head ? 10 : 5, 2.5f, 0.05f);
    if (z.hp <= 0) { KillZombie(z, head); ShowHit(true); }
    else { AddPoints(10); ShowHit(false); Play("hit"); }
}

// ---------- IA ----------
static void FaceTo(Zombie &z, float dx, float dz, float k) {
    float diff = atan2f(dx, dz) - z.yaw;
    diff = atan2f(sinf(diff), cosf(diff));
    z.yaw += diff * k;
}
static float MoveToward(Zombie &z, float tx, float tz, float dt, float mul = 1) {
    float dx = tx - z.pos.x, dz = tz - z.pos.z, d = hyp(dx, dz);
    if (d < 0.01f) return 0;
    float step = std::min(d, z.speed * mul * dt);
    z.pos.x += dx / d * step; z.pos.z += dz / d * step;
    FaceTo(z, dx, dz, dampk(8, dt));
    return d;
}
static void Attack(Zombie &z) {
    z.nextAttack = T + 1.1f;
    z.attackT = 0.9f;
    SetClip(z, C_ATTACK);
    z.clipT = 0.25f;
    PlayAt(z.dog ? "dog" : "groan", z.pos, 12);   // gruñido al golpear
    DamagePlayer(z.dog ? 25 : 40);
}

static bool UpdateDummy(Zombie &z, float dt, float pdx, float pdz);
static void UpdateZombie(Zombie &z, int zi, float dt) {
    z.hitFlash = std::max(0.0f, z.hitFlash - dt);
    z.blend = std::min(1.0f, z.blend + dt * 5);
    z.prevT += dt * z.rate;
    // ardiendo (bengala): pierde vida poco a poco
    if (z.burnT > 0 && z.state != Z_DEAD) {
        z.burnT -= dt;
        z.hp -= z.burnDps * dt;
        if (frand(0, 1) < dt * 25) Burst(Vector3Lerp(z.hipsPos, z.neckPos, frand(0, 1)), frand(0, 1) < 0.5f ? Color{ 255, 140, 40, 255 } : Color{ 255, 70, 20, 255 }, 1, 0.6f, 0.08f, 5);
        if (z.hp <= 0 || T < game.insta) { KillZombie(z, false); ShowHit(true); }
        if (z.burnT <= 0) z.burnDps = 0;
    }
    if (z.state == Z_DEAD) {
        // muñeco de trapo: se crea al morir con el empujón del último impacto
        if (!z.ragTried && !z.dog && z.y > -1.0f) {
            z.ragTried = true;
            Rig &r = RigOf(z);
            PoseZombie(z);
            bool flung = z.fling.x != 0 || z.fling.y != 0 || z.fling.z != 0;
            Vector3 push = Vector3Scale(z.hitDir, z.hitForce);
            bool topple = !flung && !z.hitAll;
            Vector3 vAll = flung ? z.fling : z.hitAll ? push : Vector3Scale(push, 0.8f);
            Vector3 hitV = flung || z.hitAll ? Vector3{ 0, 0, 0 } : push;
            z.rag = RagdollStart(r.model, r.pose, ZMatrix(z), z.yaw, z.scale, vAll, z.hitPoint, hitV, topple);
            if (z.rag) z.fling = Vector3{ 0, 0, 0 };
        }
        if (z.rag) {
            RagdollStep(*z.rag, dt);
            z.deadT += dt;
            if (z.deadT > 3.6f) z.y -= dt * 0.6f;
            if (z.deadT > 5.4f) z.remove = true;
            return;
        }
        // lanzado por el Cañón de Trueno: vuela hasta caer al suelo
        if (z.fling.x != 0 || z.fling.y != 0 || z.fling.z != 0) {
            z.pos.x += z.fling.x * dt; z.pos.z += z.fling.z * dt;
            z.y += z.fling.y * dt;
            z.fling.y -= 20 * dt;
            z.fling.x *= expf(-0.8f * dt); z.fling.z *= expf(-0.8f * dt);
            Vector3 before = z.pos;
            Collide(z.pos, 0.3f);
            if (before.x != z.pos.x) z.fling.x *= -0.3f;
            if (before.z != z.pos.z) z.fling.z *= -0.3f;
            if (z.y <= 0 && z.fling.y < 0) { z.y = 0; z.fling = Vector3{ 0, 0, 0 }; }
        }
        z.rate = 1.15f;
        z.clipT += dt * z.rate;
        z.deadT += dt;
        if (z.dog && z.deadT > 0.45f) z.remove = true;   // el perro se consume en llamas
        if (z.deadT > 3.2f) z.y -= dt * 0.6f;
        if (z.deadT > 5.0f) z.remove = true;
        return;
    }
    float pdx = player.pos.x - z.pos.x, pdz = player.pos.z - z.pos.z, pd = hyp(pdx, pdz);
    bool moving = false;
    if (z.attackT > 0) z.attackT -= dt;

    if (z.state == Z_RISE && z.dog) {
        // el perro llega con el rayo: tras un instante aparece corriendo
        z.riseT += dt;
        FaceTo(z, pdx, pdz, 1);
        if (z.riseT >= 0.6f) {
            z.state = Z_CHASE; z.y = 0;
            SetClip(z, C_RUN, true);
            Burst(Vector3{ z.pos.x, 0.5f, z.pos.z }, Color{ 255, 110, 30, 255 }, 16, 2.5f, 0.07f, 6);
            PlayAt("bark", z.pos, 30);
        }
    } else if (z.state == Z_RISE) {
        z.riseT += dt;
        z.y = std::min(0.0f, -1.9f + z.riseT * 1.2f);
        if (frand(0, 1) < 0.3f) Burst(Vector3{ z.pos.x + frand(-0.3f, 0.3f), 0.05f, z.pos.z + frand(-0.3f, 0.3f) }, Color{ 58, 42, 26, 255 }, 1, 1.5f);
        FaceTo(z, pdx, pdz, 1);
        SetClip(z, C_SCREAM);
        z.rate = 1.2f;
        if (z.y >= 0) { z.y = 0; z.state = Z_CHASE; }
    } else if (z.state == Z_APPROACH) {
        Window &w = windows[z.win];
        moving = MoveToward(z, w.outside.x, w.outside.y, dt) > 0.05f;
        if (hyp(w.outside.x - z.pos.x, w.outside.y - z.pos.z) < 0.3f) { z.state = Z_TEAR; z.tearT = T + TearInterval(); }
    } else if (z.state == Z_TEAR) {
        Window &w = windows[z.win];
        if (hyp(w.outside.x - z.pos.x, w.outside.y - z.pos.z) > 0.3f) MoveToward(z, w.outside.x, w.outside.y, dt, 0.5f);
        FaceTo(z, (float)w.nx, (float)w.nz, dampk(8, dt));
        if (w.boards > 0) {
            SetClip(z, C_ATTACK);
            z.rate = 1.3f;
            if (T >= z.tearT) { TearBoard(w); z.tearT = T + TearInterval(); }
        } else if (w.climber < 0) {
            w.climber = zi;
            z.state = Z_CLIMB; z.climbT = 0;
        } else SetClip(z, C_IDLE);
        if (pd < 1.7f && w.boards <= 2 && T >= z.nextAttack) Attack(z);
    } else if (z.state == Z_CLIMB) {
        Window &w = windows[z.win];
        z.climbT += dt / 0.9f;
        float t = std::min(1.0f, z.climbT);
        z.pos.x = lerpf(w.outside.x, w.inside.x, t); z.pos.z = lerpf(w.outside.y, w.inside.y, t);
        z.y = sinf(t * PI_F) * 0.75f;
        SetClip(z, C_WALK);
        z.rate = 1.2f;
        if (t >= 1) { z.y = 0; z.state = Z_CHASE; z.room = RoomIndex(w.room); w.climber = -1; }
    } else if (z.state == Z_CHASE && z.dummy >= 0) {
        moving = UpdateDummy(z, dt, pdx, pdz);
    } else if (z.state == Z_CHASE) {
        int r = RoomAt(z.pos.x, z.pos.z);
        if (r >= 0) z.room = r;
        float tx = player.pos.x, tz = player.pos.z;
        int nr = -1, nl = -1;
        bool path = z.room != player.room && NextRoom(z.room, player.room, nr, nl);
        if (path) {
            const Link &L = links[nl];
            bool curIsA = RoomIndex(L.a) == z.room;
            Vector2 sc = curIsA ? L.sideA : L.sideB, sn = curIsA ? L.sideB : L.sideA;
            float lateral = L.axis == 'z' ? fabsf(z.pos.z - L.z) : fabsf(z.pos.x - L.x);
            float depth = L.axis == 'z' ? fabsf(z.pos.x - L.x) : fabsf(z.pos.z - L.z);
            if (lateral > 0.9f && depth > 0.4f) { tx = sc.x; tz = sc.y; } else { tx = sn.x; tz = sn.y; }
        }
        float reach = z.dog ? 1.5f : 1.1f;   // el hocico del perro queda por delante
        if (pd > reach || path) moving = MoveToward(z, tx, tz, dt) > 0.05f;
        else FaceTo(z, pdx, pdz, dampk(8, dt));
        if (pd < reach + 0.2f && !path && T >= z.nextAttack) Attack(z);
        if (T > z.stuckCheckT) {
            if (Vector3Distance(z.pos, z.stuckPos) < 0.6f && pd > 3) { z.remove = true; game.toSpawn++; }
            z.stuckPos = z.pos;
            z.stuckCheckT = T + 6;
        }
    }

    // separación entre zombis
    if (z.state != Z_CLIMB) {
        for (auto &o : zombies) {
            if (&o == &z || o.state == Z_DEAD || o.state == Z_RISE || o.state == Z_CLIMB) continue;
            float ox = z.pos.x - o.pos.x, oz = z.pos.z - o.pos.z, od = hyp(ox, oz);
            if (od < 0.7f && od > 0.001f) { z.pos.x += ox / od * (0.7f - od) * 0.5f; z.pos.z += oz / od * (0.7f - od) * 0.5f; }
        }
    }
    if (z.state == Z_CHASE) Collide(z.pos, 0.35f);

    // animación según lo que hace
    if (z.state == Z_APPROACH || z.state == Z_CHASE) {
        if (z.attackT > 0) { SetClip(z, C_ATTACK); z.rate = 1.4f; }
        else if (moving) {
            if (z.dog && z.speed < 4) { SetClip(z, C_WALK, true); z.rate = clampf(z.speed / 2.0f, 0.7f, 1.6f); }
            else if (z.dog) { SetClip(z, C_RUN, true); z.rate = clampf(z.speed / 6.5f, 0.8f, 1.3f); }
            else if (z.runner) { SetClip(z, C_RUN, true); z.rate = clampf(z.speed / 3.2f, 0.8f, 1.4f); }
            else { SetClip(z, C_WALK, true); z.rate = clampf(z.speed / 1.1f, 0.7f, 1.9f); }
        } else { SetClip(z, C_IDLE, true); z.rate = 1; }
    }
    z.clipT += dt * z.rate;

    if (T > z.nextGroan) {
        z.nextGroan = T + (z.dog ? frand(1.2f, 3.0f) : frand(3, 8));
        if (pd < 25) PlayAt(z.dog ? "dog" : "groan", z.pos);
    }
}

void UpdateZombies(float dt) {
    for (size_t i = 0; i < zombies.size(); i++) UpdateZombie(zombies[i], (int)i, dt);
    for (int i = (int)zombies.size() - 1; i >= 0; i--) {
        if (!zombies[i].remove) continue;
        Zombie &z = zombies[i];
        if (z.win >= 0 && windows[z.win].climber == i) windows[z.win].climber = -1;
        if (z.rag) RagdollRelease(*z.rag);
        zombies.erase(zombies.begin() + i);
        // reajustar los índices de los que trepan
        for (auto &w : windows) if (w.climber > i) w.climber--;
    }
}

static Zombie NewZombie(float x, float z, int state, int win, int room) {
    Zombie zb;
    zb.pos = Vector3{ x, 0, z };
    zb.state = state; zb.win = win; zb.room = room;
    zb.hp = game.zHP;
    zb.speed = ZombieSpeed();
    zb.runner = zb.speed > 2.6f;
    zb.nextGroan = T + frand(1, 5);
    zb.stuckCheckT = T + 6; zb.stuckPos = zb.pos;
    zb.scale = frand(0.94f, 1.07f);
    int v = irand(4);
    Color tints[4] = { Color{ 225, 235, 215, 255 }, Color{ 200, 225, 190, 255 }, Color{ 215, 215, 205, 255 }, Color{ 190, 210, 200, 255 } };
    zb.tint = tints[v];
    zb.y = state == Z_RISE ? -1.9f : 0;
    zb.clip = state == Z_RISE ? C_SCREAM : (zb.runner ? C_RUN : C_WALK);
    zb.clipT = frand(0, 3);
    zb.yaw = frand(0, 6.28f);
    return zb;
}

// ---------- Campo de tiro ----------
// Dianas: unas quietas mirando al jugador y otras que caminan de lado a lado. Al morir reaparecen.
struct DummySlot { float x, z, patrol; float respawnT; bool dog; };
static std::vector<DummySlot> dummySlots;
static bool UpdateDummy(Zombie &z, float dt, float pdx, float pdz) {
    const DummySlot &d = dummySlots[z.dummy];
    if (d.patrol <= 0) { FaceTo(z, pdx, pdz, dampk(4, dt)); return false; }
    float tx = d.x + d.patrol * z.patrolDir;
    if (MoveToward(z, tx, d.z, dt) < 0.1f) z.patrolDir = -z.patrolDir;
    return true;
}
static Zombie NewZombie(float x, float z, int state, int win, int room);
static Zombie NewDog(float x, float z, int room);
void UpdatePractice(float dt) {
    (void)dt;
    if (dummySlots.empty()) {
        float s[][4] = { { -4, -14, 0 }, { 3, -16, 0 }, { -1, -21, 0 }, { 5, -26, 0 }, { -5, -31, 0 }, { 1, -38, 0 }, { -3, -43, 0 }, { 0, -27, 6.5f }, { 0, -35, 7 },
                         { 0, -18.5f, 5.5f, 1 } };   // la última es un perro del infierno
        for (auto &e : s) dummySlots.push_back({ e[0], e[1], e[2], 0, e[3] > 0 });
    }
    for (int i = 0; i < (int)dummySlots.size(); i++) {
        DummySlot &d = dummySlots[i];
        bool alive = false;
        for (auto &z : zombies) if (z.dummy == i && z.state != Z_DEAD) alive = true;
        if (alive) continue;
        if (d.respawnT == 0) d.respawnT = T + (T < 1 ? 0.3f * i : 2.5f);   // al empezar salen escalonadas
        else if (T >= d.respawnT) {
            if (d.dog) {
                Zombie dg = NewDog(d.x, d.z, 0);
                dg.dummy = i; dg.speed = 2.6f; dg.hp = 400;
                zombies.push_back(dg);
                d.respawnT = 0;
                continue;
            }
            Zombie zb = NewZombie(d.x, d.z, Z_RISE, -1, 0);
            zb.dummy = i;
            zb.speed = 1.3f; zb.runner = false;
            zb.hp = 600;
            zb.yaw = PI_F;
            zombies.push_back(zb);
            d.respawnT = 0;
        }
    }
    // munición infinita y granadas siempre llenas
    for (auto &w : player.weapons) w.reserve = Stats(w.id, w.pap).reserve;
    player.grenades = 4;
}
void ResetPractice() { dummySlots.clear(); }

void SpawnZombie() {
    struct Opt { int w; Vector2 p; int room; float d; };
    std::vector<Opt> opts;
    auto dist = [](float x, float z) { return hyp(x - player.pos.x, z - player.pos.z); };
    for (size_t i = 0; i < windows.size(); i++) {
        int r = RoomIndex(windows[i].room);
        if (Unlocked(r)) opts.push_back({ (int)i, {}, r, dist(windows[i].inside.x, windows[i].inside.y) });
    }
    extern std::vector<std::pair<int, Vector2>> GroundSpawns();
    for (auto &g : GroundSpawns()) {
        if (!Unlocked(g.first)) continue;
        float d = dist(g.second.x, g.second.y);
        if (d > 5) opts.push_back({ -1, g.second, g.first, d });
    }
    if (opts.empty()) return;
    std::sort(opts.begin(), opts.end(), [](const Opt &a, const Opt &b) { return a.d < b.d; });
    const Opt &o = opts[irand(std::min(4, (int)opts.size()))];
    if (o.w >= 0) {
        Window &w = windows[o.w];
        float out = frand(5, 9), lat = frand(-1.5f, 1.5f);
        zombies.push_back(NewZombie(w.outside.x - w.nx * out - w.nz * lat, w.outside.y - w.nz * out + w.nx * lat, Z_APPROACH, o.w, o.room));
    } else {
        zombies.push_back(NewZombie(o.p.x + frand(-0.4f, 0.4f), o.p.y + frand(-0.4f, 0.4f), Z_RISE, -1, o.room));
    }
}

// ---------- Perros del infierno ----------
static float DogHP() { return std::min(1600.0f, 220.0f + game.round * 45.0f); }
static Zombie NewDog(float x, float z, int room) {
    Zombie d = NewZombie(x, z, Z_RISE, -1, room);
    d.dog = true;
    d.y = 0; d.riseT = 0;
    d.hp = DogHP();
    d.speed = std::min(7.6f, 5.6f + game.round * 0.12f) * frand(0.92f, 1.08f);
    d.runner = true;
    d.scale = 0.34f * frand(0.95f, 1.08f);
    d.tint = WHITE;
    d.clip = C_RUN;
    d.nextGroan = T + frand(0.8f, 2.0f);
    LightningStrike(Vector3{ x, 0, z });
    return d;
}
// Aparece en un punto libre de una sala abierta, a media distancia del jugador
bool SpawnDog() {
    float bx = 0, bz = 0; int broom = -1; float bestScore = -1e9f;
    for (int tries = 0; tries < 40; tries++) {
        int r = irand((int)rooms.size());
        if (!Unlocked(r)) continue;
        const Room &rm = rooms[r];
        float x = frand(rm.x1 + 1.2f, rm.x2 - 1.2f), z = frand(rm.z1 + 1.2f, rm.z2 - 1.2f);
        if (InsideCollider(x, z, 0.7f)) continue;
        float d = hyp(x - player.pos.x, z - player.pos.z);
        float score = d < 5 ? -100 + d : -fabsf(d - 11);   // ni encima del jugador ni demasiado lejos
        if (score > bestScore) { bestScore = score; bx = x; bz = z; broom = r; }
    }
    if (broom < 0) return false;
    zombies.push_back(NewDog(bx, bz, broom));
    return true;
}
