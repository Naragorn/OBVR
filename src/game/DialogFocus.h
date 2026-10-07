#pragma once

#include "game/NiMath.h"

namespace obvr::game {

inline bool DialogPositionValid(const NiPoint3& p) {
	constexpr float limit = 1.0e7f;
	return p.x > -limit && p.x < limit && p.y > -limit && p.y < limit &&
	       p.z > -limit && p.z < limit;
}

// Values only: no actor pointer survives the engine call. The first approach
// call freezes the last gameplay eyes, before camera/arm transitions run.
struct DialogFocus {
	bool active = false;
	bool called = false;
	bool sawMenu = false;
	int graceFrames = 0;
	bool eyesValid = false;
	NiPoint3 eyes{};
	bool speakerValid = false;
	NiPoint3 speaker{};

	void Observe(bool hasActor, bool haveEyes, const NiPoint3& lastEyes,
	             bool haveSpeaker, const NiPoint3& actorPosition) {
		if (!hasActor) {
			*this = DialogFocus{};
			return;
		}
		if (!active) {
			eyesValid = haveEyes && DialogPositionValid(lastEyes);
			eyes = lastEyes;
		}
		active = true;
		called = true;
		graceFrames = 3;
		speakerValid = haveSpeaker && DialogPositionValid(actorPosition);
		speaker = actorPosition;
	}

	// Once per rendered frame. Also releases cancelled approaches and menu
	// exits when the engine omits the final null-actor camera call.
	void Step(bool menuIsUp) {
		if (!active) return;
		if (menuIsUp) {
			sawMenu = true;
		} else if (sawMenu || (!called && graceFrames == 0)) {
			*this = DialogFocus{};
			return;
		} else if (!called) {
			--graceFrames;
		}
		called = false;
	}

	// The frozen eyes only once the conversation's menu is up: while the NPC
	// is still coming over (the approach calls, no menu yet) the live eyes
	// are the player's, and freezing them there sent a guard who ran up to
	// where the player had stood when they set out (the tester, 2026-10-07:
	// "guards ... sprechen meine hand im raum an und nicht mich im headset").
	bool UseEyes(bool wanted, bool thirdPerson, bool liveValid) const {
		return wanted && (active && sawMenu ? eyesValid : (!thirdPerson && liveValid));
	}

	// Whether the live eyes still refresh the ones the conversation will
	// hold: until its menu is up.
	bool TakesLiveEyes() const { return active && !sawMenu; }
};

}  // namespace obvr::game
