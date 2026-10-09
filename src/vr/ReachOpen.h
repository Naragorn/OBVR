#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::vr {

// Opening by reaching ([Hands] ReachOpens, on by default): an open, empty
// hand brought to a container, a body, or - sneaking - a living person,
// opens what activate would open, with no button and no wait, and taking
// the hand away closes it again (the tester, 2026-10-09: "wenn ich nah
// genug rangehe mit der hand blendet sich ... das menü ein ... dann hand
// weg -> menü ist weg. ich musste nie A drücken"; "Eine offene, leere Hand
// nah am Container, aber auch nicht kurz gehalten, ich muss mich nur mit
// der hand nähern"). A locked container opens its lockpicking the same
// way, and a lock picked with the hand still there opens the container
// ("wenn das bestanden ist das neue menü"). docs/container-touch-spec.md.
//
// The engine does the opening (TESObjectREFR::Activate, as the A button
// ends in) and the closing (its close-all-menus); this decides when, from
// plain values, so every flow is tested (reach_open_test). It never closes
// a menu it did not open, and never one that is not the container's or the
// lock's - a conversation, a message, an arrest stay as the game put them.

struct ReachOpenSettings {
	bool enabled = true;         // [Hands] ReachOpens
	float openMetres = 0.10f;    // [Hands] ReachOpenMetres: a free hand this near opens
	float closeMetres = 0.45f;   // [Hands] ReachCloseMetres: the hand this far away closes
};

// The close distance as used: beyond the open one, or the hand could never
// leave what it opened - an INI with the close at or under the open (or not
// a number) gets the open plus 0.2 m.
inline float ReachCloseMetresFor(float openMetres, float closeMetres) {
	if (closeMetres > openMetres) {
		return closeMetres;
	}
	return (openMetres > 0.0f ? openMetres : 0.0f) + 0.2f;
}

// What is reached for.
enum class ReachKind : UInt8 {
	None = 0,
	Container,  // a chest, a barrel, a desk ...
	Body,       // a dead actor: looted as a container
	Pocket,     // a living actor: picked, only while sneaking
};

// Whether a thing of this kind may be opened by reaching: containers and
// bodies always; a living person only while the player sneaks (activate
// would otherwise start a conversation) and not while that person fights.
inline bool ReachKindAllowed(ReachKind kind, bool sneaking, bool personInCombat) {
	switch (kind) {
	case ReachKind::Container:
	case ReachKind::Body:
		return true;
	case ReachKind::Pocket:
		return sneaking && !personInCombat;
	default:
		return false;
	}
}

// Whether a hand may open things: tracked, its grip open, not a fist,
// holding nothing, and no weapon (or the fists) readied - a hand on a
// sword hilt or in a fight is not reaching for a chest.
inline bool HandFreeToOpen(bool valid, bool gripDown, bool fist, bool holdsSomething, bool weaponReadied) {
	return valid && !gripDown && !fist && !holdsSomething && !weaponReadied;
}

// Whether an item the same hand is near goes first: when it is nearer to
// the hand than the container is, the hand is after the item (a bottle on
// a desk, a coin on a chest's lid), and the container stays shut.
inline bool ItemGoesFirst(bool haveItem, bool itemSameHand, float itemMetres, float containerMetres) {
	return haveItem && itemSameHand && itemMetres < containerMetres;
}

// From a point to an axis-aligned box (low..high), 0 inside it. For the
// container's own box in its own frame, so a chest turned in the room is
// measured as the chest it is, not as its bound sphere (a chest's sphere
// reaches 0.66 m from its middle).
inline float DistanceToBox(const NiPoint3& p, const NiPoint3& low, const NiPoint3& high) {
	const float dx = p.x < low.x ? low.x - p.x : (p.x > high.x ? p.x - high.x : 0.0f);
	const float dy = p.y < low.y ? low.y - p.y : (p.y > high.y ? p.y - high.y : 0.0f);
	const float dz = p.z < low.z ? low.z - p.z : (p.z > high.z ? p.z - high.z : 0.0f);
	return math::Sqrt(dx * dx + dy * dy + dz * dz);
}

// From a point to a body given by its bones: the nearest bone, less the
// flesh round it (`padUnits`), 0 for a hand on the body. -1 with no bones.
inline float DistanceToBones(const NiPoint3& p, const NiPoint3* bones, UInt32 count, float padUnits) {
	float best = -1.0f;
	for (UInt32 i = 0; i < count; ++i) {
		const float d = math::Sqrt((bones[i] - p).LengthSquared());
		if (best < 0.0f || d < best) {
			best = d;
		}
	}
	if (best < 0.0f) {
		return -1.0f;
	}
	return best > padUnits ? best - padUnits : 0.0f;
}

enum class ReachPhase : UInt8 {
	Idle = 0,     // nothing opened by a reach; watching the hands
	Opening,      // activated; waiting for the menu it brings
	Open,         // the container's menu, opened by the reach, is up
	Lockpicking,  // the lock's minigame, opened by the reach, is up
	Rearm,        // done with this one: the hand has to leave it first
};

constexpr UInt32 kReachOpenWaitFrames = 30;  // for the menu to come after the activation

struct ReachOpenState {
	ReachPhase phase = ReachPhase::Idle;
	UInt32 target = 0;  // the reference reached for
	UInt32 waited = 0;  // frames in Opening
};

