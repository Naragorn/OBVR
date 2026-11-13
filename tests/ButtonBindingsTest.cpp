// Checks the button bindings of Full VR (vr/ButtonBindings.h): the names, the
// parsing of an OBVR_Controls.ini value with every refusal, the defaults with
// and without the kick, the file laid over the defaults with every note, and
// one frame's inputs onto the actions.

#include <cstdio>
#include <cstring>

#include "vr/ButtonBindings.h"

using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

ControlSource S0(const ControlBindings& b, ControlAction a) { return b.sources[static_cast<UInt32>(a)][0]; }
ControlSource S1(const ControlBindings& b, ControlAction a) { return b.sources[static_cast<UInt32>(a)][1]; }

void SetFile(ControlBindingFile& f, ControlAction a, const char* text) {
	f.set[static_cast<UInt32>(a)] = ParseControlSources(text, f.sources[static_cast<UInt32>(a)]);
}

void TestNames() {
	std::printf("Names\n");
	bool unique = true;
	for (UInt32 i = 0; i < kControlSourceCount; ++i) {
		for (UInt32 j = i + 1; j < kControlSourceCount; ++j) {
			unique = unique && std::strcmp(ControlSourceName(static_cast<ControlSource>(i)),
			                               ControlSourceName(static_cast<ControlSource>(j))) != 0;
		}
		unique = unique && std::strcmp(ControlSourceName(static_cast<ControlSource>(i)), "?") != 0;
	}
	Check(unique, "every input has a name of its own");
	unique = true;
	for (UInt32 i = 0; i < kControlActionCount; ++i) {
		for (UInt32 j = i + 1; j < kControlActionCount; ++j) {
			unique = unique && std::strcmp(ControlActionName(static_cast<ControlAction>(i)),
			                               ControlActionName(static_cast<ControlAction>(j))) != 0;
		}
		unique = unique && std::strcmp(ControlActionName(static_cast<ControlAction>(i)), "?") != 0 &&
		         std::strcmp(ControlActionPhrase(static_cast<ControlAction>(i)), "?") != 0;
	}
	Check(unique, "every action has a key and a phrase of its own");
	bool buttons = true;
	for (UInt32 i = 0; i < kHoldButtonCount; ++i) {
		const HoldButton b = static_cast<HoldButton>(i);
		buttons = buttons && HoldButtonOf(TapSourceOf(b)) == b && HoldButtonOf(HoldSourceOf(b)) == b &&
		          !IsHoldSource(TapSourceOf(b)) && IsHoldSource(HoldSourceOf(b)) &&
		          std::strcmp(HoldButtonPhrase(b), "?") != 0;
	}
	Check(buttons, "each A and B has its tap and its hold");
	Check(HoldButtonOf(ControlSource::RightStickUp) == HoldButton::Count, "a stick is no A or B");
	Check(SourceOnRightHand(ControlSource::RightBHold) && SourceOnRightHand(ControlSource::RightStickClick) &&
	          !SourceOnRightHand(ControlSource::LeftA) && !SourceOnRightHand(ControlSource::LeftTrackpad) &&
	          !SourceOnRightHand(ControlSource::LeftStickClick),
	      "which hand an input is on");
}

void TestParse() {
	std::printf("Parsing a value\n");
	ControlSource s[kSourcesPerAction];
	Check(ParseControlSources("RightB", s) && s[0] == ControlSource::RightB && s[1] == ControlSource::None,
	      "one name");
	Check(ParseControlSources("  rightb , RIGHTSTICKUP", s) && s[0] == ControlSource::RightB &&
	          s[1] == ControlSource::RightStickUp,
	      "two, any case, commas and spaces");
	Check(ParseControlSources("LeftA LeftBHold LeftTrackpad", s) && s[2] == ControlSource::LeftTrackpad,
	      "three, by spaces");
	Check(!ParseControlSources("LeftA LeftB LeftTrackpad RightA", s), "four: refused");
	Check(!ParseControlSources("RightC", s), "an unknown name: refused");
	Check(!ParseControlSources("RightB, RightB", s), "one named twice: refused");
	Check(!ParseControlSources("RightBX", s), "a longer word than a name: refused");
	Check(!ParseControlSources("Right", s), "a shorter word than a name: refused");
	Check(ParseControlSources("None", s) && s[0] == ControlSource::None, "None: no input");
	Check(ParseControlSources("", s) && s[0] == ControlSource::None, "empty: no input");
	Check(ParseControlSources(nullptr, s) && s[0] == ControlSource::None, "no text: no input");
	Check(!ParseControlSources("None, RightA", s), "None beside an input: refused");
	Check(ParseControlSources("None none", s) && s[0] == ControlSource::None, "None twice: still none");
}

