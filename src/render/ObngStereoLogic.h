#pragma once
// When OBVR renders the world in one pass through OBNG's stereo draw route
// (OBNG.dll's StereoEyesApi.h: ObngStereoPlan, ObngStereoSetEyes,
// ObngStereoClearEyes; OBNG's docs/subsystems/single-pass-stereo.md, step 4)
// instead of its own two passes. OBNG draws every partition twice - eye 0 the
// pass camera into the left half of a double-wide target, eye 1 moved by the
// pupil shift into the right half - so the second scene pass, its camera
// shift, bone rebase, culling replay, shadow share and water stereo pass are
// not needed on that path. Any doubt keeps today's dual pass.

#include <cstdint>

namespace obvr::render {

// ObngStereoPlan's answers: 0 the route off, 1 on with one eye (OBNG's 1:1
// gate), 2 on and allowed to draw twice.
inline constexpr std::uint32_t kObngPlanDrawTwice = 2;

struct ObngStereoState {
	bool enabled = false;     // [Render] SinglePassStereo
	bool bound = false;       // OBNG.dll loaded and its three functions found
	std::uint32_t plan = 0;   // ObngStereoPlan()
	bool eyesTaken = false;   // ObngStereoSetEyes answered ok this frame
};

enum class StereoPath { DualPass, SinglePass };

inline StereoPath StereoPathFor(const ObngStereoState& s) {
	if (s.enabled && s.bound && s.plan == kObngPlanDrawTwice && s.eyesTaken) {
		return StereoPath::SinglePass;
	}
	return StereoPath::DualPass;
}

inline const char* StereoPathName(StereoPath p) {
	return p == StereoPath::SinglePass ? "single pass through OBNG" : "dual pass";
}

// ObngStereoSetEyes's answers (ObngStereoResult), for the log.
inline const char* ObngStereoResultName(std::uint32_t r) {
	switch (r) {
	case 0: return "ok";
	case 1: return "a null pointer";
	case 2: return "the size does not match";
	case 3: return "the version does not match";
	case 4: return "a viewport refused";
	case 5: return "an offset that is not finite";
	case 6: return "the route is off or has one eye";
	default: return "an unknown answer";
	}
}

// Step 2: the back buffer side by side. OBNG draws eye 0 into the left half
// of the target and eye 1 into the right, so with SinglePassStereo on the
// frame is created twice an eye's width at the eye's height. The 2D keeps
// believing in one eye (its layout, the cursor and the window), so the menus
// lay out as on an eye-sized frame and land in the left half. The setting is
// read once, at device creation: the back buffer cannot change size later.

// DXVK's D3D9 caps report MaxTextureWidth/Height as MaxTextureDimension,
// 16384 (doitsujin/dxvk src/d3d9/d3d9_caps.h). Twice an eye past that is not
// asked for: the frame stays one eye and the dual pass runs as today.
inline constexpr std::uint32_t kMaxFrameDimension = 16384;

struct FrameSize {
	std::uint32_t width = 0;
	std::uint32_t height = 0;
};

enum class SideBySideAction {
	Off,        // the setting off: the frame is the eye
	NoEyeSize,  // an axis left at the game's own: nothing to double
	TooWide,    // twice the eye past kMaxFrameDimension: the frame stays one eye
	SideBySide, // twice the eye's width
};

struct BackBufferPlan {
	FrameSize frame;  // what CreateDevice is asked for
	FrameSize eye;    // what the 2D's believed size is taken from
	SideBySideAction action = SideBySideAction::Off;
};

inline BackBufferPlan BackBufferForEye(FrameSize eye, bool singlePassStereo) {
	BackBufferPlan plan{eye, eye, SideBySideAction::Off};
	if (!singlePassStereo) {
		return plan;
	}
	if (eye.width == 0 || eye.height == 0) {
		plan.action = SideBySideAction::NoEyeSize;
		return plan;
	}
	if (eye.width > kMaxFrameDimension / 2 || eye.height > kMaxFrameDimension) {
		plan.action = SideBySideAction::TooWide;
		return plan;
	}
	plan.frame.width = eye.width * 2;
	plan.action = SideBySideAction::SideBySide;
	return plan;
}

inline const char* SideBySideActionName(SideBySideAction a) {
	switch (a) {
	case SideBySideAction::SideBySide: return "side by side";
	case SideBySideAction::TooWide: return "refused, twice the eye is past the 16384 limit";
	case SideBySideAction::NoEyeSize: return "no eye size to double";
	case SideBySideAction::Off: return "off";
	}
	return "off";
}

}  // namespace obvr::render
