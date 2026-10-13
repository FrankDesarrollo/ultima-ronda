// Opciones (opciones.txt) y récords por mapa (records.txt), guardados junto al ejecutable.
#include "game.h"
#include <map>

Options opts;
extern bool lowQuality;

void LoadOptions() {
    FILE *f = fopen("opciones.txt", "r");
    if (!f) return;
    char key[64]; float v;
    while (fscanf(f, "%63s %f", key, &v) == 2) {
        if (!strcmp(key, "sensibilidad")) opts.sens = clampf(v, 0.2f, 3.0f);
        else if (!strcmp(key, "volumen")) opts.master = clampf(v, 0, 1);
        else if (!strcmp(key, "musica")) opts.music = clampf(v, 0, 1);
        else if (!strcmp(key, "efectos")) opts.sfx = clampf(v, 0, 1);
        else if (!strcmp(key, "campo_vision")) opts.fov = clampf(v, 60, 100);
        else if (!strcmp(key, "graficos_rapidos")) opts.lowGfx = v != 0;
        else if (!strcmp(key, "mostrar_fps")) opts.showFps = v != 0;
    }
    fclose(f);
    lowQuality = opts.lowGfx;
}

void SaveOptions() {
    opts.lowGfx = lowQuality;
    FILE *f = fopen("opciones.txt", "w");
    if (!f) return;
    fprintf(f, "sensibilidad %.2f\nvolumen %.2f\nmusica %.2f\nefectos %.2f\ncampo_vision %.0f\ngraficos_rapidos %d\nmostrar_fps %d\n",
            opts.sens, opts.master, opts.music, opts.sfx, opts.fov, opts.lowGfx ? 1 : 0, opts.showFps ? 1 : 0);
    fclose(f);
}

static std::map<std::string, Record> records;
static bool recordsLoaded = false;
static void LoadRecords() {
    recordsLoaded = true;
    FILE *f = fopen("records.txt", "r");
    if (!f) return;
    char map[64]; Record r;
    while (fscanf(f, "%63s %d %d %d", map, &r.round, &r.kills, &r.heads) == 4) records[map] = r;
    fclose(f);
}
Record GetRecord(const std::string &map) {
    if (!recordsLoaded) LoadRecords();
    auto it = records.find(map);
    return it == records.end() ? Record() : it->second;
}
bool SubmitRecord(const std::string &map, int round, int kills, int heads) {
    Record old = GetRecord(map);
    if (round < old.round || (round == old.round && kills <= old.kills)) return false;
    records[map] = Record{ round, kills, heads };
    FILE *f = fopen("records.txt", "w");
    if (f) {
        for (auto &p : records) fprintf(f, "%s %d %d %d\n", p.first.c_str(), p.second.round, p.second.kills, p.second.heads);
        fclose(f);
    }
    return true;
}
