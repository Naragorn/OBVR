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
};

struct QuickMenuState {
	bool open = false;
	NiPoint3 anchor{0.0f, 0.0f, 0.0f};  // the hand where the ring opened, tracking space
	NiPoint3 right{1.0f, 0.0f, 0.0f};   // the ring's right and up, tracking space
	NiPoint3 up{0.0f, 1.0f, 0.0f};
	int highlighted = -1;
	int tapSlot = -1;  // the slot whose key is down
	float tapLeft = 0.0f;
	bool padWas = false;
};

struct QuickMenuInput {
	bool pad = false;      // the right trackpad clicked, as it is now
	bool allowed = false;  // Full VR on, in the world, no menu up
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
	const bool press = in.pad && !s.padWas;
	s.padWas = in.pad;
	if (!settings.enabled || !in.allowed) {
		if (s.open) {
			s.open = false;
			s.highlighted = -1;
			v.cancelled = true;  // a menu came up, or the mode stopped
		}
		return v;
	}
	if (!s.open) {
		if (!press || s.tapSlot >= 0) {
			return v;
		}
		s.open = true;
		s.anchor = in.hand;
		s.right = LevelRight(in.headRight);
		s.up = NiPoint3{0.0f, 1.0f, 0.0f};
		v.opened = true;
	}
	s.highlighted = QuickSlotAt(in.hand - s.anchor, s.right, s.up, settings.deadZoneMetres);
	if (in.pad) {
		v.visible = true;
		v.highlighted = s.highlighted;
		return v;
	}
	// Let go.
	s.open = false;
	const int chosen = s.highlighted;
	s.highlighted = -1;
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
