// Checks the byte sequence generated for the camera hook.
//
// The test runs natively on the development machine, not inside Oblivion.
// That works because BuildTrampoline and BuildPatch are pure byte generation:
// they depend only on the addresses passed in, not on the architecture of the
// host system.
//
// The expected values were worked out by hand. Comparing against a second
// implementation of the same calculation would be worthless - the whole point
// is that the expected values were derived independently.

#include <cstdio>

#include "camera/CameraTrampoline.h"
#include "game/GameAddresses.h"

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
			std::printf("  FAIL  %s: byte %u is %02X, expected %02X\n",
			            what, i, actual[i], expected[i]);
			++g_failures;
			return;
		}
	}
	std::printf("  ok    %s\n", what);
}

void DumpHex(const UInt8* data, UInt32 size) {
	std::printf("       ");
	for (UInt32 i = 0; i < size; ++i) {
		std::printf("%02X ", data[i]);
		if ((i % 16) == 15) {
			std::printf("\n       ");
		}
	}
	std::printf("\n");
}

// Reads a rel32 field at the given position.
UInt32 ReadRel32(const UInt8* at) {
	return static_cast<UInt32>(at[0]) | (static_cast<UInt32>(at[1]) << 8) |
	       (static_cast<UInt32>(at[2]) << 16) | (static_cast<UInt32>(at[3]) << 24);
}

// Freely chosen addresses. They have to sit far enough away from the target
// addresses that the relative distances cannot come out right by accident.
constexpr UInt32 kTrampolineAddress = 0x20000000;
constexpr UInt32 kCallbackAddress = 0x30000000;

// Addresses that can only exist in a LargeAddressAware process. Without the
// 4GB patch VirtualAlloc only hands a 32-bit process addresses below
// 0x80000000; with the patch the trampoline can land above that.
constexpr UInt32 kHighTrampolineAddress = 0xC0000000;
constexpr UInt32 kHighCallbackAddress = 0xD0000000;

void TestTrampoline() {
	std::printf("Trampoline\n");

	UInt8 buffer[64] = {};
	const UInt32 size = obvr::camera::BuildTrampoline(
		buffer, sizeof(buffer), kTrampolineAddress, kCallbackAddress);

	DumpHex(buffer, size);

	// pushad(1) + pushfd(1) + push[esp+0x20](4) + call(5) + add esp(3)
	// + popfd(1) + popad(1) + cmp(8) + ja(6) + xor(2) + jmp(5) = 37
	Check(size == 37, "length is 37 bytes");

	Check(buffer[0] == 0x60, "starts with pushad");
	Check(buffer[1] == 0x9C, "followed by pushfd");

	// push dword ptr [esp+0x20] - fetches the EAX saved by pushad, which the
	// additional pushfd moved four bytes deeper.
	const UInt8 expectedPush[4] = {0xFF, 0x74, 0x24, 0x20};
	CheckBytes(buffer + 2, expectedPush, 4, "push dword ptr [esp+0x20]");

	// call rel32 to the callback. Target = address after the instruction plus
	// the relative displacement.
	Check(buffer[6] == 0xE8, "call rel32 follows");
	Check(kTrampolineAddress + 11 + ReadRel32(buffer + 7) == kCallbackAddress,
	      "call points at OBVR_OnCameraUpdated");

	const UInt8 expectedCleanup[6] = {0x83, 0xC4, 0x04, 0x9D, 0x61, 0x66};
	CheckBytes(buffer + 11, expectedCleanup, 6, "add esp,4 / popfd / popad");

	// The overwritten original instruction has to reappear verbatim, or the
	// game loses a comparison.
	CheckBytes(buffer + 16, obvr::camera::kOriginalBytes, 8,
	           "original instruction cmp word ptr [ebx+0xB6],0");

	// ja rel32 to the branch the original takes when the list is not empty.
	Check(buffer[24] == 0x0F && buffer[25] == 0x87, "ja rel32 follows");
	Check(kTrampolineAddress + 30 + ReadRel32(buffer + 26) ==
	          obvr::addr::kHookCameraUpdateResumeTaken,
	      "ja points at 0x0066BE7C");

	Check(buffer[30] == 0x33 && buffer[31] == 0xC9, "xor ecx,ecx");

	// jmp rel32 to the branch for the empty list.
	Check(buffer[32] == 0xE9, "jmp rel32 follows");
	Check(kTrampolineAddress + 37 + ReadRel32(buffer + 33) ==
	          obvr::addr::kHookCameraUpdateResumeEmpty,
	      "jmp points at 0x0066BE84");
}

