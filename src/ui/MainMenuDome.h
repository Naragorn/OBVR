#pragma once

// The main menu's dome (docs/next-up.md, "The loading stage": the main menu's dome; Nadi, 2026-11-28: "das
// hauptmenue screen soll bleiben wie es war. nur den dome noch dazu"). The main menu's picture - its film, its
// buttons - is on the overlay quad, the eyes black on a held pose (camera/FrameLogic.h, CinemaOverlay). With
// the dome the scene is held off the headset instead (vr::OpenVRBackend::HoldScene, the way the start guide's
// stage does it, ui/GuideStage.h), so the compositor shows its stage - OBVR's dome (vr/LoadingStage.h) - around
// the unchanged quad.
//
// The guide's stage and the main menu's dome are one dome: the scene is held while either wants it and the grid
// is faded in when the first of them comes and out when the last one goes, so the guide handing over to the
// main menu (or the main menu to the guide) never takes the dome out and in again.
//
// Pure: one step a frame over plain values; the host (render/StageHost.cpp) carries out the plan. Covered by
// main_menu_dome_test.

namespace obvr::ui {

struct MainMenuDomeInput {
	bool enabled = false;          // [Render] MainMenuDome and [Render] LoadingStage
	bool scene = false;            // OBVR holds the compositor (a scene application)
	bool mainMenuInStack = false;  // the main menu is among the open menus (what it opens - load, options - stands on it)
	bool loading = false;          // a load runs or the loading menu is up: its own picture and dome
	bool filmOwnsScreen = false;   // a film is the picture (the intro films; never with the main menu up)
	bool worldFrame = false;       // the engine drew the world this frame
};

// Why there is no dome, the first of these that holds; None when the dome is wanted.
enum class DomeOutReason { None, Disabled, NoScene, NotMainMenu, Loading, Film, World };

inline bool MainMenuDomeEnabled(bool mainMenuDome, bool loadingStage) { return mainMenuDome && loadingStage; }

inline DomeOutReason MainMenuDomeRefusal(const MainMenuDomeInput& in) {
	if (!in.enabled) return DomeOutReason::Disabled;
	if (!in.scene) return DomeOutReason::NoScene;
	if (!in.mainMenuInStack) return DomeOutReason::NotMainMenu;
	if (in.loading) return DomeOutReason::Loading;
	if (in.filmOwnsScreen) return DomeOutReason::Film;
	if (in.worldFrame) return DomeOutReason::World;
	return DomeOutReason::None;
}

inline const char* DomeOutReasonText(DomeOutReason reason) {
	switch (reason) {
		case DomeOutReason::Disabled: return "switched off";
		case DomeOutReason::NoScene: return "OBVR does not hold the compositor";
		case DomeOutReason::NotMainMenu: return "the main menu is gone";
		case DomeOutReason::Loading: return "a game loads - the loading stage takes over";
		case DomeOutReason::Film: return "a film is the picture";
		case DomeOutReason::World: return "the world is drawn";
		case DomeOutReason::None: break;
	}
	return "wanted";
}

enum class DomeGrid { None, In, Out };

struct DomeShareState {
	bool menuDomeOn = false;  // the main menu's dome was wanted last frame
	bool up = false;          // the grid is faded in (by the main menu's dome or the guide's stage)
};

struct DomeShareInput {
	MainMenuDomeInput menu;
	bool guideOn = false;          // the guide's stage is up this frame
	bool guideHoldsScene = false;  // and its hold is wanted ([Onboarding] GuideHoldsScene)
};

struct DomeSharePlan {
	bool holdScene = false;  // the eyes not submitted this frame
	DomeGrid grid = DomeGrid::None;
	bool menuDomeIn = false;   // the main menu's dome went in (for the log)
	bool menuDomeOut = false;  // and out
	DomeOutReason reason = DomeOutReason::None;  // why, on the frame it went out
};

inline DomeSharePlan StepDomeShare(DomeShareState& s, const DomeShareInput& in) {
	DomeSharePlan p;
	const DomeOutReason refusal = MainMenuDomeRefusal(in.menu);
	const bool wanted = refusal == DomeOutReason::None;
	const bool up = wanted || in.guideOn;
	p.holdScene = wanted || (in.guideOn && in.guideHoldsScene);
	p.menuDomeIn = wanted && !s.menuDomeOn;
	p.menuDomeOut = !wanted && s.menuDomeOn;
	if (p.menuDomeOut) {
		p.reason = refusal;
	}
	if (up && !s.up) {
		p.grid = DomeGrid::In;
	} else if (!up && s.up && refusal != DomeOutReason::Loading) {
		// Into a load the grid is not faded out: the loading stage's dome is what the compositor shows when the
		// load stops the frames, and no switch in between is wanted.
		p.grid = DomeGrid::Out;
	}
	s.menuDomeOn = wanted;
	s.up = up;
	return p;
}

}  // namespace obvr::ui
