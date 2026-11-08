// Whether a blade goes into a body (game/BladeEntryLogic.h,
// docs/blade-penetration-spec.md 3): by the material met, a stab's tip and
// angle and speed or its pressure held, a slash's edge and speed; heavy
// armour and an unknown surface never; light armour harder with its points;
// each contact judged once until it ends.

#include <cstdio>

#include "game/BladeEntryLogic.h"

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

constexpr float kUpm = 70.0f;  // units a metre

// A blade pointing +x at a surface facing -x (the body ahead along +x), the
// edge along z; touching at its tip.
BladeEntryFrame Stab(BodyMaterial m, float metresPerSecond) {
	BladeEntryFrame f;
	f.touching = true;
	f.bladed = true;
	f.material = m;
	f.along = 0.95f;
	f.axis = NiPoint3{1.0f, 0.0f, 0.0f};
	f.normal = NiPoint3{-1.0f, 0.0f, 0.0f};
	f.velocity = NiPoint3{metresPerSecond * kUpm, 0.0f, 0.0f};
	f.edgeValid = true;
	f.edge = NiPoint3{0.0f, 0.0f, 1.0f};
	f.unitsPerMetre = kUpm;
	f.dtSeconds = 1.0f / 90.0f;
	return f;
}

// The same blade swung down along its edge into the surface's side: the
// surface faces +z (the body below), the blade lies along x across it.
BladeEntryFrame Slash(BodyMaterial m, float metresPerSecond) {
	BladeEntryFrame f = Stab(m, 0.0f);
	f.along = 0.5f;
	f.normal = NiPoint3{0.0f, 0.0f, 1.0f};
	f.velocity = NiPoint3{0.0f, 0.0f, -metresPerSecond * kUpm};
	return f;
}

BladeEntryVerdict Once(const BladeEntryFrame& f) {
	BladeEntryState s;
	return StepBladeEntry(s, BladeEntrySettings{}, f);
}

void TestStabs() {
	std::printf("stabs\n");
	Check(Once(Stab(BodyMaterial::Skin, 2.0f)).entered && Once(Stab(BodyMaterial::Skin, 2.0f)).kind == BladeEntryKind::Stab,
	      "skin, 2 m/s along the blade: in, a stab");
	Check(!Once(Stab(BodyMaterial::Skin, 1.0f)).entered, "skin, 1 m/s: not in");
	Check(Once(Stab(BodyMaterial::Cloth, 2.5f)).entered && !Once(Stab(BodyMaterial::Cloth, 1.8f)).entered,
	      "cloth: 2.5 in, 1.8 not");
	Check(Once(Stab(BodyMaterial::Light, 4.5f)).entered && !Once(Stab(BodyMaterial::Light, 3.5f)).entered,
	      "light armour of no points: 4.5 in, 3.5 not");
	BladeEntryFrame glass = Stab(BodyMaterial::Light, 4.5f);
	glass.rating = 10;
	Check(!Once(glass).entered, "light armour of 10 points: 4.5 no longer enough");
	glass.velocity = NiPoint3{5.2f * kUpm, 0.0f, 0.0f};
	Check(Once(glass).entered, "10 points at 5.2 m/s: in");
	glass.rating = 255;
	glass.velocity = NiPoint3{6.4f * kUpm, 0.0f, 0.0f};
	Check(Once(glass).entered, "the points' share held to its most: 6.4 m/s goes in whatever the points");
	Check(!Once(Stab(BodyMaterial::Heavy, 20.0f)).entered, "heavy armour: never");
	Check(Once(Stab(BodyMaterial::Heavy, 20.0f)).refusal == BladeEntryRefusal::Heavy, "said as heavy armour");
	Check(Once(Stab(BodyMaterial::Unknown, 20.0f)).refusal == BladeEntryRefusal::Unknown, "an unknown surface: never");
	BladeEntryFrame mid = Stab(BodyMaterial::Skin, 3.0f);
	mid.along = 0.5f;
	Check(!Once(mid).entered || Once(mid).kind != BladeEntryKind::Stab, "met at the blade's middle: no stab");
	BladeEntryFrame slant = Stab(BodyMaterial::Skin, 3.0f);
	slant.normal = NiPoint3{-0.7f, 0.714f, 0.0f};  // 45 degrees off
	Check(Once(slant).kind != BladeEntryKind::Stab, "45 degrees off the surface's normal: no stab");
	BladeEntryFrame back = Stab(BodyMaterial::Skin, -3.0f);
	Check(!Once(back).entered, "pulled back out: nothing");
}

