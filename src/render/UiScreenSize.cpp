#include "render/UiScreenSize.h"

#include "core/Log.h"
#include "game/GameAddresses.h"

namespace obvr::render {

UiSizeLockAction DecideUiSizeLock(bool enabled, UInt32 askedWidth, UInt32 askedHeight,
                                  UInt32 createdWidth, UInt32 createdHeight,
                                  UInt32 readWidth, UInt32 readHeight) {
	if (!enabled) {
		return UiSizeLockAction::NotWanted;
	}
	if (askedWidth == createdWidth && askedHeight == createdHeight) {
		return UiSizeLockAction::NothingToDo;
	}
	if (readWidth != askedWidth || readHeight != askedHeight) {
		return UiSizeLockAction::WrongValues;
	}
	return UiSizeLockAction::Lock;
}

bool WriteUiScreenSize(UInt32 expectedWidth, UInt32 expectedHeight, UInt32 newWidth,
                       UInt32 newHeight) {
	auto* width = reinterpret_cast<UInt32*>(addr::kUiScreenWidthCopy);
	auto* height = reinterpret_cast<UInt32*>(addr::kUiScreenHeightCopy);

	if (*width != expectedWidth || *height != expectedHeight) {
		OBVR_LOG("UiSize: the copy reads %ux%u where %ux%u was expected, so nothing "
		         "was written",
		         *width, *height, expectedWidth, expectedHeight);
		return false;
	}

	// Said before it happens, learned the hard way: a wordless write was a
	// crash nobody could place. Plain stores into .data, which the engine
	// itself stores into at window creation.
	OBVR_LOG("UiSize: writing %ux%u over the screen-size copy", newWidth, newHeight);
	*width = newWidth;
	*height = newHeight;
	return true;
}

bool UiScreenSizeFollowsFrame(bool enabled, UInt32 askedWidth, UInt32 askedHeight,
                              UInt32 createdWidth, UInt32 createdHeight) {
	const UInt32 readWidth = *reinterpret_cast<const UInt32*>(addr::kUiScreenWidthCopy);
	const UInt32 readHeight = *reinterpret_cast<const UInt32*>(addr::kUiScreenHeightCopy);

	switch (DecideUiSizeLock(enabled, askedWidth, askedHeight, createdWidth, createdHeight,
	                         readWidth, readHeight)) {
		case UiSizeLockAction::NotWanted:
			OBVR_LOG("UiSize: UiFollowsFrameSize is off, so the 2D keeps the game's own "
			         "screen size");
			return false;
		case UiSizeLockAction::NothingToDo:
			return false;
		case UiSizeLockAction::WrongValues:
			OBVR_LOG("UiSize: the screen-size copy reads %ux%u, not the asked-for %ux%u - "
			         "it is not the copy this was built against, so nothing was written",
			         readWidth, readHeight, askedWidth, askedHeight);
			return false;
		case UiSizeLockAction::Lock:
			break;
	}

	return WriteUiScreenSize(askedWidth, askedHeight, createdWidth, createdHeight);
}

}  // namespace obvr::render
