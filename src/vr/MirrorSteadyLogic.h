#pragma once

// The mirror's picture held steady (docs/mirror-spec.md 13). A hand's tremor
// turns the glass, and a mirror turns its reflection by twice the glass's
// turn; on top, the picture is rendered for last frame's glass and drawn on
// this frame's. So the reflection is rendered through a STEADIED glass - the
// held glass's pose through a One Euro filter (Casiez, Roussel, Vogel, "1€
// Filter", CHI 2012: a low-pass whose cut-off rises with the speed, heavy on a
// still hand's tremor, light on a deliberate move) - with a margin around it,
// and the picture is laid onto the glass as it IS when drawn, through the
// camera it was rendered with (vr/MirrorGlassMap.h). The reflected world
// stands still while the glass's frame shakes in the hand.
//
// The filter works in the game's world, on the glass's own centre and turn:
// not relative to the head, because the head's own moves are real and the
// reflection has to follow them (its parallax); and not on the controller's
// origin, because a turn of the wrist moves the glass's centre too. The room
// would serve as well as the world while standing; it is not handed to the
// renderer, and walking and turning are fast moves the filter follows within
// its raised cut-off, a turn or a teleport a jump taken at once.
//
// Pure; covered by mirror_steady_test. The engine side is
// render/MirrorGlassPass.cpp.

#include <cstdint>

#include "core/MathFns.h"
#include "game/NiMath.h"
#include "vr/MirrorGlassLogic.h"

namespace obvr::vr {

// ----------------------------------------------------------------- settings

// [Tablet] MirrorSteady: 0 off (stage 2's picture, stuck to the glass), 1 to
// 100 stronger. The constants below are [G]: physiological hand tremor sits at
// 8 to 12 Hz with a few tenths of a degree (the owner's log of 2026-11-28
// carries no glass pose over frames to measure it from).
inline constexpr std::int32_t kMirrorSteadyMin = 0;
inline constexpr std::int32_t kMirrorSteadyMax = 100;
inline constexpr std::int32_t kMirrorSteadyDefault = 60;

// The still hand's cut-off, from 8 Hz at the weakest to 0.4 Hz at the
// strongest, evenly on a log scale: 1.33 Hz at the default, so a 10 Hz tremor
// comes through about a seventh (a first-order low-pass, fc / f) [D].
inline constexpr float kMirrorSteadyCutoffWeakHz = 8.0f;
inline constexpr float kMirrorSteadyCutoffStrongLog = -2.9957323f;  // ln(0.4 / 8)
// How fast the cut-off rises with the speed: the steady lag of a constant move
// is speed / (2 pi (min + beta speed)), so at most 1 / (2 pi beta) - about one
// degree of turn and half a unit (7 mm) of move behind, however fast [D].
inline constexpr float kMirrorTurnBetaHzPerDegree = 0.15f;
inline constexpr float kMirrorMoveBetaHzPerUnit = 0.3f;
// The speeds' own low-pass, the filter paper's 1 Hz: a tremor's speed swings
// both ways and averages out, a deliberate move's does not.
inline constexpr float kMirrorSpeedCutoffHz = 1.0f;
// The margin around the steadied glass the picture covers, a share of each
// side added on every edge: 4 percent at the weakest, 10 at the strongest
// (7.6 at the default) - over the lags above for a 21-unit glass [D].
inline constexpr float kMirrorMarginWeak = 0.04f;
inline constexpr float kMirrorMarginStrongExtra = 0.06f;

struct MirrorSteadyTuning {
	bool on = false;
	float stillCutoffHz = kMirrorSteadyCutoffWeakHz;
	float turnBeta = kMirrorTurnBetaHzPerDegree;
	float moveBeta = kMirrorMoveBetaHzPerUnit;
	float speedCutoffHz = kMirrorSpeedCutoffHz;
	float margin = 0.0f;
};

inline std::int32_t ClampMirrorSteady(std::int32_t setting) {
	return setting < kMirrorSteadyMin ? kMirrorSteadyMin : (setting > kMirrorSteadyMax ? kMirrorSteadyMax : setting);
}

inline MirrorSteadyTuning MirrorSteadyTuningFor(std::int32_t setting) {
	MirrorSteadyTuning t;
	const std::int32_t s = ClampMirrorSteady(setting);
	if (s == 0) {
		return t;
	}
	const float share = static_cast<float>(s) / static_cast<float>(kMirrorSteadyMax);
	t.on = true;
	t.stillCutoffHz = kMirrorSteadyCutoffWeakHz * math::Exp(share * kMirrorSteadyCutoffStrongLog);
	t.margin = kMirrorMarginWeak + kMirrorMarginStrongExtra * share;
	return t;
}

// ------------------------------------------------------------ the glass frame

// The glass's pose: its centre, and its turn as the columns right, up, normal.
struct GlassFrame {
	NiPoint3 centre{0.0f, 0.0f, 0.0f};
	NiMatrix33 rot = NiMatrix33::Identity();
};

inline GlassFrame GlassFrameOf(const GlassPlane& g) {
	GlassFrame f;
	f.centre = g.centre;
	f.rot = MirrorColumns(g.right, g.up, g.normal);
	return f;
}

inline NiMatrix33 MirrorTransposed(const NiMatrix33& m) {
	NiMatrix33 t{};
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			t.data[r][c] = m.data[c][r];
		}
	}
	return t;
}

