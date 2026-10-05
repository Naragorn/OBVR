#include "game/EquipWhileActing.h"

#include "core/Log.h"
#include "core/Memory.h"

namespace obvr::game {
namespace {

bool g_siteOk[4] = {false, false, false, false};
bool g_installed = false;
int g_open = -1;  // -1: not yet written

}  // namespace

void InstallEquipWhileActing() {
	static const UInt8 kCompare[3] = {0x83, 0xF8, 0xFF};
	UInt32 ok = 0;
	for (UInt32 i = 0; i < 4; ++i) {
		const UInt32 site = kEquipGateSites[i];
		const UInt8 held = *reinterpret_cast<const UInt8*>(site);
		g_siteOk[i] = mem::Verify(site - 3, kCompare, sizeof(kCompare)) &&
		              (held == kEquipGateJe || held == kEquipGateJmp);
		if (!g_siteOk[i]) {
			OBVR_LOG("Equip: the action gate at %08X is not `cmp eax,-1; je` - left to the game", site);
			mem::ReportForeignCode("Equip while acting", site);
		} else {
			++ok;
		}
	}
	g_installed = true;
	OBVR_LOG("Equip: %u of 4 action gates found (inventory equip, hotkey, unequip, drop)", ok);
}

void SetEquipWhileActing(bool open) {
	if (!g_installed || g_open == (open ? 1 : 0)) {
		return;
	}
	const UInt8 byte = EquipGateByte(open);
	for (UInt32 i = 0; i < 4; ++i) {
		if (g_siteOk[i]) {
			mem::SafeWrite(kEquipGateSites[i], &byte, 1);
		}
	}
	g_open = open ? 1 : 0;
	OBVR_LOG("Equip: %s", open ? "weapons and armour change whatever the action (Full VR)"
	                           : "the game's action gate back - no changes while attacking");
}

}  // namespace obvr::game
