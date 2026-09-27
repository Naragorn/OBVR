// Checks stowing a held item at the body and taking loose items only by hand
// (vr/Stow.h): the zone, every way a release takes, drops, waits and gives
// up, and when the activate button is kept from the game.

#include <cstdio>

#include "vr/Stow.h"

using namespace obvr;
using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

const NiPoint3 kChest{0.0f, 0.05f, -0.40f};
const NiPoint3 kAhead{0.0f, 0.45f, -0.40f};
constexpr UInt32 kRef = 0x12345678;

void TestZone() {
	std::printf("The zone round the torso\n");
	const StowSettings s;
	Check(InStowZone(kChest, s), "the chest");
	Check(InStowZone(NiPoint3{0.1f, 0.0f, -0.75f}, s), "the belly, low and a little right");
	Check(!InStowZone(kAhead, s), "held out ahead: not at the body");
	Check(!InStowZone(NiPoint3{0.0f, 0.0f, -0.10f}, s), "at the mouth: left for eating");
	Check(!InStowZone(NiPoint3{0.0f, 0.0f, -0.90f}, s), "below the hips");
	Check(!InStowZone(NiPoint3{0.35f, 0.0f, -0.40f}, s), "out at the side");
	Check(InStowZone(NiPoint3{0.0f, 0.0f, s.top}, s) && InStowZone(NiPoint3{0.0f, 0.0f, s.bottom}, s),
	      "its top and bottom belong to it");
}

StowInput Holding(const NiPoint3& hand, UInt32 ref = kRef, bool item = true) {
	StowInput in;
	in.allowed = true;
	in.keyDown = true;
	in.heldRef = ref;
	in.heldIsItem = item;
	in.handValid = true;
	in.handRelative = hand;
	in.dt = 0.011f;
	return in;
}

StowInput LetGo(UInt32 stillHeld) {
	StowInput in = Holding(kChest, stillHeld);
	in.keyDown = false;
	return in;
}

void TestFlows() {
	std::printf("Letting go at the body\n");
	const StowSettings settings;
	{
		StowState s;
		StowVerdict v = StepStow(s, Holding(kAhead), settings);
		Check(!v.atBody, "held out ahead: nothing to stow");
		v = StepStow(s, Holding(kChest), settings);
		Check(v.atBody, "brought to the chest: letting go would stow it");
		v = StepStow(s, LetGo(kRef), settings);
		Check(v.take == 0 && v.waiting, "let go, the engine still holds it: wait, no throw");
		v = StepStow(s, LetGo(0), settings);
		Check(v.take == kRef && !v.waiting, "the engine let go: take it");
		v = StepStow(s, LetGo(0), settings);
		Check(v.take == 0, "taken once");
	}
	{
		StowState s;
		StepStow(s, Holding(kChest), settings);
		const StowVerdict v = StepStow(s, LetGo(0), settings);
		Check(v.take == kRef, "the engine let go on the same frame: taken at once");
	}
	{
		StowState s;
		StepStow(s, Holding(kChest), settings);
		StepStow(s, Holding(kAhead), settings);
		const StowVerdict v = StepStow(s, LetGo(0), settings);
		Check(v.take == 0 && !v.waiting && !v.notItem, "moved away again before letting go: dropped");
	}
	{
		StowState s;
		StepStow(s, Holding(kChest, kRef, false), settings);
		const StowVerdict v = StepStow(s, LetGo(0), settings);
		Check(v.notItem && v.take == 0, "not an item (a body, say): dropped as usual");
		Check(!StepStow(s, Holding(kChest, kRef, false), settings).atBody,
		      "and it never shows as stowable");
	}
	{
		StowState s;
		StepStow(s, Holding(kChest), settings);
		StowVerdict v = StepStow(s, LetGo(kRef), settings);
		int frames = 0;
		while (!v.gaveUp && frames < 500) {
			v = StepStow(s, LetGo(kRef), settings);
			++frames;
		}
		Check(v.gaveUp && v.take == 0, "the engine never lets go: given up");
		Check(frames * 0.011f >= settings.waitSeconds - 0.02f, "after the wait");
		v = StepStow(s, LetGo(0), settings);
		Check(v.take == 0, "and not taken later");
	}
	{
		StowState s;
		StepStow(s, Holding(kChest), settings);
		StepStow(s, LetGo(kRef), settings);
		// Something else in the grab now: the stowed one is no longer held.
		const StowVerdict v = StepStow(s, Holding(kAhead, 0x22222222), settings);
		Check(v.take == kRef, "the grab moved on to another object: the first is taken");
	}
	{
		StowState s;
		StowInput in = Holding(kChest);
		in.handValid = false;
		StepStow(s, in, settings);
		const StowVerdict v = StepStow(s, LetGo(0), settings);
		Check(v.take == 0, "the holding hand untracked: no stow");
	}
	{
		StowState s;
		StowInput in = Holding(kChest);
		in.heldRef = 0;
		const StowVerdict held = StepStow(s, in, settings);
		const StowVerdict v = StepStow(s, LetGo(0), settings);
		Check(!held.atBody && v.take == 0, "the key down but nothing held: nothing");
	}
	{
		StowSettings off;
		off.enabled = false;
		StowState s;
		const StowVerdict held = StepStow(s, Holding(kChest), off);
		const StowVerdict v = StepStow(s, LetGo(0), off);
		Check(!held.atBody && v.take == 0, "switched off: never");
	}
	{
		StowState s;
		StepStow(s, Holding(kChest), settings);
		StepStow(s, LetGo(kRef), settings);
		StowInput menu = LetGo(0);
		menu.allowed = false;
		StowVerdict v = StepStow(s, menu, settings);
		Check(v.take == 0 && !v.waiting, "a menu comes up while it waits: nothing taken");
		v = StepStow(s, LetGo(0), settings);
		Check(v.take == 0, "nor after it");
	}
}

void TestActivate() {
	std::printf("The activate button over a loose item\n");
	StowSettings s;
	Check(!ActivateWithheld(s, true, true, false), "by default activating takes it");
	s.takeOnlyByHand = true;
	Check(ActivateWithheld(s, true, true, false), "only by hand: kept from the game");
	Check(!ActivateWithheld(s, true, true, true), "a book still opens to read");
	Check(!ActivateWithheld(s, true, false, false), "a door, a chest, a person: activated");
	Check(!ActivateWithheld(s, false, false, false), "nothing under the laser: passed on");
	s.enabled = false;
	Check(!ActivateWithheld(s, true, true, false),
	      "stowing off: activating takes it, or it could not be taken at all");
}

}  // namespace

int main() {
	TestZone();
	TestFlows();
	TestActivate();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
