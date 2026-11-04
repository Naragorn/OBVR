// Checks when OBVR renders the world in one pass through OBNG's stereo draw
// route instead of its own two passes (render/ObngStereoLogic.h): only with
// the setting on, OBNG.dll's three functions bound, its plan "draw twice"
// and the eye data taken; every other case keeps the dual pass of today.

#include <cstdio>
#include <cstring>

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
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
