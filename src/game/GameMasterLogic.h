#pragma once

#include "core/Types.h"

namespace obvr::game {

// Which game the engine runs: Oblivion, or a total conversion on the same
// EXE (Nehrim; Nadi, 2026-11-06). OBVR names some of Oblivion.esm's sound
// forms by their ids; a total conversion's master sits at load index 00 in
// Oblivion.esm's place and numbers its records its own way.
//
// Nehrim.esm (the classic 1.5.0.5 DE, read 2026-11-06 with a record parser)
// keeps the sounds under the same editor IDs at other ids - WPNHitBladeFlesh
// is 0000091D there, 0018B9F1 in Oblivion.esm. Where Oblivion.esm's ids
// stand in Nehrim.esm there are other records (0000C3CA ... 0000C3D0 are
// REFRs, 00000907 a SOUN instead of the Imperial race). Its sound archive has
// no fx\wpn: the files come from Oblivion's own "Oblivion - Sounds.bsa", so
// the same sounds play. Oblivion.esm's 360lofi "...X" sounds (one wav each)
// are not in Nehrim.esm; their own folders (WPNBlockBlade, WPNHitBlade ...)
// stand in. The gasps of the exhaustion are not in it at all. Pure,
// game_master_test.

enum class GameMaster : UInt8 {
	Oblivion = 0,
	Nehrim = 1,
};

// The master at load index 00 by its file name, without case. Anything but
// Nehrim.esm - Oblivion.esm, nothing read yet - is Oblivion.
inline GameMaster MasterFromFileName(const char* name) {
	if (name == nullptr) {
		return GameMaster::Oblivion;
	}
	const char* const nehrim = "nehrim.esm";
	for (UInt32 i = 0;; ++i) {
		char c = name[i];
		if (c >= 'A' && c <= 'Z') {
			c = static_cast<char>(c - 'A' + 'a');
		}
		if (c != nehrim[i]) {
			return GameMaster::Oblivion;
		}
		if (c == '\0') {
			return GameMaster::Nehrim;
		}
	}
}

struct SoundFormPair {
	UInt32 oblivion;  // Oblivion.esm's id, as OBVR names it
	UInt32 nehrim;    // Nehrim.esm's, 0 for none
};

// Every Oblivion.esm sound OBVR plays or answers for.
inline constexpr SoundFormPair kSoundFormPairs[] = {
	{0x00088834, 0x00000925},  // WPNSwishHand
	{0x00025413, 0x00000922},  // WPNHitHand
	{0x0018B9F2, 0x00000920},  // WPNHitBluntFlesh
	{0x0018B9F1, 0x0000091D},  // WPNHitBladeFlesh
	{0x00025417, 0x00000894},  // PHYDamageBone
	{0x00097C38, 0x00000918},  // WPNBowDraw
	{0x00029BB2, 0x00000923},  // WPNStaffEquip
	{0x00029BB3, 0x00000924},  // WPNStaffUnequip
	{0x0008B095, 0x0000085F},  // ITMAmmoUp
	{0x0008B096, 0x00000861},  // ITMAmmoDown
	{0x000CBA79, 0x00000839},  // FSTDodge
	{0x000CBA7A, 0x0000083A},  // FSTDodgeBackward
	{0x0000C3CB, 0x0018D877},  // WPNBlockBladeX -> WPNBlockBlade
	{0x0000C3CC, 0x0000090C},  // WPNBlockBluntX -> WPNBlockBlunt
	{0x0000C3D0, 0x00000910},  // WPNBlockShieldLightX -> WPNBlockShieldLight
	{0x0000C3CA, 0x00000922},  // WPNHitHandX -> WPNHitHand
	{0x0000C3C4, 0x0000091C},  // WPNHitBladeX -> WPNHitBlade
	{0x0000C3C7, 0x00007630},  // WPNHitBluntX -> WPNHitBlunt
	{0x000C80ED, 0x00000000},  // NPCHumanGaspMale: none in Nehrim.esm
	{0x000C80EC, 0x00000000},  // NPCHumanGaspFemale: none
};

// The id to play for Oblivion.esm's `oblivionId` in `master`'s game. In
// Oblivion the id itself. In Nehrim the table's, 0 (nothing played) for an
// index-00 id not in the table - it would name some other record of
// Nehrim.esm; an id of a later load index (a mod's own) is left as it is.
inline UInt32 SoundFormFor(GameMaster master, UInt32 oblivionId) {
	if (master == GameMaster::Oblivion || (oblivionId >> 24) != 0) {
		return oblivionId;
	}
	for (const SoundFormPair& pair : kSoundFormPairs) {
		if (pair.oblivion == oblivionId) {
			return pair.nehrim;
		}
	}
	return 0;
}

}  // namespace obvr::game
