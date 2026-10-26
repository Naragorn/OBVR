#pragma once

// The torch let go of (Nadi, 2026-10-26: "Fackeln: wenn in der hand (egal ob
// inventar oder in welt aufgenommen) kann ich sie jederzeit in die
// Spielwelt auf den boden oder sonstwo droppen durch loslassen an der hand
// und zwar lit also noch brennend"). As the drawn weapon's hold
// (game/WeaponRelease.h), for the other hand's torch: the hold closing on the
// torch in the hand arms it, the hold opening past the release time lets it
// go, decided by where the hand is:
//
//   in a holster zone     nothing here - the holster's own close stows it
//                         there (vr::StepHolster, HolsterShieldTorch)
//   at the chest spot     into the inventory, unequipped (the stow's place)
//   anywhere else         unequipped and dropped where the hand is, with the
//                         hand's speed, still burning (game/TorchFire.h)
//
// A hold that is someone else's - a grab of a world object or a person by
// the other hand, the weapon's handle in it - never arms it, and taken
// while armed it cancels: letting go of what the hand grabbed is no drop of
// the torch.
//
// Pure, covered by torch_release_test; CameraHook reads the hand and sends
// the calls.

#include "core/Types.h"
#include "game/GrabPhysics.h"
#include "game/NiMath.h"

namespace obvr::game {

struct TorchReleaseSettings {
	bool enabled = true;           // [Hands] TorchDrop
	float releaseSeconds = 0.03f;  // [Hands] WeaponReleaseSeconds, the weapon's
	float throwStrength = 1.0f;    // [Hands] WeaponThrowStrength, the weapon's
	float throwSpinStrength = 1.0f;  // [Hands] ThrowSpinStrength, the weapon's
};

enum class TorchReleasePhase : UInt8 {
	Idle,     // no torch under a closed hold of its own
	Armed,    // the hold closed on the torch
	Opening,  // the hold opened, the release time running
};

struct TorchReleaseState {
	TorchReleasePhase phase = TorchReleasePhase::Idle;
	float open = 0.0f;
	// The hand's speed while the hold is closed, and the fastest of it at the
	// opening: the throw's (the weapon's lesson - the frames after the
	// opening are the follow-through).
	VelocityHistory history;
	VelocityHistory spins;
	NiPoint3 fastest{0.0f, 0.0f, 0.0f};
	NiPoint3 fastestSpin{0.0f, 0.0f, 0.0f};
};

struct TorchReleaseInput {
	bool allowed = false;      // in the world, no menu, not dead, not mounted, the hand tracked
	bool torchInHand = false;  // the torch on the other hand's bone
	bool holdClosed = false;   // the other hand's hold
	bool holdIsTaken = false;  // that hold is a grab's or the weapon handle's
	bool inHolsterZone = false;
	bool atChest = false;
	NiPoint3 handVelocity{0.0f, 0.0f, 0.0f};  // world units a second
	NiPoint3 handAngularVelocity{0.0f, 0.0f, 0.0f};  // world axes, radians a second
	float dt = 0.0f;
};

struct TorchReleaseVerdict {
	bool armed = false;
	bool opened = false;
	bool heldOn = false;
	float openSeconds = 0.0f;
	bool holster = false;    // let go at a holster zone: the holster's to stow
	bool stow = false;       // let go at the chest: unequip into the inventory
	bool drop = false;       // let go elsewhere: drop it burning at the hand
	bool cancelled = false;  // the torch went, a menu came or the hold was taken while armed
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};  // drop: the throw's, world units a second
	NiPoint3 spin{0.0f, 0.0f, 0.0f};      // drop: the throw's spin, radians a second
};

inline TorchReleaseVerdict StepTorchRelease(TorchReleaseState& s, const TorchReleaseInput& in,
                                            const TorchReleaseSettings& settings) {
	TorchReleaseVerdict v;
	const bool live = settings.enabled && in.allowed && in.torchInHand && !in.holdIsTaken;
	if (!live) {
		v.cancelled = s.phase != TorchReleasePhase::Idle;
		s = TorchReleaseState{};
		return v;
	}
	switch (s.phase) {
	case TorchReleasePhase::Idle:
		if (in.holdClosed) {
			s.phase = TorchReleasePhase::Armed;
			s.history = VelocityHistory{};
			s.spins = VelocityHistory{};
			PushVelocity(s.history, in.handVelocity);
			PushVelocity(s.spins, in.handAngularVelocity);
			v.armed = true;
		}
		return v;
	case TorchReleasePhase::Armed:
		PushVelocity(s.history, in.handVelocity);
		PushVelocity(s.spins, in.handAngularVelocity);
		if (!in.holdClosed) {
			v.opened = true;
			s.phase = TorchReleasePhase::Opening;
			s.open = 0.0f;
			const UInt32 best = FastestSample(s.history);
			s.fastest = s.history.count > 0 ? s.history.v[best] : NiPoint3{0.0f, 0.0f, 0.0f};
			s.fastestSpin = best < s.spins.count ? s.spins.v[best] : NiPoint3{0.0f, 0.0f, 0.0f};
		}
		return v;
	case TorchReleasePhase::Opening:
		break;
	}
	if (in.holdClosed) {
		v.heldOn = true;
		v.openSeconds = s.open;
		s.phase = TorchReleasePhase::Armed;
		PushVelocity(s.history, in.handVelocity);
		PushVelocity(s.spins, in.handAngularVelocity);
		return v;
	}
	s.open += in.dt == in.dt && in.dt > 0.0f ? in.dt : 0.0f;
	if (s.open < settings.releaseSeconds) {
		return v;
	}
	if (in.inHolsterZone) {
		v.holster = true;
	} else if (in.atChest) {
		v.stow = true;
	} else {
		v.drop = true;
		VelocityHistory taken;
		PushVelocity(taken, s.fastest);
		v.velocity = ThrowVelocity(taken, settings.throwStrength);
		v.spin = ThrowSpin(v.velocity, s.fastestSpin, settings.throwSpinStrength);
	}
	s = TorchReleaseState{};
	return v;
}

}  // namespace obvr::game
