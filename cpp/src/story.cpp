// Modo historia "Operación Lázaro": seis capítulos con Milton a tu lado, objetivos, diálogos con voz,
// cinemáticas (cutscene.cpp), un jefe final y pantallas narradas entre capítulos.
// Las frases (texto y personaje) se leen de assets/sfx/historia/guion.tsv y su voz de <id>.wav en la misma carpeta;
// si falta un audio, igual sale el subtítulo.
#include "game.h"
#include <map>
#include <deque>

// ---------- Personajes ----------
enum Speaker { SP_FRANK, SP_REYES, SP_VIDAL, SP_VIDAL_TAPE, SP_MILTON, SP_NARR, SP_REYES_PA, SP_GRAB, SP_COUNT };
static const char *SPEAKER_NAME[SP_COUNT] = { "FRANK", "CORONEL REYES", "DRA. VIDAL", "DIARIO DE VIDAL", "MILTON", "", "REYES · ALTAVOCES", "GRABACIÓN" };
static const Color SPEAKER_COLOR[SP_COUNT] = { { 150, 210, 120, 255 }, { 255, 180, 80, 255 }, { 150, 200, 255, 255 }, { 150, 200, 255, 255 },
                                               { 120, 230, 230, 255 }, { 220, 220, 220, 255 }, { 255, 140, 70, 255 }, { 210, 210, 160, 255 } };
static bool IsRadio(int who) { return who == SP_REYES || who == SP_VIDAL || who == SP_REYES_PA; }

// ---------- Guion: frases leídas de guion.tsv ----------
struct Line { std::string id; int who; std::string text; };
static std::map<std::string, Line> lines;
static bool linesLoaded = false;
static void LoadLines() {
    if (linesLoaded) return;
    linesLoaded = true;
    char *data = LoadFileText("assets/sfx/historia/guion.tsv");
    if (!data) return;
    const std::pair<const char *, int> who[] = { { "frank", SP_FRANK }, { "reyes", SP_REYES }, { "vidal", SP_VIDAL }, { "vidal_tape", SP_VIDAL_TAPE },
                                                 { "milton", SP_MILTON }, { "narr", SP_NARR }, { "reyes_pa", SP_REYES_PA },
                                                 { "obrero", SP_GRAB }, { "enfermera", SP_GRAB }, { "cura", SP_GRAB }, { "tomas", SP_GRAB }, { "reyes_tape", SP_GRAB } };
    std::string all = data;
    UnloadFileText(data);
    size_t pos = 0;
    while (pos < all.size()) {
        size_t end = all.find('\n', pos);
        if (end == std::string::npos) end = all.size();
        std::string row = all.substr(pos, end - pos);
        pos = end + 1;
        if (!row.empty() && row.back() == '\r') row.pop_back();
        size_t a = row.find('\t'), b = a == std::string::npos ? a : row.find('\t', a + 1), c = b == std::string::npos ? b : row.find('\t', b + 1);
        if (c == std::string::npos) continue;
        Line l;
        l.id = row.substr(0, a);
        std::string w = row.substr(a + 1, b - a - 1);
        l.who = SP_FRANK;
        for (auto &p : who) if (w == p.first) l.who = p.second;
        // el texto sin las pausas de SSML
        std::string t = row.substr(c + 1), clean;
        for (size_t i = 0; i < t.size(); i++) {
            if (t[i] == '<') { size_t e = t.find('>', i); if (e == std::string::npos) break; i = e; if (!clean.empty() && clean.back() == ' ' && i + 1 < t.size() && t[i + 1] == ' ') i++; continue; }
            clean += t[i];
        }
        l.text = clean;
        lines[l.id] = l;
    }
}
static const Line *FindLine(const char *id) {
    LoadLines();
    auto it = lines.find(id);
    return it == lines.end() ? nullptr : &it->second;
}

// ---------- Capítulos ----------
enum StepKind { ST_ROUNDS, ST_POWER, ST_COLLECT, ST_HOLDE, ST_REACH, ST_HOLD, ST_DEFEND, ST_BOSS };
enum ItemKind { IT_DIARY, IT_CHARGE, IT_SAMPLE, IT_FILE, IT_KEYCARD, IT_TERMINAL, IT_COMPONENT, IT_COUNT };
static const char *ITEM_LABEL[IT_COUNT] = { "DIARIO", "COLOCAR CARGA", "MUESTRA", "FICHA MÉDICA", "TARJETA", "TERMINAL", "COMPONENTE" };
static const char *ITEM_MSG[IT_COUNT] = { "DIARIO", "CARGA COLOCADA", "MUESTRA", "FICHA MÉDICA", "TARJETA DE ACCESO", "CÓDIGO COPIADO", "COMPONENTE" };
static const unsigned ITEM_COLOR[IT_COUNT] = { 0xffd27a, 0xff5a2a, 0x6cff9a, 0xfff0d0, 0x7ac8ff, 0x5ab8ff, 0xc890ff };
static bool HoldItem(int k) { return k == IT_CHARGE || k == IT_TERMINAL; }

struct ItemDef { const char *room; float x, z; };
struct TimedLine { float t; const char *id; };
struct StepDef {
    int kind; const char *goal; float n;              // n: rondas, segundos, o vida del jefe (x vida de un zombi)
    const char *room;                                 // ST_REACH: sala; ST_DEFEND / ST_BOSS: dónde (x, z en zone)
    std::vector<const char *> onStart, onDone;
    std::vector<TimedLine> timed;
    int itemKind; std::vector<ItemDef> items;         // ST_COLLECT / ST_HOLDE
    std::vector<std::vector<const char *>> onItem;    // lo que se oye al tomar el 1.º, 2.º y 3.º objeto
    const char *cutscene;                             // cinemática al cumplir el paso
    Vector2 zone; float radius;                       // ST_DEFEND: zona a defender; ST_BOSS: dónde aparece
};
struct RoomLine { const char *room; std::vector<const char *> ids; };
struct ChapterDef {
    const char *map, *title, *place;
    std::vector<const char *> cards;
    const char *intro[4];
    std::vector<StepDef> steps;
    std::vector<RoomLine> roomLines;                  // al entrar por primera vez a una sala
    std::vector<std::vector<const char *>> chatter;   // charlas cuando hay calma
    const char *outro;                                // cinemática final (nullptr: fundido a negro)
    const char *ending;                               // texto al completar (pantalla narrada)
    float allyHealth;                                 // Milton herido: menos vida
};
static StepDef S(int kind, const char *goal, float n, std::vector<const char *> onStart, std::vector<const char *> onDone = {}) {
    StepDef s{}; s.kind = kind; s.goal = goal; s.n = n; s.onStart = onStart; s.onDone = onDone; s.cutscene = nullptr; s.room = nullptr; s.itemKind = -1;
    return s;
}
static StepDef Items(StepDef s, int kind, std::vector<ItemDef> it, std::vector<std::vector<const char *>> onItem) { s.itemKind = kind; s.items = it; s.onItem = onItem; return s; }
static StepDef Timed(StepDef s, std::vector<TimedLine> t) { s.timed = t; return s; }
static StepDef Cut(StepDef s, const char *id) { s.cutscene = id; return s; }
static StepDef InRoom(StepDef s, const char *room) { s.room = room; return s; }
static StepDef Zone(StepDef s, float x, float z, float r) { s.zone = Vector2{ x, z }; s.radius = r; return s; }

