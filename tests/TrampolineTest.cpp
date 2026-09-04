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
#include "camera/CastTrampoline.h"
#include "game/GameAddresses.h"
#include "game/WorldPickTrampoline.h"

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

// The cast hook's bytes, worked out by hand the same way.
//
// Its own test rather than a variation of the camera's, because the two differ
// in exactly the places where a mistake is fatal: what gets pushed for the
// callback, which instructions are displaced, and where control resumes.
void TestCastTrampoline() {
	std::printf("Cast hook trampoline\n");

	const UInt32 kTrampoline = 0x10000000;
	const UInt32 kCallback = 0x10001000;

	UInt8 buffer[64] = {};
	const UInt32 size =
		obvr::camera::BuildCastTrampoline(buffer, sizeof(buffer), kTrampoline, kCallback);

	// 60 9C 51 | E8 rel32 | 83 C4 04 | 9D 61 | 53 56 57 8B 7C 24 10 | E9 rel32
	//  3 + 5 + 3 + 2 + 7 + 5 = 25
	Check(size == 25, "the trampoline is 25 bytes");

	Check(buffer[0] == 0x60, "it saves the registers first");
	Check(buffer[1] == 0x9C, "then the flags");

	// push ecx, not a read from the saved block. ecx still holds `this`,
	// because pushad and pushfd copy registers rather than change them.
	Check(buffer[2] == 0x51, "then pushes ecx, which is the MagicCaster");

	Check(buffer[3] == 0xE8, "then calls the callback");
	const UInt32 callTarget = kTrampoline + 8;
	const UInt32 callRel = kCallback - callTarget;
	Check(*reinterpret_cast<const UInt32*>(buffer + 4) == callRel,
	      "with the offset measured from the instruction after the call");

	// The argument comes off again, then the saved state, in reverse order.
	Check(buffer[8] == 0x83 && buffer[9] == 0xC4 && buffer[10] == 0x04,
	      "the pushed argument is taken back off the stack");
	Check(buffer[11] == 0x9D, "the flags come back");
	Check(buffer[12] == 0x61, "and the registers");

	// THE PART THAT MUST BE EXACT. These four instructions were displaced from
	// the function and have to run here instead, seeing the stack they would
	// have seen there - which they do, because everything above them balances.
	const UInt8 displaced[7] = {0x53, 0x56, 0x57, 0x8B, 0x7C, 0x24, 0x10};
	CheckBytes(buffer + 13, displaced, sizeof(displaced),
	           "the four displaced instructions follow, unchanged");

	Check(buffer[20] == 0xE9, "and it jumps back into the function");
	const UInt32 jumpTarget = kTrampoline + 25;
	const UInt32 jumpRel = obvr::addr::kHookMagicCastItemResume - jumpTarget;
	Check(*reinterpret_cast<const UInt32*>(buffer + 21) == jumpRel,
	      "landing seven bytes past the hook, where the prologue continues");

	// The resume address must be the hook plus exactly what was displaced.
	// Anything else lands mid-instruction, which is a crash and not a bug.
	Check(obvr::addr::kHookMagicCastItemResume ==
	          obvr::addr::kHookMagicCastItem + obvr::addr::kHookMagicCastItemPatchSize,
	      "the resume address is the hook plus the seven displaced bytes");

	// The bytes the hook expects to find, read out of Oblivion.exe rather than
	// recalled: push ebx / push esi / push edi / mov edi,[esp+0x10].
	CheckBytes(obvr::camera::kCastOriginalBytes, displaced, sizeof(displaced),
	           "and the bytes it verifies before patching are the same four");
}

void TestCastPatch() {
	std::printf("Cast hook patch\n");

	const UInt32 kTrampoline = 0x10000000;
	UInt8 buffer[obvr::addr::kHookMagicCastItemPatchSize] = {};

	const UInt32 size = obvr::camera::BuildCastPatch(
		buffer, sizeof(buffer), obvr::addr::kHookMagicCastItem, kTrampoline);

	Check(size == obvr::addr::kHookMagicCastItemPatchSize,
	      "the patch fills all seven displaced bytes");
	Check(buffer[0] == 0xE9, "it starts with a jump");

	const UInt32 next = obvr::addr::kHookMagicCastItem + 5;
	Check(*reinterpret_cast<const UInt32*>(buffer + 1) == kTrampoline - next,
	      "whose offset is measured from the instruction after it");

	// Two nops. Without them, three bytes of `mov edi,[esp+0x10]` would be left
	// standing where the processor will run them.
	Check(buffer[5] == 0x90 && buffer[6] == 0x90,
	      "and the two bytes past the jump are nops, so no half instruction is left");

	UInt8 tooSmall[4] = {};
	Check(obvr::camera::BuildCastPatch(tooSmall, sizeof(tooSmall),
	                                   obvr::addr::kHookMagicCastItem, kTrampoline) == 0,
	      "a buffer too small for the jump reports 0 rather than half a patch");
}

