'use strict';
/* =============================================================
   NOCHE DE ZOMBIS — shooter de zombis en primera persona
   Hecho con Three.js (r128). Abre index.html en el navegador.
   ============================================================= */

// ---------- CONFIGURACIÓN (personaliza aquí) ----------
const CONFIG = {
  nombre: 'mi amor',            // aparece en la pantalla de inicio
  mensajes: {                    // mensajes especiales al empezar ciertas rondas
    5: '¡Vas increíble, sigue así! ❤',
    10: '¡Ronda 10! Eres una leyenda 👑',
    15: 'Nadie juega como tú ❤',
  },
  puntosIniciales: 500,
  sensibilidad: 0.0022,
  musica: true,
};

// ---------- Utilidades ----------
const $ = (id) => document.getElementById(id);
const rand = (a, b) => a + Math.random() * (b - a);
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const lerp = (a, b, t) => a + (b - a) * t;
const damp = (k, dt) => 1 - Math.exp(-k * dt);
const pick = (arr) => arr[Math.floor(Math.random() * arr.length)];
const hex = (c) => '#' + c.toString(16).padStart(6, '0');
const V3 = THREE.Vector3;
function setText(el, v) { v = String(v); if (el._v !== v) { el._v = v; el.textContent = v; } }
function setHTML(el, v) { if (el._v !== v) { el._v = v; el.innerHTML = v; } }

// ---------- Render ----------
const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.autoClear = false;
renderer.toneMapping = THREE.ACESFilmicToneMapping; // contraste y colores más de cine
renderer.toneMappingExposure = 1.0;
$('game').appendChild(renderer.domElement);

const scene = new THREE.Scene();
scene.background = new THREE.Color(0x07070a);
scene.fog = new THREE.Fog(0x07070a, 6, 36);

const camera = new THREE.PerspectiveCamera(75, innerWidth / innerHeight, 0.05, 250);
camera.rotation.order = 'YXZ';
scene.add(camera);

// El arma se dibuja en una escena aparte para que nunca atraviese paredes
const gunScene = new THREE.Scene();
const gunCam = new THREE.PerspectiveCamera(60, innerWidth / innerHeight, 0.01, 10);
gunScene.add(new THREE.AmbientLight(0xffffff, 0.55));
const gunSun = new THREE.DirectionalLight(0xfff0dd, 0.8);
gunSun.position.set(1, 2, 1);
gunScene.add(gunSun);

addEventListener('resize', () => {
  renderer.setSize(innerWidth, innerHeight);
  camera.aspect = gunCam.aspect = innerWidth / innerHeight;
  camera.updateProjectionMatrix();
  gunCam.updateProjectionMatrix();
});

const ambient = new THREE.AmbientLight(0x404058, 0.5);
const hemi = new THREE.HemisphereLight(0x8888aa, 0x221111, 0.3);
const moon = new THREE.DirectionalLight(0x6070a0, 0.25);
moon.position.set(-10, 20, -5);
scene.add(ambient, hemi, moon);
const muzzleLight = new THREE.PointLight(0xffaa55, 0, 9, 2);
muzzleLight.position.set(0, -0.1, -0.6);
camera.add(muzzleLight);
const boomLight = new THREE.PointLight(0xff8833, 0, 16, 2);
scene.add(boomLight);

// ---------- Texturas generadas ----------
function canvasTexture(w, h, draw) {
  const c = document.createElement('canvas');
  c.width = w; c.height = h;
  draw(c.getContext('2d'), w, h);
  const t = new THREE.CanvasTexture(c);
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.anisotropy = Math.min(8, renderer.capabilities.getMaxAnisotropy()); // nítidas aunque se vean de lado
  return t;
}
function speckle(g, w, h, n, alpha) {
  for (let i = 0; i < n; i++) {
    const v = Math.random() * 255 | 0;
    g.fillStyle = `rgba(${v},${v},${v},${alpha})`;
    g.fillRect(Math.random() * w, Math.random() * h, 2, 2);
  }
}
function stains(g, w, h, n, color) {
  for (let i = 0; i < n; i++) {
    g.fillStyle = color;
    g.beginPath(); g.arc(Math.random() * w, Math.random() * h, rand(4, 24), 0, 7); g.fill();
  }
}

const TEX = {
  brick: canvasTexture(512, 512, (g, s) => {
    g.fillStyle = '#1c1816'; g.fillRect(0, 0, s, s);
    const bw = 128, bh = 64;
    for (let row = 0; row < s / bh; row++) {
      const off = (row % 2) * bw / 2;
      for (let x = -bw; x < s + bw; x += bw) {
        const bx = x + off + 4, by = row * bh + 4, w = bw - 8, h = bh - 8;
        const r = 70 + Math.random() * 40 | 0;
        const grd = g.createLinearGradient(bx, by, bx, by + h);
        grd.addColorStop(0, `rgb(${r + 14},${(r * 0.62 | 0) + 8},${(r * 0.5 | 0) + 6})`);
        grd.addColorStop(1, `rgb(${r - 18},${(r * 0.62 | 0) - 12},${(r * 0.5 | 0) - 10})`);
        g.fillStyle = grd; g.fillRect(bx, by, w, h);
        // poros y desconchones
        for (let i = 0; i < 70; i++) {
          const v = Math.random() < 0.5 ? 'rgba(0,0,0,0.25)' : 'rgba(255,220,190,0.08)';
          g.fillStyle = v; g.fillRect(bx + Math.random() * w, by + Math.random() * h, rand(1, 4), rand(1, 3));
        }
        if (Math.random() < 0.35) { g.fillStyle = '#1c1816'; g.beginPath(); g.arc(bx + (Math.random() < 0.5 ? 0 : w), by + Math.random() * h, rand(4, 10), 0, 7); g.fill(); }
      }
    }
    speckle(g, s, s, 9000, 0.08);
    // suciedad que baja desde arriba y humedad abajo
    const dirt = g.createLinearGradient(0, 0, 0, s);
    dirt.addColorStop(0, 'rgba(0,0,0,0.25)'); dirt.addColorStop(0.5, 'rgba(0,0,0,0)'); dirt.addColorStop(1, 'rgba(10,15,5,0.35)');
    g.fillStyle = dirt; g.fillRect(0, 0, s, s);
    stains(g, s, s, 5, 'rgba(0,0,0,0.12)');
  }),
  tiles: canvasTexture(512, 512, (g, s) => {
    const n = 4, ts = s / n;
    for (let y = 0; y < n; y++) for (let x = 0; x < n; x++) {
      const v = 58 + Math.random() * 16 | 0;
      const grd = g.createRadialGradient(x * ts + ts / 2, y * ts + ts / 2, 10, x * ts + ts / 2, y * ts + ts / 2, ts * 0.75);
      grd.addColorStop(0, `rgb(${v},${v - 4},${v - 9})`); grd.addColorStop(1, `rgb(${v - 16},${v - 19},${v - 22})`);
      g.fillStyle = grd; g.fillRect(x * ts, y * ts, ts, ts);
      if (Math.random() < 0.25) { // baldosa rajada
        g.strokeStyle = 'rgba(10,8,6,0.8)'; g.lineWidth = 1.5; g.beginPath();
        let px = x * ts + rand(10, ts - 10), py = y * ts + 4; g.moveTo(px, py);
        while (py < (y + 1) * ts - 6) { px += rand(-10, 10); py += rand(8, 20); g.lineTo(px, py); }
        g.stroke();
      }
    }
    g.strokeStyle = '#121010'; g.lineWidth = 6;
    for (let i = 0; i <= n; i++) {
      g.beginPath(); g.moveTo(i * ts, 0); g.lineTo(i * ts, s); g.stroke();
      g.beginPath(); g.moveTo(0, i * ts); g.lineTo(s, i * ts); g.stroke();
    }
    speckle(g, s, s, 9000, 0.07);
    stains(g, s, s, 5, 'rgba(70,5,5,0.22)');
    stains(g, s, s, 6, 'rgba(0,0,0,0.15)');
  }),
  concrete: canvasTexture(512, 512, (g, s) => {
    g.fillStyle = '#5c5c5a'; g.fillRect(0, 0, s, s);
    for (let i = 0; i < 40; i++) { // manchas suaves de cemento
      const grd = g.createRadialGradient(0, 0, 0, 0, 0, rand(20, 70));
      const v = Math.random() < 0.5 ? '0,0,0' : '255,255,255';
      grd.addColorStop(0, `rgba(${v},0.07)`); grd.addColorStop(1, `rgba(${v},0)`);
      g.save(); g.translate(Math.random() * s, Math.random() * s); g.fillStyle = grd; g.fillRect(-80, -80, 160, 160); g.restore();
    }
    speckle(g, s, s, 14000, 0.1);
    g.strokeStyle = 'rgba(0,0,0,0.45)'; g.lineWidth = 3;
    g.beginPath(); g.moveTo(256, 0); g.lineTo(256, s); g.stroke();
    g.fillStyle = 'rgba(0,0,0,0.35)'; // agujeros del encofrado
    for (const [x, y] of [[64, 96], [192, 96], [320, 96], [448, 96], [64, 320], [192, 320], [320, 320], [448, 320]]) { g.beginPath(); g.arc(x, y, 5, 0, 7); g.fill(); }
    // chorreones de óxido y humedad
    for (let i = 0; i < 8; i++) {
      const x = Math.random() * s, grd = g.createLinearGradient(0, 0, 0, s * 0.8);
      grd.addColorStop(0, 'rgba(90,50,20,0.25)'); grd.addColorStop(1, 'rgba(90,50,20,0)');
      g.fillStyle = grd; g.fillRect(x, 0, rand(3, 10), s * 0.8);
    }
    stains(g, s, s, 6, 'rgba(30,25,20,0.18)');
    // franja de peligro abajo
    g.fillStyle = '#d0a81e'; g.fillRect(0, s - 56, s, 56);
    g.fillStyle = '#111';
    for (let x = -56; x < s; x += 56) {
      g.beginPath(); g.moveTo(x, s); g.lineTo(x + 28, s); g.lineTo(x + 56, s - 56); g.lineTo(x + 28, s - 56); g.fill();
    }
    g.globalAlpha = 0.35; speckle(g, s, s, 3000, 0.3); g.globalAlpha = 1;
  }),
  concreteFloor: canvasTexture(256, 256, (g, s) => {
    g.fillStyle = '#3e3e3c'; g.fillRect(0, 0, s, s);
    speckle(g, s, s, 7000, 0.1);
    stains(g, s, s, 6, 'rgba(10,10,10,0.25)');
    g.strokeStyle = 'rgba(0,0,0,0.5)'; g.lineWidth = 1.5;
    for (let i = 0; i < 4; i++) {
      g.beginPath(); let x = Math.random() * s, y = Math.random() * s; g.moveTo(x, y);
      for (let k = 0; k < 6; k++) { x += rand(-25, 25); y += rand(-25, 25); g.lineTo(x, y); }
      g.stroke();
    }
  }),
  ceil: canvasTexture(128, 128, (g, s) => { g.fillStyle = '#2b2b2e'; g.fillRect(0, 0, s, s); speckle(g, s, s, 2000, 0.12); }),
  metalCeil: canvasTexture(128, 128, (g, s) => {
    g.fillStyle = '#26282b'; g.fillRect(0, 0, s, s);
    g.fillStyle = '#1a1b1d'; g.fillRect(0, 56, s, 16);
    speckle(g, s, s, 1500, 0.1);
  }),
  ground: canvasTexture(256, 256, (g, s) => {
    g.fillStyle = '#1b1d14'; g.fillRect(0, 0, s, s);
    for (let i = 0; i < 3000; i++) {
      g.fillStyle = Math.random() < 0.5 ? 'rgba(40,50,25,0.5)' : 'rgba(30,24,16,0.5)';
      g.fillRect(Math.random() * s, Math.random() * s, 3, 3);
    }
  }),
  door: canvasTexture(128, 128, (g, s) => {
    g.fillStyle = '#3d2a1a'; g.fillRect(0, 0, s, s);
    for (let i = 0; i < 6; i++) {
      const v = 70 + Math.random() * 25 | 0;
      g.fillStyle = `rgb(${v},${v * 0.65 | 0},${v * 0.4 | 0})`;
      g.fillRect(i * 21 + 1, 0, 19, s);
    }
    g.fillStyle = '#555'; g.fillRect(0, 20, s, 10); g.fillRect(0, 98, s, 10);
    speckle(g, s, s, 800, 0.1);
  }),
  crate: canvasTexture(128, 128, (g, s) => {
    g.fillStyle = '#6b4a2b'; g.fillRect(0, 0, s, s);
    g.strokeStyle = '#3b2814'; g.lineWidth = 10; g.strokeRect(5, 5, s - 10, s - 10);
    g.lineWidth = 8; g.beginPath(); g.moveTo(8, 8); g.lineTo(s - 8, s - 8); g.stroke();
    speckle(g, s, s, 600, 0.1);
  }),
  metal: canvasTexture(128, 128, (g, s) => {
    g.fillStyle = '#4a4f55'; g.fillRect(0, 0, s, s);
    g.strokeStyle = '#2a2d31'; g.lineWidth = 4; g.strokeRect(4, 4, s - 8, s - 8);
    g.fillStyle = '#2a2d31';
    for (const [x, y] of [[10, 10], [s - 14, 10], [10, s - 14], [s - 14, s - 14]]) g.fillRect(x, y, 4, 4);
    speckle(g, s, s, 1200, 0.1);
    stains(g, s, s, 3, 'rgba(90,50,20,0.25)');
  }),
  barrel: canvasTexture(128, 128, (g, s) => {
    g.fillStyle = '#7a1c14'; g.fillRect(0, 0, s, s);
    g.fillStyle = '#4a0f0a'; g.fillRect(0, 20, s, 8); g.fillRect(0, 100, s, 8);
    speckle(g, s, s, 1000, 0.12);
    stains(g, s, s, 3, 'rgba(40,20,10,0.4)');
  }),
  box: canvasTexture(256, 256, (g, s) => {
    g.fillStyle = '#3a2614'; g.fillRect(0, 0, s, s);
    g.strokeStyle = '#c8a24a'; g.lineWidth = 8; g.strokeRect(8, 8, s - 16, s - 16);
    g.fillStyle = '#7fd4ff'; g.shadowColor = '#7fd4ff'; g.shadowBlur = 20;
    g.font = 'bold 170px Arial'; g.textAlign = 'center'; g.textBaseline = 'middle';
    g.fillText('?', s / 2, s / 2 + 8);
  }),
  pap: canvasTexture(256, 256, (g, s) => {
    g.fillStyle = '#1c1028'; g.fillRect(0, 0, s, s);
    g.strokeStyle = '#b04aff'; g.lineWidth = 6; g.strokeRect(10, 10, s - 20, s - 20);
    g.fillStyle = '#e0b0ff'; g.shadowColor = '#b04aff'; g.shadowBlur = 25;
    g.beginPath(); g.moveTo(140, 30); g.lineTo(80, 140); g.lineTo(125, 140); g.lineTo(100, 226); g.lineTo(180, 105); g.lineTo(133, 105); g.closePath(); g.fill();
  }),
  flash: canvasTexture(64, 64, (g, s) => {
    const grd = g.createRadialGradient(s / 2, s / 2, 0, s / 2, s / 2, s / 2);
    grd.addColorStop(0, 'rgba(255,255,255,1)');
    grd.addColorStop(0.3, 'rgba(255,200,80,0.9)');
    grd.addColorStop(1, 'rgba(255,120,0,0)');
    g.fillStyle = grd; g.fillRect(0, 0, s, s);
  }),
  blood: canvasTexture(128, 128, (g) => {
    g.fillStyle = 'rgba(90,0,0,0.85)';
    g.beginPath(); g.arc(64, 64, 30, 0, 7); g.fill();
    for (let i = 0; i < 14; i++) { g.beginPath(); g.arc(64 + rand(-50, 50), 64 + rand(-50, 50), rand(3, 12), 0, 7); g.fill(); }
  }),
  hole: canvasTexture(32, 32, (g) => {
    const grd = g.createRadialGradient(16, 16, 0, 16, 16, 16);
    grd.addColorStop(0, 'rgba(0,0,0,1)'); grd.addColorStop(0.4, 'rgba(10,8,6,0.9)'); grd.addColorStop(1, 'rgba(0,0,0,0)');
    g.fillStyle = grd; g.fillRect(0, 0, 32, 32);
  }),
  scorch: canvasTexture(128, 128, (g, s) => {
    const grd = g.createRadialGradient(64, 64, 0, 64, 64, 64);
    grd.addColorStop(0, 'rgba(0,0,0,0.9)'); grd.addColorStop(0.5, 'rgba(15,10,5,0.6)'); grd.addColorStop(1, 'rgba(0,0,0,0)');
    g.fillStyle = grd; g.fillRect(0, 0, s, s);
  }),
  camo: canvasTexture(128, 128, (g, s) => {
    g.fillStyle = '#2a0a4a'; g.fillRect(0, 0, s, s);
    for (let i = 0; i < 30; i++) {
      g.fillStyle = pick(['#6a1bb0', '#2a60ff', '#b04aff', '#14143a']);
      g.beginPath(); g.ellipse(Math.random() * s, Math.random() * s, rand(6, 22), rand(4, 12), Math.random() * 3, 0, 7); g.fill();
    }
  }),
};

