// Frases del personaje (assets/sfx/jug_<tipo><n>.wav): insultos al recibir daño, comentarios al matar,
// al recargar, al empezar ronda... Habla de vez en cuando, sin pisarse a sí mismo.
#include "game.h"

static const char *KINDS[V_COUNT] = { "herido", "baja", "cabeza", "recarga", "ronda", "perros", "vida", "municion" };
static std::vector<Sound> lines[V_COUNT];
static int lastLine[V_COUNT];
static Sound *speaking = nullptr;
static float nextSay = 0;

void LoadVoiceLines() {
    if (!IsAudioDeviceReady()) return;
    for (int k = 0; k < V_COUNT; k++) {
        lastLine[k] = -1;
        for (int i = 0; i < 10; i++) {
            const char *path = TextFormat("assets/sfx/jug_%s%d.wav", KINDS[k], i);
            if (!FileExists(path)) break;
            lines[k].push_back(LoadSound(path));
        }
    }
}

void ResetVoiceLines() { nextSay = 3; speaking = nullptr; }

void Say(int kind, float chance) {
    if (game.state != GS_PLAYING || Practice() || lines[kind].empty()) return;
    if (speaking && IsSoundPlaying(*speaking)) return;
    bool important = kind == V_ROUND || kind == V_DOGS || kind == V_LOWHP;
    if (T < nextSay && !important) return;
    if (frand(0, 1) > chance) return;
    int n = (int)lines[kind].size(), i = irand(n);
    if (n > 1 && i == lastLine[kind]) i = (i + 1) % n;   // no repetir la misma frase seguida
    lastLine[kind] = i;
    speaking = &lines[kind][i];
    SetSoundVolume(*speaking, clampf(opts.sfx, 0, 1));
    PlaySound(*speaking);
    nextSay = T + 6 + frand(0, 5);
}
