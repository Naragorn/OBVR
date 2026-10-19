// Checks the pure part of Debug.FlameProbe (game/FlameProbeLogic.h): which
// class names are time controllers', and which "next" word a chain follows.

#include <cstdio>

#include "game/FlameProbeLogic.h"

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

void TestControllerNames() {
	Check(IsControllerClassName("NiFlipController@@"), "a flip controller is one");
	Check(IsControllerClassName("NiPSysEmitterCtlr@@"), "a particle emitter controller (Ctlr) is one");
	Check(IsControllerClassName("NiPSysUpdateCtlr@@"), "a particle update controller is one");
	Check(IsControllerClassName("NiControllerManager@@"), "the controller manager is one");
	Check(IsControllerClassName("NiTransformController@@"), "a transform controller is one");
	Check(!IsControllerClassName("NiTexturingProperty@@"), "a property is not");
	Check(!IsControllerClassName("NiTriShape@@"), "a geometry is not");
	Check(!IsControllerClassName("NiNode@@"), "a node is not");
	Check(!IsControllerClassName("?"), "an unreadable class is not");
	Check(!IsControllerClassName(""), "an empty name is not");
	Check(!IsControllerClassName(nullptr), "no name is not");
}

void TestNextOffset() {
	Check(NextControllerOffset(false, false, true) == 0x34, "+0x34 when it holds a controller");
	Check(NextControllerOffset(true, true, true) == 0x34, "+0x34 first when every word does");
	Check(NextControllerOffset(true, false, false) == 0x2C, "+0x2C when only it does");
	Check(NextControllerOffset(true, true, false) == 0x2C, "+0x2C before +0x30");
	Check(NextControllerOffset(false, true, false) == 0x30, "+0x30 when only it does");
	Check(NextControllerOffset(false, false, false) == 0, "0 when none does - the chain ends");
}

}  // namespace

int main() {
	TestControllerNames();
	TestNextOffset();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
