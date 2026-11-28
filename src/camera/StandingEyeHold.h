#pragma once

// The view held still against the first-person animation while the player
// stands (Nadi in the headset, 2026-11-10: drawing a weapon the view wobbled,
// and the legs with it - nothing should wobble).
//
// Measured 2026-11-06 (draw-leg-flicker.txt, docs/player-body-spec.md, the
// open point "the view itself steps as the weapon is drawn"): the engine's
// first-person eye is Camera01 of the first-person skeleton, and the
// hastened Equip moves it - up to 0.5 units a frame there and back with the
// head and the player still, 0.55 at the draw's start at the game's own
// speed too; drawn and sheathed it stands 0.9 units apart. The shown body's
// hips hang from the camera, so its knees moved with it (half a unit of
// height moves a nearly straight knee 2.6).
//
// So while the player stands, the engine's eye is taken against the player's
// feet in the player's own frame (x right, y forward, z up) and that offset
// is eased toward the engine's over kStandingEyeSeconds: a step of an
// animation is spread over a second instead of a frame, and the change from
// sheathed to drawn arrives as a glide. Walking, it follows within
// kWalkingEyeSeconds, so a walk's start does not trail. An offset further
// than kStandingEyeSnapUnits from the engine's (a load, a teleport, a scale)
// is taken at once. Not live - third person, no headset, on a horse,
// sneaking, swimming, talking, out of Full VR - the engine's own eye.
//
// The real head is the only thing that should move the view while the
// player stands still; the room follow, the sneak's hold and the riding view
// stand on top of this as before.
//
// Under a menu with the world running behind it (the inventory tablet, a
// reached container, UnpausedMenus) the camera pass runs on, and the view is
// kept where it stood over the feet as the menu came up: nothing eased
// toward the engine's eye, no jump taken (Nadi in the headset, 2026-11-27:
// "at inventory open the vr view and body take back one step. closing inv
// moves forward" - docs/inventory-tablet-spec.md 28). A menu moves nobody's
// head. Before, a menu was "not live": the engine's eye passed as it was,
// from the menu's first frame to its last. The offset rides on the feet, so
// the walk under the tablet carries it. How far the engine's eye stood from
// the kept one under the menu is counted and said once the menu is gone
// (MenuEyeNote) - the measurement of what the engine does there, which no
// run had taken. For kMenuEyeGraceFrames after the menu a far offset is
// eased like a near one, so a last frame of the menu's eye is no jump there
// and back. Not in a conversation (the dialogue's base is held in the camera
// hook) and not knocked down under a menu: the engine's eye, as before.
//
// Sneaking (2026-11-11), the eye is held across the ground only: its height
// is the sneak's - the game's drop, or camera/SneakEyeHold.h's lift for a
// real crouch - and passes as the engine has it. Before, a sneak let go of
// the hold altogether: a weapon picked up from the floor in a real crouch
// (which sneaks) was equipped and drawn with the view going 0.5 to 0.8
// units a frame sideways for eight frames (weapon-pickup-wobble.txt).
//
// Pure, covered by standing_eye_hold_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::camera {

inline constexpr float kStandingEyeSeconds = 1.0f;
inline constexpr float kWalkingEyeSeconds = 0.1f;
inline constexpr float kStandingEyeSnapUnits = 20.0f;
inline constexpr UInt32 kMenuEyeGraceFrames = 10;
// The engine's eye "stood off" under a menu from this far (the note's first frame).
inline constexpr float kMenuEyeOffUnits = 2.0f;

// A menu's episode as it runs: how long, and where the engine's eye stood
// furthest from the kept one (the player's frame: x right, y forward, z up).
struct MenuEyeEpisode {
	bool up = false;
	UInt32 frames = 0;
	UInt32 firstFarFrame = 0;  // the first frame it stood kMenuEyeOffUnits off, counted from 1; 0 never
	NiPoint3 far{0.0f, 0.0f, 0.0f};
	float farUnits = 0.0f;
	NiPoint3 feet{0.0f, 0.0f, 0.0f};  // the feet as the menu came up, the world
};

// The episode once it is over, for the log (TakeMenuEyeNote).
struct MenuEyeNote {
	bool ready = false;
	UInt32 frames = 0;
	UInt32 firstFarFrame = 0;
	NiPoint3 far{0.0f, 0.0f, 0.0f};
	float farUnits = 0.0f;
	float feetMovedUnits = 0.0f;  // across the ground, from the menu's first frame to its last
};

struct StandingEyeHoldState {
	bool valid = false;
	NiPoint3 held{0.0f, 0.0f, 0.0f};  // the eye from the feet, the player's frame
	UInt32 graceFrames = 0;            // after a menu: a far offset eased, not taken at once
	MenuEyeEpisode menu;
	MenuEyeNote note;
};

struct StandingEyeHoldInput {
	bool live = false;
	bool underMenu = false;              // a menu up, the world running behind it (StandingEyeUnderMenu)
	bool standing = false;               // the feet asked for no step
	bool heightFree = false;             // sneaking: the height the engine's, held across the ground only
	NiPoint3 eye{0.0f, 0.0f, 0.0f};      // the engine's eye, the world
	NiPoint3 feet{0.0f, 0.0f, 0.0f};     // the player's position, the world
	float yaw = 0.0f;                    // the player's rotZ: forward (sin, cos), right (cos, -sin)
	float dt = 0.0f;
};

