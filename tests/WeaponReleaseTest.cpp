// Checks the drawn weapon let go of (game/WeaponRelease.h): the hold closing
// on the drawn weapon arms the release, the hold opening past the release
// time lets it go - sheathed at a holster zone, into the inventory at the
// chest, dropped or thrown anywhere else, kept when it is a quest item - and
// the dropped reference watched until its body is there for the hand's
// speed.

#include <cstdio>
#include <limits>

#include "game/WeaponRelease.h"

using namespace obvr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

game::WeaponReleaseSettings Settings() {
	game::WeaponReleaseSettings s;
	s.releaseSeconds = 0.12f;
	return s;
}

game::WeaponReleaseInput Drawn(bool holdClosed) {
	game::WeaponReleaseInput in;
	in.allowed = true;
	in.equipped = vr::EquippedKind::OneHand;
	in.seen = vr::WeaponSeen::Drawn;
	in.holdClosed = holdClosed;
	in.dt = 1.0f / 90.0f;
	return in;
}

// Runs the hold open for `seconds` and returns the last verdict.
game::WeaponReleaseVerdict Open(game::WeaponReleaseState& s, game::WeaponReleaseInput in, float seconds,
                                const game::WeaponReleaseSettings& settings) {
	in.holdClosed = false;
	game::WeaponReleaseVerdict v;
	for (float t = 0.0f; t < seconds; t += in.dt) {
		v = game::StepWeaponRelease(s, in, settings);
		if (v.drop || v.sheathe || v.stow || v.refused) {
			return v;
		}
	}
	return v;
}

void TestArming() {
	std::printf("Arming: the hold has to close on the drawn weapon first\n");
	const game::WeaponReleaseSettings settings = Settings();
	game::WeaponReleaseState s;
	game::WeaponReleaseInput in = Drawn(false);
	game::WeaponReleaseVerdict v = game::StepWeaponRelease(s, in, settings);
	Check(!v.armed && s.phase == game::WeaponReleasePhase::Idle, "drawn with the hand open: nothing (a weapon from the inventory stays)");
	v = Open(s, in, 1.0f, settings);
	Check(!v.drop && !v.sheathe && !v.stow, "the hand open for a second: still nothing - never armed");
	in.holdClosed = true;
	v = game::StepWeaponRelease(s, in, settings);
	Check(v.armed && s.phase == game::WeaponReleasePhase::Armed, "the hold closes on it: armed, said once");
	v = game::StepWeaponRelease(s, in, settings);
	Check(!v.armed && s.phase == game::WeaponReleasePhase::Armed, "still closed: armed, not said again");

	game::WeaponReleaseState sheathed;
	in = Drawn(true);
	in.seen = vr::WeaponSeen::Sheathed;
	v = game::StepWeaponRelease(sheathed, in, settings);
	Check(!v.armed && sheathed.phase == game::WeaponReleasePhase::Idle, "a sheathed weapon with the hold closed: not armed");
	game::WeaponReleaseState fists;
	in = Drawn(true);
	in.equipped = vr::EquippedKind::Nothing;
	v = game::StepWeaponRelease(fists, in, settings);
	Check(!v.armed && fists.phase == game::WeaponReleasePhase::Idle, "fists with the hold closed: not armed");
	game::WeaponReleaseState off;
	game::WeaponReleaseSettings disabled = Settings();
	disabled.enabled = false;
	v = game::StepWeaponRelease(off, Drawn(true), disabled);
	Check(!v.armed && off.phase == game::WeaponReleasePhase::Idle, "the option off: never armed");
}

void TestFlickerGuard() {
	std::printf("The release time: a flicker of the hold drops nothing\n");
	const game::WeaponReleaseSettings settings = Settings();
	game::WeaponReleaseState s;
	game::StepWeaponRelease(s, Drawn(true), settings);
	game::WeaponReleaseVerdict v = Open(s, Drawn(false), 0.08f, settings);
	Check(!v.drop && s.phase == game::WeaponReleasePhase::Opening, "open 0.08 s: opening, nothing yet");
	v = game::StepWeaponRelease(s, Drawn(true), settings);
	Check(!v.drop && !v.armed && s.phase == game::WeaponReleasePhase::Armed, "closed again: armed as before, quietly");
	v = Open(s, Drawn(false), 0.08f, settings);
	Check(!v.drop, "open 0.08 s again: the time starts over");
	v = Open(s, Drawn(false), 0.2f, settings);
	Check(v.drop && s.phase == game::WeaponReleasePhase::Lowering, "open past 0.12 s: dropped, then the fists watched");
	v = game::StepWeaponRelease(s, Drawn(false), settings);
	Check(!v.drop, "the next frame: dropped once only");
}

