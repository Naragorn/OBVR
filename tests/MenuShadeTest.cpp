// Checks the decisions behind the pause-menu dressing and the fDlgFocus
// override: how a tone is read and folded with its strength, where the window
// both eyes share lands in each eye, which strips lie outside it, and when
// the override captures, holds and restores. The stakes are a frozen world
// that should look paused rather than broken, and a setting that must return
// to the person's own value the moment they ask for vanilla back.

#include <cstdio>

#include "game/DialogZoom.h"
#include "render/MenuShade.h"

namespace {

using obvr::game::DecideDialogFocus;
using obvr::game::DialogFocusAction;
using obvr::render::CommonWindowInEye;
using obvr::render::ComposeShadeColor;
using obvr::render::EdgeStrips;
using obvr::render::ParseHexColor;
using obvr::render::d3d9::Rect;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool SameRect(const Rect& a, const Rect& b) {
	return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

void TestParseHexColor() {
	std::printf("Hex colour parsing\n");

	UInt32 rgb = 0xDEADBEEF;
	Check(ParseHexColor("5A452E", rgb) && rgb == 0x5A452E, "six digits parse");
	Check(ParseHexColor("#8a6d4a", rgb) && rgb == 0x8A6D4A,
	      "a leading # and lower case both parse");
	Check(ParseHexColor("FFFFFF", rgb) && rgb == 0xFFFFFF, "white parses");
	Check(ParseHexColor("000000", rgb) && rgb == 0, "black parses");

	rgb = 0xDEADBEEF;
	Check(!ParseHexColor("5A452", rgb), "five digits are refused");
	Check(!ParseHexColor("5A452E1", rgb), "seven digits are refused");
	Check(!ParseHexColor("", rgb), "an empty value is refused");
	Check(!ParseHexColor("#", rgb), "a bare # is refused");
	Check(!ParseHexColor("brown!", rgb), "a word is refused");
	Check(!ParseHexColor(nullptr, rgb), "a null string is refused");
	Check(rgb == 0xDEADBEEF, "a refusal leaves the output untouched");
}

void TestComposeShadeColor() {
	std::printf("Shade colour composition\n");

	Check(ComposeShadeColor(0x5A452E, 1.0f) == 0xFF5A452E, "full strength is opaque");
	Check(ComposeShadeColor(0x5A452E, 0.5f) == 0x805A452E, "half strength rounds to 0x80");
	Check(ComposeShadeColor(0x5A452E, 0.0f) == 0, "zero strength means no tint");
	Check(ComposeShadeColor(0x5A452E, -1.0f) == 0, "negative strength means no tint");
	Check(ComposeShadeColor(0x5A452E, 2.0f) == 0xFF5A452E, "strength clamps at one");
	Check(ComposeShadeColor(0x000000, 1.0f) == 0xFF000000,
	      "a black shade at full strength still counts as a tint");
	Check(ComposeShadeColor(0xFF5A452E, 1.0f) == 0xFF5A452E,
	      "stray high bits in the colour are masked off");
}

void TestCommonWindow() {
	std::printf("The window both eyes share\n");

	// Identical slices: nothing is exclusive to one eye, so the window is the
	// whole destination.
	const Rect src{0, 0, 2560, 1440};
	const Rect dest{100, 200, 2100, 1300};
	Check(SameRect(CommonWindowInEye(src, dest, src), dest),
	      "matching slices keep the whole picture");

	// Opposite crops, the measured shape: the left eye misses the frame's
	// right edge, the right eye its left edge. The shared window trims each
	// eye by the strip only it shows - a quarter of the frame maps to a
	// quarter of the destination's width.
	const Rect leftSrc{0, 0, 2000, 1440};
	const Rect rightSrc{560, 0, 2560, 1440};
	const Rect eyeDest{0, 0, 1000, 720};
	const Rect leftCommon = CommonWindowInEye(leftSrc, eyeDest, rightSrc);
	Check(leftCommon.left == 280 && leftCommon.right == 1000,
	      "the left eye loses its exclusive left strip");
	const Rect rightCommon = CommonWindowInEye(rightSrc, eyeDest, leftSrc);
	Check(rightCommon.left == 0 && rightCommon.right == 720,
	      "the right eye loses its exclusive right strip");
	Check(leftCommon.top == 0 && leftCommon.bottom == 720 && rightCommon.top == 0 &&
	          rightCommon.bottom == 720,
	      "matching vertical slices stay whole");

	// Vertical crops trim the same way.
	const Rect topSrc{0, 0, 2560, 1200};
	const Rect bottomSrc{0, 240, 2560, 1440};
	const Rect vCommon = CommonWindowInEye(topSrc, eyeDest, bottomSrc);
	Check(vCommon.top == 144 && vCommon.bottom == 720,
	      "a vertical mismatch trims the top the same way");

	// Degenerate inputs return the destination untouched rather than blacking
	// out a picture that is already wrong for another reason.
	const Rect emptySrc{100, 100, 100, 400};
	Check(SameRect(CommonWindowInEye(emptySrc, dest, src), dest),
	      "an empty slice keeps the whole picture");
	const Rect disjointSrc{2500, 0, 2560, 1440};
	const Rect narrowSrc{0, 0, 60, 1440};
	Check(SameRect(CommonWindowInEye(narrowSrc, dest, disjointSrc), dest),
	      "slices that do not overlap keep the whole picture");
}

void TestEdgeStrips() {
	std::printf("Edge strips\n");

	const Rect dest{0, 0, 1000, 720};
	Rect strips[4];

	Check(EdgeStrips(dest, dest, strips) == 0, "a whole window needs no strips");

	const Rect trimmedLeft{280, 0, 1000, 720};
	int count = EdgeStrips(dest, trimmedLeft, strips);
	Check(count == 1, "a left trim is one strip");
	Check(count == 1 && SameRect(strips[0], Rect{0, 0, 280, 720}),
	      "the left strip covers exactly the exclusive part");

	const Rect trimmedBoth{100, 50, 900, 700};
	count = EdgeStrips(dest, trimmedBoth, strips);
	Check(count == 4, "a window inside on every side is four strips");
	Check(count == 4 && SameRect(strips[0], Rect{0, 0, 100, 720}) &&
	          SameRect(strips[1], Rect{900, 0, 1000, 720}) &&
	          SameRect(strips[2], Rect{0, 0, 1000, 50}) &&
	          SameRect(strips[3], Rect{0, 700, 1000, 720}),
	      "the four strips cover the four exclusive margins");
}

void TestDialogFocusDecision() {
	std::printf("Dialogue focus decisions\n");

	// zoomWanted, originalKnown, slotHoldsOverride - every combination.
	Check(DecideDialogFocus(false, false, false) == DialogFocusAction::CaptureAndHold,
	      "first unwanted zoom captures the original and holds");
	Check(DecideDialogFocus(false, false, true) == DialogFocusAction::CaptureAndHold,
	      "a slot already at the figure still captures - the person may have "
	      "written it themselves");
	Check(DecideDialogFocus(false, true, false) == DialogFocusAction::Hold,
	      "a drifted slot is re-held without re-capturing");
	Check(DecideDialogFocus(false, true, true) == DialogFocusAction::Nothing,
	      "a held slot rests");
	Check(DecideDialogFocus(true, true, true) == DialogFocusAction::Restore,
	      "zoom wanted again restores the captured value");
	Check(DecideDialogFocus(true, true, false) == DialogFocusAction::Nothing,
	      "zoom wanted with the slot already the person's own rests");
	Check(DecideDialogFocus(true, false, false) == DialogFocusAction::Nothing,
	      "zoom wanted and never overridden rests");
	Check(DecideDialogFocus(true, false, true) == DialogFocusAction::Nothing,
	      "zoom wanted with nothing captured cannot restore");
}

}  // namespace

int main() {
	TestParseHexColor();
	TestComposeShadeColor();
	TestCommonWindow();
	TestEdgeStrips();
	TestDialogFocusDecision();

	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all ok\n");
	return 0;
}
