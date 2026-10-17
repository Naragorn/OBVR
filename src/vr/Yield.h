#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::vr {

// Yielding by gesture (the tester, 2026-10-07: "wenn ich die waffe
// einstecke und mit offenen händen mit der handfläche zu den gegnern zeige
// und diese leicht von aussen nach innen wippe und das wiederhole ein paar
// mal, zählt das als ein yield"; 2026-10-12: "die mit offenen handflächen
// zum gegner (egal ob mit oder ohne waffe oder schild) und beide hände
// leicht nach rechts und links wippen für ein paar sekunden - geste, muss
// etwas mehr gehärtet werden damit es nicht einfach so im combat passiert").
//
// Vanilla's yield is block and activate together, facing the attacker
// (docs/controls-spec.md). The gesture: both hands open (or gripping a
// drawn weapon, its palm to the front), held up before the body, an NPC in
// combat ahead, and both hands rocking sideways - out and in, or left and
// right together - gently for a second or two. Each hand's sideways position (outward from
// the body positive) is watched for reversals of direction after strokes of
// kYieldStrokeMinMetres to kYieldStrokeMaxMetres; kYieldReversals of them
// within kYieldWindowSeconds, at least kYieldReversalsEachHand from each
// hand and kYieldSpanSeconds from the first, are the yield. Then the caller
// holds block and presses activate with the pick on that NPC, and the
// engine's yield runs - the NPC may still refuse, as in vanilla. Not again
// within kYieldCooldownSeconds.
//
// Hardened on 2026-10-12 - the first version counted sideways reversals
// alone and fired on a hand swung about in a fight: a hand faster than
// kYieldMaxSpeed (its travel over the last kYieldSpeedWindowSeconds), a
// stroke longer than a rock, a hand swinging (the hand mode's swing, and
// kYieldSwingHoldSeconds after it), a hand hanging lower than
// kYieldBelowHeadMetres under the eyes or behind them, or its palm not
// turned to the front (kYieldPalmAhead) resets the
// gesture; a reversal within kYieldReversalGapSeconds of the last is a
// tremor's and one after more than kYieldStrokeMaxSeconds a drift's,
// neither counted; one hand rocking alone, or four reversals in under a
// second, is not it. The weapon may be in hand or away: the engine's block
// needs a weapon or the fists up, so with nothing readied the caller raises
// the fists first (CameraHook.cpp, YieldDrive - activate without a block
// opened the attacker's pockets in the harness).
//
// The palm, not the fingers (2026-10-12, the tester's run: "yield geht nun
// gar nicht mehr wenn ich es als geste mache"). The first hardening asked
// for the fingers up - the pinned hand bone's knuckles 12 degrees above
// level - and of the thirteen rockings his log caught with both hands open
// and high enough, twelve failed it on one hand or both (the knuckles'
// share up 0.12, 0.09, 0.11 and -0.05, -0.16 and -0.03 ...): how a hand is held
// open round an Index controller does not lift the bone's knuckles. What
// he described both times is the palm turned to the attacker; that is the
// gate now: each palm - the controller's side the fingers close over, as
// the hand HUD and the item pick read it - turned to the front, its share
// along the head's forward at least kYieldPalmAhead (60 degrees off it at
// most). A fist's knuckles, a blade's grip, a shield's strap and a slap's
// sideways palm all face elsewhere.
//
// The weapon hand's grip, and the open hand's palm (2026-10-16; the
// tester's session of 2026-10-14 night, "yield gingen gar nicht mit der
// geste": 44 rockings, no yield). Counted over his 44 "rocked, but no
// yield" lines: the weapon hand closed 37 times - its curl 1.00, the
// longsword in it, the fingers round the controller, which no open-hand
// rule can pass though the gesture allows the weapon; the left palm never
// within 60 degrees of the front (its share -0.81 to 0.49) while the
// right's read 0.52 to 0.69 in the rockings that look like his attempts.
// The side read is not the fault: the fit of his own hands ([Hands]
// LeftHandRoll 157.6, LeftHandPitch 71.0, LeftHandYaw 65.9 in his INI,
// written by adjusting the hands on his controllers) turns the left bone's
// palm axis - its +y, the side the fingers curl to in handtohandidle.kf
// and the Torch node sits on in skeleton.nif - to the controller's
// (0.99 -0.05 0.12), its +x, the axis read here; the right's +y to
// (-0.99 -0.08 -0.10), its -x. So what his open left hand reads for the
// palm is the pose itself, or the controller turned on its strap in a hand
// that holds nothing - unsettled, the next session's "Yield: the pose"
// lines are to tell; the weapon hand's controller, gripped, reads true.
// Now: a hand holding a drawn weapon (YieldHandInput::holdsWeapon) is not
// asked to open - its fingers hold the controller - and is the hand whose
// palm is asked to the front (kYieldPalmAhead, set from his sword hand's
// numbers), which keeps a guard's sideways palm out; an open hand is shown
// by its open fingers, its palm's share read for the log but no gate.
inline constexpr float kYieldStrokeMinMetres = 0.04f;
inline constexpr float kYieldStrokeMaxMetres = 0.30f;
inline constexpr UInt32 kYieldReversals = 4;          // two out-and-in cycles over both hands
inline constexpr UInt32 kYieldReversalsEachHand = 2;  // each hand out and in at least once
inline constexpr float kYieldReversalGapSeconds = 0.12f;  // reversals closer than this are a tremor
inline constexpr float kYieldStrokeMaxSeconds = 1.2f;     // a stroke longer than this is a drift, not a rock
inline constexpr float kYieldSpanSeconds = 1.0f;      // from the first reversal to the last
inline constexpr float kYieldWindowSeconds = 3.0f;
inline constexpr float kYieldCooldownSeconds = 3.0f;
inline constexpr float kYieldOpenCurl = 0.7f;         // a hand is open below this curl (YieldHandCurl)
inline constexpr float kYieldMaxSpeed = 0.9f;         // metres a second, over the speed window
inline constexpr float kYieldSpeedWindowSeconds = 0.1f;
inline constexpr float kYieldSwingHoldSeconds = 1.0f;
inline constexpr float kYieldBelowHeadMetres = 0.5f;  // a hand no lower than this under the eyes
inline constexpr float kYieldAheadMetres = 0.05f;     // and in front of them
// The gripping hand's palm share along the head's forward (cosine): within
// 73 degrees. The first guess was 0.5 (60 degrees); the tester's sword hand
// read 0.31 to 0.46 in a third of his attempts over two sessions
// (2026-10-14 and 2026-10-16) and 0.62 to 0.96 in the rest, so the cone is
// set from those numbers. What a guard's palm reads is unmeasured - it faces
// the side, about 0, by the way a blade is held.
inline constexpr float kYieldPalmAhead = 0.3f;

