#pragma once

// Lever 2 of docs/performance-spec.md: the second eye's pass takes the
// first pass's vertex streams instead of packing them again.
//
// What the engine does (read in Oblivion.exe 1.2.0.416, 2026-10-19 and
// 2026-10-23): before a geometry is drawn, the renderer packs its vertex
// streams into the geometry's render record through 0x00763130
// (thiscall(renderer, record, geometry, a3, desc, custom, force), six
// arguments, al = 1 on success). A geometry flagged static
// (geometry+0x2E & 0xF000 == 0x4000) with a packed record returns 1 at once;
// one flagged volatile (0x8000 - the skinned bodies, the morphing faces)
// repacks every call: every stream is copied again into one of the
// renderer's dynamic rings (the ring manager 0x007829A0 hands out a buffer,
// an offset and a lock flag: NOOVERWRITE while the request fits behind the
// ring's used mark, DISCARD otherwise). Under the dual pass that packing
// runs twice a frame, once per eye, for the same bytes - about a fifth of a
// pass with five people in view once the fast copy is in.
//
// The reuse: in the first pass every volatile pack is noted - the record,
// the frame, the buffers it locked and each buffer's discard generation. In
// the second pass a volatile pack whose record carries a note from this
// frame, whose buffers have seen no DISCARD since, returns 1 without
// packing: the record still points at the first pass's bytes, which no
// DISCARD has taken away, and the draw reads them. Anything else packs as
// the engine would. The rings are enlarged (StreamReuse.cpp, the ring
// manager's capacity raised before it creates a buffer) so that a frame's
// volume fits without a wrap; a wrap is a DISCARD, and the generation check
// refuses the skip then. A check mode packs in the second pass anyway and
// compares the written bytes with the first pass's, hash for hash.
//
// The pure half here, over plain values, so every flow runs in a test; the
// detours and the counters' log line in render/StreamReuse.cpp.

#include <cstdio>

#include "core/Types.h"

namespace obvr::render {

// Why a second-pass pack was or was not skipped.
enum class ReuseWhy : UInt8 {
	Skip,          // the first pass's streams stand: return 1
	NotVolatile,   // (unused since the type gate fell; kept so the counters' layout stands)
	Forced,        // the engine asked for a repack (the force byte)
	NotNoted,      // no first-pass note for this record
	OtherFrame,    // the note is from another frame
	Generation,    // a buffer saw a DISCARD since the note
	Overflow,      // the note could not hold every buffer, or holds none
	kCount
};

// How many DISCARD locks each dynamic buffer has seen. A DISCARD gives the
// buffer new memory for the writes that follow; everything the first pass
// wrote into it before is gone for a later reader, so a note that predates
// the DISCARD must not be trusted. A buffer the table has no room for is
// never trusted either (the table marks the overflow).
struct BufferGenerations {
	static constexpr UInt32 kSlots = 32;
	void* buffers[kSlots] = {};
	UInt32 generations[kSlots] = {};
	UInt32 count = 0;
	bool overflowed = false;

	int IndexOf(const void* buffer) const {
		for (UInt32 i = 0; i < count; ++i) {
			if (buffers[i] == buffer) return static_cast<int>(i);
		}
		return -1;
	}

	// A DISCARD on the buffer. False, and the table marked, when it is full
	// and the buffer unknown.
	bool Bump(void* buffer) {
		const int i = IndexOf(buffer);
		if (i >= 0) {
			++generations[i];
			return true;
		}
		if (count == kSlots) {
			overflowed = true;
			return false;
		}
		buffers[count] = buffer;
		generations[count] = 1;
		++count;
		return true;
	}

	// The buffer's generation: 0 for one that never saw a DISCARD (or that
	// the table could not take - which Trusted tells apart).
	UInt32 Of(const void* buffer) const {
		const int i = IndexOf(buffer);
		return i >= 0 ? generations[i] : 0;
	}

