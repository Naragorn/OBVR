#include <cmath>
#include <cstdio>
#include <cstring>

#include "vr/HintLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using namespace obvr;
using namespace obvr::vr;

constexpr float kFrame = 0.1f;
constexpr SInt32 kShield = static_cast<SInt32>(HintId::ShieldBash);
constexpr SInt32 kKick = static_cast<SInt32>(HintId::Kick);

HintFrame Wanting(SInt32 hint, float dt = kFrame) {
	HintFrame f;
	if (hint >= 0) {
		f.wants[hint] = true;
	}
	f.dt = dt;
	return f;
}

// Steps `seconds` of frames; the last step's result, and how many frames showed `hint`.
HintStep Run(HintState& s, const HintSettings& settings, const HintFrame& frame, float seconds, SInt32 hint,
             int* shownFrames = nullptr) {
	HintStep last;
	const int frames = static_cast<int>(std::lround(seconds / frame.dt));
	for (int i = 0; i < frames; ++i) {
		last = StepHints(s, settings, frame);
		if (shownFrames != nullptr && last.show == hint) {
			++*shownFrames;
		}
	}
	return last;
}

void TestWaitsForTheMoment() {
	std::printf("a hint shows once its moment has held wantSeconds, not before\n");
	const HintSettings settings;
	HintState s;
	HintStep r = Run(s, settings, Wanting(kShield), 1.4f, kShield);
	Check(r.show == -1, "1.4 s of the moment: nothing yet");
	r = StepHints(s, settings, Wanting(kShield));
	Check(r.show == kShield && r.started, "at 1.5 s: the shield's hint starts");
	Check(r.memoryChanged && s.memory.shown[kShield] == 1, "the showing is counted and written");
	r = StepHints(s, settings, Wanting(kShield));
	Check(r.show == kShield && !r.started && !r.memoryChanged, "the next frame: still up, nothing to write");
}

void TestMomentBroken() {
	std::printf("a moment broken before wantSeconds starts its wait again\n");
	const HintSettings settings;
	HintState s;
	Run(s, settings, Wanting(kShield), 1.0f, kShield);
	StepHints(s, settings, Wanting(-1));
	const HintStep r = Run(s, settings, Wanting(kShield), 1.0f, kShield);
	Check(r.show == -1, "1 s, a break, 1 s: nothing");
}

void TestShowTime() {
	std::printf("a showing lasts showSeconds\n");
	const HintSettings settings;
	HintState s;
	Run(s, settings, Wanting(kShield), 1.5f, kShield);
	int frames = 0;
	Run(s, settings, Wanting(kShield), 10.0f, kShield, &frames);
	Check(frames >= 58 && frames <= 60, "up about 6 s of the 10");
	Check(s.active == -1, "then gone");
	Check(s.memory.shown[kShield] == 1, "shown once");
}

void TestGap() {
	std::printf("no showing within gapSeconds of the last one's start\n");
	const HintSettings settings;
	HintState s;
	Run(s, settings, Wanting(kShield), 1.5f, kShield);
	int frames = 0;
	Run(s, settings, Wanting(kShield), 80.0f, kShield, &frames);
	Check(s.memory.shown[kShield] == 1, "80 s of the moment more: still one showing");
	const HintStep r = Run(s, settings, Wanting(kShield), 11.0f, kShield);
	Check(s.memory.shown[kShield] == 2 && r.show == kShield, "past 90 s: the second showing");
}

void TestMaxShows() {
	std::printf("a hint ignored maxShows times is not shown again\n");
	const HintSettings settings;
	HintState s;
	for (int i = 0; i < 6; ++i) {
		Run(s, settings, Wanting(kShield), 95.0f, kShield);
	}
	Check(s.memory.shown[kShield] == 3, "three showings in 570 s, no more");
	Check(!HintOpen(settings, s.memory, kShield), "the hint is closed");
}

void TestLearned() {
	std::printf("done once, the hint is learned and never shown\n");
	const HintSettings settings;
	HintState s;
	HintFrame f = Wanting(-1);
	f.did[kShield] = true;
	HintStep r = StepHints(s, settings, f);
	Check(r.learnedNow && r.memoryChanged && s.memory.learned[kShield], "the bash learns it, written");
	r = StepHints(s, settings, f);
	Check(!r.learnedNow && !r.memoryChanged, "a second bash changes nothing");
	r = Run(s, settings, Wanting(kShield), 10.0f, kShield);
	Check(r.show == -1 && s.memory.shown[kShield] == 0, "the moment after: never shown");
}

