// Checks the torch let go of (game/TorchRelease.h): the other hand's hold
// closing on the torch in it arms the release, the hold opening past the
// release time lets it go - left to the holster at a holster zone, into the
// inventory at the chest, dropped or thrown anywhere else - and a hold that
// is someone else's (a grab, the weapon's handle) never arms it.

#include <cstdio>

#include "game/TorchRelease.h"

using namespace obvr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

game::TorchReleaseSettings Settings() {
	game::TorchReleaseSettings s;
	s.releaseSeconds = 0.03f;
	return s;
}

game::TorchReleaseInput Held(bool holdClosed) {
	game::TorchReleaseInput in;
	in.allowed = true;
	in.torchInHand = true;
	in.holdClosed = holdClosed;
	in.dt = 1.0f / 90.0f;
	return in;
}

// Closes the hold on the torch, opens it and waits out the release time.
game::TorchReleaseVerdict LetGo(game::TorchReleaseState& s, game::TorchReleaseInput open,
                                const game::TorchReleaseSettings& settings) {
	game::StepTorchRelease(s, Held(true), settings);
	open.holdClosed = false;
	game::TorchReleaseVerdict v = game::StepTorchRelease(s, open, settings);
	for (int i = 0; i < 10 && !v.drop && !v.stow && !v.holster; ++i) {
		v = game::StepTorchRelease(s, open, settings);
	}
	return v;
}

void TestArmAndDrop() {
	std::printf("The torch held and let go\n");
	const game::TorchReleaseSettings settings = Settings();
	game::TorchReleaseState s;
	game::TorchReleaseVerdict v = game::StepTorchRelease(s, Held(false), settings);
	Check(!v.armed && s.phase == game::TorchReleasePhase::Idle, "the torch in an open hand: not armed");
	v = game::StepTorchRelease(s, Held(true), settings);
	Check(v.armed && s.phase == game::TorchReleasePhase::Armed, "the hold closes on it: armed");
	v = game::StepTorchRelease(s, Held(true), settings);
	Check(!v.armed && s.phase == game::TorchReleasePhase::Armed, "held on: armed once, said once");
	v = game::StepTorchRelease(s, Held(false), settings);
	Check(v.opened && !v.drop, "the hold opens: the release time starts, nothing let go yet");
	v = game::StepTorchRelease(s, Held(false), settings);
	Check(!v.drop, "one frame on (11 ms): still held");
	v = game::StepTorchRelease(s, Held(false), settings);
	Check(!v.drop, "two frames on (22 ms): still held");
	v = game::StepTorchRelease(s, Held(false), settings);
	Check(v.drop && !v.stow && !v.holster, "open past 0.03 s anywhere: dropped");
	Check(s.phase == game::TorchReleasePhase::Idle, "after the drop: idle, the next hold arms again");

	game::TorchReleaseState again;
	v = LetGo(again, Held(false), settings);
	Check(v.drop, "the helper: let go anywhere drops");
}

void TestHeldOn() {
	std::printf("A flicker of the hold keeps the torch\n");
	const game::TorchReleaseSettings settings = Settings();
	game::TorchReleaseState s;
	game::StepTorchRelease(s, Held(true), settings);
	game::StepTorchRelease(s, Held(false), settings);
	game::StepTorchRelease(s, Held(false), settings);
	const game::TorchReleaseVerdict v = game::StepTorchRelease(s, Held(true), settings);
	Check(v.heldOn && !v.drop && s.phase == game::TorchReleasePhase::Armed,
	      "open two frames, closed again: kept, armed");
	Check(v.openSeconds > 0.0f && v.openSeconds < 0.03f, "how long it read open is said");
}

void TestPlaces() {
	std::printf("Where the hand lets go\n");
	const game::TorchReleaseSettings settings = Settings();
	game::TorchReleaseState s;
	game::TorchReleaseInput holster = Held(false);
	holster.inHolsterZone = true;
	game::TorchReleaseVerdict v = LetGo(s, holster, settings);
	Check(v.holster && !v.drop && !v.stow, "at a holster zone: left to the holster's stow, not dropped");
	game::TorchReleaseInput chest = Held(false);
	chest.atChest = true;
	v = LetGo(s, chest, settings);
	Check(v.stow && !v.drop, "at the chest: into the inventory");
	game::TorchReleaseInput both = Held(false);
	both.atChest = true;
	both.inHolsterZone = true;
	v = LetGo(s, both, settings);
	Check(v.holster && !v.stow, "a holster zone and the chest at once: the holster wins, as for the weapon");
}

