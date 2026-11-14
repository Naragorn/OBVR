#include "render/RenderSampler.h"

#include <cstdio>
#include <new>

#include "core/BranchDecode.h"
#include "core/Config.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "core/StackScan.h"
#include "perf/Profiler.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"
#include "render/SceneRenderHook.h"

namespace obvr::render {
namespace {

// The rows kept: a ring of the newest (perf/ProfileLogic.h RingSlot), five
// minutes at 1 kHz - as long as the longest performance ring.
constexpr UInt32 kMaxSamples = 300000;
constexpr UInt32 kStackWords = 256;     // a kilobyte of stack above esp
constexpr UInt32 kChain = 8;
// Oblivion.exe's .text (objdump -h, 1.2.0.416): what counts as the engine's
// code on the stack. Everything else - DXVK, Wine, OBVR - is in the eip only.
constexpr UInt32 kTextBegin = 0x00401000u;
constexpr UInt32 kTextEnd = 0x00A28000u;

// Return addresses in other modules' code - D3D9, DXVK, the kernel, OBVR -
// kept beside the engine's chain, the innermost first: what the eip outside
// the engine was called through (a stall in vrclient's texture import named
// only its ntdll eip and text from its stack, 2026-10-27).
constexpr UInt32 kOutside = 4;

struct Sample {
	UInt64 tick;
	UInt32 eip;
	UInt32 sceneCall;
	UInt8 inPass;
	UInt8 chainCount;
	UInt8 outsideCount;
	UInt32 chain[kChain];
	UInt32 outside[kOutside];
};

Sample* g_samples = nullptr;
// The rows taken since the last dump - the ring holds the newest kMaxSamples
// of them - and whether they are an ended capture's, waiting for its folder
// (ProfileLogic.h NextSamplesStep).
struct Rows {
	UInt64 written = 0;
	bool pending = false;
};
Rows g_rows;
UInt32 g_dumps = 0;
volatile DWORD g_renderThreadId = 0;
bool g_started = false;

bool IsEngineCode(UInt32 word) { return word >= kTextBegin && word < kTextEnd; }

// A word of the engine's code that follows a call (core/StackScan.h
// FollowsCall): a return address, not text or a stale pointer that happens to
// fall in .text. The exe is never unloaded, so its bytes read directly.
bool IsEngineReturn(UInt32 word) {
	return IsEngineCode(word) && word - 7 >= kTextBegin &&
	       watchdog::FollowsCall(reinterpret_cast<const UInt8*>(word - 7));
}

// The other modules' code, learned on the sampler's thread as stack words
// point into it: each 64 KiB page (the allocation granularity, so one image
// per page) is asked of VirtualQuery once, an image's executable sections read
// off its PE headers (CodeRangesOfImage). Forgotten at every dump.
constexpr UInt32 kMemImage = 0x1000000;
constexpr UInt32 kMaxModules = 64;
constexpr UInt16 kPageUnknown = 0;
constexpr UInt16 kPageNotCode = 0xFFFF;
struct ModuleCode {
	UInt32 base;
	UInt32 count;
	watchdog::CodeRange ranges[4];
};
ModuleCode g_modules[kMaxModules];
UInt32 g_moduleCount = 0;
UInt16 g_pageKind[65536];  // kPageUnknown, kPageNotCode, or a module's index + 1

UInt16 ClassifyPage(UInt32 word) {
	const UInt32 page = word >> 16;
	if (g_pageKind[page] != kPageUnknown) {
		return g_pageKind[page];
	}
	UInt16 kind = kPageNotCode;
	MemoryBasicInfo region{};
	if (QueryMemory(reinterpret_cast<const void*>(word), region) && region.state == MEM_COMMIT &&
	    region.type == kMemImage) {
		const UInt32 base = reinterpret_cast<UInt32>(region.allocationBase);
		UInt32 index = 0;
		while (index < g_moduleCount && g_modules[index].base != base) {
			++index;
		}
		if (index == g_moduleCount && g_moduleCount < kMaxModules) {
			ModuleCode& m = g_modules[g_moduleCount];
			m.base = base;
			m.count = 0;
			MemoryBasicInfo header{};
			// The headers' page, read only when committed and readable.
			if (QueryMemory(reinterpret_cast<const void*>(base), header) && header.state == MEM_COMMIT &&
			    (header.protect & PAGE_GUARD) == 0 && header.protect != PAGE_NOACCESS) {
				const UInt32 readable =
					header.regionSize < 0x1000 ? header.regionSize : static_cast<UInt32>(0x1000);
				m.count = watchdog::CodeRangesOfImage(reinterpret_cast<const UInt8*>(base), readable, base,
				                                      m.ranges, 4);
			}
			++g_moduleCount;
		}
		if (index < g_moduleCount) {
			kind = static_cast<UInt16>(index + 1);
		}
	}
	g_pageKind[page] = kind;
	return kind;
}

// A return address in another module's code: inside one of its executable
// sections, the seven bytes before it in the same section and readable, and
// a call there.
bool IsOutsideReturn(UInt32 word) {
	if (word < 0x10000u || IsEngineCode(word)) {
		return false;
	}
	const UInt16 kind = ClassifyPage(word);
	if (kind == kPageNotCode || kind == kPageUnknown) {
		return false;
	}
	const ModuleCode& m = g_modules[kind - 1];
	bool inside = false;
	for (UInt32 i = 0; i < m.count && !inside; ++i) {
		inside = word >= m.ranges[i].begin + 7 && word < m.ranges[i].end;
	}
	if (!inside || IsBadReadPtr(reinterpret_cast<const void*>(word - 7), 7) != 0) {
		return false;
	}
	return watchdog::FollowsCall(reinterpret_cast<const UInt8*>(word - 7));
}

void ForgetModules() {
	for (UInt16& kind : g_pageKind) {
		kind = kPageUnknown;
	}
	g_moduleCount = 0;
}

bool TakeSample(HANDLE thread, Sample& out) {
	ThreadContext context{};
	bool have = false;
	UInt32 words[kStackWords];
	UInt32 wordCount = 0;
	if (SuspendThread(thread) == static_cast<DWORD>(-1)) {
		return false;
	}
	have = ReadThreadContext(thread, context);
	if (have) {
		MemoryBasicInfo region{};
		if (QueryMemory(reinterpret_cast<const void*>(context.esp), region) &&
		    region.state == MEM_COMMIT && (region.protect & PAGE_GUARD) == 0 &&
		    region.protect != PAGE_NOACCESS) {
			const UInt32 available = watchdog::StackWordsAvailable(
				context.esp, reinterpret_cast<UInt32>(region.baseAddress), region.regionSize, kStackWords);
			const UInt32* stack = reinterpret_cast<const UInt32*>(context.esp);
			for (UInt32 i = 0; i < available; ++i) {
				words[i] = stack[i];
			}
			wordCount = available;
		}
	}
	ResumeThread(thread);
	if (!have) {
		return false;
	}
	out.tick = static_cast<UInt64>(ReadPerformanceCounter());
	out.eip = context.eip;
	out.chainCount =
		static_cast<UInt8>(watchdog::CollectCodeWords(words, wordCount, &IsEngineReturn, out.chain, kChain));
	out.outsideCount =
		static_cast<UInt8>(watchdog::CollectCodeWords(words, wordCount, &IsOutsideReturn, out.outside, kOutside));
	return true;
}

// Where a dump's file goes: into the capture's folder when the writer has
// one (samples.csv, samples-modules.csv), else beside OBVR.log under the
// names of before 2026-11-02.
bool SamplesPath(const char* directory, const char* inCapture, const char* besideLog, char* path,
                 UInt32 size) {
	if (directory != nullptr) {
		return perf::JoinPath(directory, inCapture, path, size);
	}
	return platform::BuildGamePath(besideLog, path, size);
}

// The module each distinct 64 KiB page of sampled eips lies in, written as
// samples-modules.csv (page, module, base) so the analysis can name a
// bin outside the engine - DXVK, Wine, OBSE, OBVR.
void DumpModules(const char* directory, UInt32 first, UInt32 stored) {
	char path[512] = {};
	if (!SamplesPath(directory, perf::kSamplesModulesFileName, "OBVR-samples-modules.csv", path,
	                 sizeof(path))) {
		return;
	}
	FILE* file = std::fopen(path, "w");
	if (file == nullptr) {
		return;
	}
	std::fputs("page,module,base\n", file);
	constexpr UInt32 kPages = 512;
	UInt32 pages[kPages];
	UInt32 pageCount = 0;
	for (UInt32 i = first * (1 + kOutside); i < stored * (1 + kOutside) && pageCount < kPages; ++i) {
		const Sample& sample = g_samples[perf::RingOrderedSlot(g_rows.written, kMaxSamples, i / (1 + kOutside))];
		const UInt32 slot = i % (1 + kOutside);
		if (slot > 0 && slot > sample.outsideCount) {
			continue;
		}
		const UInt32 page = (slot == 0 ? sample.eip : sample.outside[slot - 1]) >> 16;
		bool seen = false;
		for (UInt32 k = 0; k < pageCount; ++k) {
			if (pages[k] == page) {
				seen = true;
				break;
			}
		}
		if (seen) {
			continue;
		}
		pages[pageCount++] = page;
		HMODULE module = nullptr;
		const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
		char modulePath[512] = "";
		const char* name = "?";
		UInt32 base = 0;
		if (GetModuleHandleExA(flags, reinterpret_cast<const char*>(page << 16), &module) != 0 &&
		    module != nullptr && GetModuleFileNameA(module, modulePath, sizeof(modulePath)) != 0) {
			name = mem::FileBaseName(modulePath);
			base = reinterpret_cast<UInt32>(module);
		}
		std::fprintf(file, "%04X,%s,%08X\n", page, name, base);
	}
	std::fclose(file);
}

// directory is the capture's folder, or null for beside the log. The rows go
// out oldest first; a performance ring's export keeps its last ringSeconds,
// and so do they (RingCutoffTick from the newest row).
void Dump(const char* directory) {
	g_rows.pending = false;
	const UInt32 stored = perf::RingStoredCount(g_rows.written, kMaxSamples);
	const auto tickAt = [](UInt32 slot) { return g_samples[slot].tick; };
	const perf::ProfileSettings& settings = perf::Profiler::Instance().Settings();
	UInt64 cutoff = 0;
	if (settings.ring && stored > 0) {
		const UInt64 newest = tickAt(perf::RingOrderedSlot(g_rows.written, kMaxSamples, stored - 1));
		cutoff = perf::RingCutoffTick(newest, settings.ringSeconds, static_cast<UInt64>(ReadPerformanceFrequency()));
	}
	const UInt32 first = perf::RingFirstKept(g_rows.written, kMaxSamples, cutoff, tickAt);
	const UInt32 kept = stored - first;
	char path[512] = {};
	if (!SamplesPath(directory, perf::kSamplesFileName, "OBVR-samples.csv", path, sizeof(path))) {
		OBVR_LOG("Sampler: no path for the samples - %u samples dropped", kept);
		g_rows.written = 0;
		return;
	}
	FILE* file = std::fopen(path, "w");
	if (file == nullptr) {
		OBVR_LOG("Sampler: could not write %s - %u samples dropped", path, kept);
		g_rows.written = 0;
		return;
	}
	std::fputs("tick,in_pass,scene_call,eip,w0,w1,w2,w3,w4,w5,w6,w7,x0,x1,x2,x3\n", file);
	for (UInt32 i = first; i < stored; ++i) {
		const Sample& s = g_samples[perf::RingOrderedSlot(g_rows.written, kMaxSamples, i)];
		std::fprintf(file, "%llu,%u,%u,%08X", static_cast<unsigned long long>(s.tick), s.inPass, s.sceneCall,
		             s.eip);
		for (UInt32 k = 0; k < kChain; ++k) {
			if (k < s.chainCount) {
				std::fprintf(file, ",%08X", s.chain[k]);
			} else {
				std::fputs(",", file);
			}
		}
		for (UInt32 k = 0; k < kOutside; ++k) {
			if (k < s.outsideCount) {
				std::fprintf(file, ",%08X", s.outside[k]);
			} else {
				std::fputs(",", file);
			}
		}
		std::fputc('\n', file);
	}
	std::fclose(file);
	DumpModules(directory, first, stored);
	ForgetModules();
	++g_dumps;
	OBVR_LOG("Sampler: %u samples of the render thread written to %s (dump %u; %llu taken since the last, the newest "
	         "%u held, %u of them before the ring's window)",
	         kept, path, g_dumps, static_cast<unsigned long long>(g_rows.written), stored, first);
	g_rows.written = 0;
}

// The rows' way out after a capture: they follow the capture into the folder
// its writer settles on, and wait for it - the writer runs in its own thread
// and may still be trying names. The exceptions are in NextSamplesStep.
void SettlePending(bool capturing) {
	perf::Profiler& profiler = perf::Profiler::Instance();
	if (!capturing && g_rows.written > 0) {
		g_rows.pending = true;
		// before the writer ends: a ring waits for these rows (RingRestartDue)
		profiler.SetSamplesPending(true);
	}
	const char* directory = profiler.ExportDirectory();
	switch (perf::NextSamplesStep(capturing, g_rows.pending, profiler.IsWriterBusy(), directory[0] != '\0')) {
	case perf::SamplesStep::WriteInCapture:
		Dump(directory);
		break;
	case perf::SamplesStep::WriteBesideLog:
		if (capturing) {
			OBVR_LOG("Sampler: a new capture began before the last one's rows had a folder - "
			         "they go beside the log");
		} else {
			OBVR_LOG("Sampler: the capture's export has no folder - its rows go beside the log");
		}
		Dump(nullptr);
		break;
	case perf::SamplesStep::Wait:
	case perf::SamplesStep::Keep:
		break;
	}
	profiler.SetSamplesPending(g_rows.pending);
}

DWORD __stdcall Run(void*) {
	HANDLE thread = nullptr;
	DWORD openedId = 0;
	for (;;) {
		Sleep(1);
		const bool capturing = perf::Profiler::Instance().IsCapturing();
		SettlePending(capturing);
		if (!capturing) {
			continue;
		}
		const DWORD id = g_renderThreadId;
		if (id == 0) {
			continue;
		}
		if (thread == nullptr || openedId != id) {
			if (thread != nullptr) {
				CloseHandle(thread);
			}
			thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, 0, id);
			openedId = id;
			if (thread == nullptr) {
				OBVR_LOG("Sampler: the render thread (id %u) could not be opened", static_cast<UInt32>(id));
				continue;
			}
		}
		Sample& s = g_samples[perf::RingSlot(g_rows.written, kMaxSamples)];
		// Read before the suspend: the flags are the render thread's and may
		// flip while it is held, but a sample a millisecond wide does not mind.
		s.inPass = SceneRenderRunning() ? 1 : 0;
		s.sceneCall = CurrentSceneCall();
		if (TakeSample(thread, s)) {
			++g_rows.written;
		}
	}
}

}  // namespace

void NoteRenderThread() {
	if (g_renderThreadId == 0) {
		g_renderThreadId = GetCurrentThreadId();
	}
}

bool StartRenderSampler() {
	if (g_started) {
		return true;
	}
	if (!GetConfig().performance.sampler) {
		return false;
	}
	g_samples = new (std::nothrow) Sample[kMaxSamples];
	if (g_samples == nullptr) {
		OBVR_LOG("Sampler: no memory for %u samples - the sampler stays off", kMaxSamples);
		return false;
	}
	HANDLE thread = CreateThread(nullptr, 0, &Run, nullptr, 0, nullptr);
	if (thread == nullptr) {
		OBVR_LOG("Sampler: the sampling thread could not be created - the sampler stays off");
		delete[] g_samples;
		g_samples = nullptr;
		return false;
	}
	CloseHandle(thread);
	g_started = true;
	OBVR_LOG("Sampler: on - the render thread is sampled about every millisecond while a capture "
	         "runs, into samples.csv in the capture's folder");
	return true;
}

}  // namespace obvr::render
