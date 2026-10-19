// Checks game/ActorLink.h: an actor's values are read only once it is an
// object with a base form (the intro-video crash of a tester, 2026-10-18).

#include <cstdio>

#include "game/ActorLink.h"

using namespace obvr::game;

int main() {
	int failures = 0;
	auto check = [&](bool condition, const char* what) {
		std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
		if (!condition) {
			++failures;
		}
	};
	check(ActorValuesReadable(true, true), "an actor with a base form is readable");
	check(!ActorValuesReadable(true, false), "an actor without a base form (not yet linked) is not");
	check(!ActorValuesReadable(false, true), "no actor is not, whatever the base form word says");
	check(!ActorValuesReadable(false, false), "nothing is not");
	if (failures != 0) {
		std::printf("%d check(s) failed\n", failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
