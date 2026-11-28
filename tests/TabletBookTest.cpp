// Checks books and letters in the hand (vr/TabletBook.h, docs/inventory-
// tablet-spec.md 25): the book menu alone is the tablet's and runs the world;
// its look from the game's user0; the finger's book mode - sideways turns a
// book and not a letter, up and down scroll a letter and not a book; a turn
// as a wheel notch on a book, a page key on the F1-F4 pages, nothing on a
// letter; its own width; its face with a letter's roller and buttons; and the
// same through the hand mode, the right stick's flick and a left-handed
// player's mirror included.

#include <cstdio>

#include "vr/MenuPlace.h"
#include "vr/TabletBook.h"

using namespace obvr;
using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float tolerance = 0.001f) {
	const float d = a - b;
	return d < tolerance && d > -tolerance;
}

UiRect Rect(float left, float top, float right, float bottom) {
	UiRect r;
	r.left = left;
	r.top = top;
	r.right = right;
	r.bottom = bottom;
	r.valid = true;
	return r;
}

PaperTile Tile(float left, float top, float right, float bottom) {
	PaperTile t;
	t.left = left;
	t.top = top;
	t.right = right;
	t.bottom = bottom;
	return t;
}

void TestWhich() {
	std::printf("The book menu alone is the tablet's\n");
	const UInt32 book[] = {game::kMenuIdBook};
	Check(TabletMenusUp(book, 1), "a book, a note or a letter from the world: held in the hand");
	const UInt32 withHud[] = {game::kMenuIdHudMain, game::kMenuIdBook, game::kMenuIdHudReticle};
	Check(TabletMenusUp(withHud, 3), "the HUD's entries beside it change nothing");
	const UInt32 fromPack[] = {game::kMenuIdBigFour, game::kMenuIdBook};
	Check(TabletMenusUp(fromPack, 2), "a book from the inventory over the pages: still the tablet's, as before");
	const UInt32 message[] = {game::kMenuIdBook, game::kMenuIdMessage};
	Check(TabletMenusUp(message, 2), "a script's message over a book: on it too");
	const UInt32 container[] = {game::kMenuIdContainer, game::kMenuIdBook};
	Check(!TabletMenusUp(container, 2), "a book under a container's menu: its own panel, as any menu beside one");
	const UInt32 dialog[] = {game::kMenuIdDialog, game::kMenuIdBook};
	Check(!TabletMenusUp(dialog, 2), "a book handed over in a conversation: its own panel");
	const UInt32 alone[] = {game::kMenuIdMessage};
	Check(!TabletMenusUp(alone, 1), "a message with neither pages nor a book under it is not");

	Check(TabletMenuRunsWorld(game::kMenuIdBook), "the world runs behind a book (Nadi, 2026-11-27: 25.5 (a))");
	Check(!TabletMenuRunsWorld(game::kMenuIdMessage) && !TabletMenuRunsWorld(game::kMenuIdGeneric),
	      "a message and a Generic box still pause it");
}

void TestLook() {
	std::printf("Its look\n");
	Check(BookMenuIsScroll(1.0f), "user0 1 (the record's scroll flag): a letter");
	Check(!BookMenuIsScroll(2.0f), "user0 2: a book");
	Check(BookMenuIsScroll(0.0f), "unread: the scroll's rules, which turn nothing");

	TabletFrame f;
	f.up = true;
	f.page = game::kMenuIdInventory;
	Check(TabletLookOf(f) == TabletLook::Pages, "the inventory's page: the pages");
	f.page = game::kMenuIdBook;
	Check(TabletLookOf(f) == TabletLook::Book, "the book menu read alone, its book look: a book");
	f.bookScroll = true;
	Check(TabletLookOf(f) == TabletLook::Letter, "its scroll look: a letter");
	f.up = false;
	Check(TabletLookOf(f) == TabletLook::Pages, "nothing up: no book");

	Check(TabletTouchTurns(TabletLook::Pages) && TabletTouchScrolls(TabletLook::Pages),
	      "the pages: a swipe turns, a drag scrolls");
	Check(TabletTouchTurns(TabletLook::Book) && !TabletTouchScrolls(TabletLook::Book),
	      "a book: a swipe turns, up and down nothing");
	Check(!TabletTouchTurns(TabletLook::Letter) && TabletTouchScrolls(TabletLook::Letter),
	      "a letter: up and down scroll, sideways nothing");
}