void TestLearnedWhileUp() {
	std::printf("done while its sign is up: the sign goes at once\n");
	const HintSettings settings;
	HintState s;
	Run(s, settings, Wanting(kShield), 1.5f, kShield);
	Check(s.active == kShield, "up");
	HintFrame f = Wanting(kShield);
	f.did[kShield] = true;
	const HintStep r = StepHints(s, settings, f);
	Check(r.show == -1 && s.active == -1 && r.learnedNow, "the bash takes it down and learns it");
}

void TestLearnedFromMemory() {
	std::printf("learned in an earlier session: never shown\n");
	const HintSettings settings;
	HintState s;
	s.memory.learned[kShield] = true;
	const HintStep r = Run(s, settings, Wanting(kShield), 5.0f, kShield);
	Check(r.show == -1, "nothing");
}

void TestOff() {
	std::printf("hints off: nothing shows, but a deed is still learned\n");
	HintSettings settings;
	settings.enabled = false;
	HintState s;
	HintStep r = Run(s, settings, Wanting(kShield), 5.0f, kShield);
	Check(r.show == -1 && s.memory.shown[kShield] == 0, "nothing shown, nothing counted");
	HintFrame f = Wanting(-1);
	f.did[kKick] = true;
	r = StepHints(s, settings, f);
	Check(r.learnedNow && s.memory.learned[kKick], "the kick learned");
	settings.enabled = true;
	HintState up;
	Run(up, settings, Wanting(kShield), 1.5f, kShield);
	settings.enabled = false;
	r = StepHints(up, settings, Wanting(kShield));
	Check(r.show == -1 && up.active == -1, "turned off while up: gone");
}

void TestQuiet() {
	std::printf("a menu or a load: nothing shows, the moment's wait starts over\n");
	const HintSettings settings;
	HintState s;
	Run(s, settings, Wanting(kShield), 1.5f, kShield);
	HintFrame quiet = Wanting(kShield);
	quiet.quiet = true;
	HintStep r = StepHints(s, settings, quiet);
	Check(r.show == -1 && s.active == -1, "a menu takes the sign down");
	HintState fresh;
	Run(fresh, settings, Wanting(kShield), 1.0f, kShield);
	StepHints(fresh, settings, quiet);
	r = Run(fresh, settings, Wanting(kShield), 1.0f, kShield);
	Check(r.show == -1, "1 s, a menu, 1 s: nothing");
}

void TestMomentGone() {
	std::printf("the moment gone while up: the sign goes\n");
	const HintSettings settings;
	HintState s;
	Run(s, settings, Wanting(kShield), 1.5f, kShield);
	const HintStep r = StepHints(s, settings, Wanting(-1));
	Check(r.show == -1 && s.active == -1, "the fight over, the sign down");
}

void TestOrder() {
	std::printf("two hints in one moment: the first in order, the other after the gap\n");
	const HintSettings settings;
	HintState s;
	HintFrame both = Wanting(kShield);
	both.wants[kKick] = true;
	HintStep r = Run(s, settings, both, 1.5f, kShield);
	Check(r.show == kShield, "the shield bash first");
	int kickFrames = 0;
	Run(s, settings, both, 85.0f, kKick, &kickFrames);
	Check(kickFrames == 0, "no kick hint within the gap");
	HintFrame f = both;
	f.did[kShield] = true;
	StepHints(s, settings, f);
	r = Run(s, settings, both, 6.0f, kKick);
	Check(r.show == kKick && s.memory.shown[kKick] == 1, "the shield learned, past the gap: the kick's hint");
}

void TestBadTime() {
	std::printf("a NaN or negative frame time counts as none\n");
	const HintSettings settings;
	HintState s;
	HintStep r = Run(s, settings, Wanting(kShield, -1.0f), 1.0f, kShield);
	HintFrame nan = Wanting(kShield);
	nan.dt = std::nanf("");
	for (int i = 0; i < 100; ++i) {
		r = StepHints(s, settings, nan);
	}
	Check(r.show == -1 && s.wantHeld[kShield] == 0.0f, "no time passes, nothing shows");
}

