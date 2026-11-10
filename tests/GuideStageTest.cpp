// Checks the start guide's stage (ui/GuideStage.h): the dome, the main menu
// hidden, the title music silent while the guide is up in front of the main
// menu; the soft track after its delay, faded in, its stretch looped; and all
// of it undone when the guide ends.

#include <cstdio>
#include <limits>

#include "ui/GuideStage.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

using obvr::ui::GuideFrameSeconds;
using obvr::ui::GuideGridAction;
using obvr::ui::GuideStageInput;
using obvr::ui::GuideStagePlan;
using obvr::ui::GuideStageSettings;
using obvr::ui::GuideStageState;
using obvr::ui::GuideTrackHasStretch;
using obvr::ui::GuideTrackVolume;
using obvr::ui::GuideUsesOwnFile;
using obvr::ui::GuideVolumeToHundredthsDb;
using obvr::ui::MainMenuVisibleTrait;
using obvr::ui::StepGuideStage;

GuideStageInput Up(float dt = 0.1f) {
	GuideStageInput in;
	in.guideShowing = true;
	in.inMainMenu = true;
	in.frameSeconds = dt;
	return in;
}

GuideStageSettings Quick() {
	GuideStageSettings s;
	s.musicDelaySeconds = 1.0f;
	s.musicFadeSeconds = 2.0f;
	s.musicVolume = 0.5f;
	return s;
}

// Steps until the track is asked for; returns how many steps it took.
int RunToTrack(GuideStageState& st, const GuideStageSettings& set) {
	StepGuideStage(st, set, Up());
	for (int i = 1; i < 200; ++i) {
		if (StepGuideStage(st, set, Up()).startTrack) {
			return i;
		}
	}
	return -1;
}

void TestComesUp() {
	std::printf("The stage comes up with the guide in front of the main menu\n");
	GuideStageState st;
	const GuideStagePlan p = StepGuideStage(st, Quick(), Up());
	Check(st.on, "on");
	Check(p.grid == GuideGridAction::In, "the dome faded in");
	Check(p.hideMainMenu && p.silenceTitle, "the main menu hidden, the title silent");
	Check(!p.startTrack && !p.setVolume, "no track yet");
	const GuideStagePlan again = StepGuideStage(st, Quick(), Up());
	Check(again.grid == GuideGridAction::None && !again.hideMainMenu && !again.silenceTitle,
	      "the next frame asks nothing again");
}

void TestNotUp() {
	std::printf("Not up without the guide, outside the main menu, or switched off\n");
	GuideStageState st;
	GuideStageInput in = Up();
	in.guideShowing = false;
	Check(StepGuideStage(st, Quick(), in).grid == GuideGridAction::None && !st.on, "no guide: nothing");
	in = Up();
	in.inMainMenu = false;
	Check(StepGuideStage(st, Quick(), in).grid == GuideGridAction::None && !st.on,
	      "the guide in the world (a hand script's door): nothing");
	GuideStageSettings off = Quick();
	off.enabled = false;
	Check(StepGuideStage(st, off, Up()).grid == GuideGridAction::None && !st.on, "GuideDome=0: nothing");
}

void TestTrackAfterDelay() {
	std::printf("The soft track after its delay, faded in\n");
	GuideStageState st;
	const int steps = RunToTrack(st, Quick());
	Check(steps >= 9 && steps <= 11, "asked for after about one second (ten frames of 0.1 s)");
	Check(st.volumeSent == 0.0f, "it starts silent");
	GuideStageInput in = Up();
	in.trackPlaying = false;
	Check(!StepGuideStage(st, Quick(), in).setVolume, "not yet playing: no volume");
	in.trackPlaying = true;
	const GuideStagePlan p = StepGuideStage(st, Quick(), in);
	Check(p.setVolume && p.volume > 0.0f && p.volume < 0.5f, "playing: the fade begins");
	float last = p.volume;
	for (int i = 0; i < 40; ++i) {
		const GuideStagePlan q = StepGuideStage(st, Quick(), in);
		if (q.setVolume) {
			last = q.volume;
		}
	}
	Check(last == 0.5f, "after the fade: its volume");
	Check(!StepGuideStage(st, Quick(), in).setVolume, "steady: no volume handed over again");
}

