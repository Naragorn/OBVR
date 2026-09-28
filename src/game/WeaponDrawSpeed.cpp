#include "game/WeaponDrawSpeed.h"

#include "core/AddressSpace.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

constexpr UInt32 kProcessAnimDataOffset = 0x17C;
constexpr UInt32 kPlayerFirstPersonAnimDataOffset = 0x5CC;
constexpr UInt32 kAnimDataClockOffset = 0x94;
constexpr UInt32 kAnimDataSequencesOffset = 0xA0;
constexpr UInt32 kAnimDataSequenceCount = 5;
constexpr UInt32 kSequenceOffsetOffset = 0x48;
constexpr UInt32 kSequenceStartOffset = 0x4C;
constexpr UInt32 kSequenceEndOffset = 0x50;
constexpr UInt32 kSequenceStateOffset = 0x44;
constexpr UInt32 kSequenceAnimGroupOffset = 0x68;
constexpr UInt32 kAnimGroupCodeOffset = 0x08;
constexpr UInt32 kPlayerProcessOffset = 0x58;
// How long a sequence is followed at most, seconds of its clock: longer than
// any draw with its blends.
constexpr float kFollowSeconds = 5.0f;

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }
bool Looks(UInt32 address) { return mem::LooksLikeObjectAddress(address); }
float& FloatAt(UInt32 address) { return *reinterpret_cast<float*>(address); }

// A draw or sheathe sequence being hastened, with the anim data whose clock
// it runs on.
struct Followed {
	UInt32 sequence = 0;
	UInt32 animData = 0;
	float lastClock = 0.0f;
	float followed = 0.0f;
	UInt32 shrunkState = 0xFFFFFFFFu;  // the blend state last shrunk, once per blend
	bool seen = false;                 // in a slot this frame
};
constexpr UInt32 kMaxFollowed = 8;
Followed g_followed[kMaxFollowed];
UInt32 g_followedCount = 0;

Followed* Find(UInt32 sequence) {
	for (UInt32 i = 0; i < g_followedCount; ++i) {
		if (g_followed[i].sequence == sequence) {
			return &g_followed[i];
		}
	}
	return nullptr;
}

// The draw or sheathe sequences in an anim data's five slots: followed from
// now on if they were not already.
void Collect(UInt32 animData, WeaponDrawSpeedReport& report) {
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
		if (!IsWeaponDrawGroup(code)) {
			continue;
		}
		Followed* f = Find(sequence);
		if (f == nullptr) {
			if (g_followedCount >= kMaxFollowed) {
				continue;
			}
			f = &g_followed[g_followedCount++];
			*f = Followed{};
			f->sequence = sequence;
			f->animData = animData;
			f->lastClock = FloatAt(animData + kAnimDataClockOffset);
			++report.newlyHastened;
			report.group = code;
		}
		f->seen = true;
	}
}

}  // namespace

WeaponDrawSpeedReport StepWeaponDrawSpeed(float speed, bool active) {
	WeaponDrawSpeedReport report;
	for (UInt32 i = 0; i < g_followedCount; ++i) {
		g_followed[i].seen = false;
	}
	const UInt32 player = Read(addr::kPlayerPointer);
	const bool wanted = active && speed > 1.0f && Looks(player);
	if (wanted) {
		const UInt32 process = Read(player + kPlayerProcessOffset);
		if (Looks(process)) {
			Collect(Read(process + kProcessAnimDataOffset), report);
		}
		Collect(Read(player + kPlayerFirstPersonAnimDataOffset), report);
	}
	for (UInt32 i = 0; i < g_followedCount;) {
		Followed& f = g_followed[i];
		bool keep = wanted && Looks(f.sequence) && Looks(f.animData) && f.followed < kFollowSeconds;
		const UInt32 state = keep ? Read(f.sequence + kSequenceStateOffset) : kSequenceInactive;
		keep = keep && state != kSequenceInactive;
		if (!keep) {
			++report.released;
			f = g_followed[--g_followedCount];
			continue;
		}
		const float clock = FloatAt(f.animData + kAnimDataClockOffset);
		const float step = ClockStep(clock, f.lastClock);
		f.lastClock = clock;
		f.followed += step;
		if (state == kSequenceAnimating) {
			float& offset = FloatAt(f.sequence + kSequenceOffsetOffset);
			offset = HastenedOffset(offset, step, speed);
		} else if (IsBlendState(state) && f.shrunkState != state &&
		           ShrinkBlend(clock, FloatAt(f.sequence + kSequenceStartOffset),
		                       FloatAt(f.sequence + kSequenceEndOffset), speed)) {
			f.shrunkState = state;
			++report.blendsShortened;
		}
		++report.hastened;
		++i;
	}
	return report;
}

}  // namespace obvr::game
