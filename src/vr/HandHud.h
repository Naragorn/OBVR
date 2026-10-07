#pragma once

// The HUD on the hands (docs/hud-on-hands-spec.md): which part of the game's
// HUD goes where, where its rectangle is in the captured interface, and how
// its quad is placed - on a controller, or for the compass in the sky above
// the view, fading in as the head looks up.
//
// Every element is cut out of the picture the game (or a UI mod) drew, so a
// mod's look comes along. Its rectangle comes from the HUD's tile tree by
// tile name (step 1 of the spec measured the vanilla names and the units); an
// element whose tiles are not found stays in the main panel, where it was.
//
// Pure: no game, no compositor. Covered by hand_hud_test.

#include "core/ChoiceWord.h"
#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/OpenVRTypes.h"

namespace obvr::vr {

// ---- The elements and where they go --------------------------------------

enum class HudElement : UInt8 {
	Bars = 0,     // health, magicka, fatigue
	Spell = 1,    // the equipped spell
	Weapon = 2,   // the equipped weapon, the arrow count, its condition
	Effects = 3,  // the active effects' icons
	LevelUp = 4,  // the level-up icon
	Compass = 5,
	Region = 6,     // the region's name, shown when one is discovered
	Messages = 7,   // the notices, top left in vanilla ("Your Blade skill increased.")
	Subtitles = 8,  // what people say
	Info = 9,       // what the crosshair is on: its name, the action, value and weight (HUDInfoMenu)
};
inline constexpr UInt32 kHudElementCount = 10;

// The INI's key stems ([HandHud] <Stem>Place, <Stem>Opacity, <Stem>Size).
inline constexpr const char* kHudElementKeys[kHudElementCount] = {
	"Bars", "Spell", "Weapon", "Effects", "LevelUp", "Compass", "Region", "Messages", "Subtitles", "Info"};

// Which menu an element's tiles are in: HUDMainMenu, HUDSubtitleMenu for the
// notices and the subtitles (hud_subtitle_menu.xml), HUDInfoMenu for what the
// crosshair is on.
inline bool HudElementInSubtitleMenu(HudElement e) {
	return e == HudElement::Messages || e == HudElement::Subtitles;
}
inline bool HudElementInInfoMenu(HudElement e) { return e == HudElement::Info; }

// Where an element goes. View leaves it in the main panel ahead of the
// player, as the game drew it; Off takes it out and shows it nowhere. Top and
// Bottom put it above or below the middle of the view, placed where the
// head looks when it appears (and again when the head turns far from it).
// Target hangs it just under what the crosshair is on, facing the eyes, so it
// is plain which thing it names (a beta tester, 2026-10-03: "Man weiss nicht
// was nun vor einem ist wenn der Beschreibung Text vor einem erscheint").
enum class HudPlace : UInt8 {
	View = 0,
	Left = 1,
	Right = 2,
	Sky = 3,
	Off = 4,
	Top = 5,
	Bottom = 6,
	Target = 7,
};
inline constexpr UInt32 kHudPlaceCount = 8;
inline constexpr const char* kHudPlaceNames[kHudPlaceCount] = {"view", "left", "right",  "sky",
                                                                "off",  "top",  "bottom", "target"};

inline bool ParseHudPlace(const char* text, HudPlace& out) {
	UInt32 index = 0;
	if (!MatchChoiceWord(text, kHudPlaceNames, kHudPlaceCount, index)) {
		return false;
	}
	out = static_cast<HudPlace>(index);
	return true;
}

// The settings row's value (0..6) as a place; out of range is View, the
// game's own.
inline HudPlace HudPlaceFromIndex(float value) {
	if (!(value > -0.5f) || !(value < static_cast<float>(kHudPlaceCount) - 0.5f)) {
		return HudPlace::View;
	}
	return static_cast<HudPlace>(static_cast<int>(value + 0.5f));
}

struct HudElementSettings {
	HudPlace place = HudPlace::View;
	float opacity = 1.0f;  // 0..1, times what the game drew
	float size = 1.0f;     // times the element's own width
};

// [HandHud], hot reloaded. The defaults are the tester's layout
// (2026-09-29): the left hand health, magicka, fatigue and the spell; the
// right hand the rest; the compass in the sky; the region's name and the
// notices at the top of the view, the subtitles at its bottom.
struct HandHudSettings {
	bool enabled = true;
	HudElementSettings element[kHudElementCount] = {
		{HudPlace::Left, 1.0f, 1.0f},  {HudPlace::Left, 1.0f, 1.0f},  {HudPlace::Right, 1.0f, 1.0f},
		{HudPlace::Right, 1.0f, 1.0f}, {HudPlace::Right, 1.0f, 1.0f}, {HudPlace::Sky, 1.0f, 1.0f},
		{HudPlace::Top, 1.0f, 1.0f},   {HudPlace::Top, 1.0f, 1.0f},   {HudPlace::Bottom, 1.0f, 1.0f},
		{HudPlace::Target, 1.0f, 1.0f},
	};
	// The panel on the back of each hand: metres along the controller's up
	// and back axes, and the tilt towards the eyes (as the wrist HUD's).
	float panelUp = 0.05f;
	float panelBack = 0.08f;
	float panelTiltDegrees = 35.0f;
	// Metres per HUD unit at size 1: the bars (199 units) come out 12 cm wide.
	float unitMetres = 0.0006f;
	// The space between two elements on one hand.
	float gapMetres = 0.01f;
	// The compass: how far above the horizon it hangs, how far away, how
	// wide, and the head's pitch over which it fades in.
	float compassElevationDegrees = 40.0f;
	float compassDistanceMetres = 1.5f;
	// Metres per HUD unit in the sky at size 1: the compass (220 units) comes
	// out half a metre wide, 1.5 m away.
	float skyUnitMetres = 0.0023f;
	float compassFadeStartDegrees = 20.0f;
	float compassFadeFullDegrees = 35.0f;
	// The top and bottom of the view: how far away, how far above (top) and
	// below (bottom) the middle of the view, and metres per HUD unit at size 1
	// (a 554-unit notice comes out 44 cm wide, 1.2 m away).
	float viewDistanceMetres = 1.2f;
	float viewTopDegrees = 15.0f;
	float viewBottomDegrees = 20.0f;
	float viewUnitMetres = 0.0008f;
	// The head turned this many degrees away from where a row was placed
	// takes it along to the middle again; 0 keeps it where it appeared.
	float viewFollowDegrees = 30.0f;
	// On: the top and bottom rows ride the head every frame instead.
	bool viewLockedToHead = false;
	// When a hand's HUD shows: always (on the back of the hand, as a watch),
	// or only while you look at the hand with its palm turned to your face
	// (the tester, 2026-09-29: "man muss die hand mit vr anschauen und die
	// hand zum gesicht drehen quasi mit offener hand"). Look is the default.
	bool showOnLook = true;
	// The hand within this many degrees of where the head looks, and the
	// palm within this many degrees of facing the eyes.
	float lookGazeDegrees = 35.0f;
	float lookPalmDegrees = 55.0f;
	// Fading in and out takes this long.
	float lookFadeSeconds = 0.15f;
	// Looked at, the HUD lies in the palm: this far below the controller's
	// tracked origin (along its -y, where the hand holds the grip) and this
	// far off the palm towards the eyes (the tester, 2026-09-29: 6 cm off and
	// at the origin was "zu weit hoch ... dachte ... in der handfläche").
	float palmDownMetres = 0.05f;
	float palmLiftMetres = 0.02f;
	// And moved on the panel itself, as the eyes see it: metres to the right
	// and up, both hands the same (the tester, 2026-09-30: "die hud an den
	// händen sind auch noch zu weit oben. man muss x, y einstellen können").
	float offsetRightMetres = 0.0f;
	float offsetUpMetres = -0.03f;
};

// ---- Looking at a hand ------------------------------------------------------

// A controller as the look check sees it: where it is and its own axes in
// tracking space (OpenVR's: x right, y up, z back along the controller), and
// whether the hand is open.
struct HandLook {
	bool valid = false;
	openvr::HmdMatrix34 pose{};
	bool open = true;
};

inline float Dot3(float ax, float ay, float az, float bx, float by, float bz) { return ax * bx + ay * by + az * bz; }

// The palm's outward direction in tracking space: the controller's -x on the
// right hand, +x on the left - the side the fingers close over. ASSUMED from
// OpenVR's controller axes (x to the right of the grip); to confirm in the
// headset. Written into nx/ny/nz.
inline void PalmNormal(const openvr::HmdMatrix34& pose, bool rightHand, float& nx, float& ny, float& nz) {
	const float s = rightHand ? -1.0f : 1.0f;
	nx = pose.m[0][0] * s;
	ny = pose.m[1][0] * s;
	nz = pose.m[2][0] * s;
}

// Whether the player is looking at this hand with its palm turned to the
// face: the hand within `gazeDegrees` of the head's forward, the palm within
// `palmDegrees` of facing the eyes, and the hand open.
inline bool PalmFacesEyes(const HandLook& hand, bool rightHand, const openvr::HmdMatrix34& head, float gazeDegrees,
                          float palmDegrees) {
	if (!hand.valid || !hand.open) {
		return false;
	}
	const float hx = head.m[0][3], hy = head.m[1][3], hz = head.m[2][3];
	float tx = hand.pose.m[0][3] - hx, ty = hand.pose.m[1][3] - hy, tz = hand.pose.m[2][3] - hz;
	const float len = math::Sqrt(Dot3(tx, ty, tz, tx, ty, tz));
	if (len < 1e-3f) {
		return false;
	}
	tx /= len;
	ty /= len;
	tz /= len;
	// The head's forward is its -z column.
	const float gaze = Dot3(-head.m[0][2], -head.m[1][2], -head.m[2][2], tx, ty, tz);
	if (gaze < math::Cos(gazeDegrees * (math::kPi / 180.0f))) {
		return false;
	}
	float nx = 0.0f, ny = 0.0f, nz = 0.0f;
	PalmNormal(hand.pose, rightHand, nx, ny, nz);
	// Facing the eyes: the palm points back along the line to the hand.
	return Dot3(nx, ny, nz, -tx, -ty, -tz) >= math::Cos(palmDegrees * (math::kPi / 180.0f));
}

// A hand HUD's opacity one frame on: towards 1 while looked at, towards 0
// otherwise, the whole way in `seconds` (0: at once).
inline float StepHandFade(float alpha, bool lookedAt, float dt, float seconds) {
	const float target = lookedAt ? 1.0f : 0.0f;
	if (!(seconds > 0.0f)) {
		return target;
	}
	const float step = dt / seconds;
	if (alpha < target) {
		alpha += step;
		return alpha > target ? target : alpha;
	}
	alpha -= step;
	return alpha < target ? target : alpha;
}

// The panel over the palm, facing the eyes: `lift` metres off the palm, its x
// level (the row lies across it), its face turned to the head. Absolute, in
// tracking space.
inline openvr::HmdMatrix34 PalmPanelPose(const HandLook& hand, bool rightHand, const openvr::HmdMatrix34& head,
                                         float lift, float down = 0.0f) {
	float nx = 0.0f, ny = 0.0f, nz = 0.0f;
	PalmNormal(hand.pose, rightHand, nx, ny, nz);
	// The palm: `down` along the controller's -y from its origin, then `lift`
	// off it along the palm's normal.
	const float cx = hand.pose.m[0][3] - hand.pose.m[0][1] * down + nx * lift;
	const float cy = hand.pose.m[1][3] - hand.pose.m[1][1] * down + ny * lift;
	const float cz = hand.pose.m[2][3] - hand.pose.m[2][1] * down + nz * lift;
	// z: from the panel to the eyes.
	float zx = head.m[0][3] - cx, zy = head.m[1][3] - cy, zz = head.m[2][3] - cz;
	float len = math::Sqrt(Dot3(zx, zy, zz, zx, zy, zz));
	if (len < 1e-4f) {
		zx = 0.0f, zy = 0.0f, zz = 1.0f, len = 1.0f;
	}
	zx /= len;
	zy /= len;
	zz /= len;
	// x: world up cross z, level; straight above or below falls back to the
	// head's own x.
	float xx = zz, xy = 0.0f, xz = -zx;
	len = math::Sqrt(xx * xx + xz * xz);
	if (len < 1e-4f) {
		xx = head.m[0][0], xy = head.m[1][0], xz = head.m[2][0];
		len = math::Sqrt(Dot3(xx, xy, xz, xx, xy, xz));
	}
	xx /= len;
	xy /= len;
	xz /= len;
	// y = z cross x.
	const float yx = zy * xz - zz * xy;
	const float yy = zz * xx - zx * xz;
	const float yz = zx * xy - zy * xx;
	openvr::HmdMatrix34 m{};
	m.m[0][0] = xx, m.m[1][0] = xy, m.m[2][0] = xz;
	m.m[0][1] = yx, m.m[1][1] = yy, m.m[2][1] = yz;
	m.m[0][2] = zx, m.m[1][2] = zy, m.m[2][2] = zz;
	m.m[0][3] = cx, m.m[1][3] = cy, m.m[2][3] = cz;
	return m;
}

// An absolute pose as seen from a device: inverse(device) * absolute, for an
// overlay hung on that device - the compositor then carries it with the
// hand's newest pose.
inline openvr::HmdMatrix34 RelativeToDevice(const openvr::HmdMatrix34& device, const openvr::HmdMatrix34& absolute) {
	openvr::HmdMatrix34 out{};
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			// (R_d^T R_a)[r][c] = sum_k R_d[k][r] R_a[k][c]
			out.m[r][c] = device.m[0][r] * absolute.m[0][c] + device.m[1][r] * absolute.m[1][c] +
			              device.m[2][r] * absolute.m[2][c];
		}
		const float dx = absolute.m[0][3] - device.m[0][3];
		const float dy = absolute.m[1][3] - device.m[1][3];
		const float dz = absolute.m[2][3] - device.m[2][3];
		out.m[r][3] = device.m[0][r] * dx + device.m[1][r] * dy + device.m[2][r] * dz;
	}
	return out;
}

