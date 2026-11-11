#pragma once

#include "core/Types.h"

namespace obvr::game {

// What a script's GetMousePos is told while OBVR drives the menus.
//
// xOBSE's GetMousePos (Cmd_GetCursorPos_Execute, llde/xOBSE) answers with the
// operating system's cursor, read by user32's GetCursorPos, in screen pixels.
// In the headset nothing moves that cursor: the laser and the engine's own
// cursor live in the game's interface, and the 2D believes a raised screen
// size (render/UiScreenSize.h) besides. Scripted menus that read it - LINK's
// mod configuration menu drags its sliders by
//     gamex = GetMousePos x * (960 / iSize H)
// every OnDragging, then forces the slider's value from that - set the
// slider back to wherever the desktop's mouse happens to rest (the tester,
// 2026-11-11: "wenn ich einen schieberegler bewegte dann sprang er nach paar
// s wieder zurück").
//
// So in menu mode the answer is the engine's interface cursor, mapped from
// the believed size to the size the game asked for (what iSize H still
// holds), as a game window at the screen's corner would have it - the
// mapping scripts like LINK undo with their 960 / iSize H.
struct OsCursorAsk {
	bool menuMode = false;
	bool cursorKnown = false;   // the interface cursor could be read
	float x = 0.0f;             // in the believed size's pixels
	float y = 0.0f;
	UInt32 askedHeight = 0;     // what the game asked for (iSize H); 0 = not known
	UInt32 believedHeight = 0;  // the interface's screen size copy
};

struct OsCursorAnswer {
	bool replace = false;  // false: the real cursor is answered
	SInt32 x = 0;
	SInt32 y = 0;
};

inline OsCursorAnswer AnswerOsCursor(const OsCursorAsk& ask) {
	OsCursorAnswer answer;
	if (!ask.menuMode || !ask.cursorKnown || ask.believedHeight == 0) {
		return answer;
	}
	if (!(ask.x >= -64.0f && ask.x <= 16384.0f && ask.y >= -64.0f && ask.y <= 16384.0f)) {
		return answer;  // NaN or off any screen
	}
	const UInt32 asked = ask.askedHeight != 0 ? ask.askedHeight : ask.believedHeight;
	const float scale = static_cast<float>(asked) / static_cast<float>(ask.believedHeight);
	const float x = ask.x * scale;
	const float y = ask.y * scale;
	answer.replace = true;
	answer.x = static_cast<SInt32>(x < 0.0f ? x - 0.5f : x + 0.5f);
	answer.y = static_cast<SInt32>(y < 0.0f ? y - 0.5f : y + 0.5f);
	return answer;
}

}  // namespace obvr::game
