// Checks the hang watchdog's stack arithmetic: how many words the stack
// pointer's region still holds, and which of them count as code.

#include <cstdio>

#include "core/StackScan.h"

namespace {

using namespace obvr::watchdog;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

void TestWordsAvailable() {
	std::printf("Words available\n");
	// A 64 KB region at 0x00100000; the pointer 16 bytes below its end.
	const UInt32 base = 0x00100000;
	const UInt32 size = 0x10000;
	Check(StackWordsAvailable(base + size - 16, base, size, 256) == 4,
	      "the region's end caps the count");
	Check(StackWordsAvailable(base, base, size, 8) == 8, "the wish caps it otherwise");
	Check(StackWordsAvailable(base + 2, base, size, 8) == 0, "an unaligned pointer reads nothing");
	Check(StackWordsAvailable(base - 4, base, size, 8) == 0, "below the region reads nothing");
	Check(StackWordsAvailable(base + size, base, size, 8) == 0, "at the region's end reads nothing");
	Check(StackWordsAvailable(0xFFFFFFF0, 0xFFFFF000, 0x2000, 8) == 0,
	      "a region that wraps the address space reads nothing");
	Check(StackWordsAvailable(base + size - 4, base, size, 0) == 0, "a wish of zero reads nothing");
}

void TestCollectCodeWords() {
	std::printf("Collect code words\n");
	const UInt32 words[] = {0x0012FF80, 0x00401234, 0x00000000, 0x7C801000, 0x00405678,
	                        0x0012FF90, 0x00409ABC};
	auto isCode = [](UInt32 w) { return (w >= 0x00400000 && w < 0x00500000) || w == 0x7C801000; };
	UInt32 out[8];
	UInt32 kept = CollectCodeWords(words, 7, isCode, out, 8);
	Check(kept == 4, "four words point into code");
	Check(out[0] == 0x00401234 && out[1] == 0x7C801000 && out[2] == 0x00405678 &&
	          out[3] == 0x00409ABC,
	      "in stack order");
	kept = CollectCodeWords(words, 7, isCode, out, 2);
	Check(kept == 2 && out[1] == 0x7C801000, "the capacity caps the list");
	kept = CollectCodeWords(words, 0, isCode, out, 8);
	Check(kept == 0, "no words, nothing kept");
	kept = CollectCodeWords(words, 7, [](UInt32) { return false; }, out, 8);
	Check(kept == 0, "nothing accepted, nothing kept");
}

}  // namespace

int main() {
	TestWordsAvailable();
	TestCollectCodeWords();

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
