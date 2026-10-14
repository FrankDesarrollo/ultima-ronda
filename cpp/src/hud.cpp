// Interfaz: HUD, minimapa, menú principal, pausa y fin de partida.
#include "game.h"

std::string promptText;
static std::string centerText, specialText;
static Color centerColor = WHITE;
static float centerT = 0, centerDur = 2.5f, specialT = 0, hitT = 0, roundFlashT = 0, denyT = 0;
static bool hitKill = false;
struct Pop { int n; float t, x; };
static std::vector<Pop> pops;
static RenderTexture2D mmRT = {};
static int uiClick = -1;
Shader &MaskShader();

void CenterMsg(const std::string &text, Color c, float dur) { centerText = text; centerColor = c; centerT = dur; centerDur = dur; }
void SpecialMsg(const std::string &text) { specialText = text; specialT = 4.5f; }
void ShowHit(bool kill) { hitKill = kill; hitT = kill ? 0.2f : 0.1f; }
void PointsPop(int n) { pops.push_back({ n, 0.9f, frand(0, 40) }); }
void PointsDeny() { denyT = 0.4f; }
void RoundFlash() { roundFlashT = 2.4f; }

void UpdateHUDTimers(float dt) {
    centerT -= dt; specialT -= dt; hitT -= dt; roundFlashT -= dt; denyT -= dt;
    for (int i = (int)pops.size() - 1; i >= 0; i--) { pops[i].t -= dt; if (pops[i].t <= 0) pops.erase(pops.begin() + i); }
}

static float S() { return GetScreenHeight() / 900.0f; }
static void Text(const Font &f, const char *t, float x, float y, float size, Color c, float align = 0, int shadow = 2) {
    Vector2 m = MeasureTextEx(f, t, size, 1);
    x -= m.x * align;
    if (shadow) {
        Color sc{ 0, 0, 0, (unsigned char)(c.a * 0.85f) };
        DrawTextEx(f, t, Vector2{ x + shadow, y + shadow }, size, 1, sc);
        DrawTextEx(f, t, Vector2{ x - 1, y - 1 }, size, 1, Color{ 0, 0, 0, (unsigned char)(c.a * 0.4f) });
    }
    DrawTextEx(f, t, Vector2{ x, y }, size, 1, c);
}

// ---------- Minimapa ----------
// Se dibuja en su textura antes de la pasada final (no se pueden anidar texturas de render)
void PrepareMinimap() {
    int px = 256;
    if (!mmRT.id) { mmRT = LoadRenderTexture(px, px); SetTextureFilter(mmRT.texture, TEXTURE_FILTER_BILINEAR); }
    const float scale = 4.2f; // píxeles por metro
    float fx = -sinf(player.yaw), fz = -cosf(player.yaw), rx = cosf(player.yaw), rz = -sinf(player.yaw);
    auto P = [&](float x, float z) {
        float dx = x - player.pos.x, dz = z - player.pos.z;
        return Vector2{ px / 2 + (dx * rx + dz * rz) * scale, px / 2 - (dx * fx + dz * fz) * scale };
    };
    auto Rect = [&](float x1, float z1, float x2, float z2, Color c) {
        Vector2 a = P(x1, z1), b = P(x2, z1), cc = P(x2, z2), d = P(x1, z2);
        DrawTriangle(a, d, cc, c); DrawTriangle(a, cc, b, c);
        DrawTriangle(a, cc, d, c); DrawTriangle(a, b, cc, c);
    };
    BeginTextureMode(mmRT);
    ClearBackground(Color{ 10, 10, 12, 200 });
    for (size_t i = 0; i < rooms.size(); i++) {
        const Room &r = rooms[i];
        Rect(r.x1, r.z1, r.x2, r.z2, Unlocked((int)i) ? Color{ 52, 50, 48, 255 } : Color{ 26, 25, 25, 255 });
    }
    for (auto &w : mmWalls) Rect(w.minX, w.minZ, w.maxX, w.maxZ, Color{ 190, 185, 175, 255 });
    for (auto &d : doors) if (d.sink < 3.2f && !links[d.link].open) Rect(d.col.minX, d.col.minZ, d.col.maxX, d.col.maxZ, Color{ 200, 140, 60, 255 });
    for (auto &w : windows) {
        Color c = w.boards == 5 ? Color{ 150, 110, 70, 255 } : w.boards > 0 ? Color{ 230, 120, 40, 255 } : Color{ 255, 40, 40, 255 };
        float hx = w.nx ? 0.25f : 0.8f, hz = w.nx ? 0.8f : 0.25f;
        Rect(w.x - hx, w.z - hz, w.x + hx, w.z + hz, c);
    }
    for (auto &ic : mmIcons) DrawCircleV(P(ic.x, ic.z), ic.r * scale, ic.c);
    DrawCircleV(P(box.pos.x, box.pos.y), 0.9f * scale, Color{ 127, 212, 255, 255 });
    for (auto &z : zombies) if (z.state != Z_DEAD && z.state != Z_RISE) DrawCircleV(P(z.pos.x, z.pos.z), 0.42f * scale, Color{ 255, 40, 40, 255 });
    Vector2 c{ px / 2.0f, px / 2.0f };
    DrawTriangle(Vector2{ c.x, c.y - 9 }, Vector2{ c.x - 6, c.y + 6 }, Vector2{ c.x + 6, c.y + 6 }, WHITE);
    EndTextureMode();
}
static void DrawMinimap(float cx, float cy, float size) {
    int px = 256;
    if (!mmRT.id) return;
    BeginShaderMode(MaskShader());
    DrawTexturePro(mmRT.texture, Rectangle{ 0, 0, (float)px, -(float)px }, Rectangle{ cx - size / 2, cy - size / 2, size, size }, Vector2{ 0, 0 }, 0, WHITE);
    EndShaderMode();
    DrawCircleLinesV(Vector2{ cx, cy }, size / 2, Color{ 255, 255, 255, 60 });
}

