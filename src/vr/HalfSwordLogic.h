#pragma once

// Half-swording with a two-handed sword (Nadi, 2026-11-01: "Bauen wir nun half
// swording nur bei 2 hand Schwertern. Es gibt eine neue Greifposition an der
// klinge mit links gleich wie beim griff. Mit Annäherung der hand."). The left
// hand takes the blade as it takes the handle (vr/TwoHandLogic.h): shaped on
// the way in, closed in the game's own grip, slid with the trigger held. The
// blade is a second span of the handle above the right hand
// (HandleSpan::above, a staff's way), from past the guard to short of the tip.
// Two-handed blades only (weapon type 1): not a one-hander, not a blunt
// two-hander, not a staff (whose whole shaft is a handle already).
//
// Distances are game units along the weapon from the right palm, as in
// TwoHandLogic.h.
//
// Pure, covered by half_sword_test.

#include "core/Types.h"
#include "game/HandGrip.h"
#include "game/PommelLogic.h"
#include "vr/TwoHandLogic.h"

namespace obvr::vr {

struct HalfSwordSettings {
	bool enabled = true;          // [Hands] HalfSwording
	float fromUnits = 12.0f;      // the blade from this far above the right palm (past the guard)
	float tipInsetUnits = 8.0f;   // to this far short of the tip
	// The right hand may let go and take the blade too (step 2).
	bool rightHand = true;        // [Hands] HalfSwordRightHand
};

// The weapon type is a two-handed blade (0 Blade1H, 1 Blade2H, 2 Blunt1H,
// 3 Blunt2H, 4 Staff, 5 Bow).
inline bool HalfSwordWeapon(const HalfSwordSettings& s, SInt32 weaponType) { return s.enabled && weaponType == 1; }

// The handle with the blade added above the right hand: from fromUnits up to
// the tip (`tipAxial`, from the right palm) less tipInsetUnits. Unchanged when
// the handle was not measured, the tip is not a number, or the blade is too
// short to leave room between the two.
inline HandleSpan WithBlade(const HalfSwordSettings& s, HandleSpan h, float tipAxial) {
	if (!h.valid || !(tipAxial == tipAxial)) {
		return h;
	}
	const float from = s.fromUnits > 0.0f ? s.fromUnits : HalfSwordSettings{}.fromUnits;
	const float inset = s.tipInsetUnits >= 0.0f ? s.tipInsetUnits : HalfSwordSettings{}.tipInsetUnits;
	const float high = tipAxial - inset;
	if (!(high > from)) {
		return h;
	}
	h.above = true;
	h.aboveLow = from;
	h.aboveHigh = high;
	return h;
}

// ---- Step 2: the right hand on the blade too ("Wenn man dort hält kann man
// mit rechts auch an der klinge festhalten auch mit Annäherung weiter oben.
// Wenn man dann zuhaut ist es auch taumeln und umfallen je nach speed.")
//
// The weapon hangs on the right hand (the engine's one weapon slot). With the
// left hand on the blade, the right may let go: the weapon is then carried by
// the left hand (Free) - its pose against the left hand kept from the moment
// the right let go - until the right closes again: on the blade above the left
// hand (Blade; the weapon moved in the right hand so that hand holds the blade
// there, either way round) or on the handle (Handle, the game's hold again).
// Each change moves the weapon over the two-hand grip's blendSeconds.
//
// With both hands on the blade the weapon is a club held by its blade: the
// hilt - pommel and guard - strikes as the pommel strike does (a stagger, a
// fall when hard), and the engine's blade strike is set aside (its blade is
// taken from the hand and the reach, which no longer match the weapon).

enum class RightHold : UInt8 {
	Handle,  // the game's hold: the weapon where the animation hangs it
	Free,    // the right hand open, the weapon carried by the left hand
	Blade,   // the right hand on the blade, the weapon moved in it
};

struct RightHoldState {
	RightHold hold = RightHold::Handle;
	float openSeconds = 0.0f;  // how long the right hold has read open
	float gripAxial = 0.0f;    // Blade: where the right palm holds it, from the handle's palm along the weapon
	float blend = 1.0f;        // 0 to 1: the weapon's way from its last pose to this hold's
};

struct RightHoldInput {
	bool halfSwordDrawn = false;  // a two-handed sword drawn, both hands tracked (HalfSwordWeapon)
	bool leftHolds = false;       // the left hand holds the weapon (TwoHandState::active)
	float leftAxial = 0.0f;       // where, from the handle's right palm along the weapon (+: the blade)
	bool rightHold = false;       // the right hand's hold closed
	bool rightClosedNow = false;  // and closed this frame
	float rightAxial = 0.0f;      // the right palm from the handle's right palm along the weapon
	float rightLateral = 0.0f;    // and from the weapon's line
	HandleSpan handle;            // with the blade (WithBlade)
	float dt = 0.0f;
};

enum class RightHoldEvent : UInt8 {
	None,
	LetGo,       // the right opened: the left carries the weapon
	TookBlade,   // the right closed on the blade above the left hand
	TookHandle,  // the right closed on the handle again
	Missed,      // the right closed on neither
	Lost,        // nobody holds it any more: the release drops it
	Reset,       // the sword went or was sheathed: the game's hold at once
};

// A fist's width: the right hand holds the blade at least this far from the left.
inline constexpr float kRightClearOfLeftUnits = 7.0f;

// Whether the right hand opening must not let the weapon go: the left holds
// it on the blade, or carries it already.
inline bool WeaponHeldByOther(const RightHoldState& st, const RightHoldInput& in,
                              const HalfSwordSettings& s = HalfSwordSettings{}) {
	return s.enabled && s.rightHand && in.halfSwordDrawn && in.leftHolds &&
	       (st.hold != RightHold::Handle || in.leftAxial > 0.0f);
}

// Whether the engine's blade strike (from the hand and the reach) holds: only
// with the right hand on the handle, where the weapon is the game's.
inline bool EngineStrikes(const RightHoldState& st) { return st.hold == RightHold::Handle; }

// Whether the hilt strikes: the right hand on the blade.
inline bool HiltStrikes(const RightHoldState& st) { return st.hold == RightHold::Blade; }

// Where the free right hand would take hold, nearest to it: the blade
// anywhere a fist's width clear of the left hand (on the hand's own side of
// it; Nadi in the headset, 2026-11-01, closed the right hand between the guard
// and the left hand every time - "above" meant up from the handle, not past
// the left hand), or the handle at the game's own place (0). How far the hand
// is from that place; nothing when the left hand does not hold the sword.
struct RightPlace {
	bool valid = false;
	bool blade = false;      // the blade, else the handle
	float axial = 0.0f;      // from the handle's right palm along the sword
	float distance = 0.0f;   // the hand from it
};

inline RightPlace NearestRightPlace(const RightHoldInput& in) {
	RightPlace best;
	if (!in.leftHolds || !(in.rightAxial == in.rightAxial) || !(in.rightLateral == in.rightLateral)) {
		return best;
	}
	// `at` the place the hand is measured to, `axial` where it then holds.
	const auto consider = [&](bool blade, float at, float axial) {
		const float along = in.rightAxial - at;
		const float d = math::Sqrt(along * along + in.rightLateral * in.rightLateral);
		if (!best.valid || d < best.distance) {
			best.valid = true;
			best.blade = blade;
			best.axial = axial;
			best.distance = d;
		}
	};
	const HandleSpan& h = in.handle;
	if (h.valid && h.above) {
		float a = ClampedTo(in.rightAxial, h.aboveLow, h.aboveHigh);
		const float gap = a - in.leftAxial;
		if (gap < kRightClearOfLeftUnits && gap > -kRightClearOfLeftUnits) {
			a = in.rightAxial >= in.leftAxial ? in.leftAxial + kRightClearOfLeftUnits
			                                  : in.leftAxial - kRightClearOfLeftUnits;
		}
		if (a >= h.aboveLow && a <= h.aboveHigh) {
			consider(true, a, a);
		}
	}
	// The handle: anywhere from its pommel end up to the blade counts as it (the
	// headset: the hand closed 7 to 12 units up came back to the handle), held
	// at the game's own place.
	const float handleTop = h.valid && h.above ? h.aboveLow : 0.0f;
	const float handleLow = h.valid ? h.low : 0.0f;
	consider(false, ClampedTo(in.rightAxial, handleLow, handleTop), 0.0f);
	return best;
}

inline RightHoldEvent StepRightHold(RightHoldState& st, const HalfSwordSettings& s, const TwoHandSettings& t,
                                    const RightHoldInput& in) {
	const float dt = in.dt == in.dt && in.dt > 0.0f ? in.dt : 0.0f;
	if (!s.enabled || !s.rightHand || !in.halfSwordDrawn) {
		const bool was = st.hold != RightHold::Handle;
		st = RightHoldState{};
		return was ? RightHoldEvent::Reset : RightHoldEvent::None;
	}
	st.openSeconds = in.rightHold ? 0.0f : st.openSeconds + dt;
	const float step = t.blendSeconds > 0.0f ? dt / t.blendSeconds : 1.0f;
	st.blend = st.blend + step > 1.0f ? 1.0f : st.blend + step;
	const bool openLong = !in.rightHold && st.openSeconds >= t.releaseSeconds;
	switch (st.hold) {
	case RightHold::Handle:
		if (openLong && in.leftHolds && in.leftAxial > 0.0f) {
			st.hold = RightHold::Free;
			st.blend = 0.0f;
			return RightHoldEvent::LetGo;
		}
		return RightHoldEvent::None;
	case RightHold::Free: {
		if (!in.leftHolds) {
			st = RightHoldState{};
			return RightHoldEvent::Lost;
		}
		if (!in.rightClosedNow) {
			return RightHoldEvent::None;
		}
		const RightPlace place = NearestRightPlace(in);
		if (!place.valid || !(place.distance <= t.reachUnits)) {
			return RightHoldEvent::Missed;
		}
		if (place.blade) {
			st.hold = RightHold::Blade;
			st.gripAxial = place.axial;
			st.blend = 0.0f;
			return RightHoldEvent::TookBlade;
		}
		st = RightHoldState{};
		st.blend = 0.0f;
		return RightHoldEvent::TookHandle;
	}
	case RightHold::Blade:
		if (openLong) {
			if (in.leftHolds) {
				st.hold = RightHold::Free;
				st.blend = 0.0f;
				return RightHoldEvent::LetGo;
			}
			st = RightHoldState{};
			return RightHoldEvent::Lost;
		}
		return RightHoldEvent::None;
	}
	return RightHoldEvent::None;
}

// ---- The weapon's pose (world or a parent's frame: rotation and position).

struct WeaponPose {
	NiMatrix33 rot = NiMatrix33::Identity();
	NiPoint3 pos{0.0f, 0.0f, 0.0f};
};

// `rel` carried by `parent`: parent * rel.
inline WeaponPose Compose(const WeaponPose& parent, const WeaponPose& rel) {
	WeaponPose p;
	p.rot = parent.rot * rel.rot;
	p.pos = parent.pos + parent.rot * rel.pos;
	return p;
}

// `world` in `parent`'s frame: inverse(parent) * world.
inline WeaponPose RelativeTo(const WeaponPose& parent, const WeaponPose& world) {
	const NiMatrix33 back = Transposed(parent.rot);
	WeaponPose p;
	p.rot = back * world.rot;
	p.pos = back * (world.pos - parent.pos);
	return p;
}

// The weapon held on its blade by a fist: turned the least from `current` so
// its axis (+y) lies along the fist's (`fistAxis`), either way round - against
// it is `reversed`, the hilt beyond the little finger - and the point
// `gripAxial` along it in the fist (`fistPoint`). Its turn round its own axis
// stays as it was.
inline WeaponPose BladeHoldPose(const WeaponPose& current, const NiPoint3& fistAxis, const NiPoint3& fistPoint,
                                float gripAxial, bool* reversed) {
	const NiPoint3 axis = current.rot * NiPoint3{0.0f, 1.0f, 0.0f};
	const bool back = axis.x * fistAxis.x + axis.y * fistAxis.y + axis.z * fistAxis.z < 0.0f;
	if (reversed != nullptr) {
		*reversed = back;
	}
	WeaponPose p;
	p.rot = RotationBetween(axis, back ? fistAxis * -1.0f : fistAxis) * current.rot;
	p.pos = fistPoint - p.rot * NiPoint3{0.0f, gripAxial, 0.0f};
	return p;
}

// The other way round: the hand brought to the sword. The hand (`hand`, its
// bone's world pose) turned the least so the fist's line (`fistAxis`, in the
// bone's frame) lies along the sword's (`swordAxis`, either way round; only
// one way when `eitherWay` is false - the handle is held as the game holds
// it), and moved so the fist's point (`fistPoint`, in the bone's frame) is on
// the sword at `swordPoint`. Its turn round the sword stays the controller's.
inline WeaponPose FistOnBlade(const WeaponPose& hand, const NiPoint3& fistAxis, const NiPoint3& fistPoint,
                              const NiPoint3& swordAxis, const NiPoint3& swordPoint, bool eitherWay) {
	const NiPoint3 axis = hand.rot * fistAxis;
	const bool back = eitherWay && axis.x * swordAxis.x + axis.y * swordAxis.y + axis.z * swordAxis.z < 0.0f;
	WeaponPose p;
	p.rot = RotationBetween(axis, back ? swordAxis * -1.0f : swordAxis) * hand.rot;
	p.pos = swordPoint - p.rot * fistPoint;
	return p;
}

// Between two poses by w (0 to 1), the rotation the shorter way.
inline WeaponPose BlendPose(const WeaponPose& a, const WeaponPose& b, float w) {
	const float t = w < 0.0f ? 0.0f : (w > 1.0f ? 1.0f : w);
	WeaponPose p;
	p.rot = game::BlendRotation(a.rot, b.rot, t);
	p.pos = a.pos + (b.pos - a.pos) * t;
	return p;
}

// ---- The hilt's strike with both hands on the blade: the pommel strike's
// speeds, no lead needed - a swing of the hilt is the blow.
inline game::ShoveKind HiltStrikeFor(const game::PommelSettings& p, bool bladeGrip, float towardsSpeed) {
	if (!p.enabled || !bladeGrip || !(towardsSpeed >= p.speed)) {
		return game::ShoveKind::None;
	}
	return towardsSpeed >= p.hardSpeed ? game::ShoveKind::Hard : game::ShoveKind::Light;
}

}  // namespace obvr::vr
