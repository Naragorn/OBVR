#include "game/DialogZoom.h"

#include "core/Log.h"
#include "core/Types.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

// The figure the Stop Conversation Zoom mod writes into fDlgFocus, past the
// point where UESP says the zoom is virtually gone. Distance, not time: the
// transition still runs, it just has almost nowhere to go.
constexpr float kNoZoomFocus = 15.0f;

// What the slot held before the first override, so DialogZoom=1 returns the
// person's own figure rather than the game's default.
float g_originalFocus = 0.0f;
bool g_originalKnown = false;

bool g_holdReported = false;

}  // namespace

void ApplyDialogZoom(bool zoomWanted) {
	// The slot lives in the game's own data segment, writable as a matter of
	// course - this is a setting being set, not code being patched.
	float* slot = reinterpret_cast<float*>(addr::kDlgFocusSetting);

	switch (DecideDialogFocus(zoomWanted, g_originalKnown, *slot == kNoZoomFocus)) {
		case DialogFocusAction::Nothing:
			return;

		case DialogFocusAction::CaptureAndHold:
			g_originalFocus = *slot;
			g_originalKnown = true;
			*slot = kNoZoomFocus;
			if (!g_holdReported) {
				g_holdReported = true;
				OBVR_LOG("Dialog: fDlgFocus held at %g in memory (the game had %g) - the "
				         "dialogue camera stays back, Oblivion.ini is not touched",
				         static_cast<double>(kNoZoomFocus),
				         static_cast<double>(g_originalFocus));
			}
			return;

		case DialogFocusAction::Hold:
			// Quietly: this is the same hold as before, re-asserted after the
			// game's own INI read or some other write went over it.
			*slot = kNoZoomFocus;
			return;

		case DialogFocusAction::Restore:
			*slot = g_originalFocus;
			OBVR_LOG("Dialog: fDlgFocus restored to %g - the dialogue zoom is back",
			         static_cast<double>(g_originalFocus));
			return;
	}
}

}  // namespace obvr::game
