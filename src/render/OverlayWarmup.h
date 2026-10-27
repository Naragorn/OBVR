#pragma once

#include <cstdint>

#include "core/Types.h"

namespace obvr::render {

// The overlay textures handed to the compositor once, right after a load
// ([Render] WarmOverlays, docs/performance-spec.md section 7). On Windows
// SteamVR's vrclient imports a Direct3D 9 texture the first time it is given
// to SetOverlayTexture, and that import held the main thread 90 to 110 ms
// (measured 2026-10-26): SteamVR then ran at half rate for about 22 frames.
// OBVR made those textures the first time they were shown - the quick menu,
// the spell ring, each enemy health bar, the snap-turn vignette - so the stall
// landed in play. Warmed at the load's first world frame, it lands in the
// load's own hitch instead.
//
// Plain values only: the steps below decide which layers of the caller's list
// to warm this frame, the caller does the Direct3D and OpenVR work.

struct OverlayWarmupState {
	bool pending = false;       // a load or a new device seen, its warm-up not finished
	std::uintptr_t device = 0;  // the game device last seen (0 before the first)
	UInt32 next = 0;            // the first layer of the list not yet warmed
};

struct OverlayWarmupInput {
	bool enabled = false;       // [Render] WarmOverlays
	bool vrRunning = false;     // OpenVR is up, so overlays can be made
	bool loading = false;       // the loading thread, or the Loading menu on top
	bool inWorld = false;       // the player stands in a cell
	bool menuIsUp = false;
	std::uintptr_t device = 0;  // the game's Direct3D device, 0 when not read
	UInt32 layerCount = 0;      // the layers in the caller's list
	UInt32 perFrame = 0;        // layers warmed per frame; 0 = all in one frame
};

// The layers to warm this frame: list[first .. first + count).
struct OverlayWarmupSlice {
	UInt32 first = 0;
	UInt32 count = 0;
};

// Once per load, as StepFirstPersonAfterLoad: a loading frame arms it, the
// first world frame without a menu warms. A device other than the last one
// seen arms it as well - the textures belong to the device, and a new one's
// are new to the compositor. The load's own frames are long anyway, so the
// default is the whole list in that one frame; perFrame spreads it.
inline OverlayWarmupSlice StepOverlayWarmup(OverlayWarmupState& s, const OverlayWarmupInput& in) {
	if (in.device != 0 && in.device != s.device) {
		s.device = in.device;
		s.pending = true;
		s.next = 0;
	}
	if (in.loading) {
		s.pending = true;
		s.next = 0;
		return {};
	}
	if (!s.pending) {
		return {};
	}
	// Off: today's behaviour, every texture made at its first show. The load
	// is let go, so switching it on warms at the next load, not mid-play.
	if (!in.enabled) {
		s.pending = false;
		return {};
	}
	if (in.device == 0 || !in.vrRunning || !in.inWorld || in.menuIsUp) {
		return {};
	}
	// The list can shrink between two slices when a setting goes off.
	if (s.next >= in.layerCount) {
		s.pending = false;
		return {};
	}
	const UInt32 left = in.layerCount - s.next;
	OverlayWarmupSlice slice;
	slice.first = s.next;
	slice.count = in.perFrame == 0 || in.perFrame > left ? left : in.perFrame;
	s.next += slice.count;
	if (s.next >= in.layerCount) {
		s.pending = false;
	}
	return slice;
}

// Which layers go on the list: only those the settings can show. Each one
// warmed is one import, about 0.1 s of the load on Windows, so a layer that
// can never come up is not worth it.
enum class OverlayWarmKind : UInt32 {
	FullVrPanel,  // the quick menu, the spell ring, the guide panels: Full VR's own
	EnemyHealth,  // [HandHud] EnemyHealthBars
	Vignette,     // [Look] SnapTurnVignette, or the teleport's
	Crosshair,    // [Render] Crosshair (the depth quad)
	SettingsMenu, // OBVR's in-headset settings menu (1024x768), opened by its key in any mode
};

struct OverlayWarmWants {
	bool fullVr = false;
	bool enemyHealth = false;
	bool vignette = false;
	bool crosshair = false;
	bool settingsMenu = false;
};

inline bool OverlayWarmWanted(OverlayWarmKind kind, const OverlayWarmWants& wants) {
	switch (kind) {
		case OverlayWarmKind::FullVrPanel:
			return wants.fullVr;
		case OverlayWarmKind::EnemyHealth:
			return wants.enemyHealth;
		case OverlayWarmKind::Vignette:
			return wants.vignette;
		case OverlayWarmKind::Crosshair:
			return wants.crosshair;
		case OverlayWarmKind::SettingsMenu:
			return wants.settingsMenu;
	}
	return false;
}

// What warming one layer came to.
enum class OverlayWarmResult : UInt32 {
	Handed,       // its texture went to SetOverlayTexture now
	AlreadyWarm,  // handed before (an earlier load, or shown already)
	Failed,       // a step refused; the layer said which, once
};

inline const char* OverlayWarmResultName(OverlayWarmResult result) {
	switch (result) {
		case OverlayWarmResult::Handed:
			return "handed to the compositor";
		case OverlayWarmResult::AlreadyWarm:
			return "already warm";
		case OverlayWarmResult::Failed:
			return "NOT handed (its reason above)";
	}
	return "?";
}

// A show that hands a texture over: whether it is the texture's first hand-
// over, the one that stalls on Windows - then the log says it happened in
// play ("created in play - not warmed"), the line the warm-up's proof looks
// for not to find.
inline bool HandedInPlay(bool handedBefore) { return !handedBefore; }

}  // namespace obvr::render
