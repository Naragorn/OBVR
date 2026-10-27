// Checks the hang watchdog's stack arithmetic: how many words the stack
// pointer's region still holds, and which of them count as code.

#include <array>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <vector>

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

// A word on the stack counts as a return address only when the bytes before
// it are a call: an E8 rel32, or an FF /2 in one of its forms. Two-byte text
// read as words (the UTF-16 strings "\WINDOWS\System32\" and "NVIDIA GeForce"
// a stall's samples held, 2026-10-27: 0x0057005C, 0x004E0049 ...) and stale
// data that happens to point into code are not.
void TestFollowsCall() {
	std::printf("Follows a call\n");
	auto bytes = [](std::initializer_list<int> list) {
		std::array<UInt8, 7> b{};
		UInt32 i = 7 - static_cast<UInt32>(list.size());
		for (int v : list) b[i++] = static_cast<UInt8>(v);
		return b;
	};
	Check(FollowsCall(bytes({0xE8, 0x11, 0xC8, 0x07, 0x00}).data()), "call rel32 (E8 xx xx xx xx)");
	Check(FollowsCall(bytes({0xFF, 0x15, 0xCC, 0x9D, 0x1B, 0x10}).data()), "call [abs32] (FF 15)");
	Check(FollowsCall(bytes({0xFF, 0xD2}).data()), "call edx (FF D2)");
	Check(FollowsCall(bytes({0xFF, 0x10}).data()), "call [eax] (FF 10)");
	Check(!FollowsCall(bytes({0xFF, 0x14}).data()), "FF 14 two bytes back is a SIB form, not complete");
	Check(FollowsCall(bytes({0xFF, 0x14, 0x88}).data()), "call [eax+ecx*4] (FF 14 sib)");
	Check(!FollowsCall(bytes({0xFF, 0x14, 0x85}).data()), "FF 14 with a SIB of base 5 wants a disp32 after it: incomplete");
	Check(FollowsCall(bytes({0xFF, 0x14, 0x85, 0x00, 0x10, 0xA9, 0x00}).data()), "call [disp32+eax*4] (FF 14 85 disp32)");
	Check(FollowsCall(bytes({0xFF, 0x50, 0x54}).data()), "call [eax+disp8] (FF 50 54)");
	Check(FollowsCall(bytes({0xFF, 0x54, 0x24, 0x10}).data()), "call [esp+disp8] (FF 54 24 10)");
	Check(FollowsCall(bytes({0xFF, 0x90, 0x64, 0x01, 0x00, 0x00}).data()), "call [eax+disp32] (FF 90)");
	Check(FollowsCall(bytes({0xFF, 0x94, 0x24, 0x00, 0x01, 0x00, 0x00}).data()), "call [esp+disp32] (FF 94 24)");
	Check(!FollowsCall(bytes({0x8B, 0x44, 0x24, 0x0C, 0x83, 0xEC, 0x1C}).data()), "a mov and a sub: no call");
	Check(!FollowsCall(bytes({0xFF, 0xE0}).data()), "jmp eax (FF E0) is no call");
	Check(!FollowsCall(bytes({0xFF, 0x20}).data()), "jmp [eax] (FF 20) is no call");
	Check(!FollowsCall(bytes({0x00, 0x57, 0x00, 0x5C, 0x00, 0x4E, 0x00}).data()), "UTF-16 text: no call");
	Check(!FollowsCall(bytes({}).data()), "all zero: no call");
}

// The code ranges of a module from its PE headers in memory: the sections
// marked executable.
void TestCodeRanges() {
	std::printf("Code ranges of a module\n");
	std::vector<UInt8> image(0x400, 0);
	auto put32 = [&](UInt32 at, UInt32 v) { std::memcpy(&image[at], &v, 4); };
	auto put16 = [&](UInt32 at, UInt16 v) { std::memcpy(&image[at], &v, 2); };
	image[0] = 'M'; image[1] = 'Z';
	put32(0x3C, 0x80);
	image[0x80] = 'P'; image[0x81] = 'E';
	put16(0x80 + 6, 3);       // three sections
	put16(0x80 + 20, 0xE0);   // the optional header's size
	const UInt32 table = 0x80 + 24 + 0xE0;
	auto section = [&](UInt32 i, UInt32 va, UInt32 size, UInt32 flags) {
		put32(table + i * 40 + 8, size);
		put32(table + i * 40 + 12, va);
		put32(table + i * 40 + 36, flags);
	};
	section(0, 0x1000, 0x5000, 0x60000020);  // .text: code, execute, read
	section(1, 0x6000, 0x2000, 0x40000040);  // .rdata
	section(2, 0x8000, 0x800, 0xE0000020);   // a second executable section
	CodeRange ranges[4];
	const UInt32 n = CodeRangesOfImage(image.data(), static_cast<UInt32>(image.size()), 0x79070000u, ranges, 4);
	Check(n == 2, "two executable sections");
	Check(n >= 1 && ranges[0].begin == 0x79071000u && ranges[0].end == 0x79076000u, "the first at base + 0x1000, 0x5000 long");
	Check(n >= 2 && ranges[1].begin == 0x79078000u && ranges[1].end == 0x79078800u, "the second at base + 0x8000");
	Check(CodeRangesOfImage(image.data(), static_cast<UInt32>(image.size()), 0x79070000u, ranges, 1) == 1,
	      "the capacity caps the ranges");
	std::vector<UInt8> notPe(image);
	notPe[0x80] = 'X';
	Check(CodeRangesOfImage(notPe.data(), static_cast<UInt32>(notPe.size()), 0x79070000u, ranges, 4) == 0,
	      "no PE signature: no ranges");
	std::vector<UInt8> notMz(image);
	notMz[0] = 'X';
	Check(CodeRangesOfImage(notMz.data(), static_cast<UInt32>(notMz.size()), 0x79070000u, ranges, 4) == 0,
	      "no MZ: no ranges");
	Check(CodeRangesOfImage(image.data(), 0x100, 0x79070000u, ranges, 4) == 0,
	      "headers past what can be read: no ranges");
	std::vector<UInt8> farHeader(image);
	const UInt32 far = 0x7FFFFFF0u;
	std::memcpy(&farHeader[0x3C], &far, 4);
	Check(CodeRangesOfImage(farHeader.data(), static_cast<UInt32>(farHeader.size()), 0x79070000u, ranges, 4) == 0,
	      "e_lfanew past the readable headers: no ranges");
	Check(InCodeRange(ranges, 2, 0x79071000u) && InCodeRange(ranges, 2, 0x790757FFu) && !InCodeRange(ranges, 2, 0x79076000u),
	      "in a range: from its begin up to before its end");
}

}  // namespace

int main() {
	TestWordsAvailable();
	TestCollectCodeWords();
	TestFollowsCall();
	TestCodeRanges();

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
