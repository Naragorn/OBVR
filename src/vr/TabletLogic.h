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
#include "game/SegmentGeometry.h"
#include "vr/HudTileTree.h"
#include "vr/OpenVRTypes.h"
#include "vr/TabletTabs.h"

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

// How the page sits in the hand that holds it, held like the sword (Nadi,
// Windows 2026-11-26: "inventory tablet muss 90 grad nach richtung kopf des
// spielers und mittig in der hand. lass diese werte einstellbar dann kann ich
// ingame das justieren"): turned from the blade's flat (PageLikeTheSword) by
// three angles in turn, each about the page's own axis as it then stands -
// mirrored for a right hand, so a value means the same in either - and its
// middle on the palm's, moved by the place (TurnHeldPage, PlaceTablet). The
// defaults are the values Nadi set in the game (Windows 2026-11-27: "the
// values ... I now have in the INI are the values that should be the
// standard"), the distance 5 cm further back than the settings row let him go.
struct HeldPlace {
	float turnDegrees = 90.0f;   // about its upright, the blade: + the face away from the knuckles, to the head
	float leanDegrees = 25.0f;   // about its width: + the top back, the face up
	float rollDegrees = -15.0f;  // in its own flat: + the top towards the body's middle
	float acrossCm = 20.0f;      // its middle off the palm's: + towards the body's middle
	float upCm = 0.0f;           // + up the page
	float outCm = -15.0f;        // + out of its face, past the hand's reach
};

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
	// The HUD's texts beside the menus shown above the tablet's page (vr/TabletTexts.h).
	bool textsOnTablet = true;
	// The Mirror tab at the end of the tablet's tabs (docs/mirror-spec.md), and
	// the light dust over its glass.
	bool mirror = true;
	bool mirrorDust = true;
	// Stage 2 (render/MirrorGlassPass.cpp): the world reflected on the glass
	// (off: the dark glass of stage 1), and the mirror's texture's side in
	// pixels, 256 to 2048 (vr/MirrorGlassLogic.h ClampMirrorPixels).
	bool mirrorWorld = true;
	UInt32 mirrorPixels = 1024;
	// The steady picture (vr/MirrorSteadyLogic.h kMirrorSteadyDefault): 0 off,
	// 1 to 100 stronger.
	UInt32 mirrorSteady = 60;
	TabletFigure figure = TabletFigure::Tab;  // Nadi, 2026-11-18: the tab by default
	TabletUnroll unroll = TabletUnroll::Sideways;
	// Where the page's lower edge meets the hand: above and ahead of the
	// controller's origin, the page leant back at the top by the tilt, and
	// how far along the lower edge from the holding hand's outer side. Held
	// like the sword (PageLikeTheSword) only the share applies in the hand;
	// the tilt still leans the scroll between the hands (vr/MenuPlace.h).
	float gripUp = 0.03f;
	float gripForward = 0.02f;
	float tiltDegrees = 30.0f;
	float gripShare = 0.25f;
	HeldPlace held;  // in the hand, held like the sword
};

// ------------------------------------------------------------- Which menus

