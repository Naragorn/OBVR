// Checks the HUD on the hands' decisions (vr/HandHud.h): the places, the
// tile tree's rectangles, the capture pixels, the atlas, the placement on
// the hands and the compass in the sky.

#include <cstdio>
#include <cstring>

#include "vr/HandHud.h"

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

bool Near(float a, float b, float eps = 1e-3f) { return a - b < eps && b - a < eps; }

HudTile Tile(const char* name, SInt32 parent, float x, float y, float w = 0.0f, float h = 0.0f) {
	HudTile t;
	std::strncpy(t.name, name, kHudTileNameSize - 1);
	t.parent = parent;
	t.x = x;
	t.y = y;
	t.width = w;
	t.height = h;
	t.hasSize = w > 0.0f || h > 0.0f;
	return t;
}

// The vanilla tree's relevant part, as the probe read it (spec section 8).
UInt32 VanillaTree(HudTile* t) {
	UInt32 n = 0;
	t[n++] = Tile("HUDMainMenu", -1, 0, 0);                                // 0
	t[n++] = Tile("hudmain_background", 0, 87, 850, 620, 70);              // 1
	t[n++] = Tile("hudmain_compass_layout", 1, 407, 5, 213, 87);           // 2
	t[n++] = Tile("hudmain_compass_window", 2, 0, 0, 200, 87);             // 3
	t[n++] = Tile("hudmain_compass_heading", 3, -668, 1, 2048, 62);        // 4
	t[n++] = Tile("hudmain_compass_frame", 2, -14, -10, 220, 84);          // 5
	t[n++] = Tile("hudmain_Magic_Icon", 1, 324, 5, 63, 63);                // 6
	t[n++] = Tile("hudmain_magic_cover", 6, -4, -8, 72, 80);               // 7
	t[n++] = Tile("hudmain_Weapon_Icon", 1, 229, 5, 63, 63);               // 8
	t[n++] = Tile("hudmain_weapon_cover", 8, -4, -8, 72, 80);              // 9
	t[n++] = Tile("hudmain_statusbars", 1, 0, 12);                         // 10
	t[n++] = Tile("hudmain_bars_cover", 10, 0, -15, 199, 80);              // 11
	t[n++] = Tile("hudmain_fatigue_empty", 10, 0, 36, 189, 16);            // 12
	t[n++] = Tile("hudmain_magic_empty", 10, 0, 18, 189, 16);              // 13
	t[n++] = Tile("hudmain_health_empty", 10, 0, 0, 189, 16);              // 14
	t[n++] = Tile("magic_icons", 0, 1642.5f, 58);                          // 15
	return n;
}

void TestPlaces() {
	std::printf("The places\n");
	HudPlace p = HudPlace::View;
	Check(ParseHudPlace("Left", p) && p == HudPlace::Left, "\"Left\": the left hand");
	Check(ParseHudPlace("sky", p) && p == HudPlace::Sky, "\"sky\": the sky");
	p = HudPlace::Right;
	Check(!ParseHudPlace("wrist", p) && p == HudPlace::Right, "an unknown word: refused, the place kept");
	Check(HudPlaceFromIndex(2.0f) == HudPlace::Right, "row value 2: right");
	Check(HudPlaceFromIndex(9.0f) == HudPlace::View, "out of range: the view");
	volatile float zero = 0.0f;
	Check(HudPlaceFromIndex(zero / zero) == HudPlace::View, "not a number: the view");

	HandHudSettings s;
	Check(HudElementLifted(s, HudElement::Bars) && HudElementShown(s, HudElement::Bars),
	      "the bars on the left hand: lifted and shown");
	s.element[0].place = HudPlace::Off;
	Check(HudElementLifted(s, HudElement::Bars) && !HudElementShown(s, HudElement::Bars),
	      "off: lifted out of the panel, shown nowhere");
	s.element[0].place = HudPlace::View;
	Check(!HudElementLifted(s, HudElement::Bars), "the view: left where the game drew it");
	s.element[0].place = HudPlace::Left;
	s.enabled = false;
	Check(!HudElementLifted(s, HudElement::Bars) && !HudElementShown(s, HudElement::Bars),
	      "the feature off: nothing lifted");
}