// The release time as shipped: none since 2026-11-26 - the frame the hold
// reads open lets go, said as an opening too (the timeline of
// game/WeaponTimeline.h begins there). The tester (Windows, 2026-11-26):
// "beim werfen von fackel, schwert kann es sein das ein paar frames, ms pause
// ist bevor es wegfliegt? nervt" - the 0.03 s kept a thrown sword three
// frames in the slowing hand (34 ms at 90 Hz, "let go at 3" in every
// timeline), against a flicker the 72 let-gos of his logs never showed once.
void TestOpeningSaid() {
	std::printf("The opening: said, and the release time as shipped\n");
	const game::WeaponReleaseSettings shipped;
	Check(shipped.releaseSeconds == 0.0f, "WeaponReleaseSeconds 0 by default");
	game::WeaponReleaseState s;
	game::WeaponReleaseVerdict v = game::StepWeaponRelease(s, Drawn(true), shipped);
	Check(v.armed && !v.opened, "armed: no opening");
	v = game::StepWeaponRelease(s, Drawn(false), shipped);
	Check(v.opened && v.drop && !v.heldOn, "the first frame read open: said and let go in it");
	Check(s.phase == game::WeaponReleasePhase::Lowering, "then the fists watched, as after any drop");

	game::WeaponReleaseState t;
	game::WeaponReleaseInput swing = Drawn(true);
	swing.handVelocity = NiPoint3{0.0f, 300.0f, 0.0f};
	game::StepWeaponRelease(t, swing, shipped);
	swing.holdClosed = false;
	v = game::StepWeaponRelease(t, swing, shipped);
	Check(v.drop && v.velocity.y > 250.0f, "a swung hand opened: thrown in that frame, with the swing's speed");

	game::WeaponReleaseState q;
	game::WeaponReleaseInput quest = Drawn(true);
	quest.questItem = true;
	game::StepWeaponRelease(q, quest, shipped);
	quest.holdClosed = false;
	v = game::StepWeaponRelease(q, quest, shipped);
	Check(v.opened && v.refused && !v.drop, "a quest item's opening is said too, and refused in that frame");
}

// A release time set ([Hands] WeaponReleaseSeconds, for a controller whose
// grip flickers): a hold read open shorter than it is kept, with how long it
// read open, so the flicker shows in the log.
void TestFlickerTime() {
	std::printf("A release time set: the flicker guard\n");
	game::WeaponReleaseSettings guard;
	guard.releaseSeconds = 0.03f;
	game::WeaponReleaseState s;
	game::WeaponReleaseVerdict v = game::StepWeaponRelease(s, Drawn(true), guard);
	v = game::StepWeaponRelease(s, Drawn(false), guard);
	Check(v.opened && !v.drop && s.phase == game::WeaponReleasePhase::Opening, "the first frame read open: said, held");
	v = game::StepWeaponRelease(s, Drawn(false), guard);
	Check(!v.opened && !v.drop, "the next frame open: not said again, nothing let go");
	v = game::StepWeaponRelease(s, Drawn(false), guard);
	Check(!v.drop, "three frames open at 90 Hz: still held");
	v = game::StepWeaponRelease(s, Drawn(true), guard);
	Check(v.heldOn && !v.armed && s.phase == game::WeaponReleasePhase::Armed, "closed again: held on, said");
	Check(v.openSeconds > 0.02f && v.openSeconds < 0.025f, "with how long it read open");
	v = game::StepWeaponRelease(s, Drawn(true), guard);
	Check(!v.heldOn, "said once");

	game::WeaponReleaseState d;
	game::StepWeaponRelease(d, Drawn(true), guard);
	int frames = 0;
	do {
		v = game::StepWeaponRelease(d, Drawn(false), guard);
		++frames;
	} while (!v.drop && frames < 100);
	Check(v.drop && frames == 4, "a still hand opened at 90 Hz lets go in its fourth open frame");
	Check(!v.heldOn && !v.opened, "a drop after the guard is no opening");
}

// After a drop the slot empties and the game keeps its ready state: the
// fists come up on an open hand (the tester, 2026-10-24: a bug, the hand
// opened to let go). The ready click lowers them once they show.
void TestFistsAfterDrop() {
	std::printf("The fists after a drop\n");
	const game::WeaponReleaseSettings settings = Settings();
	game::WeaponReleaseState s;
	game::StepWeaponRelease(s, Drawn(true), settings);
	game::WeaponReleaseVerdict v = Open(s, Drawn(false), 0.3f, settings);
	Check(v.drop, "dropped");
	v = game::StepWeaponRelease(s, Drawn(false), settings);
	Check(!v.lowerFists && !v.cancelled && s.phase == game::WeaponReleasePhase::Lowering,
	      "the frame after, the sword still shows: waits, no cancel");
	game::WeaponReleaseInput empty = Drawn(false);
	empty.equipped = vr::EquippedKind::Nothing;
	v = game::StepWeaponRelease(s, empty, settings);
	Check(v.lowerFists && s.phase == game::WeaponReleasePhase::Idle, "the slot empty with the fists up: lowered, idle");
	v = game::StepWeaponRelease(s, empty, settings);
	Check(!v.lowerFists, "once");
	{
		game::WeaponReleaseState t;
		game::StepWeaponRelease(t, Drawn(true), settings);
		Open(t, Drawn(false), 0.3f, settings);
		game::WeaponReleaseInput down = empty;
		down.seen = vr::WeaponSeen::Sheathed;
		v = game::StepWeaponRelease(t, down, settings);
		Check(!v.lowerFists && t.phase == game::WeaponReleasePhase::Idle, "the slot empty and nothing up: nothing to lower, idle");
	}
	{
		game::WeaponReleaseState t;
		game::StepWeaponRelease(t, Drawn(true), settings);
		Open(t, Drawn(false), 0.3f, settings);
		game::WeaponReleaseInput still = Drawn(false);
		still.seen = vr::WeaponSeen::Unknown;
		for (int i = 0; i < 100; ++i) {
			v = game::StepWeaponRelease(t, still, settings);
		}
		Check(!v.lowerFists && t.phase == game::WeaponReleasePhase::Idle, "nothing shows for a second: given up quietly");
	}
	{
		game::WeaponReleaseState t;
		game::StepWeaponRelease(t, Drawn(true), settings);
		Open(t, Drawn(false), 0.3f, settings);
		game::WeaponReleaseInput menu = empty;
		menu.allowed = false;
		v = game::StepWeaponRelease(t, menu, settings);
		Check(!v.lowerFists && !v.cancelled && t.phase == game::WeaponReleasePhase::Idle, "a menu meanwhile: dropped stays dropped, nothing more");
	}
	{
		game::WeaponReleaseState t;
		game::StepWeaponRelease(t, Drawn(true), settings);
		Open(t, Drawn(false), 0.3f, settings);
		game::WeaponReleaseInput closed = empty;
		closed.holdClosed = true;
		v = game::StepWeaponRelease(t, closed, settings);
		Check(v.lowerFists, "the hand closing again right after changes nothing: lowered all the same");
	}
}

