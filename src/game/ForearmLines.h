#pragma once

// The forearms' lines for the fists' block by contact (Nadi, 2026-11-27: "go
// same for ... fäuste"): with the fists readied (nothing in the weapon slot,
// the weapon state drawn), each arm's line in the world from its forearm bone
// ("Bip01 R/L Forearm": the elbow's end of the forearm) to its knuckles
// ("Bip01 R/L Finger2", the hand's span's far end - HandSpanFromBones). Read
// once a frame from game/BlockCone's SetBlockGuard, apart from the hands'
// contact step. That the first-person skeleton names its forearms "Bip01 R/L
// Forearm" is the Bethesda skeleton's naming as the code uses "Bip01 L
// ForearmTwist" for the shield [derived, not seen in a log yet].
//
// And whether an attacker is armed, as the hit handler asks it: the
// attacker's process's equipped weapon (vtable +0xEC with 1, the getter the
// handler calls at 0x005FEFA9). A hand-to-hand block against an armed attacker
// or an arrow stops nothing (0x005FF890 `fldz`, game/ParryLogic.h).

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

struct ForearmLines {
	bool rightValid = false;
	NiPoint3 rightA{0.0f, 0.0f, 0.0f};  // the elbow's end
	NiPoint3 rightB{0.0f, 0.0f, 0.0f};  // the knuckles
	bool leftValid = false;
	NiPoint3 leftA{0.0f, 0.0f, 0.0f};
	NiPoint3 leftB{0.0f, 0.0f, 0.0f};
};

// Once a frame: the fists readied and both forearms' bones found; else
// nothing valid.
ForearmLines ReadForearmLines();

// The last frame's reading, for the hand mode's next frame.
bool ForearmLinesRead();

// Whether the attacker holds a weapon (true also for an arrow or anything
// that is not an actor with a readable process - nothing a fist stops).
bool AttackerArmedOrUnplaced(UInt32 attacker);

}  // namespace obvr::game
