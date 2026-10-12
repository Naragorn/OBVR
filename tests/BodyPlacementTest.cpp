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

void TestEyesFollowHead() {
	std::printf("The eyes in front of the head the way the head faces\n");
	// The torso faces +y; the head looks 40 degrees to the left of it.
	BodyRootInput in = Standing();
	in.eyesFollowHead = true;
	in.headRot = obvr::EulerToMatrix(0.0f, 0.0f, 40.0f);
	const NiPoint3 wanted = BodyRootWorldWanted(in);
	const NiPoint3 eyes = wanted + (in.headWorld - in.rootWorld) + in.headRot * in.eyeOffsetUnits;
	Check(NearPoint(eyes, in.cameraWorld), "the eyes land on the camera along the head's yaw");
	const NiPoint3 alongBody = BodyRootWorldWanted(Standing());
	Check(!NearPoint(wanted, alongBody, 0.5f), "and not where the body's facing would put them");
	in.headRot = NiMatrix33::Identity();
	Check(NearPoint(BodyRootWorldWanted(in), alongBody), "the head facing the body's way: the same place");
	// The neck's pivot, with the eyes where the game has them (5.6 forward,
	// 6 up from the head bone): the head looking 60 degrees down. The eyes
	// swing down and a little forward about the head bone (the 6 up swings
	// forward more than the 5.6 forward swings back), so the root stands
	// higher and a little further back than with the head level - the body
	// does not follow the headset down, the camera orbits the neck.
	in.eyeOffsetUnits = NiPoint3{0.0f, 5.6f, 6.0f};
	const NiPoint3 levelRoot = BodyRootWorldWanted(in);
	in.headRot = obvr::EulerToMatrix(-60.0f, 0.0f, 0.0f);
	const NiPoint3 forward = obvr::ForwardOf(in.headRot);
	Check(forward.z < -0.8f && forward.y > 0.4f, "(the set-up: that rotation looks down ahead)");
	const NiPoint3 down = BodyRootWorldWanted(in);
	Check(NearPoint(down + (in.headWorld - in.rootWorld) + in.headRot * in.eyeOffsetUnits, in.cameraWorld),
	      "looking down: the eyes still land on the camera");
	Check(down.z > levelRoot.z + 7.0f, "the root stands 7 units higher: the eyes went down about the neck, the body did not");
	Check(down.y < levelRoot.y - 2.0f, "and 2 further back: the eyes went forward about it");
	// Looking up: the eyes swing back (and a little up) about the neck - the
	// root forward, a little lower.
	in.headRot = obvr::EulerToMatrix(60.0f, 0.0f, 0.0f);
	const NiPoint3 up = BodyRootWorldWanted(in);
	Check(up.y > levelRoot.y + 7.0f && up.z < levelRoot.z - 1.0f, "looking up: the root 7 forward, a little lower");
	in.eyeOffsetUnits = NiPoint3{0.0f, 14.0f, 6.0f};
	// Off: the root's rotation carries the offset, whatever the head does.
	in.eyesFollowHead = false;
	in.headRot = obvr::EulerToMatrix(-60.0f, 0.0f, 70.0f);
	Check(NearPoint(BodyRootWorldWanted(in), alongBody), "off: the head's rotation is not read");
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

// The body back under a bowed head (the tester, 2026-10-12: "nach unten
// schauen: ich sehe immernoch in den körper rein").
void TestLookDownBack() {
	std::printf("The body back under a bowed head\n");
	using obvr::game::BodyLookDownShare;
	using obvr::game::BodyLookDownShift;
	Check(BodyLookDownShare(NiMatrix33::Identity()) == 0.0f, "level: nothing");
	Check(BodyLookDownShare(obvr::EulerToMatrix(-15.0f, 0.0f, 0.0f)) == 0.0f, "15 degrees down: nothing yet");
	// The square of the way from 20 to 90 degrees down: 8.3 of 20 at 65, 17.2
	// at 85 - what the harness's eye dumps needed (8 hid the collar at 65,
	// 16 at 85).
	const float at65 = BodyLookDownShare(obvr::EulerToMatrix(-65.0f, 0.0f, 0.0f));
	Check(Near(at65 * 20.0f, 8.27f, 0.05f), "65 degrees down: 8.3 of 20");
	const float at85 = BodyLookDownShare(obvr::EulerToMatrix(-85.0f, 0.0f, 0.0f));
	Check(Near(at85 * 20.0f, 17.24f, 0.05f), "85 degrees down: 17.2 of 20");
	const float early = BodyLookDownShare(obvr::EulerToMatrix(-25.0f, 0.0f, 0.0f));
	Check(early > 0.0f && early < 0.01f, "just past the start: a little, eased in");
	Check(Near(BodyLookDownShare(obvr::EulerToMatrix(-90.0f, 0.0f, 0.0f)), 1.0f, 0.001f), "straight down: all of it");
	Check(BodyLookDownShare(obvr::EulerToMatrix(60.0f, 0.0f, 0.0f)) == 0.0f, "looking up: nothing");
	NiMatrix33 broken = NiMatrix33::Identity();
	broken.data[2][1] = -7.0f;
	Check(BodyLookDownShare(broken) == 0.0f, "a rotation that is none: nothing");
	// The shift: along the way the face looks, level - the body that far behind.
	const NiMatrix33 bowedAhead = obvr::EulerToMatrix(-80.0f, 0.0f, 0.0f);
	const NiPoint3 forward = obvr::ForwardOf(bowedAhead);
	const float shareAt80 = BodyLookDownShare(bowedAhead);
	const NiPoint3 shift = BodyLookDownShift(bowedAhead, 8.0f);
	const float shiftLength = 8.0f * shareAt80;
	Check(Near(shift.z, 0.0f) && Near(shift.x * shift.x + shift.y * shift.y, shiftLength * shiftLength, 0.01f) &&
	          shift.x * forward.x + shift.y * forward.y > 0.0f,
	      "80 degrees down: the share of 8 units along the way the face looks, level");
	const NiMatrix33 bowedTurned = obvr::EulerToMatrix(-80.0f, 0.0f, 90.0f);
	const NiPoint3 turnedForward = obvr::ForwardOf(bowedTurned);
	const NiPoint3 turnedShift = BodyLookDownShift(bowedTurned, 8.0f);
	Check(turnedShift.x * turnedForward.x + turnedShift.y * turnedForward.y > shiftLength * 0.17f &&
	          Near(turnedShift.x * turnedShift.x + turnedShift.y * turnedShift.y, shiftLength * shiftLength, 0.01f),
	      "turned 90 degrees: the shift turned with the face");
	Check(NearPoint(BodyLookDownShift(bowedAhead, 0.0f), NiPoint3{0.0f, 0.0f, 0.0f}) &&
	          NearPoint(BodyLookDownShift(bowedAhead, -3.0f), NiPoint3{0.0f, 0.0f, 0.0f}),
	      "no units, or fewer than none: no shift");
	Check(NearPoint(BodyLookDownShift(NiMatrix33::Identity(), 8.0f), NiPoint3{0.0f, 0.0f, 0.0f}), "level: no shift");
	// How far at most, from the camera's near plane: twice it.
	using obvr::game::LookDownBackUnitsFor;
	Check(Near(LookDownBackUnitsFor(10.0f), 20.0f), "the game's near plane of 10: the 20 measured");
	Check(Near(LookDownBackUnitsFor(4.0f), 8.0f), "a near plane of 4 ([Body] NearPlane's default): 8");
	Check(Near(LookDownBackUnitsFor(4.0f) * at85, 6.9f, 0.05f) && Near(LookDownBackUnitsFor(4.0f) * at65, 3.3f, 0.05f),
	      "with it: 6.9 at 85 degrees down, 3.3 at 65");
	Check(Near(LookDownBackUnitsFor(0.0f), 20.0f) && Near(LookDownBackUnitsFor(-2.0f), 20.0f),
	      "no near plane known, or none: the 20");
	const float nan = 0.0f / 0.0f * 0.0f;
	Check(Near(LookDownBackUnitsFor(nan), 20.0f) && Near(LookDownBackUnitsFor(1.0e6f), 20.0f),
	      "not a number, or absurd: the 20");
	// In the placement: the root that far behind, the eyes still on the camera
	// less the shift; only with the head's rotation followed.
	BodyRootInput in = Standing();
	in.eyesFollowHead = true;
	in.eyeOffsetUnits = NiPoint3{0.0f, 5.6f, 6.0f};
	in.headRot = bowedAhead;
	const NiPoint3 without = BodyRootWorldWanted(in);
	in.lookDownBackUnits = 8.0f;
	const NiPoint3 with = BodyRootWorldWanted(in);
	Check(NearPoint(without - with, shift), "LookDownBack 8 at 80 degrees down: the root that share of 8 units behind");
	in.rootWorldScale = 2.0f;
	const NiPoint3 scaledWithout = [&] {
		BodyRootInput s = in;
		s.lookDownBackUnits = 0.0f;
		return BodyRootWorldWanted(s);
	}();
	Check(NearPoint(scaledWithout - BodyRootWorldWanted(in), shift * 2.0f), "a body at scale two: twice as far");
	in.rootWorldScale = 1.0f;
	in.eyesFollowHead = false;
	const NiPoint3 offWith = BodyRootWorldWanted(in);
	in.lookDownBackUnits = 0.0f;
	Check(NearPoint(offWith, BodyRootWorldWanted(in)), "the head's rotation not followed: no shift");
}

}  // namespace

int main() {
	TestUprightBody();
	std::printf("\n");
	TestLookDownBack();
	std::printf("\n");
	TestTurnedBody();
	std::printf("\n");
	TestEyesFollowHead();
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