// Whether an element is taken out of the main panel: on a hand, in the sky,
// or switched off. The feature off, everything stays where the game drew it.
inline bool HudElementLifted(const HandHudSettings& s, HudElement e) {
	if (!s.enabled) {
		return false;
	}
	return s.element[static_cast<UInt32>(e)].place != HudPlace::View;
}

// Whether a lifted element is shown on a quad of its own (Off is lifted and
// shown nowhere).
inline bool HudElementShown(const HandHudSettings& s, HudElement e) {
	return HudElementLifted(s, e) && s.element[static_cast<UInt32>(e)].place != HudPlace::Off;
}

// ---- The tile tree --------------------------------------------------------

// Oblivion lays the HUD out in a space 960 units high; the width is 960
// times the aspect (measured, spec section 8).
inline constexpr float kHudUiHeightUnits = 960.0f;

inline constexpr UInt32 kHudTileNameSize = 40;
inline constexpr UInt32 kHudTilesMax = 192;

// One tile as read: its name, its parent's index (-1 for the root), and the
// traits that place it - a missing x or y counts as 0, a missing width or
// height means the tile has no size of its own.
struct HudTile {
	char name[kHudTileNameSize] = {};
	SInt32 parent = -1;
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
	bool hasSize = false;
	// Its alpha trait; 255 when it has none.
	float alpha = 255.0f;
	// Its justify trait: 1 left (x is its left edge), 2 centre (x is its
	// middle), 4 right (x is its right edge) - the text tiles'.
	UInt32 justify = 1;
};
inline constexpr UInt32 kHudJustifyCentre = 2;
inline constexpr UInt32 kHudJustifyRight = 4;