void TestSlashes() {
	std::printf("slashes\n");
	Check(Once(Slash(BodyMaterial::Skin, 3.0f)).entered && Once(Slash(BodyMaterial::Skin, 3.0f)).kind == BladeEntryKind::Slash,
	      "skin, 3 m/s edge first: in, a slash");
	Check(!Once(Slash(BodyMaterial::Skin, 2.0f)).entered, "skin, 2 m/s: not in");
	Check(Once(Slash(BodyMaterial::Light, 5.5f)).entered && !Once(Slash(BodyMaterial::Light, 4.5f)).entered,
	      "light: 5.5 in, 4.5 not");
	Check(!Once(Slash(BodyMaterial::Heavy, 30.0f)).entered, "heavy: never");
	BladeEntryFrame flat = Slash(BodyMaterial::Skin, 4.0f);
	flat.edge = NiPoint3{0.0f, 1.0f, 0.0f};  // the flat leads
	Check(!Once(flat).entered && Once(flat).refusal == BladeEntryRefusal::Flat, "the flat first: not in, said so");
	flat.edgeValid = false;
	Check(Once(flat).entered, "no section read: the edge taken to lead");
	BladeEntryFrame away = Slash(BodyMaterial::Skin, -4.0f);
	Check(!Once(away).entered, "moving off the surface: nothing");
}

void TestOthers() {
	std::printf("what is not judged\n");
	BladeEntryFrame blunt = Stab(BodyMaterial::Skin, 5.0f);
	blunt.bladed = false;
	Check(!Once(blunt).entered && Once(blunt).refusal == BladeEntryRefusal::NotBlade, "a mace: never");
	BladeEntryFrame none = Stab(BodyMaterial::Skin, 5.0f);
	none.touching = false;
	Check(!Once(none).judged, "touching nothing: not judged");
	BladeEntrySettings off;
	off.enabled = false;
	BladeEntryState s;
	Check(!StepBladeEntry(s, off, Stab(BodyMaterial::Skin, 5.0f)).judged, "switched off: not judged");
	BladeEntryFrame nan = Stab(BodyMaterial::Skin, 5.0f);
	nan.velocity = NiPoint3{0.0f / 0.0f, 0.0f, 0.0f};
	Check(!Once(nan).entered, "a speed that is no number: not in");
}

void TestPressure() {
	std::printf("pressure\n");
	BladeEntrySettings set;
	BladeEntryState s;
	BladeEntryFrame f = Stab(BodyMaterial::Light, 0.2f);  // slow: no speed entry
	f.held = true;
	f.pressUnits = 7.0f;  // the controller 7 units past where the blade is held
	BladeEntryVerdict v;
	UInt32 frames = 0;
	for (; frames < 90; ++frames) {
		v = StepBladeEntry(s, set, f);
		if (v.entered) {
			break;
		}
	}
	Check(v.entered && v.byPressure && v.kind == BladeEntryKind::Stab, "light armour pressed 7 units: in by pressure");
	Check(frames >= 40 && frames <= 50, "after about half a second");
	BladeEntryState s2;
	f.pressUnits = 4.0f;
	bool in = false;
	for (UInt32 i = 0; i < 200; ++i) {
		in = in || StepBladeEntry(s2, set, f).entered;
	}
	Check(!in, "light armour pressed only 4 units: never in");
	BladeEntryState s3;
	BladeEntryFrame skin = Stab(BodyMaterial::Skin, 0.2f);
	skin.held = true;
	skin.pressUnits = 3.0f;
	UInt32 skinFrames = 0;
	for (; skinFrames < 90 && !StepBladeEntry(s3, set, skin).entered; ++skinFrames) {
	}
	Check(skinFrames < 15, "skin pressed 3 units: in within a tenth of a second or so");
	BladeEntryState s4;
	BladeEntryFrame eased = Stab(BodyMaterial::Light, 0.2f);
	eased.held = true;
	eased.pressUnits = 7.0f;
	for (UInt32 i = 0; i < 30; ++i) {
		StepBladeEntry(s4, set, eased);
	}
	eased.pressUnits = 1.0f;
	StepBladeEntry(s4, set, eased);
	eased.pressUnits = 7.0f;
	bool early = false;
	for (UInt32 i = 0; i < 30; ++i) {
		early = early || StepBladeEntry(s4, set, eased).entered;
	}
	Check(!early, "the press let off: the clock starts again");
	BladeEntryState s5;
	BladeEntryFrame heavy = Stab(BodyMaterial::Heavy, 0.2f);
	heavy.held = true;
	heavy.pressUnits = 30.0f;
	bool heavyIn = false;
	for (UInt32 i = 0; i < 300; ++i) {
		heavyIn = heavyIn || StepBladeEntry(s5, set, heavy).entered;
	}
	Check(!heavyIn, "heavy armour pressed as hard as can be: never");
	BladeEntryState s6;
	BladeEntryFrame side = Slash(BodyMaterial::Skin, 0.2f);
	side.held = true;
	side.pressUnits = 10.0f;
	bool sideIn = false;
	for (UInt32 i = 0; i < 90; ++i) {
		sideIn = sideIn || StepBladeEntry(s6, set, side).entered;
	}
	Check(!sideIn, "the middle of the blade pressed in: pressure is a stab's only");
}