// One hand this frame. `position` is the hand relative to the head in
// metres: x outward from the body (positive for both hands), y ahead, z
// up. `palmAhead` is the share of the palm's direction along the head's
// forward (1: the palm straight to the front, 0: to the side, -1: to the
// face; YieldPalmAhead); `swinging` the hand mode's swing. `holdsWeapon`:
// a drawn weapon in this hand - a blade, a blunt weapon or a staff in the
// weapon hand, the bow in the bow hand - whose fingers hold the controller:
// not asked to open, and the hand whose palm is asked to the front.
struct YieldHandInput {
	bool tracked = false;
	float curl = 1.0f;
	NiPoint3 position{0.0f, 0.0f, 0.0f};
	float palmAhead = 1.0f;
	bool swinging = false;
	bool holdsWeapon = false;
};

struct YieldInput {
	YieldHandInput hand[2];  // [0] right, [1] left
	bool enemyAhead = false;
	float dt = 0.0f;
	// The rocking alone: every gate open and no stroke too long - the log's
	// "the hands rocked, but no yield" (YieldRockingAlone).
	bool rockingAlone = false;
};

// Why the gesture is not counting, as bits - for the log, and the reason
// of the last reset.
enum : UInt32 {
	kYieldRightNotTracked = 1u << 0,
	kYieldLeftNotTracked = 1u << 1,
	kYieldRightClosed = 1u << 2,
	kYieldLeftClosed = 1u << 3,
	kYieldNoEnemy = 1u << 4,
	kYieldRightLow = 1u << 5,
	kYieldLeftLow = 1u << 6,
	kYieldRightBehind = 1u << 7,
	kYieldLeftBehind = 1u << 8,
	kYieldRightPalmAway = 1u << 9,
	kYieldLeftPalmAway = 1u << 10,
	kYieldRightSwinging = 1u << 11,
	kYieldLeftSwinging = 1u << 12,
	kYieldRightFast = 1u << 13,
	kYieldLeftFast = 1u << 14,
	kYieldStrokeLong = 1u << 15,
	kYieldOneHandOnly = 1u << 16,
	kYieldTooQuick = 1u << 17,
};

