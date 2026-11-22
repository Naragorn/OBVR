#pragma once

// The player's own menus as a tablet in the off hand
// (docs/inventory-tablet-spec.md, stage 1): which menus are the tablet's,
// the rectangle of the interface's picture it is cut to, where it stands on
// the hand that holds it, and its corners for the draw into the scene. Pure; covered by tablet_test.
//
// The frames: a rectangle is in the HUD's units (vr/HudTileTree.h, 960 high)
// or in the layer's pixels, as named; the hand's frame is OpenVR's controller
// frame - x right, y up, z back towards the wrist - in metres.

#include "core/ChoiceWord.h"
#include "core/MathFns.h"
#include "core/Types.h"
#include "game/MenuType.h"
#include "game/NiMath.h"
#include "vr/HudTileTree.h"
#include "vr/OpenVRTypes.h"

namespace obvr::vr {

// Where the menu's figure is on the tablet ([Tablet] Figure; Nadi, 2026-11-18):
// on a page of its own beside the page, not on the tablet, or behind a tab
// of the tablet's own - the tablet then as small as without it.
enum class TabletFigure : UInt8 { Beside = 0, Off = 1, Tab = 2 };
inline constexpr UInt32 kTabletFigureCount = 3;
inline constexpr const char* kTabletFigureNames[kTabletFigureCount] = {"beside", "off", "tab"};

inline bool ParseTabletFigure(const char* text, TabletFigure& out) {
	UInt32 index = 0;
	if (!MatchChoiceWord(text, kTabletFigureNames, kTabletFigureCount, index)) {
		return false;
	}
	out = static_cast<TabletFigure>(index);
	return true;
}

// The settings row's value (0..2); out of range is the default, the tab.
inline TabletFigure TabletFigureFromIndex(float value) {
	if (!(value > -0.5f) || !(value < static_cast<float>(kTabletFigureCount) - 0.5f)) {
		return TabletFigure::Tab;
	}
	return static_cast<TabletFigure>(static_cast<int>(value + 0.5f));
}

// How the scroll is held to unroll it ([Tablet] Unroll; Nadi, 2026-11-18):
// by its sides, the hands pulled apart left and right, or by its top and
// bottom edges, pulled apart up and down.
enum class TabletUnroll : UInt8 { Sideways = 0, Vertical = 1 };
inline constexpr UInt32 kTabletUnrollCount = 2;
inline constexpr const char* kTabletUnrollNames[kTabletUnrollCount] = {"sideways", "vertical"};

inline bool ParseTabletUnroll(const char* text, TabletUnroll& out) {
	UInt32 index = 0;
	if (!MatchChoiceWord(text, kTabletUnrollNames, kTabletUnrollCount, index)) {
		return false;
	}
	out = static_cast<TabletUnroll>(index);
	return true;
}

inline TabletUnroll TabletUnrollFromIndex(float value) {
	return value > 0.5f && value < 1.5f ? TabletUnroll::Vertical : TabletUnroll::Sideways;
}

// [Tablet].
struct TabletSettings {
	bool enabled = true;  // off is the onboarding's "Traditional inventory"
	// How wide the page stands in the hand, metres. A page wider than the
	// others (the map's) reaches further out at the same scale.
	float widthMetres = 0.34f;
	// Drawn into the eyes' pictures, so the tapping hand is seen in front of
	// it; off, the overlay that covers the hand (spec section 3).
	bool inScene = true;
	// The tap's tip from the drawn index finger (vr/FingerTip.h); off, the controller's tip ahead.
	bool drawnFinger = true;
	TabletFigure figure = TabletFigure::Tab;  // Nadi, 2026-11-18: the tab by default
	TabletUnroll unroll = TabletUnroll::Sideways;
	// Where the page's lower edge meets the hand: above and ahead of the
	// controller's origin, the page leant back at the top by the tilt, and
	// how far along the lower edge from the holding hand's outer side.
	float gripUp = 0.03f;
	float gripForward = 0.02f;
	float tiltDegrees = 30.0f;
	float gripShare = 0.25f;
};

// ------------------------------------------------------------- Which menus

// A menu the tablet carries: the F1-F4 pages and what they raise.
inline bool TabletCarriesMenu(UInt32 menuId) {
	switch (menuId) {
	case game::kMenuIdBigFour:
	case game::kMenuIdInventory:
	case game::kMenuIdStats:
	case game::kMenuIdMagic:
	case game::kMenuIdMap:
	case game::kMenuIdQuantity:
	case game::kMenuIdMagicPopup:
	case game::kMenuIdMessage:
	case game::kMenuIdBook:
		return true;
	default:
		return false;
	}
}

// The HUD's own entries, which the stack can hold beside any menu.
inline bool IsHudMenu(UInt32 menuId) {
	return menuId == game::kMenuIdHudMain || menuId == game::kMenuIdHudInfo ||
	       menuId == game::kMenuIdHudReticle || menuId == game::kMenuIdHudSubtitle;
}

// Whether the menus that are up are the tablet's: the F1-F4 entry in the
// stack, and nothing beside it that the tablet does not carry - a
// conversation, a container, the Esc menu keep their own panel.
inline bool TabletMenusUp(const UInt32* stack, UInt32 count) {
	bool pages = false;
	for (UInt32 i = 0; stack != nullptr && i < count; ++i) {
		if (IsHudMenu(stack[i])) {
			continue;
		}
		if (!TabletCarriesMenu(stack[i])) {
			return false;
		}
		pages = pages || stack[i] == game::kMenuIdBigFour;
	}
	return pages;
}

// Whether the world keeps running behind the menu on top while the tablet's
// menus are up. A message box and a book pause as vanilla: a question waits
// for its answer, a page is read in peace.
inline bool TabletMenuRunsWorld(UInt32 topMenuId) {
	return TabletCarriesMenu(topMenuId) && topMenuId != game::kMenuIdMessage &&
	       topMenuId != game::kMenuIdBook;
}

// ---------------------------------------------------------- The rectangle

// A drawn picture of a menu - a tile with a file, shown, not clear - by its
// rectangle in HUD units. What game::ReadMenuPaper hands over.
struct PaperTile {
	float left = 0.0f;
	float top = 0.0f;
	float right = 0.0f;
	float bottom = 0.0f;
};

// A menu's page and what the tablet shows of the picture for it.
struct TabletRects {
	bool valid = false;
	UiRect paper;  // the page: the menu's largest picture
	UiRect crop;   // the page and what reaches beyond it (the map's extension)
};

// A picture at least this share of the page's area belongs to the page's
// outline (the map's extension is 0.59 of it; the tabs' strip, whose texture
// is wider than what it shows, 0.24).
inline constexpr float kPaperJoinShare = 0.4f;
// A page smaller than this share of the screen is no page: nothing to hold.
inline constexpr float kPaperLeastShare = 0.03f;

inline float RectArea(const UiRect& r) {
	return r.valid ? (r.right - r.left) * (r.bottom - r.top) : 0.0f;
}

// A tile's rectangle cut to the screen; invalid when nothing of it is on it.
inline UiRect OnScreen(const PaperTile& t, float screenWidth, float screenHeight) {
	UiRect r;
	r.left = t.left > 0.0f ? t.left : 0.0f;
	r.top = t.top > 0.0f ? t.top : 0.0f;
	r.right = t.right < screenWidth ? t.right : screenWidth;
	r.bottom = t.bottom < screenHeight ? t.bottom : screenHeight;
	r.valid = r.right > r.left && r.bottom > r.top;
	return r;
}

// The page of one menu from its pictures: the largest is the page, and every
// picture of at least kPaperJoinShare of its area joins the crop. Invalid
// with no picture, a screen without a size, or a page too small to be one.
inline TabletRects PaperOf(const PaperTile* tiles, UInt32 count, float screenWidth, float screenHeight) {
	TabletRects out;
	if (tiles == nullptr || !(screenWidth > 0.0f) || !(screenHeight > 0.0f)) {
		return out;
	}
	float most = 0.0f;
	for (UInt32 i = 0; i < count; ++i) {
		const UiRect r = OnScreen(tiles[i], screenWidth, screenHeight);
		if (RectArea(r) > most) {
			most = RectArea(r);
			out.paper = r;
		}
	}
	if (!(most >= kPaperLeastShare * screenWidth * screenHeight)) {
		return TabletRects{};
	}
	out.crop = out.paper;
	for (UInt32 i = 0; i < count; ++i) {
		const UiRect r = OnScreen(tiles[i], screenWidth, screenHeight);
		if (RectArea(r) >= kPaperJoinShare * most) {
			out.crop = UnionRect(out.crop, r);
		}
	}
	out.valid = true;
	return out;
}

// What a popup takes of the screen: all its pictures together, however
// small - a message box is no page, and must not be cut off.
inline UiRect PopupRect(const PaperTile* tiles, UInt32 count, float screenWidth, float screenHeight) {
	UiRect out;
	for (UInt32 i = 0; tiles != nullptr && i < count; ++i) {
		out = UnionRect(out, OnScreen(tiles[i], screenWidth, screenHeight));
	}
	return out;
}

// A popup over the page: the crop grows to hold it, the page stays the page.
inline void JoinPopup(TabletRects& page, const UiRect& popup) {
	if (page.valid && popup.valid) {
		page.crop = UnionRect(page.crop, popup);
	}
}

// HUD units to the layer's pixels: the layer is kHudUiHeightUnits high.
inline UiRect ToLayerPixels(const UiRect& units, float layerPixelsHeight) {
	UiRect r = units;
	const float scale = layerPixelsHeight / kHudUiHeightUnits;
	r.left *= scale;
	r.top *= scale;
	r.right *= scale;
	r.bottom *= scale;
	r.valid = units.valid && layerPixelsHeight > 0.0f;
	return r;
}

// ------------------------------------------------------------ On the hand

// What the hand mode is told each frame: the tablet's menus are up and their
// rectangles were read, both in the layer's pixels.
struct TabletFrame {
	bool up = false;
	UiRect paper;
	// The tablet's whole face, in the layer's pixels: what the quad shows and
	// the beam and the finger are mapped through (SurfaceToLayer).
	UiRect crop;
	// The page and its popups, drawn where the game draws them.
	UiRect pageCrop;
	// The figure on a page of its own beside the page (SetFigurePage; Nadi,
	// 2026-11-17: "zweite Seite daneben"): where the game draws it, where it
	// stands on the tablet and how much smaller, and the page behind it.
	bool figurePage = false;
	UiRect figure;
	UiRect figureAt;
	float figureScale = 1.0f;
	UiRect backing;
	// The figure behind a tab (SetFigureTab): the two tabs over the page's top
	// edge - the page's, the figure's - and the figure fitted into the page's
	// own rectangle, shown there instead of the page while its tab is chosen.
	bool figureTab = false;
	bool popupUp = false;  // a popup over the page: the page is shown whatever tab is chosen
	UInt32 page = 0;       // the page shown: the inventory's, stats', magic's or map's menu id
	UiRect tabs[2];
	UiRect figureFit;
	float figureFitScale = 1.0f;
};

// The figure's page: abutting the page on its right, as high as the page,
// the figure in it a gap in from the page's edge, made smaller to fit the
// page's height with a gap above and below when it is taller, centred up and
// down. The tablet's face grows to hold it. Nothing without the page or the
// figure, or with no room.
inline constexpr float kFigureGapShare = 0.04f;  // of the page's width

inline void SetFigurePage(TabletFrame& f, const UiRect& figure) {
	const float pageWidth = f.paper.right - f.paper.left;
	const float pageHeight = f.paper.bottom - f.paper.top;
	const float figureWidth = figure.right - figure.left;
	const float figureHeight = figure.bottom - figure.top;
	if (!f.up || !f.paper.valid || !figure.valid || !(pageWidth > 0.0f) || !(figureWidth > 0.0f) ||
	    !(figureHeight > 0.0f)) {
		return;
	}
	const float gap = kFigureGapShare * pageWidth;
	const float room = pageHeight - 2.0f * gap;
	if (!(room > 0.0f)) {
		return;
	}
	const float scale = figureHeight > room ? room / figureHeight : 1.0f;
	f.figurePage = true;
	f.figure = figure;
	f.figureScale = scale;
	f.figureAt.left = f.paper.right + gap;
	f.figureAt.right = f.figureAt.left + figureWidth * scale;
	f.figureAt.top = f.paper.top + 0.5f * (pageHeight - figureHeight * scale);
	f.figureAt.bottom = f.figureAt.top + figureHeight * scale;
	f.figureAt.valid = true;
	f.backing.left = f.paper.right;
	f.backing.right = f.figureAt.right + gap;
	f.backing.top = f.paper.top;
	f.backing.bottom = f.paper.bottom;
	f.backing.valid = true;
	f.crop = UnionRect(f.pageCrop.valid ? f.pageCrop : f.paper, f.backing);
}

// The figure behind a tab: two tabs standing on the page's top edge at its
// left - the page's, then the figure's - and the figure fitted into the
// page's rectangle with a gap round it, centred. The face grows up by the
// tabs; the tablet keeps the page's width. Nothing without the page or the
// figure.
inline constexpr float kTabWidthShare = 0.28f;   // of the page's width
inline constexpr float kTabHeightShare = 0.09f;  // of the page's width
inline constexpr float kTabInsetShare = 0.05f;   // from the page's left edge, and between the tabs

inline void SetFigureTab(TabletFrame& f, const UiRect& figure) {
	const float pageWidth = f.paper.right - f.paper.left;
	const float pageHeight = f.paper.bottom - f.paper.top;
	const float figureWidth = figure.right - figure.left;
	const float figureHeight = figure.bottom - figure.top;
	if (!f.up || !f.paper.valid || !figure.valid || !(pageWidth > 0.0f) || !(figureWidth > 0.0f) ||
	    !(figureHeight > 0.0f)) {
		return;
	}
	const float gap = kFigureGapShare * pageWidth;
	const float roomWidth = pageWidth - 2.0f * gap;
	const float roomHeight = pageHeight - 2.0f * gap;
	if (!(roomWidth > 0.0f) || !(roomHeight > 0.0f)) {
		return;
	}
	const float byWidth = roomWidth / figureWidth;
	const float byHeight = roomHeight / figureHeight;
	const float scale = byWidth < byHeight ? byWidth : byHeight;
	f.figureTab = true;
	f.figure = figure;
	f.figureFitScale = scale;
	f.figureFit.left = f.paper.left + 0.5f * (pageWidth - figureWidth * scale);
	f.figureFit.right = f.figureFit.left + figureWidth * scale;
	f.figureFit.top = f.paper.top + 0.5f * (pageHeight - figureHeight * scale);
	f.figureFit.bottom = f.figureFit.top + figureHeight * scale;
	f.figureFit.valid = true;
	const float tabWidth = kTabWidthShare * pageWidth;
	const float tabHeight = kTabHeightShare * pageWidth;
	const float inset = kTabInsetShare * pageWidth;
	for (int i = 0; i < 2; ++i) {
		UiRect& tab = f.tabs[i];
		tab.left = f.paper.left + inset + static_cast<float>(i) * (tabWidth + inset);
		tab.right = tab.left + tabWidth;
		tab.bottom = f.paper.top;
		tab.top = f.paper.top - tabHeight;
		tab.valid = true;
	}
	f.crop = UnionRect(f.pageCrop.valid ? f.pageCrop : f.paper, f.tabs[1]);
	f.crop = UnionRect(f.crop, f.tabs[0]);
}

// Which of the tabs a point on the face lies on: 0 the page's, 1 the
// figure's, -1 neither (or no tabs).
inline int TabletTabAt(const TabletFrame& f, float x, float y) {
	for (int i = 0; f.figureTab && i < 2; ++i) {
		const UiRect& t = f.tabs[i];
		if (x >= t.left && x <= t.right && y >= t.top && y <= t.bottom) {
			return i;
		}
	}
	return -1;
}

// With the figure's tab chosen, a point on the page's rectangle to the
// figure where the game draws it; off the figure (on its paper) nothing.
// The tabs are nothing of the game's either way.
inline bool FigureTabToLayer(const TabletFrame& f, float& x, float& y) {
	if (TabletTabAt(f, x, y) >= 0) {
		return false;
	}
	if (!f.figureTab || !(f.figureFitScale > 0.0f)) {
		return true;
	}
	if (!(x >= f.figureFit.left && x <= f.figureFit.right && y >= f.figureFit.top && y <= f.figureFit.bottom)) {
		return !(x >= f.paper.left && x <= f.paper.right && y >= f.paper.top && y <= f.paper.bottom);
	}
	x = f.figure.left + (x - f.figureFit.left) / f.figureFitScale;
	y = f.figure.top + (y - f.figureFit.top) / f.figureFitScale;
	return true;
}

// A tab chosen: the click's rising edge on a tab picks it; a popup over the
// page shows the page whatever was chosen (the game draws it there).
struct TabletTabState {
	bool showFigure = false;
	bool wasDown = false;
};

inline bool StepTabletTab(TabletTabState& s, bool figureTab, int tabUnder, bool clickDown, bool popupUp) {
	const bool edge = clickDown && !s.wasDown;
	s.wasDown = clickDown;
	if (!figureTab) {
		s.showFigure = false;
		return false;
	}
	if (edge && tabUnder >= 0) {
		s.showFigure = tabUnder == 1;
	}
	return s.showFigure && !popupUp;
}

// A point on the tablet's face to the layer's pixel the game has under it:
// on the figure's page, the figure where the game draws it; elsewhere on that
// page nothing (false); off it, the point itself.
inline bool SurfaceToLayer(const TabletFrame& f, float& x, float& y) {
	if (!f.figurePage || !(x >= f.backing.left && x <= f.backing.right && y >= f.backing.top && y <= f.backing.bottom)) {
		return true;
	}
	if (!(x >= f.figureAt.left && x <= f.figureAt.right && y >= f.figureAt.top && y <= f.figureAt.bottom) ||
	    !(f.figureScale > 0.0f)) {
		return false;
	}
	x = f.figure.left + (x - f.figureAt.left) / f.figureScale;
	y = f.figure.top + (y - f.figureAt.top) / f.figureScale;
	return true;
}

// The tablet on the hand that holds it: the transform from the controller to
// the quad's centre (its columns the quad's right, up and front), and the
// quad's size in metres.
struct TabletPlace {
	bool valid = false;
	openvr::HmdMatrix34 transform{};
	float width = 0.0f;
	float height = 0.0f;
};

// A clipboard held from below: the page's lower edge at the grip, the page
// rising from the fist and leant back at the top, facing the eyes. The page
// is `widthMetres` wide whatever the crop - a wider crop reaches further to
// the side at the same scale. `holderPhysicalLeft`: the holding controller
// is the physical left one (the page then hangs towards the body's middle,
// to its right; mirrored for the right one). `hand` is where the drawn hand
// sits on its controller, in the controller's frame (the hand's grip: the
// harness's first pictures, 2026-11-17, showed the page some 15 cm ahead of
// the hand, at the controller's origin, with the player's grip of 0.15 m back):
// the page is held by the hand the player sees.
// Where the hand holds it (Nadi, 2026-11-18: held at its side, or at its top
// or bottom edge with the scroll unrolled up and down): Bottom - the clipboard
// held from below, GripShare along the lower edge, GripUp and GripForward off
// the hand; Side - the hand at the face's outer edge (the left hand its left
// edge, the right hand its right), the page's middle at the hand's height;
// Top - the hand at the top edge's middle, the page hanging below.
enum class TabletHold : UInt8 { Bottom, Side, Top };

// How the scroll setting holds it: by its side when unrolled sideways; by its
// bottom edge up and down, or its top when the upper hand kept it.
inline TabletHold HoldFor(TabletUnroll unroll, bool upperHandKeeps) {
	if (unroll == TabletUnroll::Sideways) {
		return TabletHold::Side;
	}
	return upperHandKeeps ? TabletHold::Top : TabletHold::Bottom;
}

inline TabletPlace PlaceTablet(const TabletFrame& f, const TabletSettings& s, bool holderPhysicalLeft,
                               const NiPoint3& hand = NiPoint3{0.0f, 0.0f, 0.0f},
                               TabletHold hold = TabletHold::Bottom) {
	TabletPlace place;
	const float paperWidth = f.paper.right - f.paper.left;
	const float cropWidth = f.crop.right - f.crop.left;
	const float cropHeight = f.crop.bottom - f.crop.top;
	if (!s.enabled || !f.up || !f.paper.valid || !f.crop.valid || !(paperWidth > 0.0f) ||
	    !(cropWidth > 0.0f) || !(cropHeight > 0.0f) || !(s.widthMetres > 0.0f)) {
		return place;
	}
	const float metresPerPixel = s.widthMetres / paperWidth;
	const float share = holderPhysicalLeft ? s.gripShare : 1.0f - s.gripShare;
	const float pageMiddleY = 0.5f * (f.paper.top + f.paper.bottom);
	const float anchorX = hold == TabletHold::Side   ? (holderPhysicalLeft ? f.crop.left : f.crop.right)
	                      : hold == TabletHold::Top  ? 0.5f * (f.paper.left + f.paper.right)
	                                                 : f.paper.left + share * paperWidth;
	const float anchorY = hold == TabletHold::Side ? pageMiddleY : (hold == TabletHold::Top ? f.paper.top : f.paper.bottom);
	const float across = (0.5f * (f.crop.left + f.crop.right) - anchorX) * metresPerPixel;
	const float along = (anchorY - 0.5f * (f.crop.top + f.crop.bottom)) * metresPerPixel;
	// The clipboard's grip sits off the hand; held at an edge, the edge is in it.
	const float gripUp = hold == TabletHold::Bottom ? s.gripUp : 0.0f;
	const float gripForward = hold == TabletHold::Bottom ? s.gripForward : 0.0f;
	const float tilt = s.tiltDegrees * (math::kPi / 180.0f);
	const float c = math::Cos(tilt);
	const float sn = math::Sin(tilt);
	// Right (1,0,0); up (0,c,-sn): rising and leaning away; front (0,sn,c):
	// towards the wrist and up, where the eyes are.
	openvr::HmdMatrix34& m = place.transform;
	m.m[0][0] = 1.0f;
	m.m[1][1] = c;
	m.m[2][1] = -sn;
	m.m[1][2] = sn;
	m.m[2][2] = c;
	m.m[0][3] = hand.x + across;
	m.m[1][3] = hand.y + gripUp + c * along;
	m.m[2][3] = hand.z - gripForward - sn * along;
	place.width = cropWidth * metresPerPixel;
	place.height = cropHeight * metresPerPixel;
	place.valid = true;
	return place;
}

// ---------------------------------------------------------- Into the scene

// The quad as drawn: four corners in the holding controller's frame and the
// texture coordinate of each, in the order of a triangle strip - top left,
// top right, bottom left, bottom right.
struct TabletCorners {
	bool valid = false;
	NiPoint3 at[4] = {};
	float u[4] = {};
	float v[4] = {};
};

// One rectangle of the tablet's face (`rect`, in the face's pixels) as drawn,
// showing `source` of the texture (the layer's pixels; invalid for a quad of
// plain colour, whose texture coordinates are then 0). `share` of the face is
// unrolled, from its middle out (the scroll, vr/TabletScroll.h): the picture
// is uncovered, not squeezed, and a rectangle beyond what is open is cut, or
// not drawn at all. The texture is
// `textureWidth` by `textureHeight` pixels (the layer's own size, which can be
// larger than what the game draws into).
inline TabletCorners RectOnTablet(const TabletPlace& place, const UiRect& face, const UiRect& rect,
                                  const UiRect& source, float textureWidth, float textureHeight, float share,
                                  bool vertical = false) {
	TabletCorners out;
	const float faceWidth = face.right - face.left;
	const float faceHeight = face.bottom - face.top;
	if (!place.valid || !face.valid || !rect.valid || !(faceWidth > 0.0f) || !(faceHeight > 0.0f) ||
	    !(textureWidth > 0.0f) || !(textureHeight > 0.0f) || !(share > 0.0f)) {
		return out;
	}
	if (share > 1.0f) {
		share = 1.0f;
	}
	// Unrolled from the middle out: what is open lies `share` of the face's
	// width about its middle - of its height, `vertical`.
	const float middle = 0.5f * (face.left + face.right);
	const float centreY = 0.5f * (face.top + face.bottom);
	const float openLeft = vertical ? face.left : middle - 0.5f * share * faceWidth;
	const float openRight = vertical ? face.right : middle + 0.5f * share * faceWidth;
	const float openTop = vertical ? centreY - 0.5f * share * faceHeight : rect.top;
	const float openBottom = vertical ? centreY + 0.5f * share * faceHeight : rect.bottom;
	const float left = rect.left > openLeft ? rect.left : openLeft;
	const float right_ = rect.right < openRight ? rect.right : openRight;
	const float top = rect.top > openTop ? rect.top : openTop;
	const float bottom = rect.bottom < openBottom ? rect.bottom : openBottom;
	if (!(right_ > left) || !(bottom > top)) {
		return out;
	}
	const openvr::HmdMatrix34& m = place.transform;
	const NiPoint3 centre{m.m[0][3], m.m[1][3], m.m[2][3]};
	const NiPoint3 right{m.m[0][0], m.m[1][0], m.m[2][0]};
	const NiPoint3 up{m.m[0][1], m.m[1][1], m.m[2][1]};
	const auto at = [&](float x, float y) {
		const float across = ((x - face.left) / faceWidth - 0.5f) * place.width;
		const float rise = (0.5f - (y - face.top) / faceHeight) * place.height;
		return centre + right * across + up * rise;
	};
	out.at[0] = at(left, top);
	out.at[1] = at(right_, top);
	out.at[2] = at(left, bottom);
	out.at[3] = at(right_, bottom);
	if (source.valid) {
		const float spanX = rect.right - rect.left;
		const float spanY = rect.bottom - rect.top;
		const auto sx = [&](float x) { return source.left + (x - rect.left) / spanX * (source.right - source.left); };
		const auto sy = [&](float y) { return source.top + (y - rect.top) / spanY * (source.bottom - source.top); };
		out.u[0] = out.u[2] = sx(left) / textureWidth;
		out.u[1] = out.u[3] = sx(right_) / textureWidth;
		out.v[0] = out.v[1] = sy(top) / textureHeight;
		out.v[2] = out.v[3] = sy(bottom) / textureHeight;
	}
	out.valid = true;
	return out;
}

// The rolls at the open part's two edges while it is not fully open: plain
// strips a little taller than the face, `rollShare` of the page's width
// wide. Invalid at fully open or with nothing open.
inline constexpr float kRollShare = 0.05f;

inline UiRect RollStrip(const UiRect& face, float share, bool rightSide, float pageWidth, bool vertical = false) {
	UiRect r;
	const float faceWidth = face.right - face.left;
	const float faceHeight = face.bottom - face.top;
	if (!face.valid || !(share > 0.0f) || !(share < 1.0f) || !(faceWidth > 0.0f) || !(faceHeight > 0.0f)) {
		return r;
	}
	if (vertical) {
		// The rolls lie across the face at the open part's top and bottom
		// (`rightSide` the lower one).
		const float edge = 0.5f * (face.top + face.bottom) + (rightSide ? 0.5f : -0.5f) * share * faceHeight;
		const float half = 0.5f * kRollShare * pageWidth;
		const float over = 0.03f * faceWidth;
		r.left = face.left - over;
		r.right = face.right + over;
		r.top = edge - half;
		r.bottom = edge + half;
		r.valid = true;
		return r;
	}
	const float middle = 0.5f * (face.left + face.right);
	const float edge = middle + (rightSide ? 0.5f : -0.5f) * share * faceWidth;
	const float half = 0.5f * kRollShare * pageWidth;
	const float over = 0.03f * (face.bottom - face.top);
	r.left = edge - half;
	r.right = edge + half;
	r.top = face.top - over;
	r.bottom = face.bottom + over;
	r.valid = true;
	return r;
}

// The whole face showing itself: the tablet as one picture of the layer.
inline TabletCorners CornersOf(const TabletPlace& place, const UiRect& cropPixels, float textureWidth,
                               float textureHeight, float share) {
	return RectOnTablet(place, cropPixels, cropPixels, cropPixels, textureWidth, textureHeight, share);
}

// What the hand mode answers about where the menus hang: the wrists
// (device-to-overlay transforms for the HUD and the menu, and which hand
// carries the menu) and the tablet in the off hand, whose transform is the
// menu's too. HandModeResult's base (vr/HandMode.h).
struct MenuPlaceResult {
	bool hudOnRightWrist = false;
	bool menuOnWrist = false;
	bool menuWristRight = true;
	bool menuOnTablet = false;
	float tabletWidth = 0.0f;
	float tabletHeight = 0.0f;
	bool tabletShowsFigure = false;  // the figure's tab chosen (StepTabletTab)
	bool tabletHolderRight = false;  // the scroll kept by the right hand (vr/TabletScroll.h)
	bool tabletUpright = false;      // unrolled up and down ([Tablet] Unroll)
	bool tabletMenuKey = false;      // the scroll opens or closes the menus this frame
	bool tabletGrips = false;        // the scroll holds the grips (unrolling or rolling up): no grab from them
	float tabletShare = -1.0f;       // unrolled between the hands by this much; < 0 by the time it has been up
	UInt8 tabletRefusal = 0;         // both grips closed and the scroll not begun: why (vr::TabletScrollRefusal)
	float tabletRefusedBy = 0.0f;    // and the measure that refused it
	openvr::HmdMatrix34 hudTransform{};
	openvr::HmdMatrix34 menuTransform{};
};

}  // namespace obvr::vr