void TestTurn() {
	std::printf("A turn\n");
	TabletTurn t = TabletTurnFor(TabletLook::Book, 1);
	Check(!t.pageKey && t.wheel == -1, "a book, the finger moved left: on - one notch down");
	t = TabletTurnFor(TabletLook::Book, -1);
	Check(!t.pageKey && t.wheel == 1, "moved right: back - one notch up");
	t = TabletTurnFor(TabletLook::Letter, 1);
	Check(!t.pageKey && t.wheel == 0, "a letter: nothing");
	t = TabletTurnFor(TabletLook::Pages, -1);
	Check(t.pageKey && t.wheel == 0, "the pages: the page key, as before");
	t = TabletTurnFor(TabletLook::Book, 0);
	Check(!t.pageKey && t.wheel == 0, "no step: nothing");
}

void TestWidth() {
	std::printf("Its width\n");
	TabletSettings s;
	Check(Near(s.bookWidthMetres, 0.40f) && Near(s.widthMetres, 0.34f), "by default 0.40 m, the pages 0.34 m");
	TabletFrame f;
	f.up = true;
	f.page = game::kMenuIdBook;
	Check(Near(TabletPaperWidthMetres(f, s), 0.40f), "a book read alone: its own width");
	f.page = game::kMenuIdInventory;
	Check(Near(TabletPaperWidthMetres(f, s), 0.34f), "the inventory: the page's");

	// A book's paper 1152 units in a 1600-pixel layer's terms: 1152 wide, the face as wide.
	f.page = game::kMenuIdBook;
	f.paper = Rect(100.0f, 50.0f, 1252.0f, 906.0f);
	f.crop = f.paper;
	f.outline = f.paper;
	f.pageCrop = f.paper;
	const TabletPlace left = PlaceTablet(f, s, true);
	const TabletPlace right = PlaceTablet(f, s, false);
	Check(left.valid && Near(left.width, 0.40f) && Near(left.height, 0.40f * 856.0f / 1152.0f),
	      "held in the left hand: 0.40 m wide, as high as its paper");
	Check(right.valid && Near(right.width, left.width) && Near(right.height, left.height),
	      "a left-handed player's right hand: the same size");
}

void TestFace() {
	std::printf("Its face\n");
	// Vanilla book_menu.xml in a 1280 x 960 unit screen: the scroll's sheet
	// 990 x 740 at 145,11, its roller 1171 x 148 at 34,750, Take and Exit
	// 72 under the roller's top.
	const PaperTile letter[] = {Tile(145.0f, 11.0f, 1135.0f, 751.0f), Tile(34.0f, 750.0f, 1205.0f, 898.0f),
	                            Tile(195.0f, 822.0f, 395.0f, 872.0f), Tile(990.0f, 822.0f, 1190.0f, 872.0f)};
	TabletRects r = BookPaperOf(letter, 4, 1280.0f, 960.0f);
	Check(r.valid && Near(r.paper.left, 145.0f) && Near(r.paper.right, 1135.0f),
	      "a letter: its sheet is the paper the width is measured by");
	Check(Near(r.crop.left, 34.0f) && Near(r.crop.right, 1205.0f) && Near(r.crop.bottom, 898.0f) &&
	          Near(r.outline.bottom, 898.0f) && r.popupCount == 0,
	      "and the face holds its roller and its buttons below it");
	const TabletRects plain = PaperOf(letter, 4, 1280.0f, 960.0f);
	Check(Near(plain.crop.bottom, 751.0f), "(the pages' rule alone cut them off)");
	const PaperTile book[] = {Tile(64.0f, 52.0f, 1216.0f, 908.0f), Tile(114.0f, 817.0f, 314.0f, 867.0f)};
	r = BookPaperOf(book, 2, 1280.0f, 960.0f);
	Check(r.valid && Near(r.crop.left, 64.0f) && Near(r.crop.bottom, 908.0f), "a book: its buttons lie on its pages");
	r = BookPaperOf(nullptr, 0, 1280.0f, 960.0f);
	Check(!r.valid, "no pictures: no page");
}

