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

// What the laser was on as activate was last pressed, and the frame: a
// container menu opening soon after is that thing's, and its panel goes over
// it (vr/DialogPanel.h, ContainerAnchor). Remembered with its fingerprint
// (game/RefFingerprint.h): an item activated is taken and deleted, and its
// memory reused - read through the old pointer it crashed the game on
// 2026-10-10. CameraHook's: the reach's open writes it as well.
struct ActivatedThing {
	UInt32 ref = 0;
	game::RefFingerprint print;
	UInt32 frame = 0;
};

struct TakeByHandState {
	bool withheld = false;        // activate kept from the game this hold (was g_activateWasWithheld)
	UInt32 withheldLinesLeft = 12;
	bool activateWas = false;     // the block's function static, as it was
};

// What the step writes of the rest of the frame.
struct TakeByHandLinks {
	ActivatedThing& activated;            // what the laser was on as activate was pressed (g_activated)
	UInt32 frameCount;                    // the frame's number (g_state.frameCount)
};

void StepTakeByHand(TakeByHandState& state, const HandStep& step, const HandShared& shared,
                    const TakeByHandLinks& links);

}  // namespace obvr::camera::hands
