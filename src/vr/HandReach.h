#pragma once

// The tablet lies on the hand that carries it (docs/inventory-tablet-spec.md
// section 21; Nadi, 2026-11-25: "das tablet auf die tragende hand schieben
// sodass die hand nicht durchklippt, die hand soll also hinter dem tablet
// sein"): how far the hand as it is drawn reaches out of the page's flat
// (vr::HandReach, vr/TabletLogic.h), read from the hand's own vertices - the
// bare hand, a glove or a gauntlet, open or closed round the controller - so
// the page can stand before all of it; the reading kept and eased; and when
// the model is read again, since that costs a tenth of a millisecond
// (game::ReadHandVerticesNow). Pure; covered by tablet_on_hand_test.

#include "vr/TabletLogic.h"

namespace obvr::vr {

// Farther than this out of the palm's flat no part of a hand is, whatever
// it wears: the fingertips are some 13 cm from the palm's middle, a large
// race's in a spiked gauntlet a few more. A vertex beyond it is a misread -
// some other part's, or skinned by a bone that went - and the reading is not
// taken.
inline constexpr float kHandReachMostMetres = 0.20f;

// One hand's reach from where it is drawn: `points` its vertices, `palm` its
// palm's middle (vr::PalmBetween) and `held` its held-item node's axes, all
// in one frame - the world's as the bones stand - of `unitsPerMetre`. Along
// the face the page shows in a physical left hand, turned by `place`
// (TurnHeldPage), and along the face it shows in a right one's: unturned the
// hand's two sides, turned a quarter both away from the knuckles. The
// palm's middle lies in the flat, so neither is below nought. Not read
// without vertices, a scale or a flat between the axes, or with a vertex no
// hand reaches.
inline HandReach ReachOfHand(const NiPoint3* points, UInt32 count, const NiPoint3& palm, const HeldAxes& held,
                             const HeldPlace& place, float unitsPerMetre) {
	HandReach reach;
	const PageAxes left = TurnHeldPage(PageLikeTheSword(held, true), place, true);
	const PageAxes right = TurnHeldPage(PageLikeTheSword(held, false), place, false);
	if (points == nullptr || count == 0 || !left.valid || !right.valid || !(unitsPerMetre > 0.0f)) {
		return reach;
	}
	float leftMost = 0.0f;
	float rightMost = 0.0f;
	for (UInt32 i = 0; i < count; ++i) {
		const float outLeft = game::DotOf3(points[i] - palm, left.front);
		const float outRight = game::DotOf3(points[i] - palm, right.front);
		leftMost = outLeft > leftMost ? outLeft : leftMost;
		rightMost = outRight > rightMost ? outRight : rightMost;
	}
	const float limit = kHandReachMostMetres * unitsPerMetre;
	if (!(leftMost <= limit) || !(rightMost <= limit)) {
		return reach;
	}
	reach.valid = true;
	reach.beforeLeft = leftMost / unitsPerMetre;
	reach.beforeRight = rightMost / unitsPerMetre;
	return reach;
}

// Where on the page the hand's foremost point lies, for the log: across the
// page and up it from the palm's middle and how far out of the flat, metres,
// for the face a physical left hand's page shows or a right one's. Says what
// the page rests on - a fingertip under it, or the thumb's ball beside it.
struct HandForemost {
	bool valid = false;
	float across = 0.0f;
	float up = 0.0f;
	float out = 0.0f;
};

inline HandForemost ForemostOfHand(const NiPoint3* points, UInt32 count, const NiPoint3& palm, const HeldAxes& held,
                                   const HeldPlace& place, bool holderPhysicalLeft, float unitsPerMetre) {
	HandForemost foremost;
	const PageAxes page = TurnHeldPage(PageLikeTheSword(held, holderPhysicalLeft), place, holderPhysicalLeft);
	if (points == nullptr || count == 0 || !page.valid || !(unitsPerMetre > 0.0f)) {
		return foremost;
	}
	for (UInt32 i = 0; i < count; ++i) {
		const NiPoint3 from = points[i] - palm;
		const float out = game::DotOf3(from, page.front) / unitsPerMetre;
		if (!foremost.valid || out > foremost.out) {
			foremost.valid = true;
			foremost.out = out;
			foremost.across = game::DotOf3(from, page.right) / unitsPerMetre;
			foremost.up = game::DotOf3(from, page.up) / unitsPerMetre;
		}
	}
	return foremost;
}

// The share of the way to a new reading taken each frame, as the palm's and
// the held axes' (kHeldAxesEase): the fingers close and the page moves out
// over a few frames instead of jumping.
inline constexpr float kHandReachEase = 0.3f;

// Each frame, towards the last reading: none without one, the first taken whole.
inline void StepHandReach(HandReach& shown, const HandReach& read) {
	if (!read.valid) {
		shown = HandReach{};
		return;
	}
	if (!shown.valid) {
		shown = read;
		return;
	}
	shown.beforeLeft += (read.beforeLeft - shown.beforeLeft) * kHandReachEase;
	shown.beforeRight += (read.beforeRight - shown.beforeRight) * kHandReachEase;
}

// When the hand's model is read again while the tablet is up. The read is a
// tenth of a millisecond under Linux and a third of one in Nadi's Windows
// game ([R in its log of 2026-11-23: "the hand fit's read - 120 reads, 336
// us a read on the mean, 641 the longest, 2.0 shapes and 67.0 memory probes
// a read"]), so it is made as seldom as the hand allows:
// - with the fingers' curls told (0 open to 1 closed, and the thumb's three
//   joints): when a finger of the holding hand has moved kHandReachCurlStep
//   since the last read, but kHandReachLeastFrames after it at the earliest -
//   fingers moving fast, or a sensor that flickers, cost a read every third
//   frame at most, and the ease spreads each step over as many frames anyway
//   - and otherwise only every kHandReachEveryFramesTold frames, for what
//   the hand wears changing under the tablet (a gauntlet put on from the
//   inventory);
// - with no curls told, the fingers are the animation's and say nothing:
//   every kHandReachEveryFrames frames, as the hands' contact fit reads
//   outside menus.
// The step: [R in the harness, 2026-11-25, tablet-on-hand.txt, the curl
// stepped by 0.05] closing, the fingertips sweep the hand's foremost point
// out by 0.4 cm for each 5 % of curl up to a quarter closed (7.5, 7.9, 8.3,
// 8.6, 9.0, 9.2 cm) and back behind where it began (6.9 cm closed), so a
// reading 4 % of curl old is 0.3 cm short at most - the page's clearance.
inline constexpr UInt32 kHandReachEveryFrames = 15;
inline constexpr UInt32 kHandReachEveryFramesTold = 60;
inline constexpr UInt32 kHandReachLeastFrames = 3;
inline constexpr float kHandReachCurlStep = 0.04f;
inline constexpr UInt32 kHandReachCurls = 8;

struct HandReachClock {
	UInt32 upFrames = 0;  // frames the tablet has been up, to 2
	UInt32 age = 0;       // frames since the last read
	bool read = false;
	float curls[kHandReachCurls] = {};  // the holding hand's fingers at the last read
};

// Each frame: whether the hand's model is to be read now. Never with the
// tablet down, and not on the frame it comes up - the menu's opening frame
// is the game's heaviest, and nothing of the page shows yet. `curls`: the
// holding hand's kHandReachCurls values, or null when none are told.
inline bool StepHandReachClock(HandReachClock& clock, bool tabletUp, const float* curls) {
	if (!tabletUp) {
		clock = HandReachClock{};
		return false;
	}
	if (clock.upFrames < 2 && ++clock.upFrames < 2) {
		return false;
	}
	bool moved = false;
	for (UInt32 i = 0; curls != nullptr && i < kHandReachCurls; ++i) {
		const float d = curls[i] - clock.curls[i];
		moved = moved || d > kHandReachCurlStep || d < -kHandReachCurlStep;
	}
	clock.age += clock.read ? 1u : 0u;
	const UInt32 every = curls != nullptr ? kHandReachEveryFramesTold : kHandReachEveryFrames;
	const bool due = !clock.read || clock.age >= every || (moved && clock.age >= kHandReachLeastFrames);
	if (due) {
		clock.read = true;
		clock.age = 0;
		for (UInt32 i = 0; curls != nullptr && i < kHandReachCurls; ++i) {
			clock.curls[i] = curls[i];
		}
	}
	return due;
}

}  // namespace obvr::vr
