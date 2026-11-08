// Blood where a blade went in (game/BladeBloodLogic.h): a spurt and a stain
// where a cut begins, a stain trail with drips along a slash, a spurt as a
// stab comes out; nothing for a blade that stayed out, on heavy armour, on
// light armour not pressed through.

#include <cstdio>

#include "game/BladeBloodLogic.h"

using namespace obvr::game;
using obvr::NiPoint3;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

BladeBloodFrame Frame(BladeEntryKind kind, BodyMaterial m, float x, bool entered = true, bool touching = true) {
	BladeBloodFrame f;
	f.entered = entered;
	f.kind = kind;
	f.material = m;
	f.touching = touching;
	f.point = NiPoint3{x, 0.0f, 0.0f};
	f.normal = NiPoint3{0.0f, -1.0f, 0.0f};
	return f;
}

void TestWhich() {
	std::printf("what bleeds\n");
	Check(BleedsThrough(BodyMaterial::Skin, false) && BleedsThrough(BodyMaterial::Cloth, false), "skin and cloth");
	Check(!BleedsThrough(BodyMaterial::Light, false) && BleedsThrough(BodyMaterial::Light, true),
	      "light armour only pressed through");
	Check(!BleedsThrough(BodyMaterial::Heavy, true) && !BleedsThrough(BodyMaterial::Unknown, true), "heavy, unknown: never");
}

void TestStab() {
	std::printf("a stab\n");
	BladeBloodState s;
	BladeBloodSettings set;
	const BloodAct in = StepBladeBlood(s, set, Frame(BladeEntryKind::Stab, BodyMaterial::Skin, 0.0f));
	Check(in.spray && in.stain && !in.pulledOut && in.sprayDir.y < -0.99f && in.sprayAt.y < -1.9f && in.stainDir.y > 0.99f,
	      "going in: a spurt outward from just outside, a stain into the body");
	const BloodAct held = StepBladeBlood(s, set, Frame(BladeEntryKind::Stab, BodyMaterial::Skin, 30.0f));
	Check(!held.spray && !held.stain, "while it is in: nothing more, however the point moves");
	const BloodAct out = StepBladeBlood(s, set, Frame(BladeEntryKind::Stab, BodyMaterial::Skin, 30.0f, true, false));
	Check(out.spray && out.pulledOut && out.sprayAt.x > 29.9f, "drawn out: a spurt where it came out");
	Check(!StepBladeBlood(s, set, Frame(BladeEntryKind::Stab, BodyMaterial::Skin, 30.0f, true, false)).spray,
	      "and only once");
}

void TestSlash() {
	std::printf("a slash\n");
	BladeBloodState s;
	BladeBloodSettings set;
	const BloodAct in = StepBladeBlood(s, set, Frame(BladeEntryKind::Slash, BodyMaterial::Cloth, 0.0f));
	Check(in.spray && in.stain, "going in: a spurt and a stain");
	Check(!StepBladeBlood(s, set, Frame(BladeEntryKind::Slash, BodyMaterial::Cloth, 5.0f)).stain, "5 units on: no stain yet");
	const BloodAct second = StepBladeBlood(s, set, Frame(BladeEntryKind::Slash, BodyMaterial::Cloth, 11.0f));
	Check(second.stain && second.drip && !second.spray, "11 units on: the next stain, with a drip (every second)");
	const BloodAct third = StepBladeBlood(s, set, Frame(BladeEntryKind::Slash, BodyMaterial::Cloth, 22.0f));
	Check(third.stain && !third.drip, "the third: no drip");
	UInt32 stains = 3;
	for (int i = 3; i < 30; ++i) {
		stains += StepBladeBlood(s, set, Frame(BladeEntryKind::Slash, BodyMaterial::Cloth, 11.0f * i)).stain ? 1 : 0;
	}
	Check(stains == set.mostStainsPerCut, "at most so many a cut");
	const BloodAct end = StepBladeBlood(s, set, Frame(BladeEntryKind::Slash, BodyMaterial::Cloth, 0.0f, true, false));
	Check(!end.spray, "a slash ending: no spurt (it was the stab's)");
}

void TestNone() {
	std::printf("no blood\n");
	BladeBloodSettings set;
	BladeBloodState a;
	Check(!StepBladeBlood(a, set, Frame(BladeEntryKind::None, BodyMaterial::Skin, 0.0f, false)).spray,
	      "touching but not in (glancing off): none");
	BladeBloodState b;
	Check(!StepBladeBlood(b, set, Frame(BladeEntryKind::Slash, BodyMaterial::Heavy, 0.0f)).spray, "heavy armour: none");
	BladeBloodState c;
	Check(!StepBladeBlood(c, set, Frame(BladeEntryKind::Stab, BodyMaterial::Light, 0.0f)).spray,
	      "light armour by speed alone: none");
	BladeBloodFrame pressed = Frame(BladeEntryKind::Stab, BodyMaterial::Light, 0.0f);
	pressed.byPressure = true;
	BladeBloodState d;
	Check(StepBladeBlood(d, set, pressed).spray, "light armour pressed through: blood");
	BladeBloodSettings off;
	off.enabled = false;
	BladeBloodState e;
	e.in = true;
	e.kind = BladeEntryKind::Stab;
	Check(!StepBladeBlood(e, off, Frame(BladeEntryKind::Stab, BodyMaterial::Skin, 0.0f)).spray && !e.in,
	      "switched off: none, and nothing kept");
	BladeBloodState f;
	BladeBloodFrame flat = Frame(BladeEntryKind::Stab, BodyMaterial::Skin, 0.0f);
	flat.normal = NiPoint3{0.0f, 0.0f, 0.0f};
	Check(StepBladeBlood(f, set, flat).sprayDir.z > 0.99f, "no normal: the spurt goes up");
}

}  // namespace

int main() {
	TestWhich();
	TestStab();
	TestSlash();
	TestNone();
	if (g_failures != 0) {
		std::printf("%d FAILED\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
