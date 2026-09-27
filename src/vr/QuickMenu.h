#pragma once

// The quick menu on the right trackpad (docs/controls-spec.md 4.4): the
// eight hotkeys on a ring that opens where the hand is, chosen by moving the
// hand towards one - the way Half-Life: Alyx's hand menu is used - and used
// by letting go of the trackpad. Full VR only.
//
// The hotkey is used the way the game's own number key uses it: the key of
// its slot is tapped (Oblivion.ini [Controls] Quick1..Quick8, 1-8 by
// default). A tap and not a hold: a number key held past a moment opens the
// game's own hotkey ring instead (0x5C1F70 calls 0x5C1B80 once a timed
// threshold at 0xB38BB0 has passed, read 2026-09-27).
//
// In the inventory and the magic menu the same ring sets a hotkey instead
// (docs/controls-spec.md 4.5): what the cursor is on goes onto the slot the
// hand lets go on, empty or not. It is done the way vanilla does it on the
// PC - the slot's number key held, the item clicked while it is held
// (the PC manual; UESP Oblivion:Controls "press and hold a Hotkey, then
// left click on an item"). Held, the key opens the QuickKeys menu at once
// in those two menus (0x5C1F70 calls 0x5C1B80, which sets 0xB3B43D), and
// the menus' click handlers assign instead of equipping while it is set
// (0x5ABC1C, 0x5B39BC). So: the key down for a moment, the click, the key
// kept a moment longer, each for at least one frame. The cursor stays where
// it was while the ring is up and the sequence runs, so the click lands on
// the item the laser was on when the trackpad went down.
//
// Pure, covered by quick_menu_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::vr {

constexpr int kQuickSlots = 8;

struct QuickMenuSettings {
	bool enabled = true;
	// How far the hand has to move from where the ring opened before a slot
	// is chosen. Closer is the middle: letting go there uses nothing.
	float deadZoneMetres = 0.04f;
	// How long the slot's number key stays down.
	float tapSeconds = 0.08f;
	// The ring's radius: how far from the middle the slots are drawn, and so
	// about how far the hand moves to one.
	float ringMetres = 0.10f;
	// Setting a hotkey in a menu: how long the number key is down before the
	// click, how long the click, how long the key stays down after it.
	float assignLeadSeconds = 0.20f;
	float assignClickSeconds = 0.08f;
	float assignTailSeconds = 0.15f;
};

// Where a hotkey being set stands.
enum class AssignPhase : UInt8 { Idle, KeyDown, Click, KeyHeld };

struct QuickMenuState {
	bool open = false;
	bool assigning = false;  // the ring was opened in a menu: a release sets, not uses
	NiPoint3 anchor{0.0f, 0.0f, 0.0f};  // the hand where the ring opened, tracking space
	NiPoint3 right{1.0f, 0.0f, 0.0f};   // the ring's right and up, tracking space
	NiPoint3 up{0.0f, 1.0f, 0.0f};
	int highlighted = -1;
	int tapSlot = -1;  // the slot whose key is down
	float tapLeft = 0.0f;
	bool padWas = false;
	AssignPhase assignPhase = AssignPhase::Idle;
	int assignSlot = -1;
	float assignClock = 0.0f;  // time in the current phase
};

struct QuickMenuInput {
	bool pad = false;      // the right trackpad clicked, as it is now
	// Full VR on, and either in the world with no menu up, or in the
	// inventory or the magic menu (then `assign`).
	bool allowed = false;
	bool assign = false;   // in the inventory or the magic menu: a release sets the hotkey
	NiPoint3 hand{0.0f, 0.0f, 0.0f};       // the right controller, tracking space
	NiPoint3 headRight{1.0f, 0.0f, 0.0f};  // the head's right, tracking space
	float dt = 0.0f;
	bool filled[kQuickSlots] = {};  // which slots hold something
};

struct QuickMenuVerdict {
	bool visible = false;
	bool opened = false;  // opened this frame: the ring is placed now
	int highlighted = -1;
	int key = 0;          // 1..8 while that slot's number key is to be down, else 0
	int used = -1;        // the slot used this frame
	bool cancelled = false;  // let go on nothing, or on an empty slot
	bool assigning = false;  // the ring shown is the one that sets
	int assigned = -1;    // the slot being set, on the frame the sequence starts
	bool click = false;   // the menu click (left mouse) to be down
	bool assignDone = false;     // the sequence ended this frame, key and click up
	bool assignAborted = false;  // the menu went before the sequence ended
	bool holdsCursor = false;    // the cursor is not to move this frame
};

