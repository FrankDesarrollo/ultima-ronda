// Carga de animaciones esqueléticas desde archivos glTF que solo traen el esqueleto
// (formato Mixamo). Los huesos se emparejan por nombre ignorando el prefijo "algo:".
#include "anim.h"
#include "raymath.h"
#include "external/cgltf.h"
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <functional>

static std::string BoneSuffix(const char *n) {
    if (!n) return "";
    const char *c = strrchr(n, ':');
    return c ? std::string(c + 1) : std::string(n);
}

static Matrix FromCgltf(const float *m) {
    return Matrix{ m[0], m[4], m[8], m[12],
                   m[1], m[5], m[9], m[13],
                   m[2], m[6], m[10], m[14],
                   m[3], m[7], m[11], m[15] };
}

static Matrix NodeWorld(cgltf_node *n) {
    if (!n) return MatrixIdentity();
    float w[16];
    cgltf_node_transform_world(n, w);
    return FromCgltf(w);
}

static Matrix TRS(Vector3 t, Quaternion r, Vector3 s) {
    return MatrixMultiply(MatrixMultiply(MatrixScale(s.x, s.y, s.z), QuaternionToMatrix(r)), MatrixTranslate(t.x, t.y, t.z));
}

// Datos de reposo del esqueleto del modelo
struct Rest {
    int count = 0;
    std::vector<Vector3> t, s;
    std::vector<Quaternion> r;
    std::vector<int> parent;
    std::vector<Matrix> rootParent;  // mundo del padre (fuera del esqueleto) de los huesos raíz
    std::vector<std::string> names;
};

static bool LoadRest(const char *modelPath, Rest &rest) {
    cgltf_options opt = {};
    cgltf_data *data = nullptr;
    if (cgltf_parse_file(&opt, modelPath, &data) != cgltf_result_success) return false;
    if (data->skins_count == 0) { cgltf_free(data); return false; }
    cgltf_skin &skin = data->skins[0];
    rest.count = (int)skin.joints_count;
    for (int i = 0; i < rest.count; i++) {
        cgltf_node *n = skin.joints[i];
        rest.t.push_back(n->has_translation ? Vector3{ n->translation[0], n->translation[1], n->translation[2] } : Vector3{ 0, 0, 0 });
        rest.r.push_back(n->has_rotation ? Quaternion{ n->rotation[0], n->rotation[1], n->rotation[2], n->rotation[3] } : QuaternionIdentity());
        rest.s.push_back(n->has_scale ? Vector3{ n->scale[0], n->scale[1], n->scale[2] } : Vector3{ 1, 1, 1 });
        int p = -1;
        for (int j = 0; j < rest.count; j++) if (skin.joints[j] == n->parent) p = j;
        rest.parent.push_back(p);
        rest.rootParent.push_back(p < 0 ? NodeWorld(n->parent) : MatrixIdentity());
        rest.names.push_back(BoneSuffix(n->name));
    }
    cgltf_free(data);
    return true;
}

struct Channel {
    std::vector<float> times;
    std::vector<float> values;  // 3 o 4 floats por clave
    int comps = 0;
};

static void Sample(const Channel &c, float t, float *out) {
    size_t n = c.times.size();
    if (n == 0) return;
    if (t <= c.times[0] || n == 1) { memcpy(out, &c.values[0], c.comps * sizeof(float)); return; }
    if (t >= c.times[n - 1]) { memcpy(out, &c.values[(n - 1) * c.comps], c.comps * sizeof(float)); return; }
    size_t k = 0;
    while (k + 1 < n && c.times[k + 1] < t) k++;
    float a = (t - c.times[k]) / (c.times[k + 1] - c.times[k]);
    const float *v0 = &c.values[k * c.comps], *v1 = &c.values[(k + 1) * c.comps];
    if (c.comps == 4) {
        Quaternion q = QuaternionSlerp(Quaternion{ v0[0], v0[1], v0[2], v0[3] }, Quaternion{ v1[0], v1[1], v1[2], v1[3] }, a);
        out[0] = q.x; out[1] = q.y; out[2] = q.z; out[3] = q.w;
    } else {
        for (int i = 0; i < c.comps; i++) out[i] = v0[i] + (v1[i] - v0[i]) * a;
    }
}

