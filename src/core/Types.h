#pragma once

// Ganzzahltypen in der Schreibweise, die in der Oblivion- und OBSE-Welt
// ueblich ist. Bewusst plattformfrei gehalten, damit Bausteine wie der
// CodeWriter ohne Windows-Abhaengigkeit testbar bleiben.

using UInt8 = unsigned char;
using UInt16 = unsigned short;
using UInt32 = unsigned int;
using SInt32 = int;

static_assert(sizeof(UInt8) == 1, "UInt8 muss ein Byte sein");
static_assert(sizeof(UInt16) == 2, "UInt16 muss zwei Bytes sein");
static_assert(sizeof(UInt32) == 4, "UInt32 muss vier Bytes sein");
