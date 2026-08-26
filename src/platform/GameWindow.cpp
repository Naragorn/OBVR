#include "platform/GameWindow.h"

#include "platform/Win32Min.h"

namespace obvr::platform {
namespace {

// SWP_NOMOVE is deliberately not among these: a window that was 320x240 is
// wherever a 320x240 window was put, and leaving it there would place a
// screen-sized window mostly off the screen.
constexpr UInt32 kNoZOrder = 0x0004;
constexpr UInt32 kNoActivate = 0x0010;
constexpr UInt32 kShowWindow = 0x0040;

#if defined(OBVR_NO_WINSDK)
using Rect = WindowRect;
inline void* AsWindow(void* window) { return window; }
#else
using Rect = RECT;
inline HWND AsWindow(void* window) { return static_cast<HWND>(window); }
#endif

}  // namespace

bool SizeClientArea(void* window, UInt32 width, UInt32 height, UInt32& wasWidth,
                    UInt32& wasHeight) {
	if (window == nullptr || width == 0 || height == 0) {
		return false;
	}

	Rect client{};
	Rect outer{};
	if (!GetClientRect(AsWindow(window), &client) ||
	    !GetWindowRect(AsWindow(window), &outer)) {
		return false;
	}

	wasWidth = static_cast<UInt32>(client.right - client.left);
	wasHeight = static_cast<UInt32>(client.bottom - client.top);

	// What the border and title bar cost. Measured rather than assumed: a
	// borderless window costs nothing and a framed one costs a few pixels, and
	// getting it wrong puts the mouse a few pixels out everywhere.
	const SInt32 extraWidth =
	    (outer.right - outer.left) - static_cast<SInt32>(wasWidth);
	const SInt32 extraHeight =
	    (outer.bottom - outer.top) - static_cast<SInt32>(wasHeight);

	return SetWindowPos(AsWindow(window), nullptr, 0, 0,
	                    static_cast<int>(width) + extraWidth,
	                    static_cast<int>(height) + extraHeight,
	                    kNoZOrder | kNoActivate | kShowWindow) != 0;
}

}  // namespace obvr::platform
