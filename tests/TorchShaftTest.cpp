// Checks the torch's shaft read from its vertices (game/TorchShaftLogic.h):
// the line of the largest spread, its reach along it counted only within a
// band round the line - the fire's billboard left out - a torch's length and
// slenderness, the shaft carried into the world, and when it is read again.

#include <cmath>
#include <cstdio>
#include <limits>

#include "game/TorchShaftLogic.h"

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

float Abs(float v) { return v < 0.0f ? -v : v; }

// Lights\Torch02.NIF's shaft in the hand's frame: along `u` from `from` to
// `to` units off `grip`, a ring of `radius` every two units.
void ShaftPoints(const NiPoint3& grip, const NiPoint3& u, const NiPoint3& v, const NiPoint3& w, float from, float to,
                 float radius, DiscScatter* scatter, TorchExtent* extent, const NiPoint3* centre, const NiPoint3* axis) {
	for (float along = from; along <= to + 1e-3f; along += 2.0f) {
		for (int k = 0; k < 8; ++k) {
			const float angle = static_cast<float>(k) * 0.785398f;
			const NiPoint3 p = grip + u * along + v * (radius * std::cos(angle)) + w * (radius * std::sin(angle));
			if (scatter != nullptr) {
				scatter->Add(p);
			}
			if (extent != nullptr) {
				extent->Add(p, *centre, *axis);
			}
		}
	}
}

// The fire's billboard on the torch's FlameNode1 (27 units up the shaft):
// a quad 32 wide, from 6 below to 58 above the node, facing across.
void FireQuad(const NiPoint3& grip, const NiPoint3& u, const NiPoint3& v, DiscScatter* scatter, TorchExtent* extent,
              const NiPoint3* centre, const NiPoint3* axis) {
	const NiPoint3 flame = grip + u * 27.0f;
	const NiPoint3 corners[4] = {flame + u * -6.0f + v * -16.0f, flame + u * -6.0f + v * 16.0f,
	                             flame + u * 58.0f + v * -16.0f, flame + u * 58.0f + v * 16.0f};
	for (const NiPoint3& p : corners) {
		if (scatter != nullptr) {
			scatter->Add(p);
		}
		if (extent != nullptr) {
			extent->Add(p, *centre, *axis);
		}
	}
}

struct Read {
	bool axes = false;
	NiPoint3 centre{0.0f, 0.0f, 0.0f};
	NiPoint3 axis[3];
	float spread[3] = {0.0f, 0.0f, 0.0f};
	TorchExtent extent;
	TorchShaft shaft;
	bool valid = false;
};

// Both passes over a torch, with the fire or without, as game/ShieldShape
// reads one.
Read ReadTorch(const NiPoint3& grip, const NiPoint3& u, const NiPoint3& v, const NiPoint3& w, float from, float to,
               float radius, bool fire) {
	Read r;
	DiscScatter scatter;
	ShaftPoints(grip, u, v, w, from, to, radius, &scatter, nullptr, nullptr, nullptr);
	if (fire) {
		FireQuad(grip, u, v, &scatter, nullptr, nullptr, nullptr);
	}
	r.axes = DiscAxesFrom(scatter, r.centre, r.axis, r.spread);
	if (!r.axes) {
		return r;
	}
	ShaftPoints(grip, u, v, w, from, to, radius, nullptr, &r.extent, &r.centre, &r.axis[0]);
	if (fire) {
		FireQuad(grip, u, v, nullptr, &r.extent, &r.centre, &r.axis[0]);
	}
	r.valid = TorchShaftFrom(r.centre, r.axis[0], r.spread, r.extent, r.shaft);
	return r;
}

const NiPoint3 kGrip{3.0f, -2.0f, 5.0f};
const NiPoint3 kU{0.0f, 0.6f, 0.8f};
const NiPoint3 kV{1.0f, 0.0f, 0.0f};
const NiPoint3 kW{0.0f, 0.8f, -0.6f};

// Where a point lies along the shaft from the grip.
float AlongFromGrip(const NiPoint3& p) { return DotOf3(p - kGrip, kU); }

void TestShaft() {
	std::printf("The torch's shaft from its vertices\n");
	const Read r = ReadTorch(kGrip, kU, kV, kW, -14.0f, 34.0f, 2.5f, false);
	Check(r.axes && Abs(DotOf3(r.axis[0], kU)) > 0.999f, "the line of the largest spread is the shaft");
	Check(r.valid && r.shaft.valid, "a torch's shaft");
	const float a = AlongFromGrip(r.shaft.a);
	const float b = AlongFromGrip(r.shaft.b);
	const float low = a < b ? a : b;
	const float high = a < b ? b : a;
	Check(Near(low, -14.0f, 0.1f) && Near(high, 34.0f, 0.1f), "from 14 below the grip to 34 above it");
	Check(Near(PointSegmentDistance(kGrip, r.shaft.a, r.shaft.b), 0.0f, 0.1f), "through the grip, on the line");
	Check(r.shaft.vertices == r.extent.count && r.shaft.vertices == 25u * 8u, "every vertex of the shaft counted");
}

void TestFire() {
	std::printf("The fire's billboard\n");
	const Read r = ReadTorch(kGrip, kU, kV, kW, -14.0f, 34.0f, 2.5f, true);
	Check(r.axes && Abs(DotOf3(r.axis[0], kU)) > 0.99f, "the shaft still the line of the largest spread");
	const float a = AlongFromGrip(r.shaft.a);
	const float b = AlongFromGrip(r.shaft.b);
	const float high = a < b ? b : a;
	Check(r.valid && Near(high, 34.0f, 0.5f), "its corners 16 units off the line: left out, the head where it was");
	Check(r.shaft.vertices == 25u * 8u, "only the shaft's vertices counted");
}

