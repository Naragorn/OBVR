#pragma once

// The blade's contact with a person judged each frame for whether it goes
// in (game/BladeEntryLogic.h; docs/blade-penetration-spec.md steps 1-2):
// the material where it touches (game/BladeMaterial.h, the fitted
// capsules' fields), the contact point's motion, the pressure while held.
// Each contact's verdict said once in the log ("Blade entry:" lines), and
// kept for what follows it (the cut marks, the slash's depth).

#include "game/BladeContactLogic.h"
#include "game/BladeEntryLogic.h"
#include "game/BodyPart.h"

namespace obvr::game {

struct BladeEntryNow {
	BladeEntryVerdict verdict;
	UInt32 actor = 0;
	BodyPart part = BodyPart::Column;
	BodyMaterial material = BodyMaterial::Unknown;
	UInt8 rating = 0;
	NiPoint3 point{0.0f, 0.0f, 0.0f};   // where the blade touches
	NiPoint3 normal{0.0f, 0.0f, 1.0f};  // the surface's, outward
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};  // the contact point's, units a second
	float along = 0.0f;
	int capsule = -1;  // in the frame's bodies
};

// After StepBladeContact, on its verdict: `from` the drawn pose the step
// began at, `weaponType` the drawn weapon's (a blade: 0 and 1). Answers the
// verdict with a slash that went in sunk into them (SlashSinkOffset).
BladeContactVerdict StepBladeEntryContact(const BladeEntrySettings& settings, BladeContactVerdict v,
                                          const BladeContactFrame& f, const BladePose& from, SInt32 weaponType,
                                          float unitsPerMetre);

// The contact as the last step judged it.
const BladeEntryNow& CurrentBladeEntry();

}  // namespace obvr::game
