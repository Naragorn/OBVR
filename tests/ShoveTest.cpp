// Checks the shove's decisions (game/ShoveLogic.h): when a hand shoves, how
// hard, its speed towards the actor, the reach, and the cooldown.

#include <cstdio>
#include <limits>
#include <cstring>

#include "game/ShoveLogic.h"

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

bool Near(float a, float b) { return a - b < 1e-3f && b - a < 1e-3f; }

ShoveHand Open(float speed) {
	ShoveHand h;
	h.valid = true;
	h.open = true;
	h.towardsSpeed = speed;
	return h;
}

void TestKind() {
	std::printf("When a hand shoves\n");
	const ShoveSettings s;
	Check(ShoveFor(s, false, Open(1.0f)) == ShoveKind::None, "slower than the shove speed: nothing");
	Check(ShoveFor(s, false, Open(3.5f)) == ShoveKind::Light, "from 3 m/s (the one stagger speed): a light shove");
	Check(ShoveFor(s, false, Open(6.5f)) == ShoveKind::Hard, "from 6 m/s (the one knockdown speed): a hard one");
	Check(ShoveFor(s, true, Open(4.5f)) == ShoveKind::None, "a weapon or the fists up: a swing, no shove");
	ShoveHand fist = Open(4.5f);
	fist.open = false;
	Check(ShoveFor(s, false, fist) == ShoveKind::None, "a fist: no shove");
	ShoveHand gripping = Open(4.5f);
	gripping.gripHeld = true;
	Check(ShoveFor(s, false, gripping) == ShoveKind::None, "the grip closed (holding, reaching): no shove");
	ShoveHand lost = Open(4.5f);
	lost.valid = false;
	Check(ShoveFor(s, false, lost) == ShoveKind::None, "the hand not tracked: nothing");
	ShoveSettings off;
	off.enabled = false;
	Check(ShoveFor(off, false, Open(9.0f)) == ShoveKind::None, "switched off: nothing");
	volatile float zero = 0.0f;
	Check(ShoveFor(s, false, Open(zero / zero)) == ShoveKind::None, "a speed that is not a number: nothing");
}

// Why a hand at someone did not shove (Debug.ShoveTrace): the first of
// ShoveFor's conditions that failed, in its order, and None when it shoves.
void TestRefusal() {
	std::printf("Why a hand did not shove\n");
	const ShoveSettings s;
	Check(ShoveRefusalFor(s, false, Open(3.5f)) == ShoveRefusal::None, "a shove: nothing refused");
	ShoveSettings off;
	off.enabled = false;
	Check(ShoveRefusalFor(off, false, Open(9.0f)) == ShoveRefusal::Off, "switched off: off");
	Check(ShoveRefusalFor(s, true, Open(4.5f)) == ShoveRefusal::WeaponDrawn, "a weapon or the fists up");
	ShoveHand lost = Open(4.5f);
	lost.valid = false;
	Check(ShoveRefusalFor(s, false, lost) == ShoveRefusal::NotTracked, "the hand not tracked");
	ShoveHand fist = Open(4.5f);
	fist.open = false;
	Check(ShoveRefusalFor(s, false, fist) == ShoveRefusal::Fist, "a fist");
	ShoveHand gripping = Open(4.5f);
	gripping.gripHeld = true;
	Check(ShoveRefusalFor(s, false, gripping) == ShoveRefusal::GripHeld, "the grip held");
	Check(ShoveRefusalFor(s, false, Open(1.0f)) == ShoveRefusal::TooSlow, "slower than the shove speed");
	volatile float zero = 0.0f;
	Check(ShoveRefusalFor(s, false, Open(zero / zero)) == ShoveRefusal::TooSlow,
	      "a speed that is not a number: too slow");
	ShoveHand both = Open(1.0f);
	both.open = false;
	Check(ShoveRefusalFor(s, false, both) == ShoveRefusal::Fist, "a slow fist: the fist named first, as ShoveFor asks");
	Check((ShoveRefusalFor(s, false, Open(6.5f)) == ShoveRefusal::None) ==
	          (ShoveFor(s, false, Open(6.5f)) != ShoveKind::None),
	      "None exactly when ShoveFor shoves (hard)");
	Check(std::strcmp(ShoveRefusalName(ShoveRefusal::TooSlow), "too slow") == 0, "named for the log: too slow");
	Check(std::strcmp(ShoveRefusalName(ShoveRefusal::None), "shoves") == 0, "named for the log: shoves");
}

