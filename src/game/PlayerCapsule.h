#pragma once

#include "core/Types.h"

namespace obvr::game {

// The player's body in Havok, and a scale for its radius
// ([Hands] BodyRadiusScale; the tester, 2026-09-29: "NPCs wegschieben
// passiert immernoch, vll einfach durch den player körper? und der scheint
// mom noch einen zu großen radius zu haben. mach eine setting wo man den
// kleiner oder größer machen kann").
//
// Read in Oblivion.exe 1.2.0.416 (2026-09-29). The character controller
// (0x0065A2C0 thiscall(actor)) keeps two shapes, per actor, built from its
// skeleton's bound ("BBX") times its scale (0x00895190):
// - +0x374, slot 0: a capsule of the character's radius;
// - +0x378, slot 1: an 18-point convex hull, the one a humanoid walks with;
// - +0x36C the slot in use, +0x3A0 the radius (Havok units, the mean of the
//   bound's half width and depth), +0x3A4 the height, +0x3A8 the radius the
//   capsule grows to.
// The console's SetSize ("player.shrink") calls 0x00894BD0
// thiscall(controller, float radius), ret 4: the target at +0x3A8 (at most
// half the height; 0 or less is the game's own radius), the capsule taken
// as the shape, and the radius grown to it each frame (0x00894C70, 5 Havok
// units a second); back at the game's own radius, the hull again. Nothing
// for a controller with bit 0 of +0x1F4 (a wide creature). OBVR asks for its
// radius the same way. On entering a world the engine shrinks the capsule
// and asks for its own radius again (0x00895060); the next frame OBVR asks
// for the scaled one again.

struct PlayerBodyReading {
	bool valid = false;
	float gameRadiusUnits = 0.0f;    // +0x3A0, game units
	float targetRadiusUnits = 0.0f;  // +0x3A8
	UInt32 slot = 0;                 // +0x36C
	float capsuleRadiusUnits = 0.0f;  // slot 0's capsule as it stands
};

PlayerBodyReading ReadPlayerBody();

// Asks the controller for the game's radius times `scale` when its target
// is another. Once a frame.
void StepPlayerCapsule(float scale);

}  // namespace obvr::game
