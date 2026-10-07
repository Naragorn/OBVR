// Checks the shove's decisions (game/ShoveLogic.h): when a hand shoves, how
// hard, its speed towards the actor, the reach, and the cooldown.

#include <cstdio>
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
	Check(ShoveFor(s, false, Open(2.5f)) == ShoveKind::Light, "from 2 m/s: a light shove");
	Check(ShoveFor(s, false, Open(4.5f)) == ShoveKind::Hard, "from 3.6 m/s: a hard one");
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
	Check(SlapLeftToMod(0x0B, ShoveKind::Light, true) && !SlapLeftToMod(0, ShoveKind::Light, true) &&
	          !SlapLeftToMod(0x0B, ShoveKind::Hard, true) && !SlapLeftToMod(0x0B, ShoveKind::Light, false),
	      "the mod's slap is a light one in the face with the mod loaded, and then wholly the mod's");
	Check(!ShoveCountsAsHit(s, ShoveKind::None), "no shove: nothing");
	s.countsAsHit = false;
	Check(!ShoveCountsAsHit(s, ShoveKind::Hard), "switched off: only the disposition");
}

}  // namespace

int main() {
	TestKind();
	TestSpeedTowards();
	TestReach();
	TestCooldown();
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
