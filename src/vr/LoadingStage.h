#pragma once

// The loading stage: what SteamVR shows while Oblivion cannot hand it a
// picture - a load that blocks the game's thread, OBVR's submit with it. The
// compositor falls back to its own grid then (Nadi, 2026-10-29: "statt dem
// steamvr umgebung einfach eine schwarze ... während der ladebildschirme").
// OBVR replaces that grid with its own model, a dome nearly black with a warm
// sepia glow at the horizon, through IVRCompositor::SetStageOverride_Async
// ("Override the stage model used in the compositor to replace the grid",
// openvr.h) - and gives the grid back with ClearStageOverride when it goes.
//
// The model is OBVR's own (assets/stage, tools/make-loading-stage.ps1): the
// game's textures are Bethesda's and are not shipped.
//
// Pure decisions over plain values; the calls live in CameraHook
// (UpdateLoadingStage) and OpenVRBackend.

#include "vr/OpenVRTypes.h"

namespace obvr::vr {

// The model beside the DLL, as the package lays it out.
inline constexpr const char* kLoadingStageModel = "OBVR_Stage\\loading_dome.obj";

// How the compositor draws the dome. Seen from inside, so no backface
// culling. The vignette and the fresnel both fade towards the secondary
// colour; the vignette's radii are put far past the dome (20 m) so nothing
// fades and the texture is shown as painted, untinted.
inline openvr::StageRenderSettings LoadingStageRenderSettings() {
	openvr::StageRenderSettings s;
	s.vignetteInnerRadius = 1000.0f;
	s.vignetteOuterRadius = 1001.0f;
	s.fresnelStrength = 0.0f;
	s.backfaceCulling = false;
	return s;
}

struct LoadingStageState {
	bool set = false;      // the override is in place
	bool refused = false;  // asked and refused (or no model): not asked again until something changes
	bool lastEnabled = false;
	bool lastScene = false;
};

enum class LoadingStageAction { None, Set, Clear };

// One step: wanted is the INI's switch, scene whether OBVR holds the
// compositor (IsSceneApplication), modelFound whether the model file is
// there. Asks once; a refusal is not asked again every frame, only after the
// switch or the scene connection changes. Clears when it is no longer wanted
// while set; a scene connection lost takes the override with it on
// SteamVR's side, so that only forgets it.
inline LoadingStageAction PlanLoadingStage(LoadingStageState& s, bool wanted, bool scene, bool modelFound) {
	if (wanted != s.lastEnabled || scene != s.lastScene) {
		s.refused = false;
	}
	s.lastEnabled = wanted;
	s.lastScene = scene;
	if (!scene) {
		s.set = false;
		return LoadingStageAction::None;
	}
	if (!wanted) {
		if (s.set) {
			s.set = false;
			return LoadingStageAction::Clear;
		}
		return LoadingStageAction::None;
	}
	if (s.set || s.refused) {
		return LoadingStageAction::None;
	}
	if (!modelFound) {
		s.refused = true;
		return LoadingStageAction::None;
	}
	return LoadingStageAction::Set;
}

// What the Set call answered: 0 (VRCompositorError_None) puts it in place,
// anything else is a refusal until something changes.
inline void NoteLoadingStageAnswer(LoadingStageState& s, int error) {
	if (error == 0) {
		s.set = true;
	} else {
		s.refused = true;
	}
}

}  // namespace obvr::vr
