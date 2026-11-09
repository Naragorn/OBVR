#pragma once

// When the frame-end (Present) hook goes in. It carries the headset's submit
// at the frame's end, and everything OBVR steps once a frame (OnPresent):
// the hand script among it. Without the hook there is no OnPresent, so a
// harness run with [Render] Enabled=0 - the game drawn flat into its window,
// OBVR a background app - never loaded its script (2026-11-09, OBNG's flat
// culler scenario). The hook is wanted for the headset's submit, and for a
// named hand script whatever the headset does.

namespace obvr::render {

inline bool PresentHookWanted(bool renderToHeadset, bool submitAtFrameEnd, bool handScriptNamed) {
	return (renderToHeadset && submitAtFrameEnd) || handScriptNamed;
}

}  // namespace obvr::render
