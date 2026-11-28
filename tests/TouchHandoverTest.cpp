// Checks the laser yielding to the finger at the tablet (vr/TouchHandover.h;
// docs/inventory-tablet-spec.md 32.4): the laser fading over the last 5 cm
// before the finger takes the pointer, the finger taking it at 5 cm and
// letting it go only past 7 cm (no flicker at the line), the touch mark
// growing as the tip nears, the hand curled or the trigger pulled near the
// page keeping the finger's lead, off the face or far through it giving the
// laser back, every other menu as before; and when the laser's alpha is
// handed to the compositor.

#include <cstdio>

#include "vr/TouchHandover.h"

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

bool Near(float a, float b, float tolerance = 0.001f) {
	const float d = a - b;
	return d < tolerance && d > -tolerance;
}

void TestApproach() {
	std::printf("The finger comes to the page and goes:\n");
	TouchHandoverState s;
	TouchHandover h = StepTouchHandover(s, true, true, true, 0.20f);
	Check(!h.fingerLeads && Near(h.laserAlpha, 1.0f) && !h.mark, "20 cm off: the laser whole, no mark");
	h = StepTouchHandover(s, true, true, true, 0.10f);
	Check(!h.fingerLeads && Near(h.laserAlpha, 1.0f) && h.mark && Near(h.markShare, 0.0f),
	      "10 cm: the laser still whole, the mark comes, at its least");
	h = StepTouchHandover(s, true, true, true, 0.075f);
	Check(!h.fingerLeads && Near(h.laserAlpha, 0.5f) && Near(h.markShare, 0.25f),
	      "7.5 cm: the laser half faded, the mark a quarter grown - the laser still points and clicks");
	h = StepTouchHandover(s, true, true, true, 0.05f);
	Check(h.fingerLeads && h.changed && Near(h.laserAlpha, 0.0f) && Near(h.markShare, 0.5f),
	      "5 cm: the finger takes the pointer, the laser gone");
	h = StepTouchHandover(s, true, true, true, 0.0f);
	Check(h.fingerLeads && !h.changed && Near(h.markShare, 1.0f), "on the glass: the mark whole");
	h = StepTouchHandover(s, true, true, true, -0.03f);
	Check(h.fingerLeads && Near(h.markShare, 1.0f), "3 cm through: still the finger's, the mark whole");
	h = StepTouchHandover(s, true, true, true, 0.065f);
	Check(h.fingerLeads && Near(h.laserAlpha, 0.0f), "back to 6.5 cm: still the finger's - no flicker at 5");
	h = StepTouchHandover(s, true, true, true, 0.051f);
	h = StepTouchHandover(s, true, true, true, 0.069f);
	Check(h.fingerLeads, "wobbling between 5 and 7 cm: the finger's throughout");
	h = StepTouchHandover(s, true, true, true, 0.071f);
	Check(!h.fingerLeads && h.changed && h.laserAlpha > 0.4f, "past 7 cm: the laser back, fading in");
	h = StepTouchHandover(s, true, true, true, 0.06f);
	Check(!h.fingerLeads && Near(h.laserAlpha, 0.2f), "coming in again to 6 cm: the laser's until 5 cm");
}

void TestHandCurled() {
	std::printf("The hand near the page, its finger not out:\n");
	TouchHandoverState s;
	TouchHandover h = StepTouchHandover(s, true, false, true, 0.03f);
	Check(!h.fingerLeads && Near(h.laserAlpha, 1.0f) && !h.mark, "a fist 3 cm off, coming in: the laser's, as before");
	StepTouchHandover(s, true, true, true, 0.03f);
	h = StepTouchHandover(s, true, false, true, 0.03f);
	Check(h.fingerLeads && Near(h.laserAlpha, 0.0f),
	      "the finger leading, then curled or the trigger touched: still the finger's - no jump to the beam");
	h = StepTouchHandover(s, true, false, true, 0.08f);
	Check(!h.fingerLeads && Near(h.laserAlpha, 1.0f), "pulled back curled: the laser whole again");
}

void TestOffThePage() {
	std::printf("Off the page:\n");
	TouchHandoverState s;
	StepTouchHandover(s, true, true, true, 0.02f);
	TouchHandover h = StepTouchHandover(s, true, true, false, 0.02f);
	Check(!h.fingerLeads && h.changed && Near(h.laserAlpha, 1.0f) && !h.mark,
	      "the tip off the face's edge: the laser back, whole");
	StepTouchHandover(s, true, true, true, 0.02f);
	h = StepTouchHandover(s, true, true, true, -0.30f);
	Check(!h.fingerLeads && !h.mark, "30 cm through: a hand behind the page, the laser's");
	h = StepTouchHandover(s, false, true, true, 0.02f);
	Check(!h.fingerLeads && Near(h.laserAlpha, 1.0f) && !h.mark && !h.changed,
	      "not the tablet (the wrist menu, the big quad): as before");
	TouchHandoverState t;
	StepTouchHandover(t, true, true, true, 0.02f);
	h = StepTouchHandover(t, false, true, true, 0.02f);
	Check(!h.fingerLeads && h.changed, "the tablet gone with the finger leading: the laser's again");
}

void TestAlphaToCompositor() {
	std::printf("The laser's alpha handed on:\n");
	Check(!LaserAlphaChanged(1.0f, 0.99f), "a hundredth: not sent");
	Check(LaserAlphaChanged(1.0f, 0.95f), "five hundredths: sent");
	Check(LaserAlphaChanged(0.02f, 0.0f), "to nothing: sent, however little");
	Check(LaserAlphaChanged(0.98f, 1.0f), "to whole: sent, however little");
	Check(!LaserAlphaChanged(0.0f, 0.0f) && !LaserAlphaChanged(1.0f, 1.0f), "the same: not sent");
	Check(!LaserShownAt(0.0f) && !LaserShownAt(0.005f) && LaserShownAt(0.2f), "faded out: hidden");
}

void TestMark() {
	std::printf("The touch mark:\n");
	const TouchMarkRect faint = TouchMarkAt(true, 300.0f, 800.0f, 0.0f, 2000.0f);
	Check(faint.valid && Near(faint.left, 292.0f) && Near(faint.right, 308.0f) && Near(faint.top, 792.0f) &&
	          Near(faint.bottom, 808.0f),
	      "8 mm round the tip's foot, at 2000 pixels a metre");
	const TouchMarkRect full = TouchMarkAt(true, 300.0f, 800.0f, 1.0f, 2000.0f);
	Check((faint.colour >> 24) == 64u && (full.colour >> 24) == 230u, "brightening from a quarter to nine tenths");
	Check((full.colour & 0xFFu) == 126u && ((full.colour >> 8) & 0xFFu) == 193u, "its colour weighed by its alpha");
	Check(!TouchMarkAt(false, 300.0f, 800.0f, 1.0f, 2000.0f).valid, "not shown: none");
	Check(!TouchMarkAt(true, 300.0f, 800.0f, 1.0f, 0.0f).valid, "no face measured: none");
}

}  // namespace

int main() {
	TestMark();
	TestApproach();
	TestHandCurled();
	TestOffThePage();
	TestAlphaToCompositor();
	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