// The touch alone, in a book's and a letter's mode.
TabletTouchInput At(float x, float y, bool touching, TabletLook look) {
	TabletTouchInput in;
	in.onGlass = true;
	in.touching = touching;
	in.glassX = x;
	in.glassY = y;
	in.pixelX = 500.0f + x * 1000.0f;
	in.pixelY = 500.0f + y * 1000.0f;
	in.turns = TabletTouchTurns(look);
	in.scrolls = TabletTouchScrolls(look);
	return in;
}

struct Stroke {
	int wheel = 0;
	int pages = 0;
	bool clicked = false;
	TabletTouchEvent last = TabletTouchEvent::None;
};

Stroke Draw(TabletLook look, float x, float y, float tx, float ty) {
	TabletTouchState s;
	Stroke out;
	constexpr int kFrames = 20;
	for (int i = 0; i <= kFrames + 2; ++i) {
		const float share = i < kFrames ? static_cast<float>(i) / kFrames : 1.0f;
		const TabletTouchVerdict v =
			StepTabletTouch(s, At(x + (tx - x) * share, y + (ty - y) * share, i <= kFrames, look), 1.0f / 90.0f);
		out.wheel += v.wheel;
		out.pages += v.pageStep;
		out.clicked = out.clicked || v.mouseDown;
		out.last = v.event != TabletTouchEvent::None ? v.event : out.last;
	}
	return out;
}

void TestTouchModes() {
	std::printf("The finger on a book and on a letter\n");
	Stroke st = Draw(TabletLook::Book, 0.05f, 0.0f, -0.05f, 0.0f);
	Check(st.pages == 1 && st.wheel == 0 && !st.clicked, "a book, drawn 10 cm left: one turn on, nothing clicked");
	st = Draw(TabletLook::Book, -0.05f, 0.0f, 0.05f, 0.0f);
	Check(st.pages == -1, "drawn right: one back");
	st = Draw(TabletLook::Book, 0.0f, -0.04f, 0.0f, 0.04f);
	Check(st.wheel == 0 && st.pages == 0 && !st.clicked && st.last == TabletTouchEvent::Moved,
	      "drawn 8 cm down a book: no scroll, no turn, no click - moved and let go");
	st = Draw(TabletLook::Letter, 0.0f, 0.04f, 0.0f, -0.04f);
	Check(st.wheel < 0 && st.pages == 0 && !st.clicked, "drawn up a letter: it scrolls on (notches down)");
	st = Draw(TabletLook::Letter, 0.05f, 0.0f, -0.05f, 0.0f);
	Check(st.pages == 0 && st.wheel == 0 && !st.clicked && st.last == TabletTouchEvent::Moved,
	      "drawn sideways on a letter: nothing");
	st = Draw(TabletLook::Book, 0.0f, 0.0f, 0.004f, 0.0f);
	Check(st.clicked && st.pages == 0, "a tap on a book: the click (Take, Exit, Prev, Next)");
	st = Draw(TabletLook::Letter, 0.0f, 0.0f, 0.0f, 0.004f);
	Check(st.clicked && st.wheel == 0, "and on a letter");
}

// Through the hand mode, as tests/MenuPlaceTest.cpp does: a 1152 x 856 book
// at 100,200 of a 1600 x 1200 layer, the off hand half a metre ahead.
HandSettings BookSettings() {
	HandSettings s;
	s.enabled = true;
	s.laserOffsetRightMetres = 0.0f;
	s.laserOffsetUpMetres = 0.0f;
	s.laserPitchDegrees = 0.0f;
	s.laserYawDegrees = 0.0f;
	s.laserOriginMetres = 0.0f;
	s.laserDragScroll = false;
	s.laserCursorDirect = true;
	s.tablet.enabled = true;
	s.tablet.unroll = TabletUnroll::Vertical;
	s.handGripUpMetres = 0.0f;
	s.tablet.bookWidthMetres = 0.40f;
	s.tablet.gripUp = 0.0f;
	s.tablet.gripForward = 0.0f;
	s.tablet.tiltDegrees = 0.0f;
	s.tablet.gripShare = 0.5f;
	return s;
}

