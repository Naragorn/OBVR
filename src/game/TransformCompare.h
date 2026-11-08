#pragma once

#include "game/NiMath.h"

namespace obvr::game {

// Whether two local transforms are the same to the bit: the booking's test
// that a bone still holds what was written to it (BodyIk.cpp).
inline bool SameTransform(const NiTransform& a, const NiTransform& b) {
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			if (a.rot.data[r][c] != b.rot.data[r][c]) {
				return false;
			}
		}
	}
	return a.pos.x == b.pos.x && a.pos.y == b.pos.y && a.pos.z == b.pos.z && a.scale == b.scale;
}

}  // namespace obvr::game
