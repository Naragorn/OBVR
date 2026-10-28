#pragma once

// The drawn weapon let go of - Blade & Sorcery's hold (docs/free-weapons-spec.md
// section 5, decisions 3 and 10; section 4.2): with the option on, the weapon
// is in the hand while the hand's hold is closed on it. The hold opening
// decides by where the hand is:
//
//   in a holster zone     sheathed - the ready click, as the holster's reach
//   at the chest spot     into the inventory, unequipped (the stow's place)
//   anywhere else         unequipped and dropped where the hand is, with the
//                         hand's speed: a fall from a still hand, a throw
//                         from a swung one
//
// The hold has to close on the drawn weapon first (armed): a weapon drawn
// from the inventory or the quick menu with the hand open stays in the hand
// until the hand closes on it and opens again. A hold read open for less
// than the release time drops nothing - the flicker guard of controls-spec
// 4.10. It was 0.12 s first, and the drawn hand held the weapon that long
// past the opening (Nadi, Windows, 2026-10-24: "eine Splitsekunde Pause ...
// same beim Loslassen"); 0.03 s since 2026-10-25 - a hold read open for up
// to three frames at 90 Hz still holds on, and every hold-on is said, so a
// flickering controller shows in the log. No flicker of the pull's hold was
// ever logged: closed it reads 1.00 steady (free-weapons-spec 5.4), and its
// hysteresis opens it only below 0.4. A quest item is never dropped (the game's own rule,
// TESForm flag 0x400): it stays in the hand, and a holster still takes it.
//
// The drop is the engine's own: UnequipItem, then the player's RemoveItem
// with its drop flag and the hand's position (TESObjectREFR vtable slot
// 0x40, as the console's Drop command calls it - Oblivion.exe 0x00500650
// read 2026-10-24). The new reference gets its Havok body some frames after
// (the cell loads its model); it is watched until the body is there, and
// then sent off with the hand's speed (game/WeaponDrop.h).
//
// Pure, covered by weapon_release_test; CameraHook reads the world and
// sends the calls.

#include "core/Types.h"
#include "game/GrabPhysics.h"
#include "game/NiMath.h"
#include "vr/Holster.h"

namespace obvr::game {

struct WeaponReleaseSettings {
	bool enabled = true;            // [Hands] WeaponHold
	float releaseSeconds = 0.03f;   // [Hands] WeaponReleaseSeconds: the hold open this long lets go
	float bodyWaitSeconds = 1.0f;   // how long the dropped reference may take to get its body
	float throwStrength = 1.0f;     // [Hands] WeaponThrowStrength: the hand's speed times this, 0 never throws
	float throwSpinStrength = 1.0f; // [Hands] ThrowSpinStrength: the hand's turning speed times this, 0 no spin
};

enum class WeaponReleasePhase : UInt8 {
	Idle,      // no drawn weapon under a closed hold
	Armed,     // the hold closed on the drawn weapon
	Opening,   // the hold opened, the release time running
	Lowering,  // dropped: the engine stays in the ready state, so the empty hand's fists come up - lowered once they show
};

struct WeaponReleaseState {
	WeaponReleasePhase phase = WeaponReleasePhase::Idle;
	float open = 0.0f;       // how long the hold has been open
	bool refusedThisOpening = false;
	float wait = 0.0f;       // Lowering: how long the fists may take to show
	// The hand's speed while the hold is closed, the last kThrowHistory
	// frames, and the fastest of them and the opening frame's: the throw's
	// speed. What the hand does after the opening - the follow-through, a
	// hand swept on - is not the throw. The drop comes releaseSeconds after the opening, and the
	// frames just before it are the follow-through, not the throw (Nadi,
	// Windows, 2026-10-24: "Waffe werfen geht nicht" - all ten drops read
	// 0.00 m/s, the history then held only the last eight frames before the
	// drop).
	VelocityHistory history;
	VelocityHistory spins;   // the hand's turning speed, frame by frame with `history`
	NiPoint3 fastest{0.0f, 0.0f, 0.0f};
	NiPoint3 fastestSpin{0.0f, 0.0f, 0.0f};  // the turning speed in the fastest frame
	ThrowPeaks peaks;  // at the opening, for the log
};

inline constexpr float kWeaponReleaseLowerWaitSeconds = 1.0f;

struct WeaponReleaseInput {
	bool allowed = false;    // in the world, no menu, not dead, no fit running, not mounted
	vr::EquippedKind equipped = vr::EquippedKind::Nothing;
	vr::WeaponSeen seen = vr::WeaponSeen::Unknown;
	bool holdClosed = false;     // the holding hand's hold (the weapon hand's; the other hand's for a bow)
	bool inHolsterZone = false;  // that hand in one of its holster zones
	bool atChest = false;        // that hand at the stow spot
	bool questItem = false;      // the weapon's base form is a quest item
	NiPoint3 handVelocity{0.0f, 0.0f, 0.0f};  // the holding hand's, world units a second
	NiPoint3 handAngularVelocity{0.0f, 0.0f, 0.0f};  // its turning speed, world axes, radians a second
	float dt = 0.0f;
};

struct WeaponReleaseVerdict {
	bool armed = false;      // the hold closed on the drawn weapon, this frame
	bool opened = false;     // the armed hold read open, this frame: the release time starts
	bool heldOn = false;     // the hold closed again within the release time: kept
	float openSeconds = 0.0f;  // heldOn: how long it read open
	bool sheathe = false;    // let go in a holster zone: the ready click
	bool stow = false;       // let go at the chest: unequip, it stays in the inventory
	bool drop = false;       // let go elsewhere: unequip and drop at the hand
	bool refused = false;    // a quest item let go elsewhere: it stays in the hand
	bool cancelled = false;  // the weapon went, or a menu came, while armed
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};  // drop: the throw's, world units a second (ThrowVelocity)
	NiPoint3 spin{0.0f, 0.0f, 0.0f};      // drop: the throw's spin, radians a second (ThrowSpin)
	ThrowPeaks peaks;  // drop: the hand's peaks before the opening (ThrowPeaksOf)
	bool lowerFists = false; // after a drop the slot is empty and the game shows the fists up: the ready click lowers them (the tester, 2026-10-24: the hand is open, no fist belongs there)
};

