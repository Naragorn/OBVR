#pragma once

// Holding a two-handed weapon with both hands (the tester, 2026-09-30: "bei 2
// händern mit der linken hand meine hand an das schwert/axt/whatever
// andocken kann mit grip und so zweihändig halte").
//
// The weapon hand - the game's right hand, the dominant controller (the
// roles swap with [Hands] LeftHanded) - holds and steers the weapon as always.
// The other grip closed on its handle - near the line of the weapon, below the
// weapon hand - takes hold, and that hand goes to the weapon, never the weapon
// to it (the tester, 2026-09-30: "sie sollte aber immer zugunsten der
// dominaten hand bleiben ... die linke hand muss dann einen sprung an den
// griff machen statt andersherum"; the build before turned the weapon onto the
// line between the hands).
//
// On the handle the hand is drawn the way the game's own two-handed animation
// holds it - its bone's pose against the weapon, fingers and all - only moved
// along the handle to where it took hold (the tester, 2026-09-30: "wir
// brauchen die vanilla hand die wirklich exakt den griff greift nur eben an
// der position die wir wollten"; the first build kept the controller's own
// hand turn at the line, and the hand floated in the handle). It does not
// jump there: over blendSeconds it moves from the controller to the handle and
// its fingers close from their own pose into the game's grip, and back the
// same way when it lets go (the tester: "sanfte bewegung der hand an den griff
// ... mit allen fingern auch die sich langsam zu einem griff formen ... ähnlich
// wie alyx"). Letting go of the grip, sheathing, or pulling the hands apart
// ends it.
//
// Distances along the weapon are game units along its axis (the Weapon
// node's +y), from the right palm, negative towards the pommel.
//
// Pure, covered by two_hand_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::vr {

struct TwoHandSettings {
	bool enabled = true;
	// The left hand takes hold within this many game units of the weapon's
	// line (70 a metre)...
	float reachUnits = 12.0f;
	// ...below the right hand, at least minUnits and, when the handle could
	// not be measured, at most behindUnits.
	float behindUnits = 25.0f;
	float minUnits = 5.0f;
	// The hands this much further apart or closer than when it took hold: let go.
	float slackUnits = 30.0f;
	// The handle measured from the weapon's model: the left palm stays this far
	// inside its pommel end (half a hand), and a grip up to overhangUnits past
	// the end still takes the handle (it is put on the end).
	float endInsetUnits = 3.0f;
	float overhangUnits = 6.0f;
	// How long the hand takes from the controller to the handle, and back.
	// 0: at once.
	float blendSeconds = 0.2f;
	// The hand coming near the handle with its grip open already takes the
	// grip's shape, more the nearer it is (the tester, 2026-09-30: "je näher
	// ich komme umso mehr geht die ingame hand schonmal in die passende
	// handposition" - the way Half-Life: Alyx shapes a hand before it
	// closes): nothing from preshapeUnits away from the handle, all of it at
	// preshapeNearUnits. 0: off.
	float preshapeUnits = 18.0f;
	float preshapeNearUnits = 3.0f;
};

// The way from the controller to the handle, 0 to 1: up while held, down
// after, by the frame's share of blendSeconds.
inline float StepTwoHandBlend(float current, bool held, float dtSeconds, const TwoHandSettings& s) {
	if (!(s.blendSeconds > 0.0f)) {
		return held ? 1.0f : 0.0f;
	}
	const float step = dtSeconds > 0.0f ? dtSeconds / s.blendSeconds : 0.0f;
	const float next = held ? current + step : current - step;
	return next < 0.0f ? 0.0f : (next > 1.0f ? 1.0f : next);
}

// That way eased: starting and arriving gently (smoothstep), the share of the
// handle's pose the hand and its fingers take.
inline float TwoHandBlendWeight(float blend) {
	const float t = blend < 0.0f ? 0.0f : (blend > 1.0f ? 1.0f : blend);
	return t * t * (3.0f - 2.0f * t);
}

