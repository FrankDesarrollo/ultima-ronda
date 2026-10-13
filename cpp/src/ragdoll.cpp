// Muñecos de trapo (ragdoll) para los zombis muertos.
// Barato a propósito: 15 partículas con integración de Verlet y restricciones de distancia.
// El esqueleto completo (52 huesos) sigue a 11 "segmentos" rígidos que se calculan con esas partículas.
// Los cuerpos que se quedan quietos se duermen y dejan de calcularse.
#include "game.h"

enum { P_HIPS, P_CHEST, P_HEAD, P_LARM, P_LFORE, P_LHAND, P_RARM, P_RFORE, P_RHAND, P_LUP, P_LLEG, P_LFOOT, P_RUP, P_RLEG, P_RFOOT, NP };
enum { S_PELVIS, S_CHEST, S_HEAD, S_LUA, S_LFA, S_RUA, S_RFA, S_LTH, S_LSH, S_RTH, S_RSH, NS };
static const char *PNAME[NP] = { "Hips", "Spine2", "Head", "LeftArm", "LeftForeArm", "LeftHand", "RightArm", "RightForeArm", "RightHand",
                                 "LeftUpLeg", "LeftLeg", "LeftFoot", "RightUpLeg", "RightLeg", "RightFoot" };
// segmentos de las extremidades: partícula inicial, final y segmento padre
struct LimbDef { int seg, a, b, parent; };
static const LimbDef LIMBS[] = {
    { S_HEAD, P_CHEST, P_HEAD, S_CHEST },
    { S_LUA, P_LARM, P_LFORE, S_CHEST }, { S_LFA, P_LFORE, P_LHAND, S_LUA },
    { S_RUA, P_RARM, P_RFORE, S_CHEST }, { S_RFA, P_RFORE, P_RHAND, S_RUA },
    { S_LTH, P_LUP, P_LLEG, S_PELVIS }, { S_LSH, P_LLEG, P_LFOOT, S_LTH },
    { S_RTH, P_RUP, P_RLEG, S_PELVIS }, { S_RSH, P_RLEG, P_RFOOT, S_RTH },
};
static const struct { const char *bone; int seg; } BONESEG[] = {
    { "Hips", S_PELVIS }, { "Spine", S_PELVIS }, { "Spine1", S_CHEST }, { "Spine2", S_CHEST }, { "LeftShoulder", S_CHEST }, { "RightShoulder", S_CHEST },
    { "Neck", S_HEAD }, { "Head", S_HEAD },
    { "LeftArm", S_LUA }, { "LeftForeArm", S_LFA }, { "LeftHand", S_LFA }, { "RightArm", S_RUA }, { "RightForeArm", S_RFA }, { "RightHand", S_RFA },
    { "LeftUpLeg", S_LTH }, { "LeftLeg", S_LSH }, { "LeftFoot", S_LSH }, { "RightUpLeg", S_RTH }, { "RightLeg", S_RSH }, { "RightFoot", S_RSH },
};
// barras: huesos + refuerzos para que el tronco no se deforme
static const int STICKS[][2] = {
    { P_HIPS, P_CHEST }, { P_CHEST, P_HEAD }, { P_CHEST, P_LARM }, { P_LARM, P_LFORE }, { P_LFORE, P_LHAND }, { P_CHEST, P_RARM }, { P_RARM, P_RFORE }, { P_RFORE, P_RHAND },
    { P_HIPS, P_LUP }, { P_LUP, P_LLEG }, { P_LLEG, P_LFOOT }, { P_HIPS, P_RUP }, { P_RUP, P_RLEG }, { P_RLEG, P_RFOOT },
    { P_LARM, P_RARM }, { P_LUP, P_RUP }, { P_LUP, P_CHEST }, { P_RUP, P_CHEST }, { P_LARM, P_HIPS }, { P_RARM, P_HIPS }, { P_HEAD, P_LARM }, { P_HEAD, P_RARM },
    { P_LARM, P_RUP }, { P_RARM, P_LUP },
};
static const int NSTICK = sizeof(STICKS) / sizeof(STICKS[0]);
// distancias mínimas (fracción del reposo): rodillas, codos y columna no se pliegan del todo
static const struct { int a, b; float k; } MINS[] = {
    { P_LUP, P_LFOOT, 0.6f }, { P_RUP, P_RFOOT, 0.6f }, { P_LARM, P_LHAND, 0.5f }, { P_RARM, P_RHAND, 0.5f },
    { P_HIPS, P_HEAD, 0.85f }, { P_LFOOT, P_RFOOT, 0.25f }, { P_HEAD, P_LUP, 0.8f }, { P_HEAD, P_RUP, 0.8f },
};
static const int NMIN = sizeof(MINS) / sizeof(MINS[0]);
static const float RADIUS[NP] = { 0.16f, 0.17f, 0.13f, 0.08f, 0.07f, 0.06f, 0.08f, 0.07f, 0.06f, 0.1f, 0.08f, 0.07f, 0.1f, 0.08f, 0.07f };
static const float STEP = 1.0f / 60;

