#pragma once

// Swimming with the arms (docs/swimming-spec.md; Nadi, 2026-11-27: "go
// schwimmen mit den armen"). While the game has the player swimming, a hand
// that holds the water (its grip closed, nothing else in it) and is pulled
// through it carries the swimmer the other way; the swimmer then glides.
//
// The shape of it, from a rival mod's nine playtests (MIT, out of process;
// ideas only): the grip holds the water and the pull carries the swimmer,
// several times the hand's pace and growing with it less than in step, both
// held hands averaged into one stroke so a breaststroke's sweeps cancel, a
// stroke counted only while it goes on away from where it started, a stroke
// never slowing the swimmer except a clear push back, the glide easing out over
// seconds, the hands measured against the head. Its palm- and physics-based
// first builds were "terrible, unnatural": a controller cannot tell a palm that
// catches the water from one that slices back through it, the grip can.
//
// Everything here is plain values, in the world's axes (x east, y north, z up)
// with metres a second for the hands and game units a second for the swimmer;
// camera/hands/Swim.cpp reads the game and the hands, game/SwimStep.cpp hands
// the velocity to the player's move. Covered by swim_stroke_test.
//
// The numbers are the rival mod's playtested values, not tried here: first
// guesses until the headset (docs/guesses-overview.md).

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/RoomFollowLogic.h"

namespace obvr::vr {

struct SwimSettings {
	bool enabled = true;     // [Hands] SwimStrokes
	float strength = 1.0f;   // [Hands] SwimStrength, 0..kSwimStrengthMost (the menu: 0.5..3)
};

// The hands slower than this (m/s against the head) are the tracking's wander.
inline constexpr float kSwimLeastHandSpeed = 0.15f;
// The swimmer's speed with the hands at 1 m/s, in the game's swim speeds, and the most any stroke gives.
inline constexpr float kSwimPullAtOneMetre = 3.0f;
inline constexpr float kSwimPullMost = 4.0f;
// The most strength counted, the settings menu's top: a hand-edited INI past it is held there.
inline constexpr float kSwimStrengthMost = 3.0f;
// The swim speed when the game's cannot be read, game units a second.
inline constexpr float kSwimSpeedFallback = 60.0f;
// The glide: e-fold time when slow, how much sooner when fast (the rate grows
// by one for every this many swim speeds), and the speed (units a second)
// under which it stops.
inline constexpr float kSwimGlideSeconds = 6.0f;
inline constexpr float kSwimGlideFastSpeeds = 2.0f;
inline constexpr float kSwimGlideStop = 2.0f;
// A pull turns the glide to its way: the sideways part fades in this long.
inline constexpr float kSwimTurnSeconds = 0.25f;
// A push back is clear when the held hands' mean goes at least this share as
// fast as they move apart or together.
inline constexpr float kSwimPushBackShare = 0.5f;
// The head faster than this (m/s) is a tracking jump or a recenter, no motion.
inline constexpr float kSwimHeadJumpSpeed = 4.0f;
// A glide this slow (units a second) has no way: a pull never counts as against it.
inline constexpr float kSwimGlideNoWay = 4.0f;

// x to the power 0.375: three square roots, cubed (no pow in the plugin's
// runtime). Within 3 % of the rival's 0.4 over 0.1..4.
inline float SwimPowerStep(float x) {
	if (!(x > 0.0f)) {
		return 0.0f;
	}
	const float r = math::Sqrt(math::Sqrt(math::Sqrt(x)));
	return r * r * r;
}

inline float SwimSpeedOf(float gameSwimSpeed) {
	return gameSwimSpeed == gameSwimSpeed && gameSwimSpeed > 1.0f ? gameSwimSpeed : kSwimSpeedFallback;
}

// The strength counted: at most kSwimStrengthMost; none, negative or not a number: 0 (no stroke).
inline float SwimStrengthOf(float strength) {
	if (!(strength > 0.0f)) {
		return 0.0f;
	}
	return strength > kSwimStrengthMost ? kSwimStrengthMost : strength;
}

// The swimmer's speed a stroke gives, game units a second: the hands' speed
// against the head (m/s) raised to 0.375, 3 swim speeds at 1 m/s, at most 4,
// times the strength. None under kSwimLeastHandSpeed.
inline float StrokeSpeed(float handMetresPerSecond, float swimSpeed, float strength) {
	const float s = SwimStrengthOf(strength);
	if (!(handMetresPerSecond >= kSwimLeastHandSpeed) || !(s > 0.0f)) {
		return 0.0f;
	}
	float k = kSwimPullAtOneMetre * SwimPowerStep(handMetresPerSecond);
	k = k > kSwimPullMost ? kSwimPullMost : k;
	return k * SwimSpeedOf(swimSpeed) * s;
}

// A hand's velocity against the head, both in tracking space (m/s): walking
// about in the room moves both alike and swims nothing. A head faster than
// kSwimHeadJumpSpeed jumped (a recenter), the hand's own then stands. Not a
// number: no motion.
inline NiPoint3 SwimRelativeVelocity(const NiPoint3& hand, const NiPoint3& head) {
	NiPoint3 h = head;
	if (!(h.LengthSquared() <= kSwimHeadJumpSpeed * kSwimHeadJumpSpeed)) {
		h = NiPoint3{0.0f, 0.0f, 0.0f};
	}
	const NiPoint3 v = hand - h;
	if (!(v.x == v.x) || !(v.y == v.y) || !(v.z == v.z)) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	return v;
}

// The rate (per second) at which a glide of `speed` falls: 1 / kSwimGlideSeconds,
// the faster the quicker.
inline float SwimGlideRate(float speed, float swimSpeed) {
	const float fast = speed / (kSwimGlideFastSpeeds * SwimSpeedOf(swimSpeed));
	return (1.0f + (fast > 0.0f ? fast : 0.0f)) / kSwimGlideSeconds;
}

struct SwimHand {
	bool tracked = false;
	bool grip = false;   // the fingers closed: the hold the holsters read
	bool busy = false;   // holds or reaches for something (SwimHandBusy)
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};  // against the head, world axes, m/s
};

