// Checks the menus as a tablet in the off hand (vr/TabletLogic.h): which
// menus the tablet carries and when the stack is the tablet's, the page and
// the crop read from a menu's pictures, a popup joined, the place on the
// holding hand for either hand, and the corners and texture coordinates of
// the quad drawn into the scene as it rises.

#include <cstdio>

#include "vr/TabletLogic.h"

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

bool Near(float a, float b, float tolerance = 0.0005f) {
	const float d = a - b;
	return d < tolerance && d > -tolerance;
}

bool SameRect(const UiRect& r, float left, float top, float right, float bottom) {
	return r.valid && Near(r.left, left, 0.01f) && Near(r.top, top, 0.01f) && Near(r.right, right, 0.01f) &&
	       Near(r.bottom, bottom, 0.01f);
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

// The screen of the measured run: 960 units high at 16:9.
constexpr float kScreenWidth = 1706.7f;
constexpr float kScreenHeight = 960.0f;

// The vanilla page as read in the game (tablet-tiles.txt, 20261117-190432).
const PaperTile kPage{27.0f, 131.0f, 772.0f, 923.0f};

void TestMenus() {
	std::printf("Which menus:\n");
	const UInt32 carried[] = {game::kMenuIdBigFour,  game::kMenuIdInventory,  game::kMenuIdStats,
	                          game::kMenuIdMagic,    game::kMenuIdMap,        game::kMenuIdQuantity,
	                          game::kMenuIdMagicPopup, game::kMenuIdMessage,  game::kMenuIdBook};
	bool all = true;
	for (const UInt32 id : carried) {
		all = all && TabletCarriesMenu(id);
	}
	Check(all, "the four pages, their stack entry, the quantity box, the spell popup, a message and a book");
	const UInt32 others[] = {game::kMenuIdNone,      game::kMenuIdContainer, game::kMenuIdDialog,
	                         game::kMenuIdPause,     game::kMenuIdLockPick,  game::kMenuIdOptions,
	                         game::kMenuIdLevelUp,   game::kMenuIdSleepWait, game::kMenuIdMain,
	                         game::kMenuIdHudMain,   game::kMenuIdAlchemy,   game::kMenuIdRepair,
	                         game::kMenuIdLoading,   game::kMenuIdNegotiate, game::kMenuIdPersuasion};
	bool none = true;
	for (const UInt32 id : others) {
		none = none && !TabletCarriesMenu(id);
	}
	Check(none, "no other menu: a container, a conversation, the Esc menu, the lock, the HUD");

	Check(IsHudMenu(game::kMenuIdHudMain) && IsHudMenu(game::kMenuIdHudInfo) &&
	          IsHudMenu(game::kMenuIdHudReticle) && IsHudMenu(game::kMenuIdHudSubtitle),
	      "the HUD's four entries are the HUD's");
	Check(!IsHudMenu(game::kMenuIdBigFour) && !IsHudMenu(game::kMenuIdInventory), "a page is not");

	const UInt32 pages[] = {game::kMenuIdBigFour};
	Check(TabletMenusUp(pages, 1), "the pages alone in the stack: the tablet's");
	const UInt32 withHud[] = {game::kMenuIdHudMain, game::kMenuIdBigFour, game::kMenuIdHudReticle};
	Check(TabletMenusUp(withHud, 3), "the HUD's entries beside them change nothing");
	const UInt32 popup[] = {game::kMenuIdBigFour, game::kMenuIdQuantity};
	Check(TabletMenusUp(popup, 2), "the quantity box over the pages: still the tablet's");
	const UInt32 message[] = {game::kMenuIdBigFour, game::kMenuIdMessage};
	Check(TabletMenusUp(message, 2), "a message over the pages too");
	const UInt32 alone[] = {game::kMenuIdMessage};
	Check(!TabletMenusUp(alone, 1), "a message with no pages under it is not");
	const UInt32 container[] = {game::kMenuIdContainer, game::kMenuIdQuantity};
	Check(!TabletMenusUp(container, 2), "a container's quantity box is not");
	const UInt32 mixed[] = {game::kMenuIdBigFour, game::kMenuIdPause};
	Check(!TabletMenusUp(mixed, 2), "a menu the tablet does not carry over the pages takes them off it");
	const UInt32 dialog[] = {game::kMenuIdDialog, game::kMenuIdBigFour};
	Check(!TabletMenusUp(dialog, 2), "and so does one under them");
	Check(!TabletMenusUp(pages, 0), "an empty stack is nobody's");
	Check(!TabletMenusUp(nullptr, 3), "no stack at all neither");

	Check(TabletMenuRunsWorld(game::kMenuIdBigFour) && TabletMenuRunsWorld(game::kMenuIdInventory) &&
	          TabletMenuRunsWorld(game::kMenuIdMap) && TabletMenuRunsWorld(game::kMenuIdQuantity) &&
	          TabletMenuRunsWorld(game::kMenuIdMagicPopup),
	      "the world runs behind the pages and their two boxes");
	Check(!TabletMenuRunsWorld(game::kMenuIdMessage) && !TabletMenuRunsWorld(game::kMenuIdBook),
	      "a message and a book pause it as vanilla");
	Check(!TabletMenuRunsWorld(game::kMenuIdPause) && !TabletMenuRunsWorld(game::kMenuIdNone),
	      "and so does everything the tablet does not carry");
}

void TestPaper() {
	std::printf("The page and the crop:\n");
	// The inventory: the page, the tabs' strip (wider than what it shows), a
	// border and the paper's edge.
	const PaperTile inventory[] = {{50.0f, 673.0f, 841.0f, 849.0f},
	                               kPage,
	                               {137.0f, 196.0f, 733.0f, 356.0f},
	                               {727.0f, 131.0f, 781.0f, 923.0f}};
	TabletRects r = PaperOf(inventory, 4, kScreenWidth, kScreenHeight);
	Check(r.valid && SameRect(r.paper, 27.0f, 131.0f, 772.0f, 923.0f), "the largest picture is the page");
	Check(SameRect(r.crop, 27.0f, 131.0f, 772.0f, 923.0f),
	      "the tabs' strip, a border and an edge are too small to widen the crop");

	// The map: the page and its extension to the right.
	const PaperTile map[] = {kPage, {638.0f, 151.0f, 1205.0f, 764.0f}, {50.0f, 673.0f, 841.0f, 849.0f}};
	r = PaperOf(map, 3, kScreenWidth, kScreenHeight);
	Check(r.valid && SameRect(r.paper, 27.0f, 131.0f, 772.0f, 923.0f), "the map's page is the same page");
	Check(SameRect(r.crop, 27.0f, 131.0f, 1205.0f, 923.0f), "and its extension reaches the crop out to the right");

	// Exactly at the share, and just under it.
	const float pageArea = 745.0f * 792.0f;
	const float edge = kPaperJoinShare * pageArea / 792.0f;
	const PaperTile atShare[] = {kPage, {772.0f, 131.0f, 772.0f + edge + 0.01f, 923.0f}};
	r = PaperOf(atShare, 2, kScreenWidth, kScreenHeight);
	Check(r.crop.right > 772.0f, "a picture of the join share of the page joins");
	const PaperTile under[] = {kPage, {772.0f, 131.0f, 772.0f + edge - 1.0f, 923.0f}};
	r = PaperOf(under, 2, kScreenWidth, kScreenHeight);
	Check(Near(r.crop.right, 772.0f, 0.01f), "one just under it does not");

	// Cut to the screen.
	const PaperTile over[] = {{-100.0f, -50.0f, 900.0f, 1200.0f}};
	r = PaperOf(over, 1, kScreenWidth, kScreenHeight);
	Check(r.valid && SameRect(r.paper, 0.0f, 0.0f, 900.0f, 960.0f), "a picture reaching off the screen is cut to it");
	const PaperTile off[] = {{-500.0f, 100.0f, -10.0f, 800.0f}, {2000.0f, 0.0f, 2500.0f, 900.0f}};
	Check(!PaperOf(off, 2, kScreenWidth, kScreenHeight).valid, "pictures wholly off the screen are no page");
	const PaperTile flat[] = {{100.0f, 100.0f, 100.0f, 500.0f}, {100.0f, 100.0f, 500.0f, 100.0f},
	                          {500.0f, 500.0f, 100.0f, 100.0f}};
	Check(!PaperOf(flat, 3, kScreenWidth, kScreenHeight).valid, "pictures without a size neither");

	// Too small to be a page.
	const PaperTile icon[] = {{100.0f, 100.0f, 164.0f, 164.0f}};
	Check(!PaperOf(icon, 1, kScreenWidth, kScreenHeight).valid, "an icon alone is no page");
	const float least = kPaperLeastShare * kScreenWidth * kScreenHeight;
	const PaperTile small[] = {{0.0f, 0.0f, least / 100.0f + 0.5f, 100.0f}};
	Check(PaperOf(small, 1, kScreenWidth, kScreenHeight).valid, "a picture of the least share of the screen is");

	Check(!PaperOf(inventory, 0, kScreenWidth, kScreenHeight).valid, "no picture: no page");
	Check(!PaperOf(nullptr, 4, kScreenWidth, kScreenHeight).valid, "no list: no page");
	Check(!PaperOf(inventory, 4, 0.0f, kScreenHeight).valid && !PaperOf(inventory, 4, kScreenWidth, 0.0f).valid,
	      "a screen without a size: no page");

	Check(Near(RectArea(Rect(10.0f, 20.0f, 30.0f, 50.0f)), 600.0f) && RectArea(UiRect{}) == 0.0f,
	      "a rectangle's area, none for an invalid one");
}

void TestPopup() {
	std::printf("A popup over the page:\n");
	const PaperTile pageTiles[] = {kPage};
	TabletRects page = PaperOf(pageTiles, 1, kScreenWidth, kScreenHeight);
	// A message box: a small frame and a button, none of them a page.
	const PaperTile boxTiles[] = {{653.0f, 380.0f, 1053.0f, 580.0f}, {800.0f, 560.0f, 900.0f, 600.0f}};
	const UiRect box = PopupRect(boxTiles, 2, kScreenWidth, kScreenHeight);
	Check(SameRect(box, 653.0f, 380.0f, 1053.0f, 600.0f), "a popup's rectangle is all its pictures together");
	JoinPopup(page, box);
	Check(SameRect(page.crop, 27.0f, 131.0f, 1053.0f, 923.0f), "the crop grows to hold the box");
	Check(SameRect(page.paper, 27.0f, 131.0f, 772.0f, 923.0f), "the page stays the page");

	TabletRects again = PaperOf(pageTiles, 1, kScreenWidth, kScreenHeight);
	JoinPopup(again, UiRect{});
	Check(SameRect(again.crop, 27.0f, 131.0f, 772.0f, 923.0f), "a popup whose rectangle was not read changes nothing");
	TabletRects nothing;
	JoinPopup(nothing, box);
	Check(!nothing.valid && !nothing.crop.valid, "and no page stays no page");
	const PaperTile offScreen[] = {{-300.0f, 0.0f, -100.0f, 50.0f}, {1600.0f, 900.0f, 1900.0f, 1100.0f}};
	Check(SameRect(PopupRect(offScreen, 2, kScreenWidth, kScreenHeight), 1600.0f, 900.0f, kScreenWidth, 960.0f),
	      "a popup's pictures are cut to the screen, one wholly off it left out");
	Check(!PopupRect(boxTiles, 0, kScreenWidth, kScreenHeight).valid && !PopupRect(nullptr, 2, kScreenWidth, kScreenHeight).valid,
	      "a popup with no picture has no rectangle");

	const UiRect pixels = ToLayerPixels(page.paper, 2270.0f);
	const float scale = 2270.0f / 960.0f;
	Check(SameRect(pixels, 27.0f * scale, 131.0f * scale, 772.0f * scale, 923.0f * scale),
	      "HUD units to the layer's pixels by the layer's height over 960");
	Check(!ToLayerPixels(page.paper, 0.0f).valid && !ToLayerPixels(UiRect{}, 2270.0f).valid,
	      "no pixels without a layer height or a rectangle");
}

TabletFrame FrameOf(const UiRect& paper, const UiRect& crop) {
	TabletFrame f;
	f.up = true;
	f.paper = paper;
	f.crop = crop;
	return f;
}

void TestPlace() {
	std::printf("On the hand:\n");
	TabletSettings s;
	s.enabled = true;
	s.widthMetres = 0.40f;
	s.gripUp = 0.03f;
	s.gripForward = 0.02f;
	s.tiltDegrees = 0.0f;
	s.gripShare = 0.25f;
	const UiRect paper = Rect(100.0f, 200.0f, 900.0f, 1000.0f);  // 800 x 800 pixels: 0.0005 m a pixel

	TabletPlace p = PlaceTablet(FrameOf(paper, paper), s, true);
	Check(p.valid && Near(p.width, 0.40f) && Near(p.height, 0.40f), "the page as wide as the setting, its height by its shape");
	Check(Near(p.transform.m[0][0], 1.0f) && Near(p.transform.m[1][1], 1.0f) && Near(p.transform.m[2][2], 1.0f),
	      "upright with no tilt: the quad's axes are the controller's");
	// The grip a quarter along the lower edge from the left: the centre a
	// quarter of the width to the right, half the height above the grip.
	Check(Near(p.transform.m[0][3], 0.10f) && Near(p.transform.m[1][3], 0.03f + 0.20f) &&
	          Near(p.transform.m[2][3], -0.02f),
	      "held by the left hand: the grip a quarter along the lower edge, the page to the right of it");

	p = PlaceTablet(FrameOf(paper, paper), s, false);
	Check(Near(p.transform.m[0][3], -0.10f) && Near(p.transform.m[1][3], 0.23f),
	      "held by the right hand: mirrored, the page to the left of it");

	// Leant back by 30 degrees: up is (0, cos, -sin), front (0, sin, cos).
	s.tiltDegrees = 30.0f;
	p = PlaceTablet(FrameOf(paper, paper), s, true);
	Check(Near(p.transform.m[1][1], 0.8660f) && Near(p.transform.m[2][1], -0.5f), "leant back: its up rises and goes ahead");
	Check(Near(p.transform.m[1][2], 0.5f) && Near(p.transform.m[2][2], 0.8660f), "its front faces up and the wrist");
	Check(Near(p.transform.m[1][3], 0.03f + 0.8660f * 0.20f) && Near(p.transform.m[2][3], -0.02f - 0.5f * 0.20f),
	      "the centre half the height along that up from the grip");

	// A crop wider than the page: the same scale, the quad reaching right.
	s.tiltDegrees = 0.0f;
	const UiRect wide = Rect(100.0f, 200.0f, 1300.0f, 1000.0f);
	p = PlaceTablet(FrameOf(paper, wide), s, true);
	Check(Near(p.width, 0.60f) && Near(p.height, 0.40f), "a wider crop: the quad wider, the page's scale kept");
	Check(Near(p.transform.m[0][3], 0.10f + 0.10f), "its centre moved half the extension to the right");
	// A crop taller than the page, above it.
	const UiRect tall = Rect(100.0f, 0.0f, 900.0f, 1000.0f);
	p = PlaceTablet(FrameOf(paper, tall), s, true);
	Check(Near(p.height, 0.50f) && Near(p.transform.m[1][3], 0.03f + 0.25f), "a taller crop reaches up, the lower edge stays at the grip");

	TabletFrame f = FrameOf(paper, paper);
	f.up = false;
	Check(!PlaceTablet(f, s, true).valid, "the tablet's menus not up: no place");
	Check(TabletSettings{}.enabled, "on by default (Nadi, 2026-11-17); the traditional inventory is the switch");
	TabletSettings off = s;
	off.enabled = false;
	Check(!PlaceTablet(FrameOf(paper, paper), off, true).valid, "the tablet off: no place");
	TabletSettings noWidth = s;
	noWidth.widthMetres = 0.0f;
	Check(!PlaceTablet(FrameOf(paper, paper), noWidth, true).valid, "no width: no place");
	Check(!PlaceTablet(FrameOf(UiRect{}, paper), s, true).valid && !PlaceTablet(FrameOf(paper, UiRect{}), s, true).valid,
	      "a page or a crop that was not read: no place");
	UiRect thin = Rect(100.0f, 200.0f, 100.0f, 1000.0f);
	Check(!PlaceTablet(FrameOf(thin, paper), s, true).valid, "a page without a width: no place");
	thin = Rect(100.0f, 200.0f, 900.0f, 200.0f);
	Check(!PlaceTablet(FrameOf(paper, thin), s, true).valid, "a crop without a height: no place");
	thin = Rect(100.0f, 200.0f, 100.0f, 1000.0f);
	Check(!PlaceTablet(FrameOf(paper, thin), s, true).valid, "a crop without a width: no place");
}

void TestCorners() {
	std::printf("Into the scene:\n");
	TabletSettings s;
	s.enabled = true;
	s.widthMetres = 0.40f;
	s.gripUp = 0.0f;
	s.gripForward = 0.0f;
	s.tiltDegrees = 0.0f;
	s.gripShare = 0.5f;
	const UiRect crop = Rect(100.0f, 200.0f, 900.0f, 1000.0f);
	const TabletPlace place = PlaceTablet(FrameOf(crop, crop), s, true);

	TabletCorners c = CornersOf(place, crop, 2000.0f, 1000.0f, 1.0f);
	Check(c.valid && Near(c.at[0].x, -0.20f) && Near(c.at[0].y, 0.40f) && Near(c.at[1].x, 0.20f) &&
	          Near(c.at[1].y, 0.40f),
	      "fully up: the top corners a height above the grip, half the width to each side");
	Check(Near(c.at[2].x, -0.20f) && Near(c.at[2].y, 0.0f) && Near(c.at[3].x, 0.20f) && Near(c.at[3].y, 0.0f),
	      "the bottom corners at the grip's height");
	Check(Near(c.u[0], 0.05f) && Near(c.u[1], 0.45f) && Near(c.u[2], 0.05f) && Near(c.u[3], 0.45f),
	      "the texture's columns are the crop's over the texture's width");
	Check(Near(c.v[0], 0.2f) && Near(c.v[1], 0.2f) && Near(c.v[2], 1.0f) && Near(c.v[3], 1.0f),
	      "and its rows the crop's over the texture's height");

	c = CornersOf(place, crop, 2000.0f, 1000.0f, 0.25f);
	Check(c.valid && Near(c.at[0].x, -0.05f) && Near(c.at[1].x, 0.05f) && Near(c.at[0].y, 0.40f),
	      "a quarter unrolled: the middle quarter's width, the full height");
	Check(Near(c.u[0], 0.2f) && Near(c.u[1], 0.3f) && Near(c.v[0], 0.2f),
	      "showing the picture's middle quarter, not the whole squeezed");
	c = CornersOf(place, crop, 2000.0f, 1000.0f, 3.0f);
	Check(c.valid && Near(c.at[0].y, 0.40f) && Near(c.v[0], 0.2f), "more than fully up is fully up");

	Check(!CornersOf(place, crop, 2000.0f, 1000.0f, 0.0f).valid, "nothing up yet: nothing to draw");
	Check(!CornersOf(TabletPlace{}, crop, 2000.0f, 1000.0f, 1.0f).valid, "no place: nothing to draw");
	Check(!CornersOf(place, UiRect{}, 2000.0f, 1000.0f, 1.0f).valid, "no crop: nothing to draw");
	Check(!CornersOf(place, crop, 0.0f, 1000.0f, 1.0f).valid && !CornersOf(place, crop, 2000.0f, 0.0f, 1.0f).valid,
	      "a texture without a size: nothing to draw");

}

void TestFigurePage() {
	std::printf("The figure's own page:\n");
	TabletFrame f;
	f.up = true;
	f.paper = Rect(0.0f, 0.0f, 1000.0f, 1000.0f);
	f.pageCrop = f.paper;
	f.crop = f.paper;
	// A figure 400 wide and 1200 tall: taller than the page's room of 920.
	SetFigurePage(f, Rect(1500.0f, -100.0f, 1900.0f, 1100.0f));
	const float scale = 920.0f / 1200.0f;
	Check(f.figurePage && Near(f.figureScale, scale), "a figure taller than the page is made smaller to fit with a gap");
	Check(SameRect(f.figureAt, 1040.0f, 40.0f, 1040.0f + 400.0f * scale, 960.0f),
	      "it stands a gap in from the page's edge, centred up and down");
	Check(SameRect(f.backing, 1000.0f, 0.0f, 1040.0f + 400.0f * scale + 40.0f, 1000.0f),
	      "its page abuts the page, as high, a gap past the figure");
	Check(SameRect(f.crop, 0.0f, 0.0f, f.backing.right, 1000.0f), "the tablet's face holds both pages");

	// A short figure keeps its size.
	TabletFrame g = TabletFrame{};
	g.up = true;
	g.paper = Rect(0.0f, 0.0f, 1000.0f, 1000.0f);
	g.pageCrop = Rect(0.0f, -50.0f, 1000.0f, 1000.0f);  // a popup reaching above the page
	SetFigurePage(g, Rect(1500.0f, 300.0f, 1700.0f, 700.0f));
	Check(g.figurePage && g.figureScale == 1.0f && SameRect(g.figureAt, 1040.0f, 300.0f, 1240.0f, 700.0f),
	      "a figure that fits keeps its size");
	Check(SameRect(g.crop, 0.0f, -50.0f, 1280.0f, 1000.0f), "the face holds the page's popups too");

	TabletFrame none;
	SetFigurePage(none, Rect(1500.0f, 300.0f, 1700.0f, 700.0f));
	Check(!none.figurePage, "the tablet not up: no figure page");
	TabletFrame noFigure = f;
	noFigure.figurePage = false;
	SetFigurePage(noFigure, UiRect{});
	Check(!noFigure.figurePage, "no figure read: no figure page");
	TabletFrame thin = TabletFrame{};
	thin.up = true;
	thin.paper = Rect(0.0f, 0.0f, 1000.0f, 50.0f);  // no room for a gap above and below
	SetFigurePage(thin, Rect(1500.0f, 300.0f, 1700.0f, 700.0f));
	Check(!thin.figurePage, "a page with no room: no figure page");
	TabletFrame flat = TabletFrame{};
	flat.up = true;
	flat.paper = Rect(0.0f, 0.0f, 1000.0f, 1000.0f);
	SetFigurePage(flat, Rect(1500.0f, 300.0f, 1500.0f, 700.0f));
	Check(!flat.figurePage, "a figure without a width: no figure page");

	std::printf("A point on the face to the layer:\n");
	float x = 500.0f;
	float y = 500.0f;
	Check(SurfaceToLayer(f, x, y) && x == 500.0f && y == 500.0f, "on the page: the point itself");
	x = 1040.0f + 200.0f * scale;
	y = 500.0f;
	Check(SurfaceToLayer(f, x, y) && Near(x, 1700.0f, 0.05f) && Near(y, -100.0f + (500.0f - 40.0f) / scale, 0.05f),
	      "on the figure: where the game draws that part of it");
	x = 1020.0f;
	y = 500.0f;
	Check(!SurfaceToLayer(f, x, y), "on the figure's page beside the figure: nothing");
	x = 1100.0f;
	y = 10.0f;
	Check(!SurfaceToLayer(f, x, y), "above the figure on its page: nothing");
	TabletFrame plain;
	x = 1100.0f;
	y = 10.0f;
	Check(SurfaceToLayer(plain, x, y) && x == 1100.0f, "no figure page: every point itself");
	TabletFrame zero = f;
	zero.figureScale = 0.0f;
	x = 1100.0f;
	y = 500.0f;
	Check(!SurfaceToLayer(zero, x, y), "a figure of no size maps nowhere");

	std::printf("A rectangle of the face:\n");
	TabletSettings s;
	s.enabled = true;
	s.widthMetres = 0.40f;
	s.gripUp = 0.0f;
	s.gripForward = 0.0f;
	s.tiltDegrees = 0.0f;
	s.gripShare = 0.5f;
	TabletFrame frame;
	frame.up = true;
	frame.paper = Rect(0.0f, 0.0f, 800.0f, 800.0f);
	frame.crop = Rect(0.0f, 0.0f, 1600.0f, 800.0f);
	const TabletPlace place = PlaceTablet(frame, s, true);
	TabletCorners c = RectOnTablet(place, frame.crop, Rect(800.0f, 0.0f, 1600.0f, 800.0f), UiRect{}, 2000.0f,
	                               1000.0f, 1.0f);
	Check(c.valid && Near(c.at[0].x, 0.20f) && Near(c.at[1].x, 0.60f) && Near(c.at[0].y, 0.40f) &&
	          Near(c.at[2].y, 0.0f),
	      "the right half of the face: from the page's edge out, the full height");
	Check(c.u[0] == 0.0f && c.v[0] == 0.0f, "a plain quad carries no texture coordinates");
	c = RectOnTablet(place, frame.crop, Rect(800.0f, 0.0f, 1600.0f, 800.0f), Rect(1000.0f, 100.0f, 1400.0f, 900.0f),
	                 2000.0f, 1000.0f, 0.5f);
	Check(c.valid && Near(c.at[0].x, 0.20f) && Near(c.at[1].x, 0.40f) && Near(c.u[0], 0.5f) && Near(c.u[1], 0.6f) &&
	          Near(c.v[0], 0.1f) && Near(c.v[2], 0.9f),
	      "half unrolled: the rectangle cut at the open part's edge, its picture cut alike");
	c = RectOnTablet(place, frame.crop, Rect(1300.0f, 0.0f, 1600.0f, 800.0f), UiRect{}, 2000.0f, 1000.0f, 0.5f);
	Check(!c.valid, "a rectangle wholly beyond what is open is not drawn");
	const UiRect rollRight = RollStrip(frame.crop, 0.5f, true, 800.0f);
	Check(SameRect(rollRight, 1200.0f - 20.0f, -24.0f, 1200.0f + 20.0f, 824.0f),
	      "the right roll at the open part's right edge, a little taller than the face");
	Check(SameRect(RollStrip(frame.crop, 0.5f, false, 800.0f), 400.0f - 20.0f, -24.0f, 400.0f + 20.0f, 824.0f),
	      "the left one at its left edge");
	Check(!RollStrip(frame.crop, 1.0f, true, 800.0f).valid && !RollStrip(frame.crop, 0.0f, true, 800.0f).valid &&
	          !RollStrip(UiRect{}, 0.5f, true, 800.0f).valid,
	      "fully open, rolled up, or no face: no roll");
	Check(!RectOnTablet(place, frame.crop, UiRect{}, UiRect{}, 2000.0f, 1000.0f, 1.0f).valid,
	      "no rectangle: nothing");
	Check(!RectOnTablet(place, Rect(0.0f, 0.0f, 0.0f, 800.0f), frame.crop, UiRect{}, 2000.0f, 1000.0f, 1.0f).valid,
	      "a face without a width: nothing");
	Check(!RectOnTablet(place, frame.crop, Rect(900.0f, 0.0f, 900.0f, 800.0f), UiRect{}, 2000.0f, 1000.0f, 1.0f).valid,
	      "a rectangle without a width: nothing");
}

void TestFigureTab() {
	std::printf("The figure behind a tab:\n");
	TabletFrame f;
	f.up = true;
	f.paper = Rect(100.0f, 200.0f, 1100.0f, 1200.0f);
	f.pageCrop = f.paper;
	f.crop = f.paper;
	// A figure 500 wide, 1000 tall: the page's room is 920 by 920.
	SetFigureTab(f, Rect(2000.0f, 100.0f, 2500.0f, 1100.0f));
	Check(f.figureTab && Near(f.figureFitScale, 0.92f), "fitted to the page's height with a gap");
	Check(SameRect(f.figureFit, 600.0f - 230.0f, 240.0f, 600.0f + 230.0f, 1160.0f), "centred on the page");
	Check(SameRect(f.tabs[0], 150.0f, 110.0f, 430.0f, 200.0f) && SameRect(f.tabs[1], 480.0f, 110.0f, 760.0f, 200.0f),
	      "two tabs on the page's top edge, the page's first");
	Check(SameRect(f.crop, 100.0f, 110.0f, 1100.0f, 1200.0f), "the face grows up by the tabs, not wider");
	TabletFrame wide;
	wide.up = true;
	wide.paper = Rect(0.0f, 0.0f, 1000.0f, 1000.0f);
	SetFigureTab(wide, Rect(0.0f, 0.0f, 2000.0f, 500.0f));
	Check(wide.figureTab && Near(wide.figureFitScale, 0.46f), "a wide figure fitted by the page's width");
	TabletFrame none;
	SetFigureTab(none, Rect(0.0f, 0.0f, 10.0f, 10.0f));
	Check(!none.figureTab, "the tablet not up: no tabs");
	TabletFrame noFigure = wide;
	noFigure.figureTab = false;
	SetFigureTab(noFigure, UiRect{});
	Check(!noFigure.figureTab, "no figure: no tabs");
	TabletFrame tiny;
	tiny.up = true;
	tiny.paper = Rect(0.0f, 0.0f, 1000.0f, 60.0f);
	SetFigureTab(tiny, Rect(0.0f, 0.0f, 10.0f, 10.0f));
	Check(!tiny.figureTab, "a page with no room: no tabs");

	Check(TabletTabAt(f, 200.0f, 150.0f) == 0 && TabletTabAt(f, 600.0f, 150.0f) == 1, "each tab found");
	Check(TabletTabAt(f, 455.0f, 150.0f) == -1 && TabletTabAt(f, 200.0f, 300.0f) == -1, "between them, or on the page: none");
	Check(TabletTabAt(TabletFrame{}, 200.0f, 150.0f) == -1, "no tabs: none");

	float x = 600.0f;
	float y = 700.0f;
	Check(FigureTabToLayer(f, x, y) && Near(x, 2250.0f, 0.5f) && Near(y, 100.0f + 460.0f / 0.92f, 0.5f),
	      "the figure's tab chosen: a point on the figure is the figure where the game draws it");
	x = 200.0f;
	y = 700.0f;
	Check(!FigureTabToLayer(f, x, y), "on the page's rectangle beside the figure: nothing");
	x = 200.0f;
	y = 150.0f;
	Check(!FigureTabToLayer(f, x, y), "on a tab: nothing of the game's");
	x = 1300.0f;
	y = 700.0f;
	Check(FigureTabToLayer(f, x, y) && x == 1300.0f, "off the page (a popup's place): the point itself");
	TabletFrame plain;
	x = 200.0f;
	y = 700.0f;
	Check(FigureTabToLayer(plain, x, y) && x == 200.0f, "no tabs: every point itself");

	std::printf("Choosing a tab:\n");
	TabletTabState s;
	Check(!StepTabletTab(s, true, 1, false, false), "the page shown first");
	Check(StepTabletTab(s, true, 1, true, false), "a click on the figure's tab shows the figure");
	Check(StepTabletTab(s, true, -1, true, false), "held off the tab: still the figure");
	Check(StepTabletTab(s, true, -1, false, false), "released: still the figure");
	Check(!StepTabletTab(s, true, -1, false, true), "a popup over the page: the page");
	Check(StepTabletTab(s, true, -1, false, false), "the popup gone: the figure again");
	Check(StepTabletTab(s, true, 0, false, false), "hovering the page's tab without a click: nothing changes");
	Check(!StepTabletTab(s, true, 0, true, false), "a click on the page's tab: the page");
	Check(!StepTabletTab(s, true, 1, true, false), "a click held over from before is no new click");
	StepTabletTab(s, true, 1, false, false);
	Check(StepTabletTab(s, true, 1, true, false), "a new click on the figure's tab: the figure");
	Check(!StepTabletTab(s, false, 1, true, false) && !s.showFigure, "no tabs on the tablet: the page, and forgotten");

	std::printf("The figure setting's words:\n");
	TabletFigure figure = TabletFigure::Beside;
	Check(ParseTabletFigure("TAB", figure) && figure == TabletFigure::Tab, "a word, any case");
	Check(ParseTabletFigure("off", figure) && figure == TabletFigure::Off, "off");
	Check(!ParseTabletFigure("left", figure) && figure == TabletFigure::Off, "another word: kept");
	Check(TabletFigureFromIndex(2.0f) == TabletFigure::Tab && TabletFigureFromIndex(0.0f) == TabletFigure::Beside &&
	          TabletFigureFromIndex(1.2f) == TabletFigure::Off,
	      "the settings row's value");
	Check(TabletFigureFromIndex(5.0f) == TabletFigure::Tab && TabletFigureFromIndex(-1.0f) == TabletFigure::Tab,
	      "out of range: the default, the tab");
	Check(TabletSettings{}.figure == TabletFigure::Tab, "the tab by default");
}

}  // namespace

int main() {
	TestMenus();
	TestPaper();
	TestPopup();
	TestPlace();
	TestCorners();
	TestFigurePage();
	TestFigureTab();
	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