// Which of an actor's nodes swells their bound (Debug.ShoveTrace): a node
// whose bound reaches further than `units` from the actor's root.
void TestBoundReach() {
	std::printf("A node's bound beyond the actor\n");
	const NiPoint3 root{100.0f, 100.0f, 0.0f};
	Check(!BoundReachesBeyond(NiPoint3{100.0f, 100.0f, 60.0f}, 70.0f, root, 200.0f), "a body's own bound: inside");
	Check(BoundReachesBeyond(NiPoint3{100.0f, 900.0f, 0.0f}, 5.0f, root, 200.0f), "a small node 800 units off: beyond");
	Check(BoundReachesBeyond(NiPoint3{100.0f, 100.0f, 0.0f}, 900.0f, root, 200.0f), "a huge bound at the root: beyond");
	Check(!BoundReachesBeyond(NiPoint3{100.0f, 250.0f, 0.0f}, 50.0f, root, 200.0f), "exactly at the edge: inside");
	volatile float zero = 0.0f;
	Check(BoundReachesBeyond(NiPoint3{100.0f, 100.0f, 0.0f}, zero / zero, root, 200.0f),
	      "a radius that is not a number: beyond (worth naming)");
}

// The body's bound for the hands' tests (game::BodyBoundFor): the actor's
// whole bound while it can be a body's - no wider than 300 units round, its
// centre across the ground within its radius of the feet - and otherwise one
// from the feet to the top of the head, which a worn mesh with a broken
// bound cannot swell (the Linux beggar's shirt, 2026-11-04: 851 round, its
// centre 830 off him; his skeleton's own bound only 12.7 round, no body).
void TestBodyBound() {
	std::printf("Which bound stands for the body\n");
	const NiPoint3 feet{255.0f, -1606.0f, -256.0f};
	const NiPoint3 head{255.0f, -1604.0f, -135.0f};
	BodyBound b = BodyBoundFor(NiPoint3{255.0f, -1605.0f, -199.0f}, 75.0f, feet, true, head);
	Check(!b.rebuilt && Near(b.radius, 75.0f), "a body's own bound (Windows' beggar): kept");
	b = BodyBoundFor(NiPoint3{133.0f, -834.7f, -102.7f}, 851.6f, feet, true, head);
	Check(b.rebuilt, "the swollen bound (Linux' beggar): rebuilt");
	Check(Near(b.centre.x, 255.0f) && Near(b.centre.y, -1606.0f), "rebuilt over the feet");
	Check(Near(b.centre.z, -256.0f + (121.0f + kHeadTopUnits) * 0.5f), "its centre halfway up to the head's top");
	Check(Near(b.radius, (121.0f + kHeadTopUnits) * 0.5f), "its radius half the height");
	b = BodyBoundFor(NiPoint3{255.0f, -1300.0f, -199.0f}, 120.0f, feet, true, head);
	Check(b.rebuilt, "a bound whose centre lies further across than its radius from the feet: rebuilt");
	b = BodyBoundFor(NiPoint3{255.0f, -1606.0f, -150.0f}, 140.0f, feet, true, head);
	Check(!b.rebuilt && Near(b.radius, 140.0f), "a big creature's bound over its feet: kept");
	b = BodyBoundFor(NiPoint3{133.0f, -834.7f, -102.7f}, 851.6f, feet, false, head);
	Check(b.rebuilt && Near(b.radius, kNoHeadHeightUnits * 0.5f), "no head found: a person's height");
	volatile float zero = 0.0f;
	b = BodyBoundFor(NiPoint3{255.0f, -1605.0f, -199.0f}, zero / zero, feet, true, head);
	Check(b.rebuilt, "a radius that is not a number: rebuilt");
	b = BodyBoundFor(NiPoint3{133.0f, -834.7f, -102.7f}, 851.6f, feet, true, NiPoint3{255.0f, -1606.0f, -300.0f});
	Check(b.rebuilt && Near(b.radius, kNoHeadHeightUnits * 0.5f), "a head under the feet (lying, unread): a person's height");
}

