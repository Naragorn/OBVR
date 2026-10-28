// Checks the sneak indicator (vr/SneakIndicator.h): the choice's words and
// parsing; the step - not sneaking, not Full VR, third person, a menu, no world
// frame, the vignette's strength from the eye's alpha (undetected, detected,
// unknown, out of range), the eye shown in its mode; and the eye's lazy follow
// - inside the dead zone it stays, outside it eases towards the target, a long
// frame does not overshoot, the first frame arrives, numbers that are not
// numbers.

#include <cstdio>
#include <limits>

#include "vr/SneakIndicator.h"

namespace {

using namespace obvr::vr;
using obvr::NiPoint3;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.001f) { return a - b < eps && b - a < eps; }

const float kNaN = std::numeric_limits<float>::quiet_NaN();

void TestWords() {
	std::printf("The choice\n");
	SneakIndicator out = SneakIndicator::Eye;
	Check(ParseSneakIndicator("vignette", out) && out == SneakIndicator::Vignette, "vignette");
	Check(ParseSneakIndicator("EYE", out) && out == SneakIndicator::Eye, "eye, any case");
	Check(!ParseSneakIndicator("crosshair", out) && out == SneakIndicator::Eye, "anything else: refused, kept");
	Check(kSneakIndicatorDefault == SneakIndicator::Vignette, "the vignette by default (Nadi, 2026-10-28)");
	Check(SneakIndicatorFromIndex(1.0f) == SneakIndicator::Eye && SneakIndicatorFromIndex(0.0f) == SneakIndicator::Vignette &&
	          SneakIndicatorFromIndex(7.0f) == kSneakIndicatorDefault && SneakIndicatorFromIndex(kNaN) == kSneakIndicatorDefault,
	      "from the settings row's index, out of range the default");
}

SneakIndicatorInput Sneaking(float alpha) {
	SneakIndicatorInput in;
	in.fullVr = true;
	in.firstPerson = true;
	in.sneaking = true;
	in.menuUp = false;
	in.worldFrame = true;
	in.eyeAlpha = alpha;
	return in;
}

void TestStep() {
	std::printf("The step\n");
	SneakIndicatorVerdict v = StepSneakIndicator(Sneaking(50.0f), SneakIndicator::Vignette, 0.8f);
	Check(v.vignette && !v.eye && Near(v.vignetteStrength, 0.8f * 50.0f / 255.0f),
	      "undetected (the game's eye at alpha 50): a faint vignette, its strength the eye's share of 255");
	v = StepSneakIndicator(Sneaking(255.0f), SneakIndicator::Vignette, 0.8f);
	Check(v.vignette && Near(v.vignetteStrength, 0.8f), "detected (255): the full strength");
	v = StepSneakIndicator(Sneaking(400.0f), SneakIndicator::Vignette, 0.8f);
	Check(Near(v.vignetteStrength, 0.8f), "over 255: no more than full");
	v = StepSneakIndicator(Sneaking(kNaN), SneakIndicator::Vignette, 0.8f);
	Check(v.vignette && Near(v.vignetteStrength, 0.8f * kSneakEyeUnknownAlpha / 255.0f),
	      "the eye not read: as undetected - sneaking is still shown");
	v = StepSneakIndicator(Sneaking(-5.0f), SneakIndicator::Vignette, 0.8f);
	Check(v.vignette && Near(v.vignetteStrength, 0.8f * kSneakEyeUnknownAlpha / 255.0f), "a negative alpha: the same");
	v = StepSneakIndicator(Sneaking(0.0f), SneakIndicator::Vignette, 0.8f);
	Check(v.vignette && Near(v.vignetteStrength, 0.0f),
	      "the eye at 0 while sneaking (fading in): nothing yet, still the vignette's turn");
	v = StepSneakIndicator(Sneaking(120.0f), SneakIndicator::Vignette, kNaN);
	Check(!v.vignette && Near(v.vignetteStrength, 0.0f), "a strength that is not a number: nothing");

	v = StepSneakIndicator(Sneaking(120.0f), SneakIndicator::Eye, 0.8f);
	Check(v.eye && !v.vignette, "the eye's mode: the eye above the view, no vignette");

	SneakIndicatorInput in = Sneaking(120.0f);
	in.sneaking = false;
	v = StepSneakIndicator(in, SneakIndicator::Vignette, 0.8f);
	Check(!v.vignette && !v.eye, "not sneaking: nothing");
	in = Sneaking(120.0f);
	in.fullVr = false;
	v = StepSneakIndicator(in, SneakIndicator::Eye, 0.8f);
	Check(!v.vignette && !v.eye, "seated: nothing - the game's own eye at the crosshair as before");
	in = Sneaking(120.0f);
	in.firstPerson = false;
	v = StepSneakIndicator(in, SneakIndicator::Vignette, 0.8f);
	Check(!v.vignette && !v.eye, "third person: nothing");
	in = Sneaking(120.0f);
	in.menuUp = true;
	v = StepSneakIndicator(in, SneakIndicator::Vignette, 0.8f);
	Check(!v.vignette && !v.eye, "a menu: nothing");
	in = Sneaking(120.0f);
	in.worldFrame = false;
	v = StepSneakIndicator(in, SneakIndicator::Eye, 0.8f);
	Check(!v.vignette && !v.eye, "no world frame: nothing");
}

