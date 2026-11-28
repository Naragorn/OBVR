#include "game/DrawSound.h"

#include "core/AroundCall.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/BowDrawSound.h"
#include "game/DrawSoundLogic.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

// The key handler 0x0051AF70: "push 9 / push edi / call 0x006B07F0 / add esp,
// 8" for its Equip kind, the same with 10 for Unequip (read in Oblivion.exe
// 1.2.416, 2026-11-28). 0x006B07F0 is cdecl (it ends in a plain ret): the
// actor, the kind; it builds "WPN" + the equipped weapon's type + "Equip" or
// "Unequip" and plays it at the actor, or returns quietly (too many such
// sounds live, the actor out of range, no weapon equipped, a type past 5).
constexpr UInt32 kEquipSoundCall = 0x0051B390;
constexpr UInt32 kUnequipSoundCall = 0x0051B39D;
constexpr UInt32 kWeaponSound = 0x006B07F0;

using WeaponSoundFn = void(__cdecl*)(void* actor, UInt32 kind);

struct Watch {
	DrawSoundState state;
	UInt32 lines = 200;
};

Watch& TheWatch() {
	static Watch s_watch;
	return s_watch;
}

void* Player() { return *reinterpret_cast<void* const*>(addr::kPlayerPointer); }

void __cdecl OnWeaponSound(void* actor, UInt32 kind) {
	if (actor != nullptr && actor == Player() &&
	    (kind == static_cast<UInt32>(DrawSoundKind::Equip) || kind == static_cast<UInt32>(DrawSoundKind::Unequip)) &&
	    !EngineSoundAllowed(TheWatch().state, static_cast<DrawSoundKind>(kind))) {
		return;  // the second of the same draw or sheathe
	}
	reinterpret_cast<WeaponSoundFn>(kWeaponSound)(actor, kind);
}

bool Reroute(UInt32 site, const char* what) {
	const UInt32 displacement = mem::CallRelativeDisplacement(site, kWeaponSound);
	const UInt8 expected[5] = {0xE8, static_cast<UInt8>(displacement & 0xFF), static_cast<UInt8>((displacement >> 8) & 0xFF),
	                           static_cast<UInt8>((displacement >> 16) & 0xFF),
	                           static_cast<UInt8>((displacement >> 24) & 0xFF)};
	if (!mem::Verify(site, expected, sizeof(expected))) {
		OBVR_LOG("Draw sound: the %s call at %08X is not `call %08X` - left alone, not counted", what, site, kWeaponSound);
		mem::ReportForeignCode("Draw sound", site);
		return false;
	}
	UInt8 patch[5];
	if (mem::BuildCallSitePatch(patch, sizeof(patch), site, reinterpret_cast<UInt32>(&OnWeaponSound)) != sizeof(patch) ||
	    !mem::SafeWrite(site, patch, sizeof(patch))) {
		OBVR_LOG("Draw sound: could not reroute the %s call at %08X", what, site);
		return false;
	}
	return true;
}

const char* OwnWords(const DrawSoundRecord& r) {
	if (!r.draw) {
		return "none for a sheathe";
	}
	if (r.ownPlayed) {
		return "played";
	}
	return r.engine > 0 ? "not needed" : "not owed";
}

}  // namespace

bool InstallWeaponKeySounds() {
	const bool bow = InstallBowDrawSound();
	const bool equip = Reroute(kEquipSoundCall, "Equip");
	const bool unequip = Reroute(kUnequipSoundCall, "Unequip");
	return bow && equip && unequip;
}

void StepDrawSoundWatch(UInt32 onHand, bool out, bool menuUp, bool staff, float dt) {
	Watch& w = TheWatch();
	const DrawSoundStep step = StepDrawSound(w.state, DrawSoundFrame{onHand, out, menuUp, staff, dt});
	if (step.playEquip) {
		void* const player = Player();
		if (player != nullptr) {
			reinterpret_cast<WeaponSoundFn>(kWeaponSound)(player, static_cast<UInt32>(DrawSoundKind::Equip));
		}
	}
	if (!step.report || w.lines == 0) {
		return;
	}
	--w.lines;
	const DrawSoundRecord& r = step.said;
	OBVR_LOG("Draw sound: %08X %s %s - the engine's sound call %u time(s) (%u kept quiet as a second), OBVR's own %s",
	         r.form, r.draw ? "drawn" : "sheathed", DrawWayWords(r.way), r.engine, r.quieted, OwnWords(r));
}

void NoteWeaponTakenToHand() { NoteTakenToHand(TheWatch().state); }

}  // namespace obvr::game
