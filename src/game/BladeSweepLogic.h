#pragma once

// The strike's blade swept between two frames (docs/weapon-collision-spec.md
// 26): at a low frame rate the blade's tip moves farther in one frame than a
// torso is wide (measured on the Windows run of 2026-11-08: 42.7 units a
// frame on average at half rate, over 28 in 35 % of its swing frames), so a
// blade tested only where it is now is ahead of a body one frame and past it
// the next, and nobody is struck. The strike's touch therefore also tests the
// blade at poses between last frame's and this frame's - each end moved on a
// straight line, the chord of the swing's arc - in steps no longer than a
// thin limb is wide. The hit's rules (speed, power, refusals, the ledger)
// stay the strike's own; only where the blade was is widened.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/BladeContactLogic.h"

namespace obvr::game {

// A sub-pose at most this far from the next along the tip's way: under a
// forearm's width with the touch's margin (3.5 + 4 units each side).
inline constexpr float kBladeSweepStepUnits = 6.0f;
// At most this many poses a frame: a jump past 32 x 6 units takes longer
// steps rather than more work.
inline constexpr UInt32 kBladeSweepMostSteps = 32;
// A tip that moved farther than this in one frame was carried, not swung
// (a teleport, a load's first frame, a recenter): no sweep. A swing at
// 30 m/s at 20 fps moves 105 units.
inline constexpr float kBladeSweepMostJumpUnits = 300.0f;

// The blade one strike slot saw last: the weapon hand, the other hand's fist,
// the arrow in the hand.
struct BladeSweepMemory {
	bool valid = false;
	UInt32 frame = 0;   // the strikes' frame stamp it was seen on
	UInt32 world = 0;   // the world counter then (game/RefFingerprint.h)
	NiPoint3 base{0.0f, 0.0f, 0.0f};
	NiPoint3 tip{0.0f, 0.0f, 0.0f};
};

// Whether this frame's blade is swept from the one remembered, and if not why.
// A swing's first frame sweeps too: the blade is remembered every frame the
// hand could strike, swinging or not (a swing is only seen once it is long
// enough, at a low rate after it went through someone - the harness run of
// 2026-11-09 11:00 struck nothing that way), and its way since the frame
// before is where it was.
enum class SweepFrom {
	Swept,      // the frame before, the same world, a swing's step
	SameFrame,  // tested already this frame: nothing between
	FirstFrame, // nothing remembered
	Gap,        // the memory is older than the frame before: the strike paused
	NewWorld,   // a load or a cell change since
	Carried,    // the tip jumped farther than a swing moves it
	NotANumber, // a pose that is no number
};

inline const char* SweepFromName(SweepFrom s) {
	switch (s) {
	case SweepFrom::Swept: return "swept";
	case SweepFrom::SameFrame: return "same frame";
	case SweepFrom::FirstFrame: return "first frame";
	case SweepFrom::Gap: return "a gap";
	case SweepFrom::NewWorld: return "a new world";
	case SweepFrom::Carried: return "carried";
	case SweepFrom::NotANumber: return "no number";
	}
	return "?";
}

inline float TipJumpUnits(const BladeSweepMemory& m, const NiPoint3& tip) {
	return math::Sqrt((tip - m.tip).LengthSquared());
}

inline SweepFrom SweepStart(const BladeSweepMemory& m, UInt32 frame, UInt32 world, const NiPoint3& base,
                            const NiPoint3& tip) {
	const float sum = base.x + base.y + base.z + tip.x + tip.y + tip.z;
	if (!(sum == sum) || sum - sum != 0.0f) {
		return SweepFrom::NotANumber;
	}
	if (!m.valid) {
		return SweepFrom::FirstFrame;
	}
	if (m.world != world) {
		return SweepFrom::NewWorld;
	}
	if (m.frame == frame) {
		return SweepFrom::SameFrame;
	}
	if (m.frame + 1u != frame) {
		return SweepFrom::Gap;
	}
	if (!(TipJumpUnits(m, tip) <= kBladeSweepMostJumpUnits)) {
		return SweepFrom::Carried;
	}
	return SweepFrom::Swept;
}

// How many steps the way from last frame's blade to this one's takes: the
// farther-moving end's way over the step, rounded up, at least 1 (no pose
// between), at most kBladeSweepMostSteps.
inline UInt32 SweepSteps(const BladeSweepMemory& m, const NiPoint3& base, const NiPoint3& tip) {
	const float baseWay = math::Sqrt((base - m.base).LengthSquared());
	const float tipWay = TipJumpUnits(m, tip);
	const float way = baseWay > tipWay ? baseWay : tipWay;
	if (!(way > kBladeSweepStepUnits)) {
		return 1;
	}
	const float steps = way / kBladeSweepStepUnits;
	if (!(steps < static_cast<float>(kBladeSweepMostSteps))) {
		return kBladeSweepMostSteps;
	}
	const UInt32 whole = static_cast<UInt32>(steps);
	return static_cast<float>(whole) < steps ? whole + 1u : whole;
}

inline void RememberBlade(BladeSweepMemory& m, UInt32 frame, UInt32 world, const NiPoint3& base, const NiPoint3& tip) {
	m.valid = true;
	m.frame = frame;
	m.world = world;
	m.base = base;
	m.tip = tip;
}

// Whether the blade touched `actor` at one of the poses strictly between
// `fromBase`-`fromTip` (last frame's, tested then) and `toBase`-`toTip`
// (this frame's, tested by the caller) in `steps` steps: like
// CapsulesTouched 1 touched (the pose it touched at in atBase/atTip), 0
// missed, -1 the actor has no capsule in the list.
inline int SweptCapsulesTouched(const BladeBodies& bodies, UInt32 actor, const NiPoint3& fromBase,
                                const NiPoint3& fromTip, const NiPoint3& toBase, const NiPoint3& toTip, UInt32 steps,
                                float marginUnits, NiPoint3& atBase, NiPoint3& atTip) {
	int found = -1;
	for (UInt32 k = 1; k < steps; ++k) {
		const float t = static_cast<float>(k) / static_cast<float>(steps);
		const NiPoint3 b = fromBase + (toBase - fromBase) * t;
		const NiPoint3 p = fromTip + (toTip - fromTip) * t;
		const int touched = CapsulesTouched(bodies, actor, b, p, marginUnits);
		if (touched == 1) {
			atBase = b;
			atTip = p;
			return 1;
		}
		if (touched == 0) {
			found = 0;
		}
	}
	if (steps > 1) {
		return found;
	}
	// No pose between: whether the actor is in the list at all.
	return CapsulesTouched(bodies, actor, toBase, toTip, marginUnits) < 0 ? -1 : 0;
}

// Where the strike's walk over the actors lost them, a frame or a swing:
// list entries seen, of those Characters or Creatures, alive, with a readable
// bound, and the first other table; the bodies measured and the nearest of
// them to the blade (its distance, the distance it needed, its centre).
struct StrikeWalkTally {
	UInt32 entries = 0;
	UInt32 actors = 0;
	UInt32 alive = 0;
	UInt32 bounded = 0;
	UInt32 otherVtable = 0;
	UInt32 bodies = 0;
	float nearest = -1.0f;
	float needed = 0.0f;
	NiPoint3 centre{0.0f, 0.0f, 0.0f};
};

// A body measured `distance` from the blade (a NaN: not measured), needing
// `needed`, its centre at `centre`.
inline void NoteWalkBody(StrikeWalkTally& t, float distance, float needed, const NiPoint3& centre) {
	if (!(distance == distance)) {
		return;
	}
	++t.bodies;
	if (t.nearest < 0.0f || distance < t.nearest) {
		t.nearest = distance;
		t.needed = needed;
		t.centre = centre;
	}
}

// A frame's walk added to the swing's.
inline void AddWalkTally(StrikeWalkTally& into, const StrikeWalkTally& frame) {
	into.entries += frame.entries;
	into.actors += frame.actors;
	into.alive += frame.alive;
	into.bounded += frame.bounded;
	into.bodies += frame.bodies;
	if (into.otherVtable == 0) {
		into.otherVtable = frame.otherVtable;
	}
	if (frame.nearest >= 0.0f && (into.nearest < 0.0f || frame.nearest < into.nearest)) {
		into.nearest = frame.nearest;
		into.needed = frame.needed;
		into.centre = frame.centre;
	}
}

// One swing's summary for the log (spec 26, C): written once the swing is
// over, for every swing, so a headset run says whether the frames' jumps
// were the misses.
struct SwingSweepSummary {
	UInt32 serial = 0;
	UInt32 hand = 0;
	UInt32 frames = 0;       // frames the strike was tested on
	UInt32 sweptFrames = 0;  // of them swept from the frame before
	float mostJump = 0.0f;   // the tip's largest step, units
	UInt32 timedFrames = 0;  // frames with a plausible clock
	float seconds = 0.0f;    // their time summed
	float longestFrame = 0.0f;
	UInt32 touchedNow = 0;   // bodies touched where the blade was at a frame
	UInt32 touchedSwept = 0; // bodies touched only between two frames
	UInt32 struck = 0;       // of all of them struck (the ledger and the rules let through)
	StrikeWalkTally walk;    // the walks over the actors, the swing's frames together
	SInt32 weaponType = 0;   // the last frame's weapon type and reach
	float reach = 0.0f;
};

inline void StartSummary(SwingSweepSummary& s, UInt32 serial, UInt32 hand) {
	s = SwingSweepSummary{};
	s.serial = serial;
	s.hand = hand;
}

inline void NoteSummaryFrame(SwingSweepSummary& s, SweepFrom from, float jumpUnits, float frameSeconds) {
	++s.frames;
	if (from == SweepFrom::Swept) {
		++s.sweptFrames;
		if (jumpUnits > s.mostJump) {
			s.mostJump = jumpUnits;
		}
	}
	if (frameSeconds > 0.0f && frameSeconds < 1.0f) {
		++s.timedFrames;
		s.seconds += frameSeconds;
		if (frameSeconds > s.longestFrame) {
			s.longestFrame = frameSeconds;
		}
	}
}

// The swing's mean frame rate; 0 without a timed frame.
inline float SummaryFramesPerSecond(const SwingSweepSummary& s) {
	return s.seconds > 0.0f ? static_cast<float>(s.timedFrames) / s.seconds : 0.0f;
}

}  // namespace obvr::game