// The last kYieldSpeedWindowSeconds of a hand's positions, for its speed.
inline constexpr UInt32 kYieldSpeedSamples = 16;

struct YieldSpeedTrack {
	NiPoint3 position[kYieldSpeedSamples] = {};
	float time[kYieldSpeedSamples] = {};
	UInt32 count = 0;
	UInt32 next = 0;
	float clock = 0.0f;
};

struct YieldHandTrack {
	bool tracking = false;
	float last = 0.0f;      // the sideways position seen last
	int direction = 0;      // +1 outward, -1 inward, 0 none yet
	float travel = 0.0f;    // since the last reversal
	UInt32 reversals = 0;   // counted within the window
	float sinceReversal = 0.0f;
	float swingHold = 0.0f;  // seconds left after a swing
	YieldSpeedTrack speed;
};

struct YieldState {
	YieldHandTrack hand[2];  // [0] right, [1] left
	UInt32 reversals = 0;
	float windowSeconds = 0.0f;  // since the first counted reversal
	float cooldownLeft = 0.0f;
	UInt32 lastReset = 0;        // the gate bits that last reset the count
	UInt32 waiting = 0;          // what holds a full count back (one hand, too quick)
};

inline float YieldLength(const NiPoint3& p) { return math::Sqrt(p.x * p.x + p.y * p.y + p.z * p.z); }

// The hand's speed over the last window: its travel from the oldest sample
// still within it. Zero until two samples are in.
inline float YieldSpeed(YieldSpeedTrack& t, const NiPoint3& position, float dt) {
	t.clock += dt > 0.0f ? dt : 0.0f;
	t.position[t.next] = position;
	t.time[t.next] = t.clock;
	t.next = (t.next + 1) % kYieldSpeedSamples;
	if (t.count < kYieldSpeedSamples) {
		++t.count;
	}
	// The oldest sample within the window.
	float speed = 0.0f;
	bool found = false;
	for (UInt32 back = t.count; back >= 2 && !found; --back) {
		const UInt32 at = (t.next + kYieldSpeedSamples - back) % kYieldSpeedSamples;
		const float age = t.clock - t.time[at];
		if (age <= kYieldSpeedWindowSeconds + 1e-4f && age > 1e-5f) {
			const NiPoint3 moved = position - t.position[at];
			speed = YieldLength(moved) / age;
			found = true;
		}
	}
	return speed;
}

// The gates of one hand this frame, as bits for its side; the speed track
// is stepped here.
inline UInt32 YieldHandGates(YieldHandTrack& h, const YieldHandInput& in, bool right, float dt) {
	UInt32 bits = 0;
	if (!in.tracked) {
		return right ? kYieldRightNotTracked : kYieldLeftNotTracked;
	}
	// Open by its fingers - unless they hold a drawn weapon's controller.
	if (!in.holdsWeapon && !(in.curl < kYieldOpenCurl)) {
		bits |= right ? kYieldRightClosed : kYieldLeftClosed;
	}
	if (!(in.position.z > -kYieldBelowHeadMetres)) {
		bits |= right ? kYieldRightLow : kYieldLeftLow;
	}
	if (!(in.position.y > kYieldAheadMetres)) {
		bits |= right ? kYieldRightBehind : kYieldLeftBehind;
	}
	// The palm to the front, asked of the hand that grips a weapon: its
	// controller lies in the palm and reads it true, and a guard's palm faces
	// sideways. An open hand's palm is read for the log, not gated (2026-10-16).
	if (in.holdsWeapon && !(in.palmAhead >= kYieldPalmAhead)) {
		bits |= right ? kYieldRightPalmAway : kYieldLeftPalmAway;
	}
	if (in.swinging) {
		h.swingHold = kYieldSwingHoldSeconds;
	} else if (h.swingHold > 0.0f) {
		h.swingHold -= dt > 0.0f ? dt : 0.0f;
	}
	if (in.swinging || h.swingHold > 0.0f) {
		bits |= right ? kYieldRightSwinging : kYieldLeftSwinging;
	}
	if (YieldSpeed(h.speed, in.position, dt) > kYieldMaxSpeed) {
		bits |= right ? kYieldRightFast : kYieldLeftFast;
	}
	return bits;
}