inline float Dot3(const NiPoint3& a, const NiPoint3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// The ring's right: the head's right laid level (tracking up is +y), so the
// ring stands upright whichever way the head is tipped. A head looking
// straight up or down has no level right; the tracking x stands in.
inline NiPoint3 LevelRight(const NiPoint3& headRight) {
	NiPoint3 r{headRight.x, 0.0f, headRight.z};
	const float length = math::Sqrt(r.x * r.x + r.z * r.z);
	if (length < 1e-3f) {
		return NiPoint3{1.0f, 0.0f, 0.0f};
	}
	return NiPoint3{r.x / length, 0.0f, r.z / length};
}

// Which slot a hand offset points at: slot 0 (the "1" key) at the top, then
// round to the right - 1 top right, 2 right, ... 7 top left - each a sector
// of 45 degrees. -1 inside the dead zone.
inline int QuickSlotAt(const NiPoint3& offset, const NiPoint3& right, const NiPoint3& up,
                       float deadZone) {
	const float u = Dot3(offset, right);
	const float v = Dot3(offset, up);
	if (u * u + v * v < deadZone * deadZone) {
		return -1;
	}
	float angle = math::Atan2(u, v);  // 0 up, a quarter turn right
	if (angle < 0.0f) {
		angle += math::kTwoPi;
	}
	const int sector = static_cast<int>(angle / (math::kTwoPi / kQuickSlots) + 0.5f);
	return sector % kQuickSlots;
}

// The centre of slot i on a ring of the given radius, in the ring's own
// axes (u right, v up) - where the painter draws it and where the hand goes.
inline void QuickSlotCentre(int slot, float radius, float& u, float& v) {
	const float angle = static_cast<float>(slot) * (math::kTwoPi / kQuickSlots);
	u = radius * math::Sin(angle);
	v = radius * math::Cos(angle);
}

// A hotkey being set runs on: key down, click, key held, then both up. It
// stops, key and click up at once, when the menu goes or the mode stops.
inline void StepAssign(QuickMenuState& s, const QuickMenuInput& in,
                       const QuickMenuSettings& settings, QuickMenuVerdict& v) {
	if (s.assignPhase == AssignPhase::Idle) {
		return;
	}
	if (!settings.enabled || !in.allowed || !in.assign) {
		s.assignPhase = AssignPhase::Idle;
		s.assignSlot = -1;
		v.assignAborted = true;
		return;
	}
	// A phase lasts at least the frame it began on; the clock is checked
	// from the next one.
	s.assignClock += in.dt;
	if (s.assignPhase == AssignPhase::KeyDown && s.assignClock >= settings.assignLeadSeconds) {
		s.assignPhase = AssignPhase::Click;
		s.assignClock = 0.0f;
	} else if (s.assignPhase == AssignPhase::Click &&
	           s.assignClock >= settings.assignClickSeconds) {
		s.assignPhase = AssignPhase::KeyHeld;
		s.assignClock = 0.0f;
	} else if (s.assignPhase == AssignPhase::KeyHeld &&
	           s.assignClock >= settings.assignTailSeconds) {
		s.assignPhase = AssignPhase::Idle;
		s.assignSlot = -1;
		v.assignDone = true;
		return;
	}
	v.key = s.assignSlot + 1;
	v.click = s.assignPhase == AssignPhase::Click;
	v.holdsCursor = true;
}

inline QuickMenuVerdict StepQuickMenu(QuickMenuState& s, const QuickMenuInput& in,
                                      const QuickMenuSettings& settings) {
	QuickMenuVerdict v;
	// A key already tapped runs out whatever else happens.
	if (s.tapSlot >= 0) {
		v.key = s.tapSlot + 1;
		s.tapLeft -= in.dt;
		if (s.tapLeft <= 0.0f) {
			s.tapSlot = -1;
		}
	}
	StepAssign(s, in, settings, v);
	const bool press = in.pad && !s.padWas;
	s.padWas = in.pad;
	if (!settings.enabled || !in.allowed || (s.open && s.assigning != in.assign)) {
		if (s.open) {
			s.open = false;
			s.highlighted = -1;
			// A menu came up or went, or the mode stopped.
			v.cancelled = true;
		}
		return v;
	}
	if (!s.open) {
		if (!press || s.tapSlot >= 0 || s.assignPhase != AssignPhase::Idle) {
			return v;
		}
		s.open = true;
		s.assigning = in.assign;
		s.anchor = in.hand;
		s.right = LevelRight(in.headRight);
		s.up = NiPoint3{0.0f, 1.0f, 0.0f};
		v.opened = true;
	}
	s.highlighted = QuickSlotAt(in.hand - s.anchor, s.right, s.up, settings.deadZoneMetres);
	v.assigning = s.assigning;
	if (in.pad) {
		v.visible = true;
		v.highlighted = s.highlighted;
		v.holdsCursor = s.assigning;
		return v;
	}
	// Let go.
	s.open = false;
	const int chosen = s.highlighted;
	s.highlighted = -1;
	if (s.assigning) {
		// Any slot, empty or not: what it held is replaced.
		if (chosen >= 0) {
			s.assignPhase = AssignPhase::KeyDown;
			s.assignSlot = chosen;
			s.assignClock = 0.0f;
			v.assigned = chosen;
			v.key = chosen + 1;
			v.holdsCursor = true;
		} else {
			v.cancelled = true;
		}
		return v;
	}
	if (chosen >= 0 && in.filled[chosen]) {
		s.tapSlot = chosen;
		s.tapLeft = settings.tapSeconds;
		v.key = chosen + 1;
		v.used = chosen;
	} else {
		v.cancelled = true;
	}
	return v;
}

}  // namespace obvr::vr
