#pragma once

// Placing the four holster places by hand (Nadi, 2026-11-09: the weapon
// places as gold circles to drag, "wie beim stow area", in the first start
// and in the settings): the hips, left and right, and the back, left and
// right, each a gold circle (ui/StowSpotPainter.h) as large as its zone. A
// grip closed on a circle takes it along with that hand; opening the grip
// leaves it there; the right stick up and down sizes the circle held. Keep
// saves the places (the settings and the INI), Cancel puts them back as they
// were, Reset puts them where the T-pose would: the defaults scaled to the
// eye height (BodyMeasure's scale) - and the circles stay up to go on.
//
// The hips stay one setting, mirrored (Nadi, 2026-11-09: "Hüften bleiben
// gespiegelt"): both come from [Hands] HolsterOneHand*, so a hip dragged
// takes the other along, mirrored, and sizing one sizes both. The back's
// right is HolsterTwoHand*, its left HolsterBow*, each on its own. A circle
// stays on its own side of the body (a hip dragged across the middle would
// come back mirrored), and within reach. A setting's side (its X's sign,
// which side its weapon goes to by default - vr::DefaultSlotFor) is kept: a
// circle moves the place, never the weapon's home side.
//
// The places are in the body's frame (vr::BodyRelative: metres from the eyes,
// x right, y forward, z up), the frame the hands are read in, so where a
// circle is seen is where it is taken. Pure, covered by holster_place_test.

#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/BodyMeasure.h"
#include "vr/Holster.h"
#include "vr/StowPlace.h"