void TestTiles() {
	std::printf("The tile tree\n");
	HudTile t[kHudTilesMax];
	const UInt32 n = VanillaTree(t);
	Check(FindHudTile(t, n, "HUDMAIN_MAGIC_ICON") == 6, "found by name, any case");
	Check(FindHudTile(t, n, "hudmain_nothing") == -1, "an unknown name: -1");
	float x = 0.0f, y = 0.0f;
	Check(HudTileOrigin(t, n, 14, x, y) && Near(x, 87.0f) && Near(y, 862.0f),
	      "the health bar's origin: its parents' x and y added up");
	HudTile broken[2] = {Tile("a", 5, 1, 1, 1, 1), Tile("b", 0, 1, 1, 1, 1)};
	Check(!HudTileOrigin(broken, 2, 1, x, y), "a parent out of range: refused");
	HudTile loop[2] = {Tile("a", 1, 1, 1, 1, 1), Tile("b", 0, 1, 1, 1, 1)};
	Check(!HudTileOrigin(loop, 2, 0, x, y), "a loop: refused");
	Check(!HudTileRect(t, n, 10).valid, "a tile without a size: no rectangle");
	Check(!HudTileRect(t, n, -1).valid && !HudTileRect(t, n, 99).valid, "an index out of range: none");
	UiRect a{0, 0, 10, 10, true};
	UiRect b{5, -5, 20, 8, true};
	const UiRect u = UnionRect(a, b);
	Check(u.valid && Near(u.left, 0) && Near(u.top, -5) && Near(u.right, 20) && Near(u.bottom, 10),
	      "the union of two rectangles");
	Check(UnionRect(UiRect{}, b).left == 5.0f && UnionRect(a, UiRect{}).right == 10.0f,
	      "an invalid one does not count");
	Check(HudTileUnder(t, n, 4, 2) && !HudTileUnder(t, n, 6, 2), "the heading lies under the compass");
}

void TestElements() {
	std::printf("The elements' rectangles\n");
	HudTile t[kHudTilesMax];
	UInt32 n = VanillaTree(t);
	const UiRect bars = HudElementRect(t, n, HudElement::Bars);
	Check(bars.valid && Near(bars.left, 87) && Near(bars.top, 847) && Near(bars.right, 286) &&
	          Near(bars.bottom, 927),
	      "the bars: the three bars and their cover, (87, 847)..(286, 927)");
	const UiRect compass = HudElementRect(t, n, HudElement::Compass);
	Check(compass.valid && Near(compass.left, 480) && Near(compass.right, 700) && Near(compass.top, 845),
	      "the compass: its window and frame, not the 2048-wide heading strip");
	const UiRect spell = HudElementRect(t, n, HudElement::Spell);
	Check(spell.valid && Near(spell.left, 407) && Near(spell.right, 479), "the spell: icon and cover");
	Check(!HudElementRect(t, n, HudElement::Effects).valid, "no active effect: nothing");
	Check(!HudElementRect(t, n, HudElement::LevelUp).valid, "no level-up icon in the tree: nothing");
	t[n++] = Tile("magic_icon_1", 15, 0, 0, 32, 32);
	t[n++] = Tile("magic_icon_2", 15, 0, 34, 32, 32);
	const UiRect effects = HudElementRect(t, n, HudElement::Effects);
	Check(effects.valid && Near(effects.left, 1642.5f) && Near(effects.top, 58) && Near(effects.bottom, 124),
	      "two effects: both icons under magic_icons");
	HudTile renamed[3] = {Tile("root", -1, 0, 0), Tile("modbar_health", 0, 10, 10, 100, 10),
	                      Tile("magic_icons_x", 0, 0, 0)};
	Check(!HudElementRect(renamed, 3, HudElement::Bars).valid && !HudElementRect(renamed, 3, HudElement::Effects).valid,
	      "a UI mod that renamed the tiles: not found, left in the panel");
}

void TestCapture() {
	std::printf("The capture pixels\n");
	const UiRect bars{87, 847, 286, 927, true};
	const CaptureRect c = UiRectToCapture(bars, 2203, 3916, 3480);
	Check(c.valid && c.left == 199 && c.top == 1943 && c.right == 657 && c.bottom == 2128,
	      "2203 believed rows: 2.2948 pixels a unit, rounded outwards");
	const CaptureRect edge = UiRectToCapture(UiRect{-10, -10, 2000, 2000, true}, 960, 1706, 960);
	Check(edge.valid && edge.left == 0 && edge.top == 0 && edge.right == 1706 && edge.bottom == 960,
	      "kept inside the capture");
	Check(!UiRectToCapture(UiRect{}, 960, 100, 100).valid, "no rectangle: none");
	Check(!UiRectToCapture(bars, 0, 100, 100).valid, "no believed height: none");
	Check(!UiRectToCapture(UiRect{5000, 5000, 5100, 5100, true}, 960, 100, 100).valid,
	      "wholly outside: none");
}

