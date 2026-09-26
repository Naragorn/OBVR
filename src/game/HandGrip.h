#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// The fingers closed around what the hand holds (docs/holding-objects-spec.md,
// part 1). The first-person hand's finger bones take whatever the animation
// gives them - an open hand while an object floats at it, the magician's
// look (2026-09-26). While the engine holds an object for a hand, each finger
// bone under that hand is turned by a fixed curl about its own z axis, on
// top of the rotation it had when the hold began; when the hold ends, that
// rotation is put back.
//
// Found by name, not listed: every node under the hand bone whose name holds
// "Finger" is a finger link, whatever the skeleton calls each one. On a Bip01
// skeleton a link runs along its own x and bends about its z (3ds Max Biped's
// finger axes, not confirmed on Oblivion's first-person skeleton - hence the
// curl's sign is a setting: a negative [Hands] GripCurlDegrees bends the
// other way).

// Whether this hand's fingers close: the engine holds an object, and it is
// this hand that grabbed it.
inline bool HandGripWanted(bool rightHand, bool engineHolding, bool grabWithLeftHand) {
	return engineHolding && (rightHand != grabWithLeftHand);
}

// How far one link bends. The thumb ("Finger0", its links "Finger01",
// "Finger02") half as far, so it rests across the object instead of
// folding into the palm.
inline float FingerCurlDegrees(const char* boneName, float curlDegrees) {
	if (boneName == nullptr) {
		return curlDegrees;
	}
	for (const char* at = boneName; *at != '\0'; ++at) {
		if ((at[0] == 'F' || at[0] == 'f') && (at[1] == 'i' || at[1] == 'I') &&
		    (at[2] == 'n' || at[2] == 'N') && (at[3] == 'g' || at[3] == 'G') &&
		    (at[4] == 'e' || at[4] == 'E') && (at[5] == 'r' || at[5] == 'R')) {
			return at[6] == '0' ? 0.5f * curlDegrees : curlDegrees;
		}
	}
	return curlDegrees;
}

// A link's rotation bent by `degrees` about its own z: base * Rz.
inline NiMatrix33 CurledAboutZ(const NiMatrix33& base, float degrees) {
	const float radians = degrees * (math::kPi / 180.0f);
	const float c = math::Cos(radians);
	const float s = math::Sin(radians);
	NiMatrix33 turn = NiMatrix33::Identity();
	turn.data[0][0] = c;
	turn.data[0][1] = -s;
	turn.data[1][0] = s;
	turn.data[1][1] = c;
	return base * turn;
}

// Once per frame, after the hand bone has been pinned: closes the named
// hand's fingers when `closed`, puts them back when not.
void StepHandGrip(bool rightHand, const char* handBoneName, bool closed, float curlDegrees);

// Forgets the fingers found (a new model), without writing anything.
void ForgetHandGrip();

}  // namespace obvr::game
