#pragma once

// The player's own menus as a tablet in the off hand
// (docs/inventory-tablet-spec.md, stage 1): which menus are the tablet's,
// the rectangle of the interface's picture it is cut to, where it stands on
// the hand that holds it, its corners for the draw into the scene, and the
// one close when a fight starts. Pure; covered by tablet_test.
//
// The frames: a rectangle is in the HUD's units (vr/HudTileTree.h, 960 high)
// or in the layer's pixels, as named; the hand's frame is OpenVR's controller
// frame - x right, y up, z back towards the wrist - in metres.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/MenuType.h"
#include "game/NiMath.h"
#include "vr/HudTileTree.h"
#include "vr/OpenVRTypes.h"

namespace obvr::vr {

// [Tablet].
struct TabletSettings {
	bool enabled = false;  // stage 1: on in the INI for the headset, the default once it reads well
	// How wide the page stands in the hand, metres. A page wider than the
	// others (the map's) reaches further out at the same scale.
	float widthMetres = 0.34f;
	// Drawn into the eyes' pictures, so the tapping hand is seen in front of
	// it; off, the overlay that covers the hand (spec section 3).
	bool inScene = true;
	bool closesInCombat = true;
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
	UiRect crop;
	// The fight began with the tablet up: the menu key, held a moment
	// (StepTabletCombatClose).
	bool closeNow = false;
};

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
// to its right; mirrored for the right one).
inline TabletPlace PlaceTablet(const TabletFrame& f, const TabletSettings& s, bool holderPhysicalLeft) {
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
	const float anchorX = f.paper.left + share * paperWidth;
	const float across = (0.5f * (f.crop.left + f.crop.right) - anchorX) * metresPerPixel;
	const float along = (f.paper.bottom - 0.5f * (f.crop.top + f.crop.bottom)) * metresPerPixel;
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
	m.m[0][3] = across;
	m.m[1][3] = s.gripUp + c * along;
	m.m[2][3] = -s.gripForward - sn * along;
	place.width = cropWidth * metresPerPixel;
	place.height = cropHeight * metresPerPixel;
	place.valid = true;
	return place;
}

// ---------------------------------------------------------- Into the scene

// How long the tablet takes to rise out of the fist when it opens.
inline constexpr float kTabletOpenSeconds = 0.2f;

inline float TabletOpenShare(float secondsUp) {
	if (!(secondsUp > 0.0f)) {
		return 0.0f;
	}
	return secondsUp >= kTabletOpenSeconds ? 1.0f : secondsUp / kTabletOpenSeconds;
}

// The quad as drawn: four corners in the holding controller's frame and the
// texture coordinate of each, in the order of a triangle strip - top left,
// top right, bottom left, bottom right.
struct TabletCorners {
	bool valid = false;
	NiPoint3 at[4] = {};
	float u[4] = {};
	float v[4] = {};
};

// `share` of the page is up, from its lower edge: the picture is uncovered
// from below, not squeezed. The crop is in the layer's pixels, the texture
// `textureWidth` by `textureHeight` pixels (the layer's own size, which can
// be larger than what the game draws into).
inline TabletCorners CornersOf(const TabletPlace& place, const UiRect& cropPixels, float textureWidth,
                               float textureHeight, float share) {
	TabletCorners out;
	if (!place.valid || !cropPixels.valid || !(textureWidth > 0.0f) || !(textureHeight > 0.0f) ||
	    !(share > 0.0f)) {
		return out;
	}
	if (share > 1.0f) {
		share = 1.0f;
	}
	const openvr::HmdMatrix34& m = place.transform;
	const NiPoint3 centre{m.m[0][3], m.m[1][3], m.m[2][3]};
	const NiPoint3 right{m.m[0][0], m.m[1][0], m.m[2][0]};
	const NiPoint3 up{m.m[0][1], m.m[1][1], m.m[2][1]};
	const float halfWidth = 0.5f * place.width;
	const float bottom = -0.5f * place.height;
	const float top = bottom + share * place.height;
	const float uLeft = cropPixels.left / textureWidth;
	const float uRight = cropPixels.right / textureWidth;
	const float vBottom = cropPixels.bottom / textureHeight;
	const float vTop = (cropPixels.bottom - share * (cropPixels.bottom - cropPixels.top)) / textureHeight;
	out.at[0] = centre - right * halfWidth + up * top;
	out.at[1] = centre + right * halfWidth + up * top;
	out.at[2] = centre - right * halfWidth + up * bottom;
	out.at[3] = centre + right * halfWidth + up * bottom;
	out.u[0] = out.u[2] = uLeft;
	out.u[1] = out.u[3] = uRight;
	out.v[0] = out.v[1] = vTop;
	out.v[2] = out.v[3] = vBottom;
	out.valid = true;
	return out;
}

// --------------------------------------------------------- A fight starts

// [Tablet] ClosesInCombat: the tablet falls shut once, when a fight starts
// while it is up - and never refuses an opening: opened during the fight it
// stays until the player closes it (potions are fetched from it). The fight
// is followed whether the tablet is up or not, so one that began before the
// tablet opened is no start.
inline constexpr float kTabletCloseKeySeconds = 0.15f;

struct TabletCombatState {
	bool wasInCombat = false;
	float keySeconds = 0.0f;  // the menu key still held to close
};

// True while the menu key is to be held: from the frame the fight starts and
// for kTabletCloseKeySeconds, or until the tablet is down.
inline bool StepTabletCombatClose(TabletCombatState& state, bool closesInCombat, bool tabletUp, bool inCombat,
                                  float dtSeconds) {
	const bool began = inCombat && !state.wasInCombat;
	state.wasInCombat = inCombat;
	if (!tabletUp || !closesInCombat) {
		state.keySeconds = 0.0f;
		return false;
	}
	if (began) {
		state.keySeconds = kTabletCloseKeySeconds;
		return true;
	}
	if (state.keySeconds > 0.0f) {
		state.keySeconds -= dtSeconds > 0.0f ? dtSeconds : 0.0f;
		return state.keySeconds > 0.0f;
	}
	return false;
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
	openvr::HmdMatrix34 hudTransform{};
	openvr::HmdMatrix34 menuTransform{};
};

}  // namespace obvr::vr
