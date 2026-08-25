#pragma once

#include "core/Smoothing.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::camera {

// What OBVR does with the look controls once a headset has taken over.
//
// With a headset on, the head owns where the camera points. Leaving the stick
// and the mouse pointed at the same thing gives two answers to one question,
// and the artificial one is the answer that makes people sick: tilting a view
// that the inner ear insists is level is the classic trigger.
//
// So the vertical look is taken away from the stick, and what it drove is
// handed somewhere harmless. In third person it moves the camera up and down
// instead of tilting it - up above the character's head, down towards the feet
// - which is the shape Luke Ross's VR mods use. In first person it does
// nothing at all; there is nowhere sensible for it to go, and the head already
// covers it.
//
// Turning left and right stays with the player, because there is no other way
// to face a direction that is behind you. It can be eased, though, and that is
// the second setting here.
//
// None of this needs Oblivion's input code. The hook runs after the camera has
// been computed, so whatever the stick did is already sitting in the camera
// matrix and can be read back out and replaced - one less address to keep
// correct across game versions.

struct LookSettings {
	// Whether the vertical look is taken away from stick and keyboard at all.
	// With it off, OBVR leaves the camera rotation exactly as the game built
	// it and this whole file does nothing.
	bool blockVerticalLook = true;

	// How far the vertical look moves the camera in third person, in Oblivion
	// units, at full tilt. Negative flips the direction.
	//
	// Oblivion's own third person camera already swings vertically as it
	// tilts, and this is added on top of that. Which of the two dominates, and
	// whether they agree in direction, is a question for the headset rather
	// than for a wiki - hence a signed number in the INI rather than a
	// constant in the code. 0 keeps the game's own swing alone.
	float verticalLookRange = 60.0f;

	// Whether that vertical movement eases into place rather than tracking the
	// stick one to one.
	bool smoothVerticalLook = true;
	float verticalLookSpeed = 8.0f;

	// Whether turning left and right eases as well.
	//
	// Off by default, and deliberately so: turning is the one look control the
	// player still needs to aim with, and easing it puts the camera behind
	// where they asked it to be. It is here because smooth panning is easier
	// on some people than an instant one, which is the same trade Luke Ross
	// calls camera rotation compensation.
	bool smoothTurning = false;
	float turnSpeed = 12.0f;
};

// Turns the camera rotation the game computed into the one OBVR wants, and
// says how far the camera should move vertically with it.
//
// Deliberately free of any dependency on the game or on Windows: it is handed
// a matrix and a frame time and hands back a matrix and a number, which is
// what makes it testable without Oblivion running.
class LookControl {
public:
	void Configure(const LookSettings& settings);

	// Once per frame, before the head rotation is laid on top.
	//
	// deltaSeconds of 0 means the caller has no timing, and the easing is then
	// skipped rather than the camera being frozen.
	void Update(const NiMatrix33& vanillaRotation, bool isThirdPerson, float deltaSeconds);

	// The rotation to use in place of the one the game computed.
	const NiMatrix33& GetRotation() const { return m_rotation; }

	// How far to raise the camera, in Oblivion units along the world up axis.
	// 0 in first person, and 0 whenever the feature is switched off.
	float GetVerticalOffset() const { return m_verticalOffset; }

	// Forgets the eased state, so the next frame starts where the game is
	// rather than easing in from wherever the camera used to be. For the
	// moments where continuity would be wrong anyway - a load, a change of
	// point of view, a recenter.
	void Reset();

private:
	LookSettings m_settings;

	NiMatrix33 m_rotation = NiMatrix33::Identity();
	float m_verticalOffset = 0.0f;

	Heading m_heading;
	bool m_hasHeading = false;
};

}  // namespace obvr::camera