void TestWorldPickTrampoline() {
	std::printf("World-pick hook trampoline\n");
	constexpr UInt32 kTrampoline = 0x21000000;
	constexpr UInt32 kCallback = 0x31000000;
	UInt8 buffer[64]{};
	const UInt32 size = obvr::game::BuildWorldPickTrampoline(
		buffer, sizeof(buffer), kTrampoline, kCallback);

	// pushad, pushfd, lea eax,[esp+24], push eax, call, cleanup, restores,
	// seven displaced bytes, and the return jump.
	Check(size == 29, "the world-pick trampoline is 29 bytes");
	const UInt8 prefix[] = {0x60, 0x9C, 0x8D, 0x44, 0x24, 0x24, 0x50, 0xE8};
	CheckBytes(buffer, prefix, sizeof(prefix),
	           "it saves state and passes the interrupted ESP");
	Check(kTrampoline + 12 + ReadRel32(buffer + 8) == kCallback,
	      "its call reaches the ray replacement callback");
	const UInt8 restore[] = {0x83, 0xC4, 0x04, 0x9D, 0x61};
	CheckBytes(buffer + 12, restore, sizeof(restore),
	           "it removes the argument and restores flags/registers");
	CheckBytes(buffer + 17, obvr::game::kWorldPickOriginalBytes, 7,
	           "it replays both complete x87 instructions");
	Check(buffer[24] == 0xE9, "it jumps back after the displaced instructions");
	Check(kTrampoline + 29 + ReadRel32(buffer + 25) ==
	          obvr::addr::kHookWorldPickRayResume,
	      "the return jump lands at 0x00580813");
	Check(obvr::addr::kHookWorldPickRayResume ==
	          obvr::addr::kHookWorldPickRay + obvr::addr::kHookWorldPickRayPatchSize,
	      "the resume address follows exactly seven displaced bytes");

	UInt8 patch[obvr::addr::kHookWorldPickRayPatchSize]{};
	const UInt32 patchSize = obvr::game::BuildWorldPickPatch(
		patch, sizeof(patch), obvr::addr::kHookWorldPickRay, kTrampoline);
	Check(patchSize == sizeof(patch), "the world-pick patch fills seven bytes");
	Check(patch[0] == 0xE9 && patch[5] == 0x90 && patch[6] == 0x90,
	      "the world-pick patch is a jump followed by two nops");
	Check(obvr::addr::kHookWorldPickRay + 5 + ReadRel32(patch + 1) == kTrampoline,
	      "the world-pick patch reaches its trampoline");

	UInt8 tooSmall[4]{};
	Check(obvr::game::BuildWorldPickPatch(
	          tooSmall, sizeof(tooSmall), obvr::addr::kHookWorldPickRay, kTrampoline) == 0,
	      "a short world-pick patch buffer is refused");
	Check(obvr::game::BuildWorldPickTrampoline(
	          tooSmall, sizeof(tooSmall), kTrampoline, kCallback) == 0,
	      "a short world-pick trampoline buffer is refused");

	Check(obvr::game::kHudReticleUpdateOriginalCall[0] == 0xE8,
	      "the verified HUDReticle instruction is a relative call");
	Check(obvr::addr::kHookHudReticleUpdateCall + 5 +
	          ReadRel32(obvr::game::kHudReticleUpdateOriginalCall + 1) ==
	          obvr::addr::kHudReticleUpdate,
	      "the verified original call reaches Oblivion's HUDReticle update");
	UInt8 hudReticlePatch[5]{};
	const UInt32 hudReticlePatchSize = obvr::game::BuildHudReticleUpdateCallPatch(
		hudReticlePatch, sizeof(hudReticlePatch), obvr::addr::kHookHudReticleUpdateCall,
		kCallback);
	Check(hudReticlePatchSize == sizeof(hudReticlePatch) && hudReticlePatch[0] == 0xE8,
	      "the HUDReticle wrapper patch is exactly one relative call");
	Check(obvr::addr::kHookHudReticleUpdateCall + 5 +
	          ReadRel32(hudReticlePatch + 1) == kCallback,
	      "the HUDReticle wrapper call reaches its replacement");
	Check(obvr::game::BuildHudReticleUpdateCallPatch(
	          tooSmall, sizeof(tooSmall), obvr::addr::kHookHudReticleUpdateCall,
	          kCallback) == 0,
	      "a short HUDReticle call buffer is refused");
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
	TestCastTrampoline();
	std::printf("\n");
	TestCastPatch();
	std::printf("\n");
	TestWorldPickTrampoline();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
