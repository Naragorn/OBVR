// Checks the blade's contact with the world (game/BladeContactLogic.h)
// against a world of boxes: what each kind of body is to a blade, the ray
// that goes on past clutter and stops at a wall, the turns between poses,
// the sweep of the blade's points, and every flow of the step - free, held
// at a wall, sliding along it, stopped in a corner, across a post, let go
// by distance and by angle, through and free again, behind a wall, taken
// up inside something, a jump of the camera - and the span cache, the jump
// test and the touch felt.

#include <cstdio>
#include <limits>
#include <string>

#include "game/BladeContactLogic.h"

namespace {

using namespace obvr::game;
using obvr::NiMatrix33;
using obvr::NiPoint3;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.01f) { return a - b < eps && b - a < eps; }

const float kNaN = std::numeric_limits<float>::quiet_NaN();
const float kPi = 3.14159265f;

// A box of the world: what a ray meets, its kind and its body.
struct Box {
	NiPoint3 low;
	NiPoint3 high;
	ContactKind kind;
	UInt32 body;
};

// The boxes as Havok's ray pick answers: the nearest face a ray enters, its
// fraction and outward normal; a ray that starts inside a box does not see
// it (a convex shape seen from within).
struct BoxWorld {
	Box boxes[8];
	UInt32 count = 0;
	UInt32 casts = 0;

	void Add(const NiPoint3& low, const NiPoint3& high, ContactKind kind, UInt32 body) {
		boxes[count++] = Box{low, high, kind, body};
	}

	BladeRayHit Cast(const NiPoint3& from, const NiPoint3& to) {
		++casts;
		BladeRayHit best;
		const float o[3] = {from.x, from.y, from.z};
		const float d[3] = {to.x - from.x, to.y - from.y, to.z - from.z};
		for (UInt32 i = 0; i < count; ++i) {
			const Box& b = boxes[i];
			const float lo[3] = {b.low.x, b.low.y, b.low.z};
			const float hi[3] = {b.high.x, b.high.y, b.high.z};
			bool inside = true;
			for (int k = 0; k < 3; ++k) {
				inside = inside && o[k] > lo[k] && o[k] < hi[k];
			}
			if (inside) {
				continue;
			}
			// The slabs: the last face entered is the one hit. A ray that
			// begins on a face hits it at once.
			float enter = -1e30f;
			float leave = 1e30f;
			int axis = -1;
			float sign = 0.0f;
			bool miss = false;
			for (int k = 0; k < 3 && !miss; ++k) {
				if (d[k] == 0.0f) {
					miss = o[k] < lo[k] || o[k] > hi[k];
					continue;
				}
				float t0 = (lo[k] - o[k]) / d[k];
				float t1 = (hi[k] - o[k]) / d[k];
				float s = -1.0f;
				if (t0 > t1) {
					const float t = t0;
					t0 = t1;
					t1 = t;
					s = 1.0f;
				}
				if (t0 > enter) {
					enter = t0;
					axis = k;
					sign = s;
				}
				leave = t1 < leave ? t1 : leave;
			}
			miss = miss || axis < 0 || enter > leave || enter < 0.0f || !(enter < 1.0f);
			if (miss || !(enter < best.fraction)) {
				continue;
			}
			best.hit = true;
			best.fraction = enter;
			best.point = NiPoint3{o[0] + d[0] * enter, o[1] + d[1] * enter, o[2] + d[2] * enter};
			best.normal = NiPoint3{axis == 0 ? sign : 0.0f, axis == 1 ? sign : 0.0f, axis == 2 ? sign : 0.0f};
			best.kind = b.kind;
			best.body = b.body;
		}
		return best;
	}
};

// A longsword's blade: from the grip 60 units along the grip's y.
BodySpan Blade() {
	BodySpan s;
	s.valid = true;
	s.a = NiPoint3{0.0f, 0.0f, 0.0f};
	s.b = NiPoint3{0.0f, 60.0f, 0.0f};
	return s;
}

BladePose At(float x, float y, float z) {
	BladePose p;
	p.pos = NiPoint3{x, y, z};
	return p;
}

BladePose TurnedAt(float x, float y, float z, float radiansAboutZ) {
	BladePose p = At(x, y, z);
	p.rot = RotationAbout(NiPoint3{0.0f, 0.0f, 1.0f}, radiansAboutZ);
	return p;
}

// The blade pointed down, a little forward: its tip 59.7 below the grip and
// 6 ahead.
BladePose PointedDownAt(float x, float y, float z) {
	BladePose p = At(x, y, z);
	p.rot = RotationAbout(NiPoint3{1.0f, 0.0f, 0.0f}, -1.4707963f);
	return p;
}

// A wall across the blade's way: y from 50 to 60.
void AddWallAhead(BoxWorld& w) {
	w.Add(NiPoint3{-200.0f, 50.0f, -200.0f}, NiPoint3{200.0f, 60.0f, 200.0f}, ContactKind::Fixed, 1);
}

BladeContactFrame FrameFor(const BladePose& wanted) {
	BladeContactFrame f;
	f.active = true;
	f.wanted = wanted;
	f.span = Blade();
	f.eye = NiPoint3{0.0f, -60.0f, 0.0f};
	f.dtSeconds = 0.011f;
	return f;
}