// The share once held: from where the approach had already shaped the hand
// (`floor`, PreshapeWeight when the grip closed) the rest of the way, so
// closing the grip near the handle does not drop the hand back first.
inline float HeldWeight(float floor, float blend) {
	const float f = floor < 0.0f ? 0.0f : (floor > 1.0f ? 1.0f : floor);
	return f + (1.0f - f) * TwoHandBlendWeight(blend);
}

// How much of the grip's shape a hand `distanceUnits` from the handle takes
// while its grip is open (preshapeUnits): eased, 0 far off, 1 at the handle.
inline float PreshapeWeight(const TwoHandSettings& s, float distanceUnits) {
	if (!s.enabled || !(s.preshapeUnits > s.preshapeNearUnits) || !(distanceUnits == distanceUnits)) {
		return 0.0f;
	}
	return TwoHandBlendWeight((s.preshapeUnits - distanceUnits) / (s.preshapeUnits - s.preshapeNearUnits));
}

// The game's own left hand on a two-hander, read from its first-person
// two-handed animation while the left hand is not held: the left hand bone's
// rotation and position in the Weapon node's frame (game units, not scaled),
// and where the two palms sit along the weapon's axis. In the game's
// twohandidle.kf the left hand bone sits 8.3 units down the axis from the
// Weapon node and 6.8 from it, the right one 2.1 down (read with pyffi,
// 2026-09-30): the left hand right under the right one, around the handle.
struct VanillaGrip {
	bool valid = false;
	NiMatrix33 rot = NiMatrix33::Identity();
	NiPoint3 pos{0.0f, 0.0f, 0.0f};
	float leftPalmAxial = 0.0f;   // in the weapon's frame, from the Weapon node
	float rightPalmAxial = 0.0f;
	float leftPalmLateral = 0.0f;  // the left palm's distance from the axis
	// The left palm's place along the weapon from the right palm: where the
	// animation puts the hand, and the highest the left hand is put.
	float Below() const { return leftPalmAxial - rightPalmAxial; }
};

// How close to the weapon's axis the game's own left palm has to be for its
// pose to count as holding the handle - a fist's half-width and a little.
// Not [Hands] TwoHandReachUnits (how close the player's hand has to come):
// widened to 60 for a harness run, that let a frame of the draw animation
// with the palm 29 units off the axis pass as the game's grip (2026-09-30).
inline constexpr float kGamePalmOnHandleUnits = 9.0f;

// From the world poses of this frame's animation: the weapon node, the left
// hand bone, and the two palms (PalmCentre of each hand and its middle
// finger). Valid only when it looks like the animation holds the handle with
// both hands: the left palm within `reachUnits` of the axis and below the
// right palm - an animation with the left hand elsewhere gives nothing.
inline VanillaGrip VanillaGripFrom(const NiMatrix33& weaponRot, const NiPoint3& weaponPos, const NiMatrix33& leftRot,
                                   const NiPoint3& leftPos, const NiPoint3& leftPalm, const NiPoint3& rightPalm,
                                   float reachUnits) {
	NiMatrix33 inverse{};
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			inverse.data[r][c] = weaponRot.data[c][r];
		}
	}
	VanillaGrip g;
	g.rot = inverse * leftRot;
	g.pos = inverse * (leftPos - weaponPos);
	const NiPoint3 left = inverse * (leftPalm - weaponPos);
	const NiPoint3 right = inverse * (rightPalm - weaponPos);
	g.leftPalmAxial = left.y;
	g.rightPalmAxial = right.y;
	g.leftPalmLateral = math::Sqrt(left.x * left.x + left.z * left.z);
	g.valid = g.leftPalmLateral <= reachUnits && g.leftPalmAxial < g.rightPalmAxial;
	return g;
}

// The handle below the right hand, along the weapon from the right palm
// (negative, towards the pommel): from the model's pommel end, measured
// (AxialExtentOf) and inset, to where the game's own left hand holds it -
// right under the right hand (the tester, 2026-09-30: "wenn wir irgendwo
// andocken können müssen wir aufpassen das es zum griff der klinge passt ...
// und ich nicht irgendwo in der luft dann halte"). Invalid when the model was
// not measured or the handle has no room below the game's own left hand: the
// hand is then put where the game has it.
struct HandleSpan {
	bool valid = false;
	float low = 0.0f;   // the pommel end, inset
	float high = 0.0f;  // the game's own left hand, right under the right one
};

