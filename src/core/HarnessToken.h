#pragma once

// The test overlay's gate. OBVR-test.ini, written beside OBVR.dll by the
// hand-script runners for one run, is read over OBVR.ini - and it turns the
// game into a harness game: the save it loads, the script that moves the
// hands. Twice it reached the player's own game: a run cut short left it
// behind (2026-09-30, docs/hand-script-harness.md), and on 2026-10-23 another
// session's overlay lay there while the tester measured in the headset, so
// his game ran that session's script and recorded nothing. Now a runner
// makes a token for its run, puts it into its game's environment as
// OBVR_HARNESS and names it in the overlay as [Debug] HarnessToken, and the
// overlay counts only when the two are the same text: a game started through
// Steam carries no token, so neither a file left behind nor a run beside the
// player's game can reach it. Pure, covered by harness_token_test; Config.cpp
// reads the file and the environment.

#include <cstring>

namespace obvr {

enum class HarnessOverlayVerdict : unsigned char { Accepted, NoFileToken, NoEnvToken, Mismatch };

inline HarnessOverlayVerdict JudgeHarnessOverlay(const char* fileToken, const char* envToken) {
	if (fileToken == nullptr || fileToken[0] == '\0') {
		return HarnessOverlayVerdict::NoFileToken;
	}
	if (envToken == nullptr || envToken[0] == '\0') {
		return HarnessOverlayVerdict::NoEnvToken;
	}
	return std::strcmp(fileToken, envToken) == 0 ? HarnessOverlayVerdict::Accepted : HarnessOverlayVerdict::Mismatch;
}

// The log's words after "Config: TEST OVERLAY <path>".
inline const char* HarnessOverlayVerdictText(HarnessOverlayVerdict verdict) {
	switch (verdict) {
		case HarnessOverlayVerdict::Accepted:
			return "read over OBVR.ini (its HarnessToken is this run's)";
		case HarnessOverlayVerdict::NoFileToken:
			return "IGNORED - it names no HarnessToken (an older runner's file, or one written by hand)";
		case HarnessOverlayVerdict::NoEnvToken:
			return "IGNORED - this game carries no OBVR_HARNESS in its environment, so it is not the runner's: a "
			       "file left behind by a run, or a run beside this game";
		case HarnessOverlayVerdict::Mismatch:
			return "IGNORED - its HarnessToken is another run's";
	}
	return "";
}

}  // namespace obvr