void TestKinds() {
	std::printf("What a body is to a blade\n");
	Check(ContactKindOf(false, kMotionTypeFixed, 1) == ContactKind::Ignored, "no body: ignored");
	Check(ContactKindOf(true, kMotionTypeFixed, 1) == ContactKind::Fixed, "a fixed static: stops it");
	Check(ContactKindOf(true, kMotionTypeKeyframed, 2) == ContactKind::Fixed, "a keyframed door: stops it");
	Check(ContactKindOf(true, 1, 4) == ContactKind::Movable, "clutter, dynamic: pushed");
	Check(ContactKindOf(true, 5, 8) == ContactKind::Movable, "a ragdoll's bone: pushed");
	Check(ContactKindOf(true, 0, 1) == ContactKind::Ignored, "an unknown motion: ignored");
	Check(ContactKindOf(true, 9, 1) == ContactKind::Ignored, "a motion past fixed: ignored");
	const UInt32 ignored[] = {11, 12, 15, 20, 21, 24, 27, 31};
	bool all = true;
	for (UInt32 layer : ignored) {
		all = all && ContactKindOf(true, kMotionTypeFixed, layer) == ContactKind::Ignored;
	}
	Check(all, "water, triggers, non-collidable, controllers, the avoid box and the picks: ignored, fixed or not");
	Check(ContactKindOf(true, kMotionTypeFixed, 13) == ContactKind::Fixed, "the terrain: stops it");
}

void TestFirstStopper() {
	std::printf("The first thing that stops a blade on a ray\n");
	BoxWorld w;
	UInt32 movables = 0;
	auto count = [&](const BladeRayHit&) { ++movables; };
	Check(!FirstStopper(w, NiPoint3{0, 0, 0}, NiPoint3{0, 100, 0}, count).hit, "an empty world: nothing");
	AddWallAhead(w);
	BladeRayHit h = FirstStopper(w, NiPoint3{0, 0, 0}, NiPoint3{0, 100, 0}, count);
	Check(h.hit && Near(h.fraction, 0.5f) && h.body == 1, "a wall at half the way: its fraction and body");
	w.Add(NiPoint3{-2, 20, -2}, NiPoint3{2, 24, 2}, ContactKind::Movable, 7);
	w.Add(NiPoint3{-2, 30, -2}, NiPoint3{2, 34, 2}, ContactKind::Ignored, 8);
	h = FirstStopper(w, NiPoint3{0, 0, 0}, NiPoint3{0, 100, 0}, count);
	Check(h.hit && Near(h.fraction, 0.5f), "past a cup and a trigger the wall still stops it, along the whole ray");
	Check(movables == 1, "the cup passed is handed on once, the trigger not");
	movables = 0;
	BoxWorld cups;
	for (UInt32 i = 0; i < 6; ++i) {
		const float y = 10.0f + 10.0f * static_cast<float>(i);
		cups.Add(NiPoint3{-2, y, -2}, NiPoint3{2, y + 2.0f, 2}, ContactKind::Movable, 10 + i);
	}
	h = FirstStopper(cups, NiPoint3{0, 0, 0}, NiPoint3{0, 100, 0}, count);
	Check(!h.hit, "only clutter on the way: nothing stops it");
	Check(movables == kBladeCastPasses && cups.casts == kBladeCastPasses,
	      "and a ray goes past at most four of them, one cast each");
	Check(!FirstStopper(w, NiPoint3{0, 0, 0}, NiPoint3{0, 0, 0}).hit, "a ray of no length asks nothing");
	BoxWorld behindCup;
	behindCup.Add(NiPoint3{-2, 98, -2}, NiPoint3{2, 99.9f, 2}, ContactKind::Movable, 5);
	Check(!FirstStopper(behindCup, NiPoint3{0, 0, 0}, NiPoint3{0, 100, 0}).hit,
	      "a cup at the ray's very end: nothing past it to ask");
}

void TestTurns() {
	std::printf("Turns between poses\n");
	NiPoint3 axis;
	float angle = 1.0f;
	Check(!TurnBetween(NiMatrix33::Identity(), NiMatrix33::Identity(), axis, angle) && angle == 0.0f,
	      "no turn: none, 0");
	const NiMatrix33 quarter = RotationAbout(NiPoint3{0, 0, 1}, kPi / 2.0f);
	Check(TurnBetween(NiMatrix33::Identity(), quarter, axis, angle) && Near(angle, kPi / 2.0f) && Near(axis.z, 1.0f),
	      "a quarter about z: that axis, that angle");
	Check(Near(AngleBetweenRotations(quarter, NiMatrix33::Identity()), kPi / 2.0f), "the angle either way round");
	const NiPoint3 y = quarter * NiPoint3{0, 1, 0};
	Check(Near(y.x, -1.0f) && Near(y.y, 0.0f), "Rodrigues turns y to -x a quarter about z");
	const BladePose half = PoseBetween(At(0, 0, 0), TurnedAt(10, 20, 0, kPi / 2.0f), 0.5f);
	Check(Near(half.pos.x, 5.0f) && Near(half.pos.y, 10.0f), "half the way: the grip half along");
	Check(Near(AngleBetweenRotations(NiMatrix33::Identity(), half.rot), kPi / 4.0f), "and half the turn");
	const BladePose same = PoseBetween(At(0, 0, 0), At(4, 0, 0), 0.25f);
	Check(Near(same.pos.x, 1.0f) && Near(AngleBetweenRotations(same.rot, NiMatrix33::Identity()), 0.0f),
	      "no turn: the grip alone moves");
	const NiMatrix33 almost = RotationAbout(NiPoint3{1, 0, 0}, kPi - 0.01f);
	Check(Near(AngleBetweenRotations(NiMatrix33::Identity(), almost), kPi - 0.01f, 0.02f), "near a half turn");
	Check(Near(BladePointAt(TurnedAt(1, 2, 3, kPi / 2.0f), Blade(), 1.0f).x, -59.0f),
	      "the tip of a blade turned a quarter");
}

