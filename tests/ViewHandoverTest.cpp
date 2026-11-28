// Checks the view handover's measurement (camera/ViewHandover.h): the first
// eye as built is the sum of its terms, a world vector in the head's axes, a
// distance in half-IPDs, and when a frame is logged - at a change between the
// camera hook and the menu stand-in and the frames after it, against the last
// frame of the other kind, within a session's budget; frames nobody built, the
// session's first frame and frames of one kind are not.

#include <cstdio>

#include "camera/ViewHandover.h"

namespace {

using obvr::NiMatrix33;
using obvr::NiPoint3;
using namespace obvr::camera;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b) { return a == a && a >= b - 0.001f && a <= b + 0.001f; }

bool NearPoint(const NiPoint3& a, const NiPoint3& b) { return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z); }

ViewBuild Frame(ViewFrameKind kind, float x = 0.0f) {
	ViewBuild b;
	b.kind = kind;
	b.base = NiPoint3{x, 0.0f, 0.0f};
	return b;
}

void BuiltIsTheSum() {
	std::printf("The first eye as built: every term added, the heights on z\n");
	ViewBuild b;
	b.base = NiPoint3{100.0f, 200.0f, 100.0f};
	b.aimArc = NiPoint3{1.0f, 0.0f, 0.0f};
	b.aimTilt = NiPoint3{0.0f, 1.0f, 0.0f};
	b.engineAnim = NiPoint3{0.0f, 0.0f, 1.0f};
	b.head = NiPoint3{2.0f, 2.0f, 2.0f};
	b.firstEyeStep = NiPoint3{-2.2f, 0.0f, 0.0f};
	b.height = 3.0f;
	b.realLift = 4.0f;
	Check(NearPoint(BuiltFirstEye(b), NiPoint3{100.8f, 203.0f, 110.0f}), "base + arc + tilt + anim + head + step, z + heights");
}

void HeadAxes() {
	std::printf("A world vector in the head's axes\n");
	Check(NearPoint(InHeadAxes(NiMatrix33::Identity(), NiPoint3{1.0f, 2.0f, 3.0f}), NiPoint3{1.0f, 2.0f, 3.0f}),
	      "the identity: as it is");
	// The head turned 90 degrees to the left: its right is the world's +y, its
	// forward the world's -x (the columns of the view).
	NiMatrix33 left{};
	left.data[0][0] = 0.0f;
	left.data[1][0] = 1.0f;
	left.data[0][1] = -1.0f;
	left.data[1][1] = 0.0f;
	left.data[2][2] = 1.0f;
	Check(NearPoint(InHeadAxes(left, NiPoint3{0.0f, 2.0f, 0.0f}), NiPoint3{2.0f, 0.0f, 0.0f}), "the world's +y is to the right");
	Check(NearPoint(InHeadAxes(left, NiPoint3{-3.0f, 0.0f, 1.0f}), NiPoint3{0.0f, 3.0f, 1.0f}), "the world's -x is forward, z up");
}

void HalfIpd() {
	std::printf("A distance in half-IPDs: half the shift's length\n");
	const NiPoint3 shift{4.4f, 0.0f, 0.0f};
	Check(Near(HalfIpds(NiPoint3{2.2f, 0.0f, 0.0f}, shift), 1.0f), "half the eyes' distance is one");
	Check(Near(HalfIpds(NiPoint3{0.0f, 3.0f, 4.0f}, shift), 5.0f / 2.2f), "a diagonal by its length");
	Check(Near(HalfIpds(NiPoint3{1.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}), 0.0f), "no shift: none");
}

void NothingBuiltIgnored() {
	std::printf("A frame nobody built is ignored\n");
	ViewHandoverState s;
	StepViewHandover(s, Frame(ViewFrameKind::Hook));
	const ViewHandoverVerdict v = StepViewHandover(s, Frame(ViewFrameKind::None));
	Check(!v.log, "not logged");
	Check(s.lastKind == ViewFrameKind::Hook, "the last kind stays the hook's");
	Check(StepViewHandover(s, Frame(ViewFrameKind::Hook)).log == false, "the hook again is no change");
}

