#pragma once

// The HUD's elements as the HUD on the hands knows them
// (docs/hud-on-hands-spec.md), and the margin each one's rectangle keeps
// round its tiles. Split out of vr/HandHud.h. Pure; covered by
// hand_hud_test.

#include "core/Types.h"
#include "vr/HudTileTree.h"

namespace obvr::vr {

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
	XpText = 10,    // Ultimate Leveling's text line over the bars (hudmain_xp_description)
};
inline constexpr UInt32 kHudElementCount = 11;

// The INI's key stems ([HandHud] <Stem>Place, <Stem>Opacity, <Stem>Size).
inline constexpr const char* kHudElementKeys[kHudElementCount] = {
	"Bars", "Spell", "Weapon", "Effects", "LevelUp", "Compass", "Region", "Messages", "Subtitles", "Info", "XpText"};

// Which menu an element's tiles are in: HUDMainMenu, HUDSubtitleMenu for the
// notices and the subtitles (hud_subtitle_menu.xml), HUDInfoMenu for what the
// crosshair is on.
inline bool HudElementInSubtitleMenu(HudElement e) {
	return e == HudElement::Messages || e == HudElement::Subtitles;
}
inline bool HudElementInInfoMenu(HudElement e) { return e == HudElement::Info; }

// The elements that are mostly text: the region's name, the notices, the
// subtitles, the crosshair's info and Ultimate Leveling's line.
inline bool HudElementIsText(HudElement e) {
	return e == HudElement::Region || e == HudElement::Messages || e == HudElement::Subtitles ||
	       e == HudElement::Info || e == HudElement::XpText;
}

// The margin round a text element, in HUD units on every side. A text tile's
// width and height are its string's measured box, and the letters are drawn
// past it: the font's flourishes and the filtered edge of every glyph. Cut at
// the box, those edges stayed in the main panel as fringes on the letters
// (Nadi, 2026-11-28, Ultimate Leveling's line among others: "Fransen an den
// Buchstaben"), and the copy on the hand lost them. 4 units is a first guess
// (about 9 pixels at 2.3 a unit), not measured yet: the harness's
// hand-hud-fringe.txt logs at its marks how far the letters reach past their
// box before the cut and what is left round the cut after it
// (render/HandHudMarks.cpp; docs/hud-on-hands-spec.md section 13). In
// vanilla's layout the XP line stays above the compass (845) and the bars
// (847), the region's name left of the compass (480); the region's box
// already reaches 3 units into the covers over the weapon and the spell, which
// are lifted before it (hand_hud_test).
inline constexpr float kHudTextMarginUnits = 4.0f;

// Each active effect's timer arc (icon_timer, a NIF without a size) reaches
// past the icon - measured 2026-11-15 about 5 units left, 8 right, 13 above.
inline constexpr float kHudEffectsMarginLeft = 8.0f;
inline constexpr float kHudEffectsMarginRight = 12.0f;
inline constexpr float kHudEffectsMarginTop = 16.0f;

// An element's rectangle with its margin; an invalid one stays invalid.
inline UiRect PadHudElementRect(const UiRect& r, HudElement e) {
	if (!r.valid) {
		return r;
	}
	UiRect out = r;
	if (e == HudElement::Effects) {
		out.left -= kHudEffectsMarginLeft;
		out.right += kHudEffectsMarginRight;
		out.top -= kHudEffectsMarginTop;
	} else if (HudElementIsText(e)) {
		out.left -= kHudTextMarginUnits;
		out.top -= kHudTextMarginUnits;
		out.right += kHudTextMarginUnits;
		out.bottom += kHudTextMarginUnits;
	}
	return out;
}

}  // namespace obvr::vr