void TestSweep() {
	std::printf("The sweep of the blade's points\n");
	BoxWorld w;
	AddWallAhead(w);
	BladeKicks kicks;
	BladeSweep s = SweepBlade(w, At(0, -20, 0), At(0, 0, 0), Blade(), 0.01f, &kicks);
	Check(s.hit && Near(s.fraction, 0.5f) && Near(s.along, 1.0f), "the tip meets the wall half way");
	Check(Near(s.safeFraction, 0.5f - 1.5f / 20.0f), "held short of it by the margin");
	Check(Near(s.normal.y, -1.0f) && Near(s.speedIn, 2000.0f), "the wall's face, and the tip's speed into it");
	Check(s.body == 1, "the wall's body");
	w.casts = 0;
	s = SweepBlade(w, At(0, -20, 0), At(0.2f, -20, 0), Blade(), 0.01f, &kicks);
	Check(!s.hit && w.casts == 0, "a move under half a unit asks nothing");
	s = SweepBlade(w, At(0, -20, 0), At(0, -10.5f, 0), Blade(), 0.01f, nullptr);
	Check(s.hit && Near(s.fraction, 1.0f) && Near(-20.0f + 9.5f * s.safeFraction, -11.5f),
	      "a move that ends within the margin of the wall: held at the margin");
	BoxWorld past;
	past.Add(NiPoint3{-2, 40.5f, -2}, NiPoint3{2, 41, 2}, ContactKind::Movable, 6);
	kicks = BladeKicks{};
	SweepBlade(past, At(0, -20, 0), At(0, -20.2f, 0), Blade(), 0.01f, &kicks);
	SweepBlade(past, At(0, -20, 0), At(0, -19.6f, 0), Blade(), 0.01f, &kicks);
	Check(kicks.count == 0, "a cup only within the margin past the move: not kicked");
	BoxWorld cup;
	cup.Add(NiPoint3{-2, 10, -2}, NiPoint3{2, 50, 2}, ContactKind::Movable, 9);
	kicks = BladeKicks{};
	s = SweepBlade(cup, At(-10, 0, 0), At(10, 0, 0), Blade(), 0.01f, &kicks);
	Check(!s.hit, "a tall cup does not stop it");
	Check(kicks.count == 1 && kicks.kick[0].body == 9 && Near(kicks.kick[0].velocity.x, 2000.0f),
	      "three points passed it: kicked once, at their speed");
	kicks = BladeKicks{};
	SweepBlade(cup, At(-10, 0, 0), At(10, 0, 0), Blade(), 0.0f, &kicks);
	Check(kicks.count == 0, "no time: no kick");
	SweepBlade(cup, At(-10, 0, 0), At(10, 0, 0), Blade(), 0.01f, nullptr);
	Check(true, "no kicks asked for: none written");
	BladeKicks k;
	k.Add(1, NiPoint3{1, 0, 0});
	k.Add(1, NiPoint3{3, 0, 0});
	k.Add(1, NiPoint3{2, 0, 0});
	Check(k.count == 1 && Near(k.kick[0].velocity.x, 3.0f), "one body once, at the fastest");
	for (UInt32 b = 2; b < 12; ++b) {
		k.Add(b, NiPoint3{1, 0, 0});
	}
	Check(k.count == kBladeKicksMax, "never more than eight");
	BoxWorld back;
	back.Add(NiPoint3{-200, -200, -200}, NiPoint3{200, -50, 200}, ContactKind::Fixed, 3);
	s = SweepBlade(back, At(0, -20, 0), At(0, -60, 0), Blade(), 0.01f, nullptr);
	Check(s.hit && Near(s.along, 0.0f) && Near(s.normal.y, 1.0f), "backwards into a wall: the guard meets it");
}

void TestFree() {
	std::printf("The step: off, free, taken up\n");
	BoxWorld w;
	AddWallAhead(w);
	BladeContactSettings set;
	BladeContactState s;
	BladeContactFrame f = FrameFor(At(0, -20, 0));
	f.active = false;
	BladeContactVerdict v = StepBladeContact(s, set, f, w);
	Check(!s.have && Near(v.pose.pos.y, -20.0f) && w.casts == 0, "inactive: the wanted pose, nothing asked");
	f.active = true;
	f.span.valid = false;
	v = StepBladeContact(s, set, f, w);
	Check(!s.have && w.casts == 0, "no blade span: the same");
	f.span = Blade();
	set.enabled = false;
	v = StepBladeContact(s, set, f, w);
	Check(!s.have && w.casts == 0, "switched off: the same");
	set.enabled = true;
	v = StepBladeContact(s, set, f, w);
	Check(s.have && !v.held && !v.through && v.event == BladeContactEvent::None, "the first frame, free: taken up");
	v = StepBladeContact(s, set, FrameFor(At(5, -25, 0)), w);
	Check(!v.held && Near(v.pose.pos.x, 5.0f) && Near(v.pose.pos.y, -25.0f), "a free move: where it is wanted");
	BladeContactState fresh;
	v = StepBladeContact(fresh, set, FrameFor(At(0, 20, 0)), w);
	Check(v.through && fresh.through && v.event == BladeContactEvent::StartedInside,
	      "taken up across the wall: through from the start");
}

