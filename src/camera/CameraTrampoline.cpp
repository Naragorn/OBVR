#include "camera/CameraTrampoline.h"

#include "core/CodeWriter.h"
#include "game/GameAddresses.h"

namespace obvr::camera {

const UInt8 kOriginalBytes[8] = {
	0x66, 0x83, 0xBB, 0xB6, 0x00, 0x00, 0x00, 0x00,
};

UInt32 BuildTrampoline(UInt8* buffer, UInt32 capacity, UInt32 trampolineAddress,
                       UInt32 callbackAddress) {
	// Das Trampolin ruft OBVR auf und fuehrt danach die ueberschriebene
	// Instruktion samt ihrem Kontrollfluss originalgetreu aus:
	//
	//   pushad                          ; Register sichern
	//   pushfd                          ; Flags sichern
	//   push dword ptr [esp+0x20]       ; das gesicherte eax = CameraNode
	//   call OBVR_OnCameraUpdated
	//   add  esp, 4
	//   popfd
	//   popad
	//   cmp  word ptr [ebx+0xB6], 0     ; Original
	//   ja   0x0066BE7C                 ; Original
	//   xor  ecx, ecx                   ; Original
	//   jmp  0x0066BE84                 ; Original
	//
	// pushad legt EAX zuoberst ab, danach schiebt pushfd um vier Bytes; das
	// gesicherte EAX liegt deshalb bei [esp+0x20].
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
	// Fuenf Bytes Sprung, drei Bytes nop: die Originalinstruktion ist acht
	// Bytes lang und darf nicht halb stehen bleiben.
	mem::CodeWriter code(buffer, capacity, hookAddress);

	code.JumpRelative(trampolineAddress);
	code.Nop(addr::kHookCameraUpdatePatchSize - 5);

	return code.Overflowed() ? 0 : code.Size();
}

}  // namespace obvr::camera