static const std::vector<ChapterDef> &Chapters() {
    static std::vector<ChapterDef> list = {
        { "bunker", "LO QUE QUEDÓ ENTERRADO", "BÚNKER 7 · 23:47",
          { "h1_c1", "h1_c2", "h1_c3" }, { "h1_i1", "h1_i2", "h1_i3", "h1_i4" },
          { S(ST_ROUNDS, "Sobrevive a la primera oleada", 1, { "h1_s1" }, { "h1_s1m", "h1_s1f" }),
            S(ST_POWER, "Activa la corriente del búnker", 0, { "h1_s2a" }, { "h1_s2b" }),
            Items(S(ST_COLLECT, "Encuentra los diarios de la Dra. Vidal", 3, { "h1_s3" }), IT_DIARY,
                  { { "B", 0, -17 }, { "D", -1, 22 }, { "E", 19, -20 } }, { { "h1_d1" }, { "h1_d2" }, { "h1_d3", "h1_d3m", "h1_d3f" } }),
            S(ST_HOLD, "Resiste hasta la extracción", 90, { "h1_s4a", "h1_s4b" }, { "h1_s4c" }) },
          { { "C", { "h1_rC" } }, { "E", { "h1_rE" } } },
          { { "h1_x1", "h1_x1f" }, { "h1_x2", "h1_x2f" } },
          nullptr, "El helicóptero despegó del Búnker 7 con los diarios de Vidal. Milton no paró de hablar en todo el viaje.", 1 },
        { "fabrica", "LÍNEA DE PRODUCCIÓN", "FÁBRICA ABANDONADA · 03:10",
          { "h2_c1", "h2_c2", "h2_c3" }, { "h2_i1", "h2_i2", "h2_i3", "h2_i4" },
          { S(ST_ROUNDS, "Defiendan su posición", 1, { "h2_s1" }),
            S(ST_POWER, "Activa la corriente de la planta", 0, { "h2_s2" }),
            Items(S(ST_HOLDE, "Coloca las cargas en los tanques de suero", 3, { "h2_s3" }), IT_CHARGE,
                  { { "C", -12, 4 }, { "D", 17, -4 }, { "E", 0, 25 } }, { {}, { "h2_v1", "h2_v2", "h2_vm", "h2_vf" }, {} }),
            Timed(S(ST_HOLD, "Sobrevive hasta la detonación", 60, { "h2_s4a" }), { { 14, "h2_s4b" }, { 14, "h2_s4c" }, { 32, "h2_s4d" }, { 32, "h2_s4e" } }) },
          { { "B", { "h2_rB" } }, { "E", { "h2_rE" } } },
          { { "h2_x1", "h2_x1f" }, { "h2_x2", "h2_x2f" } },
          "fabrica_boom", "La fábrica ardió toda la noche. Esa misma noche dejamos de responder la radio de Reyes.", 1 },
        { "hospital", "PABELLÓN 6", "HOSPITAL SAN RAFAEL · 01:20",
          { "h3_c1", "h3_c2", "h3_c3" }, { "h3_i1", "h3_i2", "h3_i3", "h3_i4" },
          { S(ST_POWER, "Activa la corriente en el archivo", 0, { "h3_s1" }, { "h3_s1m" }),
            Items(S(ST_COLLECT, "Encuentra las fichas de los pacientes", 3, { "h3_s2" }), IT_FILE,
                  { { "C", -14, 24 }, { "F", 20, -2 }, { "D", 9, 28 } }, { { "h3_f1" }, { "h3_f2" }, { "h3_f3", "h3_f3b" } }),
            Items(S(ST_COLLECT, "Busca la tarjeta del director en la morgue", 1, { "h3_s3" }), IT_KEYCARD, { { "E", -16, -3 } }, { { "h3_k1" } }),
            Timed(Zone(S(ST_DEFEND, "Defiende el quirófano mientras se suben los archivos", 75, { "h3_s4", "h3_s4b" }), 17, 24, 7.5f),
                  { { 34, "h3_s4c" }, { 34, "h3_s4d" } }) },
          { { "C", { "h3_rC" } }, { "E", { "h3_rE" } } },
          { { "h3_x1", "h3_x1f" }, { "h3_x2", "h3_x2f" } },
          "hospital_alarma", nullptr, 1 },
        { "pueblo", "SAN ROQUE", "PUEBLO DE SAN ROQUE · 02:13",
          { "h4_c1", "h4_c2", "h4_c3" }, { "h4_i1", "h4_i2", "h4_i3", "h4_i4" },
          { Cut(InRoom(S(ST_REACH, "Llega a la iglesia", 0, { "h4_s1" }), "C"), "iglesia_vidal"),
            Items(S(ST_COLLECT, "Recupera las muestras de la cura", 3, {}), IT_SAMPLE,
                  { { "D", 50, 0 }, { "E", -25, 0 }, { "F", 10, 30 } }, { { "h4_m1" }, { "h4_m2" }, { "h4_m3" } }),
            S(ST_POWER, "Activa la corriente en el taller", 0, { "h4_s3" }),
            Timed(S(ST_HOLD, "Resiste hasta el amanecer", 120, { "h4_s4a", "h4_s4b", "h4_s4c" }), { { 60, "h4_s4d" }, { 60, "h4_s4e" } }) },
          { { "D", { "h4_rD" } }, { "G", { "h4_rG" } } },
          { { "h4_x1", "h4_x1f" }, { "h4_x2", "h4_x2f" } },
          "mordida", nullptr, 1 },
        { "base", "ÓRDENES", "FUERTE REYES · 04:05",
          { "h5_c1", "h5_c2", "h5_c3" }, { "h5_i1", "h5_i2", "h5_i3", "h5_i4" },
          { S(ST_ROUNDS, "Sobrevive a la guardia de Reyes", 1, { "h5_s1" }),
            S(ST_POWER, "Activa la corriente en la sala de radio", 0, { "h5_s2" }),
            Cut(Items(S(ST_HOLDE, "Copia los códigos de las terminales", 3, { "h5_s3" }), IT_TERMINAL,
                      { { "B", -28, -6 }, { "D", 30, 8 }, { "C", -28, 12 } }, { { "h5_t1" }, { "h5_t2" }, { "h5_t3" } }), "altavoz"),
            InRoom(S(ST_REACH, "Entra al despacho de Reyes", 0, { "h5_s4" }, { "h5_s4d" }), "F"),
            Timed(S(ST_HOLD, "Resiste hasta que se abra el ascensor", 90, { "h5_s5a" }, { "h5_s5d" }), { { 40, "h5_s5b" }, { 40, "h5_s5c" } }) },
          { { "C", { "h5_rC" } }, { "D", { "h5_rD" } } },
          { { "h5_x1", "h5_x1f" }, { "h5_x2" } },
          nullptr, "El ascensor bajó tres niveles. Nadie dijo una palabra.", 0.7f },
        { "laboratorio", "PACIENTE CERO", "LABORATORIO LÁZARO · NIVEL -3",
          { "h6_c1", "h6_c2", "h6_c3" }, { "h6_i1", "h6_i2", "h6_i3", "h6_i4" },
          { S(ST_POWER, "Activa la corriente en la sala de control", 0, { "h6_s1" }),
            Items(S(ST_COLLECT, "Recupera los componentes de la cura", 3, { "h6_s2" }), IT_COMPONENT,
                  { { "C", -19, 17 }, { "F", -19, -3 }, { "D", 18, 21 } }, { { "h6_p1" }, { "h6_p2" }, { "h6_p3" } }),
            Cut(InRoom(S(ST_REACH, "Lleva los componentes al núcleo", 0, { "h6_s3" }), "E"), "lazaro"),
            Timed(Zone(S(ST_BOSS, "Derrota a Lázaro", 18, { "h6_b1" }, { "h6_b3" }), 0, 41, 0), { { 30, "h6_b2" } }),
            Timed(Zone(S(ST_DEFEND, "Defiende el sintetizador", 60, { "h6_s5a" }), 0, 42, 9.5f), { { 22, "h6_s5b" }, { 22, "h6_s5c" } }) },
          { { "C", { "h6_rC" } }, { "E", { "h6_rE" } } },
          { { "h6_x1", "h6_x1f" } },
          "final", nullptr, 0.7f },
    };
    return list;
}
static int ChapterCount() { return (int)Chapters().size(); }
static const char *EPILOGUE[] = { "h6_e1", "h6_e2", "h6_e3", "h6_e4", "h6_e5" };