void TestWords() {
	std::printf("every hint has a name and words, none names a side\n");
	for (UInt32 i = 0; i < kHintCount; ++i) {
		const HintWords w = HintWordsFor(i);
		Check(std::strcmp(HintName(i), "?") != 0, "a name");
		Check(w.title[0] != '\0' && w.line[0] != '\0', "a title and a line");
		Check(std::strstr(w.line, "left") == nullptr && std::strstr(w.line, "right") == nullptr, "no side named");
	}
	Check(std::strcmp(HintName(kHintCount), "?") == 0, "past the end: ?");
	Check(HintWordsFor(kHintCount).title[0] == '\0', "past the end: no words");
	Check(!HintOnWeaponHand(kShield) && HintOnWeaponHand(kKick) &&
	          HintOnWeaponHand(static_cast<UInt32>(HintId::DrawAtBody)),
	      "the shield's sign on the other hand, the kick's and the draw's on the weapon hand");
	Check(HintOnWeaponHand(static_cast<UInt32>(HintId::Parry)) && !HintOnWeaponHand(static_cast<UInt32>(HintId::Throw)),
	      "the parry's sign on the weapon hand, the throw's over the hand that holds");
	Check(std::strcmp(HintName(static_cast<UInt32>(HintId::Parry)), "Parry") == 0 &&
	          std::strcmp(HintName(static_cast<UInt32>(HintId::Throw)), "Throw") == 0,
	      "the parry and the throw keep their INI names");
	Check(static_cast<UInt32>(HintId::Stow) < static_cast<UInt32>(HintId::Throw) &&
	          static_cast<UInt32>(HintId::EatAtMouth) < static_cast<UInt32>(HintId::Throw),
	      "stowing and eating before throwing: a held thing's first hint is where it goes");
	Check(std::strcmp(HintName(static_cast<UInt32>(HintId::Yield)), "Yield") == 0 &&
	          std::strcmp(HintName(static_cast<UInt32>(HintId::PotionBelt)), "PotionBelt") == 0,
	      "the yield and the potion belt have their INI names");
	Check(HintOnWeaponHand(static_cast<UInt32>(HintId::Yield)) &&
	          !HintOnWeaponHand(static_cast<UInt32>(HintId::PotionBelt)),
	      "the yield's sign on the weapon hand, the belt's over the hand the host names");
	Check(static_cast<UInt32>(HintId::Yield) < static_cast<UInt32>(HintId::DrawAtBody),
	      "the yield first: against a guard, giving up beats fighting better");
	Check(static_cast<UInt32>(HintId::EatAtMouth) < static_cast<UInt32>(HintId::PotionBelt) &&
	          static_cast<UInt32>(HintId::PotionBelt) < static_cast<UInt32>(HintId::Throw),
	      "a potion held: drinking first, then keeping it, then throwing");
	Check(static_cast<UInt32>(HintId::DrawAtBody) < static_cast<UInt32>(HintId::ShieldBash),
	      "drawing comes before the shield bash: one cannot bash before one holds");
}

void TestThrowCounts() {
	std::printf("a throw learns the throw's hint only at a throw's speed, not a thing set down\n");
	Check(!HintThrowCounts(0.0f), "set down: no");
	Check(!HintThrowCounts(1.9f), "1.9 m/s, let go in passing: no");
	Check(HintThrowCounts(2.0f), "2 m/s: a throw");
	Check(HintThrowCounts(8.0f), "8 m/s: a throw");
	Check(!HintThrowCounts(std::nanf("")), "NaN: no");
}

bool Wants(const HintMoment& m, HintId id) {
	bool wants[kHintCount] = {};
	HintWantsFor(m, wants);
	return wants[static_cast<UInt32>(id)];
}

