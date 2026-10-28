// Checks the drawn weapon's shape fitted to its model
// (game/WeaponShapeLogic.h, [Hands] FittedWeaponShapes) and the section's
// part in the contact (game/BladeContactLogic.h): a made-up sword - its
// length to the tip, its section's width and thickness and broad direction,
// past its grip and guard, no head; a blade off the node's axis; a mace's
// head and a war axe's one-sided, flat head, also over most of its length;
// too few vertices and too short; the
// section's reach flat on, edge on and turned; a person held the steel's
// reach short and a wall the margin, along them and grazing them too; the
// turn off a person by the section; a head held its reach short of a wall
// and turned off a person with the line.

#include <cmath>
#include <cstdio>
#include <vector>

#include "game/WeaponShapeLogic.h"

using obvr::NiMatrix33;
using obvr::NiPoint3;
using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.05f) { return a - b < eps && b - a < eps; }

constexpr float kPi = 3.14159265358979f;

// A box of vertices along y from y0 to y1: x within +-hx, z within +-hz,
// its middle at (cx, cz); a grid of `step` units.
void Box(std::vector<NiPoint3>& pts, float y0, float y1, float hx, float hz, float cx = 0.0f, float cz = 0.0f,
         float step = 0.5f) {
	for (float y = y0; y <= y1 + 1e-4f; y += step) {
		for (int i = 0; i <= 4; ++i) {
			for (int k = 0; k <= 4; ++k) {
				const float x = -hx + 2.0f * hx * static_cast<float>(i) / 4.0f;
				const float z = -hz + 2.0f * hz * static_cast<float>(k) / 4.0f;
				pts.push_back(NiPoint3{cx + x, y, cz + z});
			}
		}
	}
}

// Rings round the y axis from y0 to y1, `radius` round, the middle at (cx, cz).
void Rings(std::vector<NiPoint3>& pts, float y0, float y1, float radius, float cx = 0.0f, float cz = 0.0f) {
	for (float y = y0; y <= y1 + 1e-4f; y += 0.5f) {
		for (int k = 0; k < 24; ++k) {
			const float a = 2.0f * kPi * static_cast<float>(k) / 24.0f;
			pts.push_back(NiPoint3{cx + radius * std::cos(a), y, cz + radius * std::sin(a)});
		}
	}
}

// A longsword: a pommel behind the node, a grip, a wide guard, a blade 1.8
// wide either way along x and 0.3 thick along z, to its tip at 50.
std::vector<NiPoint3> Sword(float cx = 0.0f) {
	std::vector<NiPoint3> pts;
	Box(pts, -2.0f, 0.0f, 1.2f, 1.2f, cx);   // pommel
	Rings(pts, 0.0f, 6.0f, 0.8f, cx);        // grip
	Box(pts, 6.0f, 7.0f, 6.0f, 1.0f, cx);    // guard
	Box(pts, 7.0f, 48.0f, 1.8f, 0.3f, cx);   // blade
	pts.push_back(NiPoint3{cx, 50.0f, 0.0f});  // the point
	return pts;
}

const NiPoint3 kY{0.0f, 1.0f, 0.0f};

void TestSword() {
	std::printf("A longsword\n");
	const std::vector<NiPoint3> pts = Sword();
	WeaponShape w;
	Check(FitWeaponShape(pts.data(), static_cast<UInt32>(pts.size()), kY, w) && w.valid, "fitted");
	Check(Near(w.length, 50.0f) && Near(w.b.y, 50.0f) && Near(w.a.y, 0.0f), "from the node to its tip at 50");
	Check(Near(w.section.halfWidth, 1.8f) && Near(w.section.halfThickness, 0.3f),
	      "its section 1.8 wide either way and 0.3 thick - not the guard's 6");
	Check(std::fabs(w.section.major.x) > 0.99f && std::fabs(w.section.minor.z) > 0.99f,
	      "the edge along x, the flat's normal along z");
	Check(!w.head, "no head");
}

