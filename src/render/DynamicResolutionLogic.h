#pragma once

// Dynamic resolution for the single pass ([Render] DynamicResolution): when
// the GPU's last frame took more than its share of the headset's period, the
// world is drawn into a smaller part of each eye's half of the frame, and the
// eye copy stretches that part over the eye's picture. The decisions over
// plain values, so every flow runs in a test (tests/DynamicResolutionLogicTest.cpp):
// the scale from the GPU time, the port and the pixel extents it means.
//
// Where the scale takes effect, each read once per frame (render/
// DynamicResolution.h holds the frame's value):
//   - the world camera's port (SceneRenderHook.cpp WorldPortScope): the left
//     half's width times the scale, the height times the scale from the top;
//   - the eye data for OBNG (ObngStereo.h EyesFor): both eyes' viewports with
//     the scaled extents, eye 1 at the right half's left edge;
//   - the eye copy (EyeMirror::CopyBackBuffer): the source crop with the
//     scaled extents, stretched over the eye's whole picture - so nothing
//     downstream (the submit, the HUD layer, the held eyes) changes.
// The GPU time is the compositor's: Compositor_FrameTiming.preSubmitGpuMs of
// the frame finished one back ("gpu work submitted between WaitGetPoses and
// second Submit", openvr.h), read in HeadsetRenderer::BeginFrame.

#include <cmath>
#include <cstdint>

