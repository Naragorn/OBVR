#pragma once

// The torch's blow, the other hand with the torch in it striking someone
// (game/TorchBlowLogic.h, game/TorchTouchLogic.h; free-weapons-spec 5.9 and
// 5.15):
// - by its touch: where the torch's shaft meets someone's body - the
//   capsules the hands' contact step collected this frame - at the hand's
//   speed into them there (the tester, 2026-11-26: "gegner mit fackel links
//   schlagen ging manchmal nicht" - a chop onto a rat met it and did nothing);
// - landed as the speed makes it, or held to the end of that hand's swing
//   with [Hands] TorchKnockdownPowerOnly (a knockdown only for a power swing);
// - at the hand, as the off hand's punch (CameraHook StepShoves), when no
//   bodies were collected (HandsStopAtBodies off) or the shaft is not read.
//
// Moved out of camera/CameraHook.cpp on 2026-11-26 with the blow by touch:
// the held blows, their settling and the landing as they were.

#include "core/Config.h"
#include "core/Types.h"
#include "game/BodyShapes.h"
#include "game/NiMath.h"
#include "game/RefFingerprint.h"
#include "game/ShoveLogic.h"
#include "game/TorchTouchLogic.h"
#include "vr/HandMode.h"

namespace obvr::camera::hands {

// A blow met before its swing was a power one, held to the swing's end.
struct HeldTorchBlow {
	void* actor = nullptr;
	game::RefFingerprint print;  // a body can go while its blow is held
	UInt32 serial = 0;           // the other hand's swing that met them
	NiPoint3 from{0.0f, 0.0f, 0.0f};
	NiPoint3 centre{0.0f, 0.0f, 0.0f};
	float speed = 0.0f;    // into them (or towards them, by the hand) as it met them, m/s
	float metres = 0.0f;   // the swing's length then
	float seconds = 0.0f;  // held so far
};

inline constexpr UInt32 kHeldTorchBlows = 4;

struct TorchBlowState {
	HeldTorchBlow held[kHeldTorchBlows];
	UInt32 heldLines = 30;
	game::TorchPose before;  // the shaft at the controller's pose the frame before
	UInt32 touchLines = 30;  // the blows by touch, landed or too slow
	float quietSeconds = 0.0f;  // after a too-slow line, a second of none
	// The touch's cost, timed while someone is near the torch
	UInt32 costFrames = 0;
	double costTicks = 0.0;
	long long costLongest = 0;
	UInt32 costLines = 8;
};

// What the torch's steps read and change of the frame.
struct TorchBlowLinks {
	const vr::HandModeResult& hand;
	game::ShoveCooldown& cooldown;
	bool (*pulse)(bool device, float seconds, float hertz, float amplitude);  // the blow's buzz
	bool rolesSwapped;  // left-handed: the other hand is the right controller
};

// Whether `actor`'s blow of the current swing is held already.
bool TorchBlowHeldFor(const TorchBlowState& s, const void* actor);

// The held blows whose swing has come to a verdict: landed as it was.
void SettleTorchBlows(TorchBlowState& s, const game::ShoveSettings& settings, const TorchBlowLinks& links, float dt);

// The torch's blow on `actor`, `bySpeed` as its speed made it (never None):
// held to the swing's end when TorchKnockdownPowerOnly wants a power swing
// that is not one yet, else landed - the engine's fist blow as a torch, the
// stagger or the knockdown, the hand-to-hand hit's sound at `at`, a buzz.
// `speed` is the blow's (m/s); `byTouch` says how it was found, for the log.
void LandTorchBlow(TorchBlowState& s, const Config& config, const TorchBlowLinks& links, void* actor,
                   game::ShoveKind bySpeed, const NiPoint3& at, const NiPoint3& from, const NiPoint3& centre,
                   float speed, bool byTouch);

// The torch's blow by touch this frame: `now` the shaft at the controller's
// pose (StepTorchShaft), `bodies` the living the hands' step collected,
// `hand` the other hand's velocity and turn in the world. `byTouch`: the
// bodies were collected this frame and the shaft is read - else nothing is
// tested (the hand's own rule strikes instead). `allowed`: the off hand may
// strike (OffHandStrikes, not on a two-hander's handle, nobody held). The
// pose is kept for the next frame's sweep either way.
void StepTorchTouch(TorchBlowState& s, const Config& config, const TorchBlowLinks& links,
                    const game::BladeBodies& bodies, bool byTouch, const game::TorchPose& now,
                    const game::TorchHand& hand, bool allowed, float dt);

}  // namespace obvr::camera::hands
