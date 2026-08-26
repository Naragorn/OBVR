// Checks the byte sequences generated for a function-entry detour - the hook
// shape the scene render (dual pass) uses.
//
// Like TrampolineTest, this runs natively: the generation is pure byte
// arithmetic over the addresses passed in. The expected values were worked
// out by hand, not by running the code under test - a rel32 is
// target - (site + 5), reduced modulo 2^32, and each expected byte string
// below carries that arithmetic in a comment.

#include <cstdio>

#include "core/EntryDetour.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	if (condition) {
		std::printf("  ok    %s\n", what);
	} else {
		std::printf("  FAIL  %s\n", what);
		++g_failures;
	}
}

void CheckBytes(const UInt8* actual, const UInt8* expected, UInt32 size, const char* what) {
	for (UInt32 i = 0; i < size; ++i) {
		if (actual[i] != expected[i]) {
			std::printf("  FAIL  %s: byte %u is %02X, expected %02X\n", what, i,
			            actual[i], expected[i]);
			++g_failures;
			return;
		}
	}
	std::printf("  ok    %s\n", what);
}

// The entry this detour was built for: the first two instructions of
// Oblivion's scene render at 0x0040C830.
//
//   push -1              6A FF
//   push 0x9AA163        68 63 A1 9A 00
constexpr UInt32 kEntryAddress = 0x0040C830;
constexpr UInt32 kEntryLength = 7;
const UInt8 kEntryBytes[kEntryLength] = {0x6A, 0xFF, 0x68, 0x63, 0xA1, 0x9A, 0x00};

// Freely chosen, far from the entry so the distances cannot come out right by
// accident.
constexpr UInt32 kTrampolineAddress = 0x20000000;
constexpr UInt32 kReplacementAddress = 0x30000000;

// Addresses only a LargeAddressAware process can hand out. The jump distance
// then exceeds 2 GB and the arithmetic must wrap - see HANDOFF section 5.
constexpr UInt32 kHighTrampolineAddress = 0xC0000000;
constexpr UInt32 kHighReplacementAddress = 0xD0000000;

void TestTrampoline() {
	std::printf("Entry trampoline\n");

	UInt8 buffer[16] = {};
	const UInt32 size = obvr::mem::BuildEntryTrampoline(
		buffer, sizeof(buffer), kTrampolineAddress, kEntryAddress, kEntryBytes,
		kEntryLength);

	Check(size == kEntryLength + 5, "trampoline is the entry plus one jump");

	// The way back in jumps to 0x0040C837, the first byte the patch does not
	// touch. The jump sits at 0x20000007, so its rel32 is
	// 0x0040C837 - 0x2000000C = 0xE040C82B (mod 2^32).
	const UInt8 expected[12] = {0x6A, 0xFF, 0x68, 0x63, 0xA1, 0x9A, 0x00,
	                            0xE9, 0x2B, 0xC8, 0x40, 0xE0};
	CheckBytes(buffer, expected, sizeof(expected), "entry bytes, then jmp past the patch");
}

void TestPatch() {
	std::printf("Entry patch\n");

	UInt8 buffer[8] = {};
	const UInt32 size = obvr::mem::BuildEntryPatch(buffer, sizeof(buffer), kEntryAddress,
	                                               kReplacementAddress, kEntryLength);

	Check(size == kEntryLength, "patch covers exactly the displaced entry");

	// jmp rel32 at 0x0040C830: 0x30000000 - 0x0040C835 = 0x2FBF37CB. The two
	// displaced bytes beyond the jump become nops - a torn instruction must
	// not survive, even one nothing should ever jump into.
	const UInt8 expected[7] = {0xE9, 0xCB, 0x37, 0xBF, 0x2F, 0x90, 0x90};
	CheckBytes(buffer, expected, sizeof(expected), "jmp to the replacement, nop padded");
}

