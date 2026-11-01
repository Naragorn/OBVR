#pragma once

// The view after a load and after a knockdown (camera/FrameLogic.h,
// StepFirstPersonAfterLoad, StepFirstPersonAfterKnockdown): a save made in
// third person loads in third person, where the hands are not drawn - once
// after a load, the first world frame that finds third person switches
// through the game's own ToggleCamera, and not again until the next load
// (the every-frame force went on 2026-10-20 for fighting every third person
// chosen); and a knockdown begun in first person ends in first person,
// where a mod's fall left the view in third person (Oblivion Combat
// Extender, 2026-10-29).
//
// Moved out of camera/CameraHook.cpp's UpdateHandMode on 2026-11-01
// (docs/god-functions-plan.md, phase 2 step 2.18): the code as it was, its
// four file globals and one static now ViewAfterLoadState's members.
// Harness: load-third-person.txt, obce-knockout.txt, real-death.txt.

#include "camera/FrameLogic.h"
#include "camera/hands/HandStep.h"

namespace obvr::camera::hands {

struct ViewAfterLoadState {
	FirstPersonAfterLoadState afterLoad;
	UInt32 afterLoadLines = 20;
	FirstPersonAfterKnockdownState afterKnockdown;
	UInt32 afterKnockdownLines = 24;
	bool loadSaid = false;  // the view read said once per load
};

// What the step reads of the rest of the frame.
struct ViewAfterLoadLinks {
	UInt32 frameCount;        // the frame's number (g_state.frameCount), for the log
	bool (*thirdPerson)();    // the engine's view as it stands now (read again after the toggle)
};

void StepViewAfterLoad(ViewAfterLoadState& state, const HandStep& step, const ViewAfterLoadLinks& links);

}  // namespace obvr::camera::hands