void TestThrow() {
	std::printf("The throw's speed\n");
	const game::TorchReleaseSettings settings = Settings();
	game::TorchReleaseState s;
	game::TorchReleaseInput swung = Held(true);
	swung.handVelocity = NiPoint3{0.0f, 300.0f, 60.0f};
	game::StepTorchRelease(s, swung, settings);
	game::TorchReleaseInput open = Held(false);
	open.handVelocity = NiPoint3{0.0f, 10.0f, 0.0f};  // the follow-through after the opening
	game::TorchReleaseVerdict v = game::StepTorchRelease(s, open, settings);
	for (int i = 0; i < 5 && !v.drop; ++i) {
		v = game::StepTorchRelease(s, open, settings);
	}
	Check(v.drop && v.velocity.y > 250.0f, "a swung hand: thrown with the fastest speed while it held on");
	game::TorchReleaseState still;
	v = LetGo(still, Held(false), settings);
	Check(v.drop && v.velocity.LengthSquared() == 0.0f, "a still hand: a fall, no speed");
	game::TorchReleaseSettings never = settings;
	never.throwStrength = 0.0f;
	game::TorchReleaseState s2;
	game::StepTorchRelease(s2, swung, never);
	v = game::StepTorchRelease(s2, open, never);
	for (int i = 0; i < 5 && !v.drop; ++i) {
		v = game::StepTorchRelease(s2, open, never);
	}
	Check(v.drop && v.velocity.LengthSquared() == 0.0f, "a throw strength of 0: it falls");
	// The wrist's flick in the throw's frame: the torch tumbles.
	game::TorchReleaseState s3;
	game::TorchReleaseInput flick = Held(true);
	flick.handVelocity = NiPoint3{0.0f, 300.0f, 0.0f};
	flick.handAngularVelocity = NiPoint3{0.0f, 0.0f, 9.0f};
	game::StepTorchRelease(s3, flick, settings);
	v = game::StepTorchRelease(s3, open, settings);
	for (int i = 0; i < 5 && !v.drop; ++i) {
		v = game::StepTorchRelease(s3, open, settings);
	}
	Check(v.drop && v.spin.z > 8.99f && v.spin.z < 9.01f, "flicked: spun at the hand's turning speed");
}

void TestNotOurs() {
	std::printf("When the release is not live\n");
	const game::TorchReleaseSettings settings = Settings();
	game::TorchReleaseState s;
	game::StepTorchRelease(s, Held(true), settings);
	game::TorchReleaseInput gone = Held(true);
	gone.torchInHand = false;
	game::TorchReleaseVerdict v = game::StepTorchRelease(s, gone, settings);
	Check(v.cancelled && s.phase == game::TorchReleasePhase::Idle, "the torch left the hand while armed: cancelled");
	v = game::StepTorchRelease(s, gone, settings);
	Check(!v.cancelled, "and said once");
	game::StepTorchRelease(s, Held(true), settings);
	game::TorchReleaseInput menu = Held(false);
	menu.allowed = false;
	v = game::StepTorchRelease(s, menu, settings);
	Check(v.cancelled && !v.drop, "a menu while armed: cancelled, nothing dropped");
	game::TorchReleaseSettings off = settings;
	off.enabled = false;
	game::TorchReleaseState s2;
	v = game::StepTorchRelease(s2, Held(true), off);
	Check(!v.armed && s2.phase == game::TorchReleasePhase::Idle, "[Hands] TorchDrop off: never armed");
	// The hold someone else's: a grab of the other hand, the weapon's handle.
	game::TorchReleaseState s3;
	game::TorchReleaseInput busy = Held(true);
	busy.holdIsTaken = true;
	v = game::StepTorchRelease(s3, busy, settings);
	Check(!v.armed && s3.phase == game::TorchReleasePhase::Idle, "the hold taken by a grab: not armed");
	game::StepTorchRelease(s3, Held(true), settings);
	busy.holdClosed = false;
	v = game::StepTorchRelease(s3, busy, settings);
	Check(v.cancelled && !v.drop, "armed, then the hold taken: cancelled, the torch kept");
}

}  // namespace

int main() {
	TestArmAndDrop();
	TestHeldOn();
	TestPlaces();
	TestThrow();
	TestNotOurs();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