// ---------- HUD ----------
void DrawHUD() {
    float s = S();
    int W = GetScreenWidth(), Hh = GetScreenHeight();
    // mirilla del francotirador
    if (scoped) {
        float r = Hh * 0.36f;
        Vector2 c{ W / 2.0f, Hh / 2.0f };
        DrawRing(c, r, (float)std::max(W, Hh), 0, 360, 64, BLACK);
        DrawLineEx(Vector2{ c.x - r, c.y }, Vector2{ c.x + r, c.y }, 1.5f, BLACK);
        DrawLineEx(Vector2{ c.x, c.y - r }, Vector2{ c.x, c.y + r }, 1.5f, BLACK);
        DrawCircleLinesV(c, r, BLACK);
    }
    // número de ronda
    if (game.round > 0) {
        char rb[8]; snprintf(rb, 8, "%d", game.round);
        Color rc = game.dogRound ? Color{ 230, 100, 20, 255 } : Color{ 179, 0, 0, 255 };   // naranja en las rondas de perros
        if (roundFlashT > 0 && ((int)(roundFlashT * 2.5f) % 2 == 0)) rc = WHITE;
        float fs = 110 * s;
        DrawTextEx(fontTitle, rb, Vector2{ 34 * s + 4, 12 * s + 4 }, fs, 2, Color{ 0, 0, 0, 200 });
        for (int i = 0; i < 3; i++) DrawTextEx(fontTitle, rb, Vector2{ 34 * s + frand(-1, 1), 12 * s + frand(-1, 1) }, fs, 2, Color{ 255, 0, 0, 40 });
        DrawTextEx(fontTitle, rb, Vector2{ 34 * s, 12 * s }, fs, 2, rc);
    }
    DrawMinimap(W - 130 * s, 130 * s, 210 * s);
    if (opts.showFps) { char fb[16]; snprintf(fb, 16, "%d FPS", GetFPS()); Text(fontUI, fb, W - 130 * s, 245 * s, 18 * s, Color{ 160, 255, 160, 255 }, 0.5f, 1); }

    // mira
    if (!scoped) {
        float a = 1 - vm.ads;
        if (a > 0.05f) {
            float gap = (7 + (player.moving ? 6 : 0) + vm.kick * 10) * s, len = 9 * s;
            Color c{ 255, 255, 255, (unsigned char)(230 * a) };
            Vector2 m{ W / 2.0f, Hh / 2.0f };
            DrawRectangleV(Vector2{ m.x - 1, m.y - gap - len }, Vector2{ 2, len }, c);
            DrawRectangleV(Vector2{ m.x - 1, m.y + gap }, Vector2{ 2, len }, c);
            DrawRectangleV(Vector2{ m.x - gap - len, m.y - 1 }, Vector2{ len, 2 }, c);
            DrawRectangleV(Vector2{ m.x + gap, m.y - 1 }, Vector2{ len, 2 }, c);
        }
    }
    if (hitT > 0) {
        Vector2 m{ W / 2.0f, Hh / 2.0f };
        Color c = hitKill ? Color{ 255, 42, 42, 255 } : WHITE;
        float a = 6 * s, b = 14 * s;
        DrawLineEx(Vector2{ m.x - b, m.y - b }, Vector2{ m.x - a, m.y - a }, 2.5f, c);
        DrawLineEx(Vector2{ m.x + b, m.y - b }, Vector2{ m.x + a, m.y - a }, 2.5f, c);
        DrawLineEx(Vector2{ m.x - b, m.y + b }, Vector2{ m.x - a, m.y + a }, 2.5f, c);
        DrawLineEx(Vector2{ m.x + b, m.y + b }, Vector2{ m.x + a, m.y + a }, 2.5f, c);
    }
    // textos centrales
    if (!promptText.empty()) Text(fontUI, promptText.c_str(), W / 2.0f, Hh * 0.63f, 26 * s, WHITE, 0.5f);
    {
        const OwnedWeapon &w = CurWeapon();
        WStats st = Stats(w.id, w.pap);
        const char *rm = player.reloading ? "RECARGANDO..." : (w.mag == 0 && w.reserve == 0) ? "SIN MUNICIÓN" : (w.mag <= (int)ceilf(st.mag * 0.25f) && w.reserve > 0) ? "PRESIONA R PARA RECARGAR" : "";
        if (*rm) Text(fontUI, rm, W / 2.0f, Hh * 0.56f, 19 * s, WHITE, 0.5f);
    }
    if (centerT > 0 && game.state == GS_PLAYING) {
        float a = clampf(std::min(centerT, centerDur - centerT + 0.5f) * 2, 0, 1);
        Color c = centerColor; c.a = (unsigned char)(255 * a);
        Text(fontUI, centerText.c_str(), W / 2.0f, Hh * 0.24f, 48 * s, c, 0.5f, 3);
    }
    if (specialT > 0 && game.state == GS_PLAYING) {
        float a = clampf(std::min(specialT, 4.5f - specialT + 0.5f) * 2, 0, 1);
        Text(fontUI, specialText.c_str(), W / 2.0f, Hh * 0.33f, 36 * s, Color{ 255, 126, 182, (unsigned char)(255 * a) }, 0.5f, 3);
    }
    // power-ups activos
    {
        std::vector<std::pair<std::string, Color>> pu;
        char b[32];
        if (T < game.insta) { snprintf(b, 32, "MUERTE INSTANTÁNEA %d", (int)ceilf(game.insta - T)); pu.push_back({ b, Color{ 255, 59, 59, 255 } }); }
        if (T < game.dbl) { snprintf(b, 32, "x2 PUNTOS %d", (int)ceilf(game.dbl - T)); pu.push_back({ b, Color{ 59, 255, 107, 255 } }); }
        float total = 0;
        for (auto &p : pu) total += MeasureTextEx(fontUI, p.first.c_str(), 18 * s, 1).x + 40 * s;
        float x = W / 2.0f - total / 2;
        for (auto &p : pu) {
            float w = MeasureTextEx(fontUI, p.first.c_str(), 18 * s, 1).x + 26 * s;
            DrawRectangleRounded(Rectangle{ x, Hh - 72 * s, w, 34 * s }, 0.3f, 6, Color{ 0, 0, 0, 140 });
            DrawRectangleRoundedLinesEx(Rectangle{ x, Hh - 72 * s, w, 34 * s }, 0.3f, 6, 2, p.second);
            Text(fontUI, p.first.c_str(), x + 13 * s, Hh - 67 * s, 18 * s, p.second, 0, 0);
            x += w + 14 * s;
        }
    }
    // vida y bebidas
    float hx = 40 * s, hy = Hh - 50 * s;
    DrawRectangle((int)hx, (int)hy, (int)(240 * s), (int)(10 * s), Color{ 0, 0, 0, 140 });
    DrawRectangleGradientH((int)hx, (int)hy, (int)(240 * s * clampf(player.health / player.maxHealth, 0, 1)), (int)(10 * s), Color{ 138, 0, 0, 255 }, Color{ 232, 48, 48, 255 });
    DrawRectangleLines((int)hx, (int)hy, (int)(240 * s), (int)(10 * s), Color{ 255, 255, 255, 90 });
    Text(fontUI, TextFormat("SGTO. %s", LocalName()), hx, hy + 14 * s, 17 * s, Color{ 200, 210, 170, 220 }, 0, 1);
    // co-op: el compañero, caer y levantarse
    if (NetActive()) {
        Text(fontUI, TextFormat("%s  %d", RemoteName(), remote.points), W - 40 * s, Hh - 262 * s, 24 * s, remote.downed ? Color{ 255, 90, 60, 255 } : Color{ 150, 230, 120, 255 }, 1, 2);
        Text(fontUI, TextFormat("ping %d ms", NetPing()), W - 40 * s, Hh - 290 * s, 15 * s, Color{ 170, 170, 170, 180 }, 1, 1);
        if (remote.downed && !player.downed) {
            float d = hyp(remote.pos.x - player.pos.x, remote.pos.z - player.pos.z);
            bool blink = (int)(GetTime() * 3) % 2 == 0;
            Text(fontUI, TextFormat("¡%s CAÍDO! · a %d m", RemoteName(), (int)d), W / 2.0f, Hh * 0.12f, 30 * s, blink ? Color{ 255, 80, 60, 255 } : Color{ 255, 200, 180, 255 }, 0.5f, 2);
            if (d < 2.0f && player.reviveK <= 0) Text(fontUI, TextFormat("Mantén E para levantar a %s", RemoteName()), W / 2.0f, Hh * 0.62f, 24 * s, WHITE, 0.5f, 2);
        }
        if (player.reviveK > 0) {
            float bw = 320 * s;
            Text(fontUI, TextFormat("LEVANTANDO A %s", RemoteName()), W / 2.0f, Hh * 0.6f, 22 * s, WHITE, 0.5f, 2);
            DrawRectangle((int)(W / 2 - bw / 2), (int)(Hh * 0.6f + 32 * s), (int)bw, (int)(10 * s), Color{ 0, 0, 0, 160 });
            DrawRectangle((int)(W / 2 - bw / 2), (int)(Hh * 0.6f + 32 * s), (int)(bw * player.reviveK / 3), (int)(10 * s), Color{ 124, 196, 255, 255 });
        }
        if (player.downed) {
            Text(fontUI, "CAÍDO", W / 2.0f, Hh * 0.3f, 64 * s, Color{ 220, 30, 30, 255 }, 0.5f, 3);
            Text(fontUI, TextFormat("%s tiene %d s para levantarte", RemoteName(), (int)ceilf(std::max(0.0f, player.downT))), W / 2.0f, Hh * 0.3f + 74 * s, 24 * s, WHITE, 0.5f, 2);
        }
    }
    // lo que va diciendo Frank
    float sa = 0;
    if (const char *sub = VoiceSubtitle(sa)) {
        unsigned char a = (unsigned char)(sa * 255);
        float sy = Hh * 0.8f, fs = 26 * s;
        float tw = MeasureTextEx(fontUI, sub, fs, 1).x;
        Text(fontUI, TextFormat("%s:", LocalName()), W / 2.0f - tw / 2 - 10 * s, sy + 3 * s, 20 * s, Color{ 150, 210, 120, a }, 1, 2);
        Text(fontUI, sub, W / 2.0f, sy, fs, Color{ 245, 245, 245, a }, 0.5f, 2);
    }
    int pi = 0;
    for (int k = 0; k < P_COUNT; k++) {
        if (!HasPerk(player, k)) continue;
        Vector2 c{ hx + 21 * s + pi * 52 * s, hy - 32 * s };
        DrawCircleV(c, 21 * s, Hex(PERKS[k].color));
        DrawCircleLinesV(c, 21 * s, WHITE);
        char l[2] = { PERKS[k].letter, 0 };
        Text(fontUI, l, c.x, c.y - 12 * s, 22 * s, WHITE, 0.5f, 1);
        pi++;
    }
    // puntos
    char pb[16]; snprintf(pb, 16, "%d", player.points);
    Text(fontUI, pb, W - 40 * s, Hh - 220 * s, 44 * s, denyT > 0 ? Color{ 255, 59, 59, 255 } : Color{ 255, 233, 168, 255 }, 1, 3);
    for (auto &p : pops) {
        float k = 1 - p.t / 0.9f;
        char b[16]; snprintf(b, 16, "+%d", p.n);
        Text(fontUI, b, W - 40 * s - p.x * s - k * 50 * s, Hh - 250 * s - k * 50 * s, 24 * s, Color{ 255, 216, 74, (unsigned char)(255 * (1 - k)) }, 1, 1);
    }
    // espacios de armas (1, 2, 3)
    for (int i = 2; i >= 0; i--) {
        bool has = i < (int)player.weapons.size(), cur = i == player.cur;
        char sl[64];
        if (has) snprintf(sl, 64, "%d  %s", i + 1, Stats(player.weapons[i].id, player.weapons[i].pap).name);
        else snprintf(sl, 64, "%d  —", i + 1);
        Color c = cur ? Color{ 255, 233, 168, 255 } : has ? Color{ 160, 160, 160, 200 } : Color{ 110, 110, 110, 150 };
        Text(fontUI, sl, W - 40 * s, Hh - 330 * s + i * 22 * s, 17 * s, c, 1, 1);
    }
    // arma
    const OwnedWeapon &w = CurWeapon();
    WStats st = Stats(w.id, w.pap);
    Text(fontUI, player.papBusy ? "PACK-A-PUNCH..." : st.name, W - 40 * s, Hh - 160 * s, 22 * s, w.pap ? Color{ 208, 140, 255, 255 } : Color{ 220, 220, 220, 255 }, 1);
    char mag[16], res[16];
    snprintf(mag, 16, "%d", w.mag); snprintf(res, 16, " / %d", w.reserve);
    float rw = MeasureTextEx(fontUI, res, 28 * s, 1).x;
    Text(fontUI, res, W - 40 * s, Hh - 108 * s, 28 * s, Color{ 190, 190, 190, 255 }, 1);
    Text(fontUI, mag, W - 40 * s - rw, Hh - 128 * s, 58 * s, WHITE, 1, 3);
    std::string gr = "GRANADAS ";
    for (int i = 0; i < player.grenades; i++) gr += "o ";
    Text(fontUI, gr.c_str(), W - 40 * s, Hh - 62 * s, 19 * s, Color{ 159, 220, 143, 255 }, 1);
    DrawAllyHUD();
    DrawDialogHUD();
    DrawStoryHUD();
}

