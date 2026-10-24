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
// 4.10 (0.12 s first). A quest item is never dropped (the game's own rule,
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
#include "game/NiMath.h"
#include "vr/Holster.h"

namespace obvr::game {

struct WeaponReleaseSettings {
	bool enabled = true;            // [Hands] WeaponHold
	float releaseSeconds = 0.12f;   // [Hands] WeaponReleaseSeconds: the hold open this long lets go
	float bodyWaitSeconds = 1.0f;   // how long the dropped reference may take to get its body
	float throwStrength = 1.0f;     // [Hands] WeaponThrowStrength: the hand's speed times this, 0 never throws
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
	float dt = 0.0f;
};

struct WeaponReleaseVerdict {
	bool armed = false;      // the hold closed on the drawn weapon, this frame
	bool sheathe = false;    // let go in a holster zone: the ready click
	bool stow = false;       // let go at the chest: unequip, it stays in the inventory
	bool drop = false;       // let go elsewhere: unequip and drop at the hand
	bool refused = false;    // a quest item let go elsewhere: it stays in the hand
	bool cancelled = false;  // the weapon went, or a menu came, while armed
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
			v.armed = true;
		}
		return v;
	case WeaponReleasePhase::Armed:
		if (!in.holdClosed) {
			s.phase = WeaponReleasePhase::Opening;
			s.open = 0.0f;
			s.refusedThisOpening = false;
		}
		return v;
	case WeaponReleasePhase::Opening:
	case WeaponReleasePhase::Lowering:
		break;
	}
	if (in.holdClosed) {
		s.phase = WeaponReleasePhase::Armed;
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
		s = WeaponReleaseState{};
		s.phase = WeaponReleasePhase::Lowering;
		s.wait = kWeaponReleaseLowerWaitSeconds;
		return v;
	}
	s = WeaponReleaseState{};
	return v;
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
	float wait = 0.0f;
	UInt32 frames = 0;
};

struct DroppedWeaponVerdict {
	bool sendOff = false;  // the body is there: give it `velocity`
	bool gaveUp = false;   // no body within the wait
	UInt32 ref = 0;
	UInt32 frames = 0;
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};
};

inline void BeginDroppedWeapon(DroppedWeaponState& d, UInt32 ref, const NiPoint3& velocity, float waitSeconds) {
	d = DroppedWeaponState{};
	d.ref = ref;
	d.velocity = velocity;
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
