<div align="center">

# 🧟 ÚLTIMA RONDA

**Shooter de zombis por rondas en primera persona, inspirado en el modo Zombies clásico.**
Hecho desde cero en **C++ con raylib**: renderizado PBR, sombras, ragdolls y audio propio.

![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![raylib](https://img.shields.io/badge/raylib-5.5-black?style=for-the-badge)
![OpenGL](https://img.shields.io/badge/OpenGL-3.3-5586A4?style=for-the-badge&logo=opengl&logoColor=white)
![Windows](https://img.shields.io/badge/Windows-10%20%7C%2011-0078D6?style=for-the-badge&logo=windows&logoColor=white)
![Three.js](https://img.shields.io/badge/Prototipo%20web-Three.js-000000?style=for-the-badge&logo=threedotjs&logoColor=white)

<img src="docs/img/bunker.png" alt="Zombi en el Búnker" width="85%">

*Sobreviví todas las rondas que puedas. Cada ronda, más zombis. Cada ronda, más rápidos.*

</div>

---

## 📸 Capturas

| Fábrica | Caja misteriosa |
|:---:|:---:|
| <img src="docs/img/fabrica.png" alt="Mapa Fábrica"> | <img src="docs/img/caja.png" alt="Caja misteriosa"> |

<p align="center"><img src="docs/img/pueblo.png" alt="Mapa Pueblo, al aire libre" width="85%"><br><em>Pueblo: el mapa al aire libre</em></p>

## ✨ Características

- 🗺️ **Tres mapas**: **Búnker** (pasillos estrechos) y **Fábrica** (mapa grande), con 5 zonas cada uno, y **Pueblo**, un mapa grande **al aire libre** de noche con 7 zonas: plaza, calle principal, iglesia, cementerio, estación, casa y taller, con cielo con luna y estrellas, farolas, autos abandonados y zombis que salen de la tierra. Además, un **Campo de tiro** para probar todas las armas gratis.
- 🧗 **Te podés subir** a cajas, barriles, autos, lápidas, sacos de arena y a la caja misteriosa: saltá contra ellos y trepás solo.
- 🗣️ **El personaje habla**: insulta a los zombis cuando lo hieren y comenta las bajas, los tiros a la cabeza, las recargas, las rondas nuevas y los perros.
- 🎬 **Cinemática de llegada** en cada mapa, con un soldado hablando por radio y subtítulos: te acercás de noche a una ventana, la cámara recorre la sala y baja hasta tus ojos (se salta con `Espacio`).
- 🔫 **16 armas**, de pistolas a lanzamisiles, incluidas las **armas maravilla** que solo salen en la caja: Pistola de rayos, Wunderwaffe DG-2 (rayos en cadena) y Cañón de trueno.
- 🎁 **Caja misteriosa** (950) con su osito que la hace mudarse, y **Pack-a-Punch** (5000) para mejorar cualquier arma con nombre nuevo.
- 🥤 **Bebidas**: Quick Revive, Juggernog, Speed Cola, Double Tap y Pola Veloz.
- ⚡ **Interruptor de la corriente**: sin él, casi todas las bebidas y el Pack-a-Punch están apagados.
- 🐺 **Rondas de perros del infierno** con niebla y rayos.
- 🧟 Zombis normales, **brutos** y **explosivos**, que arrancan las tablas de las ventanas para entrar (mantené **E** para repararlas).
- 💥 **Power-ups**: Munición máxima, Muerte instantánea, Doble puntos y Bomba nuclear.
- 🦴 **Física**: los zombis caen como muñecos de trapo según dónde y con qué les das, y las cajas y barriles se pueden empujar.
- 🏆 **Récords** por mapa y **opciones** (sensibilidad, volumen, FOV, gráficos rápidos, FPS) que se guardan solas.

## 🎮 Controles

| Tecla | Acción | Tecla | Acción |
|---|---|---|---|
| `W A S D` | Moverse | `Ratón` | Apuntar |
| `Clic izq.` | Disparar | `Clic der.` | Mirilla |
| `Shift` | Correr | `Espacio` | Saltar |
| `C` | Agacharse (corriendo: deslizarse) | `V` | Cuchillo |
| `G` | Granada | `R` | Recargar |
| `E` | Comprar / abrir / reparar (mantener) | `1 2 3` / rueda | Cambiar arma |
| `M` | Música | `F3` | Gráficos rápidos |
| `F11` | Pantalla completa | `Esc` | Pausa y opciones |

## 🛠️ Compilar

El juego compila con un **g++ portátil (w64devkit)** y **raylib 5.5**, que no están en el repo por su tamaño.

1. Descargá [w64devkit](https://github.com/skeeto/w64devkit/releases) y extraelo en `cpp/tools/w64devkit/`.
2. Descargá el código de [raylib 5.5](https://github.com/raysan5/raylib/releases/tag/5.5), extraelo en `cpp/tools/raylib-5.5/` y compilá la librería (`make` dentro de `src/`).
3. Ejecutá:

```bat
cd cpp
build.bat
```

El ejecutable queda en `cpp/UltimaRonda/UltimaRonda.exe`. Copiá la carpeta `cpp/assets` al lado del `.exe` para jugar.

> **Requisitos:** Windows 10/11 y una gráfica con OpenGL 3.3.

### Prototipo web

La primera versión del juego (*Noche de Zombis*) está hecha con **Three.js**: abrí `index.html` en el navegador y listo.

## 📁 Estructura

```
├── index.html, game.js, style.css   → prototipo web (Three.js)
└── cpp/
    ├── src/        → código del juego en C++ (render, mundo, armas, zombis, ragdoll, HUD, audio)
    ├── assets/     → modelos, texturas, sonidos, música y fuentes
    ├── test/       → herramientas de prueba y preparación de assets
    └── build.bat   → script de compilación
```

## 🙏 Créditos

Todos los recursos externos son libres (CC0 u OFL):

- **Zombi y animaciones**: "Male City Zombie" de Rikindle3D (OpenGameArt)
- **Perro del infierno**: "Wolf" de Quaternius (poly.pizza)
- **Texturas**: [ambientCG](https://ambientcg.com)
- **Armas y granada**: "Guns & Explosives" de 3DModelsCC0
- **Sonidos**: The Free Firearm Sound Library, SpringySpringo, zer0_sol, artisticdude, Darsycho y pauliuw (OpenGameArt)
- **Música**: "Insistent" de yd y "Abandoned passages" de congusbongus (OpenGameArt)
- **Fuentes**: Oswald y Creepster (Google Fonts, SIL OFL)
- **Motor**: [raylib](https://www.raylib.com) (zlib)

---

<div align="center">

Hecho con 🩸 por **madebyfrank**

</div>