void TestAtlas() {
	std::printf("The atlas\n");
	const CaptureRect wide = AtlasSlotRect(0, 400, 100);
	Check(wide.valid && wide.left == 0 && wide.top == 0 && wide.right == 512 && wide.bottom == 128,
	      "a wide picture fills the slot's width, its aspect kept");
	const CaptureRect tall = AtlasSlotRect(5, 50, 200);
	Check(tall.valid && tall.left == 512 && tall.top == 512 && tall.right == 640 && tall.bottom == 1024,
	      "slot 5: second row, second column; a tall picture fills the height");
	Check(!AtlasSlotRect(1, 0, 10).valid, "an empty picture: none");
}

void TestLayout() {
	std::printf("The row on a hand\n");
	const float widths[3] = {0.12f, 0.04f, 0.04f};
	float centres[3];
	LayoutHandRow(widths, 3, 0.01f, centres);
	Check(Near(centres[0], -0.05f) && Near(centres[1], 0.04f) && Near(centres[2], 0.09f),
	      "three quads centred as a row, 1 cm apart");
	float one[1];
	const float w1[1] = {0.2f};
	LayoutHandRow(w1, 1, 0.01f, one);
	Check(Near(one[0], 0.0f), "one quad: in the middle");
	const openvr::HmdMatrix34 m = HandPanelTransform(0.05f, 0.08f, 35.0f, 0.03f);
	Check(Near(m.m[0][3], 0.03f) && Near(m.m[1][3], 0.05f) && Near(m.m[2][3], 0.08f),
	      "the panel's place: sideways, up and back along the controller");
	Check(Near(m.m[1][2], -math::Sin(-55.0f * math::kPi / 180.0f)), "its face turned up and tilted 35 degrees");
}

openvr::HmdMatrix34 HeadLooking(float pitchDegrees) {
	const float p = pitchDegrees * math::kPi / 180.0f;
	openvr::HmdMatrix34 h{};
	// x right; forward (-z) pitched up by p: z = (0, -sin p, cos p), y = (0, cos p, sin p).
	h.m[0][0] = 1.0f;
	h.m[1][1] = math::Cos(p);
	h.m[2][1] = math::Sin(p);
	h.m[1][2] = -math::Sin(p);
	h.m[2][2] = math::Cos(p);
	h.m[1][3] = 1.7f;
	return h;
}

void TestCompass() {
	std::printf("The compass in the sky\n");
	Check(Near(HeadPitchDegrees(HeadLooking(0.0f)), 0.0f) && Near(HeadPitchDegrees(HeadLooking(30.0f)), 30.0f, 0.01f),
	      "the head's pitch, up positive");
	Check(CompassOpacity(10.0f, 20.0f, 35.0f, 1.0f) == 0.0f, "looking straight: invisible");
	Check(Near(CompassOpacity(27.5f, 20.0f, 35.0f, 0.8f), 0.4f), "half way up the fade: half");
	Check(CompassOpacity(50.0f, 20.0f, 35.0f, 0.8f) == 0.8f, "above the fade: its own opacity");
	Check(CompassOpacity(25.0f, 30.0f, 30.0f, 1.0f) == 0.0f && CompassOpacity(30.0f, 30.0f, 30.0f, 1.0f) == 1.0f,
	      "no fade width: a switch at the start");
	volatile float zero = 0.0f;
	Check(CompassOpacity(zero / zero, 20.0f, 35.0f, 1.0f) == 0.0f, "a pitch that is not a number: invisible");

	const openvr::HmdMatrix34 ahead = CompassPose(HeadLooking(0.0f), 40.0f, 1.5f);
	Check(Near(ahead.m[1][3], 1.7f + 1.5f * math::Sin(40.0f * math::kPi / 180.0f)) &&
	          Near(ahead.m[2][3], -1.5f * math::Cos(40.0f * math::kPi / 180.0f)) && Near(ahead.m[0][3], 0.0f),
	      "40 degrees up, 1.5 m away, ahead of the heading");
	Check(Near(ahead.m[0][0], 1.0f) && Near(ahead.m[1][0], 0.0f), "its x stays level");
	Check(ahead.m[1][2] < 0.0f && ahead.m[2][2] > 0.0f, "its face turned down and back to the eyes");
	const openvr::HmdMatrix34 up = CompassPose(HeadLooking(60.0f), 40.0f, 1.5f);
	Check(Near(up.m[1][3], ahead.m[1][3]) && Near(up.m[2][3], ahead.m[2][3]),
	      "looking up does not move it: the heading only");
	const openvr::HmdMatrix34 zenith = CompassPose(HeadLooking(90.0f), 40.0f, 1.5f);
	Check(zenith.m[2][3] < 0.0f, "straight up: still where the heading was");
	const openvr::HmdMatrix34 nadir = CompassPose(HeadLooking(-90.0f), 40.0f, 1.5f);
	Check(nadir.m[2][3] < 0.0f, "straight down: still ahead");
}

