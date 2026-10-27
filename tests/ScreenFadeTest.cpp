// Checks the engine's screen faders turned into a fade of the whole view
// (vr/ScreenFade.h): what goes to the compositor, when, and when it is left
// alone - the teleport's own fade, a menu up, the setting off.

#include <cstdio>

#include "vr/ScreenFade.h"

using namespace obvr;
using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 1e-4f) { return a - b < eps && b - a < eps; }

ScreenFadeInput In(float opacity) {
	ScreenFadeInput in;
	in.enabled = true;
	in.worldFrame = true;
	in.opacity = opacity;
	return in;
}

void TestOpacity() {
	std::printf("The opacity of the faders\n");
	Check(Near(FaderOpacity(nullptr, 0), 0.0f), "no faders: clear");
	const float none[3] = {0.0f, 0.0f, 0.0f};
	Check(Near(FaderOpacity(none, 3), 0.0f), "all clear: clear");
	const float some[3] = {0.2f, 0.9f, 0.4f};
	Check(Near(FaderOpacity(some, 3), 0.9f), "the most opaque one counts");
	const float wild[3] = {-1.0f, 3.0f, 0.5f};
	Check(Near(FaderOpacity(wild, 3), 1.0f), "kept within 0..1");
}

void TestFade() {
	std::printf("The view's fade\n");
	ScreenFadeState s;
	ScreenFadeStep step = StepScreenFade(s, In(0.0f));
	Check(!step.send && !step.cullFaders, "no fader: the compositor left alone, nothing culled");

	step = StepScreenFade(s, In(1.0f));
	Check(step.send && Near(step.alpha, 1.0f) && step.cullFaders,
	      "a fader opaque after a load: the view goes black, the fader kept out of the HUD panel");
	step = StepScreenFade(s, In(0.995f));
	Check(!step.send && step.cullFaders, "a change under the step: not sent again");
	step = StepScreenFade(s, In(0.6f));
	Check(step.send && Near(step.alpha, 0.6f), "fading out: the new opacity sent");
	step = StepScreenFade(s, In(0.0f));
	Check(step.send && Near(step.alpha, 0.0f) && !step.cullFaders, "gone: the view back, once");
	step = StepScreenFade(s, In(0.0f));
	Check(!step.send, "and not again");
}

void TestMenuAndTeleport() {
	std::printf("A menu, the teleport, the setting\n");
	ScreenFadeState s;
	StepScreenFade(s, In(1.0f));
	ScreenFadeInput menu = In(1.0f);
	menu.worldFrame = false;
	ScreenFadeStep step = StepScreenFade(s, menu);
	Check(step.send && Near(step.alpha, 0.0f) && step.cullFaders,
	      "a menu up (the loading screen, a message): the view given back, the fader still out of the panel");
	step = StepScreenFade(s, menu);
	Check(!step.send, "and left alone while the menu stays");
	step = StepScreenFade(s, In(0.8f));
	Check(step.send && Near(step.alpha, 0.8f), "the world again: the fade back on");

	ScreenFadeInput teleport = In(0.5f);
	teleport.teleportFading = true;
	step = StepScreenFade(s, teleport);
	Check(!step.send && step.cullFaders, "the teleport's own fade: the compositor is its");
	step = StepScreenFade(s, In(0.5f));
	Check(step.send && Near(step.alpha, 0.5f), "after it: sent again, whatever the teleport left");

	ScreenFadeInput off = In(0.5f);
	off.enabled = false;
	step = StepScreenFade(s, off);
	Check(step.send && Near(step.alpha, 0.0f) && !step.cullFaders,
	      "switched off: the view given back and the fader drawn as vanilla");
	step = StepScreenFade(s, off);
	Check(!step.send && !step.cullFaders, "and nothing more");
}

}  // namespace

int main() {
	TestOpacity();
	TestFade();
	TestMenuAndTeleport();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
