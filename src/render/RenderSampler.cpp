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

constexpr UInt32 kMaxSamples = 300000;  // 5 minutes at 1 kHz
constexpr UInt32 kStackWords = 256;     // a kilobyte of stack above esp
constexpr UInt32 kChain = 8;
// Oblivion.exe's .text (objdump -h, 1.2.0.416): what counts as the engine's
// code on the stack. Everything else - DXVK, Wine, OBVR - is in the eip only.
constexpr UInt32 kTextBegin = 0x00401000u;
constexpr UInt32 kTextEnd = 0x00A28000u;

struct Sample {
	UInt64 tick;
	UInt32 eip;
	UInt32 sceneCall;
	UInt8 inPass;
	UInt8 chainCount;
	UInt32 chain[kChain];
};

Sample* g_samples = nullptr;
UInt32 g_count = 0;
UInt32 g_dumps = 0;
volatile DWORD g_renderThreadId = 0;
bool g_started = false;

bool IsEngineCode(UInt32 word) { return word >= kTextBegin && word < kTextEnd; }

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
	out.chainCount = static_cast<UInt8>(watchdog::CollectCodeWords(words, wordCount, &IsEngineCode, out.chain, kChain));
	return true;
}

// The module each distinct 64 KiB page of sampled eips lies in, written as
// OBVR-samples-modules.csv (page, module, base) so the analysis can name a
// bin outside the engine - DXVK, Wine, OBSE, OBVR.
void DumpModules() {
	char path[512] = {};
	if (!platform::BuildGamePath("OBVR-samples-modules.csv", path, sizeof(path))) {
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
	for (UInt32 i = 0; i < g_count && pageCount < kPages; ++i) {
		const UInt32 page = g_samples[i].eip >> 16;
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

void Dump() {
	char path[512] = {};
	if (!platform::BuildGamePath("OBVR-samples.csv", path, sizeof(path))) {
		OBVR_LOG("Sampler: no path for the samples - %u samples dropped", g_count);
		g_count = 0;
		return;
	}
	FILE* file = std::fopen(path, "w");
	if (file == nullptr) {
		OBVR_LOG("Sampler: could not write %s - %u samples dropped", path, g_count);
		g_count = 0;
		return;
	}
	std::fputs("tick,in_pass,scene_call,eip,w0,w1,w2,w3,w4,w5,w6,w7\n", file);
	for (UInt32 i = 0; i < g_count; ++i) {
		const Sample& s = g_samples[i];
		std::fprintf(file, "%llu,%u,%u,%08X", static_cast<unsigned long long>(s.tick), s.inPass, s.sceneCall,
		             s.eip);
		for (UInt32 k = 0; k < kChain; ++k) {
			if (k < s.chainCount) {
				std::fprintf(file, ",%08X", s.chain[k]);
			} else {
				std::fputs(",", file);
			}
		}
		std::fputc('\n', file);
	}
	std::fclose(file);
	DumpModules();
	++g_dumps;
	OBVR_LOG("Sampler: %u samples of the render thread written to %s (dump %u)", g_count, path, g_dumps);
	g_count = 0;
}

DWORD __stdcall Run(void*) {
	HANDLE thread = nullptr;
	DWORD openedId = 0;
	for (;;) {
		Sleep(1);
		const bool capturing = perf::Profiler::Instance().IsCapturing();
		if (!capturing) {
			if (g_count > 0) {
				Dump();
			}
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
		if (g_count >= kMaxSamples) {
			continue;
		}
		Sample& s = g_samples[g_count];
		// Read before the suspend: the flags are the render thread's and may
		// flip while it is held, but a sample a millisecond wide does not mind.
		s.inPass = SceneRenderRunning() ? 1 : 0;
		s.sceneCall = CurrentSceneCall();
		if (TakeSample(thread, s)) {
			++g_count;
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
	         "runs, into OBVR-samples.csv");
	return true;
}

}  // namespace obvr::render