// ---------- Estado ----------
static bool active = false;
static int chapter = 0, step = 0, itemsDone = 0;
static float stepT = 0, doneT = 0, endT = -1, storyT = 0, goalFlash = 0, defendK = 0;
static bool stepDone = false, started = false, inCutscene = false, waitCut = false;
static float frameDt = 0;   // dt del último fotograma (para mantener E)
static size_t timedNext = 0;
static int progress = -1;   // capítulos terminados (se guarda en historia.txt)
static int bossId = -1; static float bossMax = 1;
static std::vector<int> roomsSeen;
static int chatterNext = 0; static float lastTalkEnd = 0;

struct Item { int kind, step; Vector3 pos; bool taken = false; float hold = 0; int icon = -1; int light = -1; };
static std::vector<Item> items;

bool StoryActive() { return active; }
int StoryChapter() { return chapter; }
static const ChapterDef &Ch() { return Chapters()[chapter]; }
static const StepDef &St() { return Ch().steps[step]; }
bool StoryWantsAlly() { return active; }
float StoryAllyHealth() { return active ? Ch().allyHealth : 1; }

int StoryProgress() {
    if (progress < 0) {
        progress = 0;
        if (FILE *f = fopen("historia.txt", "r")) { if (fscanf(f, "%d", &progress) != 1) progress = 0; fclose(f); }
        progress = std::max(0, std::min(progress, ChapterCount()));
    }
    return progress;
}
static void SaveProgress(int done) {
    if (done <= StoryProgress()) return;
    progress = done;
    if (strcmp(recordsFile, "records.txt")) return;   // las pruebas no tocan la partida del jugador
    if (FILE *f = fopen("historia.txt", "w")) { fprintf(f, "%d\n", progress); fclose(f); }
}

// ---------- Diálogo: cola de frases con subtítulos ----------
static std::map<std::string, Sound> soundCache;
static std::deque<const Line *> radioQ;
static const Line *talking = nullptr;
static Sound *talkSnd = nullptr;
static float talkEnd = 0, talkStart = 0, nextTalk = 0;
static bool talkPaused = false;

static Sound *LineSound(const char *id) {
    if (!IsAudioDeviceReady()) return nullptr;
    auto it = soundCache.find(id);
    if (it == soundCache.end()) {
        const char *path = TextFormat("assets/sfx/historia/%s.wav", id);
        Sound s{};
        if (FileExists(path)) s = LoadSound(path);
        it = soundCache.emplace(id, s).first;
    }
    return it->second.frameCount ? &it->second : nullptr;
}
static float LineLen(const Line *l, Sound *s) {
    if (s && s->stream.sampleRate) return (float)s->frameCount / s->stream.sampleRate;
    return 1.2f + (l ? l->text.size() : 10) * 0.055f;
}
Sound *StoryLineSound(const char *id) { return LineSound(id); }
const char *StoryLineFile(const char *id) { return TextFormat("assets/sfx/historia/%s.wav", id); }
const char *StoryLineText(const char *id, int &who) { const Line *l = FindLine(id); who = l ? l->who : SP_FRANK; return l ? l->text.c_str() : id; }
const char *StorySpeakerName(int who) { return SPEAKER_NAME[std::max(0, std::min(who, SP_COUNT - 1))]; }
Color StorySpeakerColor(int who) { return SPEAKER_COLOR[std::max(0, std::min(who, SP_COUNT - 1))]; }

static void Radio(const char *id) { if (const Line *l = FindLine(id)) radioQ.push_back(l); }
static void StopRadio() {
    if (talkSnd) StopSound(*talkSnd);
    talking = nullptr; talkSnd = nullptr; radioQ.clear(); talkPaused = false;
}
void StopRadioForCutscene() { StopRadio(); }
bool StoryTalking() { return active && (talking || !radioQ.empty()); }

static void UpdateRadio() {
    if (talking && storyT >= talkEnd) { talking = nullptr; talkSnd = nullptr; nextTalk = storyT + 0.35f; lastTalkEnd = storyT; }
    if (!talking && !radioQ.empty() && storyT >= nextTalk && !AllySpeaking()) {
        talking = radioQ.front(); radioQ.pop_front();
        talkSnd = LineSound(talking->id.c_str());
        if (talkSnd) {
            SetSoundVolume(*talkSnd, clampf(opts.sfx * 1.1f, 0, 1));
            PlaySound(*talkSnd);
        }
        talkStart = storyT;
        talkEnd = storyT + LineLen(talking, talkSnd);
    }
}
// La radio se pausa con el juego (la llama el bucle principal cada fotograma)
void StoryAudioTick() {
    bool playing = game.state == GS_PLAYING;
    if (talkSnd && !playing && !talkPaused && IsSoundPlaying(*talkSnd)) { PauseSound(*talkSnd); talkPaused = true; }
    if (talkSnd && playing && talkPaused) { ResumeSound(*talkSnd); talkPaused = false; }
}

// ---------- Pantallas de texto (entre capítulos) ----------
struct Card { int type; std::string a, b; const char *voice; float dur; };   // type 0: título, 1: narración
static std::vector<Card> cards;
static int cardIdx = 0;
static float cardT = 0, cardLen = 0;
static int afterCards = -1;   // capítulo que empieza al terminar las pantallas (-1: volver al menú)
static Sound *cardSnd = nullptr;

