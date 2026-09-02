// Checks the bytes of the stub that wraps one engine call with a callback on
// either side - the shape the aim uses to set the heading only for the length
// of the call that reads it.
//
// Runs natively, like the other byte tests: the expected strings below were
// worked out by hand from the opcode tables, with each rel32 written as
// target - (site + 5) reduced modulo 2^32, and the arithmetic in a comment.

#include <cstdio>

#include "core/AroundCall.h"

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

// The engine function this is first put around: MagicCaster::UseActiveMagicItem.
constexpr UInt32 kTarget = 0x0069BEC0;
// The direct call to it inside the animation-key handler.
constexpr UInt32 kCallSite = 0x005FCE1D;

// Freely chosen and far apart, so no distance comes out right by accident.
constexpr UInt32 kStubAddress = 0x20000000;
constexpr UInt32 kBefore = 0x30000000;
constexpr UInt32 kAfter = 0x30000100;

void TestStub() {
	std::printf("Around-call stub\n");

	obvr::mem::AroundCallSlots slots;
	slots.returnAddress = 0x40000000;
	slots.arguments = 0x40000004;

	UInt8 buffer[64] = {};
	const UInt32 size = obvr::mem::BuildAroundCallStub(buffer, sizeof(buffer), kStubAddress,
	                                                    kTarget, kBefore, kAfter, slots);

	Check(size == 46, "stub is 46 bytes");

	const UInt8 expected[46] = {
		0x8F, 0x05, 0x00, 0x00, 0x00, 0x40,  // pop  dword ptr [0x40000000]
		0x89, 0x25, 0x04, 0x00, 0x00, 0x40,  // mov  dword ptr [0x40000004], esp
		0x60,                                // pushad
		0x9C,                                // pushfd
		0x51,                                // push ecx
		// call before, at 0x2000000F: 0x30000000 - 0x20000014 = 0x0FFFFFEC
		0xE8, 0xEC, 0xFF, 0xFF, 0x0F,
		0x83, 0xC4, 0x04,                    // add  esp, 4
		0x9D,                                // popfd
		0x61,                                // popad
		// call target, at 0x20000019: 0x0069BEC0 - 0x2000001E = 0xE069BEA2 (mod 2^32)
		0xE8, 0xA2, 0xBE, 0x69, 0xE0,
		0x60,                                // pushad
		0x9C,                                // pushfd
		// call after, at 0x20000020: 0x30000100 - 0x20000025 = 0x100000DB
		0xE8, 0xDB, 0x00, 0x00, 0x10,
		0x9D,                                // popfd
		0x61,                                // popad
		0xFF, 0x35, 0x00, 0x00, 0x00, 0x40,  // push dword ptr [0x40000000]
		0xC3,                                // ret
	};
	CheckBytes(buffer, expected, sizeof(expected), "return kept aside, callbacks around the call");

	UInt8 small[45] = {};
	Check(obvr::mem::BuildAroundCallStub(small, sizeof(small), kStubAddress, kTarget, kBefore,
	                                     kAfter, slots) == 0,
	      "one byte short is refused");
}

void TestCallSitePatch() {
	std::printf("Call-site patch\n");

	UInt8 buffer[8] = {};
	const UInt32 size =
		obvr::mem::BuildCallSitePatch(buffer, sizeof(buffer), kCallSite, kStubAddress);
	Check(size == 5, "a direct call is five bytes");

	// 0x20000000 - 0x005FCE22 = 0x1FA031DE
	const UInt8 expected[5] = {0xE8, 0xDE, 0x31, 0xA0, 0x1F};
	CheckBytes(buffer, expected, sizeof(expected), "call re-pointed at the stub");

	UInt8 small[4] = {};
	Check(obvr::mem::BuildCallSitePatch(small, sizeof(small), kCallSite, kStubAddress) == 0,
	      "four bytes are refused");
}

void TestDisplacement() {
	std::printf("Displacement of an untouched call\n");

	// What Oblivion.exe holds at 0x005FCE1D: E8 9E F0 09 00, read from the
	// file. 0x0069BEC0 - 0x005FCE22 = 0x0009F09E.
	Check(obvr::mem::CallRelativeDisplacement(kCallSite, kTarget) == 0x0009F09E,
	      "matches the bytes in the binary");

	// A target below the site wraps to the two's complement.
	Check(obvr::mem::CallRelativeDisplacement(0x0069B996, 0x0069A060) == 0xFFFFE6C5,
	      "backwards call wraps");
}

}  // namespace

int main() {
	TestStub();
	std::printf("\n");
	TestCallSitePatch();
	std::printf("\n");
	TestDisplacement();

	if (g_failures != 0) {
		std::printf("\n%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("\nall passed\n");
	return 0;
}