void TestSpeedTowards() {
	std::printf("The speed towards the actor\n");
	const NiPoint3 hand{0.0f, 0.0f, 100.0f};
	const NiPoint3 target{0.0f, 50.0f, 100.0f};
	Check(Near(SpeedTowards(NiPoint3{0.0f, 3.0f, 0.0f}, hand, target), 3.0f), "straight at it: all of it");
	Check(Near(SpeedTowards(NiPoint3{3.0f, 0.0f, 0.0f}, hand, target), 0.0f), "across it: none");
	Check(Near(SpeedTowards(NiPoint3{0.0f, -2.0f, 0.0f}, hand, target), -2.0f), "away from it: negative");
	Check(Near(SpeedTowards(NiPoint3{0.0f, 0.0f, -5.0f}, hand, target), 0.0f),
	      "straight down (onto a head): no part across the ground");
	Check(Near(SpeedTowards(NiPoint3{0.0f, 3.0f, 0.0f}, hand, NiPoint3{0.0f, 0.0f, 50.0f}), 0.0f),
	      "the actor right below the hand: no direction, none");
}

void TestReach() {
	std::printf("The reach\n");
	const NiPoint3 centre{0.0f, 0.0f, 60.0f};
	Check(HandAtActor(NiPoint3{0.0f, 20.0f, 60.0f}, centre, 60.0f, 0.5f, 4.0f), "inside half the bound plus 4: at it");
	Check(!HandAtActor(NiPoint3{0.0f, 40.0f, 60.0f}, centre, 60.0f, 0.5f, 4.0f), "further: not at it");
}

void TestBody() {
	std::printf("The whole body, head to feet\n");
	// A standing NPC: its bound's centre at the waist, 64 units up, radius 64.
	const NiPoint3 centre{0.0f, 0.0f, 64.0f};
	Check(HandAtBody(NiPoint3{0.0f, 20.0f, 120.0f}, centre, 64.0f, 0.5f, 1.0f, 4.0f), "at the head: at the body");
	Check(HandAtBody(NiPoint3{0.0f, 20.0f, 70.0f}, centre, 64.0f, 0.5f, 1.0f, 4.0f), "at the chest: at the body");
	Check(!HandAtActor(NiPoint3{0.0f, 20.0f, 120.0f}, centre, 64.0f, 0.5f, 4.0f),
	      "the old ball round the waist missed the head");
	Check(!HandAtBody(NiPoint3{0.0f, 20.0f, 140.0f}, centre, 64.0f, 0.5f, 1.0f, 4.0f), "above the head: not");
	Check(!HandAtBody(NiPoint3{0.0f, 40.0f, 90.0f}, centre, 64.0f, 0.5f, 1.0f, 4.0f), "a step beside them: not");
}

