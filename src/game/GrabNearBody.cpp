#include "game/GrabNearBody.h"

#include "core/Log.h"
#include "core/Memory.h"
#include "game/FirstPersonDepth.h"

namespace obvr::game {
namespace {

constexpr UInt8 kBefore[] = {0xD8, 0xD1, 0xDF, 0xE0, 0xF6, 0xC4, 0x41};
constexpr UInt8 kAfter[] = {0x67, 0xDE, 0xE1};

bool g_checked = false;
bool g_usable = false;

}  // namespace

bool AllowGrabNearBody(bool allow) {
	if (!g_checked) {
		g_checked = true;
		g_usable = mem::Verify(kGrabNearBodyBranch - sizeof(kBefore), kBefore, sizeof(kBefore)) &&
		           mem::Verify(kGrabNearBodyBranch + 1, kAfter, sizeof(kAfter));
		if (!g_usable) {
			OBVR_LOG("Hands: the grab's keep-out round the player is not where 1.2.0.416 has "
			         "it - held objects stay a hand's width from the body");
		}
	}
	if (!g_usable) {
		return false;
	}
	const UInt8 found = *reinterpret_cast<const UInt8*>(kGrabNearBodyBranch);
	// The same two forms as the depth clear's branch: jne short, jmp short.
	const UInt8 wanted = FirstPersonDepthBranchByte(found, allow);
	if (wanted == 0) {
		return false;
	}
	if (wanted == found) {
		return true;
	}
	if (!mem::SafeWrite(kGrabNearBodyBranch, &wanted, 1)) {
		return false;
	}
	static UInt32 s_linesLeft = 6;
	if (s_linesLeft > 0) {
		--s_linesLeft;
		OBVR_LOG("Hands: %s", allow ? "a held object may come right up to the body and the head"
		                            : "held objects are kept off the body again (vanilla)");
	}
	return true;
}

}  // namespace obvr::game
