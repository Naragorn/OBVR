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

// A head turned by `degrees` (positive: to the left, as a heading's angle
// grows) and tilted down by `tilt`, as the tracker reports it.
NiMatrix33 Head(float degrees, float tilt = 0.0f) { return EulerToMatrix(-tilt, 0.0f, degrees); }

// Where the face lies from the view placed as PlaceMenuCamera places it: the
// aligned base, the head on it, the eyes at the aligned offset.
float FaceAngle(const ChargenViewVerdict& v, const ChargenAlign& a, const NiMatrix33& head, const NiPoint3& offset,
                const NiPoint3& face) {
	const NiMatrix33 base = ChargenAlignedBase(v.rot, a);
	const NiPoint3 eyes = v.pos + base * ChargenAlignedOffset(offset, a);
	return ChargenFaceYawDegrees(eyes, base * head, face);
}

void TestAlign() {
	std::printf("The race menu's view laid before the head as it is at the opening\n");
	const ChargenViewInput in = Measured();
	const ChargenViewVerdict v = ChargenView(in);
	// The engine's own aim: the face 20.1 degrees to the viewer's right with
	// the head where it was recentred.
	const float engineAim = std::atan(kEngineChargenAimSideUnits / kEngineChargenDistanceUnits) * 57.29578f;
	const NiPoint3 still{0.0f, 0.0f, 0.0f};
	{
		const ChargenAlign none{};
		const float a = FaceAngle(v, none, Head(0.0f), still, in.face);
		Check(Near(a, -engineAim, 0.5f), "the head as recentred, not aligned: the face 20 degrees to the right (old)");
		const float b = FaceAngle(v, none, Head(45.0f), still, in.face);
		Check(Near(b, -engineAim - 45.0f, 0.5f),
		      "the head 45 to the left at the opening, not aligned: the face 65 to the right - the tester's case");
	}
	const float turns[] = {0.0f, 45.0f, -45.0f, 90.0f, -90.0f, 180.0f};
	for (float t : turns) {
		ChargenAlign a{};
		Heading h{};
		HeadingOf(Head(t, 25.0f), h);
		a = StepChargenAlign(a, true, true, false, h, NiPoint3{12.0f, -7.0f, 3.0f});
		Check(a.held, "the opening takes the head");
		const float angle = FaceAngle(v, a, Head(t, 25.0f), NiPoint3{12.0f, -7.0f, 3.0f}, in.face);
		char what[160];
		std::snprintf(what, sizeof what, "the head %.0f degrees turned and tilted at the opening: the face %.1f, as the engine's aim",
		              static_cast<double>(t), static_cast<double>(angle));
		Check(Near(angle, -engineAim, 0.5f), what);
		// Looked round afterwards: the face moves against the look, the
		// menu does not follow the head.
		const float later = FaceAngle(v, a, Head(t + 30.0f, 25.0f), NiPoint3{12.0f, -7.0f, 3.0f}, in.face);
		Check(Near(later, -engineAim - 30.0f, 0.5f), "a look 30 to the left afterwards: the face 30 further right");
		// The step from where the head stood at the opening, not from the
		// recentre's place: the eyes at the chargen's place.
		const NiPoint3 eyes = v.pos + ChargenAlignedBase(v.rot, a) * ChargenAlignedOffset(NiPoint3{12.0f, -7.0f, 3.0f}, a);
		Check(Near(eyes.x, v.pos.x) && Near(eyes.y, v.pos.y), "standing off the recentre's place: the eyes before the face");
		Check(Near(eyes.z, v.pos.z + 3.0f), "the height kept as the head's");
	}
	{
		ChargenAlign a{};
		Heading h{};
		HeadingOf(Head(60.0f), h);
		a = StepChargenAlign(a, true, true, false, h, still);
		Heading later{};
		HeadingOf(Head(-20.0f), later);
		const ChargenAlign kept = StepChargenAlign(a, true, true, false, later, NiPoint3{5.0f, 5.0f, 0.0f});
		Check(kept.held && Near(kept.head.cosine, a.head.cosine) && Near(kept.head.sine, a.head.sine) &&
		          Near(kept.offset, a.offset),
		      "the menu stays up: the opening's head kept, not followed");
		const ChargenAlign closed = StepChargenAlign(kept, false, true, false, later, still);
		Check(!closed.held, "the menu closed: let go");
		const ChargenAlign reopened = StepChargenAlign(closed, true, true, false, later, still);
		Check(reopened.held && Near(reopened.head.sine, later.sine), "opened again: the head as it is then");
	}
	{
		Heading h{};
		HeadingOf(Head(30.0f), h);
		const ChargenAlign unknown = StepChargenAlign(ChargenAlign{}, true, false, false, h, still);
		Check(!unknown.held, "no head read: nothing taken");
		const ChargenAlign later = StepChargenAlign(unknown, true, true, false, h, still);
		Check(later.held, "the head read on a later frame: taken then");
		const ChargenAlign none{};
		Check(Near(ChargenAlignedBase(v.rot, none).data[0][0], v.rot.data[0][0]) &&
		          Near(ChargenAlignedBase(v.rot, none).data[1][0], v.rot.data[1][0]),
		      "nothing taken: the base as it was");
		const ChargenAlign film = StepChargenAlign(ChargenAlign{}, true, true, true, h, still);
		Check(!film.held, "the film still over the menu: nothing taken");
		Heading after{};
		HeadingOf(Head(-70.0f), after);
		const ChargenAlign seen = StepChargenAlign(film, true, true, false, after, still);
		Check(seen.held && Near(seen.head.sine, after.sine),
		      "the film over (played or skipped): the head as it is when the menu is first seen");
		Heading bad{std::nanf(""), 0.0f};
		Check(!StepChargenAlign(ChargenAlign{}, true, true, false, bad, still).held, "a head that is not a number: nothing taken");
	}
	{
		// The angle's sign: a face ahead and to the right reads negative.
		const NiMatrix33 north = NiMatrix33::Identity();
		Check(Near(ChargenFaceYawDegrees(NiPoint3{0, 0, 0}, north, NiPoint3{0, 100, 0}), 0.0f), "a face straight ahead: 0");
		Check(Near(ChargenFaceYawDegrees(NiPoint3{0, 0, 0}, north, NiPoint3{100, 100, 0}), -45.0f),
		      "a face ahead and right: -45");
		Check(Near(ChargenFaceYawDegrees(NiPoint3{0, 0, 0}, north, NiPoint3{-100, 0, 0}), 90.0f), "a face to the left: 90");
	}
}

}  // namespace

int main() {
	TestView();
	TestAlign();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
