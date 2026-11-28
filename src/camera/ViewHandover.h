#pragma once

// The measurement of the view's handover between the two ways a frame's
// camera is built: by the camera hook (a world frame, and a menu frame while
// the sticks walk under the tablet) and by the menu stand-in (a menu frame
// the hook does not run on, CameraHook.cpp PrepareMenuFrameIfNeeded). Nadi in
// the headset, 2026-11-28: opening the tablet, a book or the Esc menu, "die
// vr view switcht auf die position vom linken auge", back at the close.
//
// Each frame records what its first eye was built from (ViewBuild) and, at
// the render's entry, where the eyes were drawn. At a change of the kind and
// for kViewHandoverFramesAfter frames after it the frame is logged against
// the last frame of the other kind, every term's difference in the head's
// own axes (right, forward, up) and the eye's in half-IPDs - so one log says
// which term moved. kViewHandoverChanges changes a session.
//
// Pure, covered by view_handover_test.

#include <cstdint>

#include "core/MathFns.h"
#include "game/NiMath.h"

namespace obvr::camera {

enum class ViewFrameKind : std::uint8_t { None, Hook, StandIn };

inline const char* ViewFrameKindName(ViewFrameKind k) {
	return k == ViewFrameKind::Hook ? "camera hook" : k == ViewFrameKind::StandIn ? "menu stand-in" : "none";
}

// What one frame's first eye was built from, all in world units.
struct ViewBuild {
	ViewFrameKind kind = ViewFrameKind::None;
	bool posePredicted = false;               // the head's pose predicted ahead (LatePoseWait) or waited for
	NiPoint3 base{0.0f, 0.0f, 0.0f};          // the engine's eye as kept, or the menu's base (death view, chargen)
	NiPoint3 aimArc{0.0f, 0.0f, 0.0f};
	NiPoint3 aimTilt{0.0f, 0.0f, 0.0f};
	NiPoint3 engineAnim{0.0f, 0.0f, 0.0f};
	float height = 0.0f;                      // the look's height and the sneak's lift
	float realLift = 0.0f;                    // the real-height lift
	NiPoint3 head{0.0f, 0.0f, 0.0f};          // the head's offset, carried by the base rotation
	NiPoint3 firstEyeStep{0.0f, 0.0f, 0.0f};  // to the first eye
	NiPoint3 shift{0.0f, 0.0f, 0.0f};         // from the first eye to the second
	NiMatrix33 view = NiMatrix33::Identity(); // the final rotation: the head's axes in its columns
	NiPoint3 eyeDrawn{0.0f, 0.0f, 0.0f};      // the first eye as the render had it (the camera node's world place)
	NiPoint3 cyclopean{0.0f, 0.0f, 0.0f};     // the cyclopean camera the hands and the body used
	NiPoint3 headTracked{0.0f, 0.0f, 0.0f};   // the headset's position as tracked, metres
};

// The first eye as built: the sum of the terms.
inline NiPoint3 BuiltFirstEye(const ViewBuild& b) {
	NiPoint3 eye = b.base + b.aimArc + b.aimTilt + b.engineAnim + b.head + b.firstEyeStep;
	eye.z += b.height + b.realLift;
	return eye;
}

// A world vector in the head's axes: x right, y forward, z up (the transpose
// of the view applied).
inline NiPoint3 InHeadAxes(const NiMatrix33& view, const NiPoint3& d) {
	return NiPoint3{view.data[0][0] * d.x + view.data[1][0] * d.y + view.data[2][0] * d.z,
	                view.data[0][1] * d.x + view.data[1][1] * d.y + view.data[2][1] * d.z,
	                view.data[0][2] * d.x + view.data[1][2] * d.y + view.data[2][2] * d.z};
}

// How many half-IPDs a distance is: half the shift's length is half the eyes'
// distance as used. 0 without a shift.
inline float HalfIpds(const NiPoint3& d, const NiPoint3& shift) {
	const float half = 0.5f * math::Sqrt(shift.x * shift.x + shift.y * shift.y + shift.z * shift.z);
	return half > 0.0f ? math::Sqrt(d.x * d.x + d.y * d.y + d.z * d.z) / half : 0.0f;
}

inline constexpr std::uint32_t kViewHandoverChanges = 24;
inline constexpr std::uint32_t kViewHandoverFramesAfter = 2;

struct ViewHandoverState {
	ViewBuild last[3];                       // the last frame of each kind, by ViewFrameKind
	ViewFrameKind lastKind = ViewFrameKind::None;
	std::uint32_t changesLeft = kViewHandoverChanges;
	std::uint32_t framesLeft = 0;            // frames still to log after a change
	std::uint32_t frameIndex = 0;            // 0 the change's frame, then 1.. after it
};

struct ViewHandoverVerdict {
	bool log = false;
	std::uint32_t frameIndex = 0;  // 0 the change's frame
	ViewBuild other;               // the last frame of the other kind, to compare against
};

// One frame drawn. A frame of kind None (nothing built it) is ignored; the
// first frame of a session is no change. A change while frames after the last
// one are still being logged starts over, from the budget.
inline ViewHandoverVerdict StepViewHandover(ViewHandoverState& s, const ViewBuild& frame) {
	ViewHandoverVerdict v;
	if (frame.kind == ViewFrameKind::None) {
		return v;
	}
	const bool changed = s.lastKind != ViewFrameKind::None && frame.kind != s.lastKind;
	if (changed && s.changesLeft > 0) {
		--s.changesLeft;
		s.framesLeft = kViewHandoverFramesAfter + 1;
		s.frameIndex = 0;
	}
	if (s.framesLeft > 0) {
		--s.framesLeft;
		v.log = true;
		v.frameIndex = s.frameIndex++;
		const ViewFrameKind other = frame.kind == ViewFrameKind::Hook ? ViewFrameKind::StandIn : ViewFrameKind::Hook;
		v.other = s.last[static_cast<int>(other)];
	}
	s.last[static_cast<int>(frame.kind)] = frame;
	s.lastKind = frame.kind;
	return v;
}

}  // namespace obvr::camera
