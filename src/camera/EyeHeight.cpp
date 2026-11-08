#include "camera/EyeHeight.h"

#include <cmath>

#include "core/Log.h"
#include "game/MenuMode.h"
#include "game/MenuType.h"
#include "game/PlayerAim.h"
#include "game/PlayerTeleport.h"
#include "game/RefFingerprint.h"
#include "test/HandScriptRuntime.h"
#include "vr/RealHeight.h"

namespace obvr::camera {

// The game's sneak lowers the first-person eye (Camera01's parents carry it,
// FUN_0066b710 reads it), 24.7 units on top of what a real crouch already
// took (crouch-camera.txt, 2026-10-24): in Full VR the engine's eye is held
// at its standing height above the feet, and the lift goes onto the camera
// with the head's offset (camera/SneakEyeHold.h). The engine's eye is logged
// around each change of the sneak - forty frames, six changes a game - and
// at every mark.
float StepSneakEyeLift(const Config& config, const EyeHeightFrame& f) {
	static SneakEyeHoldState s_sneakEye;
	static bool s_sneakSeen = false;
	static UInt32 s_sneakTraceLeft = 0;
	static UInt32 s_sneakChangesLeft = 6;
	static UInt32 s_sneakEventLines = 24;
	const bool sneaking = !f.thirdPerson && game::IsPlayerSneaking();
	SneakEyeHoldInput hold;
	hold.enabled = config.fullVrMode && f.headset && config.hands.keepEyeHeightSneaking;
	hold.live = !f.thirdPerson && !game::PlayerRiding() && f.playerValid;
	hold.loading = game::LoadingThreadActive();
	hold.sneaking = sneaking;
	// Only a real crouch's sneak is held; the stick's is the game's own
	// (Nadi, 2026-10-26). The crouch is the hand mode's, a frame old here.
	hold.crouched = f.crouched;
	hold.eyeAboveFeet = f.eyeAboveFeet;
	hold.dt = f.dtSeconds;
	const SneakEyeHoldVerdict held = StepSneakEyeHold(s_sneakEye, hold);
	const char* const word = SneakEyeEventWord(held.event);
	if (word != nullptr && s_sneakEventLines > 0) {
		--s_sneakEventLines;
		OBVR_LOG("Camera: %s (the engine's eye %.1f units above the feet, lifted %.1f)", word,
		         static_cast<double>(f.eyeAboveFeet), static_cast<double>(held.lift));
	}
	if (sneaking != s_sneakSeen) {
		s_sneakSeen = sneaking;
		if (s_sneakChangesLeft > 0) {
			--s_sneakChangesLeft;
			s_sneakTraceLeft = 40;
		}
	}
	const bool marked = test::HandScriptMarkedThisFrame();
	if ((s_sneakTraceLeft > 0 || marked) && f.playerValid && !f.thirdPerson) {
		if (s_sneakTraceLeft > 0) {
			--s_sneakTraceLeft;
		}
		OBVR_LOG("Camera: the engine's eye %.1f units above the feet, sneaking %d, lifted %.1f (frame %u%s)",
		         static_cast<double>(f.eyeAboveFeet), sneaking ? 1 : 0, static_cast<double>(held.lift), f.frameCount,
		         marked ? ", at the mark" : "");
	}
	return held.lift;
}

float RealHeightLiftNow(const vr::HeadTracker& tracker, const Config& config, bool thirdPerson, bool deathViewHeld) {
	return vr::RealHeightLiftNow(tracker.RealHeightMemo(), config.tracker.realHeight, !thirdPerson, game::PlayerRiding(),
	                             deathViewHeld);
}

float RealHeightBodyScaleNow(const vr::HeadTracker& tracker, const Config& config, bool deathViewHeld) {
	return vr::RealHeightBodyScale(RealHeightLiftNow(tracker, config, false, deathViewHeld),
	                               tracker.RealHeightMemo().characterEyeUnits);
}

void NoteViewEyeAtMark(bool playerValid, float viewEyeAboveFeet) {
	if (playerValid && test::HandScriptMarkedThisFrame()) {
		OBVR_LOG("Camera: at the mark the view's eye stands %.1f units over the feet", static_cast<double>(viewEyeAboveFeet));
	}
}

void StepRealHeight(vr::HeadTracker& tracker, const Config& config, const EyeHeightFrame& f, float sneakLift) {
	vr::RealHeightState& memo = tracker.RealHeightMemo();
	const bool standing = !f.thirdPerson && f.playerValid && !game::PlayerRiding() && !game::IsPlayerSneaking() &&
	                      !game::LoadingThreadActive() && !game::IsMenuMode();
	const UInt32 world = game::CurrentWorld();
	if (vr::RealHeightRetakeDue(memo, world, standing, f.eyeAboveFeet)) {
		vr::RetakeRealHeight(memo, world, f.eyeAboveFeet, config.tracker.unitsPerMetre);
		OBVR_LOG("Real height: the view lifted %.1f units - the eyes %.1f units over the floor, the character's %.1f over "
		         "its feet (world %u); the shown body scaled %.3f",
		         static_cast<double>(memo.liftUnits), static_cast<double>(memo.realEyeUnits),
		         static_cast<double>(f.eyeAboveFeet), world,
		         static_cast<double>(vr::RealHeightBodyScale(memo.liftUnits, f.eyeAboveFeet)));
	}
	if (test::HandScriptMarkedThisFrame() && f.playerValid) {
		const float lift = RealHeightLiftNow(tracker, config, f.thirdPerson, f.deathViewHeld);
		OBVR_LOG("Real height: at the mark - the game's eye %.1f units over the feet, the lift %.1f (%s), the view's eye "
		         "%.1f over the feet before the head's move",
		         static_cast<double>(f.eyeAboveFeet), static_cast<double>(lift),
		         memo.eyeKnown ? (memo.world != 0 ? "taken" : "waiting") : "none",
		         static_cast<double>(f.eyeAboveFeet + lift + sneakLift));
	}
}

}  // namespace obvr::camera