const THEMES = {
  bunker:  { wall: TEX.brick,    wallRepeatY: 4 / 3, floor: TEX.tiles,         ceil: TEX.ceil,      fog: 0x07070a, ambient: 0x404058 },
  fabrica: { wall: TEX.concrete, wallRepeatY: 1,     floor: TEX.concreteFloor, ceil: TEX.metalCeil, fog: 0x08090b, ambient: 0x3a4048 },
};

function makeLabel(text, color = '#ffffff', width = 2.4) {
  const c = document.createElement('canvas');
  c.width = 512; c.height = 128;
  const g = c.getContext('2d');
  g.font = 'bold 54px Oswald, Arial'; g.textAlign = 'center'; g.textBaseline = 'middle';
  g.lineWidth = 8; g.strokeStyle = 'rgba(0,0,0,0.85)'; g.strokeText(text, 256, 64);
  g.fillStyle = color; g.fillText(text, 256, 64);
  const sp = new THREE.Sprite(new THREE.SpriteMaterial({ map: new THREE.CanvasTexture(c), transparent: true, depthWrite: false }));
  sp.scale.set(width, width / 4, 1);
  return sp;
}
// Material con relieve: la misma textura sirve de mapa de relieve (lo oscuro queda hundido: juntas, grietas)
const stdMat = (opts) => {
  const o = Object.assign({ roughness: 0.9, metalness: 0 }, opts);
  if (o.map && o.bump !== false) { o.bumpMap = o.map; o.bumpScale = o.bumpScale || 0.035; }
  delete o.bump;
  return new THREE.MeshStandardMaterial(o);
};

// ---------- Armas ----------
const WEAPONS = {
  pistol:   { name: 'PISTOLA',          dmg: 40,  head: 3,   rate: 0.17, auto: false, mag: 8,   reserve: 80,  reload: 1.5, spread: 0.006, pellets: 1, recoil: 0.012, cost: 0,    color: 0x2e2e2e, len: 0.22, sound: 'pistol' },
  shotgun:  { name: 'ESCOPETA',         dmg: 30,  head: 2,   rate: 0.8,  auto: false, mag: 6,   reserve: 36,  reload: 2.6, spread: 0.07,  pellets: 8, recoil: 0.04,  cost: 500,  color: 0x5a3a22, len: 0.55, sound: 'shotgun' },
  smg:      { name: 'SUBFUSIL',         dmg: 42,  head: 3,   rate: 0.075,auto: true,  mag: 30,  reserve: 180, reload: 2.0, spread: 0.025, pellets: 1, recoil: 0.006, cost: 1000, color: 0x1e1e1e, len: 0.4,  sound: 'smg' },
  rifle:    { name: 'RIFLE DE ASALTO',  dmg: 85,  head: 3,   rate: 0.1,  auto: true,  mag: 30,  reserve: 150, reload: 2.3, spread: 0.012, pellets: 1, recoil: 0.008, cost: 1400, color: 0x3b4030, len: 0.55, sound: 'rifle' },
  lmg:      { name: 'AMETRALLADORA',    dmg: 110, head: 2.5, rate: 0.09, auto: true,  mag: 100, reserve: 300, reload: 4.0, spread: 0.03,  pellets: 1, recoil: 0.007, cost: 2000, color: 0x2a2a2a, len: 0.6,  sound: 'rifle' },
  sniper:   { name: 'FRANCOTIRADOR',    dmg: 900, head: 5,   rate: 1.1,  auto: false, mag: 5,   reserve: 40,  reload: 3.0, spread: 0.0,   pellets: 1, recoil: 0.06,  cost: 1500, color: 0x1d2b1d, len: 0.7,  sound: 'sniper', tracer: 0xffffaa },
  revolver: { name: 'REVÓLVER',         dmg: 220, head: 4,   rate: 0.35, auto: false, mag: 6,   reserve: 48,  reload: 2.8, spread: 0.004, pellets: 1, recoil: 0.04,  cost: 0,    color: 0x666666, len: 0.3,  sound: 'sniper' },
  raygun:   { name: 'PISTOLA DE RAYOS', dmg: 700, head: 2,   rate: 0.33, auto: false, mag: 20,  reserve: 160, reload: 2.4, spread: 0.004, pellets: 1, recoil: 0.015, cost: 0,    color: 0x2fb52f, len: 0.3,  sound: 'ray', tracer: 0x55ff55, splash: 2.2 },
  wunder:   { name: 'ARMA MARAVILLA',   dmg: 1e9, head: 1,   rate: 0.9,  auto: false, mag: 3,   reserve: 12,  reload: 3.0, spread: 0.0,   pellets: 1, recoil: 0.03,  cost: 0,    color: 0x3355aa, len: 0.45, sound: 'zap', chain: 6 },
  // Explosivas: disparan un proyectil que estalla al chocar (dmg = daño en el centro, radius = alcance)
  launcher: { name: 'LANZAGRANADAS',    dmg: 900, head: 1,   rate: 0.55, auto: false, mag: 6,   reserve: 36,  reload: 3.0, spread: 0.0,   pellets: 1, recoil: 0.05,  cost: 2500, color: 0x4a5a3a, len: 0.45, sound: 'launcher', projectile: 'grenade', radius: 4.5 },
  rpg:      { name: 'LANZAMISILES',     dmg: 2500,head: 1,   rate: 0.8,  auto: false, mag: 1,   reserve: 14,  reload: 2.4, spread: 0.0,   pellets: 1, recoil: 0.09,  cost: 3000, color: 0x3d4a2e, len: 0.9,  sound: 'rocket',   projectile: 'rocket',  radius: 6 },
};
for (const k in WEAPONS) WEAPONS[k].id = k;
const PAP_NAMES = {
  pistol: 'TRUENO GEMELO', shotgun: 'ESCUPIDORA INFERNAL', smg: 'ENJAMBRE', rifle: 'TORMENTA DE PLOMO',
  lmg: 'LA DEVASTADORA', sniper: 'OJO DEL DIABLO', revolver: 'MANO DE DIOS', raygun: 'RAYO ESTELAR', wunder: 'FURIA DE ZEUS',
  launcher: 'EL VOLCÁN', rpg: 'APOCALIPSIS',
};
const BOX_POOL = ['shotgun', 'shotgun', 'smg', 'smg', 'rifle', 'rifle', 'lmg', 'lmg', 'sniper', 'sniper',
  'revolver', 'revolver', 'raygun', 'raygun', 'wunder', 'launcher', 'launcher', 'rpg', 'rpg'];

// Estadísticas reales de un arma (con Pack-a-Punch si está mejorada)
function stats(w) {
  const d = WEAPONS[w.id];
  if (!w.pap) return d;
  if (!d.papStats) {
    d.papStats = Object.assign({}, d, {
      name: PAP_NAMES[d.id], dmg: d.dmg * 2.5, head: d.head + 1,
      mag: Math.round(d.mag * 1.5), reserve: Math.round(d.reserve * 1.6), reload: d.reload * 0.85,
      tracer: d.tracer ? 0xff4466 : undefined, chain: d.chain ? 12 : undefined, radius: d.radius ? d.radius * 1.3 : undefined,
    });
  }
  return d.papStats;
}

function makeGunModel(id, pap = false, hands = false) {
  const def = WEAPONS[id];
  const g = new THREE.Group();
  const mat = pap ? new THREE.MeshLambertMaterial({ map: TEX.camo, emissive: 0x2a0044 }) : new THREE.MeshLambertMaterial({ color: def.color });
  const dark = new THREE.MeshLambertMaterial({ color: 0x111111 });
  const wood = new THREE.MeshLambertMaterial({ color: 0x6b4423 });
  const L = def.len;
  const box = (w, h, d, m, x, y, z, rx = 0) => {
    const mesh = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), m);
    mesh.position.set(x, y, z); mesh.rotation.x = rx; g.add(mesh); return mesh;
  };
  const cyl = (r1, r2, len, m, x, y, z, seg = 14) => {
    const mesh = new THREE.Mesh(new THREE.CylinderGeometry(r1, r2, len, seg), m);
    mesh.rotation.x = Math.PI / 2; mesh.position.set(x, y, z); g.add(mesh); return mesh;
  };
  const long = !['pistol', 'raygun', 'revolver'].includes(id);
  if (id === 'rpg') {
    // tubo al hombro con la cabeza del misil asomando
    cyl(0.055, 0.055, L, mat, 0, 0.03, -L / 2 - 0.05);
    cyl(0.068, 0.068, 0.08, dark, 0, 0.03, -0.06);
    cyl(0.068, 0.068, 0.08, dark, 0, 0.03, -L);
    const warhead = cyl(0.001, 0.07, 0.22, new THREE.MeshLambertMaterial({ color: 0x5a6a3a }), 0, 0.03, -L - 0.17);
    warhead.rotation.x = -Math.PI / 2;
    box(0.05, 0.13, 0.06, dark, 0, -0.08, -0.05, 0.25);
    box(0.05, 0.12, 0.06, dark, 0, -0.08, -0.35, 0.25);
    box(0.03, 0.06, 0.08, dark, -0.07, 0.08, -0.3);            // mira
    g.userData.muzzleZ = -L - 0.1;
    if (hands) addHands(true);
    return g;
  }
  if (id === 'launcher') {
    box(0.07, 0.09, L, mat, 0, 0, -L / 2);
    cyl(0.05, 0.05, L * 0.7, dark, 0, 0.03, -L * 0.75);        // cañón gordo
    cyl(0.085, 0.085, 0.14, dark, 0, -0.02, -0.18, 6);         // tambor
    box(0.05, 0.13, 0.06, dark, 0, -0.09, -0.05, 0.25);
    box(0.06, 0.08, 0.22, dark, 0, -0.02, 0.1);
    box(0.05, 0.1, 0.08, dark, 0, -0.08, -L * 0.6, 0.2);       // empuñadura delantera
    g.userData.muzzleZ = -L * 1.1;
    if (hands) addHands(true);
    return g;
  }
  box(0.07, 0.09, L, mat, 0, 0, -L / 2);                         // cuerpo
  box(0.035, 0.035, L * 0.45, dark, 0, 0.02, -L - L * 0.22);      // cañón
  box(0.05, 0.13, 0.06, id === 'shotgun' ? wood : dark, 0, -0.09, -0.05, 0.25); // empuñadura
  if (long) {
    box(0.06, 0.08, 0.22, id === 'shotgun' ? wood : dark, 0, -0.02, 0.1); // culata
    if (id !== 'shotgun') box(0.05, id === 'lmg' ? 0.1 : 0.15, id === 'lmg' ? 0.14 : 0.07, dark, 0, -0.11, -L * 0.45);
  }
  if (id === 'shotgun') box(0.06, 0.05, 0.18, wood, 0, -0.06, -L * 0.85);
  if (id === 'revolver') {
    const cyl = new THREE.Mesh(new THREE.CylinderGeometry(0.045, 0.045, 0.07, 10), dark);
    cyl.rotation.x = Math.PI / 2; cyl.position.set(0, 0, -0.08); g.add(cyl);
  }
  if (id === 'sniper' || id === 'rifle') {
    const scope = new THREE.Mesh(new THREE.CylinderGeometry(0.025, 0.025, id === 'sniper' ? 0.3 : 0.12, 10), dark);
    scope.rotation.x = Math.PI / 2; scope.position.set(0, 0.08, -L * 0.45); g.add(scope);
  }
  if (id === 'raygun' || id === 'wunder') {
    const ringMat = new THREE.MeshLambertMaterial({ color: id === 'raygun' ? 0xd02020 : 0x88ccff, emissive: id === 'raygun' ? 0x400000 : 0x224488 });
    for (let i = 0; i < 3; i++) {
      const ring = new THREE.Mesh(new THREE.CylinderGeometry(0.055, 0.055, 0.02, 12), ringMat);
      ring.rotation.x = Math.PI / 2; ring.position.set(0, 0.01, -L * 0.3 - i * 0.07); g.add(ring);
    }
  }
  if (hands) addHands(long);
  g.userData.muzzleZ = -L - L * 0.45;
  return g;

  function addHands(twoHanded) {
    const glove = new THREE.MeshLambertMaterial({ color: 0x2a2018 });
    const sleeve = new THREE.MeshLambertMaterial({ color: 0x3b4a2a });
    const fz = id === 'rpg' ? -0.35 : -L * 0.72;
    box(0.075, 0.085, 0.1, glove, 0.005, -0.1, -0.02);
    const rs = box(0.095, 0.095, 0.45, sleeve, 0.04, -0.17, 0.24, 0.35);
    rs.rotation.y = -0.15;
    if (twoHanded) {
      box(0.08, 0.07, 0.11, glove, -0.01, -0.07, fz);
      const ls = box(0.095, 0.095, 0.5, sleeve, -0.12, -0.16, fz + 0.24, 0.3);
      ls.rotation.y = 0.55;
    } else {
      box(0.07, 0.08, 0.1, glove, -0.04, -0.13, 0.0);
      const ls = box(0.095, 0.095, 0.45, sleeve, -0.13, -0.2, 0.22, 0.35);
      ls.rotation.y = 0.45;
    }
  }
}

// ---------- Mapas ----------
const H = 4; // altura de las paredes
/* Cada mapa es una lista de salas rectangulares. Las paredes se generan solas.
   links: puertas entre salas (cost 0 = paso abierto)
   windows: [sala, x, z] sobre una pared exterior
   wallBuys/perks/boxSpots/pap: [sala, ..., x, z] sobre una pared de esa sala
   props: [x, z, ancho, fondo, alto, tipo] */
const MAPS = {
  bunker: {
    name: 'BÚNKER', theme: 'bunker', start: { x: 0, z: 4, yaw: 0 }, startRoom: 'A',
    rooms: {
      A: { x1: -10, z1: -10, x2: 10, z2: 10, light: 0xffd9a0 },
      B: { x1: -10, z1: -26, x2: 10, z2: -10, light: 0xffc890, flicker: true },
      C: { x1: 10, z1: -8, x2: 28, z2: 8, light: 0xaaccff },
      D: { x1: -10, z1: 10, x2: 10, z2: 26, light: 0xff9a9a },
      E: { x1: 10, z1: -26, x2: 28, z2: -8, light: 0xc8a0ff },
    },
    links: [
      { a: 'A', b: 'B', x: 0, z: -10, cost: 750 },
      { a: 'A', b: 'C', x: 10, z: 0, cost: 1000 },
      { a: 'A', b: 'D', x: 0, z: 10, cost: 1000 },
      { a: 'B', b: 'E', x: 10, z: -18, cost: 1250 },
      { a: 'C', b: 'E', x: 19, z: -8, cost: 0 },
    ],
    windows: [['A', -10, -5], ['A', -10, 5], ['B', -10, -18], ['B', 0, -26], ['C', 28, 0], ['C', 19, 8],
      ['D', -10, 18], ['D', 0, 26], ['D', 10, 18], ['E', 19, -26], ['E', 28, -17]],
    wallBuys: [['A', 'shotgun', -6, -10], ['A', 'smg', 6, 10], ['B', 'rifle', -10, -22], ['E', 'sniper', 28, -11],
      ['C', 'launcher', 14, -8], ['D', 'rpg', -6, 26]],
    perks: [['A', 'revive', 10, -6], ['B', 'jugg', 6, -26], ['C', 'speed', 28, -5], ['D', 'double', 6, 26], ['A', 'stamin', -10, -1]],
    boxSpots: [['A', -5, 10], ['B', -5, -26], ['C', 23, 8], ['D', 10, 23]],
    pap: ['E', 24, -26],
    props: [[-5, 4, 1.2, 1.2, 1.2], [-6.2, 4, 1.2, 1.2, 1.2], [-5.6, 4, 1.1, 1.1, 2.3], [5, -5, 1.2, 1.2, 1.2], [6.2, -5.6, 1.2, 1.2, 1.2],
      [-5, -19, 1.2, 1.2, 1.2], [4, -21, 1.2, 1.2, 1.2], [5.3, -21, 0.8, 0.8, 1.1, 'barrel'], [16, 3, 1.2, 1.2, 1.2], [22, -2, 1.2, 1.2, 1.2],
      [-4, 19, 1.2, 1.2, 1.2], [5, 16, 3, 1.2, 0.9, 'metal'], [15, -17, 1.2, 1.2, 1.2], [22, -14, 0.8, 0.8, 1.1, 'barrel'], [23, -14.9, 0.8, 0.8, 1.1, 'barrel']],
  },
  fabrica: {
    name: 'FÁBRICA', theme: 'fabrica', start: { x: 0, z: 6, yaw: 0 }, startRoom: 'A',
    rooms: {
      A: { x1: -8, z1: -8, x2: 8, z2: 12, light: 0xffe0b0 },
      B: { x1: -24, z1: -16, x2: 24, z2: -8, light: 0xb0d0ff, lights: [[-12, -12], [12, -12]] },
      C: { x1: -24, z1: -8, x2: -8, z2: 12, light: 0xffb070, flicker: true },
      D: { x1: 8, z1: -8, x2: 24, z2: 12, light: 0x9affc0 },
      E: { x1: -24, z1: 12, x2: 24, z2: 30, light: 0xff8080, lights: [[-12, 21], [12, 21]] },
    },
    links: [
      { a: 'A', b: 'B', x: 0, z: -8, cost: 750 },
      { a: 'B', b: 'C', x: -16, z: -8, cost: 1000 },
      { a: 'B', b: 'D', x: 16, z: -8, cost: 1000 },
      { a: 'C', b: 'E', x: -16, z: 12, cost: 1250 },
      { a: 'D', b: 'E', x: 16, z: 12, cost: 1250 },
    ],
    windows: [['B', -18, -16], ['B', 0, -16], ['B', 18, -16], ['C', -24, -2], ['C', -24, 8], ['D', 24, -2], ['D', 24, 8],
      ['E', -14, 30], ['E', 0, 30], ['E', 14, 30], ['E', -24, 21], ['E', 24, 21]],
    groundSpawns: { A: [[-5, -5], [5, 9], [-5, 9], [5, -5]] },
    wallBuys: [['A', 'shotgun', 8, 4], ['A', 'smg', -5, -8], ['B', 'rifle', 6, -16], ['D', 'sniper', 11, -8], ['E', 'lmg', -24, 15],
      ['C', 'launcher', -24, -6], ['E', 'rpg', 24, 16]],
    perks: [['A', 'revive', -8, 0], ['D', 'jugg', 24, 3], ['C', 'speed', -21, 12], ['B', 'double', -9, -16], ['A', 'stamin', 8, 9]],
    boxSpots: [['A', -4, 12], ['B', 9, -16], ['C', -24, 3], ['E', -6, 30]],
    pap: ['E', 6, 30],
    props: [[-4, -3, 1.2, 1.2, 1.2], [4, 7, 1.2, 1.2, 1.2], [-16, -1, 3, 1.5, 1.8, 'metal'], [-19, 5, 1.2, 1.2, 1.2], [14, 0, 2.5, 1.5, 1.6, 'metal'],
      [19, 6, 0.8, 0.8, 1.1, 'barrel'], [19.9, 6.6, 0.8, 0.8, 1.1, 'barrel'], [-12, -12, 1.2, 1.2, 1.2], [12, -12, 1.2, 1.2, 1.2],
      [-10, 20, 4, 2, 2.4, 'metal'], [10, 22, 4, 2, 2.4, 'metal'], [0, 19, 1.2, 1.2, 1.2], [-19, 25, 0.8, 0.8, 1.1, 'barrel'], [19, 25, 1.2, 1.2, 1.2]],
    pipes: [[-24, -15.4, 24, -15.4], [-24, 29.4, 24, 29.4], [-23.4, -8, -23.4, 12]],
  },
};

