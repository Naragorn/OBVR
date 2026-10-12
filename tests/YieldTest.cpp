// Checks the yield by gesture (vr/Yield.h): the gates of a hand (tracked,
// open, up before the eyes, the palm to the front, not swinging nor fast), the enemy
// ahead; the rocking - both hands, strokes of a rock's size, not a tremor,
// not over in a second, within the window, the cooldown; the speed over its
// window; the rocking alone for the log; the reasons' text.

#include <cstdio>
#include <cstring>
#include <limits>

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

constexpr float kDt = 1.0f / 90.0f;

// A hand held up before the eyes, open, the palm to the front: the gesture's pose.
YieldHandInput Pose(float lateral) {
	YieldHandInput h;
	h.tracked = true;
	h.curl = 0.2f;
	h.position = NiPoint3{lateral, 0.3f, -0.25f};
	h.palmAhead = 0.9f;
	h.swinging = false;
	return h;
}

YieldInput Frame(float rightLateral, float leftLateral, bool enemy = true, float dt = kDt) {
	YieldInput in;
	in.hand[0] = Pose(rightLateral);
	in.hand[1] = Pose(leftLateral);
	in.enemyAhead = enemy;
	in.dt = dt;
	return in;
}

// Both hands rocking together: out by `amplitude`, in again, `cycles`
// times, `framesPerHalf` frames a half stroke (16 at 90 Hz with 8 cm is
// 0.45 m/s). `rightOnly` keeps the left hand still. Returns the frame the
// yield fired on, or -1; `frames` counts them.
int Rock(YieldState& s, float amplitude, int cycles, int framesPerHalf = 16, bool rightOnly = false,
         bool enemy = true, float dt = kDt, int* frames = nullptr) {
	float x = 0.0f;
	int frame = 0;
	int fired = -1;
	for (int c = 0; c < cycles && fired < 0; ++c) {
		for (int half = 0; half < 2 && fired < 0; ++half) {
			for (int i = 0; i < framesPerHalf; ++i) {
				x += (half == 0 ? 1.0f : -1.0f) * amplitude / static_cast<float>(framesPerHalf);
				if (StepYield(s, Frame(x, rightOnly ? 0.0f : x, enemy, dt))) {
					fired = frame;
					break;
				}
				++frame;
			}
		}
	}
	if (frames != nullptr) {
		*frames = frame;
	}
	return fired;
}

// Frames of a still, allowed pose.
void Hold(YieldState& s, int frames) {
	for (int i = 0; i < frames; ++i) {
		StepYield(s, Frame(0.0f, 0.0f));
	}
}

}  // namespace

