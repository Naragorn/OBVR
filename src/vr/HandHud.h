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
};
inline constexpr UInt32 kHudElementCount = 6;

// The INI's key stems ([HandHud] <Stem>Place, <Stem>Opacity, <Stem>Size).
inline constexpr const char* kHudElementKeys[kHudElementCount] = {"Bars",    "Spell",   "Weapon",
                                                                  "Effects", "LevelUp", "Compass"};

// Where an element goes. View leaves it in the main panel ahead of the
// player, as the game drew it; Off takes it out and shows it nowhere.
enum class HudPlace : UInt8 {
	View = 0,
	Left = 1,
	Right = 2,
	Sky = 3,
	Off = 4,
};
inline constexpr UInt32 kHudPlaceCount = 5;
inline constexpr const char* kHudPlaceNames[kHudPlaceCount] = {"view", "left", "right", "sky", "off"};

inline bool ParseHudPlace(const char* text, HudPlace& out) {
	UInt32 index = 0;
	if (!MatchChoiceWord(text, kHudPlaceNames, kHudPlaceCount, index)) {
		return false;
	}
	out = static_cast<HudPlace>(index);
	return true;
}

// The settings row's value (0..4) as a place; out of range is View, the
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
// right hand the rest; the compass in the sky.
struct HandHudSettings {
	bool enabled = true;
	HudElementSettings element[kHudElementCount] = {
		{HudPlace::Left, 1.0f, 1.0f},  {HudPlace::Left, 1.0f, 1.0f},  {HudPlace::Right, 1.0f, 1.0f},
		{HudPlace::Right, 1.0f, 1.0f}, {HudPlace::Right, 1.0f, 1.0f}, {HudPlace::Sky, 1.0f, 1.0f},
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
};

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
};

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
};
inline constexpr const char* kHudEffectsRoot = "magic_icons";

// An element's rectangle in HUD units, invalid when none of its tiles is
// found with a size - a UI mod that renamed them, or nothing to show (no
// active effect).
inline UiRect HudElementRect(const HudTile* tiles, UInt32 count, HudElement element) {
	UiRect r;
	if (element == HudElement::Effects) {
		const SInt32 root = FindHudTile(tiles, count, kHudEffectsRoot);
		if (root < 0) {
			return r;
		}
		for (UInt32 i = 0; i < count; ++i) {
			if (HudTileUnder(tiles, count, static_cast<SInt32>(i), root)) {
				r = UnionRect(r, HudTileRect(tiles, count, static_cast<SInt32>(i)));
			}
		}
		return r;
	}
	for (const char* name : kHudElementTiles[static_cast<UInt32>(element)]) {
		if (name != nullptr) {
			r = UnionRect(r, HudTileRect(tiles, count, FindHudTile(tiles, count, name)));
		}
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
inline constexpr UInt32 kHandHudAtlasHeight = 1024;
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
};

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
	const HudPlace rows[3] = {HudPlace::Left, HudPlace::Right, HudPlace::Sky};
	for (HudPlace row : rows) {
		UInt32 members[kHudElementCount];
		float widths[kHudElementCount];
		float centres[kHudElementCount];
		UInt32 n = 0;
		const float unit = row == HudPlace::Sky ? s.skyUnitMetres : s.unitMetres;
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
		LayoutHandRow(widths, n, row == HudPlace::Sky ? s.gapMetres * 3.0f : s.gapMetres, centres);
		const bool tracked = row == HudPlace::Left ? f.leftValid : row == HudPlace::Right ? f.rightValid : f.haveHead;
		if (!tracked) {
			continue;
		}
		float skyAlpha = 0.0f;
		openvr::HmdMatrix34 skyPose{};
		if (row == HudPlace::Sky) {
			skyPose = CompassPose(f.head, s.compassElevationDegrees, s.compassDistanceMetres);
			skyAlpha = CompassOpacity(HeadPitchDegrees(f.head), s.compassFadeStartDegrees,
			                          s.compassFadeFullDegrees, 1.0f);
		}
		for (UInt32 i = 0; i < n; ++i) {
			const UInt32 e = members[i];
			HandHudQuad& q = out[e];
			q.widthMetres = widths[i];
			if (row == HudPlace::Sky) {
				q.onDevice = false;
				q.pose = AlongOwnX(skyPose, centres[i]);
				q.alpha = skyAlpha * s.element[e].opacity;
			} else {
				q.onDevice = true;
				q.rightHand = row == HudPlace::Right;
				q.pose = HandPanelTransform(s.panelUp, s.panelBack, s.panelTiltDegrees, centres[i]);
				q.alpha = s.element[e].opacity;
			}
			q.shown = q.widthMetres > 0.0f && q.alpha > 0.0f;
		}
	}
}

}  // namespace obvr::vr