// ---------- Estado del mundo ----------
let world = null, map = null, rooms = {}, links = [];
const colliders = [];       // cajas de colisión 2D {minX,maxX,minZ,maxZ}
const solids = [];          // mallas que detienen balas
const interactables = [];   // cosas que se usan con E
const windows = [];
const doors = [];
const mmWalls = [];         // paredes para el minimapa
const mmIcons = [];         // iconos fijos del minimapa
const unlocked = new Set();
const flickerLights = [];
const decals = [];

function addSolid(mesh, col) { world.add(mesh); solids.push(mesh); if (col) colliders.push(col); }
function removeSolid(mesh, col) {
  const i = solids.indexOf(mesh); if (i >= 0) solids.splice(i, 1);
  const j = colliders.indexOf(col); if (j >= 0) colliders.splice(j, 1);
}
const near = (a, b) => Math.abs(a - b) < 1e-6;
function roomAt(x, z) {
  for (const k in rooms) { const r = rooms[k]; if (x >= r.x1 && x <= r.x2 && z >= r.z1 && z <= r.z2) return k; }
  return null;
}
// Normal hacia dentro de la sala para un punto sobre su pared
function wallPlace(room, x, z) {
  const r = rooms[room];
  let nx = 0, nz = 0;
  if (near(r.x1, x)) nx = 1; else if (near(r.x2, x)) nx = -1; else if (near(r.z1, z)) nz = 1; else nz = -1;
  return { nx, nz, ry: Math.atan2(nx, nz) };
}
function recomputeUnlocked() {
  unlocked.clear();
  const q = [map.startRoom];
  unlocked.add(map.startRoom);
  while (q.length) {
    const c = q.shift();
    for (const l of links) {
      if (!l.open) continue;
      const n = l.a === c ? l.b : l.b === c ? l.a : null;
      if (n && !unlocked.has(n)) { unlocked.add(n); q.push(n); }
    }
  }
}
// Siguiente sala (y puerta) en el camino más corto de una sala a otra
function nextRoom(from, to) {
  if (!from || !to || from === to) return null;
  const prev = { [from]: null };
  const q = [from];
  while (q.length) {
    const c = q.shift();
    for (const l of links) {
      if (!l.open) continue;
      const n = l.a === c ? l.b : l.b === c ? l.a : null;
      if (!n || n in prev) continue;
      prev[n] = { room: c, link: l };
      if (n === to) {
        let cur = n;
        while (prev[cur].room !== from) cur = prev[cur].room;
        return { room: cur, link: prev[cur].link };
      }
      q.push(n);
    }
  }
  return null;
}

function wallSeg(x1, z1, x2, z2, theme) {
  const T = 0.4;
  const minX = Math.min(x1, x2) - (x1 === x2 ? T / 2 : 0), maxX = Math.max(x1, x2) + (x1 === x2 ? T / 2 : 0);
  const minZ = Math.min(z1, z2) - (z1 === z2 ? T / 2 : 0), maxZ = Math.max(z1, z2) + (z1 === z2 ? T / 2 : 0);
  const w = maxX - minX, d = maxZ - minZ;
  const tex = theme.wall.clone(); tex.needsUpdate = true;
  tex.repeat.set(Math.max(w, d) / 4, theme.wallRepeatY);
  const m = new THREE.Mesh(new THREE.BoxGeometry(w, H, d), stdMat({ map: tex }));
  m.position.set((minX + maxX) / 2, H / 2, (minZ + maxZ) / 2);
  const col = { minX, maxX, minZ, maxZ };
  addSolid(m, col);
  mmWalls.push(col);
}

function buildWalls(theme) {
  const lines = new Map(), gaps = new Map();
  const add = (m, key, s, e) => { if (!m.has(key)) m.set(key, []); m.get(key).push([s, e]); };
  for (const r of Object.values(rooms)) {
    add(lines, 'h' + r.z1, r.x1, r.x2); add(lines, 'h' + r.z2, r.x1, r.x2);
    add(lines, 'v' + r.x1, r.z1, r.z2); add(lines, 'v' + r.x2, r.z1, r.z2);
  }
  for (const l of links) {
    if (l.axis === 'z') add(gaps, 'v' + l.x, l.z - 1.5, l.z + 1.5); else add(gaps, 'h' + l.z, l.x - 1.5, l.x + 1.5);
  }
  for (const w of windows) {
    if (w.nx) add(gaps, 'v' + w.x, w.z - 0.8, w.z + 0.8); else add(gaps, 'h' + w.z, w.x - 0.8, w.x + 0.8);
  }
  for (const [key, ivs] of lines) {
    ivs.sort((a, b) => a[0] - b[0]);
    const merged = [];
    for (const iv of ivs) {
      const last = merged[merged.length - 1];
      if (last && iv[0] <= last[1] + 1e-6) last[1] = Math.max(last[1], iv[1]); else merged.push([iv[0], iv[1]]);
    }
    if (key[0] === 'h') for (const m of merged) { m[0] -= 0.2; m[1] += 0.2; } // tapar esquinas
    let parts = merged;
    for (const [gs, ge] of gaps.get(key) || []) {
      const next = [];
      for (const [s, e] of parts) {
        if (ge <= s || gs >= e) { next.push([s, e]); continue; }
        if (gs > s) next.push([s, gs]);
        if (ge < e) next.push([ge, e]);
      }
      parts = next;
    }
    const c = parseFloat(key.slice(1));
    for (const [s, e] of parts) {
      if (e - s < 0.05) continue;
      if (key[0] === 'h') wallSeg(s, c, e, c, theme); else wallSeg(c, s, c, e, theme);
    }
  }
}

function buildMap(key) {
  if (world) scene.remove(world);
  world = new THREE.Group();
  scene.add(world);
  colliders.length = 0; solids.length = 0; interactables.length = 0;
  windows.length = 0; doors.length = 0; mmWalls.length = 0; mmIcons.length = 0; flickerLights.length = 0;
  decals.length = 0;
  map = MAPS[key];
  rooms = map.rooms;
  links = map.links.map((l) => {
    const ra = rooms[l.a];
    const axis = (near(ra.x1, l.x) || near(ra.x2, l.x)) && l.z > ra.z1 && l.z < ra.z2 ? 'z' : 'x';
    const side = {};
    for (const rk of [l.a, l.b]) {
      const r = rooms[rk];
      if (axis === 'z') side[rk] = { x: l.x + ((r.x1 + r.x2) / 2 > l.x ? 1.3 : -1.3), z: l.z };
      else side[rk] = { x: l.x, z: l.z + ((r.z1 + r.z2) / 2 > l.z ? 1.3 : -1.3) };
    }
    return Object.assign({}, l, { axis, side, open: l.cost === 0 });
  });
  const theme = THEMES[map.theme];
  scene.fog.color.set(theme.fog);
  scene.background.set(theme.fog);
  ambient.color.set(theme.ambient);

  // Suelo exterior (se ve por las ventanas)
  const gtex = TEX.ground.clone(); gtex.needsUpdate = true; gtex.repeat.set(50, 50);
  const ground = new THREE.Mesh(new THREE.PlaneGeometry(220, 220), stdMat({ map: gtex }));
  ground.rotation.x = -Math.PI / 2; ground.position.y = -0.02;
  addSolid(ground);

  // Salas: suelo, techo y luces
  for (const k in rooms) {
    const r = rooms[k];
    const w = r.x2 - r.x1, d = r.z2 - r.z1, cx = (r.x1 + r.x2) / 2, cz = (r.z1 + r.z2) / 2;
    const ft = theme.floor.clone(); ft.needsUpdate = true; ft.repeat.set(w / 3, d / 3);
    const fl = new THREE.Mesh(new THREE.PlaneGeometry(w, d), stdMat({ map: ft, roughness: 0.6, metalness: 0.1, bumpScale: 0.05 }));
    fl.rotation.x = -Math.PI / 2; fl.position.set(cx, 0, cz); addSolid(fl);
    const ct = theme.ceil.clone(); ct.needsUpdate = true; ct.repeat.set(w / 4, d / 4);
    const ce = new THREE.Mesh(new THREE.PlaneGeometry(w, d), stdMat({ map: ct, bumpScale: 0.008 }));
    ce.rotation.x = Math.PI / 2; ce.position.set(cx, H, cz); addSolid(ce);
    for (const [lx, lz] of r.lights || [[cx, cz]]) {
      const l = new THREE.PointLight(r.light, 1.4, 24, 1.3);
      l.position.set(lx, H - 0.5, lz); world.add(l);
      const fix = new THREE.Mesh(new THREE.BoxGeometry(1.0, 0.08, 0.3), new THREE.MeshBasicMaterial({ color: r.light }));
      fix.position.set(lx, H - 0.05, lz); world.add(fix);
      if (r.flicker) flickerLights.push(l);
    }
  }

  // Ventanas (antes de las paredes para dejar el hueco)
  for (const [room, x, z] of map.windows) {
    const p = wallPlace(room, x, z);
    windows.push({ room, x, z, nx: p.nx, nz: p.nz, boards: 5, boardMeshes: [], climber: null, repairT: 0,
      outside: { x: x - p.nx * 1.1, z: z - p.nz * 1.1 }, inside: { x: x + p.nx * 1.0, z: z + p.nz * 1.0 } });
  }
  buildWalls(theme);
  for (const w of windows) buildWindow(w, theme);

  // Puertas
  for (const l of links) buildLink(l, theme);

  // Decoración y obstáculos
  for (const [x, z, w, d, h, type] of map.props || []) prop(x, z, w, d, h, type || 'crate');
  for (const [x1, z1, x2, z2] of map.pipes || []) {
    const len = Math.hypot(x2 - x1, z2 - z1);
    const pipe = new THREE.Mesh(new THREE.CylinderGeometry(0.15, 0.15, len, 10), stdMat({ color: 0x55585c, metalness: 0.5, roughness: 0.5 }));
    pipe.rotation.z = Math.PI / 2; pipe.rotation.y = -Math.atan2(z2 - z1, x2 - x1);
    pipe.position.set((x1 + x2) / 2, H - 0.35, (z1 + z2) / 2);
    world.add(pipe);
  }

  for (const [room, id, x, z] of map.wallBuys) makeWallBuy(room, id, x, z);
  for (const [room, key2, x, z] of map.perks) makePerk(room, key2, x, z);
  buildBox();
  buildPap();
  recomputeUnlocked();
}

function prop(x, z, w, d, h, type) {
  let m;
  if (type === 'barrel') {
    m = new THREE.Mesh(new THREE.CylinderGeometry(w / 2, w / 2, h, 14), stdMat({ map: TEX.barrel, roughness: 0.6, metalness: 0.3 }));
  } else {
    m = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), stdMat({ map: type === 'metal' ? TEX.metal : TEX.crate, metalness: type === 'metal' ? 0.4 : 0, roughness: type === 'metal' ? 0.6 : 0.9 }));
  }
  m.position.set(x, h / 2, z);
  addSolid(m, { minX: x - w / 2, maxX: x + w / 2, minZ: z - d / 2, maxZ: z + d / 2 });
}

function buildLink(l, theme) {
  const vertical = l.axis === 'z';
  // dintel sobre el hueco
  const lt = theme.wall.clone(); lt.needsUpdate = true; lt.repeat.set(0.75, 0.25);
  const lintel = new THREE.Mesh(new THREE.BoxGeometry(vertical ? 0.4 : 3, H - 3, vertical ? 3 : 0.4), stdMat({ map: lt }));
  lintel.position.set(l.x, (3 + H) / 2, l.z);
  addSolid(lintel);
  if (l.open) return;
  const w = vertical ? 0.3 : 3, d = vertical ? 3 : 0.3;
  const mesh = new THREE.Mesh(new THREE.BoxGeometry(w, 3, d), stdMat({ map: TEX.door }));
  mesh.position.set(l.x, 1.5, l.z);
  const col = { minX: l.x - w / 2, maxX: l.x + w / 2, minZ: l.z - d / 2, maxZ: l.z + d / 2 };
  addSolid(mesh, col);
  const labels = [];
  for (const s of [-1, 1]) {
    const lb = makeLabel(`PUERTA · ${l.cost}`, '#ffd84a', 2.2);
    lb.position.set(l.x + (vertical ? s * 0.4 : 0), 2.4, l.z + (vertical ? 0 : s * 0.4));
    world.add(lb); labels.push(lb);
  }
  const door = { link: l, mesh, col, labels, sinking: false };
  doors.push(door);
  interactables.push({
    x: l.x, z: l.z, range: 2.6,
    active: () => !l.open,
    text: () => `Presiona E para abrir la puerta [${l.cost}]`,
    action: () => {
      if (!spend(l.cost)) return;
      l.open = true;
      removeSolid(mesh, col);
      labels.forEach((lb) => world.remove(lb));
      door.sinking = true;
      recomputeUnlocked();
      play('door');
    },
  });
}

function buildWindow(win, theme) {
  const vertical = !!win.nx;
  const sx = vertical ? 0.4 : 1.6, sz = vertical ? 1.6 : 0.4;
  const t1 = theme.wall.clone(); t1.needsUpdate = true; t1.repeat.set(0.4, 0.25);
  const sill = new THREE.Mesh(new THREE.BoxGeometry(sx, 0.9, sz), stdMat({ map: t1 }));
  sill.position.set(win.x, 0.45, win.z);
  addSolid(sill, { minX: win.x - sx / 2, maxX: win.x + sx / 2, minZ: win.z - sz / 2, maxZ: win.z + sz / 2 });
  const lintel = new THREE.Mesh(new THREE.BoxGeometry(sx, H - 2.4, sz), stdMat({ map: t1 }));
  lintel.position.set(win.x, (2.4 + H) / 2, win.z);
  addSolid(lintel);
  const boardMat = stdMat({ map: TEX.door });
  for (let i = 0; i < 5; i++) {
    const b = new THREE.Mesh(new THREE.BoxGeometry(vertical ? 0.06 : 1.9, 0.2, vertical ? 1.9 : 0.06), boardMat);
    b.position.set(win.x + win.nx * 0.26, 1.05 + i * 0.28, win.z + win.nz * 0.26);
    if (vertical) b.rotation.x = rand(-0.3, 0.3); else b.rotation.z = rand(-0.3, 0.3);
    b.userData.basePos = b.position.clone();
    b.userData.baseRot = b.rotation.clone();
    world.add(b);
    win.boardMeshes.push(b);
  }
  interactables.push({
    x: win.inside.x, z: win.inside.z, range: 1.7, hold: true,
    active: () => win.boards < 5,
    text: () => 'Mantén E para reparar la ventana',
    holdAction: () => { if (T >= win.repairT) { win.repairT = T + 0.55; repairBoard(win); } },
  });
}

