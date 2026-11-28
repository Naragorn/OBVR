// Checks the shield bash's approach (game/BashApproachLogic.h): a bash is
// judged by the fastest speed towards the target within a short time before
// and at the frame the shield's middle is in their column - not by that one
// frame, in which a jab that ends at the target is already slowing down
// (Nadi's headset run of 2026-11-28: "schildstoss ging nun paar mal nicht").
// Towards only: a retreat ends the approach, a sideways sweep counts nothing.
// The log's attempts of that run are replayed as far as their lines carry the
// numbers.

#include <cstdio>
#include <limits>

#include "game/BashApproachLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.001f) { return a - b < eps && b - a < eps; }

using namespace obvr;
using namespace obvr::game;

constexpr double kFrame = 1.0 / 90.0;  // the run's headset, 90 Hz (OBVR.log line 174)
const NiPoint3 kTarget{0.0f, 0.0f, 0.0f};

// The shield at x units in front of the target (x < 0), moving at v m/s
// along +x (towards it; negative: away).
void Add(BashApproachTrail& t, float x, float v, double time) {
	AddBashSample(t, NiPoint3{v, 0.0f, 0.0f}, NiPoint3{x, 0.0f, 0.0f}, time);
}

void TestEmptyAndWindow() {
	std::printf("The window\n");
	BashApproachTrail t;
	Check(BashApproachSpeed(t, kTarget, 0.0, kBashApproachSeconds) == 0.0f, "nothing seen: no speed");
	Add(t, -40.0f, 4.0f, 0.0);
	Add(t, -36.0f, 2.0f, kFrame);
	Check(Near(BashApproachSpeed(t, kTarget, kFrame, 0.0f), 2.0f), "a window of zero: this frame alone (the old gate)");
	Check(Near(BashApproachSpeed(t, kTarget, kFrame, std::numeric_limits<float>::quiet_NaN()), 2.0f),
	      "a window not a number: as zero");
	Check(Near(BashApproachSpeed(t, kTarget, kFrame, -1.0f), 2.0f), "a negative window: as zero");
	Check(Near(BashApproachSpeed(t, kTarget, kFrame, kBashApproachSeconds), 4.0f),
	      "the frame before within the window: its speed counts");
	Check(Near(kBashApproachSeconds, 0.15f), "the window 0.15 s");
}

void TestAge() {
	std::printf("An approach too long ago\n");
	BashApproachTrail t;
	Add(t, -60.0f, 5.0f, 0.0);
	for (int i = 1; i <= 20; ++i) {
		Add(t, -40.0f, 1.0f, i * kFrame);
	}
	Check(Near(BashApproachSpeed(t, kTarget, 20 * kFrame, kBashApproachSeconds), 1.0f),
	      "a fast frame 0.22 s ago: past the window, not counted");
	BashApproachTrail late;
	Add(late, -60.0f, 5.0f, 0.0);
	Add(late, -40.0f, 1.0f, 1.0);
	Check(Near(BashApproachSpeed(late, kTarget, 1.0, kBashApproachSeconds), 1.0f),
	      "the hand gone a second (no frames between): the old fast frame is too old");
	BashApproachTrail future;
	Add(future, -40.0f, 5.0f, 2.0);
	Check(BashApproachSpeed(future, kTarget, 1.0, kBashApproachSeconds) == 0.0f, "a frame stamped after now: none");
}

