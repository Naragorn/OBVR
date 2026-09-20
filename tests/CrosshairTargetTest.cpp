#include <cstdio>

#include "game/CrosshairTarget.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

}  // namespace

int main() {
	std::printf("HUDReticle tile source fallback\n\n");
	using obvr::game::ChooseHudReticleTileSource;
	using obvr::game::HudReticleTileSource;
	using obvr::game::HudReticleWriteContext;
	using obvr::game::PersistentHudRootsWriteWanted;
	using obvr::game::HudReticleOpacityWriteWanted;
	Check(ChooseHudReticleTileSource(false, false) == HudReticleTileSource::None,
	      "no array entry and no persistent root refuses the write");
	Check(ChooseHudReticleTileSource(false, true) == HudReticleTileSource::PersistentRoot,
	      "a persistent root is used when the menu array slot is empty");
	Check(ChooseHudReticleTileSource(true, false) == HudReticleTileSource::MenuArray,
	      "a valid menu-array entry remains the first choice");
	Check(ChooseHudReticleTileSource(true, true) == HudReticleTileSource::MenuArray,
	      "the persistent root never overrides a valid array entry");
	Check(PersistentHudRootsWriteWanted(false, HudReticleWriteContext::MainMenu),
	      "the main-menu pass hides all three persistent HUD roots");
	Check(!PersistentHudRootsWriteWanted(false, HudReticleWriteContext::Gameplay),
	      "the gameplay pass leaves adjacent HUD roots untouched");
	Check(!PersistentHudRootsWriteWanted(true, HudReticleWriteContext::MainMenu),
	      "enabling the reticle never hides persistent HUD roots");
	Check(HudReticleOpacityWriteWanted(false, HudReticleWriteContext::MainMenu),
	      "the main-menu hide also clears the reticle root opacity");
	Check(!HudReticleOpacityWriteWanted(false, HudReticleWriteContext::Gameplay),
	      "gameplay hiding leaves the reticle opacity untouched");
	Check(!HudReticleOpacityWriteWanted(true, HudReticleWriteContext::MainMenu),
	      "enabling the reticle never clears opacity");

	if (g_failures == 0) {
		std::printf("\nAll checks passed.\n");
		return 0;
	}
	std::printf("\n%d check(s) failed.\n", g_failures);
	return 1;
}