void TestHeld() {
	std::printf("The step: held at a wall, sliding, a corner, a post\n");
	BoxWorld w;
	AddWallAhead(w);
	BladeContactSettings set;
	BladeContactState s;
	StepBladeContact(s, set, FrameFor(At(0, -20, 0)), w);
	BladeContactVerdict v = StepBladeContact(s, set, FrameFor(At(0, 0, 0)), w);
	Check(v.held && v.event == BladeContactEvent::Touched, "pushed into the wall: held, a touch");
	Check(Near(v.pose.pos.y, -11.5f) && Near(BladePointAt(v.pose, Blade(), 1.0f).y, 48.5f),
	      "the tip held 1.5 units short of the wall");
	Check(Near(v.gapUnits, 11.5f) && v.contact.hit && v.contact.body == 1, "its gap, and what held it");
	v = StepBladeContact(s, set, FrameFor(At(0, 2, 0)), w);
	Check(v.held && v.event == BladeContactEvent::None && Near(v.pose.pos.y, -11.5f),
	      "pressed on: still held, no new touch");
	v = StepBladeContact(s, set, FrameFor(At(0, -30, 0)), w);
	Check(!v.held && Near(v.pose.pos.y, -30.0f), "pulled back: free at once");

	BladeContactState d;
	StepBladeContact(d, set, FrameFor(At(0, -20, 0)), w);
	v = StepBladeContact(d, set, FrameFor(At(10, 0, 0)), w);
	Check(v.held && Near(v.pose.pos.x, 10.0f) && v.pose.pos.y < -11.0f && v.pose.pos.y > -12.0f,
	      "into it at a slant: it slides along to the side and stays out");

	BoxWorld corner;
	AddWallAhead(corner);
	corner.Add(NiPoint3{8, -100, -200}, NiPoint3{18, 49, 200}, ContactKind::Fixed, 2);
	BladeContactState c;
	StepBladeContact(c, set, FrameFor(At(0, -20, 0)), corner);
	v = StepBladeContact(c, set, FrameFor(At(10, 0, 0)), corner);
	Check(v.held && v.pose.pos.x < 8.0f - 1.0f && v.pose.pos.x > 4.0f, "a second wall in the slide: stopped short of it");

	BoxWorld post;
	post.Add(NiPoint3{-1, 20, -50}, NiPoint3{1, 25, 50}, ContactKind::Fixed, 4);
	BladeContactState p;
	StepBladeContact(p, set, FrameFor(At(-10, 0, 0)), post);
	v = StepBladeContact(p, set, FrameFor(At(0, 0, 0)), post);
	Check(v.held && Near(v.pose.pos.x, -10.0f), "a post between two points it would end across: it stays");
}

void TestLetGo() {
	std::printf("The step: let go, through, free again\n");
	BoxWorld w;
	AddWallAhead(w);
	BladeContactSettings set;
	// The cap these flows were laid out at: 0.30 m, 21 units (the default is
	// 0.60 m since 2026-10-10; its own flows are further down).
	set.letGoUnits = 21.0f;
	BladeContactState s;
	StepBladeContact(s, set, FrameFor(At(0, -20, 0)), w);
	BladeContactVerdict v = StepBladeContact(s, set, FrameFor(At(0, 20, 0)), w);
	Check(v.event == BladeContactEvent::LetGo && v.through && !v.held && Near(v.pose.pos.y, 20.0f),
	      "pushed 31 units past where it is held: let go, through, on the hand");
	v = StepBladeContact(s, set, FrameFor(At(0, 25, 0)), w);
	Check(v.through && v.event == BladeContactEvent::None && Near(v.pose.pos.y, 25.0f), "still across the wall: through");
	v = StepBladeContact(s, set, FrameFor(At(0, 70, 0)), w);
	Check(v.through, "wholly beyond the wall, seen from the eyes behind it: still through");
	v = StepBladeContact(s, set, FrameFor(At(0, -20, 0)), w);
	Check(!v.through && v.event == BladeContactEvent::Rearmed, "back in front of it: armed again");
	v = StepBladeContact(s, set, FrameFor(At(0, 0, 0)), w);
	Check(v.held && v.event == BladeContactEvent::Touched, "and held by the wall again");

	BoxWorld door;
	BladeContactState o;
	StepBladeContact(o, set, FrameFor(At(0, -20, 0)), door);
	door.Add(NiPoint3{-200, 10, -200}, NiPoint3{200, 12, 200}, ContactKind::Fixed, 6);
	v = StepBladeContact(o, set, FrameFor(At(0, -18, 0)), door);
	Check(v.event == BladeContactEvent::LetGo && v.through,
	      "a door swung across the blade where it was and where it goes: nowhere to hold it, let go");

	// At the default cap, 0.60 m (42 units; the tester, 2026-10-10: 0.30
	// "ist generell zu früh"), the same push still holds.
	const BladeContactSettings defaults;
	BladeContactState d;
	StepBladeContact(d, defaults, FrameFor(At(0, -20, 0)), w);
	v = StepBladeContact(d, defaults, FrameFor(At(0, 20, 0)), w);
	Check(v.held && !v.through && v.event != BladeContactEvent::LetGo, "the default 0.60 m: 31 units past, still held");
	v = StepBladeContact(d, defaults, FrameFor(At(0, 35, 0)), w);
	Check(v.event == BladeContactEvent::LetGo && v.through, "46 units past: let go");

	set.letGoUnits = 5.0f;
	BladeContactState n;
	StepBladeContact(n, set, FrameFor(At(0, -20, 0)), w);
	v = StepBladeContact(n, set, FrameFor(At(0, 0, 0)), w);
	Check(v.event == BladeContactEvent::LetGo, "a shorter cap lets go sooner");

	BoxWorld side;
	side.Add(NiPoint3{30, -200, -200}, NiPoint3{40, 200, 200}, ContactKind::Fixed, 5);
	BladeContactSettings turn;
	BladeContactState t;
	StepBladeContact(t, turn, FrameFor(TurnedAt(0, -20, 0, 0.0f)), side);
	bool letGo = false;
	bool stayedOut = true;
	float heldAt = 0.0f;
	for (int degrees = 5; degrees <= 120 && !letGo; degrees += 5) {
		v = StepBladeContact(t, turn, FrameFor(TurnedAt(0, -20, 0, -static_cast<float>(degrees) * kPi / 180.0f)), side);
		letGo = v.event == BladeContactEvent::LetGo;
		if (!letGo) {
			stayedOut = stayedOut && BladePointAt(v.pose, Blade(), 1.0f).x < 30.0f;
			heldAt = AngleBetweenRotations(NiMatrix33::Identity(), v.pose.rot);
		}
	}
	Check(stayedOut, "the wrist turned into a wall: the tip stays out of it");
	Check(heldAt > 20.0f * kPi / 180.0f && heldAt < 31.0f * kPi / 180.0f, "held where the tip meets it, near 30 degrees");
	Check(letGo && v.through, "turned on 45 degrees past it: let go");
}

