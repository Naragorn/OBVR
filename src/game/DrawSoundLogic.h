#pragma once

// The draw's and the sheathe's sound (Nadi's headset run of 2026-11-28:
// "waffen ziehen ja geht aber nun kamen manchmal keine equip sounds obwohl ich
// die ziehen konnte").
//
// The engine plays a weapon's WPN<type>Equip / Unequip only at the "Enum:
// Equip" / "Enum: Unequip" key of the draw's or sheathe's animation (the key
// handler 0x0051AF70 calls 0x006B07F0(actor, 9 or 10) at 0x0051B390 and
// 0x0051B39D). A weapon that comes to the hand without that animation is
// silent:
// - equipped while the weapon is out - the fists up - the engine hangs it on
//   the hand in the same frame and plays no Equip group (measured 2026-10-25,
//   OBNG knowledge weapon-equip-draw.md); OBVR's holster does that when the
//   fists are up and the place keeps a weapon not equipped (OBVR.log
//   2026-11-28 13:55, 24275 and 28480: "equipping the one kept there, then
//   drawing", the axe on the hand, no draw animation);
// - taken straight into the hand by a grab (camera/hands/Grab.cpp: the
//   weapon out, the equip, the engine's hang 0x00656270): no Equip group
//   either (24052, 24359, 28565, 29204 in the same log).
// For those two ways OBVR plays the engine's own sound (0x006B07F0 with kind
// 9, the same name, pitch and volume as the key would) once the weapon shows
// on the hand. Never twice: an engine Equip of the same draw - before it
// (within kDrawNoteSeconds) or after - stops OBVR's, and once one sound of a
// draw or sheathe played, a second engine one is kept quiet (the log's
// sheathe at 2260-2284 ran its Unequip key twice). Equipped in a menu with
// the fists up stays as the game has it: no draw sound.
//
// Each draw and sheathe is one record, said once when it ends (another
// weapon, or kDrawSoundRecordSeconds), for the log line that tells which way
// the weapon came and whether the engine's key fired. Pure, covered by
// draw_sound_test; the caller is game/DrawSound.cpp.

#include "core/Types.h"