function tearBoard(win) {
  if (win.boards <= 0) return;
  win.boards--;
  const b = win.boardMeshes[win.boards];
  b.userData.anim = { t: 0, v: new V3(-win.nx * 2.5 + rand(-0.5, 0.5), 1.5, -win.nz * 2.5 + rand(-0.5, 0.5)) };
  const d = Math.hypot(win.x - player.pos.x, win.z - player.pos.z);
  if (d < 20) play('board', 1 - d / 20);
}
function repairBoard(win) {
  if (win.boards >= 5) return;
  const b = win.boardMeshes[win.boards];
  b.userData.anim = null;
  b.position.copy(b.userData.basePos);
  b.rotation.copy(b.userData.baseRot);
  b.visible = true;
  win.boards++;
  play('repair');
  if (game.repairPts < 500) { game.repairPts += 10; addPoints(10); }
}
function updateWindows(dt) {
  for (const w of windows) for (const b of w.boardMeshes) {
    const a = b.userData.anim;
    if (!a) continue;
    a.t += dt;
    a.v.y -= 9.8 * dt;
    b.position.addScaledVector(a.v, dt);
    b.rotation.y += dt * 4;
    if (a.t > 0.8) { b.visible = false; b.userData.anim = null; }
  }
}

// ---------- Armas de pared ----------
function chalkTexture(def) {
  const c = document.createElement('canvas'); c.width = 512; c.height = 256;
  const g = c.getContext('2d');
  g.fillStyle = 'rgba(14,14,14,0.9)'; g.fillRect(0, 0, 512, 256);
  g.strokeStyle = 'rgba(230,230,230,0.85)'; g.lineWidth = 4; g.strokeRect(8, 8, 496, 240);
  g.lineWidth = 5;
  g.strokeRect(130, 50, 240, 38); g.strokeRect(370, 60, 80, 14);
  g.strokeRect(180, 88, 28, 55); g.strokeRect(70, 55, 60, 30);
  g.fillStyle = '#eee'; g.font = 'bold 40px Oswald, Arial'; g.textAlign = 'center';
  g.fillText(def.name, 256, 190);
  g.fillStyle = '#ffd84a'; g.font = '34px Oswald, Arial';
  g.fillText(def.cost, 256, 234);
  return new THREE.CanvasTexture(c);
}
function makeWallBuy(room, id, x, z) {
  const def = WEAPONS[id];
  const p = wallPlace(room, x, z);
  const board = new THREE.Mesh(new THREE.PlaneGeometry(1.8, 0.9), new THREE.MeshLambertMaterial({ map: chalkTexture(def), transparent: true }));
  board.position.set(x + p.nx * 0.215, 1.7, z + p.nz * 0.215);
  board.rotation.y = p.ry;
  world.add(board);
  mmIcons.push({ x, z, color: '#ffd84a', r: 0.5 });
  interactables.push({
    x: x + p.nx * 1.0, z: z + p.nz * 1.0, range: 1.8,
    active: () => !player.papBusy,
    text: () => {
      const owned = player.weapons.find((w) => w.id === id);
      if (!owned) return `Presiona E para comprar ${def.name} [${def.cost}]`;
      return `Presiona E para comprar munición [${owned.pap ? 2500 : def.cost / 2}]`;
    },
    action: () => {
      const owned = player.weapons.find((w) => w.id === id);
      if (owned) {
        const s = stats(owned);
        if (owned.reserve >= s.reserve) return;
        if (spend(owned.pap ? 2500 : def.cost / 2)) owned.reserve = s.reserve;
      } else if (spend(def.cost)) giveWeapon(id);
    },
  });
}

// ---------- Bebidas (perks) ----------
const PERKS = {
  revive: { name: 'QUICK REVIVE', letter: 'Q', color: 0x2a8cff, label: '#7cc4ff', cost: 500,  desc: 'Revives una vez' },
  jugg:   { name: 'JUGGERNOG',    letter: 'J', color: 0xb01818, label: '#ff5a5a', cost: 2500, desc: 'Más vida' },
  speed:  { name: 'SPEED COLA',   letter: 'S', color: 0x18a030, label: '#5aff7a', cost: 3000, desc: 'Recarga rápida' },
  double: { name: 'DOUBLE TAP',   letter: 'D', color: 0xd08a10, label: '#ffc04a', cost: 2000, desc: 'Disparo más rápido' },
  stamin: { name: 'POLA VELOZ',   letter: 'P', color: 0xe0b020, label: '#ffe066', cost: 2000, desc: 'Corres mucho más rápido' },
};
function makePerk(room, key, x, z) {
  const pk = PERKS[key];
  const p = wallPlace(room, x, z);
  const cx = x + p.nx * 0.65, cz = z + p.nz * 0.65;
  const body = new THREE.Mesh(new THREE.BoxGeometry(1.1, 2.2, 0.8), stdMat({ color: pk.color, emissive: pk.color, emissiveIntensity: 0.25, roughness: 0.5, metalness: 0.2 }));
  body.position.set(cx, 1.1, cz); body.rotation.y = p.ry;
  const sign = new THREE.Mesh(new THREE.BoxGeometry(0.9, 0.35, 0.05), new THREE.MeshBasicMaterial({ color: 0xffffff }));
  sign.position.set(0, 0.7, 0.42); body.add(sign);
  const slot = new THREE.Mesh(new THREE.BoxGeometry(0.5, 0.4, 0.05), new THREE.MeshBasicMaterial({ color: 0x111111 }));
  slot.position.set(0, -0.5, 0.42); body.add(slot);
  addSolid(body, { minX: cx - 0.55, maxX: cx + 0.55, minZ: cz - 0.55, maxZ: cz + 0.55 });
  const lb = makeLabel(`${pk.name} · ${pk.cost}`, pk.label, 2.4);
  lb.position.set(cx, 2.6, cz); world.add(lb);
  mmIcons.push({ x: cx, z: cz, color: hex(pk.color), r: 0.7 });
  interactables.push({
    x: cx + p.nx * 1.0, z: cz + p.nz * 1.0, range: 1.8,
    active: () => !player.perks.has(key),
    text: () => `Presiona E para beber ${pk.name} (${pk.desc}) [${pk.cost}]`,
    action: () => {
      if (player.perks.has(key) || !spend(pk.cost)) return;
      player.perks.add(key);
      if (key === 'jugg') { player.maxHealth = 250; player.health = 250; }
      play('perk');
      centerMsg(pk.name, pk.label);
      renderPerks();
    },
  });
}

// ---------- Caja misteriosa ----------
const box = { spotIdx: 0, state: 'idle', t: 0, swapT: 0, offer: null, uses: 0, group: null, lid: null, beam: null, models: {}, teddy: null, col: null, front: { x: 0, z: 0 }, pos: { x: 0, z: 0 } };
function buildBox() {
  const g = new THREE.Group();
  const mat = stdMat({ map: TEX.box, emissive: 0x112233 });
  const body = new THREE.Mesh(new THREE.BoxGeometry(1.8, 0.75, 0.8), mat);
  body.position.y = 0.375; g.add(body); solids.push(body);
  const lid = new THREE.Group(); lid.position.set(0, 0.75, -0.4); g.add(lid);
  const lidMesh = new THREE.Mesh(new THREE.BoxGeometry(1.84, 0.1, 0.84), mat);
  lidMesh.position.set(0, 0.05, 0.4); lid.add(lidMesh);
  const lb = makeLabel('CAJA MISTERIOSA · 950', '#7fd4ff', 2.6);
  lb.position.y = 2.2; g.add(lb);
  box.models = {};
  for (const id of new Set(BOX_POOL)) {
    const gm = makeGunModel(id);
    gm.scale.setScalar(WEAPONS[id].len > 0.7 ? 1.5 : 2.2); gm.rotation.y = Math.PI / 2;
    gm.position.set(0.5, 0.6, 0); gm.visible = false;
    g.add(gm); box.models[id] = gm;
  }
  // Osito: si sale, la caja se va a otro sitio
  const teddy = new THREE.Group();
  const fur = new THREE.MeshLambertMaterial({ color: 0x8a5a2b });
  const ball = (r, x, y, z) => { const m = new THREE.Mesh(new THREE.SphereGeometry(r, 12, 10), fur); m.position.set(x, y, z); teddy.add(m); };
  ball(0.22, 0, 0, 0); ball(0.16, 0, 0.3, 0); ball(0.06, -0.12, 0.44, 0); ball(0.06, 0.12, 0.44, 0);
  ball(0.08, -0.22, 0.05, 0.05); ball(0.08, 0.22, 0.05, 0.05); ball(0.09, -0.1, -0.22, 0.05); ball(0.09, 0.1, -0.22, 0.05);
  teddy.visible = false; g.add(teddy);
  box.teddy = teddy;
  box.group = g; box.lid = lid;
  box.beam = new THREE.Mesh(
    new THREE.CylinderGeometry(0.45, 0.45, H, 16, 1, true),
    new THREE.MeshBasicMaterial({ color: 0x66aaff, transparent: true, opacity: 0.16, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide })
  );
  world.add(g, box.beam);
  box.state = 'idle'; box.uses = 0; box.col = null;
  placeBox(0);
  interactables.push({
    pos: () => box.front, range: 2.0,
    active: () => (box.state === 'idle' || box.state === 'offer') && !player.papBusy,
    text: () => box.state === 'offer'
      ? `Presiona E para tomar ${WEAPONS[box.offer].name}`
      : 'Presiona E para la Caja Misteriosa [950]',
    action: () => {
      if (box.state === 'idle') {
        if (!spend(950)) return;
        box.uses++;
        box.state = 'rolling'; box.t = 0; box.swapT = 0;
        play('box');
      } else if (box.state === 'offer') {
        giveWeapon(box.offer);
        box.state = 'closing'; box.t = 0;
        showBoxModel(null);
      }
    },
  });
}
function placeBox(idx) {
  const [room, x, z] = map.boxSpots[idx];
  const p = wallPlace(room, x, z);
  const cx = x + p.nx * 0.6, cz = z + p.nz * 0.6;
  box.group.position.set(cx, 0, cz);
  box.group.rotation.y = p.ry;
  box.beam.position.set(cx, H / 2, cz);
  if (box.col) { const j = colliders.indexOf(box.col); if (j >= 0) colliders.splice(j, 1); }
  const hx = p.nx ? 0.4 : 0.9, hz = p.nx ? 0.9 : 0.4;
  box.col = { minX: cx - hx, maxX: cx + hx, minZ: cz - hz, maxZ: cz + hz };
  colliders.push(box.col);
  box.front = { x: cx + p.nx * 1.2, z: cz + p.nz * 1.2 };
  box.pos = { x: cx, z: cz };
  box.spotIdx = idx;
}
function showBoxModel(id) { for (const k in box.models) box.models[k].visible = k === id; }
function setBoxModelsY(y) { for (const k in box.models) box.models[k].position.y = y; }
function updateBox(dt) {
  const b = box;
  const open = b.state === 'rolling' || b.state === 'offer' || b.state === 'teddy';
  b.lid.rotation.x += ((open ? -1.2 : 0) - b.lid.rotation.x) * damp(8, dt);
  if (b.state === 'rolling') {
    b.t += dt; b.swapT -= dt;
    if (b.swapT <= 0) { showBoxModel(pick(BOX_POOL)); b.swapT = 0.06 + b.t * 0.05; }
    setBoxModelsY(0.5 + Math.min(b.t / 4, 1) * 0.9);
    if (b.t > 4) {
      if (b.uses >= 4 && Math.random() < 0.2 && map.boxSpots.length > 1) {
        b.state = 'teddy'; b.t = 0;
        showBoxModel(null);
        b.teddy.visible = true; b.teddy.position.set(0, 1.3, 0);
        play('laugh');
      } else {
        const owned = player.weapons.map((w) => w.id);
        const options = BOX_POOL.filter((id) => !owned.includes(id));
        b.offer = pick(options.length ? options : BOX_POOL);
        showBoxModel(b.offer);
        b.state = 'offer'; b.t = 0;
      }
    }
  } else if (b.state === 'offer') {
    b.t += dt;
    setBoxModelsY(1.4 - b.t * 0.08);
    if (b.t > 10) { showBoxModel(null); b.state = 'closing'; b.t = 0; }
  } else if (b.state === 'closing') {
    b.t += dt;
    if (b.t > 0.5) b.state = 'idle';
  } else if (b.state === 'teddy') {
    b.t += dt;
    b.teddy.rotation.y += dt * 3;
    if (b.t > 2.5) {
      b.teddy.visible = false;
      b.state = 'leaving'; b.t = 0;
      centerMsg('¡LA CAJA SE ESTÁ MOVIENDO!', '#7fd4ff');
    }
  } else if (b.state === 'leaving') {
    b.t += dt;
    b.group.position.y = b.t * b.t * 1.5;
    b.group.rotation.y += dt * b.t * 4;
    b.beam.material.opacity = 0.16 * Math.max(0, 1 - b.t / 3);
    if (b.t > 3) {
      let idx;
      do idx = Math.floor(Math.random() * map.boxSpots.length); while (idx === b.spotIdx);
      placeBox(idx);
      b.group.position.y = 0;
      b.beam.material.opacity = 0.16;
      player.points += 950;
      centerMsg('Se te devolvieron 950 puntos', '#ffd84a');
      b.state = 'idle';
    }
  }
}

// ---------- Pack-a-Punch ----------
const pap = { state: 'idle', t: 0, weaponIdx: 0, rollers: [], glow: null, front: { x: 0, z: 0 } };
function buildPap() {
  const [room, x, z] = map.pap;
  const p = wallPlace(room, x, z);
  const cx = x + p.nx * 0.85, cz = z + p.nz * 0.85;
  const g = new THREE.Group();
  g.position.set(cx, 0, cz); g.rotation.y = p.ry;
  const metal = stdMat({ map: TEX.metal, metalness: 0.5, roughness: 0.5 });
  const base = new THREE.Mesh(new THREE.BoxGeometry(1.7, 1.2, 1.2), metal);
  base.position.y = 0.6; g.add(base); solids.push(base);
  const front = new THREE.Mesh(new THREE.PlaneGeometry(1.0, 0.9), new THREE.MeshBasicMaterial({ map: TEX.pap }));
  front.position.set(0, 0.6, 0.61); g.add(front);
  const top = new THREE.Mesh(new THREE.BoxGeometry(1.5, 0.6, 1.0), stdMat({ color: 0x2a1a3a, emissive: 0x4a1a7a, emissiveIntensity: 0.5 }));
  top.position.y = 1.5; g.add(top);
  pap.glow = top.material;
  pap.rollers = [];
  for (const rz of [-0.25, 0.25]) {
    const r = new THREE.Mesh(new THREE.CylinderGeometry(0.12, 0.12, 1.3, 12), stdMat({ color: 0x777777, metalness: 0.7, roughness: 0.3 }));
    r.rotation.z = Math.PI / 2; r.position.set(0, 1.9, rz); g.add(r); pap.rollers.push(r);
  }
  const lb = makeLabel('PACK-A-PUNCH · 5000', '#d08cff', 2.6);
  lb.position.y = 2.6; g.add(lb);
  world.add(g);
  const hx = p.nx ? 0.6 : 0.85, hz = p.nx ? 0.85 : 0.6;
  colliders.push({ minX: cx - hx, maxX: cx + hx, minZ: cz - hz, maxZ: cz + hz });
  pap.front = { x: cx + p.nx * 1.4, z: cz + p.nz * 1.4 };
  pap.state = 'idle';
  mmIcons.push({ x: cx, z: cz, color: '#b04aff', r: 0.9 });
  interactables.push({
    pos: () => pap.front, range: 2.0,
    active: () => pap.state === 'idle',
    text: () => curWeapon() && curWeapon().pap ? 'Tu arma ya está mejorada' : 'Presiona E para mejorar tu arma con Pack-a-Punch [5000]',
    action: () => {
      const w = curWeapon();
      if (!w || w.pap || player.papBusy || !spend(5000)) return;
      pap.state = 'working'; pap.t = 0; pap.weaponIdx = player.cur;
      player.papBusy = true; player.reloading = false;
      play('pap');
    },
  });
}
function updatePap(dt) {
  if (pap.state !== 'working') { if (pap.glow) pap.glow.emissiveIntensity = 0.5 + Math.sin(T * 2) * 0.2; return; }
  pap.t += dt;
  for (const r of pap.rollers) r.rotation.x += dt * 12;
  pap.glow.emissiveIntensity = 0.6 + Math.random() * 0.8;
  if (pap.t > 3.5) {
    const w = player.weapons[pap.weaponIdx];
    w.pap = true;
    const s = stats(w);
    w.mag = s.mag; w.reserve = s.reserve;
    player.papBusy = false;
    player.cur = pap.weaponIdx;
    equip();
    centerMsg(s.name, '#d08cff');
    play('perk');
    pap.state = 'idle';
  }
}