inline bool WeaponReleaseLive(const WeaponReleaseInput& in) {
	return in.allowed && in.equipped != vr::EquippedKind::Nothing && in.seen == vr::WeaponSeen::Drawn;
}

inline WeaponReleaseVerdict StepWeaponRelease(WeaponReleaseState& s, const WeaponReleaseInput& in,
                                              const WeaponReleaseSettings& settings) {
	WeaponReleaseVerdict v;
	if (s.phase == WeaponReleasePhase::Lowering) {
		// The drop is through; the slot shows empty a frame or two later,
		// and the game keeps its ready state: fists up on an open hand.
		if (!settings.enabled || !in.allowed) {
			s = WeaponReleaseState{};
			return v;
		}
		if (in.equipped == vr::EquippedKind::Nothing && in.seen == vr::WeaponSeen::Drawn) {
			v.lowerFists = true;
			s = WeaponReleaseState{};
			return v;
		}
		if (in.equipped == vr::EquippedKind::Nothing && in.seen == vr::WeaponSeen::Sheathed) {
			s = WeaponReleaseState{};
			return v;
		}
		s.wait -= in.dt == in.dt && in.dt > 0.0f ? in.dt : 0.0f;
		if (s.wait <= 0.0f) {
			s = WeaponReleaseState{};
		}
		return v;
	}
	if (!settings.enabled || !WeaponReleaseLive(in)) {
		v.cancelled = settings.enabled && s.phase != WeaponReleasePhase::Idle;
		s = WeaponReleaseState{};
		return v;
	}
	switch (s.phase) {
	case WeaponReleasePhase::Idle:
		if (in.holdClosed) {
			s.phase = WeaponReleasePhase::Armed;
			s.history = VelocityHistory{};
			s.spins = VelocityHistory{};
			PushVelocity(s.history, in.handVelocity, in.dt);
			PushVelocity(s.spins, in.handAngularVelocity, in.dt);
			v.armed = true;
		}
		return v;
	case WeaponReleasePhase::Armed:
		PushVelocity(s.history, in.handVelocity, in.dt);
		PushVelocity(s.spins, in.handAngularVelocity, in.dt);
		if (!in.holdClosed) {
			v.opened = true;
			s.phase = WeaponReleasePhase::Opening;
			s.open = 0.0f;
			s.refusedThisOpening = false;
			const UInt32 best = FastestSample(s.history);
			s.fastest = s.history.count > 0 ? s.history.v[best] : NiPoint3{0.0f, 0.0f, 0.0f};
			s.fastestSpin = best < s.spins.count ? s.spins.v[best] : NiPoint3{0.0f, 0.0f, 0.0f};
			s.peaks = ThrowPeaksOf(s.history);
		}
		return v;
	case WeaponReleasePhase::Opening:
	case WeaponReleasePhase::Lowering:
		break;
	}
	if (in.holdClosed) {
		v.heldOn = true;
		v.openSeconds = s.open;
		s.phase = WeaponReleasePhase::Armed;
		PushVelocity(s.history, in.handVelocity, in.dt);
		PushVelocity(s.spins, in.handAngularVelocity, in.dt);
		return v;
	}
	s.open += in.dt == in.dt && in.dt > 0.0f ? in.dt : 0.0f;
	if (s.open < settings.releaseSeconds) {
		return v;
	}
	if (in.inHolsterZone) {
		v.sheathe = true;
	} else if (in.atChest) {
		v.stow = true;
	} else if (in.questItem) {
		if (!s.refusedThisOpening) {
			v.refused = true;
			s.refusedThisOpening = true;
		}
		// Idle with the hand open: the next closing arms again. The flag
		// keeps the refusal to one line until then.
		s.phase = WeaponReleasePhase::Idle;
		return v;
	} else {
		v.drop = true;
		VelocityHistory taken;
		PushVelocity(taken, s.fastest);
		v.velocity = ThrowVelocity(taken, settings.throwStrength);
		v.spin = ThrowSpin(v.velocity, s.fastestSpin, settings.throwSpinStrength);
		v.peaks = s.peaks;
		s = WeaponReleaseState{};
		s.phase = WeaponReleasePhase::Lowering;
		s.wait = kWeaponReleaseLowerWaitSeconds;
		return v;
	}
	s = WeaponReleaseState{};
	return v;
}