inline void YieldResetCount(YieldState& s, UInt32 why) {
	for (YieldHandTrack& h : s.hand) {
		h.tracking = false;
		h.last = 0.0f;
		h.direction = 0;
		h.travel = 0.0f;
		h.reversals = 0;
		h.sinceReversal = 0.0f;
	}
	s.reversals = 0;
	s.windowSeconds = 0.0f;
	s.waiting = 0;
	if (why != 0) {
		s.lastReset = why;
	}
}

// One frame. True on the frame the gesture is complete.
inline bool StepYield(YieldState& s, const YieldInput& in) {
	const float dt = in.dt > 0.0f && in.dt < 1.0f ? in.dt : 0.0f;
	if (s.cooldownLeft > 0.0f) {
		s.cooldownLeft -= dt;
		if (s.cooldownLeft < 0.0f) {
			s.cooldownLeft = 0.0f;
		}
	}
	UInt32 gates = YieldHandGates(s.hand[0], in.hand[0], true, dt) | YieldHandGates(s.hand[1], in.hand[1], false, dt);
	if (!in.enemyAhead) {
		gates |= kYieldNoEnemy;
	}
	if (in.rockingAlone) {
		gates = 0;
	}
	if (gates != 0) {
		YieldResetCount(s, gates);
		return false;
	}
	for (int side = 0; side < 2; ++side) {
		YieldHandTrack& h = s.hand[side];
		const float lateral = in.hand[side].position.x;
		h.sinceReversal += dt;
		if (!h.tracking) {
			h.tracking = true;
			h.last = lateral;
			continue;
		}
		const float delta = lateral - h.last;
		h.last = lateral;
		const int direction = delta > 0.0f ? 1 : (delta < 0.0f ? -1 : 0);
		if (direction == 0) {
			continue;
		}
		const float step = delta < 0.0f ? -delta : delta;
		if (h.direction == 0) {
			h.direction = direction;
			h.travel = step;
			continue;
		}
		if (direction == h.direction) {
			h.travel += step;
			if (h.travel > kYieldStrokeMaxMetres && !in.rockingAlone) {
				YieldResetCount(s, kYieldStrokeLong);  // a sweep, not a rock
				return false;
			}
			continue;
		}
		// A reversal: it counts when the stroke before it was long enough,
		// not a tremor's (the last reversal, counted or not, a gap ago) and
		// not a drift's (begun within the stroke's time).
		if (h.travel >= kYieldStrokeMinMetres && h.sinceReversal >= kYieldReversalGapSeconds &&
		    h.sinceReversal <= kYieldStrokeMaxSeconds) {
			++h.reversals;
			++s.reversals;
		}
		h.sinceReversal = 0.0f;
		h.direction = direction;
		h.travel = step;
	}
	if (s.reversals > 0) {
		s.windowSeconds += dt;
		if (s.windowSeconds > kYieldWindowSeconds) {
			YieldResetCount(s, 0);
			return false;
		}
	}
	s.waiting = 0;
	if (s.reversals >= kYieldReversals) {
		if (s.hand[0].reversals < kYieldReversalsEachHand || s.hand[1].reversals < kYieldReversalsEachHand) {
			s.waiting |= kYieldOneHandOnly;
		}
		if (s.windowSeconds < kYieldSpanSeconds) {
			s.waiting |= kYieldTooQuick;
		}
		if (s.waiting == 0) {
			if (s.cooldownLeft > 0.0f) {
				// A gesture complete within the cooldown is dropped, not kept
				// to fire the moment the cooldown ends with the hands at rest
				// (yield_test found it firing 1.6 s after the rocking).
				YieldResetCount(s, 0);
				return false;
			}
			YieldResetCount(s, 0);
			s.cooldownLeft = kYieldCooldownSeconds;
			return true;
		}
	}
	return false;
}

