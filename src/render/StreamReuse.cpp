#include "render/StreamReuse.h"

#include "core/Config.h"
#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "render/SceneRenderHook.h"
#include "render/StreamCopy.h"
#include "render/StreamReuseLogic.h"

namespace obvr::render {
namespace {

// The stream packer: thiscall(renderer, record, geometry, a3, desc, custom,
// force byte), ret 18h, al = 1 on success. Its first five bytes in 1.2.0.416
// - sub esp,1Ch; push ebx; push esi - are whole instructions with nothing
// relative in them.
constexpr UInt32 kPackStreams = 0x00763130;
constexpr UInt8 kPackBytes[] = {0x83, 0xEC, 0x1C, 0x53, 0x56};
// The geometry's flags: the type in the top four bits (0x4000 static, 0x8000
// volatile), the dirty bits below.
constexpr UInt32 kGeometryFlagsOffset = 0x2E;

// The ring manager: thiscall(ring, bytes, outOffset*, outFlags*, force
// discard byte, usage byte), ret 14h, the buffer in eax. Its first eight
// bytes - push ebx; mov ebx,[esp+8]; push esi; mov esi,ecx - likewise.
constexpr UInt32 kRingTake = 0x007829A0;
constexpr UInt8 kRingBytes[] = {0x53, 0x8B, 0x5C, 0x24, 0x08, 0x56, 0x8B, 0xF1};

// The geometry data classes by their vtables (Oblivion.exe 1.2.0.416, read
// off the RTTI 2026-10-23), for the log.
struct GeometryClass {
	UInt32 vtable;
	const char* name;
};
constexpr GeometryClass kGeometryClasses[] = {
	{0x00A7F5A4, "NiTriShapeData"},       {0x00A7F32C, "NiTriStripsData"},  {0x00A80274, "NiTriShapeDynamicData"},
	{0x00A816C4, "NiParticlesData"},      {0x00A877F4, "NiPSysData"},       {0x00A80A44, "NiLinesData"},
	{0x00A7D94C, "NiScreenElementsData"}, {0x00A800BC, "NiGeometryData"},
};

const char* ClassName(UInt32 vtable) {
	for (const GeometryClass& c : kGeometryClasses) {
		if (c.vtable == vtable) return c.name;
	}
	return "unknown";
}

// The ring as 0x007829A0 reads it.
struct Ring {
	UInt32 unknown0;
	UInt32 fvf;        // +0x04, handed to CreateVertexBuffer
	UInt32 capacity;   // +0x08, bytes of the buffer
	void* device;      // +0x0C
	void* buffer;      // +0x10, IDirect3DVertexBuffer9*
	UInt32 used;       // +0x14, the mark NOOVERWRITE requests take from
};
static_assert(sizeof(Ring) == 0x18, "the ring manager's object");

// __fastcall with a dummy edx mirrors __thiscall's register use; the stack
// arguments travel and are cleaned up the same way.
using PackFn = UInt8(__fastcall*)(void* renderer, void* edx, void* record, void* geometry, void* a3,
                                  void* desc, void* custom, UInt32 force);
using RingFn = void*(__fastcall*)(Ring* ring, void* edx, UInt32 bytes, UInt32* outOffset, UInt32* outFlags,
                                  UInt32 forceDiscard, UInt32 usage);
using ReleaseFn = UInt32(__stdcall*)(void* self);

PackFn g_originalPack = nullptr;
RingFn g_originalRing = nullptr;

bool g_on = false;
bool g_check = false;
UInt32 g_ringMinimum = 0;

BufferGenerations g_generations;
PackNotes g_notes;
TrustNotes g_trust;
ReuseCounters g_counters;
ClassVerdicts g_verdicts;
PackTypes g_types;

// The pack in progress: the buffers it locks and the hashes of what its
// copies write (sampled; every element in check mode).
bool g_packing = false;
PackNote g_current;

UInt32 g_discardsByPass[2] = {};
UInt32 g_lastReportFrame = 0;
UInt32 g_mismatchLinesLeft = 8;
UInt32 g_eyeLinesLeft = 8;
UInt32 g_ringLinesLeft = 12;
constexpr UInt32 kReportEveryFrames = 600;

UInt16 GeometryFlags(const void* geometry) {
	return *reinterpret_cast<const UInt16*>(static_cast<const UInt8*>(geometry) + kGeometryFlagsOffset);
}

void MaybeReport(UInt32 frame) {
	if (frame < g_lastReportFrame + kReportEveryFrames) return;
	g_lastReportFrame = frame;
	char line[768];
	g_counters.Summary(line, sizeof(line));
	OBVR_LOG("Stream reuse at scene call %u: %s", frame, line);
	char types[256];
	g_types.Summary(types, sizeof(types));
	char buffers[512];
	g_generations.Summary(buffers, sizeof(buffers));
	UInt32 c0i = 0, c0o = 0, c1i = 0, c1o = 0;
	UInt64 b0i = 0, b0o = 0, b1i = 0, b1o = 0;
	StreamCopyStats(0, true, &c0i, &b0i);
	StreamCopyStats(0, false, &c0o, &b0o);
	StreamCopyStats(1, true, &c1i, &b1i);
	StreamCopyStats(1, false, &c1o, &b1o);
	OBVR_LOG("Stream reuse noted packs by geometry type %s; buffer DISCARDs %s; stream copies first pass inside the "
	         "packer %u (%llu KB) outside it %u (%llu KB), second pass inside %u (%llu KB) outside %u (%llu KB); "
	         "DISCARDs in the first pass %u, in the second %u",
	         types, buffers, c0i, b0i >> 10, c0o, b0o >> 10, c1i, b1i >> 10, c1o, b1o >> 10, g_discardsByPass[0],
	         g_discardsByPass[1]);
	if (g_check && g_verdicts.count != 0) {
		char classes[1024];
		g_verdicts.Summary(classes, sizeof(classes));
		OBVR_LOG("Stream reuse classes: %s", classes);
	}
}

UInt8 PackAndNote(void* renderer, void* record, void* geometry, void* a3, void* desc, void* custom, UInt32 force,
                  UInt32 frame) {
	g_current.Reset(record, frame);
	g_packing = true;
	const UInt8 made = g_originalPack(renderer, nullptr, record, geometry, a3, desc, custom, force);
	g_packing = false;
	if (made != 0) g_current.SnapshotGenerations(g_generations);
	return made;
}

UInt8 __fastcall PackStreamsHook(void* renderer, void* /*edx*/, void* record, void* geometry, void* a3,
                                 void* desc, void* custom, UInt32 force) {
	const int pass = CurrentScenePassIndex();
	if (!g_on || pass < 0 || record == nullptr || geometry == nullptr) {
		return g_originalPack(renderer, nullptr, record, geometry, a3, desc, custom, force);
	}
	const UInt32 frame = CurrentSceneCall();
	const bool forced = (force & 0xFF) != 0;

	if (pass == 0) {
		// The first pass: pack, and note what the pack touched. A pack that
		// locked nothing (a static record the engine found packed) leaves
		// no note - there is nothing to reuse and nothing to skip.
		const UInt8 made = PackAndNote(renderer, record, geometry, a3, desc, custom, force, frame);
		if (made == 0) {
			++g_counters.firstPassFailed;
			return made;
		}
		if (g_current.bufferCount == 0) return made;
		++g_counters.firstPassPacks;
		g_types.Count(GeometryFlags(geometry));
		PackNote* note = g_notes.Put(record, frame);
		if (note == nullptr) {
			++g_counters.notesFull;
		} else {
			*note = g_current;
		}
		MaybeReport(frame);
		return made;
	}

	// The second pass. Only a record the first pass noted this frame is a
	// candidate; everything else packs as the engine would.
	PackNote* note = g_notes.Find(record);
	if (note == nullptr || note->frame != frame) {
		return g_originalPack(renderer, nullptr, record, geometry, a3, desc, custom, force);
	}
	++g_counters.secondPassPacks;
	const ReuseWhy why = DecideReuse(forced, note, frame, g_generations);
	if (why != ReuseWhy::Skip) {
		g_counters.Count(why);
		return g_originalPack(renderer, nullptr, record, geometry, a3, desc, custom, force);
	}
	// The buffers allow the skip; the record's own history decides whether
	// its bytes are the same in both passes.
	RecordTrust* trust = g_trust.Get(record, frame);
	if (trust == nullptr) {
		++g_counters.trustFull;
		return g_originalPack(renderer, nullptr, record, geometry, a3, desc, custom, force);
	}
	const TrustStep step = DecideTrust(trust, frame);
	if (step == TrustStep::Never) {
		++g_counters.neverSkipped;
		return g_originalPack(renderer, nullptr, record, geometry, a3, desc, custom, force);
	}
	const UInt32 vtable = *static_cast<const UInt32*>(geometry);
	if (step == TrustStep::Skip && !g_check) {
		g_counters.Count(ReuseWhy::Skip);
		return 1;
	}
	// Verify, or the check mode's compare: pack, and compare the copies'
	// hashes with the first pass's.
	const UInt8 made = PackAndNote(renderer, record, geometry, a3, desc, custom, force, frame);
	if (made == 0) return made;
	const bool match = SamplesMatch(*note, g_current);
	const auto* rec = static_cast<const UInt8*>(record);
	const UInt32 vertices = *reinterpret_cast<const UInt32*>(rec + 0x14);
	const UInt32 streams = *reinterpret_cast<const UInt32*>(rec + 0x1C);
	if (step == TrustStep::Verify) {
		++g_counters.verified;
		if (g_current.sampleCount == 0) {
			// The engine packed nothing (a static record it found packed): no
			// bytes to compare, nothing to learn, nothing to skip.
			++g_counters.verifyEmpty;
			return made;
		}
		if (match) {
			++trust->matches;
			trust->lastVerified = frame;
			++g_counters.verifyMatched;
		} else {
			++trust->mismatches;
			++g_counters.verifyMismatched;
			if (g_eyeLinesLeft != 0) {
				--g_eyeLinesLeft;
				OBVR_LOG("Stream reuse: eye-dependent record %p at scene call %u - class %s (%08X), flags %04X, "
				         "vertices %u, streams %u, hashes %u/%u, first vertex first pass (%.3f %.3f %.3f) second "
				         "(%.3f %.3f %.3f); never skipped again",
				         record, frame, ClassName(vtable), vtable, GeometryFlags(geometry), vertices, streams,
				         note->sampleCount, g_current.sampleCount, static_cast<double>(note->firstVertex[0]),
				         static_cast<double>(note->firstVertex[1]), static_cast<double>(note->firstVertex[2]),
				         static_cast<double>(g_current.firstVertex[0]), static_cast<double>(g_current.firstVertex[1]),
				         static_cast<double>(g_current.firstVertex[2]));
			}
		}
		return made;
	}
	// Check mode: a pack the live mode would have skipped, compared whole.
	g_counters.Count(ReuseWhy::Skip);
	++g_counters.checkCompared;
	g_verdicts.Note(vtable, !match, vertices);
	if (!match) {
		++g_counters.checkMismatch;
		if (g_mismatchLinesLeft != 0) {
			--g_mismatchLinesLeft;
			OBVR_LOG("Stream reuse: MISMATCH at scene call %u - record %p, class %s (%08X), flags %04X, vertices %u, "
			         "streams %u, hashes %u/%u, first %016llX second %016llX",
			         frame, record, ClassName(vtable), vtable, GeometryFlags(geometry), vertices, streams,
			         note->sampleCount, g_current.sampleCount, note->sampleCount ? note->samples[0] : 0ull,
			         g_current.sampleCount ? g_current.samples[0] : 0ull);
		}
	}
	return made;
}

void* __fastcall RingTakeHook(Ring* ring, void* /*edx*/, UInt32 bytes, UInt32* outOffset, UInt32* outFlags,
                              UInt32 forceDiscard, UInt32 usage) {
	if (g_on && ring != nullptr) {
		const UInt32 wanted = RingCapacityWanted(ring->capacity, bytes, g_ringMinimum);
		if (wanted != 0) {
			const UInt32 before = ring->capacity;
			if (ring->buffer != nullptr) {
				auto** vtable = *reinterpret_cast<void***>(ring->buffer);
				reinterpret_cast<ReleaseFn>(vtable[2])(ring->buffer);
				ring->buffer = nullptr;
			}
			ring->capacity = wanted;
			ring->used = 0;
			++g_counters.ringsGrown;
			if (g_ringLinesLeft != 0) {
				--g_ringLinesLeft;
				OBVR_LOG("Stream reuse: ring %p (FVF %08X) enlarged from %u to %u bytes at a request of %u", ring,
				         ring->fvf, before, wanted, bytes);
			}
		}
	}
	return g_originalRing(ring, nullptr, bytes, outOffset, outFlags, forceDiscard, usage);
}

bool InstallEntry(const char* what, UInt32 address, const UInt8* bytes, UInt32 length, const void* replacement,
                  void** originalOut) {
	if (!mem::Verify(address, bytes, length)) {
		OBVR_LOG("Stream reuse: %s entry at %08X differs; nothing installed", what, address);
		mem::ReportForeignCode("Stream reuse", address);
		return false;
	}
	constexpr UInt32 capacity = 32;
	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(capacity));
	if (trampoline == nullptr) {
		OBVR_LOG("Stream reuse: no executable memory for the %s trampoline", what);
		return false;
	}
	if (mem::BuildEntryTrampoline(trampoline, capacity, reinterpret_cast<UInt32>(trampoline), address, bytes,
	                              length) == 0) {
		OBVR_LOG("Stream reuse: the %s trampoline did not fit", what);
		return false;
	}
	UInt8 patch[16];
	if (length > sizeof(patch) ||
	    mem::BuildEntryPatch(patch, length, address, reinterpret_cast<UInt32>(replacement), length) == 0 ||
	    !mem::SafeWrite(address, patch, length)) {
		OBVR_LOG("Stream reuse: could not patch the %s entry", what);
		return false;
	}
	*originalOut = trampoline;
	return true;
}

}  // namespace