void TestTowardsOnly() {
	std::printf("Towards them only\n");
	{
		BashApproachTrail t;
		Add(t, -60.0f, 5.0f, 0.0);
		Add(t, -50.0f, -1.0f, kFrame);
		Add(t, -52.0f, 0.5f, 2 * kFrame);
		Check(Near(BashApproachSpeed(t, kTarget, 2 * kFrame, kBashApproachSeconds), 0.5f),
		      "fast in, pulled back, then slowly in again: the retreat ends the approach");
	}
	{
		BashApproachTrail t;
		Add(t, -60.0f, 5.0f, 0.0);
		Add(t, -50.0f, -0.5f, kFrame);
		Check(BashApproachSpeed(t, kTarget, kFrame, kBashApproachSeconds) == 0.0f,
		      "moving away now: no approach, whatever came before");
	}
	{
		BashApproachTrail t;
		for (int i = 0; i < 6; ++i) {
			AddBashSample(t, NiPoint3{0.0f, 5.0f, 0.0f}, NiPoint3{-40.0f, -10.0f + 5.0f * i, 0.0f}, i * kFrame);
		}
		Check(BashApproachSpeed(t, NiPoint3{0.0f, 0.0f, 0.0f}, 5 * kFrame, kBashApproachSeconds) < 3.0f,
		      "a fast sweep across in front of them: under the gate");
		BashApproachTrail across;
		AddBashSample(across, NiPoint3{0.0f, 5.0f, 0.0f}, NiPoint3{-20.0f, 0.0f, 0.0f}, 0.0);
		Check(Near(BashApproachSpeed(across, kTarget, 0.0, kBashApproachSeconds), 0.0f),
		      "straight across the line to them: nothing towards them");
	}
	{
		BashApproachTrail t;
		Add(t, -60.0f, 4.0f, 0.0);
		AddBashSample(t, NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, kFrame);
		Check(Near(BashApproachSpeed(t, kTarget, kFrame, kBashApproachSeconds), 4.0f),
		      "the shield's middle at their centre (no line): no speed of its own, not a retreat");
	}
}

void TestNotANumber() {
	std::printf("Figures not a number\n");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	BashApproachTrail t;
	Add(t, -60.0f, 4.0f, 0.0);
	AddBashSample(t, NiPoint3{nan, 0.0f, 0.0f}, NiPoint3{-50.0f, 0.0f, 0.0f}, kFrame);
	const float s = BashApproachSpeed(t, kTarget, kFrame, kBashApproachSeconds);
	Check(s == s && Near(s, 4.0f), "a velocity not a number: that frame no speed, the rest still counts");
	Check(BashApproachSpeed(t, kTarget, std::numeric_limits<double>::quiet_NaN(), kBashApproachSeconds) == 0.0f,
	      "now not a number: no speed");
	BashApproachTrail badTime;
	Add(badTime, -60.0f, 4.0f, std::numeric_limits<double>::quiet_NaN());
	Check(BashApproachSpeed(badTime, kTarget, 0.0, kBashApproachSeconds) == 0.0f, "a frame's time not a number: none");
	Check(BashApproachSpeed(t, NiPoint3{nan, 0.0f, 0.0f}, kFrame, kBashApproachSeconds) == 0.0f,
	      "their centre not a number: no speed");
}

void TestRing() {
	std::printf("The kept frames\n");
	BashApproachTrail t;
	for (UInt32 i = 0; i < kBashApproachSamples + 8; ++i) {
		Add(t, -60.0f, i == kBashApproachSamples + 7 ? 3.5f : 1.0f, i * kFrame);
	}
	Check(t.count == kBashApproachSamples, "at most the trail's frames kept, none allocated");
	Check(Near(BashApproachSpeed(t, kTarget, (kBashApproachSamples + 7) * kFrame, kBashApproachSeconds), 3.5f),
	      "the newest after the ring wrapped");
	Check(kBashApproachSamples * kFrame > kBashApproachSeconds && kBashApproachSamples / 240.0 > kBashApproachSeconds - 0.02,
	      "the ring holds the window at 90 Hz and near it at 240 Hz");
	ClearBashTrail(t);
	Check(t.count == 0 && BashApproachSpeed(t, kTarget, 0.0, kBashApproachSeconds) == 0.0f, "cleared: nothing left");
}

// One attempt of the log, as far as its line carries it: the controller's
// most speed (assumed along the line to them - the line gives its size only)
// and the speed towards them in the best frame in their column. The jab
// rises to the top over four frames, holds one, slows to the judged speed
// over four, entering the column on that last one, and slows on in it.
struct Attempt {
	const char* line;
	float top;
	float judged;
	bool landsNow;
};

