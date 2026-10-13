// Render: shaders PBR, sombras, HDR + bloom, posprocesado, materiales y mallas.
#include "game.h"
#include <map>

extern "C" __declspec(dllimport) void __stdcall glClear(unsigned int mask);
#define GL_DEPTH_BUFFER_BIT 0x00000100

Font fontUI, fontTitle;
Shader shLit, shLitSkin, shDepth, shDepthSkin, shUnlit;
static Shader shBright, shBlur, shComposite, shFxaa, shMask;
bool depthPass = false;
Texture2D texWhite, texFlat, texFlash, texBlood, texHole, texScorch, texSmoke, texCamo;
Camera3D cam;
float camFov = 75;
RenderTexture2D *captureTarget = nullptr;   // modo prueba: el cuadro final se copia aquí

static RenderTexture2D hdrRT, ldrRT, bloomA, bloomB, shadowRT[2];
static int rtW = 0, rtH = 0;
static Matrix lightVP[2];
static const int SHADOW_SIZE = 2048;
static std::map<std::string, PMat> mats;
static Material depthMat, depthMatSkin;

// ---------------------------------------------------------------
// Shaders
// ---------------------------------------------------------------
static const char *VS_LIT = R"(#version 330
in vec3 vertexPosition; in vec2 vertexTexCoord; in vec3 vertexNormal; in vec4 vertexTangent;
uniform mat4 mvp; uniform mat4 matModel; uniform mat4 matNormal;
out vec3 fragPos; out vec2 fragUV; out vec3 fragN; out vec4 fragT;
void main(){
  vec4 wp = matModel*vec4(vertexPosition,1.0);
  fragPos = wp.xyz; fragUV = vertexTexCoord;
  fragN = normalize(mat3(matNormal)*vertexNormal);
  fragT = vec4(mat3(matModel)*vertexTangent.xyz, vertexTangent.w);
  gl_Position = mvp*vec4(vertexPosition,1.0);
})";

static const char *VS_LIT_SKIN = R"(#version 330
in vec3 vertexPosition; in vec2 vertexTexCoord; in vec3 vertexNormal; in vec4 vertexTangent;
in vec4 vertexBoneIds; in vec4 vertexBoneWeights;
uniform mat4 mvp; uniform mat4 matModel; uniform mat4 matNormal;
uniform mat4 boneMatrices[64];
out vec3 fragPos; out vec2 fragUV; out vec3 fragN; out vec4 fragT;
void main(){
  mat4 sk = boneMatrices[int(vertexBoneIds.x)]*vertexBoneWeights.x + boneMatrices[int(vertexBoneIds.y)]*vertexBoneWeights.y
          + boneMatrices[int(vertexBoneIds.z)]*vertexBoneWeights.z + boneMatrices[int(vertexBoneIds.w)]*vertexBoneWeights.w;
  vec4 p = sk*vec4(vertexPosition,1.0);
  vec4 wp = matModel*p;
  fragPos = wp.xyz; fragUV = vertexTexCoord;
  fragN = normalize(mat3(matNormal)*(mat3(sk)*vertexNormal));
  fragT = vec4(mat3(matModel)*(mat3(sk)*vertexTangent.xyz), vertexTangent.w);
  gl_Position = mvp*p;
})";

static const char *FS_LIT = R"(#version 330
in vec3 fragPos; in vec2 fragUV; in vec3 fragN; in vec4 fragT;
uniform sampler2D texture0; uniform sampler2D texture2; uniform sampler2D roughMap;
uniform vec4 colDiffuse; uniform float metal; uniform float roughK; uniform float nstr; uniform vec3 emis; uniform vec2 uvOff;
uniform vec3 viewPos; uniform vec3 ambient; uniform vec3 fogColor; uniform float fogDensity; uniform float hitFlash;
#define MAXL 24
uniform int numLights; uniform vec3 lPos[MAXL]; uniform vec3 lCol[MAXL]; uniform float lRange[MAXL];
uniform int numShadows; uniform mat4 lightVP0; uniform mat4 lightVP1; uniform sampler2D shadow0; uniform sampler2D shadow1;
out vec4 finalColor;
const float PI = 3.14159265;
float shadowF(sampler2D sm, mat4 vp, vec3 p, vec3 n){
  vec4 c = vp*vec4(p + n*0.04, 1.0);
  if(c.w <= 0.0) return 1.0;
  vec3 pr = c.xyz/c.w*0.5 + 0.5;
  if(pr.x < 0.0 || pr.x > 1.0 || pr.y < 0.0 || pr.y > 1.0 || pr.z > 1.0) return 1.0;
  vec2 ts = 1.0/vec2(textureSize(sm, 0));
  float s = 0.0;
  for(int x=-1; x<=1; x++) for(int y=-1; y<=1; y++){
    float d = texture(sm, pr.xy + vec2(x,y)*ts*1.5).r;
    s += (pr.z - 0.0004 > d) ? 0.0 : 1.0;
  }
  return s/9.0;
}
void main(){
  vec2 uv = fragUV + uvOff;
  vec4 base = texture(texture0, uv)*colDiffuse;
  if(base.a < 0.35) discard;
  vec3 albedo = pow(base.rgb, vec3(2.2));
  vec3 Ng = normalize(fragN);
  vec3 N = Ng;
  vec3 Tn = fragT.xyz - Ng*dot(Ng, fragT.xyz);
  if(dot(Tn,Tn) > 1e-8){
    Tn = normalize(Tn);
    vec3 B = cross(Ng, Tn)*(fragT.w < 0.0 ? -1.0 : 1.0);
    vec3 nm = texture(texture2, uv).xyz*2.0 - 1.0;
    nm.xy *= nstr;
    N = normalize(mat3(Tn, B, Ng)*nm);
  }
  vec4 rm = texture(roughMap, uv);
  float rough = clamp(rm.r*roughK, 0.06, 1.0);
  float mtl = metal < 0.0 ? rm.g : metal;   // metal < 0: se lee del canal verde (armas glTF)
  float a = rough*rough, a2 = a*a;
  vec3 V = normalize(viewPos - fragPos);
  float NdV = max(dot(N, V), 0.001);
  vec3 F0 = mix(vec3(0.04), albedo, mtl);
  vec3 Lo = vec3(0.0);
  for(int i=0; i<MAXL; i++){
    if(i >= numLights) break;
    vec3 Lv = lPos[i] - fragPos; float d = length(Lv); vec3 L = Lv/d;
    float att = clamp(1.0 - pow(d/lRange[i], 4.0), 0.0, 1.0);
    att = att*att/(d*d*0.3 + 1.0);
    float NdL = max(dot(N, L), 0.0);
    if(att <= 0.0 || NdL <= 0.0) continue;
    float sh = 1.0;
    if(i == 0 && numShadows > 0) sh = shadowF(shadow0, lightVP0, fragPos, Ng);
    else if(i == 1 && numShadows > 1) sh = shadowF(shadow1, lightVP1, fragPos, Ng);
    vec3 Hh = normalize(L + V);
    float NdH = max(dot(N, Hh), 0.0);
    float dd = NdH*NdH*(a2 - 1.0) + 1.0;
    float D = a2/(PI*dd*dd);
    float k = (rough + 1.0)*(rough + 1.0)/8.0;
    float G = (NdV/(NdV*(1.0 - k) + k))*(NdL/(NdL*(1.0 - k) + k));
    vec3 F = F0 + (1.0 - F0)*pow(1.0 - max(dot(Hh, V), 0.0), 5.0);
    vec3 spec = D*G*F/(4.0*NdV*NdL + 0.001);
    vec3 kd = (1.0 - F)*(1.0 - mtl);
    Lo += (kd*albedo/PI + spec)*lCol[i]*att*NdL*sh;
  }
  vec3 amb = ambient*(0.6 + 0.4*N.y);
  vec3 col = Lo + amb*albedo + emis + vec3(hitFlash, hitFlash*0.1, 0.0);
  float dist = length(viewPos - fragPos);
  float fog = 1.0 - exp(-pow(dist*fogDensity, 2.0));
  col = mix(col, fogColor, clamp(fog, 0.0, 1.0));
  finalColor = vec4(col, base.a);
})";