	// "0AABBCCDD:12 0EEFF0011:3 ..." - which buffers wrap, and how often.
	void Summary(char* out, UInt32 capacity) const {
		if (capacity == 0) return;
		out[0] = 0;
		UInt32 used = 0;
		for (UInt32 i = 0; i < count && used < capacity; ++i) {
			const int n = std::snprintf(out + used, capacity - used, "%s%p:%u", used ? " " : "", buffers[i], generations[i]);
			if (n < 0) break;
			used += static_cast<UInt32>(n);
		}
	}

	// Whether the buffer's generation can be trusted: known to the table, or
	// unknown while the table never overflowed (then it truly never saw a
	// DISCARD).
	bool Trusted(const void* buffer) const {
		return IndexOf(buffer) >= 0 || !overflowed;
	}
};

// What the first pass noted about one record's pack.
struct PackNote {
	static constexpr UInt32 kBuffers = 4;
	static constexpr UInt32 kHashes = 6;
	void* record = nullptr;
	UInt32 frame = 0;
	UInt32 bufferCount = 0;
	void* buffers[kBuffers] = {};
	UInt32 generations[kBuffers] = {};
	bool overflow = false;   // more buffers than fit, or none at all
	UInt32 hashCount = 0;    // the check mode's hashes of the bytes written, in lock order
	UInt64 hashes[kHashes] = {};
	bool hashOverflow = false;
	UInt32 sampleCount = 0;  // the source samples of the pack's copies, in copy order
	UInt64 samples[kHashes] = {};
	bool sampleOverflow = false;
	float firstVertex[3] = {};   // the first sampled source's first three floats, for the log

	void Reset(void* theRecord, UInt32 theFrame) {
		*this = PackNote{};
		record = theRecord;
		frame = theFrame;
	}

	// A buffer locked during the pack; once per buffer.
	void AddBuffer(void* buffer, UInt32 generation) {
		for (UInt32 i = 0; i < bufferCount; ++i) {
			if (buffers[i] == buffer) return;
		}
		if (bufferCount == kBuffers) {
			overflow = true;
			return;
		}
		buffers[bufferCount] = buffer;
		generations[bufferCount] = generation;
		++bufferCount;
	}

	void AddHash(UInt64 hash) {
		if (hashCount == kHashes) {
			hashOverflow = true;
			return;
		}
		hashes[hashCount++] = hash;
	}

	void AddSample(UInt64 sample) {
		if (sampleCount == kHashes) {
			sampleOverflow = true;
			return;
		}
		samples[sampleCount++] = sample;
	}

	// Refreshes the generations from the table: the note's buffers as they
	// stand when the pack is over (its own DISCARD included).
	void SnapshotGenerations(const BufferGenerations& gens) {
		for (UInt32 i = 0; i < bufferCount; ++i) {
			generations[i] = gens.Of(buffers[i]);
		}
	}
};

// The first pass's notes by record, an open-addressing table. Records are
// pointers the engine keeps for a geometry's lifetime; a note from an older
// frame is dead weight and is reused for a new record.
struct PackNotes {
	static constexpr UInt32 kSlots = 2048;
	static constexpr UInt32 kProbes = 64;
	PackNote notes[kSlots];

	static UInt32 Home(const void* record) {
		UInt32 h = static_cast<UInt32>(reinterpret_cast<UInt32>(record) >> 4);
		h *= 2654435761u;
		return h & (kSlots - 1);
	}

	PackNote* Find(const void* record) {
		UInt32 i = Home(record);
		for (UInt32 p = 0; p < kProbes; ++p, i = (i + 1) & (kSlots - 1)) {
			if (notes[i].record == record) return &notes[i];
			if (notes[i].record == nullptr) return nullptr;
		}
		return nullptr;
	}

