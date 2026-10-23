// Checks the race menu's view (camera/ChargenView.h): where OBVR's eyes stand
// and face while the race and sex menu is up - in front of the character's
// face, looking at it, as the game's own camera does for that menu - from the
// face's centre, the player's facing and a distance.

#include <cmath>
#include <cstdio>

#include "camera/ChargenView.h"
#include "core/Rotation.h"

using namespace obvr;
using namespace obvr::camera;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.05f) { return std::fabs(a - b) < eps; }

bool Near(const NiPoint3& a, const NiPoint3& b, float eps = 0.05f) {
	return Near(a.x, b.x, eps) && Near(a.y, b.y, eps) && Near(a.z, b.z, eps);
}

float Length(const NiPoint3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

// The harness woman in her cell, facing north (the camera's right axis
// east), the eyes 0.9 m (63 units) before the face.
ChargenViewInput Measured() {
	ChargenViewInput in;
	in.wanted = true;
	in.raceMenuUp = true;
	in.faceKnown = true;
	in.face = NiPoint3{100.0f, 200.0f, 120.0f};
	in.facingKnown = true;
	in.facing = Heading{1.0f, 0.0f};
	in.distanceUnits = 63.0f;
	return in;
}

void TestView() {
	std::printf("The race menu's view before the face\n");
	const ChargenViewInput in = Measured();
	const ChargenViewVerdict v = ChargenView(in);
	Check(v.active, "the race menu up, the face and the facing read: active");
	// The player faces where the last world frame looked; the eyes stand the
	// distance along that facing, at the face's height.
	const NiPoint3 forward = ForwardOf(RotationFromHeading(in.facing));
	Check(Near(v.pos, in.face + forward * in.distanceUnits), "the eyes the distance in front of the face along the facing");
	Check(Near(v.pos.z, in.face.z), "at the face's height");
	// The view faces the character: level, and turned back along the facing
	// but for the engine's own angle to the side (11 over 30: 20.1 degrees),
	// aimed at a point to the player's right so the face sits at the right
	// of the picture as in the game.
	const NiPoint3 look = ForwardOf(v.rot);
	Check(Near(look.z, 0.0f, 0.001f) && Near(Length(look), 1.0f, 0.001f), "the view level and a unit forward");
	const float back = -(look.x * forward.x + look.y * forward.y);
	Check(Near(back, std::cos(std::atan(11.0f / 30.0f)), 0.001f), "turned back along the facing but for the engine's 11 over 30");
	const NiPoint3 right = NiPoint3{in.facing.cosine, in.facing.sine, 0.0f};  // the player's right
	const NiPoint3 aim = in.face + right * (in.distanceUnits * 11.0f / 30.0f);
	NiPoint3 toAim = aim - v.pos;
	toAim = toAim * (1.0f / Length(toAim));
	Check(Near(look, toAim, 0.001f), "the view aimed at the point beside the face, to the player's right");
	Heading read{};
	Check(HeadingOf(v.rot, read), "the rotation reads as a heading");

	// The engine's own numbers: 30 units off, aimed 11 to the side - the
	// same angle, the eyes nearer.
	ChargenViewInput engine = Measured();
	engine.distanceUnits = kEngineChargenDistanceUnits;
	const ChargenViewVerdict e = ChargenView(engine);
	Check(e.active && Near(Length(e.pos - in.face), 30.0f), "the engine's 30 units: the eyes 30 from the face");
	Check(Near(ForwardOf(e.rot), look, 0.001f), "and the same view angle - the side scales with the distance");

	// Another facing: the eyes follow it round.
	ChargenViewInput east = Measured();
	east.facing = Heading{0.0f, 1.0f};
	const ChargenViewVerdict w = ChargenView(east);
	const NiPoint3 eastForward = ForwardOf(RotationFromHeading(east.facing));
	Check(w.active && Near(w.pos, east.face + eastForward * 63.0f), "another facing: the eyes before the face along it");
	Check(Near(ForwardOf(w.rot).x * eastForward.x + ForwardOf(w.rot).y * eastForward.y, -back, 0.001f),
	      "looking back along it");

	// A facing read un-normalised is normalised first.
	ChargenViewInput longer = Measured();
	longer.facing = Heading{2.0f, 0.0f};
	Check(Near(ChargenView(longer).pos, v.pos), "an un-normalised facing: the same place");

	// Refusals: the setting off, another menu, no face, no facing, a
	// distance of nothing or not a number, a face or a facing that is not a
	// number, a facing of no length.
	ChargenViewInput off = Measured();
	off.wanted = false;
	Check(!ChargenView(off).active, "the setting off: nothing");
	ChargenViewInput other = Measured();
	other.raceMenuUp = false;
	Check(!ChargenView(other).active, "another menu on top: nothing");
	ChargenViewInput noFace = Measured();
	noFace.faceKnown = false;
	Check(!ChargenView(noFace).active, "the face unread: nothing");
	ChargenViewInput noFacing = Measured();
	noFacing.facingKnown = false;
	Check(!ChargenView(noFacing).active, "the facing unread: nothing");
	ChargenViewInput close = Measured();
	close.distanceUnits = 1.0f;
	Check(!ChargenView(close).active, "a distance of a unit or less: refused");
	ChargenViewInput nan = Measured();
	nan.distanceUnits = std::nanf("");
	Check(!ChargenView(nan).active, "a distance that is not a number: refused");
	ChargenViewInput badFace = Measured();
	badFace.face.y = std::nanf("");
	Check(!ChargenView(badFace).active, "a face that is not a number: refused");
	ChargenViewInput badFacing = Measured();
	badFacing.facing = Heading{std::nanf(""), 0.0f};
	Check(!ChargenView(badFacing).active, "a facing that is not a number: refused");
	ChargenViewInput flat = Measured();
	flat.facing = Heading{0.0f, 0.0f};
	Check(!ChargenView(flat).active, "a facing of no length: refused");
}

}  // namespace

int main() {
	TestView();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
