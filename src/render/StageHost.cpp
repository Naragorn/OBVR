#include "render/StageHost.h"

#include "core/Log.h"
#include "game/EngineMovie.h"
#include "game/GuideTrack.h"
#include "game/OptionsMenuVr.h"
#include "game/MenuPause.h"
#include "game/MenuType.h"
#include "game/NativeMenuPrototype.h"
#include "camera/MovieScreen.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"
#include "render/GameDomeJob.h"
#include "ui/GuideStage.h"
#include "ui/MainMenuDome.h"
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
	bool menuDomeInvolved = false;  // the main menu's dome is up or just went (for the log's prefix)
	ui::DomeShareState dome;  // the guide stage and the main menu share one dome (ui/MainMenuDome.h)
	bool ownFile = false;  // the player's own file plays, not the fallback
	long long lastCounter = 0;
	// The grid's alpha said once, a second after it was asked in or out -
	// the compositor's own word that the dome came (or went), for the log.
	float alphaCheckIn = -1.0f;
	bool alphaCheckFadingIn = false;
	bool filmHeld = false;  // the main menu's film is held off the picture (game::HoldMenuFilm)
	ui::GuideAheadState ahead;  // the title music silent before the stage is up
};
StageState& Stage() {
	static StageState s_stage;
	return s_stage;
}