	// The note for the record, fresh for the frame: the record's own slot if
	// it has one, else an empty slot, else a slot whose note is at least two
	// frames old. Null when the probe window is full of this frame's notes.
	PackNote* Put(void* record, UInt32 frame) {
		UInt32 i = Home(record);
		PackNote* stale = nullptr;
		for (UInt32 p = 0; p < kProbes; ++p, i = (i + 1) & (kSlots - 1)) {
			PackNote& n = notes[i];
			if (n.record == record || n.record == nullptr) {
				n.Reset(record, frame);
				return &n;
			}
			if (stale == nullptr && n.frame + 2 <= frame) stale = &n;
		}
		if (stale != nullptr) {
			stale->Reset(record, frame);
		}
		return stale;
	}
};

// The decision for a second-pass pack.
inline ReuseWhy DecideReuse(bool forced, const PackNote* note, UInt32 frame, const BufferGenerations& gens) {
	if (forced) return ReuseWhy::Forced;
	if (note == nullptr) return ReuseWhy::NotNoted;
	if (note->frame != frame) return ReuseWhy::OtherFrame;
	if (note->overflow || note->bufferCount == 0) return ReuseWhy::Overflow;
	for (UInt32 i = 0; i < note->bufferCount; ++i) {
		if (!gens.Trusted(note->buffers[i])) return ReuseWhy::Overflow;
		if (gens.Of(note->buffers[i]) != note->generations[i]) return ReuseWhy::Generation;
	}
	return ReuseWhy::Skip;
}

// Whether the second pass's writes hashed to the first pass's, hash for hash
// in lock order. A note or a pass that overflowed its hashes cannot be
// compared and counts as a match of nothing (false).
inline bool HashesMatch(const PackNote& first, const PackNote& second) {
	if (first.hashOverflow || second.hashOverflow) return false;
	if (first.hashCount != second.hashCount) return false;
	for (UInt32 i = 0; i < first.hashCount; ++i) {
		if (first.hashes[i] != second.hashes[i]) return false;
	}
	return true;
}

// Whether the second pass's source samples are the first pass's, sample for
// sample; an overflowed side or none at all cannot be compared (false).
inline bool SamplesMatch(const PackNote& first, const PackNote& second) {
	if (first.sampleOverflow || second.sampleOverflow) return false;
	if (first.sampleCount == 0 || first.sampleCount != second.sampleCount) return false;
	for (UInt32 i = 0; i < first.sampleCount; ++i) {
		if (first.samples[i] != second.samples[i]) return false;
	}
	return true;
}

// FNV-1a over the bytes written: enough to tell two passes' streams apart.
inline UInt64 HashBytes(const void* data, UInt32 size) {
	const auto* p = static_cast<const UInt8*>(data);
	UInt64 h = 1469598103934665603ull;
	for (UInt32 i = 0; i < size; ++i) {
		h ^= p[i];
		h *= 1099511628211ull;
	}
	return h;
}

// A hash over the ELEMENTS a stream copy wrote - count elements of
// elementBytes each, destStride apart - and nothing between them: the
// engine's staging buffer keeps whatever an earlier pack left in the gaps
// of an interleaved vertex, and a hash over the span would read that
// garbage. maxElements caps the work: sixteen elements spread over the
// run are enough to tell an eye-dependent array apart (a camera-relative
// shift moves every vertex), the check mode hashes them all.
inline UInt64 ElementHash(const void* dest, UInt32 count, UInt32 elementBytes, UInt32 destStride,
                          UInt32 maxElements) {
	const auto* base = static_cast<const UInt8*>(dest);
	UInt64 h = 1469598103934665603ull ^ count ^ (static_cast<UInt64>(elementBytes) << 32);
	if (count == 0 || elementBytes == 0) return h;
	const UInt32 taken = (maxElements == 0 || count <= maxElements) ? count : maxElements;
	for (UInt32 k = 0; k < taken; ++k) {
		const UInt32 i = taken == count ? k : static_cast<UInt32>((static_cast<UInt64>(count - 1) * k) / (taken - 1));
		const UInt8* p = base + static_cast<UInt64>(i) * destStride;
		for (UInt32 b = 0; b < elementBytes; ++b) {
			h ^= p[b];
			h *= 1099511628211ull;
		}
	}
	return h;
}
inline constexpr UInt32 kSampleElements = 16;

// The packs by the geometry's type nibble (flags+0x2E >> 12): which kinds of
// geometry the engine repacks each pass.
struct PackTypes {
	UInt32 counts[16] = {};
	void Count(UInt16 flags) { ++counts[(flags >> 12) & 0xF]; }
	void Summary(char* out, UInt32 capacity) const {
		if (capacity == 0) return;
		out[0] = 0;
		UInt32 used = 0;
		for (UInt32 t = 0; t < 16 && used < capacity; ++t) {
			if (counts[t] == 0) continue;
			const int n = std::snprintf(out + used, capacity - used, "%s%X000:%u", used ? " " : "", t, counts[t]);
			if (n < 0) break;
			used += static_cast<UInt32>(n);
		}
	}
};

// What the second pass may do with a record it could skip: verify first (pack
// and compare the source samples with the first pass's), skip, or never skip
// again. A record earns the skip after kLearnFrames matching verifications,
// keeps it while a verification every kVerifyEvery frames still matches, and
// loses it for good at the first mismatch - that is an eye-dependent array,
// whatever class it belongs to.
enum class TrustStep : UInt8 { Verify, Skip, Never };

struct RecordTrust {
	void* record = nullptr;
	UInt32 matches = 0;
	UInt32 mismatches = 0;
	UInt32 lastVerified = 0;
	UInt32 lastSeen = 0;
};

inline constexpr UInt32 kLearnFrames = 3;
inline constexpr UInt32 kVerifyEvery = 120;

inline TrustStep DecideTrust(const RecordTrust* trust, UInt32 frame) {
	if (trust == nullptr) return TrustStep::Verify;
	if (trust->mismatches != 0) return TrustStep::Never;
	if (trust->matches < kLearnFrames) return TrustStep::Verify;
	if (frame >= trust->lastVerified + kVerifyEvery) return TrustStep::Verify;
	return TrustStep::Skip;
}

// The trust by record, an open-addressing table like PackNotes; a record
// not seen for a while gives its slot up.
struct TrustNotes {
	static constexpr UInt32 kSlots = 2048;
	static constexpr UInt32 kProbes = 64;
	static constexpr UInt32 kStaleFrames = 600;
	RecordTrust notes[kSlots];

