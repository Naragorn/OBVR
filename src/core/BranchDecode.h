#pragma once

#include "core/Types.h"

namespace obvr::mem {

// The one shape a foreign patch at one of OBVR's sites almost always has: a
// five-byte relative jump or call planted by another plugin's detour
// (Detours, xOBSE's WriteRelJump, OBVR's own). Decoding it turns "the bytes
// differ" into "a jump to this address", which a module lookup can then put
// a file name to - and that name is the whole answer to "which mod is
// fighting OBVR for this site".
inline constexpr UInt8 kOpcodeJumpRel32 = 0xE9;
inline constexpr UInt8 kOpcodeCallRel32 = 0xE8;

struct RelativeBranch {
	bool isJump = false;
	bool isCall = false;
	UInt32 target = 0;
};

// Decodes the five bytes at address. False when they are neither a relative
// jump nor a relative call; the target is then left at zero.
inline bool DecodeRelativeBranch(const UInt8* bytes, UInt32 address, RelativeBranch& out) {
	out = RelativeBranch{};
	if (bytes[0] != kOpcodeJumpRel32 && bytes[0] != kOpcodeCallRel32) {
		return false;
	}
	const UInt32 rel = static_cast<UInt32>(bytes[1]) | (static_cast<UInt32>(bytes[2]) << 8) |
	                   (static_cast<UInt32>(bytes[3]) << 16) |
	                   (static_cast<UInt32>(bytes[4]) << 24);
	// rel32 is relative to the end of the five-byte instruction, and the
	// addition wraps on x86-32 exactly as the processor's does.
	out.target = address + 5 + rel;
	out.isJump = bytes[0] == kOpcodeJumpRel32;
	out.isCall = bytes[0] == kOpcodeCallRel32;
	return true;
}

// The file name at the end of a path, or the path itself when it has no
// separator. Points into the given string.
inline const char* FileBaseName(const char* path) {
	const char* base = path;
	for (const char* p = path; *p != '\0'; ++p) {
		if (*p == '\\' || *p == '/') {
			base = p + 1;
		}
	}
	return base;
}

}  // namespace obvr::mem