void UpdateLoadingStage(const Config& config, const vr::OpenVRBackend& backend) {
	// The dark model looked for once: the file does not come or go while the
	// game runs, and this runs every frame.
	static char dark[512]{};
	static char sky[512]{};
	static int s_found = -1;
	if (s_found < 0) {
		// Under Wine (Linux, Proton) ntdll exports wine_get_version: the smaller dome there.
		const HMODULE ntdll = GetModuleHandleA("ntdll.dll");
		const char* model = vr::LoadingStageModel(ntdll != nullptr && GetProcAddress(ntdll, "wine_get_version") != nullptr);
		s_found = platform::BuildPluginPath(model, dark, sizeof(dark)) && GetFileAttributesA(dark) != 0xFFFFFFFFu ? 1 : 0;
		if (s_found == 0) {
			OBVR_LOG("Render: the loading stage's model %s is missing beside OBVR.dll - SteamVR's grid stays", model);
		}
	}
	// The sky only where a dome can show at all: wanted, and OBVR holds the compositor.
	const bool scene = backend.IsSceneApplication();
	const bool skyWanted = config.loadingStage && scene && config.loadingStageLook == stage::DomeLook::Sky;
	const int model = vr::ChooseLoadingStageModel(skyWanted, skyWanted && GameDomeReady(sky, sizeof(sky)));
	const char* path = model == vr::kLoadingStageSky ? sky : dark;
	const bool found = model == vr::kLoadingStageSky || s_found == 1;
	switch (vr::PlanLoadingStage(Stage().loading, config.loadingStage, scene, found, model)) {
		case vr::LoadingStageAction::Set: {
			const int error = backend.SetStageOverride(path, vr::LoadingStageRenderSettings());
			vr::NoteLoadingStageAnswer(Stage().loading, error);
			OBVR_LOG("Render: the loading stage %s (%s, compositor answered %d)",
			         error == 0 ? "asked for in place of SteamVR's grid" : "refused", path, error);
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

// The grid's fade itself is asked for once for the guide's stage and the main menu's dome together (ShareDome);
// the guide's own line stays for its edges.
void CarryOut(const Config& config, const vr::OpenVRBackend& backend, const ui::GuideStagePlan& p,
              const ui::DomeSharePlan& share) {
	if (p.grid != ui::GuideGridAction::None) {
		const bool in = p.grid == ui::GuideGridAction::In;
		Stage().alphaCheckIn = 1.2f;
		Stage().alphaCheckFadingIn = in;
		// The grid is faded when the guide's edge is the dome's own; else the main menu's dome stands through it.
		const bool shared = share.grid == ui::DomeGrid::None;
		OBVR_LOG("Guide stage: the dome faded %s (the compositor's grid, OBVR's dome in its place: %s)",
		         in ? (shared ? "in - the guide is up in front of the main menu (the main menu's dome was up already)"
		                      : "in - the guide is up in front of the main menu")
		            : (shared ? "out - the guide is over (the main menu's dome stays)" : "out - the guide is over"),
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
		const bool restarted = game::RestartTitleMusic();
		OBVR_LOG("Guide stage: the title music %s, %s",
		         game::GiveTitleMusicBack() ? "given back its volume" : "not found - no music player",
		         restarted ? "from its beginning" : "where it was (its position could not be set)");
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

// What the main menu's dome is asked this frame (ui/MainMenuDome.h), from the engine's state.
ui::MainMenuDomeInput MenuDomeInput(const Config& config, const vr::OpenVRBackend& backend, const StageFrame& frame) {
	UInt32 stack[game::kMenuStackEntries] = {};
	const UInt32 count = game::MenuStackEntries(stack, game::kMenuStackEntries);
	bool mainMenu = false;
	for (UInt32 i = 0; i < count; ++i) {
		mainMenu = mainMenu || stack[i] == game::kMenuIdMain;
	}
	ui::MainMenuDomeInput in;
	in.enabled = ui::MainMenuDomeEnabled(config.mainMenuDome, config.loadingStage);
	in.scene = backend.IsSceneApplication();
	in.mainMenuInStack = mainMenu;
	in.loading = frame.loading;
	in.filmOwnsScreen = camera::MovieOwnsScreen(game::EngineMoviePlaying(), mainMenu);
	in.worldFrame = frame.worldFrame;
	return in;
}

// The guide's stage and the main menu's dome in one: the scene held while either wants it, the grid faded in by
// the first and out by the last (ui::StepDomeShare), so a hand-over takes nothing out and in.
ui::DomeSharePlan ShareDome(const Config& config, const vr::OpenVRBackend& backend, const StageFrame& frame) {
	ui::DomeShareInput in;
	in.menu = MenuDomeInput(config, backend, frame);
	in.guideOn = Stage().guide.on;
	in.guideHoldsScene = config.guideHoldsScene;
	const ui::DomeSharePlan plan = ui::StepDomeShare(Stage().dome, in);
	if (plan.grid != ui::DomeGrid::None) {
		backend.FadeGrid(1.0f, plan.grid == ui::DomeGrid::In);
		Stage().alphaCheckIn = 1.2f;
		Stage().alphaCheckFadingIn = plan.grid == ui::DomeGrid::In;
	}
	Stage().menuDomeInvolved = plan.menuDomeIn || plan.menuDomeOut || Stage().dome.menuDomeOn;
	if (plan.menuDomeIn) {
		OBVR_LOG("Main menu dome: in - the scene is held off the headset, the main menu's picture stays on its overlay "
		         "(delivery %s%s)", frame.delivery, Stage().guide.on ? ", the start guide's stage is up too" : "");
	}
	if (plan.menuDomeOut) {
		OBVR_LOG("Main menu dome: out - %s (delivery %s)", ui::DomeOutReasonText(plan.reason), frame.delivery);
	}
	return plan;
}

void UpdateGuideStage(const Config& config, const vr::OpenVRBackend& backend, const StageFrame& frame) {
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
	const ui::DomeSharePlan share = ShareDome(config, backend, frame);
	CarryOut(config, backend, plan, share);
	const bool hold = share.holdScene;
	if (hold != backend.SceneHeld()) {
		backend.HoldScene(hold);
		OBVR_LOG("%s: %s", Stage().menuDomeInvolved ? "Stage" : "Guide stage",
		         hold ? "the eyes held off the headset, the last frame cleared - the compositor shows the dome"
		              : "the eyes submitted again");
	}
	// The title music silent from the game's start while a guide is due, so its first bars are not heard before
	// the stage is up (ui::StepGuideAhead).
	const bool guideDue = config.guideDome && config.onboardingStartGuide && config.fullVrMode;
	const bool wasAhead = Stage().ahead.holding;
	switch (ui::StepGuideAhead(Stage().ahead, guideDue, Stage().guide.on, in.inMainMenu, dt)) {
		case ui::GuideAhead::Hold:
			if (game::HoldTitleMusicSilent() && !wasAhead) {
				OBVR_LOG("Guide stage: the title music held silent ahead of the guide");
			}
			break;
		case ui::GuideAhead::GiveBack:
			game::GiveTitleMusicBack();
			OBVR_LOG("Guide stage: no guide came - the title music given back its volume");
			break;
		case ui::GuideAhead::None:
			break;
	}
	if (Stage().guide.on) {
		game::HoldTitleMusicSilent();  // the game may set its volume again at any frame
	}
	// The main menu's film (its map, drawn into the menus' layer by the interface pass) off the picture while the
	// stage is up - asked every frame: the film may only begin once the menu's intro film is over.
	const bool held = game::HoldMenuFilm(Stage().guide.on);
	if (held != Stage().filmHeld) {
		Stage().filmHeld = held;
		OBVR_LOG("Guide stage: the main menu's film %s", held ? "held off the picture behind the guide" : "drawn again");
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

void UpdateStage(const Config& config, const vr::OpenVRBackend& backend, const StageFrame& frame) {
	UpdateLoadingStage(config, backend);
	UpdateGuideStage(config, backend, frame);
	game::StepOptionsVrButtons(config.optionsVrButtons);
}

}  // namespace obvr::render
