#pragma once

// The loading stage: what SteamVR shows while Oblivion cannot hand it a
// picture - a load that blocks the game's thread, OBVR's submit with it. The
// compositor falls back to its own grid then (Nadi, 2026-10-29: "statt dem
// steamvr umgebung einfach eine schwarze ... während der ladebildschirme").
// OBVR replaces that grid with its own model, a dome in Oblivion's colours,
// through IVRCompositor::SetStageOverride_Async ("Override the stage model
// used in the compositor to replace the grid", openvr.h) - and gives the grid
// back with ClearStageOverride when it goes.
//
// Two domes ([Render] LoadingStageLook, stage/DomeLook.h; Nadi, 2026-11-15):
//
//   * the dark one, OBVR's own painting, shipped (assets/stage, written by
//     tools/make-loading-stage.cpp): nearly black with a sepia horizon.
//     Until 2026-10-30 there was a bright one for the start and the main
//     menu as well ("light dome entfernen").
//   * the sky, made from the game's own textures on the player's machine
//     and kept beside OBVR.dll (render/GameDomeJob.h): the game's textures
//     are Bethesda's and are not shipped. Until it is made, and when it
//     cannot be, the dark one shows.
//
// Pure decisions over plain values; the calls live in render/StageHost.cpp
// (UpdateLoadingStage) and OpenVRBackend.

#include "vr/OpenVRTypes.h"

namespace obvr::vr {

// The dark model beside the DLL, as the package lays it out.
inline const char* LoadingStageModel() { return "OBVR_Stage\\loading_dome_dark.obj"; }

// Which of the two models: the numbers PlanLoadingStage is told.
inline constexpr int kLoadingStageDark = 0;
inline constexpr int kLoadingStageSky = 1;

// The sky when it is asked for and made, else the dark one.
inline int ChooseLoadingStageModel(bool skyWanted, bool skyReady) {
	return skyWanted && skyReady ? kLoadingStageSky : kLoadingStageDark;
}

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
	bool set = false;      // the dome is in place
	bool refused = false;  // asked and refused (or no model): not asked again until something changes
	bool lastEnabled = false;
	bool lastScene = false;
	int lastModel = kLoadingStageDark;   // the model the last step was told
	int setModel = kLoadingStageDark;    // the one in place while set
	int askedModel = kLoadingStageDark;  // the one the last Set asked for
};

enum class LoadingStageAction { None, Set, Clear };

// One step: wanted is the INI's switch, scene whether OBVR holds the
// compositor (IsSceneApplication), model which dome and modelFound whether
// its file is there. Asks once; a refusal is not asked again every frame,
// only after the switch, the scene connection or the model changes. Another
// model while one is set is asked for over it (the compositor loads it and
// swaps; a refusal keeps the one in place). Clears when it is no longer
// wanted while set; a scene connection lost takes the override with it on
// SteamVR's side, so that only forgets it.
inline LoadingStageAction PlanLoadingStage(LoadingStageState& s, bool wanted, bool scene, bool modelFound,
                                           int model = kLoadingStageDark) {
	if (wanted != s.lastEnabled || scene != s.lastScene || model != s.lastModel) {
		s.refused = false;
	}
	s.lastEnabled = wanted;
	s.lastScene = scene;
	s.lastModel = model;
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
	if ((s.set && s.setModel == model) || s.refused) {
		return LoadingStageAction::None;
	}
	if (!modelFound) {
		s.refused = true;
		return LoadingStageAction::None;
	}
	s.askedModel = model;
	return LoadingStageAction::Set;
}

// What the Set call answered: 0 (VRCompositorError_None) puts the dome in
// place, anything else is a refusal until something changes.
inline void NoteLoadingStageAnswer(LoadingStageState& s, int error) {
	if (error == 0) {
		s.set = true;
		s.setModel = s.askedModel;
	} else {
		s.refused = true;
	}
}

}  // namespace obvr::vr
