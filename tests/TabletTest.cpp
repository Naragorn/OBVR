// Checks the menus as a tablet in the off hand (vr/TabletLogic.h): which
// menus the tablet carries and when the stack is the tablet's, the page and
// the crop read from a menu's pictures, a popup joined, the place on the
// holding hand for either hand, and the corners and texture coordinates of
// the quad drawn into the scene as it rises.

#include <cmath>
#include <cstdio>

#include "vr/TabletLogic.h"
#include "vr/TabletPaper.h"
#include "vr/TabletTabStrip.h"

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
	Check(!TabletMenuRunsWorld(game::kMenuIdMessage) && TabletMenuRunsWorld(game::kMenuIdBook),
	      "a message pauses it as vanilla, a book runs it (tablet_book_test)");
	Check(!TabletMenuRunsWorld(game::kMenuIdPause) && !TabletMenuRunsWorld(game::kMenuIdNone),
	      "and so does everything the tablet does not carry");

	// A box of the Generic kind over the pages - the tutorial's help box (qadd_*,
	// Nadi's run of 2026-11-22: it came up over the stats page and had to be
	// looked for in the room) - is carried, and pauses the world as a message.
	Check(TabletCarriesMenu(game::kMenuIdGeneric), "a Generic box is carried");
	const UInt32 generic[] = {game::kMenuIdBigFour, game::kMenuIdGeneric};
	Check(TabletMenusUp(generic, 2), "a Generic box over the pages: on the tablet");
	const UInt32 genericAlone[] = {game::kMenuIdGeneric};
	Check(!TabletMenusUp(genericAlone, 1), "a Generic box with no pages under it: its own panel");
	Check(!TabletMenuRunsWorld(game::kMenuIdGeneric), "and it pauses the world, as a message does");

	// The menus whose pictures join the face: the stack's but the pages' entry
	// and the HUD's, and the item info (MagicPopup) whether stacked or not.
	UInt32 out[8] = {};
	const UInt32 withBox[] = {game::kMenuIdHudMain, game::kMenuIdBigFour, game::kMenuIdQuantity};
	UInt32 n = TabletFaceMenus(withBox, 3, out, 8);
	Check(n == 2 && out[0] == game::kMenuIdQuantity && out[1] == game::kMenuIdMagicPopup,
	      "the quantity box, and the item info the hover shows beside the page");
	const UInt32 stacked[] = {game::kMenuIdBigFour, game::kMenuIdMagicPopup};
	n = TabletFaceMenus(stacked, 2, out, 8);
	Check(n == 1 && out[0] == game::kMenuIdMagicPopup, "the item info in the stack: once");
	n = TabletFaceMenus(withBox, 3, out, 1);
	Check(n == 1 && out[0] == game::kMenuIdQuantity, "no room past the capacity");
	Check(TabletFaceMenus(nullptr, 0, out, 8) == 1 && TabletFaceMenus(withBox, 3, nullptr, 8) == 0,
	      "no stack: the item info alone; nowhere to write: none");
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
	Check(page.popupCount == 1 && !page.popupInfo[0], "kept as a box");
	JoinPopup(page, Rect(772.0f, 300.0f, 1150.0f, 360.0f), true);
	Check(page.popupCount == 2 && page.popupInfo[1], "an item info joined as an info");

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

