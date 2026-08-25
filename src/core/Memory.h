#pragma once

#include "platform/Win32Min.h"

namespace obvr::mem {

// Schreibt in Oblivions Codesegment und stellt den urspruenglichen Schutz
// wieder her. Gibt false zurueck, wenn VirtualProtect scheitert.
bool SafeWrite(UInt32 address, const void* data, UInt32 size);

// Liest Bytes aus dem Prozess. Dient dazu, vor dem Patchen zu pruefen, dass an
// der Zieladresse wirklich die erwartete Instruktion steht.
bool Verify(UInt32 address, const UInt8* expected, UInt32 size);

// Reserviert ausfuehrbaren Speicher fuer ein Trampolin.
void* AllocExecutable(UInt32 size);

}  // namespace obvr::mem
