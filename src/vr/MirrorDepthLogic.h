#pragma once

// The mirror's depth, stage 4 (docs/mirror-spec.md 5, 6 D and E, 8, 14). Each
// eye gets its own reflection: the eye's own position reflected in the
// (steadied) glass, its own off-axis frustum through the glass's corners, its
// own picture, laid onto that eye's glass through the camera it was rendered
// with (vr/MirrorGlassMap.h). The two cameras stand apart by the eyes' offset
// reflected in the glass, so the reflection has depth (Arm's VR guide,
// "Stereo reflections": one picture for both eyes is the flat fault).
//
// The way back ([Tablet] MirrorDepth=0): one picture for both eyes, made from
// the cyclopean eye - stage 3 exactly; none of this runs then.
//
// The budget (option E): two passes a frame by default; while the pass's mean
// over the last second stays over [Tablet] MirrorBudgetMs the eyes take
// turns, one pass a frame, each eye's picture renewed every second frame and
// the stale eye's picture kept laid through its own camera meanwhile. Back to
// both under 0.8 of the budget; a change holds at least kMirrorBudgetHoldSeconds.
//
// Pure; covered by mirror_depth_test. The engine side is
// render/MirrorGlassPass.cpp.

#include <cstdint>

#include "vr/MirrorGlassLogic.h"
#include "vr/MirrorGlassMap.h"

