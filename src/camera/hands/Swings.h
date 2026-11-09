#pragma once

// The swings and what they do: a swing become a power attack grunts once
// (vr::GruntDue), a swing that may strike swishes once as it starts
// (game::SwishDue), the weapon's lag measured for the log, the drawn weapon
// held where it meets a wall, the strike by motion of the weapon hand
// (game/MeleeHits.h), the arrow in the drawing hand that stabs the same way,
// the left fist that punches the same way, and the fatigue a swing costs
// once it ends (game/MeleeHit.h).
//
// Moved out of camera/CameraHook.cpp's UpdateHandMode on 2026-11-01
// (docs/god-functions-plan.md, phase 2 step 2.18): the code as it was, its
// thirteen statics now SwingsState's members; the contacts' state it reads
// (the blade through a wall, the bodies collected, the blade's span) named
// in SwingsLinks. Harness: swing-fatigue.txt, swing-lengths.txt,
// left-punch.txt, hit-*.txt.

#include "camera/hands/HandStep.h"
#include "game/BladeBodies.h"
#include "game/BladeContactLogic.h"
#include "game/MeleeSweep.h"
#include "vr/HalfSwordLogic.h"

namespace obvr::camera::hands {

struct SwingsState {
	// The block's function statics, as they were.
	UInt32 lastGrunted = 0;
	UInt32 lastSwished = 0;
	UInt32 gapSerial = 0;
	float gapMetres = 0.0f;
	float gapDegrees = 0.0f;
	UInt32 gapLines = 6;
	UInt32 gapAfter = 0;  // frames still watched after the swing
	UInt32 saidSerial = 0;
	UInt32 thrustLines = 40;
	UInt32 lastLeftSwished = 0;
	UInt32 lastLeftGrunted = 0;
	UInt32 leftSwingLines = 40;
	UInt32 fatigueLines = 200;
	// The strikes' frame stamp, one up each frame the strikes step runs
	// (StepStrikesOfHands): the touch sweeps from the frame before's blade
	// only (game/MeleeSweep.h).
	UInt32 strikeFrame = 0;
	game::StrikeSweeps sweeps;
};

// What the swings read of the contacts' step this frame.
struct SwingsLinks {
	bool bladeThrough;                      // the drawn blade went through a wall (let go)
	bool weaponHandThrough;                 // the weapon hand itself through a wall
	bool offHandThrough;                    // the off hand through a wall
	game::BladeBodies& bladeBodies;         // the capsules the weapon's step collected
	game::BladeBodies& handBodies;          // the capsules the hands' step collected
	const game::BladeSpanCache& bladeSpan;  // the drawn weapon's span, for the touch
	// The right hand off the handle (half-swording, vr::BladeStrikeOf): the
	// engine's strike takes its blade from the hand and the reach, which no
	// longer match the sword, so it stands aside - the swish, the grunt and the
	// swing's fatigue stay (Nadi, 2026-11-01: "halfswording sollte auch schwung
	// geräusche machen"); the hilt strikes instead (CameraHook StepHiltStrike).
	// Both hands on the blade with the tip ahead (TipByTouch), the steel ahead
	// of the right fist strikes by touch, as it is seen: tipFrom to tipTo.
	vr::BladeStrike bladeStrike;
	bool tipSpanValid;
	NiPoint3 tipFrom;
	NiPoint3 tipTo;
};

// Before the contacts' step: the grunt, the swish and the lag measured.
void StepSwingSounds(SwingsState& state, const HandStep& step, const HandShared& shared);

// After it: the strike by motion, the arrow's stab, the left fist, the fatigue.
void StepStrikes(SwingsState& state, const HandStep& step, const HandShared& shared, const SwingsLinks& links);

}  // namespace obvr::camera::hands