void TestMoments() {
	std::printf("each hint's moment, from what the frame holds\n");
	HintMoment m;
	bool none[kHintCount] = {};
	HintWantsFor(m, none);
	bool any = false;
	for (bool w : none) {
		any = any || w;
	}
	Check(!any, "nothing going on: no moment");
	m.fighting = true;
	m.weaponEquipped = true;
	m.holsters = true;
	m.kick = true;
	Check(Wants(m, HintId::DrawAtBody) && Wants(m, HintId::Kick), "a fight, a weapon sheathed: the draw, the kick");
	Check(!Wants(m, HintId::Parry), "sheathed: no parry");
	m.holsters = false;
	Check(!Wants(m, HintId::DrawAtBody), "holsters off: no draw hint");
	m.weaponDrawn = true;
	m.parries = true;
	Check(Wants(m, HintId::Parry) && !Wants(m, HintId::DrawAtBody), "drawn: the parry, not the draw");
	m.parries = false;
	Check(!Wants(m, HintId::Parry), "parries off: no parry hint");
	m.shieldBash = true;
	Check(!Wants(m, HintId::ShieldBash), "no shield worn: no bash");
	m.shieldEquipped = true;
	Check(Wants(m, HintId::ShieldBash), "a shield: the bash");
	m.fighting = false;
	Check(!Wants(m, HintId::ShieldBash) && !Wants(m, HintId::Kick) && !Wants(m, HintId::Parry),
	      "the fight over: none of the fight's");
	HintMoment held;
	held.holdingItem = true;
	Check(!Wants(held, HintId::Stow) && Wants(held, HintId::Throw), "a thing held, stowing off: the throw alone");
	held.stowAtBody = true;
	held.eatAtMouth = true;
	Check(Wants(held, HintId::Stow) && Wants(held, HintId::Throw) && !Wants(held, HintId::EatAtMouth),
	      "a thing held: stow and throw");
	held.holdingEdible = true;
	Check(!Wants(held, HintId::Stow) && Wants(held, HintId::Throw) && Wants(held, HintId::EatAtMouth),
	      "food held: eat and throw, not stow");
	held.eatAtMouth = false;
	Check(!Wants(held, HintId::EatAtMouth), "eating at the mouth off: no eat hint");
	HintMoment door;
	door.atHandle = true;
	Check(Wants(door, HintId::DoorByGrip), "at a handle: the door");
	HintMoment yield;
	yield.fighting = true;
	yield.yield = true;
	Check(!Wants(yield, HintId::Yield), "a fight with no lawful one in it (a bandit): no yield hint");
	yield.lawfulFoe = true;
	Check(Wants(yield, HintId::Yield), "a guard or a lawful one fights the player: the yield");
	yield.yield = false;
	Check(!Wants(yield, HintId::Yield), "the yield gesture off: no yield hint");
	yield.yield = true;
	yield.fighting = false;
	Check(!Wants(yield, HintId::Yield), "the fight over: no yield hint");
	HintMoment belt;
	belt.holdingItem = true;
	belt.holdingEdible = true;
	belt.holdingPotion = true;
	Check(!Wants(belt, HintId::PotionBelt), "a potion held, the belt off: no belt hint");
	belt.potionBelt = true;
	Check(Wants(belt, HintId::PotionBelt), "a potion held, the belt on: keep it at a place");
	belt.holdingPotion = false;
	Check(!Wants(belt, HintId::PotionBelt), "food held: no belt hint");
	HintMoment hurt;
	hurt.potionBelt = true;
	hurt.fighting = true;
	hurt.lowHealth = true;
	Check(!Wants(hurt, HintId::PotionBelt), "hurt in a fight, no potion kept: no belt hint");
	hurt.potionKept = true;
	Check(Wants(hurt, HintId::PotionBelt), "hurt in a fight, a potion kept: reach for it");
	hurt.fighting = false;
	Check(!Wants(hurt, HintId::PotionBelt), "hurt, no fight: no belt hint");
	hurt.fighting = true;
	hurt.lowHealth = false;
	Check(!Wants(hurt, HintId::PotionBelt), "a fight, health high: no belt hint");
}

void TestLawfulFoe() {
	std::printf("a lawful foe: a guard by class, or an NPC of responsibility 50 or more; never a creature\n");
	Check(!HintLawfulFoe(false, true, 100.0f), "a creature: never (creatures never accept a yield)");
	Check(HintLawfulFoe(true, true, 0.0f), "a guard's class: yes, whatever the responsibility");
	Check(HintLawfulFoe(true, false, 50.0f), "responsibility 50: yes");
	Check(!HintLawfulFoe(true, false, 49.0f), "responsibility 49: no");
	Check(!HintLawfulFoe(true, false, 0.0f), "a bandit's 0: no");
	Check(!HintLawfulFoe(true, false, std::nanf("")), "unread (NaN): no");
	Check(HintLawfulFoe(true, true, std::nanf("")), "unread, but a guard: yes");
}

void TestLowHealth() {
	std::printf("low health: 40 %% of the base or less, alive, the base read\n");
	Check(HintLowHealth(40.0f, 100.0f), "40 of 100: low");
	Check(!HintLowHealth(41.0f, 100.0f), "41 of 100: not");
	Check(HintLowHealth(1.0f, 100.0f), "1 of 100: low");
	Check(!HintLowHealth(0.0f, 100.0f), "dead: not");
	Check(!HintLowHealth(10.0f, 0.0f), "no base: not");
	Check(!HintLowHealth(std::nanf(""), 100.0f) && !HintLowHealth(10.0f, std::nanf("")), "NaN: not");
	Check(!HintLowHealth(150.0f, 100.0f), "fortified over the base: not");
}

}  // namespace

int main() {
	TestWaitsForTheMoment();
	TestMomentBroken();
	TestShowTime();
	TestGap();
	TestMaxShows();
	TestLearned();
	TestLearnedWhileUp();
	TestLearnedFromMemory();
	TestOff();
	TestQuiet();
	TestMomentGone();
	TestOrder();
	TestBadTime();
	TestWords();
	TestThrowCounts();
	TestMoments();
	TestLawfulFoe();
	TestLowHealth();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
