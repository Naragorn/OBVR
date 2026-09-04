#include "game/WorldPickTrampoline.h"

#include "core/CodeWriter.h"
#include "game/GameAddresses.h"

namespace obvr::game {

const UInt8 kWorldPickOriginalBytes[7] = {
	0xDB, 0x43, 0x10,        // fild dword ptr [ebx+0x10]
	0xD9, 0x5C, 0x24, 0x18  // fstp dword ptr [esp+0x18]
};

UInt32 BuildWorldPickTrampoline(UInt8* buffer, UInt32 capacity,
                                UInt32 trampolineAddress, UInt32 callbackAddress) {
	mem::CodeWriter code(buffer, capacity, trampolineAddress);

	// pushad + pushfd move the interrupted function's esp down by 0x24.  Hand
	// that original stack base to ordinary C++, which can then replace the six
	// already-computed ray floats without knowing anything about our saves.
	code.PushAllRegisters();
	code.PushFlags();
	code.Byte(0x8D);  // lea eax,[esp+0x24]
	code.Byte(0x44);
	code.Byte(0x24);
	code.Byte(0x24);
	code.Byte(0x50);  // push eax
	code.CallRelative(callbackAddress);
	code.AddStackPointer(4);
	code.PopFlags();
	code.PopAllRegisters();

	code.Bytes(kWorldPickOriginalBytes, sizeof(kWorldPickOriginalBytes));
	code.JumpRelative(addr::kHookWorldPickRayResume);
	return code.Overflowed() ? 0 : code.Size();
}

UInt32 BuildWorldPickPatch(UInt8* buffer, UInt32 capacity, UInt32 hookAddress,
                           UInt32 trampolineAddress) {
	mem::CodeWriter code(buffer, capacity, hookAddress);
	code.JumpRelative(trampolineAddress);
	code.Nop(addr::kHookWorldPickRayPatchSize - 5);
	return code.Overflowed() ? 0 : code.Size();
}

}  // namespace obvr::game
