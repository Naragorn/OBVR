#pragma once

#include "core/Types.h"

namespace obvr::mem {

// Assembles x86 machine code. Deliberately tiny: OBVR needs a handful of
// opcodes, not an assembler.
//
// Emitting the bytes here instead of using __declspec(naked) with an __asm
// block is intentional. MSVC inline assembly ties the project to MSVC, while
// this route builds with MSVC, clang-cl and clang-cross alike - and it can be
// tested without a running Oblivion.
//
// Deliberately free of Windows dependencies.
class CodeWriter {
public:
	// baseAddress is the address at which the code will later execute. It is
	// passed in rather than derived from the buffer, so that byte generation
	// can be checked outside the game process.
	CodeWriter(UInt8* buffer, UInt32 capacity, UInt32 baseAddress);

	void Byte(UInt8 value);
	void Bytes(const UInt8* data, UInt32 size);
	void DWord(UInt32 value);

	// pushad / popad save all eight general purpose registers.
	void PushAllRegisters();
	void PopAllRegisters();
	void PushFlags();
	void PopFlags();

	// push dword ptr [esp + offset]
	void PushStackValue(UInt8 offset);

	// add esp, amount
	void AddStackPointer(UInt8 amount);

	// call / jmp with a 32-bit relative target, from the current position.
	void CallRelative(UInt32 target);
	void JumpRelative(UInt32 target);

	// ja (jump if above) with a 32-bit relative target.
	void JumpAboveRelative(UInt32 target);

	// xor ecx, ecx
	void ClearEcx();

	// nop, to pad out a partially overwritten instruction.
	void Nop(UInt32 count);

	UInt32 Size() const { return m_size; }
	bool Overflowed() const { return m_overflowed; }

	// The address this code would have at the given position.
	UInt32 AddressAt(UInt32 offset) const { return m_baseAddress + offset; }

private:
	UInt8* m_buffer;
	UInt32 m_capacity;
	UInt32 m_baseAddress;
	UInt32 m_size;
	bool m_overflowed;
};

}  // namespace obvr::mem