	static UInt32 Home(const void* record) {
		UInt32 h = static_cast<UInt32>(reinterpret_cast<UInt32>(record) >> 4);
		h *= 2246822519u;
		return h & (kSlots - 1);
	}

	RecordTrust* Find(const void* record) {
		UInt32 i = Home(record);
		for (UInt32 p = 0; p < kProbes; ++p, i = (i + 1) & (kSlots - 1)) {
			if (notes[i].record == record) return &notes[i];
			if (notes[i].record == nullptr) return nullptr;
		}
		return nullptr;
	}

	// The record's trust, made if missing (an empty or a long unseen slot);
	// null when the probe window is full of live records.
	RecordTrust* Get(void* record, UInt32 frame) {
		UInt32 i = Home(record);
		RecordTrust* stale = nullptr;
		for (UInt32 p = 0; p < kProbes; ++p, i = (i + 1) & (kSlots - 1)) {
			RecordTrust& t = notes[i];
			if (t.record == record) {
				t.lastSeen = frame;
				return &t;
			}
			if (t.record == nullptr) {
				t = RecordTrust{};
				t.record = record;
				t.lastSeen = frame;
				return &t;
			}
			if (stale == nullptr && t.lastSeen + kStaleFrames <= frame) stale = &t;
		}
		if (stale != nullptr) {
			*stale = RecordTrust{};
			stale->record = record;
			stale->lastSeen = frame;
		}
		return stale;
	}
};

// The ring manager's capacity, raised to the minimum when it is below it;
// zero when nothing is to change. A request larger than the minimum is left
// to the engine's own growth (it sizes to the request then).
inline UInt32 RingCapacityWanted(UInt32 capacity, UInt32 request, UInt32 minimum) {
	if (capacity >= minimum) return 0;
	if (request > minimum) return 0;
	return minimum;
}

// The check mode's verdict per geometry class - the geometry data's vtable
// tells a skinned body's NiTriShapeData from a particle system's data - so
// an eye-dependent class can be named and kept from the skip.
struct ClassVerdicts {
	static constexpr UInt32 kClasses = 16;
	UInt32 vtables[kClasses] = {};
	UInt32 compared[kClasses] = {};
	UInt32 mismatched[kClasses] = {};
	UInt32 vertices[kClasses] = {};   // one example
	UInt32 count = 0;
	bool overflow = false;

