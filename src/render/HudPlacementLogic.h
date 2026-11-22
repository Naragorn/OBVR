#pragma once

// What the 2D layer's overlay (render/HudLayer) shows and where it hangs as
// the tablet comes and goes. Pure; covered by hud_placement_test.
//
// Nadi's Windows run of 2026-11-22: after the tablet had been up, the Esc menu
// was cut off on the right and its buttons could not be hit; a box that came
// up while the tablet was down again hung "somewhere in the room".
//
// - The bounds. The tablet shows only its cut of the layer; the layer's own
//   bounds are to come back when it goes. One flag served as "the tablet's
//   bounds are to be sent again" and as "the compositor has the tablet's
//   bounds": the tablet's placement, given every frame, cleared it, and
//   while the scene drew the tablet the overlay was hidden and not placed at
//   all - so when the tablet went the flag read false and the layer's bounds
//   never came back (read in HudLayer.cpp of f189392; the run's laser hit
//   nothing on the Esc menu after the tablet and hit it before). Here the
//   step keeps what the compositor was last given and decides from that.
// - The anchor. On the wrist or the tablet the overlay hangs from the
//   controller, and its room anchor was marked held so that leaving put the
//   head-relative transform back; in the room's anchoring that same mark
//   kept the old anchor, the transform was never handed over again, and the
//   overlay stayed on the controller until the next menu opened. Leaving,
//   the anchor is dropped in the room's anchoring, so it is taken again where
//   the head looks.

#include "vr/OpenVRTypes.h"

namespace obvr::render {

struct HudBoundsState {
	bool tabletShown = false;  // the compositor was last given the tablet's bounds
	vr::openvr::VRTextureBounds shown{};
};

enum class HudBoundsAction : UInt8 { None, Tablet, Content };

inline bool SameBounds(const vr::openvr::VRTextureBounds& a, const vr::openvr::VRTextureBounds& b) {
	return a.uMin == b.uMin && a.vMin == b.vMin && a.uMax == b.uMax && a.vMax == b.vMax;
}

// Each time the overlay is placed: `tablet` - it is the tablet now, showing
// `wanted`. Tablet: send `wanted`; Content: send the layer's own bounds.
inline HudBoundsAction StepHudBounds(HudBoundsState& s, bool tablet, const vr::openvr::VRTextureBounds& wanted) {
	if (tablet) {
		if (s.tabletShown && SameBounds(s.shown, wanted)) {
			return HudBoundsAction::None;
		}
		s.tabletShown = true;
		s.shown = wanted;
		return HudBoundsAction::Tablet;
	}
	if (!s.tabletShown) {
		return HudBoundsAction::None;
	}
	s.tabletShown = false;
	return HudBoundsAction::Content;
}

// Off the wrist (or the tablet) this placement: whether the room anchor is to
// be kept. `leaving`: it was on the wrist at the last placement. In the room's
// anchoring the anchor is dropped and taken again where the head looks -
// unless one was placed on purpose meanwhile (`placed`: HudLayer::AnchorAt, a
// conversation's or a container's panel), which is handed over as it is;
// ahead of the head it stays marked, which puts the head-relative transform
// back.
inline bool KeepAnchorOffWrist(bool leaving, bool anchorWorld, bool anchorValid, bool placed) {
	return anchorValid && (!(leaving && anchorWorld) || placed);
}

}  // namespace obvr::render