void TestHardForce() {
	std::printf("the knockdown's force: they fall over, not sink in place\n");
	// Measured 2026-11-05 (knock-force-3/10/20/30, a thrown sword): at 3 the
	// pelvis sank 43 units in half a second with the head still over it; at
	// 10 the head was under the pelvis then, 28 units carried; 30 slid 115.
	const ShoveSettings s;
	Check(s.hardForce >= 10.0f && s.hardForce < 20.0f, "ShoveHardForce 10: tipped over, not thrown across the room");
	// Nadi, 2026-11-05: a force that grows with the blow, not one value.
	Check(KnockdownForceFor(s, s.hardSpeed) == s.hardForce, "a knockdown at KnockdownSpeed: ShoveHardForce");
	Check(KnockdownForceFor(s, s.hardSpeed * 1.5f) == s.hardForce * 1.5f, "half again as fast: half again the force");
	Check(KnockdownForceFor(s, 100.0f) == s.hardForceMost && s.hardForceMost > s.hardForce,
	      "a blow far past it: ShoveHardForceMost, no further");
	Check(KnockdownForceFor(s, 0.0f) == s.hardForce && KnockdownForceFor(s, s.hardSpeed * 0.5f) == s.hardForce,
	      "a speed not known (0) or under KnockdownSpeed (a power swing's knockdown): ShoveHardForce");
	Check(KnockdownForceFor(s, std::numeric_limits<float>::quiet_NaN()) == s.hardForce, "a speed that is no number: ShoveHardForce");
	ShoveSettings low = s;
	low.hardForceMost = 2.0f;
	Check(KnockdownForceFor(low, 20.0f) == s.hardForce, "a most under the base: the base, never less");
	ShoveSettings noSpeed = s;
	noSpeed.hardSpeed = 0.0f;
	Check(KnockdownForceFor(noSpeed, 20.0f) == s.hardForce, "no KnockdownSpeed to scale by: ShoveHardForce");
	const ShoveSettings at = AtImpactSpeed(s, s.hardSpeed * 2.0f);
	Check(at.hardForce == s.hardForce * 2.0f && at.distance == s.distance, "AtImpactSpeed: the force set, the rest kept");
}

void TestPush() {
	std::printf("The light shove's push\n");
	const NiPoint3 p = ShovePush(NiPoint3{0.0f, 0.0f, 100.0f}, NiPoint3{0.0f, 40.0f, 60.0f}, 30.0f);
	Check(Near(p.x, 0.0f) && Near(p.y, 30.0f) && Near(p.z, 0.0f), "30 units away from the hand, along the ground");
	const NiPoint3 none = ShovePush(NiPoint3{5.0f, 5.0f, 0.0f}, NiPoint3{5.0f, 5.0f, 90.0f}, 30.0f);
	Check(none.LengthSquared() == 0.0f, "straight above each other: no push");
	Check(ShovePush(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 10.0f, 0.0f}, 0.0f).LengthSquared() == 0.0f,
	      "a distance of 0: no push");
}

void TestCooldown() {
	std::printf("The cooldown\n");
	ShoveCooldown c;
	int a = 0;
	int b = 0;
	Check(ShoveAllowed(c, &a), "nobody shoved yet: allowed");
	Check(!ShoveAllowed(c, nullptr), "no actor: not allowed");
	StartShoveCooldown(c, &a, 1.0f);
	Check(!ShoveAllowed(c, &a) && ShoveAllowed(c, &b), "the same actor waits, another does not");
	StepShoveCooldown(c, 0.6f);
	Check(!ShoveAllowed(c, &a), "0.6 s later: still waiting");
	StepShoveCooldown(c, 0.6f);
	Check(ShoveAllowed(c, &a) && c.actor == nullptr, "after the second: allowed again");
}

