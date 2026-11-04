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
	bool sideBySide = false;  // the frame was created twice an eye wide (step 2)
	bool firstIsLeft = false; // the pass camera is the left eye (SwapEyeOrder off):
	                          // OBNG's eye 0 is the pass camera, and eye 0 is the left half
	bool worldInBackBuffer = false; // the antialiased path: the world drawn straight into
	                                // the back buffer, not into the image-space texture
};

// The texture path (game/SceneFrame.h): image-space effects on and fewer
// than two effective samples draw the world into a frame-sized texture with
// the engine's own scaled port, then copy it full screen - the half port,
// eye 0's viewport and the eye crops do not hold there yet, so it keeps the
// dual pass.
inline bool WorldDrawnIntoBackBuffer(bool imageSpaceEffects, std::uint32_t effectiveSamples) {
	return !imageSpaceEffects || effectiveSamples >= 2;
}

// Everything but OBNG's answer: whether the eye data is worth handing over.
inline bool WorthSettingEyes(const ObngStereoState& s) {
	return s.enabled && s.bound && s.plan == kObngPlanDrawTwice && s.sideBySide && s.firstIsLeft &&
	       s.worldInBackBuffer;
}

enum class StereoPath { DualPass, SinglePass };

inline StereoPath StereoPathFor(const ObngStereoState& s) {
	if (WorthSettingEyes(s) && s.eyesTaken) {
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

// Step 3: the world into the left half. The engine takes the world render's
// viewport from the main camera's port (NiCamera +0x110, left/right/top/
// bottom as fractions of the target; game/GameCamera.h): SetCameraData
// 0x00701970 hands it to the renderer, which sets the viewport from it.
// RenderScene 0x0040C830 writes the port itself only for a scaled target
// (0x0040CBEF) and resets it to (0, 1, 1, 0) after image space (0x0040CF93,
// read 2026-11-04). So before a world render on a side-by-side frame the
// port is set to the left half - the engine's own viewport is then eye 0's,
// which OBNG's SetEyes requires - and given back after it.
struct CameraPort {
	float left = 0.0f;
	float right = 1.0f;
	float top = 1.0f;
	float bottom = 0.0f;
};

inline constexpr CameraPort kFullPort{0.0f, 1.0f, 1.0f, 0.0f};

inline CameraPort WorldPortFor(bool sideBySide) {
	return sideBySide ? CameraPort{0.0f, 0.5f, 1.0f, 0.0f} : kFullPort;
}

enum class WorldPortAction {
	LeaveAlone,       // one eye wide: the engine's port as it is
	SetLeftHalf,      // side by side, the engine's full port: the left half
	AlreadyLeftHalf,  // side by side, the left half already: nothing to write
	Unexpected,       // side by side, any other port: not written
};

inline bool SamePort(const CameraPort& a, const CameraPort& b) {
	return a.left == b.left && a.right == b.right && a.top == b.top && a.bottom == b.bottom;
}

// Written only over the exact port the engine leaves - anything else is the
// engine's scaled port or a camera read at the wrong place.
inline WorldPortAction DecideWorldPort(bool sideBySide, const CameraPort& current) {
	if (!sideBySide) {
		return WorldPortAction::LeaveAlone;
	}
	if (SamePort(current, kFullPort)) {
		return WorldPortAction::SetLeftHalf;
	}
	if (SamePort(current, WorldPortFor(true))) {
		return WorldPortAction::AlreadyLeftHalf;
	}
	return WorldPortAction::Unexpected;
}

// The width of the world's picture in the frame: what every eye crop reads.
inline std::uint32_t WorldPictureWidth(std::uint32_t frameWidth, bool sideBySide) {
	return sideBySide ? frameWidth / 2 : frameWidth;
}

// Step 5: on a single-pass frame both eyes are in the back buffer at once,
// eye 0 (the left) in the left half and eye 1 (the right) in the right half,
// so the right eye's crop reads one world picture further right. On a dual
// frame each eye's own pass drew the left half.
inline std::int32_t EyeSourceOffset(int eyeIndex, bool singlePassFrame, std::uint32_t worldWidth) {
	return singlePassFrame && eyeIndex == 1 ? static_cast<std::int32_t>(worldWidth) : 0;
}

// A crop (RECT order) moved across by an offset, its rows kept.
template <typename Rect>
Rect ShiftedCrop(const Rect& crop, std::int32_t offset) {
	Rect moved = crop;
	moved.left += offset;
	moved.right += offset;
	return moved;
}

// A5: what OBVR draws into an eye itself (the real controllers, render/
// ControllerModels.h) on a single-pass frame: the eye's half of the frame,
// and for the right eye the camera moved by the eye shift - its pass never
// moved the camera. On a dual frame the device's viewport and the camera
// node as the pass drew are right as they are.
struct EyeRect {
	std::uint32_t x = 0;
	std::uint32_t y = 0;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
};

inline bool EyeRectFor(int eyeIndex, bool singlePassFrame, std::uint32_t frameWidth, std::uint32_t frameHeight,
                       EyeRect& out) {
	if (!singlePassFrame) {
		return false;
	}
	const std::uint32_t half = WorldPictureWidth(frameWidth, true);
	out = EyeRect{eyeIndex == 1 ? half : 0u, 0u, half, frameHeight};
	return true;
}

inline void EyePositionFor(int eyeIndex, bool singlePassFrame, const float camera[3], const float shift[3],
                           float out[3]) {
	const bool moved = singlePassFrame && eyeIndex == 1;
	for (int i = 0; i < 3; ++i) {
		out[i] = camera[i] + (moved ? shift[i] : 0.0f);
	}
}

inline const char* WorldPortActionName(WorldPortAction a) {
	switch (a) {
	case WorldPortAction::SetLeftHalf: return "set to the left half";
	case WorldPortAction::AlreadyLeftHalf: return "already the left half";
	case WorldPortAction::Unexpected: return "not the engine's full port, left alone";
	case WorldPortAction::LeaveAlone: return "left alone";
	}
	return "left alone";
}

}  // namespace obvr::render
