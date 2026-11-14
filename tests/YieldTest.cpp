// Checks the yield by gesture (vr/Yield.h): the gates of a hand (tracked,
// open - or gripping a drawn weapon, its palm then to the front - up before
// the eyes, not swinging nor fast), the enemy ahead; the rocking - both
// hands, strokes of a rock's size, not a tremor, not over in a second,
// within the window, the cooldown; the speed over its window; the rocking
// alone for the log; the reasons' text.

#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>

#include "vr/Yield.h"
#include "vr/YieldBody.h"

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

// The gates' rocking: 8 cm strokes at 0.45 m/s (16 frames a half stroke)
// for `frames` frames, `spoil` applied to every frame's input. The frame the
// yield fired on, or -1 - unspoiled it fires once a second of reversals has
// passed, about frame 113.
template <typename Spoil>
int RockSpoiled(YieldState& s, Spoil spoil, int frames = 160) {
	float x = 0.0f;
	for (int i = 0; i < frames; ++i) {
		x += ((i / 16) % 2 == 0 ? 1.0f : -1.0f) * 0.005f;
		YieldInput in = Frame(x, x);
		spoil(in);
		if (StepYield(s, in)) {
			return i;
		}
	}
	return -1;
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
		Check(Rock(s, 0.015f, 6) < 0, "1.5 cm back and forth: too small, no yield");
		YieldState two;
		Check(Rock(two, 0.025f, 6, 16) >= 0, "2.5 cm back and forth at a wave's pace: the yield (at least 2 cm)");
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
		// No upper limit (2026-11-11, "mehr ist ok"): strokes of 35 cm at a
		// slow wave's pace (0.35 m/s, 0.53 s a stroke) still count.
		YieldState s;
		Check(Rock(s, 0.35f, 4, 48) >= 0, "35 cm strokes at 0.35 m/s: the yield, no upper limit");
	}
	{
		// Rocking in mirror (out and in): the lateral is outward for each hand,
		// so both move the same way in this measure - the same yield.
		YieldState s;
		Check(Rock(s, 0.10f, 4, 20) >= 0, "10 cm out and in together: the yield");
	}

	std::printf("The wave by the tilt (2026-11-11)\n");
	{
		// The hand held still, the fingers tilted like a wiper: the fingertip
		// moves sideways by kYieldFingertipMetres times the tilt's sine.
		const auto wave = [](YieldState& s, float degrees, int framesPerHalf, int cycles, float dt = kDt,
		                     bool rightOnly = false) {
			for (int c = 0; c < cycles; ++c) {
				for (int half = 0; half < 2; ++half) {
					for (int i = 0; i < framesPerHalf; ++i) {
						const float t = static_cast<float>(i + 1) / static_cast<float>(framesPerHalf);
						const float a = (half == 0 ? -1.0f + 2.0f * t : 1.0f - 2.0f * t) * degrees * 0.0174533f;
						YieldInput in = Frame(0.0f, 0.0f, true, dt);
						in.hand[0].fingers = NiPoint3{math::Sin(a), 0.0f, math::Cos(a)};
						in.hand[1].fingers = rightOnly ? NiPoint3{0.0f, 0.0f, 1.0f} : NiPoint3{math::Sin(a), 0.0f, math::Cos(a)};
						if (StepYield(s, in)) {
							return true;
						}
					}
				}
			}
			return false;
		};
		YieldState s;
		Check(wave(s, 18.0f, 22, 4), "a wiper's wave of 18 degrees either way at 2 Hz, the hands still: the yield");
		YieldState small;
		Check(!wave(small, 3.0f, 22, 6), "a tilt of 3 degrees either way (1.3 cm at the fingertip): too small");
		YieldState jitter;
		Check(!wave(jitter, 18.0f, 6, 10), "the same tilt at over 3 Hz (a stroke of 0.067 s): a tremor, no yield");
		YieldState drift;
		Check(!wave(drift, 18.0f, 110, 3), "the same tilt at under 0.5 Hz (a stroke of 1.2 s): a drift, no yield");
		YieldState one;
		Check(!wave(one, 18.0f, 22, 4, kDt, true) && (one.waiting & kYieldOneHandOnly) != 0,
		      "one hand waving, the other still: no yield, 'one hand' holds it");
		// Tilt and move together: the fingertip counts both, once.
		YieldState both;
		float x = 0.0f;
		bool fired = false;
		for (int i = 0; i < 200 && !fired; ++i) {
			const float phase = ((i / 22) % 2 == 0) ? 1.0f : -1.0f;
			x += phase * 0.0005f;
			const float a = x * 10.0f;
			YieldInput in = Frame(x, x);
			in.hand[0].fingers = NiPoint3{math::Sin(a), 0.0f, math::Cos(a)};
			in.hand[1].fingers = in.hand[0].fingers;
			fired = StepYield(both, in);
		}
		Check(fired, "a small move (1.1 cm) with a small tilt (6 degrees) together: the fingertip's sum counts");
	}
	{
		// The fingers from the controller's rotation: its pointing axis, the
		// game's local +y (the matrix's second column), the left's x outward.
		NiMatrix33 m{};
		m.data[0][1] = 0.3f;  // points 0.3 right
		m.data[1][1] = 0.0f;
		m.data[2][1] = 0.95f;  // and up
		m.data[0][2] = 1.0f;   // its top outward: not the fingers
		const NiPoint3 r = YieldHandFingers(m, true);
		const NiPoint3 l = YieldHandFingers(m, false);
		Check(r.x == 0.3f && l.x == -0.3f && r.z == 0.95f && l.z == 0.95f,
		      "the fingers along the pointing axis (the second column), not the top; to the right is outward for "
		      "the right hand, inward for the left");
	}

	std::printf("The hands in the body's frame (2026-11-11, go yield A)\n");
	{
		// A scene in tracking space (x right, y up, -z forward): the neck at
		// 1.5 m, the eyes swung about it as the head turns (kEyesFromNeck),
		// the hands up before the chest, pointing up, palms to the front.
		struct Scene {
			BodyFrameState body;
			YieldState yield;
			float headYawDeg = 0.0f;
			float headPitchDeg = 0.0f;
			NiPoint3 neck{0.0f, 1.5f, 0.0f};
			float tiltDeg = 0.0f;  // the hands' wiper tilt about the forward axis
			bool fired = false;
			bool headRelative = false;  // the old reading, for contrast
			void Step(float dt) {
				const Quaternion head =
					FromAxisAngle(0.0f, 1.0f, 0.0f, headYawDeg) * FromAxisAngle(1.0f, 0.0f, 0.0f, headPitchDeg);
				const NiPoint3 eyes = neck + ToMatrix(head) * kEyesFromNeck;
				const BodyFrame f = StepBodyFrame(body, true, head, eyes, dt, game::TorsoYawSettings{}, false, false);
				YieldInput in = Frame(0.0f, 0.0f, true, dt);
				for (int side = 0; side < 2; ++side) {
					const bool right = side == 0;
					const NiPoint3 hand{right ? 0.20f : -0.20f, 1.40f, -0.45f};
					const Quaternion rot =
						FromAxisAngle(0.0f, 0.0f, 1.0f, tiltDeg) * FromAxisAngle(1.0f, 0.0f, 0.0f, 90.0f);
					YieldHandInput& h = in.hand[side];
					if (headRelative) {
						// The hand relative to the eyes in the head's axes, the
						// fingers likewise - the reading before 2026-11-11.
						const NiMatrix33 hm = ToMatrix(head);
						const NiPoint3 d = hand - eyes;
						const NiPoint3 local{hm.data[0][0] * d.x + hm.data[1][0] * d.y + hm.data[2][0] * d.z,
						                     hm.data[0][1] * d.x + hm.data[1][1] * d.y + hm.data[2][1] * d.z,
						                     hm.data[0][2] * d.x + hm.data[1][2] * d.y + hm.data[2][2] * d.z};
						h.position = NiPoint3{right ? local.x : -local.x, -local.z, local.y};
						h.fingers = NiPoint3{0.0f, 0.0f, 1.0f};
					} else {
						YieldHandInBody(f, hand, rot, right, h);
					}
				}
				fired = StepYield(yield, in) || fired;
			}
		};
		const auto run = [](Scene& sc, float seconds, auto&& move) {
			const int frames = static_cast<int>(seconds / kDt);
			for (int i = 0; i < frames; ++i) {
				move(sc, static_cast<float>(i) * kDt);
				sc.Step(kDt);
			}
		};
		{
			Scene sc;
			run(sc, 6.0f, [](Scene& c, float t) { c.headYawDeg = 30.0f * math::Sin(t * 2.0f * 3.14159f); });
			Check(!sc.fired, "the head turning 30 degrees either way at 1 Hz, the hands still: no yield");
		}
		{
			// Following a circling bandit: a few degrees to and fro. Read
			// against the head, the still hands move 4 cm sideways - a wave.
			Scene old;
			old.headRelative = true;
			run(old, 6.0f, [](Scene& c, float t) { c.headYawDeg = 5.0f * math::Sin(t * 2.0f * 3.14159f); });
			Check(old.fired, "the head following 5 degrees to and fro, read against the head (the old way): a yield - the cause");
			Scene sc;
			run(sc, 6.0f, [](Scene& c, float t) { c.headYawDeg = 5.0f * math::Sin(t * 2.0f * 3.14159f); });
			Check(!sc.fired, "the same read against the body: no yield");
		}
		{
			Scene sc;
			run(sc, 6.0f, [](Scene& c, float t) {
				const float j = math::Sin(t * 2.0f * 2.0f * 3.14159f);
				c.neck = NiPoint3{0.03f * j, 1.5f + 0.02f * j, 0.02f * j};
				c.headPitchDeg = 8.0f * j;
			});
			Check(!sc.fired, "the head jolting 3 cm and 8 degrees at 2 Hz, the hands still: no yield");
		}
		{
			Scene sc;
			run(sc, 4.0f, [](Scene& c, float t) { c.tiltDeg = 18.0f * math::Sin(t * 2.0f * 3.14159f); });
			Check(sc.fired, "a real wave (18 degrees at 1 Hz), the head still: the yield");
		}
		{
			Scene sc;
			run(sc, 4.0f, [](Scene& c, float t) {
				c.tiltDeg = 18.0f * math::Sin(t * 2.0f * 3.14159f);
				c.headYawDeg = 20.0f * math::Sin(t * 0.7f * 3.14159f);
			});
			Check(sc.fired, "the same wave while the head turns 20 degrees to and fro: still the yield");
		}
		{
			BodyFrame none;
			YieldHandInput h;
			Check(!YieldHandInBody(none, NiPoint3{0.0f, 1.0f, 0.0f}, Quaternion::Identity(), true, h) && !h.roomLateral,
			      "no body frame: the caller's head reading stays");
		}
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
		{"a weapon hand's palm turned back past the side (-0.4 of it to the front)",
		 [](YieldHandInput& h) {
			 h.holdsWeapon = true;
			 h.curl = 1.0f;
			 h.palmAhead = -0.4f;
		 },
		 kYieldRightPalmAway},
		{"a weapon hand's palm turned to the face",
		 [](YieldHandInput& h) {
			 h.holdsWeapon = true;
			 h.curl = 1.0f;
			 h.palmAhead = -0.9f;
		 },
		 kYieldRightPalmAway},
		{"a weapon hand's palm that is no number",
		 [](YieldHandInput& h) {
			 h.holdsWeapon = true;
			 h.curl = 1.0f;
			 h.palmAhead = std::numeric_limits<float>::quiet_NaN();
		 },
		 kYieldRightPalmAway},
		{"a hand swinging", [](YieldHandInput& h) { h.swinging = true; }, kYieldRightSwinging},
	};
	for (const Gate& g : gates) {
		YieldState s;
		const int fired = RockSpoiled(s, [&g](YieldInput& in) { g.spoil(in.hand[0]); });
		Check(fired < 0 && (s.lastReset & g.bit) != 0, g.what);
	}
	{
		YieldState s;
		Check(RockSpoiled(s, [](YieldInput&) {}) >= 0, "the same rocking unspoiled: the yield");
	}

	std::printf("The weapon hand and the open hand's palm (2026-10-16)\n");
	{
		// A drawn weapon in the right hand: its fingers round the controller
		// (curl 1.0) are no gate, its palm to the front is.
		YieldState s;
		Check(RockSpoiled(s, [](YieldInput& in) {
			      in.hand[0].holdsWeapon = true;
			      in.hand[0].curl = 1.0f;
		      }) >= 0,
		      "a weapon in the right hand, gripped (curl 1.0), its palm to the front: the yield");
		YieldState closed;
		Check(RockSpoiled(closed, [](YieldInput& in) { in.hand[0].curl = 1.0f; }) < 0 &&
		          (closed.lastReset & kYieldRightClosed) != 0,
		      "the right hand closed with nothing in it: no yield, named");
	}
	{
		// An open hand's palm is a gate too (2026-11-03), since 2026-11-14 to
		// the front or nearly, angled up to about 100 degrees (Nadi).
		for (float palm : {0.3f, 0.0f, -0.15f}) {
			YieldState front;
			Check(RockSpoiled(front, [palm](YieldInput& in) { in.hand[1].palmAhead = palm; }) >= 0,
			      "the open left hand's palm to the front, sideways or angled a little past: the yield");
		}
		for (float palm : {-0.25f, -0.6f, std::numeric_limits<float>::quiet_NaN()}) {
			YieldState s;
			Check(RockSpoiled(s, [palm](YieldInput& in) { in.hand[1].palmAhead = palm; }) < 0 &&
			          (s.lastReset & kYieldLeftPalmAway) != 0,
			      "the open left hand's palm turned back, to the face or no number: no yield, named");
		}
		YieldState guard;
		Check(RockSpoiled(guard, [](YieldInput& in) {
			      in.hand[0].palmAhead = -0.01f;
			      in.hand[1].palmAhead = 0.22f;
		      }) >= 0,
		      "the guard of 2026-11-03 (palms -0.01 and 0.22, both open): a yield since 2026-11-14 (angled palms count)");
	}
	{
		// The bow in the left hand: gripped, and its palm asked.
		YieldState s;
		Check(RockSpoiled(s, [](YieldInput& in) {
			      in.hand[1].holdsWeapon = true;
			      in.hand[1].curl = 1.0f;
		      }) >= 0,
		      "the bow in the left hand, gripped, its palm to the front: the yield");
		YieldState away;
		Check(RockSpoiled(away, [](YieldInput& in) {
			      in.hand[1].holdsWeapon = true;
			      in.hand[1].curl = 1.0f;
			      in.hand[1].palmAhead = -0.4f;
		      }) < 0 &&
		          (away.lastReset & kYieldLeftPalmAway) != 0 && (away.lastReset & kYieldLeftClosed) == 0,
		      "the bow hand's palm turned back: no yield, the palm named, not the grip");
	}
	{
		// Both hands gripping (a sword and the bow cannot both be drawn, but
		// the logic has no say in it): both palms asked.
		YieldState s;
		Check(RockSpoiled(s, [](YieldInput& in) {
			      for (YieldHandInput& h : in.hand) {
				      h.holdsWeapon = true;
				      h.curl = 1.0f;
			      }
			      in.hand[0].palmAhead = 0.9f;
			      in.hand[1].palmAhead = -0.4f;
		      }) < 0 &&
		          (s.lastReset & kYieldLeftPalmAway) != 0 && (s.lastReset & kYieldRightPalmAway) == 0,
		      "both hands gripping, the left palm turned back: no yield, the left palm named");
	}
	std::printf("What a hand may grip (2026-11-11: with weapons, the shield and the torch)\n");
	{
		// The rule's every flow: the weapon hand grips a drawn melee weapon
		// only; the other hand the drawn bow, a shield up or a torch.
		Check(YieldHandHolds(true, true, false, false, false), "the weapon hand, a melee weapon drawn: gripping");
		Check(!YieldHandHolds(true, false, false, false, false), "the weapon hand, nothing drawn: not gripping");
		Check(!YieldHandHolds(true, false, true, true, true),
		      "the weapon hand with the bow, a shield or a torch only in the other: not gripping");
		Check(YieldHandHolds(false, false, true, false, false), "the other hand, the bow drawn: gripping");
		Check(YieldHandHolds(false, true, false, true, false), "the other hand, a shield up: gripping");
		Check(YieldHandHolds(false, false, false, false, true), "the other hand, a torch: gripping");
		Check(!YieldHandHolds(false, true, false, false, false),
		      "the other hand with a one-hander drawn and nothing in it: not gripping (asked to open)");
		// Left-handed the roles have swapped before this (vr::AssignHandRoles):
		// the weapon hand is the left controller, the rule the same by role.
		Check(YieldHandHolds(true, true, false, false, false) && YieldHandHolds(false, true, false, true, false),
		      "left-handed: the same rule by the hands' roles");
	}
	{
		// A sword and a shield: both hands gripping (curl 1.0), both palms to
		// the front - the yield; before 2026-11-11 the shield hand was "closed".
		const auto sword = [](YieldInput& in) {
			in.hand[0].holdsWeapon = YieldHandHolds(true, true, false, false, false);
			in.hand[0].curl = 1.0f;
		};
		YieldState s;
		Check(RockSpoiled(s, [&sword](YieldInput& in) {
			      sword(in);
			      in.hand[1].holdsWeapon = YieldHandHolds(false, true, false, true, false);
			      in.hand[1].curl = 1.0f;
		      }) >= 0,
		      "a sword and a shield, both gripped, palms to the front: the yield");
		YieldState before;
		Check(RockSpoiled(before, [&sword](YieldInput& in) {
			      sword(in);
			      in.hand[1].curl = 1.0f;  // the shield's strap gripped, not counted as holding
		      }) < 0 &&
		          (before.lastReset & kYieldLeftClosed) != 0,
		      "the same with the shield hand not counted as holding (the old rule): no yield, the left closed");
		YieldState torch;
		Check(RockSpoiled(torch, [](YieldInput& in) {
			      in.hand[1].holdsWeapon = YieldHandHolds(false, false, false, false, true);
			      in.hand[1].curl = 1.0f;
		      }) >= 0,
		      "a torch in the left hand, the right open: the yield");
		YieldState twoHander;
		Check(RockSpoiled(twoHander, [](YieldInput& in) {
			      in.hand[0].holdsWeapon = YieldHandHolds(true, true, false, false, false);
			      in.hand[0].curl = 1.0f;
			      in.hand[1].holdsWeapon = YieldHandHolds(false, true, false, false, false);
			      in.hand[1].curl = 1.0f;  // the other hand round a two-hander's handle
		      }) < 0 &&
		          (twoHander.lastReset & kYieldLeftClosed) != 0,
		      "a two-hander in both hands, rocked: a guard, no yield (the other hand closed)");
	}
	{
		// A block series: the shield held up still, the sword hand swinging
		// between blocks - no yield however long it goes on.
		YieldState s;
		float x = 0.0f;
		bool fired = false;
		for (int i = 0; i < 400; ++i) {
			YieldInput in = Frame(0.0f, 0.0f);
			in.hand[0].holdsWeapon = true;
			in.hand[0].curl = 1.0f;
			in.hand[1].holdsWeapon = true;
			in.hand[1].curl = 1.0f;
			x += ((i / 20) % 2 == 0 ? 1.0f : -1.0f) * 0.004f;
			in.hand[0].position.x = x;
			in.hand[0].swinging = (i % 45) < 15;
			fired = fired || StepYield(s, in);
		}
		Check(!fired && (s.lastReset & kYieldRightSwinging) != 0, "a block series with swings between: no yield, the swing named");
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
		      "45 cm under the eyes, 10 cm ahead, curl 0.69 (the palm not asked of an open hand): in");
		YieldState w;
		YieldInput armed = Frame(0.0f, 0.0f);
		armed.hand[0].holdsWeapon = true;
		armed.hand[0].curl = 1.0f;
		armed.hand[0].palmAhead = 0.3f;
		StepYield(w, armed);
		Check(w.lastReset == 0 && w.hand[0].tracking,
		      "a weapon hand gripped, its palm 73 degrees off the front (the tester's 0.31): in");
		armed.hand[0].palmAhead = -0.21f;
		StepYield(w, armed);
		Check((w.lastReset & kYieldRightPalmAway) != 0 && (w.lastReset & kYieldRightClosed) == 0,
		      "a weapon hand gripped, its palm 102 degrees off: out, the palm named, not the grip");
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
		set(NiPoint3{1.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 1.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 1.0f});
		Check(YieldPalmInward(m) == 1.0f, "held as handles: each palm faces the other hand");
		set(NiPoint3{0.0f, -1.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 1.0f}, NiPoint3{1.0f, 0.0f, 0.0f});
		Check(YieldPalmInward(m) == 0.0f, "a palm to the front or the face: none of it inward");
		set(NiPoint3{-1.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 1.0f, 0.0f}, NiPoint3{0.0f, 0.0f, -1.0f});
		Check(YieldPalmInward(m) == -1.0f, "the palms turned outward: -1");
	}
	{
		// The palms facing each other (Nadi, 2026-11-14: "auch wenn diese nicht
		// direkt nach vorne zeigen sondern von der einen handfläche zur anderen
		// ... aber auch leicht nach vorne"): a palm turned to the other hand
		// counts down to kYieldPalmInwardAhead of the front.
		for (float ahead : {-0.3f, -0.45f}) {
			YieldState s;
			Check(RockSpoiled(s, [ahead](YieldInput& in) {
				      for (YieldHandInput& h : in.hand) {
					      h.palmAhead = ahead;
					      h.palmInward = 0.9f;
				      }
			      }) >= 0,
			      "palms facing each other, a little turned back by the controllers' fit: the yield");
		}
		YieldState sideways;
		Check(RockSpoiled(sideways, [](YieldInput& in) {
			      in.hand[1].palmAhead = -0.3f;
			      in.hand[1].palmInward = 0.3f;
		      }) < 0 && (sideways.lastReset & kYieldLeftPalmAway) != 0,
		      "a palm turned back and not to the other hand: no yield, named");
		YieldState back;
		Check(RockSpoiled(back, [](YieldInput& in) {
			      in.hand[1].palmAhead = -0.5f;
			      in.hand[1].palmInward = 0.85f;
		      }) < 0 && (back.lastReset & kYieldLeftPalmAway) != 0,
		      "facing the other hand but turned back past -0.45: no yield");
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
