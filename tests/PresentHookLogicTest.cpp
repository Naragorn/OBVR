// When the frame-end hook goes in (render/PresentHookLogic.h): for the
// headset's submit at the frame's end, and for a named hand script whatever
// the headset does.

#include <cstdio>

#include "render/PresentHookLogic.h"

using obvr::render::PresentHookWanted;

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
	Check(PresentHookWanted(true, true, false), "the headset's submit at the frame end wants the hook");
	Check(!PresentHookWanted(true, false, false), "the headset's submit at the frame start does not");
	Check(!PresentHookWanted(false, true, false), "no headset, no script: no hook");
	Check(!PresentHookWanted(false, false, false), "nothing at all: no hook");
	Check(PresentHookWanted(false, false, true), "a named hand script wants the hook without a headset");
	Check(PresentHookWanted(false, true, true), "... and whatever the submit setting says");
	Check(PresentHookWanted(true, true, true), "the headset's submit and a script: the hook once");
	std::printf("%s (%d failure%s)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures, g_failures == 1 ? "" : "s");
	return g_failures;
}
