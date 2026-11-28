#pragma once

// What a menu frame's camera is built on (PlaceMenuCamera): exactly what the
// last world frame's was built on.
//
// A menu frame the camera hook does not run on (the stand-in, CameraHook.cpp
// PrepareMenuFrameIfNeeded) places the camera itself, from the engine's eye
// the hook kept (g_menuBasePos, taken before OBVR adds anything) and the
// levelled rotation the hook built on (g_menuBaseRot, with the body's turn
// taken back out). The world frame lays more on that eye than the head's
// offset and the real-height lift, and the menu path used to leave it out:
//
//   * the aim arc (FrameLogic.h AimArcCorrection): the eye put back where it
//     stood before the body turned under the view - the engine's eye stands
//     4.6 units in front of the body's axis, so a body turned by an angle
//     walks it along an arc. The walk direction ([Hands] WalkDirection) turns
//     the body through the same hand-over as the aim and leaves it turned
//     after the walk, so the body carries a turn most of the time: 2.4 units
//     at 30 degrees, 6.5 at 90, 8.6 at 137. Left out, the view stepped that
//     far at every menu's opening and back at its close (Nadi in the
//     headset, 2026-11-27: "the view and the body take one step back"; and
//     2026-11-28: "die vr view switcht auf die position vom linken auge" -
//     the inventory on the tablet, a book, the Esc menu alike);
//   * the aim tilt (third person, AimTiltCorrection);
//   * the engine-animation view (camera/EngineAnimView.h): the eye in the
//     body's head, and the look control's height left out there;
//   * the sneak's lift (camera/SneakEyeHold.h), up to 24.7 units of height
//     in a real crouch.
// The rotation had the same fault once (the live menu background, reported
// 2026-08-30) and was fixed the same way: the hook records what it built on.
//
// While the death view is held its own place stands in for the engine's eye
// and the aim's terms (the hook overwrites the arc and the tilt with the held
// place, StepDeathView), as MenuCameraBase decides.
//
// Pure, covered by menu_view_terms_test.

#include "game/NiMath.h"

namespace obvr::camera {

// What the last world frame laid on the engine's eye besides the head's
// offset and the real-height lift.
struct WorldViewTerms {
	NiPoint3 aimArc{0.0f, 0.0f, 0.0f};      // AimArcCorrection, as applied
	NiPoint3 aimTilt{0.0f, 0.0f, 0.0f};     // AimTiltCorrection, as applied
	NiPoint3 engineAnim{0.0f, 0.0f, 0.0f};  // the engine-animation view's move of the eye
	float lookHeight = 0.0f;                // the look control's height as used (0 when the anim view took the eye)
	float sneakLift = 0.0f;                 // the sneak's lift
};

// The menu frame's base: where the head's offset goes on, and the height
// that goes on with the real-height lift.
struct MenuViewBase {
	NiPoint3 pos{0.0f, 0.0f, 0.0f};
	float vertical = 0.0f;
};

// gameBase: MenuCameraBase's answer (the engine's eye, or the death view's
// place while it is held); deathHeld: the death view held.
inline MenuViewBase MenuViewBaseFor(const NiPoint3& gameBase, const WorldViewTerms& terms, bool deathHeld) {
	MenuViewBase base;
	base.vertical = terms.lookHeight + terms.sneakLift;
	base.pos = deathHeld ? gameBase : gameBase + terms.aimArc + terms.aimTilt + terms.engineAnim;
	return base;
}

}  // namespace obvr::camera