void TestJump() {
	std::printf("The step: a jump of the camera\n");
	BoxWorld w;
	AddWallAhead(w);
	BladeContactSettings set;
	BladeContactState s;
	StepBladeContact(s, set, FrameFor(At(0, -20, 0)), w);
	BladeContactFrame f = FrameFor(At(0, 100, 0));
	f.jumped = true;
	f.eye = NiPoint3{0, 90, 0};
	BladeContactVerdict v = StepBladeContact(s, set, f, w);
	Check(!v.held && !v.through && Near(v.pose.pos.y, 100.0f), "teleported past the wall: nothing swept across it");
	f = FrameFor(At(0, 20, 0));
	f.jumped = true;
	v = StepBladeContact(s, set, f, w);
	Check(v.through && v.event == BladeContactEvent::StartedInside, "a jump that ends across the wall: through");

	Check(!CameraJumped(NiMatrix33::Identity(), NiPoint3{0, 0, 0}, NiMatrix33::Identity(), NiPoint3{5, 0, 0}),
	      "walked 5 units: no jump");
	Check(CameraJumped(NiMatrix33::Identity(), NiPoint3{0, 0, 0}, NiMatrix33::Identity(), NiPoint3{50, 0, 0}),
	      "50 units in a frame: a jump");
	Check(!CameraJumped(NiMatrix33::Identity(), NiPoint3{0, 0, 0}, RotationAbout(NiPoint3{0, 0, 1}, 0.05f),
	                    NiPoint3{0, 0, 0}),
	      "turned 3 degrees: no jump");
	Check(CameraJumped(NiMatrix33::Identity(), NiPoint3{0, 0, 0}, RotationAbout(NiPoint3{0, 0, 1}, 0.4f),
	                   NiPoint3{0, 0, 0}),
	      "a snap turn of 23 degrees: a jump");
	Check(CameraJumped(NiMatrix33::Identity(), NiPoint3{0, 0, 0}, NiMatrix33::Identity(), NiPoint3{kNaN, 0, 0}),
	      "a camera that is not a number: a jump");
}

void TestSpanCache() {
	std::printf("The blade's span as used\n");
	BladeSpanCache c;
	BodySpan none;
	Check(!StepBladeSpanCache(c, 1, none).valid, "nothing read yet: none");
	BodySpan a = Blade();
	Check(StepBladeSpanCache(c, 1, a).valid, "the first read is taken");
	BodySpan b = a;
	b.b = NiPoint3{0, 75, 0};
	for (UInt32 i = 0; i + 1 < kSpanRebuildFrames; ++i) {
		StepBladeSpanCache(c, 1, b);
	}
	Check(Near(c.span.b.y, 60.0f), "a moved span for under 90 frames: kept");
	StepBladeSpanCache(c, 1, b);
	Check(Near(c.span.b.y, 75.0f), "for 90 frames: taken");
	StepBladeSpanCache(c, 1, a);
	StepBladeSpanCache(c, 1, b);
	Check(c.movedFrames == 0, "a frame back in place starts the count anew");
	Check(Near(StepBladeSpanCache(c, 2, a).b.y, 60.0f), "another weapon: its own span at once");
	Check(!StepBladeSpanCache(c, 3, none).valid, "another weapon not readable yet: none, not the last one's");
}

void TestCapsuleGeometry() {
	std::printf("Capsules and segments\n");
	const NiPoint3 a{0, 0, -50};
	const NiPoint3 b{0, 0, 50};
	float f = -1.0f;
	NiPoint3 n;
	bool inside = true;
	Check(SegmentEntersCapsule(NiPoint3{-30, 0, 0}, NiPoint3{30, 0, 0}, a, b, 10.0f, f, n, inside) && !inside &&
	          Near(f, 20.0f / 60.0f) && Near(n.x, -1.0f),
	      "across the side: enters at its surface, the normal outward");
	Check(SegmentEntersCapsule(NiPoint3{0, 0, 100}, NiPoint3{0, 0, 0}, a, b, 10.0f, f, n, inside) &&
	          Near(f, 40.0f / 100.0f) && Near(n.z, 1.0f),
	      "down its axis: enters at the round end");
	Check(!SegmentEntersCapsule(NiPoint3{-30, 20, 0}, NiPoint3{30, 20, 0}, a, b, 10.0f, f, n, inside),
	      "passing beside it: never enters");
	Check(!SegmentEntersCapsule(NiPoint3{-30, 0, 0}, NiPoint3{-15, 0, 0}, a, b, 10.0f, f, n, inside),
	      "stopping short of it: never enters");
	Check(SegmentEntersCapsule(NiPoint3{3, 0, 0}, NiPoint3{30, 0, 0}, a, b, 10.0f, f, n, inside) && inside && f == 0.0f,
	      "starting in it: inside");
	Check(SegmentEntersCapsule(NiPoint3{-30, 0, 0}, NiPoint3{30, 0, 0}, NiPoint3{0, 0, 0}, NiPoint3{0, 0, 0}, 10.0f, f, n, inside) &&
	          Near(f, 20.0f / 60.0f),
	      "a capsule of no length is a ball");
	Check(!SegmentEntersCapsule(NiPoint3{-30, 0, 0}, NiPoint3{30, 0, 0}, a, b, 0.0f, f, n, inside),
	      "no radius: nothing");
	Check(!SegmentEntersCapsule(NiPoint3{-30, 0, 0}, NiPoint3{-30, 0, 0}, a, b, 10.0f, f, n, inside),
	      "a segment of no length outside: nothing");
	Check(Near(SegmentSegmentDistance(NiPoint3{-10, 0, 0}, NiPoint3{10, 0, 0}, NiPoint3{0, -10, 5}, NiPoint3{0, 10, 5}),
	           5.0f),
	      "two crossing segments 5 apart");
	Check(Near(SegmentSegmentDistance(NiPoint3{0, 0, 0}, NiPoint3{10, 0, 0}, NiPoint3{0, 3, 0}, NiPoint3{10, 3, 0}),
	           3.0f),
	      "two parallel ones");
	Check(Near(SegmentSegmentDistance(NiPoint3{0, 0, 0}, NiPoint3{10, 0, 0}, NiPoint3{14, 3, 0}, NiPoint3{20, 3, 0}),
	           5.0f),
	      "end to end");
	Check(Near(SegmentSegmentDistance(NiPoint3{0, 0, 0}, NiPoint3{0, 0, 0}, NiPoint3{3, 4, 0}, NiPoint3{3, 4, 0}), 5.0f),
	      "two points");
	Check(Near(SegmentSegmentDistance(NiPoint3{0, 0, 0}, NiPoint3{0, 0, 0}, NiPoint3{-5, 2, 0}, NiPoint3{5, 2, 0}), 2.0f),
	      "a point and a segment");
	Check(Near(SegmentSegmentDistance(NiPoint3{-5, 2, 0}, NiPoint3{5, 2, 0}, NiPoint3{0, 0, 0}, NiPoint3{0, 0, 0}), 2.0f),
	      "a segment and a point");
	Check(Near(PointSegmentDistance(NiPoint3{0, 5, 0}, NiPoint3{-1, 0, 0}, NiPoint3{1, 0, 0}), 5.0f),
	      "a point beside a segment");
}