void InstallStreamReuse() {
	static bool s_done = false;
	if (s_done) return;
	s_done = true;
	const Config& config = GetConfig();
	if (!config.tracker.streamReuse) {
		OBVR_LOG("Stream reuse: off ([Render] StreamReuse=0) - the second pass packs its streams itself");
		return;
	}
	if (config.tracker.stereo != vr::StereoMode::DualPass) {
		OBVR_LOG("Stream reuse: nothing to reuse outside Stereo=dual; not installed");
		return;
	}
	if (!config.tracker.fastStreamCopy) {
		OBVR_LOG("Stream reuse: needs [Render] FastStreamCopy=1 (the streams are hashed through the fast copy); "
		         "not installed");
		return;
	}
	g_check = config.streamReuseCheck;
	g_ringMinimum = config.tracker.streamRingBytes;
	void* pack = nullptr;
	if (!InstallEntry("stream packer", kPackStreams, kPackBytes, sizeof(kPackBytes),
	                  reinterpret_cast<const void*>(&PackStreamsHook), &pack)) {
		return;
	}
	g_originalPack = reinterpret_cast<PackFn>(pack);
	void* ring = nullptr;
	if (!InstallEntry("ring manager", kRingTake, kRingBytes, sizeof(kRingBytes),
	                  reinterpret_cast<const void*>(&RingTakeHook), &ring)) {
		// The packer's detour stands and is harmless alone: without bigger
		// rings the generation check refuses nearly every skip.
		OBVR_LOG("Stream reuse: the rings keep the engine's sizes; the skips will be few");
	} else {
		g_originalRing = reinterpret_cast<RingFn>(ring);
	}
	g_on = true;
	OBVR_LOG("Stream reuse: on - the second pass takes the first pass's streams for a record whose buffers saw no "
	         "DISCARD since and whose bytes matched the first pass's %u times (re-checked every %u frames); rings "
	         "raised to %u bytes%s",
	         kLearnFrames, kVerifyEvery, g_ringMinimum,
	         g_check ? "; CHECK MODE packs every skip anyway and compares every element" : "");
}

void StreamReuseOnLock(void* buffer, UInt32 flags) {
	if (!g_on) return;
	constexpr UInt32 kLockDiscard = 0x2000;
	if ((flags & kLockDiscard) != 0) {
		g_generations.Bump(buffer);
		const int pass = CurrentScenePassIndex();
		if (pass == 0 || pass == 1) ++g_discardsByPass[pass];
	}
	if (g_packing) {
		g_current.AddBuffer(buffer, 0);
	}
}

bool StreamReusePacking() {
	return g_on && g_packing;
}

void StreamReuseOnCopy(const void* dest, UInt32 count, UInt32 elementBytes, UInt32 destStride) {
	if (!g_on || !g_packing || dest == nullptr || count == 0) return;
	if (g_current.sampleCount == 0 && elementBytes >= 12) {
		const auto* f = static_cast<const float*>(dest);
		g_current.firstVertex[0] = f[0];
		g_current.firstVertex[1] = f[1];
		g_current.firstVertex[2] = f[2];
	}
	g_current.AddSample(ElementHash(dest, count, elementBytes, destStride, g_check ? 0 : kSampleElements));
}

}  // namespace obvr::render
