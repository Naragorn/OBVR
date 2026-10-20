// Checks game/FirstPersonArmsLogic.h: the first-person skeleton's root is
// left alone under the horse's ActorParent bone (the view sliding sideways
// on a horse, 2026-10-19).

#include <cstdio>

#include "core/Rotation.h"
#include "game/FirstPersonArmsLogic.h"

using namespace obvr::game;
using obvr::EulerToMatrix;
using obvr::NiMatrix33;
using obvr::NiPoint3;

int main() {
	int failures = 0;
	auto check = [&](bool condition, const char* what) {
		std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
		if (!condition) {
			++failures;
		}
	};
	auto near = [](float a, float b) { return a - b < 0.001f && b - a < 0.001f; };
	auto nearPoint = [&](const NiPoint3& a, const NiPoint3& b) {
		return near(a.x, b.x) && near(a.y, b.y) && near(a.z, b.z);
	};
	auto nearMatrix = [&](const NiMatrix33& a, const NiMatrix33& b) {
		for (int r = 0; r < 3; ++r) {
			for (int c = 0; c < 3; ++c) {
				if (!near(a.data[r][c], b.data[r][c])) {
					return false;
				}
			}
		}
		return true;
	};

	// The root's rotation with the left-handed mirror.
	const NiMatrix33 rotation = EulerToMatrix(0.0f, 10.0f, 30.0f);  // the hands' turn, in the parent's frame
	const NiMatrix33 base = EulerToMatrix(90.0f, 0.0f, -5.0f);      // what the engine put there
	check(nearMatrix(MirroredRootRotation(rotation, base, false), rotation * base),
	      "not mirrored: the hands' turn on the engine's base, as before");
	const NiMatrix33 mirrored = MirroredRootRotation(rotation, base, true);
	check(near(obvr::Determinant(mirrored), -1.0f), "mirrored: the root turned over");
	const NiPoint3 q{1.0f, 2.0f, 3.0f};
	const NiPoint3 baseQ = base * q;
	check(nearPoint(mirrored * q, rotation * NiPoint3{-baseQ.x, baseQ.y, baseQ.z}),
	      "a point of the skeleton: the base's placement with x to -x in the parent's frame, then the turn");
	check(nearMatrix(MirroredRootRotation(mirrored, obvr::MirrorX(), false) * obvr::MirrorX(), mirrored),
	      "the mirror is its own inverse");

	check(ArmsRootUnderHorse("ActorParent"), "under the horse's ActorParent: left alone");
	check(!ArmsRootUnderHorse("ActorParentX"), "a longer name is another node");
	check(!ArmsRootUnderHorse("ActorParen"), "a shorter name is another node");
	check(!ArmsRootUnderHorse("Camera01"), "under the camera (on foot): placed");
	check(!ArmsRootUnderHorse(""), "an unnamed parent: placed");
	check(!ArmsRootUnderHorse(nullptr), "no parent name: placed");
	if (failures != 0) {
		std::printf("%d check(s) failed\n", failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