namespace obvr::render::dynres {

struct Settings {
	bool enabled = false;
	float minScale = 0.5f;     // the world never drawn smaller than this share of the eye's width and height
	float targetShare = 0.85f; // the GPU's budget as a share of the headset's period
};

// The scale moves in steps of 1/32: a step is 80 pixels of a 2568-wide eye,
// enough that the quantised value does not flicker between two neighbours on
// GPU noise.
inline constexpr float kScaleStep = 1.0f / 32.0f;
inline constexpr float kMaxScale = 1.0f;
inline constexpr float kMinScaleFloor = 0.25f;  // below this the picture is not worth showing; the setting is clamped to it
// Down at once, by as much as the GPU time asks for (pixels scale with the
// square of the scale) but at most this much a frame; up one step after this
// many frames under three quarters of the budget.
inline constexpr float kMaxDownPerFrame = 0.125f;
inline constexpr unsigned kUpHoldFrames = 15;
inline constexpr float kUpBelowShare = 0.75f;

inline float ClampMinScale(float minScale) {
	if (!(minScale > kMinScaleFloor)) {
		return kMinScaleFloor;
	}
	return minScale > kMaxScale ? kMaxScale : minScale;
}

inline float ClampTargetShare(float share) {
	if (!(share > 0.1f)) {
		return 0.1f;
	}
	return share > 1.0f ? 1.0f : share;
}

// The nearest multiple of the step inside [minScale, 1].
inline float Quantize(float scale, float minScale) {
	const float low = ClampMinScale(minScale);
	float q = std::round(scale / kScaleStep) * kScaleStep;
	if (q < low) {
		q = low;
	}
	return q > kMaxScale ? kMaxScale : q;
}

struct State {
	float scale = 1.0f;
	unsigned underBudgetFrames = 0;
};

enum class Move : std::uint8_t { kNone, kDown, kUp, kReset };

// One frame's decision. gpuMs is the last finished frame's GPU time (0 or
// less: unknown, the scale stays), periodMs the headset's period (0 or less:
// unknown, the scale stays). Off: the scale is 1 and stays there.
inline Move Update(State& state, const Settings& settings, float gpuMs, float periodMs) {
	if (!settings.enabled) {
		const bool was = state.scale != 1.0f;
		state.scale = 1.0f;
		state.underBudgetFrames = 0;
		return was ? Move::kReset : Move::kNone;
	}
	if (!(gpuMs > 0.0f) || !(periodMs > 0.0f)) {
		return Move::kNone;
	}
	const float budget = periodMs * ClampTargetShare(settings.targetShare);
	const float before = state.scale;
	if (gpuMs > budget) {
		// Pixels, and with them the GPU's time, go with the square of the scale.
		const float wanted = state.scale * std::sqrt(budget / gpuMs);
		const float floor = state.scale - kMaxDownPerFrame;
		state.scale = Quantize(wanted > floor ? wanted : floor, settings.minScale);
		state.underBudgetFrames = 0;
		// A scale already at its floor cannot go down: no move.
		return state.scale < before ? Move::kDown : Move::kNone;
	}
	if (gpuMs < budget * kUpBelowShare) {
		if (++state.underBudgetFrames < kUpHoldFrames) {
			return Move::kNone;
		}
		state.underBudgetFrames = 0;
		state.scale = Quantize(state.scale + kScaleStep, settings.minScale);
		return state.scale > before ? Move::kUp : Move::kNone;
	}
	state.underBudgetFrames = 0;
	return Move::kNone;
}

// Where the world lands in the frame at a scale: the eye's extents in pixels
// and the camera port that makes the engine set exactly them. Do_SetCameraData
// 0x00762640 builds the viewport as X = trunc(port.left * W), Y = trunc((1 -
// port.top) * H), Width = trunc((port.right - port.left) * W), Height =
// trunc((port.top - port.bottom) * H), the top edge port.top = 1 (read,
// ../OBNG/re/functions/0x00762640.md C7). The port's edges are put half a
// pixel past the wanted extent, so the truncation lands on it whatever the
// rounding of the products (an edge exactly on a pixel could truncate one
// short). At the full scale the engine's own left-half port is kept, so the
// path is the one of today.
struct Layout {
	std::uint32_t width = 0;   // the world's width in pixels, per eye
	std::uint32_t height = 0;  // its height
	float portRight = 0.5f;    // the camera port's right edge (left 0)
	float portBottom = 0.0f;   // its bottom edge (top 1)
	bool full = true;          // the whole half: the engine's port as it stands
};

inline std::uint32_t ScaledExtent(std::uint32_t full, float scale) {
	const float scaled = std::round(static_cast<float>(full) * scale);
	if (!(scaled >= 1.0f)) {
		return full == 0 ? 0u : 1u;
	}
	return scaled > static_cast<float>(full) ? full : static_cast<std::uint32_t>(scaled);
}

inline Layout LayoutFor(std::uint32_t frameWidth, std::uint32_t frameHeight, float scale) {
	Layout layout;
	const std::uint32_t half = frameWidth / 2;
	layout.width = ScaledExtent(half, scale);
	layout.height = ScaledExtent(frameHeight, scale);
	layout.full = layout.width == half && layout.height == frameHeight;
	if (layout.full || frameWidth == 0 || frameHeight == 0) {
		layout.portRight = 0.5f;
		layout.portBottom = 0.0f;
		return layout;
	}
	layout.portRight = (static_cast<float>(layout.width) + 0.5f) / static_cast<float>(frameWidth);
	layout.portBottom = 1.0f - (static_cast<float>(layout.height) + 0.5f) / static_cast<float>(frameHeight);
	return layout;
}

// What the engine's truncation makes of a port edge (for the tests and the log).
inline std::uint32_t EngineExtent(float portSpan, std::uint32_t full) {
	const float product = portSpan * static_cast<float>(full);
	return product <= 0.0f ? 0u : static_cast<std::uint32_t>(product);
}

// A crop (RECT order: left, top, right, bottom) cut to the layout's extents,
// its top-left corner kept - the part of the eye's half the world was drawn into.
template <typename Rect>
Rect CroppedTo(const Rect& crop, const Layout& layout) {
	Rect cut = crop;
	cut.right = crop.left + static_cast<decltype(crop.right)>(layout.width);
	cut.bottom = crop.top + static_cast<decltype(crop.bottom)>(layout.height);
	return cut;
}

}  // namespace obvr::render::dynres
