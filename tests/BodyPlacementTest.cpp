// Checks the visible body's arithmetic: where the third-person root has to
// stand for the head bone's eyes to land on the headset, written under the
// root's parent; the camera taken back from the first eye to the head; and
// the gate that says when the body is shown at all.
//
// Every check is convention-free where it can be: a placement is verified by
// carrying it forward again (parent * local, head from root, eyes from head)
// and asking whether it lands on the camera, rather than by comparing with a
// number worked out under the same assumptions.

#include <cstdio>

#include "core/Rotation.h"
#include "game/BodyPlacement.h"

using obvr::NiMatrix33;
using obvr::NiPoint3;
using obvr::game::BodyRootInput;
using obvr::game::BodyRootLocalPos;
using obvr::game::BodyRootWorldWanted;
using obvr::game::CyclopeanCamera;
using obvr::game::VisibleBodyWanted;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.001f) { return a - b < eps && b - a < eps; }

bool NearPoint(const NiPoint3& a, const NiPoint3& b, float eps = 0.001f) {
	return Near(a.x, b.x, eps) && Near(a.y, b.y, eps) && Near(a.z, b.z, eps);
}

// The root as the engine would have it after the local position is written:
// parent * local, scale included.
NiPoint3 RootWorldFromLocal(const BodyRootInput& in, const NiPoint3& local) {
	const float scale = in.parentScale > 0.0f ? in.parentScale : 1.0f;
	return in.parentPos + in.parentRot * (local * scale);
}

// Where the eyes end up once the root stands at rootWorld: the head keeps
// its offset from the root, the eyes their offset from the head.
NiPoint3 EyesFromRoot(const BodyRootInput& in, const NiPoint3& rootWorld) {
	const float scale = in.rootWorldScale > 0.0f ? in.rootWorldScale : 1.0f;
	const NiPoint3 headFromRoot = in.headWorld - in.rootWorld;
	return rootWorld + headFromRoot + in.rootWorldRot * (in.eyeOffsetUnits * scale);
}

BodyRootInput Standing() {
	BodyRootInput in;
	in.cameraWorld = NiPoint3{100.0f, 200.0f, 130.0f};
	in.rootWorld = NiPoint3{90.0f, 210.0f, 0.0f};
	in.headWorld = NiPoint3{90.0f, 212.0f, 60.0f};
	return in;
}

void TestUprightBody() {
	std::printf("An upright body under the headset\n");
	const BodyRootInput in = Standing();
	const NiPoint3 wanted = BodyRootWorldWanted(in);
	Check(NearPoint(wanted, NiPoint3{100.0f, 200.0f - 14.0f - 2.0f, 130.0f - 6.0f - 60.0f}),
	      "the root stands the head's offset and the eye offset back from the camera");
	Check(NearPoint(EyesFromRoot(in, wanted), in.cameraWorld),
	      "carried forward, the eyes land on the camera");

	const NiPoint3 local = BodyRootLocalPos(in);
	Check(NearPoint(local, wanted), "with an identity parent the local position is the world one");
	Check(NearPoint(EyesFromRoot(in, RootWorldFromLocal(in, local)), in.cameraWorld),
	      "and parent * local puts the eyes on the camera");
}

void TestTurnedBody() {
	std::printf("A body turned about the vertical\n");
	BodyRootInput in = Standing();
	in.rootWorldRot = obvr::EulerToMatrix(0.0f, 0.0f, 90.0f);
	const NiPoint3 wanted = BodyRootWorldWanted(in);
	Check(NearPoint(EyesFromRoot(in, wanted), in.cameraWorld),
	      "the eye offset is carried through the root's rotation");
	const NiPoint3 sideways = in.rootWorldRot * NiPoint3{0.0f, 14.0f, 6.0f};
	Check(Near(sideways.z, 6.0f) && Near(sideways.y, 0.0f, 0.01f) && Near(sideways.x * sideways.x, 196.0f, 0.5f),
	      "a quarter turn carries the forward part sideways and leaves the up part");

	in.rootWorldRot = obvr::EulerToMatrix(30.0f, 0.0f, 200.0f);
	Check(NearPoint(EyesFromRoot(in, BodyRootWorldWanted(in)), in.cameraWorld),
	      "any rotation: the eyes still land on the camera");
}