// ---------- Estado del juego ----------
let T = 0; // tiempo de juego (se detiene en pausa)
const game = { state: 'menu', round: 0, toSpawn: 0, spawnT: 0, between: true, betweenT: 3, zHP: 150, insta: 0, double: 0, drops: 0, kills: 0, heads: 0, repairPts: 0, noLock: false };
const keys = {};
const mouse = { down: false, right: false, pressedAt: -9 };
const player = {
  pos: new V3(0, 0, 3), vel: new V3(), vy: 0, onGround: true, yaw: 0, pitch: 0, eye: 1.65, room: 'A',
  health: 100, maxHealth: 100, lastHit: -99, points: CONFIG.puntosIniciales,
  weapons: [], cur: 0, reloading: false, reloadEnd: 0, reloadTime: 1, nextShot: 0,
  perks: new Set(), moving: false, sprinting: false, crouch: false,
  grenades: 2, nextGrenade: 0, papBusy: false, nextHeart: 0,
};
const curWeapon = () => player.weapons[player.cur];

// ---------- Arma en primera persona ----------
const gunHolder = new THREE.Group();
gunScene.add(gunHolder);
let gunModel = null;
const flash = new THREE.Sprite(new THREE.SpriteMaterial({ map: TEX.flash, blending: THREE.AdditiveBlending, depthWrite: false, transparent: true }));
flash.scale.set(0.2, 0.2, 1); flash.visible = false;
gunHolder.add(flash);
const vm = { kick: 0, swap: 0, ads: 0, bobT: 0, bobAmt: 0, flash: 0, shake: 0, sprint: 0, land: 0, roll: 0, recoil: 0, swayX: 0, swayY: 0, swayTX: 0, swayTY: 0, throwT: 0, fov: 75 };

function equip() {
  if (gunModel) gunHolder.remove(gunModel);
  const w = curWeapon();
  gunModel = makeGunModel(w.id, w.pap, true);
  gunModel.scale.setScalar(0.75);
  gunHolder.add(gunModel);
  flash.position.set(0, 0.015, gunModel.userData.muzzleZ * 0.75 - 0.04);
  vm.swap = 1;
  player.reloading = false;
  player.nextShot = Math.max(player.nextShot, T + (player.perks.has('speed') ? 0.18 : 0.35));
}
function giveWeapon(id) {
  const def = WEAPONS[id];
  const w = { id, mag: def.mag, reserve: def.reserve, pap: false };
  const idx = player.weapons.findIndex((x) => x.id === id);
  if (idx >= 0) { player.weapons[idx] = w; player.cur = idx; }
  else if (player.weapons.length < 2) { player.weapons.push(w); player.cur = player.weapons.length - 1; }
  else player.weapons[player.cur] = w;
  equip();
}
function switchWeapon(dir) {
  const n = player.weapons.length;
  if (n < 2 || player.papBusy) return;
  player.cur = (player.cur + dir + n) % n;
  equip();
}
function startReload() {
  const w = curWeapon(), s = stats(w);
  if (player.reloading || player.papBusy || w.mag >= s.mag || w.reserve <= 0) return;
  player.reloading = true;
  player.reloadTime = s.reload * (player.perks.has('speed') ? 0.5 : 1);
  player.reloadEnd = T + player.reloadTime;
  play('reload');
}

// ---------- Disparo ----------
const raycaster = new THREE.Raycaster();
const shotDir = new V3();
const tracers = [];

function fire() {
  const w = curWeapon(), s = stats(w);
  const dt2 = player.perks.has('double');
  w.mag--;
  player.nextShot = T + s.rate * (dt2 ? 0.7 : 1);
  play(s.sound);
  vm.kick = Math.min(1, vm.kick + (s.pellets > 1 || s.id === 'sniper' ? 1 : 0.45));
  vm.flash = 0.05;
  if (s.projectile) vm.shake = Math.max(vm.shake, s.projectile === 'rocket' ? 0.25 : 0.12);
  const green = s.id === 'raygun' || s.id === 'wunder';
  flash.material.color.set(s.id === 'wunder' ? 0x88ccff : green ? 0x66ff66 : 0xffffff);
  muzzleLight.color.set(s.id === 'wunder' ? 0x88ccff : green ? 0x55ff55 : 0xffaa55);
  flash.material.rotation = Math.random() * Math.PI;
  vm.recoil += s.recoil * (vm.ads > 0.5 ? 0.5 : 1);
  player.yaw += rand(-0.3, 0.3) * s.recoil;

  if (s.projectile) {
    launchProjectile(s, dt2 ? 1.25 : 1);
    if (w.mag <= 0 && w.reserve > 0) startReload();
    return;
  }

  const targets = solids.slice();
  for (const z of zombies) if (!z.dead) targets.push(...z.parts);
  const spread = s.spread * (vm.ads > 0.5 ? 0.35 : 1) * (player.moving ? 1.4 : 1) * (player.crouch ? 0.7 : 1);
  const muzzle = camera.localToWorld(new V3(0.15, -0.15, -0.6));
  const dmgMul = dt2 ? 1.25 : 1;
  for (let i = 0; i < s.pellets; i++) {
    camera.getWorldDirection(shotDir);
    shotDir.x += rand(-spread, spread); shotDir.y += rand(-spread, spread); shotDir.z += rand(-spread, spread);
    shotDir.normalize();
    raycaster.set(camera.position, shotDir);
    raycaster.far = 150;
    const hit = raycaster.intersectObjects(targets, false)[0];
    if (!hit) continue;
    const z = hit.object.userData.zombie;
    if (z) {
      if (s.chain) chainLightning(z, s.chain);
      else damageZombie(z, s.dmg * dmgMul, s.head, !!hit.object.userData.head, hit.point, green);
    } else {
      burst(hit.point, green ? 0x66ff66 : 0xffd27a, 3, 2);
      if (hit.face && !green) addDecal('hole', hit.point, hit.face.normal.clone().transformDirection(hit.object.matrixWorld));
    }
    if (s.splash) {
      burst(hit.point, 0x66ff66, 10, 3);
      for (const o of zombies) {
        if (o.dead || o === z) continue;
        if (Math.hypot(o.pos.x - hit.point.x, o.pos.z - hit.point.z) < s.splash) damageZombie(o, s.dmg * 0.5 * dmgMul, 1, false, hit.point, true);
      }
    }
    if (s.tracer) addTracer([muzzle, hit.point], s.tracer, 0.15);
    else if (s.chain) addTracer(jagged(muzzle, hit.point), 0x99ddff, 0.25);
  }
  if (w.mag <= 0 && w.reserve > 0) startReload();
}

function jagged(a, b) {
  const pts = [a.clone()];
  for (let i = 1; i < 8; i++) {
    const p = a.clone().lerp(b, i / 8);
    p.x += rand(-0.25, 0.25); p.y += rand(-0.25, 0.25); p.z += rand(-0.25, 0.25);
    pts.push(p);
  }
  pts.push(b.clone());
  return pts;
}
function addTracer(points, color, life) {
  const geo = new THREE.BufferGeometry().setFromPoints(points);
  const line = new THREE.Line(geo, new THREE.LineBasicMaterial({ color, transparent: true, fog: false }));
  scene.add(line);
  tracers.push({ line, t: life, life });
}
function chainLightning(first, count) {
  let cur = first;
  const hit = new Set();
  for (let i = 0; i <= count && cur; i++) {
    hit.add(cur);
    const from = cur.pos.clone().setY(1.3);
    killZombie(cur, false);
    burst(from, 0x99ddff, 8, 3);
    let next = null, nd = 7;
    for (const o of zombies) {
      if (o.dead || hit.has(o)) continue;
      const d = o.pos.distanceTo(cur.pos);
      if (d < nd) { nd = d; next = o; }
    }
    if (next) addTracer(jagged(from, next.pos.clone().setY(1.3)), 0x99ddff, 0.3);
    cur = next;
  }
  showHit(true);
}

function damageZombie(z, dmg, headMul, head, point, green) {
  if (z.dead) return;
  if (head) dmg *= headMul;
  if (T < game.insta) dmg = 1e9;
  z.hp -= dmg;
  burst(point, green ? 0x44ff44 : 0x7a0000, head ? 8 : 4, 2.5);
  if (z.hp <= 0) { killZombie(z, head); showHit(true); }
  else { addPoints(10); showHit(false); play('hit'); }
}
function killZombie(z, head, fromNuke = false) {
  if (z.dead) return;
  z.die(head);
  if (fromNuke) return;
  game.kills++;
  if (head) game.heads++;
  addPoints(head ? 100 : 60);
  if (z.state === 'chase' && game.drops < 4 && Math.random() < 0.035) { dropPowerup(z.pos.x, z.pos.z); game.drops++; }
}

// ---------- Granadas ----------
const grenades = [];
const grenadeGeo = new THREE.SphereGeometry(0.08, 10, 8);
const grenadeMat = new THREE.MeshLambertMaterial({ color: 0x3a4a2a });
const booms = [];
function throwGrenade() {
  if (player.grenades <= 0 || T < player.nextGrenade) return;
  player.grenades--;
  player.nextGrenade = T + 0.8;
  const dir = new V3(); camera.getWorldDirection(dir);
  const m = new THREE.Mesh(grenadeGeo, grenadeMat);
  m.position.copy(camera.position).addScaledVector(dir, 0.5);
  scene.add(m);
  const v = dir.multiplyScalar(15);
  v.y += 3.5; v.x += player.vel.x; v.z += player.vel.z;
  grenades.push({ m, v, t: 0 });
  vm.throwT = 0.4;
  play('pin');
}
function insideCollider(x, z, r) {
  for (const c of colliders) if (x > c.minX - r && x < c.maxX + r && z > c.minZ - r && z < c.maxZ + r) return true;
  return false;
}
function updateGrenades(dt) {
  for (let i = grenades.length - 1; i >= 0; i--) {
    const g = grenades[i];
    g.t += dt;
    g.v.y -= 18 * dt;
    const p = g.m.position;
    const nx = p.x + g.v.x * dt;
    if (insideCollider(nx, p.z, 0.08)) g.v.x *= -0.5; else p.x = nx;
    const nz = p.z + g.v.z * dt;
    if (insideCollider(p.x, nz, 0.08)) g.v.z *= -0.5; else p.z = nz;
    p.y += g.v.y * dt;
    if (p.y < 0.08) { p.y = 0.08; g.v.y = Math.abs(g.v.y) > 1.5 ? -g.v.y * 0.4 : 0; g.v.x *= 0.7; g.v.z *= 0.7; }
    if (p.y > H - 0.1) { p.y = H - 0.1; g.v.y = -Math.abs(g.v.y) * 0.4; }
    g.m.rotation.x += dt * 10;
    if (g.t > 2.2) { explode(p.clone()); scene.remove(g.m); grenades.splice(i, 1); }
  }
  for (let i = booms.length - 1; i >= 0; i--) {
    const b = booms[i];
    b.t += dt;
    const k = b.t / 0.4;
    b.m.scale.setScalar((0.5 + k * 4.5) * (b.size || 1));
    b.m.material.opacity = Math.max(0, 0.9 * (1 - k));
    if (k >= 1) { scene.remove(b.m); b.m.material.dispose(); booms.splice(i, 1); }
  }
  boomLight.intensity = Math.max(0, boomLight.intensity - dt * 12);
}
// dmg: daño en el centro · radius: alcance · selfDmg: daño máximo al jugador si está cerca
function explode(p, dmg = 400 + game.round * 80, radius = 6, selfDmg = 30, size = 1) {
  play('boom');
  boomLight.position.copy(p).setY(1.5);
  boomLight.intensity = 5 * size;
  const ball = new THREE.Mesh(new THREE.SphereGeometry(1, 16, 12), new THREE.MeshBasicMaterial({ color: 0xff8a2a, transparent: true, opacity: 0.9, blending: THREE.AdditiveBlending, depthWrite: false, fog: false }));
  ball.position.copy(p);
  scene.add(ball);
  booms.push({ m: ball, t: 0, size });
  burst(p, 0xffaa33, Math.round(25 * size), 6 * size);
  burst(p, 0x444444, Math.round(15 * size), 3);
  addDecal('scorch', new V3(p.x, 0.012, p.z), new V3(0, 1, 0));
  for (const z of zombies) {
    if (z.dead) continue;
    const d = Math.hypot(z.pos.x - p.x, z.pos.z - p.z);
    if (d < radius) damageZombie(z, dmg * (1 - d / (radius * 1.08)), 1, false, z.pos.clone().setY(1.2), false);
  }
  const pd = Math.hypot(player.pos.x - p.x, player.pos.z - p.z);
  vm.shake = Math.max(vm.shake, Math.max(0, 0.5 * size - pd * 0.04));
  if (pd < 4 && selfDmg > 0) damagePlayer(Math.round(selfDmg * (1 - pd / 4)));
}

// ---------- Proyectiles (lanzagranadas y lanzamisiles) ----------
const projectiles = [];
const rocketGeo = new THREE.CylinderGeometry(0.001, 0.07, 0.25, 10).rotateX(-Math.PI / 2);
const rocketMat = new THREE.MeshLambertMaterial({ color: 0x5a6a3a, emissive: 0x221100 });
const shellGeo = new THREE.CylinderGeometry(0.06, 0.06, 0.14, 10).rotateX(Math.PI / 2);
const shellMat = new THREE.MeshLambertMaterial({ color: 0x8a7a30 });
const smokeMat = new THREE.SpriteMaterial({ map: TEX.flash, color: 0x999999, transparent: true, depthWrite: false, opacity: 0.6 });
const smoke = [];

function launchProjectile(s, dmgMul) {
  const rocket = s.projectile === 'rocket';
  const dir = new V3(); camera.getWorldDirection(dir);
  const spread = vm.ads > 0.5 ? 0 : 0.01;
  dir.x += rand(-spread, spread); dir.y += rand(-spread, spread); dir.normalize();
  const m = new THREE.Mesh(rocket ? rocketGeo : shellGeo, rocket ? rocketMat : shellMat);
  m.position.copy(camera.position).addScaledVector(dir, 0.7);
  m.position.y -= 0.12;
  m.lookAt(m.position.clone().add(dir));
  scene.add(m);
  let glow = null;
  if (rocket) {
    glow = new THREE.Sprite(new THREE.SpriteMaterial({ map: TEX.flash, blending: THREE.AdditiveBlending, depthWrite: false, transparent: true }));
    glow.scale.set(0.5, 0.5, 1); m.add(glow); glow.position.z = 0.18;
  }
  projectiles.push({
    m, glow, rocket, t: 0, smokeT: 0,
    v: dir.multiplyScalar(rocket ? 34 : 26),
    dmg: s.dmg * dmgMul * (1 + game.round * 0.1),
    radius: s.radius,
  });
}

function updateProjectiles(dt) {
  for (let i = projectiles.length - 1; i >= 0; i--) {
    const pr = projectiles[i];
    pr.t += dt;
    if (!pr.rocket) pr.v.y -= 9 * dt;
    const p = pr.m.position;
    // varios pasos pequeños para que no atraviese a los zombis ni las paredes
    const steps = 3;
    let hit = pr.t > 5;
    for (let k = 0; k < steps && !hit; k++) {
      p.addScaledVector(pr.v, dt / steps);
      if (p.y < 0.05 || p.y > H - 0.05) hit = true;
      else if (p.y < 3 && insideCollider(p.x, p.z, 0.05)) hit = true;
      else for (const z of zombies) {
        if (z.dead || z.state === 'rise') continue;
        if (p.y < 2.0 && Math.hypot(z.pos.x - p.x, z.pos.z - p.z) < 0.5) { hit = true; break; }
      }
    }
    if (!pr.rocket) pr.m.lookAt(p.clone().add(pr.v));
    else {
      pr.glow.material.rotation = Math.random() * 6;
      pr.smokeT -= dt;
      if (pr.smokeT <= 0) {
        pr.smokeT = 0.02;
        const sp = new THREE.Sprite(smokeMat.clone());
        sp.position.copy(p); sp.scale.set(0.3, 0.3, 1);
        scene.add(sp);
        smoke.push({ sp, t: 0 });
      }
    }
    if (hit) {
      p.y = Math.max(0.2, Math.min(H - 0.3, p.y));
      explode(p.clone(), pr.dmg, pr.radius, 0, pr.rocket ? 1.5 : 1.1);
      scene.remove(pr.m);
      projectiles.splice(i, 1);
    }
  }
  for (let i = smoke.length - 1; i >= 0; i--) {
    const s = smoke[i];
    s.t += dt;
    s.sp.scale.setScalar(0.3 + s.t * 1.6);
    s.sp.material.opacity = Math.max(0, 0.5 * (1 - s.t / 0.9));
    s.sp.position.y += dt * 0.4;
    if (s.t > 0.9) { scene.remove(s.sp); s.sp.material.dispose(); smoke.splice(i, 1); }
  }
}