void TestPlacement() {
	std::printf("One frame's placement\n");
	HandHudSettings s;
	s.showOnLook = false;  // the watch: always on the back of the hand
	HandHudFrame f;
	f.leftValid = true;
	f.rightValid = true;
	f.haveHead = true;
	f.head = HeadLooking(0.0f);
	f.rect[0] = UiRect{87, 847, 286, 927, true};   // bars, 199 wide
	f.rect[1] = UiRect{407, 847, 479, 927, true};  // spell, 72 wide
	f.rect[2] = UiRect{312, 847, 384, 927, true};  // weapon
	f.rect[5] = UiRect{480, 845, 701, 932, true};  // compass
	HandHudQuad q[kHudElementCount];
	PlaceHandHud(s, f, q);
	Check(q[0].shown && q[0].onDevice && !q[0].rightHand && Near(q[0].widthMetres, 199 * 0.0006f),
	      "the bars: on the left hand, 199 units wide");
	Check(q[0].pose.m[0][3] < q[1].pose.m[0][3], "the spell beside them in the row");
	Check(q[2].shown && q[2].rightHand && Near(q[2].pose.m[0][3], 0.0f), "the weapon alone on the right: centred");
	{
		HandHudSettings moved = s;
		moved.offsetRightMetres = 0.02f;
		moved.offsetUpMetres = 0.0f;
		HandHudQuad m[kHudElementCount];
		PlaceHandHud(moved, f, m);
		Check(Near(m[2].pose.m[0][3], 0.02f), "on the back of the hand: the X setting moves it along the panel");
	}
	Check(!q[3].shown && !q[4].shown, "no effects, no level-up: nothing shown");
	Check(!q[5].onDevice && q[5].alpha == 0.0f && !q[5].shown, "the compass, looking straight: not shown");
	f.head = HeadLooking(40.0f);
	PlaceHandHud(s, f, q);
	Check(q[5].shown && Near(q[5].alpha, 1.0f) && Near(q[5].widthMetres, 221 * 0.0023f),
	      "looking up: the compass in the sky, fully");
	s.element[0].opacity = 0.5f;
	s.element[0].size = 2.0f;
	PlaceHandHud(s, f, q);
	Check(Near(q[0].alpha, 0.5f) && Near(q[0].widthMetres, 2 * 199 * 0.0006f), "its own opacity and size");
	f.leftValid = false;
	PlaceHandHud(s, f, q);
	Check(!q[0].shown && !q[1].shown && q[2].shown, "the left hand lost: its quads hidden, the right's stay");
	f.haveHead = false;
	PlaceHandHud(s, f, q);
	Check(!q[5].shown, "no head pose: no compass");
	s.element[0].opacity = 0.0f;
	f.leftValid = true;
	PlaceHandHud(s, f, q);
	Check(!q[0].shown, "opacity 0: not shown");
	s.enabled = false;
	PlaceHandHud(s, f, q);
	Check(!q[1].shown && !q[2].shown, "the feature off: nothing");
}

