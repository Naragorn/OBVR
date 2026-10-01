#pragma once

#include "core/Types.h"

namespace obvr::game {

// The bow's draw sound in parts, for the bow drawn by hand (vr::BowSoundsFor;
// the tester, 2026-09-30 evening: "den bogen spann sound müssen wir whl
// stückeln so dass das spannen des bogens und andere teile davon einzeln
// abspielen").
//
// The engine plays one sound for the whole draw: bowattack.kf has the key
// "Sound: WPNBowDraw" at 0.033 s (the arrow attached at 0.267, the draw held
// at 1.367), and the key handler (TESAnimGroup, 0x0051AF70) makes it at
// 0x0051B059 - call 0x006AE0A0 with the key's sound form - and starts it.
// WPNBowDraw is 00097C38, fx\wpn\wpn_bowdraw.wav in Oblivion.esm: 1.58 s,
// 16-bit mono at 44100 Hz. Its loudness (20 ms steps, read 2026-09-30):
// silent to 0.30 s; a first part to 0.98 with two clicks (0.52, 0.82); silent
// again to 1.04; a second, louder part to its end. Timed against the
// animation the first part follows the arrow's attach, the second runs into
// the hold - the string's stretch. Which part is which is read off that, not
// heard.
//
// So, while the bow is drawn by hand, that one call is answered with no sound
// for that form (the handler then skips the key, as for a sound it cannot
// make), and OBVR plays the file's two parts itself at the moments the hand
// makes: the first at the nock, the second as the draw begins. It cuts them
// from the game's own file - a loose Data\Sound\fx\wpn\wpn_bowdraw.wav first,
// else "Oblivion - Sounds.bsa" - and plays them on a DirectSound device of its
// own, at the game's master and effects volume. Nothing is played, and the
// engine's sound is left alone, when any of that fails.

// Puts the call at 0x0051B059 through OBVR. False when the bytes there are
// not that call.
bool InstallBowDrawSound();

// While true the engine's WPNBowDraw is kept quiet - once the parts are ready
// to play (the first true loads them).
void SetBowDrawSoundByHand(bool byHand);

enum class BowDrawPart : UInt8 {
	Nock = 0,     // the file's first part
	Stretch = 1,  // its second
};

// From its start. False when the parts could not be made.
bool PlayBowDrawPart(BowDrawPart part);
void StopBowDrawPart(BowDrawPart part);

}  // namespace obvr::game