static void StartCard() {
    cardT = 0;
    if (cardSnd) StopSound(*cardSnd);
    cardSnd = nullptr;
    Card &c = cards[cardIdx];
    cardLen = c.dur;
    if (c.voice) {
        const Line *l = FindLine(c.voice);
        cardSnd = LineSound(c.voice);
        if (cardSnd) SetSoundVolume(*cardSnd, clampf(opts.sfx * 1.1f, 0, 1));
        cardLen = 0.9f + LineLen(l, cardSnd) + 1.3f;
    }
}
static void ShowCards(int nextChapter) {
    afterCards = nextChapter;
    cardIdx = 0;
    game.state = GS_STORY;
    zombies.clear();
    StopRadio();
    gunVisible = false;
    allyView.visible = false;
    StartCard();
}
static std::string LineTextS(const char *id) { const Line *l = FindLine(id); return l ? l->text : std::string(id); }
static void ChapterCards(std::vector<Card> &v, int ch) {
    const ChapterDef &c = Chapters()[ch];
    v.push_back({ 0, TextFormat("CAPÍTULO %d", ch + 1), c.title, nullptr, 3.6f });
    for (auto id : c.cards) v.push_back({ 1, LineTextS(id), "", id, 0 });
}

void StoryStart(int ch, bool skipCards) {
    LoadLines();
    active = true;
    chapter = ch;
    if (skipCards) { afterCards = ch; cards.clear(); ResetGame(Chapters()[ch].map); return; }
    cards.clear();
    if (ch == 0) cards.push_back({ 0, "OPERACIÓN LÁZARO", "Un relato de Última Ronda", nullptr, 3.4f });
    ChapterCards(cards, ch);
    ShowCards(ch);
}

void StoryQuit() {
    StopRadio();
    if (cardSnd) StopSound(*cardSnd);
    cardSnd = nullptr;
    active = false;
    items.clear();
}

static void EndCards() {
    if (cardSnd) StopSound(*cardSnd);
    cardSnd = nullptr;
    if (afterCards >= 0) { chapter = afterCards; ResetGame(Ch().map); }
    else { StoryQuit(); BackToMenu(); }
}

void UpdateStoryCards(float dt) {
    if (cards.empty()) { EndCards(); return; }
    if (IsKeyPressed(KEY_ESCAPE)) { EndCards(); return; }   // saltar todas
    cardT += dt;
    if (cardSnd && cardT >= 0.9f && cardT - dt < 0.9f) PlaySound(*cardSnd);
    bool skip = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (skip && cardT < 0.6f) skip = false;   // no saltarse una pantalla recién aparecida por accidente
    if (cardT >= cardLen || skip) {
        if (++cardIdx >= (int)cards.size()) { EndCards(); return; }
        StartCard();
    }
}

static float Smooth(float x) { x = clampf(x, 0, 1); return x * x * (3 - 2 * x); }
static void CText(const Font &f, const char *t, float x, float y, float size, Color c, float spacing = 1) {
    Vector2 m = MeasureTextEx(f, t, size, spacing);
    DrawTextEx(f, t, Vector2{ x - m.x / 2 + 3, y + 3 }, size, spacing, Color{ 0, 0, 0, (unsigned char)(c.a * 0.8f) });
    DrawTextEx(f, t, Vector2{ x - m.x / 2, y }, size, spacing, c);
}
// Corta un texto en líneas que entren en el ancho dado
static std::vector<std::string> Wrap(const std::string &t, float size, float maxW) {
    std::vector<std::string> out;
    std::string cur, word;
    auto flush = [&]() {
        std::string test = cur.empty() ? word : cur + " " + word;
        if (!cur.empty() && MeasureTextEx(fontUI, test.c_str(), size, 1).x > maxW) { out.push_back(cur); cur = word; }
        else cur = test;
        word.clear();
    };
    for (char c : t) { if (c == ' ') flush(); else word += c; }
    if (!word.empty()) flush();
    if (!cur.empty()) out.push_back(cur);
    return out;
}
// Texto que aparece letra a letra (sin cortar las tildes)
static std::string Typed(const std::string &s, float elapsed, float cps) {
    int n = (int)(elapsed * cps);
    if (n <= 0) return "";
    int bytes = 0, chars = 0;
    while (bytes < (int)s.size() && chars < n) { bytes++; while (bytes < (int)s.size() && (s[bytes] & 0xC0) == 0x80) bytes++; chars++; }
    return s.substr(0, bytes);
}

void DrawStoryCards() {
    int W = GetScreenWidth(), Hh = GetScreenHeight();
    float s = Hh / 900.0f;
    DrawRectangle(0, 0, W, Hh, BLACK);
    if (cards.empty()) return;
    const Card &c = cards[cardIdx];
    float a = Smooth(cardT / 0.7f) * (1 - Smooth((cardT - (cardLen - 0.6f)) / 0.6f));
    DrawRectangleGradientV(0, 0, W, Hh, Color{ 30, 0, 0, (unsigned char)(70 * a) }, Color{ 0, 0, 0, 0 });
    if (c.type == 0) {
        CText(fontUI, c.a.c_str(), W / 2.0f, Hh * 0.36f, 34 * s, Color{ 230, 230, 230, (unsigned char)(220 * a) }, 8);
        float ts = 104 * s + cardT * 3 * s;
        CText(fontTitle, c.b.c_str(), W / 2.0f, Hh * 0.36f + 50 * s, ts, Color{ 204, 0, 0, (unsigned char)(255 * a) }, 5);
    } else {
        float fs = 38 * s;
        std::string shown = Typed(c.a, cardT - 0.5f, 30);
        auto ls = Wrap(c.a, fs, W * 0.7f);
        // se escribe letra a letra pero cortado como el texto completo, para que no salte de línea
        size_t used = 0;
        float y = Hh / 2.0f - ls.size() * fs * 0.65f;
        for (auto &ln : ls) {
            std::string part = used < shown.size() ? shown.substr(used, std::min(ln.size(), shown.size() - used)) : "";
            used += ln.size() + 1;
            float lw = MeasureTextEx(fontUI, ln.c_str(), fs, 1).x;
            DrawTextEx(fontUI, part.c_str(), Vector2{ W / 2.0f - lw / 2 + 3, y + 3 }, fs, 1, Color{ 0, 0, 0, (unsigned char)(200 * a) });
            DrawTextEx(fontUI, part.c_str(), Vector2{ W / 2.0f - lw / 2, y }, fs, 1, Color{ 235, 228, 215, (unsigned char)(255 * a) });
            y += fs * 1.3f;
        }
    }
    Color dim{ 150, 150, 150, (unsigned char)(150 * a) };
    if (afterCards >= 0) CText(fontUI, TextFormat("OPERACIÓN LÁZARO · CAPÍTULO %d", afterCards + 1), W / 2.0f, 40 * s, 18 * s, dim, 4);
    else CText(fontUI, "OPERACIÓN LÁZARO", W / 2.0f, 40 * s, 18 * s, dim, 4);
    const char *help = "CLIC / ESPACIO: siguiente   ·   ESC: saltar";
    Vector2 hm = MeasureTextEx(fontUI, help, 17 * s, 1);
    DrawTextEx(fontUI, help, Vector2{ W - hm.x - 30 * s, Hh - 46 * s }, 17 * s, 1, Color{ 140, 140, 140, 150 });
    for (int i = 0; i < (int)cards.size(); i++) {
        float x = W / 2.0f + (i - (cards.size() - 1) / 2.0f) * 18 * s;
        DrawCircleV(Vector2{ x, Hh - 38 * s }, 4 * s, i == cardIdx ? Color{ 200, 30, 30, 255 } : Color{ 80, 80, 80, 255 });
    }
}

