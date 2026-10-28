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

// A torch equipped from the inventory (or a hotkey) comes into an open hand:
// it has to be held - the hand has TorchGraceSeconds to close on it, else it
// falls (Nadi, 2026-10-27: "gibt zeit um die hand zu schliessen damit sie
// nicht sofort zu boden fällt. aber ich muss sie schon halten, sonst fällt
// sie").
void TestGrace() {
	std::printf("A torch equipped into an open hand\n");
	game::TorchReleaseSettings settings = Settings();
	settings.graceSeconds = 1.0f;
	game::TorchReleaseState s;
	// Equipped in the inventory: the menu up, the torch not in the hand yet.
	game::TorchReleaseInput menu = Held(false);
	menu.allowed = false;
	menu.torchInHand = false;
	menu.torchWorn = true;
	menu.justEquipped = true;
	game::TorchReleaseVerdict v = game::StepTorchRelease(s, menu, settings);
	Check(!v.drop && s.pending, "equipped in the menu: waiting for the hand");
	menu.justEquipped = false;
	for (int i = 0; i < 300; ++i) {
		v = game::StepTorchRelease(s, menu, settings);
	}
	Check(!v.drop && s.pending && s.grace > 0.99f, "the menu up for seconds: the time does not run");
	game::TorchReleaseInput open = Held(false);
	open.torchWorn = true;
	open.handVelocity = NiPoint3{100.0f, 0.0f, 0.0f};
	v = game::StepTorchRelease(s, open, settings);
	Check(!v.drop && s.pending, "in the open hand in the world: the time runs");
	int frames = 1;
	while (!v.drop && frames < 200) {
		v = game::StepTorchRelease(s, open, settings);
		++frames;
	}
	Check(v.drop && frames >= 89 && frames <= 92, "never held: it falls after the second");
	Check(v.velocity.LengthSquared() == 0.0f && !s.pending, "falls, not thrown; nothing pending after");
	v = game::StepTorchRelease(s, open, settings);
	Check(!v.drop, "once");

	// Held in time: armed as any hold, nothing falls by itself.
	game::TorchReleaseState t;
	open.justEquipped = true;
	game::StepTorchRelease(t, open, settings);
	open.justEquipped = false;
	for (int i = 0; i < 30; ++i) {
		game::StepTorchRelease(t, open, settings);
	}
	game::TorchReleaseInput held = Held(true);
	held.torchWorn = true;
	v = game::StepTorchRelease(t, held, settings);
	Check(v.armed && !t.pending, "closed within the second: armed, the wait over");
	for (int i = 0; i < 200 && !v.drop; ++i) {
		v = game::StepTorchRelease(t, held, settings);
	}
	Check(!v.drop, "held on: never falls by itself");

	// Taken from the hip or the world: equipped into a closed hold - armed at once.
	game::TorchReleaseState u;
	held.justEquipped = true;
	v = game::StepTorchRelease(u, held, settings);
	Check(v.armed && !u.pending, "equipped into a closed hand: armed at once");

	// Off with 0: an open hand keeps it, as before.
	settings.graceSeconds = 0.0f;
	game::TorchReleaseState w;
	open.justEquipped = true;
	game::StepTorchRelease(w, open, settings);
	open.justEquipped = false;
	for (int i = 0; i < 300 && !v.drop; ++i) {
		v = game::StepTorchRelease(w, open, settings);
	}
	Check(!v.drop, "TorchGraceSeconds 0: an open hand keeps it");

	// The torch taken off meanwhile (put away, unequipped): nothing falls later.
	settings.graceSeconds = 1.0f;
	game::TorchReleaseState x;
	open.justEquipped = true;
	game::StepTorchRelease(x, open, settings);
	open.justEquipped = false;
	game::TorchReleaseInput gone = open;
	gone.torchWorn = false;
	gone.torchInHand = false;
	game::StepTorchRelease(x, gone, settings);
	Check(!x.pending, "unequipped while waiting: forgotten");
	// Held by something else (the hand on a handle): the time waits.
	game::TorchReleaseState y;
	open.justEquipped = true;
	game::StepTorchRelease(y, open, settings);
	open.justEquipped = false;
	game::TorchReleaseInput busy = open;
	busy.holdIsTaken = true;
	for (int i = 0; i < 200; ++i) {
		v = game::StepTorchRelease(y, busy, settings);
	}
	Check(!v.drop && y.pending, "the hand's hold someone else's: the time waits");
	// At the chest or a holster zone when the time runs out: it falls there all the same.
	open.inHolsterZone = true;
	for (int i = 0; i < 200 && !v.drop; ++i) {
		v = game::StepTorchRelease(y, open, settings);
	}
	Check(v.drop && !v.holster, "at a holster zone, never held: it falls (a put-away is a closing and an opening)");
}

}  // namespace

// The hand opened 0.11 s after the swing's peak (Nadi, Windows 2026-10-27:
// torches thrown often, not always): still thrown with the peak, and the
// verdict carries the peaks for the log.
void TestLateOpening() {
	std::printf("Opened a little after the peak\n");
	const game::TorchReleaseSettings settings = Settings();
	game::TorchReleaseState s;
	game::TorchReleaseInput in = Held(true);
	game::StepTorchRelease(s, in, settings);
	in.handVelocity = NiPoint3{0.0f, 300.0f, 0.0f};
	game::StepTorchRelease(s, in, settings);
	in.handVelocity = NiPoint3{0.0f, 20.0f, 0.0f};
	for (int i = 0; i < 10; ++i) {
		game::StepTorchRelease(s, in, settings);
	}
	in.holdClosed = false;
	game::TorchReleaseVerdict v;
	for (int i = 0; i < 10 && !v.drop; ++i) {
		v = game::StepTorchRelease(s, in, settings);
	}
	Check(v.drop && v.velocity.y > 250.0f, "thrown with the peak 11 frames before the opening");
	Check(v.peaks.lookUnits > 299.0f && v.peaks.lookAgeSeconds > 0.1f && v.peaks.lookAgeSeconds < 0.14f,
	      "the verdict says the peak and how long before the opening it was");
}

int main() {
	TestLateOpening();
	TestArmAndDrop();
	TestHeldOn();
	TestPlaces();
	TestThrow();
	TestNotOurs();
	TestGrace();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