// The stats page's own info box (Nadi, 2026-11-25: "die infobox erscheint im
// stats tab für ein paar Einträge du hast die nur abgeschnitten"): a picture
// of the stats menu itself, slid out beside the page - not the MagicPopup
// menu. Read as one more picture of the page it was under the join share and
// stayed out of the face; a tall one joined the page's outline.
void TestOwnPopup() {
	std::printf("The page's own info box:\n");
	Check(PagePopupTileId(game::kMenuIdStats) == 61, "the stats menu's is the tile the engine knows by id 61");
	Check(PagePopupTileId(game::kMenuIdInventory) == 0 && PagePopupTileId(game::kMenuIdMagic) == 0 &&
	          PagePopupTileId(game::kMenuIdMap) == 0 && PagePopupTileId(game::kMenuIdMagicPopup) == 0,
	      "the other pages have none of their own");
	// As the game lays it out at rest: 419 wide from 20 inside the page's
	// right edge, its corner 2 beyond; the tabs' strip is the page's.
	const PaperTile box{752.0f, 170.0f, 1171.0f, 620.0f, true};
	const PaperTile corner{1163.0f, 170.0f, 1173.0f, 180.0f, true};
	const PaperTile icon{793.0f, 200.0f, 1049.0f, 456.0f, true};
	const PaperTile stats[] = {{50.0f, 673.0f, 841.0f, 849.0f}, kPage, box, corner, icon};
	TabletRects r = PaperOf(stats, 5, kScreenWidth, kScreenHeight);
	Check(r.valid && SameRect(r.paper, 27.0f, 131.0f, 772.0f, 923.0f), "the page stays the page");
	Check(SameRect(r.outline, 27.0f, 131.0f, 772.0f, 923.0f), "its outline without the box");
	Check(r.popupCount == 1 && SameRect(r.popups[0], 752.0f, 170.0f, 1173.0f, 620.0f),
	      "the box kept as a popup: all its pictures together");
	Check(r.popupInfo[0], "an info, shown while a row is pointed at - not a box");
	Check(SameRect(r.crop, 27.0f, 131.0f, 1173.0f, 923.0f), "and the face grown to hold it - shown whole");

	// What the read gave before: the same pictures, none told apart.
	PaperTile plain[5];
	for (int i = 0; i < 5; ++i) {
		plain[i] = stats[i];
		plain[i].popup = false;
	}
	r = PaperOf(plain, 5, kScreenWidth, kScreenHeight);
	Check(r.popupCount == 0 && Near(r.crop.right, 772.0f, 0.01f),
	      "(not told apart, a box under the join share was cut off at the page's edge)");

	// A long description: the box taller, over the join share.
	const PaperTile tallBox{752.0f, 170.0f, 1171.0f, 800.0f, true};
	Check(RectArea(OnScreen(tallBox, kScreenWidth, kScreenHeight)) > kPaperJoinShare * 745.0f * 792.0f,
	      "(a tall box is over the join share)");
	const PaperTile tall[] = {kPage, tallBox};
	r = PaperOf(tall, 2, kScreenWidth, kScreenHeight);
	Check(SameRect(r.outline, 27.0f, 131.0f, 772.0f, 923.0f) && r.popupCount == 1 &&
	          SameRect(r.popups[0], 752.0f, 170.0f, 1171.0f, 800.0f),
	      "a tall box is a popup too, no part of the page's outline");

	// Sliding out from behind the page: still under it.
	const PaperTile under[] = {kPage, {340.0f, 170.0f, 759.0f, 620.0f, true}};
	r = PaperOf(under, 2, kScreenWidth, kScreenHeight);
	Check(r.popupCount == 1 && SameRect(r.crop, 27.0f, 131.0f, 772.0f, 923.0f), "still behind the page: the face as it is");

	const PaperTile huge[] = {kPage, {0.0f, 0.0f, 1700.0f, 960.0f, true}};
	r = PaperOf(huge, 2, kScreenWidth, kScreenHeight);
	Check(r.valid && SameRect(r.paper, 27.0f, 131.0f, 772.0f, 923.0f), "a box larger than the page is not the page");
	const PaperTile alone[] = {box, icon};
	Check(!PaperOf(alone, 2, kScreenWidth, kScreenHeight).valid, "a box without a page: no page");
	const PaperTile offScreen[] = {kPage, {2000.0f, 100.0f, 2400.0f, 500.0f, true}};
	r = PaperOf(offScreen, 2, kScreenWidth, kScreenHeight);
	Check(r.valid && r.popupCount == 0 && Near(r.crop.right, 772.0f, 0.01f), "a box wholly off the screen: none");
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

NiPoint3 Column(const TabletPlace& p, int c) {
	return NiPoint3{p.transform.m[0][c], p.transform.m[1][c], p.transform.m[2][c]};
}

bool NearPoint(const NiPoint3& a, const NiPoint3& b, float tolerance = 0.001f) {
	return Near(a.x, b.x, tolerance) && Near(a.y, b.y, tolerance) && Near(a.z, b.z, tolerance);
}

bool SamePlace(const TabletPlace& a, const TabletPlace& b) {
	for (int c = 0; c < 4; ++c) {
		if (!NearPoint(Column(a, c), Column(b, c))) {
			return false;
		}
	}
	return a.valid == b.valid && Near(a.width, b.width) && Near(a.height, b.height);
}

NiPoint3 CrossOf(const NiPoint3& a, const NiPoint3& b) {
	return NiPoint3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

// Across the controller's own y-z plane: OpenVR's two controllers are each
// other's mirror image there.
NiPoint3 MirrorX(const NiPoint3& v) { return NiPoint3{-v.x, v.y, v.z}; }

// Turned about the controller's y by `degrees`.
NiPoint3 TurnAboutY(const NiPoint3& v, float degrees) {
	const float r = degrees * 3.14159265f / 180.0f;
	const float c = std::cos(r);
	const float s = std::sin(r);
	return NiPoint3{v.x * c + v.z * s, v.y, -v.x * s + v.z * c};
}

// Held like the sword (Nadi, 2026-11-24: "das inv soll wie das schwert
// ausgerichtet sein"): the page's up is the blade's way out of the fist, its
// face the blade's flat on the palm's side, so it is read with the palm to
// the eyes. The axes are the hand's held-item node's: its y the blade, its x
// towards the knuckles.
void TestHeldLikeTheSword() {
	std::printf("Held like the sword:\n");
	TabletSettings s;
	s.enabled = true;
	s.widthMetres = 0.40f;
	s.gripUp = 0.03f;
	s.gripForward = 0.02f;
	s.tiltDegrees = 30.0f;
	s.gripShare = 0.25f;
	s.held.turnDegrees = 0.0f;  // the sword's own flat; the turn: TestHeldTurnedAndCentred
	s.held.leanDegrees = s.held.rollDegrees = s.held.acrossCm = s.held.outCm = 0.0f;
	const UiRect paper = Rect(100.0f, 200.0f, 900.0f, 1000.0f);  // 800 x 800 pixels: 0.0005 m a pixel
	const TabletFrame f = FrameOf(paper, paper);
	// The left fist round its controller, the palm at +x against the handle:
	// the blade up and ahead out of the thumb's side, the knuckles down and
	// ahead along the handle - turned a little, so no axis is the controller's.
	HeldAxes left;
	left.valid = true;
	left.blade = TurnAboutY(NiPoint3{0.0f, 0.6f, -0.8f}, 15.0f);
	left.edge = TurnAboutY(NiPoint3{0.0f, -0.8f, -0.6f}, 15.0f);
	const NiPoint3 palm{-0.04f, 0.01f, 0.13f};
	const NiPoint3 palmSide = CrossOf(left.edge, left.blade);  // the node's z on the left hand: out of the palm
	Check(palmSide.x > 0.9f, "(the test's hand: knuckles by blade points out of the palm, to the handle)");
	// The hand's model not read here: the page an open hand's reach before
	// the palm (tablet_on_hand_test has the reach read).
	const NiPoint3 out = palmSide * PageLiftMetres(HandReach{}, true);

	TabletPlace p = PlaceTablet(f, s, true, palm, TabletHold::Side, left);
	Check(p.valid && NearPoint(Column(p, 1), left.blade), "the page's up is the blade's way out of the fist");
	Check(NearPoint(Column(p, 0), left.edge), "its right towards the knuckles - to the body's middle, the palm to the eyes");
	Check(NearPoint(Column(p, 2), palmSide), "its face the blade's flat on the palm's side");
	Check(NearPoint(Column(p, 3), palm + out), "its middle before the palm's (section 24.4: no longer at its left edge)");
	Check(Near(p.width, 0.40f) && Near(p.height, 0.40f), "its size as before");

	s.tiltDegrees = 0.0f;
	Check(SamePlace(PlaceTablet(f, s, true, palm, TabletHold::Side, left), p),
	      "the tilt is not the sword's: in the hand it changes nothing");
	s.tiltDegrees = 30.0f;

	p = PlaceTablet(f, s, true, palm, TabletHold::Bottom, left);
	Check(NearPoint(Column(p, 1), left.blade) && NearPoint(Column(p, 0), left.edge) && NearPoint(Column(p, 3), palm + out),
	      "the scroll's bottom hold: the same way round, its middle before the palm - no clipboard offsets");
	p = PlaceTablet(f, s, true, palm, TabletHold::Top, left);
	Check(NearPoint(Column(p, 3), palm + out), "and its top hold");

	// The right hand: the mirror image of the left, the page read the right
	// way round - its right turned back, its up and face mirrored.
	HeldAxes right;
	right.valid = true;
	right.blade = MirrorX(left.blade);
	right.edge = MirrorX(left.edge);
	const TabletHold holds[3] = {TabletHold::Side, TabletHold::Bottom, TabletHold::Top};
	bool mirrored = true;
	for (const TabletHold hold : holds) {
		const TabletPlace l = PlaceTablet(f, s, true, palm, hold, left);
		const TabletPlace r = PlaceTablet(f, s, false, MirrorX(palm), hold, right);
		mirrored = mirrored && r.valid && NearPoint(Column(r, 0), MirrorX(Column(l, 0)) * -1.0f) &&
		           NearPoint(Column(r, 1), MirrorX(Column(l, 1))) && NearPoint(Column(r, 2), MirrorX(Column(l, 2))) &&
		           NearPoint(Column(r, 3), MirrorX(Column(l, 3)));
	}
	Check(mirrored, "in the right hand, every hold: the left's mirror image, read the right way round");
	p = PlaceTablet(f, s, false, MirrorX(palm), TabletHold::Side, right);
	Check(NearPoint(CrossOf(Column(p, 0), Column(p, 1)), Column(p, 2)) &&
	          NearPoint(Column(p, 2), CrossOf(right.edge, right.blade) * -1.0f),
	      "its face by right and up, on the right palm's side (the weapon node's z is the back's)");

	// The axes as the bones give them: eased, so not quite square or unit.
	HeldAxes rough = left;
	rough.blade = left.blade * 0.9f;
	rough.edge = left.edge * 1.1f + left.blade * 0.3f;
	p = PlaceTablet(f, s, true, palm, TabletHold::Side, rough);
	Check(NearPoint(Column(p, 1), left.blade) && NearPoint(Column(p, 0), left.edge) && NearPoint(Column(p, 2), palmSide),
	      "read a little long, short or skew: squared to the blade, unit");

	const TabletPlace before = PlaceTablet(f, s, true, palm, TabletHold::Side);
	HeldAxes none = left;
	none.valid = false;
	Check(SamePlace(PlaceTablet(f, s, true, palm, TabletHold::Side, none), before),
	      "the node not read: on the controller as before");
	HeldAxes flat = left;
	flat.edge = left.blade;
	Check(SamePlace(PlaceTablet(f, s, true, palm, TabletHold::Side, flat), before),
	      "the knuckles along the blade: no flat to face, as before");
	HeldAxes nothing = left;
	nothing.blade = NiPoint3{0.0f, 0.0f, 0.0f};
	Check(SamePlace(PlaceTablet(f, s, true, palm, TabletHold::Side, nothing), before), "no blade: as before");

	// Kept from frame to frame like the palm (vr/FingerTip.h's ease).
	HeldAxes kept;
	StepHeldAxes(kept, false, left.blade, left.edge);
	Check(!kept.valid, "not read: none");
	StepHeldAxes(kept, true, left.blade, left.edge);
	Check(kept.valid && NearPoint(kept.blade, left.blade) && NearPoint(kept.edge, left.edge), "the first reading taken");
	StepHeldAxes(kept, true, right.blade, right.edge);
	Check(NearPoint(kept.blade, left.blade + (right.blade - left.blade) * kHeldAxesEase) &&
	          NearPoint(kept.edge, left.edge + (right.edge - left.edge) * kHeldAxesEase),
	      "then eased towards the new one by the palm's share");
	StepHeldAxes(kept, false, left.blade, left.edge);
	Check(!kept.valid, "no longer read: none");

	TabletFrame both = f;
	both.held[0] = left;
	both.held[1] = right;
	Check(NearPoint(TabletHeldAxes(both, false).blade, left.blade) && NearPoint(TabletHeldAxes(both, true).blade, right.blade),
	      "each role's own hand");
}

// Turned towards the head and held at its middle (Nadi, Windows 2026-11-26:
// "inventory tablet muss 90 grad nach richtung kopf des spielers und mittig
// in der hand. lass diese werte einstellbar"; docs/inventory-tablet-spec.md
// 24.4): a quarter turn about the blade by default, the page's middle on the
// palm's, and the turn, a lean, a roll and the place in centimetres set in
// [Tablet].
void TestHeldTurnedAndCentred() {
	std::printf("Turned towards the head, held at its middle:\n");
	TabletSettings s;
	s.enabled = true;
	s.widthMetres = 0.40f;
	const UiRect paper = Rect(100.0f, 200.0f, 900.0f, 1000.0f);  // 800 x 800 pixels: 0.0005 m a pixel
	const TabletFrame f = FrameOf(paper, paper);
	HeldAxes left;
	left.valid = true;
	left.blade = TurnAboutY(NiPoint3{0.0f, 0.6f, -0.8f}, 15.0f);
	left.edge = TurnAboutY(NiPoint3{0.0f, -0.8f, -0.6f}, 15.0f);
	const NiPoint3 palm{-0.04f, 0.01f, 0.13f};
	const NiPoint3 palmSide = CrossOf(left.edge, left.blade);
	const NiPoint3 away = left.edge * -1.0f;  // away from the knuckles: the thumb's and the wrist's way
	const float lift = PageLiftMetres(HandReach{}, true);
	Check(Near(s.held.turnDegrees, 90.0f) && Near(s.held.leanDegrees, 15.0f) && Near(s.held.rollDegrees, -10.0f) &&
	          Near(s.held.acrossCm, 22.0f) && Near(s.held.upCm, 0.0f) && Near(s.held.outCm, -9.0f),
	      "the defaults: Nadi's of 2026-11-28 - a quarter turn, leant 15, tilted -10, 22 cm across, 9 cm back");
	Check(Near(TabletSettings{}.widthMetres, 0.34f), "the default width: 0.34 m");
	// The checks below take one angle or place at a time from a quarter turn alone.
	s.held.leanDegrees = 0.0f;
	s.held.rollDegrees = 0.0f;
	s.held.acrossCm = 0.0f;
	s.held.outCm = 0.0f;

	TabletPlace p = PlaceTablet(f, s, true, palm, TabletHold::Side, left);
	Check(p.valid && NearPoint(Column(p, 1), left.blade), "its up still the blade's way out of the fist");
	Check(NearPoint(Column(p, 2), away),
	      "its face turned a quarter from the palm's side, away from the knuckles - to the head in a relaxed grip");
	Check(NearPoint(Column(p, 0), palmSide), "its right where the face was: out of the palm, to the body's middle");
	Check(NearPoint(Column(p, 3), palm + away * lift),
	      "its middle on the palm's, before the hand along the face it now shows - the hand behind it");
	const TabletHold holds[3] = {TabletHold::Side, TabletHold::Bottom, TabletHold::Top};
	bool middle = true;
	for (const TabletHold hold : holds) {
		middle = middle && SamePlace(PlaceTablet(f, s, true, palm, hold, left), p);
	}
	Check(middle, "held at its side, its bottom or its top: its middle in the hand all the same");

	// Nadi's own left hand (his log of 2026-11-26, the Torch node in the
	// Index's frame, -z ahead, +y up, +z back to him).
	HeldAxes his;
	his.valid = true;
	his.blade = NiPoint3{0.322f, 0.302f, -0.897f};
	his.edge = NiPoint3{-0.241f, -0.890f, -0.386f};
	p = PlaceTablet(f, s, true, palm, TabletHold::Side, his);
	Check(Column(p, 2).y > 0.85f && Column(p, 2).z > 0.3f && Column(p, 0).x > 0.9f,
	      "his hand: the face to the controller's top and back to him, the page across the palm to his middle");

	HeldAxes right;
	right.valid = true;
	right.blade = MirrorX(left.blade);
	right.edge = MirrorX(left.edge);
	const TabletPlace l = PlaceTablet(f, s, true, palm, TabletHold::Side, left);
	TabletPlace r = PlaceTablet(f, s, false, MirrorX(palm), TabletHold::Side, right);
	Check(r.valid && NearPoint(Column(r, 0), MirrorX(Column(l, 0)) * -1.0f) && NearPoint(Column(r, 1), MirrorX(Column(l, 1))) &&
	          NearPoint(Column(r, 2), MirrorX(Column(l, 2))) && NearPoint(Column(r, 3), MirrorX(Column(l, 3))),
	      "in the right hand: the left's mirror image, read the right way round");
	Check(NearPoint(Column(r, 2), right.edge * -1.0f), "its face away from its own knuckles too");

	s.held.acrossCm = 2.0f;
	s.held.upCm = 3.0f;
	s.held.outCm = 1.0f;
	p = PlaceTablet(f, s, true, palm, TabletHold::Side, left);
	Check(NearPoint(Column(p, 3), palm + palmSide * 0.02f + left.blade * 0.03f + away * (lift + 0.01f)),
	      "moved by the place in centimetres: across to the body's middle, up the page, out of its face");
	r = PlaceTablet(f, s, false, MirrorX(palm), TabletHold::Side, right);
	Check(NearPoint(Column(r, 3), MirrorX(Column(p, 3))), "the right hand's the mirror image: across is to the middle for either");
	s.held.acrossCm = 0.0f;
	s.held.upCm = 0.0f;
	s.held.outCm = 0.0f;

	const float c20 = std::cos(20.0f * 3.14159265f / 180.0f);
	const float s20 = std::sin(20.0f * 3.14159265f / 180.0f);
	s.held.leanDegrees = 20.0f;
	p = PlaceTablet(f, s, true, palm, TabletHold::Side, left);
	Check(NearPoint(Column(p, 1), left.blade * c20 - away * s20) && NearPoint(Column(p, 2), away * c20 + left.blade * s20) &&
	          NearPoint(Column(p, 0), palmSide),
	      "leant back 20 degrees: the top away from the face, the face up, the right as it was");
	s.held.leanDegrees = 0.0f;

	const float c10 = std::cos(10.0f * 3.14159265f / 180.0f);
	const float s10 = std::sin(10.0f * 3.14159265f / 180.0f);
	s.held.rollDegrees = 10.0f;
	p = PlaceTablet(f, s, true, palm, TabletHold::Side, left);
	Check(NearPoint(Column(p, 1), left.blade * c10 + palmSide * s10) && NearPoint(Column(p, 2), away),
	      "rolled 10 degrees: the top towards the body's middle in its own flat");
	r = PlaceTablet(f, s, false, MirrorX(palm), TabletHold::Side, right);
	Check(NearPoint(Column(r, 1), MirrorX(Column(p, 1))), "the right hand's roll the mirror image: its top to the middle too");
	s.held.rollDegrees = 0.0f;

	s.held.turnDegrees = 0.0f;
	p = PlaceTablet(f, s, true, palm, TabletHold::Side, left);
	Check(NearPoint(Column(p, 2), palmSide) && NearPoint(Column(p, 0), left.edge) && NearPoint(Column(p, 3), palm + palmSide * lift),
	      "no turn: the face on the palm's side as the sword's flat, its middle on the palm");
	s.held.turnDegrees = 90.0f;

	const TabletFrame box = FrameOf(paper, Rect(100.0f, 200.0f, 1300.0f, 1000.0f));
	p = PlaceTablet(box, s, true, palm, TabletHold::Side, left);
	Check(NearPoint(Column(p, 3), palm + palmSide * 0.10f + away * lift) && Near(p.width, 0.60f),
	      "a box beside the page: the page's middle stays on the palm, the face reaches out past it");
	p = PlaceTablet(box, s, false, MirrorX(palm), TabletHold::Side, right);
	Check(NearPoint(Column(p, 3), MirrorX(palm) + Column(p, 0) * 0.10f + right.edge * -lift),
	      "in the right hand too: the face's middle off the page's by the box, the page's middle on the palm");
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
	Check(f.tabCount == 0 && SameRect(f.crop, 100.0f, 200.0f, 1100.0f, 1200.0f), "the figure is fitted; the tabs are laid by SetTabs");
	TabSet pageAndFigure;
	pageAndFigure.count = 2;
	pageAndFigure.kind[1] = TabletTab::Figure;
	SetTabs(f, pageAndFigure);
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

// The game's paper on the pieces of OBVR's own (vr/TabletPaper.h): the page's
// picture, and the parchment cut to a piece's shape.
void TestPaperTexture() {
	std::printf("the game's paper on the tablet's own pieces\n");
	Check(SameRect(PaperPageSource(), 0.0f, 0.0f, 745.0f, 792.0f), "the page's picture: 745 by 792 texels from the top left");
	UiRect tall;
	tall.left = 100.0f;
	tall.top = 0.0f;
	tall.right = 200.0f;
	tall.bottom = 400.0f;
	tall.valid = true;
	// 1:4 inside the parchment's 650 by 540: the full height, 135 wide, centred.
	Check(SameRect(ParchmentSource(tall), 297.5f, 60.0f, 432.5f, 600.0f), "a tall piece: the parchment's height, cut to its width");
	UiRect wide = tall;
	wide.right = 1100.0f;
	wide.bottom = 100.0f;
	// 10:1: the full width, 65 high, centred.
	Check(SameRect(ParchmentSource(wide), 40.0f, 297.5f, 690.0f, 362.5f), "a wide piece: the parchment's width, cut to its height");
	UiRect same = tall;
	same.right = 100.0f + 650.0f;
	same.bottom = 540.0f;
	Check(SameRect(ParchmentSource(same), 40.0f, 60.0f, 690.0f, 600.0f), "the parchment's own shape: all of it");
	UiRect empty = tall;
	empty.right = empty.left;
	Check(!ParchmentSource(empty).valid, "an empty piece: no paper");
	UiRect none;
	Check(!ParchmentSource(none).valid, "an invalid piece: no paper");

	// The figure's empty page: parchment down to the page's foot, no leather band.
	PaperBand bands[2];
	FigurePaperBands(Rect(64.0f, 310.0f, 1825.0f, 2182.0f), bands);
	const float split = 310.0f + 1872.0f * 300.0f / 792.0f;
	Check(SameRect(bands[0].rect, 64.0f, 310.0f, 1825.0f, split) &&
	          SameRect(bands[0].source, 0.0f, 0.0f, 745.0f, 300.0f),
	      "the page's top as the game draws it: frame, bar, parchment, one texel a unit");
	Check(SameRect(bands[1].rect, 64.0f, split, 1825.0f, 2182.0f) &&
	          SameRect(bands[1].source, 0.0f, 300.0f, 745.0f, 605.0f),
	      "below it parchment to the page's lower edge, the leather band left out");
	FigurePaperBands(none, bands);
	Check(!bands[0].rect.valid && !bands[1].rect.valid, "no page: no bands");
}

bool Holds(const UiRect& r, float x, float y) {
	return r.valid && x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}

// Nadi's Windows run of 2026-11-23: "wenn ich zeilen hightlighte ... sehe ich
// auch die inv figur wie sie abgeschnitten ist". The item info joins the face
// beside the page; the face is one rectangle round both, and the page was
// drawn as that rectangle of the layer - with whatever the game draws in it
// beside the popup: the inventory's figure, at 0.545..0.913 of the screen,
// cut at the popup's right edge.
void TestPopupPictures() {
	std::printf("The page and a popup drawn as their own pictures:\n");
	const PaperTile pageTiles[] = {kPage};  // 27,131..772,923
	TabletRects rects = PaperOf(pageTiles, 1, kScreenWidth, kScreenHeight);
	const UiRect info = Rect(772.0f, 300.0f, 1150.0f, 600.0f);  // the item info beside the page
	JoinPopup(rects, info);
	const float figureX = 1100.0f;  // the game's figure, beside the page, above the info
	const float figureY = 200.0f;
	Check(Holds(rects.crop, figureX, figureY), "the face round page and info holds the figure's place (the old one picture)");
	Check(SameRect(rects.outline, 27.0f, 131.0f, 772.0f, 923.0f), "the page's outline stays without the popup");
	Check(rects.popupCount == 1 && SameRect(rects.popups[0], 772.0f, 300.0f, 1150.0f, 600.0f), "the popup kept on its own");

	TabletFrame f;
	f.up = true;
	f.paper = rects.paper;
	f.pageCrop = rects.crop;
	f.crop = rects.crop;
	f.outline = rects.outline;
	f.popupCount = rects.popupCount;
	for (UInt32 i = 0; i < rects.popupCount; ++i) {
		f.popups[i] = rects.popups[i];
	}
	UiRect pictures[kTabletPopupsMost + 1];
	const UInt32 n = LayerPictures(f, pictures, kTabletPopupsMost + 1);
	Check(n == 2, "two pictures: the page, the popup");
	bool figureShown = false;
	for (UInt32 i = 0; i < n; ++i) {
		figureShown = figureShown || Holds(pictures[i], figureX, figureY);
	}
	Check(!figureShown, "the figure's place beside them is in neither - the figure no longer shows cut");
	Check(Holds(pictures[1], 1140.0f, 590.0f), "the popup is shown to its edge");

	TabletFrame plain;
	plain.up = true;
	plain.pageCrop = Rect(0.0f, 0.0f, 100.0f, 100.0f);
	Check(LayerPictures(plain, pictures, kTabletPopupsMost + 1) == 1 && SameRect(pictures[0], 0.0f, 0.0f, 100.0f, 100.0f),
	      "no popup: the page's cut as one picture, as before");
	Check(LayerPictures(plain, pictures, 0) == 0, "no room: none");

	TabletRects many = PaperOf(pageTiles, 1, kScreenWidth, kScreenHeight);
	for (UInt32 i = 0; i < kTabletPopupsMost + 2; ++i) {
		JoinPopup(many, Rect(800.0f, 10.0f * static_cast<float>(i), 900.0f, 10.0f * static_cast<float>(i) + 5.0f));
	}
	Check(many.popupCount == kTabletPopupsMost && many.popupsOverflow, "more popups than kept: counted as overflow");
	TabletFrame over;
	over.up = true;
	over.pageCrop = many.crop;
	over.outline = many.outline;
	over.popupCount = many.popupCount;
	over.popupsOverflow = true;
	Check(LayerPictures(over, pictures, kTabletPopupsMost + 1) == 1 &&
	          SameRect(pictures[0], many.crop.left, many.crop.top, many.crop.right, many.crop.bottom),
	      "popups not all kept: the whole cut as one picture, nothing lost");
}

// Where a pixel column of the face lies along the quad's right axis, from
// the hand that holds it, metres.
float AcrossFromHand(const TabletPlace& p, const TabletFrame& f, const NiPoint3& hand, float x) {
	const NiPoint3 right = Column(p, 0);
	const NiPoint3 from = Column(p, 3) - hand;
	const float centre = from.x * right.x + from.y * right.y + from.z * right.z;
	return centre + ((x - f.crop.left) / (f.crop.right - f.crop.left) - 0.5f) * p.width;
}

// Held at its side the hand has the page's edge. It had the whole face's: in
// a physical right hand that is the edge a popup joins, so an info box
// sliding out moved the page left by the box's width and back as it went.
void TestHeldByItsOutline() {
	std::printf("Held by the page's edge, not a popup's:\n");
	TabletSettings s;
	s.enabled = true;
	s.widthMetres = 0.40f;
	const UiRect paper = Rect(100.0f, 200.0f, 900.0f, 1000.0f);  // 800 pixels wide: 0.0005 m a pixel
	TabletFrame plain = FrameOf(paper, paper);
	plain.outline = paper;
	// An info box 400 pixels out to the right of the page.
	TabletFrame box = plain;
	box.crop = Rect(100.0f, 200.0f, 1300.0f, 1000.0f);
	box.pageCrop = box.crop;
	box.popupCount = 1;
	box.popups[0] = Rect(900.0f, 300.0f, 1300.0f, 700.0f);
	const NiPoint3 hand{0.02f, -0.01f, 0.10f};

	TabletPlace p = PlaceTablet(plain, s, false, hand, TabletHold::Side);
	Check(Near(AcrossFromHand(p, plain, hand, 900.0f), 0.0f), "in the right hand: the page's right edge in the hand");
	p = PlaceTablet(box, s, false, hand, TabletHold::Side);
	Check(Near(AcrossFromHand(p, box, hand, 900.0f), 0.0f) && Near(AcrossFromHand(p, box, hand, 100.0f), -0.40f),
	      "an info box out to its right: the page stays where it was");
	Check(Near(AcrossFromHand(p, box, hand, 1300.0f), 0.20f), "and the box reaches out past the hand");
	TabletFrame unread = box;
	unread.outline = UiRect{};
	p = PlaceTablet(unread, s, false, hand, TabletHold::Side);
	Check(Near(AcrossFromHand(p, unread, hand, 1300.0f), 0.0f) && Near(AcrossFromHand(p, unread, hand, 900.0f), -0.20f),
	      "(no outline read: the face's own edge in the hand - the page a box's width aside, as it was)");

	p = PlaceTablet(box, s, true, hand, TabletHold::Side);
	Check(Near(AcrossFromHand(p, box, hand, 100.0f), 0.0f) && Near(AcrossFromHand(p, box, hand, 900.0f), 0.40f),
	      "in the left hand the box is at the far side: the page's left edge in the hand, as before");
	TabletFrame boxLeft = plain;
	boxLeft.crop = Rect(0.0f, 200.0f, 900.0f, 1000.0f);
	boxLeft.pageCrop = boxLeft.crop;
	boxLeft.popupCount = 1;
	boxLeft.popups[0] = Rect(0.0f, 300.0f, 300.0f, 700.0f);
	p = PlaceTablet(boxLeft, s, true, hand, TabletHold::Side);
	Check(Near(AcrossFromHand(p, boxLeft, hand, 100.0f), 0.0f) && Near(AcrossFromHand(p, boxLeft, hand, 0.0f), -0.05f),
	      "a box reaching out to the left of the page: the left hand keeps the page's edge too");

	// The figure's own page beside the page is no popup: its outer edge is the hand's.
	TabletFrame figure = plain;
	figure.figurePage = true;
	figure.backing = Rect(900.0f, 200.0f, 1200.0f, 1000.0f);
	figure.crop = Rect(100.0f, 200.0f, 1200.0f, 1000.0f);
	p = PlaceTablet(figure, s, false, hand, TabletHold::Side);
	Check(Near(AcrossFromHand(p, figure, hand, 1200.0f), 0.0f), "the figure's page beside it: the right hand holds that page's edge");
	Check(SameRect(HoldFace(figure), 100.0f, 200.0f, 1200.0f, 1000.0f) && SameRect(HoldFace(box), 100.0f, 200.0f, 900.0f, 1000.0f) &&
	          SameRect(HoldFace(unread), 100.0f, 200.0f, 1300.0f, 1000.0f),
	      "what the hand holds: the outline, with the figure's page, or the face when no outline was read");

	// Like the sword: along the page's own right, whichever way it is turned -
	// its middle in the hand since section 24.4, the box out past its edge.
	HeldAxes right;
	right.valid = true;
	right.blade = NiPoint3{0.0f, 0.6f, -0.8f};
	right.edge = NiPoint3{0.0f, -0.8f, -0.6f};
	s.held.leanDegrees = s.held.rollDegrees = s.held.acrossCm = s.held.outCm = 0.0f;  // the turn alone
	p = PlaceTablet(box, s, false, hand, TabletHold::Side, right);
	Check(Near(AcrossFromHand(p, box, hand, 500.0f), 0.0f) && Near(AcrossFromHand(p, box, hand, 1300.0f), 0.40f),
	      "held like the sword: the page's middle in the hand, the box out past the page's edge");
	p = PlaceTablet(box, s, false, hand, TabletHold::Bottom);
	const TabletPlace q = PlaceTablet(plain, s, false, hand, TabletHold::Bottom);
	Check(Near(AcrossFromHand(p, box, hand, 900.0f), AcrossFromHand(q, plain, hand, 900.0f)),
	      "held at its lower edge the page never moved with a box: still so");
}

}  // namespace

int main() {
	TestMenus();
	TestPaper();
	TestPopup();
	TestOwnPopup();
	TestPopupPictures();
	TestPlace();
	TestHeldByItsOutline();
	TestHeldLikeTheSword();
	TestHeldTurnedAndCentred();
	TestCorners();
	TestFigurePage();
	TestFigureTab();
	TestPaperTexture();
	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