void TestExactlyFiveBytes() {
	std::printf("A five byte entry needs no padding\n");

	// An entry of exactly one jump's length: legal, and the patch must not
	// write a single nop.
	const UInt8 entry[5] = {0x6A, 0xFF, 0x90, 0x90, 0x90};
	UInt8 patch[8] = {};
	const UInt32 patchSize = obvr::mem::BuildEntryPatch(patch, sizeof(patch), kEntryAddress,
	                                                    kReplacementAddress, 5);
	Check(patchSize == 5, "patch is the jump alone");
	const UInt8 expectedPatch[5] = {0xE9, 0xCB, 0x37, 0xBF, 0x2F};
	CheckBytes(patch, expectedPatch, sizeof(expectedPatch), "no nops appended");

	UInt8 trampoline[16] = {};
	const UInt32 trampolineSize = obvr::mem::BuildEntryTrampoline(
		trampoline, sizeof(trampoline), kTrampolineAddress, kEntryAddress, entry, 5);
	Check(trampolineSize == 10, "trampoline is five bytes plus one jump");

	// Jumps to 0x0040C835 from 0x20000005: 0x0040C835 - 0x2000000A = 0xE040C82B.
	const UInt8 expectedTrampoline[10] = {0x6A, 0xFF, 0x90, 0x90, 0x90,
	                                      0xE9, 0x2B, 0xC8, 0x40, 0xE0};
	CheckBytes(trampoline, expectedTrampoline, sizeof(expectedTrampoline),
	           "way back in lands after five bytes");
}

void TestLargeAddressAware() {
	std::printf("Above 2 GB the displacement wraps\n");

	// Trampoline at 0xC0000000, jumping down to 0x0040C837. The distance is
	// far outside a signed rel32's range read as a number; as a UInt32 it is
	// 0x0040C837 - 0xC000000C = 0x4040C82B, and EIP arithmetic wraps the same
	// way. Switching the generation to int32 arithmetic, or adding a range
	// check, breaks exactly this case - see TestLargeAddressAware in
	// TrampolineTest.cpp, which guards the same property for the camera hook.
	UInt8 buffer[16] = {};
	const UInt32 size = obvr::mem::BuildEntryTrampoline(
		buffer, sizeof(buffer), kHighTrampolineAddress, kEntryAddress, kEntryBytes,
		kEntryLength);
	Check(size == 12, "high trampoline still fits");
	const UInt8 expected[12] = {0x6A, 0xFF, 0x68, 0x63, 0xA1, 0x9A, 0x00,
	                            0xE9, 0x2B, 0xC8, 0x40, 0x40};
	CheckBytes(buffer, expected, sizeof(expected), "jump down from a high trampoline");

	// And the patch jumping up: 0xD0000000 - 0x0040C835 = 0xCFBF37CB.
	UInt8 patch[8] = {};
	const UInt32 patchSize = obvr::mem::BuildEntryPatch(
		patch, sizeof(patch), kEntryAddress, kHighReplacementAddress, kEntryLength);
	Check(patchSize == kEntryLength, "patch to a high replacement still fits");
	const UInt8 expectedPatch[7] = {0xE9, 0xCB, 0x37, 0xBF, 0xCF, 0x90, 0x90};
	CheckBytes(patch, expectedPatch, sizeof(expectedPatch), "jump up from a low entry");
}

void TestRefusals() {
	std::printf("Refusals\n");

	UInt8 buffer[16] = {};

	// Shorter than a jump: there is no way to patch four bytes with a five
	// byte instruction, and pretending otherwise would shred the entry.
	Check(obvr::mem::BuildEntryTrampoline(buffer, sizeof(buffer), kTrampolineAddress,
	                                      kEntryAddress, kEntryBytes, 4) == 0,
	      "trampoline refuses an entry shorter than a jump");
	Check(obvr::mem::BuildEntryPatch(buffer, sizeof(buffer), kEntryAddress,
	                                 kReplacementAddress, 4) == 0,
	      "patch refuses an entry shorter than a jump");

	// A buffer one byte too small. The generation must say no rather than
	// stop early: a truncated jump is a different, valid-looking instruction.
	Check(obvr::mem::BuildEntryTrampoline(buffer, 11, kTrampolineAddress, kEntryAddress,
	                                      kEntryBytes, kEntryLength) == 0,
	      "trampoline refuses a buffer one byte short");
	Check(obvr::mem::BuildEntryPatch(buffer, 6, kEntryAddress, kReplacementAddress,
	                                 kEntryLength) == 0,
	      "patch refuses a buffer one byte short");
}

}  // namespace

int main() {
	TestTrampoline();
	TestPatch();
	TestExactlyFiveBytes();
	TestLargeAddressAware();
	TestRefusals();

	if (g_failures != 0) {
		std::printf("%d FAILURES\n", g_failures);
		return 1;
	}
	std::printf("all ok\n");
	return 0;
}
