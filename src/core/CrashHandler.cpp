#include "core/CrashHandler.h"

#include <cstdio>

#include "core/CrashReport.h"
#include "core/Log.h"
#include "core/StackScan.h"
#include "core/Watchdog.h"
#include "platform/Win32Min.h"

namespace obvr::crash {
namespace {

// EXCEPTION_CONTINUE_SEARCH (winnt.h / excpt.h): let the next handler -
// Windows' own, the end of the chain - take it.
constexpr long kContinueSearch = 0;

constexpr UInt32 kStackWordsWanted = 2048;
constexpr UInt32 kCodeAddressesNamed = 24;

bool g_installed = false;
ExceptionFilterFn g_previous = nullptr;
ReportGate g_gate;

void Report(const ExceptionPointersMin* pointers) {
	const ExceptionRecordMin* const record = pointers != nullptr ? pointers->record : nullptr;
	const ThreadContext* const context = pointers != nullptr ? pointers->context : nullptr;
	if (record == nullptr) {
		OBVR_LOG("CRASH: an exception without its record - nothing more to say");
		return;
	}
	char what[128];
	DescribeException(record->code, record->information, record->numberParameters, what, sizeof(what));
	char where[128];
	watchdog::NameCodeAddress(reinterpret_cast<UInt32>(record->address), where, sizeof(where));
	OBVR_LOG("CRASH: %s at %s (code %08X, thread %u) - the frame's last step \"%s\"", what, where,
	         static_cast<UInt32>(record->code), static_cast<UInt32>(GetCurrentThreadId()), watchdog::LastStep());
	if (context == nullptr) {
		return;
	}
	OBVR_LOG("CRASH: eax %08X ebx %08X ecx %08X edx %08X esi %08X edi %08X ebp %08X esp %08X eip %08X",
	         context->eax, context->ebx, context->ecx, context->edx, context->esi, context->edi, context->ebp,
	         context->esp, context->eip);

	// The stack from the crash's esp, bounded by its region as the
	// watchdog's report bounds it.
	UInt32 count = 0;
	MemoryBasicInfo region{};
	if (QueryMemory(reinterpret_cast<const void*>(context->esp), region) && region.state == MEM_COMMIT &&
	    (region.protect & PAGE_GUARD) == 0 && region.protect != PAGE_NOACCESS) {
		count = watchdog::StackWordsAvailable(context->esp, reinterpret_cast<UInt32>(region.baseAddress),
		                                      region.regionSize, kStackWordsWanted);
	}
	const UInt32* const stack = reinterpret_cast<const UInt32*>(context->esp);
	UInt32 code[kCodeAddressesNamed];
	const UInt32 kept =
		count == 0 ? 0 : watchdog::CollectCodeWords(stack, count, &watchdog::IsModuleAddress, code, kCodeAddressesNamed);
	char line[1000] = "";
	UInt32 used = 0;
	char name[96];
	for (UInt32 i = 0; i < kept; ++i) {
		watchdog::NameCodeAddress(code[i], name, sizeof(name));
		const int wrote = std::snprintf(line + used, sizeof(line) - used, "%s%s", i == 0 ? "" : ", ", name);
		if (wrote < 0 || used + static_cast<UInt32>(wrote) >= sizeof(line) - 1) {
			break;
		}
		used += static_cast<UInt32>(wrote);
	}
	line[sizeof(line) - 1] = '\0';
	OBVR_LOG("CRASH: code addresses on the stack, nearest first (return addresses among stale ones): %s",
	         kept == 0 ? "none" : line);
	OBVR_LOG("CRASH: %s - send this OBVR.log with the report", g_previous != nullptr
	                                                              ? "handed on to the crash filter set before OBVR's"
	                                                              : "no other crash filter was set");
}

long __stdcall Filter(ExceptionPointersMin* pointers) {
	// Once: a crash inside the report, or a chain back to this filter,
	// passes on at once (core/CrashReport.h).
	if (Enter(g_gate) != FilterStep::Report) {
		return kContinueSearch;
	}
	Report(pointers);
	return g_previous != nullptr ? g_previous(pointers) : kContinueSearch;
}

}  // namespace

void InstallOnce() {
	if (g_installed) {
		return;
	}
	g_installed = true;
	g_previous = InstallUnhandledFilter(&Filter);
	char previous[128] = "none";
	if (g_previous != nullptr) {
		watchdog::NameCodeAddress(reinterpret_cast<UInt32>(g_previous), previous, sizeof(previous));
	}
	OBVR_LOG("Crash report: OBVR's filter set; the one before it %s", previous);
}

}  // namespace obvr::crash
