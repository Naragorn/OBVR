#pragma once

#include "core/Types.h"

namespace obvr::ui {

// The words OBVR gives Oblivion's controls in Full VR, where the game would
// name a key (docs/onboarding-spec.md 3.4; the tester, 2026-10-16: the new
// game's tutorial cards are to say the VR controls).
//
// The engine keeps its 29 mappable controls in one order (xOBSE
// GameOSDepend.h, MappableControl): the control map's rows, the 29
// control-name settings (sUActnForward ... sUActnGrab) and the formatter
// addr::kControlKeyName that turns a control index into "push W" all count
// the same way. A text that carries "&sUActnForward;" - the Tutorial quest's
// stage entries in Oblivion.esm, any text the font lays out - gets that
// formatter's answer spliced in; with the mode on, game/ControlWords.h
// answers from here instead. The phrases finish vanilla's sentences - "To
// move forward, ...", "approach it and ... when its name appears", "To
// attack, ...", "so ... to bring up your journal" - so each is a verb
// phrase without a capital or a full stop. The same bindings stand in
// ui/NativeControlsCard.h and README.md ("Controls in Full VR"); change all
// three. Pure, covered by control_phrase_test.

enum ControlIndex : UInt32 {
	kControlForward = 0,
	kControlBack = 1,
	kControlStrafeLeft = 2,
	kControlStrafeRight = 3,
	kControlUseEquipped = 4,  // attack; with a bow, the draw
	kControlActivate = 5,
	kControlBlock = 6,
	kControlCast = 7,
	kControlDrawSheatheWeapon = 8,
	kControlSneak = 9,
	kControlRun = 10,
	kControlAutoRun = 11,
	kControlAutoMove = 12,
	kControlJump = 13,
	kControlTogglePov = 14,
	kControlMenuMode = 15,
	kControlRest = 16,
	kControlQuickMenu = 17,
	kControlQuick1 = 18,
	kControlQuick8 = 25,
	kControlQuickSave = 26,
	kControlQuickLoad = 27,
	kControlGrab = 28,
	kControlCount = 29,
};

// How the bow is shot, which the attack control's words follow (the tester,
// 2026-10-16: "hold the right trigger to grab an arrow and release to shoot
// (wenn das arrow graben auf trigger gestellt ist je nach option)"):
// [Hands] BowByHand=0 - the trigger draws the bow while held and looses on
// release; BowByHand=1 - the arrow is taken from the quiver by the weapon
// hand's trigger (ArrowWithGrip=0) or its grip (ArrowWithGrip=1), nocked
// behind the bow, drawn and released (docs/controls-spec.md 4.2).
enum class BowWords { TriggerDraws, HandTrigger, HandGrip };

inline BowWords BowWordsFor(bool bowByHand, bool arrowWithTrigger) {
	if (!bowByHand) {
		return BowWords::TriggerDraws;
	}
	return arrowWithTrigger ? BowWords::HandTrigger : BowWords::HandGrip;
}

// xOBSE's name of the control, for the log; "?" past the last.
inline const char* ControlName(UInt32 control) {
	static const char* const kNames[kControlCount] = {
		"Forward",   "Back",     "StrafeLeft", "StrafeRight", "UseEquipped", "Activate",
		"Block",     "Cast",     "DrawSheatheWeapon", "Sneak", "Run",       "AutoRun",
		"AutoMove",  "Jump",     "TogglePOV",  "MenuMode",    "Rest",        "QuickMenu",
		"Quick1",    "Quick2",   "Quick3",     "Quick4",      "Quick5",      "Quick6",
		"Quick7",    "Quick8",   "QuickSave",  "QuickLoad",   "Grab"};
	return control < kControlCount ? kNames[control] : "?";
}

// The engine's name of the control's setting - what a text's "&name;"
// carries (the 29 strings at 0xA62674..0xA62854 of Oblivion.exe, read
// 2026-10-16); "" past the last.
inline const char* ControlToken(UInt32 control) {
	static const char* const kTokens[kControlCount] = {
		"sUActnForward",   "sUActnBack",      "sUActnSldleft",  "sUActnSldright", "sUActnUse",
		"sUActnActivate",  "sUActnBlock",     "sUActnCast",     "sUActnRdyitem",  "sUActnCrouch",
		"sUActnRun",       "sUActnTogglerun", "sUActnAutomove", "sUActnJump",     "sUActnTogglepov",
		"sUActnMenumode",  "sUActnRestmenu",  "sUActnQuickmenu", "sUActnQuick1",  "sUActnQuick2",
		"sUActnQuick3",    "sUActnQuick4",    "sUActnQuick5",   "sUActnQuick6",   "sUActnQuick7",
		"sUActnQuick8",    "sUActnQuicksave", "sUActnQuickload", "sUActnGrab"};
	return control < kControlCount ? kTokens[control] : "";
}

// The control a token names, kControlCount for none (the engine's own
// lookup is a settings map; here the 29 names, case as the engine spells
// them).
inline UInt32 ControlIndexOfToken(const char* token) {
	if (token == nullptr) {
		return kControlCount;
	}
	for (UInt32 control = 0; control < kControlCount; ++control) {
		const char* a = ControlToken(control);
		const char* b = token;
		while (*a != '\0' && *a == *b) {
			++a;
			++b;
		}
		if (*a == '\0' && *b == '\0') {
			return control;
		}
	}
	return kControlCount;
}

// Room enough for the longest phrase with either hand's words.
constexpr UInt32 kControlPhraseCapacity = 128;

// OBVR's phrase for the control with the mode on, written to `out`. False
// and `out` untouched for a control OBVR has no words for - always run,
// auto move, change view, quick save and quick load have no VR control, so
// the engine's own answer stands - for one past the last, for no buffer and
// for one too small.
inline bool ControlPhrase(UInt32 control, bool leftHanded, BowWords bow, char* out, UInt32 capacity) {
	if (out == nullptr || capacity == 0 || control >= kControlCount) {
		return false;
	}
	// The weapon hand's side and the other hand's; the walk is the other
	// hand's stick, the turn and the flicks the weapon hand's.
	const char* const weapon = leftHanded ? "left" : "right";
	const char* const other = leftHanded ? "right" : "left";
	char slot[2] = {'1', '\0'};
	const char* pieces[5] = {"", "", "", "", ""};
	switch (control) {
	case kControlForward:
		pieces[0] = "push the ";
		pieces[1] = other;
		pieces[2] = " stick forward";
		break;
	case kControlBack:
		pieces[0] = "pull the ";
		pieces[1] = other;
		pieces[2] = " stick back";
		break;
	case kControlStrafeLeft:
		pieces[0] = "push the ";
		pieces[1] = other;
		pieces[2] = " stick left";
		break;
	case kControlStrafeRight:
		pieces[0] = "push the ";
		pieces[1] = other;
		pieces[2] = " stick right";
		break;
	case kControlUseEquipped:
		pieces[0] = "swing the weapon - with a bow, hold the ";
		pieces[1] = weapon;
		if (bow == BowWords::TriggerDraws) {
			pieces[2] = " trigger to draw and let go to shoot";
		} else if (bow == BowWords::HandTrigger) {
			pieces[2] = " trigger to grab an arrow and release to shoot";
		} else {
			pieces[2] = " grip to grab an arrow and release to shoot";
		}
		break;
	case kControlActivate:
		pieces[0] = "press A on the ";
		pieces[1] = weapon;
		pieces[2] = " controller";
		break;
	case kControlBlock:
		pieces[0] = "raise your ";
		pieces[1] = other;
		pieces[2] = " hand with the shield on its arm, or hold the blade across the blow";
		break;
	case kControlCast:
		pieces[0] = "pull the ";
		pieces[1] = other;
		pieces[2] = " trigger";
		break;
	case kControlDrawSheatheWeapon:
		pieces[0] = "reach to your ";
		pieces[1] = other;
		pieces[2] = " hip and squeeze the ";
		pieces[3] = weapon;
		pieces[4] = " grip, or make a fist";
		break;
	case kControlSneak:
		pieces[0] = "flick the ";
		pieces[1] = weapon;
		pieces[2] = " stick down, or crouch";
		break;
	case kControlRun:
		pieces[0] = "hold the ";
		pieces[1] = other;
		pieces[2] = " stick clicked in";
		break;
	case kControlJump:
		pieces[0] = "flick the ";
		pieces[1] = weapon;
		pieces[2] = " stick up";
		break;
	case kControlMenuMode:
		pieces[0] = "press B on the ";
		pieces[1] = other;
		pieces[2] = " controller";
		break;
	case kControlRest:
		pieces[0] = "hold the ";
		pieces[1] = weapon;
		pieces[2] = " stick clicked in";
		break;
	case kControlQuickMenu:
		pieces[0] = "click the ";
		pieces[1] = weapon;
		pieces[2] = " stick";
		break;
	case kControlGrab:
		pieces[0] = "squeeze the grip on it";
		break;
	default:
		if (control >= kControlQuick1 && control <= kControlQuick8) {
			slot[0] = static_cast<char>('1' + (control - kControlQuick1));
			pieces[0] = "pick slot ";
			pieces[1] = slot;
			pieces[2] = " on the hotkey ring";
			break;
		}
		return false;
	}
	char phrase[kControlPhraseCapacity];
	UInt32 at = 0;
	for (const char* piece : pieces) {
		for (const char* c = piece; *c != '\0'; ++c) {
			phrase[at++] = *c;
		}
	}
	phrase[at] = '\0';
	if (at + 1 > capacity) {
		return false;
	}
	for (UInt32 i = 0; i <= at; ++i) {
		out[i] = phrase[i];
	}
	return true;
}

}  // namespace obvr::ui