// A menu the tablet carries: the F1-F4 pages and what they raise - a box of
// the Generic kind too, the tutorial's help box among them (Nadi, Windows
// 2026-11-22: "dialog boxen im inventar sollen auch über dem inventar
// erscheinen"; it hung in the room instead). OBVR's own native menus are
// Generic as well: opened over the pages they go onto the tablet with them.
inline bool TabletCarriesMenu(UInt32 menuId) {
	switch (menuId) {
	case game::kMenuIdGeneric:
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
	       topMenuId != game::kMenuIdBook && topMenuId != game::kMenuIdGeneric;
}

// The menus whose pictures join the tablet's face beside the page: every menu
// in the stack but the pages' entry and the HUD's, and the item info
// (MagicPopup) whether it is in the stack or not - the game shows it beside
// the page on a hover and it does not stack (NorthernUI's
// magic_popup_menu.xml: does_not_stack), so a face read from the stack ended
// at the page's edge and cut it off (Nadi, Windows 2026-11-22: "der info text
// der sich rechts vom inventar einblendet ist nun abgeschnitten"). Its
// pictures count only while shown (game::ReadMenuPaper skips hidden tiles and
// clear pictures). Returns how many went into `out`.
inline UInt32 TabletFaceMenus(const UInt32* stack, UInt32 count, UInt32* out, UInt32 capacity) {
	UInt32 n = 0;
	bool popup = false;
	for (UInt32 i = 0; stack != nullptr && i < count; ++i) {
		if (stack[i] == game::kMenuIdBigFour || IsHudMenu(stack[i])) {
			continue;
		}
		popup = popup || stack[i] == game::kMenuIdMagicPopup;
		if (out != nullptr && n < capacity) {
			out[n++] = stack[i];
		}
	}
	if (!popup && out != nullptr && n < capacity) {
		out[n++] = game::kMenuIdMagicPopup;
	}
	return n;
}

// ---------------------------------------------------------- The rectangle

// A drawn picture of a menu - a tile with a file, shown, not clear - by its
// rectangle in HUD units. What game::ReadMenuPaper hands over. `popup`: the
// picture is the page's own info box or lies under it (PagePopupTileId).
struct PaperTile {
	float left = 0.0f;
	float top = 0.0f;
	float right = 0.0f;
	float bottom = 0.0f;
	bool popup = false;
};

// A page's own info box. The stats menu shows an attribute's or a skill's
// description in a picture of its own (vanilla stats_menu.xml:
// stat_pop_background), a tile of the stats menu itself that slides out
// beside the page - not the MagicPopup menu the inventory and the spells
// use. Read as one more picture of the page it was under the join share and
// stayed out of the face (Nadi, 2026-11-25: "die infobox erscheint im stats
// tab für ein paar Einträge du hast die nur abgeschnitten"). The engine
// knows it by its id, whatever a UI mod calls or lays it out: [R in
// Oblivion.exe 1.2.416] StatsMenu::SetField at 0x005D9BE0 (the menu's vtable
// 0x00A6D9DC, slot 1) keeps the tile of id 61 at +0x58, the focus box of id
// 60 at +0x54. The other three pages have no box of their own: 0.
inline constexpr UInt32 kStatsInfoTileId = 61;

inline UInt32 PagePopupTileId(UInt32 menuId) { return menuId == game::kMenuIdStats ? kStatsInfoTileId : 0u; }

// The popups kept on their own beside the page (vr::LayerPictures): an item
// info, a message or quantity box, a Generic box - more at once is not seen.
inline constexpr UInt32 kTabletPopupsMost = 4;

// A menu's page and what the tablet shows of the picture for it.
struct TabletRects {
	bool valid = false;
	UiRect paper;    // the page: the menu's largest picture
	UiRect crop;     // the page, what reaches beyond it (the map's extension) and its popups
	UiRect outline;  // the page and what reaches beyond it, without the popups
	UiRect popups[kTabletPopupsMost];
	// An info shown while a row is pointed at - the item info, the page's own
	// box - not a box that waits for an answer (vr/TabletBoxes.h).
	bool popupInfo[kTabletPopupsMost] = {};
	UInt32 popupCount = 0;
	bool popupsOverflow = false;  // more popups than kept
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

// A popup over the page: the crop grows to hold it, the page stays the page,
// and the popup is kept on its own to be drawn as its own picture. `info`:
// an info shown while a row is pointed at (the item info, the page's own
// box), not a box.
inline void JoinPopup(TabletRects& page, const UiRect& popup, bool info = false) {
	if (page.valid && popup.valid) {
		page.crop = UnionRect(page.crop, popup);
		if (page.popupCount < kTabletPopupsMost) {
			page.popupInfo[page.popupCount] = info;
			page.popups[page.popupCount++] = popup;
		} else {
			page.popupsOverflow = true;
		}
	}
}

// The page of one menu from its pictures: the largest is the page, and every
// picture of at least kPaperJoinShare of its area joins the crop. The
// pictures of the page's own info box (PaperTile::popup) are no part of the
// page, however large: all of them together are a popup beside it, joined as
// one of another menu is - an info. Invalid with no picture, a screen
// without a size, or a page too small to be one.
inline TabletRects PaperOf(const PaperTile* tiles, UInt32 count, float screenWidth, float screenHeight) {
	TabletRects out;
	if (tiles == nullptr || !(screenWidth > 0.0f) || !(screenHeight > 0.0f)) {
		return out;
	}
	float most = 0.0f;
	for (UInt32 i = 0; i < count; ++i) {
		const UiRect r = OnScreen(tiles[i], screenWidth, screenHeight);
		if (!tiles[i].popup && RectArea(r) > most) {
			most = RectArea(r);
			out.paper = r;
		}
	}
	if (!(most >= kPaperLeastShare * screenWidth * screenHeight)) {
		return TabletRects{};
	}
	out.crop = out.paper;
	UiRect own;
	for (UInt32 i = 0; i < count; ++i) {
		const UiRect r = OnScreen(tiles[i], screenWidth, screenHeight);
		if (tiles[i].popup) {
			own = UnionRect(own, r);
		} else if (RectArea(r) >= kPaperJoinShare * most) {
			out.crop = UnionRect(out.crop, r);
		}
	}
	out.outline = out.crop;
	out.valid = true;
	JoinPopup(out, own, true);
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

// How an item hangs in a drawn hand, in its controller's frame: the hand's
// held-item node's own y - the blade's way out of the fist, the shaft's - and
// its own x, towards the knuckles. The engine hangs the sword on the right
// hand's "Weapon" node and the torch and the bow on the left's "Torch"; read
// from the 1st-person skeleton.nif (Windows' loose one, pyffi, 2026-11-24)
// the Torch node's y and x are the Weapon node's mirrored, to 0.2 and 6.6
// degrees, while its z is the other way round (the Weapon's z the back of
// the hand, the Torch's the palm) - so y and x say the same in either hand.
// The tablet is held like the sword with them (Nadi, 2026-11-24: "das inv
// soll wie das schwert ausgerichtet sein"; PageLikeTheSword).
struct HeldAxes {
	bool valid = false;
	NiPoint3 blade{0.0f, 1.0f, 0.0f};  // the node's y
	NiPoint3 edge{1.0f, 0.0f, 0.0f};   // the node's x
};

// The share of each new reading taken, as the palm's (vr/FingerTip.h
// kIndexTipEase): the bones are read at the end of the frame's posing.
inline constexpr float kHeldAxesEase = 0.3f;

// Each frame, from the hand's node: `read` - the node was found on a hand
// that follows its controller; `blade` and `edge` its y and x in the
// controller's frame. Not read, none; read first, taken; then eased.
inline void StepHeldAxes(HeldAxes& kept, bool read, const NiPoint3& blade, const NiPoint3& edge) {
	if (!read) {
		kept = HeldAxes{};
		return;
	}
	if (!kept.valid) {
		kept.valid = true;
		kept.blade = blade;
		kept.edge = edge;
		return;
	}
	kept.blade = kept.blade + (blade - kept.blade) * kHeldAxesEase;
	kept.edge = kept.edge + (edge - kept.edge) * kHeldAxesEase;
}

// How far a drawn hand reaches out of the flat a page held like the sword
// lies in, from its palm's middle, in metres: before the face the page shows
// in a physical left hand, and before the one it shows in a right hand - on
// the same axes the two faces look opposite ways (PageLikeTheSword), so the
// two are the hand's two sides, and a hand uses its own. Read from the
// hand's model as it is drawn, gauntlet and all (vr/HandReach.h).
struct HandReach {
	bool valid = false;
	float beforeLeft = 0.0f;
	float beforeRight = 0.0f;
};

// The page stands a hair before the hand's foremost part, so nothing of the
// hand shows through it.
inline constexpr float kPageClearMetres = 0.003f;
// The hand's model not read: an open hand's reach stands in. [R in the
// harness, 2026-11-25, tablet-on-hand.txt] The save's bare left hand reaches
// 7.5 cm before its palm's middle open - its fingertips, 10.5 cm across the
// page - and 6.9 cm closed, its curled fingers 2.6 cm across: the fingers
// stand out of the blade's flat, open or not. Its right hand 6.4 cm.
inline constexpr float kHandReachUnreadMetres = 0.075f;

// How far before the palm's middle the page stands, along its face: the hand
// that carries it lies behind it (Nadi, 2026-11-25: "das tablet auf die
// tragende hand schieben sodass die hand nicht durchklippt, die hand soll
// also hinter dem tablet sein und nicht andersherum wie es jetzt ist" - the
// page lay in the flat through the palm's middle, the fingers before it).
inline float PageLiftMetres(const HandReach& reach, bool holderPhysicalLeft) {
	const float before = !reach.valid ? kHandReachUnreadMetres : (holderPhysicalLeft ? reach.beforeLeft : reach.beforeRight);
	return before + kPageClearMetres;
}

// What the hand mode is told each frame: the tablet's menus are up and their
// rectangles were read, both in the layer's pixels.
struct TabletFrame {
	bool up = false;
	bool menusGone = false;  // menu mode on, only the HUD's menus in the stack: they close (vr/TabletClose.h)
	UiRect paper;
	// The tablet's whole face, in the layer's pixels: what the quad shows and
	// the beam and the finger are mapped through (SurfaceToLayer).
	UiRect crop;
	// The page and its popups, drawn where the game draws them.
	UiRect pageCrop;
	// The page without its popups, and the popups on their own (TabletRects).
	UiRect outline;
	UiRect popups[kTabletPopupsMost];
	UInt32 popupCount = 0;
	bool popupsOverflow = false;
	// The figure on a page of its own beside the page (SetFigurePage; Nadi,
	// 2026-11-17: "zweite Seite daneben"): where the game draws it, where it
	// stands on the tablet and how much smaller, and the page behind it.
	bool figurePage = false;
	UiRect figure;
	UiRect figureAt;
	float figureScale = 1.0f;
	UiRect backing;
	// The figure behind a tab (SetFigureTab): the figure fitted into the page's
	// own rectangle, shown there instead of the page while its tab is chosen.
	bool figureTab = false;
	// The HUD's texts beside the face (vr/TabletTexts.h), and where they stand above it.
	UiRect texts;
	UiRect textsAt;
	bool popupUp = false;  // a popup over the page: the page is shown whatever tab is chosen
	UInt32 page = 0;       // the page shown: the inventory's, stats', magic's or map's menu id
	// The tab strip over the page's top edge (SetTabs, vr/TabletTabStrip.h):
	// the page's, the figure's when it has one, the mirror's last.
	UInt32 tabCount = 0;
	TabletTab tabKind[kTabletTabsMost] = {};
	UiRect tabs[kTabletTabsMost];
	UiRect figureFit;
	float figureFitScale = 1.0f;
	// The palm's middle of each drawn hand (0 the left role's - the off hand -
	// 1 the right's), metres in its controller's frame, read
	// from the hand's bones (vr/FingerTip.h PalmBetween): the tablet is held
	// there. Unread, it is held at the hand bone, the wrist.
	bool palmValid[2] = {false, false};
	NiPoint3 palm[2] = {};
	// Each drawn hand's held-item node (0 the left role's, 1 the right's):
	// the page is turned like the sword in it (PageLikeTheSword).
	HeldAxes held[2];
	// How far each drawn hand reaches out of that flat (0 the left role's, 1
	// the right's): the page lies before the hand (PageLiftMetres).
	HandReach reach[2];
};

// The rectangles of the layer drawn on the face, in the layer's pixels: the
// page's cut as one picture; with popups up, the page's outline and each
// popup on its own (Nadi, Windows 2026-11-23: with the item info up the
// inventory's figure showed cut off - the face is one rectangle round page
// and popup, and drawn as one picture it held what the game draws beside
// them). With more popups than kept, the whole cut, so nothing is lost.
// Returns how many went into `out`.
inline UInt32 LayerPictures(const TabletFrame& f, UiRect* out, UInt32 capacity) {
	if (out == nullptr || capacity == 0) {
		return 0;
	}
	if (f.popupCount == 0 || f.popupsOverflow || !f.outline.valid) {
		out[0] = f.pageCrop;
		return f.pageCrop.valid ? 1u : 0u;
	}
	UInt32 n = 0;
	out[n++] = f.outline;
	for (UInt32 i = 0; i < f.popupCount && n < capacity; ++i) {
		out[n++] = f.popups[i];
	}
	return n;
}

// Where a hand holds the tablet, in its controller's frame: the palm's
// middle when it is read, else the drawn hand's bone.
inline NiPoint3 TabletHoldPoint(const TabletFrame& f, bool rightRole, const NiPoint3& handBone) {
	const int i = rightRole ? 1 : 0;
	return f.palmValid[i] ? f.palm[i] : handBone;
}

// How a hand holds an item, in its controller's frame: its held-item node's
// axes when read.
inline HeldAxes TabletHeldAxes(const TabletFrame& f, bool rightRole) { return f.held[rightRole ? 1 : 0]; }

// How far a hand reaches before the page it holds.
inline HandReach TabletHandReach(const TabletFrame& f, bool rightRole) { return f.reach[rightRole ? 1 : 0]; }

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

// The figure behind a tab: fitted into the page's rectangle with a gap round
// it, centred. Nothing without the page or the figure. The tabs over the page
// are laid by SetTabs.
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
}

// Which of the tabs a point on the face lies on: its place in the strip
// (tabKind says which tab that is), -1 none (or no tabs).
inline int TabletTabAt(const TabletFrame& f, float x, float y) {
	for (UInt32 i = 0; i < f.tabCount && i < kTabletTabsMost; ++i) {
		const UiRect& t = f.tabs[i];
		if (x >= t.left && x <= t.right && y >= t.top && y <= t.bottom) {
			return static_cast<int>(i);
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

// The page held like the sword (Nadi, 2026-11-24: "das inv soll wie das
// schwert ausgerichtet sein", after "es sollte sich wie eine seite papier in
// meiner hand anfühlen - momentan muss ich in mein handgelenk schauen"): its
// up the blade's way out of the fist, its right towards the knuckles - to the
// body's middle for either hand, the right one's turned back so the page
// reads the right way round - and its face, right by up, the blade's flat on
// the palm's side (the node's x by y: the Torch's z, the Weapon's turned
// over). It is read as a sword's flat is looked at: the palm to the eyes,
// the thumb up. The blade is taken as read, the knuckles squared to it; not
// valid when the node was not read or its axes have no flat between them.
struct PageAxes {
	bool valid = false;
	NiPoint3 right{1.0f, 0.0f, 0.0f};
	NiPoint3 up{0.0f, 1.0f, 0.0f};
	NiPoint3 front{0.0f, 0.0f, 1.0f};
};

// Shorter than this, a direction read is no direction (an eased one swung
// round between frames).
inline constexpr float kHeldAxisLeast = 0.5f;

inline PageAxes PageLikeTheSword(const HeldAxes& held, bool holderPhysicalLeft) {
	PageAxes a;
	if (!held.valid) {
		return a;
	}
	const float bladeLength = math::Sqrt(held.blade.LengthSquared());
	if (!(bladeLength > kHeldAxisLeast)) {
		return a;
	}
	const NiPoint3 up = held.blade * (1.0f / bladeLength);
	const NiPoint3 knuckles = held.edge - up * game::DotOf3(held.edge, up);
	const float knucklesLength = math::Sqrt(knuckles.LengthSquared());
	if (!(knucklesLength > kHeldAxisLeast)) {
		return a;
	}
	a.up = up;
	a.right = knuckles * ((holderPhysicalLeft ? 1.0f : -1.0f) / knucklesLength);
	a.front = game::CrossOf3(a.right, a.up);
	a.valid = true;
	return a;
}

// Two of a frame's axes turned in their plane by `degrees`: a towards b.
inline void TurnAxisPair(NiPoint3& a, NiPoint3& b, float degrees) {
	const float r = degrees * (math::kPi / 180.0f);
	const float c = math::Cos(r);
	const float s = math::Sin(r);
	const NiPoint3 was = a;
	a = was * c + b * s;
	b = b * c - was * s;
}

// The page held like the sword, turned by the place's angles (HeldPlace):
// first about its upright - positive turns the face from the palm's side
// away from the knuckles, in either hand - then about its width, then in its
// own flat.
inline PageAxes TurnHeldPage(const PageAxes& sword, const HeldPlace& place, bool holderPhysicalLeft) {
	PageAxes a = sword;
	if (!a.valid) {
		return a;
	}
	const float side = holderPhysicalLeft ? 1.0f : -1.0f;
	TurnAxisPair(a.front, a.right, -side * place.turnDegrees);
	TurnAxisPair(a.front, a.up, place.leanDegrees);
	TurnAxisPair(a.up, a.right, side * place.rollDegrees);
	return a;
}

// What a hand at the side holds by its outer edge: the page's outline and,
// beside it, the figure's own page - not a popup, which comes and goes. The
// edge was the whole face's: held by a physical right hand - left-handed, or
// the scroll kept by the right - the face's right edge is where a popup
// joins it, so every info box sliding out moved the page to the left by the
// box's width and back ([R in the code, 2026-11-25; the stats page's own box
// of section 22 would have added one]). A frame without an outline (none
// read) holds the face as it is.
inline UiRect HoldFace(const TabletFrame& f) {
	if (!f.outline.valid) {
		return f.crop;
	}
	return f.figurePage ? UnionRect(f.outline, f.backing) : f.outline;
}

// `held`: the holding hand's held-item node; read, the page is turned like
// the sword in that hand and held at the hold's edge before the palm - by
// `reach`, the hand as it is drawn behind it (PageLiftMetres) - without the
// clipboard's grip offsets or the tilt; else on the controller as above.
inline TabletPlace PlaceTablet(const TabletFrame& f, const TabletSettings& s, bool holderPhysicalLeft,
                               const NiPoint3& hand = NiPoint3{0.0f, 0.0f, 0.0f},
                               TabletHold hold = TabletHold::Bottom, const HeldAxes& held = HeldAxes{},
                               const HandReach& reach = HandReach{}) {
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
	const UiRect side = HoldFace(f);
	const float anchorX = hold == TabletHold::Side   ? (holderPhysicalLeft ? side.left : side.right)
	                      : hold == TabletHold::Top  ? 0.5f * (f.paper.left + f.paper.right)
	                                                 : f.paper.left + share * paperWidth;
	const float anchorY = hold == TabletHold::Side ? pageMiddleY : (hold == TabletHold::Top ? f.paper.top : f.paper.bottom);
	const float across = (0.5f * (f.crop.left + f.crop.right) - anchorX) * metresPerPixel;
	const float along = (anchorY - 0.5f * (f.crop.top + f.crop.bottom)) * metresPerPixel;
	openvr::HmdMatrix34& m = place.transform;
	place.width = cropWidth * metresPerPixel;
	place.height = cropHeight * metresPerPixel;
	place.valid = true;
	const PageAxes sword = TurnHeldPage(PageLikeTheSword(held, holderPhysicalLeft), s.held, holderPhysicalLeft);
	if (sword.valid) {
		const NiPoint3 axes[3] = {sword.right, sword.up, sword.front};
		for (int c = 0; c < 3; ++c) {
			m.m[0][c] = axes[c].x;
			m.m[1][c] = axes[c].y;
			m.m[2][c] = axes[c].z;
		}
		// The page's middle on the palm's, whatever the hold (Nadi: "mittig in
		// der hand"); the face drawn round its own middle, a box beside the
		// page reaching out past it.
		const float faceAcross = (0.5f * (f.crop.left + f.crop.right) - 0.5f * (f.paper.left + f.paper.right)) * metresPerPixel;
		const float faceAlong = (pageMiddleY - 0.5f * (f.crop.top + f.crop.bottom)) * metresPerPixel;
		const float side = holderPhysicalLeft ? 1.0f : -1.0f;
		const NiPoint3 centre = hand + sword.right * (faceAcross + side * 0.01f * s.held.acrossCm) +
		                        sword.up * (faceAlong + 0.01f * s.held.upCm) +
		                        sword.front * (PageLiftMetres(reach, holderPhysicalLeft) + 0.01f * s.held.outCm);
		m.m[0][3] = centre.x;
		m.m[1][3] = centre.y;
		m.m[2][3] = centre.z;
		return place;
	}
	// The clipboard's grip sits off the hand; held at an edge, the edge is in it.
	const float gripUp = hold == TabletHold::Bottom ? s.gripUp : 0.0f;
	const float gripForward = hold == TabletHold::Bottom ? s.gripForward : 0.0f;
	const float tilt = s.tiltDegrees * (math::kPi / 180.0f);
	const float c = math::Cos(tilt);
	const float sn = math::Sin(tilt);
	// Right (1,0,0); up (0,c,-sn): rising and leaning away; front (0,sn,c):
	// towards the wrist and up, where the eyes are.
	m.m[0][0] = 1.0f;
	m.m[1][1] = c;
	m.m[2][1] = -sn;
	m.m[1][2] = sn;
	m.m[2][2] = c;
	m.m[0][3] = hand.x + across;
	m.m[1][3] = hand.y + gripUp + c * along;
	m.m[2][3] = hand.z - gripForward - sn * along;
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
	TabletTab tabletTab = TabletTab::Page;  // the tab shown (StepTabletTab)
	bool tabletHolderRight = false;  // the scroll kept by the right hand (vr/TabletScroll.h)
	bool tabletUpright = false;      // unrolled up and down ([Tablet] Unroll)
	bool tabletMenuKey = false;      // the scroll opens or closes the menus this frame
	bool tabletGrips = false;        // the scroll holds the grips (unrolling or rolling up): no grab from them
	float tabletShare = -1.0f;       // unrolled between the hands by this much; < 0 by the time it has been up
	bool tabletRolledUp = false;     // the scroll rolled it up or back, its close sent: nothing to show
	UInt8 tabletRefusal = 0;         // both grips closed and the scroll not begun: why (vr::TabletScrollRefusal)
	float tabletRefusedBy = 0.0f;    // and the measure that refused it
	// For the handover's gate (vr/GripClaim.h): the hands where the scroll can
	// begin, the frame it began, both grips open (vr::TabletScrollResult).
	bool tabletZone = false;
	bool tabletBegan = false;
	bool tabletGripsOpen = true;
	openvr::HmdMatrix34 hudTransform{};
	openvr::HmdMatrix34 menuTransform{};
};

}  // namespace obvr::vr