// The engine's anim update 0x005FCAB0 (read 2026-11-22, decompiled lines
// 585-600): the weapon out but not on the hand with no action running plays
// the Equip group; out of the hand's way only while an action runs.
bool EngineDrawsAgain(bool out, bool onHand, SInt32 action) {
	return out && !onHand && action == -1;
}

void TestSheathHang() {
	std::printf("The hang on the sheath waits for the Unequip\n");
	// The holster's let go as the engine sees it, frame by frame: the let go
	// frame, the ready click going out, the Unequip begun. Hung in the let
	// go's frame (0cb6d33) the weapon read out and off the hand with no
	// action - the engine drew it again (Nadi, Windows 2026-11-22).
	Check(EngineDrawsAgain(true, false, -1), "the old hang: out, off the hand, no action - the engine draws it again");
	game::SheathHangState h;
	game::SheathHangStep step = game::StepSheathHang(h, true, true, false, vr::WeaponSeen::Drawn, 0.011f);
	Check(step == game::SheathHangStep::None && h.pending, "the let go's frame: the hang waits, the weapon in the hand");
	Check(!EngineDrawsAgain(true, true, -1), "and the engine leaves a weapon in the hand alone");
	step = game::StepSheathHang(h, false, true, false, vr::WeaponSeen::Drawn, 0.011f);
	Check(step == game::SheathHangStep::None && h.pending, "the ready click going out: still waiting");
	step = game::StepSheathHang(h, false, true, true, vr::WeaponSeen::Drawn, 0.011f);
	Check(step == game::SheathHangStep::Hang && !h.pending, "the Unequip begun: hung now");
	Check(!EngineDrawsAgain(true, false, 1), "off the hand with the Unequip running: the engine draws nothing");
	step = game::StepSheathHang(h, false, true, true, vr::WeaponSeen::Drawn, 0.011f);
	Check(step == game::SheathHangStep::None, "hung once");

	game::SheathHangState unseen;
	game::StepSheathHang(unseen, true, true, false, vr::WeaponSeen::Drawn, 0.011f);
	step = game::StepSheathHang(unseen, false, true, false, vr::WeaponSeen::Sheathed, 0.011f);
	Check(step == game::SheathHangStep::Sheathed && !unseen.pending, "read sheathed first: nothing left to hang");

	game::SheathHangState menu;
	game::StepSheathHang(menu, true, true, false, vr::WeaponSeen::Drawn, 0.011f);
	step = game::StepSheathHang(menu, false, false, true, vr::WeaponSeen::Drawn, 0.011f);
	Check(step == game::SheathHangStep::Cancelled && !menu.pending, "a menu or death meanwhile: given up");

	game::SheathHangState slow;
	game::StepSheathHang(slow, true, true, false, vr::WeaponSeen::Drawn, 0.011f);
	int frames = 0;
	do {
		step = game::StepSheathHang(slow, false, true, false, vr::WeaponSeen::Drawn, 0.1f);
		++frames;
	} while (step == game::SheathHangStep::None && frames < 100);
	Check(step == game::SheathHangStep::GaveUp && frames == 15, "no Unequip in 1.5 s: given up");

	game::SheathHangState idle;
	step = game::StepSheathHang(idle, false, true, true, vr::WeaponSeen::Drawn, 0.011f);
	Check(step == game::SheathHangStep::None && !idle.pending, "nothing asked: an Unequip of the game's own hangs nothing");
}