// ---------- Objetos de la misión ----------
static PMat matItem[IT_COUNT], matPlanted, matBlink;
static Texture2D itemLabel[IT_COUNT]{};
static bool assetsReady = false;

static void LoadStoryAssets() {
    if (assetsReady) return;
    assetsReady = true;
    for (int k = 0; k < IT_COUNT; k++) {
        matItem[k] = M("fixture");
        matItem[k].emis = Vector3Scale(ColorV(ITEM_COLOR[k]), HoldItem(k) ? 0.9f : 2.4f);   // las marcas en el suelo, más tenues
        itemLabel[k] = MakeLabel(ITEM_LABEL[k], Hex(ITEM_COLOR[k]), 56);
    }
    matPlanted = M("darkmetal");
    matBlink = M("fixture");
    matBlink.emis = Vector3{ 6, 0.2f, 0.1f };
}

// Busca un lugar libre en la sala cerca del punto pedido (lejos de paredes, cajas, ventanas y puertas)
static Vector2 FreeSpot(const char *room, float x, float z) {
    const Room &r = rooms[RoomIndex(room)];
    auto ok = [&](float px, float pz) {
        if (px < r.x1 + 1.4f || px > r.x2 - 1.4f || pz < r.z1 + 1.4f || pz > r.z2 - 1.4f) return false;
        if (InsideCollider(px, pz, 1.0f)) return false;
        for (auto &w : windows) if (hyp(w.x - px, w.z - pz) < 3.0f) return false;
        for (auto &l : links) if (hyp(l.x - px, l.z - pz) < 3.0f) return false;
        if (hyp(box.pos.x - px, box.pos.y - pz) < 2.5f) return false;
        return true;
    };
    for (float rad = 0; rad < 10; rad += 0.5f)
        for (int a = 0; a < (rad > 0 ? 16 : 1); a++) {
            float px = x + cosf(a * PI_F / 8) * rad, pz = z + sinf(a * PI_F / 8) * rad;
            if (ok(px, pz)) return Vector2{ px, pz };
        }
    return Vector2{ x, z };
}

static bool ItemActive(const Item &it) { return active && endT < 0 && started && !stepDone && it.step == step && !it.taken && !inCutscene; }
static int ItemsOfStep() { int n = 0; for (auto &it : items) if (it.step == step) n++; return n; }

static void TakeItem(int i) {
    Item &it = items[i];
    if (it.taken) return;
    it.taken = true;
    itemsDone++;
    if (it.icon >= 0 && it.icon < (int)mmIcons.size()) mmIcons[it.icon].r = 0;
    if (it.light >= 0 && it.light < (int)lights.size()) lights[it.light].k = it.kind == IT_CHARGE ? 0.25f : it.kind == IT_TERMINAL ? 0.5f : 0;
    Play(HoldItem(it.kind) ? "pin" : "perk", 0.8f);
    int n = ItemsOfStep();
    CenterMsg(TextFormat("%s %d/%d", ITEM_MSG[it.kind], itemsDone, n), Hex(ITEM_COLOR[it.kind]), 2.2f);
    const StepDef &s = St();
    if (itemsDone - 1 < (int)s.onItem.size())
        for (auto id : s.onItem[itemsDone - 1]) Radio(id);
}

// Lo llama ResetGame después de construir el mapa
void StorySetupMap() {
    if (!active) return;
    LoadStoryAssets();
    StopRadio();
    items.clear();
    roomsSeen.clear();
    step = 0; stepT = 0; doneT = 0; endT = -1; stepDone = false; started = false; itemsDone = 0; timedNext = 0; goalFlash = 0; defendK = 0;
    inCutscene = waitCut = false; bossId = -1; chatterNext = 0; lastTalkEnd = storyT;
    game.nextDogRound = 9999;   // la historia no tiene rondas de perros
    const ChapterDef &c = Ch();
    for (int si = 0; si < (int)c.steps.size(); si++) {
        for (auto &d : c.steps[si].items) {
            Vector2 p = FreeSpot(d.room, d.x, d.z);
            Item it; it.kind = c.steps[si].itemKind; it.step = si; it.pos = Vector3{ p.x, 0, p.y };
            mmIcons.push_back({ p.x, p.y, Hex(ITEM_COLOR[it.kind]), 0 });   // se muestra cuando toca
            it.icon = (int)mmIcons.size() - 1;
            lights.push_back({ Vector3{ p.x, 1.4f, p.y }, Vector3Scale(ColorV(ITEM_COLOR[it.kind]), 1.6f), 4.5f, false, 0, false });
            it.light = (int)lights.size() - 1;
            items.push_back(it);
        }
    }
    for (int i = 0; i < (int)items.size(); i++) {
        Interact in;
        in.pos = [i]() { return Vector2{ items[i].pos.x, items[i].pos.z }; };
        in.range = 1.7f;
        in.active = [i]() { return i < (int)items.size() && ItemActive(items[i]); };
        int kind = items[i].kind;
        if (HoldItem(kind)) {
            in.hold = true;
            in.text = [i, kind]() {
                const char *what = kind == IT_CHARGE ? "colocar la carga" : "copiar el código";
                if (items[i].hold > 0) return std::string(TextFormat("%s... %d%%", kind == IT_CHARGE ? "Armando la carga" : "Copiando el código", (int)(items[i].hold / 2.5f * 100)));
                return std::string(TextFormat("Mantén E para %s", what));
            };
            in.action = [i]() {
                items[i].hold += frameDt;
                if (items[i].hold >= 2.5f) TakeItem(i);
            };
        } else {
            in.text = [kind]() {
                switch (kind) {
                    case IT_DIARY: return std::string("Presiona E para escuchar el diario");
                    case IT_SAMPLE: return std::string("Presiona E para tomar la muestra");
                    case IT_FILE: return std::string("Presiona E para leer la ficha");
                    case IT_KEYCARD: return std::string("Presiona E para tomar la tarjeta");
                    default: return std::string("Presiona E para tomar el componente");
                }
            };
            in.action = [i]() { TakeItem(i); };
        }
        interactables.push_back(in);
    }
}