void TestSilentVolume() {
	std::printf("Volume 0 keeps it silent: no track at all\n");
	GuideStageSettings set = Quick();
	set.musicVolume = 0.0f;
	GuideStageState st;
	Check(RunToTrack(st, set) == -1, "never asked for");
}

void TestStretch() {
	std::printf("The player's own file plays its stretch, round and round\n");
	GuideStageSettings set = Quick();
	set.musicStartSeconds = 148.0f;  // 2:28
	set.musicEndSeconds = 343.0f;    // 5:43
	GuideStageState st;
	RunToTrack(st, set);
	GuideStageInput in = Up();
	in.trackPlaying = true;
	in.ownFile = true;
	in.positionSeconds = 0.0f;
	GuideStagePlan p = StepGuideStage(st, set, in);
	Check(p.seek && p.seekSeconds == 148.0f, "first playing frame: to 2:28");
	in.positionSeconds = 200.0f;
	Check(!StepGuideStage(st, set, in).seek, "inside the stretch: nothing");
	in.positionSeconds = 343.0f;
	p = StepGuideStage(st, set, in);
	Check(p.seek && p.seekSeconds == 148.0f, "at 5:43: back to 2:28");
}

void TestStretchFallback() {
	std::printf("Start and End do not apply to the game's track\n");
	GuideStageSettings set = Quick();
	set.musicStartSeconds = 148.0f;
	set.musicEndSeconds = 343.0f;
	GuideStageState st;
	RunToTrack(st, set);
	GuideStageInput in = Up();
	in.trackPlaying = true;
	in.ownFile = false;
	in.positionSeconds = 0.0f;
	Check(!StepGuideStage(st, set, in).seek, "the fallback plays from its top");
	in.atFileEnd = true;
	const GuideStagePlan p = StepGuideStage(st, set, in);
	Check(p.seek && p.seekSeconds == 0.0f, "at its end: from the top again");
}

void TestStartZero() {
	std::printf("A stretch from 0 seeks only at its end\n");
	GuideStageSettings set = Quick();
	set.musicStartSeconds = 0.0f;
	set.musicEndSeconds = 30.0f;
	GuideStageState st;
	RunToTrack(st, set);
	GuideStageInput in = Up();
	in.trackPlaying = true;
	in.ownFile = true;
	in.positionSeconds = 1.0f;
	Check(!StepGuideStage(st, set, in).seek, "from 0: no first seek");
	in.positionSeconds = 31.0f;
	const GuideStagePlan p = StepGuideStage(st, set, in);
	Check(p.seek && p.seekSeconds == 0.0f, "past its end: back to 0");
}

void TestOwnFileWholeEnd() {
	std::printf("The player's own file without a stretch loops at its end\n");
	GuideStageSettings set = Quick();
	GuideStageState st;
	RunToTrack(st, set);
	GuideStageInput in = Up();
	in.trackPlaying = true;
	in.ownFile = true;
	in.atFileEnd = true;
	const GuideStagePlan p = StepGuideStage(st, set, in);
	Check(p.seek && p.seekSeconds == 0.0f, "at its end: from the top");
}

void TestEnds() {
	std::printf("The guide ends: all of it undone\n");
	GuideStageState st;
	RunToTrack(st, Quick());
	GuideStageInput in = Up();
	in.guideShowing = false;
	const GuideStagePlan p = StepGuideStage(st, Quick(), in);
	Check(p.grid == GuideGridAction::Out, "the dome faded out");
	Check(p.showMainMenu && p.restoreTitle, "the main menu shown, the title music back");
	Check(p.stopTrack, "the soft track closed");
	Check(!st.on && !st.trackStarted, "off, forgotten");
	Check(StepGuideStage(st, Quick(), in).grid == GuideGridAction::None, "nothing more after");
}

void TestEndsBeforeTrack() {
	std::printf("Ended before the track: nothing to close\n");
	GuideStageState st;
	StepGuideStage(st, Quick(), Up());
	GuideStageInput in = Up();
	in.inMainMenu = false;  // a save loaded from the guide's comfort page
	const GuideStagePlan p = StepGuideStage(st, Quick(), in);
	Check(p.grid == GuideGridAction::Out && p.restoreTitle && !p.stopTrack, "out, title back, no track to stop");
}

