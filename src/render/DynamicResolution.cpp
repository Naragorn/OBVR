#include "render/DynamicResolution.h"

#include "camera/CameraHook.h"
#include "core/Config.h"
#include "core/Log.h"
#include "render/ObngStereo.h"
#include "render/ResolutionHook.h"
#include "render/SceneRenderHook.h"
#include "render/SubmitPacing.h"
#include "vr/HeadTracker.h"
#include "vr/OpenVRBackend.h"
#include "vr/OpenVRTypes.h"

namespace obvr::render {

namespace {

// The one owner of the state (the size ratchet allows this file no globals).
struct DynamicResolutionState {
	bool configured = false;
	dynres::Settings settings;
	dynres::State state;
	std::uint32_t moves = 0;
	std::uint32_t linesLeft = 40;  // moves logged in full; after them one line in twenty
};

DynamicResolutionState& Owner() {
	static DynamicResolutionState s_state;
	return s_state;
}

void ConfigureOnce() {
	DynamicResolutionState& d = Owner();
	if (d.configured) {
		return;
	}
	d.configured = true;
	const Config& config = GetConfig();
	d.settings.enabled = config.dynamicResolution;
	d.settings.minScale = dynres::ClampMinScale(config.dynamicResolutionMin);
	d.settings.targetShare = dynres::ClampTargetShare(config.dynamicResolutionTarget);
	OBVR_LOG("Dynamic resolution: %s - the world drawn at %.2f to 1.00 of the eye, the GPU's budget %.0f %% of the headset's period",
	         d.settings.enabled ? "on" : "off", static_cast<double>(d.settings.minScale),
	         static_cast<double>(d.settings.targetShare) * 100.0);
}

// The compositor's GPU time of the frame finished one back (preSubmitGpuMs:
// the work between WaitGetPoses and the submits) against the headset's
// period moves the scale (DynamicResolutionLogic.h Update); a move is logged.
void AdvanceFromCompositor() {
	DynamicResolutionState& d = Owner();
	if (!d.settings.enabled) {
		return;
	}
	vr::OpenVRBackend& backend = camera::GetHeadTracker().GetBackendForFrame();
	vr::openvr::CompositorFrameTiming last{};
	if (!backend.GetFrameTiming(last, 1)) {
		return;
	}
	const float periodMs = static_cast<float>(PeriodMsFromDisplayHz(backend.DisplayFrequencyHz()));
	const float before = d.state.scale;
	const dynres::Move move = dynres::Update(d.state, d.settings, last.preSubmitGpuMs, periodMs);
	if (move == dynres::Move::kNone) {
		return;
	}
	++d.moves;
	if (d.linesLeft > 0 || d.moves % 20 == 0) {
		if (d.linesLeft > 0) {
			--d.linesLeft;
		}
		OBVR_LOG("Dynamic resolution: scale %.3f -> %.3f (%s) - the GPU's median %.2f ms of a %.2f ms budget (last frame %.2f ms, period "
		         "%.2f ms); move %u",
		         static_cast<double>(before), static_cast<double>(d.state.scale),
		         move == dynres::Move::kDown ? "down" : move == dynres::Move::kUp ? "up" : "reset",
		         static_cast<double>(dynres::MedianGpuMs(d.state)), static_cast<double>(periodMs * d.settings.targetShare),
		         static_cast<double>(last.preSubmitGpuMs), static_cast<double>(periodMs), d.moves);
	}
}

}  // namespace

bool DynamicResolutionActive() {
	const DynamicResolutionState& d = Owner();
	return d.settings.enabled && d.state.scale < dynres::kMaxScale;
}

dynres::Layout DynamicResolutionLayout(std::uint32_t frameWidth, std::uint32_t frameHeight) {
	return dynres::LayoutFor(frameWidth, frameHeight, DynamicResolutionScale());
}

float DynamicResolutionScale() {
	const DynamicResolutionState& d = Owner();
	return d.settings.enabled ? d.state.scale : 1.0f;
}

ObngEyes DynamicResolutionEyes(std::uint32_t frameWidth, std::uint32_t frameHeight, const float offset[3]) {
	ConfigureOnce();
	AdvanceFromCompositor();
	const dynres::Layout layout = DynamicResolutionLayout(frameWidth, frameHeight);
	return layout.full ? EyesFor(frameWidth, frameHeight, offset) : EyesFor(frameWidth, frameHeight, offset, layout.width, layout.height);
}

CameraPort DynamicResolutionWorldPort() {
	UInt32 frameWidth = 0;
	UInt32 frameHeight = 0;
	if (!DynamicResolutionActive() || !WasDeviceCreated(frameWidth, frameHeight)) {
		return WorldPortFor(true);
	}
	const dynres::Layout layout = DynamicResolutionLayout(frameWidth, frameHeight);
	return layout.full ? WorldPortFor(true) : WorldPortCut(layout.portRight, layout.portBottom);
}

d3d9::Rect DynamicResolutionCrop(const d3d9::Rect& crop, UInt32 frameWidth, UInt32 frameHeight) {
	if (!IsSinglePassFrame() || !DynamicResolutionActive()) {
		return crop;
	}
	const dynres::Layout layout = DynamicResolutionLayout(frameWidth, frameHeight);
	return layout.full ? crop : dynres::CroppedTo(crop, layout);
}

}  // namespace obvr::render