void TestOffAxis() {
	std::printf("A blade off the node's axis\n");
	const std::vector<NiPoint3> pts = Sword(1.0f);
	WeaponShape w;
	FitWeaponShape(pts.data(), static_cast<UInt32>(pts.size()), kY, w);
	Check(w.valid && Near(w.a.x, 1.0f, 0.2f) && Near(w.b.x, 1.0f, 0.2f), "its line through the blade's middle, 1 off");
	Check(Near(w.section.halfWidth, 1.8f, 0.2f), "its width still 1.8");
}

void TestHeads() {
	std::printf("Heads\n");
	std::vector<NiPoint3> mace;
	Rings(mace, 0.0f, 26.0f, 0.8f);  // the shaft
	for (float y = 26.0f; y <= 34.0f; y += 0.5f) {
		for (int k = 0; k < 24; ++k) {
			const float a = 2.0f * kPi * static_cast<float>(k) / 24.0f;
			mace.push_back(NiPoint3{4.0f * std::cos(a), y, 4.0f * std::sin(a)});
		}
	}
	WeaponShape m;
	FitWeaponShape(mace.data(), static_cast<UInt32>(mace.size()), kY, m);
	Check(m.valid && m.head, "a mace: a head");
	Check(Near(m.headRadius, 4.0f, 0.3f), "its head 4 round");
	Check(m.headA.y > 24.0f && m.headB.y > 24.0f, "the head at the top");
	Check(Near(m.section.halfWidth, 0.8f, 0.1f) && Near(m.section.halfThickness, 0.8f, 0.1f),
	      "the line's section the shaft's, 0.8 round");

	// an iron mace's long head: more than half its length
	std::vector<NiPoint3> longHead;
	Rings(longHead, 0.0f, 14.0f, 0.8f);
	Rings(longHead, 14.0f, 31.0f, 4.5f);
	WeaponShape lh;
	FitWeaponShape(longHead.data(), static_cast<UInt32>(longHead.size()), kY, lh);
	Check(lh.valid && lh.head && Near(lh.shaftReach, 0.8f, 0.1f), "a head over half the length: still a head, the grip the shaft");
	Check(lh.headA.y > 13.0f && Near(lh.section.halfWidth, 0.8f, 0.1f), "the head from 14 on, the line's section the grip's");

	// a dagger: its guard stands out at 40 % of its length, far from the tip
	std::vector<NiPoint3> dagger;
	Rings(dagger, 0.0f, 8.0f, 0.8f);
	Box(dagger, 8.0f, 9.0f, 4.0f, 0.8f);
	Box(dagger, 9.0f, 21.0f, 1.3f, 0.15f);
	WeaponShape d;
	FitWeaponShape(dagger.data(), static_cast<UInt32>(dagger.size()), kY, d);
	Check(d.valid && !d.head, "a dagger's guard is no head");
	Check(Near(d.section.halfWidth, 1.3f, 0.15f), "its section the blade's");

	std::vector<NiPoint3> axe;
	Rings(axe, 0.0f, 30.0f, 0.8f);
	Box(axe, 24.0f, 30.0f, 4.0f, 0.4f, 4.0f);  // the blade on one side, out to x 8
	WeaponShape a;
	FitWeaponShape(axe.data(), static_cast<UInt32>(axe.size()), kY, a);
	Check(a.valid && a.head, "a war axe: a head");
	Check(a.headA.x > 1.0f && a.headB.x > 1.0f, "its line off on the blade's side, not round the shaft");
	Check(a.headSection.valid && !a.headSection.round && a.headSection.halfWidth > 3.0f * a.headSection.halfThickness,
	      "its section flat: wide along the blade, thin across it");
	Check(!m.headSection.round || Near(m.headSection.halfWidth, m.headSection.halfThickness),
	      "a round head reaches the same every way");
	Check(m.headSection.round, "the mace's head round");

	// an iron war axe: its blade along 70 % of its length, the grip thin
	// (read 2026-10-27: slices 7.4-14.7 round against a 1.3 grip)
	std::vector<NiPoint3> longAxe;
	Rings(longAxe, 0.0f, 32.0f, 1.3f);
	Box(longAxe, 10.0f, 31.0f, 6.0f, 0.6f, 6.0f);
	WeaponShape la;
	FitWeaponShape(longAxe.data(), static_cast<UInt32>(longAxe.size()), kY, la);
	Check(la.valid && la.head && Near(la.shaftReach, 1.3f, 0.1f), "a blade over most of the haft: a head, the grip the shaft");
	Check(la.head && la.headA.y < 12.0f && !la.headSection.round, "the head from where the blade starts, flat");
	Check(Near(la.section.halfWidth, 1.3f, 0.2f), "the line's section the haft's");

	// its haft only a ring at the grip and none up to the head (a few long
	// triangles, the iron war axe's model): the line read from the node on
	std::vector<NiPoint3> sparse;
	Rings(sparse, 0.0f, 3.0f, 1.3f);
	Box(sparse, 10.0f, 31.0f, 6.0f, 0.6f, 6.0f);
	sparse.push_back(NiPoint3{0.0f, 32.0f, 0.0f});
	WeaponShape sa;
	Check(FitWeaponShape(sparse.data(), static_cast<UInt32>(sparse.size()), kY, sa) && sa.head,
	      "a haft with no vertex between grip and head: still a shape and its head");
	Check(sa.valid && Near(sa.section.halfWidth, 1.3f, 0.2f), "the line's section from the grip's ring");
	// nothing before the head but the head: a round line of the shaft's
	std::vector<NiPoint3> headOnly;
	headOnly.push_back(NiPoint3{0.0f, 0.5f, 1.3f});
	headOnly.push_back(NiPoint3{0.0f, 0.5f, -1.3f});
	Box(headOnly, 10.0f, 31.0f, 6.0f, 0.6f, 6.0f);
	WeaponShape ho;
	Check(FitWeaponShape(headOnly.data(), static_cast<UInt32>(headOnly.size()), kY, ho) && ho.head &&
	          ho.section.round && Near(ho.section.halfWidth, 1.3f, 0.05f),
	      "too few before the head: the line round with the shaft's reach");
}

