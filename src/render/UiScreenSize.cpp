#include "render/UiScreenSize.h"

#include "core/Log.h"
#include "game/GameAddresses.h"

namespace obvr::render {

UiSize UiSizeForFrame(UInt32 frameWidth, UInt32 frameHeight, float menuAspect) {
	UiSize size{frameWidth, frameHeight};
	if (menuAspect <= 0.1f || frameWidth == 0 || frameHeight == 0) {
		return size;
	}

	// The full width at the wanted aspect; if the frame is narrower than the
	// aspect wants, the full height instead. Either way one axis stays the
	// frame's own and the other shrinks - the window into the frame is never
	// larger than the frame.
	const UInt32 wantedHeight =
		static_cast<UInt32>(static_cast<float>(frameWidth) / menuAspect + 0.5f);
	if (wantedHeight != 0 && wantedHeight <= frameHeight) {
		size.height = wantedHeight;
		return size;
	}

	const UInt32 wantedWidth =
		static_cast<UInt32>(static_cast<float>(frameHeight) * menuAspect + 0.5f);
	if (wantedWidth != 0 && wantedWidth <= frameWidth) {
		size.width = wantedWidth;
	}
	return size;
}

UiSizeLockAction DecideUiSizeLock(bool enabled, UInt32 askedWidth, UInt32 askedHeight,
                                  UInt32 newWidth, UInt32 newHeight, UInt32 readWidth,
                                  UInt32 readHeight) {
	if (!enabled) {
		return UiSizeLockAction::NotWanted;
	}
	if (askedWidth == newWidth && askedHeight == newHeight) {
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

InterfaceViewportAction DecideInterfaceViewport(UInt32 viewportX, UInt32 viewportY,
                                                UInt32 viewportWidth, UInt32 viewportHeight,
                                                UInt32 frameWidth, UInt32 frameHeight,
                                                UInt32 believedWidth, UInt32 believedHeight) {
	if (believedWidth == frameWidth && believedHeight == frameHeight) {
		return InterfaceViewportAction::LeaveAlone;
	}
	if (viewportX != 0 || viewportY != 0 || viewportWidth != frameWidth ||
	    viewportHeight != frameHeight) {
		return InterfaceViewportAction::LeaveAlone;
	}
	return InterfaceViewportAction::Shrink;
}

bool UiScreenSizeFollowsFrame(bool enabled, UInt32 askedWidth, UInt32 askedHeight,
                              UInt32 newWidth, UInt32 newHeight) {
	const UInt32 readWidth = *reinterpret_cast<const UInt32*>(addr::kUiScreenWidthCopy);
	const UInt32 readHeight = *reinterpret_cast<const UInt32*>(addr::kUiScreenHeightCopy);

	switch (DecideUiSizeLock(enabled, askedWidth, askedHeight, newWidth, newHeight,
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

	return WriteUiScreenSize(askedWidth, askedHeight, newWidth, newHeight);
}

}  // namespace obvr::render
