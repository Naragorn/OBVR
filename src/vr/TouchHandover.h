#pragma once

// The laser yields to the finger at the tablet (docs/inventory-tablet-spec.md
// 32.4; Nadi, 2026-11-28: "antippen geht ja nun, vll sollten wir dann den
// laserpointer auch ändern wenn ich touch und tipp mache (wie macht alyx
// das?)"). What others do [read 2026-11-28]: Meta's ray-casting spec
// (developers.meta.com/vr/design/raycasting_specs/) fades a hand's ray over
// the last 5 cm before a surface and turns the ray off while a poke is active
// on that hand - the poke takes over the interaction; Meta's hands UI
// guidance (developers.meta.com/vr/design/hands-ui-best-practices/) switches
// a panel from the ray to direct touch as the hand comes close, with a cursor
// that shows which is active; Half-Life: Alyx's menu works with the laser and
// as a touchscreen at once (Road to VR, "VR Design Unpacked").
//
// Before [R in vr/MenuPlace.h StepFingerOnTarget, vr/HandMode.cpp
// PointAtMenu]: the finger took the cursor 10 cm before the glass
// ([Hands] PokeHover) with no hysteresis, the beam stayed drawn (to
// the tip), the trigger still clicked while the finger led, and a finger
// curled or a trigger touched near the page handed the cursor straight back
// to the beam - wherever the controller pointed, often elsewhere on the page.
//
// Now, on the tablet only: the beam fades from kLaserFadeStartMetres to
// kFingerTakesMetres while it still leads; at kFingerTakesMetres the finger
// takes the pointer - the beam gone, its trigger no click, the cursor under
// the tip - and keeps it, curled or not, until the tip is back past
// kFingerLetsGoMetres, off the face, or far through it. A touch mark on the
// page where the tip would land grows from kLaserFadeStartMetres to the
// glass. The tap itself is the touch's, unchanged (vr/TabletTouch.h). Pure;
// covered by touch_handover_test and menu_place_test.

#include "core/Types.h"

namespace obvr::vr {

// [G] Meta's 5 cm for the fade and the take-over; 2 cm of hysteresis so a
// hand held at the line does not flip the pointer; deeper than the contact's
// own `deep` a hand is behind the page. One set, to be tuned in the headset.
inline constexpr float kFingerTakesMetres = 0.05f;
inline constexpr float kFingerLetsGoMetres = 0.07f;
inline constexpr float kLaserFadeMetres = 0.05f;
inline constexpr float kLaserFadeStartMetres = kFingerTakesMetres + kLaserFadeMetres;
inline constexpr float kFingerBehindMetres = 0.25f;

struct TouchHandoverState {
	bool fingerLeads = false;
};

struct TouchHandover {
	bool fingerLeads = false;  // the finger owns the cursor and the click; the laser neither
	bool changed = false;      // it took or gave the pointer this frame
	float laserAlpha = 1.0f;   // the beam and its dot, 0 gone to 1 whole
	bool mark = false;         // the touch mark shown
	float markShare = 0.0f;    // 0 at kLaserFadeStartMetres to 1 on the glass
};

// Each frame. `onTablet`: the pointing hand points at the tablet (not the
// wrist menu, not the big quad); `fingerOut`: its index out, the trigger let
// go (vr::FingerPokes); `inside`: the tip's foot on the face; `depth`: metres
// in front of the glass, negative through it.
inline TouchHandover StepTouchHandover(TouchHandoverState& s, bool onTablet, bool fingerOut, bool inside, float depth) {
	TouchHandover h;
	const bool was = s.fingerLeads;
	if (!onTablet) {
		s = TouchHandoverState{};
		h.changed = was;
		return h;
	}
	const bool over = inside && depth >= -kFingerBehindMetres;
	s.fingerLeads = was ? over && depth <= kFingerLetsGoMetres : fingerOut && over && depth <= kFingerTakesMetres;
	h.fingerLeads = s.fingerLeads;
	h.changed = s.fingerLeads != was;
	if (s.fingerLeads) {
		h.laserAlpha = 0.0f;
	} else if (fingerOut && over) {
		const float a = (depth - kFingerTakesMetres) / kLaserFadeMetres;
		h.laserAlpha = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
	}
	h.mark = over && (s.fingerLeads || (fingerOut && depth <= kLaserFadeStartMetres));
	if (h.mark) {
		const float share = 1.0f - (depth > 0.0f ? depth : 0.0f) / kLaserFadeStartMetres;
		h.markShare = share < 0.0f ? 0.0f : (share > 1.0f ? 1.0f : share);
	}
	return h;
}

// The beam's alpha is handed to the compositor only when it moved by more
// than kLaserAlphaStep, or reached nought or whole: a call a frame for a
// change nobody sees is not made. Below kLaserHiddenAlpha the beam is hidden.
inline constexpr float kLaserAlphaStep = 0.02f;
inline constexpr float kLaserHiddenAlpha = 0.01f;

inline bool LaserAlphaChanged(float shown, float wanted) {
	if (wanted == shown) {
		return false;
	}
	const float d = wanted - shown;
	return d > kLaserAlphaStep || d < -kLaserAlphaStep || wanted <= 0.0f || wanted >= 1.0f;
}

inline bool LaserShownAt(float alpha) { return alpha > kLaserHiddenAlpha; }

// The touch mark on the page: a small square kTouchMarkMetres wide round the
// tip's foot, the laser's warm gold, brightening from kTouchMarkFaintAlpha
// to kTouchMarkFullAlpha as the tip nears (`share`). Its colour is weighed by
// its alpha, as the tablet's pieces are drawn (render/TabletQuad.cpp). [G]
// the size and the alphas: to be seen in the headset.
inline constexpr float kTouchMarkMetres = 0.008f;
inline constexpr float kTouchMarkFaintAlpha = 0.25f;
inline constexpr float kTouchMarkFullAlpha = 0.9f;

struct TouchMarkRect {
	bool valid = false;
	float left = 0.0f;
	float top = 0.0f;
	float right = 0.0f;
	float bottom = 0.0f;
	UInt32 colour = 0;  // ARGB, premultiplied
};

// `x`, `y`: the tip's foot in the layer's pixels; `pixelsPerMetre` the face's.
inline TouchMarkRect TouchMarkAt(bool shown, float x, float y, float share, float pixelsPerMetre) {
	TouchMarkRect m;
	if (!shown || !(pixelsPerMetre > 0.0f)) {
		return m;
	}
	const float half = 0.5f * kTouchMarkMetres * pixelsPerMetre;
	m.valid = true;
	m.left = x - half;
	m.right = x + half;
	m.top = y - half;
	m.bottom = y + half;
	const float s = share < 0.0f ? 0.0f : (share > 1.0f ? 1.0f : share);
	const float a = kTouchMarkFaintAlpha + (kTouchMarkFullAlpha - kTouchMarkFaintAlpha) * s;
	const auto channel = [a](float c) { return static_cast<UInt32>(c * a + 0.5f); };
	m.colour = (channel(255.0f) << 24) | (channel(255.0f) << 16) | (channel(214.0f) << 8) | channel(140.0f);
	return m;
}

}  // namespace obvr::vr
