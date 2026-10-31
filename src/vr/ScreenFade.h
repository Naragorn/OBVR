#pragma once

#include "core/Types.h"

namespace obvr::vr {

// The engine's screen faders as a fade of the whole view (Nadi, 2026-10-27:
// "bei jedem spielstart kommt ein schwarzes bild mit den ausgeschnittenen
// vierecken").
//
// Oblivion fades the screen with three full-screen faders it draws in its 2D
// pass (game/ScreenFaders.h: the table at 0x00B12DC0, stepped by 0x00572F60) -
// after a load from black, and for the script and teleport fades. On a monitor
// that is a fade of the picture. In the headset the 2D pass lands on the HUD
// panel, so the fader hung there as a black panel, the hand HUD's lifted
// elements cut out of it (measured 2026-10-27: the whole believed 2568x1444 of
// the captured layer opaque black 0.3 s after the load, clear by 1.3 s, the
// interface manager's own fade at 0 throughout).
//
// So the fader is kept out of the 2D pass (its node culled) and its opacity
// goes to the compositor instead (IVRCompositor::FadeToColor, black): the view
// fades, the panel stays clear. Only on world frames - a menu up is the
// cinema screen or the held pair, and fading those would black out the very
// loading screen the fader sits behind - and never while the teleport's own
// fade owns the compositor. Pure: game/ScreenFaders reads the table and
// culls, the frame sends.

// Opacity changes smaller than this are not sent again: the engine steps the
// fader every frame, the compositor does not need every step.
inline constexpr float kScreenFadeStep = 0.02f;

struct ScreenFadeInput {
	bool enabled = false;         // [Hud] FadesInView
	bool worldFrame = false;      // a world frame, no menu up
	bool teleportFading = false;  // the teleport's fade runs (it owns the compositor)
	float opacity = 0.0f;         // the faders' opacity, FaderOpacity
};

struct ScreenFadeState {
	bool owned = false;  // the compositor holds a fade this sent
	float sent = 0.0f;   // what it was sent last
};

struct ScreenFadeStep {
	bool send = false;  // FadeToColor(0 s, black, alpha)
	float alpha = 0.0f;
	bool cullFaders = false;  // keep the faders out of the 2D pass this frame
};

// The most opaque of the faders, within 0..1.
inline float FaderOpacity(const float* opacities, UInt32 count) {
	float most = 0.0f;
	for (UInt32 i = 0; opacities != nullptr && i < count; ++i) {
		const float o = opacities[i] < 0.0f ? 0.0f : (opacities[i] > 1.0f ? 1.0f : opacities[i]);
		most = o > most ? o : most;
	}
	return most;
}

// The HUD goes while the view is faded at least this dark (Nadi, 2026-10-31:
// "obwohl wir den blackscreen aktiv haben, ich noch notifications sehe und
// popups"): FadeToColor darkens the eyes only, the overlays stay over the black.
// Measured in the log of 2026-10-30 22:06: the HUD, the hand HUD and the
// crosshair shown through the whole fade after a load.
inline constexpr float kHudHiddenFade = 0.5f;

// `fade` the alpha the view was last sent (0 when this owns no fade).
inline bool HudHiddenByFade(float fade) { return fade >= kHudHiddenFade; }

inline ScreenFadeStep StepScreenFade(ScreenFadeState& s, const ScreenFadeInput& in) {
	ScreenFadeStep step;
	const float opacity = in.opacity < 0.0f ? 0.0f : (in.opacity > 1.0f ? 1.0f : in.opacity);
	step.cullFaders = in.enabled && opacity > 0.0f;
	if (in.teleportFading) {
		// The teleport sends its own fade in and out; what it leaves is not
		// known, so the first frame after it sends afresh.
		s.owned = false;
		s.sent = -1.0f;
		return step;
	}
	const float want = in.enabled && in.worldFrame ? opacity : 0.0f;
	if (want > 0.0f) {
		const float apart = want > s.sent ? want - s.sent : s.sent - want;
		if (!s.owned || apart >= kScreenFadeStep) {
			step.send = true;
			step.alpha = want;
			s.owned = true;
			s.sent = want;
		}
		return step;
	}
	if (s.owned) {
		step.send = true;
		step.alpha = 0.0f;
	}
	s.owned = false;
	s.sent = 0.0f;
	return step;
}

}  // namespace obvr::vr
