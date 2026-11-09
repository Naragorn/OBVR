// Checks placing the four holster places by hand (vr/HolsterPlace.h).

#include <cmath>
#include <cstdio>
#include <limits>

#include "vr/HolsterPlace.h"

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

bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }
bool Near(const NiPoint3& a, const NiPoint3& b) { return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z); }

const HolsterSettings kSettings{};

HolsterPlaceInput Hands() {
	HolsterPlaceInput in;
	in.rightValid = true;
	in.leftValid = true;
	in.right = NiPoint3{0.3f, 0.3f, -0.2f};  // ahead, at none
	in.left = NiPoint3{-0.3f, 0.3f, -0.2f};
	in.reset = ResetPlaces(1.0f);
	return in;
}

HolsterPlaceVerdict Started(HolsterPlaceState& s, HolsterPlaceInput& in, const HolsterSettings& settings = kSettings) {
	in.command = HolsterPlaceCommand::Start;
	const HolsterPlaceVerdict v = StepHolsterPlace(s, in, settings);
	in.command = HolsterPlaceCommand::None;
	return v;
}

void TestPlacesOf() {
	std::printf("the circles from the settings\n");
	const HolsterPlaces p = PlacesOf(kSettings);
	Check(Near(p.at[0], NiPoint3{-0.18f, 0.02f, -0.62f}) && Near(p.at[1], NiPoint3{0.18f, 0.02f, -0.62f}),
	      "the hips: the one-hander's place, mirrored");
	Check(Near(p.at[2], NiPoint3{-0.18f, -0.12f, -0.15f}) && Near(p.at[3], NiPoint3{0.18f, -0.12f, -0.15f}),
	      "the back: the bow's left, the two-hander's right");
	Check(Near(p.radius[0], 0.22f) && Near(p.radius[1], 0.22f) && Near(p.radius[2], 0.18f) && Near(p.radius[3], 0.18f),
	      "each circle as large as its zone");
	Check(SlotOfPlace(0) == HolsterSlot::HipLeft && SlotOfPlace(3) == HolsterSlot::BackRight,
	      "the circles in the slots' order");
}

void TestClamp() {
	std::printf("a circle stays on its side and in reach\n");
	Check(Near(ClampHolsterPlace(0, NiPoint3{-0.3f, 0.0f, -0.5f}), NiPoint3{-0.3f, 0.0f, -0.5f}), "inside: as it is");
	Check(Near(ClampHolsterPlace(0, NiPoint3{0.3f, 0.0f, -0.5f}).x, -kHolsterPlaceLeastSide),
	      "the left hip pulled across the middle: held at the middle's edge on the left");
	Check(Near(ClampHolsterPlace(3, NiPoint3{-0.3f, 0.0f, -0.5f}).x, kHolsterPlaceLeastSide),
	      "the right back pulled across: held on the right");
	Check(Near(ClampHolsterPlace(1, NiPoint3{0.01f, 0.0f, -0.5f}).x, kHolsterPlaceLeastSide), "too near the middle: out");
	Check(Near(ClampHolsterPlace(2, NiPoint3{-2.0f, 0.0f, -0.5f}).x, -kHolsterPlaceMostSide), "too far out: in");
	const NiPoint3 far = ClampHolsterPlace(1, NiPoint3{0.2f, 5.0f, -5.0f});
	Check(Near(far.y, kStowPlaceMaxForward) && Near(far.z, kStowPlaceMinUp), "forward and down: StowPlace's bounds");
}

