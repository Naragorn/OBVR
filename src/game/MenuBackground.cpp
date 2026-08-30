#include "game/MenuBackground.h"

#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

// Whether the byte has been reported since the last change, so a setting left
// alone does not write a line every frame.
bool g_reported = false;
bool g_reportedValue = false;

}  // namespace

bool SetStaticMenuBackground(bool staticBackground) {
	auto* const flag = reinterpret_cast<UInt8*>(addr::kStaticMenuBackground);
	const UInt8 wanted = staticBackground ? 1 : 0;

	// Read first. The byte is written once during start-up and read only by
	// the snapshot guard, so a frame that changes nothing should touch
	// nothing - and a value that already matches needs no page protection
	// dance either.
	if (*flag != wanted) {
		if (!mem::SafeWrite(addr::kStaticMenuBackground, &wanted, sizeof(wanted))) {
			OBVR_LOG("Menu background: the static background byte at %08X could not be "
			         "written - menus keep whatever they had",
			         addr::kStaticMenuBackground);
			return false;
		}
	}

	if (!g_reported || g_reportedValue != staticBackground) {
		g_reported = true;
		g_reportedValue = staticBackground;
		OBVR_LOG("Menu background: the engine's static menu background is %s - the world "
		         "behind a menu is %s",
		         staticBackground ? "on" : "off",
		         staticBackground ? "one snapshot, held" : "rendered live, every frame");
	}
	return true;
}

bool MenuSnapshotIsValid() {
	return *reinterpret_cast<const UInt8*>(addr::kMenuSnapshotValid) != 0;
}

}  // namespace obvr::game
