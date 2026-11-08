#pragma once

// Whether a blade goes into a body (docs/blade-penetration-spec.md 3, step
// 2; Nadi, 2026-11-08: into bare skin and light clothing, into leather and
// the like only with a lot of pressure). Blade & Sorcery's way, read on its
// SDK wiki (Damager JSON): penetration by the material met, by speed
// (intensityMinVelocity) or by pressure held against the surface
// (penetrationPressureAllowed, penetrationPressureMaxDot - how square to the
// surface); a pierce goes in along the blade, a slash with the edge leading
// (direction ForwardAndBackward).
//
// Here, for the contact a blade has with a person this frame:
// - a stab: met at the tip (along >= tipShare), the blade within
//   stabMaxDegrees of the surface's inward normal, and the contact point's
//   speed along the blade past the material's - or, held at the surface,
//   the controller pressed past where the blade is held by the material's
//   depth for its time (OBVR has no force: the hand's depth past the
//   surface over time stands in for it);
// - a slash: the contact point moving across the blade into the surface,
//   the edge leading within slashEdgeMaxDegrees (where the weapon's section
//   was read; else the edge is taken to lead), its speed past the
//   material's;
// - heavy armour and a surface not known: never. Light armour's speeds rise
//   with its armour points, to a most.
// Each contact is judged until it ends; once in, it stays in for that
// contact. The numbers are guesses to be tuned in the headset (the spec).
//
// Pure; blade_entry_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/BladeMaterial.h"
#include "game/NiMath.h"
#include "game/SegmentGeometry.h"

namespace obvr::game {

struct BladeEntrySettings {
	bool enabled = true;
	// Speeds in metres a second, by material (Unknown, Skin, Cloth, Light,
	// Heavy); below 0: never.
	float stabSpeed[kBodyMaterialCount] = {-1.0f, 1.5f, 2.0f, 4.0f, -1.0f};
	float slashSpeed[kBodyMaterialCount] = {-1.0f, 2.5f, 3.0f, 5.0f, -1.0f};
	// Light armour: each armour point adds this, at most lightMostExtra.
	float lightPerPoint = 0.1f;
	float lightMostExtra = 2.0f;
	// Pressure: the controller this many units past the held blade, for
	// this long; below 0 units: never.
	float pressUnits[kBodyMaterialCount] = {-1.0f, 2.0f, 3.0f, 6.0f, -1.0f};
	float pressSeconds[kBodyMaterialCount] = {0.0f, 0.1f, 0.15f, 0.5f, 0.0f};
	float tipShare = 0.8f;
	float stabMaxDegrees = 30.0f;
	float slashEdgeMaxDegrees = 45.0f;
	// What follows a blade that went in: a cut mark (game/CutMarkLogic.h),
	// blood (game/BladeBloodLogic.h).
	bool cutMarks = true;
	bool blood = true;
};

enum class BladeEntryKind : UInt8 { None, Stab, Slash };

enum class BladeEntryRefusal : UInt8 { None, NotBlade, Unknown, Heavy, Slow, Angle, Flat, Away };

inline const char* BladeEntryRefusalName(BladeEntryRefusal r) {
	switch (r) {
	case BladeEntryRefusal::None:
		return "in";
	case BladeEntryRefusal::NotBlade:
		return "not a blade";
	case BladeEntryRefusal::Unknown:
		return "the surface not known";
	case BladeEntryRefusal::Heavy:
		return "heavy armour";
	case BladeEntryRefusal::Slow:
		return "too slow";
	case BladeEntryRefusal::Angle:
		return "neither the tip square on nor the edge across";
	case BladeEntryRefusal::Flat:
		return "the flat first";
	case BladeEntryRefusal::Away:
		return "moving off the surface";
	default:
		return "?";
	}
}

struct BladeEntryFrame {
	bool touching = false;  // the blade touches a person this frame
	bool bladed = false;    // a blade (not blunt, not a bow or a staff)
	BodyMaterial material = BodyMaterial::Unknown;
	UInt8 rating = 0;       // the armour's points there
	float along = 0.0f;     // where on the blade: 0 the guard, 1 the tip
	NiPoint3 axis{1.0f, 0.0f, 0.0f};    // guard to tip, a unit direction
	NiPoint3 normal{0.0f, 0.0f, 1.0f};  // the surface's, outward from the body
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};  // the contact point's, units a second
	bool edgeValid = false;  // the weapon's section was read
	NiPoint3 edge{0.0f, 0.0f, 1.0f};  // its edge's direction across the blade
	bool held = false;       // the blade is held at the surface
	float pressUnits = 0.0f; // how far the controller is past the held blade
	float unitsPerMetre = 70.0f;
	float dtSeconds = 0.0f;
};