// ---------- Botones ----------
static bool Button(Rectangle r, const char *label, const char *sub, int id) {
    Vector2 m = GetMousePosition();
    bool hover = CheckCollisionPointRec(m, r);
    float s = S();
    DrawRectangleRounded(r, 0.12f, 8, hover ? Color{ 184, 0, 0, 255 } : Color{ 138, 0, 0, 235 });
    DrawRectangleRoundedLinesEx(r, 0.12f, 8, 2, Color{ 255, 64, 64, 255 });
    float ty = sub ? r.y + r.height * 0.18f : r.y + r.height / 2 - 16 * s;
    Text(fontUI, label, r.x + r.width / 2, ty, 32 * s, WHITE, 0.5f, 2);
    if (sub) Text(fontUI, sub, r.x + r.width / 2, r.y + r.height * 0.62f, 16 * s, Color{ 255, 179, 179, 255 }, 0.5f, 1);
    if (hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) { uiClick = id; return true; }
    return false;
}
int MenuClicked() { int c = uiClick; uiClick = -1; return c; }

// ---------- Opciones ----------
bool optionsOpen = false;
static int dragSlider = -1;
extern bool lowQuality;
// Barra deslizante: se arrastra con el ratón. Devuelve el valor nuevo.
static float Slider(int id, float x, float y, float w, const char *label, float v, float mn, float mx, const char *valueText) {
    float s = S();
    Text(fontUI, label, x, y, 22 * s, WHITE, 0, 1);
    Rectangle track{ x + w * 0.45f, y + 10 * s, w * 0.42f, 8 * s };
    Vector2 m = GetMousePosition();
    Rectangle hit{ track.x - 10 * s, track.y - 14 * s, track.width + 20 * s, track.height + 28 * s };
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(m, hit)) dragSlider = id;
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT) && dragSlider == id) dragSlider = -1;
    if (dragSlider == id) v = mn + clampf((m.x - track.x) / track.width, 0, 1) * (mx - mn);
    float k = (v - mn) / (mx - mn);
    DrawRectangleRounded(track, 1, 6, Color{ 60, 60, 60, 255 });
    DrawRectangleRounded(Rectangle{ track.x, track.y, track.width * k, track.height }, 1, 6, Color{ 200, 30, 30, 255 });
    DrawCircleV(Vector2{ track.x + track.width * k, track.y + track.height / 2 }, 11 * s, dragSlider == id ? Color{ 255, 120, 120, 255 } : WHITE);
    Text(fontUI, valueText, x + w, y, 22 * s, Color{ 255, 216, 74, 255 }, 1, 1);
    return v;
}
static bool Toggle(float x, float y, float w, const char *label, bool v) {
    float s = S();
    Text(fontUI, label, x, y, 22 * s, WHITE, 0, 1);
    Rectangle r{ x + w - 110 * s, y - 2 * s, 110 * s, 32 * s };
    bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRounded(r, 0.4f, 6, v ? Color{ 160, 20, 20, 255 } : Color{ 55, 55, 55, 255 });
    DrawRectangleRoundedLinesEx(r, 0.4f, 6, 2, hover ? WHITE : Color{ 150, 150, 150, 255 });
    Text(fontUI, v ? "SÍ" : "NO", r.x + r.width / 2, r.y + 3 * s, 22 * s, WHITE, 0.5f, 1);
    if (hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return !v;
    return v;
}
static void Overlay();
void DrawOptions() {
    float s = S();
    int W = GetScreenWidth(), Hh = GetScreenHeight();
    Overlay();
    float pw = 680 * s, ph = (EggRosaUnlocked() ? 708 : 654) * s, px = W / 2 - pw / 2, py = Hh / 2 - ph / 2;
    DrawRectangleRounded(Rectangle{ px, py, pw, ph }, 0.05f, 8, Color{ 18, 10, 10, 235 });
    DrawRectangleRoundedLinesEx(Rectangle{ px, py, pw, ph }, 0.05f, 8, 2, Color{ 200, 40, 40, 255 });
    Text(fontUI, "OPCIONES", W / 2.0f, py + 22 * s, 46 * s, WHITE, 0.5f, 3);
    float x = px + 40 * s, w = pw - 80 * s, y = py + 100 * s, gap = 54 * s;
    char b[32];
    snprintf(b, 32, "x%.1f", opts.sens);
    opts.sens = roundf(Slider(0, x, y, w, "Sensibilidad del ratón", opts.sens, 0.2f, 3.0f, b) * 10) / 10; y += gap;
    snprintf(b, 32, "%d%%", (int)roundf(opts.master * 100));
    opts.master = Slider(1, x, y, w, "Volumen general", opts.master, 0, 1, b); y += gap;
    snprintf(b, 32, "%d%%", (int)roundf(opts.music * 100));
    opts.music = Slider(2, x, y, w, "Música", opts.music, 0, 1, b); y += gap;
    snprintf(b, 32, "%d%%", (int)roundf(opts.sfx * 100));
    opts.sfx = Slider(3, x, y, w, "Efectos de sonido", opts.sfx, 0, 1, b); y += gap;
    snprintf(b, 32, "%d°", (int)roundf(opts.fov));
    opts.fov = roundf(Slider(4, x, y, w, "Campo de visión", opts.fov, 60, 100, b)); y += gap;
    lowQuality = Toggle(x, y, w, "Gráficos rápidos (para PCs lentas)", lowQuality); y += gap;
    opts.showFps = Toggle(x, y, w, "Mostrar FPS", opts.showFps); y += gap;
    opts.ally = Toggle(x, y, w, "Compañero bot (Milton) en supervivencia", opts.ally); y += gap;
    if (EggRosaUnlocked()) { opts.allyRosa = Toggle(x, y, w, "Uniforme rosa de Milton (secreto)", opts.allyRosa); y += gap; }
    Button(Rectangle{ W / 2 - 140 * s, py + ph - 84 * s, 280 * s, 60 * s }, "VOLVER", nullptr, 6);
}

static void Overlay() {
    int W = GetScreenWidth(), Hh = GetScreenHeight();
    DrawRectangleGradientV(0, 0, W, Hh, Color{ 40, 0, 0, 150 }, Color{ 0, 0, 0, 230 });
}

// ---------- Multijugador: sala de espera ----------
static void DrawLobby() {
    float s = S();
    int W = GetScreenWidth(), Hh = GetScreenHeight();
    Overlay();
    const char *title = "MULTIJUGADOR";
    float ts = 96 * s;
    Vector2 tm = MeasureTextEx(fontTitle, title, ts, 4);
    DrawTextEx(fontTitle, title, Vector2{ W / 2 - tm.x / 2 + 4, Hh * 0.08f + 4 }, ts, 4, Color{ 0, 0, 0, 220 });
    DrawTextEx(fontTitle, title, Vector2{ W / 2 - tm.x / 2, Hh * 0.08f }, ts, 4, Color{ 204, 0, 0, 255 });
    float y = Hh * 0.08f + ts + 20 * s;
    Color grey{ 190, 190, 190, 255 }, gold{ 255, 216, 74, 255 }, green{ 150, 230, 120, 255 };
    float bw = 300 * s, bh = 92 * s, gap = 24 * s;
    int lb = NetLobbyScreen();
    if (lb == LB_CHOICE) {
        Text(fontUI, "Co-op por internet para dos: FRANK crea la partida y MILTON se une con su IP.", W / 2.0f, y, 22 * s, grey, 0.5f, 1);
        Button(Rectangle{ W / 2 - bw - gap / 2, y + 50 * s, bw, bh }, "CREAR PARTIDA", "Juegas como FRANK (anfitrión)", 20);
        Button(Rectangle{ W / 2 + gap / 2, y + 50 * s, bw, bh }, "UNIRSE", "Juegas como MILTON", 21);
        Text(fontUI, "Quien crea la partida tiene que abrir el puerto UDP 27015 en su router (reenvío de puertos).", W / 2.0f, y + 170 * s, 19 * s, grey, 0.5f, 1);
    } else if (lb == LB_HOST_PICK) {
        Text(fontUI, "Elige el mapa", W / 2.0f, y, 28 * s, WHITE, 0.5f, 2);
        const char *names[6] = { "BÚNKER", "FÁBRICA", "PUEBLO", "HOSPITAL", "FUERTE REYES", "LABORATORIO" }, *subs[6] = { "Pasillos estrechos", "Mapa grande", "Al aire libre", "San Rafael", "Base militar", "Bajo tierra" };
        const int ids[6] = { 22, 23, 24, 27, 28, 29 };
        float mw = 260 * s, x0 = W / 2 - mw * 1.5f - gap;
        for (int i = 0; i < 6; i++) Button(Rectangle{ x0 + (i % 3) * (mw + gap), y + 50 * s + (i / 3) * (bh + gap), mw, bh }, names[i], subs[i], ids[i]);
    } else if (lb == LB_HOSTING) {
        std::string pub = NetPublicIP(), loc = NetLocalIP();
        Text(fontUI, NetStatus().c_str(), W / 2.0f, y, 30 * s, gold, 0.5f, 2);
        Text(fontUI, "Pásale a MILTON tu IP pública:", W / 2.0f, y + 60 * s, 22 * s, grey, 0.5f, 1);
        Text(fontUI, pub.empty() ? "averiguando..." : pub.c_str(), W / 2.0f, y + 92 * s, 48 * s, green, 0.5f, 3);
        Text(fontUI, TextFormat("Router: reenvía el puerto UDP 27015 a esta PC (%s)", loc.empty() ? "?" : loc.c_str()), W / 2.0f, y + 160 * s, 20 * s, grey, 0.5f, 1);
        Text(fontUI, "Si Windows pregunta por el firewall, dale a Permitir.", W / 2.0f, y + 190 * s, 20 * s, grey, 0.5f, 1);
    } else if (lb == LB_JOIN || lb == LB_CONNECTING) {
        Text(fontUI, "IP pública de FRANK:", W / 2.0f, y, 24 * s, grey, 0.5f, 1);
        Rectangle box{ W / 2 - 240 * s, y + 36 * s, 480 * s, 64 * s };
        DrawRectangleRec(box, Color{ 0, 0, 0, 170 });
        DrawRectangleLinesEx(box, 2, lb == LB_JOIN ? gold : grey);
        std::string ip = NetJoinIP();
        if (lb == LB_JOIN && ((int)(GetTime() * 2) % 2 == 0)) ip += "_";
        Text(fontUI, ip.c_str(), W / 2.0f, y + 46 * s, 40 * s, WHITE, 0.5f, 1);
        if (lb == LB_JOIN) {
            Text(fontUI, "Escribe la IP (o Ctrl+V para pegarla) y pulsa CONECTAR o Enter", W / 2.0f, y + 110 * s, 19 * s, grey, 0.5f, 1);
            Button(Rectangle{ W / 2 - 140 * s, y + 145 * s, 280 * s, 60 * s }, "CONECTAR", nullptr, 25);
        }
        if (!NetStatus().empty()) Text(fontUI, NetStatus().c_str(), W / 2.0f, y + 220 * s, 22 * s, lb == LB_CONNECTING ? gold : Color{ 255, 120, 100, 255 }, 0.5f, 1);
    }
    if (lb == LB_HOST_PICK && !NetStatus().empty()) Text(fontUI, NetStatus().c_str(), W / 2.0f, y + 290 * s, 22 * s, Color{ 255, 120, 100, 255 }, 0.5f, 1);
    Button(Rectangle{ W / 2 - 130 * s, Hh * 0.8f, 260 * s, 52 * s }, lb == LB_HOSTING || lb == LB_CONNECTING ? "CANCELAR" : "VOLVER", nullptr, 26);
}

// ---------- Modo historia: elegir capítulo ----------
bool storyMenuOpen = false;
static void DrawStoryMenu() {
    float s = S();
    int W = GetScreenWidth(), Hh = GetScreenHeight();
    Overlay();
    const char *title = "MODO HISTORIA";
    float ts = 104 * s;
    Vector2 tm = MeasureTextEx(fontTitle, title, ts, 4);
    DrawTextEx(fontTitle, title, Vector2{ W / 2 - tm.x / 2 + 4, Hh * 0.1f + 4 }, ts, 4, Color{ 0, 0, 0, 220 });
    DrawTextEx(fontTitle, title, Vector2{ W / 2 - tm.x / 2, Hh * 0.1f }, ts, 4, Color{ 204, 0, 0, 255 });
    float y = Hh * 0.1f + ts + 6 * s;
    Text(fontUI, "OPERACIÓN LÁZARO", W / 2.0f, y, 30 * s, Color{ 255, 216, 74, 255 }, 0.5f, 2);
    Text(fontUI, "El Búnker 7 dejó de responder. Frank y Milton van a descubrir qué fue el Proyecto Lázaro y quién está detrás.", W / 2.0f, y + 44 * s, 20 * s, Color{ 200, 200, 200, 255 }, 0.5f, 1);
    int done = StoryProgress(), n = StoryChapterCount();
    float bw = 320 * s, bh = 96 * s, gap = 24 * s, x0 = W / 2 - bw * 1.5f - gap, by = Hh * 0.36f;
    for (int i = 0; i < n; i++) {
        const char *place, *title = StoryChapterName(i, place);
        char name[64]; snprintf(name, 64, "%d · %s", i + 1, place);
        char sub[64]; snprintf(sub, 64, "%s", title);
        Rectangle r{ x0 + (i % 3) * (bw + gap), by + (i / 3) * (bh + 54 * s), bw, bh };
        if (i <= done) Button(r, name, sub, 31 + i);
        else {
            DrawRectangleRounded(r, 0.12f, 8, Color{ 40, 40, 40, 220 });
            DrawRectangleRoundedLinesEx(r, 0.12f, 8, 2, Color{ 90, 90, 90, 255 });
            Text(fontUI, name, r.x + r.width / 2, r.y + r.height * 0.18f, 30 * s, Color{ 130, 130, 130, 255 }, 0.5f, 2);
            Text(fontUI, "BLOQUEADO", r.x + r.width / 2, r.y + r.height * 0.62f, 16 * s, Color{ 150, 150, 150, 255 }, 0.5f, 1);
        }
        const char *st = i < done ? "COMPLETADO" : i == done ? "SIGUIENTE MISIÓN" : "Termina el capítulo anterior";
        Color sc = i < done ? Color{ 150, 230, 120, 255 } : i == done ? Color{ 255, 216, 74, 255 } : Color{ 130, 130, 130, 255 };
        Text(fontUI, st, r.x + r.width / 2, r.y + bh + 8 * s, 17 * s, sc, 0.5f, 1);
    }
    float ty = by + 2 * (bh + 54 * s) + 6 * s;
    Text(fontUI, "Milton pelea a tu lado: si cae, mantén E cerca de él para levantarlo. Si caes tú, reintentas el capítulo.", W / 2.0f, ty, 18 * s, Color{ 160, 160, 160, 255 }, 0.5f, 1);
    Button(Rectangle{ W / 2 - 130 * s, Hh * 0.84f, 260 * s, 52 * s }, "VOLVER", nullptr, 37);
}

void DrawMenu() {
    float s = S();
    int W = GetScreenWidth(), Hh = GetScreenHeight();
    if (optionsOpen) { DrawOptions(); return; }
    if (NetLobbyScreen() != LB_NONE) { DrawLobby(); return; }
    if (storyMenuOpen) { DrawStoryMenu(); return; }
    Overlay();
    const char *title = CONFIG::titulo;
    float ts = 130 * s;
    Vector2 tm = MeasureTextEx(fontTitle, title, ts, 4);
    DrawTextEx(fontTitle, title, Vector2{ W / 2 - tm.x / 2 + 5, Hh * 0.1f + 5 }, ts, 4, Color{ 0, 0, 0, 220 });
    DrawTextEx(fontTitle, title, Vector2{ W / 2 - tm.x / 2, Hh * 0.1f }, ts, 4, Color{ 204, 0, 0, 255 });
    // firma del autor bajo el título y en la esquina
    Text(fontUI, CONFIG::marca, W / 2.0f, Hh * 0.1f + ts + 4 * s, 28 * s, Color{ 230, 230, 230, 255 }, 0.5f, 2);
    Text(fontUI, CONFIG::marca, W - 16 * s, Hh - 34 * s, 18 * s, Color{ 200, 200, 200, 150 }, 1.0f, 1);
    // dos filas de mapas (con su récord debajo) y la fila de modos
    struct MapBtn { const char *name, *sub, *key; int id; };
    const MapBtn mapsRow[2][4] = {
        { { "BÚNKER", "Pasillos estrechos · 5 zonas", "bunker", 0 }, { "FÁBRICA", "Mapa grande · 5 zonas", "fabrica", 1 },
          { "PUEBLO", "Al aire libre · 7 zonas", "pueblo", 7 }, { "HOSPITAL", "San Rafael · 6 zonas", "hospital", 9 } },
        { { "FUERTE REYES", "Base militar · 7 zonas", "base", 10 }, { "LABORATORIO", "Bajo tierra · 6 zonas", "laboratorio", 11 },
          { "CAMPO DE TIRO", "Todas las armas · sin daño", nullptr, 4 }, { nullptr, nullptr, nullptr, -1 } } };
    float bw = 250 * s, bh = 76 * s, gap = 18 * s, by = Hh * 0.30f;
    for (int row = 0; row < 2; row++) {
        int n = mapsRow[row][3].name ? 4 : 3;
        float bx0 = W / 2 - (bw * n + gap * (n - 1)) / 2;
        float y = by + row * (bh + 34 * s);
        for (int i = 0; i < n; i++) {
            const MapBtn &m = mapsRow[row][i];
            float x = bx0 + (bw + gap) * i;
            Button(Rectangle{ x, y, bw, bh }, m.name, m.sub, m.id);
            if (!m.key) continue;
            Record r = GetRecord(m.key);
            char b[64];
            if (r.round > 0) snprintf(b, 64, "Récord: ronda %d · %d bajas", r.round, r.kills);
            else snprintf(b, 64, "Récord: aún no hay");
            Text(fontUI, b, x + bw / 2, y + bh + 6 * s, 16 * s, r.round > 0 ? Color{ 255, 216, 74, 255 } : Color{ 150, 150, 150, 255 }, 0.5f, 1);
        }
    }
    float my = by + 2 * (bh + 34 * s);
    Button(Rectangle{ W / 2 - 400 * s, my, 260 * s, 52 * s }, "MODO HISTORIA", nullptr, 30);
    Button(Rectangle{ W / 2 - 130 * s, my, 260 * s, 52 * s }, "MULTIJUGADOR", nullptr, 8);
    Button(Rectangle{ W / 2 + 140 * s, my, 260 * s, 52 * s }, "OPCIONES", nullptr, 5);
    const char *ctrl[][2] = { { "W A S D", "moverse" }, { "Ratón", "apuntar" }, { "Clic izq.", "disparar" }, { "Clic der.", "mirilla" },
                              { "Shift", "correr" }, { "Espacio", "saltar" }, { "C", "agacharse / deslizar" }, { "G", "granada" },
                              { "R", "recargar" }, { "E", "comprar / reparar / interruptor" }, { "1 / 2 / 3", "cambiar arma" }, { "M", "música" },
                              { "Esc", "pausa" }, { "F11", "pantalla completa" }, { "V", "cuchillo" }, { "F3", "gráficos rápidos" } };
    float cy = my + 70 * s;
    for (int i = 0; i < 16; i++) {
        float x = W / 2.0f + (i % 2 ? 30 * s : -330 * s), y = cy + (i / 2) * 23 * s;
        Text(fontUI, ctrl[i][0], x, y, 18 * s, WHITE, 0, 1);
        Text(fontUI, ctrl[i][1], x + 130 * s, y, 18 * s, Color{ 187, 187, 187, 255 }, 0, 1);
    }
    Text(fontUI, "Activa la corriente con el interruptor para usar las bebidas y el Pack-a-Punch. Cada pocas rondas llegan los perros del infierno.", W / 2.0f, cy + 8 * 23 * s + 10 * s, 16 * s, Color{ 160, 160, 160, 255 }, 0.5f, 1);
    Text(fontUI, "Mantén E cerca de una ventana para repararla. Dispara a la cabeza para hacer mucho más daño.", W / 2.0f, cy + 8 * 23 * s + 32 * s, 16 * s, Color{ 160, 160, 160, 255 }, 0.5f, 1);
}

void DrawPause() {
    float s = S();
    int W = GetScreenWidth(), Hh = GetScreenHeight();
    if (optionsOpen) { DrawOptions(); return; }
    Overlay();
    Text(fontUI, "PAUSA", W / 2.0f, Hh * 0.34f, 64 * s, WHITE, 0.5f, 3);
    Text(fontUI, "Haz clic para continuar", W / 2.0f, Hh * 0.34f + 80 * s, 22 * s, Color{ 220, 220, 220, 255 }, 0.5f);
    Button(Rectangle{ W / 2 - 140 * s, Hh * 0.55f, 280 * s, 60 * s }, "OPCIONES", nullptr, 5);
    Button(Rectangle{ W / 2 - 140 * s, Hh * 0.55f + 76 * s, 280 * s, 60 * s }, "MENÚ", nullptr, 3);
}
// Botones de la pausa (para no reanudar al pulsarlos)
bool PauseButtonHover() {
    float s = S();
    int W = GetScreenWidth(), Hh = GetScreenHeight();
    Vector2 m = GetMousePosition();
    return CheckCollisionPointRec(m, Rectangle{ W / 2 - 140 * s, Hh * 0.55f, 280 * s, 136 * s });
}

bool newRecord = false;
void DrawGameOver() {
    float s = S();
    int W = GetScreenWidth(), Hh = GetScreenHeight();
    Overlay();
    const char *t = "TE ATRAPARON";
    float ts = 120 * s;
    Vector2 tm = MeasureTextEx(fontTitle, t, ts, 4);
    DrawTextEx(fontTitle, t, Vector2{ W / 2 - tm.x / 2 + 4, Hh * 0.16f + 4 }, ts, 4, BLACK);
    DrawTextEx(fontTitle, t, Vector2{ W / 2 - tm.x / 2, Hh * 0.16f }, ts, 4, Color{ 204, 0, 0, 255 });
    if (StoryActive()) {
        Text(fontUI, TextFormat("CAPÍTULO %d · %s", StoryChapter() + 1, StoryChapterTitle()), W / 2.0f, Hh * 0.34f, 30 * s, Color{ 255, 216, 74, 255 }, 0.5f, 2);
        Text(fontUI, TextFormat("Zombis eliminados: %d · a la cabeza: %d", game.kills, game.heads), W / 2.0f, Hh * 0.34f + 50 * s, 24 * s, Color{ 230, 230, 230, 255 }, 0.5f);
        Text(fontUI, "La misión fracasó. La radio se quedó en silencio...", W / 2.0f, Hh * 0.34f + 90 * s, 22 * s, Color{ 180, 180, 180, 255 }, 0.5f, 1);
        Button(Rectangle{ W / 2 - 300 * s, Hh * 0.66f, 280 * s, 70 * s }, "REINTENTAR", nullptr, 2);
        Button(Rectangle{ W / 2 + 20 * s, Hh * 0.66f, 280 * s, 70 * s }, "MENÚ", nullptr, 3);
        return;
    }
    char b[5][64];
    Record r = GetRecord(game.mapKey);
    snprintf(b[0], 64, "Mapa: %s", MapName());
    snprintf(b[1], 64, "Llegaste a la ronda %d", game.round);
    snprintf(b[2], 64, "Zombis eliminados: %d", game.kills);
    snprintf(b[3], 64, "Disparos a la cabeza: %d", game.heads);
    snprintf(b[4], 64, "Tu récord: ronda %d · %d bajas", r.round, r.kills);
    for (int i = 0; i < 5; i++) Text(fontUI, b[i], W / 2.0f, Hh * 0.34f + i * 36 * s, 26 * s, i == 4 ? Color{ 255, 216, 74, 255 } : Color{ 230, 230, 230, 255 }, 0.5f);
    if (newRecord && ((int)(GetTime() * 3) % 2 == 0)) Text(fontUI, "¡NUEVO RÉCORD!", W / 2.0f, Hh * 0.34f + 5 * 36 * s + 4 * s, 34 * s, Color{ 255, 126, 182, 255 }, 0.5f, 3);
    if (NetRole() == NET_CLIENT) Text(fontUI, "FRANK decide si juegan de nuevo", W / 2.0f - 160 * s, Hh * 0.66f + 22 * s, 22 * s, Color{ 180, 180, 180, 255 }, 0.5f, 1);
    else Button(Rectangle{ W / 2 - 300 * s, Hh * 0.66f, 280 * s, 70 * s }, "JUGAR DE NUEVO", nullptr, 2);
    Button(Rectangle{ W / 2 + 20 * s, Hh * 0.66f, 280 * s, 70 * s }, "MENÚ", nullptr, 3);
}
