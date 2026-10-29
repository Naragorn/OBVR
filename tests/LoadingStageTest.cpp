// Checks the loading stage's decisions (vr/LoadingStage.h): when OBVR asks
// SteamVR for its dome in place of the grid, when it gives the grid back, and
// that a refusal is not asked again every frame.

#include <cstdio>

#include "vr/LoadingStage.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

using obvr::vr::LoadingStageAction;
using obvr::vr::LoadingStageState;
using obvr::vr::NoteLoadingStageAnswer;
using obvr::vr::PlanLoadingStage;

void TestSetOnce() {
	std::printf("Asked once, then left in place\n");
	LoadingStageState s;
	Check(PlanLoadingStage(s, true, true, true) == LoadingStageAction::Set, "wanted, scene, model: set");
	NoteLoadingStageAnswer(s, 0);
	Check(s.set, "answered 0: in place");
	Check(PlanLoadingStage(s, true, true, true) == LoadingStageAction::None, "the next frame asks nothing");
}

void TestOff() {
	std::printf("Switched off\n");
	LoadingStageState s;
	Check(PlanLoadingStage(s, false, true, true) == LoadingStageAction::None, "off and never set: nothing");
	Check(PlanLoadingStage(s, true, true, true) == LoadingStageAction::Set, "switched on: set");
	NoteLoadingStageAnswer(s, 0);
	Check(PlanLoadingStage(s, false, true, true) == LoadingStageAction::Clear, "switched off while set: clear");
	Check(!s.set, "and forgotten");
	Check(PlanLoadingStage(s, false, true, true) == LoadingStageAction::None, "cleared once only");
	Check(PlanLoadingStage(s, true, true, true) == LoadingStageAction::Set, "on again: set again");
}

void TestNoScene() {
	std::printf("No scene connection\n");
	LoadingStageState s;
	Check(PlanLoadingStage(s, true, false, true) == LoadingStageAction::None, "no compositor: nothing to ask");
	Check(PlanLoadingStage(s, true, true, true) == LoadingStageAction::Set, "connected: set");
	NoteLoadingStageAnswer(s, 0);
	Check(PlanLoadingStage(s, true, false, true) == LoadingStageAction::None, "connection lost: no call");
	Check(!s.set, "the override forgotten with it");
	Check(PlanLoadingStage(s, true, true, true) == LoadingStageAction::Set, "connected again: set again");
}

void TestRefusals() {
	std::printf("Refusals are not asked again every frame\n");
	LoadingStageState s;
	Check(PlanLoadingStage(s, true, true, false) == LoadingStageAction::None, "no model file: nothing");
	Check(s.refused, "noted as refused");
	Check(PlanLoadingStage(s, true, true, true) == LoadingStageAction::None,
	      "the model there later: not asked until something changes");

	LoadingStageState r;
	Check(PlanLoadingStage(r, true, true, true) == LoadingStageAction::Set, "set asked");
	NoteLoadingStageAnswer(r, 1);  // VRCompositorError_RequestFailed
	Check(!r.set && r.refused, "answered 1: refused, not in place");
	Check(PlanLoadingStage(r, true, true, true) == LoadingStageAction::None, "not asked again");
	Check(PlanLoadingStage(r, false, true, true) == LoadingStageAction::None, "switched off: nothing to clear");
	Check(PlanLoadingStage(r, true, true, true) == LoadingStageAction::Set, "switched on again: asked again");

	LoadingStageState c;
	PlanLoadingStage(c, true, true, true);
	NoteLoadingStageAnswer(c, 1);
	PlanLoadingStage(c, true, false, true);
	Check(PlanLoadingStage(c, true, true, true) == LoadingStageAction::Set, "reconnected after a refusal: asked again");
}