void TestText() {
	std::printf("The region's name, the notices and the subtitles\n");
	HudPlace p = HudPlace::View;
	Check(ParseHudPlace("top", p) && p == HudPlace::Top && ParseHudPlace("Bottom", p) && p == HudPlace::Bottom,
	      "\"top\" and \"bottom\": the view's top and bottom");
	Check(HudPlaceFromIndex(6.0f) == HudPlace::Bottom, "row value 6: bottom");
	Check(HudElementInSubtitleMenu(HudElement::Messages) && HudElementInSubtitleMenu(HudElement::Subtitles) &&
	          !HudElementInSubtitleMenu(HudElement::Region),
	      "the notices and subtitles are HUDSubtitleMenu's, the region's name HUDMainMenu's");

	// HUDSubtitleMenu as the probe read it with a notice up (spec section 8).
	HudTile t[8];
	UInt32 n = 0;
	t[n++] = Tile("HUDSubtitleMenu", -1, 0, 0);
	t[n++] = Tile("hudsubtitle_text_layout", 0, 0, 0);
	t[n++] = Tile("hudsubtitle_notice", 1, 48, 40, 554, 45);
	t[n++] = Tile("hudsubtitle_icon", 1, 40, 40);  // hidden: no size
	t[n] = Tile("hudsubtitle_text", 1, 853.25f, 700, 400, 45);
	t[n++].justify = kHudJustifyCentre;
	const UiRect notice = HudElementRect(t, n, HudElement::Messages);
	Check(notice.valid && Near(notice.left, 48) && Near(notice.right, 602) && Near(notice.bottom, 85),
	      "the notice: its text, the hidden icon left out");
	const UiRect said = HudElementRect(t, n, HudElement::Subtitles);
	Check(said.valid && Near(said.left, 653.25f) && Near(said.right, 1053.25f),
	      "a subtitle, centred: x is its middle");
	t[n - 1].justify = kHudJustifyRight;
	Check(Near(HudElementRect(t, n, HudElement::Subtitles).left, 453.25f), "right-justified: x is its right edge");

	HudTile main[3] = {Tile("HUDMainMenu", -1, 0, 0), Tile("hudmain_background", 0, 87, 850, 620, 70),
	                   Tile("hudmain_region", 1, 0, -45, 360, 45)};
	main[2].alpha = 0.0f;
	Check(!HudElementRect(main, 3, HudElement::Region).valid, "the region's name at alpha 0: nothing to show");
	main[2].alpha = 200.0f;
	const UiRect region = HudElementRect(main, 3, HudElement::Region);
	Check(region.valid && Near(region.top, 805), "fading in: its rectangle");
}

void TestViewAnchor() {
	std::printf("The top and bottom of the view\n");
	openvr::HmdMatrix34 ahead = HeadLooking(0.0f);
	Check(Near(HeadingApartDegrees(ahead, ahead), 0.0f), "the same heading: 0 apart");
	openvr::HmdMatrix34 turned{};  // turned 90 degrees to the right: forward +x
	turned.m[0][2] = -1.0f;
	turned.m[1][1] = 1.0f;
	turned.m[2][0] = 1.0f;
	Check(Near(HeadingApartDegrees(ahead, turned), 90.0f, 0.05f), "a quarter turn: 90 apart");
	Check(HeadingApartDegrees(ahead, HeadLooking(90.0f)) == 0.0f, "straight up: no heading, 0");

	HudViewAnchor a;
	StepViewAnchor(a, false, true, ahead, 30.0f, false);
	Check(!a.valid, "nothing shown: no anchor");
	StepViewAnchor(a, true, true, ahead, 30.0f, false);
	Check(a.valid && Near(a.head.m[2][2], 1.0f), "it appears: placed where the head looks");
	openvr::HmdMatrix34 little = HeadLooking(0.0f);
	little.m[0][3] = 0.3f;  // moved, not turned
	StepViewAnchor(a, true, true, little, 30.0f, false);
	Check(Near(a.head.m[0][3], 0.0f), "the head moves or turns a little: it stays");
	StepViewAnchor(a, true, true, turned, 30.0f, false);
	Check(Near(a.head.m[0][2], -1.0f), "turned beyond 30 degrees: taken along");
	StepViewAnchor(a, true, true, ahead, 0.0f, false);
	Check(Near(a.head.m[0][2], -1.0f), "following off (0): it stays where it appeared");
	StepViewAnchor(a, true, true, ahead, 0.0f, true);
	Check(Near(a.head.m[2][2], 1.0f), "locked to the head: every frame");
	StepViewAnchor(a, true, false, turned, 30.0f, false);
	Check(a.valid && Near(a.head.m[2][2], 1.0f), "no head pose this frame: kept");
	StepViewAnchor(a, false, true, turned, 30.0f, false);
	Check(!a.valid, "gone: dropped, the next one appears where the head looks then");

	HandHudSettings s;
	HandHudFrame f;
	f.haveHead = true;
	f.head = ahead;
	f.rect[7] = UiRect{48, 40, 602, 85, true};    // a notice, top
	f.rect[8] = UiRect{653, 700, 1053, 745, true};  // a subtitle, bottom
	HandHudQuad q[kHudElementCount];
	PlaceHandHud(s, f, q);
	Check(!q[7].shown && !q[8].shown, "no anchor yet: not shown");
	f.viewAnchorValid[0] = f.viewAnchorValid[1] = true;
	f.viewAnchor[0] = f.viewAnchor[1] = ahead;
	PlaceHandHud(s, f, q);
	const float up = 1.7f + 1.2f * math::Sin(15.0f * math::kPi / 180.0f);
	const float down = 1.7f - 1.2f * math::Sin(20.0f * math::kPi / 180.0f);
	Check(q[7].shown && !q[7].onDevice && Near(q[7].pose.m[1][3], up) && Near(q[7].pose.m[0][3], 0.0f),
	      "the notice: 15 degrees above the middle, 1.2 m away, centred");
	Check(q[8].shown && Near(q[8].pose.m[1][3], down) && Near(q[8].widthMetres, 400 * 0.0008f),
	      "the subtitle: 20 degrees below, 400 units wide");
	Check(Near(q[7].alpha, 1.0f), "at its own opacity");
}