void TestCountsAsHit() {
	std::printf("A shove counts as a hit\n");
	ShoveSettings s;
	Check(ShoveCountsAsHit(s, ShoveKind::Hard), "pushed to the ground: a hit");
	Check(!ShoveCountsAsHit(s, ShoveKind::Light), "a light shove: no hit, only their liking");
	Check(SlapInTheFace(60.0f, 30.0f) && !SlapInTheFace(40.0f, 30.0f) && !SlapInTheFace(10.0f, 30.0f),
	      "in the face from 35 cm above the body's middle, not below");
	Check(ShoveDisposition(s, ShoveKind::Light, false) == s.dispositionLight &&
	          ShoveDisposition(s, ShoveKind::Light, true) == s.dispositionFace &&
	          ShoveDisposition(s, ShoveKind::Hard, true) == s.dispositionHard,
	      "the body's, the face's, the hard shove's liking");
	Check(SlapSoundFor(true, true) == SlapSound::Mod && SlapSoundFor(true, false) == SlapSound::Own &&
	          SlapSoundFor(false, true) == SlapSound::Own && SlapSoundFor(false, false) == SlapSound::Own,
	      "the mod's noise only with the mod and on its turn of the coin; OBVR's own otherwise");
	char lines[4][kSlapLineChars] = {};
	Check(SlapLines(0x0B, SlapSound::Mod, lines) == 4 && std::strcmp(lines[0], "playSound3D 0B005339") == 0 &&
	          std::strcmp(lines[1], "addItemNS 0B005335 1") == 0 && std::strcmp(lines[2], "pickIdle") == 0 &&
	          std::strcmp(lines[3], "removeItemNS 0B005335 1") == 0,
	      "with Put it in its Place at index 0B, its turn: its slap noise and the slapped idle by form id");
	Check(SlapLines(0x0B, SlapSound::Own, lines) == 3 && std::strcmp(lines[0], "addItemNS 0B005335 1") == 0 &&
	          std::strcmp(lines[1], "pickIdle") == 0,
	      "with the mod, OBVR's turn: the idle only, the noise is OBVR's wave");
	Check(SlapLines(0, SlapSound::Own, lines) == 1 && std::strcmp(lines[0], "playSound3D NPCHumanGaspMale") == 0,
	      "without it: the game's own gasp, no idle");
	Check(SlapLines(0xFE, SlapSound::Mod, lines) == 4 && std::strcmp(lines[0], "playSound3D FE005339") == 0,
	      "the load index in the top byte");
	Check(SlapByModsGrabTap(0x0B) && !SlapByModsGrabTap(0) && kSlapGrabTapFrames >= 2,
	      "with the mod loaded the slap is its grab tap; without it none");
	Check(!SlapGrabTapPressed(kSlapGrabTapTotalFrames) && SlapGrabTapPressed(kSlapGrabTapFrames) &&
	          SlapGrabTapPressed(1) && !SlapGrabTapPressed(0) && kSlapGrabTapFrames <= 7,
	      "the tap: the pick there first, the grab down for the frames after, let go within the mod's 7");
	Check(SlapModOn(0x0B, 1.0, 1.0) && !SlapModOn(0, 1.0, 1.0) && !SlapModOn(0x0B, 0.0, 1.0) &&
	          !SlapModOn(0x0B, 1.0, 0.0),
	      "the mod on: loaded, its master switch and its slap feature set; off with either 0 or not loaded");
	Check(SlapTriggersMod(true, ShoveKind::Light, true) && !SlapTriggersMod(false, ShoveKind::Light, true) &&
	          !SlapTriggersMod(true, ShoveKind::Hard, true) && !SlapTriggersMod(true, ShoveKind::Light, false),
	      "the mod is told of a light slap in the face with it on; OBVR's own slap runs either way");

	std::printf("The slap's noise with the mod on\n");
	SlapNoiseWait w;
	Check(StepSlapNoiseWait(w, true, 50.0f, false) == SlapNoiseStep::Idle, "nothing pending: idle");
	w.pending = true;
	w.encumbranceAtSlap = 50.0f;
	Check(StepSlapNoiseWait(w, true, 50.0f, false) == SlapNoiseStep::Waiting && w.pending,
	      "the encumbrance as at the slap: waiting on");
	Check(StepSlapNoiseWait(w, true, 60.0f, false) == SlapNoiseStep::Waiting && w.pending,
	      "a small change (an item taken): still waiting");
	Check(StepSlapNoiseWait(w, true, 2050.0f, false) == SlapNoiseStep::ModStarted && !w.pending,
	      "the mod's 2000 on it: its sequence started, the noise its own");
	Check(StepSlapNoiseWait(w, true, 2050.0f, false) == SlapNoiseStep::Idle, "then idle");
	w.pending = true;
	w.encumbranceAtSlap = 2050.0f;
	Check(StepSlapNoiseWait(w, true, 50.0f, false) == SlapNoiseStep::ModStarted && !w.pending,
	      "a step down as large (the mod's own taken back): the same");
	w.pending = true;
	w.encumbranceAtSlap = 50.0f;
	Check(StepSlapNoiseWait(w, true, 50.0f, true) == SlapNoiseStep::OurTurn && !w.pending,
	      "the watch over without it: OBVR's turn to play the mod's noise");
	w.pending = true;
	Check(StepSlapNoiseWait(w, false, 2050.0f, false) == SlapNoiseStep::Waiting && w.pending,
	      "the encumbrance not readable: a step cannot be seen, waiting");
	Check(StepSlapNoiseWait(w, false, 2050.0f, true) == SlapNoiseStep::OurTurn && !w.pending,
	      "... and OBVR's turn once the watch is over");
	Check(!ShoveCountsAsHit(s, ShoveKind::None), "no shove: nothing");
	s.countsAsHit = false;
	Check(!ShoveCountsAsHit(s, ShoveKind::Hard), "switched off: only the disposition");
	// The kick (game/KickLogic.h) is told as a hit standing too (decision 1).
	ShoveSettings kick;
	kick.lightCountsAsHit = true;
	Check(ShoveCountsAsHit(kick, ShoveKind::Light) && ShoveCountsAsHit(kick, ShoveKind::Hard),
	      "lightCountsAsHit (the kick): the stagger a hit as well");
	kick.countsAsHit = false;
	Check(!ShoveCountsAsHit(kick, ShoveKind::Light), "but not with countsAsHit off");
}

}  // namespace

