// Checks the tap that ends a grab outliving its key (game/GrabLetGo.h): the
// Quest 2 tester's dagger stayed in the engine's grab after the hand opened
// (2026-11-19). Every way through: the engine letting go by itself, the
// grace, the tap's frames, a second tap, giving up, the hand closing again, a
// living one under the crosshair, a grab OBVR's key never held.

#include <cstdio>
#include <limits>

#include "game/GrabLetGo.h"

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

constexpr float kFrame = 1.0f / 72.0f;
constexpr UInt32 kDagger = 0x32EAED68;

LetGoVerdict Frame(LetGoState& s, bool key, UInt32 holds, bool living = false, float dt = kFrame) {
	LetGoInput in;
	in.keyDown = key;
	in.engineHolds = holds;
	in.livingUnderCrosshair = living;
	in.dt = dt;
	return StepLetGo(s, in);
}

// The hand holds the dagger a few frames, then opens with the engine still
// holding it.
void HeldThenOpened(LetGoState& s) {
	Frame(s, true, 0);
	Frame(s, true, kDagger);
	Frame(s, true, kDagger);
	Frame(s, false, kDagger);
}

// Frames until a verdict with a press begins, at most `most`.
UInt32 FramesUntilTap(LetGoState& s, UInt32 holds, UInt32 most, LetGoVerdict& got) {
	for (UInt32 i = 1; i <= most; ++i) {
		got = Frame(s, false, holds);
		if (got.press) {
			return i;
		}
	}
	return 0;
}

void TestNormalRelease() {
	std::printf("The engine lets go as the key goes up\n");
	LetGoState s;
	Frame(s, true, kDagger);
	const LetGoVerdict up = Frame(s, false, 0);
	Check(!up.press && s.phase == LetGoPhase::Idle, "let go on the key's up: nothing watched, nothing pressed");
	bool pressed = false;
	for (int i = 0; i < 200; ++i) {
		pressed = pressed || Frame(s, false, 0).press;
	}
	Check(!pressed, "and nothing pressed afterwards");
}

void TestLateRelease() {
	std::printf("The engine lets go during the grace\n");
	LetGoState s;
	HeldThenOpened(s);
	Check(s.phase == LetGoPhase::Waiting && s.ref == kDagger, "still held at the key's up: watched");
	const LetGoVerdict a = Frame(s, false, kDagger);
	Check(!a.press, "within the grace: no tap yet");
	const LetGoVerdict b = Frame(s, false, 0);
	Check(b.released && !b.press && s.phase == LetGoPhase::Idle, "let go a frame later: done, no tap");
}

void TestTapEndsIt() {
	std::printf("A grab that outlives its key\n");
	LetGoState s;
	HeldThenOpened(s);
	LetGoVerdict v;
	const UInt32 frames = FramesUntilTap(s, kDagger, 100, v);
	const UInt32 graceFrames = static_cast<UInt32>(kLetGoGraceSeconds / kFrame);
	Check(frames >= graceFrames && frames <= graceFrames + 2, "the tap after the grace (a quarter second)");
	Check(v.tapStarted && v.press, "the tap's first frame: the key down, said once");
	UInt32 pressed = 1;
	for (UInt32 i = 0; i < 10; ++i) {
		const LetGoVerdict f = Frame(s, false, kDagger);
		pressed += f.press ? 1 : 0;
		Check(!f.tapStarted, "no second start inside the tap");
		if (!f.press) {
			break;
		}
	}
	Check(pressed == kLetGoTapFrames, "the key down for the tap's frames, then up");
	const LetGoVerdict after = Frame(s, false, 0);
	Check(after.released && s.phase == LetGoPhase::Idle && s.taps == 1, "the engine let go after the tap: done");
}

void TestGiveUp() {
	std::printf("Taps that do not end it\n");
	LetGoState s;
	HeldThenOpened(s);
	UInt32 starts = 0;
	bool gaveUp = false;
	UInt32 framesAfterGiveUp = 0;
	bool pressedAfter = false;
	for (UInt32 i = 0; i < 400; ++i) {
		const LetGoVerdict v = Frame(s, false, kDagger);
		starts += v.tapStarted ? 1 : 0;
		if (gaveUp) {
			++framesAfterGiveUp;
			pressedAfter = pressedAfter || v.press;
		}
		gaveUp = gaveUp || v.gaveUp;
	}
	Check(starts == kLetGoTaps, "at most kLetGoTaps taps a release");
	Check(gaveUp && s.phase == LetGoPhase::Idle, "then given up, and said");
	Check(framesAfterGiveUp > 0 && !pressedAfter, "nothing pressed after giving up");
}

void TestHandClosesAgain() {
	std::printf("The hand closes again\n");
	LetGoState s;
	HeldThenOpened(s);
	Frame(s, false, kDagger);
	const LetGoVerdict again = Frame(s, true, kDagger);
	Check(!again.press && s.phase == LetGoPhase::Idle, "the grab is the hand's again: no tap");
	// Opened once more: watched afresh, with fresh taps.
	const LetGoVerdict up = Frame(s, false, kDagger);
	Check(!up.press && s.phase == LetGoPhase::Waiting && s.taps == 0, "opened again: watched from the start");
	LetGoState t;
	HeldThenOpened(t);
	LetGoVerdict v;
	FramesUntilTap(t, kDagger, 100, v);
	const LetGoVerdict closed = Frame(t, true, kDagger);
	Check(!closed.press && t.phase == LetGoPhase::Idle, "closed during a tap: the tap stops at once");
}

void TestLivingUnderCrosshair() {
	std::printf("A living one under the crosshair\n");
	LetGoState s;
	HeldThenOpened(s);
	bool pressed = false;
	for (int i = 0; i < 100; ++i) {
		pressed = pressed || Frame(s, false, kDagger, true).press;
	}
	Check(!pressed && s.phase == LetGoPhase::Waiting, "no tap while it would be the mod's slap - it waits");
	LetGoVerdict v;
	Check(FramesUntilTap(s, kDagger, 2, v) == 1 && v.tapStarted, "the crosshair off them: the tap at once");
}

void TestNotOurs() {
	std::printf("A grab the key never held\n");
	LetGoState s;
	Frame(s, true, 0);
	const LetGoVerdict up = Frame(s, false, kDagger);
	Check(!up.press && s.phase == LetGoPhase::Idle, "taken by another way after the key went up: left alone");
	LetGoState t;
	bool pressed = false;
	for (int i = 0; i < 100; ++i) {
		pressed = pressed || Frame(t, false, kDagger).press;
	}
	Check(!pressed, "held with the key never down (telekinesis, a script): never tapped");
	LetGoState u;
	HeldThenOpened(u);
	const LetGoVerdict other = Frame(u, false, 0x11111111);
	Check(!other.press && !other.released && u.phase == LetGoPhase::Idle,
	      "another thing in the grab: not ours to end");
	LetGoState w;
	HeldThenOpened(w);
	const LetGoVerdict nan = Frame(w, false, kDagger, false, std::numeric_limits<float>::quiet_NaN());
	Check(!nan.press && w.phase == LetGoPhase::Waiting, "a frame time that is no number counts nothing");
}

}  // namespace

int main() {
	TestNormalRelease();
	TestLateRelease();
	TestTapEndsIt();
	TestGiveUp();
	TestHandClosesAgain();
	TestLivingUnderCrosshair();
	TestNotOurs();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
