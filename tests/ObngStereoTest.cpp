// Checks when OBVR renders the world in one pass through OBNG's stereo draw
// route instead of its own two passes (render/ObngStereoLogic.h): only with
// the setting on, OBNG.dll's three functions bound, its plan "draw twice"
// and the eye data taken; every other case keeps the dual pass of today.

#include <cstdio>
#include <cstring>

#include "render/ObngStereo.h"
#include "render/ObngStereoLogic.h"

using namespace obvr::render;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

ObngStereoState Ready() {
	ObngStereoState s;
	s.enabled = true;
	s.bound = true;
	s.plan = kObngPlanDrawTwice;
	s.eyesTaken = true;
	s.sideBySide = true;
	s.firstIsLeft = true;
	return s;
}

void TestPath() {
	std::printf("Which path renders the world\n");
	Check(StereoPathFor(Ready()) == StereoPath::SinglePass, "all four given: one pass through OBNG");
	ObngStereoState off = Ready();
	off.enabled = false;
	Check(StereoPathFor(off) == StereoPath::DualPass, "the setting off: the dual pass");
	ObngStereoState unbound = Ready();
	unbound.bound = false;
	Check(StereoPathFor(unbound) == StereoPath::DualPass, "OBNG.dll absent or a function missing: the dual pass");
	ObngStereoState oneEye = Ready();
	oneEye.plan = 1;
	Check(StereoPathFor(oneEye) == StereoPath::DualPass, "OBNG's plan one eye (the 1:1 gate): the dual pass");
	ObngStereoState routeOff = Ready();
	routeOff.plan = 0;
	Check(StereoPathFor(routeOff) == StereoPath::DualPass, "OBNG's route off: the dual pass");
	ObngStereoState unknown = Ready();
	unknown.plan = 7;
	Check(StereoPathFor(unknown) == StereoPath::DualPass, "a plan OBVR does not know: the dual pass");
	ObngStereoState refused = Ready();
	refused.eyesTaken = false;
	Check(StereoPathFor(refused) == StereoPath::DualPass, "the eye data refused: the dual pass");
	ObngStereoState narrow = Ready();
	narrow.sideBySide = false;
	Check(StereoPathFor(narrow) == StereoPath::DualPass, "a frame one eye wide: the dual pass");
	ObngStereoState swapped = Ready();
	swapped.firstIsLeft = false;
	Check(StereoPathFor(swapped) == StereoPath::DualPass, "SwapEyeOrder (the right eye first): the dual pass");

	std::printf("Whether the eye data is worth handing over\n");
	ObngStereoState before = Ready();
	before.eyesTaken = false;
	Check(WorthSettingEyes(before), "everything but the answer: hand it over");
	ObngStereoState noPlan = before;
	noPlan.plan = 1;
	Check(!WorthSettingEyes(noPlan), "plan one eye: not handed over");
	ObngStereoState noWide = before;
	noWide.sideBySide = false;
	Check(!WorthSettingEyes(noWide), "a narrow frame: not handed over");
	ObngStereoState noLeft = before;
	noLeft.firstIsLeft = false;
	Check(!WorthSettingEyes(noLeft), "the right eye first: not handed over");
	ObngStereoState unboundBefore = before;
	unboundBefore.bound = false;
	Check(!WorthSettingEyes(unboundBefore), "OBNG not bound: not handed over");
	ObngStereoState offBefore = before;
	offBefore.enabled = false;
	Check(!WorthSettingEyes(offBefore), "the setting off: not handed over");
}