void TestBodyCapsules() {
	std::printf("A person's capsules\n");
	NiPoint3 bones[kBladeBoneCount];
	bool have[kBladeBoneCount];
	for (UInt32 i = 0; i < kBladeBoneCount; ++i) {
		bones[i] = NiPoint3{static_cast<float>(i), 0, static_cast<float>(i) * 5.0f};
		have[i] = true;
	}
	bones[kBoneNeck] = NiPoint3{0, 0, 100};
	bones[kBoneHead] = NiPoint3{0, 0, 105};
	BladeBodies out;
	Check(BodyCapsulesFromBones(bones, have, 1.0f, 7, out) == 13 && out.count == 13, "a whole skeleton: 13 capsules");
	Check(out.cap[0].actor == 7 && Near(out.cap[0].radius, kBladeHeadRadius) &&
	          Near(out.cap[0].b.z, 105.0f + kBladeHeadAbove),
	      "the head from its bone on away from the neck");
	BladeBodies scaled;
	BodyCapsulesFromBones(bones, have, 2.0f, 7, scaled);
	Check(Near(scaled.cap[0].radius, 2.0f * kBladeHeadRadius) && Near(scaled.cap[0].b.z, 105.0f + 2.0f * kBladeHeadAbove),
	      "scaled twice: twice as round, the head twice as tall");
	BladeBodies odd;
	BodyCapsulesFromBones(bones, have, kNaN, 7, odd);
	Check(Near(odd.cap[0].radius, kBladeHeadRadius), "a scale that is not a number: 1");
	have[kBoneNeck] = false;
	BladeBodies noNeck;
	BodyCapsulesFromBones(bones, have, 1.0f, 7, noNeck);
	Check(noNeck.count == 11 && Near(noNeck.cap[0].b.z - noNeck.cap[0].a.z, kBladeHeadAbove),
	      "no neck: the head straight up, and the two links to the neck missing");
	for (UInt32 i = 0; i < kBladeBoneCount; ++i) {
		have[i] = i < kBladeBonesNeeded - 1;
	}
	BladeBodies few;
	Check(BodyCapsulesFromBones(bones, have, 1.0f, 7, few) == 0 && few.count == 0, "fewer than six bones: none");
	BladeBodies column;
	Check(BodyColumnFromBound(NiPoint3{0, 0, 60}, 50.0f, 8, column) && Near(column.cap[0].a.z, 20.0f) &&
	          Near(column.cap[0].radius, 22.5f),
	      "the column of a bound");
	Check(!BodyColumnFromBound(NiPoint3{0, 0, 60}, 0.0f, 8, column) && !BodyColumnFromBound(NiPoint3{0, 0, 60}, kNaN, 8, column),
	      "no bound: no column");
	BladeBodies full;
	for (UInt32 i = 0; i < kBladeBodyCapsulesMax; ++i) {
		full.Add(NiPoint3{0, 0, 0}, NiPoint3{0, 0, 1}, 1.0f, 1);
	}
	Check(!full.Add(NiPoint3{0, 0, 0}, NiPoint3{0, 0, 1}, 1.0f, 1) && full.count == kBladeBodyCapsulesMax,
	      "never more than the room for them");
	Check(std::string(BladeBoneName(kBoneRFoot)) == "Bip01 R Foot" && std::string(BladeBoneName(99)).empty(),
	      "the bones' names");
}

