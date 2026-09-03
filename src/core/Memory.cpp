#include "core/Memory.h"

#include "core/BranchDecode.h"
#include "core/Log.h"

namespace obvr::mem {

bool SafeWrite(UInt32 address, const void* data, UInt32 size) {
	DWORD oldProtect = 0;
	auto* target = reinterpret_cast<UInt8*>(address);

	if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
		return false;
	}

	const auto* source = static_cast<const UInt8*>(data);
	for (UInt32 i = 0; i < size; ++i) {
		target[i] = source[i];
	}

	DWORD ignored = 0;
	VirtualProtect(target, size, oldProtect, &ignored);
	FlushInstructionCache(GetCurrentProcess(), target, size);
	return true;
}

bool Verify(UInt32 address, const UInt8* expected, UInt32 size) {
	const auto* actual = reinterpret_cast<const UInt8*>(address);
	for (UInt32 i = 0; i < size; ++i) {
		if (actual[i] != expected[i]) {
			return false;
		}
	}
	return true;
}

void* AllocExecutable(UInt32 size) {
	return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
}


void ReportForeignCode(const char* subsystem, UInt32 address) {
	const auto* bytes = reinterpret_cast<const UInt8*>(address);
	RelativeBranch branch;
	if (!DecodeRelativeBranch(bytes, address, branch)) {
		OBVR_LOG("%s: at %08X the bytes are %02X %02X %02X %02X %02X %02X %02X %02X - not "
		         "the game's own and not a plugin's jump either; a different game version, "
		         "or a patch of another shape",
		         subsystem, address, bytes[0], bytes[1], bytes[2], bytes[3], bytes[4],
		         bytes[5], bytes[6], bytes[7]);
		return;
	}

	char path[512] = "";
	HMODULE module = nullptr;
	const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
	                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
	const bool named = GetModuleHandleExA(flags, reinterpret_cast<const char*>(branch.target),
	                                      &module) != 0 &&
	                   module != nullptr &&
	                   GetModuleFileNameA(module, path, sizeof(path)) != 0;
	OBVR_LOG("%s: at %08X another plugin's %s to %08X already sits there%s%s - the two "
	         "cannot share this site, and OBVR leaves it to whoever came first",
	         subsystem, address, branch.isJump ? "jump" : "call", branch.target,
	         named ? ", inside " : " (in no loaded module)", named ? FileBaseName(path) : "");
}
}  // namespace obvr::mem