ShoveHand Face(float speed) {
	ShoveHand h = Open(speed);
	h.inTheFace = true;
	return h;
}

// A slap: the open hand in the face, the weapons away, lands from the slap's
// own speed (SlapSpeed, 1.3 m/s - the tester's of 2026-10-07; 1.5 took "etwas
// zuviel kraft"), below the one stagger speed every other blow needs since
// 2026-11-02 (3 m/s) - Nadi, 2026-11-10: slaps outside a fight took too
// much force. Above KnockdownSpeed it knocks down as any blow does.
void TestSlapSpeed() {
	std::printf("A slap's speed\n");
	const ShoveSettings s;
	Check(Near(s.slapSpeed, 1.3f), "the slap's speed: 1.3 m/s by default");
	Check(ShoveFor(s, false, Face(1.5f)) == ShoveKind::Light, "a slap at 1.5 m/s lands (a light one)");
	Check(ShoveFor(s, false, Face(1.3f)) == ShoveKind::Light, "a slap at exactly the slap's speed lands");
	Check(ShoveFor(s, false, Face(1.0f)) == ShoveKind::None, "a slap slower than the slap's speed: nothing");
	Check(ShoveFor(s, false, Open(1.5f)) == ShoveKind::None, "the same hand at the body at 1.5 m/s: no shove");
	Check(ShoveFor(s, false, Face(6.5f)) == ShoveKind::Hard, "a slap from the knockdown speed knocks down");
	Check(ShoveFor(s, true, Face(2.0f)) == ShoveKind::None, "a weapon or the fists up (a fight): no slap");
	ShoveHand fist = Face(2.0f);
	fist.open = false;
	Check(ShoveFor(s, false, fist) == ShoveKind::None, "a fist in the face is no slap");
	ShoveSettings none = s;
	none.slapSpeed = 0.0f;
	Check(ShoveFor(none, false, Face(1.5f)) == ShoveKind::None, "SlapSpeed 0: a slap needs the stagger speed");
	Check(ShoveFor(none, false, Face(3.5f)) == ShoveKind::Light, "SlapSpeed 0: from the stagger speed it lands");
	Check(Near(ShoveSpeedFor(s, true), 1.3f) && Near(ShoveSpeedFor(s, false), 3.0f),
	      "the speed asked: the slap's in the face, the stagger speed at the body");
	Check(ShoveRefusalFor(s, false, Face(1.0f)) == ShoveRefusal::TooSlow, "refused: a slap too slow");
	Check(ShoveRefusalFor(s, false, Face(1.5f)) == ShoveRefusal::None, "not refused: a slap fast enough");
	Check(ShoveRefusalFor(s, false, Open(1.5f)) == ShoveRefusal::TooSlow, "refused: the body at 1.5 m/s");
	ShoveHand aimed = Open(0.0f);
	AimShoveHand(aimed, NiPoint3{2.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 40.0f}, NiPoint3{50.0f, 0.0f, 0.0f});
	Check(Near(aimed.towardsSpeed, 2.0f) && aimed.inTheFace, "aimed 40 units above their middle: a slap at 2 m/s");
	AimShoveHand(aimed, NiPoint3{2.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 10.0f}, NiPoint3{50.0f, 0.0f, 0.0f});
	Check(!aimed.inTheFace && ShoveFor(s, false, aimed) == ShoveKind::None, "aimed at their middle: a shove, too slow");
}

