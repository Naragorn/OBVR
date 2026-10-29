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
// Two domes: a bright one, parchment and gold, while the menus are what the
// player sees - the start and the main menu, which are bright ("beim start
// auch hell da das menü auch hell is") - and a dark one, nearly black with a
// sepia horizon, once a world has been drawn. The model loads asynchronously,
// so the look is changed ahead of a load: a load shows the look it starts from.
//
// The models are OBVR's own (assets/stage, tools/make-loading-stage.ps1): the
// game's textures are Bethesda's and are not shipped.
//
// Pure decisions over plain values; the calls live in CameraHook
// (UpdateLoadingStage) and OpenVRBackend.

#include "vr/OpenVRTypes.h"

namespace obvr::vr {

enum class LoadingStageLook { Bright, Dark };

// The model beside the DLL, as the package lays it out.
inline const char* LoadingStageModel(LoadingStageLook look) {
	return look == LoadingStageLook::Bright ? "OBVR_Stage\\loading_dome_bright.obj"
	                                        : "OBVR_Stage\\loading_dome_dark.obj";
}

// Which look a frame asks for: bright until the first world frame, dark from
// then on, bright again whenever the main menu is up (a quit to it, and the
// load that follows from it).
struct LoadingStageLookState {
	bool worldSeen = false;
};

inline LoadingStageLook LoadingStageLookFor(LoadingStageLookState& s, bool worldFrame, bool mainMenuUp) {
	if (mainMenuUp) {
		s.worldSeen = false;
	} else if (worldFrame) {
		s.worldSeen = true;
	}
	return s.worldSeen ? LoadingStageLook::Dark : LoadingStageLook::Bright;
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
	bool set = false;      // a dome is in place
	LoadingStageLook setLook = LoadingStageLook::Dark;  // which, while set
	bool refused = false;  // asked and refused (or no model): not asked again until something changes
	LoadingStageLook askedLook = LoadingStageLook::Dark;  // the look last asked for
	bool lastEnabled = false;
	bool lastScene = false;
	LoadingStageLook lastLook = LoadingStageLook::Dark;
};

enum class LoadingStageAction { None, Set, Clear };

// One step: wanted is the INI's switch, scene whether OBVR holds the
// compositor (IsSceneApplication), modelFound whether the look's model file
// is there, look the one this frame asks for. Asks once per look; a refusal
// is not asked again every frame, only after the switch, the scene
// connection or the look changes. Clears when it is no longer wanted while
// set; a scene connection lost takes the override with it on SteamVR's side,
// so that only forgets it.
inline LoadingStageAction PlanLoadingStage(LoadingStageState& s, bool wanted, bool scene, bool modelFound,
                                           LoadingStageLook look = LoadingStageLook::Dark) {
	if (wanted != s.lastEnabled || scene != s.lastScene || look != s.lastLook) {
		s.refused = false;
	}
	s.lastEnabled = wanted;
	s.lastScene = scene;
	s.lastLook = look;
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
	if ((s.set && s.setLook == look) || s.refused) {
		return LoadingStageAction::None;
	}
	if (!modelFound) {
		s.refused = true;
		return LoadingStageAction::None;
	}
	s.askedLook = look;
	return LoadingStageAction::Set;
}

// What the Set call answered: 0 (VRCompositorError_None) puts the asked look
// in place, anything else is a refusal until something changes - a dome
// already in place stays.
inline void NoteLoadingStageAnswer(LoadingStageState& s, int error) {
	if (error == 0) {
		s.set = true;
		s.setLook = s.askedLook;
	} else {
		s.refused = true;
	}
}

}  // namespace obvr::vr