void TestEyes() {
	std::printf("The eye data for OBNG (step 4)\n");
	const float offset[3] = {4.5f, -0.25f, 0.0f};
	const ObngEyes eyes = EyesFor(5136, 2568, offset);
	Check(eyes.size == 68 && eyes.version == 1, "size 68, version 1");
	Check(eyes.secondEyeOffset[0] == 4.5f && eyes.secondEyeOffset[1] == -0.25f &&
	          eyes.secondEyeOffset[2] == 0.0f,
	      "the offset as given");
	Check(eyes.viewport[0].x == 0 && eyes.viewport[0].y == 0 && eyes.viewport[0].width == 2568 &&
	          eyes.viewport[0].height == 2568,
	      "eye 0: the left half, what the half port gives the engine");
	Check(eyes.viewport[1].x == 2568 && eyes.viewport[1].y == 0 && eyes.viewport[1].width == 2568 &&
	          eyes.viewport[1].height == 2568,
	      "eye 1: the right half");
	Check(eyes.viewport[0].minZ == 0.0f && eyes.viewport[0].maxZ == 1.0f && eyes.viewport[1].minZ == 0.0f &&
	          eyes.viewport[1].maxZ == 1.0f,
	      "both the full depth range");
	const ObngEyes odd = EyesFor(5137, 2568, offset);
	Check(odd.viewport[1].x == 2568 && odd.viewport[1].width == 2568, "an odd width: two equal halves");
}

// A rectangle in D3D's RECT order, as d3d9::Rect is.
struct d3d9Like {
	int left;
	int top;
	int right;
	int bottom;
};

void TestEyeCrops() {
	std::printf("Where each eye's crop reads (step 5)\n");
	Check(EyeSourceOffset(1, true, 2568) == 2568, "single pass, the right eye: the right half");
	Check(EyeSourceOffset(0, true, 2568) == 0, "single pass, the left eye: the left half");
	Check(EyeSourceOffset(1, false, 2568) == 0, "dual pass, the right eye: the left half (its own pass)");
	Check(EyeSourceOffset(0, false, 2568) == 0, "dual pass, the left eye: the left half");
	const d3d9Like crop{100, 20, 2468, 2548};
	const d3d9Like moved = ShiftedCrop(crop, EyeSourceOffset(1, true, 2568));
	Check(moved.left == 2668 && moved.right == 5036 && moved.top == 20 && moved.bottom == 2548,
	      "the right eye's crop moved by half the frame, its rows kept");
	const d3d9Like kept = ShiftedCrop(crop, 0);
	Check(kept.left == 100 && kept.right == 2468, "no offset: the crop as it was");
}

void TestNames() {
	std::printf("The setting's and the result's names for the log\n");
	Check(std::strcmp(ObngStereoResultName(0), "ok") == 0, "0: ok");
	Check(std::strcmp(ObngStereoResultName(6), "the route is off or has one eye") == 0, "6: the route off");
	Check(std::strcmp(ObngStereoResultName(99), "an unknown answer") == 0, "anything else: unknown");
	Check(std::strcmp(StereoPathName(StereoPath::SinglePass), "single pass through OBNG") == 0, "the single pass");
	Check(std::strcmp(StereoPathName(StereoPath::DualPass), "dual pass") == 0, "the dual pass");
}

