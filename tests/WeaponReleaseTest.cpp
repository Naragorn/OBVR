// Checks the drawn weapon let go of (game/WeaponRelease.h): the hold closing
// on the drawn weapon arms the release, the hold opening past the release
// time lets it go - sheathed at a holster zone, into the inventory at the
// chest, dropped or thrown anywhere else, kept when it is a quest item - and
// the dropped reference watched until its body is there for the hand's
// speed.

#include <cstdio>

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
	Check(v.drop && s.phase == game::WeaponReleasePhase::Idle, "open past 0.12 s: dropped, idle after");
	v = game::StepWeaponRelease(s, Drawn(false), settings);
	Check(!v.drop, "the next frame: dropped once only");
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
	}
	{
		game::WeaponReleaseState s;
		game::StepWeaponRelease(s, Drawn(true), settings);
		game::WeaponReleaseInput in = Drawn(false);
		in.atChest = true;
		const game::WeaponReleaseVerdict v = Open(s, in, 0.3f, settings);
		Check(v.stow && !v.drop && !v.sheathe, "at the chest: into the inventory, not dropped");
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

int main() {
	TestArming();
	TestFlickerGuard();
	TestWhereItGoes();
	TestCancels();
	TestDroppedWatch();
	TestWords();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all ok\n");
	return 0;
}