struct BladeEntryState {
	bool touching = false;
	bool entered = false;
	BladeEntryKind kind = BladeEntryKind::None;
	float pressSeconds = 0.0f;
	bool byPressure = false;
};

struct BladeEntryVerdict {
	bool judged = false;   // a contact was judged this frame
	bool fresh = false;    // its first frame, or the frame it went in: to be said
	bool entered = false;  // in (this contact)
	bool byPressure = false;
	BladeEntryKind kind = BladeEntryKind::None;
	BladeEntryRefusal refusal = BladeEntryRefusal::None;
	float speedMetres = 0.0f;  // the speed judged (the stab's along the blade, else the slash's across)
	float needMetres = 0.0f;   // what the material asked for
};

// The material's speed with light armour's points; below 0: never.
inline float BladeEntryNeed(const float (&base)[kBodyMaterialCount], const BladeEntrySettings& set, BodyMaterial m,
                            UInt8 rating) {
	const UInt32 i = static_cast<UInt32>(m) < kBodyMaterialCount ? static_cast<UInt32>(m) : 0;
	const float need = base[i];
	if (!(need >= 0.0f) || m != BodyMaterial::Light) {
		return need;
	}
	const float extra = set.lightPerPoint * static_cast<float>(rating);
	return need + (extra < set.lightMostExtra ? extra : set.lightMostExtra);
}

namespace detail {

// What the frame's motion asks for: kind, speed, and why not.
struct EntryMotion {
	bool stabAligned = false;
	BladeEntryKind kind = BladeEntryKind::None;
	BladeEntryRefusal refusal = BladeEntryRefusal::None;
	float speed = 0.0f;
	float need = 0.0f;
};

inline EntryMotion JudgeMotion(const BladeEntrySettings& set, const BladeEntryFrame& f) {
	EntryMotion e;
	const float upm = f.unitsPerMetre > 0.0f ? f.unitsPerMetre : 70.0f;
	const NiPoint3 inward = f.normal * -1.0f;
	e.stabAligned = f.along >= set.tipShare &&
	                DotOf3(f.axis, inward) >= math::Cos(set.stabMaxDegrees * math::kPi / 180.0f);
	const float alongSpeed = DotOf3(f.velocity, f.axis) / upm;
	const float stabNeed = BladeEntryNeed(set.stabSpeed, set, f.material, f.rating);
	if (e.stabAligned && alongSpeed > 0.0f && DotOf3(f.velocity, inward) > 0.0f) {
		e.speed = alongSpeed;
		e.need = stabNeed;
		if (alongSpeed >= stabNeed) {
			e.kind = BladeEntryKind::Stab;
			return e;
		}
	}
	const NiPoint3 across = f.velocity - f.axis * DotOf3(f.velocity, f.axis);
	const float acrossLength = math::Sqrt(across.LengthSquared());
	if (!(acrossLength > 1e-3f) && !(alongSpeed > 0.0f)) {
		e.refusal = BladeEntryRefusal::Away;
		return e;
	}
	if (!(acrossLength > 1e-3f)) {
		e.refusal = e.stabAligned ? BladeEntryRefusal::Slow : BladeEntryRefusal::Angle;
		return e;
	}
	if (!(DotOf3(across, inward) > 0.0f)) {
		e.refusal = e.stabAligned && alongSpeed > 0.0f ? BladeEntryRefusal::Slow : BladeEntryRefusal::Away;
		return e;
	}
	const float slashSpeed = acrossLength / upm;
	if (f.edgeValid) {
		const float lead = DotOf3(across * (1.0f / acrossLength), f.edge);
		if ((lead < 0.0f ? -lead : lead) < math::Cos(set.slashEdgeMaxDegrees * math::kPi / 180.0f)) {
			e.refusal = e.stabAligned && alongSpeed > 0.0f ? BladeEntryRefusal::Slow : BladeEntryRefusal::Flat;
			return e;
		}
	}
	const float slashNeed = BladeEntryNeed(set.slashSpeed, set, f.material, f.rating);
	if (slashSpeed >= slashNeed) {
		e.kind = BladeEntryKind::Slash;
		e.speed = slashSpeed;
		e.need = slashNeed;
		return e;
	}
	if (!(e.need > 0.0f)) {
		e.speed = slashSpeed;
		e.need = slashNeed;
	}
	e.refusal = BladeEntryRefusal::Slow;
	return e;
}

}  // namespace detail

