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

}  // namespace obvr::render