HandModeFrame BookUp(bool scroll, bool leftHanded) {
	HandModeFrame f;
	f.headValid = true;
	f.menuMode = true;
	f.inWorld = true;
	f.rolesSwapped = leftHanded;
	f.layerPixelsWidth = 1600.0f;
	f.layerPixelsHeight = 1200.0f;
	f.cursorValid = true;
	f.cursorX = 800.0f;
	f.cursorY = 600.0f;
	f.dtSeconds = 1.0f / 90.0f;
	f.tablet.up = true;
	f.tablet.page = game::kMenuIdBook;
	f.tablet.bookScroll = scroll;
	f.tablet.paper = Rect(100.0f, 200.0f, 1252.0f, 1056.0f);
	f.tablet.crop = f.tablet.paper;
	f.left.valid = true;
	f.left.position = NiPoint3{0.0f, 0.0f, -0.5f};
	f.right.valid = true;
	f.right.position = NiPoint3{0.1f, 0.3f, 0.0f};
	return f;
}

struct Swept {
	int pages = 0;
	int wheel = 0;
	bool clicked = false;
};

// The right hand's tip a centimetre off the glass, drawn from one point to
// another on it, then taken away.
Swept Sweep(HandModeFrame f, const HandSettings& s, float x, float y, float tx, float ty) {
	const float glass = -0.5f + 0.08f + 0.01f;
	HandMode mode;
	Swept out;
	for (int i = 0; i <= 30; ++i) {
		const float share = static_cast<float>(i) / 30.0f;
		f.right.position = NiPoint3{x + (tx - x) * share, y + (ty - y) * share, glass};
		const HandModeResult r = mode.Update(f, s);
		out.pages += r.tabletPageStep;
		out.wheel += r.menuScroll;
		out.clicked = out.clicked || r.controls.menuClick;
	}
	f.right.position.z += 0.1f;
	const HandModeResult r = mode.Update(f, s);
	out.pages += r.tabletPageStep;
	out.wheel += r.menuScroll;
	out.clicked = out.clicked || r.controls.menuClick;
	return out;
}

void TestInHandMode() {
	std::printf("In the hand mode\n");
	HandSettings s = BookSettings();
	s.tablet.drawnFinger = false;
	for (int handed = 0; handed < 2; ++handed) {
		const bool leftHanded = handed == 1;
		const HandModeFrame book = BookUp(false, leftHanded);
		Swept w = Sweep(book, s, 0.05f, 0.2f, -0.05f, 0.2f);
		Check(w.pages == 1 && w.wheel == 0 && !w.clicked,
		      leftHanded ? "left-handed, the book in the right hand: drawn left, one turn on"
		                 : "a book in the left hand: drawn left, one turn on");
		w = Sweep(book, s, 0.0f, 0.22f, 0.0f, 0.14f);
		Check(w.pages == 0 && w.wheel == 0 && !w.clicked,
		      leftHanded ? "left-handed: drawn down the book, nothing" : "drawn down the book: nothing");
		const HandModeFrame letter = BookUp(true, leftHanded);
		w = Sweep(letter, s, 0.0f, 0.22f, 0.0f, 0.14f);
		Check(w.wheel != 0 && w.pages == 0 && !w.clicked,
		      leftHanded ? "left-handed: drawn down the letter, it scrolls" : "drawn down the letter: it scrolls");
		w = Sweep(letter, s, 0.05f, 0.2f, -0.05f, 0.2f);
		Check(w.pages == 0 && !w.clicked,
		      leftHanded ? "left-handed: sideways on the letter, nothing" : "sideways on the letter: nothing");
	}

	// The right stick: a flick to the side is a turn, as on the pages.
	HandMode stick;
	HandModeFrame f = BookUp(false, false);
	f.right.position = NiPoint3{0.3f, -0.4f, 0.0f};  // nowhere near the glass
	f.right.thumbX = 0.9f;
	HandModeResult r = stick.Update(f, s);
	int turned = r.tabletPageStep;
	r = stick.Update(f, s);
	turned += r.tabletPageStep;
	Check(turned == 1, "the right stick flicked right: one turn on, once while held");
	f.right.thumbX = 0.0f;
	stick.Update(f, s);
	f.right.thumbX = -0.9f;
	r = stick.Update(f, s);
	Check(r.tabletPageStep == -1 && TabletTurnFor(TabletLook::Book, r.tabletPageStep).wheel == 1,
	      "flicked left: one back, a notch up");
}

}  // namespace

int main() {
	TestWhich();
	TestLook();
	TestTurn();
	TestWidth();
	TestFace();
	TestTouchModes();
	TestInHandMode();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