void TestDefaults() {
	std::printf("The defaults\n");
	const ControlBindings k = DefaultControlBindings(true);
	Check(S0(k, ControlAction::Jump) == ControlSource::RightB && S1(k, ControlAction::Jump) == ControlSource::None,
	      "jump: the right B");
	Check(S0(k, ControlAction::Kick) == ControlSource::RightStickUp, "with the kick: the right stick flicked up kicks");
	Check(S0(k, ControlAction::PauseMenu) == ControlSource::LeftBHold, "the pause menu: the left B held");
	Check(S0(k, ControlAction::Menus) == ControlSource::LeftB, "Tab: the left B");
	Check(S0(k, ControlAction::Activate) == ControlSource::RightA, "activate: the right A");
	Check(S0(k, ControlAction::Sneak) == ControlSource::RightStickDown, "sneak: flicked down");
	Check(S0(k, ControlAction::ReadyWeapon) == ControlSource::RightStickClick, "ready: the right stick's hold");
	Check(S0(k, ControlAction::Run) == ControlSource::LeftStickClick, "run: the left stick in");
	Check(S0(k, ControlAction::Journal) == ControlSource::LeftTrackpad, "journal: the left trackpad");
	Check(S0(k, ControlAction::TogglePov) == ControlSource::None, "the view switch: unbound");
	Check(S0(k, ControlAction::QuickSave) == ControlSource::LeftABHold &&
	          S0(DefaultControlBindings(false), ControlAction::QuickSave) == ControlSource::LeftABHold,
	      "quick save: the left A and B held together, with the kick or without");
	Check(HoldButtonOf(ControlSource::LeftABHold) == HoldButton::Count && !IsHoldSource(ControlSource::LeftABHold) &&
	          !SourceOnRightHand(ControlSource::LeftABHold),
	      "the chord is no single button's hold, and on the other hand");
	ControlSource parsed[kSourcesPerAction];
	Check(ParseControlSources("leftabhold", parsed) && parsed[0] == ControlSource::LeftABHold,
	      "QuickSave=LeftABHold parses, any case");
	ControlBindingFile off;
	SetFile(off, ControlAction::QuickSave, "None");
	Check(S0(ResolveControlBindings(off, true, nullptr), ControlAction::QuickSave) == ControlSource::None,
	      "QuickSave=None unbinds it");
	ControlSourceFrame chord;
	chord.Fire(ControlSource::LeftABHold, true);
	Check(FireControlBindings(k, chord, true, true).Fired(ControlAction::QuickSave) &&
	          !FireControlBindings(k, chord, true, false).Fired(ControlAction::QuickSave),
	      "the chord reached fires the quick save; the other hand lost: nothing");
	Check(ButtonHasHold(k, HoldButton::LeftB) && !ButtonHasHold(k, HoldButton::RightB) &&
	          !ButtonHasHold(k, HoldButton::RightA) && !ButtonHasHold(k, HoldButton::LeftA),
	      "only the left B has a hold");
	const ControlBindings n = DefaultControlBindings(false);
	Check(S0(n, ControlAction::Kick) == ControlSource::None && S0(n, ControlAction::Jump) == ControlSource::RightB &&
	          S1(n, ControlAction::Jump) == ControlSource::RightStickUp,
	      "without the kick: the right B jumps, and the free flick too");
	Check(S0(n, ControlAction::PauseMenu) == ControlSource::LeftBHold && ButtonHasHold(n, HoldButton::LeftB),
	      "and the pause menu stays on the left B held");
	Check(ControlBindings{} != k && DefaultControlBindings(true) == k, "equality");
}

