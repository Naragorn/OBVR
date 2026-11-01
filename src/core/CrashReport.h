#pragma once

// The crash report's pure part (core/CrashHandler.cpp holds the Win32 one).
//
// Until 2026-11-01 a crash left OBVR.log ending mid-sentence and nothing
// else: no address, no module. Testers without a crash logger plugin had
// nothing to send. The handler writes the exception, where it happened
// (module+offset), the registers and the code addresses on the stack into
// OBVR.log, then hands the crash on to the filter that was there before it
// (a crash logger's, the engine's).
//
// The exception codes are ntstatus.h's and winnt.h's; EXCEPTION_RECORD's
// ExceptionInformation[0] for an access violation is 0 read, 1 write, 8 DEP
// (execute), [1] the address (MSDN, EXCEPTION_RECORD).
//
// Pure; crash_report_test covers it.

#include <cstdio>
#include <cstring>

#include "core/Types.h"

namespace obvr::crash {

inline const char* ExceptionName(UInt32 code) {
	switch (code) {
	case 0xC0000005: return "access violation";
	case 0xC00000FD: return "stack overflow";
	case 0xC000001D: return "illegal instruction";
	case 0xC0000094: return "integer divide by zero";
	case 0xC0000096: return "privileged instruction";
	case 0xC0000409: return "stack buffer overrun";
	case 0xC0000374: return "heap corruption";
	case 0xC000008C: return "array bounds exceeded";
	case 0xC000008E: return "float divide by zero";
	case 0xC0000090: return "float invalid operation";
	case 0xC0000006: return "in-page error";
	case 0xE06D7363: return "C++ exception";
	case 0x80000003: return "breakpoint";
	default: return nullptr;
	}
}

// What the exception was, into `out` (always terminated): its name, and for
// an access violation what it did where.
inline void DescribeException(UInt32 code, const UInt32* information, UInt32 parameters, char* out,
                              UInt32 size) {
	if (out == nullptr || size == 0) {
		return;
	}
	const char* const name = ExceptionName(code);
	int wrote = 0;
	if (name == nullptr) {
		wrote = std::snprintf(out, size, "exception %08X", code);
	} else if (code == 0xC0000005 && information != nullptr && parameters >= 2) {
		const UInt32 kind = information[0];
		const char* const verb = kind == 0 ? "reading" : kind == 1 ? "writing" : kind == 8 ? "executing" : nullptr;
		if (verb != nullptr) {
			wrote = std::snprintf(out, size, "%s %s %08X", name, verb, information[1]);
		} else {
			wrote = std::snprintf(out, size, "%s (kind %u) at %08X", name, kind, information[1]);
		}
	} else {
		wrote = std::snprintf(out, size, "%s", name);
	}
	(void)wrote;
	out[size - 1] = '\0';
}

// Once only: a crash inside the report, or a filter chain that leads back
// to OBVR's (another plugin's filter set after OBVR's, calling the one it
// replaced), passes on instead of reporting again.
struct ReportGate {
	UInt32 entries = 0;
};

enum class FilterStep { Report, PassOn };

inline FilterStep Enter(ReportGate& gate) {
	++gate.entries;
	return gate.entries == 1 ? FilterStep::Report : FilterStep::PassOn;
}

}  // namespace obvr::crash