namespace obvr::vr {

// The circles: the places in vr::HolsterSlot's order without None.
inline constexpr UInt32 kHolsterPlaceCount = 4;

inline HolsterSlot SlotOfPlace(UInt32 place) { return static_cast<HolsterSlot>(place + 1); }

// The words on each circle.
inline constexpr const char* kHolsterPlaceNames[kHolsterPlaceCount] = {"Left hip", "Right hip", "Left back",
                                                                       "Right back"};

// How near the middle of the body a circle may come, and how far out.
inline constexpr float kHolsterPlaceLeastSide = 0.05f;
inline constexpr float kHolsterPlaceMostSide = 0.6f;

enum class HolsterPlaceCommand : UInt8 { None, Start, Keep, Cancel, Reset };

struct HolsterPlaces {
	NiPoint3 at[kHolsterPlaceCount] = {};
	float radius[kHolsterPlaceCount] = {};
};

// The circles as the settings put the places this frame (vr::SlotZonesFor).
inline HolsterPlaces PlacesOf(const HolsterSettings& s) {
	const SlotZones z = SlotZonesFor(s);
	HolsterPlaces p;
	for (UInt32 i = 0; i < kHolsterPlaceCount; ++i) {
		p.at[i] = z.at[i + 1];
		p.radius[i] = z.radius[i + 1];
	}
	return p;
}

inline bool PlaceOnLeft(UInt32 place) { return place == 0 || place == 2; }

// A spot kept on its circle's side and within reach (StowPlace's bounds
// forward and up).
inline NiPoint3 ClampHolsterPlace(UInt32 place, const NiPoint3& p) {
	const float across = ClampTo(p.x < 0.0f ? -p.x : p.x, kHolsterPlaceLeastSide, kHolsterPlaceMostSide);
	const bool onLeftNow = p.x < 0.0f;
	// Across the middle: held at the middle's edge on its own side.
	const float side = onLeftNow == PlaceOnLeft(place) ? across : kHolsterPlaceLeastSide;
	return NiPoint3{PlaceOnLeft(place) ? -side : side, ClampTo(p.y, kStowPlaceMinForward, kStowPlaceMaxForward),
	                ClampTo(p.z, kStowPlaceMinUp, kStowPlaceMaxUp)};
}

// The other hip, mirrored: the hips are one setting.
inline UInt32 MirrorHip(UInt32 place) { return place == 0 ? 1u : (place == 1 ? 0u : place); }

inline bool PlaceIsHip(UInt32 place) { return place < 2; }

// Moves one circle to `to` (clamped) and the other hip along when it is a hip.
inline void MovePlace(HolsterPlaces& p, UInt32 place, const NiPoint3& to) {
	p.at[place] = ClampHolsterPlace(place, to);
	if (PlaceIsHip(place)) {
		const UInt32 other = MirrorHip(place);
		p.at[other] = NiPoint3{-p.at[place].x, p.at[place].y, p.at[place].z};
	}
}

inline void SizePlace(HolsterPlaces& p, UInt32 place, float radius) {
	p.radius[place] = ClampTo(radius, kStowMinRadius, kStowMaxRadius);
	if (PlaceIsHip(place)) {
		p.radius[MirrorHip(place)] = p.radius[place];
	}
}

// The setting's X with the setting's own sign (its weapon's home side) and
// the circle's distance across.
inline float KeepSign(float settingX, float circleX) {
	const float across = circleX < 0.0f ? -circleX : circleX;
	return settingX < 0.0f ? -across : across;
}

// The places back into the settings: the hips into HolsterOneHand*, the
// back's right into HolsterTwoHand*, its left into HolsterBow*.
inline void ApplyPlaces(HolsterSettings& s, const HolsterPlaces& p) {
	s.oneHandZone = NiPoint3{KeepSign(s.oneHandZone.x, p.at[0].x), p.at[0].y, p.at[0].z};
	s.oneHandRadius = p.radius[0];
	s.bowZone = NiPoint3{KeepSign(s.bowZone.x, p.at[2].x), p.at[2].y, p.at[2].z};
	s.bowRadius = p.radius[2];
	s.twoHandZone = NiPoint3{KeepSign(s.twoHandZone.x, p.at[3].x), p.at[3].y, p.at[3].z};
	s.twoHandRadius = p.radius[3];
}

// The places a T-pose would give at this eye height: the defaults scaled
// (vr::MeasureBody's scale; the radii are not scaled, as there). An eye
// height unknown or out of its range gives the defaults as they are. The
// sides are the settings' own (`current`).
inline float PlaceScaleFor(bool eyeHeightKnown, float eyeHeightMetres) {
	if (!eyeHeightKnown || !(eyeHeightMetres >= kEyeLeast) || !(eyeHeightMetres <= kEyeMost)) {
		return 1.0f;
	}
	return MeasureClamp(eyeHeightMetres / kPlacesEyeHeightMetres, kPlaceScaleLeast, kPlaceScaleMost);
}

inline HolsterPlaces ResetPlaces(float scale) {
	HolsterSettings d{};
	d.oneHandZone = d.oneHandZone * scale;
	d.twoHandZone = d.twoHandZone * scale;
	d.bowZone = d.bowZone * scale;
	return PlacesOf(d);
}

struct HolsterPlaceState {
	bool active = false;
	HolsterPlaces places;
	HolsterPlaces original;  // as they were when the circles came up
	int hand = 0;            // dragging with: 0 none, 1 right, 2 left
	UInt32 held = 0;         // the circle dragged, while `hand` is not 0
	NiPoint3 offset{0.0f, 0.0f, 0.0f};  // the circle minus the dragging hand
	bool rightGripWas = true;  // a grip held when it starts is not a press
	bool leftGripWas = true;
};

struct HolsterPlaceInput {
	HolsterPlaceCommand command = HolsterPlaceCommand::None;
	bool rightValid = false;
	bool leftValid = false;
	NiPoint3 right{0.0f, 0.0f, 0.0f};  // BodyRelative
	NiPoint3 left{0.0f, 0.0f, 0.0f};
	bool rightGrip = false;
	bool leftGrip = false;
	float resize = 0.0f;  // -1..1, the right stick's y: grow up, shrink down
	float dtSeconds = 0.0f;
	HolsterPlaces reset;  // where Reset puts them (ResetPlaces)
};

struct HolsterPlaceVerdict {
	bool active = false;  // the circles are up at `places`
	HolsterPlaces places;
	bool lit[kHolsterPlaceCount] = {};  // held, or a free hand at it
	bool grabbed = false;   // a grip took a circle this frame
	bool dropped = false;   // and let it go
	UInt32 circle = 0;      // the one grabbed or dropped
	bool save = false;      // keep `places` for good: the settings and the INI
	bool restore = false;   // put `places` back into the settings, nothing saved
	bool reset = false;     // Reset moved them; still placing
};

// The circle a hand closing at `hand` takes: the nearest within reach of
// its edge (StowPlace's margin); -1 for none.
inline int CircleAt(const HolsterPlaces& p, const NiPoint3& hand) {
	int best = -1;
	float bestDistance = 0.0f;
	for (UInt32 i = 0; i < kHolsterPlaceCount; ++i) {
		const NiPoint3 d = hand - p.at[i];
		const float distance = d.LengthSquared();
		const float reach = p.radius[i] + kStowPlaceGrabMargin;
		if (distance <= reach * reach && (best < 0 || distance < bestDistance)) {
			best = static_cast<int>(i);
			bestDistance = distance;
		}
	}
	return best;
}

inline HolsterPlaceVerdict StepHolsterPlace(HolsterPlaceState& s, const HolsterPlaceInput& in,
                                            const HolsterSettings& settings) {
	HolsterPlaceVerdict v;
	// A grip held as the circles come up is not a press.
	const bool starting = in.command == HolsterPlaceCommand::Start;
	const bool rightPress = in.rightGrip && !s.rightGripWas && !starting;
	const bool leftPress = in.leftGrip && !s.leftGripWas && !starting;
	s.rightGripWas = in.rightGrip;
	s.leftGripWas = in.leftGrip;

	switch (in.command) {
	case HolsterPlaceCommand::Start:
		s = HolsterPlaceState{};
		s.active = true;
		s.places = s.original = PlacesOf(settings);
		s.rightGripWas = in.rightGrip;
		s.leftGripWas = in.leftGrip;
		break;
	case HolsterPlaceCommand::Keep:
		if (s.active) {
			v.save = true;
			v.places = s.places;
		}
		s = HolsterPlaceState{};
		return v;
	case HolsterPlaceCommand::Cancel:
		if (s.active) {
			v.restore = true;
			v.places = s.original;
		}
		s = HolsterPlaceState{};
		return v;
	case HolsterPlaceCommand::Reset:
		if (s.active) {
			s.places = in.reset;
			s.hand = 0;
			v.reset = true;
		}
		break;
	case HolsterPlaceCommand::None:
		break;
	}
	if (!s.active) {
		return v;
	}

	// The dragging hand: its grip opened, or it was lost - the circle stays.
	if ((s.hand == 1 && (!in.rightGrip || !in.rightValid)) || (s.hand == 2 && (!in.leftGrip || !in.leftValid))) {
		s.hand = 0;
		v.dropped = true;
		v.circle = s.held;
	}
	// A grip closed on a circle takes it; the right hand first when both do.
	if (s.hand == 0) {
		const int byRight = rightPress && in.rightValid ? CircleAt(s.places, in.right) : -1;
		const int byLeft = leftPress && in.leftValid ? CircleAt(s.places, in.left) : -1;
		if (byRight >= 0) {
			s.hand = 1;
			s.held = static_cast<UInt32>(byRight);
			s.offset = s.places.at[s.held] - in.right;
		} else if (byLeft >= 0) {
			s.hand = 2;
			s.held = static_cast<UInt32>(byLeft);
			s.offset = s.places.at[s.held] - in.left;
		}
		if (s.hand != 0) {
			v.grabbed = true;
			v.circle = s.held;
		}
	}
	if (s.hand != 0) {
		MovePlace(s.places, s.held, (s.hand == 1 ? in.right : in.left) + s.offset);
		// The stick sizes the circle held, clamped.
		if (in.resize != 0.0f && in.dtSeconds > 0.0f) {
			SizePlace(s.places, s.held, s.places.radius[s.held] + in.resize * kStowResizeMetresPerSecond * in.dtSeconds);
		}
	}
	v.active = true;
	v.places = s.places;
	for (UInt32 i = 0; i < kHolsterPlaceCount; ++i) {
		const bool heldNow = s.hand != 0 && s.held == i;
		const bool rightAt = s.hand != 1 && in.rightValid && CircleAt(s.places, in.right) == static_cast<int>(i);
		const bool leftAt = s.hand != 2 && in.leftValid && CircleAt(s.places, in.left) == static_cast<int>(i);
		v.lit[i] = heldNow || rightAt || leftAt;
	}
	return v;
}

}  // namespace obvr::vr
