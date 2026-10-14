// Sonido sintetizado (sin archivos): efectos y música de fondo.
#include "game.h"
#include <map>

static const int SR = 44100;
struct Buf {
    std::vector<float> s;
    void Ensure(size_t n) { if (s.size() < n) s.resize(n, 0.0f); }
};

// Envolvente: subida exponencial a "peak" y caída a casi cero en "decay" segundos
static float Env(float t, float a, float peak, float decay) {
    if (t < 0) return 0;
    if (t < a) return 0.0001f * powf(peak / 0.0001f, t / a);
    float k = (t - a) / decay;
    if (k >= 1) return 0;
    return peak * powf(0.0001f / peak, k);
}

// Filtro biquad (RBJ)
struct Biquad {
    float b0, b1, b2, a1, a2, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    Biquad(int type, float f, float q = 0.7071f) {
        float w = 2 * PI_F * f / SR, c = cosf(w), s = sinf(w), al = s / (2 * q);
        float B0, B1, B2, A0 = 1 + al, A1 = -2 * c, A2 = 1 - al;
        if (type == 0) { B0 = (1 - c) / 2; B1 = 1 - c; B2 = (1 - c) / 2; }        // pasa bajos
        else if (type == 1) { B0 = (1 + c) / 2; B1 = -(1 + c); B2 = (1 + c) / 2; } // pasa altos
        else { B0 = al; B1 = 0; B2 = -al; }                                        // pasa banda
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    float Step(float x) { float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; }
};
enum { LP, HP, BP };

static void Noise(Buf &b, float dur, int type, float freq, float peak, float decay, float delay = 0) {
    size_t s0 = (size_t)(delay * SR), n = (size_t)(dur * SR);
    b.Ensure(s0 + n);
    Biquad f(type, freq, type == BP ? 1.0f : 0.7071f);
    for (size_t i = 0; i < n; i++) {
        float t = (float)i / SR;
        b.s[s0 + i] += f.Step(frand(-1, 1)) * Env(t, 0.002f, peak, decay);
    }
}
enum { SINE, SQUARE, SAW, TRI };
static float Osc(int type, float ph) {
    float x = ph - floorf(ph);
    switch (type) {
    case SQUARE: return x < 0.5f ? 0.6f : -0.6f;
    case SAW: return (2 * x - 1) * 0.7f;
    case TRI: return 1 - 4 * fabsf(x - 0.5f);
    default: return sinf(2 * PI_F * x);
    }
}
static void Tone(Buf &b, int type, float f1, float f2, float dur, float peak, float delay = 0) {
    size_t s0 = (size_t)(delay * SR), n = (size_t)((dur + 0.05f) * SR);
    b.Ensure(s0 + n);
    float ph = 0;
    for (size_t i = 0; i < n; i++) {
        float t = (float)i / SR;
        float f = f1 * powf(f2 / f1, std::min(t / dur, 1.0f));
        ph += f / SR;
        b.s[s0 + i] += Osc(type, ph) * Env(t, 0.005f, peak, dur);
    }
}

struct SoundSet { std::vector<Sound> pool; int next = 0; };
static std::map<std::string, std::vector<SoundSet>> sounds;   // varias variantes por nombre
bool musicOn = true;
static Sound music;
static bool audioOk = false;

static Sound ToSound(Buf &b, float gain = 0.5f) {
    Wave w{};
    w.frameCount = (unsigned)b.s.size();
    w.sampleRate = SR; w.sampleSize = 16; w.channels = 1;
    short *d = (short *)MemAlloc(w.frameCount * sizeof(short));
    for (size_t i = 0; i < b.s.size(); i++) d[i] = (short)(clampf(b.s[i] * gain, -1, 1) * 32000);
    w.data = d;
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
}
static void Add(const char *name, std::function<void(Buf &)> gen, int poolSize = 4, int variants = 1, float gain = 0.5f) {
    for (int v = 0; v < variants; v++) {
        Buf b; gen(b);
        SoundSet set;
        Sound base = ToSound(b, gain);
        set.pool.push_back(base);
        for (int i = 1; i < poolSize; i++) set.pool.push_back(LoadSoundAlias(base));
        sounds[name].push_back(set);
    }
}

// Capa de tensión sintetizada (40 s en bucle): zumbido grave disonante, latido y cuerdas agudas que crecen.
// Se mezcla con la música grabada y sube de volumen con las rondas.
static void BuildMusic() {
    const float len = 40;
    Buf b; b.Ensure((size_t)(len * SR));
    Biquad lp(LP, 900);
    for (size_t i = 0; i < b.s.size(); i++) {
        float t = (float)i / SR;
        float swell = 0.55f + 0.45f * sinf(2 * PI_F * t / 20 - PI_F / 2);   // crece y baja cada 20 s
        // zumbido: la, si bemol y mi graves (segunda menor = inquietud)
        float drone = sinf(2 * PI_F * 55 * t) * 0.11f + sinf(2 * PI_F * 58.27f * t) * 0.07f + sinf(2 * PI_F * 82.4f * t) * 0.05f;
        // cuerdas agudas: racimo disonante con trémolo
        float trem = 0.6f + 0.4f * sinf(2 * PI_F * 6.5f * t);
        float strings = (Osc(SAW, 440 * t + 0.002f * sinf(t * 5)) + Osc(SAW, 466.16f * t) * 0.8f + Osc(SAW, 622.25f * t) * 0.5f) * 0.03f * trem * swell * swell;
        b.s[i] += drone * (0.7f + 0.3f * swell) + lp.Step(strings);
    }
    // latido (dos golpes graves) cada 0,85 s, más fuerte en la parte alta
    for (float t = 0.2f; t < len - 1; t += 0.85f) {
        float swell = 0.55f + 0.45f * sinf(2 * PI_F * t / 20 - PI_F / 2);
        Tone(b, SINE, 62, 38, 0.16f, 0.45f * swell, t);
        Tone(b, SINE, 58, 36, 0.14f, 0.32f * swell, t + 0.22f);
    }
    // golpes metálicos lejanos de vez en cuando
    for (float t = 3.3f; t < len - 2; t += 7.7f) { Noise(b, 1.6f, BP, 700, 0.12f, 1.5f, t); Tone(b, TRI, 196, 185, 1.4f, 0.05f, t); }
    for (size_t i = 0; i < 4000; i++) { float k = i / 4000.0f; b.s[i] *= k; b.s[b.s.size() - 1 - i] *= k; }
    music = ToSound(b, 0.7f);
}

// Música grabada (CC0): "Insistent" de yd y "Abandoned passages" de congusbongus (OpenGameArt)
static Music trackMain{}, trackAmb{};
static bool trackMainOk = false, trackAmbOk = false;
static float tensionK = 0;
static void LoadMusicTracks() {
    if (FileExists("assets/music/insistent.ogg")) { trackMain = LoadMusicStream("assets/music/insistent.ogg"); trackMainOk = trackMain.frameCount > 0; }
    if (FileExists("assets/music/freezer.ogg")) { trackAmb = LoadMusicStream("assets/music/freezer.ogg"); trackAmbOk = trackAmb.frameCount > 0; }
    if (trackMainOk) { trackMain.looping = true; SeekMusicStream(trackMain, 9.0f); }   // empieza donde ya suena algo
    if (trackAmbOk) trackAmb.looping = true;
}

// Sonidos grabados (CC0, en assets/sfx). Si faltan, se usan los sintetizados.
static bool AddFile(const char *name, const char *path, int poolSize) {
    if (!FileExists(path)) return false;
    Sound base = LoadSound(path);
    if (!base.frameCount) return false;
    SoundSet set;
    set.pool.push_back(base);
    for (int i = 1; i < poolSize; i++) set.pool.push_back(LoadSoundAlias(base));
    sounds[name].push_back(set);
    return true;
}
static void LoadRealSounds() {
    const char *guns[] = { "pistol", "shotgun", "smg", "rifle", "ak47", "sniper", "luger", "suomi", "flare", "magnum", "sawed" };
    for (const char *g : guns) AddFile(TextFormat("g_%s", g), TextFormat("assets/sfx/%s.wav", g), !strcmp(g, "smg") || !strcmp(g, "ak47") || !strcmp(g, "rifle") || !strcmp(g, "suomi") ? 10 : 5);
    const char *rel[] = { "reload_pistol", "reload_rifle", "reload_smg" };
    for (const char *r : rel) AddFile(r, TextFormat("assets/sfx/%s.wav", r), 2);
    // voces de zombi: sustituyen a los gemidos sintetizados
    std::vector<SoundSet> synth = sounds["groan"];
    sounds["groan"].clear();
    for (int i = 0; AddFile("groan", TextFormat("assets/sfx/zombie%d.wav", i), 2); i++) {}
    if (sounds["groan"].empty()) sounds["groan"] = synth;
    // perros del infierno: gruñidos y ladridos
    for (int i = 0; i < 24 && AddFile("dog", TextFormat("assets/sfx/dog_snarl%d.wav", i), 2); i++) {}
    for (int i = 0; AddFile("bark", TextFormat("assets/sfx/dog_bark%d.wav", i), 2); i++) {}
    if (!HasSound("dog")) sounds["dog"] = sounds["groan"];
    if (!HasSound("bark")) sounds["bark"] = sounds["groan"];
}
bool HasSound(const char *name) { auto it = sounds.find(name); return it != sounds.end() && !it->second.empty(); }

void InitAudioSynth() {
    InitAudioDevice();
    audioOk = IsAudioDeviceReady();
    if (!audioOk) return;
    SetMasterVolume(0.8f);
    Add("pistol", [](Buf &b) { Noise(b, 0.3f, LP, 2500, 0.9f, 0.18f); Tone(b, SQUARE, 180, 60, 0.08f, 0.3f); Noise(b, 0.4f, LP, 600, 0.25f, 0.35f, 0.02f); }, 6);
    Add("smg", [](Buf &b) { Noise(b, 0.2f, LP, 3000, 0.6f, 0.1f); Tone(b, SQUARE, 200, 80, 0.05f, 0.2f); Noise(b, 0.3f, LP, 700, 0.2f, 0.25f, 0.02f); }, 8);
    Add("rifle", [](Buf &b) { Noise(b, 0.35f, LP, 2000, 0.9f, 0.22f); Tone(b, SAW, 150, 50, 0.1f, 0.35f); Noise(b, 0.6f, LP, 500, 0.3f, 0.5f, 0.03f); }, 8);
    Add("shotgun", [](Buf &b) { Noise(b, 0.6f, LP, 1400, 1.0f, 0.45f); Tone(b, SQUARE, 110, 40, 0.2f, 0.5f); Noise(b, 0.9f, LP, 400, 0.35f, 0.8f, 0.03f); Noise(b, 0.08f, BP, 1800, 0.3f, 0.06f, 0.45f); Noise(b, 0.08f, BP, 1400, 0.3f, 0.06f, 0.6f); }, 3);
    Add("sniper", [](Buf &b) { Noise(b, 0.9f, LP, 1800, 1.0f, 0.7f); Tone(b, SAW, 120, 30, 0.3f, 0.5f); Noise(b, 1.4f, LP, 350, 0.35f, 1.3f, 0.05f); }, 3);
    Add("ray", [](Buf &b) { Tone(b, SQUARE, 1400, 180, 0.28f, 0.22f); Tone(b, SINE, 700, 90, 0.3f, 0.3f); }, 4);
    Add("zap", [](Buf &b) { Noise(b, 0.5f, HP, 2500, 0.8f, 0.4f); Tone(b, SAW, 2000, 100, 0.4f, 0.3f); Tone(b, SQUARE, 60, 40, 0.4f, 0.4f); }, 3);
    Add("launcher", [](Buf &b) { Tone(b, SINE, 220, 60, 0.18f, 0.7f); Noise(b, 0.2f, LP, 800, 0.6f, 0.12f); }, 4);
    Add("rocket", [](Buf &b) { Noise(b, 1.0f, BP, 900, 0.8f, 0.9f); Noise(b, 0.3f, LP, 400, 1.0f, 0.25f); Tone(b, SAW, 300, 90, 0.7f, 0.2f); }, 3);
    Add("empty", [](Buf &b) { Noise(b, 0.05f, HP, 3000, 0.3f, 0.03f); }, 2);
    Add("reload", [](Buf &b) { Noise(b, 0.06f, BP, 2000, 0.4f, 0.05f); Noise(b, 0.08f, BP, 1500, 0.5f, 0.06f, 0.35f); Noise(b, 0.05f, BP, 2600, 0.35f, 0.04f, 0.7f); }, 2);
    Add("buy", [](Buf &b) { Tone(b, SINE, 988, 988, 0.12f, 0.3f); Tone(b, SINE, 1319, 1319, 0.3f, 0.3f, 0.1f); }, 2);
    Add("deny", [](Buf &b) { Tone(b, SQUARE, 160, 120, 0.25f, 0.2f); }, 2);
    Add("hurt", [](Buf &b) { Tone(b, SINE, 160, 50, 0.25f, 0.6f); Noise(b, 0.2f, LP, 600, 0.5f, 0.15f); }, 3);
    Add("hit", [](Buf &b) { Noise(b, 0.05f, HP, 4000, 0.25f, 0.03f); Noise(b, 0.08f, LP, 500, 0.3f, 0.06f); }, 6);
    Add("heart", [](Buf &b) { Tone(b, SINE, 70, 40, 0.12f, 0.6f); Tone(b, SINE, 65, 38, 0.12f, 0.45f, 0.18f); }, 2);
    Add("pin", [](Buf &b) { Noise(b, 0.05f, HP, 5000, 0.3f, 0.04f); Tone(b, SINE, 2400, 2000, 0.08f, 0.15f, 0.05f); }, 2);
    Add("boom", [](Buf &b) { Noise(b, 1.6f, LP, 500, 1.0f, 1.4f); Tone(b, SINE, 80, 25, 0.9f, 0.9f); Noise(b, 0.3f, LP, 3000, 0.6f, 0.2f); Noise(b, 2.0f, LP, 200, 0.4f, 1.9f, 0.1f); }, 4);
    Add("bigboom", [](Buf &b) {   // explosión grande: golpe muy grave, rugido largo y crujidos de escombros
        Tone(b, SINE, 60, 18, 1.8f, 1.0f); Noise(b, 3.5f, LP, 160, 0.9f, 3.2f); Noise(b, 0.5f, LP, 1500, 0.8f, 0.35f);
        Noise(b, 2.5f, BP, 450, 0.35f, 2.2f, 0.15f);
        for (int i = 0; i < 14; i++) Noise(b, 0.08f, BP, frand(1200, 3500), frand(0.08f, 0.2f), 0.06f, 0.4f + frand(0, 2.2f));
    }, 2);
    Add("debris", [](Buf &b) { for (int i = 0; i < 18; i++) Noise(b, 0.07f, BP, frand(900, 4000), frand(0.06f, 0.22f), 0.05f, 0.3f + powf(frand(0, 1), 1.6f) * 2.4f); }, 3);
    Add("crossbow", [](Buf &b) {   // cuerda que se suelta: chasquido y zumbido
        Noise(b, 0.05f, HP, 3000, 0.7f, 0.03f); Tone(b, TRI, 190, 120, 0.35f, 0.45f); Tone(b, SINE, 95, 70, 0.3f, 0.3f); Noise(b, 0.3f, BP, 700, 0.2f, 0.25f, 0.02f);
    }, 3);
    Add("flame", [](Buf &b) {      // lanzallamas: soplido de gas que ruge
        Noise(b, 0.45f, LP, 700, 0.6f, 0.4f); Noise(b, 0.45f, BP, 1800, 0.25f, 0.35f); Noise(b, 0.4f, LP, 160, 0.5f, 0.38f);
    }, 4);
    Add("spin", [](Buf &b) { Tone(b, SAW, 70, 420, 0.5f, 0.18f); Tone(b, SQUARE, 35, 210, 0.5f, 0.08f); Noise(b, 0.5f, BP, 1200, 0.15f, 0.45f); }, 2);
    Add("beep", [](Buf &b) { Tone(b, SQUARE, 1800, 1800, 0.06f, 0.18f); }, 4);
    Add("ping", [](Buf &b) { Tone(b, SINE, 3100, 3000, 0.5f, 0.25f); Tone(b, SINE, 4650, 4600, 0.35f, 0.1f); Noise(b, 0.03f, HP, 4000, 0.2f, 0.02f); }, 2);
    Add("board", [](Buf &b) { Noise(b, 0.25f, BP, 900, 0.6f, 0.2f); Noise(b, 0.15f, HP, 2500, 0.3f, 0.1f, 0.05f); }, 4);
    Add("repair", [](Buf &b) { Noise(b, 0.06f, BP, 1200, 0.5f, 0.05f); Noise(b, 0.06f, BP, 1100, 0.5f, 0.05f, 0.18f); }, 3);
    Add("laugh", [](Buf &b) { for (int i = 0; i < 6; i++) Tone(b, TRI, 600.0f - i * 50, 300.0f - i * 30, 0.25f, 0.25f, i * 0.3f); }, 1);
    Add("pap", [](Buf &b) { Tone(b, SAW, 80, 160, 3.2f, 0.2f); Noise(b, 3.2f, BP, 600, 0.25f, 3.0f); for (int i = 0; i < 6; i++) Tone(b, SQUARE, 300.0f + i * 120, 300.0f + i * 120, 0.1f, 0.12f, i * 0.5f); }, 1);
    Add("round", [](Buf &b) { float f[3] = { 220, 207, 196 }; for (int i = 0; i < 3; i++) Tone(b, SAW, f[i], f[i] * 0.98f, 0.9f, 0.15f, i * 0.45f); Tone(b, SINE, 55, 50, 2.0f, 0.5f); }, 1);
    Add("roundEnd", [](Buf &b) { Tone(b, TRI, 392, 392, 0.4f, 0.25f); Tone(b, TRI, 330, 330, 0.4f, 0.25f, 0.35f); Tone(b, TRI, 262, 262, 0.9f, 0.25f, 0.7f); }, 1);
    Add("powerup", [](Buf &b) { Tone(b, SINE, 600, 1200, 0.3f, 0.3f); Tone(b, SINE, 900, 1800, 0.3f, 0.25f, 0.15f); }, 2);
    Add("door", [](Buf &b) { Noise(b, 1.2f, LP, 300, 0.7f, 1.0f); Tone(b, SAW, 60, 40, 1.0f, 0.25f); }, 1);
    Add("perk", [](Buf &b) { Tone(b, SINE, 523, 523, 0.2f, 0.3f); Tone(b, SINE, 659, 659, 0.2f, 0.3f, 0.15f); Tone(b, SINE, 784, 784, 0.4f, 0.3f, 0.3f); }, 1);
    Add("box", [](Buf &b) { for (int i = 0; i < 12; i++) Tone(b, TRI, frand(500, 1000), frand(500, 1000), 0.12f, 0.12f, i * 0.3f); }, 1);
    Add("knife", [](Buf &b) { Noise(b, 0.22f, BP, 2600, 0.45f, 0.18f); Noise(b, 0.18f, HP, 4500, 0.2f, 0.14f, 0.03f); }, 2);
    Add("stab", [](Buf &b) { Noise(b, 0.15f, LP, 500, 0.9f, 0.12f); Tone(b, SINE, 120, 50, 0.12f, 0.6f); Noise(b, 0.1f, BP, 1200, 0.35f, 0.08f, 0.02f); }, 3);
    Add("slide", [](Buf &b) { Noise(b, 0.7f, BP, 900, 0.55f, 0.6f); Noise(b, 0.6f, LP, 350, 0.35f, 0.5f, 0.05f); }, 2);
    Add("flareshot", [](Buf &b) { Noise(b, 0.25f, LP, 1800, 0.6f, 0.15f); Noise(b, 0.9f, BP, 1500, 0.35f, 0.8f, 0.04f); Tone(b, SINE, 300, 120, 0.3f, 0.2f); }, 3);
    Add("flareburst", [](Buf &b) { Noise(b, 0.6f, LP, 1200, 0.7f, 0.5f); Noise(b, 1.5f, BP, 2500, 0.25f, 1.4f, 0.05f); Tone(b, SINE, 90, 40, 0.4f, 0.5f); }, 3);
    Add("thunder", [](Buf &b) {   // Cañón de Trueno: golpe enorme + ráfaga de viento
        Tone(b, SINE, 70, 28, 0.9f, 1.0f); Noise(b, 0.5f, LP, 900, 1.0f, 0.4f); Noise(b, 1.6f, BP, 700, 0.6f, 1.5f, 0.03f);
        Noise(b, 1.2f, HP, 3000, 0.25f, 1.0f, 0.05f); Tone(b, SAW, 220, 40, 0.6f, 0.25f, 0.02f);
    }, 3);
    Add("power", [](Buf &b) {     // interruptor de corriente: golpe metálico y zumbido que sube
        Noise(b, 0.25f, LP, 700, 0.9f, 0.2f); Tone(b, SQUARE, 80, 70, 0.15f, 0.4f);
        Tone(b, SAW, 40, 120, 2.8f, 0.25f, 0.3f); Tone(b, SINE, 60, 180, 2.8f, 0.3f, 0.3f); Noise(b, 2.5f, BP, 400, 0.15f, 2.3f, 0.4f);
    }, 1);
    Add("dogstart", [](Buf &b) {  // ronda de perros: trueno y aullido grave
        Noise(b, 3.0f, LP, 250, 1.0f, 2.8f); Noise(b, 0.4f, LP, 3000, 0.7f, 0.3f); Tone(b, SINE, 45, 25, 2.5f, 0.8f);
        Tone(b, SAW, 180, 260, 1.6f, 0.12f, 0.8f); Tone(b, SAW, 260, 140, 1.4f, 0.12f, 2.3f);
    }, 1);
    Add("strike", [](Buf &b) {    // rayo que trae a un perro
        Noise(b, 0.12f, HP, 2500, 1.0f, 0.1f); Noise(b, 1.2f, LP, 400, 0.8f, 1.0f, 0.03f); Tone(b, SINE, 60, 30, 0.6f, 0.6f);
    }, 4);
    Add("dogdie", [](Buf &b) { Noise(b, 0.5f, LP, 1500, 0.8f, 0.4f); Noise(b, 1.0f, BP, 2000, 0.3f, 0.9f, 0.05f); Tone(b, SINE, 120, 40, 0.3f, 0.5f); }, 4);
    Add("step", [](Buf &b) { Noise(b, 0.08f, LP, 700, 0.25f, 0.06f); Noise(b, 0.03f, BP, 2000, 0.06f, 0.02f); }, 3, 3);
    // gemidos de zombi (varias voces)
    Add("groan", [](Buf &b) {
        float base = frand(70, 110), end = base * frand(0.55f, 0.85f), dur = frand(0.9f, 1.5f);
        size_t n = (size_t)(dur * SR);
        b.Ensure(n);
        Biquad f(LP, frand(400, 650));
        float ph = 0;
        for (size_t i = 0; i < n; i++) {
            float t = (float)i / SR, k = t / dur;
            float fr = lerpf(base, end, k) * (1 + 0.04f * sinf(t * 31));
            ph += fr / SR;
            float x = Osc(SAW, ph) + frand(-0.25f, 0.25f);
            b.s[i] += f.Step(x) * Env(t, 0.15f, 0.5f, dur - 0.15f);
        }
    }, 2, 6, 0.6f);
    LoadRealSounds();
    BuildMusic();
    LoadMusicTracks();
}

void Play(const char *name, float vol, float pan) {
    if (!audioOk) return;
    auto it = sounds.find(name);
    if (it == sounds.end()) return;
    SoundSet &set = it->second[irand((int)it->second.size())];
    Sound &s = set.pool[set.next];
    set.next = (set.next + 1) % set.pool.size();
    SetSoundVolume(s, clampf(vol * opts.sfx, 0, 1));
    SetSoundPan(s, clampf(pan, 0, 1));
    PlaySound(s);
}

// Sonido con volumen y paneo según la posición respecto al jugador
void PlayAt(const char *name, Vector3 p, float maxDist) {
    float dx = p.x - player.pos.x, dz = p.z - player.pos.z, d = hyp(dx, dz);
    if (d > maxDist) return;
    float rx = cosf(player.yaw), rz = -sinf(player.yaw);
    float side = d > 0.01f ? (dx * rx + dz * rz) / d : 0;
    Play(name, (1 - d / maxDist) * 0.9f, 0.5f - side * 0.35f);
}

void UpdateMusic() {
    if (!audioOk) return;
    SetMasterVolume(opts.master);
    float dt = std::min(GetFrameTime(), 0.05f);
    bool inGame = game.state == GS_PLAYING || game.state == GS_PAUSED;
    float mv = musicOn ? opts.music : 0;
    // la tensión sube con las rondas y va al máximo en las rondas de perros
    float target = !inGame ? 0 : Practice() ? 0.1f : game.dogRound ? 1.0f : clampf((game.round - 1) / 9.0f, 0.15f, 0.85f);
    tensionK = lerpf(tensionK, target, dampk(0.5f, dt));
    if (trackMainOk) {
        if (!IsMusicStreamPlaying(trackMain)) PlayMusicStream(trackMain);
        UpdateMusicStream(trackMain);
        SetMusicVolume(trackMain, mv * (inGame ? (StoryTalking() ? 0.3f : 0.55f) : (game.state == GS_INTRO || game.state == GS_STORY || game.state == GS_CUTSCENE) ? 0.3f : 0.8f));   // en la cinemática, más baja para oír la radio
    }
    if (trackAmbOk) {
        if (!IsMusicStreamPlaying(trackAmb)) PlayMusicStream(trackAmb);
        UpdateMusicStream(trackAmb);
        SetMusicVolume(trackAmb, mv * (inGame || game.state == GS_INTRO || game.state == GS_CUTSCENE ? 0.45f : 0.25f));
    }
    if (!IsSoundPlaying(music)) PlaySound(music);
    SetSoundVolume(music, mv * tensionK);
    SetSoundPitch(music, game.dogRound ? 1.12f : 1.0f + std::min(game.round, 20) * 0.004f);
}
void ToggleMusic() { musicOn = !musicOn; }
