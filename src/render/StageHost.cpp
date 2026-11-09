#include "render/StageHost.h"

#include "core/Log.h"
#include "game/GuideTrack.h"
#include "game/NativeMenuPrototype.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"
#include "ui/GuideStage.h"
#include "vr/LoadingStage.h"

namespace obvr::render {

namespace {

// The loading stage (vr/LoadingStage.h): OBVR's dome in place of SteamVR's
// grid while a load stops the game's frames. It calls the compositor only
// when the plan changes.
// What the stage keeps from frame to frame, owned here.
struct StageState {
	vr::LoadingStageState loading;
	ui::GuideStageState guide;
	bool ownFile = false;  // the player's own file plays, not the fallback
	long long lastCounter = 0;
	// The grid's alpha said once, a second after it was asked in or out -
	// the compositor's own word that the dome came (or went), for the log.
	float alphaCheckIn = -1.0f;
	bool alphaCheckFadingIn = false;
};
StageState& Stage() {
	static StageState s_stage;
	return s_stage;
}

void UpdateLoadingStage(const Config& config, const vr::OpenVRBackend& backend) {
	// The model looked for once: the file does not come or go while the game
	// runs, and this runs every frame.
	static char model[512]{};
	static int s_found = -1;
	if (s_found < 0) {
		s_found = platform::BuildPluginPath(vr::LoadingStageModel(), model, sizeof(model)) &&
		                  GetFileAttributesA(model) != 0xFFFFFFFFu
		              ? 1
		              : 0;
		if (s_found == 0) {
			OBVR_LOG("Render: the loading stage's model %s is missing beside OBVR.dll - SteamVR's grid stays",
			         vr::LoadingStageModel());
		}
	}
	switch (vr::PlanLoadingStage(Stage().loading, config.loadingStage, backend.IsSceneApplication(), s_found == 1)) {
		case vr::LoadingStageAction::Set: {
			const int error = backend.SetStageOverride(model, vr::LoadingStageRenderSettings());
			vr::NoteLoadingStageAnswer(Stage().loading, error);
			OBVR_LOG("Render: the loading stage %s (%s, compositor answered %d)",
			         error == 0 ? "asked for in place of SteamVR's grid" : "refused", model, error);
			break;
		}
		case vr::LoadingStageAction::Clear:
			backend.ClearStageOverride();
			OBVR_LOG("Render: the loading stage switched off - SteamVR's grid is back");
			break;
		case vr::LoadingStageAction::None:
			break;
	}
}


float FrameSeconds() {
	const long long now = ReadPerformanceCounter();
	const long long frequency = ReadPerformanceFrequency();
	const long long last = Stage().lastCounter;
	Stage().lastCounter = now;
	if (last == 0 || frequency <= 0) {
		return 0.0f;
	}
	return static_cast<float>(static_cast<double>(now - last) / static_cast<double>(frequency));
}

ui::GuideStageSettings GuideSettings(const Config& config) {
	ui::GuideStageSettings s;
	s.enabled = config.guideDome;
	s.musicDelaySeconds = config.guideMusicDelay;
	s.musicFadeSeconds = config.guideMusicFade;
	s.musicVolume = config.guideMusicVolume;
	s.musicStartSeconds = config.guideMusicStart;
	s.musicEndSeconds = config.guideMusicEnd;
	return s;
}

void StartTrack(const Config& config) {
	const char* own = config.guideMusicFile;
	Stage().ownFile = ui::GuideUsesOwnFile(own, own[0] != '\0' && GetFileAttributesA(own) != 0xFFFFFFFFu);
	const char* const path = Stage().ownFile ? own : ui::kGuideFallbackTrack;
	const char* why = "";
	if (game::OpenGuideTrack(path, &why)) {
		OBVR_LOG("Guide stage: the soft track started - %s (%s), %.0f s long, to volume %.2f over %.0f s%s", path,
		         Stage().ownFile ? "the player's own file" : "the game's track, no own file there",
		         static_cast<double>(game::GuideTrackDuration()), static_cast<double>(config.guideMusicVolume),
		         static_cast<double>(config.guideMusicFade),
		         Stage().ownFile && ui::GuideTrackHasStretch(GuideSettings(config)) ? ", its stretch looped" : "");
	} else {
		OBVR_LOG("Guide stage: the soft track could not start - %s: %s", path, why);
	}
}

void CarryOut(const Config& config, const vr::OpenVRBackend& backend, const ui::GuideStagePlan& p) {
	if (p.grid != ui::GuideGridAction::None) {
		const bool in = p.grid == ui::GuideGridAction::In;
		backend.FadeGrid(1.0f, in);
		Stage().alphaCheckIn = 1.2f;
		Stage().alphaCheckFadingIn = in;
		OBVR_LOG("Guide stage: the dome faded %s (the compositor's grid, OBVR's dome in its place: %s)",
		         in ? "in - the guide is up in front of the main menu" : "out - the guide is over",
		         backend.IsSceneApplication() ? "scene" : "no scene, nothing to fade");
	}
	if (p.hideMainMenu || p.showMainMenu) {
		const bool done = game::SetMainMenuShown(p.showMainMenu);
		OBVR_LOG("Guide stage: the main menu %s%s", p.showMainMenu ? "shown again" : "hidden behind the guide",
		         done ? "" : " - not loaded, nothing to do");
	}
	if (p.silenceTitle) {
		OBVR_LOG("Guide stage: the title music %s",
		         game::HoldTitleMusicSilent() ? "held silent" : "not found - no music player");
	}
	if (p.restoreTitle) {
		OBVR_LOG("Guide stage: the title music %s",
		         game::GiveTitleMusicBack() ? "given back its volume" : "not found - no music player");
	}
	if (p.stopTrack) {
		game::CloseGuideTrack();
		OBVR_LOG("Guide stage: the soft track closed");
	}
	if (p.startTrack) {
		StartTrack(config);
	}
	if (p.seek) {
		game::SeekGuideTrack(p.seekSeconds);
		OBVR_LOG("Guide stage: the soft track to %.1f s", static_cast<double>(p.seekSeconds));
	}
	if (p.setVolume) {
		game::SetGuideTrackVolume(p.volume);
		static int s_volumeLines = 0;
		const bool settled = p.volume == ui::GuideTrackVolume(GuideSettings(config), 1.0e9f);
		if (settled || s_volumeLines < 3) {
			++s_volumeLines;
			OBVR_LOG("Guide stage: the soft track at volume %.3f (%ld hundredths of a dB)%s",
			         static_cast<double>(p.volume), ui::GuideVolumeToHundredthsDb(p.volume),
			         settled ? " - its set volume" : "");
		}
	}
}

void UpdateGuideStage(const Config& config, const vr::OpenVRBackend& backend) {
	const float dt = FrameSeconds();
	ui::GuideStageInput in;
	in.guideShowing = game::StartGuideShowing();
	in.inMainMenu = game::MainMenuLoaded();
	in.frameSeconds = dt;
	in.trackPlaying = game::GuideTrackOpen();
	in.ownFile = Stage().ownFile;
	if (in.trackPlaying) {
		in.positionSeconds = game::GuideTrackPosition();
		const float duration = game::GuideTrackDuration();
		in.atFileEnd = duration > 0.0f && in.positionSeconds >= duration - 0.05f;
	}
	const ui::GuideStagePlan plan = ui::StepGuideStage(Stage().guide, GuideSettings(config), in);
	CarryOut(config, backend, plan);
	if (Stage().guide.on) {
		game::HoldTitleMusicSilent();  // the game may set its volume again at any frame
	}
	if (Stage().alphaCheckIn > 0.0f) {
		Stage().alphaCheckIn -= ui::GuideFrameSeconds(dt);
		if (Stage().alphaCheckIn <= 0.0f) {
			OBVR_LOG("Guide stage: a second after the dome faded %s the compositor's grid alpha is %.2f",
			         Stage().alphaCheckFadingIn ? "in" : "out", static_cast<double>(backend.CurrentGridAlpha()));
		}
	}
}

}  // namespace

void UpdateStage(const Config& config, const vr::OpenVRBackend& backend) {
	UpdateLoadingStage(config, backend);
	UpdateGuideStage(config, backend);
}

}  // namespace obvr::render
