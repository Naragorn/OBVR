// Checks the hand-over of a heading written outside the camera pass's own
// turns (camera/BodyYawHandOver.h): the step's sign and wrap, when the view
// is kept, the booking alone and on top of a step already waiting, and that
// the camera's remaining head turn is unchanged by a booked turn.

#include <cmath>
#include <cstdio>

#include "camera/BodyYawHandOver.h"

using namespace obvr::camera;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) < eps; }

constexpr float kDegree = 0.01745329252f;

// FrameLogic.h's PlayerYawForGaze without the 0..2pi fold: the heading the
// step writes.
float Written(float engineYaw, float step) { return engineYaw - step; }

// FrameLogic.h's AimYawRemaining: the head turn the camera still shows.
float Remaining(float headYaw, float offset) { return obvr::math::WrapAngle(headYaw - offset); }

void TestStep() {
	std::printf("The step\n");
	Check(Near(HandOverStep(1.0f, 1.0f), 0.0f), "facing already: no step");
	const float s = HandOverStep(10.0f * kDegree, 40.0f * kDegree);
	Check(Near(Written(10.0f * kDegree, s), 40.0f * kDegree), "the step writes the wanted heading");
	const float across = HandOverStep(350.0f * kDegree, 10.0f * kDegree);
	Check(Near(across, -20.0f * kDegree), "across north: the short way, 20 degrees");
	Check(Near(HandOverStep(10.0f * kDegree, 350.0f * kDegree), 20.0f * kDegree), "and back the short way");
}

void TestKeepsView() {
	std::printf("When the view is kept\n");
	Check(HandOverKeepsView(true, false), "headset, first person: kept");
	Check(!HandOverKeepsView(false, false), "no headset: the view is the body's, plain write");
	Check(!HandOverKeepsView(true, true), "third person: the chase camera follows the body, plain write");
	Check(!HandOverKeepsView(false, true), "neither");
}

void TestBookAlone() {
	std::printf("A booking alone\n");
	BodyYawBook book;
	const float engine = 30.0f * kDegree;
	const float step = HandOverStep(engine, 150.0f * kDegree);
	BookHandOver(book, Written(engine, step), step);
	Check(book.pending, "waits for its landing check");
	Check(Near(book.offset, step), "the offset holds the turn");
	Check(Near(book.stepTaken, step), "the check would take the step back out");
	Check(Near(book.wrote, 150.0f * kDegree), "the heading written is recorded");
	// The view: the camera is the body's heading turned by what is left of
	// the head. Body plus remaining head is the same before and after.
	const float head = 5.0f * kDegree;
	const float viewBefore = -engine + Remaining(head, 0.0f);
	const float viewAfter = -Written(engine, step) + Remaining(head, book.offset);
	Check(Near(obvr::math::WrapAngle(viewAfter - viewBefore), 0.0f), "the view does not move");
}

void TestBookOnTop() {
	std::printf("On top of a step already waiting\n");
	BodyYawBook book;
	book.offset = 4.0f * kDegree;
	book.stepTaken = 3.0f * kDegree;
	book.wrote = 97.0f * kDegree;
	book.pending = true;
	const float step = HandOverStep(97.0f * kDegree, 60.0f * kDegree);
	BookHandOver(book, 60.0f * kDegree, step);
	Check(Near(book.offset, (4.0f + 37.0f) * kDegree), "the offsets add");
	Check(Near(book.stepTaken, (3.0f + 37.0f) * kDegree), "the check takes the frame's steps out together");
	Check(Near(book.wrote, 60.0f * kDegree), "the check compares with the last heading written");
}

void TestRelease() {
	std::printf("Turning back afterwards\n");
	BodyYawBook book;
	const float start = 0.0f;
	const float s1 = HandOverStep(start, 90.0f * kDegree);
	BookHandOver(book, 90.0f * kDegree, s1);
	book.pending = false;  // landed
	const float s2 = HandOverStep(90.0f * kDegree, start);
	BookHandOver(book, start, s2);
	Check(Near(book.offset, 0.0f), "turned back: the offset is gone again");
}

void TestWrapOffset() {
	std::printf("The offset wraps\n");
	BodyYawBook book;
	book.offset = 170.0f * kDegree;
	BookHandOver(book, 0.0f, 30.0f * kDegree);
	Check(Near(book.offset, -160.0f * kDegree), "past half a circle it wraps");
}

}  // namespace

int main() {
	TestStep();
	TestKeepsView();
	TestBookAlone();
	TestBookOnTop();
	TestRelease();
	TestWrapOffset();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
