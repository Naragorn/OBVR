#include "render/UiScreenSize.h"

#include "core/Log.h"
#include "game/GameAddresses.h"

namespace obvr::render {

UiSizeLockAction DecideUiSizeLock(bool enabled, UInt32 believedWidth, UInt32 believedHeight,
                                  UInt32 createdWidth, UInt32 createdHeight,
                                  UInt32 readWidth, UInt32 readHeight) {
	if (!enabled) {
		return UiSizeLockAction::NotWanted;
	}
	if (believedWidth == createdWidth && believedHeight == createdHeight) {
		return UiSizeLockAction::NothingToDo;
	}
	if (readWidth != createdWidth || readHeight != createdHeight) {
		return UiSizeLockAction::WrongValues;
	}
	return UiSizeLockAction::Lock;
}

void TryLockUiScreenSize(bool enabled, UInt32 believedWidth, UInt32 believedHeight,
                         UInt32 createdWidth, UInt32 createdHeight) {
	// Retired after the first attempt against a real renderer; retried while
	// there is none, because a renderer that does not exist yet is not an
	// answer.
	static bool s_done = false;
	if (s_done) {
		return;
	}

	auto* renderer = *reinterpret_cast<UInt8**>(addr::kRendererPointer);
	if (renderer == nullptr) {
		return;
	}
	s_done = true;

	// Read first, and said out loud either way: these two lines are the
	// evidence the next decision stands on, whatever this run's outcome.
	auto* width = reinterpret_cast<UInt32*>(renderer + addr::kRendererWidthOffset);
	auto* height = reinterpret_cast<UInt32*>(renderer + addr::kRendererHeightOffset);
	OBVR_LOG("UiSize: the renderer holds %ux%u as its screen size (frame %ux%u, the game "
	         "believes %ux%u)",
	         *width, *height, createdWidth, createdHeight, believedWidth, believedHeight);

	switch (DecideUiSizeLock(enabled, believedWidth, believedHeight, createdWidth,
	                         createdHeight, *width, *height)) {
		case UiSizeLockAction::NotWanted:
			OBVR_LOG("UiSize: MenuLayoutAtGameSize is off, so the renderer keeps its own");
			return;
		case UiSizeLockAction::NothingToDo:
			return;
		case UiSizeLockAction::WrongValues:
			OBVR_LOG("UiSize: that pair is not the frame's size, so the offset does not "
			         "mean what it is believed to mean and nothing was written");
			return;
		case UiSizeLockAction::Lock:
			break;
	}

	// Said before it happens, learned the hard way: a wordless write was a
	// crash nobody could place.
	OBVR_LOG("UiSize: writing %ux%u over it, so menus built from here lay out against "
	         "the size the mouse maps against",
	         believedWidth, believedHeight);
	*width = believedWidth;
	*height = believedHeight;
	OBVR_LOG("UiSize: written");
}

}  // namespace obvr::render
