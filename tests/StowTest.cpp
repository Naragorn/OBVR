// Checks stowing a held item at the body and taking loose items only by hand
// (vr/Stow.h): the zone, every way a release takes, drops, waits and gives
// up, and when the activate button is kept from the game.

#include <cstdio>

#include <cmath>

#include "vr/Holster.h"
#include "vr/Stow.h"
#include "vr/StowPlace.h"
#include "game/TakeItem.h"

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

const NiPoint3 kChest{0.02f, 0.20f, -0.25f};
const NiPoint3 kAhead{0.0f, 0.45f, -0.40f};
constexpr UInt32 kRef = 0x12345678;

void TestZone() {
	std::printf("The spot at the chest\n");
	const StowSettings s;
	Check(InStowZone(kChest, s), "the chest");
	Check(InStowZone(NiPoint3{s.centreRight, s.centreForward, s.centreUp}, s), "the spot's middle");
	Check(InStowZone(NiPoint3{0.0f, s.centreForward, s.centreUp + s.radius}, s),
	      "its edge belongs to it");
	// Where the tester held items against the chest (OBVR.log, 2026-09-27).
	Check(InStowZone(NiPoint3{0.06f, 0.22f, -0.21f}, s), "the chest, just under the chin");
	Check(InStowZone(NiPoint3{0.01f, 0.23f, -0.23f}, s), "the chest, the hand held out a little");
	Check(InStowZone(NiPoint3{0.05f, 0.22f, -0.29f}, s), "the chest, lower");
	// Letting go in front of oneself drops the item again (the second test:
	// with the wide zone nothing could be dropped ahead).
	Check(!InStowZone(kAhead, s), "held out ahead: dropped");
	Check(!InStowZone(NiPoint3{0.00f, 0.23f, -0.67f}, s), "down at the belly, in front: dropped");
	Check(!InStowZone(NiPoint3{0.0f, 0.0f, -0.10f}, s), "at the mouth: left for eating");
	Check(!InStowZone(NiPoint3{0.35f, 0.2f, -0.30f}, s), "out at the side");
}

void TestSpotPose() {
	std::printf("Where the spot is drawn\n");
	const StowSettings s;
	const NiPoint3 eyes{0.3f, 1.6f, -0.2f};
	// Looking along -z (OpenVR's forward): ahead is -z, right is +x.
	NiPoint3 at = StowSpotInTracking(Quaternion::Identity(), eyes, s);
	Check(std::fabs(at.x - (0.3f + s.centreRight)) < 1e-4f &&
	          std::fabs(at.y - (1.6f + s.centreUp)) < 1e-4f &&
	          std::fabs(at.z - (-0.2f - s.centreForward)) < 1e-4f,
	      "ahead of the eyes and below them");
	// Turned 90 degrees to the left: ahead is -x.
	const Quaternion left = FromAxisAngle(0.0f, 1.0f, 0.0f, 90.0f);
	at = StowSpotInTracking(left, eyes, s);
	Check(std::fabs(at.x - (0.3f - s.centreForward)) < 1e-3f && std::fabs(at.z + 0.2f) < 1e-3f,
	      "it turns with the heading");
	// And it is where BodyRelative puts the zone's middle.
	StowSettings offset = s;
	offset.centreRight = 0.1f;
	at = StowSpotInTracking(left, eyes, offset);
	const NiPoint3 back = BodyRelative(left, eyes, at);
	Check(std::fabs(back.x - 0.1f) < 1e-3f && std::fabs(back.y - offset.centreForward) < 1e-3f &&
	          std::fabs(back.z - offset.centreUp) < 1e-3f,
	      "the inverse of the body frame");
	// Looking straight down: the level heading falls back to ahead = -z.
	const Quaternion down = FromAxisAngle(1.0f, 0.0f, 0.0f, -90.0f);
	at = StowSpotInTracking(down, eyes, s);
	Check(std::fabs(at.y - (1.6f + s.centreUp)) < 1e-4f, "looking down, still at the chest");
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
		Check(v.showSpot, "an item held: the spot shows");
		v = StepStow(s, Holding(kChest), settings);
		Check(v.atBody && v.showSpot, "brought to the chest: lit, letting go would stow it");
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
		for (int i = 0; i < 40; ++i) {
			StepStow(s, Holding(kAhead), settings);  // 0.44 s away
		}
		const StowVerdict v = StepStow(s, LetGo(0), settings);
		Check(v.take == 0 && !v.waiting && !v.notItem, "moved away again, then let go: dropped");
	}
	{
		// The hand leaves the zone as it opens: a frame or two later still stows.
		StowState s;
		StepStow(s, Holding(kChest), settings);
		StepStow(s, Holding(kAhead), settings);
		StepStow(s, Holding(kAhead), settings);
		StowInput open = LetGo(0);
		open.handRelative = kAhead;
		const StowVerdict v = StepStow(s, open, settings);
		Check(v.take == kRef, "let go 0.02 s after leaving the body: still stowed");
	}
	{
		// A new object is not stowed on the time the last one spent there.
		StowState s;
		StepStow(s, Holding(kChest), settings);
		StepStow(s, LetGo(0), settings);
		StepStow(s, Holding(kAhead, 0x33333333), settings);
		const StowVerdict v = StepStow(s, LetGo(0), settings);
		Check(v.take == 0, "the next object, held only away from the body: dropped");
	}
	{
		StowState s;
		StepStow(s, Holding(kChest, kRef, false), settings);
		const StowVerdict v = StepStow(s, LetGo(0), settings);
		Check(v.notItem && v.take == 0, "not an item (a body, say): dropped as usual");
		const StowVerdict held = StepStow(s, Holding(kChest, kRef, false), settings);
		Check(!held.atBody && !held.showSpot, "and it never shows as stowable, nor the spot");
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
	Check(ActivateWithheld(s, true, true, false), "by default (only by hand, since 2026-10-07): kept from the game");
	s.takeOnlyByHand = false;
	Check(!ActivateWithheld(s, true, true, false), "switched off: activating takes it");
	s.takeOnlyByHand = true;
	Check(!ActivateWithheld(s, true, true, true), "a book still opens to read");
	Check(!ActivateWithheld(s, true, false, false), "a door, a chest, a person: activated");
	Check(!ActivateWithheld(s, false, false, false), "nothing under the laser: passed on");
	s.enabled = false;
	Check(!ActivateWithheld(s, true, true, false),
	      "stowing off: activating takes it, or it could not be taken at all");
}

}  // namespace