static const char *VS_DEPTH = R"(#version 330
in vec3 vertexPosition; uniform mat4 mvp;
void main(){ gl_Position = mvp*vec4(vertexPosition,1.0); })";
static const char *VS_DEPTH_SKIN = R"(#version 330
in vec3 vertexPosition; in vec4 vertexBoneIds; in vec4 vertexBoneWeights;
uniform mat4 mvp; uniform mat4 boneMatrices[64];
void main(){
  mat4 sk = boneMatrices[int(vertexBoneIds.x)]*vertexBoneWeights.x + boneMatrices[int(vertexBoneIds.y)]*vertexBoneWeights.y
          + boneMatrices[int(vertexBoneIds.z)]*vertexBoneWeights.z + boneMatrices[int(vertexBoneIds.w)]*vertexBoneWeights.w;
  gl_Position = mvp*(sk*vec4(vertexPosition,1.0));
})";
static const char *FS_DEPTH = R"(#version 330
void main(){})";

static const char *VS_BASIC = R"(#version 330
in vec3 vertexPosition; in vec2 vertexTexCoord; in vec4 vertexColor;
uniform mat4 mvp; out vec2 fragTexCoord; out vec4 fragColor;
void main(){ fragTexCoord = vertexTexCoord; fragColor = vertexColor; gl_Position = mvp*vec4(vertexPosition,1.0); })";

static const char *FS_UNLIT = R"(#version 330
in vec2 fragTexCoord; in vec4 fragColor; uniform sampler2D texture0; uniform vec4 colDiffuse; uniform float intensity;
uniform vec3 fogColor; uniform float fogDensity; uniform vec3 viewPos;
out vec4 finalColor;
void main(){
  vec4 t = texture(texture0, fragTexCoord)*fragColor*colDiffuse;
  if(t.a < 0.01) discard;
  finalColor = vec4(pow(t.rgb, vec3(2.2))*intensity, t.a);
})";

static const char *FS_BRIGHT = R"(#version 330
in vec2 fragTexCoord; uniform sampler2D texture0; out vec4 finalColor;
void main(){
  vec3 c = texture(texture0, fragTexCoord).rgb;
  float l = max(c.r, max(c.g, c.b));
  finalColor = vec4(c*max(l - 0.9, 0.0)/max(l, 1e-4), 1.0);
})";

static const char *FS_BLUR = R"(#version 330
in vec2 fragTexCoord; uniform sampler2D texture0; uniform vec2 dir; out vec4 finalColor;
void main(){
  float w[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
  vec3 c = texture(texture0, fragTexCoord).rgb*w[0];
  for(int i=1; i<5; i++){
    c += texture(texture0, fragTexCoord + dir*float(i)).rgb*w[i];
    c += texture(texture0, fragTexCoord - dir*float(i)).rgb*w[i];
  }
  finalColor = vec4(c, 1.0);
})";

static const char *FS_COMPOSITE = R"(#version 330
in vec2 fragTexCoord; uniform sampler2D texture0; uniform sampler2D bloomTex;
uniform float exposure; uniform float time; uniform float damage; uniform float flash; uniform float bloomK; uniform vec2 res;
out vec4 finalColor;
vec3 aces(vec3 x){ const float a=2.51, b=0.03, c=2.43, d=0.59, e=0.14; return clamp((x*(a*x + b))/(x*(c*x + d) + e), 0.0, 1.0); }
void main(){
  vec2 uv = fragTexCoord;
  vec2 cc = uv - 0.5; float r2 = dot(cc, cc);
  vec3 col;
  col.r = texture(texture0, uv - cc*0.004).r;
  col.g = texture(texture0, uv).g;
  col.b = texture(texture0, uv + cc*0.004).b;
  col += texture(bloomTex, uv).rgb*bloomK;
  col *= exposure;
  col = aces(col);
  col = pow(col, vec3(1.0/2.2));
  // gradación: sombras frías, luces cálidas
  float lum = dot(col, vec3(0.299, 0.587, 0.114));
  col = mix(col, col*vec3(0.9, 1.0, 1.08), (1.0 - lum)*0.35);
  col = mix(vec3(lum), col, 0.88);
  float vig = smoothstep(0.75, 0.15, r2*1.5);
  col *= mix(0.45, 1.0, vig);
  col = mix(col, vec3(0.42, 0.0, 0.0), damage*smoothstep(0.05, 0.5, r2*2.2));
  col = mix(col, vec3(1.0), flash);
  float n = fract(sin(dot(uv*res + vec2(time*61.0, time*17.0), vec2(12.9898, 78.233)))*43758.5453);
  col += (n - 0.5)*0.03;
  finalColor = vec4(col, 1.0);
})";

static const char *FS_FXAA = R"(#version 330
in vec2 fragTexCoord; uniform sampler2D texture0; uniform vec2 res; out vec4 finalColor;
float lu(vec3 c){ return dot(c, vec3(0.299, 0.587, 0.114)); }
void main(){
  vec2 px = 1.0/res; vec2 uv = fragTexCoord;
  vec3 rgbNW = texture(texture0, uv + vec2(-1.0,-1.0)*px).rgb;
  vec3 rgbNE = texture(texture0, uv + vec2( 1.0,-1.0)*px).rgb;
  vec3 rgbSW = texture(texture0, uv + vec2(-1.0, 1.0)*px).rgb;
  vec3 rgbSE = texture(texture0, uv + vec2( 1.0, 1.0)*px).rgb;
  vec3 rgbM  = texture(texture0, uv).rgb;
  float lNW = lu(rgbNW), lNE = lu(rgbNE), lSW = lu(rgbSW), lSE = lu(rgbSE), lM = lu(rgbM);
  float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
  float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));
  vec2 dir = vec2(-((lNW + lNE) - (lSW + lSE)), ((lNW + lSW) - (lNE + lSE)));
  float red = max((lNW + lNE + lSW + lSE)*0.03125, 0.0078125);
  float rcp = 1.0/(min(abs(dir.x), abs(dir.y)) + red);
  dir = clamp(dir*rcp, vec2(-8.0), vec2(8.0))*px;
  vec3 A = 0.5*(texture(texture0, uv + dir*(1.0/3.0 - 0.5)).rgb + texture(texture0, uv + dir*(2.0/3.0 - 0.5)).rgb);
  vec3 B = A*0.5 + 0.25*(texture(texture0, uv + dir*-0.5).rgb + texture(texture0, uv + dir*0.5).rgb);
  float lB = lu(B);
  finalColor = vec4((lB < lMin || lB > lMax) ? A : B, 1.0);
})";