// The dropped weapon passes through the player and the hands until it is clear
// of them. It comes into the world where the hand is, into the hand's
// keyframed bodies, and a hand moving on - as a real hand does when it opens
// - knocked it away with the hand's whole speed though OBVR gave it none
// (weapon-drop-sweep.txt, 2026-10-25: sent off at 0 units/s, then 551, 370,
// 238 units/s, 23 units on in eight frames; Nadi, Windows, 2026-10-24: "Waffe
// loslassen ist zu viel Force"). So its body takes the player's collision
// group at the send-off - the hands' bodies are in it (game/HandBodies.cpp) -
// and gets its own back once its bound is a hand's margin away from both
// tracked hands and out of the player's capsule on the horizontal, the way a
// held object's group waits (game/GrabPhysics.h, InsidePlayerCapsule).
inline constexpr float kDroppedHandClearUnits = 12.0f;  // about a hand's length

inline bool DroppedWeaponClear(const NiPoint3& centre, float radius, const NiPoint3& player, float capsuleUnits,
                               bool rightValid, const NiPoint3& right, bool leftValid, const NiPoint3& left) {
	const float r = radius > 0.0f ? radius : 0.0f;
	const float reach = r + kDroppedHandClearUnits;
	if (rightValid && (right - centre).LengthSquared() < reach * reach) {
		return false;
	}
	if (leftValid && (left - centre).LengthSquared() < reach * reach) {
		return false;
	}
	return !(capsuleUnits > 0.0f && InsidePlayerCapsule(centre, r, player, capsuleUnits));
}

// The log's word for what a release did; null when it did nothing.
inline const char* WeaponReleaseWord(const WeaponReleaseVerdict& v) {
	if (v.sheathe) {
		return "let go at the holster - sheathed";
	}
	if (v.stow) {
		return "let go at the chest - into the inventory";
	}
	if (v.drop) {
		return "let go - dropped where the hand is";
	}
	if (v.refused) {
		return "let go, but a quest item cannot be dropped - it stays in the hand";
	}
	return nullptr;
}

// The dropped reference, watched until its Havok body is there: the engine
// builds the model and the body some frames after the drop (hand-script-
// harness.md: a sword from player.drop gets its body frames later), and the
// hand's speed can only go onto a body. The speed is kept from the moment of
// the release.
struct DroppedWeaponState {
	UInt32 ref = 0;
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};  // world units a second
	NiPoint3 spin{0.0f, 0.0f, 0.0f};      // radians a second, world axes
	float wait = 0.0f;
	UInt32 frames = 0;
};

struct DroppedWeaponVerdict {
	bool sendOff = false;  // the body is there: give it `velocity`
	bool gaveUp = false;   // no body within the wait
	UInt32 ref = 0;
	UInt32 frames = 0;
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};
	NiPoint3 spin{0.0f, 0.0f, 0.0f};
};

inline void BeginDroppedWeapon(DroppedWeaponState& d, UInt32 ref, const NiPoint3& velocity, float waitSeconds,
                               const NiPoint3& spin = NiPoint3{0.0f, 0.0f, 0.0f}) {
	d = DroppedWeaponState{};
	d.ref = ref;
	d.velocity = velocity;
	d.spin = spin;
	d.wait = waitSeconds == waitSeconds && waitSeconds > 0.0f ? waitSeconds : 1.0f;
}

inline DroppedWeaponVerdict StepDroppedWeapon(DroppedWeaponState& d, bool hasBody, float dt) {
	DroppedWeaponVerdict v;
	if (d.ref == 0) {
		return v;
	}
	++d.frames;
	v.ref = d.ref;
	v.frames = d.frames;
	v.velocity = d.velocity;
	v.spin = d.spin;
	if (hasBody) {
		v.sendOff = true;
		d = DroppedWeaponState{};
		return v;
	}
	d.wait -= dt == dt && dt > 0.0f ? dt : 0.0f;
	if (d.wait <= 0.0f) {
		v.gaveUp = true;
		d = DroppedWeaponState{};
	}
	return v;
}

}  // namespace obvr::game