// ---------- Partículas y marcas ----------
const particles = [];
const pGeo = new THREE.BoxGeometry(0.05, 0.05, 0.05);
const pMats = {};
function burst(pos, color, n, speed) {
  const mat = pMats[color] || (pMats[color] = new THREE.MeshBasicMaterial({ color }));
  for (let i = 0; i < n; i++) {
    const m = new THREE.Mesh(pGeo, mat);
    m.position.copy(pos);
    scene.add(m);
    particles.push({ m, v: new V3(rand(-1, 1) * speed, rand(0, 1.5) * speed, rand(-1, 1) * speed), life: rand(0.3, 0.6) });
  }
}
const decalMats = {
  hole: new THREE.MeshBasicMaterial({ map: TEX.hole, transparent: true, depthWrite: false, polygonOffset: true, polygonOffsetFactor: -4 }),
  blood: new THREE.MeshBasicMaterial({ map: TEX.blood, transparent: true, depthWrite: false, polygonOffset: true, polygonOffsetFactor: -4 }),
  scorch: new THREE.MeshBasicMaterial({ map: TEX.scorch, transparent: true, depthWrite: false, polygonOffset: true, polygonOffsetFactor: -4 }),
};
const decalGeo = { hole: new THREE.PlaneGeometry(0.12, 0.12), blood: new THREE.PlaneGeometry(1.6, 1.6), scorch: new THREE.PlaneGeometry(3, 3) };
function addDecal(type, point, normal) {
  const m = new THREE.Mesh(decalGeo[type], decalMats[type]);
  m.position.copy(point).addScaledVector(normal, 0.01);
  m.lookAt(point.clone().add(normal));
  if (type === 'blood') m.rotateZ(Math.random() * 6.3);
  world.add(m);
  decals.push(m);
  if (decals.length > 90) world.remove(decals.shift());
}
function updateParticles(dt) {
  for (let i = particles.length - 1; i >= 0; i--) {
    const p = particles[i];
    p.life -= dt;
    p.v.y -= 9.8 * dt;
    p.m.position.addScaledVector(p.v, dt);
    if (p.m.position.y < 0.02) { p.m.position.y = 0.02; p.v.set(0, 0, 0); }
    if (p.life <= 0) { scene.remove(p.m); particles.splice(i, 1); }
  }
  for (let i = tracers.length - 1; i >= 0; i--) {
    const tr = tracers[i];
    tr.t -= dt;
    tr.line.material.opacity = Math.max(0, tr.t / tr.life);
    if (tr.t <= 0) { scene.remove(tr.line); tr.line.geometry.dispose(); tr.line.material.dispose(); tracers.splice(i, 1); }
  }
}

// ---------- Colisiones ----------
function collide(pos, r) {
  for (const c of colliders) {
    const cx = clamp(pos.x, c.minX, c.maxX), cz = clamp(pos.z, c.minZ, c.maxZ);
    const dx = pos.x - cx, dz = pos.z - cz;
    const d2 = dx * dx + dz * dz;
    if (d2 >= r * r) continue;
    if (d2 > 1e-8) {
      const d = Math.sqrt(d2);
      pos.x += dx / d * (r - d); pos.z += dz / d * (r - d);
    } else {
      const l = pos.x - c.minX, rr = c.maxX - pos.x, t = pos.z - c.minZ, b = c.maxZ - pos.z;
      const m = Math.min(l, rr, t, b);
      if (m === l) pos.x = c.minX - r; else if (m === rr) pos.x = c.maxX + r;
      else if (m === t) pos.z = c.minZ - r; else pos.z = c.maxZ + r;
    }
  }
}

// ---------- Zombis ----------
const zombies = [];
const zGeo = {
  leg: new THREE.BoxGeometry(0.22, 0.9, 0.22).translate(0, -0.45, 0),
  arm: new THREE.BoxGeometry(0.17, 0.72, 0.17).translate(0, -0.36, 0),
  hand: new THREE.BoxGeometry(0.15, 0.16, 0.13).translate(0, -0.8, 0),
  torso: new THREE.BoxGeometry(0.56, 0.72, 0.32),
  head: new THREE.BoxGeometry(0.4, 0.42, 0.4),
  eye: new THREE.BoxGeometry(0.08, 0.05, 0.02),
};
const SKINS = [0x7a9a5e, 0x8a9a70, 0x6b8a5a, 0x9aa080, 0x8f8a7a];
const SHIRTS = [0x5a4a3a, 0x3a4a5a, 0x6a2a2a, 0x4a4a4a, 0x55603a, 0x7a7a70];
const zMat = {
  skin: SKINS.map((c) => new THREE.MeshLambertMaterial({ color: c })),
  pants: [0x2b2b3a, 0x3a3428, 0x26303a].map((c) => new THREE.MeshLambertMaterial({ color: c })),
  hair: new THREE.MeshLambertMaterial({ color: 0x1a1410 }),
  eye: new THREE.MeshBasicMaterial({ color: 0xffc400, fog: false }),
  faces: {}, shirts: {},
};
function faceMat(si, style) {
  const key = si + '_' + style;
  if (zMat.faces[key]) return zMat.faces[key];
  const t = canvasTexture(64, 64, (g) => {
    g.fillStyle = hex(SKINS[si]); g.fillRect(0, 0, 64, 64);
    speckle(g, 64, 64, 250, 0.15);
    g.fillStyle = 'rgba(20,10,10,0.85)'; g.fillRect(12, 20, 14, 10); g.fillRect(38, 20, 14, 10);
    const mw = style === 1 ? 30 : 22, mh = style === 2 ? 14 : 9;
    g.fillStyle = '#1a0505'; g.fillRect(32 - mw / 2, 42, mw, mh);
    g.fillStyle = '#d8d0b0';
    for (let i = 0; i < 5; i++) g.fillRect(32 - mw / 2 + 2 + i * (mw / 5), 42, 3, 4);
    g.fillStyle = 'rgba(120,0,0,0.85)';
    for (let i = 0; i < 4; i++) g.fillRect(28 + rand(-9, 9), 46, 3, rand(8, 16));
    if (style === 2) { g.fillStyle = 'rgba(90,0,0,0.75)'; g.beginPath(); g.arc(50, 10, 9, 0, 7); g.fill(); }
  });
  return (zMat.faces[key] = new THREE.MeshLambertMaterial({ map: t }));
}
function shirtMat(ci) {
  if (zMat.shirts[ci]) return zMat.shirts[ci];
  const t = canvasTexture(64, 64, (g) => {
    g.fillStyle = hex(SHIRTS[ci]); g.fillRect(0, 0, 64, 64);
    speckle(g, 64, 64, 300, 0.12);
    g.fillStyle = 'rgba(0,0,0,0.35)';
    for (let i = 0; i < 4; i++) g.fillRect(Math.random() * 60, Math.random() * 60, rand(4, 12), rand(3, 8));
    stains(g, 64, 64, 4, 'rgba(110,0,0,0.6)');
  });
  return (zMat.shirts[ci] = new THREE.MeshLambertMaterial({ map: t }));
}

class Zombie {
  constructor(opts) {
    this.pos = new V3(opts.x, 0, opts.z);
    this.state = opts.state;             // 'approach' | 'rise'
    this.win = opts.win || null;
    this.room = opts.room || (opts.win && opts.win.room);
    this.hp = game.zHP;
    this.speed = zombieSpeed();
    this.runner = this.speed > 2.6;
    this.dead = false; this.deadT = 0; this.fallDir = 1; this.remove = false;
    this.riseT = 0; this.climbT = 0; this.tearT = 0;
    this.nextAttack = 0; this.attackT = 0;
    this.walkT = Math.random() * 6;
    this.nextGroan = T + rand(1, 5);
    this.stuckCheckT = T + 6; this.stuckPos = this.pos.clone();
    this.parts = [];

    const g = new THREE.Group();
    g.rotation.order = 'YXZ';
    this.group = g;
    const si = Math.floor(Math.random() * SKINS.length);
    const skin = zMat.skin[si];
    const add = (geo, mat, px, py, pz, parent, head = false) => {
      const m = new THREE.Mesh(geo, mat);
      m.position.set(px, py, pz); parent.add(m);
      m.userData.zombie = this; m.userData.head = head;
      this.parts.push(m);
      return m;
    };
    const pivot = (px, py, pz) => { const p = new THREE.Group(); p.position.set(px, py, pz); g.add(p); return p; };
    const pants = pick(zMat.pants);
    this.legL = pivot(-0.14, 0.9, 0); this.legR = pivot(0.14, 0.9, 0);
    add(zGeo.leg, pants, 0, 0, 0, this.legL);
    add(zGeo.leg, pants, 0, 0, 0, this.legR);
    add(zGeo.torso, shirtMat(Math.floor(Math.random() * SHIRTS.length)), 0, 1.26, 0, g);
    this.armL = pivot(-0.37, 1.56, 0); this.armR = pivot(0.37, 1.56, 0);
    add(zGeo.arm, skin, 0, 0, 0, this.armL); add(zGeo.hand, skin, 0, 0, 0, this.armL);
    if (Math.random() > 0.12) { add(zGeo.arm, skin, 0, 0, 0, this.armR); add(zGeo.hand, skin, 0, 0, 0, this.armR); }
    this.armBaseL = -Math.PI / 2 + rand(-0.25, 0.2);
    this.armBaseR = -Math.PI / 2 + rand(-0.25, 0.2);
    this.headG = pivot(0, 1.83, 0);
    const top = Math.random() < 0.5 ? zMat.hair : skin;
    add(zGeo.head, [skin, skin, top, skin, faceMat(si, Math.floor(Math.random() * 3)), skin], 0, 0, 0, this.headG, true);
    add(zGeo.eye, zMat.eye, -0.08, 0.046, 0.205, this.headG, true);
    add(zGeo.eye, zMat.eye, 0.08, 0.046, 0.205, this.headG, true);
    this.headG.rotation.z = rand(-0.25, 0.25);
    g.scale.setScalar(rand(0.94, 1.08));
    g.position.set(this.pos.x, this.state === 'rise' ? -1.9 : 0, this.pos.z);
    scene.add(g);
    g.updateMatrixWorld(true);
  }

  faceTo(dx, dz, k) {
    let diff = Math.atan2(dx, dz) - this.group.rotation.y;
    diff = Math.atan2(Math.sin(diff), Math.cos(diff));
    this.group.rotation.y += diff * k;
  }
  moveToward(tx, tz, dt, mul = 1) {
    const dx = tx - this.pos.x, dz = tz - this.pos.z;
    const d = Math.hypot(dx, dz);
    if (d < 0.01) return 0;
    const step = Math.min(d, this.speed * mul * dt);
    this.pos.x += dx / d * step; this.pos.z += dz / d * step;
    this.walkT += dt * (2 + this.speed * 2) * mul;
    this.faceTo(dx, dz, damp(8, dt));
    return d;
  }
  die(head) {
    this.dead = true;
    this.fallDir = Math.random() < 0.65 ? 1 : -1;
    if (this.win && this.win.climber === this) this.win.climber = null;
    if (head) {
      this.headG.visible = false;
      burst(this.pos.clone().setY(1.8), 0x8a0000, 18, 3);
    }
    if (this.state === 'chase') addDecal('blood', new V3(this.pos.x, 0.01, this.pos.z), new V3(0, 1, 0));
  }
  attack() {
    this.nextAttack = T + 1.1;
    this.attackT = 0.35;
    damagePlayer(40);
  }

  update(dt) {
    const g = this.group;
    if (this.dead) {
      this.deadT += dt;
      g.rotation.x = Math.max(-Math.PI / 2, -this.deadT * 5) * this.fallDir;
      if (this.deadT > 2) g.position.y -= dt * 0.8;
      if (this.deadT > 3.5) this.remove = true;
      return;
    }
    const pdx = player.pos.x - this.pos.x, pdz = player.pos.z - this.pos.z;
    const pd = Math.hypot(pdx, pdz);
    let moving = false;

    if (this.state === 'rise') {
      this.riseT += dt;
      g.position.y = Math.min(0, -1.9 + this.riseT * 1.7);
      if (Math.random() < 0.3) burst(new V3(this.pos.x + rand(-0.3, 0.3), 0.05, this.pos.z + rand(-0.3, 0.3)), 0x3a2a1a, 1, 1.5);
      this.faceTo(pdx, pdz, 1);
      if (g.position.y >= 0) { g.position.y = 0; this.state = 'chase'; }
      return;
    }
    if (this.state === 'approach') {
      const o = this.win.outside;
      moving = this.moveToward(o.x, o.z, dt) > 0.05;
      if (Math.hypot(o.x - this.pos.x, o.z - this.pos.z) < 0.3) { this.state = 'tear'; this.tearT = T + tearInterval(); }
    } else if (this.state === 'tear') {
      const o = this.win.outside;
      if (Math.hypot(o.x - this.pos.x, o.z - this.pos.z) > 0.3) this.moveToward(o.x, o.z, dt, 0.5);
      this.faceTo(this.win.nx, this.win.nz, damp(8, dt));
      if (this.win.boards > 0) {
        if (T >= this.tearT) { tearBoard(this.win); this.attackT = 0.35; this.tearT = T + tearInterval(); }
      } else if (!this.win.climber) {
        this.win.climber = this;
        this.state = 'climb'; this.climbT = 0;
      }
      if (pd < 1.7 && this.win.boards <= 2 && T >= this.nextAttack) this.attack();
    } else if (this.state === 'climb') {
      this.climbT += dt / 0.9;
      const t = Math.min(1, this.climbT), o = this.win.outside, i = this.win.inside;
      this.pos.x = lerp(o.x, i.x, t); this.pos.z = lerp(o.z, i.z, t);
      g.position.y = Math.sin(t * Math.PI) * 0.75;
      if (t >= 1) { g.position.y = 0; this.state = 'chase'; this.room = this.win.room; this.win.climber = null; }
    } else if (this.state === 'chase') {
      this.room = roomAt(this.pos.x, this.pos.z) || this.room;
      let tx = player.pos.x, tz = player.pos.z;
      const nr = this.room !== player.room ? nextRoom(this.room, player.room) : null;
      if (nr) {
        const L = nr.link, sc = L.side[this.room], sn = L.side[nr.room];
        const lateral = L.axis === 'z' ? Math.abs(this.pos.z - L.z) : Math.abs(this.pos.x - L.x);
        const depth = L.axis === 'z' ? Math.abs(this.pos.x - L.x) : Math.abs(this.pos.z - L.z);
        if (lateral > 0.9 && depth > 0.4) { tx = sc.x; tz = sc.z; } else { tx = sn.x; tz = sn.z; }
      }
      if (pd > 1.1 || nr) moving = this.moveToward(tx, tz, dt) > 0.05;
      else this.faceTo(pdx, pdz, damp(8, dt));
      if (pd < 1.3 && !nr && T >= this.nextAttack) this.attack();
      if (T > this.stuckCheckT) {
        if (this.pos.distanceTo(this.stuckPos) < 0.6 && pd > 3) { this.remove = true; game.toSpawn++; }
        this.stuckPos.copy(this.pos);
        this.stuckCheckT = T + 6;
      }
    }

    // Separación entre zombis
    if (this.state !== 'climb') {
      for (const o of zombies) {
        if (o === this || o.dead || o.state === 'rise' || o.state === 'climb') continue;
        const ox = this.pos.x - o.pos.x, oz = this.pos.z - o.pos.z;
        const od = Math.hypot(ox, oz);
        if (od < 0.7 && od > 0.001) { this.pos.x += ox / od * (0.7 - od) * 0.5; this.pos.z += oz / od * (0.7 - od) * 0.5; }
      }
    }
    if (this.state === 'chase') collide(this.pos, 0.35);
    g.position.x = this.pos.x; g.position.z = this.pos.z;

    // Animación
    const sw = Math.sin(this.walkT) * (this.runner ? 0.9 : 0.6) * (moving ? 1 : 0.2);
    this.legL.rotation.x = sw; this.legR.rotation.x = -sw;
    let atk = 0;
    if (this.attackT > 0) { this.attackT -= dt; atk = Math.sin((1 - this.attackT / 0.35) * Math.PI) * 0.9; }
    if (this.runner && moving && this.state === 'chase') {
      g.rotation.x = lerp(g.rotation.x, 0.3, damp(6, dt));
      this.armL.rotation.x = -0.4 - sw * 1.2 - atk * 1.5;
      this.armR.rotation.x = -0.4 + sw * 1.2 - atk * 1.5;
    } else {
      g.rotation.x = lerp(g.rotation.x, 0, damp(6, dt));
      this.armL.rotation.x = this.armBaseL + Math.sin(this.walkT * 0.5) * 0.1 + atk;
      this.armR.rotation.x = this.armBaseR - Math.sin(this.walkT * 0.5) * 0.1 + atk;
    }

    if (T > this.nextGroan) {
      this.nextGroan = T + rand(3, 8);
      if (pd < 25) play('groan', 1 - pd / 25);
    }
  }
}

function zombieHP(r) {
  if (r < 10) return 150 + 100 * (r - 1);
  let h = 950;
  for (let i = 10; i <= r; i++) h *= 1.1;
  return Math.round(h);
}
function zombieSpeed() { return Math.min(4.4, 1.2 + game.round * 0.32) * rand(0.85, 1.15); }
function tearInterval() { return Math.max(0.45, 1.3 - game.round * 0.06); }

function spawnZombie() {
  const opts = [];
  const dist = (x, z) => Math.hypot(x - player.pos.x, z - player.pos.z);
  for (const w of windows) if (unlocked.has(w.room)) opts.push({ w, d: dist(w.inside.x, w.inside.z) });
  for (const r of unlocked) for (const p of (map.groundSpawns && map.groundSpawns[r]) || []) {
    const d = dist(p[0], p[1]);
    if (d > 5) opts.push({ p, room: r, d });
  }
  if (!opts.length) return;
  opts.sort((a, b) => a.d - b.d);
  const o = pick(opts.slice(0, 4));
  if (o.w) {
    const w = o.w, out = rand(5, 9), lat = rand(-1.5, 1.5);
    zombies.push(new Zombie({ x: w.outside.x - w.nx * out - w.nz * lat, z: w.outside.z - w.nz * out + w.nx * lat, state: 'approach', win: w }));
  } else {
    zombies.push(new Zombie({ x: o.p[0] + rand(-0.4, 0.4), z: o.p[1] + rand(-0.4, 0.4), state: 'rise', room: o.room }));
  }
}

