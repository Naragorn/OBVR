#pragma once

// What the blade's contact gives the hand and the log, apart from where it
// is drawn (game/BladeContactLogic.h): the pulse of a touch, the scrape
// while it slides, the knock's sound, and the watch on a person's hold.
// Pure; blade_contact_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"
#include "game/SegmentGeometry.h"

namespace obvr::game {

// The touch felt in the hand: a pulse by the blade's speed into the
// surface, from a light knock to the full strength at 3 m/s.
inline constexpr float kBladeTouchFullUnitsPerSecond = 210.0f;
inline constexpr float kBladeTouchMinAmplitude = 0.15f;

inline float BladeTouchAmplitude(float speedInUnitsPerSecond) {
	if (!(speedInUnitsPerSecond > 0.0f)) {
		return kBladeTouchMinAmplitude;
	}
	const float a = speedInUnitsPerSecond / kBladeTouchFullUnitsPerSecond;
	return a < kBladeTouchMinAmplitude ? kBladeTouchMinAmplitude : (a > 1.0f ? 1.0f : a);
}

// The scrape felt while a held shape slides along what holds it: a pulse
// every kScrapeEveryFrames while it slides along their surface faster than
// kScrapeMinUnitsPerSecond, stronger the faster up to full at
// kScrapeFullUnitsPerSecond; held still, or moved only into them or back
// along their normal, nothing (Nadi, 2026-10-27: the buzz while the blade is
// held against someone "stört mich"; Blade & Sorcery's rubbing haptics
// "end the instant the contact or movement stop", r/BladeAndSorcery 2019).
// The threshold, 0.15 m/s, is a first guess, not measured.
inline constexpr float kScrapeMinUnitsPerSecond = 10.5f;
inline constexpr float kScrapeFullUnitsPerSecond = 105.0f;
inline constexpr UInt32 kScrapeEveryFrames = 8;

struct ScrapeState {
	bool have = false;  // a point the frame before, held
	NiPoint3 last{0.0f, 0.0f, 0.0f};
	UInt32 frames = 0;  // sliding frames since the last pulse
};

// One frame: `point` the drawn shape's point where it is held, `normal`
// the surface's. Answers the pulse's strength this frame, 0 for none.
inline float StepScrape(ScrapeState& s, bool held, const NiPoint3& point, const NiPoint3& normal, float dtSeconds) {
	if (!held) {
		s = ScrapeState{};
		return 0.0f;
	}
	const bool had = s.have;
	const NiPoint3 was = s.last;
	s.have = true;
	s.last = point;
	if (!had || !(dtSeconds > 0.0f)) {
		s.frames = 0;
		return 0.0f;
	}
	const NiPoint3 moved = point - was;
	const NiPoint3 along = moved - normal * DotOf3(moved, normal);
	const float speed = math::Sqrt(along.LengthSquared()) / dtSeconds;
	if (!(speed >= kScrapeMinUnitsPerSecond)) {
		s.frames = 0;
		return 0.0f;
	}
	if (++s.frames < kScrapeEveryFrames) {
		return 0.0f;
	}
	s.frames = 0;
	const float a = speed / kScrapeFullUnitsPerSecond;
	return a < kBladeTouchMinAmplitude ? kBladeTouchMinAmplitude : (a > 1.0f ? 1.0f : a);
}

// A knock loud enough for a sound: 1.5 m/s into the surface, and not again
// within a quarter of a second.
inline constexpr float kBladeSoundUnitsPerSecond = 105.0f;
inline constexpr float kBladeSoundGapSeconds = 0.25f;

inline bool BladeKnockSounds(float speedInUnitsPerSecond, float secondsSinceLast) {
	return speedInUnitsPerSecond >= kBladeSoundUnitsPerSecond && secondsSinceLast >= kBladeSoundGapSeconds;
}

// The watch on a person's hold, for the log: one line when it ends - how
// many frames, the most the grip and the blade's turn stood off the hand's,
// and the frames the drawn tip stood still while the hand's moved (the
// sticking Nadi felt, 2026-10-26). A tip that moved less than this stood
// still.
inline constexpr float kBladeStillUnits = 0.6f;

struct BladeHoldSummary {
	bool ended = false;
	UInt32 actor = 0;
	UInt32 frames = 0;
	UInt32 stillFrames = 0;
	float mostGripUnits = 0.0f;
	float mostTurnRadians = 0.0f;
};

struct BladeHoldWatch {
	BladeHoldSummary now;  // the hold going on (ended false)
	bool haveLast = false;
	NiPoint3 lastDrawnTip{0.0f, 0.0f, 0.0f};
	NiPoint3 lastWantedTip{0.0f, 0.0f, 0.0f};
};

// One frame: who holds the blade (0 nobody; a person, not a stopper), how
// far it is drawn from the hand, and the drawn and the hand's tip. Answers
// the hold that ended this frame, if one did.
inline BladeHoldSummary StepBladeHoldWatch(BladeHoldWatch& w, UInt32 heldBy, float gapUnits, float gapRadians,
                                           const NiPoint3& drawnTip, const NiPoint3& wantedTip) {
	BladeHoldSummary ended;
	if (w.now.frames > 0 && heldBy != w.now.actor) {
		ended = w.now;
		ended.ended = true;
		w.now = BladeHoldSummary{};
	}
	if (heldBy != 0) {
		w.now.actor = heldBy;
		++w.now.frames;
		w.now.mostGripUnits = gapUnits > w.now.mostGripUnits ? gapUnits : w.now.mostGripUnits;
		w.now.mostTurnRadians = gapRadians > w.now.mostTurnRadians ? gapRadians : w.now.mostTurnRadians;
		if (w.haveLast) {
			const NiPoint3 drawnMoved = drawnTip - w.lastDrawnTip;
			const NiPoint3 wantedMoved = wantedTip - w.lastWantedTip;
			const float still = kBladeStillUnits * kBladeStillUnits;
			if (drawnMoved.LengthSquared() < still && !(wantedMoved.LengthSquared() < still)) {
				++w.now.stillFrames;
			}
		}
	}
	w.haveLast = true;
	w.lastDrawnTip = drawnTip;
	w.lastWantedTip = wantedTip;
	return ended;
}

}  // namespace obvr::game
