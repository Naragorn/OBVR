#pragma once

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// The engine's free fly camera, the console's ToggleFlyCam ("tfc", opcode
// 0x16E, "Toggles the Free Fly camera (UFO cam)"; handler 0x005013E0 calls
// 0x006669F0 on the player). Read in Oblivion.exe 1.2.416:
//
// - 0x006669F0 flips the byte PlayerCharacter+0x748; switched on it seeds the
//   camera's place +0x754/+0x758/+0x75C from the player (eye height added)
//   and its yaw +0x74C from rotZ, its pitch +0x750 from rotX.
// - PlayerCharacter::Update (0x00671620) calls 0x006643A0 every frame while
//   it is set: the mouse turns yaw and pitch, Forward/Back/StrafeLeft/Right
//   move 10 units a frame along the look (no up or down, frame time not
//   counted), and the result is written into the scene graph's camera.
// - The same Update skips 0x0066B710 while it is set - the player's camera
//   update, where OBVR's camera hook sits (0x0066BE6E): no camera pass.
// - The player's own walking and mouse turn are off while it is set.
//
// Found 2026-10-31 (Nadi: "manche mods schalten tfc an ... auf keyboard kann
// man die steuern. in obvr mit sticks nicht").
struct FlyCamPose {
	bool on = false;
	NiPoint3 at{0.0f, 0.0f, 0.0f};
	float yaw = 0.0f;    // radians, as rotZ
	float pitch = 0.0f;  // radians, as rotX
};

// Whether the toggle is the game's own (0x006669F0's first bytes test +0x748),
// checked once. Without it nothing here reads or writes.
bool FlyCamKnown();

// The fly camera as the player holds it; false when the player is not there
// or the toggle is not the game's own.
bool ReadFlyCam(FlyCamPose& out);

// Writes the fly camera's place and yaw (the pitch stays 0 - the head looks up
// and down). The engine's next step starts from them.
bool WriteFlyCam(const NiPoint3& at, float yaw);

}  // namespace obvr::game
