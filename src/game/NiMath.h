#pragma once

#include "core/Types.h"

// Die Rechentypen aus Oblivions Gamebryo-Szenengraph.
//
// Bewusst getrennt von GameTypes.h: diese Typen bestehen nur aus float und
// haben auf jeder Architektur dasselbe Layout. Damit lassen sich Rotation und
// Quaternion-Mathematik nativ auf dem Entwicklungsrechner testen, waehrend
// GameTypes.h mit seinen zeigerbehafteten Strukturen 32-Bit voraussetzt.

namespace obvr {

struct NiPoint3 {
	float x;
	float y;
	float z;
};

static_assert(sizeof(NiPoint3) == 0x0C, "NiPoint3 muss 0x0C gross sein");

// Zeilen-Hauptordnung: data[Zeile][Spalte].
struct NiMatrix33 {
	float data[3][3];

	static NiMatrix33 Identity() {
		NiMatrix33 result{};
		result.data[0][0] = 1.0f;
		result.data[1][1] = 1.0f;
		result.data[2][2] = 1.0f;
		return result;
	}

	NiMatrix33 operator*(const NiMatrix33& rhs) const {
		NiMatrix33 result{};
		for (int row = 0; row < 3; ++row) {
			for (int col = 0; col < 3; ++col) {
				result.data[row][col] =
					data[row][0] * rhs.data[0][col] +
					data[row][1] * rhs.data[1][col] +
					data[row][2] * rhs.data[2][col];
			}
		}
		return result;
	}
};

static_assert(sizeof(NiMatrix33) == 0x24, "NiMatrix33 muss 0x24 gross sein");

struct NiTransform {
	NiMatrix33 rot;    // 0x00
	NiPoint3 pos;      // 0x24
	float scale;       // 0x30
};

static_assert(sizeof(NiTransform) == 0x34, "NiTransform muss 0x34 gross sein");

struct NiBound {
	NiPoint3 center;
	float radius;
};

static_assert(sizeof(NiBound) == 0x10, "NiBound muss 0x10 gross sein");

}  // namespace obvr