inline HandleSpan HandleSpanFor(const TwoHandSettings& s, bool measured, float pommelAxial, float highestAxial) {
	HandleSpan h;
	if (!measured || !(pommelAxial == pommelAxial) || !(highestAxial == highestAxial)) {
		return h;
	}
	h.low = pommelAxial + s.endInsetUnits;
	h.high = highestAxial;
	h.valid = h.high >= h.low;
	return h;
}

// Where a point lies against a line through `origin` along the unit vector
// `dir`: how far along it (`axial`, negative behind) and how far from it.
inline void AxialLateral(const NiPoint3& point, const NiPoint3& origin, const NiPoint3& dir, float& axial,
                         float& lateral) {
	const NiPoint3 d = point - origin;
	axial = d.x * dir.x + d.y * dir.y + d.z * dir.z;
	const NiPoint3 off = d - dir * axial;
	lateral = math::Sqrt(off.LengthSquared());
}

struct TwoHandState {
	bool active = false;
	float distance = 0.0f;    // between the controllers when it took hold
	float handAxial = 0.0f;   // where the left palm is put, from the right palm
};

// Whether the left grip, closing now at `axial` along the weapon from the
// right hand and `lateral` from its line, takes hold of the handle: below the
// right hand, down to the measured handle's end (or up to overhangUnits past
// it), else down to behindUnits. Above the right hand is the blade.
inline bool TwoHandTakes(const TwoHandSettings& s, bool twoHandedDrawn, bool leftGripClosedNow, float axial,
                         float lateral, const HandleSpan& handle = HandleSpan{}) {
	if (!s.enabled || !twoHandedDrawn || !leftGripClosedNow || !(lateral <= s.reachUnits) ||
	    !(axial <= -s.minUnits)) {
		return false;
	}
	return handle.valid ? axial >= handle.low - s.overhangUnits : axial >= -s.behindUnits;
}

// Where on the handle the left palm is put, from the right palm: where it
// closed, kept on the measured handle; where the game's own left hand holds
// it (`gameAxial`) when the handle was not measured.
inline float LeftHandAxial(const HandleSpan& handle, float axial, float gameAxial) {
	if (!handle.valid) {
		return gameAxial;
	}
	return axial < handle.low ? handle.low : (axial > handle.high ? handle.high : axial);
}

inline void StartTwoHand(TwoHandState& t, float axial, float handAxial) {
	t.active = true;
	t.distance = axial < 0.0f ? -axial : axial;
	t.handAxial = handAxial;
}

// Whether it still holds this frame; ends it when not (where the hand was put
// is kept, for the way back to the controller).
inline bool TwoHandHolds(TwoHandState& t, const TwoHandSettings& s, bool twoHandedDrawn, bool leftGripDown,
                         float handsApartUnits) {
	if (!t.active) {
		return false;
	}
	const float change = handsApartUnits - t.distance;
	const bool held = s.enabled && twoHandedDrawn && leftGripDown && handsApartUnits == handsApartUnits &&
	                  (change < 0.0f ? -change : change) <= s.slackUnits;
	if (!held) {
		t.active = false;
	}
	return held;
}

inline NiMatrix33 Transposed(const NiMatrix33& m) {
	NiMatrix33 t{};
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			t.data[r][c] = m.data[c][r];
		}
	}
	return t;
}

