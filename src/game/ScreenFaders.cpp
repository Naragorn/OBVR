#include "game/ScreenFaders.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

constexpr UInt16 kAppCulledBit = 0x0001;

// The nodes this culled, by fader, so only those are given back.
UInt32 g_culled[addr::kScreenFaderCount] = {};
UInt32 g_cullLines = 8;

UInt8* Record(UInt32 index) {
	return reinterpret_cast<UInt8*>(addr::kScreenFaders + index * addr::kScreenFaderStride);
}

UInt32 NodeOf(UInt32 index) {
	return *reinterpret_cast<const UInt32*>(Record(index) + addr::kScreenFaderNodeOffset);
}

UInt16& FlagsOf(UInt32 node) { return *reinterpret_cast<UInt16*>(node + addr::kNiFlagsOffset); }

}  // namespace

bool ReadScreenFaderOpacities(float (&out)[3]) {
	static_assert(addr::kScreenFaderCount == 3, "one opacity per fader");
	for (UInt32 i = 0; i < addr::kScreenFaderCount; ++i) {
		const UInt32 node = NodeOf(i);
		out[i] = mem::LooksLikeObjectAddress(node)
		             ? *reinterpret_cast<const float*>(Record(i) + addr::kScreenFaderOpacityOffset)
		             : 0.0f;
	}
	return true;
}

void CullScreenFaders(bool cull) {
	for (UInt32 i = 0; i < addr::kScreenFaderCount; ++i) {
		const UInt32 node = NodeOf(i);
		const bool there = mem::LooksLikeObjectAddress(node);
		if (g_culled[i] != 0 && g_culled[i] != node) {
			// Detached and freed by the engine (or replaced): not ours to write.
			g_culled[i] = 0;
		}
		if (!there) {
			continue;
		}
		UInt16& flags = FlagsOf(node);
		if (cull && g_culled[i] == 0 && (flags & kAppCulledBit) == 0) {
			flags = static_cast<UInt16>(flags | kAppCulledBit);
			g_culled[i] = node;
			if (g_cullLines > 0) {
				--g_cullLines;
				OBVR_LOG("Screen fade: fader %u (node %08X) kept out of the 2D pass - the view fades instead", i,
				         node);
			}
		} else if (!cull && g_culled[i] == node) {
			flags = static_cast<UInt16>(flags & ~kAppCulledBit);
			g_culled[i] = 0;
		}
	}
}

}  // namespace obvr::game
