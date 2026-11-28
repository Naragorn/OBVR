#pragma once

// The shield bash's watch in the game (game/ShieldBashWatch.h): fed by
// CameraHook's StepShieldBash every frame it runs, it costs nothing while the
// shield moves slowly; while it moves fast near someone it reads the nearest
// one's bound, and it logs one line per attempt:
//
//   Shield bash attempt: <end> - <frames> frames, the shield up to <m/s> (its
//   controller <m/s>); closest at <actor>: the approach <judged> m/s towards
//   their middle over the last <window> s, judged (the bash wants <gate>); in
//   that frame alone the controller <m/s> towards their middle (<ground> across
//   the ground alone), <across> units across and <up> up from their middle (the
//   column <wide> wide, <high> high)
//
// The approach (camera/hands/ShieldBashApproach.h) since 2026-11-28 13:55: the
// run's refused jabs were fast before the column and slowing in it.
//
// Added 2026-11-28 for Nadi's bash that "did not hit" an imp: the log of that
// run cannot tell a shield outside the imp's column from one too slow towards
// it, as StepShieldBash said nothing when it refused.

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::camera::hands {

struct ShieldBashLook {
	NiPoint3 at{0.0f, 0.0f, 0.0f};                  // the shield's middle in the world
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};            // the drawn shield hand's, world axes, m/s
	NiPoint3 controllerVelocity{0.0f, 0.0f, 0.0f};  // its controller's
	void* actor = nullptr;                          // the one whose column the shield's middle is in
	NiPoint3 centre{0.0f, 0.0f, 0.0f};              // that one's bound's centre
	bool cooling = false;                           // that one cools down from a shove
	bool handBusy = false;
	float towards = 0.0f;                           // the gate's: the controller's approach (ShieldBashApproach)
	float gateSpeed = 0.0f;                         // [Hands] StaggerSpeed, the bash's
	bool landed = false;
};

void TraceShieldBash(const ShieldBashLook& look);

}  // namespace obvr::camera::hands
