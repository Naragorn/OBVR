// Checks the shield bash's watch (game/ShieldBashWatch.h): an attempt opens
// when the shield moves fast near someone (by the drawn shield's speed or the
// controller's), is folded frame by frame, and ends in one summary - landed,
// never in their column, in it but too slow towards them, cooling down, the
// hand busy - with the frame that came closest.

#include <cstdio>
#include <cstring>

#include "game/ShieldBashWatch.h"

namespace {

using namespace obvr::game;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.0001f) { return a - b < eps && b - a < eps; }

BashAttemptFrame Fast() {
	BashAttemptFrame f;
	f.near = true;
	f.actor = 0x2B0C8210u;
	f.speed = 2.5f;
	f.controllerSpeed = 2.5f;
	f.towards = 2.0f;
	f.across = 50.0f;
	f.columnWide = 36.0f;
	f.columnHigh = 64.0f;
	return f;
}

BashAttemptFrame Still() {
	BashAttemptFrame f = Fast();
	f.speed = 0.2f;
	f.controllerSpeed = 0.2f;
	f.towards = 0.1f;
	return f;
}

void TestOpening() {
	std::printf("When an attempt opens\n");
	BashAttemptState s;
	Check(!StepBashAttempt(s, Still()).ended && !s.open, "a slow shield near someone: no attempt");
	BashAttemptFrame far = Fast();
	far.near = false;
	Check(!StepBashAttempt(s, far).ended && !s.open, "a fast shield with nobody near: no attempt");
	Check(!StepBashAttempt(s, Fast()).ended && s.open, "fast near someone: an attempt opens");
	BashAttemptState lagged;
	BashAttemptFrame f = Fast();
	f.speed = 0.5f;  // the drawn shield still behind
	StepBashAttempt(lagged, f);
	Check(lagged.open, "the controller fast and the drawn shield not yet: it opens too");
}