struct SwimInput {
	bool swimming = false;  // the gate: the setting, in the world, the game's swimming state
	float dt = 0.0f;
	float swimSpeed = 0.0f;  // the game's swim speed, units a second (0: unread)
	SwimHand hand[2];        // the right role's, the left role's
};

struct SwimState {
	NiPoint3 glide{0.0f, 0.0f, 0.0f};  // the swimmer's velocity, world units a second
	NiPoint3 reach{0.0f, 0.0f, 0.0f};  // the held hands' way since the catch, metres
	bool caught[2] = {false, false};
};

struct SwimVerdict {
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};  // the swimmer's, world units a second
	bool holding = false;                 // a hand holds the water
	bool pulling = false;                 // a stroke counted this frame
	bool caughtNow[2] = {false, false};   // a hand's catch began this frame: the pulse
	float effort = 0.0f;                  // 0..1, the stroke against the full pull
};

// Whether a hand is busy (SwimHand::busy): what is in it or where its grip is.
struct SwimBusyInput {
	bool weaponDrawn = false;      // a weapon, staff or spell is out
	bool twoHanderOrBow = false;   // the drawn weapon wants the other hand too
	bool torchHeld = false;        // a torch in the other hand
	bool grabHolds = false;        // the grab holds something ...
	bool grabWithLeft = false;     // ... with the left role's hand
	bool claimed[2] = {false, false};  // a hand's grip is the holster's (a reach)
};

inline bool SwimHandBusy(const SwimBusyInput& in, bool leftRole) {
	const int side = leftRole ? 1 : 0;
	if (in.claimed[side] || (in.grabHolds && in.grabWithLeft == leftRole)) {
		return true;
	}
	if (!leftRole) {
		return in.weaponDrawn;
	}
	return in.torchHeld || (in.weaponDrawn && in.twoHanderOrBow);
}

