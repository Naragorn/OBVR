#pragma once

// The worn-gear check's judgements, as plain values ([Debug] WearCheck,
// game/WearCheck.h): how many hands are drawn a side and with how many
// fingers, and whether every shape of a worn item was drawn.
//
// Why it exists: a tester's "the Deceiver's clothing remains invisible"
// (beta 8, 2026-11-06) and the doubled hand of 2026-10-18 ("nur 10 Finger" in
// the Finery was the fix: one hand a side) were judged from pictures until
// now. These rules let a harness run say red or green on its own.
//
// A hand is a drawn shape skinned to that side's finger bones - the body's
// hand mesh, a glove, a gauntlet or a garment's own hand part alike. One
// hand mesh may carry both sides (it counts once for each). A finger counts
// when one of its bones owns vertices of the shape and stands at a scale
// (an arm collapsed to nothing draws no fingers, however it is drawn).

#include <vector>

#include "core/Types.h"

namespace obvr::game {

inline constexpr UInt8 kWearRight = 0;
inline constexpr UInt8 kWearLeft = 1;
inline constexpr UInt8 kAllFingers = 0x1F;  // thumb (Finger0) to little finger (Finger4)

// A finger bone's side and finger: "Bip01 R Finger0" (the right thumb),
// "Bip01 L Finger21" (the left middle finger's second joint). False for
// every other name, and for a finger past the fifth.
inline bool FingerOfBone(const char* name, UInt8& side, UInt8& finger) {
	if (name == nullptr) {
		return false;
	}
	const char* const prefix = "Bip01 ";
	const char* p = name;
	for (const char* q = prefix; *q != '\0'; ++q, ++p) {
		if (*p != *q) {
			return false;
		}
	}
	if (*p != 'R' && *p != 'L') {
		return false;
	}
	const UInt8 s = *p == 'R' ? kWearRight : kWearLeft;
	++p;
	const char* const middle = " Finger";
	for (const char* q = middle; *q != '\0'; ++q, ++p) {
		if (*p != *q) {
			return false;
		}
	}
	if (*p < '0' || *p > '4') {
		return false;
	}
	side = s;
	finger = static_cast<UInt8>(*p - '0');
	return true;
}

inline UInt32 FingerCount(UInt8 mask) {
	UInt32 n = 0;
	for (UInt8 bit = 0; bit < 5; ++bit) {
		n += (mask >> bit) & 1u;
	}
	return n;
}

// One drawn shape: the fingers it draws on each side (a mask of
// kAllFingers' bits; 0 where it draws no finger of that side).
struct HandShapeFingers {
	UInt8 fingers[2] = {0, 0};
};

struct HandSideTally {
	UInt32 shapes = 0;  // drawn shapes with at least one finger of this side
	UInt8 fewest = kAllFingers;  // the smallest finger set among them (every finger when none)
};

struct HandsTally {
	HandSideTally side[2];
};

inline HandsTally TallyHands(const std::vector<HandShapeFingers>& drawn) {
	HandsTally t;
	for (const HandShapeFingers& shape : drawn) {
		for (UInt8 s = 0; s < 2; ++s) {
			if (shape.fingers[s] == 0) {
				continue;
			}
			HandSideTally& side = t.side[s];
			if (side.shapes == 0 || FingerCount(shape.fingers[s]) < FingerCount(side.fewest)) {
				side.fewest = shape.fingers[s];
			}
			++side.shapes;
		}
	}
	return t;
}

enum class HandVerdict : UInt8 {
	Ok,              // one hand, five fingers
	NoneDrawn,       // no hand of this side drawn (out of view, or missing)
	Doubled,         // more than one hand drawn on this side
	MissingFingers,  // one hand, fewer than five fingers drawn
};

inline HandVerdict JudgeHand(const HandSideTally& t) {
	if (t.shapes == 0) {
		return HandVerdict::NoneDrawn;
	}
	if (t.shapes > 1) {
		return HandVerdict::Doubled;
	}
	return t.fewest == kAllFingers ? HandVerdict::Ok : HandVerdict::MissingFingers;
}

// Wrong whichever way the hands are held: a hand doubled or short of fingers.
// A hand not drawn at all is not wrong by itself (it can be out of view); a
// scenario that holds both hands up expects "hands ok".
inline bool HandWrong(HandVerdict v) { return v == HandVerdict::Doubled || v == HandVerdict::MissingFingers; }

inline const char* HandVerdictWords(HandVerdict v) {
	switch (v) {
	case HandVerdict::Ok: return "ok";
	case HandVerdict::NoneDrawn: return "none drawn";
	case HandVerdict::Doubled: return "doubled";
	case HandVerdict::MissingFingers: return "short of fingers";
	}
	return "?";
}

// A worn item's shape as one tree holds it this frame: the body's
// third-person tree, the first-person tree or the menu's paperdoll.
struct WornShapeSeen {
	UInt32 wornId = 0;
	const char* name = "";
	bool shown = false;  // neither it nor a node above it hidden
	bool drawn = false;  // the renderer drew it in the counted passes
};

// One item over the trees given: its shapes by name (the same garment's
// "Arms" in the first-person tree and in the body is one shape), each shown
// if any tree shows it and drawn if any tree drew it.
struct WornItemTally {
	UInt32 wornId = 0;
	UInt32 shapes = 0;
	UInt32 shown = 0;
	UInt32 drawn = 0;
};

inline bool SameShapeName(const char* a, const char* b) {
	if (a == nullptr || b == nullptr) {
		return a == b;
	}
	for (; *a != '\0' && *a == *b; ++a, ++b) {
	}
	return *a == *b;
}

inline std::vector<WornItemTally> TallyWorn(const std::vector<WornShapeSeen>& seen) {
	struct Named {
		UInt32 wornId;
		const char* name;
		bool shown;
		bool drawn;
	};
	std::vector<Named> named;
	for (const WornShapeSeen& s : seen) {
		if (s.wornId == 0) {
			continue;
		}
		bool merged = false;
		for (Named& n : named) {
			if (n.wornId == s.wornId && SameShapeName(n.name, s.name)) {
				n.shown = n.shown || s.shown;
				n.drawn = n.drawn || s.drawn;
				merged = true;
				break;
			}
		}
		if (!merged) {
			named.push_back(Named{s.wornId, s.name, s.shown, s.drawn});
		}
	}
	std::vector<WornItemTally> items;
	for (const Named& n : named) {
		WornItemTally* item = nullptr;
		for (WornItemTally& i : items) {
			if (i.wornId == n.wornId) {
				item = &i;
				break;
			}
		}
		if (item == nullptr) {
			items.push_back(WornItemTally{n.wornId, 0, 0, 0});
			item = &items.back();
		}
		++item->shapes;
		item->shown += n.shown ? 1u : 0u;
		item->drawn += n.drawn ? 1u : 0u;
	}
	return items;
}

inline bool EveryShapeDrawn(const WornItemTally& t) { return t.shapes > 0 && t.drawn == t.shapes; }

// The paperdoll in the menu's view: its top and bottom as tangents of their
// angle over the view's axis (the menu's camera stands at the origin looking
// along +y, z up), against the view's half-angle tangent.
struct FigureInView {
	bool top = false;
	bool bottom = false;
};

inline FigureInView JudgeFigureInView(float topZ, float topY, float bottomZ, float bottomY, float tanHalfY) {
	FigureInView v;
	if (!(tanHalfY > 0.0f) || !(topY > 0.0f) || !(bottomY > 0.0f)) {
		return v;
	}
	v.top = topZ / topY <= tanHalfY && topZ / topY >= -tanHalfY;
	v.bottom = bottomZ / bottomY >= -tanHalfY && bottomZ / bottomY <= tanHalfY;
	return v;
}

}  // namespace obvr::game