struct UiRect {
	float left = 0.0f;
	float top = 0.0f;
	float right = 0.0f;
	float bottom = 0.0f;
	bool valid = false;
};

inline bool SameTileName(const char* a, const char* b) {
	if (a == nullptr || b == nullptr) {
		return false;
	}
	for (;; ++a, ++b) {
		const char ca = (*a >= 'A' && *a <= 'Z') ? static_cast<char>(*a + ('a' - 'A')) : *a;
		const char cb = (*b >= 'A' && *b <= 'Z') ? static_cast<char>(*b + ('a' - 'A')) : *b;
		if (ca != cb) {
			return false;
		}
		if (ca == '\0') {
			return true;
		}
	}
}

// The first tile of that name, any case, or -1.
inline SInt32 FindHudTile(const HudTile* tiles, UInt32 count, const char* name) {
	for (UInt32 i = 0; i < count; ++i) {
		if (SameTileName(tiles[i].name, name)) {
			return static_cast<SInt32>(i);
		}
	}
	return -1;
}

// A tile's top left in HUD units: its x and y plus every parent's. False on
// a broken chain (an index out of range, or deeper than any real menu).
inline bool HudTileOrigin(const HudTile* tiles, UInt32 count, SInt32 index, float& x, float& y) {
	x = 0.0f;
	y = 0.0f;
	for (UInt32 depth = 0; index >= 0; ++depth) {
		if (static_cast<UInt32>(index) >= count || depth > 32) {
			return false;
		}
		x += tiles[index].x;
		y += tiles[index].y;
		index = tiles[index].parent;
	}
	return true;
}

