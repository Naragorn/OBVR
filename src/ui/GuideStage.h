#pragma once

// The start guide's stage (docs/onboarding-spec.md, "The guide in the dome"):
// while the first start's chain (ui/FirstStart.h) runs in front of the main
// menu, the headset shows OBVR's dark dome (vr/LoadingStage.h) instead of the
// main menu, the main menu's own picture is hidden behind the guide's window,
// the game's title music is silent and, after a while, a very soft track fades
// in. When the guide ends, the dome goes, the main menu shows and the title
// music is back as it was. The menu clicks and every other sound stay.
//
// Nadi, 2026-11-09: the guide before the main menu in the dome, "nicht ohne
// Sounds und ohne Mukke, irgendwann super seichtes" - sounds yes, the game's
// music no, after a while something very soft; his own track (a file he puts
// there himself, OBVR does not ship one) from 2:28 to 5:43.
//
// Pure: one step a frame over plain values; the host (game/GuideStage.cpp)
// carries out what the plan says. Covered by guide_stage_test.

#include <cmath>

namespace obvr::ui {

struct GuideStageSettings {
	bool enabled = true;          // [Onboarding] GuideDome
	float musicDelaySeconds = 20.0f;  // [Onboarding] GuideMusicDelay: silence before the soft track
	float musicFadeSeconds = 10.0f;   // [Onboarding] GuideMusicFade: from silent to its volume
	float musicVolume = 0.25f;        // [Onboarding] GuideMusicVolume, 0..1 of full; 0 keeps it silent
	// Where the track plays from and to, seconds, for the player's own file
	// ([Onboarding] GuideMusicStart / GuideMusicEnd); an end at or before the
	// start plays to the file's end. At the end it starts again from Start.
	float musicStartSeconds = 0.0f;
	float musicEndSeconds = 0.0f;
};

struct GuideStageState {
	bool on = false;            // the stage is up: dome in, main menu hidden, title silent
	float seconds = 0.0f;       // since it went up
	bool trackStarted = false;  // the soft track was asked for
	bool trackPlaying = false;  // and it plays (the host's answer)
	bool startSought = false;   // the track's start position was set once it played
	float volumeSent = -1.0f;   // the last volume handed over (-1: none yet)
};

struct GuideStageInput {
	bool guideShowing = false;  // the chain (or its comfort page) is up
	bool inMainMenu = false;    // the main menu is the menu underneath
	float frameSeconds = 0.0f;
	bool trackPlaying = false;  // the soft track's graph runs
	float positionSeconds = -1.0f;  // where it is, -1 unknown
	bool atFileEnd = false;     // it has played to the file's end
	bool ownFile = false;       // the player's own file plays (Start/End apply), not the fallback
};

enum class GuideGridAction { None, In, Out };

struct GuideStagePlan {
	GuideGridAction grid = GuideGridAction::None;
	bool hideMainMenu = false;    // hide it now
	bool showMainMenu = false;    // show it again now
	bool silenceTitle = false;    // the title music to silent now
	bool restoreTitle = false;    // the title music back as it was now
	bool startTrack = false;      // open and play the soft track now
	bool stopTrack = false;       // close it now
	bool setVolume = false;       // hand over volume now
	float volume = 0.0f;          // 0..1
	bool seek = false;            // move the track to seekSeconds now
	float seekSeconds = 0.0f;
};

// The game's own soft track when the player's file is not there (measured
// 2026-11-09 with ffmpeg's ebur128: -19.9 LUFS integrated, 281 s - the
// quietest of the exploration tracks).
inline constexpr const char* kGuideFallbackTrack = "Data\\Music\\Explore\\atmosphere_04.mp3";

// The player's file plays when one is named and it is there; otherwise the
// game's track, whole (Start and End belong to the player's file).
inline bool GuideUsesOwnFile(const char* ownPath, bool ownExists) {
	return ownPath != nullptr && ownPath[0] != '\0' && ownExists;
}

// Not a number, negative or absurd frame times count as nothing.
inline float GuideFrameSeconds(float seconds) {
	return std::isfinite(seconds) && seconds > 0.0f && seconds < 1.0f ? seconds : 0.0f;
}

// The volume at this moment: silent before the delay, then a straight ramp
// over the fade to the set volume. A fade of zero (or less) jumps.
inline float GuideTrackVolume(const GuideStageSettings& s, float secondsUp) {
	const float target = std::isfinite(s.musicVolume) ? (s.musicVolume < 0.0f ? 0.0f : (s.musicVolume > 1.0f ? 1.0f : s.musicVolume)) : 0.0f;
	const float since = secondsUp - s.musicDelaySeconds;
	if (since < 0.0f) {
		return 0.0f;
	}
	if (!(s.musicFadeSeconds > 0.0f)) {
		return target;
	}
	const float t = since / s.musicFadeSeconds;
	return t >= 1.0f ? target : target * t;
}

// Whether Start/End make a stretch to play: both finite, start not negative,
// end after start. Otherwise the file plays whole.
inline bool GuideTrackHasStretch(const GuideStageSettings& s) {
	return std::isfinite(s.musicStartSeconds) && std::isfinite(s.musicEndSeconds) && s.musicStartSeconds >= 0.0f &&
	       s.musicEndSeconds > s.musicStartSeconds;
}

// DirectShow's IBasicAudio volume: hundredths of a decibel, 0 full, -10000
// silent. 0..1 of full is taken as amplitude (20 log10).
inline long GuideVolumeToHundredthsDb(float volume) {
	if (!(volume > 0.0001f)) {
		return -10000;
	}
	if (volume >= 1.0f) {
		return 0;
	}
	// log10 in double: the C89 one every msvcrt exports.
	const float db = static_cast<float>(20.0 * std::log10(static_cast<double>(volume)));
	const long hundredths = static_cast<long>(db * 100.0f);
	return hundredths < -10000 ? -10000 : hundredths;
}

// The value the main menu's visible trait (0x0FA1) is written with: Oblivion's
// XML booleans are &true; = 2 and &false; = 1, any other number false. Shown
// again with a 1 the main menu stayed hidden after the guide (Windows,
// 2026-11-10: "the main menu shown again", and every click after it met no
// tile of the main menu).
inline float MainMenuVisibleTrait(bool shown) { return shown ? 2.0f : 1.0f; }

inline GuideStagePlan StepGuideStage(GuideStageState& s, const GuideStageSettings& set, const GuideStageInput& in) {
	GuideStagePlan p;
	const bool wanted = set.enabled && in.guideShowing && in.inMainMenu;
	if (!s.on) {
		if (!wanted) {
			return p;
		}
		s = GuideStageState{};
		s.on = true;
		p.grid = GuideGridAction::In;
		p.hideMainMenu = true;
		p.silenceTitle = true;
		return p;
	}
	if (!wanted) {
		p.grid = GuideGridAction::Out;
		p.showMainMenu = true;
		p.restoreTitle = true;
		p.stopTrack = s.trackStarted;
		s = GuideStageState{};
		return p;
	}
	s.seconds += GuideFrameSeconds(in.frameSeconds);
	s.trackPlaying = in.trackPlaying;
	const bool audible = set.musicVolume > 0.0f;
	if (!s.trackStarted) {
		if (audible && s.seconds >= set.musicDelaySeconds) {
			s.trackStarted = true;
			p.startTrack = true;
			p.setVolume = true;
			p.volume = 0.0f;
			s.volumeSent = 0.0f;
		}
		return p;
	}
	if (!s.trackPlaying) {
		return p;
	}
	const bool stretch = in.ownFile && GuideTrackHasStretch(set);
	if (stretch && !s.startSought) {
		s.startSought = true;
		if (set.musicStartSeconds > 0.0f) {
			p.seek = true;
			p.seekSeconds = set.musicStartSeconds;
		}
	} else if ((stretch && in.positionSeconds >= set.musicEndSeconds) || in.atFileEnd) {
		// Round again: from Start for a stretch, from the top otherwise.
		p.seek = true;
		p.seekSeconds = stretch ? set.musicStartSeconds : 0.0f;
	}
	const float v = GuideTrackVolume(set, s.seconds);
	// Small steps are left out, but the last one, onto the set volume, never.
	const bool settled = v == GuideTrackVolume(set, s.seconds + set.musicFadeSeconds + 1.0f);
	if (v != s.volumeSent && (std::fabs(v - s.volumeSent) >= 0.005f || settled)) {
		p.setVolume = true;
		p.volume = v;
		s.volumeSent = v;
	}
	return p;
}

}  // namespace obvr::ui
