#pragma once

// The sound a heard teleport makes when it lands ([Locomotion] TeleportSound;
// the tester, 2026-09-29: "fügen wir nun einen sound hinzu. whl den von der
// rolle ... wir bieten in den settings alle 4 an. mit default auf
// fstdodge"; then, 2026-09-29: "setzen wir als default: landing").
//
// Played with TeleportMakesNoise only: a teleport the engine does not hear
// stays silent to the player as well.
//
// The sounds are Oblivion.esm's (its SOUN records, read 2026-09-29):
// - dodge: FSTDodge 000CBA79, fx\fst\dodge\fst_dodge.wav, the Acrobatics
//   dodge roll's;
// - dodgeback: FSTDodgeBackward 000CBA7A, the roll backwards;
// - landing: the engine's own landing after a jump (0x006B1900), which picks
//   FootSound{Earth,Grass,Metal,Stone,Water,Wood}Land (00000219-0000021E)
//   by the ground under the player, and the armour's landing with it;
// - swish: WPNSwishHand 00088834 (the folder fx/wpn/swish/hand, one of three
//   files at random), a fist's swing through the air. Not WPNSwishHandX
//   0000C3D1: its file is under fx/wpn/360lofi, which the PC's
//   Oblivion - Sounds.bsa does not
//   have - the game made the sound and nothing was heard (the tester,
//   2026-09-29: "swish macht keinen sound").
//
// Pure, covered by teleport_sound_test.

#include "core/ChoiceWord.h"
#include "core/Types.h"

namespace obvr::vr {

enum class TeleportSound : UInt8 {
	Dodge = 0,
	DodgeBackward = 1,
	Landing = 2,
	Swish = 3,
};

inline constexpr UInt32 kTeleportSoundCount = 4;
inline constexpr TeleportSound kTeleportSoundDefault = TeleportSound::Landing;

// The INI's words, in the order of the values, and the settings row's.
inline constexpr const char* kTeleportSoundNames[kTeleportSoundCount] = {"dodge", "dodgeback", "landing", "swish"};

inline constexpr UInt32 kSoundFormDodge = 0x000CBA79;
inline constexpr UInt32 kSoundFormDodgeBackward = 0x000CBA7A;
inline constexpr UInt32 kSoundFormSwishHand = 0x00088834;

inline bool ParseTeleportSound(const char* text, TeleportSound& out) {
	UInt32 index = 0;
	if (!MatchChoiceWord(text, kTeleportSoundNames, kTeleportSoundCount, index)) {
		return false;
	}
	out = static_cast<TeleportSound>(index);
	return true;
}

// The settings row's value (0..3) as a sound; out of range is the default.
inline TeleportSound TeleportSoundFromIndex(float value) {
	if (!(value > -0.5f) || !(value < static_cast<float>(kTeleportSoundCount) - 0.5f)) {
		return kTeleportSoundDefault;
	}
	return static_cast<TeleportSound>(static_cast<int>(value + 0.5f));
}

// The sound form to play, or 0 for the landing: that one the engine picks
// by the ground.
inline UInt32 TeleportSoundForm(TeleportSound sound) {
	switch (sound) {
	case TeleportSound::Dodge:
		return kSoundFormDodge;
	case TeleportSound::DodgeBackward:
		return kSoundFormDodgeBackward;
	case TeleportSound::Swish:
		return kSoundFormSwishHand;
	case TeleportSound::Landing:
	default:
		return 0;
	}
}

// Whether this frame plays it: the move has just landed, and the teleport
// is heard.
inline bool TeleportSoundDue(bool makesNoise, bool landedThisFrame) { return makesNoise && landedThisFrame; }

}  // namespace obvr::vr
