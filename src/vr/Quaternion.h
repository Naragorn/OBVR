#pragma once

#include "game/NiMath.h"

namespace obvr::vr {

// OpenXR liefert Orientierungen als Quaternion. Oblivion rechnet mit
// 3x3-Matrizen in einem anderen Koordinatensystem. Dieser Header haelt beides
// auseinander und die Umrechnung an einer Stelle.

struct Quaternion {
	float x;
	float y;
	float z;
	float w;

	static Quaternion Identity() { return Quaternion{0.0f, 0.0f, 0.0f, 1.0f}; }

	// Die Inverse einer Einheitsquaternion. Wird fuer Recenter gebraucht:
	// die gespeicherte Referenzorientierung wird aus der aktuellen
	// herausgerechnet.
	Quaternion Conjugate() const { return Quaternion{-x, -y, -z, w}; }

	// Hintereinanderausfuehrung. Erst rhs, dann this.
	Quaternion operator*(const Quaternion& rhs) const;

	// Gleitkommadrift ueber viele Frames laesst die Laenge weglaufen; eine
	// nicht normierte Quaternion ergaebe eine skalierende Kameramatrix.
	Quaternion Normalized() const;

	float LengthSquared() const { return x * x + y * y + z * z + w * w; }
};

// Baut eine Quaternion aus Achse und Winkel. Vor allem fuer Tests und den
// simulierten Kopf.
Quaternion FromAxisAngle(float axisX, float axisY, float axisZ, float degrees);

// Rechnet eine Orientierung aus OpenXR in Oblivions Kameraraum um.
//
// Die beiden Koordinatensysteme unterscheiden sich in der Achsenbelegung:
//
//     OpenXR                   Oblivion / Gamebryo
//     X = rechts               X = rechts
//     Y = oben                 Y = vorne
//     Z = hinten (-Z vorne)    Z = oben
//
// Der Basiswechsel bildet also ab:
//
//     x_obl =  x_xr
//     y_obl = -z_xr
//     z_obl =  y_xr
//
// Die zugehoerige Matrix hat Determinante +1, die Haendigkeit bleibt somit
// erhalten und der Vektorteil der Quaternion laesst sich direkt umsetzen.
//
// Gegenprobe an den im Spiel bestaetigten Achsen: eine Drehung um OpenXRs
// Y-Achse (nach links und rechts schauen) wird zu einer Drehung um Oblivions
// Z-Achse, und Z ist dort das Yaw. Eine Drehung um OpenXRs X-Achse bleibt X
// und damit Pitch.
Quaternion FromOpenXR(const Quaternion& openXrOrientation);

// Wandelt eine Quaternion in die Rotationsmatrix um, die Oblivion erwartet.
// Erwartet eine bereits normierte Quaternion in Oblivion-Konvention.
NiMatrix33 ToMatrix(const Quaternion& rotation);

}  // namespace obvr::vr
