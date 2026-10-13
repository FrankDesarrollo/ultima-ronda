// Prepara los sonidos reales (CC0) para el juego: mono 44.1 kHz 16 bits, recortados, normalizados y con fundido.
// Uso: sndprep.exe <carpeta descargas> <carpeta salida>
#include "raylib.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>

static std::vector<float> Load(const char *path) {
    Wave w = LoadWave(path);
    if (!w.data) { printf("NO SE PUDO CARGAR %s\n", path); return {}; }
    WaveFormat(&w, 44100, 32, 1);
    std::vector<float> s((float *)w.data, (float *)w.data + w.frameCount);
    UnloadWave(w);
    return s;
}
static void Save(std::vector<float> s, const char *path, float fadeFrac) {
    float peak = 1e-6f;
    for (float x : s) peak = fmaxf(peak, fabsf(x));
    size_t n = s.size(), fade = (size_t)(n * fadeFrac), fin = 88;
    for (size_t i = 0; i < n; i++) {
        float g = 0.9f / peak;
        if (i < fin) g *= (float)i / fin;
        if (i + fade >= n && fade) g *= powf((float)(n - i) / fade, 2.0f);
        s[i] *= g;
    }
    std::vector<short> d(n);
    for (size_t i = 0; i < n; i++) d[i] = (short)fmaxf(-32767, fminf(32767, s[i] * 32767));
    Wave w{ (unsigned)n, 44100, 16, 1, d.data() };
    ExportWave(w, path);
    printf("%-28s %.2f s\n", GetFileName(path), n / 44100.0f);
}
// desde el primer golpe (por encima del 15% del pico) y como mucho "len" segundos
static std::vector<float> Shot(const std::vector<float> &s, float len) {
    float peak = 0; for (float x : s) peak = fmaxf(peak, fabsf(x));
    size_t on = 0;
    while (on < s.size() && fabsf(s[on]) < peak * 0.15f) on++;
    on = on > 220 ? on - 220 : 0;
    size_t n = std::min(s.size() - on, (size_t)(len * 44100));
    return std::vector<float>(s.begin() + on, s.begin() + on + n);
}
// quita el silencio del principio y del final
static std::vector<float> Trim(const std::vector<float> &s, float maxLen) {
    float peak = 0; for (float x : s) peak = fmaxf(peak, fabsf(x));
    size_t a = 0, b = s.size();
    while (a < b && fabsf(s[a]) < peak * 0.03f) a++;
    while (b > a && fabsf(s[b - 1]) < peak * 0.03f) b--;
    b = std::min(b, a + (size_t)(maxLen * 44100));
    return std::vector<float>(s.begin() + a, s.begin() + b);
}

// trozos separados por silencios (thr: umbral relativo al pico, gap: silencio mínimo en s)
static int Split(const std::vector<float> &s, const std::string &prefix, int k, float minLen, float maxLen, float thr, float gap, float pitch) {
    float peak = 0; for (float x : s) peak = fmaxf(peak, fabsf(x));
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && fabsf(s[i]) < peak * thr) i++;
        size_t a = i > 600 ? i - 600 : 0, quiet = 0;
        while (i < s.size() && quiet < (size_t)(gap * 44100)) { quiet = fabsf(s[i]) < peak * thr ? quiet + 1 : 0; i++; }
        size_t b = i - quiet + 1500 < s.size() ? i - quiet + 1500 : s.size();
        if (b > a + (size_t)(minLen * 44100)) {
            std::vector<float> c(s.begin() + a, s.begin() + std::min(b, a + (size_t)(maxLen * 44100)));
            if (pitch != 1) {   // más grave: se estira la muestra
                std::vector<float> r((size_t)(c.size() / pitch));
                for (size_t j = 0; j < r.size(); j++) { float p = j * pitch; size_t q = (size_t)p; float f = p - q; r[j] = c[q] * (1 - f) + (q + 1 < c.size() ? c[q + 1] : 0) * f; }
                c = r;
            }
            Save(c, TextFormat("%s%d.wav", prefix.c_str(), k++), 0.2f);
        }
    }
    return k;
}

