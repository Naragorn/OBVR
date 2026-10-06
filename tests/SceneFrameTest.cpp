// Checks when the frame is closed between the dual pass's renders
// (game/SceneFrame.h).

#include <cstdio>

#include "game/SceneFrame.h"

using namespace obvr;
using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

}  // namespace

int main() {
	std::printf("The frame closed between the passes\n");
	Check(SceneFrameCloseWanted(true, 0, 1, 0), "the texture path, the frame open after the copy: closed");
	Check(SceneFrameCloseWanted(true, 1, 1, 0), "one sample counts as the texture path too");
	Check(!SceneFrameCloseWanted(true, 8, 1, 0), "antialiased: the world is in the back buffer already");
	Check(!SceneFrameCloseWanted(false, 0, 1, 0), "image-space effects off: the world went straight to the back buffer");
	Check(!SceneFrameCloseWanted(true, 0, 0, 0), "no frame open: nothing to close");
	Check(!SceneFrameCloseWanted(true, 0, 2, 0), "a frame already ended: left to the engine");
	Check(!SceneFrameCloseWanted(true, 0, 1, 1), "an offscreen frame under way: left alone");
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