void TestWhereItGoes() {
	std::printf("Where the hand is when it opens\n");
	const game::WeaponReleaseSettings settings = Settings();
	{
		game::WeaponReleaseState s;
		game::StepWeaponRelease(s, Drawn(true), settings);
		game::WeaponReleaseInput in = Drawn(false);
		in.inHolsterZone = true;
		const game::WeaponReleaseVerdict v = Open(s, in, 0.3f, settings);
		Check(v.sheathe && !v.drop && !v.stow, "in a holster zone: sheathed, not dropped");
		Check(v.hangOnSheath, "in a holster zone: the hang on the sheath asked for (SheatheAtOnce), to wait for the Unequip");
	}
	{
		game::WeaponReleaseSettings slow = settings;
		slow.sheatheAtOnce = false;
		game::WeaponReleaseState s;
		game::StepWeaponRelease(s, Drawn(true), slow);
		game::WeaponReleaseInput in = Drawn(false);
		in.inHolsterZone = true;
		const game::WeaponReleaseVerdict v = Open(s, in, 0.3f, slow);
		Check(v.sheathe && !v.hangOnSheath, "SheatheAtOnce 0: sheathed by the game's animation alone");
	}
	{
		game::WeaponReleaseState s;
		game::StepWeaponRelease(s, Drawn(true), settings);
		game::WeaponReleaseInput in = Drawn(false);
		in.inHolsterZone = true;
		in.dt = 0.01f;
		const game::WeaponReleaseVerdict v = game::StepWeaponRelease(s, in, settings);
		Check(!v.sheathe && !v.hangOnSheath, "at the holster, the release time not yet passed: not hung back");
	}
	{
		game::WeaponReleaseState s;
		game::StepWeaponRelease(s, Drawn(true), settings);
		game::WeaponReleaseInput in = Drawn(false);
		in.atChest = true;
		const game::WeaponReleaseVerdict v = Open(s, in, 0.3f, settings);
		Check(v.stow && !v.drop && !v.sheathe && !v.hangOnSheath, "at the chest: into the inventory, not dropped");
	}
	{
		game::WeaponReleaseState s;
		game::StepWeaponRelease(s, Drawn(true), settings);
		game::WeaponReleaseInput in = Drawn(false);
		in.inHolsterZone = true;
		in.atChest = true;
		const game::WeaponReleaseVerdict v = Open(s, in, 0.3f, settings);
		Check(v.sheathe && !v.stow, "zone and chest at once: the holster wins");
	}
	{
		game::WeaponReleaseState s;
		game::StepWeaponRelease(s, Drawn(true), settings);
		game::WeaponReleaseInput in = Drawn(false);
		in.questItem = true;
		game::WeaponReleaseVerdict v = Open(s, in, 0.3f, settings);
		Check(v.refused && !v.drop && s.phase == game::WeaponReleasePhase::Idle, "a quest item anywhere else: refused, stays in the hand");
		v = Open(s, in, 0.3f, settings);
		Check(!v.refused, "refused once per opening");
		in.holdClosed = true;
		v = game::StepWeaponRelease(s, in, settings);
		Check(v.armed, "closing on it again arms again");
		in.questItem = true;
		in.inHolsterZone = true;
		v = Open(s, in, 0.3f, settings);
		Check(v.sheathe && !v.refused, "a quest item at the holster: sheathed as any weapon");
	}
}

void TestCancels() {
	std::printf("What ends an armed hold without a drop\n");
	const game::WeaponReleaseSettings settings = Settings();
	{
		game::WeaponReleaseState s;
		game::StepWeaponRelease(s, Drawn(true), settings);
		game::WeaponReleaseInput in = Drawn(true);
		in.allowed = false;
		const game::WeaponReleaseVerdict v = game::StepWeaponRelease(s, in, settings);
		Check(v.cancelled && s.phase == game::WeaponReleasePhase::Idle, "a menu: cancelled");
		const game::WeaponReleaseVerdict again = game::StepWeaponRelease(s, in, settings);
		Check(!again.cancelled, "said once");
	}
	{
		game::WeaponReleaseState s;
		game::StepWeaponRelease(s, Drawn(true), settings);
		game::WeaponReleaseInput in = Drawn(true);
		in.seen = vr::WeaponSeen::Sheathed;
		const game::WeaponReleaseVerdict v = game::StepWeaponRelease(s, in, settings);
		Check(v.cancelled && s.phase == game::WeaponReleasePhase::Idle, "the weapon sheathed by the game: cancelled");
	}
	{
		game::WeaponReleaseState s;
		game::StepWeaponRelease(s, Drawn(true), settings);
		Open(s, Drawn(false), 0.05f, settings);
		game::WeaponReleaseInput in = Drawn(false);
		in.allowed = false;
		const game::WeaponReleaseVerdict v = game::StepWeaponRelease(s, in, settings);
		Check(v.cancelled && !v.drop, "a menu while opening: cancelled, no drop");
	}
	{
		game::WeaponReleaseState s;
		game::WeaponReleaseInput in = Drawn(true);
		in.allowed = false;
		const game::WeaponReleaseVerdict v = game::StepWeaponRelease(s, in, settings);
		Check(!v.cancelled, "idle and not allowed: nothing to cancel");
	}
}