void TestSwitchedOffWhileUp() {
	std::printf("GuideDome switched off while up: undone\n");
	GuideStageState st;
	StepGuideStage(st, Quick(), Up());
	GuideStageSettings off = Quick();
	off.enabled = false;
	Check(StepGuideStage(st, off, Up()).grid == GuideGridAction::Out, "out");
}

void TestHelpers() {
	std::printf("Helpers\n");
	Check(GuideFrameSeconds(std::numeric_limits<float>::quiet_NaN()) == 0.0f, "a frame time that is no number: 0");
	Check(GuideFrameSeconds(-1.0f) == 0.0f && GuideFrameSeconds(5.0f) == 0.0f, "negative or a hitch: 0");
	Check(GuideFrameSeconds(0.011f) == 0.011f, "a frame: itself");
	GuideStageSettings s = Quick();
	Check(GuideTrackVolume(s, 0.5f) == 0.0f, "before the delay: silent");
	Check(GuideTrackVolume(s, 2.0f) == 0.25f, "half through the fade: half");
	Check(GuideTrackVolume(s, 9.0f) == 0.5f, "after: the volume");
	s.musicFadeSeconds = 0.0f;
	Check(GuideTrackVolume(s, 1.0f) == 0.5f, "no fade: jumps");
	s.musicVolume = 3.0f;
	Check(GuideTrackVolume(s, 1.0f) == 1.0f, "above 1: 1");
	s.musicVolume = -1.0f;
	Check(GuideTrackVolume(s, 1.0f) == 0.0f, "below 0: 0");
	s.musicVolume = std::numeric_limits<float>::quiet_NaN();
	Check(GuideTrackVolume(s, 1.0f) == 0.0f, "no number: 0");
	s.musicStartSeconds = 10.0f;
	s.musicEndSeconds = 5.0f;
	Check(!GuideTrackHasStretch(s), "end before start: the whole file");
	s.musicStartSeconds = -1.0f;
	s.musicEndSeconds = 5.0f;
	Check(!GuideTrackHasStretch(s), "negative start: the whole file");
	s.musicStartSeconds = 1.0f;
	s.musicEndSeconds = std::numeric_limits<float>::infinity();
	Check(!GuideTrackHasStretch(s), "endless end: the whole file");
	Check(GuideVolumeToHundredthsDb(0.0f) == -10000, "0: silent");
	Check(GuideVolumeToHundredthsDb(1.0f) == 0 && GuideVolumeToHundredthsDb(2.0f) == 0, "1 and above: full");
	const long half = GuideVolumeToHundredthsDb(0.5f);
	Check(half <= -601 && half >= -603, "0.5: about -6 dB");
	Check(GuideVolumeToHundredthsDb(0.0002f) <= -7000 && GuideVolumeToHundredthsDb(0.0002f) > -10000,
	      "very quiet: a large negative, not yet silent");
	Check(GuideUsesOwnFile("Data\\Music\\OBVR\\guide.mp3", true), "own file there: it");
	Check(!GuideUsesOwnFile("Data\\Music\\OBVR\\guide.mp3", false), "own file missing: the fallback");
	Check(!GuideUsesOwnFile("", true) && !GuideUsesOwnFile(nullptr, true), "none named: the fallback");
}

}  // namespace

// The main menu's visible trait as Oblivion's XML reads it: &true; is 2 and
// &false; is 1. Shown again with a 1 the menu stayed hidden after the guide
// (Windows, 2026-11-10: "the main menu shown again", then every click on it
// met no tile).
void TestMainMenuVisibleTrait() {
	std::printf("The main menu's visible trait\n");
	Check(MainMenuVisibleTrait(true) == 2.0f, "shown: &true; (2), not 1");
	Check(MainMenuVisibleTrait(false) == 1.0f, "hidden: &false; (1)");
}

int main() {
	TestMainMenuVisibleTrait();
	TestComesUp();
	TestNotUp();
	TestTrackAfterDelay();
	TestSilentVolume();
	TestStretch();
	TestStretchFallback();
	TestStartZero();
	TestOwnFileWholeEnd();
	TestEnds();
	TestEndsBeforeTrack();
	TestSwitchedOffWhileUp();
	TestHelpers();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
