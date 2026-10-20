#include "game/PlayerAnimProbe.h"

#include <cstdio>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Types.h"
#include "game/GameAddresses.h"

namespace obvr::game {

namespace {

// The layouts game/CombatRing.cpp and game/WeaponDrawSpeed.h read by (xOBSE
// GameProcess.h, GameObjects.h, NiNodes.h).
constexpr UInt32 kPlayerProcessOffset = 0x58;
constexpr UInt32 kProcessAnimDataOffset = 0x17C;
constexpr UInt32 kPlayerFirstPersonAnimDataOffset = 0x5CC;
constexpr UInt32 kAnimDataSequencesOffset = 0xA0;
constexpr UInt32 kAnimDataSequenceCount = 5;
constexpr UInt32 kSequenceStateOffset = 0x44;
constexpr UInt32 kSequenceAnimGroupOffset = 0x68;
constexpr UInt32 kAnimGroupCodeOffset = 0x08;
// Enhanced Camera's kIsIdlePlayingCall: thiscall on the animation data, the
// answer in al; a special idle plays when its low bit is clear.
constexpr UInt32 kIsIdlePlaying = 0x00472EA0;

UInt32 Read(UInt32 at) { return *reinterpret_cast<const UInt32*>(at); }
bool Looks(UInt32 at) { return mem::LooksLikeObjectAddress(at); }

void Describe(const char* which, UInt32 animData, char* out, size_t size) {
	if (!Looks(animData)) {
		std::snprintf(out, size, "%s anim data unreadable (%08X)", which, animData);
		return;
	}
	char slots[96] = "";
	size_t at = 0;
	for (UInt32 i = 0; i < kAnimDataSequenceCount && at < sizeof(slots) - 16; ++i) {
		const UInt32 sequence = Read(animData + kAnimDataSequencesOffset + 4 * i);
		if (!Looks(sequence)) {
			at += static_cast<size_t>(std::snprintf(slots + at, sizeof(slots) - at, "%u:- ", i));
			continue;
		}
		const UInt32 state = Read(sequence + kSequenceStateOffset);
		const UInt32 group = Read(sequence + kSequenceAnimGroupOffset);
		const UInt32 code = Looks(group) ? *reinterpret_cast<const UInt8*>(group + kAnimGroupCodeOffset) : 0xFFu;
		at += static_cast<size_t>(std::snprintf(slots + at, sizeof(slots) - at, "%u:%u/%02X ", i, state, code));
	}
	using IdleFn = UInt8(__fastcall*)(UInt32 self, UInt32 edx);
	const UInt8 answer = reinterpret_cast<IdleFn>(kIsIdlePlaying)(animData, 0);
	std::snprintf(out, size, "%s slots (state/group) %s- the engine's idle answer %u (%s)", which, slots, answer,
	              (answer & 1u) == 0 ? "a special idle plays" : "no special idle");
}

}  // namespace

void LogPlayerAnimationAtMark() {
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!Looks(player)) {
		return;
	}
	const UInt32 process = Read(player + kPlayerProcessOffset);
	const UInt32 third = Looks(process) ? Read(process + kProcessAnimDataOffset) : 0;
	const UInt32 first = Read(player + kPlayerFirstPersonAnimDataOffset);
	char a[192];
	char b[192];
	Describe("third-person", third, a, sizeof(a));
	Describe("first-person", first, b, sizeof(b));
	OBVR_LOG("Hands: at the mark - the player's animation: %s; %s", a, b);
}

}  // namespace obvr::game