function updateZombies(dt) {
  for (let i = zombies.length - 1; i >= 0; i--) {
    const z = zombies[i];
    z.update(dt);
    if (z.remove) {
      if (!z.dead && z.win && z.win.climber === z) z.win.climber = null;
      scene.remove(z.group);
      zombies.splice(i, 1);
    }
  }
}

// ---------- Power-ups ----------
const POWERUPS = {
  max:    { name: 'MUNICIÓN MÁXIMA',    short: 'MAX', color: 0xffd23f },
  insta:  { name: 'MUERTE INSTANTÁNEA', short: '☠',   color: 0xff3b3b },
  double: { name: 'DOBLE PUNTOS',       short: 'x2',  color: 0x3bff6b },
  nuke:   { name: 'BOMBA NUCLEAR',      short: '☢',   color: 0xffa23b },
};
const powerups = [];
function dropPowerup(x, z) {
  const type = pick(Object.keys(POWERUPS));
  const p = POWERUPS[type];
  const grp = new THREE.Group();
  const core = new THREE.Mesh(new THREE.BoxGeometry(0.4, 0.4, 0.4), new THREE.MeshBasicMaterial({ color: p.color, transparent: true, opacity: 0.85, fog: false }));
  grp.add(core);
  const lb = makeLabel(p.short, hex(p.color), 1.6);
  lb.position.y = 0.6; grp.add(lb);
  grp.position.set(x, 1, z);
  scene.add(grp);
  powerups.push({ type, grp, core, t: 0 });
}
function applyPowerup(type) {
  const p = POWERUPS[type];
  centerMsg(p.name, hex(p.color));
  play('powerup');
  if (type === 'max') { for (const w of player.weapons) w.reserve = stats(w).reserve; player.grenades = 4; }
  if (type === 'insta') game.insta = T + 20;
  if (type === 'double') game.double = T + 20;
  if (type === 'nuke') {
    for (const z of zombies) if (!z.dead) killZombie(z, false, true);
    addPoints(400);
    whiteFlash(0.9);
  }
}
function whiteFlash(op) {
  const f = $('flash');
  f.style.transition = 'none'; f.style.opacity = op;
  requestAnimationFrame(() => { f.style.transition = 'opacity 1.2s'; f.style.opacity = 0; });
}
function updatePowerups(dt) {
  for (let i = powerups.length - 1; i >= 0; i--) {
    const p = powerups[i];
    p.t += dt;
    p.core.rotation.y += dt * 2; p.core.rotation.x += dt;
    p.grp.position.y = 1 + Math.sin(p.t * 3) * 0.15;
    p.grp.visible = p.t < 20 || Math.floor(p.t * 6) % 2 === 0;
    const d = Math.hypot(p.grp.position.x - player.pos.x, p.grp.position.z - player.pos.z);
    if (d < 1.3 || p.t > 26) {
      if (d < 1.3) applyPowerup(p.type);
      scene.remove(p.grp);
      powerups.splice(i, 1);
    }
  }
}

// ---------- Jugador ----------
function damagePlayer(n) {
  if (game.state !== 'playing' || n <= 0) return;
  player.health -= n;
  player.lastHit = T;
  vm.shake = Math.max(vm.shake, 0.3);
  play('hurt');
  if (player.health > 0) return;
  if (player.perks.has('revive')) {
    player.perks.delete('revive');
    renderPerks();
    player.health = player.maxHealth;
    for (const z of zombies) if (!z.dead && z.state === 'chase' && z.pos.distanceTo(player.pos) < 6) killZombie(z, false, true);
    centerMsg('¡REVIVISTE!', '#7cc4ff');
    whiteFlash(0.7);
  } else gameOver();
}

function updatePlayer(dt) {
  const fx = -Math.sin(player.yaw), fz = -Math.cos(player.yaw);
  const rx = Math.cos(player.yaw), rz = -Math.sin(player.yaw);
  let mx = 0, mz = 0;
  if (keys.KeyW) { mx += fx; mz += fz; }
  if (keys.KeyS) { mx -= fx; mz -= fz; }
  if (keys.KeyD) { mx += rx; mz += rz; }
  if (keys.KeyA) { mx -= rx; mz -= rz; }
  const len = Math.hypot(mx, mz);
  player.crouch = !!(keys.KeyC || keys.ControlLeft);
  player.moving = len > 0;
  player.sprinting = player.moving && (keys.ShiftLeft || keys.ShiftRight) && keys.KeyW && !mouse.right && !player.reloading && !player.crouch;
  let speed = player.crouch ? 2.8 : player.sprinting ? 8.6 : 5.6;
  if (player.perks.has('stamin')) speed *= player.sprinting ? 1.35 : 1.25;
  if (mouse.right) speed *= 0.6;
  const tx = len ? mx / len * speed : 0, tz = len ? mz / len * speed : 0;
  const k = damp(player.onGround ? (len ? 14 : 10) : 2, dt);
  player.vel.x += (tx - player.vel.x) * k;
  player.vel.z += (tz - player.vel.z) * k;
  player.pos.x += player.vel.x * dt;
  player.pos.z += player.vel.z * dt;

  if (keys.Space && player.onGround && !player.crouch) { player.vy = 6.2; player.onGround = false; }
  const wasAir = !player.onGround;
  player.vy -= 18 * dt;
  player.pos.y += player.vy * dt;
  if (player.pos.y <= 0) {
    if (wasAir && player.vy < -3) vm.land = Math.min(0.18, -player.vy * 0.022);
    player.pos.y = 0; player.vy = 0; player.onGround = true;
  }

  for (const z of zombies) {
    if (z.dead || z.state !== 'chase') continue;
    const dx = player.pos.x - z.pos.x, dz = player.pos.z - z.pos.z;
    const d = Math.hypot(dx, dz);
    if (d < 0.75 && d > 0.001) { player.pos.x += dx / d * (0.75 - d); player.pos.z += dz / d * (0.75 - d); }
  }
  collide(player.pos, 0.4);
  player.room = roomAt(player.pos.x, player.pos.z) || player.room;

  if (T - player.lastHit > 4 && player.health < player.maxHealth) player.health = Math.min(player.maxHealth, player.health + 60 * dt);
  if (player.health / player.maxHealth < 0.4 && T > player.nextHeart) { play('heart'); player.nextHeart = T + 0.75; }

  // Cámara suave: balanceo, inclinación lateral y aterrizaje
  const hspeed = Math.hypot(player.vel.x, player.vel.z);
  if (player.onGround) vm.bobT += dt * Math.min(hspeed, 9) * 1.8;
  vm.bobAmt = lerp(vm.bobAmt, player.onGround ? Math.min(1, hspeed / 6) : 0, damp(10, dt));
  const bobY = Math.sin(vm.bobT * 2) * 0.04 * vm.bobAmt;
  const bobX = Math.cos(vm.bobT) * 0.025 * vm.bobAmt;
  vm.land = Math.max(0, vm.land - dt * 0.7);
  vm.shake = Math.max(0, vm.shake - dt);
  player.eye = lerp(player.eye, player.crouch ? 1.05 : 1.65, damp(12, dt));
  const sh = vm.shake * 0.12;
  camera.position.set(
    player.pos.x + rx * bobX + rand(-sh, sh),
    player.pos.y + player.eye + bobY - vm.land,
    player.pos.z + rz * bobX + rand(-sh, sh)
  );
  const strafe = player.vel.x * rx + player.vel.z * rz;
  vm.roll = lerp(vm.roll, -strafe * 0.006, damp(8, dt));
  // retroceso suave
  const rc = vm.recoil * damp(20, dt);
  player.pitch = clamp(player.pitch + rc, -1.5, 1.5);
  vm.recoil -= rc;
  camera.rotation.set(player.pitch, player.yaw, vm.roll);
}

function updateWeapon(dt) {
  const w = curWeapon(), s = stats(w);
  if (player.reloading && T >= player.reloadEnd) {
    const n = Math.min(s.mag - w.mag, w.reserve);
    w.mag += n; w.reserve -= n;
    player.reloading = false;
  }
  if (mouse.down && !player.papBusy && (s.auto || T - mouse.pressedAt < 0.25) && !player.reloading && T >= player.nextShot) {
    if (w.mag > 0) fire();
    else if (w.reserve > 0) startReload();
    else { play('empty'); player.nextShot = T + 0.3; }
    mouse.pressedAt = -9;
  }

  // Animación del arma
  vm.kick = Math.max(0, vm.kick - dt * 7);
  vm.swap = Math.max(0, vm.swap - dt * 3);
  vm.throwT = Math.max(0, vm.throwT - dt);
  vm.ads += ((mouse.right && !player.reloading && !player.papBusy ? 1 : 0) - vm.ads) * damp(12, dt);
  vm.sprint += ((player.sprinting ? 1 : 0) - vm.sprint) * damp(10, dt);
  vm.swayX = lerp(vm.swayX, vm.swayTX, damp(10, dt));
  vm.swayY = lerp(vm.swayY, vm.swayTY, damp(10, dt));
  vm.swayTX *= Math.exp(-8 * dt); vm.swayTY *= Math.exp(-8 * dt);
  vm.flash -= dt;
  flash.visible = vm.flash > 0;
  muzzleLight.intensity = vm.flash > 0 ? 2.5 : 0;
  let rl = 0;
  if (player.reloading) rl = Math.sin(clamp(1 - (player.reloadEnd - T) / player.reloadTime, 0, 1) * Math.PI);
  const thr = Math.sin((vm.throwT / 0.4) * Math.PI);
  const sway = 1 - vm.ads * 0.8;
  const bx = Math.sin(vm.bobT) * 0.012 * vm.bobAmt * sway;
  const by = Math.abs(Math.cos(vm.bobT)) * 0.01 * vm.bobAmt * sway;
  gunHolder.position.set(
    lerp(0.17, 0, vm.ads) + bx + vm.sprint * 0.04 + vm.swayX * sway,
    lerp(-0.17, -0.1, vm.ads) - by - rl * 0.12 - vm.swap * 0.3 - vm.sprint * 0.04 - thr * 0.25 + vm.swayY * sway,
    lerp(-0.42, -0.34, vm.ads) + vm.kick * 0.06
  );
  gunHolder.rotation.set(vm.kick * 0.12 - rl * 0.7 - vm.sprint * 0.2, vm.sprint * 0.6 + vm.swayX * 2, rl * 0.3 - vm.roll * 2);
  if (gunModel && w.pap) TEX.camo.offset.x += dt * 0.15;

  const scoped = s.id === 'sniper' && vm.ads > 0.85;
  const fov = 75 + vm.sprint * 6 - vm.ads * (s.id === 'sniper' ? 50 : 20);
  if (Math.abs(fov - vm.fov) > 0.01) { vm.fov = fov; camera.fov = fov; camera.updateProjectionMatrix(); }
  gunHolder.visible = !scoped && !player.papBusy;
  $('scope').classList.toggle('hidden', !scoped);
  $('crosshair').style.opacity = 1 - vm.ads;
}

// ---------- Rondas ----------
const MAX_ALIVE = 24;
function startRound() {
  game.round++;
  game.between = false;
  game.toSpawn = Math.min(70, Math.round(5 + game.round * 3 + (game.round > 5 ? game.round * 1.5 : 0)));
  game.spawnT = 1;
  game.drops = 0;
  game.repairPts = 0;
  game.zHP = zombieHP(game.round);
  if (game.round > 1) player.grenades = Math.min(4, player.grenades + 2);
  const r = $('round');
  r.textContent = game.round;
  r.classList.remove('flash'); void r.offsetWidth; r.classList.add('flash');
  play('round');
  if (CONFIG.mensajes[game.round]) specialMsg(CONFIG.mensajes[game.round]);
}
function updateRounds(dt) {
  if (game.between) {
    game.betweenT -= dt;
    if (game.betweenT <= 0) startRound();
    return;
  }
  const alive = zombies.filter((z) => !z.dead).length;
  if (game.toSpawn > 0) {
    game.spawnT -= dt;
    if (game.spawnT <= 0 && alive < MAX_ALIVE) {
      spawnZombie();
      game.toSpawn--;
      game.spawnT = Math.max(0.5, 2.2 - game.round * 0.15);
    }
  } else if (alive === 0) {
    game.between = true;
    game.betweenT = 9;
    centerMsg('RONDA SUPERADA', '#ffd84a');
    play('roundEnd');
  }
}

// ---------- Interacción ----------
let currentInteract = null;
function updateInteract() {
  let best = null, bd = Infinity;
  for (const it of interactables) {
    if (!it.active()) continue;
    const p = it.pos ? it.pos() : it;
    const d = Math.hypot(p.x - player.pos.x, p.z - player.pos.z);
    if (d < it.range && d < bd) { best = it; bd = d; }
  }
  currentInteract = best;
  setText($('prompt'), best ? best.text() : '');
  if (best && best.hold && keys.KeyE) best.holdAction();
}
function spend(cost) {
  if (player.points >= cost) { player.points -= cost; play('buy'); return true; }
  play('deny');
  const el = $('points');
  el.classList.add('deny');
  setTimeout(() => el.classList.remove('deny'), 400);
  return false;
}
function addPoints(n) {
  if (T < game.double) n *= 2;
  player.points += n;
  const d = document.createElement('div');
  d.className = 'pop'; d.textContent = '+' + n;
  d.style.right = rand(0, 40) + 'px';
  $('points-pop').appendChild(d);
  setTimeout(() => d.remove(), 900);
}

// ---------- HUD ----------
let hitTimer;
function showHit(kill) {
  const el = $('hitmarker');
  el.classList.toggle('kill', kill);
  el.classList.add('show');
  clearTimeout(hitTimer);
  hitTimer = setTimeout(() => el.classList.remove('show'), kill ? 200 : 100);
}
let cmTimer, smTimer;
function centerMsg(text, color = '#fff', ms = 2500) {
  const el = $('center-msg');
  el.textContent = text; el.style.color = color;
  el.classList.add('show');
  clearTimeout(cmTimer);
  cmTimer = setTimeout(() => el.classList.remove('show'), ms);
}
function specialMsg(text) {
  const el = $('special-msg');
  el.textContent = text;
  el.classList.add('show');
  clearTimeout(smTimer);
  smTimer = setTimeout(() => el.classList.remove('show'), 4500);
}
function renderPerks() {
  $('perks').innerHTML = [...player.perks].map((k) => {
    const p = PERKS[k];
    return `<div class="perk" style="background:${hex(p.color)}">${p.letter}</div>`;
  }).join('');
}
function updateHUD() {
  const w = curWeapon(), s = stats(w);
  setText($('points'), player.points);
  const wn = $('weapon-name');
  setText(wn, player.papBusy ? 'PACK-A-PUNCH...' : s.name);
  wn.classList.toggle('pap', !!w.pap);
  setText($('mag'), w.mag);
  setText($('reserve'), ' / ' + w.reserve);
  setText($('grenades'), 'GRANADAS ' + '● '.repeat(player.grenades).trim());
  setText($('reload-msg'),
    player.reloading ? 'RECARGANDO...' :
    w.mag === 0 && w.reserve === 0 ? 'SIN MUNICIÓN' :
    w.mag <= Math.ceil(s.mag * 0.25) && w.reserve > 0 ? 'PRESIONA R PARA RECARGAR' : '');
  $('health-fill').style.width = (player.health / player.maxHealth * 100) + '%';
  $('damage').style.opacity = clamp(1 - player.health / player.maxHealth, 0, 1) * 0.9;
  let pu = '';
  if (T < game.insta) pu += `<div class="pu" style="color:#ff3b3b">☠ ${Math.ceil(game.insta - T)}</div>`;
  if (T < game.double) pu += `<div class="pu" style="color:#3bff6b">x2 ${Math.ceil(game.double - T)}</div>`;
  setHTML($('powerups'), pu);
  drawMinimap();
}