// Correspondencia del esqueleto del modelo (se calcula una vez)
static std::vector<int> boneSeg;
static int pBone[NP];
static int skelBones = -1;
static bool skelOk = false;

static const char *Suffix(const char *n) { const char *c = strrchr(n, ':'); return c ? c + 1 : n; }
static void BuildSkeletonMap(const Model &m) {
    skelBones = m.boneCount;
    skelOk = true;
    for (int i = 0; i < NP; i++) {
        pBone[i] = -1;
        for (int b = 0; b < m.boneCount; b++) if (!strcmp(Suffix(m.bones[b].name), PNAME[i])) pBone[i] = b;
        if (pBone[i] < 0) skelOk = false;
    }
    boneSeg.assign(m.boneCount, -1);
    for (int b = 0; b < m.boneCount; b++) {
        // el segmento del propio hueso o el del antepasado más cercano que tenga uno
        for (int a = b, guard = 0; a >= 0 && guard < 64; a = m.bones[a].parent, guard++) {
            int s = -1;
            for (auto &e : BONESEG) if (!strcmp(Suffix(m.bones[a].name), e.bone)) s = e.seg;
            if (s >= 0) { boneSeg[b] = s; break; }
        }
        if (boneSeg[b] < 0) boneSeg[b] = S_PELVIS;
    }
}

struct Ragdoll {
    Vector3 p[NP], q[NP], p0[NP];
    float len[NSTICK], minLen[NMIN];
    Quaternion frame0[2];                        // bases de reposo de pelvis y pecho
    std::vector<Vector3> bpos0; std::vector<Quaternion> brot0; std::vector<Vector3> bscale;
    std::vector<Transform> cache;                // pose final (espacio del modelo)
    Matrix invZm; Quaternion invYaw;
    float acc = 0, still = 0, age = 0;
    bool sleeping = false;
};

static int activeRagdolls = 0;   // se recalcula cada fotograma
static Quaternion Frame(Vector3 up, Vector3 side) {
    Vector3 y = Vector3Normalize(up);
    Vector3 x = Vector3Subtract(side, Vector3Scale(y, Vector3DotProduct(side, y)));
    if (Vector3Length(x) < 1e-4f) x = Vector3{ 1, 0, 0 };
    x = Vector3Normalize(x);
    Vector3 z = Vector3CrossProduct(x, y);
    Matrix m = MatrixIdentity();
    m.m0 = x.x; m.m1 = x.y; m.m2 = x.z;
    m.m4 = y.x; m.m5 = y.y; m.m6 = y.z;
    m.m8 = z.x; m.m9 = z.y; m.m10 = z.z;
    return QuaternionNormalize(QuaternionFromMatrix(m));
}

