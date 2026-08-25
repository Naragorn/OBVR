#include "camera/CameraTrampoline.h"

#include "core/CodeWriter.h"
#include "game/GameAddresses.h"

namespace obvr::camera {

const UInt8 kOriginalBytes[8] = {
	0x66, 0x83, 0xBB, 0xB6, 0x00, 0x00, 0x00, 0x00,
};

UInt32 BuildTrampoline(UInt8* buffer, UInt32 capacity, UInt32 trampolineAddress,
                       UInt32 callbackAddress) {
	// The trampoline calls OBVR and then runs the overwritten instruction
	// together with its original control flow, faithfully reproduced:
	//
	//   pushad                          ; save registers
	//   pushfd                          ; save flags
	//   push dword ptr [esp+0x20]       ; the saved eax = CameraNode
	//   call OBVR_OnCameraUpdated
	//   add  esp, 4
	//   popfd
	//   popad
	//   cmp  word ptr [ebx+0xB6], 0     ; original
	//   ja   0x0066BE7C                 ; original
	//   xor  ecx, ecx                   ; original
	//   jmp  0x0066BE84                 ; original
	//
	// pushad puts EAX on top, then pushfd shifts everything by four bytes, so
	// the saved EAX sits at [esp+0x20].
	mem::CodeWriter code(buffer, capacity, trampolineAddress);

	code.PushAllRegisters();
	code.PushFlags();
	code.PushStackValue(0x20);
	code.CallRelative(callbackAddress);
	code.AddStackPointer(4);
	code.PopFlags();
	code.PopAllRegisters();

	code.Bytes(kOriginalBytes, sizeof(kOriginalBytes));
	code.JumpAboveRelative(addr::kHookCameraUpdateResumeTaken);
	code.ClearEcx();
	code.JumpRelative(addr::kHookCameraUpdateResumeEmpty);

	return code.Overflowed() ? 0 : code.Size();
}

UInt32 BuildPatch(UInt8* buffer, UInt32 capacity, UInt32 hookAddress,
                  UInt32 trampolineAddress) {
	// Five bytes of jump, three bytes of nop: the original instruction is
	// eight bytes long and must not be left half standing.
	mem::CodeWriter code(buffer, capacity, hookAddress);

	code.JumpRelative(trampolineAddress);
	code.Nop(addr::kHookCameraUpdatePatchSize - 5);

	return code.Overflowed() ? 0 : code.Size();
}

}  // namespace obvr::camera
