// Checks the decoder that names another plugin's detour at one of OBVR's
// patch sites: the five-byte relative jump and call, the wrap of the
// rel32 addition, the refusal of anything else, and the file-name cut.

#include <cstdio>
#include <cstring>

#include "core/BranchDecode.h"

namespace {

using obvr::mem::DecodeRelativeBranch;
using obvr::mem::FileBaseName;
using obvr::mem::RelativeBranch;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

void TestJumpAndCall() {
	std::printf("Jump and call\n");

	// jmp +0x100 from 0x0040C830: the target is the end of the instruction
	// plus the displacement.
	const UInt8 jump[5] = {0xE9, 0x00, 0x01, 0x00, 0x00};
	RelativeBranch b;
	Check(DecodeRelativeBranch(jump, 0x0040C830, b), "a rel32 jump decodes");
	Check(b.isJump && !b.isCall, "and is a jump");
	Check(b.target == 0x0040C830 + 5 + 0x100, "to the end of the instruction plus rel32");

	const UInt8 call[5] = {0xE8, 0xFB, 0xFF, 0xFF, 0xFF};  // call -5: itself
	Check(DecodeRelativeBranch(call, 0x00500000, b), "a rel32 call decodes");
	Check(b.isCall && !b.isJump, "and is a call");
	Check(b.target == 0x00500000, "a negative displacement wraps back correctly");

	// A detour into a DLL above the game: 0x0040C830 -> 0x10001000.
	const UInt32 rel = 0x10001000u - (0x0040C830u + 5);
	const UInt8 far[5] = {0xE9, static_cast<UInt8>(rel), static_cast<UInt8>(rel >> 8),
	                      static_cast<UInt8>(rel >> 16), static_cast<UInt8>(rel >> 24)};
	Check(DecodeRelativeBranch(far, 0x0040C830, b) && b.target == 0x10001000,
	      "a jump into another module lands on that module's address");
}

void TestRefusal() {
	std::printf("Refusal\n");

	const UInt8 prologue[5] = {0x6A, 0xFF, 0x68, 0x63, 0xA1};  // push -1; push ...
	RelativeBranch b;
	b.target = 0x1234;
	Check(!DecodeRelativeBranch(prologue, 0x0040C830, b), "the game's own prologue is no branch");
	Check(!b.isJump && !b.isCall && b.target == 0, "and the result is cleared");
}

void TestBaseName() {
	std::printf("Base name\n");

	Check(std::strcmp(FileBaseName("D:\\Oblivion\\Data\\OBSE\\Plugins\\OblivionReloaded.dll"),
	                  "OblivionReloaded.dll") == 0,
	      "a backslash path yields its file name");
	Check(std::strcmp(FileBaseName("plugins/OBVR.dll"), "OBVR.dll") == 0,
	      "a forward-slash path as well");
	Check(std::strcmp(FileBaseName("OBVR.dll"), "OBVR.dll") == 0,
	      "a bare name is returned whole");
	Check(std::strcmp(FileBaseName(""), "") == 0, "an empty string stays empty");
}

}  // namespace

int main() {
	TestJumpAndCall();
	TestRefusal();
	TestBaseName();

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