void TestApply() {
	std::printf("the circles back into the settings\n");
	HolsterSettings s{};
	HolsterPlaces p = PlacesOf(s);
	MovePlace(p, 1, NiPoint3{0.25f, 0.05f, -0.7f});
	Check(Near(p.at[0], NiPoint3{-0.25f, 0.05f, -0.7f}), "a hip dragged takes the other along, mirrored");
	MovePlace(p, 3, NiPoint3{0.3f, -0.2f, -0.1f});
	Check(Near(p.at[2], NiPoint3{-0.18f, -0.12f, -0.15f}), "the back's right alone: its left stays");
	SizePlace(p, 0, 0.3f);
	Check(Near(p.radius[1], 0.3f) && Near(p.radius[2], 0.18f), "a hip sized sizes both, the back not");
	SizePlace(p, 2, 9.0f);
	Check(Near(p.radius[2], kStowMaxRadius) && Near(p.radius[3], 0.18f), "the back's left sized alone, clamped");
	ApplyPlaces(s, p);
	Check(Near(s.oneHandZone, NiPoint3{-0.25f, 0.05f, -0.7f}) && Near(s.oneHandRadius, 0.3f),
	      "the hips into HolsterOneHand*, its left sign kept");
	Check(Near(s.twoHandZone, NiPoint3{0.3f, -0.2f, -0.1f}) && Near(s.bowRadius, kStowMaxRadius),
	      "the back into HolsterTwoHand* and HolsterBow*");
	Check(Near(PlacesOf(s).at[1], p.at[1]), "read back: the places as placed");

	// A one-hander kept on the right hip (its X positive): the side stays.
	HolsterSettings right{};
	right.oneHandZone.x = 0.18f;
	right.bowZone.x = 0.18f;  // the bow homed right
	HolsterPlaces q = PlacesOf(right);
	MovePlace(q, 0, NiPoint3{-0.22f, 0.0f, -0.6f});
	MovePlace(q, 2, NiPoint3{-0.25f, -0.1f, -0.2f});
	ApplyPlaces(right, q);
	Check(Near(right.oneHandZone.x, 0.22f) && Near(right.bowZone.x, 0.25f), "a home on the right stays on the right");
	Check(DefaultSlotFor(SlotItem::OneHand, right, false) == HolsterSlot::HipRight &&
	          DefaultSlotFor(SlotItem::Bow, right, false) == HolsterSlot::BackRight,
	      "the weapons' homes unchanged by moving circles");
	// Left-handed: the places are the body's, the homes mirror as before.
	HolsterSettings lefty{};
	HolsterPlaces r = PlacesOf(lefty);
	MovePlace(r, 1, NiPoint3{0.2f, 0.0f, -0.55f});
	ApplyPlaces(lefty, r);
	Check(DefaultSlotFor(SlotItem::OneHand, lefty, true) == HolsterSlot::HipRight &&
	          DefaultSlotFor(SlotItem::OneHand, lefty, false) == HolsterSlot::HipLeft,
	      "left-handed: the one-hander's home still mirrors");
}

void TestReset() {
	std::printf("reset: the T-pose's places\n");
	Check(Near(PlaceScaleFor(false, 1.8f), 1.0f), "no floor: the defaults");
	Check(Near(PlaceScaleFor(true, 1.0f), 1.0f) && Near(PlaceScaleFor(true, 3.0f), 1.0f), "eyes out of range: the defaults");
	Check(Near(PlaceScaleFor(true, std::numeric_limits<float>::quiet_NaN()), 1.0f), "eyes no number: the defaults");
	Check(Near(PlaceScaleFor(true, 1.8f), 1.125f), "1.8 m eyes: scaled 1.125");
	Check(Near(PlaceScaleFor(true, 2.05f), kPlaceScaleMost), "tall: held at the most");
	const HolsterPlaces p = ResetPlaces(1.125f);
	Check(Near(p.at[0].z, -0.62f * 1.125f) && Near(p.at[3].x, 0.18f * 1.125f) && Near(p.radius[0], 0.22f),
	      "the defaults scaled, the radii not");
}

