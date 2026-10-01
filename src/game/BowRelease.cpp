#include "game/BowRelease.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

constexpr UInt32 kPlayerProcessOffset = 0x58;
constexpr UInt32 kProcessAnimDataOffset = 0x17C;
constexpr UInt32 kPlayerFirstPersonAnimDataOffset = 0x5CC;
constexpr UInt32 kAnimDataClockOffset = 0x94;
constexpr UInt32 kAnimDataSlot3Sequence = 0xA0 + 3 * 4;
constexpr UInt32 kAnimDataSlot3KeyCount = 0x48 + 3 * 4;
constexpr UInt32 kSequenceOffsetOffset = 0x48;
constexpr UInt32 kSequenceAnimGroupOffset = 0x68;
constexpr UInt32 kAnimGroupCodeOffset = 0x08;
constexpr UInt32 kGroupKeyTime = 0x0051AE20;
// The AttackBow group's code (the bow's draw, 0xB102E0's entry 0x13).
constexpr UInt8 kGroupAttackBow = 0x13;
constexpr UInt32 kKeyAttach = 1;
constexpr UInt32 kKeyHold = 2;

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }
bool Looks(UInt32 address) { return mem::LooksLikeObjectAddress(address); }

enum class Snap : UInt8 { Unreadable, Done, NotNeeded };

Snap SnapOne(UInt32 animData, const char* which) {
	if (!Looks(animData)) {
		return Snap::Unreadable;
	}
	const UInt32 sequence = Read(animData + kAnimDataSlot3Sequence);
	const UInt32 group = Looks(sequence) ? Read(sequence + kSequenceAnimGroupOffset) : 0;
	if (!Looks(group) || *reinterpret_cast<const UInt8*>(group + kAnimGroupCodeOffset) != kGroupAttackBow) {
		return Snap::Unreadable;
	}
	// Only with the Attach passed and the Hold not yet: the engine steps the
	// rest itself (Hold, then Release).
	if (Read(animData + kAnimDataSlot3KeyCount) != kKeyAttach) {
		return Snap::NotNeeded;
	}
	using KeyTimeFn = float(__thiscall*)(UInt32 self, UInt32 index);
	const float hold = reinterpret_cast<KeyTimeFn>(kGroupKeyTime)(group, kKeyHold);
	const float clock = *reinterpret_cast<const float*>(animData + kAnimDataClockOffset);
	float& offset = *reinterpret_cast<float*>(sequence + kSequenceOffsetOffset);
	const float before = offset + clock;
	if (!(hold > before)) {
		return Snap::NotNeeded;
	}
	offset = hold - clock;
	static UInt32 s_lines = 12;
	if (s_lines > 0) {
		--s_lines;
		OBVR_LOG("Bow release: the %s draw put at its Hold (%.3f s, it was at %.3f s) - the loose comes now", which,
		         static_cast<double>(hold), static_cast<double>(before));
	}
	return Snap::Done;
}

}  // namespace

bool SnapBowDrawToHold() {
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!Looks(player)) {
		return false;
	}
	const UInt32 process = Read(player + kPlayerProcessOffset);
	const Snap first = SnapOne(Read(player + kPlayerFirstPersonAnimDataOffset), "first-person");
	const Snap third = Looks(process) ? SnapOne(Read(process + kProcessAnimDataOffset), "third-person")
	                                  : Snap::Unreadable;
	return first != Snap::Unreadable || third != Snap::Unreadable;
}

}  // namespace obvr::game