// A controller at (x, y, z) whose x axis points along (ax, 0, az), y up.
HandLook Controller(float x, float y, float z, float ax, float az) {
	HandLook h;
	h.valid = true;
	h.pose.m[0][0] = ax;
	h.pose.m[2][0] = az;
	h.pose.m[1][1] = 1.0f;
	// z = x cross y = (ax,0,az) x (0,1,0) = (-az, 0, ax)
	h.pose.m[0][2] = -az;
	h.pose.m[2][2] = ax;
	h.pose.m[0][3] = x;
	h.pose.m[1][3] = y;
	h.pose.m[2][3] = z;
	return h;
}

void TestLook() {
	std::printf("Looking at a hand\n");
	const openvr::HmdMatrix34 head = HeadLooking(0.0f);  // at (0, 1.7, 0), looking -z
	// The left hand 40 cm ahead, its palm (+x) turned back to the face: x = +z.
	HandLook left = Controller(0.0f, 1.6f, -0.4f, 0.0f, 1.0f);
	Check(PalmFacesEyes(left, false, head, 35.0f, 55.0f), "the left palm to the face, the hand in view: looked at");
	// The right hand's palm is its -x: x = -z turns the palm to the face.
	HandLook right = Controller(0.0f, 1.6f, -0.4f, 0.0f, -1.0f);
	Check(PalmFacesEyes(right, true, head, 35.0f, 55.0f), "the right palm (its -x) to the face: looked at");
	Check(!PalmFacesEyes(left, true, head, 35.0f, 55.0f), "the same controller as a right hand: the back of it, not");
	HandLook down = Controller(0.0f, 1.6f, -0.4f, 1.0f, 0.0f);  // palm sideways
	Check(!PalmFacesEyes(down, false, head, 35.0f, 55.0f), "the palm turned sideways: not");
	HandLook aside = Controller(0.6f, 1.6f, -0.1f, -0.6f, 0.8f);  // far to the right, out of view
	Check(!PalmFacesEyes(aside, false, head, 35.0f, 55.0f), "the hand out of view: not");
	HandLook fist = left;
	fist.open = false;
	Check(!PalmFacesEyes(fist, false, head, 35.0f, 55.0f), "a fist: not");
	HandLook lost = left;
	lost.valid = false;
	Check(!PalmFacesEyes(lost, false, head, 35.0f, 55.0f), "not tracked: not");
	HandLook atHead = Controller(0.0f, 1.7f, 0.0f, 0.0f, 1.0f);
	Check(!PalmFacesEyes(atHead, false, head, 35.0f, 55.0f), "at the eyes themselves: no direction, not");

	Check(Near(StepHandFade(0.0f, true, 0.05f, 0.15f), 0.3333f), "fading in: a third in 50 ms of 150");
	Check(StepHandFade(0.9f, true, 0.05f, 0.15f) == 1.0f, "never past 1");
	Check(StepHandFade(0.1f, false, 0.05f, 0.15f) == 0.0f, "fading out: never below 0");
	Check(StepHandFade(0.5f, true, 0.01f, 0.0f) == 1.0f && StepHandFade(0.5f, false, 0.01f, 0.0f) == 0.0f,
	      "no fade time: at once");

	const openvr::HmdMatrix34 inPalm = PalmPanelPose(left, false, head, 0.02f, 0.05f);
	Check(Near(inPalm.m[1][3], 1.55f) && Near(inPalm.m[2][3], -0.38f),
	      "in the palm: 5 cm down the controller, 2 cm off the palm");
	const openvr::HmdMatrix34 panel = PalmPanelPose(left, false, head, 0.06f);
	Check(Near(panel.m[2][3], -0.34f) && Near(panel.m[1][3], 1.6f), "6 cm off the palm towards the eyes");
	Check(panel.m[2][2] > 0.9f && Near(panel.m[1][0], 0.0f), "facing the eyes, its x level");
	const openvr::HmdMatrix34 rel = RelativeToDevice(left.pose, panel);
	// Back to absolute: device * rel must give the panel again.
	float back[3];
	for (int r = 0; r < 3; ++r) {
		back[r] = left.pose.m[r][0] * rel.m[0][3] + left.pose.m[r][1] * rel.m[1][3] + left.pose.m[r][2] * rel.m[2][3] +
		          left.pose.m[r][3];
	}
	Check(Near(back[0], panel.m[0][3]) && Near(back[1], panel.m[1][3]) && Near(back[2], panel.m[2][3]),
	      "relative to the controller and back: the same place");
	const openvr::HmdMatrix34 moved = AlongOwnXY(panel, 0.02f, -0.03f);
	Check(Near(moved.m[0][3], panel.m[0][3] + 0.02f * panel.m[0][0] - 0.03f * panel.m[0][1]) &&
	          Near(moved.m[1][3], panel.m[1][3] + 0.02f * panel.m[1][0] - 0.03f * panel.m[1][1]) &&
	          Near(moved.m[2][3], panel.m[2][3] + 0.02f * panel.m[2][0] - 0.03f * panel.m[2][1]) &&
	          moved.m[1][3] < panel.m[1][3] - 0.025f,
	      "moved on its own face: 2 cm right, 3 cm down, as the eyes see it");

	HandHudSettings s;
	HandHudFrame f;
	f.leftValid = f.rightValid = f.haveHead = true;
	f.head = head;
	f.rect[0] = UiRect{87, 847, 286, 927, true};
	f.rect[2] = UiRect{312, 847, 384, 927, true};
	f.hand[0] = left;
	f.hand[1] = right;
	HandHudQuad q[kHudElementCount];
	PlaceHandHud(s, f, q);
	Check(!q[0].shown && !q[2].shown, "looked at, but not yet faded in: nothing");
	f.handAlpha[0] = 0.5f;
	PlaceHandHud(s, f, q);
	Check(q[0].shown && Near(q[0].alpha, 0.5f) && q[0].onDevice && !q[2].shown,
	      "the left faded half in: its bars at half, the right still hidden");
	HandHudSettings still = s;
	still.offsetRightMetres = 0.0f;
	still.offsetUpMetres = 0.0f;
	HandHudQuad plain[kHudElementCount];
	PlaceHandHud(still, f, plain);
	HandHudSettings shifted = still;
	shifted.offsetUpMetres = -0.05f;
	HandHudQuad lower[kHudElementCount];
	PlaceHandHud(shifted, f, lower);
	const openvr::HmdMatrix34 a = PalmPanelPose(f.hand[0], false, head, s.palmLiftMetres, s.palmDownMetres);
	const openvr::HmdMatrix34 b = AlongOwnXY(a, 0.0f, -0.05f);
	const openvr::HmdMatrix34 wantPlain = RelativeToDevice(f.hand[0].pose, AlongOwnX(a, 0.0f));
	const openvr::HmdMatrix34 wantLower = RelativeToDevice(f.hand[0].pose, AlongOwnX(b, 0.0f));
	Check(Near(plain[0].pose.m[1][3], wantPlain.m[1][3]) && Near(lower[0].pose.m[1][3], wantLower.m[1][3]) &&
	          Near(lower[0].pose.m[2][3], wantLower.m[2][3]),
	      "the Y setting moves the palm's HUD down on its face");
	f.hand[0].valid = false;
	PlaceHandHud(s, f, q);
	Check(!q[0].shown, "the hand lost: hidden");
}

}  // namespace