bool Replays(const Attempt& a, bool byApproach) {
	BashApproachTrail t;
	float speeds[16];
	int n = 0;
	for (int i = 1; i <= 4; ++i) {
		speeds[n++] = 1.5f + (a.top - 1.5f) * i / 4.0f;
	}
	speeds[n++] = a.top;
	for (int i = 1; i <= 4; ++i) {
		speeds[n++] = a.top + (a.judged - a.top) * i / 4.0f;
	}
	const int column = n - 1;
	speeds[column] = a.judged;  // as the line gives it, not a rounding of the slowing above
	for (int i = 1; i <= 3; ++i) {
		speeds[n++] = a.judged - 0.6f * i;
	}
	float x = -80.0f;
	ShieldBashSettings s;
	for (int i = 0; i < n; ++i) {
		Add(t, x, speeds[i], i * kFrame);
		x += speeds[i] * 70.0f * static_cast<float>(kFrame);
		if (i < column) {
			continue;  // not yet in their column: no gate
		}
		ShieldBashHand h;
		h.shieldWorn = true;
		h.valid = true;
		h.towardsSpeed = byApproach ? BashApproachSpeed(t, kTarget, i * kFrame, kBashApproachSeconds) : speeds[i];
		if (ShieldBashFor(s, h) != ShoveKind::None) {
			return true;
		}
	}
	return false;
}

void TestTheLog() {
	std::printf("The attempts of 2026-11-28 13:55 (OBVR.log), replayed\n");
	const Attempt attempts[] = {
		{"45763: top 4.5, judged 2.8 - lands", 4.5f, 2.8f, true},
		{"45777: top 5.1, judged 2.8 - lands", 5.1f, 2.8f, true},
		{"47936: top 4.1, judged 2.4 - lands", 4.1f, 2.4f, true},
		{"47961: top 4.0, judged 1.8 - lands", 4.0f, 1.8f, true},
		{"47974: top 3.9, judged 2.9 - lands", 3.9f, 2.9f, true},
		{"51547: top 3.8, judged 1.6 - lands", 3.8f, 1.6f, true},
		{"23369: top 2.1, judged 0.5 - no bash", 2.1f, 0.5f, false},
		{"23514: top 2.4, judged 1.9 - no bash", 2.4f, 1.9f, false},
		{"51594: top 2.0, judged 1.6 - no bash", 2.0f, 1.6f, false},
		{"45765: top 1.5, moving away (-1.1) - no bash", 1.5f, -1.1f, false},
		{"45779: top 1.6, moving away (-0.7) - no bash", 1.6f, -0.7f, false},
		{"51548: top 1.9, moving away (-0.5) - no bash", 1.9f, -0.5f, false},
		{"23644: landed at 3.2 (top 4.2) - lands still", 4.2f, 3.2f, true},
		{"47918: landed at 3.1 (top 4.3) - lands still", 4.3f, 3.1f, true},
		{"51549: landed at 3.1 (top 3.5) - lands still", 3.5f, 3.1f, true},
		{"51567: landed at 3.0 (top 3.2) - lands still", 3.2f, 3.0f, true},
		{"51580: landed at 3.9 (top 3.9) - lands still", 3.9f, 3.9f, true},
	};
	int refusedBefore = 0;
	for (const Attempt& a : attempts) {
		Check(Replays(a, true) == a.landsNow, a.line);
		refusedBefore += !Replays(a, false) && a.landsNow ? 1 : 0;
	}
	Check(refusedBefore == 6, "the frame of contact alone refused the six with a real approach (as the log said)");
}

void TestHard() {
	std::printf("The hard bash by the approach\n");
	BashApproachTrail t;
	Add(t, -80.0f, 6.4f, 0.0);
	Add(t, -75.0f, 4.0f, kFrame);
	ShieldBashSettings s;
	ShieldBashHand h;
	h.shieldWorn = true;
	h.valid = true;
	h.towardsSpeed = BashApproachSpeed(t, kTarget, kFrame, kBashApproachSeconds);
	Check(ShieldBashFor(s, h) == ShoveKind::Hard, "an approach at 6.4 m/s slowing to 4 at the contact: a hard bash");
}

}  // namespace

int main() {
	TestEmptyAndWindow();
	TestAge();
	TestTowardsOnly();
	TestNotANumber();
	TestRing();
	TestTheLog();
	TestHard();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all bash approach checks passed\n");
	return 0;
}