void TestOnce() {
	std::printf("once a contact\n");
	BladeEntrySettings set;
	BladeEntryState s;
	const BladeEntryVerdict first = StepBladeEntry(s, set, Stab(BodyMaterial::Skin, 3.0f));
	const BladeEntryVerdict second = StepBladeEntry(s, set, Stab(BodyMaterial::Skin, 3.0f));
	Check(first.judged && first.fresh && first.entered, "the first frame: judged, fresh, in");
	Check(second.entered && !second.fresh, "while it lasts: still in, not fresh");
	BladeEntryFrame off = Stab(BodyMaterial::Skin, 3.0f);
	off.touching = false;
	StepBladeEntry(s, set, off);
	Check(StepBladeEntry(s, set, Stab(BodyMaterial::Skin, 3.0f)).fresh, "touching again after a gap: fresh");
	BladeEntryState r;
	const BladeEntryVerdict refused = StepBladeEntry(r, set, Stab(BodyMaterial::Skin, 0.5f));
	const BladeEntryVerdict later = StepBladeEntry(r, set, Stab(BodyMaterial::Skin, 3.0f));
	Check(refused.fresh && !refused.entered && later.entered && later.fresh,
	      "refused at first, faster later in the same contact: in, said once");
	Check(BladeEntryRefusalName(BladeEntryRefusal::Heavy)[0] != '\0' && BladeEntryRefusalName(static_cast<BladeEntryRefusal>(99))[0] != '\0',
	      "every refusal has words");
}

void TestSink() {
	std::printf("a slash in sinks to the blade's width\n");
	const NiPoint3 n{0.0f, 0.0f, 1.0f};  // the surface faces up, the body below
	const NiPoint3 held{0.0f, 0.0f, 0.0f};
	const NiPoint3 deep = SlashSinkOffset(true, held, NiPoint3{0.0f, 0.0f, -10.0f}, n, 3.0f);
	Check(deep.z > -3.0001f && deep.z < -2.9999f && deep.x == 0.0f, "the hand 10 deep: the blade 3 in, no further");
	const NiPoint3 some = SlashSinkOffset(true, held, NiPoint3{5.0f, 0.0f, -1.5f}, n, 3.0f);
	Check(some.z > -1.5001f && some.z < -1.4999f && some.x == 0.0f, "the hand 1.5 deep: 1.5 in, only into the body");
	const NiPoint3 out = SlashSinkOffset(true, held, NiPoint3{0.0f, 0.0f, 2.0f}, n, 3.0f);
	Check(out.LengthSquared() == 0.0f, "the hand back out of them: the blade comes out with it");
	Check(SlashSinkOffset(false, held, NiPoint3{0.0f, 0.0f, -10.0f}, n, 3.0f).LengthSquared() == 0.0f,
	      "not a slash that went in: held at the surface as before");
	Check(SlashSinkOffset(true, held, NiPoint3{0.0f, 0.0f, -10.0f}, n, 0.0f).LengthSquared() == 0.0f,
	      "no depth: none");
	Check(BladeSinkUnits(true, 1.6f) > 3.19f && BladeSinkUnits(true, 1.6f) < 3.21f && BladeSinkUnits(false, 0.0f) == kBladeSinkFallbackUnits,
	      "the depth: the blade's width where its section was read, else the fallback");
}

}  // namespace

int main() {
	TestStabs();
	TestSlashes();
	TestOthers();
	TestPressure();
	TestOnce();
	TestSink();
	if (g_failures != 0) {
		std::printf("%d FAILED\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