void TestTarget() {
	std::printf("What the crosshair is on, under it\n");
	HudPlace p = HudPlace::View;
	Check(ParseHudPlace("target", p) && p == HudPlace::Target && HudPlaceFromIndex(7.0f) == HudPlace::Target,
	      "\"target\" and row value 7: under the target");
	HandHudSettings s;
	Check(s.element[static_cast<UInt32>(HudElement::Info)].place == HudPlace::Target &&
	          HudElementInInfoMenu(HudElement::Info) && !HudElementInInfoMenu(HudElement::Bars),
	      "the info is under the target by default, read from HUDInfoMenu");
	HudTile t[4] = {Tile("HUDInfoMenu", -1, 0, 0), Tile("hudinfo_name", 0, 1600, 880, 120, 45),
	                Tile("hudinfo_action_icon", 0, 1580, 827, 64, 64), Tile("elsewhere", -1, 10, 10, 5, 5)};
	const UiRect info = HudElementRect(t, 4, HudElement::Info);
	Check(info.valid && Near(info.left, 1600) && Near(info.top, 880) && Near(info.right, 1720) && Near(info.bottom, 925),
	      "the info: every tile under HUDInfoMenu but its action icon, nothing else");
	HudTile noIcon[3] = {Tile("HUDInfoMenu", -1, 0, 0), Tile("hudinfo_name", 0, 1600, 880, 120, 45),
	                     Tile("elsewhere", -1, 10, 10, 5, 5)};
	const UiRect infoNoIcon = HudElementRect(noIcon, 3, HudElement::Info);
	Check(infoNoIcon.valid && Near(infoNoIcon.left, 1600) && Near(infoNoIcon.bottom, 925),
	      "the info without an action icon tile: the same");

	const NiPoint3 hit{10.0f, 20.0f, 104.0f};
	const NiPoint3 low = TargetHangPoint(hit, true, NiPoint3{10.0f, 20.0f, 100.0f}, 5.0f);
	Check(Near(low.x, 10.0f) && Near(low.y, 20.0f) && Near(low.z, 92.0f),
	      "a small thing: under its bound, a little gap lower");
	const NiPoint3 tall = TargetHangPoint(NiPoint3{0, 0, 80.0f}, true, NiPoint3{0, 0, 50.0f}, 70.0f);
	Check(Near(tall.z, 27.0f), "a person or a door: no more than 20 units below its middle");
	const NiPoint3 bare = TargetHangPoint(hit, false, NiPoint3{}, 0.0f);
	Check(Near(bare.z, 101.0f), "no bound: just under the hit");
	const NiPoint3 above = TargetHangPoint(NiPoint3{0, 0, 10.0f}, true, NiPoint3{0, 0, 50.0f}, 5.0f);
	Check(Near(above.z, 7.0f), "hit below its bound's bottom: under the hit");
	const NiPoint3 aside = TargetHangPoint(NiPoint3{14.0f, 23.0f, 104.0f}, true, NiPoint3{10.0f, 20.0f, 100.0f}, 5.0f);
	Check(Near(aside.x, 10.0f) && Near(aside.y, 20.0f), "a small thing hit off its middle: under the middle");
	const NiPoint3 wide = TargetHangPoint(NiPoint3{14.0f, 23.0f, 80.0f}, true, NiPoint3{10.0f, 20.0f, 50.0f}, 70.0f);
	Check(Near(wide.x, 14.0f) && Near(wide.y, 23.0f), "a large thing: under the hit, not its far middle");

	HandHudFrame f;
	f.haveHead = true;
	f.head.m[0][0] = f.head.m[1][1] = f.head.m[2][2] = 1.0f;
	f.head.m[1][3] = 1.6f;
	f.rect[static_cast<UInt32>(HudElement::Info)] = UiRect{0.0f, 0.0f, 200.0f, 50.0f, true};
	HandHudQuad q[kHudElementCount];
	PlaceHandHud(s, f, q);
	Check(!q[static_cast<UInt32>(HudElement::Info)].shown, "nothing under the crosshair: not shown");
	f.targetValid = true;
	f.targetDistanceMetres = 2.4f;
	f.target = TargetRowPose(f.head, 0.0f, 1.2f, -2.35f);
	PlaceHandHud(s, f, q);
	const HandHudQuad& iq = q[static_cast<UInt32>(HudElement::Info)];
	Check(iq.shown && !iq.onDevice, "something under it: shown, in the room");
	Check(Near(iq.widthMetres, 200.0f * s.viewUnitMetres * 2.0f),
	      "twice the view's distance away: twice as wide, so it looks as large");
	{
		const float dx = iq.pose.m[0][3] - 0.0f, dy = iq.pose.m[1][3] - 1.2f, dz = iq.pose.m[2][3] + 2.35f;
		const float half = iq.widthMetres * 0.25f * 0.5f;
		Check(Near(dx * dx + dy * dy + dz * dz, half * half, 1e-5f) && dy < 0.0f,
		      "hung from its top edge: half its height under the anchor");
	}
	Check(iq.pose.m[2][2] > 0.98f && iq.pose.m[1][2] > 0.0f && Near(iq.pose.m[1][0], 0.0f),
	      "its face turned to the eyes, level");
}

int main() {
	TestTarget();
	TestPlaces();
	TestTiles();
	TestElements();
	TestCapture();
	TestAtlas();
	TestLayout();
	TestCompass();
	TestPlacement();
	TestText();
	TestViewAnchor();
	TestLook();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