void TestResolve() {
	std::printf("The file over the defaults\n");
	ControlBindingFile empty;
	BindingNotes notes;
	Check(ResolveControlBindings(empty, true, &notes) == DefaultControlBindings(true) && notes.count == 0,
	      "no file: the defaults with the kick, nothing said");
	Check(ResolveControlBindings(empty, false, &notes) == DefaultControlBindings(false) && notes.count == 0,
	      "and without it");

	// The layout of before 2026-11-13 by file: the kick on the B the jump has.
	ControlBindingFile kickB;
	SetFile(kickB, ControlAction::Kick, "RightB");
	notes = BindingNotes{};
	ControlBindings b = ResolveControlBindings(kickB, true, &notes);
	Check(S0(b, ControlAction::Kick) == ControlSource::RightB && S1(b, ControlAction::Kick) == ControlSource::None,
	      "Kick=RightB: the B kicks, the flick no longer");
	Check(S0(b, ControlAction::Jump) == ControlSource::None, "the jump's default B went to the kick");
	Check(notes.count == 1 && notes.notes[0].kind == BindingNoteKind::Moved &&
	          notes.notes[0].action == ControlAction::Jump && notes.notes[0].source == ControlSource::RightB &&
	          notes.notes[0].owner == ControlAction::Kick,
	      "and that is said");
	Check(S0(b, ControlAction::PauseMenu) == ControlSource::LeftBHold, "the rest stays");

	ControlBindingFile jumpFlick = kickB;
	SetFile(jumpFlick, ControlAction::Jump, "RightStickUp");
	b = ResolveControlBindings(jumpFlick, true, nullptr);
	Check(S0(b, ControlAction::Kick) == ControlSource::RightB && S0(b, ControlAction::Jump) == ControlSource::RightStickUp,
	      "Jump=RightStickUp beside it: the old layout whole");

	ControlBindingFile clash;
	SetFile(clash, ControlAction::Jump, "RightA");
	SetFile(clash, ControlAction::Sneak, "RightA, LeftA");
	notes = BindingNotes{};
	b = ResolveControlBindings(clash, true, &notes);
	Check(S0(b, ControlAction::Jump) == ControlSource::RightA, "two keys on one input: the earlier action keeps it");
	Check(S0(b, ControlAction::Sneak) == ControlSource::LeftA && S1(b, ControlAction::Sneak) == ControlSource::None,
	      "the later keeps its other input");
	Check(S0(b, ControlAction::Activate) == ControlSource::None, "activate's default A is taken");
	bool taken = false;
	bool moved = false;
	for (UInt32 i = 0; i < notes.count; ++i) {
		taken = taken || (notes.notes[i].kind == BindingNoteKind::Taken &&
		                  notes.notes[i].action == ControlAction::Sneak && notes.notes[i].owner == ControlAction::Jump);
		moved = moved || (notes.notes[i].kind == BindingNoteKind::Moved &&
		                  notes.notes[i].action == ControlAction::Activate);
	}
	Check(taken && moved, "both said");

	ControlBindingFile unbind;
	SetFile(unbind, ControlAction::Jump, "None");
	b = ResolveControlBindings(unbind, true, nullptr);
	Check(S0(b, ControlAction::Jump) == ControlSource::None, "Jump=None: no jump at all");
	Check(ActionOfSource(b, ControlSource::RightB) == ControlAction::Count, "the B is free");

	ControlBindingFile kickOff;
	SetFile(kickOff, ControlAction::Kick, "LeftA");
	notes = BindingNotes{};
	b = ResolveControlBindings(kickOff, false, &notes);
	Check(S0(b, ControlAction::Kick) == ControlSource::None && b == DefaultControlBindings(false),
	      "[Hands] Kick=0: the file's kick unused, the kick-less defaults");
	Check(notes.count == 1 && notes.notes[0].kind == BindingNoteKind::KickOff, "and said");
	ControlBindingFile kickOffNone;
	SetFile(kickOffNone, ControlAction::Kick, "None");
	notes = BindingNotes{};
	ResolveControlBindings(kickOffNone, false, &notes);
	Check(notes.count == 0, "Kick=None with the kick off: nothing to say");

	ControlBindingFile pauseHold;
	SetFile(pauseHold, ControlAction::PauseMenu, "RightBHold");
	b = ResolveControlBindings(pauseHold, true, nullptr);
	Check(ButtonHasHold(b, HoldButton::RightB) && ButtonHasHold(b, HoldButton::LeftB) == false,
	      "the pause menu on the right B held: that B has the hold, the left none");
	Check(S0(b, ControlAction::Menus) == ControlSource::LeftB, "the left B still Tab");

	ControlBindingFile bad;
	bad.set[static_cast<UInt32>(ControlAction::Jump)] = ParseControlSources("Nonsense", bad.sources[0]);
	Check(ResolveControlBindings(bad, true, nullptr) == DefaultControlBindings(true),
	      "a value that does not parse: the default kept");

	BindingNotes full;
	for (UInt32 i = 0; i < kMaxBindingNotes + 3; ++i) {
		full.Add(BindingNote{});
	}
	Check(full.count == kMaxBindingNotes, "the notes stop at their room");
}