void TestDroppedWatch() {
	std::printf("The dropped reference watched for its body\n");
	game::DroppedWeaponState d;
	game::DroppedWeaponVerdict v = game::StepDroppedWeapon(d, false, 0.011f);
	Check(!v.sendOff && !v.gaveUp, "nothing dropped: nothing");
	game::BeginDroppedWeapon(d, 0x1234, NiPoint3{10.0f, 0.0f, 0.0f}, 1.0f);
	v = game::StepDroppedWeapon(d, false, 0.011f);
	Check(!v.sendOff && !v.gaveUp && d.frames == 1, "no body yet: waits, a frame counted");
	v = game::StepDroppedWeapon(d, false, 0.011f);
	v = game::StepDroppedWeapon(d, true, 0.011f);
	Check(v.sendOff && v.ref == 0x1234 && v.frames == 3 && v.velocity.x == 10.0f, "the body there: sent off with the kept speed, the frames told");
	Check(d.ref == 0, "then forgotten");
	v = game::StepDroppedWeapon(d, true, 0.011f);
	Check(!v.sendOff, "once");

	game::BeginDroppedWeapon(d, 0x1234, NiPoint3{0.0f, 0.0f, 0.0f}, 0.05f);
	for (int i = 0; i < 10 && !v.gaveUp; ++i) {
		v = game::StepDroppedWeapon(d, false, 0.011f);
	}
	Check(v.gaveUp && v.ref == 0x1234 && d.ref == 0, "no body in time: given up, forgotten");

	game::BeginDroppedWeapon(d, 0x1234, NiPoint3{0.0f, 0.0f, 0.0f}, 1.0f);
	game::BeginDroppedWeapon(d, 0x5678, NiPoint3{0.0f, 0.0f, 0.0f}, 1.0f);
	v = game::StepDroppedWeapon(d, true, 0.011f);
	Check(v.ref == 0x5678, "a second drop replaces the first watch");
}

void TestWords() {
	std::printf("The log's words\n");
	Check(game::WeaponReleaseWord(game::WeaponReleaseVerdict{}) == nullptr, "nothing: no word");
	game::WeaponReleaseVerdict v;
	v.sheathe = true;
	Check(game::WeaponReleaseWord(v) != nullptr, "sheathed has a word");
}

}  // namespace

// The throw's speed is the hand's at the moment the hold opens (Nadi,
// Windows, 2026-10-24: "Waffe werfen geht nicht" - all ten drops read 0.00
// m/s): the drop comes releaseSeconds after the opening, and the eight
// frames before it were the follow-through, not the throw.
game::WeaponReleaseInput Moving(bool holdClosed, float unitsPerSecond) {
	game::WeaponReleaseInput in = Drawn(holdClosed);
	in.handVelocity = NiPoint3{0.0f, unitsPerSecond, 0.0f};
	return in;
}

float DropSpeed(game::WeaponReleaseState& s, const game::WeaponReleaseSettings& settings, float afterSpeed,
                bool& dropped) {
	dropped = false;
	for (int frame = 0; frame < 30 && !dropped; ++frame) {
		const game::WeaponReleaseVerdict v = game::StepWeaponRelease(s, Moving(false, afterSpeed), settings);
		if (v.drop) {
			dropped = true;
			return math::Sqrt(v.velocity.LengthSquared());
		}
	}
	return -1.0f;
}

void TestThrowSpeed() {
	std::printf("The throw's speed taken at the opening\n");
	const game::WeaponReleaseSettings settings = Settings();
	bool dropped = false;
	{
		game::WeaponReleaseState s;
		for (int frame = 0; frame < 20; ++frame) {
			game::StepWeaponRelease(s, Moving(true, frame < 15 ? 20.0f : 300.0f), settings);
		}
		game::StepWeaponRelease(s, Moving(false, 300.0f), settings);
		const float speed = DropSpeed(s, settings, 10.0f, dropped);
		Check(dropped && speed > 299.0f && speed < 301.0f,
		      "swung at 300 units/s, opened at the peak, slowed to 10 while the release time ran: thrown at 300");
	}
	{
		game::WeaponReleaseState s;
		for (int frame = 0; frame < 20; ++frame) {
			game::StepWeaponRelease(s, Moving(true, 0.0f), settings);
		}
		const float speed = DropSpeed(s, settings, 0.0f, dropped);
		Check(dropped && speed == 0.0f, "a still hand: a fall, no speed");
	}
	{
		game::WeaponReleaseState s;
		for (int frame = 0; frame < 20; ++frame) {
			game::StepWeaponRelease(s, Moving(true, frame < 5 ? 400.0f : 0.0f), settings);
		}
		const float speed = DropSpeed(s, settings, 0.0f, dropped);
		Check(dropped && speed == 0.0f, "fast fifteen frames before the opening, still at it: a fall");
	}
	{
		game::WeaponReleaseState s;
		for (int frame = 0; frame < 20; ++frame) {
			game::StepWeaponRelease(s, Moving(true, 0.0f), settings);
		}
		game::StepWeaponRelease(s, Moving(false, 0.0f), settings);
		game::StepWeaponRelease(s, Moving(false, 500.0f), settings);
		const float speed = DropSpeed(s, settings, 0.0f, dropped);
		Check(dropped && speed == 0.0f,
		      "the hand moving on only after the opening (a follow-through, a hand swept on): no throw");
	}
	{
		game::WeaponReleaseState s;
		for (int frame = 0; frame < 20; ++frame) {
			game::StepWeaponRelease(s, Moving(true, 300.0f), settings);
		}
		game::StepWeaponRelease(s, Moving(false, 300.0f), settings);  // a flicker open...
		for (int frame = 0; frame < 20; ++frame) {
			game::StepWeaponRelease(s, Moving(true, 0.0f), settings);  // ...closed again, the hand stopped
		}
		const float speed = DropSpeed(s, settings, 0.0f, dropped);
		Check(dropped && speed == 0.0f, "a flicker's speed is not kept for the real opening later");
	}
	{
		game::WeaponReleaseSettings none = settings;
		none.throwStrength = 0.0f;
		game::WeaponReleaseState s;
		for (int frame = 0; frame < 20; ++frame) {
			game::StepWeaponRelease(s, Moving(true, 300.0f), none);
		}
		const float speed = DropSpeed(s, none, 300.0f, dropped);
		Check(dropped && speed == 0.0f, "WeaponThrowStrength 0: never thrown");
	}
}