bool BakeAnimation(const Model &model, const char *modelPath, const char *animPath, float fps, bool inPlace, ModelAnimation *outAnim, const char *animName) {
    static Rest rest;
    static std::string restPath;
    if (restPath != modelPath) {
        rest = Rest();
        if (!LoadRest(modelPath, rest)) return false;
        restPath = modelPath;
    }
    if (rest.count != model.boneCount) return false;

    cgltf_options opt = {};
    cgltf_data *data = nullptr;
    if (cgltf_parse_file(&opt, animPath, &data) != cgltf_result_success) return false;
    if (cgltf_load_buffers(&opt, data, animPath) != cgltf_result_success || data->animations_count == 0) { cgltf_free(data); return false; }

    // con animName se elige la animación por nombre ("Gallop" vale también para "Armature|Gallop")
    cgltf_animation *pick = &data->animations[0];
    if (animName) {
        pick = nullptr;
        for (size_t i = 0; i < data->animations_count && !pick; i++) {
            const char *n = data->animations[i].name ? data->animations[i].name : "";
            const char *bar = strrchr(n, '|');
            if (!strcmp(n, animName) || (bar && !strcmp(bar + 1, animName))) pick = &data->animations[i];
        }
        if (!pick) { cgltf_free(data); return false; }
    }
    cgltf_animation &an = *pick;
    std::vector<Channel> rot(rest.count), tra(rest.count);
    cgltf_node *hipsNode = nullptr;
    float duration = 0;
    for (size_t ci = 0; ci < an.channels_count; ci++) {
        cgltf_animation_channel &ch = an.channels[ci];
        if (!ch.target_node) continue;
        std::string nm = BoneSuffix(ch.target_node->name);
        int bone = -1;
        for (int i = 0; i < rest.count; i++) if (rest.names[i] == nm) bone = i;
        if (bone < 0) continue;
        Channel *dst = nullptr;
        int comps = 0;
        if (ch.target_path == cgltf_animation_path_type_rotation) { dst = &rot[bone]; comps = 4; }
        else if (ch.target_path == cgltf_animation_path_type_translation) { dst = &tra[bone]; comps = 3; if (rest.parent[bone] < 0) hipsNode = ch.target_node; }
        else continue;
        cgltf_accessor *in = ch.sampler->input, *out = ch.sampler->output;
        dst->comps = comps;
        dst->times.resize(in->count);
        dst->values.resize(out->count * comps);
        for (size_t k = 0; k < in->count; k++) cgltf_accessor_read_float(in, k, &dst->times[k], 1);
        for (size_t k = 0; k < out->count; k++) cgltf_accessor_read_float(out, k, &dst->values[k * comps], comps);
        if (!dst->times.empty()) duration = fmaxf(duration, dst->times.back());
    }

    // La cadera (hueso raíz) se pasa por el espacio del mundo para que encaje en el modelo
    int root = -1;
    for (int i = 0; i < rest.count; i++) if (rest.parent[i] < 0) { root = i; break; }
    Matrix animRootParent = hipsNode ? NodeWorld(hipsNode->parent) : MatrixIdentity();
    Vector3 apT; Quaternion apR; Vector3 apS;
    MatrixDecompose(animRootParent, &apT, &apR, &apS);
    Vector3 mpT; Quaternion mpR; Vector3 mpS;
    MatrixDecompose(rest.rootParent[root], &mpT, &mpR, &mpS);
    Matrix modelRootParentInv = MatrixInvert(rest.rootParent[root]);
    Vector3 restHipsWorld = Vector3Transform(rest.t[root], rest.rootParent[root]);
    float scaleK = 1.0f;
    Vector3 animHips0{ 0, 0, 0 };
    if (!tra[root].times.empty()) {
        float v[3]; Sample(tra[root], 0, v);
        animHips0 = Vector3Transform(Vector3{ v[0], v[1], v[2] }, animRootParent);
        if (fabsf(animHips0.y) > 1e-4f) scaleK = restHipsWorld.y / animHips0.y;
    }

    int frames = (int)(duration * fps) + 1;
    if (frames < 1) frames = 1;
    ModelAnimation a = {};
    a.boneCount = rest.count;
    a.frameCount = frames;
    a.bones = (BoneInfo *)malloc(sizeof(BoneInfo) * rest.count);
    memcpy(a.bones, model.bones, sizeof(BoneInfo) * rest.count);
    a.framePoses = (Transform **)malloc(sizeof(Transform *) * frames);
    strncpy(a.name, GetFileNameWithoutExt(animPath), 31);

    std::vector<Matrix> local(rest.count), global(rest.count);
    std::vector<char> done(rest.count);
    for (int f = 0; f < frames; f++) {
        float t = f / fps;
        for (int i = 0; i < rest.count; i++) {
            Vector3 T = rest.t[i]; Quaternion R = rest.r[i];
            float v[4];
            if (!rot[i].times.empty()) { Sample(rot[i], t, v); R = Quaternion{ v[0], v[1], v[2], v[3] }; }
            if (i == root) {
                if (!rot[i].times.empty()) R = QuaternionMultiply(QuaternionInvert(mpR), QuaternionMultiply(apR, R));
                if (!tra[i].times.empty()) {
                    Sample(tra[i], t, v);
                    Vector3 w = Vector3Transform(Vector3{ v[0], v[1], v[2] }, animRootParent);
                    w = Vector3Scale(Vector3Subtract(w, Vector3{ animHips0.x, 0, animHips0.z }), scaleK);
                    if (inPlace) { w.x = 0; w.z = 0; }
                    w = Vector3Add(w, Vector3{ restHipsWorld.x, 0, restHipsWorld.z });
                    T = Vector3Transform(w, modelRootParentInv);
                }
            }
            local[i] = TRS(T, R, rest.s[i]);
            done[i] = 0;
        }
        std::function<void(int)> solve = [&](int i) {
            if (done[i]) return;
            int p = rest.parent[i];
            if (p >= 0) { solve(p); global[i] = MatrixMultiply(local[i], global[p]); }
            else global[i] = MatrixMultiply(local[i], rest.rootParent[i]);
            done[i] = 1;
        };
        a.framePoses[f] = (Transform *)malloc(sizeof(Transform) * rest.count);
        for (int i = 0; i < rest.count; i++) {
            solve(i);
            MatrixDecompose(global[i], &a.framePoses[f][i].translation, &a.framePoses[f][i].rotation, &a.framePoses[f][i].scale);
        }
    }
    cgltf_free(data);
    *outAnim = a;
    return true;
}

// Matrices inversas de unión del esqueleto (glTF), en el orden de los huesos del modelo
bool LoadInverseBind(const char *path, std::vector<Matrix> &out) {
    cgltf_options opt = {};
    cgltf_data *data = nullptr;
    if (cgltf_parse_file(&opt, path, &data) != cgltf_result_success) return false;
    bool ok = cgltf_load_buffers(&opt, data, path) == cgltf_result_success && data->skins_count > 0 && data->skins[0].inverse_bind_matrices;
    if (ok) {
        cgltf_skin &skin = data->skins[0];
        out.resize(skin.joints_count);
        // raylib ya aplicó a los vértices la transformación del nodo de la malla: se deshace aquí
        Matrix undo = MatrixIdentity();
        for (size_t n = 0; n < data->nodes_count; n++)
            if (data->nodes[n].mesh && data->nodes[n].skin) { undo = MatrixInvert(NodeWorld(&data->nodes[n])); break; }
        for (size_t i = 0; i < skin.joints_count; i++) {
            float m[16];
            cgltf_accessor_read_float(skin.inverse_bind_matrices, i, m, 16);
            out[i] = MatrixMultiply(undo, FromCgltf(m));
        }
    }
    cgltf_free(data);
    return ok;
}
