#pragma once

// Integer types in the spelling that is customary in the Oblivion and OBSE
// world. Deliberately platform free, so that building blocks like CodeWriter
// stay testable without a Windows dependency.

using UInt8 = unsigned char;
using UInt16 = unsigned short;
using UInt32 = unsigned int;
using UInt64 = unsigned long long;
using SInt16 = short;
using SInt32 = int;

static_assert(sizeof(UInt8) == 1, "UInt8 must be one byte");
static_assert(sizeof(UInt16) == 2, "UInt16 must be two bytes");
static_assert(sizeof(SInt16) == 2, "SInt16 must be two bytes");
static_assert(sizeof(UInt32) == 4, "UInt32 must be four bytes");
static_assert(sizeof(UInt64) == 8, "UInt64 must be eight bytes");