// A tile's rectangle, or invalid when it has no size of its own.
inline UiRect HudTileRect(const HudTile* tiles, UInt32 count, SInt32 index) {
	UiRect r;
	if (index < 0 || static_cast<UInt32>(index) >= count) {
		return r;
	}
	const HudTile& t = tiles[index];
	if (!t.hasSize || !(t.width > 0.0f) || !(t.height > 0.0f)) {
		return r;
	}
	float x = 0.0f;
	float y = 0.0f;
	if (!HudTileOrigin(tiles, count, index, x, y)) {
		return r;
	}
	if (t.justify == kHudJustifyCentre) {
		x -= t.width * 0.5f;
	} else if (t.justify == kHudJustifyRight) {
		x -= t.width;
	}
	r.left = x;
	r.top = y;
	r.right = x + t.width;
	r.bottom = y + t.height;
	r.valid = true;
	return r;
}

inline UiRect UnionRect(const UiRect& a, const UiRect& b) {
	if (!a.valid) {
		return b;
	}
	if (!b.valid) {
		return a;
	}
	UiRect r;
	r.left = a.left < b.left ? a.left : b.left;
	r.top = a.top < b.top ? a.top : b.top;
	r.right = a.right > b.right ? a.right : b.right;
	r.bottom = a.bottom > b.bottom ? a.bottom : b.bottom;
	r.valid = true;
	return r;
}

// Whether tile `index` lies under tile `ancestor`.
inline bool HudTileUnder(const HudTile* tiles, UInt32 count, SInt32 index, SInt32 ancestor) {
	for (UInt32 depth = 0; index >= 0 && static_cast<UInt32>(index) < count && depth <= 32; ++depth) {
		index = tiles[index].parent;
		if (index == ancestor) {
			return true;
		}
	}
	return false;
}

// The tiles each element is made of (vanilla's names, spec section 8): the
// element's rectangle is their union. The covers and the frame are the
// ornament drawn around the icon or the bars. The compass's heading strip is
// 2048 units wide and clipped by its window, so it is left out - the window
// and the frame are the picture.
inline constexpr UInt32 kHudElementTilesMax = 4;
inline constexpr const char* kHudElementTiles[kHudElementCount][kHudElementTilesMax] = {
	{"hudmain_health_empty", "hudmain_magic_empty", "hudmain_fatigue_empty", "hudmain_bars_cover"},
	{"hudmain_Magic_Icon", "hudmain_magic_cover", nullptr, nullptr},
	{"hudmain_Weapon_Icon", "hudmain_weapon_cover", "hudmain_weapon_ammo", "hudmain_weapon_status"},
	{nullptr, nullptr, nullptr, nullptr},  // the active effects: every tile under magic_icons
	{"hudmain_Levelup_Icon", nullptr, nullptr, nullptr},
	{"hudmain_compass_window", "hudmain_compass_frame", nullptr, nullptr},
	{"hudmain_region", nullptr, nullptr, nullptr},
	{"hudsubtitle_notice", "hudsubtitle_icon", nullptr, nullptr},
	{"hudsubtitle_text", nullptr, nullptr, nullptr},
	{nullptr, nullptr, nullptr, nullptr},  // what the crosshair is on: every tile under kHudInfoRoot
};
inline constexpr const char* kHudEffectsRoot = "magic_icons";
inline constexpr const char* kHudInfoRoot = "HUDInfoMenu";
// HUDInfoMenu's action icon is left out of the Info element: the crosshair
// quad already carries the game's context icon (the hand, the lock, the
// speech bubble) at the laser or in the reach ring, and the same icon in the
// row under the thing as well showed twice at two sizes (the tester,
// 2026-10-07: "doppelte tooltips ... nicht dieselbe größe").
inline constexpr const char* kHudInfoActionIcon = "hudinfo_action_icon";

// An element's rectangle in HUD units, invalid when none of its tiles is
// found with a size - a UI mod that renamed them, or nothing to show (no
// active effect).
inline UiRect HudElementRect(const HudTile* tiles, UInt32 count, HudElement element) {
	UiRect r;
	if (element == HudElement::Effects || element == HudElement::Info) {
		const SInt32 root =
			FindHudTile(tiles, count, element == HudElement::Effects ? kHudEffectsRoot : kHudInfoRoot);
		if (root < 0) {
			return r;
		}
		const SInt32 actionIcon = element == HudElement::Info ? FindHudTile(tiles, count, kHudInfoActionIcon) : -1;
		for (UInt32 i = 0; i < count; ++i) {
			if (static_cast<SInt32>(i) != actionIcon && HudTileUnder(tiles, count, static_cast<SInt32>(i), root)) {
				r = UnionRect(r, HudTileRect(tiles, count, static_cast<SInt32>(i)));
			}
		}
		return r;
	}
	for (const char* name : kHudElementTiles[static_cast<UInt32>(element)]) {
		if (name == nullptr) {
			continue;
		}
		const SInt32 index = FindHudTile(tiles, count, name);
		// The region's name stays in the tree all the time and fades in by
		// its alpha; at 0 there is nothing to show.
		if (element == HudElement::Region && index >= 0 && !(tiles[index].alpha > 0.0f)) {
			continue;
		}
		r = UnionRect(r, HudTileRect(tiles, count, index));
	}
	return r;
}

struct CaptureRect {
	SInt32 left = 0;
	SInt32 top = 0;
	SInt32 right = 0;
	SInt32 bottom = 0;
	bool valid = false;
};