// Pose del esqueleto completo a partir de las partículas
static void SolvePose(Ragdoll &r) {
    Quaternion seg[NS]; Vector3 o0[NS], o1[NS];
    Quaternion f1 = Frame(Vector3Subtract(r.p[P_CHEST], r.p[P_HIPS]), Vector3Subtract(r.p[P_RUP], r.p[P_LUP]));
    seg[S_PELVIS] = QuaternionMultiply(f1, QuaternionInvert(r.frame0[0]));
    o0[S_PELVIS] = r.p0[P_HIPS]; o1[S_PELVIS] = r.p[P_HIPS];
    Quaternion f2 = Frame(Vector3Subtract(r.p[P_CHEST], r.p[P_HIPS]), Vector3Subtract(r.p[P_RARM], r.p[P_LARM]));
    seg[S_CHEST] = QuaternionMultiply(f2, QuaternionInvert(r.frame0[1]));
    o0[S_CHEST] = r.p0[P_CHEST]; o1[S_CHEST] = r.p[P_CHEST];
    for (auto &l : LIMBS) {
        Quaternion qp = seg[l.parent];
        Vector3 d0 = Vector3RotateByQuaternion(Vector3Subtract(r.p0[l.b], r.p0[l.a]), qp);
        Vector3 d1 = Vector3Subtract(r.p[l.b], r.p[l.a]);
        Quaternion qa = QuaternionFromVector3ToVector3(Vector3Normalize(d0), Vector3Normalize(d1));
        seg[l.seg] = QuaternionMultiply(qa, qp);
        o0[l.seg] = r.p0[l.a]; o1[l.seg] = r.p[l.a];
    }
    int n = (int)r.bpos0.size();
    r.cache.resize(n);
    for (int b = 0; b < n; b++) {
        int s = boneSeg[b];
        Vector3 wp = Vector3Add(Vector3RotateByQuaternion(Vector3Subtract(r.bpos0[b], o0[s]), seg[s]), o1[s]);
        Quaternion wr = QuaternionMultiply(seg[s], r.brot0[b]);
        r.cache[b].translation = Vector3Transform(wp, r.invZm);
        r.cache[b].rotation = QuaternionNormalize(QuaternionMultiply(r.invYaw, wr));
        r.cache[b].scale = r.bscale[b];
    }
}

// pose: pose actual del modelo (espacio del modelo); zm: matriz del zombi; yaw: giro del zombi
// vAll: velocidad de todo el cuerpo; hit/hitV: punto del impacto y su empujón extra
std::shared_ptr<Ragdoll> RagdollStart(const Model &m, const std::vector<Transform> &pose, Matrix zm, float yaw, float scale,
                                      Vector3 vAll, Vector3 hit, Vector3 hitV, bool topple) {
    if (skelBones != m.boneCount) BuildSkeletonMap(m);
    if (!skelOk || activeRagdolls >= 14) return nullptr;   // límite para no gastar de más
    auto r = std::make_shared<Ragdoll>();
    Quaternion yq = QuaternionFromAxisAngle(Vector3{ 0, 1, 0 }, yaw);
    r->invZm = MatrixInvert(zm);
    r->invYaw = QuaternionInvert(yq);
    int n = m.boneCount;
    r->bpos0.resize(n); r->brot0.resize(n); r->bscale.resize(n);
    for (int b = 0; b < n; b++) {
        r->bpos0[b] = Vector3Transform(pose[b].translation, zm);
        r->brot0[b] = QuaternionMultiply(yq, pose[b].rotation);
        r->bscale[b] = pose[b].scale;
    }
    int nearest = 0; float nd = 1e9f;
    for (int i = 0; i < NP; i++) {
        r->p0[i] = r->p[i] = r->bpos0[pBone[i]];
        float d = Vector3Distance(r->p[i], hit);
        if (d < nd) { nd = d; nearest = i; }
    }
    for (int i = 0; i < NP; i++) {
        // topple: el empujón va sobre todo al tronco y la cabeza, así el cuerpo se vuelca en vez de desplomarse
        float w = !topple ? 1.0f : i == P_HIPS ? 0.45f : i >= P_LUP ? 0.05f : 1.0f;
        Vector3 v = Vector3Scale(vAll, w);
        if (i == nearest) v = Vector3Add(v, hitV);
        else if (nd < 1) v = Vector3Add(v, Vector3Scale(hitV, 0.35f));   // el golpe arrastra un poco al resto
        r->q[i] = Vector3Subtract(r->p[i], Vector3Scale(v, STEP));
    }
    for (int k = 0; k < NSTICK; k++) r->len[k] = Vector3Distance(r->p[STICKS[k][0]], r->p[STICKS[k][1]]);
    for (int k = 0; k < NMIN; k++) r->minLen[k] = Vector3Distance(r->p[MINS[k].a], r->p[MINS[k].b]) * MINS[k].k;
    r->frame0[0] = Frame(Vector3Subtract(r->p[P_CHEST], r->p[P_HIPS]), Vector3Subtract(r->p[P_RUP], r->p[P_LUP]));
    r->frame0[1] = Frame(Vector3Subtract(r->p[P_CHEST], r->p[P_HIPS]), Vector3Subtract(r->p[P_RARM], r->p[P_LARM]));
    (void)scale;
    SolvePose(*r);
    activeRagdolls++;
    return r;
}