void TestPassLedger() {
	std::printf("Those passed\n");
	BladePassLedger l;
	Check(l.Add(5) && l.Has(5) && !l.Add(5), "added once");
	BladeBodies bodies;
	bodies.Add(NiPoint3{0, 0, -50}, NiPoint3{0, 0, 50}, 10.0f, 5);
	l.Step(NiPoint3{-5, 0, 0}, NiPoint3{5, 0, 0}, &bodies, 0.1f);
	Check(l.Has(5) && l.clear[0] == 0.0f, "the blade in them: kept, no time out");
	l.Step(NiPoint3{50, 0, 0}, NiPoint3{60, 0, 0}, &bodies, 0.1f);
	l.Step(NiPoint3{50, 0, 0}, NiPoint3{60, 0, 0}, &bodies, 0.1f);
	Check(l.Has(5), "out of them 0.2 s: still passed");
	l.Step(NiPoint3{50, 0, 0}, NiPoint3{60, 0, 0}, &bodies, 0.05f);
	Check(!l.Has(5), "out of them 0.25 s: no longer");
	l.Add(5);
	l.Step(NiPoint3{50, 0, 0}, NiPoint3{60, 0, 0}, nullptr, 0.01f);
	Check(!l.Has(5), "no longer near (no capsules): let go of at once");
	BladePassLedger fullLedger;
	for (UInt32 a = 1; a <= kBladePassedMax; ++a) {
		fullLedger.Add(a);
	}
	Check(!fullLedger.Add(99) && !fullLedger.Has(99), "never more than eight");
}

// A person standing at y 45: the torso a column 10 round, so its face at y 35.
BladeBodies PersonAhead(UInt32 actor = 11) {
	BladeBodies b;
	b.Add(NiPoint3{0, 45, -60}, NiPoint3{0, 45, 60}, 10.0f, actor);
	return b;
}

BladeContactFrame LivingFrame(const BladePose& wanted, const BladeBodies* bodies, bool swinging) {
	BladeContactFrame f = FrameFor(wanted);
	f.bodies = bodies;
	f.swinging = swinging;
	return f;
}

void TestLiving() {
	std::printf("The step: the living\n");
	BoxWorld none;
	BladeContactSettings set;
	set.letGoUnits = 21.0f;  // the cap these flows were laid out at (0.30 m)
	const BladeBodies person = PersonAhead();
	BladeContactState s;
	StepBladeContact(s, set, LivingFrame(At(0, -30, 0), &person, false), none);
	BladeContactVerdict v = StepBladeContact(s, set, LivingFrame(At(0, -20, 0), &person, false), none);
	Check(v.held && v.apart && v.event == BladeContactEvent::Touched && v.contact.actor == 11 && v.contact.body == 0,
	      "held slowly onto someone: it rests on them, a touch");
	Check(Near(BladePointAt(v.pose, Blade(), 1.0f).y, 33.5f), "the tip 1.5 units short of their body");
	v = StepBladeContact(s, set, LivingFrame(At(0, 10, 0), &person, false), none);
	Check(v.event == BladeContactEvent::LetGo && !v.through && s.passed.Has(11),
	      "pressed on past the cap: it goes into them, passed, not through a wall");
	v = StepBladeContact(s, set, LivingFrame(At(0, 12, 0), &person, false), none);
	Check(!v.held && !v.through && Near(v.pose.pos.y, 12.0f), "and stays in them, unheld");

	BladeContactState w;
	StepBladeContact(w, set, LivingFrame(At(0, -30, 0), &person, true), none);
	v = StepBladeContact(w, set, LivingFrame(At(0, 0, 0), &person, true), none);
	Check(!v.held && v.enteredBody && w.passed.Has(11) && Near(v.pose.pos.y, 0.0f),
	      "a swing into them: through, gone in, passed");
	v = StepBladeContact(w, set, LivingFrame(At(0, 2, 0), &person, false), none);
	Check(!v.held && !v.enteredBody, "slowing inside them: still passed, no second time in");
	for (int i = 0; i < 30; ++i) {
		v = StepBladeContact(w, set, LivingFrame(At(0, -40, 0), &person, false), none);
	}
	Check(!w.passed.Has(11), "out of them for a third of a second: no longer passed");
	v = StepBladeContact(w, set, LivingFrame(At(0, -20, 0), &person, false), none);
	Check(v.held && v.contact.actor == 11, "and a slow blade rests on them again");

	BladeContactState r;
	const BladeBodies away = PersonAhead();
	StepBladeContact(r, set, LivingFrame(At(0, -30, 0), &away, false), none);
	BladeBodies walkedIn;
	walkedIn.Add(NiPoint3{0, 10, -60}, NiPoint3{0, 10, 60}, 10.0f, 12);
	v = StepBladeContact(r, set, LivingFrame(At(0, -29, 0), &walkedIn, false), none);
	Check(!v.held && r.passed.Has(12), "someone walked into the resting blade: passed, not pushed against");

	BoxWorld wall;
	AddWallAhead(wall);
	BladeBodies near;
	near.Add(NiPoint3{0, 25, -60}, NiPoint3{0, 25, 60}, 5.0f, 13);
	BladeContactState t;
	StepBladeContact(t, set, LivingFrame(At(0, -20, 0), &near, true), wall);
	v = StepBladeContact(t, set, LivingFrame(At(0, 0, 0), &near, true), wall);
	Check(v.held && v.contact.body == 1 && v.contact.actor == 0 && t.passed.Has(13),
	      "a swing through someone into the wall behind them: the wall holds it");

	BoxWorld floor;
	floor.Add(NiPoint3{-200, -200, -50}, NiPoint3{200, 200, -40}, ContactKind::Fixed, 2);
	BladeBodies lying;
	lying.Add(NiPoint3{0, 20, -35}, NiPoint3{0, 60, -35}, 5.0f, 14);
	BladeContactState g;
	StepBladeContact(g, set, LivingFrame(PointedDownAt(0, 0, 25), &lying, false), floor);
	v = StepBladeContact(g, set, LivingFrame(PointedDownAt(0, 0, 10), &lying, false), floor);
	Check(v.held && v.contact.body == 2, "a blade pressed down onto the floor: the floor holds it");
	v = StepBladeContact(g, set, LivingFrame(PointedDownAt(0, 22, 15), &lying, false), floor);
	Check(v.held && v.contact.actor == 14, "slid along the floor into someone lying there: now they hold it");

	BladeBodies arm;
	arm.Add(NiPoint3{5, 22, -30}, NiPoint3{5, 22, 30}, 2.0f, 15);
	BladeContactState a;
	StepBladeContact(a, set, LivingFrame(At(-5, 0, 0), &arm, false), none);
	v = StepBladeContact(a, set, LivingFrame(At(5, 0, 0), &arm, false), none);
	Check(v.held && Near(v.pose.pos.x, -5.0f) && v.contact.actor == 15 && !a.passed.Has(15),
	      "slowly across an arm between two points: held where it was, on them, not passed");
	v = StepBladeContact(a, set, LivingFrame(At(5, 0, 0), &arm, true), none);
	Check(!v.held && Near(v.pose.pos.x, 5.0f), "swung across it: through");

	BladeContactState first;
	v = StepBladeContact(first, set, LivingFrame(At(0, 0, 0), &person, false), none);
	Check(first.passed.Has(11) && !v.through, "taken up inside someone: they are passed");
	BladeContactState thr;
	AddWallAhead(none);
	StepBladeContact(thr, set, LivingFrame(At(0, 20, 0), &person, false), none);
	Check(thr.through && thr.passed.Has(11), "taken up across a wall and in someone: through, and they passed");
}

