#include "core/CodeWriter.h"

namespace obvr::mem {

CodeWriter::CodeWriter(UInt8* buffer, UInt32 capacity, UInt32 baseAddress)
	: m_buffer(buffer),
	  m_capacity(capacity),
	  m_baseAddress(baseAddress),
	  m_size(0),
	  m_overflowed(false) {}

void CodeWriter::Byte(UInt8 value) {
	if (m_size >= m_capacity) {
		m_overflowed = true;
		return;
	}
	m_buffer[m_size++] = value;
}

void CodeWriter::Bytes(const UInt8* data, UInt32 size) {
	for (UInt32 i = 0; i < size; ++i) {
		Byte(data[i]);
	}
}

void CodeWriter::DWord(UInt32 value) {
	Byte(static_cast<UInt8>(value & 0xFF));
	Byte(static_cast<UInt8>((value >> 8) & 0xFF));
	Byte(static_cast<UInt8>((value >> 16) & 0xFF));
	Byte(static_cast<UInt8>((value >> 24) & 0xFF));
}

void CodeWriter::PushAllRegisters() { Byte(0x60); }
void CodeWriter::PopAllRegisters() { Byte(0x61); }
void CodeWriter::PushFlags() { Byte(0x9C); }
void CodeWriter::PopFlags() { Byte(0x9D); }

void CodeWriter::PushStackValue(UInt8 offset) {
	// ff 74 24 <offset>
	Byte(0xFF);
	Byte(0x74);
	Byte(0x24);
	Byte(offset);
}

void CodeWriter::AddStackPointer(UInt8 amount) {
	// 83 c4 <amount>
	Byte(0x83);
	Byte(0xC4);
	Byte(amount);
}

void CodeWriter::CallRelative(UInt32 target) {
	// e8 <rel32>, measured from the end of the instruction.
	//
	// The arithmetic is unsigned throughout, which is what makes it correct
	// on a 4GB-patched Oblivion.exe: with LargeAddressAware the trampoline
	// can sit above 2 GB, and the distance then wraps. On x86 the address
	// space is exactly 2^32 and the CPU computes EIP = EIP_next + rel32
	// modulo 2^32, so every target is reachable from every source.
	Byte(0xE8);
	const UInt32 next = AddressAt(m_size + 4);
	DWord(target - next);
}

void CodeWriter::JumpRelative(UInt32 target) {
	// e9 <rel32>
	Byte(0xE9);
	const UInt32 next = AddressAt(m_size + 4);
	DWord(target - next);
}

void CodeWriter::JumpAboveRelative(UInt32 target) {
	// 0f 87 <rel32>
	Byte(0x0F);
	Byte(0x87);
	const UInt32 next = AddressAt(m_size + 4);
	DWord(target - next);
}

void CodeWriter::PushEcx() {
	// 51
	Byte(0x51);
}

void CodeWriter::ClearEcx() {
	// 33 c9
	Byte(0x33);
	Byte(0xC9);
}

void CodeWriter::Nop(UInt32 count) {
	for (UInt32 i = 0; i < count; ++i) {
		Byte(0x90);
	}
}

void CodeWriter::PopToMemory(UInt32 address) {
	// 8F /0 with a moffs32 operand: pop dword ptr [imm32]
	Byte(0x8F);
	Byte(0x05);
	DWord(address);
}

void CodeWriter::PushFromMemory(UInt32 address) {
	// FF /6 with a moffs32 operand: push dword ptr [imm32]
	Byte(0xFF);
	Byte(0x35);
	DWord(address);
}

void CodeWriter::StoreStackPointer(UInt32 address) {
	// 89 /r, reg = esp (100), rm = disp32 (00 100 101): mov [imm32], esp
	Byte(0x89);
	Byte(0x25);
	DWord(address);
}

void CodeWriter::Return() { Byte(0xC3); }

}  // namespace obvr::mem