namespace obvr::game {

enum class DrawWay : UInt8 { Animation, EquippedWhileOut, TakenToHand, Menu };
enum class DrawSoundKind : UInt8 { Equip = 9, Unequip = 10 };

// The longest draw: twohandequip.kf 0.933 s at the game's pace (OBNG
// knowledge); its key fires within it.
constexpr float kDrawSoundRecordSeconds = 1.0f;
// How far apart a path's note or an engine sound and the weapon's change on
// the hand may be and still be the same draw - a few frames (a guess with
// room: the take's hang and its equip land within two frames, the log).
constexpr float kDrawNoteSeconds = 0.25f;

struct DrawSoundFrame {
	UInt32 onHand = 0;     // the drawn weapon's form id; 0: none out (sheathed, the fists, nothing)
	bool out = false;      // the weapon-out flag
	bool menuUp = false;
	bool staff = false;    // the weapon is a staff
	float dt = 0.0f;
};

struct DrawSoundRecord {
	bool open = false;
	bool draw = false;     // a draw; false: a sheathe
	UInt32 form = 0;
	DrawWay way = DrawWay::Animation;
	float age = 0.0f;
	bool flipped = false;  // the weapon-out flag came on with it
	UInt32 engine = 0;     // the engine's sounds of this record played
	UInt32 quieted = 0;    // and the seconds kept quiet
	bool ownPlayed = false;
};

struct DrawSoundState {
	bool started = false;
	UInt32 lastOnHand = 0;
	bool lastOut = false;
	bool noted = false;     // a take straight into the hand, before its weapon shows
	float noteAge = 0.0f;
	UInt32 looseEquips = 0;  // the engine's sounds with no record open, the last kDrawNoteSeconds
	UInt32 looseUnequips = 0;
	float looseAge = 0.0f;
	DrawSoundRecord rec;
};

struct DrawSoundStep {
	bool playEquip = false;  // OBVR plays the engine's Equip sound now
	bool report = false;     // `said` ended: one log line
	DrawSoundRecord said;
};

inline bool OwesOwnDrawSound(DrawWay way, bool flipped, bool staff) {
	if (way == DrawWay::EquippedWhileOut) {
		return true;
	}
	// A staff coming out plays its own (camera/hands/ControlsOut.cpp, the staff's
	// draw sound on the weapon-out change).
	return way == DrawWay::TakenToHand && !(staff && flipped);
}

// A grab hung the taken weapon on the hand (camera/hands/Grab.cpp).
inline void NoteTakenToHand(DrawSoundState& s) {
	if (s.rec.open && s.rec.draw && s.rec.age <= kDrawNoteSeconds) {
		if (s.rec.way == DrawWay::Animation) {
			s.rec.way = DrawWay::TakenToHand;  // seen on the hand earlier in this frame
		}
		return;
	}
	s.noted = true;
	s.noteAge = 0.0f;
}

// The engine is about to play its Equip or Unequip for the player: true lets
// it, false keeps it quiet - the second of the same draw or sheathe.
inline bool EngineSoundAllowed(DrawSoundState& s, DrawSoundKind kind) {
	const bool equip = kind == DrawSoundKind::Equip;
	if (s.rec.open && s.rec.draw == equip) {
		if (s.rec.ownPlayed || s.rec.engine > 0) {
			++s.rec.quieted;
			return false;
		}
		++s.rec.engine;
		return true;
	}
	if (equip) {
		++s.looseEquips;
	} else {
		++s.looseUnequips;
	}
	s.looseAge = 0.0f;
	return true;
}

inline void OpenDrawRecord(DrawSoundState& s, const DrawSoundFrame& f) {
	DrawSoundRecord& r = s.rec;
	r = DrawSoundRecord{};
	r.open = true;
	if (f.onHand != 0) {
		r.draw = true;
		r.form = f.onHand;
		r.flipped = !s.lastOut && f.out;
		r.way = f.menuUp ? DrawWay::Menu
		        : s.noted ? DrawWay::TakenToHand
		        : s.lastOut && f.out ? DrawWay::EquippedWhileOut
		                             : DrawWay::Animation;
		r.engine = s.looseEquips;
		s.looseEquips = 0;
		s.noted = false;
	} else {
		r.form = s.lastOnHand;
		r.way = f.menuUp ? DrawWay::Menu : DrawWay::Animation;
		r.engine = s.looseUnequips;
		s.looseUnequips = 0;
	}
}

inline DrawSoundStep StepDrawSound(DrawSoundState& s, const DrawSoundFrame& f) {
	DrawSoundStep out;
	const float dt = f.dt > 0.0f ? f.dt : 0.0f;  // not a number: no time
	if (s.noted && (s.noteAge += dt) > kDrawNoteSeconds) {
		s.noted = false;
	}
	if ((s.looseEquips != 0 || s.looseUnequips != 0) && (s.looseAge += dt) > kDrawNoteSeconds) {
		s.looseEquips = 0;
		s.looseUnequips = 0;
	}
	if (!s.started) {
		s.started = true;
		s.lastOnHand = f.onHand;
		s.lastOut = f.out;
		return out;
	}
	if (s.rec.open) {
		s.rec.age += dt;
	}
	const bool changed = f.onHand != s.lastOnHand;
	if (s.rec.open && (changed || s.rec.age >= kDrawSoundRecordSeconds)) {
		out.report = true;
		out.said = s.rec;
		s.rec = DrawSoundRecord{};
	}
	if (changed) {
		OpenDrawRecord(s, f);
	}
	DrawSoundRecord& r = s.rec;
	if (r.open && r.draw && !r.ownPlayed && r.engine == 0 && OwesOwnDrawSound(r.way, r.flipped, f.staff)) {
		r.ownPlayed = true;
		out.playEquip = true;
	}
	s.lastOnHand = f.onHand;
	s.lastOut = f.out;
	return out;
}

inline const char* DrawWayWords(DrawWay way) {
	switch (way) {
	case DrawWay::EquippedWhileOut:
		return "equipped while out (no Equip group)";
	case DrawWay::TakenToHand:
		return "taken straight into the hand";
	case DrawWay::Menu:
		return "equipped in a menu";
	default:
		return "by its animation";
	}
}

}  // namespace obvr::game