// A HUD rectangle in the captured interface's pixels: one unit is
// believedHeight / 960 pixels from the top left (measured, spec section 8),
// rounded outwards and kept inside the capture. Invalid when nothing of it
// is left.
inline CaptureRect UiRectToCapture(const UiRect& r, UInt32 believedHeight, UInt32 captureWidth,
                                   UInt32 captureHeight) {
	CaptureRect out;
	if (!r.valid || believedHeight == 0) {
		return out;
	}
	const float scale = static_cast<float>(believedHeight) / kHudUiHeightUnits;
	float left = r.left * scale;
	float top = r.top * scale;
	float right = r.right * scale;
	float bottom = r.bottom * scale;
	left = left < 0.0f ? 0.0f : left;
	top = top < 0.0f ? 0.0f : top;
	const float maxX = static_cast<float>(captureWidth);
	const float maxY = static_cast<float>(captureHeight);
	right = right > maxX ? maxX : right;
	bottom = bottom > maxY ? maxY : bottom;
	out.left = static_cast<SInt32>(left);
	out.top = static_cast<SInt32>(top);
	out.right = static_cast<SInt32>(right + 0.999f);
	out.bottom = static_cast<SInt32>(bottom + 0.999f);
	if (out.right > static_cast<SInt32>(captureWidth)) {
		out.right = static_cast<SInt32>(captureWidth);
	}
	if (out.bottom > static_cast<SInt32>(captureHeight)) {
		out.bottom = static_cast<SInt32>(captureHeight);
	}
	out.valid = out.right > out.left && out.bottom > out.top;
	return out;
}

// ---- The atlas ------------------------------------------------------------

// Every lifted element is copied into one texture, each into its own slot of
// a grid, keeping its aspect - one texture for all the quads, each showing
// its slot through the overlay's texture bounds.
inline constexpr UInt32 kHandHudAtlasWidth = 2048;
inline constexpr UInt32 kHandHudAtlasHeight = 2048;
inline constexpr UInt32 kHandHudAtlasColumns = 4;
inline constexpr UInt32 kHandHudSlotSize = kHandHudAtlasWidth / kHandHudAtlasColumns;  // 512
static_assert(kHudElementCount <= kHandHudAtlasColumns * (kHandHudAtlasHeight / kHandHudSlotSize),
              "every element has a slot");

// Where element `slot`'s picture goes in the atlas: the slot's top left,
// scaled to fit it whole with its aspect kept. Invalid for an empty source.
inline CaptureRect AtlasSlotRect(UInt32 slot, SInt32 sourceWidth, SInt32 sourceHeight) {
	CaptureRect out;
	if (sourceWidth <= 0 || sourceHeight <= 0) {
		return out;
	}
	const float size = static_cast<float>(kHandHudSlotSize);
	const float sx = size / static_cast<float>(sourceWidth);
	const float sy = size / static_cast<float>(sourceHeight);
	const float scale = sx < sy ? sx : sy;
	SInt32 w = static_cast<SInt32>(static_cast<float>(sourceWidth) * scale);
	SInt32 h = static_cast<SInt32>(static_cast<float>(sourceHeight) * scale);
	w = w < 1 ? 1 : w;
	h = h < 1 ? 1 : h;
	out.left = static_cast<SInt32>((slot % kHandHudAtlasColumns) * kHandHudSlotSize);
	out.top = static_cast<SInt32>((slot / kHandHudAtlasColumns) * kHandHudSlotSize);
	out.right = out.left + w;
	out.bottom = out.top + h;
	out.valid = true;
	return out;
}

// ---- Placement ------------------------------------------------------------

// The quad of the panel on the back of a hand, relative to the controller:
// turned about the controller's x axis so its front faces up and leans
// towards the eyes by the tilt (as the wrist HUD's, vr::WristOverlayTransform),
// at up/back along the controller, moved `sideways` metres along its x.
inline openvr::HmdMatrix34 HandPanelTransform(float up, float back, float tiltDegrees, float sideways) {
	const float radians = -(90.0f - tiltDegrees) * (math::kPi / 180.0f);
	const float c = math::Cos(radians);
	const float s = math::Sin(radians);
	openvr::HmdMatrix34 m{};
	m.m[0][0] = 1.0f;
	m.m[1][1] = c;
	m.m[1][2] = -s;
	m.m[2][1] = s;
	m.m[2][2] = c;
	m.m[0][3] = sideways;
	m.m[1][3] = up;
	m.m[2][3] = back;
	return m;
}

// The centres of `count` quads of these widths in a row, `gap` apart, the
// row centred on 0. Pure layout along the panel's x.
inline void LayoutHandRow(const float* widths, UInt32 count, float gap, float* centres) {
	float total = 0.0f;
	for (UInt32 i = 0; i < count; ++i) {
		total += widths[i];
	}
	if (count > 1) {
		total += gap * static_cast<float>(count - 1);
	}
	float at = -total * 0.5f;
	for (UInt32 i = 0; i < count; ++i) {
		centres[i] = at + widths[i] * 0.5f;
		at += widths[i] + gap;
	}
}

// The head's pitch in degrees, up positive: its forward (-z column) against
// the horizontal.
inline float HeadPitchDegrees(const openvr::HmdMatrix34& head) {
	float up = -head.m[1][2];
	up = up > 1.0f ? 1.0f : (up < -1.0f ? -1.0f : up);
	return math::Asin(up) * (180.0f / math::kPi);
}

// How visible the compass is at this head pitch: nothing up to `start`
// degrees above the horizon, all of `opacity` from `full`, in between in
// proportion. A `full` not above `start` switches at `start`.
inline float CompassOpacity(float pitchDegrees, float startDegrees, float fullDegrees, float opacity) {
	if (!(pitchDegrees == pitchDegrees)) {
		return 0.0f;
	}
	if (fullDegrees <= startDegrees) {
		return pitchDegrees >= startDegrees ? opacity : 0.0f;
	}
	if (pitchDegrees <= startDegrees) {
		return 0.0f;
	}
	if (pitchDegrees >= fullDegrees) {
		return opacity;
	}
	return opacity * (pitchDegrees - startDegrees) / (fullDegrees - startDegrees);
}

