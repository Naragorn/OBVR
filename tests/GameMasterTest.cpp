// Checks the game master's sound ids (game/GameMasterLogic.h): the master
// told by its file name, the ids as they stand in Oblivion, Nehrim's for
// each of Oblivion.esm's, none for the gasps, 0 for an index-00 id OBVR has
// no pair for, a mod's own id left alone - and that every sound id OBVR
// names has a pair.

#include <cstdio>

#include "game/DismemberLogic.h"
#include "game/ExhaustionLogic.h"
#include "game/GameMasterLogic.h"
#include "game/KickLogic.h"
#include "game/ParryLogic.h"
#include "vr/Archery.h"
#include "vr/Holster.h"
#include "vr/TeleportSound.h"

namespace {

using namespace obvr;
using namespace obvr::game;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

void TestMasterFromFileName() {
	std::printf("MasterFromFileName\n");
	Check(MasterFromFileName("Nehrim.esm") == GameMaster::Nehrim, "Nehrim.esm is Nehrim");
	Check(MasterFromFileName("NEHRIM.ESM") == GameMaster::Nehrim, "without case");
	Check(MasterFromFileName("Oblivion.esm") == GameMaster::Oblivion, "Oblivion.esm is Oblivion");
	Check(MasterFromFileName("Nehrim.esm.bak") == GameMaster::Oblivion, "a longer name is not Nehrim");
	Check(MasterFromFileName("Nehrim") == GameMaster::Oblivion, "a shorter name is not Nehrim");
	Check(MasterFromFileName("") == GameMaster::Oblivion, "an empty name is Oblivion");
	Check(MasterFromFileName(nullptr) == GameMaster::Oblivion, "no name is Oblivion");
}

void TestSoundFormFor() {
	std::printf("SoundFormFor\n");
	Check(SoundFormFor(GameMaster::Oblivion, kSoundCutFlesh) == kSoundCutFlesh, "Oblivion keeps the id");
	Check(SoundFormFor(GameMaster::Oblivion, 0x00123456) == 0x00123456, "Oblivion keeps an unpaired id");
	Check(SoundFormFor(GameMaster::Nehrim, kSoundCutFlesh) == 0x0000091D, "Nehrim: WPNHitBladeFlesh 091D");
	Check(SoundFormFor(GameMaster::Nehrim, kSoundCutBone) == 0x00000894, "Nehrim: PHYDamageBone 0894");
	Check(SoundFormFor(GameMaster::Nehrim, kParrySoundBlade) == 0x0018D877, "Nehrim: the X sound's folder stands in");
	Check(SoundFormFor(GameMaster::Nehrim, kSoundFormGaspMale) == 0, "Nehrim: no gasp");
	Check(SoundFormFor(GameMaster::Nehrim, kSoundFormGaspFemale) == 0, "Nehrim: no gasp (female)");
	Check(SoundFormFor(GameMaster::Nehrim, 0x00123456) == 0, "Nehrim: an unpaired index-00 id plays nothing");
	Check(SoundFormFor(GameMaster::Nehrim, 0x05000ADA) == 0x05000ADA, "Nehrim: a mod's own id is left alone");
	Check(SoundFormFor(GameMaster::Nehrim, 0) == 0, "Nehrim: 0 stays 0");
}

void TestEveryNamedSoundPaired() {
	std::printf("every sound OBVR names has a pair\n");
	const UInt32 named[] = {
		kSoundSwishHand, kSoundHitHand, kSoundHitBluntFlesh, kSoundCutFlesh, kSoundCutBone,
		kParrySoundBlade, kParrySoundBlunt, kParrySoundShield, kSoundFormGaspMale, kSoundFormGaspFemale,
		vr::kSoundFormAmmoUp, vr::kSoundFormAmmoDown, vr::kStaffEquipSound, vr::kStaffUnequipSound,
		vr::kSoundFormDodge, vr::kSoundFormDodgeBackward, vr::kSoundFormSwishHand,
		0x00097C38,  // WPNBowDraw (game/BowDrawSound.cpp)
		0x0000C3CA,  // WPNHitHandX (camera/CameraHook.cpp)
		0x0000C3C4,  // WPNHitBladeX (camera/CameraHook.cpp)
		0x0000C3C7,  // WPNHitBluntX (camera/CameraHook.cpp)
	};
	for (const UInt32 id : named) {
		bool paired = false;
		for (const SoundFormPair& pair : kSoundFormPairs) {
			paired = paired || pair.oblivion == id;
		}
		char what[64];
		std::snprintf(what, sizeof(what), "%08X has a pair", static_cast<unsigned>(id));
		Check(paired, what);
	}
	bool distinct = true;
	for (const SoundFormPair& a : kSoundFormPairs) {
		UInt32 seen = 0;
		for (const SoundFormPair& b : kSoundFormPairs) {
			seen += a.oblivion == b.oblivion ? 1 : 0;
		}
		distinct = distinct && seen == 1;
	}
	Check(distinct, "each Oblivion id is paired once");
}

}  // namespace

int main() {
	TestMasterFromFileName();
	TestSoundFormFor();
	TestEveryNamedSoundPaired();
	if (g_failures != 0) {
		std::printf("%d FAILED\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
