#pragma once

// Placing the stow spot (vr/Stow.h) where the player wants it on the body
// (the tester, 2026-09-28): the gold ring is hidden by default - the spot
// still stows - and a settings row opens a window (MenuQue, the one the
// hands' fit uses); for as long as it is open the ring is shown. A grip closed on
// the ring takes it along with that hand; opening the grip leaves it there.
// Done keeps the spot for good (the INI) and hides the ring again; Cancel
// puts it back where it was; Reset puts it back to the default.
//
// The spot is in the body's frame (vr::BodyRelative, metres from the eyes:
// x right, y forward, z up), the same frame the hands are read in, so where
// the ring is seen is where it is taken.
//
// Pure, covered by stow_test.

#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/Stow.h"

namespace obvr::vr {

// How far outside the ring a closed grip still takes it: a hand reaching for
// a ring 32 cm across need not be inside it.
constexpr float kStowPlaceGrabMargin = 0.06f;

// Where the spot may be put: within arm's reach of the body. A ring dragged
// off into the room would stow nothing anybody could reach.
constexpr float kStowPlaceMaxSide = 0.6f;
constexpr float kStowPlaceMinForward = -0.4f;
constexpr float kStowPlaceMaxForward = 0.8f;
constexpr float kStowPlaceMinUp = -1.4f;
constexpr float kStowPlaceMaxUp = 0.3f;

enum class StowPlaceCommand : UInt8 { None, Start, Keep, Cancel, Reset };

struct StowPlaceState {
	bool active = false;
	NiPoint3 spot{0.0f, 0.0f, 0.0f};      // where the ring is now
	NiPoint3 original{0.0f, 0.0f, 0.0f};  // where it was when the window opened
	int hand = 0;                         // dragging with: 0 none, 1 right, 2 left
	NiPoint3 offset{0.0f, 0.0f, 0.0f};    // spot minus the dragging hand
	bool rightGripWas = true;             // a grip held when it starts is not a press
	bool leftGripWas = true;
};

struct StowPlaceInput {
	StowPlaceCommand command = StowPlaceCommand::None;
	bool rightValid = false;
	bool leftValid = false;
	NiPoint3 right{0.0f, 0.0f, 0.0f};  // BodyRelative
	NiPoint3 left{0.0f, 0.0f, 0.0f};
	bool rightGrip = false;
	bool leftGrip = false;
};

struct StowPlaceVerdict {
	bool active = false;    // the window is open: the ring shows at `spot`
	NiPoint3 spot{0.0f, 0.0f, 0.0f};
	bool dragging = false;
	bool grabbed = false;   // a grip took the ring this frame
	bool dropped = false;   // and let it go
	bool save = false;      // keep `spot` for good: the settings and the INI
	bool restore = false;   // put `spot` back into the settings, nothing saved
};

inline float ClampTo(float v, float low, float high) { return v < low ? low : (v > high ? high : v); }

inline NiPoint3 ClampStowSpot(const NiPoint3& p) {
	return NiPoint3{ClampTo(p.x, -kStowPlaceMaxSide, kStowPlaceMaxSide),
	                ClampTo(p.y, kStowPlaceMinForward, kStowPlaceMaxForward),
	                ClampTo(p.z, kStowPlaceMinUp, kStowPlaceMaxUp)};
}

inline NiPoint3 StowSpotOf(const StowSettings& s) {
	return NiPoint3{s.centreRight, s.centreForward, s.centreUp};
}

inline bool NearStowSpot(const NiPoint3& hand, const NiPoint3& spot, float radius) {
	const NiPoint3 d = hand - spot;
	const float reach = radius + kStowPlaceGrabMargin;
	return d.LengthSquared() <= reach * reach;
}

inline StowPlaceVerdict StepStowPlace(StowPlaceState& s, const StowPlaceInput& in,
                                      const StowSettings& settings) {
	StowPlaceVerdict v;
	const bool rightPress = in.rightGrip && !s.rightGripWas;
	const bool leftPress = in.leftGrip && !s.leftGripWas;
	s.rightGripWas = in.rightGrip;
	s.leftGripWas = in.leftGrip;

	switch (in.command) {
	case StowPlaceCommand::Start:
		s = StowPlaceState{};
		s.active = true;
		s.spot = s.original = StowSpotOf(settings);
		s.rightGripWas = in.rightGrip;
		s.leftGripWas = in.leftGrip;
		break;
	case StowPlaceCommand::Keep:
		if (s.active) {
			v.save = true;
			v.spot = s.spot;
		}
		s = StowPlaceState{};
		return v;
	case StowPlaceCommand::Cancel:
		if (s.active) {
			v.restore = true;
			v.spot = s.original;
		}
		s = StowPlaceState{};
		return v;
	case StowPlaceCommand::Reset:
		if (s.active) {
			v.save = true;
			v.spot = StowSpotOf(StowSettings{});
		}
		s = StowPlaceState{};
		return v;
	case StowPlaceCommand::None:
		break;
	}
	if (!s.active) {
		return v;
	}

	// The dragging hand: its grip opened, or it was lost - the ring stays.
	if (s.hand == 1 && (!in.rightGrip || !in.rightValid)) {
		s.hand = 0;
		v.dropped = true;
	} else if (s.hand == 2 && (!in.leftGrip || !in.leftValid)) {
		s.hand = 0;
		v.dropped = true;
	}
	// A grip closed on the ring takes it; the right hand first when both do.
	if (s.hand == 0) {
		if (rightPress && in.rightValid && NearStowSpot(in.right, s.spot, settings.radius)) {
			s.hand = 1;
			s.offset = s.spot - in.right;
			v.grabbed = true;
		} else if (leftPress && in.leftValid && NearStowSpot(in.left, s.spot, settings.radius)) {
			s.hand = 2;
			s.offset = s.spot - in.left;
			v.grabbed = true;
		}
	}
	if (s.hand == 1) {
		s.spot = ClampStowSpot(in.right + s.offset);
	} else if (s.hand == 2) {
		s.spot = ClampStowSpot(in.left + s.offset);
	}
	v.active = true;
	v.spot = s.spot;
	v.dragging = s.hand != 0;
	return v;
}

// Whether the ring is drawn: while it is being placed, or - with the ring
// switched on ([Hands] StowSpotVisible) - while an item is held.
inline bool StowRingShown(bool placing, bool spotVisible, bool itemHeld) {
	return placing || (spotVisible && itemHeld);
}

}  // namespace obvr::vr
