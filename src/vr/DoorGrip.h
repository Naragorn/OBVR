#pragma once

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::vr {

// Doors by grip ([Hands] DoorsByGrip, on by default; docs/door-grip-spec.md
// option B): an empty hand on a door's leaf at handle height closes its
// grip, then moves 12 cm along the leaf's normal - pulled or pushed - and the
// door opens as A opens it (TESObjectREFR::Activate on the player's behalf,
// game::ActivateByPlayer): a load door loads, a locked one gets the engine's
// own lock handling, owners and crime as with A. A keeps working.
//
// What the engine does not say, and so is not decided here: which side the
// hinge is on (a door model has no handle node and no hinge mark; the zone
// is the leaf's whole width at handle height) and which way the door
// swings (its open animation lives in its model; pull and push both open).
// This decides when, from plain values, so every flow is tested
// (door_grip_test).

struct DoorGripSettings {
	bool enabled = true;      // [Hands] DoorsByGrip
	float moveMetres = 0.12f; // [Hands] DoorGripMoveMetres: the pull or push that opens
	float offLeafMetres = 0.15f;  // a hand this near either face of the leaf is on it
};

inline constexpr float kDoorLeastMoveMetres = 0.05f;
inline constexpr float kDoorMostMoveMetres = 0.40f;
// A leaf: its thinnest horizontal side this thin at most, and at least this
// high (a hatch or a lid is not a door to pull).
inline constexpr float kDoorMostThickMetres = 0.35f;
inline constexpr float kDoorLeastHighMetres = 1.0f;
// The handle's band: 0.7 to 1.4 m above the leaf's foot, or 35 to 80 % of
// a leaf too low for it; a hand this far beside the leaf's edge still on it.
inline constexpr float kHandleLowMetres = 0.7f;
inline constexpr float kHandleHighMetres = 1.4f;
inline constexpr float kHandleLowShare = 0.35f;
inline constexpr float kHandleHighShare = 0.80f;
inline constexpr float kHandleEdgeMetres = 0.05f;

// The threshold as used: an INI value out of 5..40 cm (or not a number)
// clamped, 12 cm for a non-number.
inline float DoorMoveMetresFor(float metres) {
	if (!(metres == metres)) {
		return 0.12f;
	}
	return metres < kDoorLeastMoveMetres ? kDoorLeastMoveMetres
	                                     : (metres > kDoorMostMoveMetres ? kDoorMostMoveMetres : metres);
}

// The door model's box in its own frame, in metres (z up, as a placed door
// stands).
struct DoorBoxMetres {
	NiPoint3 low{0.0f, 0.0f, 0.0f};
	NiPoint3 high{0.0f, 0.0f, 0.0f};
};

// Which of x (0) or y (1) the leaf is thin along: the thinner horizontal
// side, if it is a leaf at all - thin enough, clearly thinner than wide,
// and high enough. -1 for none.
inline int DoorThinAxis(const DoorBoxMetres& box) {
	const float w = box.high.x - box.low.x;
	const float d = box.high.y - box.low.y;
	const float h = box.high.z - box.low.z;
	if (!(w > 0.0f) || !(d > 0.0f) || !(h >= kDoorLeastHighMetres)) {
		return -1;
	}
	const int axis = d < w ? 1 : 0;
	const float thin = axis == 1 ? d : w;
	const float wide = axis == 1 ? w : d;
	if (thin > kDoorMostThickMetres || thin > 0.5f * wide) {
		return -1;
	}
	return axis;
}

// The handle's height band in the box's frame.
inline void HandleBand(const DoorBoxMetres& box, const DoorGripSettings&, float& lowZ, float& highZ) {
	const float h = box.high.z - box.low.z;
	if (h >= kHandleHighMetres + 0.2f) {
		lowZ = box.low.z + kHandleLowMetres;
		highZ = box.low.z + kHandleHighMetres;
	} else {
		lowZ = box.low.z + kHandleLowShare * h;
		highZ = box.low.z + kHandleHighShare * h;
	}
}

// Whether a hand at `p` (the door's frame, metres) is at the handle: within
// the leaf's width, in the band, and this near either face. `normal` is its
// coordinate along the leaf's thin axis, which the pull and the push move.
inline bool InHandleZone(const NiPoint3& p, const DoorBoxMetres& box, const DoorGripSettings& s, float& normal) {
	const int axis = DoorThinAxis(box);
	if (axis < 0) {
		return false;
	}
	float lowZ = 0.0f;
	float highZ = 0.0f;
	HandleBand(box, s, lowZ, highZ);
	if (p.z < lowZ || p.z > highZ) {
		return false;
	}
	const float along = axis == 1 ? p.x : p.y;
	const float alongLow = axis == 1 ? box.low.x : box.low.y;
	const float alongHigh = axis == 1 ? box.high.x : box.high.y;
	if (along < alongLow - kHandleEdgeMetres || along > alongHigh + kHandleEdgeMetres) {
		return false;
	}
	normal = axis == 1 ? p.y : p.x;
	const float faceLow = axis == 1 ? box.low.y : box.low.x;
	const float faceHigh = axis == 1 ? box.high.y : box.high.x;
	const float off = normal < faceLow ? faceLow - normal : (normal > faceHigh ? normal - faceHigh : 0.0f);
	return off <= s.offLeafMetres;
}

// Whether a hand may take a door: tracked, holding nothing, no weapon
// readied - as vr::HandFreeToOpen, less the grip (the grip is what takes it).
inline bool HandFreeForDoor(bool valid, bool holdsSomething, bool weaponReadied) {
	return valid && !holdsSomething && !weaponReadied;
}

enum class DoorGripPhase : UInt8 {
	Idle = 0,
	Holding,  // a grip closed on the handle
	Done,     // opened; waits for that grip to open
};

enum class DoorGripAction : UInt8 {
	None = 0,
	Take,   // the grip took the handle: a tick
	Open,   // pulled or pushed past the threshold: activate the door
	LetGo,  // let go before it opened
};

struct DoorGripState {
	DoorGripPhase phase = DoorGripPhase::Idle;
	UInt32 ref = 0;
	bool left = false;
	float startNormalMetres = 0.0f;
	bool gripWas[2] = {false, false};
};

// One frame's view. [0] the right hand, [1] the left.
struct DoorGripFrame {
	bool enabled = false;
	bool menuUp = false;
	bool gripDown[2] = {false, false};
	bool handFree[2] = {false, false};
	// The door a hand is at the handle of now (0 none), which hand, and
	// that hand's normal coordinate (InHandleZone).
	UInt32 candidate = 0;
	bool candidateLeft = false;
	float candidateNormalMetres = 0.0f;
	bool itemFirst = false;  // an item under that hand goes first (vr::ItemGoesFirst)
	// The held door: its holding hand's normal coordinate this frame, or
	// gone (deleted, unloaded).
	bool heldKnown = false;
	float heldNormalMetres = 0.0f;
	bool heldGone = false;
};

struct DoorGripVerdict {
	DoorGripAction action = DoorGripAction::None;
	UInt32 ref = 0;
	bool left = false;
};

inline DoorGripVerdict StepDoorGrip(DoorGripState& st, const DoorGripSettings& s, const DoorGripFrame& f) {
	DoorGripVerdict v;
	const bool closed[2] = {f.gripDown[0] && !st.gripWas[0], f.gripDown[1] && !st.gripWas[1]};
	st.gripWas[0] = f.gripDown[0];
	st.gripWas[1] = f.gripDown[1];
	const int held = st.left ? 1 : 0;
	switch (st.phase) {
	case DoorGripPhase::Idle: {
		const int side = f.candidateLeft ? 1 : 0;
		if (f.enabled && !f.menuUp && f.candidate != 0 && closed[side] && f.handFree[side] && !f.itemFirst) {
			st.phase = DoorGripPhase::Holding;
			st.ref = f.candidate;
			st.left = f.candidateLeft;
			st.startNormalMetres = f.candidateNormalMetres;
			v.action = DoorGripAction::Take;
			v.ref = st.ref;
			v.left = st.left;
		}
		return v;
	}
	case DoorGripPhase::Holding: {
		v.ref = st.ref;
		v.left = st.left;
		if (!f.enabled || f.menuUp || f.heldGone || !f.gripDown[held] || !f.handFree[held]) {
			st = DoorGripState{DoorGripPhase::Idle, 0, false, 0.0f, {st.gripWas[0], st.gripWas[1]}};
			v.action = DoorGripAction::LetGo;
			return v;
		}
		if (!f.heldKnown) {
			return DoorGripVerdict{};
		}
		const float moved = f.heldNormalMetres - st.startNormalMetres;
		if (moved >= s.moveMetres || -moved >= s.moveMetres) {
			st.phase = DoorGripPhase::Done;
			v.action = DoorGripAction::Open;
			return v;
		}
		return DoorGripVerdict{};
	}
	case DoorGripPhase::Done:
		if (!f.gripDown[held]) {
			st = DoorGripState{DoorGripPhase::Idle, 0, false, 0.0f, {st.gripWas[0], st.gripWas[1]}};
		}
		return v;
	}
	return v;
}

}  // namespace obvr::vr