void FirstFrameNoChange() {
	std::printf("The session's first frame is no change, nor frames of one kind\n");
	ViewHandoverState s;
	Check(!StepViewHandover(s, Frame(ViewFrameKind::StandIn)).log, "the first frame (a menu at the start)");
	Check(!StepViewHandover(s, Frame(ViewFrameKind::StandIn)).log, "the stand-in again");
	Check(s.changesLeft == kViewHandoverChanges, "no change spent");
}

void ChangeAndFramesAfter() {
	std::printf("A change: its frame and the frames after it, against the other kind's last frame\n");
	ViewHandoverState s;
	StepViewHandover(s, Frame(ViewFrameKind::Hook, 1.0f));
	StepViewHandover(s, Frame(ViewFrameKind::Hook, 2.0f));
	const ViewHandoverVerdict open = StepViewHandover(s, Frame(ViewFrameKind::StandIn, 10.0f));
	Check(open.log && open.frameIndex == 0, "the menu's first frame logged as the change");
	Check(open.other.kind == ViewFrameKind::Hook && Near(open.other.base.x, 2.0f), "against the last world frame");
	for (std::uint32_t i = 1; i <= kViewHandoverFramesAfter; ++i) {
		const ViewHandoverVerdict after = StepViewHandover(s, Frame(ViewFrameKind::StandIn, 10.0f + i));
		Check(after.log && after.frameIndex == i, "a frame after it logged, counted");
		Check(Near(after.other.base.x, 2.0f), "still against the last world frame");
	}
	Check(!StepViewHandover(s, Frame(ViewFrameKind::StandIn)).log, "then no more");
	Check(s.changesLeft == kViewHandoverChanges - 1, "one change spent");
	const ViewHandoverVerdict close = StepViewHandover(s, Frame(ViewFrameKind::Hook, 3.0f));
	Check(close.log && close.frameIndex == 0, "the close: the world's first frame logged");
	Check(close.other.kind == ViewFrameKind::StandIn, "against the last menu frame");
}

void ChangeWhileLogging() {
	std::printf("A change while the frames after the last are still logged starts over\n");
	ViewHandoverState s;
	StepViewHandover(s, Frame(ViewFrameKind::Hook));
	StepViewHandover(s, Frame(ViewFrameKind::StandIn));
	const ViewHandoverVerdict back = StepViewHandover(s, Frame(ViewFrameKind::Hook));
	Check(back.log && back.frameIndex == 0, "the change back is a change's frame");
	Check(s.changesLeft == kViewHandoverChanges - 2, "two changes spent");
}

void BudgetSpent() {
	std::printf("The budget spent: changes are no longer logged\n");
	ViewHandoverState s;
	s.changesLeft = 1;
	StepViewHandover(s, Frame(ViewFrameKind::Hook));
	Check(StepViewHandover(s, Frame(ViewFrameKind::StandIn)).log, "the last change logged");
	for (std::uint32_t i = 0; i < kViewHandoverFramesAfter; ++i) {
		StepViewHandover(s, Frame(ViewFrameKind::StandIn));
	}
	Check(!StepViewHandover(s, Frame(ViewFrameKind::Hook)).log, "the next change is not");
	Check(s.last[static_cast<int>(ViewFrameKind::Hook)].kind == ViewFrameKind::Hook, "the frames still kept");
}

void NoOtherYet() {
	std::printf("A change to a kind whose other was never seen cannot happen; the first other is kept\n");
	ViewHandoverState s;
	StepViewHandover(s, Frame(ViewFrameKind::StandIn, 5.0f));
	const ViewHandoverVerdict v = StepViewHandover(s, Frame(ViewFrameKind::Hook, 6.0f));
	Check(v.log && Near(v.other.base.x, 5.0f), "the stand-in at the start is what the hook is compared with");
}

}  // namespace

int main() {
	BuiltIsTheSum();
	HeadAxes();
	HalfIpd();
	NothingBuiltIgnored();
	FirstFrameNoChange();
	ChangeAndFramesAfter();
	ChangeWhileLogging();
	BudgetSpent();
	NoOtherYet();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