// recorta un dibujo en forma de círculo (minimapa)
static const char *FS_MASK = R"(#version 330
in vec2 fragTexCoord; in vec4 fragColor; uniform sampler2D texture0; out vec4 finalColor;
void main(){
  vec2 c = fragTexCoord - 0.5; float r = length(c);
  if(r > 0.5) discard;
  vec4 t = texture(texture0, fragTexCoord);
  finalColor = vec4(t.rgb, t.a*fragColor.a*smoothstep(0.5, 0.48, r));
})";

struct LitLocs {
    int metal, roughK, nstr, emis, uvOff, viewPos, ambient, fogColor, fogDensity, hitFlash, numLights, lPos, lCol, lRange, numShadows, lightVP0, lightVP1, shadow0, shadow1, colDiffuse;
};
static LitLocs locLit, locSkin;
static int locUnlitI, locBlurDir, locCompExp, locCompTime, locCompDmg, locCompFlash, locCompBloom, locCompRes, locCompBloomTex, locFxaaRes;

static void GetLitLocs(Shader &s, LitLocs &l) {
    l.metal = GetShaderLocation(s, "metal"); l.roughK = GetShaderLocation(s, "roughK"); l.nstr = GetShaderLocation(s, "nstr");
    l.emis = GetShaderLocation(s, "emis"); l.uvOff = GetShaderLocation(s, "uvOff"); l.viewPos = GetShaderLocation(s, "viewPos");
    l.ambient = GetShaderLocation(s, "ambient"); l.fogColor = GetShaderLocation(s, "fogColor"); l.fogDensity = GetShaderLocation(s, "fogDensity");
    l.hitFlash = GetShaderLocation(s, "hitFlash"); l.numLights = GetShaderLocation(s, "numLights"); l.lPos = GetShaderLocation(s, "lPos");
    l.lCol = GetShaderLocation(s, "lCol"); l.lRange = GetShaderLocation(s, "lRange"); l.numShadows = GetShaderLocation(s, "numShadows");
    l.lightVP0 = GetShaderLocation(s, "lightVP0"); l.lightVP1 = GetShaderLocation(s, "lightVP1");
    l.shadow0 = GetShaderLocation(s, "shadow0"); l.shadow1 = GetShaderLocation(s, "shadow1"); l.colDiffuse = GetShaderLocation(s, "colDiffuse");
    s.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(s, "roughMap");
}

