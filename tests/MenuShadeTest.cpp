// Checks the decisions behind the pause-menu dressing and the dialogue-zoom
// patch: how a tone is read and folded with its strength, where the window
// both eyes share lands in each eye, which strips lie outside it, and when
// the zoom patch acts. The stakes are a frozen world that should look paused
// rather than broken, and a patch that must act exactly once per change.

#include <cstdio>

#include "game/DialogZoom.h"
#include "render/MenuShade.h"

namespace {

using obvr::game::DecideDialogPov;
using obvr::game::DecideDialogZoom;
using obvr::game::DialogPovAction;
using obvr::game::DialogZoomAction;
using obvr::render::CommonWindowInEye;
using obvr::render::ComposeShadeColor;
using obvr::render::EdgeStrips;
using obvr::render::ParseHexColor;
using obvr::render::ShadeRectangleForPicture;
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

void TestShadeRectangle() {
	std::printf("The rectangle repainted by the shade\n");
	const Rect eye{10, 20, 990, 700};
	const Rect common{50, 20, 950, 700};
	const Rect flat{200, 150, 800, 550};

	Check(&ShadeRectangleForPicture(eye, common, flat, false, false) == &eye,
	      "a normal untrimmed eye shades its world rectangle");
	Check(&ShadeRectangleForPicture(eye, common, flat, true, false) == &common,
	      "a trimmed held eye shades only the shared window");
	Check(&ShadeRectangleForPicture(eye, common, flat, false, true) == &flat,
	      "a cinema frame shades its flat rectangle");
	Check(&ShadeRectangleForPicture(eye, common, flat, true, true) == &flat,
	      "flat placement outranks an irrelevant held-border request");
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

void TestDialogZoomDecision() {
	std::printf("Dialogue zoom decisions\n");

	Check(DecideDialogZoom(false, false) == DialogZoomAction::Patch,
	      "zoom unwanted and unpatched patches");
	Check(DecideDialogZoom(false, true) == DialogZoomAction::Nothing,
	      "zoom unwanted and already patched rests");
	Check(DecideDialogZoom(true, true) == DialogZoomAction::Restore,
	      "zoom wanted again restores");
	Check(DecideDialogZoom(true, false) == DialogZoomAction::Nothing,
	      "zoom wanted and untouched rests");
}

void TestDialogPovDecision() {
	std::printf("Dialogue point-of-view decisions\n");

	// A conversation starting with a third-person player flips - and it
	// flips WHATEVER the latch says. The first version asked the latch too,
	// and when the game's call pattern left it set, every dialogue after the
	// first went unflipped. This is the flow that pins the fix.
	Check(DecideDialogPov(true, true, false, true) == DialogPovAction::FlipToFirst,
	      "a starting conversation flips a third-person player");
	Check(DecideDialogPov(true, true, true, true) == DialogPovAction::FlipToFirst,
	      "even with the latch still set from a lost ending");

	// A first-person player is already where the flip would put them.
	Check(DecideDialogPov(true, false, false, true) == DialogPovAction::Nothing,
	      "a first-person player is left alone");
	Check(DecideDialogPov(true, false, true, true) == DialogPovAction::Nothing,
	      "also when a flip is still latched - the view is already right");

	// The feature switch gates the flip and only the flip.
	Check(DecideDialogPov(true, true, false, false) == DialogPovAction::Nothing,
	      "with the feature off a starting conversation changes nothing");
	Check(DecideDialogPov(false, false, true, false) == DialogPovAction::FlipBack,
	      "but a flip already made is still cleaned up when the conversation ends");

	// The end undoes OBVR's own flip and nothing else.
	Check(DecideDialogPov(false, false, true, true) == DialogPovAction::FlipBack,
	      "the end flips back what the start flipped");
	Check(DecideDialogPov(false, false, false, true) == DialogPovAction::Nothing,
	      "and leaves a player who was never flipped alone");
	Check(DecideDialogPov(false, true, false, true) == DialogPovAction::Nothing,
	      "including one already back in third person");
	Check(DecideDialogPov(false, true, true, true) == DialogPovAction::FlipBack,
	      "a latched flip is undone even if the view already looks third person");
}

}  // namespace

int main() {
	TestParseHexColor();
	TestComposeShadeColor();
	TestShadeRectangle();
	TestCommonWindow();
	TestEdgeStrips();
	TestDialogZoomDecision();
	TestDialogPovDecision();

	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all ok\n");
	return 0;
}
