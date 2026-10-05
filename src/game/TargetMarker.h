#pragma once

#include "core/Types.h"

namespace obvr::game {

// The thing a closed grip would take, marked on itself (the tester,
// 2026-10-05: "Das Ziel selbst markieren, mit Umriss (default an) oder leichtem
// Aufleuchten (default off)"), with the engine's own effect shaders, the way
// the console's PlayMagicShaderVisuals puts one on a reference.
//
// Read in Oblivion.exe 1.2.0.416: PlayMagicShaderVisuals (0x00505FE0) checks
// the reference's cell is loaded and it has 3D (vtable +0x154), allocates
// 0x4C bytes (0x00401F00), builds a MagicShaderHitEffect (0x006A0980,
// thiscall(target, shader, duration) - a duration of 0 or less lasts until
// stopped), initialises it (its vtable +0x68, a bool) and hands it to the
// process manager's temporary effects (0x00678D30 on 0x00B3BD00); a failed
// init deletes it (vtable +0, 1). StopMagicShaderVisuals is 0x00678E70,
// thiscall(manager; ref, shader): it marks the effect finished and it fades
// out. Items lying in the world take it as actors do - vanilla's
// Telekinesis puts its shader on the item it holds (0x006A7560) - and an
// effect on a reference whose 3D goes is stopped by the engine.
//
// The shaders (Oblivion.esm EFSH): the outline is effectFortify 000562C8, an
// edge only (a white rim, no fill, no particles); the glow is
// effectTelekinesis 00181C2E, vanilla's own mark of a held item (a soft
// fill). Not seen in the headset yet.
inline constexpr UInt32 kShaderOutline = 0x000562C8;
inline constexpr UInt32 kShaderGlow = 0x00181C2E;

// When the mark moves: a reference has to be the wanted one for a few frames
// before it is marked, and the mark stays a few frames after it is no longer
// wanted - each start builds an effect and each stop lets one fade, so a
// target flickering between two things should not churn them.
struct MarkState {
	UInt32 marked = 0;     // the reference marked now, 0 none
	UInt32 candidate = 0;  // the one wanted lately
	UInt32 frames = 0;     // frames it has been wanted (or none has)
};
inline constexpr UInt32 kMarkSettleFrames = 4;

struct MarkStep {
	UInt32 stop = 0;   // unmark this one
	UInt32 start = 0;  // mark this one
};

inline MarkStep StepMark(MarkState& s, UInt32 wanted) {
	MarkStep out;
	if (wanted != s.candidate) {
		s.candidate = wanted;
		s.frames = 0;
	}
	if (s.frames < kMarkSettleFrames) {
		++s.frames;
	}
	if (s.candidate != s.marked && s.frames >= kMarkSettleFrames) {
		out.stop = s.marked;
		out.start = s.candidate;
		s.marked = s.candidate;
	}
	return out;
}

// Every frame on the game's thread, outside a render: the reference to mark
// (0 none) and which marks are wanted. Turning a mark off takes it off.
void StepTargetMarker(UInt32 ref, bool outline, bool glow);

}  // namespace obvr::game