bool NearPoint(const NiPoint3& a, const NiPoint3& b) {
	return std::fabs(a.x - b.x) < 1e-4f && std::fabs(a.y - b.y) < 1e-4f && std::fabs(a.z - b.z) < 1e-4f;
}

bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

void TestPlace() {
	std::printf("Placing the stow spot\n");
	StowSettings settings;
	settings.centreRight = 0.05f;
	const NiPoint3 spot{0.05f, settings.centreForward, settings.centreUp};

	StowPlaceState s;
	StowPlaceInput in;
	StowPlaceVerdict v = StepStowPlace(s, in, settings);
	Check(!v.active && !v.save && !v.restore, "not started: nothing");
	in.command = StowPlaceCommand::Keep;
	v = StepStowPlace(s, in, settings);
	Check(!v.save && !v.active, "a stray Done with nothing open saves nothing");
	in.command = StowPlaceCommand::Cancel;
	v = StepStowPlace(s, in, settings);
	Check(!v.restore, "nor a stray Cancel");
	in.command = StowPlaceCommand::Reset;
	v = StepStowPlace(s, in, settings);
	Check(!v.save, "nor a stray Reset");

	// Started with the right grip already closed: not a press.
	in = StowPlaceInput{};
	in.command = StowPlaceCommand::Start;
	in.rightValid = in.leftValid = true;
	in.right = spot;
	in.rightGrip = true;
	v = StepStowPlace(s, in, settings);
	Check(v.active && NearPoint(v.spot, spot) && !v.dragging,
	      "started: the ring shows where the spot is; a grip already closed takes nothing");
	in.command = StowPlaceCommand::None;
	in.right = NiPoint3{0.3f, 0.3f, -0.3f};
	v = StepStowPlace(s, in, settings);
	Check(!v.dragging && NearPoint(v.spot, spot), "still held from before: the ring stays");

	// A press away from the ring takes nothing.
	in.rightGrip = false;
	StepStowPlace(s, in, settings);
	in.rightGrip = true;
	v = StepStowPlace(s, in, settings);
	Check(!v.grabbed && !v.dragging, "a grip closed far from the ring takes nothing");

	// A press at the ring's edge takes it, with its offset.
	in.rightGrip = false;
	in.right = spot + NiPoint3{settings.radius + 0.04f, 0.0f, 0.0f};
	StepStowPlace(s, in, settings);
	in.rightGrip = true;
	v = StepStowPlace(s, in, settings);
	Check(v.grabbed && v.dragging && NearPoint(v.spot, spot),
	      "a grip closed just outside the ring takes it, without a jump");
	in.right = in.right + NiPoint3{0.0f, 0.1f, 0.05f};
	v = StepStowPlace(s, in, settings);
	const NiPoint3 moved = spot + NiPoint3{0.0f, 0.1f, 0.05f};
	Check(v.dragging && NearPoint(v.spot, moved), "it moves with the hand");
	in.rightGrip = false;
	v = StepStowPlace(s, in, settings);
	Check(v.dropped && !v.dragging && NearPoint(v.spot, moved), "the grip opened: it stays there");
	in.right = NiPoint3{0.0f, 0.0f, 0.0f};
	v = StepStowPlace(s, in, settings);
	Check(NearPoint(v.spot, moved), "and does not follow the open hand");

	// The left hand takes it too; lost tracking drops it.
	in.left = moved;
	in.leftGrip = true;
	v = StepStowPlace(s, in, settings);
	Check(v.grabbed && v.dragging, "the left hand takes it as well");
	in.leftValid = false;
	v = StepStowPlace(s, in, settings);
	Check(v.dropped && !v.dragging, "the dragging hand lost: dropped where it was");
	in.leftValid = true;
	in.leftGrip = false;
	StepStowPlace(s, in, settings);

	// Both grips at once: the right one.
	in.right = moved;
	in.left = moved;
	in.rightGrip = in.leftGrip = true;
	v = StepStowPlace(s, in, settings);
	in.left = moved + NiPoint3{0.2f, 0.0f, 0.0f};
	in.right = moved + NiPoint3{0.0f, 0.05f, 0.0f};
	v = StepStowPlace(s, in, settings);
	Check(NearPoint(v.spot, moved + NiPoint3{0.0f, 0.05f, 0.0f}), "both at once: the right hand drags");

	// Dragged off into the room: kept within reach.
	in.right = NiPoint3{5.0f, 5.0f, 5.0f};
	v = StepStowPlace(s, in, settings);
	Check(v.spot.x == kStowPlaceMaxSide && v.spot.y == kStowPlaceMaxForward &&
	          v.spot.z == kStowPlaceMaxUp,
	      "too far: held at the edge of reach");
	in.right = NiPoint3{-5.0f, -5.0f, -5.0f};
	v = StepStowPlace(s, in, settings);
	Check(v.spot.x == -kStowPlaceMaxSide && v.spot.y == kStowPlaceMinForward &&
	          v.spot.z == kStowPlaceMinUp,
	      "and the other way");
	in.right = moved;
	const StowPlaceVerdict placed = StepStowPlace(s, in, settings);
	in.rightGrip = in.leftGrip = false;
	StepStowPlace(s, in, settings);

	// Done keeps it; the state is closed afterwards.
	in.command = StowPlaceCommand::Keep;
	v = StepStowPlace(s, in, settings);
	Check(v.save && !v.restore && !v.active && NearPoint(v.spot, placed.spot),
	      "Done: the spot saved where the ring is, the ring hidden");
	in.command = StowPlaceCommand::None;
	v = StepStowPlace(s, in, settings);
	Check(!v.active, "and it stays closed");

	// Cancel puts it back.
	in.command = StowPlaceCommand::Start;
	StepStowPlace(s, in, settings);
	in.command = StowPlaceCommand::None;
	in.right = spot;
	in.rightGrip = true;
	StepStowPlace(s, in, settings);
	in.right = spot + NiPoint3{0.1f, 0.0f, 0.0f};
	StepStowPlace(s, in, settings);
	in.command = StowPlaceCommand::Cancel;
	v = StepStowPlace(s, in, settings);
	Check(v.restore && !v.save && NearPoint(v.spot, spot), "Cancel: back where it was, nothing saved");

	// Reset: the default, saved.
	in.command = StowPlaceCommand::Start;
	StepStowPlace(s, in, settings);
	in.command = StowPlaceCommand::Reset;
	v = StepStowPlace(s, in, settings);
	const StowSettings defaults;
	Check(v.save && NearPoint(v.spot, NiPoint3{defaults.centreRight, defaults.centreForward,
	                                           defaults.centreUp}),
	      "Reset: the default spot, saved");
	Check(v.radius == defaults.radius, "Reset: the default size with it");

	// Sized by the stick while placing: up grows, down shrinks, clamped,
	// Done keeps it, Cancel puts it back, nothing without a window.
	settings.radius = 0.16f;
	in = StowPlaceInput{};
	in.command = StowPlaceCommand::Start;
	StepStowPlace(s, in, settings);
	in.command = StowPlaceCommand::None;
	in.resize = 1.0f;
	in.dtSeconds = 0.5f;
	v = StepStowPlace(s, in, settings);
	Check(v.active && Near(v.radius, 0.16f + 0.5f * kStowResizeMetresPerSecond), "the stick up: grown by its speed and the frame");
	in.resize = -0.5f;
	in.dtSeconds = 0.25f;
	v = StepStowPlace(s, in, settings);
	Check(Near(v.radius, 0.16f + 0.5f * kStowResizeMetresPerSecond - 0.125f * kStowResizeMetresPerSecond),
	      "half down: shrunk by half the speed");
	in.resize = 1.0f;
	in.dtSeconds = 100.0f;
	v = StepStowPlace(s, in, settings);
	Check(Near(v.radius, kStowMaxRadius), "held up for long: stops at the largest");
	in.resize = -1.0f;
	v = StepStowPlace(s, in, settings);
	Check(Near(v.radius, kStowMinRadius), "held down for long: stops at the smallest");
	in.resize = 1.0f;
	in.dtSeconds = 0.0f;
	v = StepStowPlace(s, in, settings);
	Check(Near(v.radius, kStowMinRadius), "no frame time: unchanged");
	in.resize = 0.0f;
	in.command = StowPlaceCommand::Keep;
	v = StepStowPlace(s, in, settings);
	Check(v.save && Near(v.radius, kStowMinRadius), "Done: the size kept with the spot");
	in.command = StowPlaceCommand::Start;
	StepStowPlace(s, in, settings);
	in.command = StowPlaceCommand::None;
	in.resize = 1.0f;
	in.dtSeconds = 1.0f;
	StepStowPlace(s, in, settings);
	in.resize = 0.0f;
	in.command = StowPlaceCommand::Cancel;
	v = StepStowPlace(s, in, settings);
	Check(v.restore && Near(v.radius, 0.16f), "Cancel: the size it opened with");
	in.command = StowPlaceCommand::None;
	in.resize = 1.0f;
	in.dtSeconds = 1.0f;
	v = StepStowPlace(s, in, settings);
	Check(!v.active && v.radius == 0.16f, "no window: the stick sizes nothing");

	// Shown.
	Check(StowRingShown(true, false, false), "placing: the ring shows");
	Check(!StowRingShown(false, false, true), "the ring off: not shown while holding an item");
	Check(StowRingShown(false, true, true), "switched on: shown while holding an item");
	Check(!StowRingShown(false, true, false), "switched on, nothing held: not shown");
	Check(defaults.spotVisible, "on by default (since 2026-10-18, for the beta's testers)");
}

