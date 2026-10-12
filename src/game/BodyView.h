#pragma once

#include "game/NiMath.h"

namespace obvr::game {

// What the eyes see of the player's own body, at a hand script's mark
// (Debug.BodyView): the harness's answer to whether a look down sees down
// the body or into it, where the screenshots alone needed someone to look
// at them. The arithmetic is in BodyViewLogic.h; this reads the shapes the
// draw is about to use - the third-person body's skinned NiTriShapes, their
// vertices, triangles and skins as the engine holds them - and the camera's
// frustum, and casts each eye's rays at them.

struct BodyViewFrame {
	// The headset between the eyes, in the world, and the camera's rotation:
	// column 0 right, 1 forward, 2 up (core/Rotation.h ForwardOf).
	NiPoint3 cyclopean{0.0f, 0.0f, 0.0f};
	NiMatrix33 rotation = NiMatrix33::Identity();
	// The first eye's step from the middle (the camera hook's
	// g_bodyFirstEyeStep); the other eye is as far the other way. Zero
	// without stereo.
	NiPoint3 firstEyeStep{0.0f, 0.0f, 0.0f};
	bool firstIsLeft = true;
	// The mark's name, for the log and the picture's.
	const char* mark = "";
};

// Casts each eye's rays at the body as it is about to be drawn, logs what
// they meet ("Body view:" lines, one an eye and a verdict) and writes the
// picture OBVR-BodyView-<mark>.bmp beside Oblivion.exe: green the trunk from
// outside, blue the limbs, red the trunk's inside, orange a limb's; the rows
// below the view's bottom edge darker. False when the body or the camera's
// frustum could not be read; the log says why.
bool MeasureBodyView(const BodyViewFrame& frame);

}  // namespace obvr::game
