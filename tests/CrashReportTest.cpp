// Checks the crash report's pure part (core/CrashReport.h): how an
// exception is named, how an access violation says what it touched, and
// that a crash inside the report (or a filter chain that leads back to
// OBVR's) passes on instead of reporting again.

#include <cstdio>
#include <cstring>

#include "core/CrashReport.h"

using namespace obvr;
using namespace obvr::crash;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Says(UInt32 code, UInt32 params, UInt32 p0, UInt32 p1, const char* wanted) {
	char out[128];
	const UInt32 info[2] = {p0, p1};
	DescribeException(code, info, params, out, sizeof(out));
	if (std::strcmp(out, wanted) != 0) {
		std::printf("        got \"%s\"\n", out);
		return false;
	}
	return true;
}

void TestNames() {
	std::printf("The exception's name\n");
	Check(Says(0xC0000005, 2, 0, 0x00000010, "access violation reading 00000010"), "an access violation reading");
	Check(Says(0xC0000005, 2, 1, 0x00000000, "access violation writing 00000000"), "writing");
	Check(Says(0xC0000005, 2, 8, 0x12345678, "access violation executing 12345678"), "executing (DEP)");
	Check(Says(0xC0000005, 0, 0, 0, "access violation"), "without its parameters: the name alone");
	Check(Says(0xC0000005, 2, 5, 0x10, "access violation (kind 5) at 00000010"), "an unknown kind: said as a number");
	Check(Says(0xC00000FD, 0, 0, 0, "stack overflow"), "a stack overflow");
	Check(Says(0xC000001D, 0, 0, 0, "illegal instruction"), "an illegal instruction");
	Check(Says(0xC0000094, 0, 0, 0, "integer divide by zero"), "a division by zero");
	Check(Says(0xC0000096, 0, 0, 0, "privileged instruction"), "a privileged instruction");
	Check(Says(0xC0000409, 0, 0, 0, "stack buffer overrun"), "the stack cookie (fast fail)");
	Check(Says(0xC0000374, 0, 0, 0, "heap corruption"), "the heap's corruption");
	Check(Says(0xE06D7363, 0, 0, 0, "C++ exception"), "an MSVC C++ throw nobody caught");
	Check(Says(0x80000003, 0, 0, 0, "breakpoint"), "a breakpoint");
	Check(Says(0xDEADBEEF, 0, 0, 0, "exception DEADBEEF"), "anything else: its code");
	char small[8];
	const UInt32 info[2] = {0, 0x10};
	DescribeException(0xC0000005, info, 2, small, sizeof(small));
	Check(std::strlen(small) == sizeof(small) - 1, "a short buffer: cut, terminated");
	DescribeException(0xC0000005, nullptr, 2, small, sizeof(small));
	Check(std::strcmp(small, "access ") == 0, "no parameter array: the name alone, cut");
}

void TestOnce() {
	std::printf("Reported once, the chain passed on\n");
	ReportGate gate;
	Check(Enter(gate) == FilterStep::Report, "the first crash: reported");
	Check(Enter(gate) == FilterStep::PassOn, "a crash inside the report, or the chain coming back: passed on");
	Check(Enter(gate) == FilterStep::PassOn, "and every one after");
	Check(gate.entries == 3, "each counted");
}

}  // namespace

int main() {
	TestNames();
	TestOnce();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
