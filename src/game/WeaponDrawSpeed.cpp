#include "game/WeaponDrawSpeed.h"

#include "core/AddressSpace.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

constexpr UInt32 kProcessAnimDataOffset = 0x17C;
constexpr UInt32 kPlayerFirstPersonAnimDataOffset = 0x5CC;
constexpr UInt32 kAnimDataSequencesOffset = 0xA0;
constexpr UInt32 kAnimDataSequenceCount = 5;
constexpr UInt32 kSequenceFreqOffset = 0x28;
constexpr UInt32 kSequenceAnimGroupOffset = 0x68;
constexpr UInt32 kAnimGroupCodeOffset = 0x08;
constexpr UInt32 kPlayerProcessOffset = 0x58;

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }
bool Looks(UInt32 address) { return mem::LooksLikeObjectAddress(address); }

// The sequences sped up, with their own frequency.
struct Hastened {
	UInt32 sequence = 0;
	float freq = 1.0f;
};
constexpr UInt32 kMaxHastened = 8;
Hastened g_hastened[kMaxHastened];
UInt32 g_hastenedCount = 0;

Hastened* Find(UInt32 sequence) {
	for (UInt32 i = 0; i < g_hastenedCount; ++i) {
		if (g_hastened[i].sequence == sequence) {
			return &g_hastened[i];
		}
	}
	return nullptr;
}

float& Freq(UInt32 sequence) { return *reinterpret_cast<float*>(sequence + kSequenceFreqOffset); }

// The draw or sheathe sequence among an ActorAnimData's five, 0 with none.
void CollectDrawSequences(UInt32 animData, UInt32 (&out)[2 * kAnimDataSequenceCount], UInt32& count,
                          UInt8* groupOut) {
	if (!Looks(animData)) {
		return;
	}
	for (UInt32 i = 0; i < kAnimDataSequenceCount; ++i) {
		const UInt32 sequence = Read(animData + kAnimDataSequencesOffset + 4 * i);
		if (!Looks(sequence)) {
			continue;
		}
		const UInt32 group = Read(sequence + kSequenceAnimGroupOffset);
		if (!Looks(group)) {
			continue;
		}
		const UInt8 code = *reinterpret_cast<const UInt8*>(group + kAnimGroupCodeOffset);
		if (IsWeaponDrawGroup(code) && count < 2 * kAnimDataSequenceCount) {
			out[count++] = sequence;
			*groupOut = code;
		}
	}
}

}  // namespace

WeaponDrawSpeedReport StepWeaponDrawSpeed(float speed, bool active) {
	WeaponDrawSpeedReport report;
	UInt32 found[2 * kAnimDataSequenceCount] = {};
	UInt32 count = 0;
	const UInt32 player = Read(addr::kPlayerPointer);
	const bool wanted = active && speed > 1.0f && Looks(player);
	if (wanted) {
		const UInt32 process = Read(player + kPlayerProcessOffset);
		if (Looks(process)) {
			CollectDrawSequences(Read(process + kProcessAnimDataOffset), found, count, &report.group);
		}
		CollectDrawSequences(Read(player + kPlayerFirstPersonAnimDataOffset), found, count,
		                     &report.group);
	}
	// Speed up what draws or sheathes now.
	for (UInt32 i = 0; i < count; ++i) {
		const UInt32 sequence = found[i];
		Hastened* h = Find(sequence);
		if (h == nullptr) {
			if (g_hastenedCount >= kMaxHastened) {
				continue;
			}
			h = &g_hastened[g_hastenedCount++];
			h->sequence = sequence;
			h->freq = Freq(sequence);
			++report.newlyHastened;
			report.originalFreq = h->freq;
		}
		Freq(sequence) = h->freq * speed;
		++report.hastened;
	}
	// Give back their own speed to the ones no longer in the player's hands.
	for (UInt32 i = 0; i < g_hastenedCount;) {
		bool still = false;
		for (UInt32 j = 0; j < count; ++j) {
			still = still || found[j] == g_hastened[i].sequence;
		}
		if (still) {
			++i;
			continue;
		}
		if (Looks(g_hastened[i].sequence)) {
			Freq(g_hastened[i].sequence) = g_hastened[i].freq;
		}
		++report.restored;
		g_hastened[i] = g_hastened[--g_hastenedCount];
	}
	return report;
}

}  // namespace obvr::game
