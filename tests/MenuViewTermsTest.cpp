// Checks what a menu frame's camera is built on (camera/MenuViewTerms.h):
// the last world frame's own terms on the engine's eye - the aim arc, the aim
// tilt, the engine-animation view's move and the sneak's lift - and, while the
// death view is held, its place alone; the height the look's and the sneak's
// (Nadi in the headset, 2026-11-28: the view switching to the left eye's place
// as the tablet, a book or the Esc menu opens, back as it closes).

#include <cstdio>

#include "camera/MenuViewTerms.h"

namespace {

using obvr::NiPoint3;
using namespace obvr::camera;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b) { return a == a && a >= b - 0.001f && a <= b + 0.001f; }

bool NearPoint(const NiPoint3& a, const NiPoint3& b) { return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z); }

const NiPoint3 kEye{100.0f, 200.0f, 120.0f};

void NothingAdded() {
	std::printf("No terms: the engine's eye and no height\n");
	const MenuViewBase b = MenuViewBaseFor(kEye, WorldViewTerms{}, false);
	Check(NearPoint(b.pos, kEye), "the engine's eye");
	Check(Near(b.vertical, 0.0f), "no height");
}

void AimArcCarried() {
	std::printf("The body turned under the view (the walk direction, the aim): the arc carried\n");
	WorldViewTerms t;
	t.aimArc = NiPoint3{-2.3f, 4.0f, 0.0f};
	const MenuViewBase b = MenuViewBaseFor(kEye, t, false);
	Check(NearPoint(b.pos, NiPoint3{97.7f, 204.0f, 120.0f}), "the eye put back where the world frame had it");
}

void TiltAndAnimCarried() {
	std::printf("Third person: the tilt and the engine-animation view's move carried\n");
	WorldViewTerms t;
	t.aimTilt = NiPoint3{0.0f, 1.0f, -3.0f};
	t.engineAnim = NiPoint3{5.0f, -6.0f, -40.0f};
	const MenuViewBase b = MenuViewBaseFor(kEye, t, false);
	Check(NearPoint(b.pos, NiPoint3{105.0f, 195.0f, 77.0f}), "both moves on the engine's eye");
}

void AllTermsSum() {
	std::printf("Every term at once: their sum\n");
	WorldViewTerms t;
	t.aimArc = NiPoint3{1.0f, 0.0f, 0.0f};
	t.aimTilt = NiPoint3{0.0f, 2.0f, 0.0f};
	t.engineAnim = NiPoint3{0.0f, 0.0f, 3.0f};
	t.lookHeight = -4.0f;
	t.sneakLift = 24.7f;
	const MenuViewBase b = MenuViewBaseFor(kEye, t, false);
	Check(NearPoint(b.pos, NiPoint3{101.0f, 202.0f, 123.0f}), "the three moves added");
	Check(Near(b.vertical, 20.7f), "the look's height and the sneak's lift");
}

void SneakLiftCarried() {
	std::printf("A real crouch: the sneak's lift in the height\n");
	WorldViewTerms t;
	t.sneakLift = 24.7f;
	const MenuViewBase b = MenuViewBaseFor(kEye, t, false);
	Check(Near(b.vertical, 24.7f), "the lift the world frame had");
	Check(NearPoint(b.pos, kEye), "the place unchanged");
}

void LookHeightAsUsed() {
	std::printf("The look's height as the world frame used it (0 when the anim view took the eye)\n");
	WorldViewTerms t;
	t.lookHeight = 0.0f;
	t.engineAnim = NiPoint3{0.0f, 0.0f, -75.0f};
	const MenuViewBase b = MenuViewBaseFor(kEye, t, false);
	Check(Near(b.vertical, 0.0f), "no height of the chase camera's");
	Check(NearPoint(b.pos, NiPoint3{100.0f, 200.0f, 45.0f}), "the eye in the body's head");
}

void DeathHeld() {
	std::printf("The death view held: its place alone, the aim's terms left out\n");
	WorldViewTerms t;
	t.aimArc = NiPoint3{3.0f, 3.0f, 0.0f};
	t.aimTilt = NiPoint3{0.0f, 0.0f, 2.0f};
	t.engineAnim = NiPoint3{1.0f, 1.0f, 1.0f};
	t.lookHeight = 1.5f;
	const NiPoint3 held{50.0f, 60.0f, 70.0f};
	const MenuViewBase b = MenuViewBaseFor(held, t, true);
	Check(NearPoint(b.pos, held), "the held place");
	Check(Near(b.vertical, 1.5f), "the look's height as used");
}

}  // namespace

int main() {
	NothingAdded();
	AimArcCarried();
	TiltAndAnimCarried();
	AllTermsSum();
	SneakLiftCarried();
	LookHeightAsUsed();
	DeathHeld();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
