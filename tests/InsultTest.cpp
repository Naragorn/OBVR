// Checks the middle finger at an NPC (game/Insult.h).

#include <cstdio>

#include "game/Insult.h"

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

}  // namespace

int main() {
	std::printf("The gesture\n");
	const float finger[5] = {0.2f, 0.9f, 0.1f, 0.9f, 0.95f};
	const float thumbIn[5] = {0.9f, 0.9f, 0.1f, 0.9f, 0.95f};
	const float fist[5] = {0.9f, 0.9f, 0.9f, 0.9f, 0.95f};
	const float open[5] = {0.1f, 0.1f, 0.1f, 0.1f, 0.1f};
	const float pointing[5] = {0.2f, 0.1f, 0.9f, 0.9f, 0.95f};
	const float loose[5] = {0.2f, 0.5f, 0.1f, 0.9f, 0.95f};
	Check(InsultGesture(true, finger), "the middle finger out, the others curled: the gesture");
	Check(InsultGesture(true, thumbIn), "the thumb in too: still the gesture");
	Check(!InsultGesture(true, fist), "a fist: not");
	Check(!InsultGesture(true, open), "an open hand: not");
	Check(!InsultGesture(true, pointing), "the index finger out: not");
	Check(!InsultGesture(true, loose), "the index finger half curled: not");
	Check(!InsultGesture(false, finger), "no curls read: not");

	std::printf("Landing it\n");
	const int npc = 1;
	const int other = 2;
	InsultState s;
	Check(!StepInsult(s, true, &npc, 0.2f), "0.2 s at them: not yet");
	Check(!StepInsult(s, true, &npc, 0.15f), "0.35 s: not yet");
	Check(StepInsult(s, true, &npc, 0.1f), "0.45 s: landed");
	Check(!StepInsult(s, true, &npc, 0.5f), "held on: not again");
	Check(!StepInsult(s, false, nullptr, 0.1f), "let go: nothing");
	Check(!StepInsult(s, true, &npc, 0.5f), "again at the same NPC within the cooldown: nothing");
	Check(StepInsult(s, true, &other, 0.5f), "but another NPC takes it");

	InsultState t;
	StepInsult(t, true, &npc, 0.3f);
	StepInsult(t, true, nullptr, 0.1f);
	Check(t.heldSeconds == 0.0f && !t.fired, "the hand turned away before it landed: the clock reset, the gesture live");
	Check(!StepInsult(t, true, &npc, 0.3f), "back at them: counted from the start");
	Check(StepInsult(t, true, &npc, 0.15f), "... and lands after its own 0.4 s");

	InsultState u;
	StepInsult(u, true, &npc, 0.5f);
	StepInsult(u, false, nullptr, 0.1f);
	for (int i = 0; i < 9; ++i) {
		StepInsult(u, false, nullptr, 1.0f);
	}
	Check(u.cooldownLeft == 0.0f && u.lastActor == nullptr, "the cooldown over after 8 s");
	Check(StepInsult(u, true, &npc, 0.5f), "the same NPC again after it: landed");

	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