// The spin a throw takes: the hand's turning speed in the frame of the
// fastest speed, times ThrowSpinStrength, at most kThrowSpinMostRadians (Nadi,
// 2026-10-26: a throw with a flick of the wrist should tumble).
game::WeaponReleaseInput Spinning(bool holdClosed, float unitsPerSecond, float radiansPerSecond) {
	game::WeaponReleaseInput in = Moving(holdClosed, unitsPerSecond);
	in.handAngularVelocity = NiPoint3{radiansPerSecond, 0.0f, 0.0f};
	return in;
}

game::WeaponReleaseVerdict DropVerdict(game::WeaponReleaseState& s, const game::WeaponReleaseSettings& settings) {
	for (int frame = 0; frame < 30; ++frame) {
		const game::WeaponReleaseVerdict v = game::StepWeaponRelease(s, Spinning(false, 0.0f, 0.0f), settings);
		if (v.drop) {
			return v;
		}
	}
	return game::WeaponReleaseVerdict{};
}

void TestThrowSpin() {
	std::printf("The throw's spin\n");
	const game::WeaponReleaseSettings settings = Settings();
	{
		game::WeaponReleaseState s;
		for (int frame = 0; frame < 20; ++frame) {
			game::StepWeaponRelease(s, Spinning(true, frame < 15 ? 20.0f : 300.0f, frame < 15 ? 1.0f : 8.0f),
			                        settings);
		}
		const game::WeaponReleaseVerdict v = DropVerdict(s, settings);
		Check(v.drop && v.spin.x > 7.99f && v.spin.x < 8.01f,
		      "flicked at 8 radians a second in the throw's fastest frame: spun at 8");
	}
	{
		game::WeaponReleaseState s;
		for (int frame = 0; frame < 20; ++frame) {
			game::StepWeaponRelease(s, Spinning(true, frame == 16 ? 300.0f : 20.0f, frame == 18 ? 12.0f : 2.0f),
			                        settings);
		}
		const game::WeaponReleaseVerdict v = DropVerdict(s, settings);
		Check(v.drop && v.spin.x > 1.99f && v.spin.x < 2.01f,
		      "the spin of the fastest frame, not the fastest spin: a turn after the throw's peak is not it");
	}
	{
		game::WeaponReleaseState s;
		for (int frame = 0; frame < 20; ++frame) {
			game::StepWeaponRelease(s, Spinning(true, 0.0f, 10.0f), settings);
		}
		const game::WeaponReleaseVerdict v = DropVerdict(s, settings);
		Check(v.drop && v.spin.LengthSquared() == 0.0f, "a still hand turning: a fall, no spin");
	}
	{
		game::WeaponReleaseSettings half = settings;
		half.throwSpinStrength = 0.5f;
		game::WeaponReleaseState s;
		for (int frame = 0; frame < 20; ++frame) {
			game::StepWeaponRelease(s, Spinning(true, 300.0f, 8.0f), half);
		}
		const game::WeaponReleaseVerdict v = DropVerdict(s, half);
		Check(v.drop && v.spin.x > 3.99f && v.spin.x < 4.01f, "ThrowSpinStrength 0.5: half the spin");
	}
	{
		game::WeaponReleaseState s;
		for (int frame = 0; frame < 20; ++frame) {
			game::StepWeaponRelease(s, Spinning(true, 300.0f, 100.0f), settings);
		}
		const game::WeaponReleaseVerdict v = DropVerdict(s, settings);
		Check(v.drop && v.spin.x > game::kThrowSpinMostRadians - 0.01f && v.spin.x < game::kThrowSpinMostRadians + 0.01f,
		      "a tracking glitch of 100 radians a second: held to the most");
	}
	{
		game::WeaponReleaseSettings none = settings;
		none.throwSpinStrength = 0.0f;
		game::WeaponReleaseState s;
		for (int frame = 0; frame < 20; ++frame) {
			game::StepWeaponRelease(s, Spinning(true, 300.0f, 8.0f), none);
		}
		const game::WeaponReleaseVerdict v = DropVerdict(s, none);
		Check(v.drop && v.spin.LengthSquared() == 0.0f && v.velocity.LengthSquared() > 0.0f,
		      "ThrowSpinStrength 0: thrown, not spun");
	}
	{
		game::DroppedWeaponState d;
		game::BeginDroppedWeapon(d, 0x1234, NiPoint3{10.0f, 0.0f, 0.0f}, 1.0f, NiPoint3{0.0f, 0.0f, 6.0f});
		const game::DroppedWeaponVerdict v = game::StepDroppedWeapon(d, true, 1.0f / 90.0f);
		Check(v.sendOff && v.spin.z == 6.0f, "the dropped weapon's watch keeps the spin for its body");
	}
}

