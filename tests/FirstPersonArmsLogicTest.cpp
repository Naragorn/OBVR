// Checks game/FirstPersonArmsLogic.h: the first-person skeleton's root is
// left alone under the horse's ActorParent bone (the view sliding sideways
// on a horse, 2026-10-19).

#include <cstdio>

#include "game/FirstPersonArmsLogic.h"

using namespace obvr::game;

int main() {
	int failures = 0;
	auto check = [&](bool condition, const char* what) {
		std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
		if (!condition) {
			++failures;
		}
	};
	check(ArmsRootUnderHorse("ActorParent"), "under the horse's ActorParent: left alone");
	check(!ArmsRootUnderHorse("ActorParentX"), "a longer name is another node");
	check(!ArmsRootUnderHorse("ActorParen"), "a shorter name is another node");
	check(!ArmsRootUnderHorse("Camera01"), "under the camera (on foot): placed");
	check(!ArmsRootUnderHorse(""), "an unnamed parent: placed");
	check(!ArmsRootUnderHorse(nullptr), "no parent name: placed");
	if (failures != 0) {
		std::printf("%d check(s) failed\n", failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
