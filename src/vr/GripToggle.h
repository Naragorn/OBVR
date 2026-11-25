#pragma once

// The grip toggle ([Hands] GripToggle, default off; Nadi, 2026-11-25: "man
// muss die hand nicht dauerhaft mehr an der klinge halten um diese zu
// halten, einmal grip reicht zum nehmen und waffe bleibt gehalten, wenn
// gehalten lässt grip die waffe wieder los"). Without it a hand holds a
// weapon only while its grip is closed (vr/GripHold.h, Blade & Sorcery's
// hold - free-weapons-spec 5.1, decision 3, which kept the toggle as an
// option). With it, a grip that took what a hand keeps leaves that hand's
// hold closed after the grip opens, and the next grip lets go: the hold reads
// open at once, and stays open until that grip opens.
//
// It changes the hold, not what reads it: every reader of the hand's hold -
// the weapon's and the torch's let go (game/WeaponRelease.h,
// game/TorchRelease.h), the holsters, the take at a grip, the second hand on
// the handle (vr/TwoHandLogic.h), the hand-over - sees a hand that stays
// closed while it keeps something and opens with the next grip. So the let go
// does what an opening does today, by where the hand is: a holster sheathes,
// the chest stows, anywhere else drops, a swung hand throws.
//
// What a hand keeps by its hold (GripKeepsOf): the weapon hand a drawn
// weapon (not a bow - that is the other hand's, the weapon hand draws its
// string by its own grip); the other hand a drawn bow, a torch it holds, a
// weapon's handle or blade. Loose things the grab carries (potions, a body)
// and the pull to the hand stay on the grip as before - they are the engine's
// grab, on the grip's click.
//
// A grip that takes something may show it a few frames later (the take at a
// grip, the draw from a place): the hold stays closed kTapSeconds after a
// grip, so a short grip is not read as an opening on the way. What a hand
// kept, gone without a grip for kLossSeconds (sheathed by a key, taken off in
// a menu), opens the hold again. Menus pause it; a load ends it.
//
// Pure, covered by grip_toggle_test; the hand mode steps it after the hold
// (vr/HandMode.cpp), its state rides in each hand's GripHoldState so it goes
// with the hand when the sides switch (HandMode::StepRolesSwitch).

#include "core/Types.h"
#include "vr/HandInput.h"
#include "vr/Holster.h"