// The dropped weapon's group back only once it is clear of the hands'
// keyframed bodies and the player's capsule (weapon-drop-sweep.txt: OBVR gave
// it 0 units/s and the hand sweeping on knocked it away at 551).
void TestDroppedClear() {
	std::printf("The dropped weapon clear of the hands and the player\n");
	const NiPoint3 player{0.0f, 0.0f, 0.0f};
	const float capsule = 25.0f;
	const NiPoint3 far{200.0f, 0.0f, 0.0f};
	Check(!game::DroppedWeaponClear(NiPoint3{60.0f, 0.0f, 0.0f}, 20.0f, player, capsule, true, NiPoint3{62.0f, 0.0f, 0.0f},
	                                 true, far),
	      "the right hand still at it: not clear");
	Check(!game::DroppedWeaponClear(NiPoint3{60.0f, 0.0f, 0.0f}, 20.0f, player, capsule, true, far, true,
	                                 NiPoint3{60.0f, 25.0f, 0.0f}),
	      "the left hand within its reach and a hand's margin: not clear");
	Check(game::DroppedWeaponClear(NiPoint3{60.0f, 0.0f, 0.0f}, 20.0f, player, capsule, true, far, true, far),
	      "both hands away, outside the capsule: clear");
	Check(!game::DroppedWeaponClear(NiPoint3{30.0f, 0.0f, 0.0f}, 20.0f, player, capsule, true, far, true, far),
	      "its bound reaching into the capsule: not clear");
	Check(game::DroppedWeaponClear(NiPoint3{60.0f, 0.0f, -80.0f}, 20.0f, player, capsule, true, far, true, far),
	      "the capsule on the horizontal only: lying below the hand's height counts as out");
	Check(game::DroppedWeaponClear(NiPoint3{60.0f, 0.0f, 0.0f}, 20.0f, player, capsule, false,
	                                NiPoint3{60.0f, 0.0f, 0.0f}, false, NiPoint3{60.0f, 0.0f, 0.0f}),
	      "untracked hands do not hold it");
	Check(game::DroppedWeaponClear(NiPoint3{60.0f, 0.0f, 0.0f}, 20.0f, player, 0.0f, true, far, true, far),
	      "no capsule read: the hands alone decide");
}

// A closed grip reaching for a thing let go of at the body ends its pass
// through the player at once (game::GripReachesFor): in the player's group
// the engine's pick passes through it, and the grip's pull never took the
// tester's sword back (Windows, 2026-11-26).
void TestGripEndsThePass() {
	std::printf("A grip reaching for a thing let go of at the body\n");
	Check(game::GripReachesFor(true, true, true, 0x8C6222B4u) == 0x8C6222B4u,
	      "a closed grip whose reach runs, the pick on the thing: reached for");
	Check(game::GripReachesFor(false, true, true, 0x8C6222B4u) == 0u, "the grip open: nothing reached for");
	Check(game::GripReachesFor(true, false, true, 0x8C6222B4u) == 0u,
	      "the grip's first frame, its reach not running yet: nothing reached for");
	Check(game::GripReachesFor(true, true, false, 0x8C6222B4u) == 0u, "no pick: nothing reached for");
}

// The weapon the same as the torch: the hand opened 0.11 s after the
// swing's peak still throws with it, and the verdict carries the peaks.
void TestLateOpening() {
	std::printf("Opened a little after the peak\n");
	const game::WeaponReleaseSettings settings = Settings();
	game::WeaponReleaseState s;
	game::WeaponReleaseInput in = Drawn(true);
	game::StepWeaponRelease(s, in, settings);
	in.handVelocity = NiPoint3{0.0f, 300.0f, 0.0f};
	game::StepWeaponRelease(s, in, settings);
	in.handVelocity = NiPoint3{0.0f, 20.0f, 0.0f};
	for (int i = 0; i < 10; ++i) {
		game::StepWeaponRelease(s, in, settings);
	}
	const game::WeaponReleaseVerdict v = Open(s, in, 0.5f, settings);
	Check(v.drop && v.velocity.y > 250.0f, "thrown with the peak 11 frames before the opening");
	Check(v.peaks.lookUnits > 299.0f && v.peaks.lookAgeSeconds > 0.1f && v.peaks.lookAgeSeconds < 0.14f,
	      "the verdict says the peak and how long before the opening it was");
}