void TestDrawnHand() {
	std::printf("The drawn hand from its controller\n");
	const NiPoint3 controller{0.3f, 1.2f, -0.4f};
	// No grip: the hand is the controller's own place.
	NiPoint3 at = DrawnHandInTracking(Quaternion::Identity(), controller, NiPoint3{0.0f, 0.0f, 0.0f});
	Check(NearPoint(at, controller), "no grip: the controller's own place");
	// The tester's grip, 0.23 m behind the controller along its forward: the
	// controller pointing ahead (-z), the hand 0.23 m back (+z).
	at = DrawnHandInTracking(Quaternion::Identity(), controller, NiPoint3{0.0f, -0.23f, 0.0f});
	Check(NearPoint(at, NiPoint3{0.3f, 1.2f, -0.17f}), "a grip 0.23 m back: the hand behind the controller");
	// Right and down along the controller's own axes.
	at = DrawnHandInTracking(Quaternion::Identity(), controller, NiPoint3{0.1f, 0.0f, -0.04f});
	Check(NearPoint(at, NiPoint3{0.4f, 1.16f, -0.4f}), "a grip right and under: the hand right of and under the controller");
	// The controller turned a quarter to the left about the up axis: its
	// forward is -x, so a grip 0.2 m forward puts the hand 0.2 m at -x, and
	// its up stays up.
	const Quaternion left = FromAxisAngle(0.0f, 1.0f, 0.0f, 90.0f);
	at = DrawnHandInTracking(left, controller, NiPoint3{0.0f, 0.2f, 0.05f});
	Check(NearPoint(at, NiPoint3{0.1f, 1.25f, -0.4f}), "the controller turned left: the grip turns with it");
	// Pointing down (a quarter about x, the forward -z turned to -y): a grip
	// forward goes down, the grip's up goes ahead (-z).
	const Quaternion down = FromAxisAngle(1.0f, 0.0f, 0.0f, -90.0f);
	at = DrawnHandInTracking(down, controller, NiPoint3{0.0f, 0.2f, 0.05f});
	Check(NearPoint(at, NiPoint3{0.3f, 1.0f, -0.45f}), "the controller pointing down: the grip's forward goes down, its up ahead");
	// What a stow then reads: the tester's controller 0.40 m out with the
	// hand drawn 0.23 m back sits at the chest, in the zone.
	const StowSettings s;
	const NiPoint3 eyes{0.0f, 1.6f, 0.0f};
	const NiPoint3 heldOut{0.02f, 1.6f + s.centreUp, -0.40f};
	Check(!InStowZone(BodyRelative(Quaternion::Identity(), eyes, heldOut), s),
	      "the controller 0.40 m out: not in the zone by itself");
	const NiPoint3 drawn = DrawnHandInTracking(Quaternion::Identity(), heldOut, NiPoint3{0.0f, -0.23f, 0.0f});
	Check(InStowZone(BodyRelative(Quaternion::Identity(), eyes, drawn), s),
	      "the hand drawn 0.23 m behind it: at the chest, in the zone");
}