// ---------------------------------------------------------------
// Render targets
// ---------------------------------------------------------------
static RenderTexture2D LoadRT(int w, int h, int format, bool depth) {
    RenderTexture2D t = {};
    t.id = rlLoadFramebuffer();
    rlEnableFramebuffer(t.id);
    t.texture.id = rlLoadTexture(NULL, w, h, format, 1);
    t.texture.width = w; t.texture.height = h; t.texture.format = format; t.texture.mipmaps = 1;
    rlFramebufferAttach(t.id, t.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    if (depth) {
        t.depth.id = rlLoadTextureDepth(w, h, true);
        t.depth.width = w; t.depth.height = h;
        rlFramebufferAttach(t.id, t.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
    }
    rlFramebufferComplete(t.id);
    rlDisableFramebuffer();
    SetTextureFilter(t.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(t.texture, TEXTURE_WRAP_CLAMP);
    return t;
}
static RenderTexture2D LoadShadowRT(int size) {
    RenderTexture2D t = {};
    t.id = rlLoadFramebuffer();
    rlEnableFramebuffer(t.id);
    t.depth.id = rlLoadTextureDepth(size, size, false);
    t.depth.width = size; t.depth.height = size; t.depth.format = 19; t.depth.mipmaps = 1;
    t.texture.width = size; t.texture.height = size;
    rlFramebufferAttach(t.id, t.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferComplete(t.id);
    rlDisableFramebuffer();
    return t;
}
static void UnloadRT(RenderTexture2D &t) {
    if (!t.id) return;
    rlUnloadFramebuffer(t.id);
    rlUnloadTexture(t.texture.id);
    if (t.depth.id) rlUnloadTexture(t.depth.id);
    t = {};
}

void ResizeRender() {
    int w = GetRenderWidth(), h = GetRenderHeight();
    if (w <= 0 || h <= 0 || (w == rtW && h == rtH)) return;
    UnloadRT(hdrRT); UnloadRT(ldrRT); UnloadRT(bloomA); UnloadRT(bloomB);
    rtW = w; rtH = h;
    hdrRT = LoadRT(w, h, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, true);
    ldrRT = LoadRT(w, h, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, false);
    bloomA = LoadRT(w / 4, h / 4, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, false);
    bloomB = LoadRT(w / 4, h / 4, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, false);
}

// ---------------------------------------------------------------
// Texturas generadas
// ---------------------------------------------------------------
static Texture2D TexFromImage(Image img, bool mip = true) {
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    if (mip) GenTextureMipmaps(&t);
    SetTextureFilter(t, mip ? TEXTURE_FILTER_TRILINEAR : TEXTURE_FILTER_BILINEAR);
    return t;
}
static Image RadialImage(int s, Color inner, Color mid, Color outer, float midPos) {
    Image img = GenImageColor(s, s, BLANK);
    Color *px = (Color *)img.data;
    for (int y = 0; y < s; y++) for (int x = 0; x < s; x++) {
        float d = hyp(x - s / 2.f + 0.5f, y - s / 2.f + 0.5f) / (s / 2.f);
        Color c;
        if (d < midPos) c = ColorLerp(inner, mid, d / midPos);
        else c = ColorLerp(mid, outer, clampf((d - midPos) / (1 - midPos), 0, 1));
        px[y * s + x] = c;
    }
    return img;
}
static void GenTextures() {
    texWhite = TexFromImage(GenImageColor(4, 4, WHITE), false);
    texFlat = TexFromImage(GenImageColor(4, 4, Color{ 128, 128, 255, 255 }), false);
    texFlash = TexFromImage(RadialImage(64, Color{ 255, 255, 255, 255 }, Color{ 255, 200, 80, 230 }, Color{ 255, 120, 0, 0 }, 0.3f));
    texSmoke = TexFromImage(RadialImage(64, Color{ 200, 200, 200, 160 }, Color{ 150, 150, 150, 90 }, Color{ 120, 120, 120, 0 }, 0.5f));
    texHole = TexFromImage(RadialImage(32, Color{ 0, 0, 0, 255 }, Color{ 10, 8, 6, 230 }, Color{ 0, 0, 0, 0 }, 0.4f));
    texScorch = TexFromImage(RadialImage(128, Color{ 0, 0, 0, 230 }, Color{ 15, 10, 5, 150 }, Color{ 0, 0, 0, 0 }, 0.5f));
    // sangre: manchas irregulares
    Image b = GenImageColor(128, 128, BLANK);
    ImageDrawCircle(&b, 64, 64, 30, Color{ 90, 0, 0, 220 });
    for (int i = 0; i < 16; i++) ImageDrawCircle(&b, 64 + irand(100) - 50, 64 + irand(100) - 50, 3 + irand(10), Color{ 80, 0, 0, 210 });
    texBlood = TexFromImage(b);
    // camuflaje del Pack-a-Punch
    Image c = GenImageColor(128, 128, Color{ 42, 10, 74, 255 });
    Color cs[4] = { Hex(0x6a1bb0), Hex(0x2a60ff), Hex(0xb04aff), Hex(0x14143a) };
    for (int i = 0; i < 40; i++) ImageDrawCircle(&c, irand(128), irand(128), 5 + irand(14), cs[irand(4)]);
    texCamo = TexFromImage(c);
    SetTextureWrap(texCamo, TEXTURE_WRAP_REPEAT);
}

// ---------------------------------------------------------------
// Materiales
// ---------------------------------------------------------------
static Texture2D LoadTex(const char *path) {
    Texture2D t = LoadTexture(path);
    if (t.id == 0) return texWhite;
    GenTextureMipmaps(&t);
    SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);       // mipmaps suaves (sin centelleo)
    SetTextureFilter(t, TEXTURE_FILTER_ANISOTROPIC_8X);  // y nítidas vistas de lado
    SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
    return t;
}
static PMat &NewMat(const char *name, Texture2D col, Texture2D nrm, Texture2D rgh, Color tint, float metal, float rough, float nstr = 1) {
    PMat p;
    p.m = LoadMaterialDefault();
    p.m.shader = shLit;
    p.m.maps[MATERIAL_MAP_ALBEDO].texture = col;
    p.m.maps[MATERIAL_MAP_ALBEDO].color = tint;
    p.m.maps[MATERIAL_MAP_NORMAL].texture = nrm;
    p.m.maps[MATERIAL_MAP_ROUGHNESS].texture = rgh;
    p.metal = metal; p.rough = rough; p.nstr = nstr;
    mats[name] = p;
    return mats[name];
}
static void TexMat(const char *name, const char *file, Color tint, float metal, float rough, float nstr = 1) {
    std::string b = std::string("assets/tex/") + file;
    NewMat(name, LoadTex((b + "_col.jpg").c_str()), LoadTex((b + "_nrm.jpg").c_str()), LoadTex((b + "_rgh.jpg").c_str()), tint, metal, rough, nstr);
}
// Material de un arma real (texturas en assets/guns): metal y rugosidad vienen del mapa _mr
PMat &GunMat(const char *name, bool papd) {
    std::string key = std::string("gun_") + name + (papd ? "_pap" : "");
    auto it = mats.find(key);
    if (it != mats.end()) return it->second;
    std::string b = std::string("assets/guns/") + name;
    if (papd) {
        PMat &base = GunMat(name, false);
        // Pack-a-Punch: tinte morado con brillo propio
        PMat &p = NewMat(key.c_str(), base.m.maps[MATERIAL_MAP_ALBEDO].texture, base.m.maps[MATERIAL_MAP_NORMAL].texture, base.m.maps[MATERIAL_MAP_ROUGHNESS].texture,
                         Color{ 175, 140, 225, 255 }, -1, 0.8f);
        p.emis = Vector3{ 0.025f, 0.0f, 0.06f };
        return p;
    }
    return NewMat(key.c_str(), LoadTex((b + "_col.png").c_str()), LoadTex((b + "_nrm.png").c_str()), LoadTex((b + "_mr.png").c_str()), WHITE, -1, 1.0f);
}
PMat &M(const char *name) {
    auto it = mats.find(name);
    if (it == mats.end()) { TraceLog(LOG_WARNING, "Material no encontrado: %s", name); return mats.begin()->second; }
    return it->second;
}

void LoadMaterials() {
    TexMat("bricks", "bricks", WHITE, 0, 1.0f, 1.2f);
    TexMat("tiles", "tiles", Color{ 200, 200, 195, 255 }, 0, 0.75f);
    TexMat("concwall", "concwall", Color{ 170, 170, 165, 255 }, 0, 1.0f);
    TexMat("concfloor", "concfloor", WHITE, 0.05f, 0.85f);
    TexMat("ceil", "ceil", Color{ 120, 120, 120, 255 }, 0, 1.0f);
    TexMat("metalceil", "metalceil", Color{ 140, 140, 145, 255 }, 0.25f, 1.0f, 0.5f);
    TexMat("planks", "planks", WHITE, 0, 1.0f);
    TexMat("crate", "crate", Color{ 210, 180, 140, 255 }, 0, 1.0f);
    TexMat("rust", "rust", WHITE, 0.55f, 1.0f);
    TexMat("metal", "metal", Color{ 170, 170, 175, 255 }, 0.85f, 0.8f);
    TexMat("barrel", "barrel", WHITE, 0.35f, 1.0f);
    TexMat("ground", "ground", Color{ 110, 105, 95, 255 }, 0, 1.0f);
    TexMat("door", "planks", Color{ 190, 160, 130, 255 }, 0, 1.0f);
    TexMat("gunmetal", "metal", Color{ 70, 72, 78, 255 }, 0.9f, 0.6f, 0.6f);
    PMat &metal = M("metal");
    NewMat("polymer", texWhite, metal.m.maps[MATERIAL_MAP_NORMAL].texture, metal.m.maps[MATERIAL_MAP_ROUGHNESS].texture, Color{ 34, 34, 36, 255 }, 0.0f, 0.9f, 0.3f);
    PMat &crate = M("crate");
    NewMat("wood", crate.m.maps[MATERIAL_MAP_ALBEDO].texture, crate.m.maps[MATERIAL_MAP_NORMAL].texture, crate.m.maps[MATERIAL_MAP_ROUGHNESS].texture, Color{ 150, 95, 55, 255 }, 0, 0.7f, 0.5f);
    PMat &camo = NewMat("camo", texCamo, metal.m.maps[MATERIAL_MAP_NORMAL].texture, metal.m.maps[MATERIAL_MAP_ROUGHNESS].texture, WHITE, 0.5f, 0.6f, 0.4f);
    camo.emis = Vector3{ 0.12f, 0.0f, 0.25f }; camo.uvScroll = 0.15f;
    NewMat("glove", texWhite, metal.m.maps[MATERIAL_MAP_NORMAL].texture, metal.m.maps[MATERIAL_MAP_ROUGHNESS].texture, Color{ 45, 35, 28, 255 }, 0, 1.0f, 0.3f);
    PMat &planks = M("planks");
    NewMat("sleeve", texWhite, planks.m.maps[MATERIAL_MAP_NORMAL].texture, planks.m.maps[MATERIAL_MAP_ROUGHNESS].texture, Color{ 70, 80, 52, 255 }, 0, 1.0f, 0.15f);
    PMat &barrel = M("barrel");
    for (int i = 0; i < P_COUNT; i++) {
        char n[16]; snprintf(n, 16, "perk%d", i);
        PMat &p = NewMat(n, texWhite, barrel.m.maps[MATERIAL_MAP_NORMAL].texture, barrel.m.maps[MATERIAL_MAP_ROUGHNESS].texture, Hex(PERKS[i].color), 0.3f, 0.8f);
        p.emis = Vector3Scale(ColorV(PERKS[i].color), 0.08f);
    }
    PMat &fx = NewMat("fixture", texWhite, texFlat, texWhite, WHITE, 0, 1);
    fx.emis = Vector3{ 6, 5.5f, 4.5f };
    PMat &dark = NewMat("darkmetal", texWhite, metal.m.maps[MATERIAL_MAP_NORMAL].texture, metal.m.maps[MATERIAL_MAP_ROUGHNESS].texture, Color{ 25, 25, 28, 255 }, 0.8f, 0.7f, 0.5f);
    (void)dark;
    PMat &glow = NewMat("papglow", texWhite, texFlat, texWhite, Color{ 42, 26, 58, 255 }, 0, 0.5f);
    glow.emis = Vector3{ 0.6f, 0.15f, 1.4f };
    NewMat("fur", texWhite, planks.m.maps[MATERIAL_MAP_NORMAL].texture, texWhite, Color{ 138, 90, 43, 255 }, 0, 1.0f, 0.4f);
    PMat &red = NewMat("raygunred", texWhite, texFlat, texWhite, Color{ 210, 30, 30, 255 }, 0.3f, 0.5f);
    red.emis = Vector3{ 0.3f, 0, 0 };
    PMat &blue = NewMat("wunderblue", texWhite, texFlat, texWhite, Color{ 130, 200, 255, 255 }, 0.3f, 0.4f);
    blue.emis = Vector3{ 0.2f, 0.5f, 1.0f };
    NewMat("olive", texWhite, metal.m.maps[MATERIAL_MAP_NORMAL].texture, metal.m.maps[MATERIAL_MAP_ROUGHNESS].texture, Color{ 80, 95, 60, 255 }, 0.2f, 0.8f, 0.5f);
    NewMat("brass", texWhite, metal.m.maps[MATERIAL_MAP_NORMAL].texture, metal.m.maps[MATERIAL_MAP_ROUGHNESS].texture, Color{ 160, 130, 60, 255 }, 0.9f, 0.5f, 0.3f);
}

void SetSkinned(bool on) { (void)on; }

void DrawPM(const Mesh &mesh, PMat &pm, Matrix tf) {
    if (depthPass) { DrawMesh(mesh, depthMat, tf); return; }
    float uvOff[2] = { pm.uvScroll * T, 0 };
    SetShaderValue(shLit, locLit.metal, &pm.metal, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shLit, locLit.roughK, &pm.rough, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shLit, locLit.nstr, &pm.nstr, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shLit, locLit.emis, &pm.emis, SHADER_UNIFORM_VEC3);
    SetShaderValue(shLit, locLit.uvOff, uvOff, SHADER_UNIFORM_VEC2);
    DrawMesh(mesh, pm.m, tf);
}

// Material de zombi: los parámetros se ponen en el shader con piel
void SetSkinParams(float metal, float rough, float nstr, Vector3 emis, float hitFlash) {
    SetShaderValue(shLitSkin, locSkin.metal, &metal, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shLitSkin, locSkin.roughK, &rough, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shLitSkin, locSkin.nstr, &nstr, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shLitSkin, locSkin.emis, &emis, SHADER_UNIFORM_VEC3);
    SetShaderValue(shLitSkin, locSkin.hitFlash, &hitFlash, SHADER_UNIFORM_FLOAT);
    float uv[2] = { 0, 0 };
    SetShaderValue(shLitSkin, locSkin.uvOff, uv, SHADER_UNIFORM_VEC2);
}
Material &DepthMatSkin() { return depthMatSkin; }

// ---------------------------------------------------------------
// MeshBuilder
// ---------------------------------------------------------------
void MeshBuilder::Quad(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, Vector3 nrm, Vector2 uv0, Vector2 uv1, Vector2 uv2, Vector2 uv3, Vector3 tan, float w) {
    unsigned short b = (unsigned short)Verts();
    Vector3 P[4] = { p0, p1, p2, p3 }; Vector2 U[4] = { uv0, uv1, uv2, uv3 };
    for (int i = 0; i < 4; i++) {
        v.insert(v.end(), { P[i].x, P[i].y, P[i].z });
        n.insert(n.end(), { nrm.x, nrm.y, nrm.z });
        t.insert(t.end(), { U[i].x, U[i].y });
        tg.insert(tg.end(), { tan.x, tan.y, tan.z, w });
    }
    Vector3 c = Vector3CrossProduct(Vector3Subtract(p1, p0), Vector3Subtract(p2, p0));
    if (Vector3DotProduct(c, nrm) >= 0) idx.insert(idx.end(), { b, (unsigned short)(b + 1), (unsigned short)(b + 2), b, (unsigned short)(b + 2), (unsigned short)(b + 3) });
    else idx.insert(idx.end(), { b, (unsigned short)(b + 2), (unsigned short)(b + 1), b, (unsigned short)(b + 3), (unsigned short)(b + 2) });
}

void MeshBuilder::Box(Vector3 mn, Vector3 mx, float uvScale, int uvMode, unsigned faces) {
    Vector3 c = Vector3Scale(Vector3Add(mn, mx), 0.5f);
    Vector3 h = Vector3Scale(Vector3Subtract(mx, mn), 0.5f);
    struct F { Vector3 N, R, U; };
    const F fs[6] = {
        { { 1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 } }, { { -1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } },
        { { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, -1 } }, { { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } },
        { { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 } }, { { 0, 0, -1 }, { -1, 0, 0 }, { 0, 1, 0 } },
    };
    auto comp = [](Vector3 a, Vector3 b) { return fabsf(a.x * b.x) + fabsf(a.y * b.y) + fabsf(a.z * b.z); };
    for (int f = 0; f < 6; f++) {
        if (!(faces & (1u << f))) continue;
        const F &F_ = fs[f];
        Vector3 fc = Vector3Add(c, Vector3Scale(F_.N, comp(F_.N, h)));
        Vector3 R = Vector3Scale(F_.R, comp(F_.R, h)), U = Vector3Scale(F_.U, comp(F_.U, h));
        Vector3 P[4] = { Vector3Subtract(Vector3Subtract(fc, R), U), Vector3Subtract(Vector3Add(fc, R), U), Vector3Add(Vector3Add(fc, R), U), Vector3Add(Vector3Subtract(fc, R), U) };
        Vector2 uv[4];
        for (int i = 0; i < 4; i++) {
            if (uvMode == 0) uv[i] = Vector2{ Vector3DotProduct(P[i], F_.R) / uvScale, -Vector3DotProduct(P[i], F_.U) / uvScale };
            else uv[i] = Vector2{ (i == 1 || i == 2) ? 1.f : 0.f, (i >= 2) ? 0.f : 1.f };
        }
        Quad(P[0], P[1], P[2], P[3], F_.N, uv[0], uv[1], uv[2], uv[3], F_.R, 1);
    }
}

void MeshBuilder::Cylinder(float r, float h, int seg, float uvScale) {
    (void)uvScale;
    for (int i = 0; i < seg; i++) {
        float a0 = -2 * PI_F * i / seg, a1 = -2 * PI_F * (i + 1) / seg;
        Vector3 n0{ cosf(a0), 0, sinf(a0) }, n1{ cosf(a1), 0, sinf(a1) };
        Vector3 b0{ n0.x * r, -h / 2, n0.z * r }, b1{ n1.x * r, -h / 2, n1.z * r };
        Vector3 t0{ b0.x, h / 2, b0.z }, t1{ b1.x, h / 2, b1.z };
        Vector3 nm = Vector3Normalize(Vector3Add(n0, n1));
        Vector3 tan{ sinf((a0 + a1) / 2), 0, -cosf((a0 + a1) / 2) };
        // lados con normales suaves por vértice
        unsigned short b = (unsigned short)Verts();
        Vector3 P[4] = { b0, b1, t1, t0 }; Vector3 NN[4] = { n0, n1, n1, n0 };
        float u0 = (float)i / seg, u1 = (float)(i + 1) / seg;
        Vector2 U[4] = { { u0, 1 }, { u1, 1 }, { u1, 0 }, { u0, 0 } };
        for (int k = 0; k < 4; k++) {
            v.insert(v.end(), { P[k].x, P[k].y, P[k].z });
            n.insert(n.end(), { NN[k].x, NN[k].y, NN[k].z });
            t.insert(t.end(), { U[k].x, U[k].y });
            Vector3 tk{ NN[k].z, 0, -NN[k].x };
            tg.insert(tg.end(), { tk.x, tk.y, tk.z, 1 });
        }
        Vector3 cr = Vector3CrossProduct(Vector3Subtract(b1, b0), Vector3Subtract(t1, b0));
        if (Vector3DotProduct(cr, nm) >= 0) idx.insert(idx.end(), { b, (unsigned short)(b + 1), (unsigned short)(b + 2), b, (unsigned short)(b + 2), (unsigned short)(b + 3) });
        else idx.insert(idx.end(), { b, (unsigned short)(b + 2), (unsigned short)(b + 1), b, (unsigned short)(b + 3), (unsigned short)(b + 2) });
        (void)tan;
        // tapas
        for (int cap = 0; cap < 2; cap++) {
            float y = cap ? h / 2 : -h / 2;
            Vector3 N{ 0, cap ? 1.f : -1.f, 0 };
            Vector3 c0{ 0, y, 0 }, p0{ n0.x * r, y, n0.z * r }, p1{ n1.x * r, y, n1.z * r };
            unsigned short bb = (unsigned short)Verts();
            Vector3 Q[3] = { c0, p0, p1 };
            for (int k = 0; k < 3; k++) {
                v.insert(v.end(), { Q[k].x, Q[k].y, Q[k].z });
                n.insert(n.end(), { N.x, N.y, N.z });
                t.insert(t.end(), { Q[k].x / (2 * r) + 0.5f, Q[k].z / (2 * r) + 0.5f });
                tg.insert(tg.end(), { 1, 0, 0, 1 });
            }
            Vector3 cc = Vector3CrossProduct(Vector3Subtract(p0, c0), Vector3Subtract(p1, c0));
            if (Vector3DotProduct(cc, N) >= 0) idx.insert(idx.end(), { bb, (unsigned short)(bb + 1), (unsigned short)(bb + 2) });
            else idx.insert(idx.end(), { bb, (unsigned short)(bb + 2), (unsigned short)(bb + 1) });
        }
    }
}

Mesh MeshBuilder::Build() {
    Mesh m = {};
    m.vertexCount = Verts();
    m.triangleCount = (int)idx.size() / 3;
    m.vertices = (float *)MemAlloc(v.size() * sizeof(float)); memcpy(m.vertices, v.data(), v.size() * sizeof(float));
    m.normals = (float *)MemAlloc(n.size() * sizeof(float)); memcpy(m.normals, n.data(), n.size() * sizeof(float));
    m.texcoords = (float *)MemAlloc(t.size() * sizeof(float)); memcpy(m.texcoords, t.data(), t.size() * sizeof(float));
    m.tangents = (float *)MemAlloc(tg.size() * sizeof(float)); memcpy(m.tangents, tg.data(), tg.size() * sizeof(float));
    m.indices = (unsigned short *)MemAlloc(idx.size() * sizeof(unsigned short)); memcpy(m.indices, idx.data(), idx.size() * sizeof(unsigned short));
    UploadMesh(&m, false);
    return m;
}

// Tangentes para mallas con índices (la función de raylib solo sirve sin índices).
// dxNormals: el mapa de relieve usa la convención de DirectX (verde hacia abajo).
void ComputeTangents(Mesh &m, bool dxNormals) {
    int n = m.vertexCount;
    if (!m.texcoords || !m.normals) return;
    std::vector<Vector3> tan(n, Vector3{ 0, 0, 0 }), bit(n, Vector3{ 0, 0, 0 });
    for (int t = 0; t < m.triangleCount; t++) {
        int i[3];
        for (int k = 0; k < 3; k++) i[k] = m.indices ? m.indices[t * 3 + k] : t * 3 + k;
        Vector3 p[3]; Vector2 uv[3];
        for (int k = 0; k < 3; k++) {
            p[k] = Vector3{ m.vertices[i[k] * 3], m.vertices[i[k] * 3 + 1], m.vertices[i[k] * 3 + 2] };
            uv[k] = Vector2{ m.texcoords[i[k] * 2], m.texcoords[i[k] * 2 + 1] };
        }
        Vector3 e1 = Vector3Subtract(p[1], p[0]), e2 = Vector3Subtract(p[2], p[0]);
        float du1 = uv[1].x - uv[0].x, dv1 = uv[1].y - uv[0].y, du2 = uv[2].x - uv[0].x, dv2 = uv[2].y - uv[0].y;
        float det = du1 * dv2 - du2 * dv1;
        if (fabsf(det) < 1e-12f) continue;
        float r = 1.0f / det;
        Vector3 sd = Vector3Scale(Vector3Subtract(Vector3Scale(e1, dv2), Vector3Scale(e2, dv1)), r);
        Vector3 td = Vector3Scale(Vector3Subtract(Vector3Scale(e2, du1), Vector3Scale(e1, du2)), r);
        for (int k = 0; k < 3; k++) { tan[i[k]] = Vector3Add(tan[i[k]], sd); bit[i[k]] = Vector3Add(bit[i[k]], td); }
    }
    if (!m.tangents) m.tangents = (float *)MemAlloc(n * 4 * sizeof(float));
    for (int v = 0; v < n; v++) {
        Vector3 N{ m.normals[v * 3], m.normals[v * 3 + 1], m.normals[v * 3 + 2] };
        Vector3 Tt = Vector3Subtract(tan[v], Vector3Scale(N, Vector3DotProduct(N, tan[v])));
        if (Vector3Length(Tt) < 1e-8f) Tt = fabsf(N.x) < 0.9f ? Vector3{ 1, 0, 0 } : Vector3{ 0, 1, 0 };
        Tt = Vector3Normalize(Tt);
        float w = Vector3DotProduct(Vector3CrossProduct(N, Tt), bit[v]) < 0 ? -1.0f : 1.0f;
        if (!dxNormals) w = -w;
        m.tangents[v * 4] = Tt.x; m.tangents[v * 4 + 1] = Tt.y; m.tangents[v * 4 + 2] = Tt.z; m.tangents[v * 4 + 3] = w;
    }
    const int TL = RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT;
    if (m.vboId && m.vboId[TL] == 0) {
        rlEnableVertexArray(m.vaoId);
        m.vboId[TL] = rlLoadVertexBuffer(m.tangents, n * 4 * sizeof(float), false);
        rlSetVertexAttribute(TL, 4, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(TL);
        rlDisableVertexArray();
    } else if (m.vboId) UpdateMeshBuffer(m, TL, m.tangents, n * 4 * sizeof(float), 0);
}

// ---------------------------------------------------------------
// Etiquetas de texto en el mundo
// ---------------------------------------------------------------
Texture2D MakeLabel(const char *text, Color color, int fontSize) {
    Vector2 sz = MeasureTextEx(fontUI, text, (float)fontSize, 1);
    int pad = 10;
    Image img = GenImageColor((int)sz.x + pad * 2, (int)sz.y + pad * 2, BLANK);
    for (int dx = -3; dx <= 3; dx += 3) for (int dy = -3; dy <= 3; dy += 3)
        if (dx || dy) ImageDrawTextEx(&img, fontUI, text, Vector2{ (float)pad + dx, (float)pad + dy }, (float)fontSize, 1, Color{ 0, 0, 0, 200 });
    ImageDrawTextEx(&img, fontUI, text, Vector2{ (float)pad, (float)pad }, (float)fontSize, 1, color);
    return TexFromImage(img);
}

void DrawWorldLabel(Texture2D tex, Vector3 pos, float width) {
    float h = width * tex.height / (float)tex.width;
    DrawBillboard(cam, tex, pos, h, WHITE);
}

// ---------------------------------------------------------------
// Inicialización
// ---------------------------------------------------------------
void InitRender() {
    shLit = LoadShaderFromMemory(VS_LIT, FS_LIT);
    shLitSkin = LoadShaderFromMemory(VS_LIT_SKIN, FS_LIT);
    shDepth = LoadShaderFromMemory(VS_DEPTH, FS_DEPTH);
    shDepthSkin = LoadShaderFromMemory(VS_DEPTH_SKIN, FS_DEPTH);
    shUnlit = LoadShaderFromMemory(VS_BASIC, FS_UNLIT);
    shBright = LoadShaderFromMemory(VS_BASIC, FS_BRIGHT);
    shBlur = LoadShaderFromMemory(VS_BASIC, FS_BLUR);
    shComposite = LoadShaderFromMemory(VS_BASIC, FS_COMPOSITE);
    shFxaa = LoadShaderFromMemory(VS_BASIC, FS_FXAA);
    shMask = LoadShaderFromMemory(VS_BASIC, FS_MASK);
    GetLitLocs(shLit, locLit);
    GetLitLocs(shLitSkin, locSkin);
    locUnlitI = GetShaderLocation(shUnlit, "intensity");
    locBlurDir = GetShaderLocation(shBlur, "dir");
    locCompExp = GetShaderLocation(shComposite, "exposure");
    locCompTime = GetShaderLocation(shComposite, "time");
    locCompDmg = GetShaderLocation(shComposite, "damage");
    locCompFlash = GetShaderLocation(shComposite, "flash");
    locCompBloom = GetShaderLocation(shComposite, "bloomK");
    locCompRes = GetShaderLocation(shComposite, "res");
    locCompBloomTex = GetShaderLocation(shComposite, "bloomTex");
    locFxaaRes = GetShaderLocation(shFxaa, "res");
    depthMat = LoadMaterialDefault(); depthMat.shader = shDepth;
    depthMatSkin = LoadMaterialDefault(); depthMatSkin.shader = shDepthSkin;
    for (int i = 0; i < 2; i++) shadowRT[i] = LoadShadowRT(SHADOW_SIZE);
    GenTextures();
    ResizeRender();
}

// ---------------------------------------------------------------
// Luces del cuadro
// ---------------------------------------------------------------
struct FrameLight { Vector3 pos, col; float range; bool caster; float d; };
static std::vector<FrameLight> frameLights;
static int frameShadows = 0;
bool lowQuality = false;   // F3: sin sombras ni resplandor (para PCs modestas)
float themeFogDensity = 0.04f;
Vector3 themeFog{ 0.006f, 0.006f, 0.008f }, themeAmbient{ 0.015f, 0.015f, 0.02f };
static float flickerK[64];

static void GatherLights() {
    frameLights.clear();
    int li = 0;
    for (auto &l : lights) {
        float k = 1;
        if (l.flicker) {
            float &fk = flickerK[li % 64];
            if (frand(0, 1) < 0.04f) fk = frand(0.1f, 1.0f); else fk = lerpf(fk, 1.0f, 0.1f);
            k = fk;
        }
        li++;
        FrameLight f{ l.pos, Vector3Scale(l.color, k * l.k), l.range, l.caster, Vector3Distance(l.pos, cam.position) };
        frameLights.push_back(f);
    }
    if (muzzleLightI > 0) frameLights.push_back({ muzzleLightPos, Vector3Scale(ColorV(vm.flashColor), muzzleLightI), 9, false, 0 });
    if (boomLightI > 0) frameLights.push_back({ boomLightPos, Vector3Scale(Vector3{ 1, 0.55f, 0.2f }, boomLightI), 18, false, 0.5f });
    // luz azul de la caja misteriosa abierta
    if (box.lid > 0.05f) frameLights.push_back({ Vector3{ box.pos.x, 1.3f, box.pos.y }, Vector3Scale(Vector3{ 0.5f, 0.8f, 1.6f }, box.lid * 3.0f), 6, false, Vector3Distance(Vector3{ box.pos.x, 1.3f, box.pos.y }, cam.position) });
    // las 2 luces que proyectan sombra más cercanas van primero
    std::sort(frameLights.begin(), frameLights.end(), [](const FrameLight &a, const FrameLight &b) {
        if (a.caster != b.caster) return a.caster > b.caster;
        return a.d < b.d;
    });
    frameShadows = 0;
    for (int i = 0; i < 2 && i < (int)frameLights.size(); i++) if (frameLights[i].caster && frameLights[i].d < 22) frameShadows = i + 1;
    if (lowQuality) frameShadows = 0;
    // del resto, las más cercanas
    if ((int)frameLights.size() > 24) {
        std::sort(frameLights.begin() + frameShadows, frameLights.end(), [](const FrameLight &a, const FrameLight &b) { return a.d < b.d; });
        frameLights.resize(24);
    }
}

static void UploadLights(Shader &s, LitLocs &l) {
    int n = (int)frameLights.size();
    Vector3 pos[24], col[24]; float range[24];
    for (int i = 0; i < n; i++) { pos[i] = frameLights[i].pos; col[i] = frameLights[i].col; range[i] = frameLights[i].range; }
    SetShaderValue(s, l.numLights, &n, SHADER_UNIFORM_INT);
    if (n) {
        SetShaderValueV(s, l.lPos, pos, SHADER_UNIFORM_VEC3, n);
        SetShaderValueV(s, l.lCol, col, SHADER_UNIFORM_VEC3, n);
        SetShaderValueV(s, l.lRange, range, SHADER_UNIFORM_FLOAT, n);
    }
    SetShaderValue(s, l.numShadows, &frameShadows, SHADER_UNIFORM_INT);
    SetShaderValueMatrix(s, l.lightVP0, lightVP[0]);
    SetShaderValueMatrix(s, l.lightVP1, lightVP[1]);
    SetShaderValue(s, l.viewPos, &cam.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(s, l.ambient, &themeAmbient, SHADER_UNIFORM_VEC3);
    SetShaderValue(s, l.fogColor, &themeFog, SHADER_UNIFORM_VEC3);
    SetShaderValue(s, l.fogDensity, &themeFogDensity, SHADER_UNIFORM_FLOAT);
    float zero = 0;
    SetShaderValue(s, l.hitFlash, &zero, SHADER_UNIFORM_FLOAT);
    int s0 = 10, s1 = 11;
    SetShaderValue(s, l.shadow0, &s0, SHADER_UNIFORM_INT);
    SetShaderValue(s, l.shadow1, &s1, SHADER_UNIFORM_INT);
}

// ---------------------------------------------------------------
// Escena
// ---------------------------------------------------------------
void DrawWorldDynamic();
void DrawLabels();
void PrepareMinimap();

static void DrawSceneGeometry() {
    for (auto &sm : staticMeshes) DrawPM(sm.first, *sm.second, MatrixIdentity());
    for (auto &p : props) if (p.visible) DrawPM(*p.mesh, *p.mat, p.tf);
    DrawWorldDynamic();
    DrawBoxAndPap();
    DrawZombies();
    DrawProjectiles();
    DrawPowerups();
}

static void ShadowPass() {
    for (int i = 0; i < frameShadows; i++) {
        Vector3 lp = frameLights[i].pos;
        Camera3D lc{};
        lc.position = lp;
        lc.target = Vector3{ lp.x, lp.y - 1, lp.z };
        lc.up = Vector3{ 0, 0, -1 };
        lc.fovy = 150;
        lc.projection = CAMERA_PERSPECTIVE;
        Matrix view = GetCameraMatrix(lc);
        Matrix proj = MatrixPerspective(lc.fovy * DEG2RAD, 1.0, 0.1, 16.0);
        lightVP[i] = MatrixMultiply(view, proj);
        BeginTextureMode(shadowRT[i]);
        rlViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
        ClearBackground(WHITE);
        rlSetClipPlanes(0.1, 16.0);
        BeginMode3D(lc);
        depthPass = true;
        DrawSceneGeometry();
        depthPass = false;
        EndMode3D();
        EndTextureMode();
    }
}

static void FullscreenPass(RenderTexture2D &dst, Texture2D src, Shader *sh) {
    BeginTextureMode(dst);
    ClearBackground(BLACK);
    if (sh) BeginShaderMode(*sh);
    DrawTexturePro(src, Rectangle{ 0, 0, (float)src.width, -(float)src.height }, Rectangle{ 0, 0, (float)dst.texture.width, (float)dst.texture.height }, Vector2{ 0, 0 }, 0, WHITE);
    if (sh) EndShaderMode();
    EndTextureMode();
}

void DrawUnlitBegin(float intensity) {
    rlDrawRenderBatchActive();
    BeginShaderMode(shUnlit);
    SetShaderValue(shUnlit, locUnlitI, &intensity, SHADER_UNIFORM_FLOAT);
}
void DrawUnlitEnd() { rlDrawRenderBatchActive(); EndShaderMode(); }
Shader &MaskShader() { return shMask; }

float exposureK = 1.15f, flashK = 0;

void RenderFrame(bool menu) {
    ResizeRender();
    GatherLights();
    ShadowPass();

    // --- escena principal en HDR ---
    BeginTextureMode(hdrRT);
    // el búfer HDR es lineal: el color de fondo se pasa ya en lineal (casi negro, noche)
    Color fogc{ (unsigned char)(themeFog.x * 255 + 0.5f), (unsigned char)(themeFog.y * 255 + 0.5f), (unsigned char)(themeFog.z * 255 + 0.5f), 255 };
    ClearBackground(fogc);
    rlActiveTextureSlot(10); rlEnableTexture(shadowRT[0].depth.id);
    rlActiveTextureSlot(11); rlEnableTexture(shadowRT[1].depth.id);
    rlActiveTextureSlot(0);
    UploadLights(shLit, locLit);
    UploadLights(shLitSkin, locSkin);
    cam.fovy = camFov;
    rlSetClipPlanes(0.05, 150.0);
    BeginMode3D(cam);
    DrawSceneGeometry();
    rlDisableDepthMask();
    DrawEffects();
    DrawLabels();
    rlEnableDepthMask();
    EndMode3D();

    // --- arma en primera persona (con su propio campo de visión) ---
    if (!menu && gunVisible) {
        rlDrawRenderBatchActive();
        glClear(GL_DEPTH_BUFFER_BIT);
        Camera3D gc = cam;
        gc.fovy = 60;
        rlSetClipPlanes(0.01, 10.0);
        BeginMode3D(gc);
        const OwnedWeapon &w = CurWeapon();
        DrawGun(gunModels[w.id], gunWorld, w.pap);
        DrawKnife();
        if (vm.flash > 0) {
            Vector3 fp = Vector3Transform(Vector3{ 0, gunModels[w.id].muzzleY, gunModels[w.id].muzzleZ - 0.04f }, gunWorld);
            DrawUnlitBegin(8.0f);
            rlSetBlendMode(BLEND_ADDITIVE);
            DrawBillboard(gc, texFlash, fp, 0.22f, ColorFromNormalized(Vector4{ ColorV(vm.flashColor).x, ColorV(vm.flashColor).y, ColorV(vm.flashColor).z, 1 }));
            DrawUnlitEnd();
            rlSetBlendMode(BLEND_ALPHA);
        }
        EndMode3D();
    }
    EndTextureMode();
    rlActiveTextureSlot(10); rlDisableTexture();
    rlActiveTextureSlot(11); rlDisableTexture();
    rlActiveTextureSlot(0);

    // --- bloom ---
    FullscreenPass(bloomA, hdrRT.texture, &shBright);
    for (int i = 0; i < (lowQuality ? 0 : 2); i++) {
        float dx[2] = { 1.5f / bloomA.texture.width, 0 }, dy[2] = { 0, 1.5f / bloomA.texture.height };
        SetShaderValue(shBlur, locBlurDir, dx, SHADER_UNIFORM_VEC2);
        FullscreenPass(bloomB, bloomA.texture, &shBlur);
        SetShaderValue(shBlur, locBlurDir, dy, SHADER_UNIFORM_VEC2);
        FullscreenPass(bloomA, bloomB.texture, &shBlur);
    }

    // --- composición: tono de cine, viñeta, grano ---
    float t = (float)GetTime(), dmg = menu ? 0 : clampf(1 - player.health / player.maxHealth, 0, 1) * 0.85f;
    float res[2] = { (float)rtW, (float)rtH }, bk = lowQuality ? 0.0f : 0.9f;
    SetShaderValue(shComposite, locCompExp, &exposureK, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shComposite, locCompTime, &t, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shComposite, locCompDmg, &dmg, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shComposite, locCompFlash, &flashK, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shComposite, locCompBloom, &bk, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shComposite, locCompRes, res, SHADER_UNIFORM_VEC2);
    BeginTextureMode(ldrRT);
    BeginShaderMode(shComposite);
    SetShaderValueTexture(shComposite, locCompBloomTex, bloomA.texture);
    DrawTexturePro(hdrRT.texture, Rectangle{ 0, 0, (float)rtW, -(float)rtH }, Rectangle{ 0, 0, (float)rtW, (float)rtH }, Vector2{ 0, 0 }, 0, WHITE);
    EndShaderMode();
    EndTextureMode();

    // --- pantalla: antialiasing FXAA + interfaz ---
    if (!menu) PrepareMinimap();
    auto finalPass = [&]() {
        BeginShaderMode(shFxaa);
        SetShaderValue(shFxaa, locFxaaRes, res, SHADER_UNIFORM_VEC2);
        DrawTexturePro(ldrRT.texture, Rectangle{ 0, 0, (float)rtW, -(float)rtH }, Rectangle{ 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() }, Vector2{ 0, 0 }, 0, WHITE);
        EndShaderMode();
        if (menu) DrawMenu();
        else {
            DrawHUD();
            if (game.state == GS_PAUSED) DrawPause();
            if (game.state == GS_OVER) DrawGameOver();
        }
    };
    if (captureTarget) {
        BeginTextureMode(*captureTarget);
        ClearBackground(BLACK);
        finalPass();
        EndTextureMode();
    }
    BeginDrawing();
    ClearBackground(BLACK);
    finalPass();
    EndDrawing();
}
