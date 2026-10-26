// Checks the reading of a torch's shapes for their render state
// (game/FlameStateLogic.h): which compiled classes are drawn shapes, which
// nodes are a torch's model, and the reading's verdict.

#include <cstdio>

#include "game/FlameStateLogic.h"

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

void TestShapes() {
	std::printf("Drawn shapes by their class\n");
	Check(IsDrawnShapeClass("NiTriShape@@"), "a NiTriShape");
	Check(IsDrawnShapeClass("NiTriStrips@@"), "a NiTriStrips (the FlameCap, the torch)");
	Check(IsDrawnShapeClass("NiParticleSystem@@"), "a NiParticleSystem (the Sparks)");
	Check(IsDrawnShapeClass("NiMeshParticleSystem@@"), "a NiMeshParticleSystem");
	Check(!IsDrawnShapeClass("NiNode@@") && !IsDrawnShapeClass("BSFadeNode@@"), "nodes are not");
	Check(!IsDrawnShapeClass("NiPointLight@@"), "a light is not");
	Check(!IsDrawnShapeClass("?") && !IsDrawnShapeClass(nullptr), "an unread class is not");
}

void TestTorchNodes() {
	std::printf("A torch's model by its node's name\n");
	Check(IsTorchModelName("BASE Lights\\Torch02.NIF"), "the world torch's root, by its file");
	Check(IsTorchModelName("Torch02"), "any case of torch");
	Check(IsTorchModelName("lights\\middletorch01.nif"), "lower case");
	Check(!IsTorchModelName("Weapon") && !IsTorchModelName("") && !IsTorchModelName(nullptr), "others not");
}

void TestVerdict() {
	std::printf("The reading's verdict\n");
	FlameShapeStates s;
	Check(!ShapesLackState(s), "no shapes read: nothing lacks");
	s.shapes = 5;
	Check(!ShapesLackState(s), "five shapes, each with its state: none lacks");
	s.withoutState = 1;
	Check(ShapesLackState(s), "one shape without: it lacks - the renderer would draw it without");
}

}  // namespace

int main() {
	TestShapes();
	TestTorchNodes();
	TestVerdict();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