// Food and potions let go at the mouth are eaten and drunk (Nadi, Windows,
// 2026-10-24: "Essen und Tränke an den Mund ist Essen"; HIGGS for Skyrim VR
// does it the same way, "by dropping them at the mouth"). The release
// decides, as at the chest; the mouth wins where the two meet.
StowInput HeldAtMouth(bool atMouth, bool edible, const NiPoint3& hand = kAhead) {
	StowInput in = Holding(hand);
	in.heldIsEdible = edible;
	in.handAtMouth = atMouth;
	return in;
}

StowInput LetGoAtMouth(UInt32 stillHeld, bool atMouth = true, const NiPoint3& hand = kAhead) {
	StowInput in = HeldAtMouth(atMouth, true, hand);
	in.keyDown = false;
	in.heldRef = stillHeld;
	return in;
}

void TestMouth() {
	std::printf("The mouth\n");
	const StowSettings s;
	const NiPoint3 eyes{0.3f, 1.6f, -0.2f};
	NiPoint3 mouth = MouthInTracking(Quaternion::Identity(), eyes, s);
	Check(std::fabs(mouth.x - 0.3f) < 1e-4f && std::fabs(mouth.y - (1.6f - s.mouthDown)) < 1e-4f &&
	          std::fabs(mouth.z - (-0.2f - s.mouthForward)) < 1e-4f,
	      "looking ahead: below the eyes and a little ahead");
	const Quaternion pitched = FromAxisAngle(1.0f, 0.0f, 0.0f, -30.0f);
	mouth = MouthInTracking(pitched, eyes, s);
	const NiPoint3 turned = Rotate(pitched, NiPoint3{0.0f, -s.mouthDown, -s.mouthForward});
	Check(NearPoint(mouth, eyes + turned), "the head pitched: the mouth goes with the head, not the level heading");
	Check(AtMouth(mouth, mouth, s), "the mouth itself");
	Check(AtMouth(mouth + NiPoint3{0.0f, 0.0f, -s.mouthRadius}, mouth, s), "its edge belongs to it");
	Check(!AtMouth(mouth + NiPoint3{0.0f, -0.25f, 0.0f}, mouth, s), "at the chest: not the mouth");
	// Wider since 2026-10-26 (Nadi: "Mundfläche zum essen etwas vergrößern"):
	// in his Windows log of 2026-10-25 the hand that ate was 10 to 13.5 cm
	// from the mouth, at the 13 cm edge; eat-at-mouth.txt lets the bread go
	// 0.22 m from it, at the chest.
	Check(std::fabs(s.mouthRadius - 0.17f) < 1e-6f, "the reach round the mouth: 0.17 m (0.13 until 2026-10-26)");
	Check(AtMouth(mouth + NiPoint3{0.0f, -0.15f, 0.0f}, mouth, s), "the hand 15 cm under the mouth: at it");
	Check(!AtMouth(mouth + NiPoint3{0.0f, -0.22f, 0.0f}, mouth, s), "22 cm under it, the chest's stow: not the mouth");
	StowSettings off = s;
	off.eatAtMouth = false;
	Check(!AtMouth(mouth, mouth, off), "EatAtMouth off: no mouth");
}