// The smallest rotation that turns the unit vector `from` onto the unit
// vector `to` (Rodrigues). Opposite vectors turn half round an axis across
// `from`; a vector of no length leaves everything as it is.
inline NiMatrix33 RotationBetween(const NiPoint3& from, const NiPoint3& to) {
	const float fl = math::Sqrt(from.LengthSquared());
	const float tl = math::Sqrt(to.LengthSquared());
	if (!(fl > 1e-6f) || !(tl > 1e-6f)) {
		return NiMatrix33::Identity();
	}
	const NiPoint3 a = from * (1.0f / fl);
	const NiPoint3 b = to * (1.0f / tl);
	const float c = a.x * b.x + a.y * b.y + a.z * b.z;
	const NiPoint3 v{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
	if (c < -0.9999f) {
		// Half round an axis across `a`.
		NiPoint3 axis = (a.x < 0.9f && a.x > -0.9f) ? NiPoint3{0.0f, -a.z, a.y} : NiPoint3{a.z, 0.0f, -a.x};
		axis = axis * (1.0f / math::Sqrt(axis.LengthSquared()));
		NiMatrix33 m{};
		const float u[3] = {axis.x, axis.y, axis.z};
		for (int r = 0; r < 3; ++r) {
			for (int col = 0; col < 3; ++col) {
				m.data[r][col] = 2.0f * u[r] * u[col] - (r == col ? 1.0f : 0.0f);
			}
		}
		return m;
	}
	const float k = 1.0f / (1.0f + c);
	NiMatrix33 m{};
	m.data[0][0] = c + v.x * v.x * k;
	m.data[0][1] = v.x * v.y * k - v.z;
	m.data[0][2] = v.x * v.z * k + v.y;
	m.data[1][0] = v.y * v.x * k + v.z;
	m.data[1][1] = c + v.y * v.y * k;
	m.data[1][2] = v.y * v.z * k - v.x;
	m.data[2][0] = v.z * v.x * k - v.y;
	m.data[2][1] = v.z * v.y * k + v.x;
	m.data[2][2] = c + v.z * v.z * k;
	return m;
}

// The left hand bone's world pose on the handle: the game's grip - the
// handle through the fist where the game's hand has it - at `handAxial`
// along the weapon, turned round the handle as the player's own hand is
// turned (`handRot`, the bone where the controller alone would put it).
//
// The tester, 2026-09-30: "die linke hand hat eine ausrichtung vom controller.
// wir zwingen aber die ingame hand an die waffe mit bestimmter position und
// angle ... Das fühlt sich schlecht an. daher fix die ingame hand bleibt beim
// gleichen angle wie die controller hand". A fist round a handle can turn
// round the handle and nothing else without letting go of it: so the hand
// keeps the controller's turn, and is only tilted - the smallest turn - until
// the handle it would hold lies along the weapon. Held as the game holds it,
// that is no tilt and the game's pose exactly.
struct LeftHandPose {
	NiMatrix33 rot = NiMatrix33::Identity();
	NiPoint3 pos{0.0f, 0.0f, 0.0f};
};

inline LeftHandPose LeftHandOnHandle(const VanillaGrip& grip, float handAxial, const NiMatrix33& weaponRot,
                                     const NiPoint3& weaponPos, const NiMatrix33& handRot) {
	const NiPoint3 up{0.0f, 1.0f, 0.0f};
	const NiMatrix33 gripToHand = Transposed(grip.rot);
	// The handle's direction and the way from the bone to the handle's line,
	// both in the hand bone's own frame (the game's grip).
	const NiPoint3 handleInHand = gripToHand * up;
	const NiPoint3 toLineInHand = gripToHand * NiPoint3{-grip.pos.x, 0.0f, -grip.pos.z};
	const NiPoint3 axis = weaponRot * up;
	LeftHandPose p;
	p.rot = RotationBetween(handRot * handleInHand, axis) * handRot;
	const NiPoint3 onLine = weaponPos + axis * (grip.pos.y + (handAxial - grip.Below()));
	p.pos = onLine - p.rot * toLineInHand;
	return p;
}

// The handle's nearest point to a hand `axial` along the weapon from the right
// hand and `lateral` from its line: on the measured handle, or the game's own
// place when it was not measured; and how far the hand is from it.
struct HandleReach {
	float axial = 0.0f;
	float distance = 0.0f;
};

inline HandleReach NearestOnHandle(const HandleSpan& handle, float gameAxial, float axial, float lateral) {
	HandleReach r;
	r.axial = LeftHandAxial(handle, axial, gameAxial);
	const float along = axial - r.axial;
	r.distance = math::Sqrt(lateral * lateral + along * along);
	return r;
}

}  // namespace obvr::vr
