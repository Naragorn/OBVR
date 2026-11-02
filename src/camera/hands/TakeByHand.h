#pragma once

// Taking loose items only by hand ([Hands] TakeOnlyByHand, vr/Stow.h): the
// activate button is kept from the game while the laser is on a loose item,
// read on the press and kept for as long as it is held; what the laser was on
// as activate was pressed is remembered with its fingerprint and frame for
// the container panel (a container menu opening soon after is that thing's).
//
// Moved out of camera/CameraHook.cpp's UpdateHandMode on 2026-11-02
// (docs/god-functions-plan.md, phase 2 step 2.18g): the code as it was, its
// two file globals and one static now TakeByHandState's members; the
// activated thing stays CameraHook's (the reach's open writes it too, the
// panel reads it) and comes in by reference. Harness: take-by-hand.txt,
// activate-takes.txt, container-panel.txt.

#include "camera/hands/HandStep.h"
#include "game/RefFingerprint.h"

namespace obvr::camera::hands {

struct TakeByHandState {
	bool withheld = false;        // activate kept from the game this hold (was g_activateWasWithheld)
	UInt32 withheldLinesLeft = 12;
	bool activateWas = false;     // the block's function static, as it was
};

// What the step writes of the rest of the frame.
struct TakeByHandLinks {
	UInt32& activatedRef;                 // what the laser was on as activate was pressed (g_activatedRef)
	game::RefFingerprint& activatedPrint;
	UInt32& activatedFrame;
	UInt32 frameCount;                    // the frame's number (g_state.frameCount)
};

void StepTakeByHand(TakeByHandState& state, const HandStep& step, const HandShared& shared,
                    const TakeByHandLinks& links);

}  // namespace obvr::camera::hands