const mmCanvas = $('minimap'), mmCtx = mmCanvas.getContext('2d');
function drawMinimap() {
  const g = mmCtx, W = mmCanvas.width, S = 4.2;
  g.clearRect(0, 0, W, W);
  g.save();
  g.beginPath(); g.arc(W / 2, W / 2, W / 2 - 2, 0, 7); g.clip();
  g.fillStyle = 'rgba(8,8,12,0.78)'; g.fillRect(0, 0, W, W);
  g.translate(W / 2, W / 2); g.rotate(player.yaw); g.scale(S, S); g.translate(-player.pos.x, -player.pos.z);
  for (const k in rooms) {
    const r = rooms[k];
    g.fillStyle = unlocked.has(k) ? 'rgba(110,100,90,0.5)' : 'rgba(45,45,45,0.55)';
    g.fillRect(r.x1, r.z1, r.x2 - r.x1, r.z2 - r.z1);
  }
  g.fillStyle = '#d8d8d8';
  for (const w of mmWalls) g.fillRect(w.minX, w.minZ, w.maxX - w.minX, w.maxZ - w.minZ);
  g.fillStyle = '#ffcc33';
  for (const d of doors) if (!d.link.open) g.fillRect(d.col.minX, d.col.minZ, d.col.maxX - d.col.minX, d.col.maxZ - d.col.minZ);
  for (const w of windows) {
    g.fillStyle = w.boards === 0 ? '#ff3030' : w.boards < 5 ? '#ffa030' : '#5ad1ff';
    g.fillRect(w.x - (w.nx ? 0.3 : 0.8), w.z - (w.nx ? 0.8 : 0.3), w.nx ? 0.6 : 1.6, w.nx ? 1.6 : 0.6);
  }
  const dot = (x, z, r, c) => { g.fillStyle = c; g.beginPath(); g.arc(x, z, r, 0, 7); g.fill(); };
  for (const ic of mmIcons) dot(ic.x, ic.z, ic.r, ic.color);
  if (box.state !== 'leaving') dot(box.pos.x, box.pos.z, 0.9, '#4aa8ff');
  for (const p of powerups) dot(p.grp.position.x, p.grp.position.z, 0.6, '#3bff6b');
  for (const z of zombies) if (!z.dead) dot(z.pos.x, z.pos.z, 0.45, '#ff2a2a');
  g.restore();
  g.fillStyle = '#fff';
  g.beginPath(); g.moveTo(W / 2, W / 2 - 8); g.lineTo(W / 2 - 6, W / 2 + 6); g.lineTo(W / 2 + 6, W / 2 + 6); g.closePath(); g.fill();
  g.strokeStyle = 'rgba(255,255,255,0.45)'; g.lineWidth = 2;
  g.beginPath(); g.arc(W / 2, W / 2, W / 2 - 2, 0, 7); g.stroke();
}

// ---------- Sonido (sintetizado, sin archivos) ----------
let actx = null, master = null, noiseBuf = null;
function initAudio() {
  if (actx) return;
  try {
    actx = new (window.AudioContext || window.webkitAudioContext)();
    master = actx.createGain(); master.gain.value = 0.45; master.connect(actx.destination);
    noiseBuf = actx.createBuffer(1, actx.sampleRate, actx.sampleRate);
    const d = noiseBuf.getChannelData(0);
    for (let i = 0; i < d.length; i++) d[i] = Math.random() * 2 - 1;
  } catch (e) { actx = null; }
}
function env(g, t, a, peak, decay) {
  peak = Math.max(0.0002, peak);
  g.gain.setValueAtTime(0.0001, t);
  g.gain.exponentialRampToValueAtTime(peak, t + a);
  g.gain.exponentialRampToValueAtTime(0.0001, t + a + decay);
}
function noise(dur, type, freq, peak, decay, delay = 0) {
  const t = actx.currentTime + delay;
  const src = actx.createBufferSource(); src.buffer = noiseBuf;
  const f = actx.createBiquadFilter(); f.type = type; f.frequency.value = freq;
  const g = actx.createGain(); env(g, t, 0.002, peak, decay);
  src.connect(f).connect(g).connect(master);
  src.start(t); src.stop(t + dur);
}
function tone(type, f1, f2, dur, peak, delay = 0, out = master, at = null) {
  const t = at !== null ? at : actx.currentTime + delay;
  const o = actx.createOscillator(); o.type = type;
  o.frequency.setValueAtTime(f1, t);
  o.frequency.exponentialRampToValueAtTime(f2, t + dur);
  const g = actx.createGain(); env(g, t, 0.005, peak, dur);
  o.connect(g).connect(out);
  o.start(t); o.stop(t + dur + 0.05);
}
const SFX = {
  pistol()  { noise(0.3, 'lowpass', 2500, 0.9, 0.18); tone('square', 180, 60, 0.08, 0.3); },
  smg()     { noise(0.2, 'lowpass', 3000, 0.6, 0.1); tone('square', 200, 80, 0.05, 0.2); },
  rifle()   { noise(0.35, 'lowpass', 2000, 0.9, 0.22); tone('sawtooth', 150, 50, 0.1, 0.35); },
  shotgun() { noise(0.6, 'lowpass', 1400, 1.0, 0.45); tone('square', 110, 40, 0.2, 0.5); },
  sniper()  { noise(0.9, 'lowpass', 1800, 1.0, 0.7); tone('sawtooth', 120, 30, 0.3, 0.5); },
  ray()     { tone('square', 1400, 180, 0.28, 0.22); tone('sine', 700, 90, 0.3, 0.3); },
  zap()     { noise(0.5, 'highpass', 2500, 0.8, 0.4); tone('sawtooth', 2000, 100, 0.4, 0.3); tone('square', 60, 40, 0.4, 0.4); },
  launcher() { tone('sine', 220, 60, 0.18, 0.7); noise(0.2, 'lowpass', 800, 0.6, 0.12); },
  rocket()  { noise(1.0, 'bandpass', 900, 0.8, 0.9); noise(0.3, 'lowpass', 400, 1.0, 0.25); tone('sawtooth', 300, 90, 0.7, 0.2); },
  empty()   { noise(0.05, 'highpass', 3000, 0.3, 0.03); },
  reload()  { noise(0.06, 'bandpass', 2000, 0.4, 0.05); noise(0.08, 'bandpass', 1500, 0.5, 0.06, 0.35); },
  buy()     { tone('sine', 988, 988, 0.12, 0.3); tone('sine', 1319, 1319, 0.3, 0.3, 0.1); },
  deny()    { tone('square', 160, 120, 0.25, 0.2); },
  hurt()    { tone('sine', 160, 50, 0.25, 0.6); noise(0.2, 'lowpass', 600, 0.5, 0.15); },
  hit()     { noise(0.05, 'highpass', 4000, 0.2, 0.03); },
  heart()   { tone('sine', 70, 40, 0.12, 0.6); tone('sine', 65, 38, 0.12, 0.45, 0.18); },
  pin()     { noise(0.05, 'highpass', 5000, 0.3, 0.04); tone('sine', 2400, 2000, 0.08, 0.15, 0.05); },
  boom()    { noise(1.6, 'lowpass', 500, 1.0, 1.4); tone('sine', 80, 25, 0.9, 0.9); noise(0.3, 'lowpass', 3000, 0.6, 0.2); },
  board(v)  { noise(0.25, 'bandpass', 900, 0.6 * v, 0.2); noise(0.15, 'highpass', 2500, 0.3 * v, 0.1, 0.05); },
  repair()  { noise(0.06, 'bandpass', 1200, 0.5, 0.05); noise(0.06, 'bandpass', 1100, 0.5, 0.05, 0.18); },
  laugh()   { for (let i = 0; i < 6; i++) tone('triangle', 600 - i * 50, 300 - i * 30, 0.25, 0.25, i * 0.3); },
  pap()     { tone('sawtooth', 80, 160, 3.2, 0.2); noise(3.2, 'bandpass', 600, 0.25, 3.0); for (let i = 0; i < 6; i++) tone('square', 300 + i * 120, 300 + i * 120, 0.1, 0.12, i * 0.5); },
  groan(vol) {
    const t = actx.currentTime;
    const o = actx.createOscillator(); o.type = 'sawtooth';
    const base = rand(70, 110);
    o.frequency.setValueAtTime(base, t);
    o.frequency.linearRampToValueAtTime(base * rand(0.6, 0.85), t + 0.9);
    const f = actx.createBiquadFilter(); f.type = 'lowpass'; f.frequency.value = 500;
    const g = actx.createGain(); env(g, t, 0.15, 0.35 * vol, 0.8);
    o.connect(f).connect(g).connect(master);
    o.start(t); o.stop(t + 1.1);
  },
  round()    { [220, 207, 196].forEach((f, i) => tone('sawtooth', f, f * 0.98, 0.9, 0.15, i * 0.45)); tone('sine', 55, 50, 2.0, 0.5); },
  roundEnd() { tone('triangle', 392, 392, 0.4, 0.25); tone('triangle', 330, 330, 0.4, 0.25, 0.35); tone('triangle', 262, 262, 0.9, 0.25, 0.7); },
  powerup()  { tone('sine', 600, 1200, 0.3, 0.3); tone('sine', 900, 1800, 0.3, 0.25, 0.15); },
  door()     { noise(1.2, 'lowpass', 300, 0.7, 1.0); tone('sawtooth', 60, 40, 1.0, 0.25); },
  perk()     { tone('sine', 523, 523, 0.2, 0.3); tone('sine', 659, 659, 0.2, 0.3, 0.15); tone('sine', 784, 784, 0.4, 0.3, 0.3); },
  box()      { for (let i = 0; i < 12; i++) tone('triangle', rand(500, 1000), rand(500, 1000), 0.12, 0.12, i * 0.3); },
};
function play(name, ...args) { if (actx && SFX[name]) SFX[name](...args); }

// Música de fondo generada: zumbido grave + caja de música inquietante
const music = { on: CONFIG.musica, gain: null, next: 0, step: 0, started: false };
function startMusic() {
  if (!actx || music.started) return;
  music.started = true;
  music.gain = actx.createGain();
  music.gain.gain.value = 0;
  music.gain.connect(master);
  music.gain.gain.setTargetAtTime(music.on ? 0.5 : 0, actx.currentTime, 1.5);
  const lp = actx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 260;
  const dg = actx.createGain(); dg.gain.value = 0.06;
  lp.connect(dg).connect(music.gain);
  for (const f of [55, 55.4, 82.4]) {
    const o = actx.createOscillator(); o.type = 'sawtooth'; o.frequency.value = f; o.connect(lp); o.start();
  }
  const lfo = actx.createOscillator(); lfo.frequency.value = 0.07;
  const lg = actx.createGain(); lg.gain.value = 120;
  lfo.connect(lg).connect(lp.frequency); lfo.start();
  music.next = actx.currentTime + 1;
  setInterval(scheduleMusic, 100);
}
function scheduleMusic() {
  if (game.state !== 'playing' || !music.on) { music.next = actx.currentTime + 0.2; return; }
  const scale = [220, 261.6, 293.7, 329.6, 392, 440, 523.3];
  const pattern = [0, 2, 4, 2, 1, 3, 5, 3, 0, 2, 4, 6, 4, 2, 1, 0];
  while (music.next < actx.currentTime + 0.3) {
    const t = music.next, s = music.step++;
    const low = (s % 32) >= 16 ? 0.75 : 1;
    const f = scale[pattern[s % 16]] * 2 * low;
    if (Math.random() < 0.8) tone('sine', f, f, 0.6, 0.07, 0, music.gain, t);
    if (s % 8 === 0) tone('triangle', 110 * low, 110 * low, 1.2, 0.18, 0, music.gain, t);
    music.next += 0.34 - Math.min(0.12, game.round * 0.008);
  }
}
function toggleMusic() {
  music.on = !music.on;
  if (music.gain) music.gain.gain.setTargetAtTime(music.on ? 0.5 : 0, actx.currentTime, 0.3);
  centerMsg(music.on ? 'MÚSICA: SÍ' : 'MÚSICA: NO', '#fff', 1200);
}

// ---------- Controles ----------
const canvas = renderer.domElement;
document.addEventListener('keydown', (e) => {
  keys[e.code] = true;
  if (e.code === 'Space') e.preventDefault();
  if (game.state === 'playing' && game.noLock && e.code === 'Escape') { pauseGame(); return; }
  if (game.state !== 'playing') return;
  if (e.code === 'KeyR') startReload();
  if (e.code === 'KeyE' && currentInteract && currentInteract.action && !e.repeat) currentInteract.action();
  if (e.code === 'KeyG') throwGrenade();
  if (e.code === 'KeyM') toggleMusic();
  if (e.code === 'Digit1' && player.cur !== 0) switchWeapon(-1);
  if (e.code === 'Digit2' && player.cur !== 1) switchWeapon(1);
});
document.addEventListener('keyup', (e) => { keys[e.code] = false; });
document.addEventListener('mousedown', (e) => {
  if (game.state !== 'playing') return;
  if (e.button === 0) { mouse.down = true; mouse.pressedAt = T; }
  if (e.button === 2) mouse.right = true;
});
document.addEventListener('mouseup', (e) => {
  if (e.button === 0) mouse.down = false;
  if (e.button === 2) mouse.right = false;
});
document.addEventListener('contextmenu', (e) => e.preventDefault());
document.addEventListener('wheel', (e) => { if (game.state === 'playing') switchWeapon(e.deltaY > 0 ? 1 : -1); });
document.addEventListener('mousemove', (e) => {
  if (game.state !== 'playing') return;
  if (!game.noLock && document.pointerLockElement !== canvas) return;
  const s = CONFIG.sensibilidad * (camera.fov / 75);
  player.yaw -= e.movementX * s;
  player.pitch = clamp(player.pitch - e.movementY * s, -1.5, 1.5);
  vm.swayTX = clamp(vm.swayTX - e.movementX * 0.00025, -0.04, 0.04);
  vm.swayTY = clamp(vm.swayTY + e.movementY * 0.00025, -0.04, 0.04);
});

// Bloqueo del ratón (con entrada sin aceleración si el navegador lo permite)
function lock() {
  let p;
  try { p = canvas.requestPointerLock({ unadjustedMovement: true }); } catch (e) { p = null; }
  if (p && p.catch) {
    p.catch(() => {
      let p2;
      try { p2 = canvas.requestPointerLock(); } catch (e) { p2 = null; }
      if (p2 && p2.catch) p2.catch(() => noLockStart());
    });
  }
}
// Si el navegador no deja bloquear el ratón, se juega igual moviéndolo libremente
function noLockStart() {
  if (game.state !== 'menu' && game.state !== 'paused') return;
  game.noLock = true;
  resumeGame();
}
function resumeGame() {
  if (game.state === 'menu') startGame();
  game.state = 'playing';
  $('menu').classList.add('hidden');
  $('pause').classList.add('hidden');
  if (actx && actx.state === 'suspended') actx.resume();
}
function pauseGame() {
  game.state = 'paused';
  mouse.down = mouse.right = false;
  for (const k in keys) keys[k] = false;
  $('pause').classList.remove('hidden');
  if (actx) actx.suspend();
}
document.addEventListener('pointerlockchange', () => {
  const locked = document.pointerLockElement === canvas;
  if (locked && (game.state === 'menu' || game.state === 'paused')) resumeGame();
  else if (!locked && game.state === 'playing') pauseGame();
});
document.addEventListener('pointerlockerror', () => setTimeout(noLockStart, 50));

let currentMapKey = 'bunker';
document.querySelectorAll('.map-btn').forEach((btn) => btn.addEventListener('click', () => {
  const key = btn.dataset.map;
  if (key !== currentMapKey) { currentMapKey = key; buildMap(key); }
  initAudio();
  startMusic();
  lock();
}));
$('pause').addEventListener('click', () => { if (game.noLock) resumeGame(); else lock(); });
$('again').addEventListener('click', () => location.reload());
$('dedicatoria').textContent = `Hecho con amor para ${CONFIG.nombre} ❤`;

function startGame() {
  const s = map.start;
  player.pos.set(s.x, 0, s.z);
  player.yaw = s.yaw;
  player.room = map.startRoom;
  giveWeapon('pistol');
  $('hud').classList.remove('hidden');
  centerMsg('SOBREVIVE', '#ff4040', 2500);
}

function gameOver() {
  game.state = 'over';
  if (document.pointerLockElement) document.exitPointerLock();
  $('hud').classList.add('hidden');
  $('scope').classList.add('hidden');
  $('stats').innerHTML =
    `Mapa: <b>${map.name}</b><br>` +
    `Llegaste a la ronda <b>${game.round}</b><br>` +
    `Zombis eliminados: <b>${game.kills}</b><br>` +
    `Disparos a la cabeza: <b>${game.heads}</b>`;
  $('gameover').classList.remove('hidden');
}

// ---------- Bucle principal ----------
function update(dt) {
  T += dt;
  updatePlayer(dt);
  updateWeapon(dt);
  updateZombies(dt);
  updateRounds(dt);
  updateBox(dt);
  updatePap(dt);
  updateWindows(dt);
  updateGrenades(dt);
  updateProjectiles(dt);
  updatePowerups(dt);
  updateInteract();
  updateHUD();
  for (const d of doors) if (d.sinking) { d.mesh.position.y -= dt * 2; if (d.mesh.position.y < -2) { world.remove(d.mesh); d.sinking = false; } }
  for (const l of flickerLights) if (Math.random() < 0.04) l.intensity = Math.random() < 0.5 ? 0.2 : 1.4;
}

buildMap('bunker');
let last = performance.now() / 1000;
function loop() {
  requestAnimationFrame(loop);
  const now = performance.now() / 1000;
  const dt = Math.min(0.05, now - last);
  last = now;
  if (game.state === 'playing') update(dt);
  else if (game.state === 'menu') {
    camera.position.set(Math.sin(now * 0.1) * 4, 1.8, Math.cos(now * 0.1) * 4);
    camera.rotation.set(0, now * 0.1, 0);
  }
  updateParticles(dt);
  renderer.clear();
  renderer.render(scene, camera);
  renderer.clearDepth();
  if (game.state !== 'menu') renderer.render(gunScene, gunCam);
}
loop();
