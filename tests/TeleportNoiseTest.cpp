// Checks which reads of the movement flags hear a teleport
// (game/TeleportNoise.h): only the detection's two, never the ones that move
// the player.

#include <cstdio>

#include "game/TeleportNoise.h"

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
	Check(TeleportNoiseReadsFlags(0x005F66DD), "the detection's read for moving: heard");
	Check(TeleportNoiseReadsFlags(0x005F66FD), "its read for running: answered too (walk, not run)");
	Check(!TeleportNoiseReadsFlags(0x005F66DB) && !TeleportNoiseReadsFlags(0x005F66FB),
	      "the call instructions themselves are not return addresses");
	Check(!TeleportNoiseReadsFlags(0x006725AA) && !TeleportNoiseReadsFlags(0),
	      "any other read (the player's own movement): the flags as they are");
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