void TestBand() {
	std::printf("The band round the line\n");
	const NiPoint3 centre{0.0f, 0.0f, 0.0f};
	const NiPoint3 axis{0.0f, 0.0f, 1.0f};
	TorchExtent e;
	e.Add(NiPoint3{kTorchBandUnits, 0.0f, 10.0f}, centre, axis);
	Check(e.count == 1 && Near(e.high, 10.0f), "on the band's edge: counted");
	e.Add(NiPoint3{kTorchBandUnits + 0.1f, 0.0f, 40.0f}, centre, axis);
	Check(e.count == 1 && Near(e.high, 10.0f), "just past it: left out");
	e.Add(NiPoint3{0.0f, 1.0f, -20.0f}, centre, axis);
	Check(e.count == 2 && Near(e.low, -20.0f) && Near(e.high, 10.0f), "the reach both ways");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	e.Add(NiPoint3{nan, 0.0f, 50.0f}, centre, axis);
	Check(e.count == 2 && Near(e.high, 10.0f), "a vertex that is not a number: left out");
}

void TestRefused() {
	std::printf("What is no torch\n");
	Check(!ReadTorch(kGrip, kU, kV, kW, -4.0f, 8.0f, 1.0f, false).valid, "12 units long: a stick, no torch");
	Check(!ReadTorch(kGrip, kU, kV, kW, -60.0f, 80.0f, 2.5f, false).valid, "140 units long: a staff, no torch");
	Check(ReadTorch(kGrip, kU, kV, kW, -10.0f, 8.0f, 2.5f, false).valid, "18 units: the shortest kind still");
	// A disc: a shield's spread, as wide as it is long.
	Read disc;
	DiscScatter scatter;
	for (int i = -10; i <= 10; ++i) {
		for (int j = -10; j <= 10; ++j) {
			scatter.Add(NiPoint3{static_cast<float>(i) * 2.0f, static_cast<float>(j) * 2.0f, 0.0f});
		}
	}
	disc.axes = DiscAxesFrom(scatter, disc.centre, disc.axis, disc.spread);
	for (int i = -10; i <= 10; ++i) {
		disc.extent.Add(NiPoint3{0.0f, static_cast<float>(i) * 2.0f, 0.0f}, disc.centre, disc.axis[0]);
		disc.extent.Add(NiPoint3{static_cast<float>(i) * 2.0f, 0.0f, 0.0f}, disc.centre, disc.axis[0]);
	}
	Check(disc.axes && !TorchShaftFrom(disc.centre, disc.axis[0], disc.spread, disc.extent, disc.shaft) &&
	          !disc.shaft.valid,
	      "a disc's spread, wide as it is long: no shaft");
	TorchExtent few;
	for (int i = 0; i < 5; ++i) {
		few.Add(NiPoint3{0.0f, 0.0f, static_cast<float>(i) * 10.0f}, NiPoint3{0.0f, 0.0f, 0.0f},
		        NiPoint3{0.0f, 0.0f, 1.0f});
	}
	const float slender[3] = {100.0f, 1.0f, 1.0f};
	TorchShaft out;
	Check(!TorchShaftFrom(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 1.0f}, slender, few, out),
	      "five vertices in the band: too few");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float notANumber[3] = {nan, 1.0f, 1.0f};
	const Read r = ReadTorch(kGrip, kU, kV, kW, -14.0f, 34.0f, 2.5f, false);
	Check(!TorchShaftFrom(r.centre, r.axis[0], notANumber, r.extent, out), "a spread that is not a number: none");
}

void TestWorld() {
	std::printf("The shaft in the world\n");
	TorchShaft shaft;
	shaft.valid = true;
	shaft.a = NiPoint3{0.0f, -14.0f, 0.0f};
	shaft.b = NiPoint3{0.0f, 34.0f, 0.0f};
	// The hand turned a quarter about z (x to y) and 100 units up.
	NiMatrix33 rot = NiMatrix33::Identity();
	rot.data[0][0] = 0.0f;
	rot.data[0][1] = -1.0f;
	rot.data[1][0] = 1.0f;
	rot.data[1][1] = 0.0f;
	const NiPoint3 pos{10.0f, 20.0f, 100.0f};
	NiPoint3 a;
	NiPoint3 b;
	Check(TorchShaftInWorld(shaft, rot, pos, a, b), "a shaft read: in the world");
	Check(Near(a.x, 24.0f) && Near(a.y, 20.0f) && Near(a.z, 100.0f) && Near(b.x, -24.0f) && Near(b.y, 20.0f),
	      "carried by the hand's turn and place");
	TorchShaft none;
	Check(!TorchShaftInWorld(none, rot, pos, a, b), "none read: none in the world");
}

void TestStale() {
	std::printf("When the shaft is read again\n");
	Check(TorchShaftStale(false, 0x100, 0x100, 0x200, 0x200, 0), "none read yet: read");
	Check(!TorchShaftStale(true, 0x100, 0x100, 0x200, 0x200, 0), "the same node and torch: kept");
	Check(TorchShaftStale(true, 0x100, 0x104, 0x200, 0x200, 0), "another node: read");
	Check(TorchShaftStale(true, 0x100, 0x100, 0x200, 0x204, 0), "another torch: read");
	Check(!TorchShaftStale(true, 0x100, 0x100, 0x200, 0x200, kTorchShaftReadFrames - 1), "kept a while");
	Check(TorchShaftStale(true, 0x100, 0x100, 0x200, 0x200, kTorchShaftReadFrames), "and read again every so often");
}

}  // namespace

int main() {
	TestShaft();
	TestFire();
	TestBand();
	TestRefused();
	TestWorld();
	TestStale();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
