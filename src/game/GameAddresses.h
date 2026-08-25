#pragma once

#include "core/Types.h"

// Feste Adressen in Oblivion.exe 1.2.0.416 (ImageBase 0x00400000).
//
// Jede Adresse hier ist gegen die tatsaechliche Binary disassembliert und
// unten mit dem Befund belegt. Nichts davon ist geraten oder aus einer
// fremden Quelle uebernommen, ohne es nachzupruefen.

namespace obvr::addr {

// Ende der Vanilla-Kameraberechnung.
//
// Der relevante Block sieht so aus:
//
//   0066BE1B  mov  ebx, [esp+0x14]              ; Kameraobjekt
//   0066BE1F  cmp  word ptr [ebx+0xB6], 0       ; Knotenliste leer?
//   0066BE27  ja   0066BE2D
//   0066BE29  xor  eax, eax
//   0066BE2B  jmp  0066BE35
//   0066BE2D  mov  edx, [ebx+0xB0]              ; Knotenliste
//   0066BE33  mov  eax, [edx]                   ; eax = CameraNode (NiAVObject*)
//   0066BE35  mov  ecx, [esp+0x38]
//   0066BE39  mov  edx, [esp+0x3C]
//   0066BE3D  mov  [eax+0x54], ecx              ; localTransform.pos.x
//   0066BE40  mov  ecx, [esp+0x40]
//   0066BE44  mov  [eax+0x58], edx              ; localTransform.pos.y
//   0066BE47  mov  [eax+0x5C], ecx              ; localTransform.pos.z
//   0066BE4A  cmp  word ptr [ebx+0xB6], 0
//   0066BE52  ja   0066BE58
//   0066BE54  xor  eax, eax
//   0066BE56  jmp  0066BE60
//   0066BE58  mov  edx, [ebx+0xB0]
//   0066BE5E  mov  eax, [edx]                   ; eax = CameraNode
//   0066BE60  lea  edi, [eax+0x30]              ; localTransform.rot
//   0066BE63  mov  ecx, 9
//   0066BE68  lea  esi, [esp+0x60]
//   0066BE6C  rep  movsd                        ; 9 DWORDs = NiMatrix33
//   0066BE6E  <-- kHookCameraUpdate
//
// Ab 0x0066BE6E stehen Position und Rotation der Kamera fest und eax haelt
// noch den CameraNode: genau der Punkt, an dem OBVR die Kopfrotation
// aufsetzen will.
//
// Die Schreibziele [eax+0x54] und [eax+0x30] belegen zugleich das
// NiAVObject-Layout in GameTypes.h.
inline constexpr UInt32 kHookCameraUpdate = 0x0066BE6E;

// Ueberschriebene Originalinstruktion an kHookCameraUpdate:
//
//   0066BE6E  66 83 BB B6 00 00 00 00   cmp word ptr [ebx+0xB6], 0
//
// Acht Bytes, also genug Platz fuer einen 5-Byte-jmp. Der nachfolgende
// Kontrollfluss wird im Trampolin originalgetreu nachgebaut:
//
//   0066BE76  ja   0066BE7C     -> kHookCameraUpdateResumeTaken
//   0066BE78  xor  ecx, ecx
//   0066BE7A  jmp  0066BE84     -> kHookCameraUpdateResumeEmpty
inline constexpr UInt32 kHookCameraUpdatePatchSize = 8;
inline constexpr UInt32 kHookCameraUpdateResumeTaken = 0x0066BE7C;
inline constexpr UInt32 kHookCameraUpdateResumeEmpty = 0x0066BE84;

// Kurz nach dem Hook ruft das Spiel auf dem CameraNode auf:
//
//   0066BE84  fldz
//   0066BE86  push 0
//   0066BE88  push ecx                          ; ecx = CameraNode
//   0066BE89  fstp [esp]
//   0066BE8C  call 00707370
//
// und 0x00707370 dispatcht ueber vtable-Slot 0x64:
//
//   007073A7  mov  eax, [esi]
//   007073A9  mov  edx, [eax+0x64]
//   007073B0  call edx
//
// Slot 0x64 / 4 = Index 25 = NiAVObject::UpdateSelectedDownwardPass.
//
// Daraus folgt der entscheidende Punkt fuer OBVR: nach dem Hook laeuft noch
// ein Szenengraph-Update-Pass. OBVR muss deshalb localTransform aendern,
// nicht worldTransform - die Welttransformation wird ohnehin neu aus
// parent * local berechnet.
inline constexpr UInt32 kUpdateSelectedDownwardPass = 0x00707370;

// Zeiger auf den Spieler. Belegt durch 0x0066C580 (ToggleCamera), das auf
// [ecx+0x588] schreibt - dem isThirdPerson-Flag im PlayerCharacter.
inline constexpr UInt32 kPlayerPointer = 0x00B333C4;
inline constexpr UInt32 kPlayerIsThirdPersonOffset = 0x588;

// Erwartete Spielversion. OBSE meldet sie als oblivionVersion.
inline constexpr UInt32 kOblivionVersion_1_2_416 = 0x010201A0;

}  // namespace obvr::addr
