// Checks the paperdoll's fit (render/MenuModelFit.h): where the figure in the
// interface manager's own scene has to stand for it to fill the rows the 2D
// layer shows, from the engine's own place for it, the figure's top and
// bottom over its root and the camera's vertical half-angle.

#include <cmath>
#include <cstdio>

#include "render/MenuModelFit.h"

using namespace obvr;
using namespace obvr::render;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.05f) { return std::fabs(a - b) < eps; }

MenuModelFitInput Measured() {
	// finery-paperdoll.txt 2026-10-23 12:22 and 12:34: the holder (and the
	// model's root on it) at 33.3 right, 168.0 ahead and 98.8 below the UI
	// camera, the projection's y scale 4.229 (a half-angle of 13.3 degrees),
	// the woman's figure from 14 units below its root (the feet) to 130
	// above (the drawn sword's tip), read off the picture of 12:34.
	MenuModelFitInput in;
	in.holderFound = true;
	in.modelAttached = true;
	in.figureBottom = -14.0f;
	in.figureTop = 130.0f;
	in.tanHalfY = 1.0f / 4.229f;
	in.engine = NiPoint3{33.3f, 168.0f, -98.8f};
	return in;
}

void TestFit() {
	std::printf("The paperdoll fitted to the rows the layer shows\n");
	const MenuModelFitVerdict v = FitMenuModel(Measured());
	Check(v.fit, "the measured scene: fitted");
	// The figure's middle on the camera's axis: the root 58 below it.
	Check(Near(v.pos.z, -58.0f), "the root as far below the axis as the figure's middle is above the root");
	// The distance from the half-height (72), the margin and the half-angle.
	Check(Near(v.pos.y, 72.0f * kMenuModelMargin * 4.229f, 0.5f), "the distance from the half-height, the margin and the half-angle");
	Check(v.pos.y > 168.0f, "further away than the engine's 168, which cut the figure at the hips");
	Check(Near(v.pos.x / v.pos.y, 33.3f / 168.0f, 0.001f), "the engine's own angle to the side kept");
	// The top and the bottom inside the view, the margin left both ways.
	const float top = (v.pos.z + 130.0f) / v.pos.y;
	const float bottom = (v.pos.z - 14.0f) / v.pos.y;
	Check(top < 1.0f / 4.229f && bottom > -1.0f / 4.229f, "top and bottom inside the half-angle");
	Check(Near(top * 4.229f, 1.0f / kMenuModelMargin, 0.01f) && Near(bottom * 4.229f, -1.0f / kMenuModelMargin, 0.01f),
	      "the figure spans the margin's share of the rows, top and bottom alike");

	// The engine's place against the same view: the feet far below it.
	MenuModelFitInput m = Measured();
	Check((m.engine.z + m.figureBottom) / m.engine.y < -1.0f / 4.229f,
	      "the engine's own place puts the feet below the view - the hips cut");

	// A taller figure stands further away; a wider half-angle nearer; a
	// figure whose root is its middle stands on the axis.
	MenuModelFitInput tall = Measured();
	tall.figureTop = 150.0f;
	const MenuModelFitVerdict t = FitMenuModel(tall);
	Check(t.fit && t.pos.y > v.pos.y && Near(t.pos.z, -68.0f), "a taller figure: further away, its middle on the axis");
	MenuModelFitInput wide = Measured();
	wide.tanHalfY = 1.0f / 2.378f;  // a square frame's own half-angle
	const MenuModelFitVerdict w = FitMenuModel(wide);
	Check(w.fit && w.pos.y < v.pos.y && Near(w.pos.y, 72.0f * kMenuModelMargin * 2.378f, 0.5f),
	      "a wider half-angle: nearer");
	MenuModelFitInput centred = Measured();
	centred.figureBottom = -60.0f;
	centred.figureTop = 60.0f;
	Check(Near(FitMenuModel(centred).pos.z, 0.0f), "a figure whose root is its middle: the root on the axis");

	// Refusals: no holder, no model on it, a figure or a view that reads as
	// nothing, an engine place without a distance.
	MenuModelFitInput none = Measured();
	none.holderFound = false;
	Check(!FitMenuModel(none).fit, "no holder: nothing fitted");
	MenuModelFitInput empty = Measured();
	empty.modelAttached = false;
	Check(!FitMenuModel(empty).fit, "no model on the holder: nothing fitted");
	MenuModelFitInput flat = Measured();
	flat.figureTop = flat.figureBottom + 5.0f;
	Check(!FitMenuModel(flat).fit, "a figure of no height: refused");
	MenuModelFitInput upsideDown = Measured();
	upsideDown.figureTop = -20.0f;
	Check(!FitMenuModel(upsideDown).fit, "a top below the bottom: refused");
	MenuModelFitInput blind = Measured();
	blind.tanHalfY = 0.0f;
	Check(!FitMenuModel(blind).fit, "a view of no height: refused");
	MenuModelFitInput nan = Measured();
	nan.tanHalfY = std::nanf("");
	Check(!FitMenuModel(nan).fit, "a view that is not a number: refused");
	MenuModelFitInput close = Measured();
	close.engine = NiPoint3{0.0f, 0.0f, 0.0f};
	Check(!FitMenuModel(close).fit, "an engine place at the camera itself: no angle to keep, refused");
}

}  // namespace

int main() {
	TestFit();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
