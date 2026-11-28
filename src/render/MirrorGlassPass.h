#pragma once

// The mirror's reflection on the tablet's glass, stage 2 (docs/mirror-spec.md
// 8 and 12): one world render a frame from the camera behind the glass - the
// viewer's eye between the eyes reflected in the glass's plane, the frustum
// through the glass's corners (vr/MirrorGlassLogic.h) - copied into a texture
// of OBVR's own and handed to the glass (render/TabletQuad.h SetMirrorGlass).
//
// Where in the frame, and why there. The engine's world render on the
// owner's route (antialiasing on, image space on: OBVR.log "the back buffer
// has 2 samples", the target set at 0x0040CA48) draws into the back buffer
// whether or not it is handed a texture (RenderScene 0x0040C830, the
// multisampled branch at 0x0040CA36), so a mirror render overwrites whatever
// is in the back buffer. It runs only after both eye pictures of the frame
// are copied out (HeadsetRenderer::BothEyesCaptured), from the camera hook's
// afterWorldRender, before the per-draw changes (the hidden head) go back -
// and the glass shows it in the next frame's captures: a frame late, as the
// tablet's page already is.
//
// Nothing of it runs while the mirror's page is not up: one read of the page
// state a frame, and the glass handed back dark once when it goes.

#include "core/Types.h"

namespace obvr::render {

class HeadsetRenderer;

// The frame's pass, from afterWorldRender. stereoFrame: this frame delivers
// the two eye pictures it captured (the dual delivery, not a flat or held
// frame).
void RunMirrorGlassPass(HeadsetRenderer& renderer, bool stereoFrame);

// Whether this frame's single-pass world render records its bone rows for the
// mirror's render to replace from (render/SceneRenderHook.cpp): the page was
// up at the last frame's pass, the reflection on and not switched off.
bool MirrorWantsBones();

}  // namespace obvr::render