void TestLooks() {
	std::printf("Bright before the first world, dark after, bright again at the main menu\n");
	using obvr::vr::LoadingStageLook;
	using obvr::vr::LoadingStageLookFor;
	obvr::vr::LoadingStageLookState l;
	Check(LoadingStageLookFor(l, false, false) == LoadingStageLook::Bright, "the start: bright, the menus are");
	Check(LoadingStageLookFor(l, false, true) == LoadingStageLook::Bright, "the main menu: bright");
	Check(LoadingStageLookFor(l, true, false) == LoadingStageLook::Dark, "the first world frame: dark");
	Check(LoadingStageLookFor(l, false, false) == LoadingStageLook::Dark, "a load after it: still dark");
	Check(LoadingStageLookFor(l, false, true) == LoadingStageLook::Bright, "back at the main menu: bright again");
	Check(LoadingStageLookFor(l, false, false) == LoadingStageLook::Bright, "and the load from it: bright");
	Check(LoadingStageLookFor(l, true, true) == LoadingStageLook::Bright, "a world drawn behind the main menu: bright");

	Check(obvr::vr::LoadingStageModel(LoadingStageLook::Bright) != obvr::vr::LoadingStageModel(LoadingStageLook::Dark),
	      "two models");
}

void TestLookChange() {
	std::printf("A change of look asks for the other model\n");
	using obvr::vr::LoadingStageLook;
	LoadingStageState s;
	Check(PlanLoadingStage(s, true, true, true, LoadingStageLook::Bright) == LoadingStageAction::Set, "bright set");
	NoteLoadingStageAnswer(s, 0);
	Check(s.set && s.setLook == LoadingStageLook::Bright, "bright in place");
	Check(PlanLoadingStage(s, true, true, true, LoadingStageLook::Bright) == LoadingStageAction::None, "kept");
	Check(PlanLoadingStage(s, true, true, true, LoadingStageLook::Dark) == LoadingStageAction::Set, "dark wanted: set");
	NoteLoadingStageAnswer(s, 0);
	Check(s.setLook == LoadingStageLook::Dark, "dark in place");

	// A refused look is asked again once the look changes, and not before.
	LoadingStageState r;
	PlanLoadingStage(r, true, true, true, LoadingStageLook::Bright);
	NoteLoadingStageAnswer(r, 1);
	Check(PlanLoadingStage(r, true, true, true, LoadingStageLook::Bright) == LoadingStageAction::None,
	      "bright refused: not asked again");
	Check(PlanLoadingStage(r, true, true, true, LoadingStageLook::Dark) == LoadingStageAction::Set,
	      "the look changed: dark asked");
	NoteLoadingStageAnswer(r, 1);
	Check(!r.set, "refused too: nothing in place");

	// Refused while another look stood: that one stays in place.
	LoadingStageState k;
	PlanLoadingStage(k, true, true, true, LoadingStageLook::Bright);
	NoteLoadingStageAnswer(k, 0);
	PlanLoadingStage(k, true, true, true, LoadingStageLook::Dark);
	NoteLoadingStageAnswer(k, 1);
	Check(k.set && k.setLook == LoadingStageLook::Bright, "the dark refused: the bright one still stands");
}
void TestRenderSettings() {
	std::printf("Render settings\n");
	const obvr::vr::openvr::StageRenderSettings s = obvr::vr::LoadingStageRenderSettings();
	Check(!s.backfaceCulling, "seen from inside: no backface culling");
	Check(s.vignetteInnerRadius > 20.0f && s.vignetteOuterRadius > s.vignetteInnerRadius,
	      "the vignette past the 20 m dome: nothing fades");
	Check(s.fresnelStrength == 0.0f && !s.greyscale && !s.wireframe, "no fresnel, colour, solid");
	Check(s.primary[0] == 1.0f && s.primary[1] == 1.0f && s.primary[2] == 1.0f && s.primary[3] == 1.0f,
	      "untinted: the texture as painted");
}

}  // namespace

int main() {
	std::printf("OBVR loading stage test\n\n");
	TestSetOnce();
	TestOff();
	TestNoScene();
	TestRefusals();
	TestLooks();
	TestLookChange();
	TestRenderSettings();
	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}
	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