struct ReachOpenFrame {
	// The hand mode in the world, first person, the camera known.
	bool active = false;
	bool menuUp = false;
	bool containerOnTop = false;  // the ContainerMenu is the menu on top
	bool lockpickOnTop = false;   // the LockPickMenu is
	bool quantityOnTop = false;   // the quantity popup over the container's menu
	bool moving = false;          // the stick walks the player: passing by opens nothing
	// The nearest thing a free hand is at this frame (0: none), its kind,
	// whether its kind may be opened (ReachKindAllowed), its distance, and
	// whether an item goes first (ItemGoesFirst).
	UInt32 candidate = 0;
	ReachKind kind = ReachKind::None;
	bool candidateAllowed = false;
	float candidateMetres = 0.0f;
	bool itemFirst = false;
	// The target held: its distance from the nearer tracked hand, free or
	// not, and whether it is locked. `targetKnown` false: it could not be
	// measured this frame (no camera on a menu's closing frame, say) - then
	// nothing is decided on its distance. `targetGone`: the reference is no
	// more (disabled, deleted, out of the cell).
	bool targetKnown = false;
	bool targetGone = false;
	float targetMetres = 0.0f;
	bool targetLocked = false;
};

enum class ReachAction : UInt8 {
	None = 0,
	Activate,  // TESObjectREFR::Activate(player) on `ref`
	Close,     // close the menu (the container's or the lock's) the reach opened
};

struct ReachOpenVerdict {
	ReachAction action = ReachAction::None;
	UInt32 ref = 0;
	// Set on every change of phase, for the log: what happened.
	const char* event = nullptr;
};

inline ReachOpenVerdict StepReachOpen(ReachOpenState& s, const ReachOpenSettings& settings, const ReachOpenFrame& f) {
	ReachOpenVerdict v;
	if (!settings.enabled) {
		if (s.phase != ReachPhase::Idle) {
			v.event = "switched off - the menu the reach opened is left to the player";
		}
		s = ReachOpenState{};
		return v;
	}
	const bool handLeft = f.targetKnown && f.targetMetres > settings.closeMetres;
	switch (s.phase) {
	case ReachPhase::Idle:
		if (!f.active || f.menuUp || f.moving || f.candidate == 0 || !f.candidateAllowed || f.itemFirst ||
		    !(f.candidateMetres <= settings.openMetres)) {
			return v;
		}
		s.phase = ReachPhase::Opening;
		s.target = f.candidate;
		s.waited = 0;
		v.action = ReachAction::Activate;
		v.ref = f.candidate;
		v.event = "reached - activated";
		return v;

	case ReachPhase::Opening:
		if (f.menuUp) {
			if (f.containerOnTop) {
				s.phase = ReachPhase::Open;
				v.event = "its menu opened";
				return v;
			}
			if (f.lockpickOnTop) {
				s.phase = ReachPhase::Lockpicking;
				v.event = "its lock's minigame opened";
				return v;
			}
			if (!f.quantityOnTop) {
				s.phase = ReachPhase::Rearm;
				v.event = "another menu came (a conversation, a message) - left to the player";
				return v;
			}
		}
		if (++s.waited > kReachOpenWaitFrames) {
			s.phase = ReachPhase::Rearm;
			v.event = "no menu came (a key needed, no lockpicks, a script's own activation)";
		}
		return v;

	case ReachPhase::Open:
		if (!f.menuUp) {
			s.phase = ReachPhase::Rearm;
			v.event = "its menu was closed by the player or the game";
			return v;
		}
		if (f.quantityOnTop) {
			return v;  // a count being chosen: never closed under it
		}
		if (!f.containerOnTop) {
			s.phase = ReachPhase::Rearm;
			v.event = "another menu went over it (a conversation, an arrest) - left to the game";
			return v;
		}
		if (handLeft) {
			s.phase = ReachPhase::Rearm;
			v.action = ReachAction::Close;
			v.ref = s.target;
			v.event = "the hand left - closed";
		}
		return v;

	case ReachPhase::Lockpicking:
		if (f.menuUp && f.containerOnTop) {
			s.phase = ReachPhase::Open;
			v.event = "the lock gave and its menu opened";
			return v;
		}
		if (!f.menuUp) {
			if (f.active && f.targetKnown && !f.targetLocked && !handLeft) {
				s.phase = ReachPhase::Opening;
				s.waited = 0;
				v.action = ReachAction::Activate;
				v.ref = s.target;
				v.event = "the lock gave - activated again for its menu";
				return v;
			}
			s.phase = ReachPhase::Rearm;
			v.event = f.targetLocked ? "the lock held (cancelled, or the picks broke)"
			                         : "the minigame closed with the hand away";
			return v;
		}
		if (!f.lockpickOnTop) {
			s.phase = ReachPhase::Rearm;
			v.event = "another menu went over the minigame - left to the game";
			return v;
		}
		if (handLeft) {
			s.phase = ReachPhase::Rearm;
			v.action = ReachAction::Close;
			v.ref = s.target;
			v.event = "the hand left the lock - closed";
		}
		return v;

	case ReachPhase::Rearm:
		// Only on a measured leave or a target that is no more: a frame
		// without a camera must not re-arm a hand still at the chest, or the
		// next frame would open it again.
		if (f.targetGone || handLeft) {
			s = ReachOpenState{};
			v.event = "the hand is away - armed again";
		}
		return v;
	}
	return v;
}

inline const char* ReachKindName(ReachKind kind) {
	switch (kind) {
	case ReachKind::Container:
		return "container";
	case ReachKind::Body:
		return "body";
	case ReachKind::Pocket:
		return "pocket";
	default:
		return "none";
	}
}

}  // namespace obvr::vr
