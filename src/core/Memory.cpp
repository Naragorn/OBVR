#include "core/Memory.h"

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

}  // namespace obvr::mem