// The compass's pose in the room: in the head's heading (yaw only), raised
// `elevationDegrees` above the horizon at `distance` metres from the eyes,
// its face turned to them.
inline openvr::HmdMatrix34 CompassPose(const openvr::HmdMatrix34& head, float elevationDegrees,
                                       float distance) {
	// The heading: the head's forward (-z) flattened; straight up or down
	// falls back to the head's up turned forward.
	float fx = -head.m[0][2];
	float fz = -head.m[2][2];
	float len = math::Sqrt(fx * fx + fz * fz);
	if (len < 1e-4f) {
		fx = head.m[0][1];
		fz = head.m[2][1];
		len = math::Sqrt(fx * fx + fz * fz);
		if (len < 1e-4f) {
			fx = 0.0f;
			fz = -1.0f;
			len = 1.0f;
		}
		if (head.m[1][2] < 0.0f) {  // looking up: the head's up points back
			fx = -fx;
			fz = -fz;
		}
	}
	fx /= len;
	fz /= len;
	const float e = elevationDegrees * (math::kPi / 180.0f);
	const float ce = math::Cos(e);
	const float se = math::Sin(e);
	// Towards the quad, and the quad's axes: x right (level), z back to the
	// eyes, y = z cross x.
	const float dx = fx * ce, dy = se, dz = fz * ce;
	const float rx = -fz, ry = 0.0f, rz = fx;  // forward cross up
	const float zx = -dx, zy = -dy, zz = -dz;
	const float yx = zy * rz - zz * ry;
	const float yy = zz * rx - zx * rz;
	const float yz = zx * ry - zy * rx;
	openvr::HmdMatrix34 m{};
	m.m[0][0] = rx;
	m.m[1][0] = ry;
	m.m[2][0] = rz;
	m.m[0][1] = yx;
	m.m[1][1] = yy;
	m.m[2][1] = yz;
	m.m[0][2] = zx;
	m.m[1][2] = zy;
	m.m[2][2] = zz;
	m.m[0][3] = head.m[0][3] + dx * distance;
	m.m[1][3] = head.m[1][3] + dy * distance;
	m.m[2][3] = head.m[2][3] + dz * distance;
	return m;
}

// ---- One frame -------------------------------------------------------------

// What a frame knows: which hands are tracked, the head's pose, and each
// lifted element's rectangle in HUD units (invalid: not lifted this frame).
struct HandHudFrame {
	bool leftValid = false;
	bool rightValid = false;
	bool haveHead = false;
	openvr::HmdMatrix34 head{};
	UiRect rect[kHudElementCount];
	// Where the top and bottom rows were placed: the head's pose then
	// (StepViewAnchor); [0] the top, [1] the bottom.
	bool viewAnchorValid[2] = {false, false};
	openvr::HmdMatrix34 viewAnchor[2] = {};
	// With showOnLook: the controllers ([0] left, [1] right, physically) and
	// each hand HUD's opacity from StepHandFade.
	HandLook hand[2];
	float handAlpha[2] = {0.0f, 0.0f};
	// What the crosshair is on, for HudPlace::Target: the point the row hangs
	// from (its top edge's middle), absolute, facing the eyes
	// (TargetRowPose), and how far it is from them.
	bool targetValid = false;
	openvr::HmdMatrix34 target{};
	float targetDistanceMetres = 0.0f;
};

// Where a Target row hangs from, in the world (game units, z up): below the
// thing's bound - its centre less its radius, no more than
// kTargetHangMaxDropUnits down, so a door or a person does not send it to
// the floor - and a little gap lower still, never above the hit. Sideways
// under the thing's own middle when it is small (a bound within
// kTargetHangCentreUnits), so the text stands still while the ray's hit
// wanders over it (2026-10-06); under the hit for a large thing, where the
// middle of a door or a person can be far from where one points. Without a
// bound, just under the hit.
constexpr float kTargetHangMaxDropUnits = 20.0f;  // 28 cm
constexpr float kTargetHangGapUnits = 3.0f;       // 4 cm
constexpr float kTargetHangCentreUnits = 40.0f;   // 56 cm

inline NiPoint3 TargetHangPoint(const NiPoint3& hit, bool haveBound, const NiPoint3& boundCentre, float boundRadius) {
	float x = hit.x;
	float y = hit.y;
	float z = hit.z;
	if (haveBound) {
		const float drop = boundRadius < kTargetHangMaxDropUnits ? (boundRadius > 0.0f ? boundRadius : 0.0f)
		                                                         : kTargetHangMaxDropUnits;
		const float below = boundCentre.z - drop;
		z = below < z ? below : z;
		if (boundRadius <= kTargetHangCentreUnits) {
			x = boundCentre.x;
			y = boundCentre.y;
		}
	}
	return NiPoint3{x, y, z - kTargetHangGapUnits};
}

// Where the crosshair quad - the game's reticle with the context icon it
// carries, the hand, the lock, the speech bubble - hangs in Full VR (the
// tester, 2026-10-07: "die tooltips vom laserpointer ausschneiden und
// stattdessen wie beim heben von objekten in die welt legen über das
// objekt wo die hand hinzeigt oder mittig der vr view (wechselbar in
// settings). denn so muss man bischen schielen"). On the laser, ahead of
// the pointing hand at the aim's depth, the quad stands off the line of
// sight and the eyes have to cross for it; over the thing the hand points
// at, facing the eyes, it stands where the eyes already are; in the view,
// straight ahead of the head at the aim's depth. [Hands] CrosshairPlace.
enum class CrosshairPlace : UInt8 {
	Laser = 0,
	Target = 1,
	View = 2,
};
inline constexpr UInt32 kCrosshairPlaceCount = 3;
inline constexpr const char* kCrosshairPlaceNames[kCrosshairPlaceCount] = {"laser", "target", "view"};
inline constexpr CrosshairPlace kCrosshairPlaceDefault = CrosshairPlace::Target;