void TestFire() {
	std::printf("One frame\n");
	const ControlBindings k = DefaultControlBindings(true);
	ControlSourceFrame f;
	f.Fire(ControlSource::RightStickUp, true);
	f.Hold(ControlSource::RightStickUp, true);
	BoundActions a = FireControlBindings(k, f, true, true);
	Check(a.Fired(ControlAction::Kick) && !a.Fired(ControlAction::Jump) && !a.Fired(ControlAction::PauseMenu),
	      "the flick: the kick");
	a = FireControlBindings(k, f, false, true);
	Check(!a.Fired(ControlAction::Kick) && !a.Down(ControlAction::Kick), "the weapon hand lost: nothing");
	ControlSourceFrame bPress;
	bPress.Fire(ControlSource::RightB, true);
	a = FireControlBindings(k, bPress, true, true);
	Check(a.Fired(ControlAction::Jump) && !a.Fired(ControlAction::Kick), "the right B: the jump");

	ControlSourceFrame run;
	run.Hold(ControlSource::LeftStickClick, true);
	a = FireControlBindings(k, run, true, true);
	Check(a.Active(ControlAction::Run) && !a.Fired(ControlAction::Run), "the left stick held: run, no click");
	a = FireControlBindings(k, run, true, false);
	Check(!a.Active(ControlAction::Run), "the other hand lost: no run");

	ControlBindingFile two;
	SetFile(two, ControlAction::Jump, "RightB, RightStickUp");
	SetFile(two, ControlAction::Kick, "None");
	const ControlBindings j = ResolveControlBindings(two, true, nullptr);
	ControlSourceFrame flick;
	flick.Fire(ControlSource::RightStickUp, true);
	Check(FireControlBindings(j, flick, true, true).Fired(ControlAction::Jump), "two inputs: either fires");
	Check(!FireControlBindings(j, ControlSourceFrame{}, true, true).Active(ControlAction::Jump), "neither: nothing");
	Check(FiredSourceOf(j, flick, ControlAction::Jump) == ControlSource::RightStickUp,
	      "which input fired: the flick");
	Check(FiredSourceOf(j, ControlSourceFrame{}, ControlAction::Jump) == ControlSource::None, "none fired: None");
	flick.Fire(ControlSource::RightB, true);
	Check(FiredSourceOf(j, flick, ControlAction::Jump) == ControlSource::RightB, "both: the first bound");
	bool phrases = std::strcmp(ControlSourcePhrase(ControlSource::RightStickUp), "right stick flicked up") == 0 &&
	               std::strcmp(ControlSourcePhrase(ControlSource::None), "none") == 0;
	for (UInt32 s = 1; s < kControlSourceCount; ++s) {
		phrases = phrases && std::strcmp(ControlSourcePhrase(static_cast<ControlSource>(s)), "none") != 0;
	}
	Check(phrases, "every input said in words, the flick as the log always said it");

	std::printf("A button onto its inputs\n");
	ButtonStep press;
	press.down = true;
	press.pressed = true;
	ControlSourceFrame plain;
	FeedButton(plain, HoldButton::RightA, false, press);
	Check(plain.fired[static_cast<UInt32>(ControlSource::RightA)] && plain.down[static_cast<UInt32>(ControlSource::RightA)],
	      "no hold: the press fires and the button is down");
	Check(!plain.fired[static_cast<UInt32>(ControlSource::RightAHold)], "and its hold never");
	ControlSourceFrame waiting;
	FeedButton(waiting, HoldButton::LeftB, true, press);
	Check(!waiting.fired[static_cast<UInt32>(ControlSource::LeftB)] &&
	          !waiting.down[static_cast<UInt32>(ControlSource::LeftB)],
	      "with a hold: the press waits");
	ButtonStep tap;
	tap.tap = true;
	ControlSourceFrame tapped;
	FeedButton(tapped, HoldButton::LeftB, true, tap);
	Check(tapped.fired[static_cast<UInt32>(ControlSource::LeftB)] &&
	          !tapped.fired[static_cast<UInt32>(ControlSource::LeftBHold)],
	      "let go short: the tap");
	ButtonStep reached;
	reached.down = true;
	reached.reached = true;
	ControlSourceFrame held;
	FeedButton(held, HoldButton::LeftB, true, reached);
	Check(held.fired[static_cast<UInt32>(ControlSource::LeftBHold)] &&
	          !held.fired[static_cast<UInt32>(ControlSource::LeftB)],
	      "held long enough: the hold");
}