// What a slap sounds like (Nadi, 2026-11-10: the slap's noise and the hit's
// together - only the slap's): OBVR's own slap when the mod does not take it,
// for a light one and a hard one alike; nothing of OBVR's at the body.
void TestSlapSound() {
	std::printf("A slap's sound\n");
	Check(SlapPlaysOwnSound(true, ShoveKind::Light, false), "a light slap, the mod not set off: OBVR's slap");
	Check(SlapPlaysOwnSound(true, ShoveKind::Hard, false), "a slap that knocks down: OBVR's slap too");
	Check(!SlapPlaysOwnSound(true, ShoveKind::Light, true), "the mod set off: its own noise, not OBVR's");
	Check(!SlapPlaysOwnSound(false, ShoveKind::Light, false), "a shove at the body: no slap");
	Check(!SlapPlaysOwnSound(true, ShoveKind::None, false), "no blow: nothing");
}

// The fists raised are no weapon for a slap or a shove (Nadi's Windows log
// of 2026-11-11: open hands refused with the fists up).
void TestFistsUp() {
	std::printf("The fists up, a weapon readied\n");
	Check(!WeaponTakesHands(false, -1), "nothing readied: the hands are free");
	Check(!WeaponTakesHands(true, -1), "the fists up: the hands are free for a slap");
	Check(WeaponTakesHands(true, 0), "a one-handed blade readied: taken");
	Check(WeaponTakesHands(true, 4), "a staff readied: taken");
	Check(WeaponTakesHands(true, 5), "a bow readied: taken");
	Check(!WeaponTakesHands(false, 0), "a blade sheathed: free");
	const ShoveSettings s;
	Check(ShoveFor(s, WeaponTakesHands(true, -1), Face(1.5f)) == ShoveKind::Light, "the fists up, an open hand in the face: a slap");
	Check(ShoveFor(s, WeaponTakesHands(true, 0), Face(1.5f)) == ShoveKind::None, "a blade readied: no slap");
}

int main() {
	TestFistsUp();
	TestKind();
	TestRefusal();
	TestSlapSpeed();
	TestSlapSound();
	TestBoundReach();
	TestBodyBound();
	TestSpeedTowards();
	TestReach();
	TestCooldown();
	TestHardForce();
	TestPush();
	TestBody();
	TestCountsAsHit();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
