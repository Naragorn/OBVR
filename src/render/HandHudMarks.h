#pragma once

// The HUD on the hands at a hand script's mark (docs/hud-on-hands-spec.md):
// what each element found and lifted, and with [Debug] HudTileProbe on how
// far the letters reach past their boxes before the cut, the fringes left
// round each cut after it, the atlas and the panel left behind as pictures. Moved out of CameraHook's MaybeSubmitOverlays, which only calls
// it. Runs on a mark's frame only: the probe's readbacks stall the GPU.

#include "core/Types.h"
#include "render/HandHudLayer.h"
#include "vr/HandHud.h"

namespace obvr::render {

// The captured interface the elements were lifted from, after the lift.
struct HandHudPanel {
	void* device = nullptr;
	void* surface = nullptr;
	UInt32 width = 0;
	UInt32 height = 0;
};

// Before the lift, with the probe on: how far the pixels drawn round each
// element to be lifted (`lift`) reach past its box without the margin
// (`boxes`, vr::HudElementBox) - the letters' overhang a text's margin has
// to take in.
void LogHandHudOverhang(const char* mark, const vr::CaptureRect* boxes, const bool* lift, const HandHudPanel& panel);

// After the lift. `rects`: each element's rectangle in the panel's pixels this frame, the
// ones Lift erased. `probe`: [Debug] HudTileProbe.
void ReportHandHudAtMark(const char* mark, const vr::HandHudSettings& settings, const HandHudLayer& layer,
                         const vr::CaptureRect* rects, const HandHudPanel& panel, bool probe);

}  // namespace obvr::render
