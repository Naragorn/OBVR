// game/CutStrokeLogic.h and DismemberLogic.h's verdict for it: one part of a
// body a swing (the tester, 2026-11-26: "teilweise fliegt auch kopf UND arm ab
// obwohl nur kopf sollte").

#include <cstdio>
#include <string>

#include "game/CutStrokeLogic.h"
#include "game/DismemberLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using namespace obvr;
using namespace obvr::game;

constexpr UInt32 kZombie = 0x2B001A50;
constexpr UInt32 kGoblin = 0x2B009630;

void TestSwingCuts() {
	std::printf("which bodies a swing has cut\n");
	SwingCuts s;
	Check(!SwingTookPartOf(s, 34, kZombie), "nothing cut yet: free");
	// OBVR.log 2026-11-26: swing 34 took the zombie's head by the killing strike; the arms came after it.
	NoteSwingCut(s, 34, kZombie);
	Check(SwingTookPartOf(s, 34, kZombie), "its head cut by swing 34: swing 34 takes nothing more of it");
	Check(!SwingTookPartOf(s, 34, kGoblin), "another body in the same swing: free");
	Check(!SwingTookPartOf(s, 35, kZombie), "the next swing: another part may come off");
	NoteSwingCut(s, 34, kGoblin);
	Check(SwingTookPartOf(s, 34, kZombie) && SwingTookPartOf(s, 34, kGoblin), "two bodies through one swing: one part each");
	NoteSwingCut(s, 35, kGoblin);
	Check(SwingTookPartOf(s, 35, kGoblin) && !SwingTookPartOf(s, 35, kZombie) && !SwingTookPartOf(s, 34, kZombie),
	      "a new swing's cut forgets the swing before");
	NoteSwingCut(s, 35, kGoblin);
	Check(s.count == 1, "the same body noted twice: kept once");
	NoteSwingCut(s, 35, 0);
	Check(s.count == 1 && !SwingTookPartOf(s, 35, 0), "no actor: nothing noted, nothing held back");
	SwingCuts full;
	for (UInt32 i = 1; i <= SwingCuts::kMost + 2; ++i) {
		NoteSwingCut(full, 7, i);
	}
	Check(full.count == SwingCuts::kMost && SwingTookPartOf(full, 7, 1) && !SwingTookPartOf(full, 7, SwingCuts::kMost + 1),
	      "more bodies in one swing than kept: the rest are not held back");
}

void TestVerdict() {
	std::printf("the cut's rule with it\n");
	CutSettings settings;
	CutInput in;
	in.enabled = true;
	in.dead = true;
	in.edge = true;
	in.onNeck = true;
	in.limb = true;
	in.speedMetres = 14.7f;
	Check(DecideCut(settings, in) == CutVerdict::Cut, "the zombie's right shoulder at 14.7 m/s, dead: cut (before)");
	in.swingTookAnother = true;
	Check(DecideCut(settings, in) == CutVerdict::SwingTookAnother,
	      "the swing took its head already: not cut");
	Check(std::string(CutVerdictName(CutVerdict::SwingTookAnother)).find("this swing") != std::string::npos,
	      "the log says why");
	in.alreadyCut = true;
	Check(DecideCut(settings, in) == CutVerdict::AlreadyCut, "the part itself off already: said so first");
	in.alreadyCut = false;
	in.enabled = false;
	Check(DecideCut(settings, in) == CutVerdict::Off, "off: off");
	in.enabled = true;
	in.dead = false;
	in.swingTookAnother = false;
	Check(DecideCut(settings, in) == CutVerdict::Living, "alive, nothing cut by the swing: the living keep their parts");
	in.swingTookAnother = true;
	Check(!ArmsKillingBlow(settings, in), "a living neck the swing already took a part of: no killing blow waits");
	in.swingTookAnother = false;
	Check(ArmsKillingBlow(settings, in), "and without: it waits, as before");
}

}  // namespace

int main() {
	TestSwingCuts();
	TestVerdict();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
