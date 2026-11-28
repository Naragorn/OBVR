// Checks the texts beside the tablet's page (vr/TabletTexts.h): which of the
// layer's text rectangles go above the page and which never, and where they
// stand there.

#include <cstdio>

#include "vr/TabletTexts.h"

using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b) { return a - b < 0.01f && b - a < 0.01f; }

UiRect Rect(float l, float t, float r, float b) {
	UiRect u;
	u.left = l;
	u.top = t;
	u.right = r;
	u.bottom = b;
	u.valid = true;
	return u;
}

// The face of the run of 2026-11-28 (OBVR.log 4127: the Map page's face
// 63,151..2849,2182) and its notices (1946: Messages 113,94..1750,201).
const UiRect kFace = Rect(63.0f, 151.0f, 2849.0f, 2182.0f);
const UiRect kNotices = Rect(113.0f, 94.0f, 1750.0f, 201.0f);

void TestBeside() {
	std::printf("Which texts go above the page:\n");
	UiRect texts[kTabletTextsMost];
	Check(!TextsBesidePage(texts, kTabletTextsMost, kFace).valid, "none read: none");
	Check(!TextsBesidePage(nullptr, 3, kFace).valid, "no list: none");
	texts[0] = kNotices;
	UiRect r = TextsBesidePage(texts, kTabletTextsMost, kFace);
	Check(r.valid && Near(r.left, 113.0f) && Near(r.bottom, 201.0f), "the notices, half above the face: they go");
	texts[1] = Rect(500.0f, 600.0f, 900.0f, 640.0f);
	r = TextsBesidePage(texts, 2, kFace);
	Check(Near(r.bottom, 201.0f), "a subtitle wholly on the face: on the tablet already, not taken twice");
	texts[2] = Rect(3000.0f, 2100.0f, 3400.0f, 2150.0f);
	r = TextsBesidePage(texts, 3, kFace);
	Check(Near(r.left, 113.0f) && Near(r.right, 3400.0f) && Near(r.top, 94.0f) && Near(r.bottom, 2150.0f),
	      "a line beside the face as well: one rectangle round both");
	UiRect flat[2] = {Rect(10.0f, 10.0f, 10.0f, 50.0f), Rect(10.0f, 10.0f, 40.0f, 10.0f)};
	Check(!TextsBesidePage(flat, 2, kFace).valid, "rectangles without width or height: none");
	UiRect noFace;
	texts[1] = Rect(500.0f, 600.0f, 900.0f, 640.0f);
	r = TextsBesidePage(texts + 1, 1, noFace);
	Check(r.valid, "no face to be on: every text counts");
}

void TestAbove() {
	std::printf("Where they stand:\n");
	UiRect at = TextsAbovePage(kNotices, kFace);
	const float gap = kTabletTextsGapShare * (2182.0f - 151.0f);
	Check(at.valid && Near(at.bottom, 151.0f - gap) && Near(at.bottom - at.top, 107.0f),
	      "above the face's top edge, a small gap between, at the page's own scale");
	Check(Near(0.5f * (at.left + at.right), 0.5f * (63.0f + 2849.0f)) && Near(at.right - at.left, 1637.0f),
	      "its middle over the face's middle, as wide as read");
	const UiRect wide = Rect(0.0f, 0.0f, 5572.0f, 100.0f);
	at = TextsAbovePage(wide, kFace);
	Check(Near(at.right - at.left, 2786.0f) && Near(at.bottom - at.top, 50.0f),
	      "wider than the face: narrowed to its width, its shape kept");
	Check(!TextsAbovePage(UiRect{}, kFace).valid, "no texts: nothing");
	Check(!TextsAbovePage(kNotices, UiRect{}).valid, "no face: nothing");
}

}  // namespace

int main() {
	TestBeside();
	TestAbove();
	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