void TestEat() {
	std::printf("Letting go at the mouth\n");
	Check(game::IsEdibleType(0x19) && game::IsEdibleType(0x28), "an ingredient and a potion are eaten and drunk");
	Check(!game::IsEdibleType(0x21) && !game::IsEdibleType(0x15) && !game::IsEdibleType(0x1B),
	      "a weapon, a book, a misc item are not");
	const StowSettings settings;
	{
		StowState s;
		StowVerdict v = StepStow(s, HeldAtMouth(true, true), settings);
		Check(v.atMouth && !v.atBody && v.showSpot, "food at the mouth: letting go eats it");
		v = StepStow(s, LetGoAtMouth(kRef), settings);
		Check(v.take == 0 && v.waiting, "let go, the engine still holds it: wait");
		v = StepStow(s, LetGoAtMouth(0), settings);
		Check(v.take == kRef && v.eat, "the engine let go: taken and eaten");
		v = StepStow(s, LetGoAtMouth(0), settings);
		Check(v.take == 0 && !v.eat, "once");
	}
	{
		StowState s;
		StowVerdict v = StepStow(s, HeldAtMouth(true, false), settings);
		Check(!v.atMouth, "something not to eat at the mouth: no mouth");
		StowInput open = LetGoAtMouth(0);
		open.heldIsEdible = false;
		v = StepStow(s, open, settings);
		Check(v.take == 0 && !v.eat, "let go there: dropped as anywhere");
	}
	{
		StowState s;
		StepStow(s, HeldAtMouth(true, true, kChest), settings);
		const StowVerdict v = StepStow(s, LetGoAtMouth(0, true, kChest), settings);
		Check(v.take == kRef && v.eat, "at the mouth and the chest's edge at once: the mouth wins");
	}
	{
		StowState s;
		const StowVerdict held = StepStow(s, HeldAtMouth(false, true, kChest), settings);
		Check(held.atBody && !held.atMouth, "food at the chest: lit for the stow");
		const StowVerdict v = StepStow(s, LetGoAtMouth(0, false, kChest), settings);
		Check(v.take == kRef && !v.eat, "let go at the chest: into the inventory, not eaten");
	}
	{
		// The hand comes off the mouth as it opens.
		StowState s;
		StepStow(s, HeldAtMouth(true, true), settings);
		StepStow(s, HeldAtMouth(false, true), settings);
		const StowVerdict v = StepStow(s, LetGoAtMouth(0, false), settings);
		Check(v.take == kRef && v.eat, "let go a frame after leaving the mouth: still eaten");
	}
	{
		StowState s;
		StepStow(s, HeldAtMouth(true, true), settings);
		for (int i = 0; i < 40; ++i) {
			StepStow(s, HeldAtMouth(false, true), settings);  // 0.44 s away
		}
		const StowVerdict v = StepStow(s, LetGoAtMouth(0, false), settings);
		Check(v.take == 0 && !v.eat, "moved away again, then let go: dropped");
	}
	{
		StowSettings off = settings;
		off.enabled = false;
		StowState s;
		StepStow(s, HeldAtMouth(true, true), off);
		const StowVerdict v = StepStow(s, LetGoAtMouth(0), off);
		Check(v.take == 0 && !v.eat, "StowAtBody off: the mouth does nothing either");
	}
}

// Eaten wherever it is held (Nadi, 2026-10-28: "egal wo gehalten an den
// mund geführt -> eat", let go there): the held thing's bound within
// MouthThingReachMetres of the mouth counts as well as the hand.
void TestThingAtMouth() {
	std::printf("The held thing at the mouth\n");
	StowSettings s;
	Check(ThingAtMouth(false, 0.02f, s), "the hand far, the bottle's end at the lips: at the mouth");
	Check(ThingAtMouth(true, 0.30f, s), "the hand at the mouth: as before");
	Check(!ThingAtMouth(false, 0.10f, s), "the thing 10 cm off, the hand far: not");
	Check(!ThingAtMouth(false, -1.0f, s), "no bound read, the hand far: not");
	StowSettings off = s;
	off.eatAtMouth = false;
	Check(!ThingAtMouth(true, 0.0f, off), "EatAtMouth off: never");
	Check(std::fabs(s.mouthThingReach - 0.05f) < 1e-6f, "the thing's reach: 5 cm (a first guess)");
}

int main() {
	TestThingAtMouth();
	TestZone();
	TestMouth();
	TestEat();
	TestSpotPose();
	TestDrawnHand();
	TestFlows();
	TestActivate();
	TestPlace();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
