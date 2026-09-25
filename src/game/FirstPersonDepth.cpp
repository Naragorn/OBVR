#include "game/FirstPersonDepth.h"

#include "core/Log.h"
#include "core/Memory.h"

namespace obvr::game {
namespace {

// The bytes around the branch, checked once before anything is written: the
// compare before it and the clear after it (see the header).
constexpr UInt8 kBefore[] = {0x38, 0x88, 0x0C, 0x02, 0x00, 0x00};
constexpr UInt8 kAfter[] = {0x10, 0x8B, 0xC8, 0x8B, 0x11, 0x8B, 0x82, 0x3C, 0x01, 0x00, 0x00,
                            0x6A, 0x04, 0x6A, 0x00, 0xFF, 0xD0};

bool g_checked = false;
bool g_usable = false;

}  // namespace

bool KeepFirstPersonDepth(bool keepDepth) {
	if (!g_checked) {
		g_checked = true;
		g_usable = mem::Verify(kFirstPersonDepthClearBranch - sizeof(kBefore), kBefore,
		                       sizeof(kBefore)) &&
		           mem::Verify(kFirstPersonDepthClearBranch + 1, kAfter, sizeof(kAfter));
		if (!g_usable) {
			OBVR_LOG("Hands: the first-person depth clear is not where 1.2.0.416 has it - the "
			         "hands stay drawn on top");
		}
	}
	if (!g_usable) {
		return false;
	}
	const UInt8 found = *reinterpret_cast<const UInt8*>(kFirstPersonDepthClearBranch);
	const UInt8 wanted = FirstPersonDepthBranchByte(found, keepDepth);
	if (wanted == 0) {
		return false;
	}
	if (wanted == found) {
		return true;
	}
	if (!mem::SafeWrite(kFirstPersonDepthClearBranch, &wanted, 1)) {
		return false;
	}
	static UInt32 s_linesLeft = 6;
	if (s_linesLeft > 0) {
		--s_linesLeft;
		OBVR_LOG("Hands: the first-person model is now drawn %s",
		         keepDepth ? "against the world's depth (the clear before it skipped)"
		                   : "on top of the world again (vanilla's depth clear)");
	}
	return true;
}

}  // namespace obvr::game