void DrawStoryWorld() {
    if (!active || items.empty()) return;
    float t = (float)GetTime();
    for (auto &it : items) {
        Vector3 p = it.pos;
        bool show = ItemActive(it);
        if (it.kind == IT_CHARGE || it.kind == IT_TERMINAL) {
            if (it.taken && it.kind == IT_CHARGE) {   // carga colocada: bloque oscuro con una luz roja que parpadea
                DrawPM(unitCube, matPlanted, MatrixMultiply(MatrixScale(0.5f, 0.3f, 0.36f), MatrixTranslate(p.x, 0.15f, p.z)));
                if (fmodf(t, 0.8f) < 0.25f) DrawPM(unitCube, matBlink, MatrixMultiply(MatrixScale(0.08f, 0.08f, 0.08f), MatrixTranslate(p.x, 0.34f, p.z)));
            } else if (it.kind == IT_TERMINAL) {   // terminal: siempre está; brilla cuando toca usarla
                DrawPM(unitCube, matPlanted, MatrixMultiply(MatrixScale(0.7f, 1.1f, 0.5f), MatrixTranslate(p.x, 0.55f, p.z)));
                if (show || it.taken) DrawPM(unitCube, matItem[IT_TERMINAL], MatrixMultiply(MatrixScale(0.55f, 0.35f, 0.52f), MatrixTranslate(p.x, 1.25f, p.z)));
            } else if (show) {
                float k = 0.6f + 0.4f * sinf(t * 4);
                DrawPM(unitCube, matItem[IT_CHARGE], MatrixMultiply(MatrixScale(0.75f * k + 0.1f, 0.03f, 0.75f * k + 0.1f), MatrixTranslate(p.x, 0.03f, p.z)));
            }
            continue;
        }
        if (!show) continue;
        float bob = 1.0f + sinf(t * 2 + p.x) * 0.08f;
        Matrix rot = MatrixRotateY(t * 1.4f);
        if (it.kind == IT_SAMPLE || it.kind == IT_COMPONENT)
            DrawPM(unitCyl, matItem[it.kind], MatrixMultiply(MatrixMultiply(MatrixScale(0.12f, 0.36f, 0.12f), rot), MatrixTranslate(p.x, bob - 0.18f, p.z)));
        else if (it.kind == IT_KEYCARD)
            DrawPM(unitCube, matItem[it.kind], MatrixMultiply(MatrixMultiply(MatrixScale(0.3f, 0.03f, 0.2f), rot), MatrixTranslate(p.x, bob, p.z)));
        else
            DrawPM(unitCube, matItem[it.kind], MatrixMultiply(MatrixMultiply(MatrixScale(0.42f, 0.12f, 0.3f), rot), MatrixTranslate(p.x, bob, p.z)));
    }
}
void DrawUnlitBegin(float intensity);
void DrawUnlitEnd();
void DrawStoryLabels() {
    if (!active) return;
    DrawUnlitBegin(1.2f);
    for (auto &it : items) if (ItemActive(it)) DrawWorldLabel(itemLabel[it.kind], Vector3{ it.pos.x, it.kind == IT_CHARGE ? 1.0f : 1.75f, it.pos.z }, 1.1f);
    DrawUnlitEnd();
}

// ---------- Pasos de la misión ----------
static void StartStep() {
    stepT = 0; doneT = 0; stepDone = false; timedNext = 0; goalFlash = 3; itemsDone = 0; defendK = 0;
    const StepDef &s = St();
    for (auto id : s.onStart) Radio(id);
    for (auto &it : items) if (it.step == step) {
        if (it.icon >= 0 && it.icon < (int)mmIcons.size()) mmIcons[it.icon].r = 0.55f;
        if (it.light >= 0 && it.light < (int)lights.size()) lights[it.light].k = 1;
    }
    if (s.kind == ST_BOSS) {
        float hp = 9000 + game.zHP * s.n;
        bossId = SpawnBoss(Vector3{ s.zone.x, 0, s.zone.y }, hp);
        bossMax = hp;
        vm.shake = std::max(vm.shake, 0.6f);
    }
    Play("round", 0.35f);
}

// Al terminar la cinemática de llegada
void StoryOnIntroEnd() {   // charla entre Frank y Milton antes de empezar
    inCutscene = true;
    StartCutscene(TextFormat("charla_%d", chapter));
}

static bool StepComplete() {
    const StepDef &s = St();
    switch (s.kind) {
        case ST_ROUNDS: return game.round >= s.n && game.between;
        case ST_POWER: return powerOn;
        case ST_COLLECT: case ST_HOLDE: return itemsDone >= ItemsOfStep();
        case ST_REACH: return RoomAt(player.pos.x, player.pos.z) == RoomIndex(s.room);
        case ST_HOLD: return stepT >= s.n;
        case ST_DEFEND: return defendK >= s.n;
        case ST_BOSS: return !FindBoss() && stepT > 1;
    }
    return false;
}

static void ClearThreat() {
    game.toSpawn = 0; game.between = true; game.betweenT = 1e9f;
    for (auto &z : zombies) if (z.state != Z_DEAD) KillZombie(z, false, true);
}

static void FinishChapter() {
    endT = 0;
    SaveProgress(chapter + 1);
    ClearThreat();
    if (Ch().outro) {   // cinemática final del capítulo y después las pantallas
        StopRadio();
        inCutscene = true;
        StartCutscene(Ch().outro);
        return;
    }
    StopRadio();
    for (auto id : St().onDone) Radio(id);
    CenterMsg(chapter == 0 ? "¡EXTRACCIÓN!" : "¡LO LOGRARON!", Color{ 120, 230, 120, 255 }, 3);
}

static void ToNextCards() {
    cards.clear();
    if (chapter + 1 < ChapterCount()) {
        cards.push_back({ 0, TextFormat("CAPÍTULO %d COMPLETADO", chapter + 1), Ch().title, nullptr, 3.4f });
        if (Ch().ending) cards.push_back({ 1, Ch().ending, "", nullptr, 4.6f });
        ChapterCards(cards, chapter + 1);
        ShowCards(chapter + 1);
    } else {
        cards.push_back({ 0, TextFormat("CAPÍTULO %d COMPLETADO", chapter + 1), Ch().title, nullptr, 3.2f });
        for (auto id : EPILOGUE) cards.push_back({ 1, LineTextS(id), "", id, 0 });
        cards.push_back({ 0, "Gracias por jugar · madebyfrank", "FIN", nullptr, 5.0f });
        ShowCards(-1);
    }
}

// La cinemática terminó: seguir con la misión (o pasar al próximo capítulo)
void StoryOnCutsceneEnd(const char *id) {
    (void)id;
    if (!active) return;
    inCutscene = false;
    if (!strcmp(id, "charla")) { CenterMsg(Ch().title, Color{ 255, 64, 64, 255 }, 2.8f); return; }
    if (endT >= 0) { ToNextCards(); return; }
    if (waitCut) { waitCut = false; doneT = 99; }   // pasa enseguida al próximo paso
}

void StoryOnAllyDown() {
    if (active && !StoryTalking()) Radio(frand(0, 1) < 0.5f ? "g_down1" : "g_down2");
}

