#pragma once

#include "core/Types.h"

namespace obvr::render {

// The open mappings of the dynamic pool: which buffer is locked right now,
// where its mapped range starts and how long it is. Lock opens an entry,
// Unlock closes it and hands the range back, so the bytes the game wrote
// can be fingerprinted in the moment before they leave the CPU. Bounded
// like the pointer set is: a handful of buffers are ever mapped at once,
// and when that assumption breaks the ledger refuses instead of growing,
// leaving a skip for the caller to count.
struct LockLedger {
	static constexpr UInt32 kSlots = 8;

	struct Entry {
		void* buffer = nullptr;
		const void* data = nullptr;
		UInt32 size = 0;
	};

	Entry entries[kSlots];

	// Opens the entry for buffer, replacing an entry the same buffer left
	// behind - a re-lock without an unlock supersedes the old range. False
	// when every slot belongs to some other buffer; the unlock will then
	// not find the range, and the caller counts the skip.
	bool Begin(void* buffer, const void* data, UInt32 size) {
		Entry* slot = nullptr;
		for (UInt32 i = 0; i < kSlots; ++i) {
			if (entries[i].buffer == buffer) {
				slot = &entries[i];
				break;
			}
			if (slot == nullptr && entries[i].buffer == nullptr) {
				slot = &entries[i];
			}
		}
		if (slot == nullptr) {
			return false;
		}
		slot->buffer = buffer;
		slot->data = data;
		slot->size = size;
		return true;
	}

	// Closes the entry for buffer and hands its range back; the slot is
	// free again afterwards. False when the buffer has no open entry.
	bool End(void* buffer, const void** data, UInt32* size) {
		for (UInt32 i = 0; i < kSlots; ++i) {
			if (entries[i].buffer == buffer) {
				*data = entries[i].data;
				*size = entries[i].size;
				entries[i] = Entry{};
				return true;
			}
		}
		return false;
	}
};

// Sums the first min(size, cap) bytes of a written range. A window rather
// than the whole range because the memory under it is write-combined and
// slow to read back; collapsed vertices corrupt the very first positions,
// so a short window catches them at a cost no frame can feel.
inline UInt32 SumLeadingBytes(const void* data, UInt32 size, UInt32 cap) {
	const unsigned char* bytes = static_cast<const unsigned char*>(data);
	const UInt32 limit = size < cap ? size : cap;
	UInt32 sum = 0;
	for (UInt32 i = 0; i < limit; ++i) {
		sum += bytes[i];
	}
	return sum;
}

}  // namespace obvr::render