void TestPatch() {
	std::printf("Patch\n");

	UInt8 buffer[8] = {};
	const UInt32 size = obvr::camera::BuildPatch(
		buffer, sizeof(buffer), obvr::addr::kHookCameraUpdate, kTrampolineAddress);

	DumpHex(buffer, size);

	// It has to cover the original instruction completely; if a remnant were
	// left standing, the game would execute fragments.
	Check(size == obvr::addr::kHookCameraUpdatePatchSize,
	      "length covers the 8-byte original instruction");

	Check(buffer[0] == 0xE9, "starts with jmp rel32");
	Check(obvr::addr::kHookCameraUpdate + 5 + ReadRel32(buffer + 1) == kTrampolineAddress,
	      "jmp points at the trampoline");

	Check(buffer[5] == 0x90 && buffer[6] == 0x90 && buffer[7] == 0x90,
	      "remainder padded with nop");
}

// The 4GB patch (LargeAddressAware) only changes two bytes in the PE header of
// Oblivion.exe, not the code - the eight bytes checked at 0x0066BE6E are
// identical in both variants, so the hook itself is unaffected.
//
// What does change is where the trampoline sits: VirtualAlloc can now place it
// above 2 GB, and the jump from the hook to it then spans more than 2 GB.
//
// On x86-64 that would be impossible: rel32 is a signed displacement of +-2 GB
// within a 64-bit address space there. On x86-32 the address space is exactly
// 2^32 and the CPU computes EIP = EIP_next + rel32 modulo 2^32 - every target
// is reachable from every source, the distance simply wraps.
//
// CodeWriter computes the displacements in UInt32 throughout. The wraparound
// is therefore well defined and matches the CPU exactly. If someone later
// switched this to int32_t or added a range check, it would surface here
// rather than in the game - and there only on machines with the 4GB patch,
// which would make the hunt unpleasant.
void TestLargeAddressAware() {
	std::printf("4GB patch: trampoline above 2 GB\n");

	UInt8 buffer[64] = {};
	const UInt32 size = obvr::camera::BuildTrampoline(
		buffer, sizeof(buffer), kHighTrampolineAddress, kHighCallbackAddress);

	Check(size == 37, "length unchanged at 37 bytes");

	Check(static_cast<UInt32>(kHighTrampolineAddress + 11 + ReadRel32(buffer + 7)) ==
	          kHighCallbackAddress,
	      "call reaches the callback above 2 GB");

	// The two return jumps run from 0xC0000000 back down to 0x0066BExx, a
	// distance of roughly -3.2 GB. This is where the wraparound shows.
	Check(static_cast<UInt32>(kHighTrampolineAddress + 30 + ReadRel32(buffer + 26)) ==
	          obvr::addr::kHookCameraUpdateResumeTaken,
	      "ja jumps from above back to 0x0066BE7C");

	Check(static_cast<UInt32>(kHighTrampolineAddress + 37 + ReadRel32(buffer + 33)) ==
	          obvr::addr::kHookCameraUpdateResumeEmpty,
	      "jmp jumps from above back to 0x0066BE84");

	// The patch at the hook site has to reach a high trampoline just as well
	// as a low one.
	UInt8 patch[8] = {};
	const UInt32 patchSize = obvr::camera::BuildPatch(
		patch, sizeof(patch), obvr::addr::kHookCameraUpdate, kHighTrampolineAddress);

	Check(patchSize == obvr::addr::kHookCameraUpdatePatchSize, "patch still 8 bytes");
	Check(patch[0] == 0xE9, "patch starts with jmp rel32");
	Check(static_cast<UInt32>(obvr::addr::kHookCameraUpdate + 5 + ReadRel32(patch + 1)) ==
	          kHighTrampolineAddress,
	      "jmp reaches the trampoline above 2 GB");

	// Cross-check: the address range may only affect the relative fields. If
	// opcodes differed too, byte generation would have picked different
	// instructions depending on the address - a fault the individual checks
	// above would not see.
	UInt8 low[64] = {};
	obvr::camera::BuildTrampoline(low, sizeof(low), kTrampolineAddress, kCallbackAddress);

	bool opcodesEqual = true;
	for (UInt32 i = 0; i < 37; ++i) {
		const bool isRel32Field =
			(i >= 7 && i <= 10) || (i >= 26 && i <= 29) || (i >= 33 && i <= 36);
		if (!isRel32Field && low[i] != buffer[i]) {
			std::printf("  FAIL  byte %u differs: %02X against %02X\n", i, low[i], buffer[i]);
			opcodesEqual = false;
		}
	}
	Check(opcodesEqual, "identical opcodes, only the rel32 fields differ");
}

void TestOverflowIsReported() {
	std::printf("Capacity\n");

	UInt8 tooSmall[16] = {};
	const UInt32 size = obvr::camera::BuildTrampoline(
		tooSmall, sizeof(tooSmall), kTrampolineAddress, kCallbackAddress);

	// A silently truncated trampoline would be the worst case: the hook would
	// be installed and end in the middle of nowhere.
	Check(size == 0, "a buffer that is too small reports 0 instead of truncating");
}

}  // namespace

int main() {
	std::printf("OBVR trampoline test\n\n");

	TestTrampoline();
	std::printf("\n");
	TestPatch();
	std::printf("\n");
	TestLargeAddressAware();
	std::printf("\n");
	TestOverflowIsReported();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