// The turn carrying rotation `from` into `to` (to = turn * from): its unit axis
// and its angle in radians, 0 to pi; the axis is +z for no turn.
inline float TurnBetween(const NiMatrix33& from, const NiMatrix33& to, NiPoint3& axis) {
	const NiMatrix33 r = to * MirrorTransposed(from);
	const NiPoint3 v{r.data[2][1] - r.data[1][2], r.data[0][2] - r.data[2][0], r.data[1][0] - r.data[0][1]};
	const float twiceSine = math::Sqrt(v.LengthSquared());
	const float twiceCosine = r.data[0][0] + r.data[1][1] + r.data[2][2] - 1.0f;
	axis = NiPoint3{0.0f, 0.0f, 1.0f};
	if (!(twiceSine > 1.0e-7f)) {
		return twiceCosine < 0.0f ? math::kPi : 0.0f;
	}
	axis = v * (1.0f / twiceSine);
	return math::Atan2(twiceSine, twiceCosine);
}

// The angle between two glass turns, in degrees: what the cost line reports.
inline float GlassTurnDegrees(const NiMatrix33& a, const NiMatrix33& b) {
	NiPoint3 axis{0.0f, 0.0f, 0.0f};
	return TurnBetween(a, b, axis) * math::kRadiansToDegrees;
}

// A rotation by `radians` about the unit `axis` (Rodrigues).
inline NiMatrix33 MirrorAxisTurn(const NiPoint3& axis, float radians) {
	const float c = math::Cos(radians);
	const float s = math::Sin(radians);
	const float k = 1.0f - c;
	NiMatrix33 m{};
	m.data[0][0] = c + axis.x * axis.x * k;
	m.data[0][1] = axis.x * axis.y * k - axis.z * s;
	m.data[0][2] = axis.x * axis.z * k + axis.y * s;
	m.data[1][0] = axis.y * axis.x * k + axis.z * s;
	m.data[1][1] = c + axis.y * axis.y * k;
	m.data[1][2] = axis.y * axis.z * k - axis.x * s;
	m.data[2][0] = axis.z * axis.x * k - axis.y * s;
	m.data[2][1] = axis.z * axis.y * k + axis.x * s;
	m.data[2][2] = c + axis.z * axis.z * k;
	return m;
}

// Kept a rotation over many small turns: right and up made unit and square
// again, the normal their cross product.
inline NiMatrix33 MirrorOrthonormal(const NiMatrix33& m) {
	NiPoint3 right{m.data[0][0], m.data[1][0], m.data[2][0]};
	NiPoint3 up{m.data[0][1], m.data[1][1], m.data[2][1]};
	right = right * (1.0f / math::Sqrt(right.LengthSquared()));
	up = up - right * MirrorDot(up, right);
	up = up * (1.0f / math::Sqrt(up.LengthSquared()));
	return MirrorColumns(right, up, MirrorCross(right, up));
}

// --------------------------------------------------------------- the filter

// A jump the filter takes at once: further in a frame than a hand moves (a
// teleport, a load, a turn of the view by the stick), or a glass that goes
// away and comes back. And a frame time that is no time.
inline constexpr float kMirrorJumpUnits = 20.0f;     // 29 cm at 70 units a metre
inline constexpr float kMirrorJumpDegrees = 30.0f;
inline constexpr float kMirrorLongestStepSeconds = 0.25f;