// How closed a hand is for the yield: the most curled of the middle, ring
// and little fingers (curls thumb, index, middle, ring, little). The index
// is left out - on the Index controller it rests on the trigger and read
// 0.6 to 1.0 with the hands held open in the tester's log (2026-10-07,
// "the left index curled" on every rocking), so the index said nothing
// about the hand being open.
inline float YieldHandCurl(const float curl[5]) {
	float most = curl[2];
	if (curl[3] > most) {
		most = curl[3];
	}
	if (curl[4] > most) {
		most = curl[4];
	}
	return most;
}

// The palm's share along the head's forward, -1 to 1, from the controller's
// rotation relative to the head in the game's axes (x right, y ahead, z up;
// HandModeResult's hand rotation): the palm is the controller's side the
// fingers close over - its -x on the right hand, +x on the left - as the hand
// HUD (vr/HandHud.h, PalmNormal) and the item pick read it; the tester sees
// the hand HUD on the palm he turns to his face, so the sides hold.
inline float YieldPalmAhead(const NiMatrix33& controllerRelative, bool rightHand) {
	const float side = rightHand ? -1.0f : 1.0f;
	return controllerRelative.data[1][0] * side;
}

// The same frame with every gate open - the rocking alone, for the log's
// "the hands rocked, but no yield".
inline YieldInput YieldRockingAlone(const YieldInput& in) {
	YieldInput loose = in;
	loose.rockingAlone = true;
	return loose;
}

// The reasons, comma separated, into `out` (at most `size` chars, always
// terminated); "nothing" for none. Returns how many.
inline UInt32 YieldReasons(UInt32 bits, char* out, UInt32 size) {
	struct Named {
		UInt32 bit;
		const char* text;
	};
	static const Named kNamed[] = {
		{kYieldRightNotTracked, "the right hand not tracked"},
		{kYieldLeftNotTracked, "the left hand not tracked"},
		{kYieldRightClosed, "the right hand closed"},
		{kYieldLeftClosed, "the left hand closed"},
		{kYieldNoEnemy, "no one in combat ahead of the head"},
		{kYieldRightLow, "the right hand too low"},
		{kYieldLeftLow, "the left hand too low"},
		{kYieldRightBehind, "the right hand not ahead"},
		{kYieldLeftBehind, "the left hand not ahead"},
		{kYieldRightPalmAway, "the right palm not to the front"},
		{kYieldLeftPalmAway, "the left palm not to the front"},
		{kYieldRightSwinging, "the right hand swinging"},
		{kYieldLeftSwinging, "the left hand swinging"},
		{kYieldRightFast, "the right hand too fast"},
		{kYieldLeftFast, "the left hand too fast"},
		{kYieldStrokeLong, "a stroke too long for a rock"},
		{kYieldOneHandOnly, "one hand rocking alone"},
		{kYieldTooQuick, "over in under a second"},
	};
	UInt32 n = 0;
	UInt32 at = 0;
	const auto put = [&](const char* text) {
		for (const char* c = text; *c != '\0' && at + 1 < size; ++c) {
			out[at++] = *c;
		}
	};
	if (size == 0) {
		for (const Named& named : kNamed) {
			n += (bits & named.bit) != 0 ? 1u : 0u;
		}
		return n;
	}
	for (const Named& named : kNamed) {
		if ((bits & named.bit) == 0) {
			continue;
		}
		if (n > 0) {
			put(", ");
		}
		put(named.text);
		++n;
	}
	if (n == 0) {
		put("nothing");
	}
	out[at] = '\0';
	return n;
}

}  // namespace obvr::vr