namespace obvr::vr {

// How long a grip's hold stays closed for what it takes to show: the take
// at a grip shows the weapon drawn within three frames since 2026-11-18, the
// draw from a place within about five (weapon-drop 2026-11-18: the reach, the
// ready click and "the hold closed on the drawn weapon" in five log lines).
inline constexpr float kGripToggleTapSeconds = 0.5f;
// What a hand kept read gone this long without a grip: the toggle lets go.
// Longer than a reading's gap - the sides' switch at a hand-over takes a frame.
inline constexpr float kGripToggleLossSeconds = 0.2f;

struct GripToggleState {
	bool latched = false;   // closed by the toggle: what the hand took stays until the next grip
	bool swallow = false;   // the grip that let go: the hold open until that grip opens
	bool pressWas = false;  // the grip's own hold last frame
	bool gap = false;       // untracked since: the next tracked frame is no grip
	float pending = 0.0f;   // a grip's time left to show what it took
	float lost = 0.0f;      // latched with nothing kept, this long
};

struct GripToggleInput {
	bool hold = false;    // the grip's own hold (StepGripHold)
	bool keeps = false;   // the hand has what a hold keeps (GripKeepsOf)
	bool valid = false;   // the hand is tracked
	bool world = false;   // a game is loaded; out of it (the main menu, a load) every toggle ends
	bool paused = false;  // a menu up: the toggle as it is, no grip counts, nothing is lost
	float dt = 0.0f;
};

enum class GripToggleEvent : UInt8 {
	None,
	Kept,   // a grip took what the hand keeps: held until the next grip
	LetGo,  // the next grip: the hold open
	Lost,   // what the hand kept went without a grip: the hold the grip's again
};

struct GripToggleOut {
	bool hold = false;        // the hand's hold as everything reads it
	bool takesClick = false;  // the grip's click is the toggle's: it grabs nothing
	GripToggleEvent event = GripToggleEvent::None;
};

inline GripToggleOut StepGripToggle(GripToggleState& t, const GripToggleInput& in, bool on) {
	GripToggleOut o;
	if (!on || !in.world) {
		t = GripToggleState{};
		t.pressWas = in.valid && in.hold;
		o.hold = in.hold;
		return o;
	}
	if (!in.valid) {
		t.gap = true;
		o.hold = t.latched;
		return o;
	}
	const bool press = in.hold && !t.pressWas && !t.gap && !in.paused;
	t.pressWas = in.hold;
	t.gap = false;
	if (in.paused) {
		o.hold = t.latched || in.hold;
		return o;
	}
	const float dt = in.dt == in.dt && in.dt > 0.0f ? in.dt : 0.0f;
	if (t.latched) {
		if (press) {
			t = GripToggleState{};
			t.pressWas = true;
			t.swallow = true;
			o.takesClick = true;
			o.event = GripToggleEvent::LetGo;
			return o;
		}
		t.lost = in.keeps ? 0.0f : t.lost + dt;
		if (t.lost < kGripToggleLossSeconds) {
			o.hold = true;
			o.takesClick = true;
			return o;
		}
		t.latched = false;
		t.lost = 0.0f;
		o.hold = in.hold;
		o.event = GripToggleEvent::Lost;
		return o;
	}
	if (t.swallow) {
		if (in.hold) {
			o.takesClick = true;
			return o;
		}
		t.swallow = false;
	}
	if (press) {
		t.pending = kGripToggleTapSeconds;
	} else if (t.pending > 0.0f) {
		t.pending -= dt;
	}
	const bool closed = in.hold || t.pending > 0.0f;
	if (closed && in.keeps) {
		t.latched = true;
		t.pending = 0.0f;
		t.lost = 0.0f;
		o.hold = true;
		o.takesClick = true;
		o.event = GripToggleEvent::Kept;
		return o;
	}
	o.hold = closed;
	return o;
}

struct GripKeeps {
	bool right = false;  // the weapon hand
	bool left = false;   // the other hand
};

// What each hand keeps by its hold, from what the frame reads: the weapon
// hand a drawn weapon of its own (a one- or two-hander, a staff); the other
// hand a drawn bow, a torch - unless a bow or a two-hander drawn took it off
// the hand (the game's rule; TorchWithTwoHander keeps it beside a two-hander)
// - or a weapon's handle or blade. A shield stays on the arm whatever the
// grip does: nothing to keep.
inline GripKeeps GripKeepsOf(EquippedKind equipped, WeaponSeen seen, OffHandItem offHand, bool torchBesideTwoHander,
                             bool leftOnWeapon) {
	GripKeeps k;
	const bool drawn = seen == WeaponSeen::Drawn;
	const bool bow = drawn && equipped == EquippedKind::Bow;
	k.right = drawn && (equipped == EquippedKind::OneHand || equipped == EquippedKind::TwoHand);
	const bool torchOffTheHand = bow || (drawn && equipped == EquippedKind::TwoHand && !torchBesideTwoHander);
	k.left = bow || (offHand == OffHandItem::Torch && !torchOffTheHand) || leftOnWeapon;
	return k;
}

// Both hands' readings for a frame.
struct GripToggleFrame {
	bool rightValid = false;
	bool leftValid = false;
	bool world = false;
	bool paused = false;
	float dt = 0.0f;
	GripKeeps keeps;
};

// The hand mode's frame (vr::HandModeFrame) read for the toggle - a template,
// so this header needs nothing of the hand mode's.
template <class Frame>
GripToggleFrame GripToggleFrameOf(const Frame& f, bool rightValid, bool leftValid) {
	GripToggleFrame g;
	g.rightValid = rightValid;
	g.leftValid = leftValid;
	g.world = f.inWorld;
	g.paused = f.menuMode || f.settingsMenuOpen;
	g.dt = f.dtSeconds;
	g.keeps = GripKeepsOf(f.equipped, f.weaponSeen, f.offHand, f.torchBesideTwoHander,
	                      f.leftGripOnHandle || f.bothHandsOnHandle);
	return g;
}

struct GripToggleSeen {
	GripToggleEvent right = GripToggleEvent::None;
	GripToggleEvent left = GripToggleEvent::None;
};

// Both hands: their holds as everything reads them, their clicks taken while
// the toggle has them; `on` is the setting. Off, nothing changes.
inline GripToggleSeen StepGripToggles(GripToggleState& right, GripToggleState& left, bool& rightHold, bool& leftHold,
                                      bool& rightClick, bool& leftClick, const GripToggleFrame& f, bool on) {
	GripToggleSeen seen;
	GripToggleInput in;
	in.world = f.world;
	in.paused = f.paused;
	in.dt = f.dt;
	in.hold = rightHold;
	in.keeps = f.keeps.right;
	in.valid = f.rightValid;
	const GripToggleOut r = StepGripToggle(right, in, on);
	in.hold = leftHold;
	in.keeps = f.keeps.left;
	in.valid = f.leftValid;
	const GripToggleOut l = StepGripToggle(left, in, on);
	rightHold = r.hold;
	leftHold = l.hold;
	rightClick = rightClick && !r.takesClick;
	leftClick = leftClick && !l.takesClick;
	seen.right = r.event;
	seen.left = l.event;
	return seen;
}

// The log's words for an event, null for none.
inline const char* GripToggleWords(GripToggleEvent e) {
	switch (e) {
	case GripToggleEvent::Kept:
		return "took hold - kept closed until the next grip";
	case GripToggleEvent::LetGo:
		return "let go at the next grip";
	case GripToggleEvent::Lost:
		return "lost what it kept without a grip - the hold is the grip's again";
	case GripToggleEvent::None:
		break;
	}
	return nullptr;
}

}  // namespace obvr::vr