inline BladeEntryVerdict StepBladeEntry(BladeEntryState& s, const BladeEntrySettings& set, const BladeEntryFrame& f) {
	BladeEntryVerdict v;
	if (!set.enabled || !f.touching) {
		s = BladeEntryState{};
		return v;
	}
	v.judged = true;
	const bool first = !s.touching;
	s.touching = true;
	if (s.entered) {
		v.entered = true;
		v.kind = s.kind;
		v.byPressure = s.byPressure;
		return v;
	}
	v.fresh = first;
	if (!f.bladed) {
		v.refusal = BladeEntryRefusal::NotBlade;
		return v;
	}
	if (f.material == BodyMaterial::Heavy || f.material == BodyMaterial::Unknown) {
		v.refusal = f.material == BodyMaterial::Heavy ? BladeEntryRefusal::Heavy : BladeEntryRefusal::Unknown;
		return v;
	}
	const detail::EntryMotion e = detail::JudgeMotion(set, f);
	v.speedMetres = e.speed;
	v.needMetres = e.need;
	BladeEntryKind kind = e.kind;
	// Pressure: a stab's only, held at the surface.
	const UInt32 m = static_cast<UInt32>(f.material);
	const float pressNeed = set.pressUnits[m];
	if (kind == BladeEntryKind::None && f.held && e.stabAligned && pressNeed >= 0.0f && f.pressUnits >= pressNeed) {
		s.pressSeconds += f.dtSeconds > 0.0f && f.dtSeconds < 1.0f ? f.dtSeconds : 0.0f;
		if (s.pressSeconds + 1e-5f >= set.pressSeconds[m]) {
			kind = BladeEntryKind::Stab;
			v.byPressure = true;
		}
	} else if (kind == BladeEntryKind::None) {
		s.pressSeconds = 0.0f;
	}
	if (kind == BladeEntryKind::None) {
		v.refusal = e.refusal == BladeEntryRefusal::None ? BladeEntryRefusal::Slow : e.refusal;
		return v;
	}
	s.entered = true;
	s.kind = kind;
	s.byPressure = v.byPressure;
	v.entered = true;
	v.fresh = true;
	v.kind = kind;
	return v;
}

// A slash that went in sinks (step 4; B&S: a slash's penetrationDepth "from
// center to the edge of the blade", penetrationAllowSlide): the drawn blade,
// held at the surface, goes on toward the hand along the surface's inward
// normal, no further than `mostUnits` and only while the hand is deeper -
// the hand drawn back out takes it out; the slide along the cut is the
// contact's own (BladeSlidesOffPeople). The offset for the drawn grip.
inline NiPoint3 SlashSinkOffset(bool slashIn, const NiPoint3& heldPos, const NiPoint3& wantedPos, const NiPoint3& normal,
                                float mostUnits) {
	if (!slashIn || !(mostUnits > 0.0f)) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	const NiPoint3 inward = normal * -1.0f;
	float depth = DotOf3(wantedPos - heldPos, inward);
	if (!(depth > 0.0f)) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	depth = depth < mostUnits ? depth : mostUnits;
	return inward * depth;
}

// How deep a slash sinks: the blade's width where its section was read (the
// edge in to the far side of the steel; guessed, to tune), else this.
inline constexpr float kBladeSinkFallbackUnits = 3.0f;
inline float BladeSinkUnits(bool sectionRead, float halfWidth) {
	return sectionRead && halfWidth > 0.0f ? 2.0f * halfWidth : kBladeSinkFallbackUnits;
}

}  // namespace obvr::game
