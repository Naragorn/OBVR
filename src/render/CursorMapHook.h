#pragma once

namespace obvr::render {

// Redirects (or restores) the two renderer-size calls inside the game's
// cursor-node placement, so the visible cursor sprite sits exactly on its own
// hit position while the screen-size copy is raised away from the frame. The
// story and the pure decision live at CursorSurfaceAxis/DecideCursorMap in
// UiScreenSize.h; the bytes and their evidence at kCursorSizeCalls.
//
// raised says whether the copy currently believes something other than the
// game's own numbers. Called from the CreateDevice hook right after that is
// decided, and again with false on the fallback path that takes the raise
// back.
void ApplyCursorMap(bool raised);

}  // namespace obvr::render