void TestRefused() {
	std::printf("Nothing to fit\n");
	std::vector<NiPoint3> few;
	Box(few, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f);
	few.resize(8);
	WeaponShape w;
	Check(!FitWeaponShape(few.data(), static_cast<UInt32>(few.size()), kY, w) && !w.valid, "too few vertices");
	std::vector<NiPoint3> stub;
	Box(stub, 0.0f, 3.0f, 1.0f, 1.0f);
	Check(!FitWeaponShape(stub.data(), static_cast<UInt32>(stub.size()), kY, w), "shorter than a blade");
	std::vector<NiPoint3> behind;
	Box(behind, -40.0f, -1.0f, 1.0f, 1.0f);
	Check(!FitWeaponShape(behind.data(), static_cast<UInt32>(behind.size()), kY, w), "all behind the node");
	Check(!FitWeaponShape(nullptr, 0, kY, w), "no vertices");
}

void TestReach() {
	std::printf("The section's reach\n");
	BladeSection s;
	s.valid = true;
	s.major = NiPoint3{1.0f, 0.0f, 0.0f};
	s.minor = NiPoint3{0.0f, 0.0f, 1.0f};
	s.halfWidth = 1.8f;
	s.halfThickness = 0.3f;
	const NiMatrix33 id = NiMatrix33::Identity();
	Check(Near(SectionReach(s, id, NiPoint3{0.0f, 0.0f, 1.0f}), 0.3f), "flat on: its half thickness");
	Check(Near(SectionReach(s, id, NiPoint3{-1.0f, 0.0f, 0.0f}), 1.8f), "edge on: its half width");
	const float d = 0.70710678f;
	Check(Near(SectionReach(s, id, NiPoint3{d, 0.0f, d}), (1.8f + 0.3f) * d), "between: a box's reach");
	NiMatrix33 turn = NiMatrix33::Identity();  // a quarter turn about y: x to -z
	turn.data[0][0] = 0.0f;
	turn.data[0][2] = 1.0f;
	turn.data[2][0] = -1.0f;
	turn.data[2][2] = 0.0f;
	Check(Near(SectionReach(s, turn, NiPoint3{0.0f, 0.0f, 1.0f}), 1.8f), "turned a quarter: the edge toward z");
	Check(Near(PersonMargin(&s, id, NiPoint3{0.0f, 0.0f, 1.0f}, 1.5f), 0.3f + kBladeSectionSpareUnits),
	      "a person: the reach and the spare");
	BladeSection none;
	Check(Near(PersonMargin(&none, id, NiPoint3{0.0f, 0.0f, 1.0f}, 1.5f), 1.5f), "no section read: the margin");
	Check(Near(PersonMargin(nullptr, id, NiPoint3{0.0f, 0.0f, 1.0f}, 1.5f), 1.5f), "none given: the margin");
	Check(Near(SectionMostReach(&s, 1.5f), 1.8f + 0.3f + kBladeSectionSpareUnits), "the ray looks the most reach on");
	Check(Near(SectionMostReach(&none, 1.5f), 1.5f), "or the margin");
	BladeSection ball;
	ball.valid = true;
	ball.round = true;
	ball.halfWidth = 4.0f;
	Check(Near(SectionReach(ball, id, NiPoint3{0.0f, 0.0f, 1.0f}), 4.0f) &&
	          Near(SectionReach(ball, id, NiPoint3{d, 0.0f, d}), 4.0f),
	      "a round head: its radius every way across its axis");
	// Its axis is y (major x, minor z): a disc across it, nothing along it -
	// a mace's tip is its narrow end (the iron mace's slices, 2026-10-27:
	// 8.5 round at its flanges, 1.6 at the tip).
	Check(Near(SectionReach(ball, id, NiPoint3{0.0f, -1.0f, 0.0f}), 0.0f), "along its axis: nothing");
	Check(Near(SectionReach(ball, id, NiPoint3{0.0f, d, d}), 4.0f * d), "half way between: the radius times the sine");
	Check(Near(SectionMostReach(&ball, 1.5f), 4.0f + kBladeSectionSpareUnits), "and its ray looks that far on");
}

