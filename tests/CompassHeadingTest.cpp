// Checks the compass's view heading (game/CompassHeading.h).

#include <cmath>
#include <cstdio>

#include "game/CompassHeading.h"

using namespace obvr;
using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

constexpr float kDegrees = 3.14159265f / 180.0f;

bool NearDegrees(float radians, float degrees) { return std::fabs(radians / kDegrees - degrees) < 0.05f; }

// A camera turned `heading` from north towards east and pitched `pitch` up:
// forward in column 1, up in column 2, right in column 0.
NiMatrix33 Camera(float headingDegrees, float pitchDegrees) {
	const float h = headingDegrees * kDegrees;
	const float p = pitchDegrees * kDegrees;
	const float sh = std::sin(h), ch = std::cos(h), sp = std::sin(p), cp = std::cos(p);
	NiMatrix33 m = NiMatrix33::Identity();
	const float forward[3] = {sh * cp, ch * cp, sp};
	const float up[3] = {-sh * sp, -ch * sp, cp};
	const float right[3] = {ch, -sh, 0.0f};
	for (int i = 0; i < 3; ++i) {
		m.data[i][0] = right[i];
		m.data[i][1] = forward[i];
		m.data[i][2] = up[i];
	}
	return m;
}

void TestViewHeading() {
	std::printf("The compass's view heading\n");
	float r = -1.0f;
	Check(ViewHeadingOf(Camera(0.0f, 0.0f), r) && NearDegrees(r, 0.0f), "level, north: 0");
	Check(ViewHeadingOf(Camera(30.0f, 0.0f), r) && NearDegrees(r, 30.0f), "level, 30 towards east: 30");
	Check(ViewHeadingOf(Camera(300.0f, 0.0f), r) && NearDegrees(r, 300.0f), "west of north: in 0..360, 300");
	Check(ViewHeadingOf(Camera(120.0f, 60.0f), r) && NearDegrees(r, 120.0f), "looking up 60: the same heading");
	Check(ViewHeadingOf(Camera(120.0f, 90.0f), r) && NearDegrees(r, 120.0f),
	      "looking straight up: still the heading, from the up axis");
	Check(ViewHeadingOf(Camera(200.0f, -90.0f), r) && NearDegrees(r, 200.0f),
	      "looking straight down: still the heading");
	Check(ViewHeadingOf(Camera(200.0f, -45.0f), r) && NearDegrees(r, 200.0f), "looking down 45: the same heading");
	NiMatrix33 broken = NiMatrix33::Identity();
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			broken.data[i][j] = 0.0f;
		}
	}
	r = 7.0f;
	Check(!ViewHeadingOf(broken, r) && r == 7.0f, "a broken matrix: no heading, nothing written");
}

}  // namespace

int main() {
	TestViewHeading();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
