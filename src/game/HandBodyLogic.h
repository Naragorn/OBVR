#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/GrabPhysics.h"
#include "game/NiMath.h"

namespace obvr::game {

// The decisions behind the hands' and the weapon's Havok bodies
// (docs/hand-weapon-collision-spec.md, game/HandBodies.h): pure, over plain
// values, covered by hand_body_test.

// ------------------------------------------------------------- the filter
//
// Layer 22: unnamed in the engine's table (0x00B2EB40), nothing refers to
// it, it has a row in the layer matrix (filled with every bit by 0x008A83C0)
// and the pick rows (0xA277F, 0x00535A7C) leave it out. The group is the
// player's, as HIGGS does: the hands then touch neither the player's own
// capsule nor what the player holds (both in that group), and stay out of
// every ray asked as the player's group (the grab ray, 0x0066DAAC). Bit 14
// (0x4000) switches a body's collision off (the rule 0x008A7F70, step 1:
// any layer but 29).
inline constexpr UInt32 kHandBodyLayer = 22;
// Layer 23, the other unnamed one: the bodies that do not push people
// (below). OBVR takes the character controllers' layer out of its row.
inline constexpr UInt32 kHandBodyQuietLayer = 23;
inline constexpr UInt32 kCharControllerLayer = 20;
inline constexpr UInt32 kFilterNoCollision = 0x4000;

inline UInt32 HandBodyFilter(UInt32 group, bool collides, bool pushesActors = true) {
	const UInt32 g = group != 0 ? (group & 0xFFFF) : kPlayerCollisionGroupFallback;
	return (g << 16) | (pushesActors ? kHandBodyLayer : kHandBodyQuietLayer) | (collides ? 0u : kFilterNoCollision);
}

// The layer: the low 6 bits (GrabPhysics.h).
inline UInt32 FilterLayer(UInt32 filter) { return filter & 0x3F; }

// ------------------------------------------------------ pushing the people
//
// A hand's body pushed an opponent's character controller away before the
// swing landed (the tester, 2026-09-29: "npcs kann man nun wegschieben. das
// macht aber h2h combat schwierig da man nicht mehr nah genug ran kommt zum
// hauen"). So in combat no body pushes people, and out of it a hand made
// into a fist does not either: "wegschieben aus im Kampf, und mit geballter
// Faust". The hits themselves are OBVR's own hit test (game/MeleeHits), not
// the bodies, so they still land.
//
// Such a body goes to layer 23 (kHandBodyQuietLayer), the other unnamed
// layer, whose matrix row OBVR keeps without the character controllers'
// layer; the filter change reaches Havok through the wrapper's vtable +0x80
// (0x008B0060: hkWorld::updateCollisionFilterOnEntity, 0x0089B630), as
// the engine's own setters 0x0089F4D0 and 0x0089F520 do.
// Whether a hand is a fist, for its body: the four fingers (the thumb left
// out, as vr::StepFist does) all past closeCurl; one stays a fist until they
// are all below openLimit. No curls: no fist.
inline bool HandBodyFist(bool wasFist, bool curlValid, const float* curl, float closeCurl, float openLimit) {
	if (!curlValid || curl == nullptr) {
		return false;
	}
	float lowest = curl[1];
	float highest = curl[1];
	for (int finger = 2; finger < 5; ++finger) {
		lowest = curl[finger] < lowest ? curl[finger] : lowest;
		highest = curl[finger] > highest ? curl[finger] : highest;
	}
	return wasFist ? !(highest <= openLimit) : lowest >= closeCurl;
}

// Whether a body pushes people: never in combat; a hand not while it is a
// fist; the weapon out of combat always.
inline bool HandBodyPushesActors(bool isHand, bool inCombat, bool fist) {
	if (inCombat) {
		return false;
	}
	return !(isHand && fist);
}

// ------------------------------------------------- measuring (open bugs)
//
// A keyframed body is moved in steps, and Havok tests it against the others
// only where each step leaves it (no continuous collision between a
// keyframed body and clutter is asked for). A capsule of radius r passes an
// object t thick without ever overlapping it once a step carries it
// further than t + 2r: the tip of a swung sword is the fastest part.
inline float PassThroughTravelUnits(float thicknessUnits, float radiusUnits) {
	return thicknessUnits + 2.0f * radiusUnits;
}

// The travel of one physics step, from a frame's travel and the steps the
// frame ran; a frame of no step (or an absurd count) counts as one.
inline float TravelPerStep(float frameTravelUnits, UInt32 steps) {
	return frameTravelUnits / static_cast<float>(steps >= 1 && steps <= 6 ? steps : 1u);
}

// How far an object's Havok box stands out beyond its mesh's box, side by
// side (both world, axis-aligned): the most and the least of the six gaps,
// positive where the Havok shape reaches further than the model.
struct BoxGaps {
	float most = 0.0f;
	float least = 0.0f;
};

inline BoxGaps HavokBeyondMesh(const NiPoint3& meshLow, const NiPoint3& meshHigh, const NiPoint3& havokLow,
                               const NiPoint3& havokHigh) {
	const float gaps[6] = {meshLow.x - havokLow.x,   meshLow.y - havokLow.y,   meshLow.z - havokLow.z,
	                       havokHigh.x - meshHigh.x, havokHigh.y - meshHigh.y, havokHigh.z - meshHigh.z};
	BoxGaps out;
	out.most = gaps[0];
	out.least = gaps[0];
	for (int i = 1; i < 6; ++i) {
		out.most = gaps[i] > out.most ? gaps[i] : out.most;
		out.least = gaps[i] < out.least ? gaps[i] : out.least;
	}
	return out;
}

// Where a point lies from the grip, along the body's axis and off it
// (game units): for the fingers against the capsule's front end.
struct AlongAxis {
	float ahead = 0.0f;
	float aside = 0.0f;
};

inline AlongAxis MeasureAlongAxis(const NiPoint3& grip, const NiPoint3& axis, const NiPoint3& point) {
	const NiPoint3 d = point - grip;
	AlongAxis out;
	out.ahead = d.x * axis.x + d.y * axis.y + d.z * axis.z;
	const NiPoint3 off = d - axis * out.ahead;
	out.aside = math::Sqrt(off.LengthSquared());
	return out;
}

// Whether a body already in the world needs its filter written again.
inline bool HandBodyRefilterNeeded(UInt32 currentFilter, UInt32 wantedFilter) { return currentFilter != wantedFilter; }

// ------------------------------------------------------------- the shapes
//
// Each body is a capsule along its own y axis (the hand's forward, as the
// hand rotation has it - game::HandSegment): bhkCapsuleShape (0x00563BB0)
// takes its two end points and its radius in Havok units. The spec asked
// for a box for the hands; a capsule is the same constructor for hands and
// weapon, and a hand from wrist to fingertips is near enough one.
struct CapsuleSpec {
	NiPoint3 a{0.0f, 0.0f, 0.0f};  // Havok units, in the body's own frame
	NiPoint3 b{0.0f, 0.0f, 0.0f};
	float radius = 0.0f;
};

// The hand: from 4 units behind the grip to 9 ahead (game::HandSegment),
// 2.5 units round (about 3.5 cm - a hand's half width).
inline constexpr float kHandBodyBackUnits = 4.0f;
inline constexpr float kHandBodyAheadUnits = 9.0f;
inline constexpr float kHandBodyRadiusUnits = 2.5f;
// The blade: from the grip to the tip, 1.5 units round (about 2 cm).
inline constexpr float kBladeBodyRadiusUnits = 1.5f;
// A blade shorter than this is no blade (a bound that could not be read).
inline constexpr float kBladeMinUnits = 5.0f;

inline CapsuleSpec HandCapsule(float radiusUnits) {
	CapsuleSpec c;
	c.a = NiPoint3{0.0f, -kHandBodyBackUnits * kHavokPerUnit, 0.0f};
	c.b = NiPoint3{0.0f, kHandBodyAheadUnits * kHavokPerUnit, 0.0f};
	c.radius = radiusUnits * kHavokPerUnit;
	return c;
}

// False for a blade too short to be one: then the weapon hand keeps its hand.
inline bool BladeCapsule(float lengthUnits, float radiusUnits, CapsuleSpec& out) {
	if (!(lengthUnits >= kBladeMinUnits) || !(radiusUnits > 0.0f)) {
		return false;
	}
	out.a = NiPoint3{0.0f, 0.0f, 0.0f};
	out.b = NiPoint3{0.0f, lengthUnits * kHavokPerUnit, 0.0f};
	out.radius = radiusUnits * kHavokPerUnit;
	return true;
}

// A weapon body is made anew only when the blade's length has changed by
// more than this (another weapon): the bound wobbles a little frame to frame.
inline constexpr float kBladeRebuildUnits = 3.0f;

inline bool BladeNeedsRebuild(float builtUnits, float nowUnits) {
	const float d = builtUnits - nowUnits;
	return d > kBladeRebuildUnits || d < -kBladeRebuildUnits;
}

// ------------------------------------------- the shapes from what is drawn
//
// Measured 2026-09-29 (the spec's open bugs): the constant hand capsule
// stood in front of the drawn hand (the wrist 11 units behind the grip, the
// capsule to 11.5 ahead) and the blade's 11 degrees off the drawn blade. So
// both are built from the drawn skeleton: each frame the drawn points are
// taken into the body's own frame (the grip's), and a body whose span has
// moved away from them for a while is made anew.

// A world point in the frame of a body at `pos` turned by `rot` (world from
// body): rot transposed times the offset.
inline NiPoint3 ToBodyFrame(const NiMatrix33& rot, const NiPoint3& pos, const NiPoint3& world) {
	const NiPoint3 d = world - pos;
	return NiPoint3{rot.data[0][0] * d.x + rot.data[1][0] * d.y + rot.data[2][0] * d.z,
	                rot.data[0][1] * d.x + rot.data[1][1] * d.y + rot.data[2][1] * d.z,
	                rot.data[0][2] * d.x + rot.data[1][2] * d.y + rot.data[2][2] * d.z};
}

// A body's span: its capsule's two end points in its own frame, game units.
struct BodySpan {
	bool valid = false;
	NiPoint3 a{0.0f, 0.0f, 0.0f};
	NiPoint3 b{0.0f, 0.0f, 0.0f};
};

// No drawn point of a hand or a blade's start lies further from the grip than
// this; one that does was read before the hand was placed (the first frames:
// 50 units off, measured 2026-09-29), and the span is refused.
inline constexpr float kSpanMaxFromGripUnits = 40.0f;

inline bool NearGrip(const NiPoint3& local) {
	return local.LengthSquared() <= kSpanMaxFromGripUnits * kSpanMaxFromGripUnits;
}

// The hand from the drawn bones: from the wrist to the fingers' ends. The
// ends are taken as far past the middle finger's base as the base is from
// the wrist - a finger about as long as the palm (a proportion of the human
// hand, not measured on the model). Refused when the two bones are closer
// than 1 unit or further than 30 apart (not a hand).
inline BodySpan HandSpanFromBones(const NiPoint3& wristLocal, const NiPoint3& knuckleLocal) {
	BodySpan s;
	const NiPoint3 palm = knuckleLocal - wristLocal;
	const float length = math::Sqrt(palm.LengthSquared());
	if (!(length >= 1.0f && length <= 30.0f) || !NearGrip(wristLocal) || !NearGrip(knuckleLocal)) {
		return s;
	}
	s.valid = true;
	s.a = wristLocal;
	s.b = knuckleLocal + palm;
	return s;
}

// The blade from the drawn weapon node: from its attach point along its own
// y axis (the blade: measured 0.99 with the line to its bound's centre) to
// the bound's far end on that axis. Refused when that is under
// kBladeMinUnits or over 200 units, or the axis is no unit vector.
inline BodySpan BladeSpanFromNode(const NiPoint3& attachLocal, const NiPoint3& axisLocal,
                                  const NiPoint3& boundCentreLocal, float boundRadius) {
	BodySpan s;
	const float axisLength = axisLocal.LengthSquared();
	if (!(axisLength > 0.81f && axisLength < 1.21f) || !(boundRadius > 0.0f) || !NearGrip(attachLocal)) {
		return s;
	}
	const NiPoint3 toCentre = boundCentreLocal - attachLocal;
	const float along = toCentre.x * axisLocal.x + toCentre.y * axisLocal.y + toCentre.z * axisLocal.z + boundRadius;
	if (!(along >= kBladeMinUnits && along <= 200.0f)) {
		return s;
	}
	s.valid = true;
	s.a = attachLocal;
	s.b = attachLocal + axisLocal * along;
	return s;
}

// The capsule for a span, in Havok units.
inline CapsuleSpec CapsuleFromSpan(const BodySpan& span, float radiusUnits) {
	CapsuleSpec c;
	c.a = span.a * kHavokPerUnit;
	c.b = span.b * kHavokPerUnit;
	c.radius = radiusUnits * kHavokPerUnit;
	return c;
}

// Whether the drawn span has left the built one: either end further than
// this from where it was built.
inline constexpr float kSpanRebuildUnits = 2.0f;
// And for how many frames in a row before the body is made anew: the drawn
// bones are a frame behind the controller, so a fast hand reads a moved
// span for a frame or two (90 frames: a second at 90 Hz).
inline constexpr UInt32 kSpanRebuildFrames = 90;

inline bool SpanMoved(const BodySpan& built, const BodySpan& now) {
	if (!built.valid || !now.valid) {
		return false;
	}
	const float limit = kSpanRebuildUnits * kSpanRebuildUnits;
	return (now.a - built.a).LengthSquared() > limit || (now.b - built.b).LengthSquared() > limit;
}

// Counts the frames the span stays moved; true once it has for long enough
// (and starts counting anew).
inline bool StepSpanWatch(UInt32& frames, bool moved) {
	if (!moved) {
		frames = 0;
		return false;
	}
	if (++frames < kSpanRebuildFrames) {
		return false;
	}
	frames = 0;
	return true;
}

// ------------------------------------------------------------- the drive
//
// A keyframed body is moved by its velocities: Havok integrates them over
// each step. So each frame the body gets the velocity that reaches the
// tracked pose in one step (HIGGS's applyHardKeyFrame, gain 1). The step is
// the planner's length (0x00BA790C, 1/60 s), whatever the frame: at 90 fps
// about one frame in three has no step, and a velocity for the frame's time
// would fall short. Too far to reach in a step at a sane speed (a teleport,
// a recenter, the first frame) it is placed there instead.
inline constexpr float kHandBodyDefaultStepSeconds = 1.0f / 60.0f;
// 30 m/s and 12 turns a second: a swung sword's tip, not a teleport.
inline constexpr float kHandBodyMaxHavokPerSecond = 300.0f;
inline constexpr float kHandBodyMaxRadiansPerSecond = 75.0f;
// Farther than this from the tracked pose, game units (about 60 cm): placed.
inline constexpr float kHandBodyPlaceGapUnits = 42.0f;

inline float HandBodyStepSeconds(float plannerStep) {
	return plannerStep > 0.001f && plannerStep <= 0.1f ? plannerStep : kHandBodyDefaultStepSeconds;
}

// The time the physics runs this frame: the planner's step count
// (0x00BA7914) times its length (0x00BA790C). The planner (0x00889810) has
// two ways: fixed steps of fMaxTime, the rest carried (0 steps on some
// frames), or - with iUpdateType set - the frame's whole time split into
// equal steps (at least one). A velocity for one step would carry the body
// n steps far. No steps, or a count past the cap of 3: one step's time.
inline float HandBodyDriveSeconds(UInt32 steps, float plannerStep) {
	const float step = HandBodyStepSeconds(plannerStep);
	return steps >= 1 && steps <= 3 ? step * static_cast<float>(steps) : step;
}

inline bool HandBodyPlaceInstead(float gapUnits) { return !(gapUnits <= kHandBodyPlaceGapUnits); }

// Linear: all of the gap in one step, clamped.
inline NiPoint3 HardKeyframeLinear(const NiPoint3& current, const NiPoint3& target, float stepSeconds) {
	const float dt = HandBodyStepSeconds(stepSeconds);
	NiPoint3 v = (target - current) * (1.0f / dt);
	const float speed = math::Sqrt(v.LengthSquared());
	if (speed > kHandBodyMaxHavokPerSecond) {
		v = v * (kHandBodyMaxHavokPerSecond / speed);
	}
	return v;
}

// Angular: the error rotation target * current^T as axis and angle, the
// short way round, all of it in one step, clamped (world axes).
inline NiPoint3 HardKeyframeAngular(const NiMatrix33& current, const NiMatrix33& target, float stepSeconds) {
	NiMatrix33 error;
	for (UInt32 i = 0; i < 3; ++i) {
		for (UInt32 j = 0; j < 3; ++j) {
			float sum = 0.0f;
			for (UInt32 k = 0; k < 3; ++k) {
				sum += target.data[i][k] * current.data[j][k];
			}
			error.data[i][j] = sum;
		}
	}
	float q[4];
	QuaternionFromRotation(error, q);
	if (q[3] < 0.0f) {
		q[0] = -q[0];
		q[1] = -q[1];
		q[2] = -q[2];
		q[3] = -q[3];
	}
	const float sine = math::Sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
	if (!(sine > 1e-6f)) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	const float angle = 2.0f * math::Atan2(sine, q[3]);
	float rate = angle / HandBodyStepSeconds(stepSeconds);
	if (rate > kHandBodyMaxRadiansPerSecond) {
		rate = kHandBodyMaxRadiansPerSecond;
	}
	const float scale = rate / sine;
	return NiPoint3{q[0] * scale, q[1] * scale, q[2] * scale};
}

// ------------------------------------------------------------- the lifecycle
//
// What happens to one body this frame. The body is only ever in the
// player's world, and only while it is wanted: out of it in menus and while
// loading (so a world torn down by a load never holds it), and moved when
// the player changes worlds (a door).
enum class HandBodyAction : UInt8 {
	None,    // not wanted, not in a world: nothing to do
	Create,  // wanted, not made yet
	Enter,   // made, wanted, in no world or another one: into the player's
	Drive,   // made, wanted, in the player's world: driven to the hand
	Leave,   // made, in a world, not wanted: taken out
	Abandon, // in a world that no longer looks like one: left alone, made anew
};

struct HandBodyState {
	bool wanted = false;        // setting on, in the world, no menu, first person, tracked
	bool made = false;          // the body exists
	UInt32 bodyWorld = 0;       // its hkWorld (body+8), 0 in none
	bool bodyWorldSound = true; // that world still looks like a world
	UInt32 playerWorld = 0;     // the player's hkWorld, 0 for none
};

inline HandBodyAction DecideHandBody(const HandBodyState& s) {
	if (s.made && s.bodyWorld != 0 && !s.bodyWorldSound) {
		return HandBodyAction::Abandon;
	}
	const bool want = s.wanted && s.playerWorld != 0;
	if (!want) {
		return s.made && s.bodyWorld != 0 ? HandBodyAction::Leave : HandBodyAction::None;
	}
	if (!s.made) {
		return HandBodyAction::Create;
	}
	return s.bodyWorld == s.playerWorld ? HandBodyAction::Drive : HandBodyAction::Enter;
}

// ------------------------------------------------------------- the step rate
//
// [Hands] PhysicsRate: 0 leaves the game's fixed 1/60 s step; otherwise the
// runtime copy of fMaxTime:HAVOK (0x00B2E2E8) is set to 1/rate, clamped to
// 30..180 Hz. Not the INI setting: a written setting persists to the user's
// INI (the iSize incident).
inline float PhysicsStepFor(float rateHz) {
	if (!(rateHz > 0.0f)) {
		return 0.0f;
	}
	const float r = rateHz < 30.0f ? 30.0f : (rateHz > 180.0f ? 180.0f : rateHz);
	return 1.0f / r;
}

}  // namespace obvr::game