inline bool ParseCrosshairPlace(const char* text, CrosshairPlace& out) {
	UInt32 index = 0;
	if (!MatchChoiceWord(text, kCrosshairPlaceNames, kCrosshairPlaceCount, index)) {
		return false;
	}
	out = static_cast<CrosshairPlace>(index);
	return true;
}

inline CrosshairPlace CrosshairPlaceFromIndex(float value) {
	if (!(value > -0.5f) || !(value < static_cast<float>(kCrosshairPlaceCount) - 0.5f)) {
		return kCrosshairPlaceDefault;
	}
	return static_cast<CrosshairPlace>(static_cast<int>(value + 0.5f));
}

// Where the quad goes this frame. Over the target only while the pick has
// settled on something (the Info row's anchor, game/PickHold.h) - with
// nothing under the hand the plain reticle rides the laser as before, so
// the dot and the reticle do not come apart. The reach ring's icon and an
// arrow on the string are placed by their owners and win over all of this.
enum class CrosshairQuadAt : UInt8 { Laser, Target, Head };
inline CrosshairQuadAt CrosshairQuadPlace(CrosshairPlace place, bool fullVr, bool targetSettled) {
	if (!fullVr) {
		return CrosshairQuadAt::Head;
	}
	switch (place) {
	case CrosshairPlace::Target:
		return targetSettled ? CrosshairQuadAt::Target : CrosshairQuadAt::Laser;
	case CrosshairPlace::View:
		return CrosshairQuadAt::Head;
	default:
		return CrosshairQuadAt::Laser;
	}
}

// Where the quad hovers over a thing, from the Target row's hang point
// under it (TargetHangPoint): the same sideways, and above the bound of a
// small thing or above the hit of a large one by as much as the row hangs
// below - the mirror of the hang, so icon and name frame the thing.
inline NiPoint3 TargetHoverPoint(const NiPoint3& hang, bool haveBound, const NiPoint3& boundCentre,
                                 float boundRadius) {
	if (!haveBound) {
		return NiPoint3{hang.x, hang.y, hang.z + 2.0f * kTargetHangGapUnits};
	}
	if (boundRadius <= kTargetHangCentreUnits) {
		const float radius = boundRadius > 0.0f ? boundRadius : 0.0f;
		return NiPoint3{hang.x, hang.y, boundCentre.z + radius + kTargetHangGapUnits};
	}
	return NiPoint3{hang.x, hang.y, hang.z + 2.0f * (kTargetHangMaxDropUnits + kTargetHangGapUnits)};
}

// The pose a Target row hangs from: at `point` (tracking space), its face
// turned to the head, level - as the compass's and the dialogue panel's.
inline openvr::HmdMatrix34 TargetRowPose(const openvr::HmdMatrix34& head, float px, float py, float pz) {
	float zx = head.m[0][3] - px, zy = head.m[1][3] - py, zz = head.m[2][3] - pz;
	float len = math::Sqrt(zx * zx + zy * zy + zz * zz);
	if (len < 1e-4f) {
		zx = 0.0f, zy = 0.0f, zz = 1.0f, len = 1.0f;
	}
	zx /= len, zy /= len, zz /= len;
	float xx = zz, xz = -zx;
	float xl = math::Sqrt(xx * xx + xz * xz);
	if (xl < 1e-4f) {
		xx = 1.0f, xz = 0.0f, xl = 1.0f;
	}
	xx /= xl, xz /= xl;
	const float xy = 0.0f;
	openvr::HmdMatrix34 m{};
	m.m[0][0] = xx, m.m[1][0] = xy, m.m[2][0] = xz;
	m.m[0][1] = zy * xz - zz * xy, m.m[1][1] = zz * xx - zx * xz, m.m[2][1] = zx * xy - zy * xx;
	m.m[0][2] = zx, m.m[1][2] = zy, m.m[2][2] = zz;
	m.m[0][3] = px, m.m[1][3] = py, m.m[2][3] = pz;
	return m;
}

// The head's heading and another's, apart by this many degrees (yaw only).
inline float HeadingApartDegrees(const openvr::HmdMatrix34& a, const openvr::HmdMatrix34& b) {
	const float ax = -a.m[0][2], az = -a.m[2][2];
	const float bx = -b.m[0][2], bz = -b.m[2][2];
	const float la = math::Sqrt(ax * ax + az * az);
	const float lb = math::Sqrt(bx * bx + bz * bz);
	if (la < 1e-4f || lb < 1e-4f) {
		return 0.0f;  // straight up or down: no heading to be apart by
	}
	const float dot = ax * bx + az * bz;
	float cross = ax * bz - az * bx;
	cross = cross < 0.0f ? -cross : cross;
	return math::Atan2(cross, dot) * (180.0f / math::kPi);
}

// A top or bottom row's anchor for this frame. Taken from the head when the
// row appears, when the head has turned more than `followDegrees` from it
// (0: never), or every frame when locked to the head; dropped when the row
// has nothing to show, so the next one appears where the head looks then.
struct HudViewAnchor {
	bool valid = false;
	openvr::HmdMatrix34 head{};
};
inline void StepViewAnchor(HudViewAnchor& a, bool shown, bool haveHead, const openvr::HmdMatrix34& head,
                           float followDegrees, bool lockedToHead) {
	if (!shown || !haveHead) {
		a.valid = shown && a.valid;  // no head this frame: keep what there is
		return;
	}
	if (!a.valid || lockedToHead ||
	    (followDegrees > 0.0f && HeadingApartDegrees(a.head, head) > followDegrees)) {
		a.head = head;
		a.valid = true;
	}
}

// Where one element's quad goes: on a hand (relative to that controller) or
// in the room (absolute), its width and its opacity.
struct HandHudQuad {
	bool shown = false;
	bool onDevice = false;
	bool rightHand = false;
	openvr::HmdMatrix34 pose{};
	float widthMetres = 0.0f;
	float alpha = 0.0f;
};