void TestHitStop() {
	std::printf("The hit-stop\n");
	Check(Near(HitStopShare(0), 0.25f) && Near(HitStopShare(2), 0.25f) && Near(HitStopShare(3), 0.5f) &&
	          Near(HitStopShare(4), 0.75f) && Near(HitStopShare(5), 1.0f) && Near(HitStopShare(1000), 1.0f),
	      "a quarter for three frames, then a half, three quarters, all");
	BoxWorld none;
	BladeContactSettings set;
	BladeContactState s;
	StepBladeContact(s, set, FrameFor(At(0, 0, 0)), none);
	BladeContactFrame f = FrameFor(At(40, 0, 0));
	f.followShare = 0.25f;
	BladeContactVerdict v = StepBladeContact(s, set, f, none);
	Check(Near(v.pose.pos.x, 10.0f) && v.apart && !v.held, "slowed: a quarter of the way, apart, not held");
	f.followShare = 1.0f;
	v = StepBladeContact(s, set, f, none);
	Check(Near(v.pose.pos.x, 40.0f) && !v.apart, "then all of it again");
	f.followShare = 0.25f;
	f.wanted = At(200, 0, 0);
	v = StepBladeContact(s, set, f, none);
	Check(v.event != BladeContactEvent::LetGo && Near(v.pose.pos.x, 80.0f), "slowed far behind: never let go of");
}

void TestSettings() {
	std::printf("The settings as used\n");
	BladeContactSettings s = BladeContactSettingsFor(true, 0.30f, 45.0f, 70.0f);
	Check(s.enabled && Near(s.letGoUnits, 21.0f) && Near(s.letGoRadians, kPi / 4.0f), "0.30 m and 45 degrees: 21 units");
	s = BladeContactSettingsFor(false, 0.30f, 45.0f, 70.0f);
	Check(!s.enabled, "off stays off");
	s = BladeContactSettingsFor(true, kNaN, kNaN, 70.0f);
	Check(Near(s.letGoUnits, 42.0f) && Near(s.letGoRadians, kPi / 4.0f), "not numbers: the defaults, 0.60 m and 45 degrees");
	Check(Near(BladeContactSettings{}.letGoUnits, 42.0f), "the plain default: 0.60 m, 42 units");
	s = BladeContactSettingsFor(true, 0.60f, 45.0f, 70.0f);
	Check(Near(s.letGoUnits, 42.0f), "0.60 m: 42 units");
	s = BladeContactSettingsFor(true, 0.0f, 1.0f, 70.0f);
	Check(Near(s.letGoUnits, 3.5f) && Near(s.letGoRadians, 5.0f * kPi / 180.0f), "too small: 0.05 m and 5 degrees");
	s = BladeContactSettingsFor(true, 9.0f, 400.0f, 70.0f);
	Check(Near(s.letGoUnits, 140.0f) && Near(s.letGoRadians, kPi), "too large: 2 m and 180 degrees");
	s = BladeContactSettingsFor(true, 0.5f, 45.0f, 0.0f);
	Check(Near(s.letGoUnits, 35.0f), "no scale: 70 units a metre");
}

void TestFeel() {
	std::printf("The touch felt and heard\n");
	Check(Near(BladeTouchAmplitude(0.0f), kBladeTouchMinAmplitude), "no speed: the lightest pulse");
	Check(Near(BladeTouchAmplitude(kNaN), kBladeTouchMinAmplitude), "a speed that is not a number: the same");
	Check(Near(BladeTouchAmplitude(105.0f), 0.5f), "1.5 m/s: half");
	Check(Near(BladeTouchAmplitude(1000.0f), 1.0f), "faster than 3 m/s: full");
	Check(Near(BladeTouchAmplitude(10.0f), kBladeTouchMinAmplitude), "a slow touch: not under the lightest");
	Check(!BladeKnockSounds(100.0f, 1.0f), "under 1.5 m/s: no sound");
	Check(BladeKnockSounds(105.0f, 0.25f), "1.5 m/s a quarter second after the last: a sound");
	Check(!BladeKnockSounds(500.0f, 0.1f), "too soon after the last: none");
}

}  // namespace

int main() {
	TestKinds();
	TestFirstStopper();
	TestTurns();
	TestSweep();
	TestFree();
	TestHeld();
	TestLetGo();
	TestJump();
	TestSpanCache();
	TestCapsuleGeometry();
	TestBodyCapsules();
	TestPassLedger();
	TestLiving();
	TestHitStop();
	TestSettings();
	TestFeel();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