// Half-swording (vr/HalfSwordLogic.h): the weapon hand lets go while the
// other hand holds the blade - the weapon stays, held by that hand; the other
// hand letting go too drops it.
void TestHeldByOther() {
	std::printf("The other hand holds it\n");
	const game::WeaponReleaseSettings settings = Settings();
	{
		game::WeaponReleaseState s;
		game::StepWeaponRelease(s, Drawn(true), settings);
		game::WeaponReleaseInput in = Drawn(false);
		in.heldByOther = true;
		game::WeaponReleaseVerdict v = Open(s, in, 1.0f, settings);
		Check(!v.drop && !v.sheathe && !v.stow && !v.refused, "opened while the other hand holds it: nothing happens");
		in.inHolsterZone = true;
		v = Open(s, in, 0.3f, settings);
		Check(!v.sheathe, "not even in a holster zone");
		in.inHolsterZone = false;
		in.holdClosed = true;
		v = game::StepWeaponRelease(s, in, settings);
		Check(v.heldOn && s.phase == game::WeaponReleasePhase::Armed, "closed on it again: held as before");
	}
	{
		game::WeaponReleaseState s;
		game::StepWeaponRelease(s, Drawn(true), settings);
		game::WeaponReleaseInput in = Drawn(false);
		in.heldByOther = true;
		Open(s, in, 1.0f, settings);
		in.heldByOther = false;
		const game::WeaponReleaseVerdict v = game::StepWeaponRelease(s, in, settings);
		Check(v.drop, "the other hand lets go while this one is open: dropped at once");
	}
}

bool NearPoint(const NiPoint3& a, const NiPoint3& b) {
	const NiPoint3 d = a - b;
	return d.LengthSquared() < 1.0e-6f;
}

// The throw taken at the weapon's middle (WeaponMiddleVelocity): the hand's
// speed plus its turn carried out to the middle.
void TestThrowFromMiddle() {
	const NiPoint3 hand{0.0f, 210.0f, 0.0f};  // 3 m/s forward
	const NiPoint3 controller{10.0f, 20.0f, 30.0f};
	const NiPoint3 up{10.0f, 20.0f, 55.0f};   // the middle 25 units above the controller
	const NiPoint3 still{0.0f, 0.0f, 0.0f};
	const NiPoint3 flick{10.0f, 0.0f, 0.0f};  // turning about +x: a point above goes to -y
	float from = -1.0f;
	NiPoint3 v = game::WeaponMiddleVelocity(hand, flick, controller, up, false, &from);
	Check(NearPoint(v, hand) && from == 0.0f, "no middle read: the controller's speed");
	v = game::WeaponMiddleVelocity(hand, still, controller, up, true, &from);
	Check(NearPoint(v, hand) && from > 24.9f && from < 25.1f, "a hand not turning: the middle moves as the hand");
	v = game::WeaponMiddleVelocity(hand, flick, controller, up, true, &from);
	Check(NearPoint(v, NiPoint3{0.0f, -40.0f, 0.0f}), "the middle above, turned back: the hand's 210 less 10 x 25");
	const NiPoint3 w = game::WeaponMiddleVelocity(still, flick, controller, up, true);
	Check(NearPoint(w, NiPoint3{0.0f, -250.0f, 0.0f}), "w x r: about +x, a point above goes to -y (right-handed)");
	const NiPoint3 back{10.0f, 20.0f, 5.0f};  // the middle below: the same turn takes it the other way
	v = game::WeaponMiddleVelocity(hand, flick, controller, back, true);
	Check(NearPoint(v, NiPoint3{0.0f, 460.0f, 0.0f}), "the middle below the turning hand: 210 plus 250");
	const NiPoint3 far{10.0f, 20.0f, 30.0f + game::kThrowMiddleMostUnits + 1.0f};
	v = game::WeaponMiddleVelocity(hand, flick, controller, far, true, &from);
	Check(NearPoint(v, hand) && from == 0.0f, "a middle off the hand: the controller's speed");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	v = game::WeaponMiddleVelocity(hand, flick, controller, NiPoint3{nan, 0.0f, 0.0f}, true, &from);
	Check(NearPoint(v, hand) && from == 0.0f, "a middle not finite: the controller's speed");
	v = game::WeaponMiddleVelocity(hand, NiPoint3{nan, 0.0f, 0.0f}, controller, up, true, &from);
	Check(NearPoint(v, hand) && from == 0.0f, "a turning speed not finite: the controller's speed");
	// Through the release: the middle's speed in, thrown with it.
	const game::WeaponReleaseSettings settings = Settings();
	game::WeaponReleaseState s;
	game::WeaponReleaseInput in = Drawn(true);
	in.handVelocity = game::WeaponMiddleVelocity(hand, flick, controller, back, true);
	game::StepWeaponRelease(s, in, settings);
	in.handVelocity = NiPoint3{0.0f, 0.0f, 0.0f};
	const game::WeaponReleaseVerdict r = Open(s, in, 1.0f, settings);
	Check(r.drop && r.velocity.y > 459.0f && r.velocity.y < 461.0f, "released: thrown at the middle's 460 units a second");
}

int main() {
	TestThrowFromMiddle();
	TestLateOpening();
	TestArming();
	TestFlickerGuard();
	TestOpeningSaid();
	TestFlickerTime();
	TestWhereItGoes();
	TestSheathHang();
	TestHeldByOther();
	TestCancels();
	TestFistsAfterDrop();
	TestDroppedWatch();
	TestThrowSpeed();
	TestThrowSpin();
	TestDroppedClear();
	TestGripEndsThePass();
	TestWords();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all ok\n");
	return 0;
}
