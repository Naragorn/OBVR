#pragma once

#include "core/Types.h"

namespace obvr::vr {

// ------------------------------------------------------- The button bindings
//
// Which controller input does which action in Full VR, in the world
// (OBVR_Controls.ini, [Buttons]; a tester, 2026-11-07: "A kick hotkey? ...
// I would also rather it be jumping, as pressing on thumbsticks can cause
// quicker drift"; Nadi: "b als eigene ini"). The defaults are the layout as
// it was built (docs/controls-spec.md section 1), so a missing or empty file
// changes nothing.
//
// What can be bound: the discrete actions - the ones a press or a hold
// starts. What stays fixed: the sticks' walking and turning, the triggers,
// the grips, the gestures, the rings, the stick chord, and every button in a
// menu (there the B buttons are Tab and Escape, the left A drops).
//
// "Right" is the weapon hand: left-handed the controllers swap roles before
// any of this (vr::AssignHandRoles). On an Oculus Touch the left A and B are
// X and Y.
//
// Pure, covered by button_bindings_test and hand_mode_test.

enum class ControlSource : UInt8 {
	None,
	RightA,
	RightAHold,
	RightB,
	RightBHold,
	LeftA,
	LeftAHold,
	LeftB,
	LeftBHold,
	RightStickUp,     // flicked up and back (vr::StepTeleportStick's jump)
	RightStickDown,   // flicked down
	RightStickClick,  // its button held past StickHoldSeconds (vr::StepStickHold)
	LeftStickClick,   // its button: held (level) and its click (the run toggle's)
	LeftTrackpad,     // its click (an Index; the spell ring's when that is on the pad)
	LeftABHold,       // the other hand's A and B held together past the hold (vr::StepChordHold)
	Count
};
constexpr UInt32 kControlSourceCount = static_cast<UInt32>(ControlSource::Count);

enum class ControlAction : UInt8 {
	Jump,
	Kick,         // only with [Hands] Kick=1
	PauseMenu,    // Escape
	Menus,        // Tab
	Activate,     // held while the input is held
	Sneak,
	ReadyWeapon,  // with [Hands] Holsters on: the Rest menu instead
	Run,          // held while the input is held; RunToggle switches on its press
	Journal,      // the game's Quick Menu key (F1)
	TogglePov,    // the view switch (R)
	QuickSave,    // the game's quick save key (F5)
	Count
};
constexpr UInt32 kControlActionCount = static_cast<UInt32>(ControlAction::Count);

// How many inputs one action may have.
constexpr UInt32 kSourcesPerAction = 3;

// The four buttons that can be held for a ring (the A and B of each hand).
enum class HoldButton : UInt8 { RightA, RightB, LeftA, LeftB, Count };
constexpr UInt32 kHoldButtonCount = static_cast<UInt32>(HoldButton::Count);

inline const char* ControlSourceName(ControlSource s) {
	switch (s) {
	case ControlSource::None: return "None";
	case ControlSource::RightA: return "RightA";
	case ControlSource::RightAHold: return "RightAHold";
	case ControlSource::RightB: return "RightB";
	case ControlSource::RightBHold: return "RightBHold";
	case ControlSource::LeftA: return "LeftA";
	case ControlSource::LeftAHold: return "LeftAHold";
	case ControlSource::LeftB: return "LeftB";
	case ControlSource::LeftBHold: return "LeftBHold";
	case ControlSource::RightStickUp: return "RightStickUp";
	case ControlSource::RightStickDown: return "RightStickDown";
	case ControlSource::RightStickClick: return "RightStickClick";
	case ControlSource::LeftStickClick: return "LeftStickClick";
	case ControlSource::LeftTrackpad: return "LeftTrackpad";
	case ControlSource::LeftABHold: return "LeftABHold";
	default: return "?";
	}
}

// The INI key of an action.
inline const char* ControlActionName(ControlAction a) {
	switch (a) {
	case ControlAction::Jump: return "Jump";
	case ControlAction::Kick: return "Kick";
	case ControlAction::PauseMenu: return "PauseMenu";
	case ControlAction::Menus: return "Menus";
	case ControlAction::Activate: return "Activate";
	case ControlAction::Sneak: return "Sneak";
	case ControlAction::ReadyWeapon: return "ReadyWeapon";
	case ControlAction::Run: return "Run";
	case ControlAction::Journal: return "Journal";
	case ControlAction::TogglePov: return "TogglePov";
	case ControlAction::QuickSave: return "QuickSave";
	default: return "?";
	}
}

// Said on the controls card and in the log: "B: <phrase>".
inline const char* ControlActionPhrase(ControlAction a) {
	switch (a) {
	case ControlAction::Jump: return "jump";
	case ControlAction::Kick: return "kick";
	case ControlAction::PauseMenu: return "the pause menu";
	case ControlAction::Menus: return "the Tab menus";
	case ControlAction::Activate: return "use";
	case ControlAction::Sneak: return "sneak";
	case ControlAction::ReadyWeapon: return "ready weapon";
	case ControlAction::Run: return "run";
	case ControlAction::Journal: return "the journal";
	case ControlAction::TogglePov: return "switch the view";
	case ControlAction::QuickSave: return "quick save";
	default: return "?";
	}
}

// The button a source belongs to, for the A and B sources; Count for the rest.
inline HoldButton HoldButtonOf(ControlSource s) {
	switch (s) {
	case ControlSource::RightA:
	case ControlSource::RightAHold: return HoldButton::RightA;
	case ControlSource::RightB:
	case ControlSource::RightBHold: return HoldButton::RightB;
	case ControlSource::LeftA:
	case ControlSource::LeftAHold: return HoldButton::LeftA;
	case ControlSource::LeftB:
	case ControlSource::LeftBHold: return HoldButton::LeftB;
	default: return HoldButton::Count;
	}
}

inline bool IsHoldSource(ControlSource s) {
	return s == ControlSource::RightAHold || s == ControlSource::RightBHold || s == ControlSource::LeftAHold ||
	       s == ControlSource::LeftBHold;
}

// The weapon hand's (true) or the other hand's (false).
inline bool SourceOnRightHand(ControlSource s) {
	switch (s) {
	case ControlSource::RightA:
	case ControlSource::RightAHold:
	case ControlSource::RightB:
	case ControlSource::RightBHold:
	case ControlSource::RightStickUp:
	case ControlSource::RightStickDown:
	case ControlSource::RightStickClick: return true;
	default: return false;
	}
}

// "the left B", for the log and the card.
inline const char* HoldButtonPhrase(HoldButton b) {
	switch (b) {
	case HoldButton::RightA: return "the right A";
	case HoldButton::RightB: return "the right B";
	case HoldButton::LeftA: return "the left A";
	case HoldButton::LeftB: return "the left B";
	default: return "?";
	}
}

inline ControlSource TapSourceOf(HoldButton b) {
	switch (b) {
	case HoldButton::RightA: return ControlSource::RightA;
	case HoldButton::RightB: return ControlSource::RightB;
	case HoldButton::LeftA: return ControlSource::LeftA;
	case HoldButton::LeftB: return ControlSource::LeftB;
	default: return ControlSource::None;
	}
}

inline ControlSource HoldSourceOf(HoldButton b) {
	switch (b) {
	case HoldButton::RightA: return ControlSource::RightAHold;
	case HoldButton::RightB: return ControlSource::RightBHold;
	case HoldButton::LeftA: return ControlSource::LeftAHold;
	case HoldButton::LeftB: return ControlSource::LeftBHold;
	default: return ControlSource::None;
	}
}

// The inputs of every action. A slot left None is empty.
struct ControlBindings {
	ControlSource sources[kControlActionCount][kSourcesPerAction] = {};
};

inline bool operator==(const ControlBindings& a, const ControlBindings& b) {
	for (UInt32 i = 0; i < kControlActionCount; ++i) {
		for (UInt32 j = 0; j < kSourcesPerAction; ++j) {
			if (a.sources[i][j] != b.sources[i][j]) {
				return false;
			}
		}
	}
	return true;
}
inline bool operator!=(const ControlBindings& a, const ControlBindings& b) { return !(a == b); }

inline void BindSource(ControlBindings& b, ControlAction a, ControlSource s) {
	ControlSource* slots = b.sources[static_cast<UInt32>(a)];
	for (UInt32 j = 0; j < kSourcesPerAction; ++j) {
		if (slots[j] == ControlSource::None) {
			slots[j] = s;
			return;
		}
	}
}

inline bool ActionHasSource(const ControlBindings& b, ControlAction a, ControlSource s) {
	for (UInt32 j = 0; j < kSourcesPerAction; ++j) {
		if (b.sources[static_cast<UInt32>(a)][j] == s) {
			return true;
		}
	}
	return false;
}

// The action that has this input, or Count for none.
inline ControlAction ActionOfSource(const ControlBindings& b, ControlSource s) {
	if (s == ControlSource::None) {
		return ControlAction::Count;
	}
	for (UInt32 i = 0; i < kControlActionCount; ++i) {
		if (ActionHasSource(b, static_cast<ControlAction>(i), s)) {
			return static_cast<ControlAction>(i);
		}
	}
	return ControlAction::Count;
}

// Whether a button has a hold bound: its press then waits for the release
// (a tap) or the hold, with the ring over the controller.
inline bool ButtonHasHold(const ControlBindings& b, HoldButton button) {
	return ActionOfSource(b, HoldSourceOf(button)) != ControlAction::Count;
}

// The layout as built (since 2026-11-13): the weapon hand's B jumps and the
// other B held is the pause menu (its tap Tab); with [Hands] Kick the right
// stick flicked up kicks, without it the flick jumps as well; the other hand's
// A and B held together quick save (2026-11-13). In a menu the
// weapon hand's B stays Escape (HandInput's menu path, not these bindings).
inline ControlBindings DefaultControlBindings(bool kick) {
	ControlBindings b;
	BindSource(b, ControlAction::Jump, ControlSource::RightB);
	if (kick) {
		BindSource(b, ControlAction::Kick, ControlSource::RightStickUp);
	} else {
		BindSource(b, ControlAction::Jump, ControlSource::RightStickUp);
	}
	BindSource(b, ControlAction::PauseMenu, ControlSource::LeftBHold);
	BindSource(b, ControlAction::Menus, ControlSource::LeftB);
	BindSource(b, ControlAction::Activate, ControlSource::RightA);
	BindSource(b, ControlAction::Sneak, ControlSource::RightStickDown);
	BindSource(b, ControlAction::ReadyWeapon, ControlSource::RightStickClick);
	BindSource(b, ControlAction::Run, ControlSource::LeftStickClick);
	BindSource(b, ControlAction::Journal, ControlSource::LeftTrackpad);
	BindSource(b, ControlAction::QuickSave, ControlSource::LeftABHold);
	return b;
}

// ------------------------------------------------------------ The file
//
// What OBVR_Controls.ini said, before it is laid over the defaults: for each
// action whether its key was there, and its inputs. A key that is there and
// empty, or "None", unbinds the action.
struct ControlBindingFile {
	bool set[kControlActionCount] = {};
	ControlSource sources[kControlActionCount][kSourcesPerAction] = {};
};

inline char BindingLowerAscii(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

inline bool BindingWordEquals(const char* word, UInt32 length, const char* name) {
	UInt32 i = 0;
	for (; i < length; ++i) {
		if (name[i] == '\0' || BindingLowerAscii(word[i]) != BindingLowerAscii(name[i])) {
			return false;
		}
	}
	return name[i] == '\0';
}

// One INI value: up to kSourcesPerAction names, split by commas or spaces,
// any case. False for an unknown name, too many names, or one named twice -
// the value is then not used at all. "None" alone, or nothing, is no input.
inline bool ParseControlSources(const char* text, ControlSource out[kSourcesPerAction]) {
	for (UInt32 j = 0; j < kSourcesPerAction; ++j) {
		out[j] = ControlSource::None;
	}
	if (text == nullptr) {
		return true;
	}
	UInt32 count = 0;
	bool sawNone = false;
	const char* p = text;
	while (*p != '\0') {
		while (*p == ' ' || *p == ',' || *p == '\t') {
			++p;
		}
		if (*p == '\0') {
			break;
		}
		const char* start = p;
		while (*p != '\0' && *p != ' ' && *p != ',' && *p != '\t') {
			++p;
		}
		const UInt32 length = static_cast<UInt32>(p - start);
		ControlSource found = ControlSource::Count;
		for (UInt32 s = 0; s < kControlSourceCount; ++s) {
			if (BindingWordEquals(start, length, ControlSourceName(static_cast<ControlSource>(s)))) {
				found = static_cast<ControlSource>(s);
				break;
			}
		}
		if (found == ControlSource::Count) {
			return false;
		}
		if (found == ControlSource::None) {
			sawNone = true;
			continue;
		}
		for (UInt32 j = 0; j < count; ++j) {
			if (out[j] == found) {
				return false;
			}
		}
		if (count == kSourcesPerAction) {
			return false;
		}
		out[count++] = found;
	}
	// "None" beside a real input says two things at once.
	return !(sawNone && count > 0);
}

// What the laying-over did that the player did not write in so many words,
// said in the log.
enum class BindingNoteKind : UInt8 {
	Taken,    // two keys in the file named the same input: the earlier action keeps it
	Moved,    // a default's input went to the action the file gave it to
	KickOff,  // the file binds the kick, but [Hands] Kick=0
};

struct BindingNote {
	BindingNoteKind kind = BindingNoteKind::Taken;
	ControlAction action = ControlAction::Count;  // the action that lost the input
	ControlSource source = ControlSource::None;
	ControlAction owner = ControlAction::Count;  // the action that has it
};

constexpr UInt32 kMaxBindingNotes = kControlActionCount * kSourcesPerAction;

struct BindingNotes {
	BindingNote notes[kMaxBindingNotes];
	UInt32 count = 0;
	void Add(const BindingNote& n) {
		if (count < kMaxBindingNotes) {
			notes[count++] = n;
		}
	}
};

// The file over the defaults. Every input does one action at most: the
// file's keys come first, in the order of ControlAction (a later key that
// names an input an earlier one has loses it, Taken); then the defaults of the
// actions the file does not name, without the inputs the file gave elsewhere
// (Moved). With the kick off the file's Kick is not used (KickOff) and the
// defaults are the kick-less ones.
inline ControlBindings ResolveControlBindings(const ControlBindingFile& file, bool kick, BindingNotes* notes) {
	ControlBindings out;
	const ControlBindings defaults = DefaultControlBindings(kick);
	for (UInt32 i = 0; i < kControlActionCount; ++i) {
		const ControlAction action = static_cast<ControlAction>(i);
		if (!file.set[i]) {
			continue;
		}
		if (action == ControlAction::Kick && !kick) {
			if (notes != nullptr && file.sources[i][0] != ControlSource::None) {
				notes->Add(BindingNote{BindingNoteKind::KickOff, action, file.sources[i][0], ControlAction::Count});
			}
			continue;
		}
		for (UInt32 j = 0; j < kSourcesPerAction; ++j) {
			const ControlSource s = file.sources[i][j];
			if (s == ControlSource::None) {
				continue;
			}
			const ControlAction owner = ActionOfSource(out, s);
			if (owner != ControlAction::Count) {
				if (notes != nullptr) {
					notes->Add(BindingNote{BindingNoteKind::Taken, action, s, owner});
				}
				continue;
			}
			BindSource(out, action, s);
		}
	}
	for (UInt32 i = 0; i < kControlActionCount; ++i) {
		const ControlAction action = static_cast<ControlAction>(i);
		const bool fromFile = file.set[i] && !(action == ControlAction::Kick && !kick);
		if (fromFile) {
			continue;
		}
		for (UInt32 j = 0; j < kSourcesPerAction; ++j) {
			const ControlSource s = defaults.sources[i][j];
			if (s == ControlSource::None) {
				continue;
			}
			const ControlAction owner = ActionOfSource(out, s);
			if (owner != ControlAction::Count) {
				if (notes != nullptr) {
					notes->Add(BindingNote{BindingNoteKind::Moved, action, s, owner});
				}
				continue;
			}
			BindSource(out, action, s);
		}
	}
	return out;
}

// --------------------------------------------------------- One frame
//
// What every input did this frame: `fired` the frame it acts (a press, a
// tap's release, a hold reached, a flick), `down` while it is held (the
// buttons without a hold, the left stick's button).
struct ControlSourceFrame {
	bool fired[kControlSourceCount] = {};
	bool down[kControlSourceCount] = {};

	void Fire(ControlSource s, bool on) { fired[static_cast<UInt32>(s)] = fired[static_cast<UInt32>(s)] || on; }
	void Hold(ControlSource s, bool on) { down[static_cast<UInt32>(s)] = down[static_cast<UInt32>(s)] || on; }
};

struct BoundActions {
	bool fired[kControlActionCount] = {};
	bool down[kControlActionCount] = {};

	bool Fired(ControlAction a) const { return fired[static_cast<UInt32>(a)]; }
	bool Down(ControlAction a) const { return down[static_cast<UInt32>(a)]; }
	// For the held actions (activate, run): held, or fired this frame - a
	// tap's release or a hold reached is one frame of it.
	bool Active(ControlAction a) const { return Fired(a) || Down(a); }
};

// The actions of a frame's inputs. An input of a hand that is not tracked
// does nothing.
inline BoundActions FireControlBindings(const ControlBindings& b, const ControlSourceFrame& f, bool rightValid,
                                        bool leftValid) {
	BoundActions out;
	for (UInt32 i = 0; i < kControlActionCount; ++i) {
		for (UInt32 j = 0; j < kSourcesPerAction; ++j) {
			const ControlSource s = b.sources[i][j];
			if (s == ControlSource::None) {
				continue;
			}
			if (!(SourceOnRightHand(s) ? rightValid : leftValid)) {
				continue;
			}
			out.fired[i] = out.fired[i] || f.fired[static_cast<UInt32>(s)];
			out.down[i] = out.down[i] || f.down[static_cast<UInt32>(s)];
		}
	}
	return out;
}

// The input that fired an action this frame, the first bound one; None when
// none did.
inline ControlSource FiredSourceOf(const ControlBindings& b, const ControlSourceFrame& f, ControlAction a) {
	for (UInt32 j = 0; j < kSourcesPerAction; ++j) {
		const ControlSource s = b.sources[static_cast<UInt32>(a)][j];
		if (s != ControlSource::None && f.fired[static_cast<UInt32>(s)]) {
			return s;
		}
	}
	return ControlSource::None;
}

// An input in words, for the log: "right stick flicked up".
inline const char* ControlSourcePhrase(ControlSource s) {
	switch (s) {
	case ControlSource::RightA: return "right A";
	case ControlSource::RightAHold: return "right A held";
	case ControlSource::RightB: return "right B";
	case ControlSource::RightBHold: return "right B held";
	case ControlSource::LeftA: return "left A";
	case ControlSource::LeftAHold: return "left A held";
	case ControlSource::LeftB: return "left B";
	case ControlSource::LeftBHold: return "left B held";
	case ControlSource::RightStickUp: return "right stick flicked up";
	case ControlSource::RightStickDown: return "right stick flicked down";
	case ControlSource::RightStickClick: return "right stick's button held";
	case ControlSource::LeftStickClick: return "left stick's button";
	case ControlSource::LeftTrackpad: return "left trackpad";
	case ControlSource::LeftABHold: return "left A and B held together";
	default: return "none";
	}
}

// The input that fired each action this frame - an input of a hand that is
// not tracked does not count - and None for the actions that did not fire;
// the jump's in words too ("none" without one).
inline void NoteBoundFires(const ControlBindings& b, const ControlSourceFrame& f, bool rightValid, bool leftValid,
                           ControlSource out[kControlActionCount], const char*& jumpBy) {
	for (UInt32 i = 0; i < kControlActionCount; ++i) {
		out[i] = ControlSource::None;
		for (UInt32 j = 0; j < kSourcesPerAction && out[i] == ControlSource::None; ++j) {
			const ControlSource s = b.sources[i][j];
			if (s != ControlSource::None && f.fired[static_cast<UInt32>(s)] &&
			    (SourceOnRightHand(s) ? rightValid : leftValid)) {
				out[i] = s;
			}
		}
	}
	jumpBy = ControlSourcePhrase(out[static_cast<UInt32>(ControlAction::Jump)]);
}

// The log of the bound actions ("Buttons: Jump from the right A"): a line the
// frame an action fires, the first kBoundFireLines of each action - what the
// controls matrix (tools/gen-controls-matrix.py) reads to see every action
// on the input OBVR_Controls.ini gave it, and on no other.
constexpr UInt32 kBoundFireLines = 20;

struct BoundFireLog {
	bool was[kControlActionCount] = {};
	UInt32 left[kControlActionCount] = {};
	bool started = false;
};

// Whether this frame's firing of an action gets its line.
inline bool StepBoundFireLine(BoundFireLog& log, ControlAction a, ControlSource firedBy) {
	if (!log.started) {
		for (UInt32 i = 0; i < kControlActionCount; ++i) {
			log.left[i] = kBoundFireLines;
		}
		log.started = true;
	}
	const UInt32 i = static_cast<UInt32>(a);
	if (i >= kControlActionCount) {
		return false;
	}
	const bool now = firedBy != ControlSource::None;
	const bool line = now && !log.was[i] && log.left[i] > 0;
	if (line) {
		--log.left[i];
	}
	log.was[i] = now;
	return line;
}

// One A or B button's frame onto its two inputs. With a hold bound the press
// waits: let go before the ring its tap fires, held long enough its hold
// fires, and neither is "down" meanwhile. Without one the press fires at once
// and the button is down while held.
struct ButtonStep {
	bool down = false;      // the button down now
	bool pressed = false;   // its rising edge
	bool tap = false;       // with a hold: the short press's release
	bool reached = false;   // with a hold: the frame the hold is reached
};

inline void FeedButton(ControlSourceFrame& f, HoldButton button, bool hasHold, const ButtonStep& step) {
	const ControlSource tap = TapSourceOf(button);
	const ControlSource hold = HoldSourceOf(button);
	if (hasHold) {
		f.Fire(tap, step.tap);
		f.Fire(hold, step.reached);
	} else {
		f.Fire(tap, step.pressed);
		f.Hold(tap, step.down);
	}
}

}  // namespace obvr::vr
