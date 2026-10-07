// Checks the yield by gesture (vr/Yield.h).

#include <cstdio>
#include <cstring>

#include "vr/Yield.h"

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

// Both hands rocking together: out by `amplitude`, in again, `cycles`
// times, `stepSeconds` a frame, 8 frames a half stroke. Returns the frame
// the yield fired on, or -1.
int Rock(YieldState& s, float amplitude, int cycles, float stepSeconds, bool allowed = true) {
	float x = 0.0f;
	int frame = 0;
	for (int c = 0; c < cycles; ++c) {
		for (int half = 0; half < 2; ++half) {
			for (int i = 0; i < 8; ++i) {
				x += (half == 0 ? 1.0f : -1.0f) * amplitude / 8.0f;
				const float lateral[2] = {x, x};
				if (StepYield(s, allowed, lateral, stepSeconds)) {
					return frame;
				}
				++frame;
			}
		}
	}
	return -1;
}

}  // namespace

int main() {
	std::printf("Allowed\n");
	Check(YieldAllowed(true, true, true, 0.1f, 0.2f, true), "weapon away, hands open and tracked, an enemy ahead");
	Check(!YieldAllowed(false, true, true, 0.1f, 0.2f, true), "a weapon out: not");
	Check(!YieldAllowed(true, false, true, 0.1f, 0.2f, true) && !YieldAllowed(true, true, false, 0.1f, 0.2f, true),
	      "a hand not tracked: not");
	Check(!YieldAllowed(true, true, true, 0.9f, 0.2f, true) && !YieldAllowed(true, true, true, 0.1f, 0.8f, true),
	      "a hand closed: not");
	Check(!YieldAllowed(true, true, true, 0.1f, 0.2f, false), "no one in combat ahead: not");

	std::printf("How closed a hand is\n");
	const float onTrigger[5] = {0.1f, 1.0f, 0.1f, 0.2f, 0.3f};
	Check(YieldHandCurl(onTrigger) == 0.3f, "the index on the trigger does not count; the most curled of the rest");
	const float fist[5] = {0.9f, 0.9f, 0.95f, 0.9f, 0.8f};
	Check(YieldHandCurl(fist) == 0.95f && !(YieldHandCurl(fist) < kYieldOpenCurl), "a fist: closed");
	const float ringOnly[5] = {0.0f, 0.0f, 0.0f, 0.7f, 0.0f};
	Check(YieldHandCurl(ringOnly) == 0.7f, "one finger curled is enough to close it");

	std::printf("Why not, for the log\n");
	char why[160] = {};
	Check(YieldBlockedBy(true, true, true, 0.1f, 0.2f, true, why, sizeof(why)) == 0 && std::strcmp(why, "nothing") == 0,
	      "all holding: nothing");
	Check(YieldBlockedBy(false, true, true, 0.1f, 0.2f, true, why, sizeof(why)) == 1 &&
	          std::strcmp(why, "a weapon or the fists in hand") == 0,
	      "a weapon out: named");
	Check(YieldBlockedBy(true, false, false, 0.9f, 0.8f, false, why, sizeof(why)) == 5 &&
	          std::strcmp(why, "the right hand not tracked, the left hand not tracked, the right hand closed, the left "
	                           "hand closed, no one in combat ahead of the head") == 0,
	      "everything else failing: each named, comma separated");
	char tiny[12] = {};
	Check(YieldBlockedBy(false, true, true, 0.1f, 0.2f, true, tiny, sizeof(tiny)) == 1 &&
	          std::strcmp(tiny, "a weapon or") == 0,
	      "a short buffer: cut and terminated");
	Check(YieldBlockedBy(false, true, true, 0.1f, 0.2f, true, tiny, 0) == 1, "no buffer: the count alone");

	std::printf("The rocking\n");
	YieldState s;
	const int fired = Rock(s, 0.08f, 3, 0.011f);
	Check(fired >= 0 && fired < 40, "both hands out and in twice (8 cm): the yield, on the second cycle");
	Check(s.cooldownLeft > 0.0f && s.reversals == 0, "then the cooldown, the count reset");
	YieldState small;
	Check(Rock(small, 0.03f, 4, 0.011f) < 0, "3 cm rocking: too small, no yield");
	YieldState slow;
	Check(Rock(slow, 0.08f, 3, 0.35f) < 0, "turnarounds 2.8 s apart: too slow for the window");
	YieldState held;
	Check(Rock(held, 0.08f, 3, 0.011f, false) < 0, "not allowed (a weapon out): nothing");
	YieldState again;
	Rock(again, 0.08f, 3, 0.011f);
	Check(Rock(again, 0.08f, 3, 0.011f) < 0, "right after: the cooldown holds it");
	const float still[2] = {0.0f, 0.0f};
	for (int i = 0; i < 300; ++i) {
		StepYield(again, true, still, 0.011f);
	}
	Check(again.cooldownLeft == 0.0f && Rock(again, 0.08f, 3, 0.011f) >= 0, "3.3 s later: again");
	YieldState oneHand;
	{
		float x = 0.0f;
		int fired1 = -1;
		for (int i = 0; i < 64 && fired1 < 0; ++i) {
			x += ((i / 8) % 2 == 0 ? 1.0f : -1.0f) * 0.01f;
			const float lateral[2] = {x, 0.0f};
			if (StepYield(oneHand, true, lateral, 0.011f)) {
				fired1 = i;
			}
		}
		Check(fired1 >= 0, "one hand rocking four strokes counts too (the reversals are summed)");
	}
	YieldState broken;
	{
		float x = 0.0f;
		for (int i = 0; i < 16; ++i) {
			x += (i < 8 ? 1.0f : -1.0f) * 0.01f;
			const float lateral[2] = {x, x};
			StepYield(broken, true, lateral, 0.011f);
		}
		const UInt32 counted = broken.reversals;
		StepYield(broken, false, still, 0.011f);
		Check(counted > 0 && broken.reversals == 0 && !broken.hand[0].tracking,
		      "a weapon drawn half way: the count and the tracks dropped");
	}

	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
