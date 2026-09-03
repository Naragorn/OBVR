#pragma once

#include "core/Types.h"

namespace obvr::game {

// The aim set inside the engine call that reads it.
//
// A spell leaves along the caster's rotation fields, an arrow along the
// shooter's heading and pitch, and a swing is tested against the attacker's
// heading - and the player walks along the same rotZ, elsewhere. For a long
// time OBVR aimed by turning the body for the few frames around the moment
// that mattered, and what remained of that was a few frames of walking
// pulled towards the aim and a turn the engine animated, there and back.
// This puts the heading where it is read instead: on the way INTO the call,
// the player's rotX and rotZ are set to the gaze; on the way OUT they are
// put back. No frame ever sees the change, so nothing is walked, animated
// or compensated.
//
// One call covers all three. The animation-key handler on the actor's
// MagicCaster base (addr::kAnimationKeyHandler, with the reading of the
// binary behind it) is where the bow's release makes the arrow from the
// shooter's heading and pitch, where the cast key calls UseActiveMagicItem,
// and where the hit key calls AttackHandling - each inside one invocation.
// The player's own vtable slot is re-pointed at a stub that runs OBVR before
// and after it (core/AroundCall.h), so NPCs never pass through here at all.
//
// The slot is checked before it is written, and a slot that does not hold
// what this build expects is left alone and reported; the turn machinery
// then keeps its old job, see AimAtSourceInstalled.
//
// Installed once at load. The write is a vtable slot that needs no player
// to exist yet; the callback refuses anything that is not the player.
void InstallAimAtSource();

// Whether the hook went in. The turn's ownership of attack actions is
// switched by this, so a refused slot keeps the old mechanism rather than
// losing the aim.
bool AimAtSourceInstalled();

// What the callback writes, decided every frame by the camera pass: whether
// the swap is wanted at all, the head's turn away from where the body faces
// in radians (the engine's own reckoning, so it goes straight through
// camera::PlayerYawForGaze), and the pitch to write in the engine's
// convention, positive looking DOWN.
struct AimSourcePose {
	bool wanted = false;
	float headYaw = 0.0f;
	float pitch = 0.0f;
};

void SetAimSourcePose(const AimSourcePose& pose);

// The hand-tracked mode's grab: while wanted, the per-frame grab update
// (kGrabUpdate, called from the grab handler at kCallGrabUpdate) runs with
// the player's rotation swapped to the aim pose above - the hand's
// direction - AND the grab distance at kPlayerGrabDistanceOffset set to the
// given game units, both put back on the way out. The engine then builds
// the spring's target from the player's eye vector and that distance, so
// the grabbed object hovers where the hand is; move the hand fast and let
// go, and the object keeps the spring's velocity - vanilla's own fling.
void SetGrabAtHand(bool wanted, float distanceUnits);

}  // namespace obvr::game