// What decides whether the hold is live this camera pass. `base`: first
// person, a headset, Full VR. A menu's frame only reaches the camera pass
// with the world running behind the menu.
struct StandingEyeGates {
	bool base = false;
	bool playerRead = false;
	bool riding = false;
	bool swimming = false;
	bool menuUp = false;
	bool talking = false;      // a conversation: the camera hook holds the dialogue's base itself
	bool knockedDown = false;
};

inline bool StandingEyeUnderMenu(const StandingEyeGates& g) {
	return g.base && g.playerRead && !g.riding && !g.swimming && g.menuUp && !g.talking && !g.knockedDown;
}

inline bool StandingEyeLive(const StandingEyeGates& g) {
	return g.base && g.playerRead && !g.riding && !g.swimming && (!g.menuUp || StandingEyeUnderMenu(g));
}

inline NiPoint3 EyeToPlayerFrame(const NiPoint3& world, float yaw) {
	const float s = math::Sin(yaw);
	const float c = math::Cos(yaw);
	return NiPoint3{world.x * c - world.y * s, world.x * s + world.y * c, world.z};
}

inline NiPoint3 EyeFromPlayerFrame(const NiPoint3& local, float yaw) {
	const float s = math::Sin(yaw);
	const float c = math::Cos(yaw);
	return NiPoint3{local.x * c + local.y * s, -local.x * s + local.y * c, local.z};
}

inline bool EyeHoldFinite(const NiPoint3& p) {
	return p.x == p.x && p.y == p.y && p.z == p.z && p.x < 1.0e7f && p.x > -1.0e7f && p.y < 1.0e7f &&
	       p.y > -1.0e7f && p.z < 1.0e7f && p.z > -1.0e7f;
}

// The episode's note, once: what the log says of a menu that is gone.
inline MenuEyeNote TakeMenuEyeNote(StandingEyeHoldState& s) {
	const MenuEyeNote note = s.note;
	s.note = MenuEyeNote{};
	return note;
}

// The menu's episode over: its note written, with how far the feet went.
inline void EndMenuEyeEpisode(StandingEyeHoldState& s, const NiPoint3& feet) {
	MenuEyeNote note;
	note.ready = true;
	note.frames = s.menu.frames;
	note.firstFarFrame = s.menu.firstFarFrame;
	note.far = s.menu.far;
	note.farUnits = s.menu.farUnits;
	const float dx = feet.x - s.menu.feet.x;
	const float dy = feet.y - s.menu.feet.y;
	const float moved = math::Sqrt(dx * dx + dy * dy);
	note.feetMovedUnits = moved == moved && moved < 1.0e7f ? moved : 0.0f;
	s.note = note;
	s.menu = MenuEyeEpisode{};
}

// A frame under a menu: the kept offset on the feet, and the engine's eye
// against it counted. `gap` is the engine's offset less the kept one.
inline NiPoint3 StepEyeUnderMenu(StandingEyeHoldState& s, const StandingEyeHoldInput& in, const NiPoint3& gap) {
	if (!s.menu.up) {
		s.menu = MenuEyeEpisode{};
		s.menu.up = true;
		s.menu.feet = in.feet;
	}
	++s.menu.frames;
	const float off = math::Sqrt(gap.LengthSquared());
	if (off > s.menu.farUnits) {
		s.menu.farUnits = off;
		s.menu.far = gap;
	}
	if (s.menu.firstFarFrame == 0 && off >= kMenuEyeOffUnits) {
		s.menu.firstFarFrame = s.menu.frames;
	}
	s.graceFrames = kMenuEyeGraceFrames;
	return in.feet + EyeFromPlayerFrame(s.held, in.yaw);
}

// The eye to place the camera at this frame, in the world.
inline NiPoint3 StepStandingEyeHold(StandingEyeHoldState& s, const StandingEyeHoldInput& in) {
	const NiPoint3 offset = EyeToPlayerFrame(in.eye - in.feet, in.yaw);
	const bool usable = in.live && EyeHoldFinite(offset) && in.yaw == in.yaw;
	const bool underMenu = usable && in.underMenu && s.valid;
	if (s.menu.up && !underMenu) {
		EndMenuEyeEpisode(s, in.feet);
	}
	if (!usable) {
		s.valid = false;
		s.held = NiPoint3{0.0f, 0.0f, 0.0f};
		s.graceFrames = 0;
		return in.eye;
	}
	if (in.underMenu && !s.valid) {
		return in.eye;  // nothing held as the menu came up: the engine's, and nothing taken from a menu's frame
	}
	NiPoint3 gap = offset - s.held;
	if (in.heightFree) {
		gap.z = 0.0f;  // the sneak's height is not a jump
	}
	if (underMenu) {
		if (in.heightFree) {
			s.held.z = offset.z;
		}
		return StepEyeUnderMenu(s, in, gap);
	}
	const bool far = gap.LengthSquared() > kStandingEyeSnapUnits * kStandingEyeSnapUnits;
	if (!s.valid || (far && s.graceFrames == 0)) {
		s.valid = true;
		s.held = offset;
		s.graceFrames = 0;
		return in.eye;
	}
	if (s.graceFrames > 0) {
		--s.graceFrames;
	}
	const float dt = in.dt == in.dt && in.dt > 0.0f && in.dt < 1.0f ? in.dt : 0.0f;
	const float seconds = in.standing ? kStandingEyeSeconds : kWalkingEyeSeconds;
	const float share = 1.0f - math::Exp(-dt / seconds);
	s.held = s.held + gap * share;
	if (in.heightFree) {
		s.held.z = offset.z;
	}
	return in.feet + EyeFromPlayerFrame(s.held, in.yaw);
}

}  // namespace obvr::camera