// The input each action fired from, for the log: swapped inputs, an untracked
// hand, two inputs of one action, nothing fired.
void TestNoteBoundFires() {
	ControlBindingFile file;
	SetFile(file, ControlAction::Jump, "RightA");
	SetFile(file, ControlAction::Activate, "RightB");
	SetFile(file, ControlAction::Sneak, "LeftA, RightStickUp");
	const ControlBindings b = ResolveControlBindings(file, true, nullptr);
	ControlSourceFrame f;
	f.Fire(ControlSource::RightA, true);
	f.Fire(ControlSource::RightB, true);
	f.Fire(ControlSource::LeftA, true);
	f.Fire(ControlSource::RightStickUp, true);
	ControlSource by[kControlActionCount];
	const char* jumpBy = "?";
	NoteBoundFires(b, f, true, true, by, jumpBy);
	Check(by[static_cast<UInt32>(ControlAction::Jump)] == ControlSource::RightA, "the jump from the right A");
	Check(std::strcmp(jumpBy, "right A") == 0, "the jump in words");
	Check(by[static_cast<UInt32>(ControlAction::Activate)] == ControlSource::RightB, "use from the right B");
	Check(by[static_cast<UInt32>(ControlAction::Sneak)] == ControlSource::LeftA, "two inputs fired: the first bound one");
	Check(by[static_cast<UInt32>(ControlAction::Kick)] == ControlSource::None, "the kick lost the flick: not fired");
	NoteBoundFires(b, f, true, false, by, jumpBy);
	Check(by[static_cast<UInt32>(ControlAction::Sneak)] == ControlSource::RightStickUp,
	      "the left hand untracked: its input does not count, the right one does");
	NoteBoundFires(b, f, false, false, by, jumpBy);
	Check(by[static_cast<UInt32>(ControlAction::Jump)] == ControlSource::None && std::strcmp(jumpBy, "none") == 0,
	      "no hand tracked: nothing");
	ControlSourceFrame none;
	NoteBoundFires(b, none, true, true, by, jumpBy);
	bool any = false;
	for (UInt32 i = 0; i < kControlActionCount; ++i) {
		any = any || by[i] != ControlSource::None;
	}
	Check(!any, "nothing fired: no input");
}

// A line the frame an action fires, not while it keeps firing, again on the
// next press, and no more than kBoundFireLines of each.
void TestBoundFireLines() {
	BoundFireLog log;
	Check(StepBoundFireLine(log, ControlAction::Jump, ControlSource::RightB), "the first fire: a line");
	Check(!StepBoundFireLine(log, ControlAction::Jump, ControlSource::RightB), "still firing: no second line");
	Check(!StepBoundFireLine(log, ControlAction::Jump, ControlSource::None), "not fired: no line");
	Check(StepBoundFireLine(log, ControlAction::Jump, ControlSource::RightB), "the next press: a line again");
	Check(StepBoundFireLine(log, ControlAction::Kick, ControlSource::RightStickUp), "each action has its own lines");
	UInt32 lines = 2;
	for (UInt32 n = 0; n < 100; ++n) {
		StepBoundFireLine(log, ControlAction::Jump, ControlSource::None);
		lines += StepBoundFireLine(log, ControlAction::Jump, ControlSource::RightB) ? 1 : 0;
	}
	Check(lines == kBoundFireLines, "no more than kBoundFireLines of one action");
	Check(!StepBoundFireLine(log, ControlAction::Count, ControlSource::RightB), "no action: no line");
}

}  // namespace

int main() {
	TestNames();
	TestParse();
	TestDefaults();
	TestResolve();
	TestFire();
	TestNoteBoundFires();
	TestBoundFireLines();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("All button binding checks passed\n");
	return 0;
}