// A world with nothing fixed in it.
struct EmptyWorld {
	BladeRayHit Cast(const NiPoint3&, const NiPoint3&) { return BladeRayHit{}; }
};

// A world with one wall: everything at z >= `at` is solid, its face down.
struct WallWorld {
	float at = 10.0f;
	BladeRayHit Cast(const NiPoint3& from, const NiPoint3& to) {
		BladeRayHit h;
		if (from.z < at && to.z >= at) {
			h.hit = true;
			h.fraction = (at - from.z) / (to.z - from.z);
			h.point = from + (to - from) * h.fraction;
			h.normal = NiPoint3{0.0f, 0.0f, -1.0f};
			h.kind = ContactKind::Fixed;
			h.body = 0x1234;
		}
		return h;
	}
};

BladePose At(float x, float y, float z) {
	BladePose p;
	p.pos = NiPoint3{x, y, z};
	return p;
}

void TestHeldShort() {
	std::printf("Held short of a person by the steel, of a wall by the margin\n");
	BodySpan blade;
	blade.valid = true;
	blade.b = NiPoint3{0.0f, 40.0f, 0.0f};
	BladeSection s;
	s.valid = true;
	s.major = NiPoint3{1.0f, 0.0f, 0.0f};
	s.minor = NiPoint3{0.0f, 0.0f, 1.0f};
	s.halfWidth = 1.8f;
	s.halfThickness = 0.3f;
	// a person's capsule along y, its surface at z 10: the blade lowered
	// onto it flat (its flat's normal z) and edge first (turned a quarter)
	BladeBodies person;
	person.Add(NiPoint3{0.0f, -20.0f, 15.0f}, NiPoint3{0.0f, 60.0f, 15.0f}, 5.0f, 0x2000);
	const BladeLiving living{&person, nullptr, false, nullptr, nullptr, true};
	EmptyWorld empty;
	const BladeSweep flat = SweepBlade(empty, At(0, 0, 0), At(0, 0, 12), blade, 0.01f, nullptr, living, 1.5f, &s);
	Check(flat.hit && flat.actor == 0x2000, "flat on: held by them");
	Check(Near(12.0f * flat.safeFraction, 10.0f - 0.3f - kBladeSectionSpareUnits),
	      "flat on: held its half thickness and the spare short");
	BladePose edge = At(0, 0, 12);
	edge.rot.data[0][0] = 0.0f;
	edge.rot.data[0][2] = 1.0f;
	edge.rot.data[2][0] = -1.0f;
	edge.rot.data[2][2] = 0.0f;
	BladePose edgeFrom = edge;
	edgeFrom.pos = NiPoint3{0.0f, 0.0f, 0.0f};
	const BladeSweep edged = SweepBlade(empty, edgeFrom, edge, blade, 0.01f, nullptr, living, 1.5f, &s);
	Check(edged.hit && Near(12.0f * edged.safeFraction, 10.0f - 1.8f - kBladeSectionSpareUnits),
	      "edge on: held its half width and the spare short");
	const BladeSweep old = SweepBlade(empty, At(0, 0, 0), At(0, 0, 12), blade, 0.01f, nullptr, living, 1.5f, nullptr);
	Check(old.hit && Near(12.0f * old.safeFraction, 10.0f - 1.5f), "no section: the margin, as before");
	// grazing: lowered onto them at 60 degrees off their surface's normal
	// (z 12 down, x 20.8 along): its reach kept off the surface, not along
	// the way
	// (a capsule across the blade, along x, so the way along x keeps its
	// distance)
	BladeBodies across;
	across.Add(NiPoint3{-60.0f, 20.0f, 15.0f}, NiPoint3{60.0f, 20.0f, 15.0f}, 5.0f, 0x2004);
	const BladeLiving acrossLiving{&across, nullptr, false, nullptr, nullptr, true};
	const BladeSweep graze =
		SweepBlade(empty, At(-20.78f, 0, 0), At(0, 0, 12), blade, 0.01f, nullptr, acrossLiving, 1.5f, &s);
	const float wayUnits = std::sqrt(20.78f * 20.78f + 12.0f * 12.0f);
	const float stopZ = 12.0f * graze.safeFraction;
	Check(graze.hit && Near(stopZ, 10.0f - 0.3f - kBladeSectionSpareUnits, 0.1f),
	      "grazing: still its half thickness and the spare off their surface");
	Check(graze.safeFraction * wayUnits < wayUnits, "short of where it would go");
	// along them 1.5 units off their surface: flat on (its reach 0.4) it
	// passes; edge on (its reach 1.9) it is held where it is - a way that
	// never enters the bare capsule is stopped by the grown one
	BladePose alongFrom = At(-20, 0, 8.5f);
	BladePose alongTo = At(20, 0, 8.5f);
	const BladeSweep flatAlong = SweepBlade(empty, alongFrom, alongTo, blade, 0.01f, nullptr, acrossLiving, 1.5f, &s);
	Check(!flatAlong.hit, "along them flat on, 1.5 off: free");
	alongFrom.rot = edge.rot;
	alongTo.rot = edge.rot;
	const BladeSweep edgeAlong = SweepBlade(empty, alongFrom, alongTo, blade, 0.01f, nullptr, acrossLiving, 1.5f, &s);
	Check(edgeAlong.hit && edgeAlong.actor == 0x2004 && Near(edgeAlong.safeFraction, 0.0f),
	      "along them edge on, its reach into their surface: held where it is");
	const BladeSweep oldAlong =
		SweepBlade(empty, alongFrom, alongTo, blade, 0.01f, nullptr, acrossLiving, 1.5f, nullptr);
	Check(!oldAlong.hit, "without a section: never entering the bare capsule, free as before");
	WallWorld wall;
	const BladeSweep walled = SweepBlade(wall, At(0, 0, 0), At(0, 0, 12), blade, 0.01f, nullptr, BladeLiving{}, 1.5f, &s);
	Check(walled.hit && walled.actor == 0 && Near(12.0f * walled.safeFraction, 10.0f - 1.5f),
	      "a wall: the margin, the section or not");

	// the turn off them: the blade 0.5 into the clear of the margin but
	// clear of its steel's reach is already off them with the section
	const BladePose near = At(0, 0, 9.0f);
	Check(DeepestInPeople(near, blade, living, 1.5f, false) > 0.0f, "0.8 units over their skin: in the margin's clear");
	Check(!(DeepestInPeople(near, blade, living, 1.5f, false, nullptr, nullptr, &s) > 0.0f),
	      "but clear of the steel's reach flat on");
}