void UpdateStory(float dt) {
    if (!active) return;
    storyT += dt;
    frameDt = dt;
    goalFlash = std::max(0.0f, goalFlash - dt);
    UpdateRadio();
    if (!IsKeyDown(KEY_E)) for (auto &it : items) if (!it.taken) it.hold = 0;
    if (endT >= 0) {
        endT += dt;
        if (endT > 4.5f && !StoryTalking() && endT > 6.0f) ToNextCards();
        return;
    }
    if (!started) {   // primer objetivo un momento después de la cinemática
        stepT += dt;
        if (stepT > 1.5f) { started = true; StartStep(); }
        return;
    }
    // al entrar por primera vez a una sala
    int pr = RoomAt(player.pos.x, player.pos.z);
    if (pr >= 0 && std::find(roomsSeen.begin(), roomsSeen.end(), pr) == roomsSeen.end()) {
        roomsSeen.push_back(pr);
        for (auto &rl : Ch().roomLines) if (RoomIndex(rl.room) == pr && pr != startRoom) for (auto id : rl.ids) Radio(id);
    }
    // charla cuando hay calma
    int alive = 0;
    for (auto &z : zombies) if (z.state != Z_DEAD) alive++;
    if (!StoryTalking() && !AllySpeaking() && ally.active && !ally.downed && chatterNext < (int)Ch().chatter.size()
        && storyT - lastTalkEnd > 40 && (game.between || alive == 0) && stepT > 8) {
        for (auto id : Ch().chatter[chatterNext]) Radio(id);
        chatterNext++;
    }
    const StepDef &s = St();
    stepT += dt;
    while (timedNext < s.timed.size() && stepT >= s.timed[timedNext].t) Radio(s.timed[timedNext++].id);
    // defender: la barra solo avanza con el jugador dentro de la zona
    if (s.kind == ST_DEFEND && !stepDone && hyp(player.pos.x - s.zone.x, player.pos.z - s.zone.y) < s.radius) defendK += dt;
    if (!stepDone && StepComplete()) {
        stepDone = true; doneT = 0;
        if (step + 1 >= (int)Ch().steps.size()) {
            for (auto id : s.onDone) Radio(id);
            FinishChapter();
            return;
        }
        for (auto id : s.onDone) Radio(id);
        CenterMsg("OBJETIVO CUMPLIDO", Color{ 255, 216, 74, 255 }, 2.2f);
        Play("roundEnd", 0.6f);
        if (s.cutscene) { waitCut = true; inCutscene = true; StartCutscene(s.cutscene); return; }
    }
    if (stepDone && !waitCut) {
        doneT += dt;
        if (doneT > 2.5f) { step++; StartStep(); }
    }
}
// Fundido a negro al final del capítulo (sin cinemática)
float StoryFade() { return active && endT > 4.5f && !Ch().outro ? Smooth((endT - 4.5f) / 1.5f) : 0; }

// ---------- Interfaz ----------
static void HText(const char *t, float x, float y, float size, Color c, float align = 0) {
    Vector2 m = MeasureTextEx(fontUI, t, size, 1);
    x -= m.x * align;
    DrawTextEx(fontUI, t, Vector2{ x + 2, y + 2 }, size, 1, Color{ 0, 0, 0, (unsigned char)(c.a * 0.85f) });
    DrawTextEx(fontUI, t, Vector2{ x, y }, size, 1, c);
}

static bool Target(Vector3 &p) {
    if (endT >= 0 || !started || stepDone) return false;
    const StepDef &s = St();
    if (s.kind == ST_POWER) { Vector2 v; if (!powerOn && PowerSwitchSpot(v)) { p = Vector3{ v.x, 1.6f, v.y }; return true; } return false; }
    if (s.kind == ST_REACH) {
        const Room &r = rooms[RoomIndex(s.room)];
        p = Vector3{ (r.x1 + r.x2) / 2, 1.6f, (r.z1 + r.z2) / 2 };
        return true;
    }
    if (s.kind == ST_DEFEND) { p = Vector3{ s.zone.x, 1.2f, s.zone.y }; return hyp(player.pos.x - s.zone.x, player.pos.z - s.zone.y) > s.radius * 0.8f; }
    if (s.kind == ST_COLLECT || s.kind == ST_HOLDE) {
        float bd = 1e9f; bool any = false;
        for (auto &it : items) {
            if (it.taken || it.step != step) continue;
            float d = hyp(it.pos.x - player.pos.x, it.pos.z - player.pos.z);
            if (d < bd) { bd = d; p = Vector3{ it.pos.x, 1.3f, it.pos.z }; any = true; }
        }
        return any;
    }
    return false;
}

