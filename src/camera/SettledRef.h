#pragma once

// A settled reference (game/PickHold.h) with the fingerprint of what it
// shows: the thing shown can be deleted while the settle keeps it - an item
// taken with Activate is, and its memory reused - and is dropped before
// anything reads through it again (game/RefFingerprint.h; the crash of
// 2026-10-12, a shield taken from the floor). The reach ring's and the
// Target row's.

#include "game/PickHold.h"
#include "game/RefFingerprint.h"

namespace obvr::camera {

struct SettledRef {
	game::RefSettleState settle;
	game::RefFingerprint print;

	UInt32 Step(UInt32 wanted, float dt, UInt32 trusted) {
		const bool gone = settle.shown != 0 && !game::RefStillThere(print);
		const UInt32 shown = game::StepRefSettle(settle, wanted, dt, trusted, game::kShownSettleSeconds, gone);
		if (shown != print.ref) {
			print = game::TakeRefFingerprint(shown);
		}
		return shown;
	}
};

}  // namespace obvr::camera
