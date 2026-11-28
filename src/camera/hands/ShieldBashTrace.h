#pragma once

// The shield bash's watch in the game (game/ShieldBashWatch.h): fed by
// CameraHook's StepShieldBash every frame it runs, it costs nothing while the
// shield moves slowly; while it moves fast near someone it reads the nearest
// one's bound, and it logs one line per attempt:
//
//   Shield bash attempt: <end> - <frames> frames, the shield up to <m/s> (its
//   controller <m/s>); closest at <actor>: the approach <judged> m/s into their
//   body over the last <window> s, judged (the bash wants <gate>); the shield
//   met them by its disc|by the contact step|not - its disc <gap> units from
//   their column; in that frame alone the controller <m/s> towards their middle
//   (<ground> across the ground alone), <degrees> off the line to it, <across>
//   units across and <up> up from their middle (the column <wide> wide, <high>
//   high)
//
// Added 2026-11-28 for Nadi's bash that "did not hit" an imp; the approach
// since the 13:55 run (jabs fast before the column, slowing in it); the disc
// and the speed into the body since the 14:56 run (game/BashContactLogic.h).

#include "camera/hands/ShieldBashApproach.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::camera::hands {

struct ShieldBashLook {
	BashTouch touch;                                // the disc, the one it meets, the approach (ShieldBashTouch)
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};            // the drawn shield hand's, world axes, m/s
	NiPoint3 controllerVelocity{0.0f, 0.0f, 0.0f};  // its controller's
	bool cooling = false;                           // the one met cools down from a shove
	bool handBusy = false;
	float gateSpeed = 0.0f;                         // [Hands] StaggerSpeed, the bash's
	bool landed = false;
};

void TraceShieldBash(const ShieldBashLook& look);

}  // namespace obvr::camera::hands
