// Checks where the game's menu is for the pointing hand (vr/MenuPlace.h):
// the tablet in the off hand before the menu wrist before the big quad, the
// quad a hand carries, the laser's and the finger's pixel inside the
// tablet's crop, and - through HandMode::Update - the tablet's flows in the
// hand mode: who points, the click.

#include <cstdio>

#include "vr/MenuPlace.h"

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

bool NearPoint(const NiPoint3& a, const NiPoint3& b, float tolerance = 0.001f) {
	return Near(a.x, b.x, tolerance) && Near(a.y, b.y, tolerance) && Near(a.z, b.z, tolerance);
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

// Rays along the tracked -z from the controller's origin, no offsets, the
// click on the pull and the cursor placed where the beam meets the quad.
HandSettings PlainSettings() {
	HandSettings s;
	s.enabled = true;
	s.laserOffsetRightMetres = 0.0f;
	s.laserOffsetUpMetres = 0.0f;
	s.laserPitchDegrees = 0.0f;
	s.laserYawDegrees = 0.0f;
	s.laserOriginMetres = 0.0f;
	s.laserDragScroll = false;
	s.laserCursorDirect = true;
	// An upright tablet 0.4 m wide, held at the middle of its lower edge.
	s.tablet.enabled = true;
	s.tablet.unroll = TabletUnroll::Vertical;  // opened by the B: held from below, the clipboard
	s.handGripUpMetres = 0.0f;  // the drawn hand at the controller's origin, but where a test says
	s.tablet.widthMetres = 0.40f;
	s.tablet.gripUp = 0.0f;
	s.tablet.gripForward = 0.0f;
	s.tablet.tiltDegrees = 0.0f;
	s.tablet.gripShare = 0.5f;
	return s;
}

// In the world with the pages up: an 800 x 800 pixel page at 100,200 of a
// 1600 x 1200 layer, the off hand half a metre ahead of the head.
HandModeFrame TabletFrameUp() {
	HandModeFrame f;
	f.headValid = true;
	f.menuMode = true;
	f.inWorld = true;
	f.layerPixelsWidth = 1600.0f;
	f.layerPixelsHeight = 1200.0f;
	f.cursorValid = true;
	f.cursorX = 800.0f;
	f.cursorY = 600.0f;
	f.tablet.up = true;
	f.tablet.paper = Rect(100.0f, 200.0f, 900.0f, 1000.0f);
	f.tablet.crop = f.tablet.paper;
	f.left.valid = true;
	f.left.position = NiPoint3{0.0f, 0.0f, -0.5f};
	f.right.valid = true;
	f.right.position = NiPoint3{0.1f, 0.3f, 0.0f};
	// The big quad the layer would hang on otherwise.
	f.menuQuad.valid = true;
	f.menuQuad.centre = NiPoint3{0.0f, 0.0f, -2.0f};
	f.menuQuad.width = 1.6f;
	f.menuQuad.height = 1.2f;
	return f;
}

void TestPlace() {
	std::printf("Where the menu is\n");
	const HandSettings s = PlainSettings();
	HandModeFrame f = TabletFrameUp();
	MenuPlaceResult r;
	MenuTarget t = PlaceMenu(f, s, true, r);
	Check(r.menuOnTablet && !r.menuOnWrist, "the tablet's menus up, the off hand tracked: on the tablet");
	Check(Near(r.tabletWidth, 0.40f) && Near(r.tabletHeight, 0.40f), "the tablet's size handed on");
	Check(t.quad.valid && NearPoint(t.quad.centre, NiPoint3{0.0f, 0.2f, -0.5f}) && Near(t.quad.width, 0.40f) &&
	          Near(t.quad.height, 0.40f),
	      "the quad rises from the off hand: its centre half its height above the grip");
	Check(NearPoint(t.quad.right, NiPoint3{1.0f, 0.0f, 0.0f}) && NearPoint(t.quad.up, NiPoint3{0.0f, 1.0f, 0.0f}),
	      "its axes the hand's own with no tilt");
	Check(t.pointHand == &f.right && t.pointRight, "the weapon hand points and taps");
	Check(Near(t.pixelLeft, 100.0f) && Near(t.pixelTop, 200.0f) && Near(t.pixelWidth, 800.0f) &&
	          Near(t.pixelHeight, 800.0f),
	      "the quad shows the crop of the layer, not the whole");

	// The hand turned: the quad turns with it. A quarter turn about y puts
	// the hand's x along the tracked -z.
	f.left.orientation = Quaternion{0.0f, 0.7071068f, 0.0f, 0.7071068f};
	r = MenuPlaceResult{};
	t = PlaceMenu(f, s, true, r);
	Check(NearPoint(t.quad.right, NiPoint3{0.0f, 0.0f, -1.0f}) && NearPoint(t.quad.up, NiPoint3{0.0f, 1.0f, 0.0f}),
	      "the quad is carried by the hand's turn");
	f.left.orientation = Quaternion::Identity();

	// The drawn hand sits on its controller by the grip: the page goes with it.
	HandSettings gripped = s;
	gripped.leftHandGripX = 0.01f;
	gripped.leftHandGripY = -0.15f;  // 15 cm back towards the wrist
	gripped.leftHandGripZ = 0.07f;
	gripped.handGripUpMetres = -0.04f;
	gripped.handGripForwardMetres = 0.02f;
	const NiPoint3 hand = DrawnHandOnController(gripped, true);
	Check(Near(hand.x, 0.01f) && Near(hand.y, 0.03f) && Near(hand.z, 0.13f),
	      "the drawn hand on its controller: right, up the grip's up and the shared up, back the grip's back");
	r = MenuPlaceResult{};
	t = PlaceMenu(f, gripped, true, r);
	Check(NearPoint(t.quad.centre, NiPoint3{0.01f, 0.2f + 0.03f, -0.5f + 0.13f}),
	      "the page held by the drawn hand, not at the controller's origin");
	// The palm's middle read from the drawn hand's bones: held there, not at
	// the hand bone, which is the wrist (Nadi, 2026-11-22).
	f.tablet.palmValid[0] = true;
	f.tablet.palm[0] = hand + NiPoint3{0.0f, -0.01f, -0.05f};
	r = MenuPlaceResult{};
	t = PlaceMenu(f, gripped, true, r);
	Check(NearPoint(t.quad.centre, NiPoint3{0.01f, 0.2f + 0.03f - 0.01f, -0.5f + 0.13f - 0.05f}),
	      "the palm read: the page held in the palm, 5 cm on from the wrist");
	f.tablet.palmValid[1] = true;
	f.tablet.palm[1] = NiPoint3{0.3f, 0.0f, 0.0f};
	Check(NearPoint(TabletHoldPoint(f.tablet, false, hand), f.tablet.palm[0]) &&
	          NearPoint(TabletHoldPoint(f.tablet, true, hand), NiPoint3{0.3f, 0.0f, 0.0f}),
	      "each controller's own palm");
	f.tablet.palmValid[0] = f.tablet.palmValid[1] = false;
	Check(NearPoint(TabletHoldPoint(f.tablet, false, hand), hand) && NearPoint(TabletHoldPoint(f.tablet, true, hand), hand),
	      "no palm read: the drawn hand's bone, as before");
	gripped.rightHandGripX = 0.05f;
	f.rolesSwapped = true;
	Check(Near(DrawnHandOnController(gripped, false).x, 0.05f), "the physical right hand's own grip for it");
	r = MenuPlaceResult{};
	t = PlaceMenu(f, gripped, true, r);
	Check(Near(t.quad.centre.x, 0.05f), "left-handed: the off hand is the physical right one, its grip used");
	f.rolesSwapped = false;

	// Left-handed: the off hand is the physical right one, the page hangs to
	// its other side.
	HandSettings side = s;
	side.tablet.gripShare = 0.25f;
	r = MenuPlaceResult{};
	t = PlaceMenu(f, side, true, r);
	Check(Near(t.quad.centre.x, 0.10f), "the off hand the physical left: the page to its right");
	f.rolesSwapped = true;
	r = MenuPlaceResult{};
	t = PlaceMenu(f, side, true, r);
	Check(Near(t.quad.centre.x, -0.10f), "the roles swapped: the page to its left");
	f.rolesSwapped = false;

	// What takes the tablet away.
	f.left.valid = false;
	r = MenuPlaceResult{};
	t = PlaceMenu(f, s, true, r);
	Check(!r.menuOnTablet && NearPoint(t.quad.centre, NiPoint3{0.0f, 0.0f, -2.0f}) && Near(t.pixelWidth, 1600.0f) &&
	          Near(t.pixelLeft, 0.0f),
	      "the off hand not tracked: the big quad and the whole layer");
	f.left.valid = true;
	f.inWorld = false;
	r = MenuPlaceResult{};
	PlaceMenu(f, s, true, r);
	Check(!r.menuOnTablet, "before a game is loaded: no tablet");
	f.inWorld = true;
	f.tablet.up = false;
	r = MenuPlaceResult{};
	PlaceMenu(f, s, true, r);
	Check(!r.menuOnTablet, "the menus up are not the tablet's: no tablet");
	f.tablet.up = true;
	HandSettings off = s;
	off.tablet.enabled = false;
	r = MenuPlaceResult{};
	PlaceMenu(f, off, true, r);
	Check(!r.menuOnTablet, "the tablet switched off: no tablet");
	off = s;
	off.enabled = false;
	r = MenuPlaceResult{};
	PlaceMenu(f, off, true, r);
	Check(!r.menuOnTablet, "the hand mode off: no tablet");
	f.menuMode = false;
	r = MenuPlaceResult{};
	t = PlaceMenu(f, s, true, r);
	Check(!r.menuOnTablet && !t.quad.valid && t.pointHand == nullptr, "no menu: no tablet, no quad, nobody points");
	f.menuMode = true;

	// The tablet before the wrist; the wrist when the tablet is not up.
	HandSettings wrist = s;
	wrist.wristMenu = true;
	r = MenuPlaceResult{};
	PlaceMenu(f, wrist, true, r);
	Check(r.menuOnTablet && !r.menuOnWrist, "the wrist menu on as well: the tablet's menus go to the tablet");
	f.tablet.up = false;
	r = MenuPlaceResult{};
	t = PlaceMenu(f, wrist, true, r);
	Check(!r.menuOnTablet && r.menuOnWrist && r.menuWristRight && t.pointHand == &f.left && !t.pointRight,
	      "another menu: the menu wrist as before, the other hand pointing");
	Check(Near(t.quad.width, wrist.wristMenuWidth) && Near(t.quad.height, wrist.wristMenuWidth * 0.75f) &&
	          Near(t.pixelWidth, 1600.0f) && Near(t.pixelHeight, 1200.0f),
	      "the wrist's quad the layer's shape, showing the whole layer");
	wrist.menuOnRight = false;
	r = MenuPlaceResult{};
	t = PlaceMenu(f, wrist, true, r);
	Check(r.menuOnWrist && !r.menuWristRight && t.pointHand == &f.right && t.pointRight,
	      "the menu wrist the left one: the right hand points");
	f.left.valid = false;
	r = MenuPlaceResult{};
	PlaceMenu(f, wrist, true, r);
	Check(!r.menuOnWrist, "the menu wrist's hand lost: the big quad");
	f.left.valid = true;

	// The big quad: the latch says who points.
	r = MenuPlaceResult{};
	t = PlaceMenu(f, s, true, r);
	Check(t.pointHand == &f.right && t.pointRight, "the big quad: the hand that last pulled points");
	t = PlaceMenu(f, s, false, r);
	Check(t.pointHand == &f.left && !t.pointRight, "the latch on the left: the left hand");
	f.right.valid = false;
	t = PlaceMenu(f, s, true, r);
	Check(t.pointHand == &f.left && !t.pointRight, "the latched hand not tracked: the other one");
	f.right.valid = true;
	f.left.valid = false;
	t = PlaceMenu(f, s, false, r);
	Check(t.pointHand == &f.right && t.pointRight, "and the other way round");

	// No layer size: the quad carried by a hand is no target.
	f = TabletFrameUp();
	f.layerPixelsWidth = 0.0f;
	r = MenuPlaceResult{};
	t = PlaceMenu(f, s, true, r);
	Check(r.menuOnTablet && !t.quad.valid, "the layer without a size: placed, but nothing to point at");
}

void TestPixels() {
	std::printf("The pixel under the laser and the finger\n");
	MenuTarget t;
	t.quad.valid = true;
	t.quad.centre = NiPoint3{0.0f, 0.2f, -0.5f};
	t.quad.width = 0.4f;
	t.quad.height = 0.4f;
	t.pixelLeft = 100.0f;
	t.pixelTop = 200.0f;
	t.pixelWidth = 800.0f;
	t.pixelHeight = 800.0f;
	LaserHit hit = LaserOnTarget(NiPoint3{0.1f, 0.3f, 0.0f}, NiPoint3{0.0f, 0.0f, -1.0f}, t);
	Check(hit.hit && Near(hit.pixelX, 700.0f, 0.1f) && Near(hit.pixelY, 400.0f, 0.1f),
	      "the laser's pixel lies inside the crop, in the layer's own pixels");
	hit = LaserOnTarget(NiPoint3{0.5f, 0.3f, 0.0f}, NiPoint3{0.0f, 0.0f, -1.0f}, t);
	Check(!hit.hit && hit.pixelX == 0.0f && hit.pixelY == 0.0f, "a beam past the quad hits nothing");
	PokeSample poke = PokeOnTarget(NiPoint3{-0.1f, 0.1f, -0.49f}, t);
	Check(poke.inside && Near(poke.depth, 0.01f) && Near(poke.pixelX, 300.0f, 0.1f) && Near(poke.pixelY, 800.0f, 0.1f),
	      "the finger's pixel likewise");
	poke = PokeOnTarget(NiPoint3{-0.5f, 0.1f, -0.49f}, t);
	Check(!poke.inside && poke.pixelX == 0.0f, "a tip beside the quad is not on it");
	// The figure's own page on the tablet: the face 100..1300 wide, the page
	// 100..900, the figure's page beside it, the figure drawn by the game at
	// 1200..1400 and standing on the tablet at 932..1132 (gap 32, scale 1).
	TabletFrame face;
	face.up = true;
	face.paper = Rect(100.0f, 200.0f, 900.0f, 1000.0f);
	face.pageCrop = face.paper;
	SetFigurePage(face, Rect(1200.0f, 400.0f, 1400.0f, 800.0f));
	MenuTarget onTablet = t;
	onTablet.tablet = &face;
	onTablet.pixelLeft = face.crop.left;
	onTablet.pixelTop = face.crop.top;
	onTablet.pixelWidth = face.crop.right - face.crop.left;
	onTablet.pixelHeight = face.crop.bottom - face.crop.top;
	const float metresPerPixel = 0.4f / onTablet.pixelWidth;
	const float figureX = (1032.0f - face.crop.left) * metresPerPixel - 0.2f;
	const float figureY = 0.2f + 0.2f - (600.0f - face.crop.top) * (0.4f / onTablet.pixelHeight);
	hit = LaserOnTarget(NiPoint3{figureX, figureY, 0.0f}, NiPoint3{0.0f, 0.0f, -1.0f}, onTablet);
	Check(hit.hit && Near(hit.pixelX, 1300.0f, 1.0f) && Near(hit.pixelY, 600.0f, 1.0f),
	      "the beam on the figure's page hits the figure where the game draws it");
	const float besideX = (1150.0f - face.crop.left) * metresPerPixel - 0.2f;
	hit = LaserOnTarget(NiPoint3{besideX, figureY, 0.0f}, NiPoint3{0.0f, 0.0f, -1.0f}, onTablet);
	Check(!hit.hit, "on the figure's page beside the figure: nothing");
	poke = PokeOnTarget(NiPoint3{besideX, figureY, -0.49f}, onTablet);
	Check(!poke.inside, "and the finger there presses nothing");
	poke = PokeOnTarget(NiPoint3{figureX, figureY, -0.49f}, onTablet);
	Check(poke.inside && Near(poke.pixelX, 1300.0f, 1.0f), "the finger on the figure: the figure");

	t.pixelLeft = 0.0f;
	t.pixelTop = 0.0f;
	t.pixelWidth = 1600.0f;
	t.pixelHeight = 1200.0f;
	hit = LaserOnTarget(NiPoint3{0.1f, 0.3f, 0.0f}, NiPoint3{0.0f, 0.0f, -1.0f}, t);
	Check(hit.hit && Near(hit.pixelX, 1200.0f, 0.1f) && Near(hit.pixelY, 300.0f, 0.1f),
	      "the whole layer shown: the pixel as it always was");
}

void TestInHandMode() {
	std::printf("The tablet in the hand mode\n");
	const HandSettings s = PlainSettings();
	HandModeFrame f = TabletFrameUp();
	HandMode mode;
	HandModeResult r = mode.Update(f, s);
	Check(r.menuOnTablet && r.laserVisible && r.laserRight, "the tablet up: the weapon hand's beam points at it");
	Check(r.laserHit && Near(r.laserPixelX, 700.0f, 0.5f) && Near(r.laserPixelY, 400.0f, 0.5f),
	      "the beam meets the tablet at the crop's pixel");
	Check(r.cursorWanted && Near(r.cursorWantedX, 700.0f, 0.5f) && Near(r.cursorWantedY, 400.0f, 0.5f),
	      "and the cursor is placed there");
	Check(Near(r.laserLengthMetres, 0.5f, 0.01f), "the beam ends on the tablet");

	// The finger: the tip 8 cm ahead of the hand, a centimetre off the glass.
	f.right.position = NiPoint3{-0.1f, 0.1f, -0.5f + 0.08f + 0.01f};
	r = mode.Update(f, s);
	Check(r.pokeHover && r.pokePress && !r.controls.menuClick,
	      "the finger's tip on the tablet touches it - no click yet (vr/TabletTouch.h)");
	Check(r.cursorWanted && Near(r.cursorWantedX, 300.0f, 0.5f) && Near(r.cursorWantedY, 800.0f, 0.5f),
	      "with the cursor under it, in the crop's pixels");
	f.right.position.z += 0.05f;
	r = mode.Update(f, s);
	Check(r.controls.menuClick && Near(r.cursorWantedX, 300.0f, 0.5f) && Near(r.cursorWantedY, 800.0f, 0.5f) &&
	          r.tabletTouch.event == TabletTouchEvent::Tap,
	      "lifted off the glass: the click, where it went down");
	r = mode.Update(f, s);
	Check(!r.controls.menuClick, "and the button up the frame after");

	// The off hand lost: the laser goes for the big quad two metres out.
	f.right.position = NiPoint3{0.1f, 0.3f, 0.0f};
	f.left.valid = false;
	r = mode.Update(f, s);
	Check(!r.menuOnTablet && r.laserHit && Near(r.laserPixelX, 900.0f, 0.5f) && Near(r.laserPixelY, 300.0f, 0.5f),
	      "the off hand lost: the big quad again, the whole layer's pixels");
}

// The finger on the tablet's glass as on a touch screen, through the hand
// mode: a swipe down scrolls, a sideways one turns the page, a hold drags,
// through the glass from behind clicks nothing; the tip from the drawn
// index finger where it is read.
void TestTouch() {
	std::printf("The finger on the tablet in the hand mode\n");
	const HandSettings s = PlainSettings();
	const float glass = -0.5f + 0.08f + 0.01f;  // the hand with its tip a centimetre off the glass
	HandModeFrame f = TabletFrameUp();
	f.dtSeconds = 1.0f / 90.0f;
	HandMode mode;
	f.right.position = NiPoint3{0.0f, 0.2f, glass};
	HandModeResult r = mode.Update(f, s);
	int wheel = r.menuScroll;
	bool clicked = r.controls.menuClick;
	for (int i = 1; i <= 30; ++i) {
		f.right.position.y = 0.2f - 0.05f * static_cast<float>(i) / 30.0f;
		r = mode.Update(f, s);
		wheel += r.menuScroll;
		clicked = clicked || r.controls.menuClick;
	}
	f.right.position.z += 0.05f;
	r = mode.Update(f, s);
	wheel += r.menuScroll;
	clicked = clicked || r.controls.menuClick;
	Check(wheel == 2 && !clicked, "the finger drawn 5 cm down the glass: two notches, no click");

	HandMode pages;
	f.right.position = NiPoint3{0.05f, 0.2f, glass};
	int turned = 0;
	for (int i = 0; i <= 20; ++i) {
		f.right.position.x = 0.05f - 0.1f * static_cast<float>(i) / 20.0f;
		r = pages.Update(f, s);
		turned += r.tabletPageStep;
	}
	Check(turned == 1, "drawn 10 cm to the left: the page on the right");

	HandMode hold;
	f.right.position = NiPoint3{0.0f, 0.2f, glass};
	bool down = false;
	for (int i = 0; i < 60; ++i) {
		r = hold.Update(f, s);
		down = down || r.controls.menuClick;
	}
	Check(down && r.controls.menuClick, "held still on the glass: the button down and held");

	HandMode back;
	f.right.position = NiPoint3{0.0f, 0.2f, glass};
	back.Update(f, s);
	f.right.position.z = glass - 0.31f;  // pushed 30 cm through: a hand behind the tablet
	r = back.Update(f, s);
	bool any = r.controls.menuClick;
	f.right.position.z = glass + 0.2f;
	r = back.Update(f, s);
	any = any || r.controls.menuClick;
	Check(!any, "through the glass from behind and away: nothing clicked");

	// The glass gives no resistance: a finger 7 cm through it and back out is
	// a tap (the headset run of 2026-11-20 cut that off at 6 cm).
	HandMode through;
	f.right.position = NiPoint3{0.0f, 0.2f, glass};
	through.Update(f, s);
	f.right.position.z = glass - 0.08f;
	r = through.Update(f, s);
	bool tapped = r.controls.menuClick;
	f.right.position.z = glass - 0.05f;  // pulled back 3 cm, still behind the glass
	r = through.Update(f, s);
	tapped = tapped || r.controls.menuClick;
	Check(tapped && r.tabletTouch.event == TabletTouchEvent::Tap &&
	          r.tabletTouch.end == TabletContactEnd::PulledBack,
	      "7 cm through the glass and 3 cm back: the tap, pulled back behind the glass");

	HandMode bone;
	f.right.position = NiPoint3{-0.1f - 0.05f, 0.1f, glass};
	f.rightIndexTip.valid = true;
	f.rightIndexTip.local = NiPoint3{0.05f, 0.0f, -0.08f};
	r = bone.Update(f, s);
	Check(r.pokeHover && Near(r.cursorWantedX, 300.0f, 0.5f) && Near(r.cursorWantedY, 800.0f, 0.5f),
	      "the drawn index finger read: its tip taps, 5 cm beside the controller's");
	f.rightIndexTip.valid = false;
	r = bone.Update(f, s);
	Check(r.pokeHover && Near(r.cursorWantedX, 200.0f, 0.5f), "not read: the controller's tip again");
	HandMode off;
	HandSettings controllerTip = s;
	controllerTip.tablet.drawnFinger = false;
	f.rightIndexTip.valid = true;
	r = off.Update(f, controllerTip);
	Check(r.pokeHover && Near(r.cursorWantedX, 200.0f, 0.5f), "[Tablet] DrawnFinger=0: the controller's tip though read");
}

void TestTabs() {
	std::printf("The figure's tab in the hand mode\n");
	const HandSettings s = PlainSettings();
	HandModeFrame f = TabletFrameUp();
	SetFigureTab(f.tablet, Rect(2000.0f, 100.0f, 2400.0f, 1100.0f));
	HandMode mode;
	// The beam on the figure's tab: 404..628 by 128..200 on the face, the
	// page 800 pixels to 0.4 m, held at its middle.
	f.right.position = NiPoint3{(516.0f - 500.0f) * 0.0005f, (1000.0f - 164.0f) * 0.0005f, 0.0f};
	HandModeResult r = mode.Update(f, s);
	Check(r.menuOnTablet && !r.tabletShowsFigure, "the beam on the figure's tab: the page still shown");
	Check(!r.laserHit, "the tab is nothing of the game's: no hit for the cursor");
	f.right.trigger = 1.0f;
	r = mode.Update(f, s);
	Check(r.tabletShowsFigure, "the trigger pulled on it: the figure shown");
	f.right.trigger = 0.0f;
	f.right.position = NiPoint3{0.0f, (1000.0f - 600.0f) * 0.0005f, 0.0f};
	r = mode.Update(f, s);
	Check(r.tabletShowsFigure && r.laserHit && r.laserPixelX > 2000.0f,
	      "it stays; the beam on the page's middle hits the figure where the game draws it");
	f.tablet.popupUp = true;
	r = mode.Update(f, s);
	Check(!r.tabletShowsFigure, "a popup over the page: the page");
	f.tablet.popupUp = false;
	// The finger on the page's tab: 150..374.
	f.right.position = NiPoint3{(262.0f - 500.0f) * 0.0005f, (1000.0f - 164.0f) * 0.0005f, -0.5f + 0.08f + 0.01f};
	r = mode.Update(f, s);
	Check(!r.tabletShowsFigure, "the finger through the page's tab: the page");
}

void TestHolds() {
	std::printf("Where the hand holds it\n");
	HandSettings s = PlainSettings();
	s.tablet.unroll = TabletUnroll::Sideways;
	HandModeFrame f = TabletFrameUp();
	MenuPlaceResult r;
	MenuTarget t = PlaceMenu(f, s, true, r);
	// The page 800 pixels to 0.4 m; the face 100..900 by 200..1000, the
	// left hand at its left edge's middle.
	Check(r.menuOnTablet && NearPoint(t.quad.centre, NiPoint3{0.2f, 0.0f, -0.5f}),
	      "sideways: held by its left edge, the page's middle at the hand's height");
	s.tablet.unroll = TabletUnroll::Vertical;
	r = MenuPlaceResult{};
	t = PlaceMenu(f, s, true, r);
	Check(NearPoint(t.quad.centre, NiPoint3{0.0f, 0.2f, -0.5f}), "up and down, opened by the B: held by its bottom edge");
	TabletFrame face = f.tablet;
	TabletSettings ts = s.tablet;
	TabletPlace top = PlaceTablet(face, ts, true, NiPoint3{0.0f, 0.0f, 0.0f}, TabletHold::Top);
	Check(top.valid && Near(top.transform.m[1][3], -0.2f) && Near(top.transform.m[0][3], 0.0f),
	      "kept by the upper hand: held by its top edge, hanging below");
	TabletPlace right = PlaceTablet(face, ts, false, NiPoint3{0.0f, 0.0f, 0.0f}, TabletHold::Side);
	Check(right.valid && Near(right.transform.m[0][3], -0.2f), "the right hand at a side: its right edge");
	Check(HoldFor(TabletUnroll::Sideways, true) == TabletHold::Side &&
	          HoldFor(TabletUnroll::Vertical, false) == TabletHold::Bottom &&
	          HoldFor(TabletUnroll::Vertical, true) == TabletHold::Top,
	      "the hold by the setting and the hand that kept it");
	TabletUnroll u = TabletUnroll::Sideways;
	Check(ParseTabletUnroll("Vertical", u) && u == TabletUnroll::Vertical && !ParseTabletUnroll("up", u),
	      "the setting's words");
	Check(TabletUnrollFromIndex(1.0f) == TabletUnroll::Vertical && TabletUnrollFromIndex(0.0f) == TabletUnroll::Sideways &&
	          TabletUnrollFromIndex(7.0f) == TabletUnroll::Sideways && TabletSettings{}.unroll == TabletUnroll::Sideways,
	      "the row's value, out of range and the default: sideways");
}

// The tablet held like the sword (Nadi, 2026-11-24): the hand's held-item
// node's axes, read from the bones into the tablet's frame, turn the page -
// the off hand's in its hand, the weapon hand's when that one kept it.
void TestSword() {
	std::printf("Held like the sword\n");
	HandSettings s = PlainSettings();
	s.tablet.unroll = TabletUnroll::Sideways;
	HandModeFrame f = TabletFrameUp();
	HeldAxes left;
	left.valid = true;
	left.blade = NiPoint3{0.0f, 0.6f, -0.8f};
	left.edge = NiPoint3{0.0f, -0.8f, -0.6f};
	f.tablet.held[0] = left;
	MenuPlaceResult r;
	MenuTarget t = PlaceMenu(f, s, true, r);
	Check(r.menuOnTablet && NearPoint(t.quad.up, left.blade) && NearPoint(t.quad.right, left.edge),
	      "the off hand's node read: the page's up the blade, its right towards the knuckles");
	Check(NearPoint(t.quad.centre, f.left.position + left.edge * 0.2f),
	      "held at its side in the hand, across the knuckles");
	// A quarter turn about y: the page goes with the hand.
	f.left.orientation = Quaternion{0.0f, 0.7071068f, 0.0f, 0.7071068f};
	r = MenuPlaceResult{};
	t = PlaceMenu(f, s, true, r);
	Check(NearPoint(t.quad.up, Rotate(f.left.orientation, left.blade)) &&
	          NearPoint(t.quad.right, Rotate(f.left.orientation, left.edge)),
	      "turned with the hand, as the sword is");
	f.left.orientation = Quaternion::Identity();

	// Kept by the weapon hand: its own node's axes, the page to the left of it.
	HeldAxes right;
	right.valid = true;
	right.blade = NiPoint3{0.0f, 0.6f, -0.8f};
	right.edge = NiPoint3{0.0f, -0.8f, -0.6f};
	f.tablet.held[1] = right;
	const TabletPlace kept = HeldByRight(f, s, TabletHold::Side);
	const NiPoint3 keptRight{kept.transform.m[0][0], kept.transform.m[1][0], kept.transform.m[2][0]};
	const NiPoint3 keptUp{kept.transform.m[0][1], kept.transform.m[1][1], kept.transform.m[2][1]};
	Check(kept.valid && NearPoint(keptUp, right.blade) && NearPoint(keptRight, right.edge * -1.0f),
	      "kept by the right hand: its blade up, the page reaching back across its knuckles");
	f.tablet.held[0].valid = false;
	r = MenuPlaceResult{};
	t = PlaceMenu(f, s, true, r);
	Check(NearPoint(t.quad.right, NiPoint3{1.0f, 0.0f, 0.0f}) && NearPoint(t.quad.up, NiPoint3{0.0f, 1.0f, 0.0f}),
	      "the off hand's node not read: on the controller as before");
}

void TestScrollUpright() {
	std::printf("The scroll up and down\n");
	HandSettings s = PlainSettings();
	// The inventory ring where the hands begin: the scroll begins there only.
	s.stow.centreForward = 0.5f;
	s.stow.centreUp = -0.3f;
	HandModeFrame f;
	f.headValid = true;
	f.inWorld = true;
	f.dtSeconds = 0.05f;
	f.layerPixelsWidth = 1600.0f;
	f.layerPixelsHeight = 1200.0f;
	f.left.valid = true;
	f.right.valid = true;
	f.left.position = NiPoint3{0.0f, -0.36f, -0.5f};
	f.right.position = NiPoint3{0.0f, -0.24f, -0.5f};
	HandMode mode;
	for (int i = 0; i < 9; ++i) {
		mode.Update(f, s);
	}
	const UInt64 grip = 1ull << openvr::kButtonIndexGrip;
	f.left.buttonsPressed = grip;
	f.right.buttonsPressed = grip;
	HandModeResult r = mode.Update(f, s);
	Check(r.controls.menu, "hands one above the other, grips closed: the menus open");
	f.menuMode = true;
	f.tablet = TabletFrameUp().tablet;
	f.left.position = NiPoint3{0.0f, -0.5f, -0.5f};
	f.right.position = NiPoint3{0.0f, -0.1f, -0.5f};
	r = mode.Update(f, s);
	Check(r.menuOnTablet && r.tabletUpright && Near(r.tabletShare, 1.0f), "pulled apart up and down past its height: open");
	Check(Near(r.menuTransform.m[1][1], 1.0f) && Near(r.menuTransform.m[1][3], 0.2f) && Near(r.menuTransform.m[0][0], 1.0f),
	      "upright between the hands, its middle between them, facing the eyes");
	f.left.buttonsPressed = 0;
	r = mode.Update(f, s);
	Check(r.tabletHolderRight, "the lower hand let go: kept by the upper one");
}

void TestScroll() {
	std::printf("The scroll in the hand mode\n");
	HandSettings s = PlainSettings();
	s.tablet.unroll = TabletUnroll::Sideways;
	s.stow.centreForward = 0.5f;  // the inventory ring where the hands begin
	s.stow.centreUp = -0.3f;
	HandModeFrame f;
	f.headValid = true;
	f.inWorld = true;
	f.dtSeconds = 0.05f;
	f.layerPixelsWidth = 1600.0f;
	f.layerPixelsHeight = 1200.0f;
	f.left.valid = true;
	f.right.valid = true;
	f.left.position = NiPoint3{-0.06f, -0.3f, -0.5f};
	f.right.position = NiPoint3{0.06f, -0.3f, -0.5f};
	HandMode mode;
	HandModeResult r;
	for (int i = 0; i < 9; ++i) {
		r = mode.Update(f, s);
	}
	Check(!r.controls.menu, "the pose held with open grips: nothing yet");
	const UInt64 grip = 1ull << openvr::kButtonIndexGrip;
	f.left.buttonsPressed = grip;
	f.right.buttonsPressed = grip;
	r = mode.Update(f, s);
	Check(r.controls.menu && r.tabletGrips, "both grips closed: the menu key, the menus open as it unrolls; the grips are the scroll's");
	// The menus up, the hands pulled 0.3 m apart.
	const HandModeFrame up = TabletFrameUp();
	f.menuMode = true;
	f.tablet = up.tablet;
	f.left.position = NiPoint3{-0.15f, -0.3f, -0.5f};
	f.right.position = NiPoint3{0.15f, -0.3f, -0.5f};
	r = mode.Update(f, s);
	Check(r.menuOnTablet && !r.controls.menu && Near(r.tabletShare, (0.3f - 0.08f) / (0.4f - 0.08f)),
	      "unrolling between the hands by their distance");
	Check(Near(r.menuTransform.m[0][3], 0.15f) && Near(r.menuTransform.m[1][3], 0.0f) &&
	          Near(r.menuTransform.m[0][0], 1.0f),
	      "its middle between the hands, across from the left to the right, as the left controller's place");
	f.left.position = NiPoint3{-0.25f, -0.3f, -0.5f};
	f.right.position = NiPoint3{0.25f, -0.3f, -0.5f};
	r = mode.Update(f, s);
	Check(Near(r.tabletShare, 1.0f), "pulled past its width: fully open");
	f.left.buttonsPressed = 0;
	r = mode.Update(f, s);
	Check(r.menuOnTablet && r.tabletHolderRight && r.tabletShare < 0.0f && !r.controls.menu,
	      "the left let go: kept by the right hand, the menus open");
	Check(r.laserVisible && !r.laserRight, "the left hand points at it now");
	// Opened by the left B instead: held by the left hand.
	HandMode byButton;
	HandModeFrame b = TabletFrameUp();
	b.dtSeconds = 0.05f;
	r = byButton.Update(b, s);
	Check(r.menuOnTablet && !r.tabletHolderRight && r.tabletShare < 0.0f, "opened by the B: in the left hand, by time");

	// The scroll measures the hands at their palms (Nadi's grip puts the hand
	// bone 18 cm behind the controller: two hands held together read 29 to 35 cm
	// apart there, 2026-11-22).
	HandModeFrame palms = f;
	HandSettings far = s;
	far.leftHandGripY = far.rightHandGripY = -0.18f;
	palms.left.position = NiPoint3{-0.06f, -0.3f, -0.5f};
	palms.right.position = NiPoint3{0.06f, -0.3f, -0.5f};
	palms.left.orientation = Quaternion{0.0f, -0.3826834f, 0.0f, 0.9238795f};  // both pointing 45 degrees in
	palms.right.orientation = Quaternion{0.0f, 0.3826834f, 0.0f, 0.9238795f};
	TabletScrollFrame g = ScrollFrameOf(palms, far, TabletPlace{});
	Check(Distance(g.left, g.right) > 0.25f, "no palm read: the hand bones, turned in behind the controllers, far apart");
	palms.tablet.palmValid[0] = palms.tablet.palmValid[1] = true;
	palms.tablet.palm[0] = NiPoint3{0.0f, -0.02f, 0.03f};
	palms.tablet.palm[1] = NiPoint3{0.0f, -0.02f, 0.03f};
	g = ScrollFrameOf(palms, far, TabletPlace{});
	Check(Distance(g.left, g.right) < 0.20f &&
	          NearPoint(g.left, palms.left.position + Rotate(palms.left.orientation, palms.tablet.palm[0])),
	      "the palms read: measured there, together");
}

}  // namespace

int main() {
	TestPlace();
	TestPixels();
	TestInHandMode();
	TestTabs();
	TestTouch();
	TestScroll();
	TestHolds();
	TestSword();
	TestScrollUpright();
	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