void DrawStoryHUD() {
    if (!active) return;
    int W = GetScreenWidth(), Hh = GetScreenHeight();
    float s = Hh / 900.0f;
    if (started && endT < 0) {
        const StepDef &st = St();
        std::string goal = st.goal;
        if (st.kind == ST_COLLECT || st.kind == ST_HOLDE) { int n = ItemsOfStep(); if (n > 1) goal += TextFormat("  %d/%d", itemsDone, n); }
        if (st.kind == ST_HOLD) { int left = (int)ceilf(std::max(0.0f, st.n - stepT)); goal += TextFormat("  %d:%02d", left / 60, left % 60); }
        if (st.kind == ST_DEFEND) goal += TextFormat("  %d%%", (int)(std::min(1.0f, defendK / st.n) * 100));
        float x = 40 * s, y = 150 * s;
        float gw = MeasureTextEx(fontUI, goal.c_str(), 25 * s, 1).x;
        DrawRectangleGradientH((int)(x - 14 * s), (int)(y - 8 * s), (int)(gw + 90 * s), (int)(74 * s), Color{ 0, 0, 0, 170 }, Color{ 0, 0, 0, 0 });
        DrawRectangle((int)(x - 14 * s), (int)(y - 8 * s), (int)(4 * s), (int)(74 * s), Color{ 200, 30, 30, 255 });
        HText(TextFormat("CAPÍTULO %d · %s", chapter + 1, Ch().title), x, y, 16 * s, Color{ 255, 200, 90, 230 });
        Color gc = stepDone ? Color{ 150, 230, 120, 255 } : WHITE;
        if (goalFlash > 0 && (int)(goalFlash * 4) % 2 == 0) gc = Color{ 255, 216, 74, 255 };
        HText(stepDone ? TextFormat("%s  · HECHO", st.goal) : goal.c_str(), x, y + 24 * s, 25 * s, gc);
        // defender: barra de progreso y aviso si sales de la zona
        if (st.kind == ST_DEFEND && !stepDone) {
            float bw = 360 * s, bx = W / 2 - bw / 2, by = 96 * s;
            bool in = hyp(player.pos.x - st.zone.x, player.pos.z - st.zone.y) < st.radius;
            DrawRectangle((int)bx, (int)by, (int)bw, (int)(10 * s), Color{ 0, 0, 0, 160 });
            DrawRectangle((int)bx, (int)by, (int)(bw * std::min(1.0f, defendK / st.n)), (int)(10 * s), in ? Color{ 90, 200, 255, 255 } : Color{ 255, 120, 60, 255 });
            if (!in && (int)(GetTime() * 2) % 2 == 0) HText("¡VUELVE A LA ZONA!", W / 2.0f, by + 16 * s, 22 * s, Color{ 255, 120, 60, 255 }, 0.5f);
        }
        // jefe: barra de vida arriba
        if (st.kind == ST_BOSS) {
            if (const Zombie *b = FindBoss()) {
                float bw = 520 * s, bx = W / 2 - bw / 2, by = 70 * s;
                HText("LÁZARO · PACIENTE CERO", W / 2.0f, by - 30 * s, 24 * s, Color{ 255, 90, 70, 255 }, 0.5f);
                DrawRectangle((int)bx, (int)by, (int)bw, (int)(14 * s), Color{ 0, 0, 0, 170 });
                DrawRectangleGradientH((int)bx, (int)by, (int)(bw * clampf(b->hp / bossMax, 0, 1)), (int)(14 * s), Color{ 120, 0, 0, 255 }, Color{ 230, 40, 30, 255 });
                DrawRectangleLines((int)bx, (int)by, (int)bw, (int)(14 * s), Color{ 255, 255, 255, 90 });
            }
        }
        // marcador en pantalla hacia el objetivo
        Vector3 tp;
        if (Target(tp)) {
            Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
            Vector3 to = Vector3Subtract(tp, cam.position);
            bool behind = Vector3DotProduct(fwd, to) < 0.2f;
            Vector2 sp = GetWorldToScreen(tp, cam);
            if (behind) { sp.x = W - sp.x; sp.y = Hh * 0.85f; }
            float m = 60 * s;
            sp.x = clampf(sp.x, m, W - m); sp.y = clampf(sp.y, m + 120 * s, Hh - m - 60 * s);
            float d = hyp(tp.x - player.pos.x, tp.z - player.pos.z);
            if (d > 2.2f) {
                float r = 11 * s;
                Color mc{ 255, 216, 74, 230 };
                DrawTriangle(Vector2{ sp.x, sp.y - r }, Vector2{ sp.x - r, sp.y }, Vector2{ sp.x + r, sp.y }, mc);
                DrawTriangle(Vector2{ sp.x - r, sp.y }, Vector2{ sp.x, sp.y + r }, Vector2{ sp.x + r, sp.y }, mc);
                HText(TextFormat("%d m", (int)d), sp.x, sp.y + r + 4 * s, 16 * s, Color{ 255, 236, 180, 230 }, 0.5f);
            }
        }
    }
    // subtítulos del diálogo
    if (talking) {
        float k = storyT - talkStart, len = talkEnd - talkStart;
        float a = Smooth(k / 0.15f) * (1 - Smooth((k - len + 0.15f) / 0.15f));
        float fs = 26 * s;
        auto ls = Wrap(talking->text, fs, W * 0.6f);
        float y = Hh * 0.79f - (ls.size() - 1) * fs * 1.25f;
        Color wc = SPEAKER_COLOR[talking->who]; wc.a = (unsigned char)(a * 255);
        HText(TextFormat("%s%s", IsRadio(talking->who) ? "RADIO · " : "", SPEAKER_NAME[talking->who]), W / 2.0f, y - 26 * s, 18 * s, wc, 0.5f);
        for (auto &ln : ls) {
            HText(ln.c_str(), W / 2.0f, y, fs, Color{ 245, 245, 245, (unsigned char)(a * 255) }, 0.5f);
            y += fs * 1.25f;
        }
    }
    float fade = StoryFade();
    if (fade > 0) DrawRectangle(0, 0, W, Hh, Color{ 0, 0, 0, (unsigned char)(fade * 255) });
}

// ---------- Cinemática de llegada ----------
bool StoryIntroLines(const char *&place, const char *ids[4]) {
    if (!active) return false;
    place = Ch().place;
    for (int i = 0; i < 4; i++) ids[i] = Ch().intro[i];
    return true;
}
const char *StoryChapterTitle() { return active ? Ch().title : ""; }
const char *StoryChapterName(int ch, const char *&sub) {
    static const char *SUB[] = { "Búnker 7", "La fábrica", "Hospital San Rafael", "San Roque", "Fuerte Reyes", "Laboratorio Lázaro" };
    sub = ch >= 0 && ch < 6 ? SUB[ch] : "";
    return ch >= 0 && ch < ChapterCount() ? Chapters()[ch].title : "";
}
int StoryChapterCount() { return ChapterCount(); }

// Para la prueba automática (--story)
const char *StoryDebug() {
    static const char *KIND[] = { "ROUNDS", "POWER", "COLLECT", "PLANT", "REACH", "HOLD", "DEFEND", "BOSS" };
    if (!active) return "inactiva";
    if (inCutscene) return TextFormat("cap %d cinemática", chapter + 1);
    if (endT >= 0) return TextFormat("cap %d terminado (radio %d)", chapter + 1, (int)StoryTalking());
    if (!started) return TextFormat("cap %d esperando", chapter + 1);
    const StepDef &s = St();
    return TextFormat("cap %d paso %d %s%s%s objetos %d/%d ronda %d%s", chapter + 1, step + 1, KIND[s.kind], s.room ? " sala " : "", s.room ? s.room : "",
                      itemsDone, ItemsOfStep(), game.round, stepDone ? " hecho" : "");
}
Vector2 StoryDefendZone() { return active && started ? St().zone : Vector2{ 0, 0 }; }

// ---------- Volumen de la voz (para que el que habla mueva la cabeza) ----------
static std::map<std::string, std::vector<float>> envCache;
float VoiceEnvelope(const char *file, float t) {
    auto it = envCache.find(file);
    if (it == envCache.end()) {
        std::vector<float> env;
        if (FileExists(file)) {
            Wave w = LoadWave(file);
            if (w.data && w.frameCount) {
                WaveFormat(&w, 22050, 32, 1);
                const float *s = (const float *)w.data;
                int win = 22050 / 25;   // 25 valores por segundo
                float peak = 1e-4f;
                for (unsigned i = 0; i < w.frameCount; i += win) {
                    float acc = 0; int n = 0;
                    for (unsigned k = i; k < w.frameCount && k < i + win; k++) { acc += s[k] * s[k]; n++; }
                    env.push_back(sqrtf(acc / std::max(1, n)));
                    peak = std::max(peak, env.back());
                }
                for (auto &e : env) e = clampf(e / peak * 1.3f, 0, 1);
            }
            UnloadWave(w);
        }
        it = envCache.emplace(file, env).first;
    }
    const auto &env = it->second;
    if (env.empty() || t < 0) return 0;
    int i = (int)(t * 25);
    return i < (int)env.size() ? env[i] : 0;
}
// ¿Milton está diciendo una frase del guion? (nivel de su voz ahora)
float StoryMiltonTalk() {
    if (!active || !talking || talking->who != SP_MILTON) return 0;
    return VoiceEnvelope(StoryLineFile(talking->id.c_str()), storyT - talkStart);
}
