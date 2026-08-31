// Checks the letters OBVR draws with and the canvas it draws them on.
//
// These two are the whole reason the settings menu can be written at all
// without a headset. Everything the menu will look like is decided here, in
// integer arithmetic over a plain buffer, so a layout can be got right at a
// desk instead of by putting a headset on to see whether a number is cut off.
//
// The font is worth this much attention for a reason that is easy to miss:
// ninety-five glyphs were typed in by hand, and the failure mode of a hand-made
// font is not a crash. It is one letter quietly being another letter, or two
// characters sharing a shape because a row was pasted and not edited, and that
// survives every review because nobody reads a font - they read the text it
// draws, and their eye corrects it. So the checks below are the ones a reader
// cannot do: every glyph has ink, and no two of them are the same.
//
// No Windows API here, so this one builds and runs on Linux as well.

#include <cstdio>

#include "ui/MenuCanvas.h"
#include "ui/MenuFont.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

using obvr::render::Pixel;

const Pixel kWhite{255, 255, 255, 255};
const Pixel kBlack{0, 0, 0, 255};
const Pixel kClear{0, 0, 0, 0};

bool Same(Pixel a, Pixel b) {
	return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

// How many pixels of ink a glyph has. Used both to find empty glyphs and to
// compare two of them.
UInt32 InkCount(char character) {
	UInt32 count = 0;
	for (UInt32 y = 0; y < obvr::ui::kGlyphHeight; ++y) {
		for (UInt32 x = 0; x < obvr::ui::kGlyphWidth; ++x) {
			if (obvr::ui::GlyphPixel(character, x, y)) {
				++count;
			}
		}
	}
	return count;
}

bool SameShape(char a, char b) {
	for (UInt32 y = 0; y < obvr::ui::kGlyphHeight; ++y) {
		for (UInt32 x = 0; x < obvr::ui::kGlyphWidth; ++x) {
			if (obvr::ui::GlyphPixel(a, x, y) != obvr::ui::GlyphPixel(b, x, y)) {
				return false;
			}
		}
	}
	return true;
}

void TestGlyphBasics() {
	std::printf("The glyphs\n");

	using obvr::ui::GlyphPixel;
	using obvr::ui::kGlyphHeight;
	using obvr::ui::kGlyphWidth;

	// A space is the one printable character that is meant to be empty.
	Check(InkCount(' ') == 0, "a space has no ink");

	// Known shapes, checked at a specific pixel rather than by counting, so
	// that a glyph shifted by one column fails here.
	Check(GlyphPixel('|', 2, 0) && GlyphPixel('|', 2, 6), "a bar is inked top and bottom");
	Check(!GlyphPixel('|', 0, 3), "and not at its left edge");
	Check(GlyphPixel('-', 0, 3) && GlyphPixel('-', 4, 3), "a hyphen spans the middle row");
	Check(!GlyphPixel('-', 2, 0), "and nothing above it");

	// Out of the box in every direction. A menu positions text by arithmetic,
	// and arithmetic produces coordinates one past the end.
	Check(!GlyphPixel('A', kGlyphWidth, 0), "nothing to the right of a glyph");
	Check(!GlyphPixel('A', 0, kGlyphHeight), "nothing below one");
	Check(!GlyphPixel('A', 1000, 1000), "and nothing far outside");

	// Characters with no glyph at all, on both sides of the range and beyond
	// signed char. A tab, a newline and a stray high byte all have to answer
	// rather than read past the table.
	Check(InkCount('\t') == 0, "a tab draws nothing");
	Check(InkCount('\n') == 0, "a newline draws nothing");
	Check(InkCount(static_cast<char>(127)) == 0, "the character past the last glyph draws nothing");
	Check(InkCount(static_cast<char>(200)) == 0, "and so does a high byte");
	Check(InkCount(static_cast<char>(0)) == 0, "and a null");
}

void TestEveryGlyphIsDrawn() {
	std::printf("Every glyph, and every glyph different\n");

	using obvr::ui::kFirstGlyph;
	using obvr::ui::kLastGlyph;

	// An empty glyph is a letter that draws as a hole in a word. The space is
	// the only one allowed to be blank.
	char blank = '\0';
	for (char c = kFirstGlyph; c <= kLastGlyph && c > 0; ++c) {
		if (c == ' ') {
			continue;
		}
		if (InkCount(c) == 0) {
			blank = c;
			break;
		}
	}
	if (blank != '\0') {
		std::printf("        the empty glyph is '%c'\n", blank);
	}
	Check(blank == '\0', "every printable character except space has ink");

	// The check no reader can do. Two glyphs with the same shape means one of
	// them is wrong - a row pasted from the character above and never edited -
	// and the text it draws still looks like words, so nothing else finds it.
	char firstOfPair = '\0';
	char secondOfPair = '\0';
	for (char a = kFirstGlyph; a <= kLastGlyph && a > 0 && firstOfPair == '\0'; ++a) {
		if (a == ' ') {
			continue;
		}
		for (char b = static_cast<char>(a + 1); b <= kLastGlyph && b > 0; ++b) {
			if (SameShape(a, b)) {
				firstOfPair = a;
				secondOfPair = b;
				break;
			}
		}
	}
	if (firstOfPair != '\0') {
		std::printf("        '%c' and '%c' have the same shape\n", firstOfPair, secondOfPair);
	}
	Check(firstOfPair == '\0', "no two characters share a shape");

	// The pairs a reader is most likely to confuse, named individually so a
	// failure says which one rather than only that some pair collided.
	Check(!SameShape('0', 'O'), "zero and capital O differ");
	Check(!SameShape('1', 'l'), "one and lower-case L differ");
	Check(!SameShape('1', 'I'), "one and capital I differ");
	Check(!SameShape('8', 'B'), "eight and capital B differ");
	Check(!SameShape('5', 'S'), "five and capital S differ");
	Check(!SameShape(',', '.'), "a comma and a full stop differ");
}

void TestTextWidth() {
	std::printf("How wide a string is\n");

	using obvr::ui::kGlyphAdvance;
	using obvr::ui::kGlyphWidth;
	using obvr::ui::TextWidth;

	Check(TextWidth("") == 0, "an empty string has no width");
	Check(TextWidth(nullptr) == 0, "and neither does no string at all");

	// One character is its glyph, with no trailing gap - that gap belongs
	// between characters, and counting it puts a right-aligned number a pixel
	// out every single time.
	Check(TextWidth("A") == kGlyphWidth, "one character is one glyph wide");
	Check(TextWidth("AB") == kGlyphWidth + kGlyphAdvance, "two are a glyph and an advance");
	Check(TextWidth("ABC") == kGlyphWidth + 2 * kGlyphAdvance, "and three, two advances");

	// A space is as wide as anything else. A menu lines values up in columns,
	// and a proportional space would take the columns apart.
	Check(TextWidth("A B") == TextWidth("ABC"), "a space is as wide as a letter");
}

void TestCanvasPixels() {
	std::printf("Pixels on a canvas\n");

	Pixel buffer[4 * 3];
	obvr::ui::Canvas canvas(buffer, 4, 3);

	canvas.Fill(kClear);
	Check(Same(canvas.GetPixel(0, 0), kClear), "a fill reaches the first pixel");
	Check(Same(canvas.GetPixel(3, 2), kClear), "and the last one");

	canvas.SetPixel(1, 1, kWhite);
	Check(Same(canvas.GetPixel(1, 1), kWhite), "a pixel set is a pixel read back");
	Check(Same(canvas.GetPixel(0, 1), kClear), "and its neighbour is untouched");

	// Off every edge. None of these may write, and none may be rejected in a
	// way the caller has to know about - a menu draws a highlight by
	// arithmetic that does not know where the edge is.
	canvas.SetPixel(-1, 1, kWhite);
	canvas.SetPixel(1, -1, kWhite);
	canvas.SetPixel(4, 1, kWhite);
	canvas.SetPixel(1, 3, kWhite);
	Check(Same(canvas.GetPixel(0, 1), kClear), "a pixel off the left edge writes nothing");
	Check(Same(canvas.GetPixel(3, 1), kClear), "nor one off the right");

	// Reading outside answers rather than faulting, which is what lets a test
	// probe around a shape without knowing the canvas size.
	Check(Same(canvas.GetPixel(-1, -1), kClear), "reading outside gives a clear pixel");
	Check(Same(canvas.GetPixel(99, 99), kClear), "however far outside");

	// A canvas with no buffer. This is the case a failed texture lock leaves
	// behind, and drawing on it has to be a no-op rather than a fault.
	obvr::ui::Canvas nothing(nullptr, 4, 3);
	nothing.Fill(kWhite);
	nothing.SetPixel(1, 1, kWhite);
	nothing.DrawText(0, 0, "text", 1, kWhite);
	Check(Same(nothing.GetPixel(1, 1), kClear), "a canvas with no buffer draws nothing");
}

void TestCanvasRects() {
	std::printf("Rectangles on a canvas\n");

	Pixel buffer[8 * 8];
	obvr::ui::Canvas canvas(buffer, 8, 8);

	canvas.Fill(kClear);
	canvas.FillRect(2, 2, 3, 2, kWhite);
	Check(Same(canvas.GetPixel(2, 2), kWhite), "a rectangle starts where it says");
	Check(Same(canvas.GetPixel(4, 3), kWhite), "and ends where it says");
	Check(Same(canvas.GetPixel(5, 3), kClear), "one past its right edge is clear");
	Check(Same(canvas.GetPixel(4, 4), kClear), "and one past its bottom");
	Check(Same(canvas.GetPixel(1, 2), kClear), "and one before its left");

	// Nothing at all, which is what a value bar at the bottom of its range
	// asks for. Handled here so the call site does not need the test.
	canvas.Fill(kClear);
	canvas.FillRect(2, 2, 0, 5, kWhite);
	canvas.FillRect(2, 2, 5, 0, kWhite);
	canvas.FillRect(2, 2, -3, -3, kWhite);
	Check(Same(canvas.GetPixel(2, 2), kClear), "a rectangle with no size draws nothing");

	// Half off the canvas on each side: the half that is on has to be drawn.
	canvas.Fill(kClear);
	canvas.FillRect(-2, -2, 4, 4, kWhite);
	Check(Same(canvas.GetPixel(0, 0), kWhite), "a rectangle off the top left draws its corner");
	Check(Same(canvas.GetPixel(1, 1), kWhite), "and the rest of what is on the canvas");
	Check(Same(canvas.GetPixel(2, 2), kClear), "and stops where it should");

	canvas.Fill(kClear);
	canvas.FillRect(6, 6, 4, 4, kWhite);
	Check(Same(canvas.GetPixel(7, 7), kWhite), "and the same off the bottom right");

	// Entirely off, in both directions.
	canvas.Fill(kClear);
	canvas.FillRect(-10, -10, 4, 4, kWhite);
	canvas.FillRect(20, 20, 4, 4, kWhite);
	Check(Same(canvas.GetPixel(0, 0), kClear), "a rectangle entirely outside draws nothing");
	Check(Same(canvas.GetPixel(7, 7), kClear), "from either direction");

	// A frame is hollow.
	canvas.Fill(kClear);
	canvas.DrawFrame(1, 1, 6, 6, 1, kWhite);
	Check(Same(canvas.GetPixel(1, 1), kWhite), "a frame has a top left corner");
	Check(Same(canvas.GetPixel(6, 6), kWhite), "and a bottom right one");
	Check(Same(canvas.GetPixel(3, 1), kWhite), "and a top edge");
	Check(Same(canvas.GetPixel(3, 3), kClear), "and nothing in the middle");

	// A frame thicker than the box it is drawn in fills it, which is the
	// sensible answer - the alternative is an empty rectangle where a solid
	// one was asked for.
	canvas.Fill(kClear);
	canvas.DrawFrame(2, 2, 3, 3, 5, kWhite);
	Check(Same(canvas.GetPixel(3, 3), kWhite), "a frame thicker than its box fills it");

	canvas.Fill(kClear);
	canvas.DrawFrame(1, 1, 4, 4, 0, kWhite);
	Check(Same(canvas.GetPixel(1, 1), kClear), "a frame with no thickness draws nothing");
}

void TestCanvasText() {
	std::printf("Text on a canvas\n");

	using obvr::ui::kGlyphAdvance;

	Pixel buffer[40 * 16];
	obvr::ui::Canvas canvas(buffer, 40, 16);

	// A bar at a known place, so the position of the ink can be checked
	// exactly rather than by "something got drawn".
	canvas.Fill(kClear);
	canvas.DrawText(0, 0, "|", 1, kWhite);
	Check(Same(canvas.GetPixel(2, 0), kWhite), "text lands at the position given");
	Check(Same(canvas.GetPixel(0, 0), kClear), "and not to the left of it");

	canvas.Fill(kClear);
	canvas.DrawText(10, 4, "|", 1, kWhite);
	Check(Same(canvas.GetPixel(12, 4), kWhite), "and it moves with the position");

	// The second character sits one advance along, which is what keeps a line
	// of text from overlapping itself.
	canvas.Fill(kClear);
	canvas.DrawText(0, 0, "||", 1, kWhite);
	Check(Same(canvas.GetPixel(2 + static_cast<SInt32>(kGlyphAdvance), 0), kWhite),
	      "the second character sits one advance along");

	// Scale multiplies both directions, and a font pixel becomes a square
	// block - the block below the first row has to be inked too, or the
	// scaling is stretching rows instead of scaling pixels.
	canvas.Fill(kClear);
	canvas.DrawText(0, 0, "|", 2, kWhite);
	Check(Same(canvas.GetPixel(4, 0), kWhite), "a scaled glyph starts at the scaled position");
	Check(Same(canvas.GetPixel(5, 1), kWhite), "and one font pixel is a square block");
	Check(Same(canvas.GetPixel(3, 0), kClear), "with nothing beside it");

	// A scale of zero or less would otherwise draw rectangles of no size
	// forever, or worse.
	canvas.Fill(kClear);
	canvas.DrawText(0, 0, "|", 0, kWhite);
	canvas.DrawText(0, 0, "|", -2, kWhite);
	Check(Same(canvas.GetPixel(2, 0), kClear), "text at no scale draws nothing");

	// Text running off the edge draws the part that fits. A menu that
	// truncates a long setting name must not also corrupt the buffer.
	canvas.Fill(kClear);
	canvas.DrawText(36, 0, "||||||||", 1, kWhite);
	Check(Same(canvas.GetPixel(38, 0), kWhite), "text off the right edge draws what fits");

	canvas.Fill(kClear);
	canvas.DrawText(-4, 0, "|", 1, kWhite);
	Check(Same(canvas.GetPixel(0, 0), kClear), "and a glyph entirely off the left draws nothing");

	// A character with no glyph leaves a gap and does not stop the string -
	// the characters after it still have to appear, or one bad byte silently
	// truncates a line.
	canvas.Fill(kClear);
	canvas.DrawText(0, 0, "\t|", 1, kWhite);
	Check(Same(canvas.GetPixel(2 + static_cast<SInt32>(kGlyphAdvance), 0), kWhite),
	      "an unknown character leaves a gap and the line goes on");

	canvas.Fill(kClear);
	canvas.DrawText(0, 0, nullptr, 1, kWhite);
	Check(Same(canvas.GetPixel(0, 0), kClear), "no string draws nothing");

	// Colour is carried through rather than assumed white, since the menu
	// draws the same text in more than one colour.
	canvas.Fill(kClear);
	canvas.DrawText(0, 0, "|", 1, kBlack);
	Check(Same(canvas.GetPixel(2, 0), kBlack), "text is drawn in the colour asked for");
}

void TestStride() {
	std::printf("A canvas whose rows are further apart than they are wide\n");

	// What a locked Direct3D surface hands back: a pitch of the driver's
	// choosing, wider than the picture. A canvas that assumed rows were
	// adjacent would draw an image that sheared further sideways with every
	// row - and it would do it only on hardware that pads, which is the worst
	// kind of fault to go looking for.
	constexpr UInt32 kWide = 4;
	constexpr UInt32 kTall = 3;
	constexpr UInt32 kStride = 8;

	Pixel buffer[kStride * kTall];
	for (UInt32 at = 0; at < kStride * kTall; ++at) {
		buffer[at] = kBlack;
	}

	obvr::ui::Canvas canvas(buffer, kWide, kTall, kStride);
	canvas.Fill(kClear);

	// Every visible pixel was filled...
	bool filled = true;
	for (UInt32 row = 0; row < kTall && filled; ++row) {
		for (UInt32 column = 0; column < kWide; ++column) {
			if (!Same(buffer[row * kStride + column], kClear)) {
				filled = false;
				break;
			}
		}
	}
	Check(filled, "a fill reaches every visible pixel");

	// ...and nothing in the padding was. That padding belongs to the driver on
	// a locked surface, and writing into it is the fault this test exists for.
	bool paddingIntact = true;
	for (UInt32 row = 0; row < kTall && paddingIntact; ++row) {
		for (UInt32 column = kWide; column < kStride; ++column) {
			if (!Same(buffer[row * kStride + column], kBlack)) {
				paddingIntact = false;
				break;
			}
		}
	}
	Check(paddingIntact, "and a fill writes nothing into the padding");

	// A pixel lands at its strided position, not its dense one. Row 1 column 3
	// is index 11 with a stride of 8, and index 7 without - and index 7 is row
	// 1 column 3 of a dense buffer, so a canvas that ignored the stride would
	// still write somewhere plausible. That is precisely why this is checked by
	// index rather than by reading it back through the canvas.
	canvas.SetPixel(3, 1, kWhite);
	Check(Same(buffer[1 * kStride + 3], kWhite), "a pixel lands at its strided position");
	Check(!Same(buffer[1 * kWide + 3], kWhite), "and nothing was written at the dense one");
	Check(Same(canvas.GetPixel(3, 1), kWhite), "and reads back through the canvas");

	// Clipping still goes by the visible width, not the stride. A canvas that
	// clipped at the stride would let a rectangle run into the padding.
	canvas.Fill(kClear);
	canvas.FillRect(0, 0, 100, 100, kWhite);
	bool clipped = true;
	for (UInt32 row = 0; row < kTall && clipped; ++row) {
		for (UInt32 column = kWide; column < kStride; ++column) {
			if (!Same(buffer[row * kStride + column], kBlack)) {
				clipped = false;
				break;
			}
		}
	}
	Check(clipped, "a rectangle is clipped to the visible width, not the stride");

	// A stride of zero means the rows are adjacent, which is what a plain
	// buffer wants and what every other test here relies on.
	Pixel dense[kWide * kTall];
	obvr::ui::Canvas tight(dense, kWide, kTall, 0);
	tight.Fill(kClear);
	tight.SetPixel(3, 1, kWhite);
	Check(Same(dense[1 * kWide + 3], kWhite), "a stride of zero means rows are adjacent");
}

void TestChannelSwap() {
	std::printf("Red and blue exchanged\n");

	using obvr::ui::SwapRedAndBlue;

	// Pixel is laid out red first; Direct3D 9's A8R8G8B8 is blue first in
	// memory. A theme drawn straight into a locked D3D9 surface without this
	// comes out with its reds and blues exchanged - which looks deliberate
	// enough that it might survive a first look in a headset.
	const Pixel red{200, 10, 20, 255};
	const Pixel swapped = SwapRedAndBlue(red);
	Check(swapped.r == 20 && swapped.g == 10 && swapped.b == 200 && swapped.a == 255,
	      "red and blue change places and green and alpha do not");

	// Twice is the original, which is the property that makes it safe to apply
	// to a palette without tracking whether it has been applied.
	const Pixel back = SwapRedAndBlue(swapped);
	Check(Same(back, red), "swapping twice gives back what went in");

	// A grey is its own swap, so a monochrome theme cannot tell the two apart -
	// worth knowing when a test picks a colour to look for.
	const Pixel grey{128, 128, 128, 255};
	Check(Same(SwapRedAndBlue(grey), grey), "a grey is unchanged by the swap");
}

}  // namespace

int main() {
	std::printf("OBVR menu drawing test\n\n");

	TestGlyphBasics();
	std::printf("\n");
	TestEveryGlyphIsDrawn();
	std::printf("\n");
	TestTextWidth();
	std::printf("\n");
	TestCanvasPixels();
	std::printf("\n");
	TestCanvasRects();
	std::printf("\n");
	TestCanvasText();
	std::printf("\n");
	TestStride();
	std::printf("\n");
	TestChannelSwap();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
