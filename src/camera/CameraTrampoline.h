#pragma once

#include "core/Types.h"

namespace obvr::camera {

// Erzeugung des Hook-Codes, getrennt vom Setzen des Hooks.
//
// Diese Trennung hat einen praktischen Grund: die Bytefolge ist der Teil, bei
// dem ein Fehler das Spiel zuverlaessig zum Absturz bringt, und zugleich der
// einzige Teil, der sich ohne laufendes Oblivion pruefen laesst. Beide
// Funktionen sind deshalb frei von Windows- und Prozessabhaengigkeiten und
// werden von tests/ direkt geprueft.

// Die acht Bytes, die an addr::kHookCameraUpdate stehen muessen:
//   cmp word ptr [ebx+0xB6], 0
extern const UInt8 kOriginalBytes[8];

// Schreibt das Trampolin nach buffer. trampolineAddress ist die Adresse, an
// der es spaeter liegt; callbackAddress zeigt auf OBVR_OnCameraUpdated.
//
// Gibt die Anzahl geschriebener Bytes zurueck, oder 0, wenn die Kapazitaet
// nicht reicht.
UInt32 BuildTrampoline(UInt8* buffer, UInt32 capacity, UInt32 trampolineAddress,
                       UInt32 callbackAddress);

// Schreibt den Patch, der an die Hook-Stelle kommt: ein Sprung zum Trampolin,
// aufgefuellt auf die Laenge der ueberschriebenen Originalinstruktion.
//
// Gibt die Anzahl geschriebener Bytes zurueck, oder 0 bei zu kleiner
// Kapazitaet.
UInt32 BuildPatch(UInt8* buffer, UInt32 capacity, UInt32 hookAddress,
                  UInt32 trampolineAddress);

}  // namespace obvr::camera