namespace obvr::vr {

// ------------------------------------------------------- each eye's camera

// The camera of each eye's reflection (index 0 left). Depth off: stage 3's
// one camera - the cyclopean eye reflected - in both. Depth on: each eye's own
// place reflected; an eye the glass was not drawn into has none (NoGlass).
struct MirrorEyeCameras {
	bool depth = false;
	MirrorCamera eye[2];
};

inline MirrorEyeCameras MirrorEyeCamerasFor(const GlassPlane& glass, const NiPoint3 (&corners)[4],
                                            const GlassOnScreen (&eyes)[2], bool depth) {
	MirrorEyeCameras c;
	c.depth = depth;
	if (!depth) {
		NiPoint3 middle{0.0f, 0.0f, 0.0f};
		if (CyclopeanEye(eyes, middle)) {
			c.eye[0] = MirrorCameraFor(glass, corners, middle);
			c.eye[1] = c.eye[0];
		}
		return c;
	}
	for (int i = 0; i < 2; ++i) {
		if (eyes[i].drawn) {
			c.eye[i] = MirrorCameraFor(glass, corners, eyes[i].eye);
		}
	}
	return c;
}

// ------------------------------------------------------------- the mode

// How a frame renders the mirror: one picture for both eyes (MirrorDepth=0,
// stage 3), each eye its own every frame, or the eyes taking turns.
enum class MirrorDepthMode : std::uint8_t { OneForBoth, Both, Turns, Count };

inline const char* MirrorDepthModeName(MirrorDepthMode m) {
	switch (m) {
	case MirrorDepthMode::OneForBoth: return "one picture for both eyes";
	case MirrorDepthMode::Both: return "both eyes";
	case MirrorDepthMode::Turns: return "the eyes taking turns";
	case MirrorDepthMode::Count: break;
	}
	return "unknown";
}

// ------------------------------------------------------------- the budget

// [Tablet] MirrorBudgetMs: the pass's mean over a second above which the eyes
// take turns. 2.5 ms a pass is the spec's rule (section 8) [G: from the
// owner's two runs of 2026-11-28 - means 1.39 ms, 2 of 127 lines over it]; 0
// never takes turns.
inline constexpr float kMirrorBudgetMsDefault = 2.5f;
inline constexpr float kMirrorBudgetMsMax = 20.0f;
inline constexpr float kMirrorBudgetWindowSeconds = 1.0f;
// Back to both eyes under this share of the budget: the two ways do not meet,
// so a cost right at the budget does not flip every second [G].
inline constexpr float kMirrorBudgetBackShare = 0.8f;
// A change holds at least this long before the next [G].
inline constexpr float kMirrorBudgetHoldSeconds = 3.0f;
// A frame time past this is a pause or a load, not counted.
inline constexpr float kMirrorBudgetLongestStepSeconds = 0.25f;

inline float ClampMirrorBudgetMs(float ms) {
	if (!(ms == ms)) {
		return kMirrorBudgetMsDefault;
	}
	if (ms < 0.0f) {
		return 0.0f;
	}
	return ms > kMirrorBudgetMsMax ? kMirrorBudgetMsMax : ms;
}

struct MirrorBudget {
	bool turns = false;
	float windowSeconds = 0.0f;
	double windowMs = 0.0;
	std::uint32_t windowPasses = 0;
	float heldSeconds = kMirrorBudgetHoldSeconds;  // the first decision needs no hold
	double lastMeanMs = 0.0;                        // the last full second's pass mean
};

enum class MirrorBudgetChange : std::uint8_t { None, ToTurns, ToBoth };

// One frame with the page up: its passes' milliseconds together and their
// count, and the frame's time. Decides at the end of each second.
inline MirrorBudgetChange StepMirrorBudget(MirrorBudget& b, float budgetMs, float seconds, double passesMs,
                                           std::uint32_t passes) {
	if (!(budgetMs > 0.0f)) {
		const bool was = b.turns;
		b = MirrorBudget{};
		return was ? MirrorBudgetChange::ToBoth : MirrorBudgetChange::None;
	}
	if (!(seconds > 0.0f) || !(seconds <= kMirrorBudgetLongestStepSeconds)) {
		return MirrorBudgetChange::None;
	}
	b.windowSeconds += seconds;
	b.heldSeconds += seconds;
	b.windowMs += passesMs;
	b.windowPasses += passes;
	if (b.windowSeconds < kMirrorBudgetWindowSeconds) {
		return MirrorBudgetChange::None;
	}
	const bool measured = b.windowPasses > 0;
	const double mean = measured ? b.windowMs / static_cast<double>(b.windowPasses) : 0.0;
	b.windowSeconds = 0.0f;
	b.windowMs = 0.0;
	b.windowPasses = 0;
	if (!measured) {
		return MirrorBudgetChange::None;
	}
	b.lastMeanMs = mean;
	if (b.heldSeconds < kMirrorBudgetHoldSeconds) {
		return MirrorBudgetChange::None;
	}
	if (!b.turns && mean > static_cast<double>(budgetMs)) {
		b.turns = true;
		b.heldSeconds = 0.0f;
		return MirrorBudgetChange::ToTurns;
	}
	if (b.turns && mean < static_cast<double>(budgetMs * kMirrorBudgetBackShare)) {
		b.turns = false;
		b.heldSeconds = 0.0f;
		return MirrorBudgetChange::ToBoth;
	}
	return MirrorBudgetChange::None;
}

inline MirrorDepthMode MirrorDepthModeFor(bool depthSetting, const MirrorBudget& b) {
	if (!depthSetting) {
		return MirrorDepthMode::OneForBoth;
	}
	return b.turns ? MirrorDepthMode::Turns : MirrorDepthMode::Both;
}

// ------------------------------------------------------ which eyes render

// A picture's age in frames; none yet.
inline constexpr std::uint32_t kMirrorNoPicture = 0xFFFFFFFFu;

struct MirrorEyePlan {
	bool render[2] = {false, false};
	std::uint32_t passes = 0;
};

// One for both: the one pass (index 0, the cyclopean camera, decided before).
// Both: every eye that can (its camera there and the glass in its view).
// Turns: the eye that can with the older picture - one without a picture
// first, the left on a tie.
inline MirrorEyePlan PlanMirrorEyes(MirrorDepthMode mode, const bool (&canRender)[2], const std::uint32_t (&age)[2]) {
	MirrorEyePlan p;
	if (mode == MirrorDepthMode::OneForBoth) {
		p.render[0] = true;
	} else if (mode == MirrorDepthMode::Both) {
		p.render[0] = canRender[0];
		p.render[1] = canRender[1];
	} else if (mode == MirrorDepthMode::Turns) {
		if (canRender[0] && (!canRender[1] || age[0] >= age[1])) {
			p.render[0] = true;
		} else if (canRender[1]) {
			p.render[1] = true;
		}
	}
	p.passes = (p.render[0] ? 1u : 0u) + (p.render[1] ? 1u : 0u);
	return p;
}

// ------------------------------------------------------- the eyes' pictures

// Each eye's picture as it stands in its texture: there or not, the part of
// the texture it fills, the camera it was rendered with (valid only with the
// steady picture), and its age in frames.
struct MirrorEyePictures {
	bool have[2] = {false, false};
	float rect[2][4] = {};
	MirrorPictureView view[2];
	std::uint32_t age[2] = {kMirrorNoPicture, kMirrorNoPicture};
};

// A frame passes: every picture there one frame older.
inline void AgeMirrorEyes(MirrorEyePictures& p) {
	for (std::uint32_t& a : p.age) {
		a = a == kMirrorNoPicture || a + 1 == kMirrorNoPicture ? a : a + 1;
	}
}

// An eye's picture rendered this frame, with its view and rect (none: all of
// the texture).
inline void NoteMirrorEyeRendered(MirrorEyePictures& p, int eye, const MirrorPictureView& view, const float* rect) {
	if (eye < 0 || eye > 1) {
		return;
	}
	p.have[eye] = true;
	p.view[eye] = view;
	p.age[eye] = 0;
	const float whole[4] = {0.0f, 0.0f, 1.0f, 1.0f};
	for (int i = 0; i < 4; ++i) {
		p.rect[eye][i] = rect != nullptr ? rect[i] : whole[i];
	}
}

inline void ForgetMirrorEyes(MirrorEyePictures& p) { p = MirrorEyePictures{}; }

// Which picture each eye's glass shows (0 the left's, 1 the right's, -1 none:
// the dark glass). Depth off: the one picture in both. Depth on: its own, or
// the other's while its own is not there yet - never a dark flash in one eye.
inline void MirrorGlassSources(bool depth, const bool (&have)[2], int (&out)[2]) {
	if (!depth) {
		out[0] = out[1] = have[0] ? 0 : -1;
		return;
	}
	for (int i = 0; i < 2; ++i) {
		out[i] = have[i] ? i : (have[1 - i] ? 1 - i : -1);
	}
}

// ------------------------------------------------------------ the cost line

// A frame's second pass: its milliseconds and draws, with the passes'.
inline void NoteMirrorExtraPass(MirrorCostWindow& w, double ms, std::uint32_t draws) {
	++w.passes;
	w.totalMs += ms;
	w.worstMs = ms > w.worstMs ? ms : w.worstMs;
	w.draws += draws;
}

// A frame that rendered: its mode, its passes together, which eye's pass ran
// (one for both counts as the left), and with two passes whether the engine's
// skin stamp moved between them.
inline void NoteMirrorDepthFrame(MirrorCostWindow& w, MirrorDepthMode mode, double frameMs, bool left, bool right,
                                 bool stampChanged) {
	if (mode != MirrorDepthMode::Count) {
		++w.modeFrames[static_cast<int>(mode)];
	}
	w.eyePasses[0] += left ? 1u : 0u;
	w.eyePasses[1] += right ? 1u : 0u;
	++w.framesRendered;
	w.frameMsSum += frameMs;
	w.frameWorstMs = frameMs > w.frameWorstMs ? frameMs : w.frameWorstMs;
	if (left && right) {
		++w.stampAsked;
		w.stampChanged += stampChanged ? 1u : 0u;
	}
}

inline void NoteMirrorShadowsKept(MirrorCostWindow& w, std::uint32_t kept) { w.shadowsKept += kept; }

inline double MeanFrameMs(const MirrorCostWindow& w) {
	return w.framesRendered > 0 ? w.frameMsSum / static_cast<double>(w.framesRendered) : 0.0;
}

}  // namespace obvr::vr