float Dot(const NiPoint3& a, const NiPoint3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

void TestFollow() {
	std::printf("The eye's lazy follow\n");
	const NiPoint3 ahead{0.0f, 0.0f, -1.0f};
	const NiPoint3 slight{0.087f, 0.0f, -0.996f};  // 5 degrees aside
	const NiPoint3 far{0.707f, 0.0f, -0.707f};     // 45 degrees aside
	NiPoint3 d = StepLazyDirection(ahead, slight, 10.0f, 4.0f, 0.011f);
	Check(Near(d.x, ahead.x) && Near(d.z, ahead.z), "5 degrees off, inside the 10-degree dead zone: it stays");
	d = StepLazyDirection(ahead, far, 10.0f, 4.0f, 0.011f);
	const float moved = Dot(d, far);
	Check(moved > Dot(ahead, far) && moved < 0.999f, "45 degrees off: it eases a step towards the target, not all the way");
	Check(Near(Dot(d, d), 1.0f), "and stays a unit direction");
	d = StepLazyDirection(ahead, far, 10.0f, 4.0f, 5.0f);
	Check(Near(Dot(d, far), 1.0f), "a long frame: it arrives, never past");
	d = StepLazyDirection(NiPoint3{0.0f, 0.0f, 0.0f}, far, 10.0f, 4.0f, 0.011f);
	Check(Near(Dot(d, far), 1.0f), "no direction yet (the first frame): it arrives at once");
	d = StepLazyDirection(NiPoint3{kNaN, 0.0f, -1.0f}, far, 10.0f, 4.0f, 0.011f);
	Check(Near(Dot(d, far), 1.0f), "a direction that is not a number: it arrives at the target");
	d = StepLazyDirection(ahead, NiPoint3{0.0f, 0.0f, 0.0f}, 10.0f, 4.0f, 0.011f);
	Check(Near(d.z, -1.0f), "no target: it stays");
}

void TestAbove() {
	std::printf("Where the eye is wanted\n");
	const NiPoint3 up = AboveView(NiPoint3{0.0f, 0.0f, -1.0f}, NiPoint3{0.0f, 1.0f, 0.0f}, 20.0f);
	Check(Near(up.y, 0.342f) && Near(up.z, -0.940f), "20 degrees above the view, along the head's up");
	const NiPoint3 flat = AboveView(NiPoint3{0.0f, 0.0f, -1.0f}, NiPoint3{0.0f, 1.0f, 0.0f}, 0.0f);
	Check(Near(flat.z, -1.0f), "0 degrees: the view itself");
}

}  // namespace

int main() {
	TestWords();
	TestStep();
	TestFollow();
	TestAbove();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
