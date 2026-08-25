#pragma once

#include "core/Types.h"

namespace obvr::mem {

// Baut x86-Maschinencode zusammen. Bewusst winzig gehalten: OBVR braucht eine
// Handvoll Opcodes, keinen Assembler.
//
// Der Umweg ueber selbst emittierte Bytes statt __declspec(naked) mit
// __asm-Block ist Absicht. MSVC-Inline-Assembly bindet das Projekt an MSVC,
// waehrend dieser Weg mit MSVC, clang-cl und clang-cross gleichermassen baut -
// und sich ausserdem ohne laufendes Oblivion testen laesst.
//
// Bewusst frei von Windows-Abhaengigkeiten.
class CodeWriter {
public:
	// baseAddress ist die Adresse, an der der Code spaeter ausgefuehrt wird.
	// Sie wird uebergeben statt aus dem Puffer abgeleitet, damit sich die
	// Byteerzeugung ausserhalb des Spielprozesses pruefen laesst.
	CodeWriter(UInt8* buffer, UInt32 capacity, UInt32 baseAddress);

	void Byte(UInt8 value);
	void Bytes(const UInt8* data, UInt32 size);
	void DWord(UInt32 value);

	// pushad / popad sichern alle acht Universalregister.
	void PushAllRegisters();
	void PopAllRegisters();
	void PushFlags();
	void PopFlags();

	// push dword ptr [esp + offset]
	void PushStackValue(UInt8 offset);

	// add esp, amount
	void AddStackPointer(UInt8 amount);

	// call / jmp mit 32-Bit-Relativziel, ausgehend von der aktuellen Position.
	void CallRelative(UInt32 target);
	void JumpRelative(UInt32 target);

	// ja (jump if above) mit 32-Bit-Relativziel.
	void JumpAboveRelative(UInt32 target);

	// xor ecx, ecx
	void ClearEcx();

	// nop, zum Auffuellen angebrochener Instruktionen.
	void Nop(UInt32 count);

	UInt32 Size() const { return m_size; }
	bool Overflowed() const { return m_overflowed; }

	// Adresse, die der Code an der angegebenen Position spaeter haette.
	UInt32 AddressAt(UInt32 offset) const { return m_baseAddress + offset; }

private:
	UInt8* m_buffer;
	UInt32 m_capacity;
	UInt32 m_baseAddress;
	UInt32 m_size;
	bool m_overflowed;
};

}  // namespace obvr::mem