void TestEnds() {
	std::printf("How an attempt ends\n");
	{
		BashAttemptState s;
		StepBashAttempt(s, Fast());
		StepBashAttempt(s, Fast());
		const BashAttemptSummary r = StepBashAttempt(s, Still());
		Check(r.ended && r.end == BashAttemptEnd::OutsideColumn && r.frames == 2 && !s.open,
		      "never in their column: outside, two frames, closed");
		Check(!StepBashAttempt(s, Still()).ended, "and said once");
	}
	{
		BashAttemptState s;
		BashAttemptFrame nearer = Fast();
		nearer.across = 57.0f;
		nearer.gap = 3.0f;
		StepBashAttempt(s, nearer);
		BashAttemptFrame middleCloser = Fast();
		middleCloser.across = 50.0f;
		middleCloser.gap = 12.0f;
		StepBashAttempt(s, middleCloser);
		const BashAttemptSummary r = StepBashAttempt(s, Still());
		Check(Near(r.best.gap, 3.0f) && Near(r.best.across, 57.0f),
		      "never met: the closest frame by the disc's gap to their column, not by its middle");
	}
	{
		BashAttemptState s;
		BashAttemptFrame in = Fast();
		in.inColumn = true;
		in.across = 12.0f;
		in.towards = 1.2f;
		StepBashAttempt(s, in);
		BashAttemptFrame in2 = in;
		in2.towards = 2.1f;
		in2.across = 20.0f;
		StepBashAttempt(s, in2);
		BashAttemptFrame outside = Fast();
		outside.towards = 2.9f;
		StepBashAttempt(s, outside);
		const BashAttemptSummary r = StepBashAttempt(s, Still());
		Check(r.ended && r.end == BashAttemptEnd::TooSlow, "in the column, never fast enough: too slow");
		Check(Near(r.best.towards, 2.1f) && Near(r.best.across, 20.0f),
		      "the closest frame: in the column at the most speed towards them, not a faster one outside");
	}
	{
		BashAttemptState s;
		BashAttemptFrame first = Fast();
		first.inColumn = true;
		first.approach = 2.9f;
		first.towards = 1.5f;
		first.across = 25.0f;
		StepBashAttempt(s, first);
		BashAttemptFrame later = first;
		later.approach = 2.2f;
		later.towards = 2.2f;
		later.across = 10.0f;
		StepBashAttempt(s, later);
		const BashAttemptSummary r = StepBashAttempt(s, Still());
		Check(Near(r.best.approach, 2.9f) && Near(r.best.towards, 1.5f) && Near(r.best.across, 25.0f),
		      "the closest frame by the gate's measure, the approach - not by the frame's own speed");
	}
	{
		BashAttemptState s;
		BashAttemptFrame in = Fast();
		in.inColumn = true;
		in.cooling = true;
		in.towards = 3.5f;
		StepBashAttempt(s, in);
		Check(StepBashAttempt(s, Still()).end == BashAttemptEnd::CoolingDown, "in the column, fast, cooling: cooling down");
	}
	{
		BashAttemptState s;
		BashAttemptFrame in = Fast();
		in.inColumn = true;
		in.handBusy = true;
		in.towards = 3.5f;
		StepBashAttempt(s, in);
		Check(StepBashAttempt(s, Still()).end == BashAttemptEnd::HandBusy, "the hand on a handle or holding someone: busy");
	}
	{
		BashAttemptState s;
		BashAttemptFrame in = Fast();
		in.inColumn = true;
		StepBashAttempt(s, in);
		BashAttemptFrame far = Fast();
		far.near = false;
		Check(StepBashAttempt(s, far).end == BashAttemptEnd::TooSlow, "they move off: it ends too");
	}
	{
		BashAttemptState s;
		StepBashAttempt(s, Fast());
		BashAttemptFrame hit = Fast();
		hit.inColumn = true;
		hit.towards = 3.4f;
		hit.landed = true;
		const BashAttemptSummary r = StepBashAttempt(s, hit);
		Check(r.ended && r.end == BashAttemptEnd::Landed && r.frames == 2 && !s.open, "it lands: ended at once, landed");
	}
	{
		BashAttemptState s;
		BashAttemptFrame hit = Fast();
		hit.speed = 1.0f;
		hit.controllerSpeed = 1.0f;
		hit.inColumn = true;
		hit.landed = true;
		Check(StepBashAttempt(s, hit).end == BashAttemptEnd::Landed, "a bash below the attempt's speed still says landed");
	}
	{
		BashAttemptState s;
		BashAttemptFrame a = Fast();
		a.speed = 2.0f;
		a.controllerSpeed = 3.1f;
		StepBashAttempt(s, a);
		BashAttemptFrame b = Fast();
		b.speed = 2.6f;
		b.controllerSpeed = 2.2f;
		StepBashAttempt(s, b);
		const BashAttemptSummary r = StepBashAttempt(s, Still());
		Check(Near(r.mostSpeed, 2.6f) && Near(r.mostControllerSpeed, 3.1f), "the most speed of the shield and of the controller");
	}
}

void TestWords() {
	std::printf("The words\n");
	Check(std::strcmp(BashAttemptEndWords(BashAttemptEnd::Landed), "landed") == 0, "landed");
	Check(std::strcmp(BashAttemptEndWords(BashAttemptEnd::OutsideColumn), "the shield never met them") == 0,
	      "outside");
	Check(std::strcmp(BashAttemptEndWords(BashAttemptEnd::TooSlow), "it met them, too slow into them") == 0,
	      "too slow");
	Check(std::strcmp(BashAttemptEndWords(BashAttemptEnd::CoolingDown), "they were cooling down from a shove") == 0,
	      "cooling");
	Check(std::strcmp(BashAttemptEndWords(BashAttemptEnd::HandBusy), "the hand on a handle or holding someone") == 0,
	      "busy");
}

}  // namespace

int main() {
	TestOpening();
	TestEnds();
	TestWords();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all shield bash watch checks passed\n");
	return 0;
}