namespace swim_detail {

inline float Dot(const NiPoint3& a, const NiPoint3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float Length(const NiPoint3& a) { return math::Sqrt(a.LengthSquared()); }

// The glide eased and, under kSwimGlideStop, stopped.
inline NiPoint3 EaseGlide(const NiPoint3& glide, float dt, float swimSpeed) {
	const float speed = Length(glide);
	if (!(speed >= kSwimGlideStop)) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	return glide * math::Exp(-SwimGlideRate(speed, swimSpeed) * dt);
}

// What the pull `speed` along `way` (unit) does to the glide: the part along
// the way raised to the pull and the sideways part turned, never slower; a
// pull against the glide counts only as a clear push back. `used` says
// whether the pull counted.
inline NiPoint3 TakePull(const NiPoint3& glide, const NiPoint3& way, float speed, bool clearPush, float dt,
                         bool& used) {
	const float along = Dot(glide, way);
	const bool against = along < 0.0f && Length(glide) >= kSwimGlideNoWay;
	used = true;
	if (against) {
		used = clearPush;
		return clearPush ? way * speed : glide;
	}
	if (!(speed > along)) {
		return glide;
	}
	const NiPoint3 sideways = glide - way * along;
	const float keep = math::Exp(-dt / kSwimTurnSeconds);
	return way * speed + sideways * keep;
}

inline bool IsNumber(const NiPoint3& v) { return v.x == v.x && v.y == v.y && v.z == v.z; }

}  // namespace swim_detail

// One frame of swimming by the arms. Outside the game's swimming nothing:
// the glide, the reach and the catches are gone.
inline SwimVerdict StepSwim(SwimState& s, const SwimSettings& settings, const SwimInput& in) {
	SwimVerdict v;
	if (!settings.enabled || !in.swimming) {
		s = SwimState{};
		return v;
	}
	const float dt = in.dt > 0.0f && in.dt < 1.0f ? in.dt : 0.0f;
	bool caught[2];
	NiPoint3 velocity[2];
	int held = 0;
	NiPoint3 mean{0.0f, 0.0f, 0.0f};
	for (int i = 0; i < 2; ++i) {
		const SwimHand& h = in.hand[i];
		velocity[i] = swim_detail::IsNumber(h.velocity) ? h.velocity : NiPoint3{0.0f, 0.0f, 0.0f};
		caught[i] = h.tracked && h.grip && !h.busy;
		v.caughtNow[i] = caught[i] && !s.caught[i];
		if (caught[i]) {
			++held;
			mean = mean + velocity[i];
		}
	}
	if (caught[0] != s.caught[0] || caught[1] != s.caught[1]) {
		s.reach = NiPoint3{0.0f, 0.0f, 0.0f};  // a new stroke: the way is measured from here
	}
	s.caught[0] = caught[0];
	s.caught[1] = caught[1];
	v.holding = held > 0;
	s.glide = swim_detail::EaseGlide(s.glide, dt, in.swimSpeed);
	if (held > 0) {
		mean = mean * (1.0f / static_cast<float>(held));
		const float handSpeed = swim_detail::Length(mean);
		s.reach = s.reach + mean * dt;
		// Counted only while it goes on away from where it started.
		const bool onward = swim_detail::Dot(mean, s.reach) >= 0.0f;
		const float speed = StrokeSpeed(handSpeed, in.swimSpeed, settings.strength);
		if (onward && speed > 0.0f) {
			const NiPoint3 way = mean * (-1.0f / handSpeed);
			const float sweep = held == 2 ? swim_detail::Length(velocity[0] - velocity[1]) * 0.5f : 0.0f;
			const bool clear = handSpeed >= kSwimPushBackShare * sweep;
			s.glide = swim_detail::TakePull(s.glide, way, speed, clear, dt, v.pulling);
			const float full = kSwimPullAtOneMetre * SwimSpeedOf(in.swimSpeed) * SwimStrengthOf(settings.strength);
			v.effort = v.pulling && full > 0.0f ? (speed < full ? speed / full : 1.0f) : 0.0f;
		}
	}
	v.velocity = s.glide;
	return v;
}

// Whether the arms swim this frame: the setting on, Full VR running with its
// hands, the player in the world with no menu up, not riding, no fly camera,
// the view known, and the game's own state swimming.
struct SwimGate {
	bool fullVr = false;
	bool inWorld = false;
	bool menuUp = false;
	bool riding = false;
	bool flyCam = false;
	bool cameraKnown = false;
	bool gameSwimming = false;
};

inline bool SwimAllowed(const SwimSettings& s, const SwimGate& g) {
	return s.enabled && g.fullVr && g.inWorld && !g.menuUp && !g.riding && !g.flyCam && g.cameraKnown &&
	       g.gameSwimming;
}

// A closed hand that held the water is no fist raised to fight (vr::StepFist
// takes a closed hand with no weapon drawn for one after a quarter second): the
// ready click is dropped while a hand holds the water and this long after.
inline constexpr float kSwimReadyHoldOffSeconds = 0.5f;

inline bool SwimDropsReadyClick(float secondsSinceHolding) {
	return secondsSinceHolding < kSwimReadyHoldOffSeconds;
}

// The glide shows the game a swimmer only above this speed, game units a second.
inline constexpr float kSwimNudgeShowsAbove = 6.0f;

// The direction the game is shown (a stick's, x right and y forward, unit) for
// the swimmer's world velocity: its horizontal part in the player's frame
// (heading rotZ), straight ahead when it is vertical, slow or not a number.
inline void SwimNudgeDirection(const NiPoint3& world, float rotZ, float& x, float& y) {
	x = 0.0f;
	y = 1.0f;
	const float flat = world.x * world.x + world.y * world.y;
	if (!(flat > 1.0f) || !(rotZ == rotZ)) {
		return;
	}
	const NiPoint3 local = WorldStepToActor(world.x, world.y, rotZ);
	const float len = math::Sqrt(local.x * local.x + local.y * local.y);
	if (len > 1e-6f) {
		x = local.x / len;
		y = local.y / len;
	}
}

// The step the game takes, the stick's and the arms' together: the sum, cut to
// the longer of the two, so the arms and the stick never go faster than the
// faster of them alone. `engine` is the game's own step (the stick's, or the
// tiny push that shows the game a swimmer), `arms` the strokes'.
inline NiPoint3 CombineSwimStep(const NiPoint3& engine, const NiPoint3& arms) {
	const float a = swim_detail::Length(arms);
	if (!(a > 1e-6f)) {
		return engine;
	}
	const float e = swim_detail::Length(engine);
	if (!(e > 1e-6f)) {
		return arms;
	}
	const NiPoint3 sum = engine + arms;
	const float l = swim_detail::Length(sum);
	const float most = e > a ? e : a;
	return l > most ? sum * (most / l) : sum;
}

}  // namespace obvr::vr