static void Simulate(Ragdoll &r) {
    float moved = 0;
    for (int i = 0; i < NP; i++) {
        Vector3 v = Vector3Scale(Vector3Subtract(r.p[i], r.q[i]), 0.995f);
        r.q[i] = r.p[i];
        r.p[i] = Vector3Add(r.p[i], v);
        r.p[i].y -= 9.8f * STEP * STEP;
        moved = std::max(moved, Vector3Length(v));
    }
    for (int it = 0; it < 6; it++) {
        for (int k = 0; k < NSTICK; k++) {
            Vector3 &a = r.p[STICKS[k][0]], &b = r.p[STICKS[k][1]];
            Vector3 d = Vector3Subtract(b, a);
            float l = Vector3Length(d);
            if (l < 1e-5f) continue;
            Vector3 c = Vector3Scale(d, 0.5f * (l - r.len[k]) / l);
            a = Vector3Add(a, c); b = Vector3Subtract(b, c);
        }
        for (int k = 0; k < NMIN; k++) {
            Vector3 &a = r.p[MINS[k].a], &b = r.p[MINS[k].b];
            Vector3 d = Vector3Subtract(b, a);
            float l = Vector3Length(d);
            if (l >= r.minLen[k] || l < 1e-5f) continue;
            Vector3 c = Vector3Scale(d, 0.5f * (l - r.minLen[k]) / l);
            a = Vector3Add(a, c); b = Vector3Subtract(b, c);
        }
        for (int i = 0; i < NP; i++) {
            Vector3 &p = r.p[i];
            if (p.y < RADIUS[i]) {   // suelo con rozamiento
                p.y = RADIUS[i];
                r.q[i].x = p.x - (p.x - r.q[i].x) * 0.6f;
                r.q[i].z = p.z - (p.z - r.q[i].z) * 0.6f;
                if (r.q[i].y < p.y) r.q[i].y = p.y - (p.y - r.q[i].y) * 0.2f;
            }
        }
    }
    for (int i = 0; i < NP; i++) Collide(r.p[i], RADIUS[i]);   // paredes, cajas y máquinas
    r.still = moved < 0.0012f ? r.still + STEP : 0;
}

// Avanza la física; devuelve false si está dormido (no hay que recalcular nada)
bool RagdollStep(Ragdoll &r, float dt) {
    if (r.sleeping) return false;
    r.age += dt;
    r.acc = std::min(r.acc + dt, STEP * 3);
    while (r.acc >= STEP) { Simulate(r); r.acc -= STEP; }
    if (r.still > 0.35f || r.age > 4.0f) { r.sleeping = true; activeRagdolls = std::max(0, activeRagdolls - 1); }
    SolvePose(r);
    return true;
}
void RagdollRelease(Ragdoll &r) { if (!r.sleeping) { r.sleeping = true; activeRagdolls = std::max(0, activeRagdolls - 1); } }
const std::vector<Transform> &RagdollPose(const Ragdoll &r) { return r.cache; }
Vector3 RagdollPoint(const Ragdoll &r, int which) { return r.p[which == 0 ? P_HEAD : which == 1 ? P_HIPS : P_CHEST]; }
void RagdollResetCount() { activeRagdolls = 0; }