void TestScaledBody() {
	std::printf("A scaled body\n");
	BodyRootInput in = Standing();
	in.rootWorldScale = 2.0f;
	in.headWorld = in.rootWorld + NiPoint3{0.0f, 4.0f, 120.0f};
	const NiPoint3 wanted = BodyRootWorldWanted(in);
	Check(NearPoint(wanted, NiPoint3{100.0f, 200.0f - 28.0f - 4.0f, 130.0f - 12.0f - 120.0f}),
	      "the eye offset grows with the root's scale");
	Check(NearPoint(EyesFromRoot(in, wanted), in.cameraWorld), "and the eyes land on the camera");

	in.rootWorldScale = 0.0f;
	Check(NearPoint(BodyRootWorldWanted(in),
	                NiPoint3{100.0f, 200.0f - 14.0f - 4.0f, 130.0f - 6.0f - 120.0f}),
	      "a zero root scale is taken as one rather than collapsing the offset");
	in.rootWorldScale = -1.0f;
	Check(NearPoint(BodyRootWorldWanted(in),
	                NiPoint3{100.0f, 200.0f - 14.0f - 4.0f, 130.0f - 6.0f - 120.0f}),
	      "so is a negative one");
}

void TestParent() {
	std::printf("A root under a moved, turned and scaled parent\n");
	BodyRootInput in = Standing();
	in.parentPos = NiPoint3{-40.0f, 15.0f, 8.0f};
	in.parentRot = obvr::EulerToMatrix(10.0f, -5.0f, 120.0f);
	in.parentScale = 2.0f;
	const NiPoint3 local = BodyRootLocalPos(in);
	const NiPoint3 wanted = BodyRootWorldWanted(in);
	Check(NearPoint(RootWorldFromLocal(in, local), wanted),
	      "parent * local reproduces the wanted world position");
	Check(NearPoint(EyesFromRoot(in, RootWorldFromLocal(in, local)), in.cameraWorld),
	      "and the eyes land on the camera through the parent");

	in.parentScale = 0.0f;
	const NiPoint3 localNoScale = BodyRootLocalPos(in);
	Check(NearPoint(RootWorldFromLocal(in, localNoScale), wanted),
	      "a zero parent scale is refused as one rather than dividing by it");
}

void TestAbsolutePlacementDoesNotDrift() {
	std::printf("Writing the placement twice writes the same number twice\n");
	BodyRootInput in = Standing();
	in.rootWorldRot = obvr::EulerToMatrix(0.0f, 0.0f, 45.0f);
	const NiPoint3 first = BodyRootLocalPos(in);

	// The engine now has the root where it was put, and the head with it.
	const NiPoint3 headFromRoot = in.headWorld - in.rootWorld;
	in.rootWorld = RootWorldFromLocal(in, first);
	in.headWorld = in.rootWorld + headFromRoot;
	const NiPoint3 second = BodyRootLocalPos(in);
	Check(NearPoint(first, second), "the second frame's placement equals the first's");

	// And a lean carries the body with the camera by exactly the lean.
	in.cameraWorld = in.cameraWorld + NiPoint3{5.0f, -3.0f, -10.0f};
	const NiPoint3 third = BodyRootLocalPos(in);
	Check(NearPoint(third - second, NiPoint3{5.0f, -3.0f, -10.0f}),
	      "a lean of the camera moves the root by the same amount");
}

void TestCyclopeanCamera() {
	std::printf("The camera between the eyes\n");
	const NiPoint3 firstEye{10.0f, 20.0f, 30.0f};
	Check(NearPoint(CyclopeanCamera(firstEye, NiPoint3{0.0f, 0.0f, 0.0f}), firstEye),
	      "with no stereo step the node is the camera");
	Check(NearPoint(CyclopeanCamera(firstEye, NiPoint3{-2.2f, 0.4f, 0.0f}),
	                NiPoint3{12.2f, 19.6f, 30.0f}),
	      "the first eye's step is taken back out");
}

void TestGate() {
	std::printf("When the body is shown\n");
	for (int bits = 0; bits < 8; ++bits) {
		const bool enabled = (bits & 1) != 0;
		const bool third = (bits & 2) != 0;
		const bool menu = (bits & 4) != 0;
		if (VisibleBodyWanted(enabled, third, menu) != (enabled && !third && !menu)) {
			Check(false, "one of the eight gate combinations disagrees");
			return;
		}
	}
	Check(true, "all eight gate combinations agree: on, first person, no menu");
}

}  // namespace

int main() {
	TestUprightBody();
	std::printf("\n");
	TestTurnedBody();
	std::printf("\n");
	TestScaledBody();
	std::printf("\n");
	TestParent();
	std::printf("\n");
	TestAbsolutePlacementDoesNotDrift();
	std::printf("\n");
	TestCyclopeanCamera();
	std::printf("\n");
	TestGate();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}
	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
