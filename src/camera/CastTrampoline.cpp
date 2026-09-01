#include "camera/CastTrampoline.h"

#include "core/CodeWriter.h"
#include "game/GameAddresses.h"

namespace obvr::camera {

const UInt8 kCastOriginalBytes[7] = {
	0x53,                    // push ebx
	0x56,                    // push esi
	0x57,                    // push edi
	0x8B, 0x7C, 0x24, 0x10,  // mov edi, [esp+0x10]
};

UInt32 BuildCastTrampoline(UInt8* buffer, UInt32 capacity, UInt32 trampolineAddress,
                           UInt32 callbackAddress) {
	// The trampoline tells OBVR a cast is starting and then runs the four
	// instructions it displaced, leaving the function to carry on as if
	// nothing had happened:
	//
	//   pushad                     ; save registers
	//   pushfd                     ; save flags
	//   push ecx                   ; this = MagicCaster*
	//   call OBVR_OnMagicCastItem
	//   add  esp, 4
	//   popfd
	//   popad
	//   push ebx                   ; original
	//   push esi                   ; original
	//   push edi                   ; original
	//   mov  edi, [esp+0x10]       ; original
	//   jmp  0x00699197
	//
	// ecx is pushed directly rather than fished back out of the saved block.
	// pushad and pushfd copy registers to the stack; they do not change them,
	// so ecx still holds `this` when it is pushed - and reading it from the
	// register is one thing that can be wrong instead of two.
	//
	// THE DISPLACED INSTRUCTIONS SEE THE RIGHT STACK, which is the part worth
	// checking rather than assuming. Everything above them balances: pushad
	// and pushfd are undone, and the pushed argument is taken off by the add.
	// So when `push ebx` runs, esp is exactly what it was when the function
	// was entered, and the [esp+0x10] four instructions later reaches the
	// first argument just as it did before the patch.
	mem::CodeWriter code(buffer, capacity, trampolineAddress);

	code.PushAllRegisters();
	code.PushFlags();
	code.PushEcx();
	code.CallRelative(callbackAddress);
	code.AddStackPointer(4);
	code.PopFlags();
	code.PopAllRegisters();

	code.Bytes(kCastOriginalBytes, sizeof(kCastOriginalBytes));
	code.JumpRelative(addr::kHookMagicCastItemResume);

	return code.Overflowed() ? 0 : code.Size();
}

UInt32 BuildCastPatch(UInt8* buffer, UInt32 capacity, UInt32 hookAddress,
                      UInt32 trampolineAddress) {
	// Five bytes of jump and two of nop. The fourth displaced instruction ends
	// at seven, and leaving three bytes of it behind would be a crash rather
	// than a bug.
	mem::CodeWriter code(buffer, capacity, hookAddress);

	code.JumpRelative(trampolineAddress);
	code.Nop(addr::kHookMagicCastItemPatchSize - 5);

	return code.Overflowed() ? 0 : code.Size();
}

}  // namespace obvr::camera
