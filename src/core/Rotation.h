#pragma once

#include "game/NiMath.h"

namespace obvr {

// Rotationsmatrix aus Eulerwinkeln in Grad, aufgebaut aus den drei
// elementaren Achsendrehungen in der Reihenfolge Z * Y * X.
//
// Die Achsenbelegung ist im laufenden Spiel bestaetigt:
//   X = Pitch (hoch und runter schauen)
//   Y = Roll  (Kopf zur Seite neigen)
//   Z = Yaw   (nach links und rechts schauen)
//
// Bewusst frei von Windows-Abhaengigkeiten, damit die Tests sie als Referenz
// fuer die Quaternion-Route heranziehen koennen.
NiMatrix33 EulerToMatrix(float degreesX, float degreesY, float degreesZ);

}  // namespace obvr
