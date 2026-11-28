#pragma once

// The player's head shown for the mirror's render alone (game/MirrorHeadLogic.h
// for what is switched and why; docs/mirror-spec.md 13). Called around the
// mirror's world render in render/MirrorGlassPass.cpp RenderFromMirror, after
// both eyes' captures: nothing here runs while the mirror does not render.

#include <cstdint>

#include "game/MirrorHeadLogic.h"

namespace obvr {
struct NiAVObject;
}

namespace obvr::game {

// What was switched, for the putting back.
struct MirrorHeadSwitch {
	bool asked = false;  // ShowPlayerHeadForMirror ran for this pass
	MirrorHeadPlan plan;
	NiAVObject* head = nullptr;
	MirrorHeadSkins skins;
	double ms = 0.0;  // the switch's own time, both ways, for the cost line
};

// Before the mirror's render: the head's true transform, the FaceGen bits this
// code set taken off, the head's skins' palettes made again - or nothing, with
// the refusal in the plan (logged once each time it changes).
void ShowPlayerHeadForMirror(MirrorHeadSwitch& s);

// After it, in reverse: the stamps turned again, the head collapsed again, the
// bits put back on. Nothing for a refused switch.
void HidePlayerHeadAfterMirror(MirrorHeadSwitch& s);

}  // namespace obvr::game