void TestStep() {
	std::printf("dragging\n");
	HolsterPlaceState s;
	HolsterPlaceInput in = Hands();
	Check(!StepHolsterPlace(s, in, kSettings).active, "not started: nothing");
	in.rightGrip = true;  // held as it starts: not a press
	in.right = NiPoint3{0.18f, 0.02f, -0.62f};
	HolsterPlaceVerdict v = Started(s, in);
	Check(v.active && !v.grabbed, "started with a grip held: no grab");
	Check(v.lit[1] && !v.lit[0], "a free hand at a circle lights it");
	in.rightGrip = false;
	StepHolsterPlace(s, in, kSettings);
	in.rightGrip = true;
	v = StepHolsterPlace(s, in, kSettings);
	Check(v.grabbed && v.circle == 1 && v.lit[1], "a grip closed on the right hip takes it");
	in.right = NiPoint3{0.25f, 0.06f, -0.7f};
	in.resize = 1.0f;
	in.dtSeconds = 0.5f;
	v = StepHolsterPlace(s, in, kSettings);
	Check(Near(v.places.at[1], NiPoint3{0.25f, 0.06f, -0.7f}) && Near(v.places.at[0], NiPoint3{-0.25f, 0.06f, -0.7f}),
	      "it follows the hand, the other hip mirrored");
	Check(Near(v.places.radius[1], 0.22f + 0.06f) && Near(v.places.radius[0], 0.28f), "the stick grows both hips");
	in.resize = 0.0f;
	in.rightGrip = false;
	v = StepHolsterPlace(s, in, kSettings);
	Check(v.dropped && v.circle == 1 && v.active, "the grip opened: let go, still placing");

	// The left hand takes the left back; the right's lost hand drops nothing.
	in.left = NiPoint3{-0.18f, -0.12f, -0.15f};
	in.leftGrip = true;
	v = StepHolsterPlace(s, in, kSettings);
	Check(v.grabbed && v.circle == 2, "the other hand takes the left back");
	in.left = NiPoint3{-0.3f, -0.2f, -0.1f};
	v = StepHolsterPlace(s, in, kSettings);
	Check(Near(v.places.at[2], NiPoint3{-0.3f, -0.2f, -0.1f}) && Near(v.places.at[3], NiPoint3{0.18f, -0.12f, -0.15f}),
	      "the left back alone");
	in.leftValid = false;
	v = StepHolsterPlace(s, in, kSettings);
	Check(v.dropped && v.circle == 2, "the dragging hand lost: let go");
	in.leftValid = true;
	in.leftGrip = false;
	StepHolsterPlace(s, in, kSettings);

	// Both grips close at once on circles: the right hand's first.
	in.right = NiPoint3{0.18f, -0.12f, -0.15f};
	in.left = NiPoint3{-0.3f, -0.2f, -0.1f};
	in.rightGrip = true;
	in.leftGrip = true;
	v = StepHolsterPlace(s, in, kSettings);
	Check(v.grabbed && v.circle == 3, "both at once: the right hand's circle");
	in.rightGrip = false;
	in.leftGrip = false;
	StepHolsterPlace(s, in, kSettings);
	// A grip closed at no circle takes nothing.
	in.right = NiPoint3{0.5f, 0.6f, 0.2f};
	in.rightGrip = true;
	v = StepHolsterPlace(s, in, kSettings);
	Check(!v.grabbed && v.active, "a grip at no circle: nothing taken");
	in.rightGrip = false;
	StepHolsterPlace(s, in, kSettings);
	in.rightValid = false;
	in.rightGrip = true;
	v = StepHolsterPlace(s, in, kSettings);
	Check(!v.grabbed, "a grip of a hand not tracked: nothing");
	in.rightValid = true;
	in.rightGrip = false;

	// Reset: the T-pose's places, still placing.
	in.command = HolsterPlaceCommand::Reset;
	in.reset = ResetPlaces(1.125f);
	v = StepHolsterPlace(s, in, kSettings);
	in.command = HolsterPlaceCommand::None;
	Check(v.reset && v.active && Near(v.places.at[0].z, -0.62f * 1.125f), "reset: the scaled defaults, circles up");

	// Keep saves what is placed.
	in.command = HolsterPlaceCommand::Keep;
	v = StepHolsterPlace(s, in, kSettings);
	Check(v.save && !v.active && Near(v.places.at[0].z, -0.62f * 1.125f), "keep: saved as placed, the circles gone");
	Check(!s.active, "and the placing over");
}

void TestCancel() {
	std::printf("cancel and stray commands\n");
	HolsterPlaceState s;
	HolsterPlaceInput in = Hands();
	in.command = HolsterPlaceCommand::Keep;
	Check(!StepHolsterPlace(s, in, kSettings).save, "keep without placing: nothing saved");
	in.command = HolsterPlaceCommand::Cancel;
	Check(!StepHolsterPlace(s, in, kSettings).restore, "cancel without placing: nothing");
	in.command = HolsterPlaceCommand::Reset;
	Check(!StepHolsterPlace(s, in, kSettings).reset, "reset without placing: nothing");
	Started(s, in);
	in.right = NiPoint3{-0.18f, 0.02f, -0.62f};
	in.rightGrip = true;
	StepHolsterPlace(s, in, kSettings);
	in.right = NiPoint3{-0.3f, 0.1f, -0.5f};
	StepHolsterPlace(s, in, kSettings);
	in.command = HolsterPlaceCommand::Cancel;
	const HolsterPlaceVerdict v = StepHolsterPlace(s, in, kSettings);
	Check(v.restore && Near(v.places.at[0], NiPoint3{-0.18f, 0.02f, -0.62f}), "cancel: back as they were");
	Check(CircleAt(PlacesOf(kSettings), NiPoint3{0.0f, 2.0f, 0.0f}) == -1, "far from all: no circle");
	Check(CircleAt(PlacesOf(kSettings), NiPoint3{-0.18f, -0.05f, -0.4f}) == 0,
	      "between a hip and the back: the nearer");
}

}  // namespace

int main() {
	TestPlacesOf();
	TestClamp();
	TestApply();
	TestReset();
	TestStep();
	TestCancel();
	if (g_failures != 0) {
		std::printf("%d FAILED\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
