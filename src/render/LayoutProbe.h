#pragma once

#include "core/Types.h"

// The layout probe: measures which rectangle of a surface actually carries a
// picture, by reading the surface back and finding the bounding box of its
// covered pixels.
//
// Why it exists: OBVR now creates the frame at the headset's size, and the
// user's first run on it showed the 2D side torn three ways - intro films and
// the main menu sitting small in a corner, in-game menus square instead of
// wide, and clicks landing nowhere. All three symptoms hang on one unknown:
// which screen size each part of Oblivion's 2D lays out against, the size the
// game believes from its INI or the size the frame really is. The viewport
// cannot answer that - it is fully open either way, which is exactly the
// mistake an earlier reading of it made. The bounding box of the pixels that
// were actually drawn can.
//
// The readback stalls the GPU for the frame it runs on, which is why the
// probe is a hot-reloadable debug switch and measures at most once every
// couple of seconds, not every frame.
namespace obvr::render {

// The bounding box of the covered pixels, inclusive, plus how many pixels
// were covered at all. covered == 0 means the box fields are meaningless.
struct CoveredRect {
	UInt32 minX = 0;
	UInt32 minY = 0;
	UInt32 maxX = 0;
	UInt32 maxY = 0;
	UInt32 covered = 0;
};

// Whether one A8R8G8B8/X8R8G8B8 pixel counts as drawn on.
//
// byAlpha true is for OBVR's own capture texture, which is cleared to
// transparent black before the pass draws - any alpha at all is the pass's
// doing. byAlpha false is for the game's back buffer, whose alpha channel
// means nothing (X8R8G8B8): there "drawn on" is any colour above the noise
// floor, the same 0x00F0F0F0 floor the HUD content dump uses. A black pixel
// the game deliberately drew is invisible to that criterion - a film's own
// letterbox bars will not widen the box - so the box is the picture, not the
// clear.
bool PixelCovered(UInt32 pixel, bool byAlpha);

// Folds one row of pixels into the box. Pure, so the box arithmetic is
// testable without a device; the readback below is only the plumbing that
// feeds it.
void AccumulateCoveredRow(const UInt32* row, UInt32 width, UInt32 y, bool byAlpha,
                          CoveredRect& box);

// Reads the surface back and reports the bounding box of its covered pixels.
// format is the surface's own D3DFORMAT, because GetRenderTargetData insists
// the staging copy match it. False when any step of the plumbing refuses;
// the caller owns deciding when the stall is acceptable.
bool MeasureCoveredRect(void* gameDevice, void* surface, UInt32 width, UInt32 height,
                        UInt32 format, bool byAlpha, CoveredRect& out);

// The same measurement on the game's back buffer, which is where the cinema
// frames - films, loading screens, the main menu - carry their picture.
// Hands back the buffer's size alongside the box so the log line can show
// both rectangles.
bool MeasureBackBufferCoveredRect(void* gameDevice, UInt32& widthOut, UInt32& heightOut,
                                  CoveredRect& out);

// Writes a quarter-scale 24-bit BMP of a render-target surface next to the
// log, overwriting the file each time. For the hunts where a bounding box is
// not enough and the actual picture is the question - where the cursor
// stands relative to the menu, in the very texture the overlay shows and the
// very back buffer the cinema crops. Same GPU stall as the measurements, so
// the caller throttles it the same way.
bool DumpSurfaceBmp(void* gameDevice, void* surface, UInt32 width, UInt32 height,
                    UInt32 format, const char* path);

// The back-buffer flavour, mirroring MeasureBackBufferCoveredRect.
bool DumpBackBufferBmp(void* gameDevice, const char* path);

}  // namespace obvr::render