void TestHeadInContact() {
	std::printf("A head in the contact\n");
	BodySpan line;
	line.valid = true;
	line.b = NiPoint3{0.0f, 40.0f, 0.0f};
	BodySpan head;
	head.valid = true;
	head.a = NiPoint3{0.0f, 30.0f, 0.0f};
	head.b = NiPoint3{0.0f, 40.0f, 0.0f};
	BladeSection ball;
	ball.valid = true;
	ball.round = true;
	ball.halfWidth = 4.0f;
	// a wall at z 10: the head held its reach short of it, not the margin
	WallWorld wall;
	const BladeSweep atWall =
		SweepBlade(wall, At(0, 0, 0), At(0, 0, 12), head, 0.01f, nullptr, BladeLiving{}, 1.5f, &ball, true);
	Check(atWall.hit && Near(12.0f * atWall.safeFraction, 10.0f - 4.0f - kBladeSectionSpareUnits),
	      "a head at a wall: its reach and the spare short");
	const BladeSweep lineAtWall =
		SweepBlade(wall, At(0, 0, 0), At(0, 0, 12), head, 0.01f, nullptr, BladeLiving{}, 1.5f, &ball, false);
	Check(lineAtWall.hit && Near(12.0f * lineAtWall.safeFraction, 10.0f - 1.5f), "a line's section at walls: the margin");

	// A flat head (the iron warhammer's: 13.9 wide, 4.3 thick) flat on to a
	// wall: its ray looks the most reach on (18.3), but a wall further than
	// its reach toward it is no contact - warhammer-wall.txt 2026-10-28: "the
	// blade met body" and a knock 13 units over the floor, held 0.0 units.
	BladeSection flat;
	flat.valid = true;
	flat.major = NiPoint3{1.0f, 0.0f, 0.0f};
	flat.minor = NiPoint3{0.0f, 0.0f, 1.0f};
	flat.halfWidth = 13.9f;
	flat.halfThickness = 4.3f;
	const BladeSweep farOff =
		SweepBlade(wall, At(0, 0, 0), At(0, 0, 2), head, 0.01f, nullptr, BladeLiving{}, 1.5f, &flat, true);
	Check(!farOff.hit, "a wall within the ray's look, further than the head reaches toward it: no contact");
	const BladeSweep near =
		SweepBlade(wall, At(0, 0, 0), At(0, 0, 8), head, 0.01f, nullptr, BladeLiving{}, 1.5f, &flat, true);
	Check(near.hit && Near(8.0f * near.safeFraction, 10.0f - 4.3f - kBladeSectionSpareUnits),
	      "closer than its reach: held its reach short");
	const BladeSweep lineFar = SweepBlade(wall, At(0, 0, 0), At(0, 0, 8), line, 0.01f, nullptr);
	Check(!lineFar.hit, "a line 2 units short of the wall after its way: no contact either");

	// the whole shape at the wall tip first, the line's tip and the head's
	// end at the same point (a mace pointed down at the floor): a round head
	// reaches nothing past its end, so the line's margin holds it (2026-10-28;
	// before, the head was a ball and held the tip its radius short)
	BladeContactFrame shape;
	shape.active = true;
	shape.span = line;
	shape.span2 = head;
	shape.section2 = ball;
	shape.margin2Units = 1.5f;
	shape.dtSeconds = 0.01f;
	BladePose down = At(0, 0, 0);
	BladePose downTo = At(0, 0, 0);
	// the shape along z this time: guard at 0, tip at 40 up, the wall at z 50
	down.rot.data[1][1] = 0.0f;
	down.rot.data[2][2] = 0.0f;
	down.rot.data[1][2] = -1.0f;
	down.rot.data[2][1] = 1.0f;
	downTo.rot = down.rot;
	downTo.pos = NiPoint3{0.0f, 0.0f, 12.0f};
	WallWorld high;
	high.at = 50.0f;
	const BladeSweep together = SweepShape(high, down, downTo, shape, nullptr, BladeLiving{});
	Check(together.hit && Near(12.0f * together.safeFraction, 10.0f - 1.5f), "tip first at the wall: the margin");

	// sideways, the line and the head meeting the wall at once: the span that
	// must stop soonest holds the shape - the head, its reach short - and the
	// contact says it was the head's (mace-wall.txt 2026-10-27: the line's
	// margin let the head into the floor)
	const BladeSweep side = SweepShape(wall, At(0, 0, 0), At(0, 0, 12), shape, nullptr, BladeLiving{});
	Check(side.hit && Near(12.0f * side.safeFraction, 10.0f - 4.0f - kBladeSectionSpareUnits) && side.second,
	      "sideways at the wall: held by the head's reach, the head's contact");
	const BladeSweep lineOnlyAtWall = SweepBlade(wall, At(0, 0, 0), At(0, 0, 12), line, 0.01f, nullptr);
	Check(lineOnlyAtWall.hit && !lineOnlyAtWall.second, "one span: the line's contact");

	// someone 6 under the head's line: the line clear of them by its margin,
	// the head (4 round) not
	BladeBodies person;
	person.Add(NiPoint3{-20.0f, 35.0f, 6.0f}, NiPoint3{20.0f, 35.0f, 6.0f}, 3.0f, 0x2004);
	const BladeLiving living{&person, nullptr, false, nullptr, nullptr, true};
	ShapeParts lineOnly = OneSpan(line, 1.5f);
	ShapeParts both = OneSpan(line, 1.5f);
	both.Add(head, 1.5f, &ball);
	const BladePose level = At(0, 0, 0);
	Check(!(DeepestInShape(level, lineOnly, living, false) > 0.0f), "the line alone: clear of them");
	Check(DeepestInShape(level, both, living, false) > 1.0f, "with its head: 1.1 into them");
	BladePose turned = level;
	Check(TurnOffPeople(turned, both, living), "turned off them");
	Check(!(DeepestInShape(turned, both, living, false) > kTurnOffSlackUnits), "the head clear of them after the turn");
	Check(!(ShareOffPeople(level, turned, both, living, true) < 1.0f), "the way there grazes them at most");
}

}  // namespace

int main() {
	TestSword();
	TestOffAxis();
	TestHeads();
	TestRefused();
	TestReach();
	TestHeldShort();
	TestHeadInContact();
	std::printf(g_failures == 0 ? "All weapon shape checks passed\n" : "%d weapon shape checks FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
