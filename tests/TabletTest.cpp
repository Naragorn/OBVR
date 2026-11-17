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
	Check(c.valid && Near(c.at[0].y, 0.10f) && Near(c.at[2].y, 0.0f), "a quarter up: the top edge a quarter of the height above the grip");
	Check(Near(c.v[0], 0.8f) && Near(c.v[2], 1.0f), "showing the picture's lowest quarter, not the whole squeezed");
	c = CornersOf(place, crop, 2000.0f, 1000.0f, 3.0f);
	Check(c.valid && Near(c.at[0].y, 0.40f) && Near(c.v[0], 0.2f), "more than fully up is fully up");

	Check(!CornersOf(place, crop, 2000.0f, 1000.0f, 0.0f).valid, "nothing up yet: nothing to draw");
	Check(!CornersOf(TabletPlace{}, crop, 2000.0f, 1000.0f, 1.0f).valid, "no place: nothing to draw");
	Check(!CornersOf(place, UiRect{}, 2000.0f, 1000.0f, 1.0f).valid, "no crop: nothing to draw");
	Check(!CornersOf(place, crop, 0.0f, 1000.0f, 1.0f).valid && !CornersOf(place, crop, 2000.0f, 0.0f, 1.0f).valid,
	      "a texture without a size: nothing to draw");

	Check(TabletOpenShare(0.0f) == 0.0f && TabletOpenShare(-1.0f) == 0.0f, "not up yet: none of it");
	Check(Near(TabletOpenShare(0.5f * kTabletOpenSeconds), 0.5f), "half the time: half of it");
	Check(TabletOpenShare(kTabletOpenSeconds) == 1.0f && TabletOpenShare(5.0f) == 1.0f, "after the time: all of it");
}

}  // namespace

int main() {
	TestMenus();
	TestPaper();
	TestPopup();
	TestPlace();
	TestCorners();
	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
