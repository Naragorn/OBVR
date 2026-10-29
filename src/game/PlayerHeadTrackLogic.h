#pragma once

// The player's upper body turned towards NPCs (Nadi, 2026-10-29: "player ober
// körper dreht sich manchmal zu anderen npcs ... das aus"). The engine's head
// tracking runs for the player too and, besides the head, twists the spine
// towards whom it looks at (read in Oblivion.exe 1.2.0.416; game/PlayerHeadTrack.h).
// While OBVR shows the body in first person the headset decides where the
// body faces, so the engine's head tracking is stopped for the player then,
// with the engine's own switch, and given back afterwards.
//
// Pure: which write a frame wants.

namespace obvr::game {

struct HeadTrackState {
	bool frozen = false;  // OBVR set the switch and owes the engine its release
};

enum class HeadTrackAction { None, Freeze, Release };

// wanted: [Body] PlayerHeadTracking - the engine's head tracking kept.
// bodyShown: the body is shown in first person this frame.
// objectThere: the player's head-tracking object is readable.
// Freeze is asked every frame it holds: the engine may build the object anew.
// A release only for a switch OBVR set on an object still there.
inline HeadTrackAction StepPlayerHeadTrack(HeadTrackState& s, bool wanted, bool bodyShown, bool objectThere) {
	if (!objectThere) {
		s.frozen = false;
		return HeadTrackAction::None;
	}
	if (!wanted && bodyShown) {
		s.frozen = true;
		return HeadTrackAction::Freeze;
	}
	if (s.frozen) {
		s.frozen = false;
		return HeadTrackAction::Release;
	}
	return HeadTrackAction::None;
}

}  // namespace obvr::game