int main(int argc, char **argv) {
    if (argc < 3) return 1;
    SetTraceLogLevel(LOG_WARNING);
    std::string in = argv[1], out = argv[2], fa = in + "/fa/Prepared SFX Library/";
    struct { const char *src, *dst; float len; } shots[] = {
        { "1911/A_42P.wav", "pistol", 0.9f },
        { "Carl Gustav M45/G_31P.wav", "smg", 0.55f },
        { "AR-15/D_32P.wav", "rifle", 0.8f },
        { "AK-47/C_28P.wav", "ak47", 0.85f },
        { "Nova/O_21P.wav", "shotgun", 1.2f },
        { "Tikka/W_29P.wav", "sniper", 1.6f },
        { "Walther PPQ/X_39P.wav", "luger", 0.9f },
        { "PPSh/P_30P.wav", "suomi", 0.5f },
        { "Smith & Wesson 642/V_27P.wav", "magnum", 1.1f },
        { "Model 12/K_22P.wav", "sawed", 1.2f },
        { "Ruger Mark III/R_35P.wav", "flare", 0.6f },
    };
    for (auto &s : shots) Save(Shot(Load((fa + s.src).c_str()), s.len), (out + "/" + s.dst + ".wav").c_str(), 0.6f);
    Save(Trim(Load((in + "/reload.wav").c_str()), 2.0f), (out + "/reload_pistol.wav").c_str(), 0.1f);
    Save(Trim(Load((in + "/assaultriflereload1_0.wav").c_str()), 2.4f), (out + "/reload_rifle.wav").c_str(), 0.1f);
    Save(Trim(Load((in + "/gunreload1.wav").c_str()), 2.4f), (out + "/reload_smg.wav").c_str(), 0.1f);
    // zombis: 24 sonidos sueltos
    int k = 0;
    for (int i = 1; i <= 24; i++) {
        std::vector<float> z = Trim(Load(TextFormat("%s/zomb/zombies/zombie-%d.wav", in.c_str(), i)), 2.5f);
        if (z.size() > 4410) Save(z, TextFormat("%s/zombie%d.wav", out.c_str(), k++), 0.25f);
    }
    // gemidos largos: se separan por silencios de más de 0.25 s
    const char *moans[] = { "darsycho__zombie-moans_0.ogg", "hungry-ghoul_0.ogg" };
    for (const char *mf : moans) {
        std::vector<float> s = Load((in + "/" + mf).c_str());
        float peak = 0; for (float x : s) peak = fmaxf(peak, fabsf(x));
        size_t i = 0;
        while (i < s.size()) {
            while (i < s.size() && fabsf(s[i]) < peak * 0.05f) i++;
            size_t a = i, quiet = 0;
            while (i < s.size() && quiet < 11025) { quiet = fabsf(s[i]) < peak * 0.05f ? quiet + 1 : 0; i++; }
            size_t b = i - quiet;
            if (b > a + 17640) Save(std::vector<float>(s.begin() + a, s.begin() + std::min(b, a + (size_t)(3.0f * 44100))), TextFormat("%s/zombie%d.wav", out.c_str(), k++), 0.25f);
        }
    }
    printf("zombis: %d\n", k);
    // perros del infierno (tercer argumento: carpeta con los sonidos de perro)
    if (argc > 3) {
        std::string d = argv[3];
        int n = 0;
        const char *sn[] = { "monster-snarls_0.ogg", "monster-snarls-2_0.ogg", "monsters-snarls-3_0.ogg", "monster-snarl-attack.ogg", "monster-snarl-5.ogg" };
        for (const char *f : sn) n = Split(Load((d + "/" + f).c_str()), out + "/dog_snarl", n, 0.35f, 1.6f, 0.12f, 0.25f, 1.0f);
        printf("gruñidos: %d\n", n);
        n = Split(Load((d + "/dog/Dog/Raw/Dog Bark.wav").c_str()), out + "/dog_bark", 0, 0.12f, 0.8f, 0.1f, 0.15f, 0.82f);
        printf("ladridos: %d\n", n);
    }
}
