#include "game/WeaponDrawSpeed.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
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
constexpr UInt32 kSequenceFrequencyOffset = 0x28;
constexpr UInt32 kSequenceTextKeysOffset = 0x20;
constexpr UInt32 kTextKeysCountOffset = 0x0C;
constexpr UInt32 kTextKeysArrayOffset = 0x10;
constexpr UInt32 kTextKeySize = 8;
constexpr UInt32 kTextKeyTextOffset = 4;
constexpr UInt32 kMostTextKeys = 32;
constexpr UInt8 kEquipGroup = 17;
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
	bool fireKeys = false;             // its keys are fired over the span moved
	bool jumpToAttach = false;         // a taken weapon's draw: on to its Attach key (JumpNextDrawToAttach)
};
constexpr UInt32 kMaxFollowed = 8;
Followed g_followed[kMaxFollowed];
UInt32 g_followedCount = 0;
UInt32 g_attachJumpFrames = 0;  // armed by a take for this many frames
bool g_attachJumpTaken = false; // a sequence took the arming this frame
UInt32 g_attachJumpLines = 12;

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
void Collect(UInt32 animData, bool fireKeys, WeaponDrawSpeedReport& report) {
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
			f->fireKeys = fireKeys;
			if (code == kEquipGroup && g_attachJumpFrames > 0) {
				f->jumpToAttach = true;
				g_attachJumpTaken = true;
			}
			++report.newlyHastened;
			report.group = code;
		}
		f->seen = true;
	}
}

// The sequence's own time at an anim-data clock, as the anim update computes
// it for its key window (addr::kSequenceTimeAt: the offset added and scaled
// by Gamebryo, 0 when not playing).
float SequenceTime(UInt32 sequence, float clock) {
	using TimeFn = float(__fastcall*)(UInt32 self, void* edx, float clock);
	return reinterpret_cast<TimeFn>(addr::kSequenceTimeAt)(sequence, nullptr, clock);
}

// The sequence's Attach key time from its text keys (WeaponDrawSpeed.h,
// JumpNextDrawToAttach); false when they cannot be read or have none.
bool ReadAttachTime(UInt32 sequence, float& out) {
	const UInt32 keys = Read(sequence + kSequenceTextKeysOffset);
	if (!Looks(keys)) {
		return false;
	}
	const UInt32 count = Read(keys + kTextKeysCountOffset);
	const UInt32 array = Read(keys + kTextKeysArrayOffset);
	if (count == 0 || count > kMostTextKeys || !Looks(array)) {
		return false;
	}
	float times[kMostTextKeys];
	const char* texts[kMostTextKeys];
	for (UInt32 i = 0; i < count; ++i) {
		const UInt32 key = array + i * kTextKeySize;
		times[i] = FloatAt(key);
		const UInt32 text = Read(key + kTextKeyTextOffset);
		texts[i] = Looks(text) ? reinterpret_cast<const char*>(text) : nullptr;
	}
	return AttachKeyTime(times, texts, count, out);
}

// TESAnimGroup's key handler (addr::kAnimGroupHandleKeys, thiscall on the
// group, ret 10h): (actor, from, to, sequence) - the keys after `from` up to
// `to`; the Equip and Unequip sounds are its kinds 9 and 10 (0x0051B38D,
// 0x0051B39A), the weapon's attach is not among them.

void FireKeys(const Followed& f, float from, float to) {
	if (!(to > from) || !f.fireKeys) {
		return;
	}
	const UInt32 player = Read(addr::kPlayerPointer);
	const UInt32 group = Read(f.sequence + kSequenceAnimGroupOffset);
	if (!Looks(player) || !Looks(group)) {
		return;
	}
	using KeysFn = void(__fastcall*)(UInt32 self, void* edx, UInt32 actor, float from, float to, UInt32 sequence);
	reinterpret_cast<KeysFn>(addr::kAnimGroupHandleKeys)(group, nullptr, player, from, to, f.sequence);
	// Which spans were fired, per draw: whether a sequence's sound key fell in
	// one (the tester, 2026-09-30: the bow and the staff are drawn silently).
	static UInt32 s_lines = 16;
	if (s_lines > 0) {
		--s_lines;
		OBVR_LOG("Draw speed: keys fired from %.3f to %.3f s (group %u, sequence %08X)", static_cast<double>(from),
		         static_cast<double>(to), static_cast<unsigned>(*reinterpret_cast<const UInt8*>(group + kAnimGroupCodeOffset)),
		         f.sequence);
	}
}

}  // namespace

void JumpNextDrawToAttach() { g_attachJumpFrames = kAttachJumpArmedFrames; }

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
			Collect(Read(process + kProcessAnimDataOffset), false, report);
		}
		Collect(Read(player + kPlayerFirstPersonAnimDataOffset), true, report);
	}
	// The arming goes to the draw that starts while it stands - both its
	// sequences, the first- and the third-person one, start in one frame.
	if (g_attachJumpTaken || !wanted) {
		g_attachJumpFrames = 0;
		g_attachJumpTaken = false;
	} else if (g_attachJumpFrames > 0) {
		--g_attachJumpFrames;
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
			const float before = SequenceTime(f.sequence, clock);
			offset = HastenedOffset(offset, step, speed);
			if (f.jumpToAttach) {
				f.jumpToAttach = false;
				float attach = 0.0f;
				const float now = SequenceTime(f.sequence, clock);
				const bool read = ReadAttachTime(f.sequence, attach);
				if (read) {
					offset = AttachJumpOffset(offset, now, attach, FloatAt(f.sequence + kSequenceFrequencyOffset));
				}
				if (g_attachJumpLines > 0) {
					--g_attachJumpLines;
					if (read) {
						OBVR_LOG("Draw speed: a taken weapon's draw moved on to its Attach at %.3f s (it was at %.3f; "
						         "sequence %08X, %s person)",
						         static_cast<double>(attach), static_cast<double>(now), f.sequence,
						         f.fireKeys ? "first" : "third");
					} else {
						OBVR_LOG("Draw speed: a taken weapon's draw has no Attach key to read (sequence %08X) - it "
						         "plays on at the draw speed",
						         f.sequence);
					}
				}
			}
			// The keys in the span moved over (the draw's sound), through the
			// engine's own handler.
			FireKeys(f, before, SequenceTime(f.sequence, clock));
		} else if (IsBlendState(state) && f.shrunkState != state &&
		           ShrinkBlend(clock, FloatAt(f.sequence + kSequenceStartOffset),
		                       FloatAt(f.sequence + kSequenceEndOffset),
		                       f.jumpToAttach ? kAttachJumpBlendSpeed : speed)) {
			f.shrunkState = state;
			++report.blendsShortened;
		}
		++report.hastened;
		++i;
	}
	return report;
}

}  // namespace obvr::game