	void Note(UInt32 vtable, bool mismatch, UInt32 vertexCount) {
		for (UInt32 i = 0; i < count; ++i) {
			if (vtables[i] == vtable) {
				++compared[i];
				if (mismatch) ++mismatched[i];
				return;
			}
		}
		if (count == kClasses) {
			overflow = true;
			return;
		}
		vtables[count] = vtable;
		compared[count] = 1;
		mismatched[count] = mismatch ? 1 : 0;
		vertices[count] = vertexCount;
		++count;
	}

	// "vtable AABBCCDD: 1200 compared, 0 mismatched (e.g. 1275 vertices); ..."
	void Summary(char* out, UInt32 capacity) const {
		if (capacity == 0) return;
		out[0] = 0;
		UInt32 used = 0;
		for (UInt32 i = 0; i < count && used < capacity; ++i) {
			const int n = std::snprintf(out + used, capacity - used, "%s%08X: %u compared, %u mismatched (e.g. %u vertices)",
			                            i ? "; " : "", vtables[i], compared[i], mismatched[i], vertices[i]);
			if (n < 0) break;
			used += static_cast<UInt32>(n);
		}
		if (overflow && used < capacity) {
			std::snprintf(out + used, capacity - used, "; more classes than slots");
		}
	}
};

// What happened, for the log.
struct ReuseCounters {
	UInt32 firstPassPacks = 0;        // volatile packs noted in first passes
	UInt32 firstPassFailed = 0;       // the engine's pack returned 0
	UInt32 secondPassPacks = 0;       // volatile packs asked for in second passes
	UInt32 why[static_cast<UInt32>(ReuseWhy::kCount)] = {};
	UInt32 notesFull = 0;             // Put found no slot
	UInt32 ringsGrown = 0;
	UInt32 checkCompared = 0;
	UInt32 checkMismatch = 0;
	UInt32 checkUncomparable = 0;     // hashes overflowed on one side
	UInt32 verified = 0;              // second-pass packs done to compare the source samples
	UInt32 verifyMatched = 0;
	UInt32 verifyMismatched = 0;      // a record marked eye-dependent
	UInt32 verifyEmpty = 0;           // the second pass copied nothing (a static record the engine found packed)
	UInt32 neverSkipped = 0;          // packs of records marked eye-dependent
	UInt32 trustFull = 0;

	void Count(ReuseWhy w) { ++why[static_cast<UInt32>(w)]; }

	// One line: the second pass's packs, how many were skipped and why the
	// rest were not, the rings grown, the check's verdict.
	void Summary(char* out, UInt32 capacity) const {
		const UInt32 skipped = why[static_cast<UInt32>(ReuseWhy::Skip)];
		const double share = secondPassPacks ? 100.0 * skipped / secondPassPacks : 0.0;
		std::snprintf(out, capacity,
		              "first pass noted %u (failed %u), second pass asked %u, skipped %u (%.0f%%); not skipped: "
		              "forced %u, not noted %u, other frame %u, generation %u, overflow %u, verifying %u, "
		              "eye-dependent %u; verified %u (matched %u, mismatched %u, empty %u); notes full %u, trust full %u; rings "
		              "grown %u; check compared %u, mismatches %u, uncomparable %u",
		              firstPassPacks, firstPassFailed, secondPassPacks, skipped, share,
		              why[static_cast<UInt32>(ReuseWhy::Forced)], why[static_cast<UInt32>(ReuseWhy::NotNoted)],
		              why[static_cast<UInt32>(ReuseWhy::OtherFrame)],
		              why[static_cast<UInt32>(ReuseWhy::Generation)],
		              why[static_cast<UInt32>(ReuseWhy::Overflow)], verified, neverSkipped, verified, verifyMatched,
		              verifyMismatched, verifyEmpty, notesFull, trustFull, ringsGrown, checkCompared, checkMismatch,
		              checkUncomparable);
	}
};

}  // namespace obvr::render
