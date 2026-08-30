#pragma once

namespace obvr::render {

// Detours the point normalization of the tile-under-cursor pick
// (kPickNormalizePoint) so the cursor pixels are divided by the believed
// size instead of the renderer's real one.
//
// The measured why: hover - and through hover, clicks - is answered by a
// scene-graph pick on the ui scene. The cursor pixels it is handed live in
// the screen-size copy's space, but the pick normalizes them over the
// renderer's real width and height before the camera's port and frustum
// test. Equal in vanilla; with the copy raised to a 16:9 slice the division
// runs over the wrong height, and the cursor ladder measured the result:
// hit zones with the drawn spacing, the centre half the height difference
// low - the highlight three entries above the pointer, the click with it.
// Dividing by the believed size is the identity the drawing already uses.
// When belief and frame agree the detour forwards untouched.
bool InstallCursorPickHook();

}  // namespace obvr::render