int main() {
	std::printf("The rocking\n");
	{
		YieldState s;
		const int fired = Rock(s, 0.08f, 4);
		Check(fired >= 0, "both hands out and in, 8 cm, 0.45 m/s: the yield");
		Check(fired >= static_cast<int>(kYieldSpanSeconds / kDt) - 1, "not before a second of it has passed");
		Check(s.cooldownLeft > 0.0f && s.reversals == 0 && !s.hand[0].tracking, "then the cooldown, the count and the tracks reset");
		Check(Rock(s, 0.08f, 4) < 0, "right after: the cooldown holds it");
		Check(s.reversals == 0, "and the rocking within it is dropped, not kept for the cooldown's end");
		Hold(s, 300);
		Check(s.cooldownLeft == 0.0f && Rock(s, 0.08f, 5) >= 0, "3.3 s later, rocking again: again");
	}
	{
		YieldState s;
		Check(Rock(s, 0.03f, 6) < 0, "3 cm rocking: too small, no yield");
	}
	{
		YieldState s;
		Check(Rock(s, 0.08f, 4, 16, true) < 0 && (s.waiting & kYieldOneHandOnly) != 0,
		      "one hand rocking alone: no yield, 'one hand' holds it");
	}
	{
		// Fast reversals: 8 cm in 3 frames a half stroke is 2.4 m/s - too fast.
		YieldState s;
		Check(Rock(s, 0.08f, 6, 3) < 0 && (s.lastReset & kYieldRightFast) != 0 && (s.lastReset & kYieldLeftFast) != 0,
		      "8 cm in 3 frames (2.4 m/s): too fast, named");
	}
	{
		// A tremor: 5 cm strokes reversing every 4 frames (0.044 s) at 1.1 m/s
		// is too fast as well; slowed to 0.5 m/s by a longer frame the
		// reversals are a tremor's and are not counted.
		YieldState s;
		Check(Rock(s, 0.05f, 8, 4, false, true, 0.025f) < 0 && s.lastReset == 0,
		      "5 cm reversing every 0.1 s at 0.5 m/s: a tremor, nothing counted, nothing reset");
	}
	{
		// Too slow: half strokes of 1.6 s are a drift, not a rock - never counted.
		YieldState s;
		Check(Rock(s, 0.08f, 4, 16, false, true, 0.1f) < 0 && s.reversals == 0, "half strokes 1.6 s long: a drift, nothing counted");
		// Half strokes of 0.9 s (16 frames of 0.056 s): a slow rock, counted.
		YieldState slow;
		Check(Rock(slow, 0.08f, 4, 16, false, true, 0.056f) >= 0, "half strokes of 0.9 s: a slow rock, the yield");
	}
	{
		// A sweep: a stroke of 35 cm.
		YieldState s;
		int frames = 0;
		Rock(s, 0.08f, 1, 16, false, true, kDt, &frames);
		float x = 0.0f;
		for (int i = 0; i < 70; ++i) {
			x += 0.005f;
			StepYield(s, Frame(x, x));
		}
		Check(s.reversals == 0 && (s.lastReset & kYieldStrokeLong) != 0, "a 35 cm sweep: a stroke too long, the count dropped");
	}
	{
		// Rocking in mirror (out and in): the lateral is outward for each hand,
		// so both move the same way in this measure - the same yield.
		YieldState s;
		Check(Rock(s, 0.10f, 4, 20) >= 0, "10 cm out and in together: the yield");
	}

	std::printf("The gates\n");
	{
		YieldState s;
		Check(Rock(s, 0.08f, 4, 16, false, false) < 0 && (s.lastReset & kYieldNoEnemy) != 0, "no one in combat ahead: no yield, named");
	}
	struct Gate {
		const char* what;
		void (*spoil)(YieldHandInput&);
		UInt32 bit;
	};
	const Gate gates[] = {
		{"a hand not tracked", [](YieldHandInput& h) { h.tracked = false; }, kYieldRightNotTracked},
		{"a hand closed (curl 0.75)", [](YieldHandInput& h) { h.curl = 0.75f; }, kYieldRightClosed},
		{"a hand 60 cm under the eyes", [](YieldHandInput& h) { h.position.z = -0.6f; }, kYieldRightLow},
		{"a hand behind the eyes", [](YieldHandInput& h) { h.position.y = -0.1f; }, kYieldRightBehind},
		{"a palm turned to the side (0.3 of it to the front)", [](YieldHandInput& h) { h.palmAhead = 0.3f; },
		 kYieldRightPalmAway},
		{"a palm turned to the face", [](YieldHandInput& h) { h.palmAhead = -0.9f; }, kYieldRightPalmAway},
		{"a palm that is no number",
		 [](YieldHandInput& h) { h.palmAhead = std::numeric_limits<float>::quiet_NaN(); }, kYieldRightPalmAway},
		{"a hand swinging", [](YieldHandInput& h) { h.swinging = true; }, kYieldRightSwinging},
	};
	for (const Gate& g : gates) {
		YieldState s;
		float x = 0.0f;
		bool fired = false;
		for (int i = 0; i < 160 && !fired; ++i) {
			x += ((i / 16) % 2 == 0 ? 1.0f : -1.0f) * 0.005f;
			YieldInput in = Frame(x, x);
			g.spoil(in.hand[0]);
			fired = StepYield(s, in);
		}
		Check(!fired && (s.lastReset & g.bit) != 0, g.what);
	}
	{
		// The swing's hold: a swing one frame, then a second of rocking does
		// not count; after it, it does.
		YieldState s;
		YieldInput swing = Frame(0.0f, 0.0f);
		swing.hand[1].swinging = true;
		StepYield(s, swing);
		Check((s.lastReset & kYieldLeftSwinging) != 0, "the left hand swinging: reset, named");
		int frames = 0;
		Check(Rock(s, 0.08f, 2, 16, false, true, kDt, &frames) < 0 && frames == 64 && s.reversals == 0,
		      "0.7 s of rocking right after: nothing counted (the swing's hold)");
		Hold(s, 40);
		Check(Rock(s, 0.08f, 4) >= 0, "a second after the swing: the gesture counts again");
	}
	{
		// Open gates by the pose's margins.
		YieldState s;
		YieldInput in = Frame(0.0f, 0.0f);
		in.hand[0].position.z = -0.45f;
		in.hand[0].position.y = 0.1f;
		in.hand[0].palmAhead = 0.5f;
		in.hand[0].curl = 0.69f;
		StepYield(s, in);
		Check(s.lastReset == 0 && s.hand[0].tracking,
		      "45 cm under the eyes, 10 cm ahead, the palm 60 degrees off the front, curl 0.69: in");
	}
	{
		// The palm from the controller's rotation relative to the head, in the
		// game's axes (x right, y ahead, z up): the right palm is the
		// controller's -x, the left its +x.
		NiMatrix33 m{};
		const auto set = [&m](const NiPoint3& x, const NiPoint3& y, const NiPoint3& z) {
			m.data[0][0] = x.x, m.data[1][0] = x.y, m.data[2][0] = x.z;
			m.data[0][1] = y.x, m.data[1][1] = y.y, m.data[2][1] = y.z;
			m.data[0][2] = z.x, m.data[1][2] = z.y, m.data[2][2] = z.z;
		};
		// Held as a handle is, pointing ahead: the right palm faces left.
		set(NiPoint3{1.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 1.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 1.0f});
		Check(YieldPalmAhead(m, true) == 0.0f && YieldPalmAhead(m, false) == 0.0f,
		      "the controllers held as handles, pointing ahead: neither palm to the front");
		// Pointing up, its x turned to the back: the right palm faces the front.
		set(NiPoint3{0.0f, -1.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 1.0f}, NiPoint3{1.0f, 0.0f, 0.0f});
		Check(YieldPalmAhead(m, true) == 1.0f, "the right controller up, its x to the back: the right palm to the front");
		Check(YieldPalmAhead(m, false) == -1.0f, "the same turn on the left hand: the left palm to the face");
		// Pointing up, its x to the front: the left palm faces the front.
		set(NiPoint3{0.0f, 1.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 1.0f}, NiPoint3{-1.0f, 0.0f, 0.0f});
		Check(YieldPalmAhead(m, false) == 1.0f, "the left controller up, its x to the front: the left palm to the front");
	}
	{
		// A weapon in hand changes nothing here: the input has no weapon.
		Check(true, "(the weapon in hand or away: not a gate of the gesture since 2026-10-12)");
	}

	std::printf("The speed over its window\n");
	{
		YieldSpeedTrack t;
		Check(YieldSpeed(t, NiPoint3{0.0f, 0.0f, 0.0f}, kDt) == 0.0f, "one sample: no speed yet");
		float speed = 0.0f;
		for (int i = 1; i <= 9; ++i) {
			speed = YieldSpeed(t, NiPoint3{0.01f * static_cast<float>(i), 0.0f, 0.0f}, kDt);
		}
		Check(speed > 0.85f && speed < 0.95f, "1 cm a frame at 90 Hz: 0.9 m/s over the window");
		for (int i = 0; i < 20; ++i) {
			speed = YieldSpeed(t, NiPoint3{0.09f, 0.0f, 0.0f}, kDt);
		}
		Check(speed == 0.0f, "held still for the window: zero");
		YieldSpeedTrack jump;
		YieldSpeed(jump, NiPoint3{0.0f, 0.0f, 0.0f}, 0.05f);
		Check(YieldSpeed(jump, NiPoint3{0.0f, 0.4f, 0.0f}, 0.05f) > 7.0f, "a 40 cm jump in 0.05 s: 8 m/s");
		YieldSpeedTrack noTime;
		YieldSpeed(noTime, NiPoint3{0.0f, 0.0f, 0.0f}, 0.0f);
		Check(YieldSpeed(noTime, NiPoint3{1.0f, 0.0f, 0.0f}, 0.0f) == 0.0f, "no time passing: no speed");
	}

	std::printf("The rocking alone, for the log\n");
	{
		YieldState s;
		YieldInput in = Frame(0.0f, 0.0f, false);
		in.hand[0].tracked = false;
		in.hand[1].curl = 0.9f;
		const YieldInput loose = YieldRockingAlone(in);
		Check(loose.rockingAlone && !loose.enemyAhead && !loose.hand[0].tracked, "the frame as it is, every gate ignored");
		float x = 0.0f;
		bool fired = false;
		for (int i = 0; i < 200 && !fired; ++i) {
			x += ((i / 16) % 2 == 0 ? 1.0f : -1.0f) * 0.02f;  // 32 cm strokes at 1.8 m/s, no one ahead, a hand closed
			YieldInput f = Frame(x, x, false);
			f.hand[1].curl = 0.9f;
			fired = StepYield(s, YieldRockingAlone(f));
		}
		Check(fired, "fast, long rocking with a hand closed and no one ahead: the rocking alone completes all the same");
	}

	std::printf("How closed a hand is\n");
	const float onTrigger[5] = {0.1f, 1.0f, 0.1f, 0.2f, 0.3f};
	Check(YieldHandCurl(onTrigger) == 0.3f, "the index on the trigger does not count; the most curled of the rest");
	const float fist[5] = {0.9f, 0.9f, 0.95f, 0.9f, 0.8f};
	Check(YieldHandCurl(fist) == 0.95f && !(YieldHandCurl(fist) < kYieldOpenCurl), "a fist: closed");
	const float ringOnly[5] = {0.0f, 0.0f, 0.0f, 0.7f, 0.0f};
	Check(YieldHandCurl(ringOnly) == 0.7f, "one finger curled is enough to close it");

	std::printf("The reasons, for the log\n");
	char why[200] = {};
	Check(YieldReasons(0, why, sizeof(why)) == 0 && std::strcmp(why, "nothing") == 0, "none: nothing");
	Check(YieldReasons(kYieldRightFast, why, sizeof(why)) == 1 && std::strcmp(why, "the right hand too fast") == 0,
	      "one: named");
	Check(YieldReasons(kYieldLeftClosed | kYieldNoEnemy | kYieldOneHandOnly, why, sizeof(why)) == 3 &&
	          std::strcmp(why, "the left hand closed, no one in combat ahead of the head, one hand rocking alone") == 0,
	      "several: each named, comma separated, in the gates' order");
	char tiny[12] = {};
	Check(YieldReasons(kYieldRightSwinging, tiny, sizeof(tiny)) == 1 && std::strcmp(tiny, "the right h") == 0,
	      "a short buffer: cut and terminated");
	Check(YieldReasons(kYieldRightSwinging | kYieldTooQuick, tiny, 0) == 2, "no buffer: the count alone");

	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