void TestBackBuffer() {
	std::printf("The back buffer for an eye's size (step 2)\n");
	const FrameSize eye{3200, 3200};

	const BackBufferPlan off = BackBufferForEye(eye, false);
	Check(off.action == SideBySideAction::Off, "the setting off: Off");
	Check(off.frame.width == 3200 && off.frame.height == 3200, "the setting off: the frame is the eye");

	const BackBufferPlan wide = BackBufferForEye(eye, true);
	Check(wide.action == SideBySideAction::SideBySide, "the setting on: side by side");
	Check(wide.frame.width == 6400 && wide.frame.height == 3200, "side by side: twice the width, the same height");
	Check(wide.eye.width == 3200 && wide.eye.height == 3200, "side by side: the 2D's basis stays one eye");

	const BackBufferPlan noWidth = BackBufferForEye(FrameSize{0, 3200}, true);
	Check(noWidth.action == SideBySideAction::NoEyeSize, "no eye width: nothing doubled");
	Check(noWidth.frame.width == 0 && noWidth.frame.height == 3200, "no eye width: the size as given");
	const BackBufferPlan noHeight = BackBufferForEye(FrameSize{3200, 0}, true);
	Check(noHeight.action == SideBySideAction::NoEyeSize, "no eye height: nothing doubled");

	const BackBufferPlan edge = BackBufferForEye(FrameSize{kMaxFrameDimension / 2, 4000}, true);
	Check(edge.action == SideBySideAction::SideBySide && edge.frame.width == kMaxFrameDimension,
	      "an eye of half the limit: exactly the limit");
	const BackBufferPlan tooWide = BackBufferForEye(FrameSize{kMaxFrameDimension / 2 + 1, 4000}, true);
	Check(tooWide.action == SideBySideAction::TooWide, "past the limit: refused");
	Check(tooWide.frame.width == kMaxFrameDimension / 2 + 1, "past the limit: the frame stays one eye");
	Check(BackBufferForEye(FrameSize{4000, kMaxFrameDimension + 1}, true).action == SideBySideAction::TooWide,
	      "a height past the limit: refused");

	Check(std::strcmp(SideBySideActionName(SideBySideAction::SideBySide), "side by side") == 0, "name: side by side");
	Check(std::strcmp(SideBySideActionName(SideBySideAction::TooWide),
	                  "refused, twice the eye is past the 16384 limit") == 0, "name: too wide");
	Check(std::strcmp(SideBySideActionName(SideBySideAction::NoEyeSize), "no eye size to double") == 0, "name: no size");
	Check(std::strcmp(SideBySideActionName(SideBySideAction::Off), "off") == 0, "name: off");
}

void TestWorldPort() {
	std::printf("The world camera's port and the world picture (step 3)\n");
	Check(SamePort(WorldPortFor(false), kFullPort), "one eye wide: the full port");
	const CameraPort left = WorldPortFor(true);
	Check(left.left == 0.0f && left.right == 0.5f && left.top == 1.0f && left.bottom == 0.0f,
	      "side by side: the left half, full height");

	Check(DecideWorldPort(false, kFullPort) == WorldPortAction::LeaveAlone,
	      "one eye wide: the port left alone");
	Check(DecideWorldPort(true, kFullPort) == WorldPortAction::SetLeftHalf,
	      "side by side and the engine's full port: the left half");
	Check(DecideWorldPort(true, WorldPortFor(true)) == WorldPortAction::AlreadyLeftHalf,
	      "side by side and already the left half: nothing to write");
	const CameraPort scaled{0.0f, 0.75f, 0.75f, 0.0f};
	Check(DecideWorldPort(true, scaled) == WorldPortAction::Unexpected,
	      "a port the engine scaled (or a wrong offset): not written");
	Check(DecideWorldPort(false, scaled) == WorldPortAction::LeaveAlone,
	      "one eye wide and any port: left alone");

	Check(WorldPictureWidth(5136, true) == 2568, "side by side: the world picture is half the frame");
	Check(WorldPictureWidth(2568, false) == 2568, "one eye wide: the world picture is the frame");
	Check(WorldPictureWidth(5137, true) == 2568, "an odd width: the left half rounded down");

	Check(std::strcmp(WorldPortActionName(WorldPortAction::SetLeftHalf), "set to the left half") == 0,
	      "name: set");
	Check(std::strcmp(WorldPortActionName(WorldPortAction::Unexpected),
	                  "not the engine's full port, left alone") == 0, "name: unexpected");
	Check(std::strcmp(WorldPortActionName(WorldPortAction::AlreadyLeftHalf), "already the left half") == 0,
	      "name: already");
	Check(std::strcmp(WorldPortActionName(WorldPortAction::LeaveAlone), "left alone") == 0, "name: alone");
}

}  // namespace

int main() {
	TestPath();
	TestNames();
	TestBackBuffer();
	TestWorldPort();
	TestEyes();
	TestEyeCrops();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