struct MirrorSteadyState {
	bool have = false;
	GlassFrame frame;
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};  // units a second, low-passed
	NiPoint3 turnRate{0.0f, 0.0f, 0.0f};  // degrees a second about each axis, low-passed
};

enum class MirrorSteadyStep : std::uint8_t { Off, Started, Jumped, NoTime, Filtered };

// The share of the way a low-pass at `cutoffHz` moves in `dt` seconds.
inline float MirrorLowPassShare(float cutoffHz, float dt) {
	const float tau = 1.0f / (2.0f * math::kPi * cutoffHz);
	return 1.0f / (1.0f + tau / dt);
}

inline void RestartMirrorSteady(MirrorSteadyState& s, const GlassFrame& now) {
	s.have = true;
	s.frame = now;
	s.velocity = NiPoint3{0.0f, 0.0f, 0.0f};
	s.turnRate = NiPoint3{0.0f, 0.0f, 0.0f};
}

// One frame: the glass's pose now in, the steadied pose out. Off hands the
// glass through and forgets; the first frame, a jump and a frame time of no
// number take the glass as it is and start over.
inline MirrorSteadyStep StepMirrorSteady(MirrorSteadyState& s, const GlassFrame& now, float dt,
                                         const MirrorSteadyTuning& t, GlassFrame& out) {
	out = now;
	if (!t.on) {
		s = MirrorSteadyState{};
		return MirrorSteadyStep::Off;
	}
	if (!s.have) {
		RestartMirrorSteady(s, now);
		return MirrorSteadyStep::Started;
	}
	if (!(dt > 0.0f) || !(dt <= kMirrorLongestStepSeconds)) {
		RestartMirrorSteady(s, now);
		return MirrorSteadyStep::NoTime;
	}
	const NiPoint3 moved = now.centre - s.frame.centre;
	NiPoint3 axis{0.0f, 0.0f, 1.0f};
	const float turn = TurnBetween(s.frame.rot, now.rot, axis);
	if (!(moved.LengthSquared() <= kMirrorJumpUnits * kMirrorJumpUnits) ||
	    !(turn * math::kRadiansToDegrees <= kMirrorJumpDegrees)) {
		RestartMirrorSteady(s, now);
		return MirrorSteadyStep::Jumped;
	}
	const float speedShare = MirrorLowPassShare(t.speedCutoffHz, dt);
	s.velocity = s.velocity + (moved * (1.0f / dt) - s.velocity) * speedShare;
	const NiPoint3 rate = axis * (turn * math::kRadiansToDegrees / dt);
	s.turnRate = s.turnRate + (rate - s.turnRate) * speedShare;
	const float moveCutoff = t.stillCutoffHz + t.moveBeta * math::Sqrt(s.velocity.LengthSquared());
	const float turnCutoff = t.stillCutoffHz + t.turnBeta * math::Sqrt(s.turnRate.LengthSquared());
	s.frame.centre = s.frame.centre + moved * MirrorLowPassShare(moveCutoff, dt);
	s.frame.rot = MirrorOrthonormal(MirrorAxisTurn(axis, turn * MirrorLowPassShare(turnCutoff, dt)) * s.frame.rot);
	out = s.frame;
	return MirrorSteadyStep::Filtered;
}

// --------------------------------------------------------- the steady glass

// The glass's corners (top left, top right, bottom left, bottom right) moved
// from where they are (`now`, in frame `from`) to the steadied frame `to`,
// each grown away from the centre by (1 + 2 margin) for the picture's margin.
inline void SteadyGlassCorners(const NiPoint3 (&corners)[4], const GlassFrame& from, const GlassFrame& to,
                               float margin, NiPoint3 (&out)[4]) {
	const NiMatrix33 back = MirrorTransposed(from.rot);
	const float grow = 1.0f + 2.0f * margin;
	for (int i = 0; i < 4; ++i) {
		NiPoint3 local = back * (corners[i] - from.centre);
		local.x *= grow;
		local.y *= grow;
		out[i] = to.centre + to.rot * local;
	}
}

}  // namespace obvr::vr
