// Checks which torch lying in the world OBVR gives its light back to
// (game/TorchGroundLogic.h): a carried, off-by-default light whose fire the
// reference itself says is on - OBVR's mark, saved by the engine - with its
// 3D and flames there and no light under it.

#include <cstdio>

#include "game/TorchGroundLogic.h"

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

TorchOnGround Torch02Lit() {
	TorchOnGround t;
	t.lightFlags = 0x2B;  // Torch02: dynamic, carried, flicker, off by default
	t.formFlags = 0x08;   // the fire on
	t.haveNode = true;
	t.flames = 1;
	t.lightsUnder = 0;
	return t;
}

void TestNeeds() {
	std::printf("Which torch on the ground gets its light back\n");
	Check(TorchOnGroundNeeds(Torch02Lit()) == TorchGroundNeed::Light,
	      "Torch02 with the fire on, its flames there, no light: the light");
	TorchOnGround lit = Torch02Lit();
	lit.lightsUnder = 1;
	Check(TorchOnGroundNeeds(lit) == TorchGroundNeed::Nothing, "a light already under it: nothing");
	TorchOnGround dark = Torch02Lit();
	dark.formFlags = 0x88;
	Check(TorchOnGroundNeeds(dark) == TorchGroundNeed::NotOurs,
	      "the fire out (0x80): vanilla's dark log, never lit by OBVR - not ours");
	TorchOnGround wall = Torch02Lit();
	wall.lightFlags = 0x09;  // a fixed light, not carried
	Check(TorchOnGroundNeeds(wall) == TorchGroundNeed::NotOurs, "a light not carried: the engine's own");
	TorchOnGround onByDefault = Torch02Lit();
	onByDefault.lightFlags = 0x0B;  // carried, but on by default
	Check(TorchOnGroundNeeds(onByDefault) == TorchGroundNeed::NotOurs,
	      "carried but on by default: the engine lights it itself");
	TorchOnGround noNode = Torch02Lit();
	noNode.haveNode = false;
	Check(TorchOnGroundNeeds(noNode) == TorchGroundNeed::Wait, "no 3D yet: wait");
	TorchOnGround noFlames = Torch02Lit();
	noFlames.flames = 0;
	Check(TorchOnGroundNeeds(noFlames) == TorchGroundNeed::Wait,
	      "no flames yet: wait for the engine's set-up, OBVR puts none on a 3D in the scene");
	TorchOnGround notLight = Torch02Lit();
	notLight.isLight = false;
	Check(TorchOnGroundNeeds(notLight) == TorchGroundNeed::NotOurs, "not a light at all: not ours");
}

}  // namespace

int main() {
	TestNeeds();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
