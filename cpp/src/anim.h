#pragma once
#include "raylib.h"

// Convierte una animación de un archivo glTF (solo esqueleto) en una ModelAnimation de raylib
// adaptada al esqueleto del modelo. inPlace: el personaje no se desplaza (la IA lo mueve).
bool BakeAnimation(const Model &model, const char *modelPath, const char *animPath, float fps, bool inPlace, ModelAnimation *outAnim, const char *animName = nullptr);
#include <vector>
// Matrices inversas de unión (skinning glTF estándar: hueso = inversa de unión * pose)
bool LoadInverseBind(const char *path, std::vector<Matrix> &out);