// The pose moved `sideways` metres along its own x axis.
inline openvr::HmdMatrix34 AlongOwnX(openvr::HmdMatrix34 pose, float sideways) {
	pose.m[0][3] += pose.m[0][0] * sideways;
	pose.m[1][3] += pose.m[1][0] * sideways;
	pose.m[2][3] += pose.m[2][0] * sideways;
	return pose;
}

// And along its own x and y: sideways and up on the quad's face.
inline openvr::HmdMatrix34 AlongOwnXY(openvr::HmdMatrix34 pose, float sideways, float up) {
	pose = AlongOwnX(pose, sideways);
	pose.m[0][3] += pose.m[0][1] * up;
	pose.m[1][3] += pose.m[1][1] * up;
	pose.m[2][3] += pose.m[2][1] * up;
	return pose;
}

// Every element's quad this frame. The elements of one hand stand in a row
// across its panel in the order of HudElement; the sky's in a row across the
// compass's place. An element whose hand is not tracked, or with no head
// pose for the sky, is not shown.
inline void PlaceHandHud(const HandHudSettings& s, const HandHudFrame& f, HandHudQuad out[kHudElementCount]) {
	for (UInt32 e = 0; e < kHudElementCount; ++e) {
		out[e] = HandHudQuad{};
	}
	if (!s.enabled) {
		return;
	}
	const HudPlace rows[6] = {HudPlace::Left, HudPlace::Right,  HudPlace::Sky,
	                          HudPlace::Top,  HudPlace::Bottom, HudPlace::Target};
	for (HudPlace row : rows) {
		const bool inView = row == HudPlace::Top || row == HudPlace::Bottom;
		const bool atTarget = row == HudPlace::Target;
		const UInt32 viewRow = row == HudPlace::Top ? 0u : 1u;
		UInt32 members[kHudElementCount];
		float widths[kHudElementCount];
		float centres[kHudElementCount];
		UInt32 n = 0;
		// At the target the text keeps the size it has at the view's distance,
		// however far the thing is: the unit grows with the distance.
		const float targetScale =
			atTarget && s.viewDistanceMetres > 0.0f ? f.targetDistanceMetres / s.viewDistanceMetres : 1.0f;
		const float unit = row == HudPlace::Sky ? s.skyUnitMetres
		                   : inView             ? s.viewUnitMetres
		                   : atTarget           ? s.viewUnitMetres * targetScale
		                                        : s.unitMetres;
		for (UInt32 e = 0; e < kHudElementCount; ++e) {
			const HudElementSettings& es = s.element[e];
			if (es.place != row || !f.rect[e].valid) {
				continue;
			}
			members[n] = e;
			widths[n] = (f.rect[e].right - f.rect[e].left) * unit * es.size;
			++n;
		}
		if (n == 0) {
			continue;
		}
		LayoutHandRow(widths, n, (row == HudPlace::Sky || inView || atTarget) ? s.gapMetres * 3.0f : s.gapMetres,
		              centres);
		const bool tracked = row == HudPlace::Left    ? f.leftValid
		                     : row == HudPlace::Right ? f.rightValid
		                     : inView                 ? f.viewAnchorValid[viewRow]
		                     : atTarget               ? f.targetValid && f.haveHead
		                                              : f.haveHead;
		if (!tracked) {
			continue;
		}
		float skyAlpha = 0.0f;
		openvr::HmdMatrix34 skyPose{};
		if (row == HudPlace::Sky) {
			skyPose = CompassPose(f.head, s.compassElevationDegrees, s.compassDistanceMetres);
			skyAlpha = CompassOpacity(HeadPitchDegrees(f.head), s.compassFadeStartDegrees,
			                          s.compassFadeFullDegrees, 1.0f);
		} else if (inView) {
			skyPose = CompassPose(f.viewAnchor[viewRow], viewRow == 0 ? s.viewTopDegrees : -s.viewBottomDegrees,
			                      s.viewDistanceMetres);
			skyAlpha = 1.0f;
		}
		for (UInt32 i = 0; i < n; ++i) {
			const UInt32 e = members[i];
			HandHudQuad& q = out[e];
			q.widthMetres = widths[i];
			if (row == HudPlace::Sky || inView) {
				q.onDevice = false;
				q.pose = AlongOwnX(skyPose, centres[i]);
				q.alpha = skyAlpha * s.element[e].opacity;
			} else if (atTarget) {
				// Hung from its top edge: half its height lower than the anchor.
				const UiRect& rc = f.rect[e];
				const float aspect = (rc.bottom - rc.top) / (rc.right - rc.left);
				q.onDevice = false;
				q.pose = AlongOwnXY(f.target, centres[i], -widths[i] * aspect * 0.5f);
				q.alpha = s.element[e].opacity;
			} else {
				q.onDevice = true;
				q.rightHand = row == HudPlace::Right;
				const UInt32 side = q.rightHand ? 1u : 0u;
				if (s.showOnLook) {
					// Over the palm, facing the eyes, while looked at; hung on
					// the controller so the compositor carries it with the hand.
					const HandLook& hand = f.hand[side];
					if (!hand.valid || !f.haveHead) {
						continue;
					}
					q.pose = RelativeToDevice(
						hand.pose, AlongOwnXY(PalmPanelPose(hand, q.rightHand, f.head, s.palmLiftMetres, s.palmDownMetres),
					                          centres[i] + s.offsetRightMetres, s.offsetUpMetres));
					q.alpha = s.element[e].opacity * f.handAlpha[side];
				} else {
					q.pose = AlongOwnXY(HandPanelTransform(s.panelUp, s.panelBack, s.panelTiltDegrees, centres[i]),
					                    s.offsetRightMetres, s.offsetUpMetres);
					q.alpha = s.element[e].opacity;
				}
			}
			q.shown = q.widthMetres > 0.0f && q.alpha > 0.0f;
		}
	}
}

}  // namespace obvr::vr
